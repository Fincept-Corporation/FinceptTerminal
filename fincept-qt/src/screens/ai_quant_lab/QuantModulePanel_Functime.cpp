// src/screens/ai_quant_lab/QuantModulePanel_Functime.cpp
//
// Functime panel — 6 analysis modes (forecast, anomaly, seasonality, metrics,
// confidence intervals, stationarity). Mirrors the GS Quant pattern. Extracted
// from QuantModulePanel.cpp to keep that file maintainable.
//
// The seasonality renderer also draws Statsmodels' "decompose" result (the same
// STL payload). The script returns null for a value it cannot compute; figures
// go through the *_safe formatters so null reads "—", never 0.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
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
using namespace fincept::screens::quant_charts;
using namespace fincept::screens::quant_advanced_helpers;

namespace {

// A right-aligned table cell; null → "—".
QTableWidgetItem* ft_num_cell(const QJsonValue& v, int decimals, const QString& color = {}) {
    auto* it = new QTableWidgetItem(fmt_num_safe(v, decimals));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (!color.isEmpty() && v.isDouble())
        it->setForeground(QColor(color));
    return it;
}

// Δ of `v` against `ref`; NaN when either is null.
double ft_delta(const QJsonValue& v, const QJsonValue& ref) {
    return v.isDouble() && ref.isDouble() ? v.toDouble() - ref.toDouble() : kQcNaN;
}

// Up / down colour of a Δ; neutral when it is not known.
QString ft_delta_color(double v) {
    return std::isfinite(v) ? gs_pos_neg_color(v) : QString(ui::colors::TEXT_PRIMARY());
}

// Threshold colour of an R² that may be null.
QString ft_r2_color(const QJsonValue& r2) {
    if (!r2.isDouble())
        return ui::colors::TEXT_SECONDARY();
    const double v = r2.toDouble();
    return v >= 0.7 ? QString(ui::colors::POSITIVE()) : v >= 0.3 ? QString(ui::colors::WARNING())
                                                                 : QString(ui::colors::NEGATIVE());
}

// Functime labels every point with its ISO date when the caller supplied dates,
// otherwise "obs <position>" for history and "t+<step>" for forecast steps.
// x of each forecast row on the history's axis: its date, else last obs + step.
QVector<double> ft_forecast_x(const QJsonArray& rows, const XAxis& hist) {
    QVector<double> out;
    const double last = hist.xs.isEmpty() ? 0.0 : hist.xs.last();
    double day = 86400000.0;
    for (qsizetype i = 0; i < rows.size(); ++i) {
        double ms = 0.0;
        if (hist.time && qc_date_ms(rows.at(i).toObject().value("date").toString(), ms))
            out.append(ms);
        else
            out.append(last + (hist.time ? day : 1.0) * static_cast<double>(i + 1));
    }
    return out;
}

// Tooltip title on a position axis: "obs 241" for history, "t+3" past it.
std::function<QString(double)> ft_x_label(const XAxis& hist) {
    if (hist.time)
        return {};
    const double last = hist.xs.isEmpty() ? 0.0 : hist.xs.last();
    return [last](double x) {
        return x <= last ? QStringLiteral("obs %1").arg(static_cast<qlonglong>(std::llround(x)))
                         : QStringLiteral("t+%1").arg(static_cast<qlonglong>(std::llround(x - last)));
    };
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// FUNCTIME PANEL — 6 analysis modes (forecast, anomaly, seasonality, metrics,
// confidence intervals, stationarity). Mirrors the GS Quant pattern. Each view
// opens on real data (D1: the daily prices of a symbol, 1 year) and runs on first
// show; the paste box is the "Custom series" source.
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_functime_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = make_view_tabs(w);

    // Local sample-button helper — same shape as gs_quant's
    // Shared helper: synthetic-sample button styled by the panel sheet; runs
    // fed with its data are flagged SAMPLE INPUT on the results.
    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed, const QString& tip,
                                 bool walk = false) -> QPushButton* {
        return make_sample_button(edit, parent, tip, [seed, walk]() {
            return walk ? sample_random_walk(seed) : sample_series(seed);
        });
    };

