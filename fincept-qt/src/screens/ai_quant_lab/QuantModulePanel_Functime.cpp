// src/screens/ai_quant_lab/QuantModulePanel_Functime.cpp
//
// Functime panel — 6 analysis modes (forecast, anomaly, seasonality, metrics,
// confidence intervals, stationarity). Mirrors the GS Quant pattern. Extracted
// from QuantModulePanel.cpp to keep that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

// ═══════════════════════════════════════════════════════════════════════════════
// FUNCTIME PANEL — 6 analysis modes (forecast, anomaly, seasonality, metrics,
// confidence intervals, stationarity). Mirrors the GS Quant pattern.
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_functime_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // Local sample-button helper — same shape as gs_quant's
    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed,
                                  const QString& tip, bool walk = false) -> QPushButton* {
        auto* btn = new QPushButton("LOAD SAMPLE", parent);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(22);
        btn->setToolTip(tip);
        btn->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %2;"
                                   "padding:0 8px; font-size:9px; font-weight:700; letter-spacing:0.5px; border-radius:2px; }"
                                   "QPushButton:hover { color:%3; border-color:%3; background:rgba(255,255,255,0.04); }")
                               .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), module_.color.name()));
        QPointer<QLineEdit> guard(edit);
        connect(btn, &QPushButton::clicked, this, [guard, seed, walk]() {
            if (guard) guard->setText(walk ? sample_random_walk(seed) : sample_series(seed));
        });
        return btn;
    };

    // ── Forecast ─────────────────────────────────────────────────────────────
    auto* fc = new QWidget(this);
    auto* fcl = new QVBoxLayout(fc);
    fcl->setContentsMargins(12, 12, 12, 12);
    fcl->setSpacing(8);

    auto* fc_vals = new QLineEdit(fc);
    fc_vals->setPlaceholderText("Time series values (>= 30). CSV, space, or newline separated.");
    fc_vals->setStyleSheet(input_ss());
    text_inputs_["fn_fc_values"] = fc_vals;
    fcl->addWidget(build_input_row("Series Values", fc_vals, fc));
    fcl->addWidget(add_sample_btn(fc_vals, fc, 11,
                                   "200-pt synthetic series: trend + 30-pt seasonal cycle + noise"));

    auto* fc_model = new QComboBox(fc);
    fc_model->addItems({"linear", "ridge", "lasso", "elasticnet", "knn", "naive", "drift",
                        "holt_winters", "arima"});
    fc_model->setStyleSheet(combo_ss());
    combo_inputs_["fn_fc_model"] = fc_model;
    fcl->addWidget(build_input_row("Model", fc_model, fc));

    auto* fc_horizon = new QSpinBox(fc);
    fc_horizon->setRange(1, 365);
    fc_horizon->setValue(14);
    fc_horizon->setStyleSheet(spinbox_ss());
    int_inputs_["fn_fc_horizon"] = fc_horizon;
    fcl->addWidget(build_input_row("Forecast Horizon (steps)", fc_horizon, fc));

    auto* fc_lags = new QSpinBox(fc);
    fc_lags->setRange(1, 60);
    fc_lags->setValue(7);
    fc_lags->setStyleSheet(spinbox_ss());
    int_inputs_["fn_fc_lags"] = fc_lags;
    fcl->addWidget(build_input_row("Lag Window (ML models)", fc_lags, fc));

    auto* fc_season = new QSpinBox(fc);
    fc_season->setRange(0, 365);
    fc_season->setValue(0);
    fc_season->setSpecialValueText("none");
    fc_season->setStyleSheet(spinbox_ss());
    int_inputs_["fn_fc_season"] = fc_season;
    fcl->addWidget(build_input_row("Seasonal Period (HW/ARIMA)", fc_season, fc));

    auto* fc_alpha = make_double_spin(0.0001, 100, 1.0, 4, "", fc);
    double_inputs_["fn_fc_alpha"] = fc_alpha;
    fcl->addWidget(build_input_row("Regularization α (Ridge/Lasso/EN)", fc_alpha, fc));

    auto* fc_run = make_run_button("RUN FORECAST", fc);
    connect(fc_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["fn_fc_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 30) {
            display_error(QString("Need at least 30 observations; you provided %1. "
                                  "Click LOAD SAMPLE for 200.")
                              .arg(vals.size()));
            return;
        }
        const QString model = combo_inputs_["fn_fc_model"]->currentText();
        show_loading(QString("Fitting %1 on %2 obs and projecting %3 steps...")
                         .arg(model).arg(vals.size()).arg(int_inputs_["fn_fc_horizon"]->value()));
        QJsonObject params;
        params["values"] = vals;
        params["model"] = model;
        params["horizon"] = int_inputs_["fn_fc_horizon"]->value();
        params["lags"] = int_inputs_["fn_fc_lags"]->value();
        params["season"] = int_inputs_["fn_fc_season"]->value();
        params["alpha"] = double_inputs_["fn_fc_alpha"]->value();
        AIQuantLabService::instance().functime_forecast(params);
    });
    fcl->addWidget(fc_run);
    fcl->addStretch();
    tabs->addTab(fc, "Forecast");

    // ── Anomaly Detection ────────────────────────────────────────────────────
    auto* an = new QWidget(this);
    auto* anl = new QVBoxLayout(an);
    anl->setContentsMargins(12, 12, 12, 12);
    anl->setSpacing(8);

    auto* an_vals = new QLineEdit(an);
    an_vals->setPlaceholderText("Time series values (>= 20)");
    an_vals->setStyleSheet(input_ss());
    text_inputs_["fn_an_values"] = an_vals;
    anl->addWidget(build_input_row("Series Values", an_vals, an));
    anl->addWidget(add_sample_btn(an_vals, an, 21,
                                   "200-pt series — paste in your own outliers to test detection"));

    auto* an_method = new QComboBox(an);
    an_method->addItems({"zscore", "iqr", "isolation_forest", "residual"});
    an_method->setStyleSheet(combo_ss());
    combo_inputs_["fn_an_method"] = an_method;
    anl->addWidget(build_input_row("Detection Method", an_method, an));

    auto* an_thr = make_double_spin(0.5, 10.0, 3.0, 2, " σ", an);
    double_inputs_["fn_an_threshold"] = an_thr;
    anl->addWidget(build_input_row("Z-Score / Residual Threshold", an_thr, an));

    auto* an_iqr = make_double_spin(0.5, 5.0, 1.5, 2, "×", an);
    double_inputs_["fn_an_iqr_k"] = an_iqr;
    anl->addWidget(build_input_row("IQR Multiplier (IQR method)", an_iqr, an));

    auto* an_cont = make_double_spin(0.001, 0.5, 0.05, 3, "", an);
    double_inputs_["fn_an_contamination"] = an_cont;
    anl->addWidget(build_input_row("Expected Anomaly Fraction (Isolation Forest)", an_cont, an));

    auto* an_run = make_run_button("DETECT ANOMALIES", an);
    connect(an_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["fn_an_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 20) {
            display_error(QString("Need at least 20 observations; you provided %1.").arg(vals.size()));
            return;
        }
        const QString method = combo_inputs_["fn_an_method"]->currentText();
        show_loading(QString("Scanning %1 obs with %2...").arg(vals.size()).arg(method));
        QJsonObject params;
        params["values"] = vals;
        params["method"] = method;
        params["threshold"] = double_inputs_["fn_an_threshold"]->value();
        params["iqr_k"] = double_inputs_["fn_an_iqr_k"]->value();
        params["contamination"] = double_inputs_["fn_an_contamination"]->value();
        AIQuantLabService::instance().functime_anomaly_detection(params);
    });
    anl->addWidget(an_run);
    anl->addStretch();
    tabs->addTab(an, "Anomalies");

    // ── Seasonality ──────────────────────────────────────────────────────────
    auto* se = new QWidget(this);
    auto* sel = new QVBoxLayout(se);
    sel->setContentsMargins(12, 12, 12, 12);
    sel->setSpacing(8);

    auto* se_vals = new QLineEdit(se);
    se_vals->setPlaceholderText("Time series values (>= 24). Period auto-detected if left at 0.");
    se_vals->setStyleSheet(input_ss());
    text_inputs_["fn_se_values"] = se_vals;
    sel->addWidget(build_input_row("Series Values", se_vals, se));
    sel->addWidget(add_sample_btn(se_vals, se, 31,
                                   "200-pt series with embedded period-30 seasonality"));

    auto* se_period = new QSpinBox(se);
    se_period->setRange(0, 365);
    se_period->setValue(0);
    se_period->setSpecialValueText("auto");
    se_period->setStyleSheet(spinbox_ss());
    int_inputs_["fn_se_period"] = se_period;
    sel->addWidget(build_input_row("Period (0 = auto-detect)", se_period, se));

    auto* se_robust = new QComboBox(se);
    se_robust->addItems({"true", "false"});
    se_robust->setStyleSheet(combo_ss());
    combo_inputs_["fn_se_robust"] = se_robust;
    sel->addWidget(build_input_row("Robust STL", se_robust, se));

    auto* se_run = make_run_button("DECOMPOSE SEASONALITY", se);
    connect(se_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["fn_se_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 24) {
            display_error(QString("Need at least 24 observations; you provided %1.").arg(vals.size()));
            return;
        }
        show_loading(QString("Running STL decomposition on %1 obs...").arg(vals.size()));
        QJsonObject params;
        params["values"] = vals;
        params["period"] = int_inputs_["fn_se_period"]->value();
        params["robust"] = combo_inputs_["fn_se_robust"]->currentText() == "true";
        AIQuantLabService::instance().functime_seasonality(params);
    });
    sel->addWidget(se_run);
    sel->addStretch();
    tabs->addTab(se, "Seasonality");

    // ── Metrics ──────────────────────────────────────────────────────────────
    auto* me = new QWidget(this);
    auto* mel = new QVBoxLayout(me);
    mel->setContentsMargins(12, 12, 12, 12);
    mel->setSpacing(8);

    auto* me_act = new QLineEdit(me);
    me_act->setPlaceholderText("Actual values (must match predicted length)");
    me_act->setStyleSheet(input_ss());
    text_inputs_["fn_me_actual"] = me_act;
    mel->addWidget(build_input_row("Actual", me_act, me));
    mel->addWidget(add_sample_btn(me_act, me, 41,
                                   "100-pt synthetic 'actual' series (level ~50)"));

    auto* me_pred = new QLineEdit(me);
    me_pred->setPlaceholderText("Predicted values (same length as actual)");
    me_pred->setStyleSheet(input_ss());
    text_inputs_["fn_me_pred"] = me_pred;
    mel->addWidget(build_input_row("Predicted", me_pred, me));
    mel->addWidget(add_sample_btn(me_pred, me, 42,
                                   "100-pt synthetic 'predicted' series (similar shape, mild noise)"));

    auto* me_run = make_run_button("CALCULATE METRICS", me);
    connect(me_run, &QPushButton::clicked, this, [this]() {
        QJsonArray a, p;
        QString bad;
        if (!parse_doubles(text_inputs_["fn_me_actual"]->text(), a, &bad)) {
            display_error(QString("Actual: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["fn_me_pred"]->text(), p, &bad)) {
            display_error(QString("Predicted: '%1' is not numeric.").arg(bad));
            return;
        }
        if (a.size() < 2 || p.size() < 2) {
            display_error("Need at least 2 observations in both actual and predicted.");
            return;
        }
        if (a.size() != p.size()) {
            display_error(QString("Actual (%1) and Predicted (%2) must have the same length.")
                              .arg(a.size()).arg(p.size()));
            return;
        }
        show_loading(QString("Computing forecast accuracy on %1 pairs...").arg(a.size()));
        QJsonObject params;
        params["actual"] = a;
        params["predicted"] = p;
        AIQuantLabService::instance().functime_metrics(params);
    });
    mel->addWidget(me_run);
    mel->addStretch();
    tabs->addTab(me, "Metrics");

    // ── Confidence Intervals ─────────────────────────────────────────────────
    auto* ci = new QWidget(this);
    auto* cil = new QVBoxLayout(ci);
    cil->setContentsMargins(12, 12, 12, 12);
    cil->setSpacing(8);

    auto* ci_vals = new QLineEdit(ci);
    ci_vals->setPlaceholderText("Series values (training history, >= 30)");
    ci_vals->setStyleSheet(input_ss());
    text_inputs_["fn_ci_values"] = ci_vals;
    cil->addWidget(build_input_row("Series Values", ci_vals, ci));
    cil->addWidget(add_sample_btn(ci_vals, ci, 51,
                                   "200-pt synthetic series with seasonality"));

    auto* ci_method = new QComboBox(ci);
    ci_method->addItems({"bootstrap", "residual"});
    ci_method->setStyleSheet(combo_ss());
    combo_inputs_["fn_ci_method"] = ci_method;
    cil->addWidget(build_input_row("Interval Method", ci_method, ci));

    auto* ci_horizon = new QSpinBox(ci);
    ci_horizon->setRange(1, 90);
    ci_horizon->setValue(14);
    ci_horizon->setStyleSheet(spinbox_ss());
    int_inputs_["fn_ci_horizon"] = ci_horizon;
    cil->addWidget(build_input_row("Forecast Horizon", ci_horizon, ci));

    auto* ci_lags = new QSpinBox(ci);
    ci_lags->setRange(1, 60);
    ci_lags->setValue(7);
    ci_lags->setStyleSheet(spinbox_ss());
    int_inputs_["fn_ci_lags"] = ci_lags;
    cil->addWidget(build_input_row("Lag Window", ci_lags, ci));

    auto* ci_boot = new QSpinBox(ci);
    ci_boot->setRange(50, 1000);
    ci_boot->setValue(200);
    ci_boot->setStyleSheet(spinbox_ss());
    int_inputs_["fn_ci_n_boot"] = ci_boot;
    cil->addWidget(build_input_row("Bootstrap Iterations", ci_boot, ci));

    auto* ci_conf = make_double_spin(0.50, 0.999, 0.95, 3, "", ci);
    double_inputs_["fn_ci_confidence"] = ci_conf;
    cil->addWidget(build_input_row("Confidence Level", ci_conf, ci));

    auto* ci_run = make_run_button("BUILD INTERVALS", ci);
    connect(ci_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["fn_ci_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 30) {
            display_error(QString("Need at least 30 observations; you provided %1.").arg(vals.size()));
            return;
        }
        const QString method = combo_inputs_["fn_ci_method"]->currentText();
        show_loading(QString("Building %1 prediction intervals...").arg(method));
        QJsonObject params;
        params["values"] = vals;
        params["method"] = method;
        params["horizon"] = int_inputs_["fn_ci_horizon"]->value();
        params["lags"] = int_inputs_["fn_ci_lags"]->value();
        params["n_boot"] = int_inputs_["fn_ci_n_boot"]->value();
        params["confidence"] = double_inputs_["fn_ci_confidence"]->value();
        AIQuantLabService::instance().functime_confidence_intervals(params);
    });
    cil->addWidget(ci_run);
    cil->addStretch();
    tabs->addTab(ci, "Confidence Intervals");

    // ── Stationarity ─────────────────────────────────────────────────────────
    auto* st = new QWidget(this);
    auto* stl = new QVBoxLayout(st);
    stl->setContentsMargins(12, 12, 12, 12);
    stl->setSpacing(8);

    auto* st_vals = new QLineEdit(st);
    st_vals->setPlaceholderText("Series values (>= 30) — runs ADF + KPSS at each differencing order");
    st_vals->setStyleSheet(input_ss());
    text_inputs_["fn_st_values"] = st_vals;
    stl->addWidget(build_input_row("Series Values", st_vals, st));
    stl->addWidget(add_sample_btn(st_vals, st, 61,
                                   "150-pt random walk with drift — should require d=1", true));

    auto* st_max = new QSpinBox(st);
    st_max->setRange(0, 3);
    st_max->setValue(2);
    st_max->setStyleSheet(spinbox_ss());
    int_inputs_["fn_st_max_d"] = st_max;
    stl->addWidget(build_input_row("Max Differencing Order to Test", st_max, st));

    auto* st_run = make_run_button("RUN STATIONARITY TESTS", st);
    connect(st_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["fn_st_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 30) {
            display_error(QString("Need at least 30 observations; you provided %1.").arg(vals.size()));
            return;
        }
        show_loading(QString("Running ADF + KPSS up to d=%1...").arg(int_inputs_["fn_st_max_d"]->value()));
        QJsonObject params;
        params["values"] = vals;
        params["max_d"] = int_inputs_["fn_st_max_d"]->value();
        AIQuantLabService::instance().functime_stationarity(params);
    });
    stl->addWidget(st_run);
    stl->addStretch();
    tabs->addTab(st, "Stationarity");

    vl->addWidget(tabs);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ── FUNCTIME: rich card-based display per command ───────────────────────────

