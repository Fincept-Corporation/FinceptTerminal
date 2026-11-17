// src/screens/ai_quant_lab/QuantModulePanel_Gluonts.cpp
//
// Gluonts panel — 5 probabilistic-forecasting modes (probabilistic forecast,
// quantile forecast, distribution fit, evaluate forecast, seasonal naive).
// Mirrors GS Quant / Functime / Statsmodels / Fortitudo / Quant Reporting.
// Extracted from QuantModulePanel.cpp to keep that file maintainable.
//
// The script returns null for a value it cannot compute (a KS p-value of a
// failed fit, a MAPE over all-zero actuals); figures go through the *_safe
// formatters so null reads "—", never 0.
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
using namespace fincept::screens::quant_charts;
using namespace fincept::screens::quant_advanced_helpers;

namespace {

// A right-aligned table cell; null → "—" (and no colour).
QTableWidgetItem* gl_num_cell(const QJsonValue& v, int decimals, const QString& color = {}) {
    auto* it = new QTableWidgetItem(fmt_num_safe(v, decimals));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (!color.isEmpty() && v.isDouble())
        it->setForeground(QColor(color));
    return it;
}

// Δ of `v` against `ref`; NaN when either is null.
double gl_delta(const QJsonValue& v, const QJsonValue& ref) {
    return v.isDouble() && ref.isDouble() ? v.toDouble() - ref.toDouble() : kQcNaN;
}

// Up / down colour of a Δ; neutral when it is not known.
QString gl_delta_color(double v) {
    return std::isfinite(v) ? gs_pos_neg_color(v) : QString(ui::colors::TEXT_PRIMARY());
}

// Colour of a percentage error (MAPE / sMAPE) that may be null.
QString gl_err_color(const QJsonValue& v) {
    if (!v.isDouble())
        return ui::colors::TEXT_SECONDARY();
    const double x = v.toDouble();
    return x < 10 ? QString(ui::colors::POSITIVE()) : x < 30 ? QString(ui::colors::WARNING())
                                                            : QString(ui::colors::NEGATIVE());
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// GLUONTS PANEL — 5 probabilistic-forecasting modes (probabilistic forecast,
// quantile forecast, distribution fit, evaluate forecast, seasonal naive).
// Mirrors GS Quant / Functime / Statsmodels / Fortitudo / Quant Reporting. Each
// view opens on real data (D1: the daily prices of a symbol, 1 year) and runs on
// first show; the paste box is the "Custom series" source.
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_gluonts_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = make_view_tabs(w);

    // Shared helper: synthetic-sample button styled by the panel sheet; runs
    // fed with its data are flagged SAMPLE INPUT on the results.
    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed, const QString& tip) -> QPushButton* {
        return make_sample_button(edit, parent, tip, [seed]() { return sample_series(seed); });
    };