    // ── Forecast ─────────────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("forecast"), .cost = ViewCost::Fetch, .loading = tr("Fitting the forecast model…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Forecast"),
                 [this, add_sample_btn](QWidget* fc, QVBoxLayout* fcl) {
        add_series_source(fc, fcl, SeriesShape::Single); // daily closes of the symbol (D1)

        auto* fc_vals = new QLineEdit(fc);
        fc_vals->setPlaceholderText(tr("Time series values (>= 30). CSV, space, or newline separated."));
        fc_vals->setStyleSheet(input_ss());
        text_inputs_["fn_fc_values"] = fc_vals;
        mark_series_input(fc_vals, SeriesKind::Closes);
        fcl->addWidget(build_input_row(tr("Series Values"), fc_vals, fc));
        fcl->addWidget(
            add_sample_btn(fc_vals, fc, 11, tr("200-pt synthetic series: trend + 30-pt seasonal cycle + noise")));

        auto* fc_model = new QComboBox(fc);
        fc_model->addItems({"linear", "ridge", "lasso", "elasticnet", "knn", "naive", "drift", "holt_winters", "arima"});
        fc_model->setStyleSheet(combo_ss());
        combo_inputs_["fn_fc_model"] = fc_model;
        fcl->addWidget(build_input_row(tr("Model"), fc_model, fc));

        auto* fc_horizon = new QSpinBox(fc);
        fc_horizon->setRange(1, 365);
        fc_horizon->setValue(14);
        fc_horizon->setStyleSheet(spinbox_ss());
        int_inputs_["fn_fc_horizon"] = fc_horizon;
        fcl->addWidget(build_input_row(tr("Forecast Horizon (steps)"), fc_horizon, fc));

        auto* fc_lags = new QSpinBox(fc);
        fc_lags->setRange(1, 60);
        fc_lags->setValue(7);
        fc_lags->setStyleSheet(spinbox_ss());
        int_inputs_["fn_fc_lags"] = fc_lags;
        fcl->addWidget(build_input_row(tr("Lag Window (ML models)"), fc_lags, fc));

        auto* fc_season = new QSpinBox(fc);
        fc_season->setRange(0, 365);
        fc_season->setValue(0);
        fc_season->setSpecialValueText(tr("none"));
        fc_season->setStyleSheet(spinbox_ss());
        int_inputs_["fn_fc_season"] = fc_season;
        fcl->addWidget(build_input_row(tr("Seasonal Period (HW/ARIMA)"), fc_season, fc));

        auto* fc_alpha = make_double_spin(0.0001, 100, 1.0, 4, "", fc);
        fc_alpha->setToolTip(tr("Penalty strength of the ridge, lasso and elasticnet models."));
        double_inputs_["fn_fc_alpha"] = fc_alpha;
        fcl->addWidget(build_input_row(tr("Regularization α"), fc_alpha, fc));

        auto* fc_run = make_run_button(tr("RUN FORECAST"), fc);
        set_action(fc_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("fn_fc_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 30) {
                display_error(tr("Need at least 30 observations; you provided %1. "
                                 "Click LOAD SYNTHETIC SAMPLE for 200.")
                                  .arg(vals.size()));
                return;
            }
            const QString model = combo_inputs_["fn_fc_model"]->currentText();
            show_loading(tr("Fitting %1 on %2 obs and projecting %3 steps...")
                             .arg(model)
                             .arg(vals.size())
                             .arg(int_inputs_["fn_fc_horizon"]->value()));
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
    });

    // ── Anomaly Detection ────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("anomalies"), .cost = ViewCost::Fetch, .loading = tr("Scanning for anomalies…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Anomalies"),
                 [this, add_sample_btn](QWidget* an, QVBoxLayout* anl) {
        // Daily returns of the symbol (D1): the outlier days. A price level has none at 3σ.
        add_series_source(an, anl, SeriesShape::Single);

        auto* an_vals = new QLineEdit(an);
        an_vals->setPlaceholderText(tr("Time series values (>= 20)"));
        an_vals->setStyleSheet(input_ss());
        text_inputs_["fn_an_values"] = an_vals;
        mark_series_input(an_vals, SeriesKind::Returns);
        anl->addWidget(build_input_row(tr("Series Values"), an_vals, an));
        anl->addWidget(add_sample_btn(an_vals, an, 21, tr("200-pt series — paste in your own outliers to test detection")));

        auto* an_method = new QComboBox(an);
        an_method->addItems({"zscore", "iqr", "isolation_forest", "residual"});
        an_method->setStyleSheet(combo_ss());
        combo_inputs_["fn_an_method"] = an_method;
        anl->addWidget(build_input_row(tr("Detection Method"), an_method, an));

        auto* an_thr = make_double_spin(0.5, 10.0, 3.0, 2, " σ", an);
        double_inputs_["fn_an_threshold"] = an_thr;
        anl->addWidget(build_input_row(tr("Z-Score / Residual Threshold"), an_thr, an));

        auto* an_iqr = make_double_spin(0.5, 5.0, 1.5, 2, "×", an);
        double_inputs_["fn_an_iqr_k"] = an_iqr;
        an_iqr->setToolTip(tr("Used by the iqr method."));
        anl->addWidget(build_input_row(tr("IQR Multiplier"), an_iqr, an));

        auto* an_cont = make_double_spin(0.001, 0.5, 0.05, 3, "", an);
        an_cont->setToolTip(tr("Expected share of anomalies; used by the isolation_forest method."));
        double_inputs_["fn_an_contamination"] = an_cont;
        anl->addWidget(build_input_row(tr("Anomaly Fraction"), an_cont, an));

        auto* an_run = make_run_button(tr("DETECT ANOMALIES"), an);
        set_action(an_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("fn_an_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 20) {
                display_error(tr("Need at least 20 observations; you provided %1.").arg(vals.size()));
                return;
            }
            const QString method = combo_inputs_["fn_an_method"]->currentText();
            show_loading(tr("Scanning %1 obs with %2...").arg(vals.size()).arg(method));
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
    });