void QuantModulePanel::display_functime_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    if (!payload.value("success").toBool(false)) {
        const QString err = payload.value("error").toString("Unknown error");
        const QString kind = payload.value("error_kind").toString();
        const QString prefix = kind == "validation" ? "Input error: "
                              : kind == "runtime"    ? "Computation failed: "
                                                     : "";
        display_error(prefix + err);
        return;
    }

    const QJsonObject d = payload.value("data").toObject();
    const QString accent = module_.color.name();

    // Section header strip — shared with GS Quant for visual consistency.
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_observations"))
        header_text += QString("  |  %1 OBS").arg(d.value("n_observations").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 1. CHECK STATUS ──────────────────────────────────────────────────────
    if (command == "check_status") {
        auto bool_card = [this](const QString& label, bool ok) {
            return gs_make_card(label, ok ? "AVAILABLE" : "MISSING", this,
                                ok ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        };
        QList<QWidget*> deps = {
            bool_card("SKLEARN", d.value("sklearn").toBool()),
            bool_card("STATSMODELS", d.value("statsmodels").toBool()),
            bool_card("SCIPY", d.value("scipy").toBool()),
            gs_make_card("BACKEND", d.value("backend").toString().toUpper(), this),
        };
        results_layout_->addWidget(gs_card_row(deps, this));

        const auto ops = d.value("ops_available").toArray();
        QStringList op_names;
        for (const auto& v : ops) op_names << v.toString();
        auto* lbl = new QLabel(QString("Operations available: %1").arg(op_names.join(", ")));
        lbl->setWordWrap(true);
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(lbl);
        status_label_->setText("Functime backend ready");
        return;
    }

    // ── 2. FORECAST ──────────────────────────────────────────────────────────
    if (command == "forecast") {
        const QString model = d.value("model").toString();
        const int horizon = d.value("horizon").toInt();
        const int lags = d.value("lags").toInt();
        const double r2 = d.value("in_sample_r2").toDouble();
        const double mae = d.value("in_sample_mae").toDouble();
        const double rmse = d.value("in_sample_rmse").toDouble();
        const double resid_std = d.value("residual_std").toDouble();
        const double last_actual = d.value("last_actual").toDouble();
        const double first_fc = d.value("first_forecast").toDouble();
        const double last_fc = d.value("last_forecast").toDouble();

        auto* hdr = new QLabel(QString("MODEL %1  |  HORIZON %2 STEPS  |  LAGS %3")
                                   .arg(model.toUpper()).arg(horizon).arg(lags));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> fit = {
            gs_make_card("IN-SAMPLE R²", gs_fmt_num(r2, 3), this,
                         r2 >= 0.7 ? ui::colors::POSITIVE()
                                  : r2 >= 0.3 ? ui::colors::WARNING()
                                              : ui::colors::NEGATIVE()),
            gs_make_card("IN-SAMPLE MAE", gs_fmt_num(mae, 4), this),
            gs_make_card("IN-SAMPLE RMSE", gs_fmt_num(rmse, 4), this),
            gs_make_card("RESIDUAL σ", gs_fmt_num(resid_std, 4), this),
        };
        results_layout_->addWidget(gs_card_row(fit, this));

        const double drift = first_fc - last_actual;
        const double horizon_drift = last_fc - last_actual;
        QList<QWidget*> proj = {
            gs_make_card("LAST ACTUAL", gs_fmt_num(last_actual, 4), this),
            gs_make_card("FIRST FORECAST", gs_fmt_num(first_fc, 4), this, gs_pos_neg_color(drift)),
            gs_make_card("LAST FORECAST", gs_fmt_num(last_fc, 4), this, gs_pos_neg_color(horizon_drift)),
            gs_make_card("HORIZON Δ", gs_fmt_num(horizon_drift, 4), this, gs_pos_neg_color(horizon_drift)),
        };
        results_layout_->addWidget(gs_card_row(proj, this));

        QList<QWidget*> bounds = {
            gs_make_card("FORECAST MIN", gs_fmt_num(d.value("forecast_min").toDouble(), 4), this,
                         ui::colors::NEGATIVE()),
            gs_make_card("FORECAST MEAN", gs_fmt_num(d.value("forecast_mean").toDouble(), 4), this),
            gs_make_card("FORECAST MAX", gs_fmt_num(d.value("forecast_max").toDouble(), 4), this,
                         ui::colors::POSITIVE()),
            gs_make_card("SEASON USED", d.value("season").toInt() > 0
                                            ? QString::number(d.value("season").toInt())
                                            : QString("none"), this),
        };
        results_layout_->addWidget(gs_card_row(bounds, this));

        // Forecast table — every step
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            auto* table = new QTableWidget(forecast.size(), 3, this);
            table->setHorizontalHeaderLabels({"Step", "Date", "Forecast"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
            table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * 24 + 32, 320));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                const double v = o.value("value").toDouble();
                const double delta = v - last_actual;
                table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
                table->setItem(i, 1, new QTableWidgetItem(o.value("date").toString()));
                auto* val_item = new QTableWidgetItem(QString::number(v, 'f', 4));
                val_item->setForeground(QColor(gs_pos_neg_color(delta)));
                val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 2, val_item);
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("%1 trained — R²=%2  |  forecast %3 → %4")
                                   .arg(model)
                                   .arg(r2, 0, 'f', 3)
                                   .arg(first_fc, 0, 'f', 3)
                                   .arg(last_fc, 0, 'f', 3));
        return;
    }

    // ── 3. ANOMALY DETECTION ────────────────────────────────────────────────
    if (command == "anomaly_detection") {
        const QString method = d.value("method").toString();
        const int n_anom = d.value("n_anomalies").toInt();
        const double rate = d.value("anomaly_rate_pct").toDouble();
        const int n_obs = d.value("n_observations").toInt();

        auto* hdr = new QLabel(QString("METHOD %1  |  %2 / %3 ANOMALIES (%4%)")
                                   .arg(method.toUpper())
                                   .arg(n_anom).arg(n_obs)
                                   .arg(rate, 0, 'f', 2));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("ANOMALIES FOUND", QString::number(n_anom), this,
                         n_anom == 0 ? ui::colors::POSITIVE()
                                     : rate > 5  ? ui::colors::NEGATIVE()
                                                : ui::colors::WARNING()),
            gs_make_card("ANOMALY RATE", QString::number(rate, 'f', 2) + "%", this,
                         rate > 5 ? ui::colors::NEGATIVE()
                                  : rate > 1 ? ui::colors::WARNING()
                                             : ui::colors::POSITIVE()),
            gs_make_card("MAX |SCORE|", gs_fmt_num(std::max(std::abs(d.value("score_min").toDouble()),
                                                              std::abs(d.value("score_max").toDouble())), 3),
                         this, ui::colors::WARNING()),
            gs_make_card("SCORE σ", gs_fmt_num(d.value("score_std").toDouble(), 3), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> dist = {
            gs_make_card("OBSERVATIONS", QString::number(n_obs), this),
            gs_make_card("MIN SCORE", gs_fmt_num(d.value("score_min").toDouble(), 3), this),
            gs_make_card("MAX SCORE", gs_fmt_num(d.value("score_max").toDouble(), 3), this),
            gs_make_card("MEAN SCORE", gs_fmt_num(d.value("score_mean").toDouble(), 3), this),
        };
        results_layout_->addWidget(gs_card_row(dist, this));

        // Anomalies table (top 200, sorted by abs score)
        const QJsonArray anomalies = d.value("anomalies").toArray();
        if (!anomalies.isEmpty()) {
            auto* table = new QTableWidget(anomalies.size(), 4, this);
            table->setHorizontalHeaderLabels({"Index", "Date", "Value", "Score"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(anomalies.size() * 24 + 32, 360));
            for (int i = 0; i < anomalies.size(); ++i) {
                const auto o = anomalies[i].toObject();
                const double score = o.value("score").toDouble();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("index").toInt())));
                table->setItem(i, 1, new QTableWidgetItem(o.value("date").toString()));
                auto* val_item = new QTableWidgetItem(QString::number(o.value("value").toDouble(), 'f', 4));
                val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 2, val_item);
                auto* sc_item = new QTableWidgetItem(QString::number(score, 'f', 3));
                sc_item->setForeground(QColor(std::abs(score) > 5 ? ui::colors::NEGATIVE()
                                                                  : ui::colors::WARNING()));
                sc_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 3, sc_item);
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        } else {
            auto* clean = new QLabel("No anomalies detected at the configured threshold.");
            clean->setStyleSheet(QString("color:%1; font-size:11px; padding:10px;"
                                         "background:rgba(34,197,94,0.06); border:1px solid rgba(34,197,94,0.25); border-radius:2px;")
                                     .arg(ui::colors::POSITIVE()));
            results_layout_->addWidget(clean);
        }

        status_label_->setText(QString("%1: %2 anomalies in %3 obs (%4%)")
                                   .arg(method).arg(n_anom).arg(n_obs).arg(rate, 0, 'f', 2));
        return;
    }

    // ── 4. SEASONALITY ──────────────────────────────────────────────────────
    if (command == "seasonality") {
        const int period = d.value("period").toInt();
        const bool auto_det = d.value("period_auto_detected").toBool();
        const double tstr = d.value("trend_strength").toDouble();
        const double sstr = d.value("seasonal_strength").toDouble();
        const double rstd = d.value("residual_std").toDouble();

        auto* hdr = new QLabel(QString("PERIOD %1%2  |  STL DECOMPOSITION")
                                   .arg(period).arg(auto_det ? "  (auto-detected)" : ""));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        auto strength_color = [](double s) {
            if (s > 0.6) return ui::colors::POSITIVE();
            if (s > 0.3) return ui::colors::WARNING();
            return ui::colors::TEXT_PRIMARY();
        };

        QList<QWidget*> strengths = {
            gs_make_card("PERIOD", QString::number(period), this, ui::colors::INFO()),
            gs_make_card("TREND STRENGTH", gs_fmt_num(tstr, 3), this, strength_color(tstr)),
            gs_make_card("SEASONAL STRENGTH", gs_fmt_num(sstr, 3), this, strength_color(sstr)),
            gs_make_card("RESIDUAL σ", gs_fmt_num(rstd, 4), this),
        };
        results_layout_->addWidget(gs_card_row(strengths, this));

        // Interpretation strip
        QString interp;
        if (sstr > 0.6 && tstr > 0.6) interp = "Strong trend AND strong seasonality detected.";
        else if (sstr > 0.6) interp = "Strong seasonal cycle, weak trend.";
        else if (tstr > 0.6) interp = "Strong trend, weak/no seasonality.";
        else interp = "Weak structure — series is close to noise around its mean.";
        auto* il = new QLabel(interp);
        il->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                  "padding:8px 10px; background:%2; border-left:3px solid %3;")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(il);

        // Decomposition table — first/middle/last 5 of trend / seasonal / residual
        const QJsonArray trend = d.value("trend").toArray();
        const QJsonArray seasonal = d.value("seasonal").toArray();
        const QJsonArray residual = d.value("residual").toArray();
        const int rows = std::min<int>(15, trend.size());
        if (rows > 0) {
            auto* table = new QTableWidget(rows, 4, this);
            table->setHorizontalHeaderLabels({"Date", "Trend", "Seasonal", "Residual"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * 22 + 32);
            const int step = std::max<int>(1, trend.size() / rows);
            for (int i = 0, r = 0; i < trend.size() && r < rows; i += step, ++r) {
                const auto td = trend[i].toObject();
                const auto sd = seasonal.size() > i ? seasonal[i].toObject() : QJsonObject{};
                const auto rd = residual.size() > i ? residual[i].toObject() : QJsonObject{};
                table->setItem(r, 0, new QTableWidgetItem(td.value("date").toString()));
                auto add_num = [&](int col, double v) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(r, col, it);
                };
                add_num(1, td.value("value").toDouble());
                add_num(2, sd.value("value").toDouble());
                add_num(3, rd.value("value").toDouble());
                table->setRowHeight(r, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("STL period=%1  |  trend=%2  seasonal=%3")
                                   .arg(period).arg(tstr, 0, 'f', 2).arg(sstr, 0, 'f', 2));
        return;
    }

    // ── 5. METRICS ───────────────────────────────────────────────────────────
    if (command == "metrics") {
        const double mae = d.value("mae").toDouble();
        const double rmse = d.value("rmse").toDouble();
        const double r2 = d.value("r_squared").toDouble();
        const double mape = d.value("mape_pct").toDouble();
        const double smape = d.value("smape_pct").toDouble();
        const double bias = d.value("bias").toDouble();
        const double dir_acc = d.value("direction_accuracy_pct").toDouble();

        QList<QWidget*> err_row = {
            gs_make_card("MAE", gs_fmt_num(mae, 4), this),
            gs_make_card("RMSE", gs_fmt_num(rmse, 4), this),
            gs_make_card("MSE", gs_fmt_num(d.value("mse").toDouble(), 4), this),
            gs_make_card("BIAS", gs_fmt_num(bias, 4), this, gs_pos_neg_color(-std::abs(bias))),
        };
        results_layout_->addWidget(gs_card_row(err_row, this));

        QList<QWidget*> pct_row = {
            gs_make_card("MAPE", QString::number(mape, 'f', 2) + "%", this,
                         mape < 10 ? ui::colors::POSITIVE()
                                  : mape < 30 ? ui::colors::WARNING()
                                              : ui::colors::NEGATIVE()),
            gs_make_card("SMAPE", QString::number(smape, 'f', 2) + "%", this,
                         smape < 10 ? ui::colors::POSITIVE()
                                   : smape < 30 ? ui::colors::WARNING()
                                                : ui::colors::NEGATIVE()),
            gs_make_card("R²", gs_fmt_num(r2, 4), this,
                         r2 > 0.7 ? ui::colors::POSITIVE()
                                  : r2 > 0.3 ? ui::colors::WARNING()
                                             : ui::colors::NEGATIVE()),
            gs_make_card("DIRECTION ACC", QString::number(dir_acc, 'f', 1) + "%", this,
                         dir_acc > 60 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(pct_row, this));

        status_label_->setText(QString("MAE %1  |  RMSE %2  |  R² %3  |  MAPE %4%")
                                   .arg(mae, 0, 'f', 3).arg(rmse, 0, 'f', 3)
                                   .arg(r2, 0, 'f', 3).arg(mape, 0, 'f', 2));
        return;
    }

    // ── 6. CONFIDENCE INTERVALS ──────────────────────────────────────────────
    if (command == "confidence_intervals") {
        const QString method = d.value("method").toString();
        const int horizon = d.value("horizon").toInt();
        const double conf = d.value("confidence").toDouble();
        const double mean_w = d.value("mean_width").toDouble();

        auto* hdr = new QLabel(QString("METHOD %1  |  CONFIDENCE %2%  |  HORIZON %3")
                                   .arg(method.toUpper()).arg(conf * 100, 0, 'f', 1).arg(horizon));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("RESIDUAL σ", gs_fmt_num(d.value("residual_std").toDouble(), 4), this),
            gs_make_card("MEAN WIDTH", gs_fmt_num(mean_w, 4), this, ui::colors::WARNING()),
            gs_make_card("FIRST LOWER", gs_fmt_num(d.value("first_lower").toDouble(), 4), this,
                         ui::colors::NEGATIVE()),
            gs_make_card("FIRST UPPER", gs_fmt_num(d.value("first_upper").toDouble(), 4), this,
                         ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> wide = {
            gs_make_card("LAST LOWER", gs_fmt_num(d.value("last_lower").toDouble(), 4), this,
                         ui::colors::NEGATIVE()),
            gs_make_card("LAST UPPER", gs_fmt_num(d.value("last_upper").toDouble(), 4), this,
                         ui::colors::POSITIVE()),
            gs_make_card("LAGS", QString::number(d.value("lags").toInt()), this),
            gs_make_card("BOOT N",
                         d.value("n_boot").isNull() ? QString("n/a")
                                                    : QString::number(d.value("n_boot").toInt()),
                         this),
        };
        results_layout_->addWidget(gs_card_row(wide, this));

        const QJsonArray intervals = d.value("intervals").toArray();
        if (!intervals.isEmpty()) {
            auto* table = new QTableWidget(intervals.size(), 5, this);
            table->setHorizontalHeaderLabels({"Step", "Date", "Lower", "Point", "Upper"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
            table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(intervals.size() * 24 + 32, 320));
            for (int i = 0; i < intervals.size(); ++i) {
                const auto o = intervals[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
                table->setItem(i, 1, new QTableWidgetItem(o.value("date").toString()));
                auto* lo = new QTableWidgetItem(QString::number(o.value("lower").toDouble(), 'f', 4));
                lo->setForeground(QColor(ui::colors::NEGATIVE()));
                lo->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 2, lo);
                auto* pt = new QTableWidgetItem(QString::number(o.value("point").toDouble(), 'f', 4));
                pt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 3, pt);
                auto* up = new QTableWidgetItem(QString::number(o.value("upper").toDouble(), 'f', 4));
                up->setForeground(QColor(ui::colors::POSITIVE()));
                up->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 4, up);
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("%1 intervals @ %2%  |  mean width %3")
                                   .arg(intervals.size()).arg(conf * 100, 0, 'f', 1)
                                   .arg(mean_w, 0, 'f', 3));
        return;
    }

    // ── 7. STATIONARITY ──────────────────────────────────────────────────────
    if (command == "stationarity") {
        const int rec_d = d.value("recommended_d").toInt();
        const int max_d = d.value("max_d_tested").toInt();
        const QJsonArray tests = d.value("tests").toArray();

        auto* hdr = new QLabel(QString("RECOMMENDED DIFFERENCING ORDER:  d = %1   (tested 0..%2)")
                                   .arg(rec_d).arg(max_d));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        // One card row per tested d
        for (const auto& v : tests) {
            const auto t = v.toObject();
            const int dval = t.value("differencing_order").toInt();
            const bool both_stat = t.value("both_stationary").toBool();
            const QString label = QString("d = %1").arg(dval);
            const QString verdict = both_stat ? "STATIONARY" : "NON-STATIONARY";
            const QString verdict_col = both_stat ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();

            QList<QWidget*> row = {
                gs_make_card(label.toUpper(), verdict, this, verdict_col),
                gs_make_card("ADF p", gs_fmt_num(t.value("adf_p_value").toDouble(), 4), this,
                             t.value("adf_stationary").toBool() ? ui::colors::POSITIVE()
                                                                : ui::colors::WARNING()),
                gs_make_card("KPSS p", gs_fmt_num(t.value("kpss_p_value").toDouble(), 4), this,
                             t.value("kpss_stationary").toBool() ? ui::colors::POSITIVE()
                                                                  : ui::colors::WARNING()),
                gs_make_card("OBS USED", QString::number(t.value("n_observations").toInt()), this),
            };
            results_layout_->addWidget(gs_card_row(row, this));
        }

        // Test legend
        auto* legend = new QLabel(
            "ADF: H₀ = unit root (non-stationary). p < 0.05 ⇒ stationary.\n"
            "KPSS: H₀ = stationary. p ≥ 0.05 ⇒ stationary.\n"
            "Verdict requires BOTH tests to agree.");
        legend->setStyleSheet(QString("color:%1; font-size:9px; font-family:'Courier New';"
                                      "padding:6px 10px; background:%2;")
                                  .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED()));
        results_layout_->addWidget(legend);

        status_label_->setText(QString("Recommended d = %1 (tested %2 orders)")
                                   .arg(rec_d).arg(tests.size()));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
