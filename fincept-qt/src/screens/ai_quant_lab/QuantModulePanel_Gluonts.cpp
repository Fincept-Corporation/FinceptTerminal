// src/screens/ai_quant_lab/QuantModulePanel_Gluonts.cpp
//
// Gluonts panel — 5 probabilistic-forecasting modes (probabilistic forecast,
// quantile forecast, distribution fit, evaluate forecast, seasonal naive).
// Mirrors GS Quant / Functime / Statsmodels / Fortitudo / Quant Reporting.
// Extracted from QuantModulePanel.cpp to keep that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
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
#include <limits>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

// ═══════════════════════════════════════════════════════════════════════════════
// GLUONTS PANEL — 5 probabilistic-forecasting modes (probabilistic forecast,
// quantile forecast, distribution fit, evaluate forecast, seasonal naive).
// Mirrors GS Quant / Functime / Statsmodels / Fortitudo / Quant Reporting.
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_gluonts_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));

    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed,
                                  const QString& tip) -> QPushButton* {
        auto* btn = new QPushButton("LOAD SAMPLE", parent);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(22);
        btn->setToolTip(tip);
        btn->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %2;"
                                   "padding:0 8px; font-size:9px; font-weight:700; letter-spacing:0.5px; border-radius:2px; }"
                                   "QPushButton:hover { color:%3; border-color:%3; background:rgba(255,255,255,0.04); }")
                               .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), module_.color.name()));
        QPointer<QLineEdit> guard(edit);
        connect(btn, &QPushButton::clicked, this, [guard, seed]() {
            if (guard) guard->setText(sample_series(seed));
        });
        return btn;
    };

    // ── Probabilistic Forecast ───────────────────────────────────────────────
    auto* pf = new QWidget(this);
    auto* pfl = new QVBoxLayout(pf);
    pfl->setContentsMargins(12, 12, 12, 12);
    pfl->setSpacing(8);

    auto* pf_vals = new QLineEdit(pf);
    pf_vals->setPlaceholderText("Series values (>= 30). Bootstrap residual ensemble forecasts the next H steps with quantile bands.");
    pf_vals->setStyleSheet(input_ss());
    text_inputs_["gn_pf_values"] = pf_vals;
    pfl->addWidget(build_input_row("Series Values", pf_vals, pf));
    pfl->addWidget(add_sample_btn(pf_vals, pf, 311, "200-pt synthetic series with seasonality"));

    auto* pf_horizon = new QSpinBox(pf);
    pf_horizon->setRange(1, 365);
    pf_horizon->setValue(14);
    pf_horizon->setStyleSheet(spinbox_ss());
    int_inputs_["gn_pf_horizon"] = pf_horizon;
    pfl->addWidget(build_input_row("Forecast Horizon", pf_horizon, pf));

    auto* pf_lags = new QSpinBox(pf);
    pf_lags->setRange(1, 60);
    pf_lags->setValue(7);
    pf_lags->setStyleSheet(spinbox_ss());
    int_inputs_["gn_pf_lags"] = pf_lags;
    pfl->addWidget(build_input_row("AR Lag Window", pf_lags, pf));

    auto* pf_npaths = new QSpinBox(pf);
    pf_npaths->setRange(100, 5000);
    pf_npaths->setSingleStep(100);
    pf_npaths->setValue(500);
    pf_npaths->setStyleSheet(spinbox_ss());
    int_inputs_["gn_pf_n_paths"] = pf_npaths;
    pfl->addWidget(build_input_row("Bootstrap Paths", pf_npaths, pf));

    auto* pf_run = make_run_button("RUN PROBABILISTIC FORECAST", pf);
    connect(pf_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["gn_pf_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 30) {
            display_error(QString("Need at least 30 obs; you provided %1.").arg(vals.size()));
            return;
        }
        show_loading(QString("Bootstrapping %1 paths over %2 steps...")
                         .arg(int_inputs_["gn_pf_n_paths"]->value())
                         .arg(int_inputs_["gn_pf_horizon"]->value()));
        QJsonObject params;
        params["values"] = vals;
        params["horizon"] = int_inputs_["gn_pf_horizon"]->value();
        params["lags"] = int_inputs_["gn_pf_lags"]->value();
        params["n_paths"] = int_inputs_["gn_pf_n_paths"]->value();
        AIQuantLabService::instance().gluon_probabilistic_forecast(params);
    });
    pfl->addWidget(pf_run);
    pfl->addStretch();
    tabs->addTab(pf, "Probabilistic Forecast");

    // ── Quantile Forecast ────────────────────────────────────────────────────
    auto* qf = new QWidget(this);
    auto* qfl = new QVBoxLayout(qf);
    qfl->setContentsMargins(12, 12, 12, 12);
    qfl->setSpacing(8);

    auto* qf_vals = new QLineEdit(qf);
    qf_vals->setPlaceholderText("Series values (>= 30)");
    qf_vals->setStyleSheet(input_ss());
    text_inputs_["gn_qf_values"] = qf_vals;
    qfl->addWidget(build_input_row("Series Values", qf_vals, qf));
    qfl->addWidget(add_sample_btn(qf_vals, qf, 321, "200-pt synthetic series"));

    auto* qf_quantiles = new QLineEdit(qf);
    qf_quantiles->setPlaceholderText("Quantiles in (0, 1) — e.g. 0.05, 0.5, 0.95");
    qf_quantiles->setText("0.05, 0.25, 0.5, 0.75, 0.95");
    qf_quantiles->setStyleSheet(input_ss());
    text_inputs_["gn_qf_quantiles"] = qf_quantiles;
    qfl->addWidget(build_input_row("Quantile List", qf_quantiles, qf));

    auto* qf_horizon = new QSpinBox(qf);
    qf_horizon->setRange(1, 365);
    qf_horizon->setValue(14);
    qf_horizon->setStyleSheet(spinbox_ss());
    int_inputs_["gn_qf_horizon"] = qf_horizon;
    qfl->addWidget(build_input_row("Forecast Horizon", qf_horizon, qf));

    auto* qf_lags = new QSpinBox(qf);
    qf_lags->setRange(1, 60);
    qf_lags->setValue(7);
    qf_lags->setStyleSheet(spinbox_ss());
    int_inputs_["gn_qf_lags"] = qf_lags;
    qfl->addWidget(build_input_row("AR Lag Window", qf_lags, qf));

    auto* qf_npaths = new QSpinBox(qf);
    qf_npaths->setRange(100, 5000);
    qf_npaths->setSingleStep(100);
    qf_npaths->setValue(500);
    qf_npaths->setStyleSheet(spinbox_ss());
    int_inputs_["gn_qf_n_paths"] = qf_npaths;
    qfl->addWidget(build_input_row("Bootstrap Paths", qf_npaths, qf));

    auto* qf_run = make_run_button("RUN QUANTILE FORECAST", qf);
    connect(qf_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals, quants;
        QString bad;
        if (!parse_doubles(text_inputs_["gn_qf_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 30) {
            display_error(QString("Need at least 30 obs; you provided %1.").arg(vals.size()));
            return;
        }
        if (!parse_doubles(text_inputs_["gn_qf_quantiles"]->text(), quants, &bad)) {
            display_error(QString("Quantiles: '%1' is not numeric.").arg(bad));
            return;
        }
        if (quants.isEmpty()) {
            display_error("Provide at least one quantile in (0, 1).");
            return;
        }
        for (const auto& v : quants) {
            const double q = v.toDouble();
            if (!(0.0 < q && q < 1.0)) {
                display_error(QString("Quantile %1 must be in (0, 1).").arg(q));
                return;
            }
        }
        show_loading(QString("Bootstrapping %1 paths and computing %2 quantiles...")
                         .arg(int_inputs_["gn_qf_n_paths"]->value()).arg(quants.size()));
        QJsonObject params;
        params["values"] = vals;
        params["quantiles"] = quants;
        params["horizon"] = int_inputs_["gn_qf_horizon"]->value();
        params["lags"] = int_inputs_["gn_qf_lags"]->value();
        params["n_paths"] = int_inputs_["gn_qf_n_paths"]->value();
        AIQuantLabService::instance().gluon_quantile_forecast(params);
    });
    qfl->addWidget(qf_run);
    qfl->addStretch();
    tabs->addTab(qf, "Quantile Forecast");

    // ── Distribution Fit ─────────────────────────────────────────────────────
    auto* df = new QWidget(this);
    auto* dfl = new QVBoxLayout(df);
    dfl->setContentsMargins(12, 12, 12, 12);
    dfl->setSpacing(8);

    auto* df_vals = new QLineEdit(df);
    df_vals->setPlaceholderText("Numeric values (>= 30). Fits normal, student-t, lognormal (positive only), skewnormal.");
    df_vals->setStyleSheet(input_ss());
    text_inputs_["gn_df_values"] = df_vals;
    dfl->addWidget(build_input_row("Values", df_vals, df));
    dfl->addWidget(add_sample_btn(df_vals, df, 331, "200-pt synthetic series"));

    auto* df_hint = new QLabel(
        "Fits 4 candidate distributions and ranks them by AIC + BIC. KS test reports whether each "
        "fit can be rejected at the 5% level — useful for picking the right tail model.", df);
    df_hint->setWordWrap(true);
    df_hint->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    dfl->addWidget(df_hint);

    auto* df_run = make_run_button("FIT DISTRIBUTIONS", df);
    connect(df_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["gn_df_values"]->text(), vals, &bad)) {
            display_error(QString("Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 30) {
            display_error(QString("Need at least 30 values; you provided %1.").arg(vals.size()));
            return;
        }
        show_loading(QString("Fitting 4 distributions to %1 values...").arg(vals.size()));
        QJsonObject params;
        params["values"] = vals;
        AIQuantLabService::instance().gluon_distribution_fit(params);
    });
    dfl->addWidget(df_run);
    dfl->addStretch();
    tabs->addTab(df, "Distribution Fit");

    // ── Evaluate Forecast ────────────────────────────────────────────────────
    auto* ef = new QWidget(this);
    auto* efl = new QVBoxLayout(ef);
    efl->setContentsMargins(12, 12, 12, 12);
    efl->setSpacing(8);

    auto* ef_actuals = new QLineEdit(ef);
    ef_actuals->setPlaceholderText("Realized actuals (>= 5 values)");
    ef_actuals->setStyleSheet(input_ss());
    text_inputs_["gn_ef_actuals"] = ef_actuals;
    efl->addWidget(build_input_row("Actuals", ef_actuals, ef));
    efl->addWidget(add_sample_btn(ef_actuals, ef, 341, "100-pt synthetic actuals"));

    auto* ef_point = new QLineEdit(ef);
    ef_point->setPlaceholderText("Point forecast (same length as actuals)");
    ef_point->setStyleSheet(input_ss());
    text_inputs_["gn_ef_point"] = ef_point;
    efl->addWidget(build_input_row("Point Forecast", ef_point, ef));
    efl->addWidget(add_sample_btn(ef_point, ef, 342, "100-pt synthetic point forecast"));

    auto* ef_lower = new QLineEdit(ef);
    ef_lower->setPlaceholderText("Lower band (optional, same length as actuals)");
    ef_lower->setStyleSheet(input_ss());
    text_inputs_["gn_ef_lower"] = ef_lower;
    efl->addWidget(build_input_row("Lower Band (optional)", ef_lower, ef));

    auto* ef_upper = new QLineEdit(ef);
    ef_upper->setPlaceholderText("Upper band (optional, same length as actuals)");
    ef_upper->setStyleSheet(input_ss());
    text_inputs_["gn_ef_upper"] = ef_upper;
    efl->addWidget(build_input_row("Upper Band (optional)", ef_upper, ef));

    auto* ef_training = new QLineEdit(ef);
    ef_training->setPlaceholderText("Training history (optional; enables MASE)");
    ef_training->setStyleSheet(input_ss());
    text_inputs_["gn_ef_training"] = ef_training;
    efl->addWidget(build_input_row("Training History (optional)", ef_training, ef));
    efl->addWidget(add_sample_btn(ef_training, ef, 343, "200-pt synthetic training history"));

    auto* ef_season = new QSpinBox(ef);
    ef_season->setRange(1, 365);
    ef_season->setValue(1);
    ef_season->setStyleSheet(spinbox_ss());
    int_inputs_["gn_ef_season"] = ef_season;
    efl->addWidget(build_input_row("MASE Seasonal Period", ef_season, ef));

    auto* ef_target = make_double_spin(50.0, 99.9, 80.0, 1, "%", ef);
    double_inputs_["gn_ef_target"] = ef_target;
    efl->addWidget(build_input_row("Coverage Target", ef_target, ef));

    auto* ef_run = make_run_button("EVALUATE FORECAST", ef);
    connect(ef_run, &QPushButton::clicked, this, [this]() {
        QJsonArray actuals, point;
        QString bad;
        if (!parse_doubles(text_inputs_["gn_ef_actuals"]->text(), actuals, &bad)) {
            display_error(QString("Actuals: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["gn_ef_point"]->text(), point, &bad)) {
            display_error(QString("Point: '%1' is not numeric.").arg(bad));
            return;
        }
        if (actuals.size() < 5) {
            display_error(QString("Need at least 5 actuals; you provided %1.").arg(actuals.size()));
            return;
        }
        if (actuals.size() != point.size()) {
            display_error(QString("Actuals (%1) and point (%2) must have the same length.")
                              .arg(actuals.size()).arg(point.size()));
            return;
        }
        QJsonObject params;
        params["actuals"] = actuals;
        params["point"] = point;
        params["coverage_target_pct"] = double_inputs_["gn_ef_target"]->value();

        const QString lo_text = text_inputs_["gn_ef_lower"]->text().trimmed();
        const QString hi_text = text_inputs_["gn_ef_upper"]->text().trimmed();
        if (!lo_text.isEmpty() || !hi_text.isEmpty()) {
            QJsonArray lower, upper;
            if (!parse_doubles(lo_text, lower, &bad)) {
                display_error(QString("Lower band: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(hi_text, upper, &bad)) {
                display_error(QString("Upper band: '%1' is not numeric.").arg(bad));
                return;
            }
            if (lower.size() != actuals.size() || upper.size() != actuals.size()) {
                display_error("Lower and upper bands must match actuals length.");
                return;
            }
            params["lower"] = lower;
            params["upper"] = upper;
        }

        const QString train_text = text_inputs_["gn_ef_training"]->text().trimmed();
        if (!train_text.isEmpty()) {
            QJsonArray training;
            if (!parse_doubles(train_text, training, &bad)) {
                display_error(QString("Training: '%1' is not numeric.").arg(bad));
                return;
            }
            params["training"] = training;
            params["season"] = int_inputs_["gn_ef_season"]->value();
        }

        show_loading(QString("Evaluating forecast on %1 obs...").arg(actuals.size()));
        AIQuantLabService::instance().gluon_evaluate_forecast(params);
    });
    efl->addWidget(ef_run);
    efl->addStretch();
    tabs->addTab(ef, "Evaluate Forecast");

    // ── Seasonal Naive ───────────────────────────────────────────────────────
    auto* sn = new QWidget(this);
    auto* snl = new QVBoxLayout(sn);
    snl->setContentsMargins(12, 12, 12, 12);
    snl->setSpacing(8);

    auto* sn_vals = new QLineEdit(sn);
    sn_vals->setPlaceholderText("Series values (>= 1). Forecast = repeat last `season_length` observations.");
    sn_vals->setStyleSheet(input_ss());
    text_inputs_["gn_sn_values"] = sn_vals;
    snl->addWidget(build_input_row("Series Values", sn_vals, sn));
    snl->addWidget(add_sample_btn(sn_vals, sn, 351, "200-pt synthetic series"));

    auto* sn_horizon = new QSpinBox(sn);
    sn_horizon->setRange(1, 365);
    sn_horizon->setValue(14);
    sn_horizon->setStyleSheet(spinbox_ss());
    int_inputs_["gn_sn_horizon"] = sn_horizon;
    snl->addWidget(build_input_row("Forecast Horizon", sn_horizon, sn));

    auto* sn_season = new QSpinBox(sn);
    sn_season->setRange(1, 365);
    sn_season->setValue(1);
    sn_season->setStyleSheet(spinbox_ss());
    int_inputs_["gn_sn_season"] = sn_season;
    snl->addWidget(build_input_row("Season Length (1 = pure naive)", sn_season, sn));

    auto* sn_hint = new QLabel(
        "Baseline forecaster everyone compares against. season_length=1 is the pure naive 'repeat last value'. "
        "Larger values cycle through the most recent N observations.", sn);
    sn_hint->setWordWrap(true);
    sn_hint->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    snl->addWidget(sn_hint);

    auto* sn_run = make_run_button("RUN SEASONAL NAIVE", sn);
    connect(sn_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["gn_sn_values"]->text(), vals, &bad)) {
            display_error(QString("Series Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.isEmpty()) {
            display_error("Provide at least 1 observation.");
            return;
        }
        const int season = int_inputs_["gn_sn_season"]->value();
        if (season > vals.size()) {
            display_error(QString("Season length (%1) exceeds series length (%2).")
                              .arg(season).arg(vals.size()));
            return;
        }
        show_loading(QString("Repeating last %1 obs forward over %2 steps...")
                         .arg(season).arg(int_inputs_["gn_sn_horizon"]->value()));
        QJsonObject params;
        params["values"] = vals;
        params["horizon"] = int_inputs_["gn_sn_horizon"]->value();
        params["season_length"] = season;
        AIQuantLabService::instance().gluon_seasonal_naive(params);
    });
    snl->addWidget(sn_run);
    snl->addStretch();
    tabs->addTab(sn, "Seasonal Naive");

    vl->addWidget(tabs);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);

    return w;
}

// ── GLUONTS: rich card-based display per command ────────────────────────────

void QuantModulePanel::display_gluonts_result(const QString& command, const QJsonObject& payload) {
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
            bool_card("SCIPY", d.value("scipy").toBool()),
            bool_card("SKLEARN", d.value("sklearn").toBool()),
            gs_make_card("BACKEND", d.value("backend").toString().toUpper(), this),
            gs_make_card("OPS", QString::number(d.value("ops_available").toArray().size()),
                         this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(deps, this));

        const auto note = d.value("note").toString();
        if (!note.isEmpty()) {
            auto* lbl = new QLabel(note);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                       "padding:8px 10px; background:%2; border:1px solid %3;")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                        ui::colors::BORDER_DIM()));
            results_layout_->addWidget(lbl);
        }
        status_label_->setText("GluonTS backend ready");
        return;
    }

    // ── 2. PROBABILISTIC FORECAST ───────────────────────────────────────────
    if (command == "probabilistic_forecast") {
        const int horizon = d.value("horizon").toInt();
        const int n_paths = d.value("n_paths").toInt();
        const double last_actual = d.value("last_actual").toDouble();
        const double first_p50 = d.value("first_forecast_p50").toDouble();
        const double last_p50 = d.value("last_forecast_p50").toDouble();
        const double drift = d.value("horizon_drift").toDouble();
        const double avg_w = d.value("mean_p80_width").toDouble();
        const double first_w = d.value("first_p80_width").toDouble();
        const double last_w = d.value("last_p80_width").toDouble();

        auto* hdr = new QLabel(QString("HORIZON %1 STEPS  |  %2 BOOTSTRAP PATHS  |  LAGS %3")
                                   .arg(horizon).arg(n_paths).arg(d.value("lags").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("LAST ACTUAL", gs_fmt_num(last_actual, 4), this),
            gs_make_card("FIRST P50", gs_fmt_num(first_p50, 4), this,
                         gs_pos_neg_color(first_p50 - last_actual)),
            gs_make_card("LAST P50", gs_fmt_num(last_p50, 4), this, gs_pos_neg_color(drift)),
            gs_make_card("HORIZON Δ", gs_fmt_num(drift, 4), this, gs_pos_neg_color(drift)),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> width = {
            gs_make_card("AVG P80 WIDTH", gs_fmt_num(avg_w, 4), this, ui::colors::WARNING()),
            gs_make_card("FIRST P80 WIDTH", gs_fmt_num(first_w, 4), this),
            gs_make_card("LAST P80 WIDTH", gs_fmt_num(last_w, 4), this,
                         last_w > first_w * 2 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card("RESIDUAL σ", gs_fmt_num(d.value("residual_std").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(width, this));

        // Forecast table — every step with all 5 quantiles
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            auto* table = new QTableWidget(forecast.size(), 8, this);
            table->setHorizontalHeaderLabels({"Step", "P10", "P25", "P50", "Mean", "P75", "P90", "P80 Width"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * 22 + 32, 320));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("step").toInt())));
                auto add = [&](int col, double v, const QString& col_color = {}) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    if (!col_color.isEmpty()) it->setForeground(QColor(col_color));
                    table->setItem(i, col, it);
                };
                add(1, o.value("p10").toDouble(), ui::colors::NEGATIVE());
                add(2, o.value("p25").toDouble(), ui::colors::WARNING());
                add(3, o.value("p50").toDouble());
                add(4, o.value("mean").toDouble(), ui::colors::INFO());
                add(5, o.value("p75").toDouble(), ui::colors::WARNING());
                add(6, o.value("p90").toDouble(), ui::colors::POSITIVE());
                add(7, o.value("width_80").toDouble());
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("Forecast: P50 %1 → %2  |  P80 width %3 → %4")
                                   .arg(first_p50, 0, 'f', 3).arg(last_p50, 0, 'f', 3)
                                   .arg(first_w, 0, 'f', 3).arg(last_w, 0, 'f', 3));
        return;
    }

    // ── 3. QUANTILE FORECAST ────────────────────────────────────────────────
    if (command == "quantile_forecast") {
        const int horizon = d.value("horizon").toInt();
        const int n_q = d.value("n_quantiles").toInt();

        auto* hdr = new QLabel(QString("HORIZON %1 STEPS  |  %2 QUANTILES  |  %3 BOOTSTRAP PATHS")
                                   .arg(horizon).arg(n_q).arg(d.value("n_paths").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("LAST ACTUAL", gs_fmt_num(d.value("last_actual").toDouble(), 4), this),
            gs_make_card("LAGS", QString::number(d.value("lags").toInt()), this),
            gs_make_card("RESIDUAL σ", gs_fmt_num(d.value("residual_std").toDouble(), 4), this),
            gs_make_card("OBSERVATIONS", QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-quantile summary cards (up to 4 at a time)
        const QJsonArray q_summary = d.value("quantile_summary").toArray();
        if (!q_summary.isEmpty()) {
            auto* lbl = new QLabel("PER-QUANTILE SUMMARY");
            lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:4px 0 0 2px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(lbl);
            auto* table = new QTableWidget(q_summary.size(), 6, this);
            table->setHorizontalHeaderLabels({"Quantile", "Mean", "Min", "Max", "First", "Last"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(q_summary.size() * 24 + 32, 240));
            for (int i = 0; i < q_summary.size(); ++i) {
                const auto q = q_summary[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(q.value("label").toString()));
                auto add = [&](int col, double v) {
                    auto* it = new QTableWidgetItem(QString::number(v, 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, col, it);
                };
                add(1, q.value("mean").toDouble());
                add(2, q.value("min").toDouble());
                add(3, q.value("max").toDouble());
                add(4, q.value("first").toDouble());
                add(5, q.value("last").toDouble());
                table->setRowHeight(i, 24);
            }
            results_layout_->addWidget(table);
        }

        // Per-step forecast table — show first quantile row to derive headers
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            const auto first = forecast[0].toObject();
            QStringList headers = {"Step"};
            QStringList keys;
            for (auto it = first.begin(); it != first.end(); ++it) {
                if (it.key() == "step") continue;
                keys << it.key();
                headers << it.key().toUpper();
            }
            auto* lbl = new QLabel("FORECAST BY STEP");
            lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; padding:4px 0 0 2px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(lbl);
            auto* table = new QTableWidget(forecast.size(), headers.size(), this);
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * 22 + 32, 280));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("step").toInt())));
                for (int c = 0; c < keys.size(); ++c) {
                    auto* it = new QTableWidgetItem(
                        QString::number(o.value(keys[c]).toDouble(), 'f', 4));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, c + 1, it);
                }
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 quantiles over %2 steps").arg(n_q).arg(horizon));
        return;
    }

    // ── 4. DISTRIBUTION FIT ─────────────────────────────────────────────────
    if (command == "distribution_fit") {
        const QString best_aic = d.value("best_by_aic").toString();
        const QString best_bic = d.value("best_by_bic").toString();

        auto* hdr = new QLabel(QString("BEST BY AIC: %1   |   BEST BY BIC: %2")
                                   .arg(best_aic.toUpper()).arg(best_bic.toUpper()));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::POSITIVE()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> stats = {
            gs_make_card("MEAN", gs_fmt_num(d.value("data_mean").toDouble(), 4), this),
            gs_make_card("STD", gs_fmt_num(d.value("data_std").toDouble(), 4), this),
            gs_make_card("SKEW", gs_fmt_num(d.value("data_skew").toDouble(), 3), this,
                         gs_pos_neg_color(d.value("data_skew").toDouble())),
            gs_make_card("KURT", gs_fmt_num(d.value("data_kurtosis").toDouble(), 3), this),
        };
        results_layout_->addWidget(gs_card_row(stats, this));

        // Fits table
        const QJsonArray fits = d.value("fits").toArray();
        if (!fits.isEmpty()) {
            auto* table = new QTableWidget(fits.size(), 6, this);
            table->setHorizontalHeaderLabels({"Distribution", "Params", "Log-Lik", "AIC", "BIC", "KS p"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(fits.size() * 26 + 32, 240));

            // Find min AIC for highlighting
            double min_aic = std::numeric_limits<double>::infinity();
            for (const auto& v : fits) {
                const auto f = v.toObject();
                if (!f.value("aic").isNull()) {
                    const double a = f.value("aic").toDouble();
                    if (a < min_aic) min_aic = a;
                }
            }

            for (int i = 0; i < fits.size(); ++i) {
                const auto f = fits[i].toObject();
                const QString name = f.value("distribution").toString();
                const bool is_best = (!f.value("aic").isNull() &&
                                      std::abs(f.value("aic").toDouble() - min_aic) < 1e-9);
                auto* nm = new QTableWidgetItem(is_best ? name + "  ★" : name);
                if (is_best) nm->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(i, 0, nm);
                table->setItem(i, 1, new QTableWidgetItem(QString::number(f.value("n_params").toInt())));
                auto add = [&](int col, const QJsonValue& v, int dec) {
                    auto* it = v.isNull()
                                   ? new QTableWidgetItem("—")
                                   : new QTableWidgetItem(QString::number(v.toDouble(), 'f', dec));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, col, it);
                };
                add(2, f.value("log_likelihood"), 2);
                add(3, f.value("aic"), 2);
                add(4, f.value("bic"), 2);
                const double ks_p = f.value("ks_p_value").toDouble();
                auto* ksi = new QTableWidgetItem(QString::number(ks_p, 'f', 4));
                ksi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ksi->setForeground(QColor(f.value("cannot_reject_at_5pct").toBool()
                                              ? ui::colors::POSITIVE()
                                              : ui::colors::WARNING()));
                table->setItem(i, 5, ksi);
                table->setRowHeight(i, 26);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("Best fit: %1 (AIC) / %2 (BIC)").arg(best_aic).arg(best_bic));
        return;
    }

    // ── 5. EVALUATE FORECAST ────────────────────────────────────────────────
    if (command == "evaluate_forecast") {
        const double mae = d.value("mae").toDouble();
        const double rmse = d.value("rmse").toDouble();
        const double mape = d.value("mape_pct").toDouble();
        const double smape = d.value("smape_pct").toDouble();
        const double bias = d.value("bias").toDouble();
        const double dir_acc = d.value("direction_accuracy_pct").toDouble();
        const bool has_ci = d.value("has_intervals").toBool();

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
            gs_make_card("MASE",
                         d.value("mase").isNull() ? QString("—")
                                                  : gs_fmt_num(d.value("mase").toDouble(), 3),
                         this,
                         d.value("mase").isNull() ? ui::colors::TEXT_PRIMARY()
                             : (d.value("mase").toDouble() < 1.0 ? ui::colors::POSITIVE()
                                                                  : ui::colors::WARNING())),
            gs_make_card("DIRECTION ACC", QString::number(dir_acc, 'f', 1) + "%", this,
                         dir_acc > 60 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(pct_row, this));

        if (has_ci) {
            const double cov = d.value("coverage_pct").toDouble();
            const double target = d.value("coverage_target_pct").toDouble();
            const double cov_gap = cov - target;
            QList<QWidget*> ci_row = {
                gs_make_card("COVERAGE", QString::number(cov, 'f', 1) + "%", this,
                             std::abs(cov_gap) < 5.0 ? ui::colors::POSITIVE()
                                                      : ui::colors::WARNING()),
                gs_make_card("TARGET", QString::number(target, 'f', 1) + "%", this, ui::colors::INFO()),
                gs_make_card("AVG WIDTH", gs_fmt_num(d.value("avg_interval_width").toDouble(), 4), this),
                gs_make_card("INTERVAL SCORE",
                             gs_fmt_num(d.value("interval_score").toDouble(), 4), this,
                             ui::colors::INFO()),
            };
            results_layout_->addWidget(gs_card_row(ci_row, this));

            // Interpretation
            QString verdict;
            QString verdict_col = ui::colors::TEXT_PRIMARY();
            if (std::abs(cov_gap) < 5.0) {
                verdict = "Coverage on target — intervals are well-calibrated.";
                verdict_col = ui::colors::POSITIVE();
            } else if (cov_gap > 5.0) {
                verdict = QString("Over-covered by %1pp — intervals are too wide (conservative).").arg(cov_gap, 0, 'f', 1);
                verdict_col = ui::colors::WARNING();
            } else {
                verdict = QString("Under-covered by %1pp — intervals are too narrow (overconfident).").arg(-cov_gap, 0, 'f', 1);
                verdict_col = ui::colors::NEGATIVE();
            }
            auto* il = new QLabel(verdict);
            il->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                      "padding:8px 10px; background:%2; border-left:3px solid %3;")
                                  .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), verdict_col));
            results_layout_->addWidget(il);
        }

        status_label_->setText(
            has_ci
                ? QString("MAE %1  RMSE %2  MAPE %3%%  Coverage %4%%")
                      .arg(mae, 0, 'f', 3).arg(rmse, 0, 'f', 3).arg(mape, 0, 'f', 2)
                      .arg(d.value("coverage_pct").toDouble(), 0, 'f', 1)
                : QString("MAE %1  RMSE %2  MAPE %3%%  Bias %4")
                      .arg(mae, 0, 'f', 3).arg(rmse, 0, 'f', 3).arg(mape, 0, 'f', 2).arg(bias, 0, 'f', 4));
        return;
    }

    // ── 6. SEASONAL NAIVE ───────────────────────────────────────────────────
    if (command == "seasonal_naive") {
        const int horizon = d.value("horizon").toInt();
        const int season = d.value("season_length").toInt();

        auto* hdr = new QLabel(QString("METHOD: %1   |   HORIZON %2 STEPS")
                                   .arg(d.value("method").toString().toUpper()).arg(horizon));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        const double last_actual = d.value("last_actual").toDouble();
        const double first_fc = d.value("first_forecast").toDouble();
        const double last_fc = d.value("last_forecast").toDouble();

        QList<QWidget*> top = {
            gs_make_card("LAST ACTUAL", gs_fmt_num(last_actual, 4), this),
            gs_make_card("FIRST FORECAST", gs_fmt_num(first_fc, 4), this,
                         gs_pos_neg_color(first_fc - last_actual)),
            gs_make_card("LAST FORECAST", gs_fmt_num(last_fc, 4), this,
                         gs_pos_neg_color(last_fc - last_actual)),
            gs_make_card("SEASON", QString("%1 obs").arg(season), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> bounds = {
            gs_make_card("FORECAST MIN", gs_fmt_num(d.value("forecast_min").toDouble(), 4), this,
                         ui::colors::NEGATIVE()),
            gs_make_card("FORECAST MEAN", gs_fmt_num(d.value("forecast_mean").toDouble(), 4), this),
            gs_make_card("FORECAST MAX", gs_fmt_num(d.value("forecast_max").toDouble(), 4), this,
                         ui::colors::POSITIVE()),
            gs_make_card("OBSERVATIONS", QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(bounds, this));

        // Forecast table — every step
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            auto* table = new QTableWidget(forecast.size(), 2, this);
            table->setHorizontalHeaderLabels({"Step", "Value"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * 22 + 32, 320));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                const double v = o.value("value").toDouble();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("step").toInt())));
                auto* vi = new QTableWidgetItem(QString::number(v, 'f', 4));
                vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                vi->setForeground(QColor(gs_pos_neg_color(v - last_actual)));
                table->setItem(i, 1, vi);
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(QString("%1 — last %2 → forecast %3")
                                   .arg(d.value("method").toString())
                                   .arg(last_actual, 0, 'f', 3)
                                   .arg(first_fc, 0, 'f', 3));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