    // ── Seasonality ──────────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("seasonality"), .cost = ViewCost::Fetch, .loading = tr("Decomposing trend and seasonality (STL)…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Seasonality"),
                 [this, add_sample_btn](QWidget* se, QVBoxLayout* sel) {
        add_series_source(se, sel, SeriesShape::Single); // daily closes of the symbol (D1)

        auto* se_vals = new QLineEdit(se);
        se_vals->setPlaceholderText(tr("Time series values (>= 24). Period auto-detected if left at 0."));
        se_vals->setStyleSheet(input_ss());
        text_inputs_["fn_se_values"] = se_vals;
        mark_series_input(se_vals, SeriesKind::Closes);
        sel->addWidget(build_input_row(tr("Series Values"), se_vals, se));
        auto* se_sample = add_sample_btn(se_vals, se, 31, tr("200-pt series with embedded period-30 seasonality"));
        sel->addWidget(se_sample);

        // Auto-detection finds no real cycle in daily prices (a 62-day "period" with no seasonal
        // strength), so the default is one trading month; the synthetic sample has its own period 30.
        auto* se_period = new QSpinBox(se);
        se_period->setRange(0, 365);
        se_period->setValue(21);
        se_period->setSpecialValueText(tr("auto"));
        se_period->setToolTip(tr("Length of one seasonal cycle in observations: 21 = a trading month of daily "
                                 "prices, 5 = a trading week. 0 detects it from the autocorrelation."));
        se_period->setStyleSheet(spinbox_ss());
        int_inputs_["fn_se_period"] = se_period;
        sel->addWidget(build_input_row(tr("Period (0 = auto)"), se_period, se));
        connect(se_sample, &QPushButton::clicked, se_period, [se_period]() { se_period->setValue(0); });

        auto* se_robust = new QComboBox(se);
        se_robust->addItems({"true", "false"});
        se_robust->setStyleSheet(combo_ss());
        combo_inputs_["fn_se_robust"] = se_robust;
        sel->addWidget(build_input_row(tr("Robust STL"), se_robust, se));

        auto* se_run = make_run_button(tr("DECOMPOSE SEASONALITY"), se);
        set_action(se_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("fn_se_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 24) {
                display_error(tr("Need at least 24 observations; you provided %1.").arg(vals.size()));
                return;
            }
            show_loading(tr("Running STL decomposition on %1 obs...").arg(vals.size()));
            QJsonObject params;
            params["values"] = vals;
            params["period"] = int_inputs_["fn_se_period"]->value();
            params["robust"] = combo_inputs_["fn_se_robust"]->currentText() == "true";
            AIQuantLabService::instance().functime_seasonality(params);
        });
        sel->addWidget(se_run);
        sel->addStretch();
    });

    // ── Metrics ──────────────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("metrics"), .cost = ViewCost::Fetch, .loading = tr("Scoring the forecast against the actuals…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Metrics"),
                 [this, add_sample_btn](QWidget* me, QVBoxLayout* mel) {
        // D1: each day's close against the naive forecast (the previous close) — the baseline a
        // model has to beat.
        add_series_source(me, mel, SeriesShape::Single);

        auto* me_act = new QLineEdit(me);
        me_act->setPlaceholderText(tr("Actual values (must match predicted length)"));
        me_act->setStyleSheet(input_ss());
        text_inputs_["fn_me_actual"] = me_act;
        mark_series_input(me_act, SeriesKind::ActualCloses);
        mel->addWidget(build_input_row(tr("Actual"), me_act, me));
        mel->addWidget(add_sample_btn(me_act, me, 41, tr("100-pt synthetic 'actual' series (level ~50)")));

        auto* me_pred = new QLineEdit(me);
        me_pred->setPlaceholderText(tr("Predicted values (same length as actual)"));
        me_pred->setStyleSheet(input_ss());
        text_inputs_["fn_me_pred"] = me_pred;
        mark_series_input(me_pred, SeriesKind::NaiveForecast);
        mel->addWidget(build_input_row(tr("Predicted"), me_pred, me));
        mel->addWidget(
            add_sample_btn(me_pred, me, 42, tr("100-pt synthetic 'predicted' series (similar shape, mild noise)")));

        auto* me_hint = new QLabel(tr("With a ticker as the source, each day's close is scored against the naive "
                                      "forecast (the previous close): the baseline any model has to beat."),
                                   me);
        me_hint->setWordWrap(true);
        me_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
        mark_series_widget(me_hint, /*ticker_only=*/true);
        mel->addWidget(me_hint);

        auto* me_run = make_run_button(tr("CALCULATE METRICS"), me);
        set_action(me_run, [this]() {
            QJsonArray a, p;
            QString bad;
            if (!parse_doubles(series_text("fn_me_actual"), a, &bad)) {
                display_error(tr("Actual: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("fn_me_pred"), p, &bad)) {
                display_error(tr("Predicted: '%1' is not numeric.").arg(bad));
                return;
            }
            if (a.size() < 2 || p.size() < 2) {
                display_error(tr("Need at least 2 observations in both actual and predicted."));
                return;
            }
            if (a.size() != p.size()) {
                display_error(tr("Actual (%1) and Predicted (%2) must have the same length.").arg(a.size()).arg(p.size()));
                return;
            }
            show_loading(tr("Computing forecast accuracy on %1 pairs...").arg(a.size()));
            QJsonObject params;
            params["actual"] = a;
            params["predicted"] = p;
            AIQuantLabService::instance().functime_metrics(params);
        });
        mel->addWidget(me_run);
        mel->addStretch();
    });

    // ── Confidence Intervals ─────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("intervals"), .cost = ViewCost::Fetch, .loading = tr("Building prediction intervals…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Confidence Intervals"),
                 [this, add_sample_btn](QWidget* ci, QVBoxLayout* cil) {
        add_series_source(ci, cil, SeriesShape::Single); // daily closes of the symbol (D1)

        auto* ci_vals = new QLineEdit(ci);
        ci_vals->setPlaceholderText(tr("Series values (training history, >= 30)"));
        ci_vals->setStyleSheet(input_ss());
        text_inputs_["fn_ci_values"] = ci_vals;
        mark_series_input(ci_vals, SeriesKind::Closes);
        cil->addWidget(build_input_row(tr("Series Values"), ci_vals, ci));
        cil->addWidget(add_sample_btn(ci_vals, ci, 51, tr("200-pt synthetic series with seasonality")));

        auto* ci_method = new QComboBox(ci);
        ci_method->addItems({"bootstrap", "residual"});
        ci_method->setStyleSheet(combo_ss());
        combo_inputs_["fn_ci_method"] = ci_method;
        cil->addWidget(build_input_row(tr("Interval Method"), ci_method, ci));

        auto* ci_horizon = new QSpinBox(ci);
        ci_horizon->setRange(1, 90);
        ci_horizon->setValue(14);
        ci_horizon->setStyleSheet(spinbox_ss());
        int_inputs_["fn_ci_horizon"] = ci_horizon;
        cil->addWidget(build_input_row(tr("Forecast Horizon"), ci_horizon, ci));

        auto* ci_lags = new QSpinBox(ci);
        ci_lags->setRange(1, 60);
        ci_lags->setValue(7);
        ci_lags->setStyleSheet(spinbox_ss());
        int_inputs_["fn_ci_lags"] = ci_lags;
        cil->addWidget(build_input_row(tr("Lag Window"), ci_lags, ci));

        auto* ci_boot = new QSpinBox(ci);
        ci_boot->setRange(50, 1000);
        ci_boot->setValue(200);
        ci_boot->setStyleSheet(spinbox_ss());
        int_inputs_["fn_ci_n_boot"] = ci_boot;
        cil->addWidget(build_input_row(tr("Bootstrap Iterations"), ci_boot, ci));

        auto* ci_conf = make_double_spin(0.50, 0.999, 0.95, 3, "", ci);
        double_inputs_["fn_ci_confidence"] = ci_conf;
        cil->addWidget(build_input_row(tr("Confidence Level"), ci_conf, ci));

        auto* ci_run = make_run_button(tr("BUILD INTERVALS"), ci);
        set_action(ci_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("fn_ci_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 30) {
                display_error(tr("Need at least 30 observations; you provided %1.").arg(vals.size()));
                return;
            }
            const QString method = combo_inputs_["fn_ci_method"]->currentText();
            show_loading(tr("Building %1 prediction intervals...").arg(method));
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
    });

    // ── Stationarity ─────────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("stationarity"), .cost = ViewCost::Fetch, .loading = tr("Testing stationarity at each differencing order…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Stationarity"),
                 [this, add_sample_btn](QWidget* st, QVBoxLayout* stl) {
        // Price levels (D1): non-stationary at d = 0, stationary returns at d = 1.
        add_series_source(st, stl, SeriesShape::Single);

        auto* st_vals = new QLineEdit(st);
        st_vals->setPlaceholderText(tr("Series values (>= 30) — runs ADF + KPSS at each differencing order"));
        st_vals->setStyleSheet(input_ss());
        text_inputs_["fn_st_values"] = st_vals;
        mark_series_input(st_vals, SeriesKind::Closes);
        stl->addWidget(build_input_row(tr("Series Values"), st_vals, st));
        stl->addWidget(add_sample_btn(st_vals, st, 61, tr("150-pt random walk with drift — should require d=1"), true));

        auto* st_max = new QSpinBox(st);
        st_max->setRange(0, 3);
        st_max->setValue(2);
        st_max->setToolTip(tr("ADF and KPSS run at every differencing order from 0 up to this one."));
        st_max->setStyleSheet(spinbox_ss());
        int_inputs_["fn_st_max_d"] = st_max;
        stl->addWidget(build_input_row(tr("Max Differencing d"), st_max, st));

        auto* st_run = make_run_button(tr("RUN STATIONARITY TESTS"), st);
        set_action(st_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("fn_st_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 30) {
                display_error(tr("Need at least 30 observations; you provided %1.").arg(vals.size()));
                return;
            }
            show_loading(tr("Running ADF + KPSS up to d=%1...").arg(int_inputs_["fn_st_max_d"]->value()));
            QJsonObject params;
            params["values"] = vals;
            params["max_d"] = int_inputs_["fn_st_max_d"]->value();
            AIQuantLabService::instance().functime_stationarity(params);
        });
        stl->addWidget(st_run);
        stl->addStretch();
    });

    vl->addWidget(tabs);

    add_result_pane(vl);

    return w;
}