    // ── Probabilistic Forecast ───────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("probabilistic"), .cost = ViewCost::Fetch, .loading = tr("Bootstrapping forecast paths…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Probabilistic Forecast"),
                 [this, add_sample_btn](QWidget* pf, QVBoxLayout* pfl) {
        add_series_source(pf, pfl, SeriesShape::Single); // daily closes of the symbol (D1)

        auto* pf_vals = new QLineEdit(pf);
        pf_vals->setPlaceholderText(
            tr("Series values (>= 30). Bootstrap residual ensemble forecasts the next H steps with quantile bands."));
        pf_vals->setStyleSheet(input_ss());
        text_inputs_["gn_pf_values"] = pf_vals;
        mark_series_input(pf_vals, SeriesKind::Closes);
        pfl->addWidget(build_input_row(tr("Series Values"), pf_vals, pf));
        pfl->addWidget(add_sample_btn(pf_vals, pf, 311, tr("200-pt synthetic series with seasonality")));

        auto* pf_horizon = new QSpinBox(pf);
        pf_horizon->setRange(1, 365);
        pf_horizon->setValue(14);
        pf_horizon->setStyleSheet(spinbox_ss());
        int_inputs_["gn_pf_horizon"] = pf_horizon;
        pfl->addWidget(build_input_row(tr("Forecast Horizon"), pf_horizon, pf));

        auto* pf_lags = new QSpinBox(pf);
        pf_lags->setRange(1, 60);
        pf_lags->setValue(7);
        pf_lags->setStyleSheet(spinbox_ss());
        int_inputs_["gn_pf_lags"] = pf_lags;
        pfl->addWidget(build_input_row(tr("AR Lag Window"), pf_lags, pf));

        auto* pf_npaths = new QSpinBox(pf);
        pf_npaths->setRange(100, 5000);
        pf_npaths->setSingleStep(100);
        pf_npaths->setValue(500);
        pf_npaths->setStyleSheet(spinbox_ss());
        int_inputs_["gn_pf_n_paths"] = pf_npaths;
        pfl->addWidget(build_input_row(tr("Bootstrap Paths"), pf_npaths, pf));

        auto* pf_run = make_run_button(tr("RUN PROBABILISTIC FORECAST"), pf);
        set_action(pf_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("gn_pf_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 30) {
                display_error(tr("Need at least 30 obs; you provided %1.").arg(vals.size()));
                return;
            }
            show_loading(tr("Bootstrapping %1 paths over %2 steps...")
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
    });

    // ── Quantile Forecast ────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("quantile"), .cost = ViewCost::Fetch, .loading = tr("Bootstrapping quantile paths…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Quantile Forecast"),
                 [this, add_sample_btn](QWidget* qf, QVBoxLayout* qfl) {
        add_series_source(qf, qfl, SeriesShape::Single); // daily closes of the symbol (D1)

        auto* qf_vals = new QLineEdit(qf);
        qf_vals->setPlaceholderText(tr("Series values (>= 30)"));
        qf_vals->setStyleSheet(input_ss());
        text_inputs_["gn_qf_values"] = qf_vals;
        mark_series_input(qf_vals, SeriesKind::Closes);
        qfl->addWidget(build_input_row(tr("Series Values"), qf_vals, qf));
        qfl->addWidget(add_sample_btn(qf_vals, qf, 321, tr("200-pt synthetic series")));

        auto* qf_quantiles = new QLineEdit(qf);
        qf_quantiles->setPlaceholderText(tr("Quantiles in (0, 1) — e.g. 0.05, 0.5, 0.95"));
        qf_quantiles->setText("0.05, 0.25, 0.5, 0.75, 0.95");
        qf_quantiles->setStyleSheet(input_ss());
        text_inputs_["gn_qf_quantiles"] = qf_quantiles;
        qfl->addWidget(build_input_row(tr("Quantile List"), qf_quantiles, qf));

        auto* qf_horizon = new QSpinBox(qf);
        qf_horizon->setRange(1, 365);
        qf_horizon->setValue(14);
        qf_horizon->setStyleSheet(spinbox_ss());
        int_inputs_["gn_qf_horizon"] = qf_horizon;
        qfl->addWidget(build_input_row(tr("Forecast Horizon"), qf_horizon, qf));

        auto* qf_lags = new QSpinBox(qf);
        qf_lags->setRange(1, 60);
        qf_lags->setValue(7);
        qf_lags->setStyleSheet(spinbox_ss());
        int_inputs_["gn_qf_lags"] = qf_lags;
        qfl->addWidget(build_input_row(tr("AR Lag Window"), qf_lags, qf));

        auto* qf_npaths = new QSpinBox(qf);
        qf_npaths->setRange(100, 5000);
        qf_npaths->setSingleStep(100);
        qf_npaths->setValue(500);
        qf_npaths->setStyleSheet(spinbox_ss());
        int_inputs_["gn_qf_n_paths"] = qf_npaths;
        qfl->addWidget(build_input_row(tr("Bootstrap Paths"), qf_npaths, qf));

        auto* qf_run = make_run_button(tr("RUN QUANTILE FORECAST"), qf);
        set_action(qf_run, [this]() {
            QJsonArray vals, quants;
            QString bad;
            if (!parse_doubles(series_text("gn_qf_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 30) {
                display_error(tr("Need at least 30 obs; you provided %1.").arg(vals.size()));
                return;
            }
            if (!parse_doubles(text_inputs_["gn_qf_quantiles"]->text(), quants, &bad)) {
                display_error(tr("Quantiles: '%1' is not numeric.").arg(bad));
                return;
            }
            if (quants.isEmpty()) {
                display_error(tr("Provide at least one quantile in (0, 1)."));
                return;
            }
            for (const auto& v : quants) {
                const double q = v.toDouble();
                if (!(0.0 < q && q < 1.0)) {
                    display_error(tr("Quantile %1 must be in (0, 1).").arg(q));
                    return;
                }
            }
            show_loading(tr("Bootstrapping %1 paths and computing %2 quantiles...")
                             .arg(int_inputs_["gn_qf_n_paths"]->value())
                             .arg(quants.size()));
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
    });

    // ── Distribution Fit ─────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("distribution_fit"), .cost = ViewCost::Fetch, .loading = tr("Fitting candidate distributions…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Distribution Fit"),
                 [this, add_sample_btn](QWidget* df, QVBoxLayout* dfl) {
        // Daily returns of the symbol (D1): the tail model of its return distribution.
        add_series_source(df, dfl, SeriesShape::Single);

        auto* df_vals = new QLineEdit(df);
        df_vals->setPlaceholderText(
            tr("Numeric values (>= 30). Fits normal, student-t, lognormal (positive only), skewnormal."));
        df_vals->setStyleSheet(input_ss());
        text_inputs_["gn_df_values"] = df_vals;
        mark_series_input(df_vals, SeriesKind::Returns);
        dfl->addWidget(build_input_row(tr("Values"), df_vals, df));
        dfl->addWidget(add_sample_btn(df_vals, df, 331, tr("200-pt synthetic series")));

        auto* df_hint =
            new QLabel(tr("Fits 4 candidate distributions and ranks them by AIC + BIC. KS test reports whether each "
                          "fit can be rejected at the 5% level — useful for picking the right tail model."),
                       df);
        df_hint->setWordWrap(true);
        df_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        dfl->addWidget(df_hint);

        auto* df_run = make_run_button(tr("FIT DISTRIBUTIONS"), df);
        set_action(df_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("gn_df_values"), vals, &bad)) {
                display_error(tr("Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 30) {
                display_error(tr("Need at least 30 values; you provided %1.").arg(vals.size()));
                return;
            }
            show_loading(tr("Fitting 4 distributions to %1 values...").arg(vals.size()));
            QJsonObject params;
            params["values"] = vals;
            AIQuantLabService::instance().gluon_distribution_fit(params);
        });
        dfl->addWidget(df_run);
        dfl->addStretch();
    });

    // ── Evaluate Forecast ────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("evaluate"), .cost = ViewCost::Fetch, .loading = tr("Scoring the forecast against what happened…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Evaluate Forecast"),
                 [this, add_sample_btn](QWidget* ef, QVBoxLayout* efl) {
        // D1: the last 20 trading days held out — their closes against the naive forecast made
        // from the history before them (that history enables MASE).
        add_series_source(ef, efl, SeriesShape::Single);

        auto* ef_actuals = new QLineEdit(ef);
        ef_actuals->setPlaceholderText(tr("Realized actuals (>= 5 values)"));
        ef_actuals->setStyleSheet(input_ss());
        text_inputs_["gn_ef_actuals"] = ef_actuals;
        mark_series_input(ef_actuals, SeriesKind::HoldoutActual);
        efl->addWidget(build_input_row(tr("Actuals"), ef_actuals, ef));
        efl->addWidget(add_sample_btn(ef_actuals, ef, 341, tr("100-pt synthetic actuals")));

        auto* ef_point = new QLineEdit(ef);
        ef_point->setPlaceholderText(tr("Point forecast (same length as actuals)"));
        ef_point->setStyleSheet(input_ss());
        text_inputs_["gn_ef_point"] = ef_point;
        mark_series_input(ef_point, SeriesKind::HoldoutNaive);
        efl->addWidget(build_input_row(tr("Point Forecast"), ef_point, ef));
        efl->addWidget(add_sample_btn(ef_point, ef, 342, tr("100-pt synthetic point forecast")));

        auto* ef_lower = new QLineEdit(ef);
        ef_lower->setPlaceholderText(tr("Lower band (optional, same length as actuals)"));
        ef_lower->setStyleSheet(input_ss());
        text_inputs_["gn_ef_lower"] = ef_lower;
        efl->addWidget(build_input_row(tr("Lower Band (optional)"), ef_lower, ef));
        mark_series_widget(ef_lower->parentWidget(), /*ticker_only=*/false); // a ticker run has no band

        auto* ef_upper = new QLineEdit(ef);
        ef_upper->setPlaceholderText(tr("Upper band (optional, same length as actuals)"));
        ef_upper->setStyleSheet(input_ss());
        text_inputs_["gn_ef_upper"] = ef_upper;
        efl->addWidget(build_input_row(tr("Upper Band (optional)"), ef_upper, ef));
        mark_series_widget(ef_upper->parentWidget(), /*ticker_only=*/false);

        auto* ef_training = new QLineEdit(ef);
        ef_training->setPlaceholderText(tr("Training history (optional; enables MASE)"));
        ef_training->setStyleSheet(input_ss());
        text_inputs_["gn_ef_training"] = ef_training;
        mark_series_input(ef_training, SeriesKind::HoldoutTrain);
        efl->addWidget(build_input_row(tr("Training History (optional)"), ef_training, ef));
        efl->addWidget(add_sample_btn(ef_training, ef, 343, tr("200-pt synthetic training history")));

        auto* ef_hint = new QLabel(tr("With a ticker as the source, the last 20 trading days are held out: their "
                                      "closes are scored against the naive forecast (the last close before them, "
                                      "held flat), with the history before them as the training series."),
                                   ef);
        ef_hint->setWordWrap(true);
        ef_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
        mark_series_widget(ef_hint, /*ticker_only=*/true);
        efl->addWidget(ef_hint);

        auto* ef_season = new QSpinBox(ef);
        ef_season->setRange(1, 365);
        ef_season->setValue(1);
        ef_season->setStyleSheet(spinbox_ss());
        int_inputs_["gn_ef_season"] = ef_season;
        efl->addWidget(build_input_row(tr("MASE Seasonal Period"), ef_season, ef));

        auto* ef_target = make_double_spin(50.0, 99.9, 80.0, 1, "%", ef);
        double_inputs_["gn_ef_target"] = ef_target;
        efl->addWidget(build_input_row(tr("Coverage Target"), ef_target, ef));

        auto* ef_run = make_run_button(tr("EVALUATE FORECAST"), ef);
        set_action(ef_run, [this]() {
            QJsonArray actuals, point;
            QString bad;
            if (!parse_doubles(series_text("gn_ef_actuals"), actuals, &bad)) {
                display_error(tr("Actuals: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("gn_ef_point"), point, &bad)) {
                display_error(tr("Point: '%1' is not numeric.").arg(bad));
                return;
            }
            if (actuals.size() < 5) {
                display_error(tr("Need at least 5 actuals; you provided %1.").arg(actuals.size()));
                return;
            }
            if (actuals.size() != point.size()) {
                display_error(
                    tr("Actuals (%1) and point (%2) must have the same length.").arg(actuals.size()).arg(point.size()));
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
                    display_error(tr("Lower band: '%1' is not numeric.").arg(bad));
                    return;
                }
                if (!parse_doubles(hi_text, upper, &bad)) {
                    display_error(tr("Upper band: '%1' is not numeric.").arg(bad));
                    return;
                }
                if (lower.size() != actuals.size() || upper.size() != actuals.size()) {
                    display_error(tr("Lower and upper bands must match actuals length."));
                    return;
                }
                params["lower"] = lower;
                params["upper"] = upper;
            }

            const QString train_text = series_text("gn_ef_training").trimmed();
            if (!train_text.isEmpty()) {
                QJsonArray training;
                if (!parse_doubles(train_text, training, &bad)) {
                    display_error(tr("Training: '%1' is not numeric.").arg(bad));
                    return;
                }
                params["training"] = training;
                params["season"] = int_inputs_["gn_ef_season"]->value();
            }

            show_loading(tr("Evaluating forecast on %1 obs...").arg(actuals.size()));
            AIQuantLabService::instance().gluon_evaluate_forecast(params);
        });
        efl->addWidget(ef_run);
        efl->addStretch();
    });

    // ── Seasonal Naive ───────────────────────────────────────────────────────
    add_view_tab(tabs, {.key = QStringLiteral("seasonal_naive"), .cost = ViewCost::Fetch, .loading = tr("Projecting the naive baseline…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Seasonal Naive"),
                 [this, add_sample_btn](QWidget* sn, QVBoxLayout* snl) {
        add_series_source(sn, snl, SeriesShape::Single); // daily closes of the symbol (D1)

        auto* sn_vals = new QLineEdit(sn);
        sn_vals->setPlaceholderText(tr("Series values (>= 1). Forecast = repeat last `season_length` observations."));
        sn_vals->setStyleSheet(input_ss());
        text_inputs_["gn_sn_values"] = sn_vals;
        mark_series_input(sn_vals, SeriesKind::Closes);
        snl->addWidget(build_input_row(tr("Series Values"), sn_vals, sn));
        snl->addWidget(add_sample_btn(sn_vals, sn, 351, tr("200-pt synthetic series")));

        auto* sn_horizon = new QSpinBox(sn);
        sn_horizon->setRange(1, 365);
        sn_horizon->setValue(14);
        sn_horizon->setStyleSheet(spinbox_ss());
        int_inputs_["gn_sn_horizon"] = sn_horizon;
        snl->addWidget(build_input_row(tr("Forecast Horizon"), sn_horizon, sn));

        // A trading week by default: the last 5 sessions repeated (1 would be a flat line at the
        // last close — no seasonality to show).
        auto* sn_season = new QSpinBox(sn);
        sn_season->setRange(1, 365);
        sn_season->setValue(5);
        sn_season->setToolTip(tr("5 = a trading week of daily prices (each forecast day repeats the same weekday "
                                 "of the last week); 1 = the pure naive forecast, the last value held flat."));
        sn_season->setStyleSheet(spinbox_ss());
        int_inputs_["gn_sn_season"] = sn_season;
        snl->addWidget(build_input_row(tr("Season Length (1 = pure naive)"), sn_season, sn));

        auto* sn_hint = new QLabel(
            tr("Baseline forecaster everyone compares against. season_length=1 is the pure naive 'repeat last value'. "
               "Larger values cycle through the most recent N observations."),
            sn);
        sn_hint->setWordWrap(true);
        sn_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        snl->addWidget(sn_hint);

        auto* sn_run = make_run_button(tr("RUN SEASONAL NAIVE"), sn);
        set_action(sn_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("gn_sn_values"), vals, &bad)) {
                display_error(tr("Series Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.isEmpty()) {
                display_error(tr("Provide at least 1 observation."));
                return;
            }
            const int season = int_inputs_["gn_sn_season"]->value();
            if (season > vals.size()) {
                display_error(tr("Season length (%1) exceeds series length (%2).").arg(season).arg(vals.size()));
                return;
            }
            show_loading(tr("Repeating last %1 obs forward over %2 steps...")
                             .arg(season)
                             .arg(int_inputs_["gn_sn_horizon"]->value()));
            QJsonObject params;
            params["values"] = vals;
            params["horizon"] = int_inputs_["gn_sn_horizon"]->value();
            params["season_length"] = season;
            AIQuantLabService::instance().gluon_seasonal_naive(params);
        });
        snl->addWidget(sn_run);
        snl->addStretch();
    });

    vl->addWidget(tabs);

    add_result_pane(vl);

    return w;
}

// ── GLUONTS: rich card-based display per command ────────────────────────────

void QuantModulePanel::display_gluonts_result(const QString& command, const QJsonObject& payload) {
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

    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_observations"))
        header_text += tr("  |  %1 OBS").arg(d.value("n_observations").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 2. PROBABILISTIC FORECAST ───────────────────────────────────────────
    if (command == "probabilistic_forecast") {
        const int horizon = d.value("horizon").toInt();
        const int n_paths = d.value("n_paths").toInt();
        const QJsonValue last_actual = d.value("last_actual");
        const QJsonValue first_p50 = d.value("first_forecast_p50");
        const QJsonValue last_p50 = d.value("last_forecast_p50");
        const QJsonValue drift_v = d.value("horizon_drift");
        const double drift = drift_v.isDouble() ? drift_v.toDouble() : kQcNaN;
        const QJsonValue first_w = d.value("first_p80_width");
        const QJsonValue last_w = d.value("last_p80_width");

        auto* hdr = new QLabel(tr("HORIZON %1 STEPS  |  %2 BOOTSTRAP PATHS  |  LAGS %3")
                                   .arg(horizon)
                                   .arg(n_paths)
                                   .arg(d.value("lags").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card(tr("LAST ACTUAL"), fmt_num_safe(last_actual, 4), this),
            gs_make_card(tr("FIRST P50"), fmt_num_safe(first_p50, 4), this,
                         gl_delta_color(gl_delta(first_p50, last_actual))),
            gs_make_card(tr("LAST P50"), fmt_num_safe(last_p50, 4), this, gl_delta_color(drift)),
            gs_make_card(tr("HORIZON Δ"), fmt_num_safe(drift_v, 4), this, gl_delta_color(drift)),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> width = {
            gs_make_card(tr("AVG P80 WIDTH"), fmt_num_safe(d.value("mean_p80_width"), 4), this, ui::colors::WARNING()),
            gs_make_card(tr("FIRST P80 WIDTH"), fmt_num_safe(first_w, 4), this),
            gs_make_card(tr("LAST P80 WIDTH"), fmt_num_safe(last_w, 4), this,
                         !(first_w.isDouble() && last_w.isDouble()) ? ui::colors::TEXT_PRIMARY()
                         : last_w.toDouble() > first_w.toDouble() * 2 ? ui::colors::NEGATIVE()
                                                                       : ui::colors::WARNING()),
            gs_make_card(tr("RESIDUAL σ"), fmt_num_safe(d.value("residual_std"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(width, this));

        // Forecast fan: P10-P90 and P25-P75 bands opening from the last actual value,
        // P50 as the central line (the script sends no history), then every step below.
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            QcForecast f;
            f.title = tr("PROBABILISTIC FORECAST  ·  P10-P90 / P25-P75 BANDS, P50 LINE  ·  FROM THE LAST ACTUAL");
            f.fc_x = qc_column(forecast, QStringLiteral("step"));
            f.point = qc_column(forecast, QStringLiteral("p50"));
            QcBand outer;
            outer.lower_pct = 10;
            outer.upper_pct = 90;
            outer.lower = qc_column(forecast, QStringLiteral("p10"));
            outer.upper = qc_column(forecast, QStringLiteral("p90"));
            QcBand inner;
            inner.lower_pct = 25;
            inner.upper_pct = 75;
            inner.lower = qc_column(forecast, QStringLiteral("p25"));
            inner.upper = qc_column(forecast, QStringLiteral("p75"));
            f.bands = {outer, inner};
            f.anchor_x = 0.0;
            f.anchor_y = last_actual.toDouble(kQcNaN);
            results_layout_->addWidget(qc_forecast_chart(f, this));

            auto* table = new QTableWidget(forecast.size(), 8, this);
            table->setHorizontalHeaderLabels(
                {tr("Step"), tr("P10"), tr("P25"), tr("P50"), tr("Mean"), tr("P75"), tr("P90"), tr("P80 Width")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * row_px(22) + row_px(32), row_px(220)));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("step").toInt())));
                table->setItem(i, 1, gl_num_cell(o.value("p10"), 4, ui::colors::NEGATIVE()));
                table->setItem(i, 2, gl_num_cell(o.value("p25"), 4, ui::colors::WARNING()));
                table->setItem(i, 3, gl_num_cell(o.value("p50"), 4));
                table->setItem(i, 4, gl_num_cell(o.value("mean"), 4, ui::colors::INFO()));
                table->setItem(i, 5, gl_num_cell(o.value("p75"), 4, ui::colors::WARNING()));
                table->setItem(i, 6, gl_num_cell(o.value("p90"), 4, ui::colors::POSITIVE()));
                table->setItem(i, 7, gl_num_cell(o.value("width_80"), 4));
                table->setRowHeight(i, row_px(22));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("Forecast: P50 %1 → %2  |  P80 width %3 → %4")
                                   .arg(fmt_num_safe(first_p50, 3), fmt_num_safe(last_p50, 3), fmt_num_safe(first_w, 3),
                                        fmt_num_safe(last_w, 3)));
        return;
    }

    // ── 3. QUANTILE FORECAST ────────────────────────────────────────────────
    if (command == "quantile_forecast") {
        const int horizon = d.value("horizon").toInt();
        const int n_q = d.value("n_quantiles").toInt();

        auto* hdr = new QLabel(tr("HORIZON %1 STEPS  |  %2 QUANTILES  |  %3 BOOTSTRAP PATHS")
                                   .arg(horizon)
                                   .arg(n_q)
                                   .arg(d.value("n_paths").toInt()));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card(tr("LAST ACTUAL"), fmt_num_safe(d.value("last_actual"), 4), this),
            gs_make_card(tr("LAGS"), QString::number(d.value("lags").toInt()), this),
            gs_make_card(tr("RESIDUAL σ"), fmt_num_safe(d.value("residual_std"), 4), this),
            gs_make_card(tr("OBSERVATIONS"), QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        // Per-quantile summary cards (up to 4 at a time)
        const QJsonArray q_summary = d.value("quantile_summary").toArray();
        if (!q_summary.isEmpty()) {
            auto* lbl = new QLabel(tr("PER-QUANTILE SUMMARY"));
            lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 0 0 2px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(lbl);
            auto* table = new QTableWidget(q_summary.size(), 6, this);
            table->setHorizontalHeaderLabels(
                {tr("Quantile"), tr("Mean"), tr("Min"), tr("Max"), tr("First"), tr("Last")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(q_summary.size() * row_px(24) + row_px(32), 240));
            for (int i = 0; i < q_summary.size(); ++i) {
                const auto q = q_summary[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(q.value("label").toString()));
                table->setItem(i, 1, gl_num_cell(q.value("mean"), 4));
                table->setItem(i, 2, gl_num_cell(q.value("min"), 4));
                table->setItem(i, 3, gl_num_cell(q.value("max"), 4));
                table->setItem(i, 4, gl_num_cell(q.value("first"), 4));
                table->setItem(i, 5, gl_num_cell(q.value("last"), 4));
                table->setRowHeight(i, row_px(24));
            }
            results_layout_->addWidget(table);
        }

        // Per-step forecast table — show first quantile row to derive headers
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            // Fan of the requested quantiles: the lowest pairs with the highest, the next
            // with the next; q50 (when requested) is the central line. Opens from the last
            // actual value (the script sends no history).
            // quantile_summary pairs each quantile with the column key the script used.
            QList<QPair<double, QString>> qs;
            for (const auto& v : q_summary) {
                const auto q = v.toObject();
                if (forecast[0].toObject().contains(q.value("label").toString()))
                    qs.append({q.value("quantile").toDouble(), q.value("label").toString()});
            }
            std::sort(qs.begin(), qs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            QcForecast f;
            f.title = tr("QUANTILE FORECAST  ·  FROM THE LAST ACTUAL");
            f.fc_x = qc_column(forecast, QStringLiteral("step"));
            for (qsizetype lo = 0, hi = qs.size() - 1; lo < hi; ++lo, --hi) {
                QcBand b;
                b.lower_pct = qs.at(lo).first * 100.0;
                b.upper_pct = qs.at(hi).first * 100.0;
                b.lower = qc_column(forecast, qs.at(lo).second);
                b.upper = qc_column(forecast, qs.at(hi).second);
                f.bands.append(b);
            }
            for (const auto& q : qs) {
                if (std::abs(q.first - 0.5) < 1e-9)
                    f.point = qc_column(forecast, q.second);
            }
            f.anchor_x = 0.0;
            f.anchor_y = d.value("last_actual").toDouble(kQcNaN);
            if (!f.bands.isEmpty() || !f.point.isEmpty())
                results_layout_->addWidget(qc_forecast_chart(f, this));

            const auto first = forecast[0].toObject();
            QStringList headers = {tr("Step")};
            QStringList keys;
            for (auto it = first.begin(); it != first.end(); ++it) {
                if (it.key() == "step")
                    continue;
                keys << it.key();
                headers << it.key().toUpper();
            }
            auto* lbl = new QLabel(tr("FORECAST BY STEP"));
            lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 0 0 2px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
            results_layout_->addWidget(lbl);
            auto* table = new QTableWidget(forecast.size(), headers.size(), this);
            table->setHorizontalHeaderLabels(headers);
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * row_px(22) + row_px(32), 280));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("step").toInt())));
                for (int c = 0; c < keys.size(); ++c)
                    table->setItem(i, c + 1, gl_num_cell(o.value(keys[c]), 4));
                table->setRowHeight(i, row_px(22));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 quantiles over %2 steps").arg(n_q).arg(horizon));
        return;
    }

    // ── 4. DISTRIBUTION FIT ─────────────────────────────────────────────────
    if (command == "distribution_fit") {
        const QString best_aic = d.value("best_by_aic").toString();
        const QString best_bic = d.value("best_by_bic").toString();

        // best_by_* are null when no candidate could be fitted.
        auto* hdr = new QLabel(tr("BEST BY AIC: %1   |   BEST BY BIC: %2")
                                   .arg(best_aic.isEmpty() ? QString::fromUtf8("—") : best_aic.toUpper(),
                                        best_bic.isEmpty() ? QString::fromUtf8("—") : best_bic.toUpper()));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::POSITIVE()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> stats = {
            gs_make_card(tr("MEAN"), fmt_num_safe(d.value("data_mean"), 4), this),
            gs_make_card(tr("STD"), fmt_num_safe(d.value("data_std"), 4), this),
            gs_make_card(tr("SKEW"), fmt_num_safe(d.value("data_skew"), 3), this,
                         gs_pos_neg_color(d.value("data_skew").toDouble())),
            gs_make_card(tr("KURT"), fmt_num_safe(d.value("data_kurtosis"), 3), this),
        };
        results_layout_->addWidget(gs_card_row(stats, this));

        // Fits table
        const QJsonArray fits = d.value("fits").toArray();
        if (!fits.isEmpty()) {
            auto* table = new QTableWidget(fits.size(), 6, this);
            table->setHorizontalHeaderLabels(
                {tr("Distribution"), tr("Params"), tr("Log-Lik"), tr("AIC"), tr("BIC"), tr("KS p")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(fits.size() * row_px(26) + row_px(32), 240));

            // Find min AIC for highlighting
            double min_aic = std::numeric_limits<double>::infinity();
            for (const auto& v : fits) {
                const auto f = v.toObject();
                if (!f.value("aic").isNull()) {
                    const double a = f.value("aic").toDouble();
                    if (a < min_aic)
                        min_aic = a;
                }
            }

            for (int i = 0; i < fits.size(); ++i) {
                const auto f = fits[i].toObject();
                const QString name = f.value("distribution").toString();
                const bool is_best = (!f.value("aic").isNull() && std::abs(f.value("aic").toDouble() - min_aic) < 1e-9);
                auto* nm = new QTableWidgetItem(is_best ? name + "  ★" : name);
                if (is_best)
                    nm->setForeground(QColor(ui::colors::POSITIVE()));
                table->setItem(i, 0, nm);
                table->setItem(i, 1, new QTableWidgetItem(QString::number(f.value("n_params").toInt())));
                auto add = [&](int col, const QJsonValue& v, int dec) {
                    auto* it = v.isNull() ? new QTableWidgetItem("—")
                                          : new QTableWidgetItem(QString::number(v.toDouble(), 'f', dec));
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(i, col, it);
                };
                add(2, f.value("log_likelihood"), 2);
                add(3, f.value("aic"), 2);
                add(4, f.value("bic"), 2);
                // The KS p-value is null when the fit failed: "—" in the neutral colour, not a
                // 0.0000 painted as a rejection.
                const QJsonValue ks_p = f.value("ks_p_value");
                table->setItem(i, 5,
                               gl_num_cell(ks_p, 4,
                                           f.value("cannot_reject_at_5pct").toBool() ? ui::colors::POSITIVE()
                                                                                     : ui::colors::WARNING()));
                table->setRowHeight(i, row_px(26));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("Best fit: %1 (AIC) / %2 (BIC)")
                                   .arg(best_aic.isEmpty() ? QString::fromUtf8("—") : best_aic,
                                        best_bic.isEmpty() ? QString::fromUtf8("—") : best_bic));
        return;
    }

    // ── 5. EVALUATE FORECAST ────────────────────────────────────────────────
    if (command == "evaluate_forecast") {
        // mape_pct / smape_pct are null when every actual (or actual + forecast) is zero;
        // mase without a usable training history; the interval figures without bands.
        const QJsonValue mape = d.value("mape_pct");
        const QJsonValue smape = d.value("smape_pct");
        const QJsonValue dir_acc = d.value("direction_accuracy_pct");
        const double bias = d.value("bias").toDouble();
        const QJsonValue cov_v = d.value("coverage_pct");
        const bool has_ci = d.value("has_intervals").toBool() && cov_v.isDouble();

        QList<QWidget*> err_row = {
            gs_make_card(tr("MAE"), fmt_num_safe(d.value("mae"), 4), this),
            gs_make_card(tr("RMSE"), fmt_num_safe(d.value("rmse"), 4), this),
            gs_make_card(tr("MSE"), fmt_num_safe(d.value("mse"), 4), this),
            gs_make_card(tr("BIAS"), fmt_num_safe(d.value("bias"), 4), this, gs_pos_neg_color(-std::abs(bias))),
        };
        results_layout_->addWidget(gs_card_row(err_row, this));

        QList<QWidget*> pct_row = {
            gs_make_card(tr("MAPE"), fmt_pts_safe(mape, 2), this, gl_err_color(mape)),
            gs_make_card(tr("SMAPE"), fmt_pts_safe(smape, 2), this, gl_err_color(smape)),
            gs_make_card(tr("MASE"), fmt_num_safe(d.value("mase"), 3), this,
                         !d.value("mase").isDouble()
                             ? ui::colors::TEXT_PRIMARY()
                             : (d.value("mase").toDouble() < 1.0 ? ui::colors::POSITIVE() : ui::colors::WARNING())),
            gs_make_card(tr("DIRECTION ACC"), fmt_pts_safe(dir_acc, 1), this,
                         !dir_acc.isDouble()          ? ui::colors::TEXT_SECONDARY()
                         : dir_acc.toDouble() > 60.0 ? ui::colors::POSITIVE()
                                                     : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(pct_row, this));

        if (has_ci) {
            const double cov = cov_v.toDouble();
            const double target = d.value("coverage_target_pct").toDouble();
            const double cov_gap = cov - target;
            QList<QWidget*> ci_row = {
                gs_make_card(tr("COVERAGE"), fmt_pts_safe(cov_v, 1), this,
                             std::abs(cov_gap) < 5.0 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
                gs_make_card(tr("TARGET"), fmt_pts_safe(d.value("coverage_target_pct"), 1), this, ui::colors::INFO()),
                gs_make_card(tr("AVG WIDTH"), fmt_num_safe(d.value("avg_interval_width"), 4), this),
                gs_make_card(tr("INTERVAL SCORE"), fmt_num_safe(d.value("interval_score"), 4), this,
                             ui::colors::INFO()),
            };
            results_layout_->addWidget(gs_card_row(ci_row, this));

            // Interpretation
            QString verdict;
            QString verdict_col = ui::colors::TEXT_PRIMARY();
            if (std::abs(cov_gap) < 5.0) {
                verdict = tr("Coverage on target — intervals are well-calibrated.");
                verdict_col = ui::colors::POSITIVE();
            } else if (cov_gap > 5.0) {
                verdict = tr("Over-covered by %1pp — intervals are too wide (conservative).").arg(cov_gap, 0, 'f', 1);
                verdict_col = ui::colors::WARNING();
            } else {
                verdict =
                    tr("Under-covered by %1pp — intervals are too narrow (overconfident).").arg(-cov_gap, 0, 'f', 1);
                verdict_col = ui::colors::NEGATIVE();
            }
            auto* il = new QLabel(verdict);
            il->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "padding:8px 10px; background:%2; border-left:3px solid %3;")
                                  .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), verdict_col));
            results_layout_->addWidget(il);
        }

        // (These formats used to read "%3%%", which QString::arg leaves as a doubled "%%".)
        status_label_->setText(has_ci ? tr("MAE %1  RMSE %2  MAPE %3  Coverage %4")
                                            .arg(fmt_num_safe(d.value("mae"), 3), fmt_num_safe(d.value("rmse"), 3),
                                                 fmt_pts_safe(mape, 2), fmt_pts_safe(cov_v, 1))
                                      : tr("MAE %1  RMSE %2  MAPE %3  Bias %4")
                                            .arg(fmt_num_safe(d.value("mae"), 3), fmt_num_safe(d.value("rmse"), 3),
                                                 fmt_pts_safe(mape, 2), fmt_num_safe(d.value("bias"), 4)));
        return;
    }

    // ── 6. SEASONAL NAIVE ───────────────────────────────────────────────────
    if (command == "seasonal_naive") {
        const int horizon = d.value("horizon").toInt();
        const int season = d.value("season_length").toInt();

        auto* hdr = new QLabel(
            tr("METHOD: %1   |   HORIZON %2 STEPS").arg(d.value("method").toString().toUpper()).arg(horizon));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border-left:3px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), accent));
        results_layout_->addWidget(hdr);

