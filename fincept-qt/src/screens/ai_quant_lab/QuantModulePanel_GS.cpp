// src/screens/ai_quant_lab/QuantModulePanel_GS.cpp
//
// GS Quant panel — 8 analysis modes (risk metrics, portfolio analytics, greeks,
// VaR, stress test, backtest, statistics, component VaR). Extracted from
// QuantModulePanel.cpp to keep that file maintainable.
//
// The script returns null for a value it cannot compute (an Omega ratio with no
// losing period, a recovery that has not happened); figures go through the
// *_safe formatters so null reads "—", never 0.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_AdvancedHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_ResultCharts.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "ui/charts/BarChart.h"
#include "ui/tables/BarCellDelegate.h"
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
#include <QPair>
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

// Currency amount; null → "—".
QString gs_money_safe(const QJsonValue& v) {
    return v.isDouble() ? gs_fmt_money(v.toDouble()) : QString::fromUtf8("—");
}

// A sortable cell: `text` shown, `v` sorted on; null sorts first.
QTableWidgetItem* gs_cell(const QJsonValue& v, const QString& text, const QString& color = {}) {
    return num_item(text, v.isDouble() ? v.toDouble() : -std::numeric_limits<double>::infinity(), color);
}

// Threshold colour of a ratio that may be null (null → neutral).
QString gs_ratio_color(const QJsonValue& v) {
    if (!v.isDouble())
        return ui::colors::TEXT_SECONDARY();
    const double x = v.toDouble();
    return x >= 1.0 ? QString(ui::colors::POSITIVE()) : x >= 0 ? QString(ui::colors::TEXT_PRIMARY())
                                                               : QString(ui::colors::NEGATIVE());
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// GS QUANT PANEL — 8 analysis modes
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_gs_quant_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = make_view_tabs(w);

    // Helper: small "load sample" link button next to a CSV input
    // Shared helper: synthetic-sample button styled by the panel sheet; runs
    // fed with its data are flagged SAMPLE INPUT on the results.
    auto add_sample_btn = [this](QLineEdit* edit, QWidget* parent, unsigned seed, const QString& tip) -> QPushButton* {
        return make_sample_button(edit, parent, tip, [seed]() { return sample_returns(seed); });
    };

    // ── Risk Metrics ──
    add_view_tab(tabs, {.key = QStringLiteral("risk_metrics"), .cost = ViewCost::Fetch, .loading = tr("Computing risk metrics…")}, tr("Risk Metrics"),
                 [this, add_sample_btn](QWidget* risk, QVBoxLayout* rvl) {
        // Real data by default (D1): the symbol's daily returns; the box is the Custom series.
        add_series_source(risk, rvl, SeriesShape::Single);
        auto* risk_returns = new QLineEdit(risk);
        risk_returns->setPlaceholderText(
            tr("Daily returns: comma-, space-, or newline-separated. Need at least 5 values."));
        risk_returns->setStyleSheet(input_ss());
        text_inputs_["gs_risk_returns"] = risk_returns;
        mark_series_input(risk_returns, SeriesKind::Returns);
        rvl->addWidget(build_input_row(tr("Daily Returns"), risk_returns, risk));
        rvl->addWidget(
            add_sample_btn(risk_returns, risk, 11, tr("Inserts 252 synthetic daily returns (mu=0.05%, sigma=1.2%)")));
        auto* risk_rf = make_double_spin(0, 20, 4.5, 2, "%", risk);
        double_inputs_["gs_risk_rf"] = risk_rf;
        rvl->addWidget(build_input_row(tr("Risk-Free Rate"), risk_rf, risk));
        auto* risk_run = make_run_button(tr("CALCULATE RISK METRICS"), risk);
        set_action(risk_run, [this]() {
            QJsonArray rets;
            QString bad;
            if (!parse_doubles(series_text("gs_risk_returns"), rets, &bad)) {
                display_error(tr("Daily Returns: '%1' is not a number. "
                                 "Use comma-, space-, or newline-separated decimals (e.g. 0.01, -0.02, 0.005).")
                                  .arg(bad));
                return;
            }
            if (rets.size() < 5) {
                display_error(tr("Need at least 5 daily returns; you provided %1. "
                                 "Click LOAD SYNTHETIC SAMPLE to insert 252 synthetic values.")
                                  .arg(rets.size()));
                return;
            }
            show_loading(tr("Calculating risk metrics on %1 observations...").arg(rets.size()));
            QJsonObject params;
            params["returns"] = rets;
            params["risk_free_rate"] = double_inputs_["gs_risk_rf"]->value() / 100.0;
            AIQuantLabService::instance().gs_risk_metrics(params);
        });
        rvl->addWidget(risk_run);
        rvl->addStretch();
    });

    // ── Portfolio Analytics ──
    add_view_tab(tabs, {.key = QStringLiteral("portfolio"), .cost = ViewCost::Fetch, .loading = tr("Analysing the portfolio against its benchmark…")}, tr("Portfolio"),
                 [this, add_sample_btn](QWidget* port, QVBoxLayout* pvl) {
        add_series_source(port, pvl, SeriesShape::Pair, tr("Benchmark"));
        auto* port_ret = new QLineEdit(port);
        port_ret->setPlaceholderText(tr("Portfolio daily returns (decimals, same length as benchmark)"));
        port_ret->setStyleSheet(input_ss());
        text_inputs_["gs_port_returns"] = port_ret;
        mark_series_input(port_ret, SeriesKind::Returns, 0);
        pvl->addWidget(build_input_row(tr("Portfolio Returns"), port_ret, port));
        pvl->addWidget(
            add_sample_btn(port_ret, port, 21, tr("Inserts 252 synthetic portfolio returns (slight outperformance)")));
        auto* bench_ret = new QLineEdit(port);
        bench_ret->setPlaceholderText(tr("Benchmark daily returns (decimals)"));
        bench_ret->setStyleSheet(input_ss());
        text_inputs_["gs_bench_returns"] = bench_ret;
        mark_series_input(bench_ret, SeriesKind::Returns, 1);
        pvl->addWidget(build_input_row(tr("Benchmark Returns"), bench_ret, port));
        pvl->addWidget(add_sample_btn(bench_ret, port, 22, tr("Inserts 252 synthetic benchmark returns")));
        auto* port_rf = make_double_spin(0, 20, 4.5, 2, "%", port);
        double_inputs_["gs_port_rf"] = port_rf;
        pvl->addWidget(build_input_row(tr("Risk-Free Rate"), port_rf, port));
        auto* port_run = make_run_button(tr("ANALYZE PORTFOLIO"), port);
        set_action(port_run, [this]() {
            QJsonArray prets, brets;
            QString bad;
            if (!parse_doubles(series_text("gs_port_returns"), prets, &bad)) {
                display_error(tr("Portfolio Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (!parse_doubles(series_text("gs_bench_returns"), brets, &bad)) {
                display_error(tr("Benchmark Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (prets.size() < 5 || brets.size() < 5) {
                display_error(tr("Need at least 5 observations for both portfolio and benchmark returns."));
                return;
            }
            if (prets.size() != brets.size()) {
                display_error(
                    tr("Portfolio (%1) and benchmark (%2) must have the same length.").arg(prets.size()).arg(brets.size()));
                return;
            }
            show_loading(tr("Analyzing portfolio vs benchmark on %1 observations...").arg(prets.size()));
            QJsonObject params;
            params["returns"] = prets;
            params["benchmark_returns"] = brets;
            params["risk_free_rate"] = double_inputs_["gs_port_rf"]->value() / 100.0;
            AIQuantLabService::instance().gs_portfolio_analytics(params);
        });
        pvl->addWidget(port_run);
        pvl->addStretch();
    });

    // ── Options Greeks ──
    add_view_tab(tabs, {.key = QStringLiteral("greeks"), .cost = ViewCost::Free, .loading = tr("Pricing the option and its Greeks…"), .shape = ui::LoadingSkeleton::Shape::Cards}, tr("Greeks"),
                 [this](QWidget* greeks, QVBoxLayout* gvl) {
        auto* g_spot = make_double_spin(0.01, 1e6, 100, 2, "", greeks);
        double_inputs_["gs_spot"] = g_spot;
        gvl->addWidget(build_input_row(tr("Spot Price ($)"), g_spot, greeks));
        auto* g_strike = make_double_spin(0.01, 1e6, 100, 2, "", greeks);
        double_inputs_["gs_strike"] = g_strike;
        gvl->addWidget(build_input_row(tr("Strike Price ($)"), g_strike, greeks));
        auto* g_expiry = make_double_spin(0.1, 120, 6, 1, " mo", greeks);
        double_inputs_["gs_expiry"] = g_expiry;
        gvl->addWidget(build_input_row(tr("Expiry (months)"), g_expiry, greeks));
        auto* g_rate = make_double_spin(0, 30, 5, 2, "%", greeks);
        double_inputs_["gs_rate"] = g_rate;
        gvl->addWidget(build_input_row(tr("Risk-Free Rate"), g_rate, greeks));
        auto* g_vol = make_double_spin(0.1, 200, 25, 1, "%", greeks);
        double_inputs_["gs_vol"] = g_vol;
        gvl->addWidget(build_input_row(tr("Volatility"), g_vol, greeks));
        auto* g_type = new QComboBox(greeks);
        g_type->addItems({"call", "put"});
        g_type->setStyleSheet(combo_ss());
        combo_inputs_["gs_option_type"] = g_type;
        gvl->addWidget(build_input_row(tr("Option Type"), g_type, greeks));
        auto* greeks_run = make_run_button(tr("CALCULATE GREEKS"), greeks);
        set_action(greeks_run, [this]() {
            show_loading(tr("Calculating Black-Scholes Greeks..."));
            QJsonObject params;
            params["spot"] = double_inputs_["gs_spot"]->value();
            params["strike"] = double_inputs_["gs_strike"]->value();
            params["expiry"] = double_inputs_["gs_expiry"]->value() / 12.0;
            params["rate"] = double_inputs_["gs_rate"]->value() / 100.0;
            params["vol"] = double_inputs_["gs_vol"]->value() / 100.0;
            params["option_type"] = combo_inputs_["gs_option_type"]->currentText();
            AIQuantLabService::instance().gs_greeks(params);
        });
        gvl->addWidget(greeks_run);
        gvl->addStretch();
    });

    // ── VaR Analysis ──
    add_view_tab(tabs, {.key = QStringLiteral("var"), .cost = ViewCost::Fetch, .loading = tr("Computing VaR and CVaR…")}, tr("VaR"),
                 [this, add_sample_btn](QWidget* var, QVBoxLayout* vvl) {
        add_series_source(var, vvl, SeriesShape::Single);
        auto* var_ret = new QLineEdit(var);
        var_ret->setPlaceholderText(tr("Daily returns (decimals). Need at least 30 values for stable VaR."));
        var_ret->setStyleSheet(input_ss());
        text_inputs_["gs_var_returns"] = var_ret;
        mark_series_input(var_ret, SeriesKind::Returns);
        vvl->addWidget(build_input_row(tr("Daily Returns"), var_ret, var));
        vvl->addWidget(add_sample_btn(var_ret, var, 31, tr("Inserts 252 synthetic daily returns")));
        auto* var_pos = make_double_spin(1, 1e12, 1e6, 0, "", var);
        double_inputs_["gs_var_position"] = var_pos;
        vvl->addWidget(build_input_row(tr("Position Value ($)"), var_pos, var));
        auto* var_conf = make_double_spin(0.80, 0.999, 0.95, 3, "", var);
        double_inputs_["gs_var_confidence"] = var_conf;
        vvl->addWidget(build_input_row(tr("Confidence Level"), var_conf, var));
        auto* var_run = make_run_button(tr("CALCULATE VaR"), var);
        set_action(var_run, [this]() {
            QJsonArray rets;
            QString bad;
            if (!parse_doubles(series_text("gs_var_returns"), rets, &bad)) {
                display_error(tr("Daily Returns: '%1' is not numeric.").arg(bad));
                return;
            }
            if (rets.size() < 30) {
                display_error(
                    tr("VaR needs at least 30 observations for a stable estimate; you provided %1.").arg(rets.size()));
                return;
            }
            show_loading(tr("Computing parametric, historical, MC and CVaR on %1 observations...").arg(rets.size()));
            QJsonObject params;
            params["returns"] = rets;
            params["position_value"] = double_inputs_["gs_var_position"]->value();
            params["confidence"] = double_inputs_["gs_var_confidence"]->value();
            AIQuantLabService::instance().gs_var_analysis(params);
        });
        vvl->addWidget(var_run);
        vvl->addStretch();
    });

    // ── Stress Test ──
    add_view_tab(tabs, {.key = QStringLiteral("stress_test"), .cost = ViewCost::Free, .loading = tr("Applying 9 historical crisis scenarios…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Stress Test"),
                 [this](QWidget* stress, QVBoxLayout* svl) {
        auto* st_pos = make_double_spin(1, 1e12, 1e6, 0, "", stress);
        double_inputs_["gs_stress_position"] = st_pos;
        svl->addWidget(build_input_row(tr("Portfolio Value ($)"), st_pos, stress));
        auto* st_eq = make_double_spin(0, 100, 60, 1, "%", stress);
        double_inputs_["gs_stress_eq"] = st_eq;
        svl->addWidget(build_input_row(tr("Equity Allocation"), st_eq, stress));
        auto* st_bd = make_double_spin(0, 100, 30, 1, "%", stress);
        double_inputs_["gs_stress_bd"] = st_bd;
        svl->addWidget(build_input_row(tr("Bond Allocation"), st_bd, stress));
        auto* st_cm = make_double_spin(0, 100, 10, 1, "%", stress);
        double_inputs_["gs_stress_cm"] = st_cm;
        svl->addWidget(build_input_row(tr("Commodity Allocation"), st_cm, stress));
        auto* st_hint = new QLabel(tr("Applies 9 historical crisis scenarios (2008, COVID-19, Dot-Com, etc.) to the "
                                      "weighted portfolio. Allocations need not sum to 100% — they will be normalized."),
                                   stress);
        st_hint->setWordWrap(true);
        st_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        svl->addWidget(st_hint);
        auto* stress_run = make_run_button(tr("RUN STRESS TEST"), stress);
        set_action(stress_run, [this]() {
            const double eq = double_inputs_["gs_stress_eq"]->value();
            const double bd = double_inputs_["gs_stress_bd"]->value();
            const double cm = double_inputs_["gs_stress_cm"]->value();
            if (eq + bd + cm <= 0) {
                display_error(tr("At least one allocation must be greater than zero."));
                return;
            }
            show_loading(tr("Running 9 historical crisis scenarios..."));
            QJsonObject params;
            params["position_value"] = double_inputs_["gs_stress_position"]->value();
            QJsonObject mix;
            mix["equity"] = eq;
            mix["bond"] = bd;
            mix["commodity"] = cm;
            params["mix"] = mix;
            AIQuantLabService::instance().gs_stress_test(params);
        });
        svl->addWidget(stress_run);
        svl->addStretch();
    });

    // ── Backtest ──
    add_view_tab(tabs, {.key = QStringLiteral("backtest"), .cost = ViewCost::Fetch, .loading = tr("Downloading prices and running the backtest…"), .shape = ui::LoadingSkeleton::Shape::Chart}, tr("Backtest"),
                 [this](QWidget* bt, QVBoxLayout* bvl) {
        auto* bt_strat = new QComboBox(bt);
        bt_strat->addItems({"buy_and_hold", "momentum", "mean_reversion", "rebalancing"});
        bt_strat->setStyleSheet(combo_ss());
        combo_inputs_["gs_bt_strategy"] = bt_strat;
        bvl->addWidget(build_input_row(tr("Strategy"), bt_strat, bt));
        auto* bt_ticker = new QLineEdit(bt);
        bt_ticker->setPlaceholderText(tr("e.g. AAPL — fetched from Yahoo Finance"));
        bt_ticker->setStyleSheet(input_ss());
        bt_ticker->setText(series_default_symbol());
        text_inputs_["gs_bt_ticker"] = bt_ticker;
        bvl->addWidget(build_input_row(tr("Ticker"), bt_ticker, bt));
        auto* bt_period = new QComboBox(bt);
        bt_period->addItems({"1y", "2y", "3y", "5y", "10y", "max"});
        bt_period->setCurrentText("2y");
        bt_period->setStyleSheet(combo_ss());
        combo_inputs_["gs_bt_period"] = bt_period;
        bvl->addWidget(build_input_row(tr("History Period"), bt_period, bt));
        auto* bt_capital = make_double_spin(1000, 1e12, 100000, 0, "", bt);
        double_inputs_["gs_bt_capital"] = bt_capital;
        bvl->addWidget(build_input_row(tr("Initial Capital ($)"), bt_capital, bt));
        auto* bt_lookback = new QSpinBox(bt);
        bt_lookback->setRange(5, 252);
        bt_lookback->setValue(20);
        bt_lookback->setStyleSheet(spinbox_ss());
        int_inputs_["gs_bt_lookback"] = bt_lookback;
        bvl->addWidget(build_input_row(tr("Lookback Period (days)"), bt_lookback, bt));
        auto* bt_comm = make_double_spin(0, 5, 0.10, 3, "%", bt);
        double_inputs_["gs_bt_comm"] = bt_comm;
        bvl->addWidget(build_input_row(tr("Commission per Trade"), bt_comm, bt));
        auto* bt_run = make_run_button(tr("RUN BACKTEST"), bt);
        set_action(bt_run, [this]() {
            const QString tk = text_inputs_["gs_bt_ticker"]->text().trimmed();
            if (tk.isEmpty()) {
                display_error(tr("Enter a ticker (e.g. AAPL, SPY, MSFT) to fetch price history."));
                return;
            }
            show_loading(tr("Fetching %1 history from yfinance and running %2 backtest...")
                             .arg(tk, combo_inputs_["gs_bt_strategy"]->currentText()));
            QJsonObject params;
            params["strategy"] = combo_inputs_["gs_bt_strategy"]->currentText();
            params["ticker"] = tk.toUpper();
            params["period"] = combo_inputs_["gs_bt_period"]->currentText();
            params["initial_capital"] = double_inputs_["gs_bt_capital"]->value();
            params["lookback"] = int_inputs_["gs_bt_lookback"]->value();
            params["commission"] = double_inputs_["gs_bt_comm"]->value() / 100.0;
            AIQuantLabService::instance().gs_backtest(params);
        });
        bvl->addWidget(bt_run);
        bvl->addStretch();
    });

    // ── Statistics ──
    add_view_tab(tabs, {.key = QStringLiteral("statistics"), .cost = ViewCost::Fetch, .loading = tr("Computing descriptive statistics…")}, tr("Statistics"),
                 [this, add_sample_btn](QWidget* stats, QVBoxLayout* stvl) {
        add_series_source(stats, stvl, SeriesShape::Single);
        auto* st_vals = new QLineEdit(stats);
        st_vals->setPlaceholderText(tr("Numeric values (e.g. 10.5, 11.2, 9.8, 12.1, ...). Need at least 2."));
        st_vals->setStyleSheet(input_ss());
        text_inputs_["gs_stats_values"] = st_vals;
        mark_series_input(st_vals, SeriesKind::Closes);
        stvl->addWidget(build_input_row(tr("Values"), st_vals, stats));
        stvl->addWidget(add_sample_btn(st_vals, stats, 41, tr("Inserts 100 synthetic observations (mean ~50, sd ~5)")));
        auto* stats_run = make_run_button(tr("CALCULATE STATISTICS"), stats);
        set_action(stats_run, [this]() {
            QJsonArray vals;
            QString bad;
            if (!parse_doubles(series_text("gs_stats_values"), vals, &bad)) {
                display_error(tr("Values: '%1' is not numeric.").arg(bad));
                return;
            }
            if (vals.size() < 2) {
                display_error(tr("Need at least 2 values; you provided %1.").arg(vals.size()));
                return;
            }
            show_loading(tr("Computing descriptive statistics on %1 values...").arg(vals.size()));
            QJsonObject params;
            params["values"] = vals;
            AIQuantLabService::instance().gs_statistics(params);
        });
        stvl->addWidget(stats_run);
        stvl->addStretch();
    });

    // ── Component VaR ──
    // Tickers fetched by the script (like Backtest), weights in ticker order.
    add_view_tab(tabs, {.key = QStringLiteral("component_var"), .cost = ViewCost::Fetch, .loading = tr("Downloading prices and decomposing the VaR…"), .shape = ui::LoadingSkeleton::Shape::Table}, tr("Component VaR"),
                 [this](QWidget* cvt, QVBoxLayout* cvvl) {
        auto* cv_tickers = new QLineEdit(cvt);
        cv_tickers->setPlaceholderText(tr("Tickers (comma-separated, >= 2) — daily closes fetched from Yahoo Finance"));
        cv_tickers->setText(QStringLiteral("AAPL,MSFT,GOOG,TSLA"));
        cv_tickers->setStyleSheet(input_ss());
        text_inputs_["gs_cv_tickers"] = cv_tickers;
        cvvl->addWidget(build_input_row(tr("Tickers"), cv_tickers, cvt));
        auto* cv_period = new QComboBox(cvt);
        cv_period->addItems({"6mo", "1y", "2y", "3y", "5y"});
        cv_period->setCurrentText("1y");
        cv_period->setStyleSheet(combo_ss());
        combo_inputs_["gs_cv_period"] = cv_period;
        cvvl->addWidget(build_input_row(tr("History Period"), cv_period, cvt));
        auto* cv_weights = new QLineEdit(cvt);
        cv_weights->setPlaceholderText(
            tr("Weights in ticker order, e.g. 0.4, 0.3, 0.2, 0.1. Blank = equal weight; rescaled if they do not sum to 1."));
        cv_weights->setStyleSheet(input_ss());
        text_inputs_["gs_cv_weights"] = cv_weights;
        cvvl->addWidget(build_input_row(tr("Weights"), cv_weights, cvt));
        auto* cv_pos = make_double_spin(1, 1e12, 1e6, 0, "", cvt);
        double_inputs_["gs_cv_position"] = cv_pos;
        cvvl->addWidget(build_input_row(tr("Portfolio Value ($)"), cv_pos, cvt));
        auto* cv_conf = make_double_spin(0.80, 0.999, 0.95, 3, "", cvt);
        double_inputs_["gs_cv_confidence"] = cv_conf;
        cvvl->addWidget(build_input_row(tr("Confidence Level"), cv_conf, cvt));
        auto* cv_horizon = new QSpinBox(cvt);
        cv_horizon->setRange(1, 250);
        cv_horizon->setValue(1);
        cv_horizon->setSuffix(tr(" days"));
        cv_horizon->setStyleSheet(spinbox_ss());
        int_inputs_["gs_cv_horizon"] = cv_horizon;
        cvvl->addWidget(build_input_row(tr("Horizon"), cv_horizon, cvt));
        auto* cv_hint = new QLabel(tr("Parametric (delta-normal, zero-mean) VaR of the weighted portfolio, split into "
                                      "per-asset contributions (Euler allocation): the components add up to the "
                                      "portfolio VaR. A negative contribution is a hedge."),
                                   cvt);
        cv_hint->setWordWrap(true);
        cv_hint->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        cvvl->addWidget(cv_hint);
        auto* cv_run = make_run_button(tr("DECOMPOSE VaR"), cvt);
        set_action(cv_run, [this]() {
            QStringList tickers;
            for (const auto& v : parse_csv_strings(text_inputs_["gs_cv_tickers"]->text()))
                tickers << v.toString().toUpper();
            if (tickers.size() < 2) {
                display_error(tr("Enter at least 2 tickers (e.g. AAPL,MSFT,GOOG) to fetch price history."));
                return;
            }
            QJsonObject params;
            const QString w_text = text_inputs_["gs_cv_weights"]->text().trimmed();
            if (!w_text.isEmpty()) {
                QJsonArray w;
                QString bad;
                if (!parse_doubles(w_text, w, &bad)) {
                    display_error(tr("Weights: '%1' is not numeric.").arg(bad));
                    return;
                }
                if (w.size() != tickers.size()) {
                    display_error(tr("Weights (%1) must match the number of tickers (%2), in the same order.")
                                      .arg(w.size())
                                      .arg(tickers.size()));
                    return;
                }
                params["weights"] = w;
            }
            const double conf = double_inputs_["gs_cv_confidence"]->value();
            show_loading(tr("Fetching %1 from Yahoo Finance and decomposing the %2% VaR...")
                             .arg(tickers.join(QStringLiteral(", ")))
                             .arg(conf * 100.0, 0, 'f', 1));
            params["tickers"] = tickers.join(QLatin1Char(','));
            params["period"] = combo_inputs_["gs_cv_period"]->currentText();
            params["position_value"] = double_inputs_["gs_cv_position"]->value();
            params["confidence"] = conf;
            params["horizon_days"] = int_inputs_["gs_cv_horizon"]->value();
            AIQuantLabService::instance().gs_component_var(params);
        });
        cvvl->addWidget(cv_run);
        cvvl->addStretch();
    });

    vl->addWidget(tabs);

    add_result_pane(vl);

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GS QUANT — display_gs_result: rich card-based display per command
// ═══════════════════════════════════════════════════════════════════════════════

void QuantModulePanel::display_gs_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    // Unified error path
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

    // Header strip — operation name + observation count when available
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_observations"))
        header_text += tr("  |  %1 OBS").arg(d.value("n_observations").toInt());
    else if (d.contains("n_scenarios"))
        header_text += tr("  |  %1 SCENARIOS").arg(d.value("n_scenarios").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 1. RISK METRICS ──────────────────────────────────────────────────────
    if (command == "risk_metrics") {
        const QJsonValue sharpe = d.value("sharpe_ratio");
        const QJsonValue sortino = d.value("sortino_ratio");
        const QJsonValue calmar = d.value("calmar_ratio");
        const QJsonValue mdd_v = d.value("max_drawdown");
        const double mdd = mdd_v.toDouble();
        const auto days = [](const QJsonValue& v) {
            return v.isDouble() ? tr("%1 days").arg(v.toInt()) : QString::fromUtf8("—");
        };

        // Sortino / Calmar / Omega are null when undefined (no downside, no drawdown, no
        // losing period) — "—", not a 0.000 that reads as a terrible ratio.
        QList<QWidget*> ratios = {
            gs_make_card(tr("SHARPE"), fmt_num_safe(sharpe, 3), this, gs_ratio_color(sharpe)),
            gs_make_card(tr("SORTINO"), fmt_num_safe(sortino, 3), this, gs_pos_neg_color(sortino.toDouble())),
            gs_make_card(tr("CALMAR"), fmt_num_safe(calmar, 3), this, gs_pos_neg_color(calmar.toDouble())),
            gs_make_card(tr("OMEGA"), fmt_num_safe(d.value("omega_ratio"), 3), this),
        };
        results_layout_->addWidget(gs_card_row(ratios, this));

        QList<QWidget*> vol = {
            gs_make_card(tr("VOLATILITY (ANN)"), fmt_pct_safe(d.value("volatility_annualized")), this),
            gs_make_card(tr("DAILY RISK"), fmt_pct_safe(d.value("daily_risk"), 3), this),
            gs_make_card(tr("DOWNSIDE RISK"), fmt_pct_safe(d.value("downside_risk")), this, ui::colors::WARNING()),
            gs_make_card(tr("MAX DRAWDOWN"), fmt_pct_safe(mdd_v), this,
                         mdd <= -0.20 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(vol, this));

        // Recovery = trough → first new high, over the drawdowns that recovered (null when
        // none did). A drawdown still open at the end is reported separately.
        const QJsonValue ongoing = d.value("ongoing_drawdown_periods_since_trough");
        auto* recovery = gs_make_card(tr("LONGEST RECOVERY"), days(d.value("max_recovery_period")), this);
        recovery->setToolTip(tr("Trough → first new high, longest over the drawdowns that recovered. "
                                "— when no drawdown recovered."));
        QList<QWidget*> var_cards = {
            gs_make_card(tr("VaR 95%"), fmt_pct_safe(d.value("var_95"), 3), this, ui::colors::WARNING()),
            gs_make_card(tr("VaR 99%"), fmt_pct_safe(d.value("var_99"), 3), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("MAX DD LENGTH"), days(d.value("max_drawdown_length")), this),
            recovery,
        };
        results_layout_->addWidget(gs_card_row(var_cards, this));

        QList<QWidget*> dd_cards = {
            gs_make_card(tr("RECOVERED DRAWDOWNS"), fmt_int_safe(d.value("n_recovered_drawdowns")), this),
            gs_make_card(tr("OPEN DRAWDOWN"),
                         ongoing.isDouble() ? tr("%1 days since trough").arg(ongoing.toInt()) : tr("none — at a high"),
                         this, ongoing.isDouble() ? ui::colors::WARNING() : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("OBSERVATIONS"), QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(dd_cards, this));

        // Drawdown timeline strip (only when there was a drawdown at all)
        const QString peak = d.value("peak_date").toString().left(10);
        const QString trough = d.value("trough_date").toString().left(10);
        const QString recov = d.value("recovery_date").toString().left(10);
        if (!peak.isEmpty() && peak != "None" && mdd < 0.0) {
            auto* dd_lbl =
                new QLabel(tr("Drawdown: peak %1  →  trough %2  →  recovery %3")
                               .arg(peak, trough, recov.isEmpty() || recov == "None" ? QStringLiteral("—") : recov));
            dd_lbl->setStyleSheet(
                QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                        "padding:6px 10px; background:%2; border:1px solid %3;")
                    .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
            results_layout_->addWidget(dd_lbl);
        }
        status_label_->setText(tr("Sharpe %1  |  MaxDD %2  |  Vol %3")
                                   .arg(fmt_num_safe(sharpe, 2), fmt_pct_safe(mdd_v, 1),
                                        fmt_pct_safe(d.value("volatility_annualized"), 1)));
        return;
    }

    // ── 2. PORTFOLIO ANALYTICS ───────────────────────────────────────────────
    if (command == "portfolio_analytics") {
        const QJsonValue alpha = d.value("alpha");
        const QJsonValue info = d.value("information_ratio");
        const QJsonValue r2 = d.value("r_squared");
        const QJsonValue hit = d.value("hit_rate"); // percent points

        QList<QWidget*> top = {
            gs_make_card(tr("ALPHA"), fmt_pct_safe(alpha, 2), this, gs_pos_neg_color(alpha.toDouble())),
            gs_make_card(tr("SHARPE"), fmt_num_safe(d.value("sharpe_ratio"), 3), this,
                         gs_pos_neg_color(d.value("sharpe_ratio").toDouble())),
            gs_make_card(tr("SORTINO"), fmt_num_safe(d.value("sortino_ratio"), 3), this),
            gs_make_card(tr("INFO RATIO"), fmt_num_safe(info, 3), this, gs_pos_neg_color(info.toDouble())),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> vsBench = {
            gs_make_card(tr("TRACKING ERROR"), fmt_pct_safe(d.value("tracking_error")), this),
            gs_make_card(tr("R²"), fmt_num_safe(r2, 4), this),
            gs_make_card(tr("HIT RATE"), fmt_pts_safe(hit, 1), this,
                         !hit.isDouble()          ? ui::colors::TEXT_SECONDARY()
                         : hit.toDouble() > 50.0 ? ui::colors::POSITIVE()
                                                 : ui::colors::WARNING()),
            gs_make_card(tr("MAX DRAWDOWN"), fmt_pct_safe(d.value("max_drawdown")), this, ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(vsBench, this));

        QList<QWidget*> capture = {
            gs_make_card(tr("UP CAPTURE"), fmt_pts_safe(d.value("up_capture"), 1), this),
            gs_make_card(tr("DOWN CAPTURE"), fmt_pts_safe(d.value("down_capture"), 1), this),
            gs_make_card(tr("CAPTURE RATIO"), fmt_num_safe(d.value("capture_ratio"), 3), this),
            gs_make_card(tr("ANNUAL RISK"), fmt_pct_safe(d.value("annual_risk")), this),
        };
        results_layout_->addWidget(gs_card_row(capture, this));

        QList<QWidget*> jensen = {
            gs_make_card(tr("JENSEN ALPHA"), fmt_pct_safe(d.value("jensen_alpha")), this,
                         gs_pos_neg_color(d.value("jensen_alpha").toDouble())),
            gs_make_card(tr("JENSEN BULL"), fmt_pct_safe(d.value("jensen_alpha_bull")), this),
            gs_make_card(tr("JENSEN BEAR"), fmt_pct_safe(d.value("jensen_alpha_bear")), this),
            gs_make_card(tr("TREYNOR"), fmt_num_safe(d.value("treynor_measure"), 3), this),
        };
        results_layout_->addWidget(gs_card_row(jensen, this));

        QList<QWidget*> moments = {
            gs_make_card(tr("SKEWNESS"), fmt_num_safe(d.value("skewness"), 3), this),
            gs_make_card(tr("KURTOSIS"), fmt_num_safe(d.value("kurtosis"), 3), this),
            gs_make_card(tr("CALMAR"), fmt_num_safe(d.value("calmar_ratio"), 3), this),
            gs_make_card(tr("MODIGLIANI"), fmt_pct_safe(d.value("modigliani_ratio")), this),
        };
        results_layout_->addWidget(gs_card_row(moments, this));

        status_label_->setText(tr("Alpha %1  |  IR %2  |  R² %3")
                                   .arg(fmt_pct_safe(alpha, 2), fmt_num_safe(info, 2), fmt_num_safe(r2, 3)));
        return;
    }

    // ── 3. GREEKS ────────────────────────────────────────────────────────────
    if (command == "greeks") {
        const QJsonObject g = d.value("greeks").toObject();
        const double spot = d.value("spot").toDouble();
        const double strike = d.value("strike").toDouble();
        const QString opt = d.value("option_type").toString().toUpper();
        const double moneyness = d.value("moneyness").toDouble();
        const QString moneyness_label = moneyness > 1.02 ? tr("ITM") : moneyness < 0.98 ? tr("OTM") : tr("ATM");

        // Contract summary strip
        auto* summary = new QLabel(tr("%1  |  Spot %2  →  Strike %3  |  %4 (%5)  |  Vol %6%  |  T %7y")
                                       .arg(opt)
                                       .arg(spot, 0, 'f', 2)
                                       .arg(strike, 0, 'f', 2)
                                       .arg(moneyness_label)
                                       .arg(moneyness, 0, 'f', 3)
                                       .arg(d.value("vol").toDouble() * 100, 0, 'f', 1)
                                       .arg(d.value("expiry_years").toDouble(), 0, 'f', 3));
        summary->setStyleSheet(
            QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                    "padding:8px 10px; background:%2; border:1px solid %3;")
                .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(summary);

        // First-order Greeks (the four every desk watches)
        QList<QWidget*> first = {
            gs_make_card(tr("DELTA"), fmt_num_safe(g.value("delta"), 4), this,
                         opt == "CALL" ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card(tr("GAMMA"), fmt_num_safe(g.value("gamma"), 4), this),
            gs_make_card(tr("VEGA"), fmt_num_safe(g.value("vega"), 4), this),
            gs_make_card(tr("THETA"), fmt_num_safe(g.value("theta"), 4), this, ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(first, this));

        QList<QWidget*> second = {
            gs_make_card(tr("RHO"), fmt_num_safe(g.value("rho"), 4), this),
            gs_make_card(tr("VANNA"), fmt_num_safe(g.value("vanna"), 4), this),
            gs_make_card(tr("VOLGA"), fmt_num_safe(g.value("volga"), 4), this),
            gs_make_card(tr("CHARM"), fmt_num_safe(g.value("charm"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(second, this));

        QList<QWidget*> higher = {
            gs_make_card(tr("SPEED"), fmt_num_safe(g.value("speed"), 6), this),
            gs_make_card(tr("ZOMMA"), fmt_num_safe(g.value("zomma"), 6), this),
            gs_make_card(tr("COLOR"), fmt_num_safe(g.value("color"), 6), this),
            gs_make_card(tr("ULTIMA"), fmt_num_safe(g.value("ultima"), 6), this),
        };
        results_layout_->addWidget(gs_card_row(higher, this));

        status_label_->setText(tr("%1 %2 / %3  |  Δ=%4  Γ=%5  ν=%6")
                                   .arg(opt)
                                   .arg(spot, 0, 'f', 2)
                                   .arg(strike, 0, 'f', 2)
                                   .arg(fmt_num_safe(g.value("delta"), 3), fmt_num_safe(g.value("gamma"), 4),
                                        fmt_num_safe(g.value("vega"), 3)));
        return;
    }

    // ── 4. VaR ANALYSIS ──────────────────────────────────────────────────────
    if (command == "var_analysis") {
        const double pos = d.value("position_value").toDouble();
        const double conf = d.value("confidence").toDouble();
        const QJsonObject parm = d.value("parametric_var").toObject();
        const QJsonObject hist = d.value("historical_var").toObject();
        const QJsonObject mc = d.value("monte_carlo_var").toObject();
        const QJsonObject cv = d.value("cvar").toObject();

        // Header strip with position + confidence
        auto* hdr = new QLabel(
            tr("Position %1  |  Confidence %2%  |  1-day horizon").arg(gs_fmt_money(pos)).arg(conf * 100, 0, 'f', 1));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        // VaR amount cards (loss in $) — color escalates with size
        auto var_color = [pos](const QJsonValue& amt) {
            if (!amt.isDouble())
                return QString(ui::colors::TEXT_SECONDARY());
            const double pct = pos > 0 ? std::abs(amt.toDouble()) / pos : 0;
            if (pct > 0.05)
                return QString(ui::colors::NEGATIVE());
            if (pct > 0.02)
                return QString(ui::colors::WARNING());
            return QString(ui::colors::TEXT_PRIMARY());
        };
        // VaR as a positive loss percentage; null → "—".
        auto loss_pct = [](const QJsonValue& v) {
            return v.isDouble() ? gs_fmt_num(std::abs(v.toDouble()), 2) + "%" : QString::fromUtf8("—");
        };

        QList<QWidget*> amounts = {
            gs_make_card(tr("PARAMETRIC VaR"), gs_money_safe(parm.value("var_amount")), this,
                         var_color(parm.value("var_amount"))),
            gs_make_card(tr("HISTORICAL VaR"), gs_money_safe(hist.value("var_amount")), this,
                         var_color(hist.value("var_amount"))),
            gs_make_card(tr("MONTE CARLO VaR"), gs_money_safe(mc.value("var_amount")), this,
                         var_color(mc.value("var_amount"))),
            gs_make_card(tr("CVaR (EXP. SHORT.)"), gs_money_safe(cv.value("var_amount")), this,
                         ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(amounts, this));

        // VaR as % of position. The parametric figure comes back positive but the historical /
        // Monte Carlo ones negative (a loss), so the cards disagreed in sign for the same
        // quantity; show every VaR as a positive loss percentage, like CVaR already did.
        QList<QWidget*> pcts = {
            gs_make_card(tr("PARAMETRIC %"), loss_pct(parm.value("var_percentage")), this),
            gs_make_card(tr("HISTORICAL %"), loss_pct(hist.value("var_percentage")), this),
            gs_make_card(tr("MONTE CARLO %"), loss_pct(mc.value("var_percentage")), this),
            gs_make_card(tr("CVaR %"), loss_pct(cv.value("var_percentage")), this, ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(pcts, this));

        // Methodology table — each method as a row
        auto* table = new QTableWidget(4, 4, this);
        table->setHorizontalHeaderLabels({tr("Method"), tr("VaR Amount"), tr("VaR %"), tr("Notes")});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(150);

        const QList<QPair<QString, QJsonObject>> rows = {
            {tr("Parametric (Variance-Covariance)"), parm},
            {tr("Historical Simulation"), hist},
            {tr("Monte Carlo Simulation"), mc},
            {tr("CVaR / Expected Shortfall"), cv},
        };
        for (int r = 0; r < rows.size(); ++r) {
            const auto& obj = rows[r].second;
            table->setItem(r, 0, new QTableWidgetItem(rows[r].first));
            const QJsonValue amt = obj.value("var_amount");
            table->setItem(r, 1, gs_cell(amt, gs_money_safe(amt), ui::colors::NEGATIVE()));
            const QJsonValue vpct = obj.value("var_percentage");
            table->setItem(r, 2,
                           gs_cell(vpct.isDouble() ? QJsonValue(std::abs(vpct.toDouble())) : vpct,
                                   vpct.isDouble() ? QString::number(std::abs(vpct.toDouble()), 'f', 3) + "%"
                                                   : QString::fromUtf8("—")));
            table->setItem(r, 3, new QTableWidgetItem(obj.value("method").toString()));
            table->setRowHeight(r, row_px(24));
        }
        finish_table(table);
        results_layout_->addWidget(table);

        status_label_->setText(tr("VaR(%1%) param=%2  hist=%3  mc=%4  cvar=%5")
                                   .arg(conf * 100, 0, 'f', 0)
                                   .arg(gs_money_safe(parm.value("var_amount")), gs_money_safe(hist.value("var_amount")),
                                        gs_money_safe(mc.value("var_amount")), gs_money_safe(cv.value("var_amount"))));
        return;
    }

    // ── 5. STRESS TEST ───────────────────────────────────────────────────────
    if (command == "stress_test") {
        const double pos = d.value("position_value").toDouble();
        const QJsonArray scenarios = d.value("scenarios").toArray();
        const QJsonObject positions = d.value("positions").toObject();

        // Allocation summary strip
        QStringList parts;
        for (auto it = positions.begin(); it != positions.end(); ++it)
            parts << QString("%1: %2").arg(it.key(), gs_fmt_money(it.value().toDouble()));
        auto* alloc = new QLabel(tr("Portfolio %1  |  %2").arg(gs_fmt_money(pos), parts.join("  •  ")));
        alloc->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                     "padding:8px 10px; background:%2; border:1px solid %3;")
                                 .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(alloc);

        if (scenarios.isEmpty()) {
            display_error(tr("Stress test returned no scenarios."));
            return;
        }

        // Worst / best summary cards
        double worst_pnl = scenarios[0].toObject().value("pnl").toDouble();
        double best_pnl = worst_pnl;
        QString worst_name = scenarios[0].toObject().value("scenario_name").toString();
        QString best_name = worst_name;
        for (const auto& v : scenarios) {
            const auto o = v.toObject();
            const double p = o.value("pnl").toDouble();
            if (p < worst_pnl) {
                worst_pnl = p;
                worst_name = o.value("scenario_name").toString();
            }
            if (p > best_pnl) {
                best_pnl = p;
                best_name = o.value("scenario_name").toString();
            }
        }
        QList<QWidget*> summary_cards = {
            gs_make_card(tr("WORST CASE PnL"), gs_fmt_signed_money(worst_pnl), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("WORST CASE %"), QString("%1%").arg(worst_pnl / pos * 100, 0, 'f', 2), this,
                         ui::colors::NEGATIVE()),
            gs_make_card(tr("BEST CASE PnL"), gs_fmt_signed_money(best_pnl), this,
                         best_pnl > 0 ? ui::colors::POSITIVE() : ui::colors::TEXT_PRIMARY()),
            gs_make_card(tr("SCENARIOS RUN"), QString::number(scenarios.size()), this),
        };
        results_layout_->addWidget(gs_card_row(summary_cards, this));

        // Worst/best name strip
        auto* wb = new QLabel(tr("Worst:  %1   |   Best:  %2").arg(worst_name, best_name));
        wb->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                  "padding:6px 10px; background:%2; border-left:3px solid %3;")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(), ui::colors::WARNING()));
        results_layout_->addWidget(wb);

        // Which scenario hurts most, at a glance: P&L % per scenario as signed horizontal bars.
        {
            QStringList names;
            QVector<double> pcts;
            for (const auto& v : scenarios) {
                const auto o = v.toObject();
                names << o.value("scenario_name").toString();
                pcts << o.value("pnl_percentage").toDouble(kQcNaN); // null → no bar
            }
            results_layout_->addWidget(qc_caption(tr("P&L BY SCENARIO (% OF PORTFOLIO)")));
            auto* bars = new ui::BarChart(this);
            bars->set_data(names, pcts, tr("P&L %"));
            bars->set_orientation(Qt::Horizontal);
            bars->set_signed_colors(true);
            bars->set_show_values(true);
            bars->set_value_formatter(qc_fmt_pct(2));
            bars->setFixedHeight(row_px(std::clamp(40 + 26 * static_cast<int>(scenarios.size()), 140, 360)));
            results_layout_->addWidget(bars);
        }

        // Full scenario table
        auto* table = new QTableWidget(scenarios.size(), 4, this);
        table->setHorizontalHeaderLabels({tr("Scenario"), tr("PnL"), tr("PnL %"), tr("Shocked Value")});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(qMin(scenarios.size() * row_px(26) + row_px(32), 320));

        for (int i = 0; i < scenarios.size(); ++i) {
            const auto o = scenarios[i].toObject();
            const QJsonValue pnl = o.value("pnl");
            const QJsonValue pnl_pct = o.value("pnl_percentage");
            const QJsonValue shocked = o.value("shocked_value");
            table->setItem(i, 0, new QTableWidgetItem(o.value("scenario_name").toString()));
            table->setItem(i, 1,
                           gs_cell(pnl, pnl.isDouble() ? gs_fmt_signed_money(pnl.toDouble()) : QString::fromUtf8("—"),
                                   gs_pos_neg_color(pnl.toDouble())));
            table->setItem(i, 2, gs_cell(pnl_pct, fmt_pts_safe(pnl_pct, 2), gs_pos_neg_color(pnl_pct.toDouble())));
            table->setItem(i, 3, gs_cell(shocked, gs_money_safe(shocked)));
            table->setRowHeight(i, row_px(24));
        }
        finish_table(table);
        results_layout_->addWidget(table);

        status_label_->setText(tr("Worst: %1  (%2)  |  %3 scenarios")
                                   .arg(gs_fmt_signed_money(worst_pnl), worst_name)
                                   .arg(scenarios.size()));
        return;
    }

    // ── 6. BACKTEST ──────────────────────────────────────────────────────────
    if (command == "backtest") {
        const QString strategy = d.value("strategy").toString();
        const QString ticker = d.value("ticker").toString();
        const QString start = d.value("start_date").toString();
        const QString end = d.value("end_date").toString();
        const double init_cap = d.value("initial_capital").toDouble();
        const QJsonObject m = d.value("metrics").toObject();
        const QJsonArray curve = d.value("equity_curve").toArray();

        const QJsonValue total_ret = m.value("total_return");
        const QJsonValue ann_ret = m.value("annualized_return");
        const QJsonValue sharpe = m.value("sharpe_ratio"); // null when the equity never moved
        const double mdd = m.value("max_drawdown").toDouble();
        const QJsonValue final_v = m.value("final_value");
        const double final_val = final_v.toDouble();
        const int trades = m.value("num_trades").toInt();
        const QJsonValue win = m.value("win_rate_pct");
        const QJsonValue profit = final_v.isDouble() ? QJsonValue(final_val - init_cap) : QJsonValue();

        // Header strip
        auto* hdr =
            new QLabel(tr("%1  |  %2  |  %3  →  %4  |  Initial %5")
                           .arg(strategy.toUpper().replace('_', ' '), ticker, start, end, gs_fmt_money(init_cap)));
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card(tr("FINAL VALUE"), gs_money_safe(final_v), this, gs_pos_neg_color(profit.toDouble())),
            gs_make_card(tr("TOTAL RETURN"), fmt_pct_safe(total_ret), this, gs_pos_neg_color(total_ret.toDouble())),
            gs_make_card(tr("ANN. RETURN"), fmt_pct_safe(ann_ret), this, gs_pos_neg_color(ann_ret.toDouble())),
            gs_make_card(tr("SHARPE"), fmt_num_safe(sharpe, 3), this, gs_ratio_color(sharpe)),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> risk_row = {
            gs_make_card(tr("VOLATILITY"), fmt_pct_safe(m.value("volatility")), this),
            gs_make_card(tr("MAX DRAWDOWN"), fmt_pct_safe(m.value("max_drawdown")), this,
                         mdd <= -0.20 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card(tr("BEST DAY"), fmt_pct_safe(m.value("best_day")), this, ui::colors::POSITIVE()),
            gs_make_card(tr("WORST DAY"), fmt_pct_safe(m.value("worst_day")), this, ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(risk_row, this));

        QList<QWidget*> activity = {
            gs_make_card(tr("TRADES"), QString::number(trades), this),
            gs_make_card(tr("WIN RATE"), fmt_pts_safe(win, 1), this,
                         !win.isDouble()          ? ui::colors::TEXT_SECONDARY()
                         : win.toDouble() > 50.0 ? ui::colors::POSITIVE()
                                                 : ui::colors::WARNING()),
            gs_make_card(tr("DAYS"), QString::number(m.value("n_days").toInt()), this),
            gs_make_card(tr("PROFIT"),
                         profit.isDouble() ? gs_fmt_signed_money(profit.toDouble()) : QString::fromUtf8("—"), this,
                         gs_pos_neg_color(profit.toDouble())),
        };
        results_layout_->addWidget(gs_card_row(activity, this));

        // Equity curve (the script sends ~250 points that always end on the last day) with
        // the starting capital as a reference and its drawdown below; then the latest points.
        if (curve.size() >= 2) {
            const XAxis x = qc_x_axis(curve, {QStringLiteral("date")});
            auto* chart = qc_equity_chart(tr("EQUITY CURVE  ·  %1").arg(ticker), x,
                                          qc_column(curve, QStringLiteral("value")), {},
                                          [](double v) { return gs_fmt_money(v); }, QString(), this);
            if (init_cap > 0)
                chart->add_hline(init_cap, tr("Initial capital"), ui::ChartColorRole::Muted);
            results_layout_->addWidget(chart);

            const QVector<int> latest = qc_latest_rows(curve.size());
            results_layout_->addWidget(qc_caption(qc_latest_caption(latest.size(), curve.size())));
            auto* eq_table = new QTableWidget(0, 3, this);
            eq_table->setHorizontalHeaderLabels({tr("Date"), tr("Portfolio Value"), tr("Return")});
            eq_table->verticalHeader()->setVisible(false);
            eq_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            eq_table->setSelectionMode(QAbstractItemView::NoSelection);
            eq_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            eq_table->setStyleSheet(table_ss());
            eq_table->setMaximumHeight(280);
            int row = 0;
            for (const int i : latest) {
                const auto o = curve[i].toObject();
                const QJsonValue val = o.value("value");
                const QJsonValue ret = val.isDouble() && init_cap > 0 ? QJsonValue(val.toDouble() / init_cap - 1.0)
                                                                      : QJsonValue();
                eq_table->insertRow(row);
                eq_table->setItem(row, 0, new QTableWidgetItem(o.value("date").toString()));
                auto* val_item = new QTableWidgetItem(gs_money_safe(val));
                val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                eq_table->setItem(row, 1, val_item);
                auto* ret_item = new QTableWidgetItem(fmt_pct_safe(ret));
                ret_item->setForeground(QColor(gs_pos_neg_color(ret.toDouble())));
                ret_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                eq_table->setItem(row, 2, ret_item);
                eq_table->setRowHeight(row, row_px(22));
                ++row;
            }
            results_layout_->addWidget(eq_table);
        }

        status_label_->setText(tr("%1 on %2: %3 over %4 trades  (Sharpe %5)")
                                   .arg(strategy, ticker)
                                   .arg(fmt_pct_safe(total_ret))
                                   .arg(trades)
                                   .arg(fmt_num_safe(sharpe, 2)));
        return;
    }

    // ── 7. STATISTICS ────────────────────────────────────────────────────────
    if (command == "statistics") {
        QList<QWidget*> central = {
            gs_make_card(tr("MEAN"), fmt_num_safe(d.value("mean"), 4), this),
            gs_make_card(tr("MEDIAN"), fmt_num_safe(d.value("median"), 4), this),
            gs_make_card(tr("STD DEV"), fmt_num_safe(d.value("std"), 4), this),
            gs_make_card(tr("VARIANCE"), fmt_num_safe(d.value("variance"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(central, this));

        QList<QWidget*> spread = {
            gs_make_card(tr("MIN"), fmt_num_safe(d.value("min"), 4), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("MAX"), fmt_num_safe(d.value("max"), 4), this, ui::colors::POSITIVE()),
            gs_make_card(tr("RANGE"), fmt_num_safe(d.value("range"), 4), this),
            gs_make_card(tr("SUM"), fmt_num_safe(d.value("sum"), 2), this),
        };
        results_layout_->addWidget(gs_card_row(spread, this));

        QList<QWidget*> shape = {
            gs_make_card(tr("SKEWNESS"), fmt_num_safe(d.value("skewness"), 3), this,
                         gs_pos_neg_color(d.value("skewness").toDouble())),
            gs_make_card(tr("KURTOSIS"), fmt_num_safe(d.value("kurtosis"), 3), this),
            gs_make_card(tr("SEMI-VAR"), fmt_num_safe(d.value("semi_variance"), 4), this),
            gs_make_card(tr("REALIZED VAR"), fmt_num_safe(d.value("realized_variance"), 4), this),
        };
        results_layout_->addWidget(gs_card_row(shape, this));

        // Percentile table
        const QJsonObject pcts = d.value("percentiles").toObject();
        if (!pcts.isEmpty()) {
            // Order keys numerically
            QList<int> ordered_keys;
            for (auto it = pcts.begin(); it != pcts.end(); ++it)
                ordered_keys.append(it.key().toInt());
            std::sort(ordered_keys.begin(), ordered_keys.end());

            auto* table = new QTableWidget(ordered_keys.size(), 2, this);
            table->setHorizontalHeaderLabels({tr("Percentile"), tr("Value")});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(ordered_keys.size() * row_px(24) + row_px(32), 280));
            for (int i = 0; i < ordered_keys.size(); ++i) {
                const int k = ordered_keys[i];
                table->setItem(i, 0, new QTableWidgetItem(tr("p%1").arg(k)));
                auto* v_item = new QTableWidgetItem(fmt_num_safe(pcts.value(QString::number(k)), 4));
                v_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 1, v_item);
                table->setRowHeight(i, row_px(22));
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("μ=%1  σ=%2  n=%3")
                                   .arg(fmt_num_safe(d.value("mean"), 3), fmt_num_safe(d.value("std"), 3))
                                   .arg(d.value("count").toInt()));
        return;
    }

    // ── 8. COMPONENT VaR ─────────────────────────────────────────────────────
    if (command == "component_var") {
        const double pos = d.value("position_value").toDouble();
        const double conf = d.value("confidence").toDouble();
        const int horizon = d.value("horizon_days").toInt();
        const QJsonValue port_var = d.value("portfolio_var");
        const QJsonValue benefit = d.value("diversification_benefit");
        const QJsonValue standalone = d.value("sum_standalone_var");
        const QJsonArray assets = d.value("assets").toArray();

        auto* hdr = new QLabel(tr("Portfolio %1  |  Confidence %2%  |  %3-day horizon  |  %4 assets  |  %5")
                                   .arg(gs_fmt_money(pos))
                                   .arg(conf * 100, 0, 'f', 1)
                                   .arg(horizon)
                                   .arg(assets.size())
                                   .arg(d.value("method").toString()));
        hdr->setWordWrap(true);
        hdr->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        // Largest share of the VaR.
        QString top_asset;
        double top_pct = -std::numeric_limits<double>::infinity();
        for (const auto& v : assets) {
            const auto o = v.toObject();
            const QJsonValue p = o.value("pct_contribution");
            if (p.isDouble() && p.toDouble() > top_pct) {
                top_pct = p.toDouble();
                top_asset = o.value("asset").toString();
            }
        }
        const bool has_benefit_pct = benefit.isDouble() && standalone.isDouble() && standalone.toDouble() > 0.0;
        const double benefit_pct = has_benefit_pct ? benefit.toDouble() / standalone.toDouble() * 100.0 : 0.0;

        QList<QWidget*> top = {
            gs_make_card(tr("PORTFOLIO VaR"), gs_money_safe(port_var), this, ui::colors::NEGATIVE()),
            gs_make_card(tr("VaR % OF PORTFOLIO"), fmt_pts_safe(d.value("portfolio_var_pct"), 2), this,
                         ui::colors::NEGATIVE()),
            gs_make_card(tr("DIVERSIFICATION BENEFIT"), gs_money_safe(benefit), this, ui::colors::INFO()),
            gs_make_card(tr("PORTFOLIO σ (PER DAY)"), fmt_pct_safe(d.value("portfolio_volatility"), 3), this),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> second = {
            gs_make_card(tr("SUM OF STANDALONE VaR"), gs_money_safe(standalone), this),
            gs_make_card(tr("BENEFIT % OF STANDALONE"),
                         has_benefit_pct ? QString::number(benefit_pct, 'f', 1) + "%" : QString::fromUtf8("—"), this,
                         ui::colors::INFO()),
            gs_make_card(tr("LARGEST CONTRIBUTOR"),
                         top_asset.isEmpty() ? QString::fromUtf8("—")
                                             : tr("%1  (%2%)").arg(top_asset).arg(top_pct, 0, 'f', 1),
                         this, ui::colors::WARNING()),
            gs_make_card(tr("OBSERVATIONS"), QString::number(d.value("n_observations").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(second, this));

        if (!assets.isEmpty()) {
            // Share of the portfolio VaR per asset (sums to 100%; a hedge is negative). One
            // series colour: a large share is not "good", so no up / down colouring.
            QStringList names;
            QVector<double> shares;
            bool any_negative = false;
            for (const auto& v : assets) {
                const auto o = v.toObject();
                names << o.value("asset").toString();
                shares << o.value("pct_contribution").toDouble(kQcNaN);
                any_negative = any_negative || o.value("pct_contribution").toDouble() < 0.0;
            }
            results_layout_->addWidget(qc_caption(tr("SHARE OF PORTFOLIO VaR BY ASSET (%)  ·  COMPONENTS SUM TO 100%")));
            auto* bars = new ui::BarChart(this);
            bars->set_data(names, shares, tr("% of VaR"));
            bars->set_orientation(Qt::Horizontal);
            bars->set_show_values(true);
            bars->set_value_formatter(qc_fmt_pct(1));
            bars->setFixedHeight(row_px(std::clamp(40 + 26 * static_cast<int>(assets.size()), 140, 360)));
            results_layout_->addWidget(bars);

            auto* table = new QTableWidget(assets.size(), 7, this);
            table->setHorizontalHeaderLabels({tr("Asset"), tr("Weight"), tr("Position"), tr("Standalone VaR"),
                                              tr("Marginal VaR"), tr("Component VaR"), tr("% of VaR")});
            if (auto* h = table->horizontalHeaderItem(4))
                h->setToolTip(tr("Change in portfolio VaR per unit of weight (∂VaR/∂w), as a share of the "
                                 "portfolio value."));
            if (auto* h = table->horizontalHeaderItem(3))
                h->setToolTip(tr("The asset's own VaR at its weight, ignoring diversification."));
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(assets.size() * row_px(24) + row_px(32), 320));
            for (int i = 0; i < assets.size(); ++i) {
                const auto o = assets[i].toObject();
                table->setItem(i, 0, new QTableWidgetItem(o.value("asset").toString()));
                table->setItem(i, 1, gs_cell(o.value("weight"), fmt_pct_safe(o.value("weight"), 2)));
                table->setItem(i, 2, gs_cell(o.value("position_value"), gs_money_safe(o.value("position_value"))));
                table->setItem(i, 3, gs_cell(o.value("standalone_var"), gs_money_safe(o.value("standalone_var"))));
                table->setItem(i, 4, gs_cell(o.value("marginal_var"), fmt_pct_safe(o.value("marginal_var"), 3)));
                table->setItem(i, 5, gs_cell(o.value("component_var"), gs_money_safe(o.value("component_var"))));
                table->setItem(i, 6, gs_cell(o.value("pct_contribution"), fmt_pts_safe(o.value("pct_contribution"), 1)));
                table->setRowHeight(i, row_px(24));
            }
            if (!any_negative)
                table->setItemDelegateForColumn(6, new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table));
            finish_table(table);
            results_layout_->addWidget(table);
        }

        status_label_->setText(tr("VaR %1% %2d: %3 (%4)  ·  diversification benefit %5  ·  largest %6")
                                   .arg(conf * 100, 0, 'f', 1)
                                   .arg(horizon)
                                   .arg(gs_money_safe(port_var), fmt_pts_safe(d.value("portfolio_var_pct"), 2),
                                        gs_money_safe(benefit), top_asset.isEmpty() ? QString::fromUtf8("—") : top_asset));
        return;
    }

    // ── Fallback for any unknown command — generic table ─────────────────────
    display_result(d);
}

} // namespace fincept::screens