// ── FUNCTIME: rich card-based display per command ───────────────────────────

void QuantModulePanel::display_functime_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    if (!payload.value("success").toBool(false)) {
        const QString err = payload.value("error").toString(tr("Unknown error"));
        const QString kind = payload.value("error_kind").toString();
        const QString prefix = kind == "validation" ? tr("Input error: ")
                               : kind == "runtime"  ? tr("Computation failed: ")
                                                    : QString();
        display_error(prefix + err);
        return;
    }

    const QJsonObject d = payload.value("data").toObject();
    const QString accent = ui::colors::legible(module_.color);

    // Section header strip — shared with GS Quant for visual consistency.
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_observations"))
        header_text += tr("  |  %1 OBS").arg(d.value("n_observations").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 2. FORECAST ──────────────────────────────────────────────────────────
    if (command == "forecast") {
        const QString model = d.value("model").toString();
        const int horizon = d.value("horizon").toInt();
        const int lags = d.value("lags").toInt();
        const QJsonValue r2 = d.value("in_sample_r2");
        const QJsonValue last_actual = d.value("last_actual");
        const QJsonValue first_fc = d.value("first_forecast");
        const QJsonValue last_fc = d.value("last_forecast");

        auto* hdr =
            new QLabel(tr("MODEL %1  |  HORIZON %2 STEPS  |  LAGS %3").arg(model.toUpper()).arg(horizon).arg(lags));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> fit = {
            gs_make_card(tr("IN-SAMPLE R²"), fmt_num_safe(r2, 3), this, ft_r2_color(r2)),
            gs_make_card(tr("IN-SAMPLE MAE"), fmt_num_safe(d.value("in_sample_mae"), 4), this),
            gs_make_card(tr("IN-SAMPLE RMSE"), fmt_num_safe(d.value("in_sample_rmse"), 4), this),
            gs_make_card(tr("RESIDUAL σ"), fmt_num_safe(d.value("residual_std"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(fit, this));

        const double drift = ft_delta(first_fc, last_actual);
        const double horizon_drift = ft_delta(last_fc, last_actual);
        QList<QWidget*> proj = {
            gs_make_card(tr("LAST ACTUAL"), fmt_num_safe(last_actual, 4), this),
            gs_make_card(tr("FIRST FORECAST"), fmt_num_safe(first_fc, 4), this, ft_delta_color(drift)),
            gs_make_card(tr("LAST FORECAST"), fmt_num_safe(last_fc, 4), this, ft_delta_color(horizon_drift)),
            gs_make_card(tr("HORIZON Δ"),
                         std::isfinite(horizon_drift) ? gs_fmt_num(horizon_drift, 4) : QString::fromUtf8("—"), this,
                         ft_delta_color(horizon_drift)),
        };
        results_layout_->addWidget(gs_card_row(proj, this));

        QList<QWidget*> bounds = {
            gs_make_card(tr("FORECAST MIN"), fmt_num_safe(d.value("forecast_min"), 4), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("FORECAST MEAN"), fmt_num_safe(d.value("forecast_mean"), 4), this),
            gs_make_card(tr("FORECAST MAX"), fmt_num_safe(d.value("forecast_max"), 4), this, ui::colors::POSITIVE()),
            gs_make_card(tr("SEASON USED"),
                         d.value("season").toInt() > 0 ? QString::number(d.value("season").toInt()) : tr("none"), this),
        };
        results_layout_->addWidget(gs_card_row(bounds, this));

        // Forecast chart: the history the script sent (sampled to ~250 points, always
        // ending on the last observation), the forecast continuing from the last actual,
        // and the in-sample fit behind its legend entry. The table below keeps every step.
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            const QJsonArray hist = d.value("history").toArray();
            const QJsonArray fitted = d.value("fitted").toArray();
            QcForecast f;
            f.title = tr("HISTORY + %1-STEP FORECAST").arg(forecast.size());
            f.history_x = qc_x_axis(hist, {QStringLiteral("date")});
            f.history = qc_column(hist, QStringLiteral("value"));
            const XAxis fit_x = qc_x_axis(fitted, {QStringLiteral("date")});
            if (fit_x.time == f.history_x.time) {
                f.fitted_x = fit_x.xs;
                f.fitted = qc_column(fitted, QStringLiteral("value"));
            }
            f.fc_x = ft_forecast_x(forecast, f.history_x);
            f.point = qc_column(forecast, QStringLiteral("value"));
            f.anchor_x = f.history_x.xs.isEmpty() ? 0.0 : f.history_x.xs.last();
            f.anchor_y = last_actual.toDouble(kQcNaN);
            f.x_label = ft_x_label(f.history_x);
            results_layout_->addWidget(qc_forecast_chart(f, this));
            results_layout_->addWidget(qc_caption(tr("FORECAST BY STEP  ·  Δ VS LAST ACTUAL COLOURED")));

            auto* table = new QTableWidget(forecast.size(), 3, this);
            table->setHorizontalHeaderLabels({tr("Step"), tr("Date"), tr("Forecast")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            // Filled with fixed-width sections, sized to the contents once afterwards (a
            // ResizeToContents section re-measures every column on each setItem).
            table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
            table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
            table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * row_px(24) + row_px(32), row_px(220)));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
                table->setItem(i, 1, new QTableWidgetItem(o.value("date").toString()));
                table->setItem(i, 2,
                               ft_num_cell(o.value("value"), 4,
                                           ft_delta_color(ft_delta(o.value("value"), last_actual))));
                table->setRowHeight(i, row_px(22));
            }
            table->resizeColumnsToContents();
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("%1 trained — R²=%2  |  forecast %3 → %4")
                                   .arg(model)
                                   .arg(fmt_num_safe(r2, 3), fmt_num_safe(first_fc, 3), fmt_num_safe(last_fc, 3)));
        return;
    }

    // ── 3. ANOMALY DETECTION ────────────────────────────────────────────────
    if (command == "anomaly_detection") {
        const QString method = d.value("method").toString();
        const int n_anom = d.value("n_anomalies").toInt();
        const double rate = d.value("anomaly_rate_pct").toDouble();
        const int n_obs = d.value("n_observations").toInt();

        auto* hdr = new QLabel(tr("METHOD %1  |  %2 / %3 ANOMALIES (%4%)")
                                   .arg(method.toUpper())
                                   .arg(n_anom)
                                   .arg(n_obs)
                                   .arg(rate, 0, 'f', 2));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card(tr("ANOMALIES FOUND"), QString::number(n_anom), this,
                         n_anom == 0 ? ui::colors::POSITIVE()
                         : rate > 5  ? ui::colors::NEGATIVE()
                                     : ui::colors::WARNING()),
            gs_make_card(tr("ANOMALY RATE"), QString::number(rate, 'f', 2) + "%", this,
                         rate > 5   ? ui::colors::NEGATIVE()
                         : rate > 1 ? ui::colors::WARNING()
                                    : ui::colors::POSITIVE()),
            gs_make_card(tr("MAX |SCORE|"),
                         d.value("score_min").isDouble() && d.value("score_max").isDouble()
                             ? gs_fmt_num(std::max(std::abs(d.value("score_min").toDouble()),
                                                   std::abs(d.value("score_max").toDouble())),
                                          3)
                             : QString::fromUtf8("—"),
                         this, ui::colors::WARNING()),
            gs_make_card(tr("SCORE σ"), fmt_num_safe(d.value("score_std"), 3), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> dist = {
            gs_make_card(tr("OBSERVATIONS"), QString::number(n_obs), this),
            gs_make_card(tr("MIN SCORE"), fmt_num_safe(d.value("score_min"), 3), this),
            gs_make_card(tr("MAX SCORE"), fmt_num_safe(d.value("score_max"), 3), this),
            gs_make_card(tr("MEAN SCORE"), fmt_num_safe(d.value("score_mean"), 3), this),
        };
        results_layout_->addWidget(gs_card_row(dist, this));

        // Anomalies table (top 200, sorted by abs score)
        const QJsonArray anomalies = d.value("anomalies").toArray();
        if (!anomalies.isEmpty()) {
            auto* table = new QTableWidget(anomalies.size(), 4, this);
            table->setHorizontalHeaderLabels({tr("Index"), tr("Date"), tr("Value"), tr("Score")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(anomalies.size() * row_px(24) + row_px(32), 360));
            for (int i = 0; i < anomalies.size(); ++i) {
                const auto o = anomalies[i].toObject();
                const double score = o.value("score").toDouble();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("index").toInt())));
                table->setItem(i, 1, new QTableWidgetItem(o.value("date").toString()));
                table->setItem(i, 2, ft_num_cell(o.value("value"), 4));
                table->setItem(i, 3,
                               ft_num_cell(o.value("score"), 3,
                                           std::abs(score) > 5 ? ui::colors::NEGATIVE() : ui::colors::WARNING()));
                table->setRowHeight(i, row_px(22));
            }
            results_layout_->addWidget(table);
        } else {
            auto* clean = new QLabel(tr("No anomalies detected at the configured threshold."));
            clean->setStyleSheet(
                QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; padding:10px;"
                        "background:%2; border:1px solid %3; border-radius:2px;")
                    .arg(ui::colors::POSITIVE(), ui::colors::POSITIVE_BG(), ui::colors::POSITIVE_DIM()));
            results_layout_->addWidget(clean);
        }

        status_label_->setText(
            tr("%1: %2 anomalies in %3 obs (%4%)").arg(method).arg(n_anom).arg(n_obs).arg(rate, 0, 'f', 2));
        return;
    }

    // ── 4. SEASONALITY (also Statsmodels' "decompose": the same STL payload) ─
    if (command == "seasonality" || command == "decompose") {
        const int period = d.value("period").toInt();
        const bool auto_det = d.value("period_auto_detected").toBool();
        // Strengths are null when a component has no variance to compare against.
        const QJsonValue tstr_v = d.value("trend_strength");
        const QJsonValue sstr_v = d.value("seasonal_strength");
        const double tstr = tstr_v.toDouble();
        const double sstr = sstr_v.toDouble();
        // Statsmodels says whether the fit was robust; Functime does not.
        const QString robust = !d.contains("robust")         ? QString()
                               : d.value("robust").toBool() ? tr("  |  ROBUST")
                                                            : tr("  |  NON-ROBUST");

        auto* hdr = new QLabel(tr("PERIOD %1%2  |  STL DECOMPOSITION%3")
                                   .arg(period)
                                   .arg(auto_det ? tr("  (auto-detected)") : QString(), robust));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        auto strength_color = [](const QJsonValue& s) {
            if (!s.isDouble())
                return ui::colors::TEXT_SECONDARY();
            if (s.toDouble() > 0.6)
                return ui::colors::POSITIVE();
            if (s.toDouble() > 0.3)
                return ui::colors::WARNING();
            return ui::colors::TEXT_PRIMARY();
        };

        QList<QWidget*> strengths = {
            gs_make_card(tr("PERIOD"), QString::number(period), this, ui::colors::INFO()),
            gs_make_card(tr("TREND STRENGTH"), fmt_num_safe(tstr_v, 3), this, strength_color(tstr_v)),
            gs_make_card(tr("SEASONAL STRENGTH"), fmt_num_safe(sstr_v, 3), this, strength_color(sstr_v)),
            gs_make_card(tr("RESIDUAL σ"), fmt_num_safe(d.value("residual_std"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(strengths, this));

        // Interpretation strip
        QString interp;
        if (!tstr_v.isDouble() || !sstr_v.isDouble())
            interp = tr("A component has no variance, so its strength is undefined.");
        else if (sstr > 0.6 && tstr > 0.6)
            interp = tr("Strong trend AND strong seasonality detected.");
        else if (sstr > 0.6)
            interp = tr("Strong seasonal cycle, weak trend.");
        else if (tstr > 0.6)
            interp = tr("Strong trend, weak/no seasonality.");
        else
            interp = tr("Weak structure — series is close to noise around its mean.");
        auto* il = new QLabel(interp);
        il->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                  "padding:8px 10px; background:%2; border-left:3px solid %3;")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(il);

        // Decomposition: the whole series as a chart (observed + trend, then the seasonal and
        // residual components in their own panes on the same x), and the latest rows below it.
        // The four arrays are sampled by the script on the same index, so row i is one date.
        const QJsonArray history = d.value("history").toArray();
        const QJsonArray trend = d.value("trend").toArray();
        const QJsonArray seasonal = d.value("seasonal").toArray();
        const QJsonArray residual = d.value("residual").toArray();
        if (!trend.isEmpty()) {
            const XAxis x = qc_x_axis(trend, {QStringLiteral("date")});
            QcChartSpec spec;
            spec.title = tr("STL DECOMPOSITION  ·  PERIOD %1").arg(period);
            spec.x_time = x.time;
            spec.xs = x.xs;
            spec.fmt = qc_fmt_auto();
            if (history.size() == trend.size()) {
                QcLine obs;
                obs.name = tr("Observed");
                obs.ys = qc_column(history, QStringLiteral("value"));
                obs.color = ui::ChartColorRole::Muted;
                obs.width = 1.0;
                spec.lines.append(obs);
            }
            QcLine tl;
            tl.name = tr("Trend");
            tl.ys = qc_column(trend, QStringLiteral("value"));
            tl.color = ui::ChartColorRole::Accent;
            tl.width = 2.0;
            spec.lines.append(tl);
            QcLine sl;
            sl.name = tr("Seasonal");
            sl.ys = qc_column(seasonal, QStringLiteral("value"));
            sl.color = ui::ChartColorRole::Info;
            sl.pane = 1;
            spec.lines.append(sl);
            QcLine rl;
            rl.name = tr("Residual");
            rl.ys = qc_column(residual, QStringLiteral("value"));
            rl.style = ui::TimeSeriesChart::Style::Bars;
            rl.color = ui::ChartColorRole::Muted;
            rl.pane = 2;
            spec.lines.append(rl);
            spec.pane_titles = {tr("SEASONAL"), tr("RESIDUAL")};
            spec.pane_fmts = {qc_fmt_auto(), qc_fmt_auto()};
            if (!x.time) {
                spec.x_label = [](double v) {
                    return QStringLiteral("obs %1").arg(static_cast<qlonglong>(std::llround(v)));
                };
            }
            results_layout_->addWidget(qc_series_chart(spec, this));
        }
        const QVector<int> latest = qc_latest_rows(trend.size());
        const int rows = latest.size();
        if (rows > 0) {
            results_layout_->addWidget(qc_caption(qc_latest_caption(rows, trend.size())));
            auto* table = new QTableWidget(rows, 4, this);
            table->setHorizontalHeaderLabels({tr("Date"), tr("Trend"), tr("Seasonal"), tr("Residual")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(rows * row_px(22) + row_px(32));
            for (int r = 0; r < rows; ++r) {
                const int i = latest.at(r);
                const auto td = trend[i].toObject();
                const auto sd = seasonal.size() > i ? seasonal[i].toObject() : QJsonObject{};
                const auto rd = residual.size() > i ? residual[i].toObject() : QJsonObject{};
                table->setItem(r, 0, new QTableWidgetItem(td.value("date").toString()));
                table->setItem(r, 1, ft_num_cell(td.value("value"), 4));
                table->setItem(r, 2, ft_num_cell(sd.value("value"), 4));
                table->setItem(r, 3, ft_num_cell(rd.value("value"), 4));
                table->setRowHeight(r, row_px(22));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("STL period=%1  |  trend=%2  seasonal=%3")
                                   .arg(period)
                                   .arg(fmt_num_safe(tstr_v, 2), fmt_num_safe(sstr_v, 2)));
        return;
    }

    // ── 5. METRICS ───────────────────────────────────────────────────────────
    if (command == "metrics") {
        // r_squared is null when the actuals are constant; the percentage errors when the
        // actuals are all zero.
        const QJsonValue r2 = d.value("r_squared");
        const QJsonValue mape = d.value("mape_pct");
        const QJsonValue smape = d.value("smape_pct");
        const QJsonValue dir_acc = d.value("direction_accuracy_pct");
        const double bias = d.value("bias").toDouble();
        const auto err_color = [](const QJsonValue& v) {
            if (!v.isDouble())
                return ui::colors::TEXT_SECONDARY();
            return v.toDouble() < 10 ? ui::colors::POSITIVE() : v.toDouble() < 30 ? ui::colors::WARNING()
                                                                                    : ui::colors::NEGATIVE();
        };

        QList<QWidget*> err_row = {
            gs_make_card(tr("MAE"), fmt_num_safe(d.value("mae"), 4), this),
            gs_make_card(tr("RMSE"), fmt_num_safe(d.value("rmse"), 4), this),
            gs_make_card(tr("MSE"), fmt_num_safe(d.value("mse"), 4), this),
            gs_make_card(tr("BIAS"), fmt_num_safe(d.value("bias"), 4), this, gs_pos_neg_color(-std::abs(bias))),
        };
        results_layout_->addWidget(gs_card_row(err_row, this));

        QList<QWidget*> pct_row = {
            gs_make_card(tr("MAPE"), fmt_pts_safe(mape, 2), this, err_color(mape)),
            gs_make_card(tr("SMAPE"), fmt_pts_safe(smape, 2), this, err_color(smape)),
            gs_make_card(tr("R²"), fmt_num_safe(r2, 4), this,
                         !r2.isDouble()         ? ui::colors::TEXT_SECONDARY()
                         : r2.toDouble() > 0.7 ? ui::colors::POSITIVE()
                         : r2.toDouble() > 0.3 ? ui::colors::WARNING()
                                               : ui::colors::NEGATIVE()),
            gs_make_card(tr("DIRECTION ACC"), fmt_pts_safe(dir_acc, 1), this,
                         !dir_acc.isDouble()          ? ui::colors::TEXT_SECONDARY()
                         : dir_acc.toDouble() > 60.0 ? ui::colors::POSITIVE()
                                                     : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(pct_row, this));

        status_label_->setText(tr("MAE %1  |  RMSE %2  |  R² %3  |  MAPE %4")
                                   .arg(fmt_num_safe(d.value("mae"), 3), fmt_num_safe(d.value("rmse"), 3),
                                        fmt_num_safe(r2, 3), fmt_pts_safe(mape, 2)));
        return;
    }

    // ── 6. CONFIDENCE INTERVALS ──────────────────────────────────────────────
    if (command == "confidence_intervals") {
        const QString method = d.value("method").toString();
        const int horizon = d.value("horizon").toInt();
        const double conf = d.value("confidence").toDouble();
        const QJsonValue mean_w = d.value("mean_width");

        auto* hdr = new QLabel(tr("METHOD %1  |  CONFIDENCE %2%  |  HORIZON %3")
                                   .arg(method.toUpper())
                                   .arg(conf * 100, 0, 'f', 1)
                                   .arg(horizon));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card(tr("RESIDUAL σ"), fmt_num_safe(d.value("residual_std"), 4), this),
            gs_make_card(tr("MEAN WIDTH"), fmt_num_safe(mean_w, 4), this, ui::colors::WARNING()),
            gs_make_card(tr("FIRST LOWER"), fmt_num_safe(d.value("first_lower"), 4), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("FIRST UPPER"), fmt_num_safe(d.value("first_upper"), 4), this, ui::colors::POSITIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // n_boot is null for the residual method (no bootstrap): "n/a", the method does not use it.
        QList<QWidget*> wide = {
            gs_make_card(tr("LAST LOWER"), fmt_num_safe(d.value("last_lower"), 4), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("LAST UPPER"), fmt_num_safe(d.value("last_upper"), 4), this, ui::colors::POSITIVE()),
            gs_make_card(tr("LAGS"), QString::number(d.value("lags").toInt()), this),
            gs_make_card(tr("BOOT N"),
                         d.value("n_boot").isNull() ? tr("n/a") : QString::number(d.value("n_boot").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(wide, this));

        const QJsonArray intervals = d.value("intervals").toArray();
        if (!intervals.isEmpty()) {
            // History + point forecast + the interval, continuing from the last observation.
            const QJsonArray hist = d.value("history").toArray();
            QcForecast f;
            f.title = tr("HISTORY + FORECAST  ·  %1% INTERVAL").arg(conf * 100, 0, 'f', 1);
            f.history_x = qc_x_axis(hist, {QStringLiteral("date")});
            f.history = qc_column(hist, QStringLiteral("value"));
            f.fc_x = ft_forecast_x(intervals, f.history_x);
            f.point = qc_column(intervals, QStringLiteral("point"));
            QcBand band;
            band.lower_pct = (1.0 - conf) * 50.0;
            band.upper_pct = 100.0 - band.lower_pct;
            band.lower = qc_column(intervals, QStringLiteral("lower"));
            band.upper = qc_column(intervals, QStringLiteral("upper"));
            band.lower_name = tr("Lower %1%").arg(conf * 100, 0, 'g', 3);
            band.upper_name = tr("Upper %1%").arg(conf * 100, 0, 'g', 3);
            f.bands.append(band);
            if (!f.history.isEmpty()) {
                f.anchor_x = f.history_x.xs.last();
                f.anchor_y = f.history.last();
            }
            f.x_label = ft_x_label(f.history_x);
            results_layout_->addWidget(qc_forecast_chart(f, this));
            results_layout_->addWidget(qc_caption(tr("INTERVAL BY STEP")));

            auto* table = new QTableWidget(intervals.size(), 5, this);
            table->setHorizontalHeaderLabels({tr("Step"), tr("Date"), tr("Lower"), tr("Point"), tr("Upper")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            // Fixed-width sections while filling, sized to the contents once afterwards.
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
            table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(intervals.size() * row_px(24) + row_px(32), row_px(220)));
            for (int i = 0; i < intervals.size(); ++i) {
                const auto o = intervals[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
                table->setItem(i, 1, new QTableWidgetItem(o.value("date").toString()));
                table->setItem(i, 2, ft_num_cell(o.value("lower"), 4, ui::colors::NEGATIVE()));
                table->setItem(i, 3, ft_num_cell(o.value("point"), 4));
                table->setItem(i, 4, ft_num_cell(o.value("upper"), 4, ui::colors::POSITIVE()));
                table->setRowHeight(i, row_px(22));
            }
            table->resizeColumnsToContents();
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("%1 intervals @ %2%  |  mean width %3")
                                   .arg(intervals.size())
                                   .arg(conf * 100, 0, 'f', 1)
                                   .arg(fmt_num_safe(mean_w, 3)));
        return;
    }

    // ── 7. STATIONARITY ──────────────────────────────────────────────────────
    if (command == "stationarity") {
        const int rec_d = d.value("recommended_d").toInt();
        const int max_d = d.value("max_d_tested").toInt();
        const QJsonArray tests = d.value("tests").toArray();

        auto* hdr = new QLabel(tr("RECOMMENDED DIFFERENCING ORDER:  d = %1   (tested 0..%2)").arg(rec_d).arg(max_d));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        // One card row per tested d
        for (const auto& v : tests) {
            const auto t = v.toObject();
            const int dval = t.value("differencing_order").toInt();
            const bool both_stat = t.value("both_stationary").toBool();
            const QString label = tr("d = %1").arg(dval);
            const QString verdict = both_stat ? tr("STATIONARY") : tr("NON-STATIONARY");
            const QString verdict_col = both_stat ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();

            QList<QWidget*> row = {
                gs_make_card(label.toUpper(), verdict, this, verdict_col),
                gs_make_card(tr("ADF p"), fmt_num_safe(t.value("adf_p_value"), 4), this,
                             t.value("adf_stationary").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card(tr("KPSS p"), fmt_num_safe(t.value("kpss_p_value"), 4), this,
                             t.value("kpss_stationary").toBool() ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card(tr("OBS USED"), QString::number(t.value("n_observations").toInt()), this),
            };
            results_layout_->addWidget(gs_card_row(row, this));
        }

        // Test legend
        auto* legend = new QLabel(tr("ADF: H₀ = unit root (non-stationary). p < 0.05 ⇒ stationary.\n"
                                     "KPSS: H₀ = stationary. p ≥ 0.05 ⇒ stationary.\n"
                                     "Verdict requires BOTH tests to agree."));
        legend->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;"
                                      "padding:6px 10px; background:%2;")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED()));
        results_layout_->addWidget(legend);

        status_label_->setText(tr("Recommended d = %1 (tested %2 orders)").arg(rec_d).arg(tests.size()));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