        const QJsonValue last_actual = d.value("last_actual");
        const QJsonValue first_fc = d.value("first_forecast");
        const QJsonValue last_fc = d.value("last_forecast");

        QList<QWidget*> top = {
            gs_make_card(tr("LAST ACTUAL"), fmt_num_safe(last_actual, 4), this),
            gs_make_card(tr("FIRST FORECAST"), fmt_num_safe(first_fc, 4), this,
                         gl_delta_color(gl_delta(first_fc, last_actual))),
            gs_make_card(tr("LAST FORECAST"), fmt_num_safe(last_fc, 4), this,
                         gl_delta_color(gl_delta(last_fc, last_actual))),
            gs_make_card(tr("SEASON"), tr("%1 obs").arg(season), this, ui::colors::INFO()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> bounds = {
            gs_make_card(tr("FORECAST MIN"), fmt_num_safe(d.value("forecast_min"), 4), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("FORECAST MEAN"), fmt_num_safe(d.value("forecast_mean"), 4), this),
            gs_make_card(tr("FORECAST MAX"), fmt_num_safe(d.value("forecast_max"), 4), this, ui::colors::POSITIVE()),
            gs_make_card(tr("OBSERVATIONS"), QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(bounds, this));

        // Forecast path from the last actual value (no interval: a naive baseline), then every step.
        const QJsonArray forecast = d.value("forecast").toArray();
        if (!forecast.isEmpty()) {
            QcForecast f;
            f.title = tr("%1 FORECAST  ·  FROM THE LAST ACTUAL").arg(d.value("method").toString().toUpper());
            f.fc_x = qc_column(forecast, QStringLiteral("step"));
            f.point = qc_column(forecast, QStringLiteral("value"));
            f.point_name = tr("Naive forecast");
            f.anchor_x = 0.0;
            f.anchor_y = last_actual.toDouble(kQcNaN);
            results_layout_->addWidget(qc_forecast_chart(f, this));

            auto* table = new QTableWidget(forecast.size(), 2, this);
            table->setHorizontalHeaderLabels({tr("Step"), tr("Value")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(forecast.size() * row_px(22) + row_px(32), 320));
            for (int i = 0; i < forecast.size(); ++i) {
                const auto o = forecast[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(QString::number(o.value("step").toInt())));
                table->setItem(i, 1,
                               gl_num_cell(o.value("value"), 4,
                                           gl_delta_color(gl_delta(o.value("value"), last_actual))));
                table->setRowHeight(i, row_px(22));
            }
            results_layout_->addWidget(table);
        }
        status_label_->setText(tr("%1 — last %2 → forecast %3")
                                   .arg(d.value("method").toString(), fmt_num_safe(last_actual, 3),
                                        fmt_num_safe(first_fc, 3)));
        return;
    }

    // ── Fallback ─────────────────────────────────────────────────────────────
    display_result(d);
}

} // namespace fincept::screens
