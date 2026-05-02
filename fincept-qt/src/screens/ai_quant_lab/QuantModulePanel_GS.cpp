// src/screens/ai_quant_lab/QuantModulePanel_GS.cpp
//
// GS Quant panel — 7 analysis modes (risk metrics, portfolio analytics, greeks,
// VaR, stress test, backtest, statistics). Extracted from QuantModulePanel.cpp
// to keep that file maintainable.
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

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

// ═══════════════════════════════════════════════════════════════════════════════
// GS QUANT PANEL — 7 analysis modes
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_gs_quant_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));

    // Helper: small "load sample" link button next to a CSV input
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
            if (guard) guard->setText(sample_returns(seed));
        });
        return btn;
    };

    // ── Risk Metrics ──
    auto* risk = new QWidget(this);
    auto* rvl = new QVBoxLayout(risk);
    rvl->setContentsMargins(12, 12, 12, 12);
    rvl->setSpacing(8);
    auto* risk_returns = new QLineEdit(risk);
    risk_returns->setPlaceholderText("Daily returns: comma-, space-, or newline-separated. Need at least 5 values.");
    risk_returns->setStyleSheet(input_ss());
    text_inputs_["gs_risk_returns"] = risk_returns;
    rvl->addWidget(build_input_row("Daily Returns", risk_returns, risk));
    rvl->addWidget(add_sample_btn(risk_returns, risk, 11,
                                   "Inserts 252 synthetic daily returns (mu=0.05%, sigma=1.2%)"));
    auto* risk_rf = make_double_spin(0, 20, 4.5, 2, "%", risk);
    double_inputs_["gs_risk_rf"] = risk_rf;
    rvl->addWidget(build_input_row("Risk-Free Rate", risk_rf, risk));
    auto* risk_run = make_run_button("CALCULATE RISK METRICS", risk);
    connect(risk_run, &QPushButton::clicked, this, [this]() {
        QJsonArray rets;
        QString bad;
        if (!parse_doubles(text_inputs_["gs_risk_returns"]->text(), rets, &bad)) {
            display_error(QString("Daily Returns: '%1' is not a number. "
                                  "Use comma-, space-, or newline-separated decimals (e.g. 0.01, -0.02, 0.005).")
                              .arg(bad));
            return;
        }
        if (rets.size() < 5) {
            display_error(QString("Need at least 5 daily returns; you provided %1. "
                                  "Click LOAD SAMPLE to insert 252 synthetic values.")
                              .arg(rets.size()));
            return;
        }
        show_loading(QString("Calculating risk metrics on %1 observations...").arg(rets.size()));
        QJsonObject params;
        params["returns"] = rets;
        params["risk_free_rate"] = double_inputs_["gs_risk_rf"]->value() / 100.0;
        AIQuantLabService::instance().gs_risk_metrics(params);
    });
    rvl->addWidget(risk_run);
    rvl->addStretch();
    tabs->addTab(risk, "Risk Metrics");

    // ── Portfolio Analytics ──
    auto* port = new QWidget(this);
    auto* pvl = new QVBoxLayout(port);
    pvl->setContentsMargins(12, 12, 12, 12);
    pvl->setSpacing(8);
    auto* port_ret = new QLineEdit(port);
    port_ret->setPlaceholderText("Portfolio daily returns (decimals, same length as benchmark)");
    port_ret->setStyleSheet(input_ss());
    text_inputs_["gs_port_returns"] = port_ret;
    pvl->addWidget(build_input_row("Portfolio Returns", port_ret, port));
    pvl->addWidget(add_sample_btn(port_ret, port, 21,
                                   "Inserts 252 synthetic portfolio returns (slight outperformance)"));
    auto* bench_ret = new QLineEdit(port);
    bench_ret->setPlaceholderText("Benchmark daily returns (decimals)");
    bench_ret->setStyleSheet(input_ss());
    text_inputs_["gs_bench_returns"] = bench_ret;
    pvl->addWidget(build_input_row("Benchmark Returns", bench_ret, port));
    pvl->addWidget(add_sample_btn(bench_ret, port, 22,
                                   "Inserts 252 synthetic benchmark returns"));
    auto* port_rf = make_double_spin(0, 20, 4.5, 2, "%", port);
    double_inputs_["gs_port_rf"] = port_rf;
    pvl->addWidget(build_input_row("Risk-Free Rate", port_rf, port));
    auto* port_run = make_run_button("ANALYZE PORTFOLIO", port);
    connect(port_run, &QPushButton::clicked, this, [this]() {
        QJsonArray prets, brets;
        QString bad;
        if (!parse_doubles(text_inputs_["gs_port_returns"]->text(), prets, &bad)) {
            display_error(QString("Portfolio Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (!parse_doubles(text_inputs_["gs_bench_returns"]->text(), brets, &bad)) {
            display_error(QString("Benchmark Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (prets.size() < 5 || brets.size() < 5) {
            display_error("Need at least 5 observations for both portfolio and benchmark returns.");
            return;
        }
        if (prets.size() != brets.size()) {
            display_error(QString("Portfolio (%1) and benchmark (%2) must have the same length.")
                              .arg(prets.size()).arg(brets.size()));
            return;
        }
        show_loading(QString("Analyzing portfolio vs benchmark on %1 observations...").arg(prets.size()));
        QJsonObject params;
        params["returns"] = prets;
        params["benchmark_returns"] = brets;
        params["risk_free_rate"] = double_inputs_["gs_port_rf"]->value() / 100.0;
        AIQuantLabService::instance().gs_portfolio_analytics(params);
    });
    pvl->addWidget(port_run);
    pvl->addStretch();
    tabs->addTab(port, "Portfolio");

    // ── Options Greeks ──
    auto* greeks = new QWidget(this);
    auto* gvl = new QVBoxLayout(greeks);
    gvl->setContentsMargins(12, 12, 12, 12);
    gvl->setSpacing(8);
    auto* g_spot = make_double_spin(0.01, 1e6, 100, 2, "", greeks);
    double_inputs_["gs_spot"] = g_spot;
    gvl->addWidget(build_input_row("Spot Price ($)", g_spot, greeks));
    auto* g_strike = make_double_spin(0.01, 1e6, 100, 2, "", greeks);
    double_inputs_["gs_strike"] = g_strike;
    gvl->addWidget(build_input_row("Strike Price ($)", g_strike, greeks));
    auto* g_expiry = make_double_spin(0.1, 120, 6, 1, " mo", greeks);
    double_inputs_["gs_expiry"] = g_expiry;
    gvl->addWidget(build_input_row("Expiry (months)", g_expiry, greeks));
    auto* g_rate = make_double_spin(0, 30, 5, 2, "%", greeks);
    double_inputs_["gs_rate"] = g_rate;
    gvl->addWidget(build_input_row("Risk-Free Rate", g_rate, greeks));
    auto* g_vol = make_double_spin(0.1, 200, 25, 1, "%", greeks);
    double_inputs_["gs_vol"] = g_vol;
    gvl->addWidget(build_input_row("Volatility", g_vol, greeks));
    auto* g_type = new QComboBox(greeks);
    g_type->addItems({"call", "put"});
    g_type->setStyleSheet(combo_ss());
    combo_inputs_["gs_option_type"] = g_type;
    gvl->addWidget(build_input_row("Option Type", g_type, greeks));
    auto* greeks_run = make_run_button("CALCULATE GREEKS", greeks);
    connect(greeks_run, &QPushButton::clicked, this, [this]() {
        show_loading("Calculating Black-Scholes Greeks...");
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
    tabs->addTab(greeks, "Greeks");

    // ── VaR Analysis ──
    auto* var = new QWidget(this);
    auto* vvl = new QVBoxLayout(var);
    vvl->setContentsMargins(12, 12, 12, 12);
    vvl->setSpacing(8);
    auto* var_ret = new QLineEdit(var);
    var_ret->setPlaceholderText("Daily returns (decimals). Need at least 30 values for stable VaR.");
    var_ret->setStyleSheet(input_ss());
    text_inputs_["gs_var_returns"] = var_ret;
    vvl->addWidget(build_input_row("Daily Returns", var_ret, var));
    vvl->addWidget(add_sample_btn(var_ret, var, 31, "Inserts 252 synthetic daily returns"));
    auto* var_pos = make_double_spin(1, 1e12, 1e6, 0, "", var);
    double_inputs_["gs_var_position"] = var_pos;
    vvl->addWidget(build_input_row("Position Value ($)", var_pos, var));
    auto* var_conf = make_double_spin(0.80, 0.999, 0.95, 3, "", var);
    double_inputs_["gs_var_confidence"] = var_conf;
    vvl->addWidget(build_input_row("Confidence Level", var_conf, var));
    auto* var_run = make_run_button("CALCULATE VaR", var);
    connect(var_run, &QPushButton::clicked, this, [this]() {
        QJsonArray rets;
        QString bad;
        if (!parse_doubles(text_inputs_["gs_var_returns"]->text(), rets, &bad)) {
            display_error(QString("Daily Returns: '%1' is not numeric.").arg(bad));
            return;
        }
        if (rets.size() < 30) {
            display_error(QString("VaR needs at least 30 observations for a stable estimate; you provided %1.")
                              .arg(rets.size()));
            return;
        }
        show_loading(QString("Computing parametric, historical, MC and CVaR on %1 observations...").arg(rets.size()));
        QJsonObject params;
        params["returns"] = rets;
        params["position_value"] = double_inputs_["gs_var_position"]->value();
        params["confidence"] = double_inputs_["gs_var_confidence"]->value();
        AIQuantLabService::instance().gs_var_analysis(params);
    });
    vvl->addWidget(var_run);
    vvl->addStretch();
    tabs->addTab(var, "VaR");

    // ── Stress Test ──
    auto* stress = new QWidget(this);
    auto* svl = new QVBoxLayout(stress);
    svl->setContentsMargins(12, 12, 12, 12);
    svl->setSpacing(8);
    auto* st_pos = make_double_spin(1, 1e12, 1e6, 0, "", stress);
    double_inputs_["gs_stress_position"] = st_pos;
    svl->addWidget(build_input_row("Portfolio Value ($)", st_pos, stress));
    auto* st_eq = make_double_spin(0, 100, 60, 1, "%", stress);
    double_inputs_["gs_stress_eq"] = st_eq;
    svl->addWidget(build_input_row("Equity Allocation", st_eq, stress));
    auto* st_bd = make_double_spin(0, 100, 30, 1, "%", stress);
    double_inputs_["gs_stress_bd"] = st_bd;
    svl->addWidget(build_input_row("Bond Allocation", st_bd, stress));
    auto* st_cm = make_double_spin(0, 100, 10, 1, "%", stress);
    double_inputs_["gs_stress_cm"] = st_cm;
    svl->addWidget(build_input_row("Commodity Allocation", st_cm, stress));
    auto* st_hint = new QLabel(
        "Applies 9 historical crisis scenarios (2008, COVID-19, Dot-Com, etc.) to the "
        "weighted portfolio. Allocations need not sum to 100% — they will be normalized.", stress);
    st_hint->setWordWrap(true);
    st_hint->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    svl->addWidget(st_hint);
    auto* stress_run = make_run_button("RUN STRESS TEST", stress);
    connect(stress_run, &QPushButton::clicked, this, [this]() {
        const double eq = double_inputs_["gs_stress_eq"]->value();
        const double bd = double_inputs_["gs_stress_bd"]->value();
        const double cm = double_inputs_["gs_stress_cm"]->value();
        if (eq + bd + cm <= 0) {
            display_error("At least one allocation must be greater than zero.");
            return;
        }
        show_loading("Running 9 historical crisis scenarios...");
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
    tabs->addTab(stress, "Stress Test");

    // ── Backtest ──
    auto* bt = new QWidget(this);
    auto* bvl = new QVBoxLayout(bt);
    bvl->setContentsMargins(12, 12, 12, 12);
    bvl->setSpacing(8);
    auto* bt_strat = new QComboBox(bt);
    bt_strat->addItems({"buy_and_hold", "momentum", "mean_reversion", "rebalancing"});
    bt_strat->setStyleSheet(combo_ss());
    combo_inputs_["gs_bt_strategy"] = bt_strat;
    bvl->addWidget(build_input_row("Strategy", bt_strat, bt));
    auto* bt_ticker = new QLineEdit(bt);
    bt_ticker->setPlaceholderText("e.g. AAPL — fetched from Yahoo Finance");
    bt_ticker->setStyleSheet(input_ss());
    text_inputs_["gs_bt_ticker"] = bt_ticker;
    bvl->addWidget(build_input_row("Ticker", bt_ticker, bt));
    auto* bt_period = new QComboBox(bt);
    bt_period->addItems({"1y", "2y", "3y", "5y", "10y", "max"});
    bt_period->setCurrentText("2y");
    bt_period->setStyleSheet(combo_ss());
    combo_inputs_["gs_bt_period"] = bt_period;
    bvl->addWidget(build_input_row("History Period", bt_period, bt));
    auto* bt_capital = make_double_spin(1000, 1e12, 100000, 0, "", bt);
    double_inputs_["gs_bt_capital"] = bt_capital;
    bvl->addWidget(build_input_row("Initial Capital ($)", bt_capital, bt));
    auto* bt_lookback = new QSpinBox(bt);
    bt_lookback->setRange(5, 252);
    bt_lookback->setValue(20);
    bt_lookback->setStyleSheet(spinbox_ss());
    int_inputs_["gs_bt_lookback"] = bt_lookback;
    bvl->addWidget(build_input_row("Lookback Period (days)", bt_lookback, bt));
    auto* bt_comm = make_double_spin(0, 5, 0.10, 3, "%", bt);
    double_inputs_["gs_bt_comm"] = bt_comm;
    bvl->addWidget(build_input_row("Commission per Trade", bt_comm, bt));
    auto* bt_run = make_run_button("RUN BACKTEST", bt);
    connect(bt_run, &QPushButton::clicked, this, [this]() {
        const QString tk = text_inputs_["gs_bt_ticker"]->text().trimmed();
        if (tk.isEmpty()) {
            display_error("Enter a ticker (e.g. AAPL, SPY, MSFT) to fetch price history.");
            return;
        }
        show_loading(QString("Fetching %1 history from yfinance and running %2 backtest...")
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
    tabs->addTab(bt, "Backtest");

    // ── Statistics ──
    auto* stats = new QWidget(this);
    auto* stvl = new QVBoxLayout(stats);
    stvl->setContentsMargins(12, 12, 12, 12);
    stvl->setSpacing(8);
    auto* st_vals = new QLineEdit(stats);
    st_vals->setPlaceholderText("Numeric values (e.g. 10.5, 11.2, 9.8, 12.1, ...). Need at least 2.");
    st_vals->setStyleSheet(input_ss());
    text_inputs_["gs_stats_values"] = st_vals;
    stvl->addWidget(build_input_row("Values", st_vals, stats));
    stvl->addWidget(add_sample_btn(st_vals, stats, 41,
                                    "Inserts 100 synthetic observations (mean ~50, sd ~5)"));
    auto* stats_run = make_run_button("CALCULATE STATISTICS", stats);
    connect(stats_run, &QPushButton::clicked, this, [this]() {
        QJsonArray vals;
        QString bad;
        if (!parse_doubles(text_inputs_["gs_stats_values"]->text(), vals, &bad)) {
            display_error(QString("Values: '%1' is not numeric.").arg(bad));
            return;
        }
        if (vals.size() < 2) {
            display_error(QString("Need at least 2 values; you provided %1.").arg(vals.size()));
            return;
        }
        show_loading(QString("Computing descriptive statistics on %1 values...").arg(vals.size()));
        QJsonObject params;
        params["values"] = vals;
        AIQuantLabService::instance().gs_statistics(params);
    });
    stvl->addWidget(stats_run);
    stvl->addStretch();
    tabs->addTab(stats, "Statistics");

    vl->addWidget(tabs);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GS QUANT — display_gs_result: rich card-based display per command
// ═══════════════════════════════════════════════════════════════════════════════

void QuantModulePanel::display_gs_result(const QString& command, const QJsonObject& payload) {
    clear_results();

    // Unified error path
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

    // Header strip — operation name + observation count when available
    QString header_text = command.toUpper();
    header_text.replace('_', ' ');
    if (d.contains("n_observations"))
        header_text += QString("  |  %1 OBS").arg(d.value("n_observations").toInt());
    else if (d.contains("n_scenarios"))
        header_text += QString("  |  %1 SCENARIOS").arg(d.value("n_scenarios").toInt());
    results_layout_->addWidget(gs_section_header(header_text, accent));

    // ── 1. RISK METRICS ──────────────────────────────────────────────────────
    if (command == "risk_metrics") {
        const double sharpe = d.value("sharpe_ratio").toDouble();
        const double sortino = d.value("sortino_ratio").toDouble();
        const double calmar = d.value("calmar_ratio").toDouble();
        const double mdd = d.value("max_drawdown").toDouble();

        QList<QWidget*> ratios = {
            gs_make_card("SHARPE", gs_fmt_num(sharpe, 3), this,
                         sharpe >= 1.0 ? ui::colors::POSITIVE()
                                      : sharpe >= 0  ? ui::colors::TEXT_PRIMARY()
                                                     : ui::colors::NEGATIVE()),
            gs_make_card("SORTINO", gs_fmt_num(sortino, 3), this, gs_pos_neg_color(sortino)),
            gs_make_card("CALMAR", gs_fmt_num(calmar, 3), this, gs_pos_neg_color(calmar)),
            gs_make_card("OMEGA", gs_fmt_num(d.value("omega_ratio").toDouble(), 3), this),
        };
        results_layout_->addWidget(gs_card_row(ratios, this));

        QList<QWidget*> vol = {
            gs_make_card("VOLATILITY (ANN)", gs_fmt_pct(d.value("volatility_annualized").toDouble()), this),
            gs_make_card("DAILY RISK", gs_fmt_pct(d.value("daily_risk").toDouble(), 3), this),
            gs_make_card("DOWNSIDE RISK", gs_fmt_pct(d.value("downside_risk").toDouble()), this,
                         ui::colors::WARNING()),
            gs_make_card("MAX DRAWDOWN", gs_fmt_pct(mdd), this,
                         mdd <= -0.20 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(vol, this));

        QList<QWidget*> var_cards = {
            gs_make_card("VaR 95%", gs_fmt_pct(d.value("var_95").toDouble(), 3), this, ui::colors::WARNING()),
            gs_make_card("VaR 99%", gs_fmt_pct(d.value("var_99").toDouble(), 3), this, ui::colors::NEGATIVE()),
            gs_make_card("DD LENGTH", QString("%1 days").arg(d.value("max_drawdown_length").toInt()), this),
            gs_make_card("RECOVERY", QString("%1 days").arg(d.value("max_recovery_period").toInt()), this),
        };
        results_layout_->addWidget(gs_card_row(var_cards, this));

        // Drawdown timeline strip
        const QString peak = d.value("peak_date").toString().left(10);
        const QString trough = d.value("trough_date").toString().left(10);
        const QString recov = d.value("recovery_date").toString().left(10);
        if (!peak.isEmpty() && peak != "None") {
            auto* dd_lbl = new QLabel(QString("Drawdown: peak %1  →  trough %2  →  recovery %3")
                                          .arg(peak, trough, recov.isEmpty() || recov == "None" ? "—" : recov));
            dd_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                          "padding:6px 10px; background:%2; border:1px solid %3;")
                                      .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                           ui::colors::BORDER_DIM()));
            results_layout_->addWidget(dd_lbl);
        }
        status_label_->setText(QString("Sharpe %1  |  MaxDD %2  |  Vol %3")
                                   .arg(sharpe, 0, 'f', 2).arg(mdd * 100, 0, 'f', 1)
                                   .arg(d.value("volatility_annualized").toDouble() * 100, 0, 'f', 1));
        return;
    }

    // ── 2. PORTFOLIO ANALYTICS ───────────────────────────────────────────────
    if (command == "portfolio_analytics") {
        const double alpha = d.value("alpha").toDouble();
        const double sharpe = d.value("sharpe_ratio").toDouble();
        const double info = d.value("information_ratio").toDouble();
        const double te = d.value("tracking_error").toDouble();
        const double r2 = d.value("r_squared").toDouble();
        const double hit = d.value("hit_rate").toDouble();

        QList<QWidget*> top = {
            gs_make_card("ALPHA", gs_fmt_pct(alpha, 2), this, gs_pos_neg_color(alpha)),
            gs_make_card("SHARPE", gs_fmt_num(sharpe, 3), this, gs_pos_neg_color(sharpe)),
            gs_make_card("SORTINO", gs_fmt_num(d.value("sortino_ratio").toDouble(), 3), this),
            gs_make_card("INFO RATIO", gs_fmt_num(info, 3), this, gs_pos_neg_color(info)),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> vsBench = {
            gs_make_card("TRACKING ERROR", gs_fmt_pct(te), this),
            gs_make_card("R²", gs_fmt_num(r2, 4), this),
            gs_make_card("HIT RATE", QString::number(hit, 'f', 1) + "%", this,
                         hit > 50 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("MAX DRAWDOWN", gs_fmt_pct(d.value("max_drawdown").toDouble()), this,
                         ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(vsBench, this));

        QList<QWidget*> capture = {
            gs_make_card("UP CAPTURE", QString::number(d.value("up_capture").toDouble(), 'f', 1) + "%", this),
            gs_make_card("DOWN CAPTURE", QString::number(d.value("down_capture").toDouble(), 'f', 1) + "%", this),
            gs_make_card("CAPTURE RATIO", gs_fmt_num(d.value("capture_ratio").toDouble(), 3), this),
            gs_make_card("ANNUAL RISK", gs_fmt_pct(d.value("annual_risk").toDouble()), this),
        };
        results_layout_->addWidget(gs_card_row(capture, this));

        QList<QWidget*> jensen = {
            gs_make_card("JENSEN ALPHA", gs_fmt_pct(d.value("jensen_alpha").toDouble()), this,
                         gs_pos_neg_color(d.value("jensen_alpha").toDouble())),
            gs_make_card("JENSEN BULL", gs_fmt_pct(d.value("jensen_alpha_bull").toDouble()), this),
            gs_make_card("JENSEN BEAR", gs_fmt_pct(d.value("jensen_alpha_bear").toDouble()), this),
            gs_make_card("TREYNOR", gs_fmt_num(d.value("treynor_measure").toDouble(), 3), this),
        };
        results_layout_->addWidget(gs_card_row(jensen, this));

        QList<QWidget*> moments = {
            gs_make_card("SKEWNESS", gs_fmt_num(d.value("skewness").toDouble(), 3), this),
            gs_make_card("KURTOSIS", gs_fmt_num(d.value("kurtosis").toDouble(), 3), this),
            gs_make_card("CALMAR", gs_fmt_num(d.value("calmar_ratio").toDouble(), 3), this),
            gs_make_card("MODIGLIANI", gs_fmt_pct(d.value("modigliani_ratio").toDouble()), this),
        };
        results_layout_->addWidget(gs_card_row(moments, this));

        status_label_->setText(QString("Alpha %1%  |  IR %2  |  R² %3")
                                   .arg(alpha * 100, 0, 'f', 2).arg(info, 0, 'f', 2)
                                   .arg(r2, 0, 'f', 3));
        return;
    }

    // ── 3. GREEKS ────────────────────────────────────────────────────────────
    if (command == "greeks") {
        const QJsonObject g = d.value("greeks").toObject();
        const double spot = d.value("spot").toDouble();
        const double strike = d.value("strike").toDouble();
        const QString opt = d.value("option_type").toString().toUpper();
        const double moneyness = d.value("moneyness").toDouble();
        const QString moneyness_label = moneyness > 1.02 ? "ITM" : moneyness < 0.98 ? "OTM" : "ATM";

        // Contract summary strip
        auto* summary = new QLabel(QString("%1  |  Spot %2  →  Strike %3  |  %4 (%5)  |  Vol %6%  |  T %7y")
                                       .arg(opt)
                                       .arg(spot, 0, 'f', 2).arg(strike, 0, 'f', 2)
                                       .arg(moneyness_label).arg(moneyness, 0, 'f', 3)
                                       .arg(d.value("vol").toDouble() * 100, 0, 'f', 1)
                                       .arg(d.value("expiry_years").toDouble(), 0, 'f', 3));
        summary->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                       "padding:8px 10px; background:%2; border:1px solid %3;")
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                        ui::colors::BORDER_DIM()));
        results_layout_->addWidget(summary);

        // First-order Greeks (the four every desk watches)
        QList<QWidget*> first = {
            gs_make_card("DELTA", gs_fmt_num(g.value("delta").toDouble(), 4), this,
                         opt == "CALL" ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()),
            gs_make_card("GAMMA", gs_fmt_num(g.value("gamma").toDouble(), 4), this),
            gs_make_card("VEGA", gs_fmt_num(g.value("vega").toDouble(), 4), this),
            gs_make_card("THETA", gs_fmt_num(g.value("theta").toDouble(), 4), this,
                         ui::colors::WARNING()),
        };
        results_layout_->addWidget(gs_card_row(first, this));

        QList<QWidget*> second = {
            gs_make_card("RHO", gs_fmt_num(g.value("rho").toDouble(), 4), this),
            gs_make_card("VANNA", gs_fmt_num(g.value("vanna").toDouble(), 4), this),
            gs_make_card("VOLGA", gs_fmt_num(g.value("volga").toDouble(), 4), this),
            gs_make_card("CHARM", gs_fmt_num(g.value("charm").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(second, this));

        QList<QWidget*> higher = {
            gs_make_card("SPEED", gs_fmt_num(g.value("speed").toDouble(), 6), this),
            gs_make_card("ZOMMA", gs_fmt_num(g.value("zomma").toDouble(), 6), this),
            gs_make_card("COLOR", gs_fmt_num(g.value("color").toDouble(), 6), this),
            gs_make_card("ULTIMA", gs_fmt_num(g.value("ultima").toDouble(), 6), this),
        };
        results_layout_->addWidget(gs_card_row(higher, this));

        status_label_->setText(QString("%1 %2 / %3  |  Δ=%4  Γ=%5  ν=%6")
                                   .arg(opt).arg(spot, 0, 'f', 2).arg(strike, 0, 'f', 2)
                                   .arg(g.value("delta").toDouble(), 0, 'f', 3)
                                   .arg(g.value("gamma").toDouble(), 0, 'f', 4)
                                   .arg(g.value("vega").toDouble(), 0, 'f', 3));
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
        auto* hdr = new QLabel(QString("Position %1  |  Confidence %2%  |  1-day horizon")
                                   .arg(gs_fmt_money(pos)).arg(conf * 100, 0, 'f', 1));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        // VaR amount cards (loss in $) — color escalates with size
        auto var_color = [pos](double amt) {
            const double pct = pos > 0 ? std::abs(amt) / pos : 0;
            if (pct > 0.05) return QString(ui::colors::NEGATIVE());
            if (pct > 0.02) return QString(ui::colors::WARNING());
            return QString(ui::colors::TEXT_PRIMARY());
        };

        QList<QWidget*> amounts = {
            gs_make_card("PARAMETRIC VaR", gs_fmt_money(parm.value("var_amount").toDouble()), this,
                         var_color(parm.value("var_amount").toDouble())),
            gs_make_card("HISTORICAL VaR", gs_fmt_money(hist.value("var_amount").toDouble()), this,
                         var_color(hist.value("var_amount").toDouble())),
            gs_make_card("MONTE CARLO VaR", gs_fmt_money(mc.value("var_amount").toDouble()), this,
                         var_color(mc.value("var_amount").toDouble())),
            gs_make_card("CVaR (EXP. SHORT.)", gs_fmt_money(cv.value("var_amount").toDouble()), this,
                         ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(amounts, this));

        // VaR as % of position
        QList<QWidget*> pcts = {
            gs_make_card("PARAMETRIC %", gs_fmt_num(parm.value("var_percentage").toDouble(), 2) + "%", this),
            gs_make_card("HISTORICAL %", gs_fmt_num(hist.value("var_percentage").toDouble(), 2) + "%", this),
            gs_make_card("MONTE CARLO %", gs_fmt_num(mc.value("var_percentage").toDouble(), 2) + "%", this),
            gs_make_card("CVaR %", gs_fmt_num(std::abs(cv.value("var_percentage").toDouble()), 2) + "%", this,
                         ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(pcts, this));

        // Methodology table — each method as a row
        auto* table = new QTableWidget(4, 4, this);
        table->setHorizontalHeaderLabels({"Method", "VaR Amount", "VaR %", "Notes"});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(150);

        const QList<QPair<QString, QJsonObject>> rows = {
            {"Parametric (Variance-Covariance)", parm},
            {"Historical Simulation", hist},
            {"Monte Carlo Simulation", mc},
            {"CVaR / Expected Shortfall", cv},
        };
        for (int r = 0; r < rows.size(); ++r) {
            const auto& obj = rows[r].second;
            table->setItem(r, 0, new QTableWidgetItem(rows[r].first));
            auto* amt_item = new QTableWidgetItem(gs_fmt_money(obj.value("var_amount").toDouble()));
            amt_item->setForeground(QColor(ui::colors::NEGATIVE()));
            amt_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, 1, amt_item);
            auto* pct_item = new QTableWidgetItem(QString::number(obj.value("var_percentage").toDouble(), 'f', 3) + "%");
            pct_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, 2, pct_item);
            table->setItem(r, 3, new QTableWidgetItem(obj.value("method").toString()));
            table->setRowHeight(r, 24);
        }
        results_layout_->addWidget(table);

        status_label_->setText(QString("VaR(%1%) param=%2  hist=%3  mc=%4  cvar=%5")
                                   .arg(conf * 100, 0, 'f', 0)
                                   .arg(gs_fmt_money(parm.value("var_amount").toDouble()))
                                   .arg(gs_fmt_money(hist.value("var_amount").toDouble()))
                                   .arg(gs_fmt_money(mc.value("var_amount").toDouble()))
                                   .arg(gs_fmt_money(cv.value("var_amount").toDouble())));
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
        auto* alloc = new QLabel(QString("Portfolio %1  |  %2").arg(gs_fmt_money(pos), parts.join("  •  ")));
        alloc->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New';"
                                      "padding:8px 10px; background:%2; border:1px solid %3;")
                                  .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                       ui::colors::BORDER_DIM()));
        results_layout_->addWidget(alloc);

        if (scenarios.isEmpty()) {
            display_error("Stress test returned no scenarios.");
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
            if (p < worst_pnl) { worst_pnl = p; worst_name = o.value("scenario_name").toString(); }
            if (p > best_pnl)  { best_pnl  = p; best_name  = o.value("scenario_name").toString(); }
        }
        QList<QWidget*> summary_cards = {
            gs_make_card("WORST CASE PnL", gs_fmt_signed_money(worst_pnl), this, ui::colors::NEGATIVE()),
            gs_make_card("WORST CASE %", QString("%1%").arg(worst_pnl / pos * 100, 0, 'f', 2), this,
                         ui::colors::NEGATIVE()),
            gs_make_card("BEST CASE PnL", gs_fmt_signed_money(best_pnl), this,
                         best_pnl > 0 ? ui::colors::POSITIVE() : ui::colors::TEXT_PRIMARY()),
            gs_make_card("SCENARIOS RUN", QString::number(scenarios.size()), this),
        };
        results_layout_->addWidget(gs_card_row(summary_cards, this));

        // Worst/best name strip
        auto* wb = new QLabel(QString("Worst:  %1   |   Best:  %2").arg(worst_name, best_name));
        wb->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                  "padding:6px 10px; background:%2; border-left:3px solid %3;")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                   ui::colors::WARNING()));
        results_layout_->addWidget(wb);

        // Full scenario table
        auto* table = new QTableWidget(scenarios.size(), 4, this);
        table->setHorizontalHeaderLabels({"Scenario", "PnL", "PnL %", "Shocked Value"});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        table->setStyleSheet(table_ss());
        table->setMaximumHeight(qMin(scenarios.size() * 26 + 32, 320));

        for (int i = 0; i < scenarios.size(); ++i) {
            const auto o = scenarios[i].toObject();
            const double pnl = o.value("pnl").toDouble();
            const double pnl_pct = o.value("pnl_percentage").toDouble();
            const double shocked = o.value("shocked_value").toDouble();
            table->setItem(i, 0, new QTableWidgetItem(o.value("scenario_name").toString()));
            auto* pnl_item = new QTableWidgetItem(gs_fmt_signed_money(pnl));
            pnl_item->setForeground(QColor(gs_pos_neg_color(pnl)));
            pnl_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(i, 1, pnl_item);
            auto* pct_item = new QTableWidgetItem(QString::number(pnl_pct, 'f', 2) + "%");
            pct_item->setForeground(QColor(gs_pos_neg_color(pnl_pct)));
            pct_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(i, 2, pct_item);
            auto* sv_item = new QTableWidgetItem(gs_fmt_money(shocked));
            sv_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(i, 3, sv_item);
            table->setRowHeight(i, 24);
        }
        results_layout_->addWidget(table);

        status_label_->setText(QString("Worst: %1  (%2)  |  %3 scenarios")
                                   .arg(gs_fmt_signed_money(worst_pnl), worst_name).arg(scenarios.size()));
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

        const double total_ret = m.value("total_return").toDouble();
        const double ann_ret = m.value("annualized_return").toDouble();
        const double sharpe = m.value("sharpe_ratio").toDouble();
        const double mdd = m.value("max_drawdown").toDouble();
        const double final_val = m.value("final_value").toDouble();
        const int trades = m.value("num_trades").toInt();

        // Header strip
        auto* hdr = new QLabel(QString("%1  |  %2  |  %3  →  %4  |  Initial %5")
                                   .arg(strategy.toUpper().replace('_', ' '), ticker, start, end,
                                        gs_fmt_money(init_cap)));
        hdr->setStyleSheet(QString("color:%1; font-size:11px; font-family:'Courier New'; font-weight:700;"
                                   "padding:8px 10px; background:%2; border:1px solid %3;")
                               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(),
                                    ui::colors::BORDER_DIM()));
        results_layout_->addWidget(hdr);

        QList<QWidget*> top = {
            gs_make_card("FINAL VALUE", gs_fmt_money(final_val), this, gs_pos_neg_color(final_val - init_cap)),
            gs_make_card("TOTAL RETURN", gs_fmt_pct(total_ret), this, gs_pos_neg_color(total_ret)),
            gs_make_card("ANN. RETURN", gs_fmt_pct(ann_ret), this, gs_pos_neg_color(ann_ret)),
            gs_make_card("SHARPE", gs_fmt_num(sharpe, 3), this,
                         sharpe >= 1.0 ? ui::colors::POSITIVE()
                                      : sharpe >= 0  ? ui::colors::TEXT_PRIMARY()
                                                     : ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(top, this));

        QList<QWidget*> risk_row = {
            gs_make_card("VOLATILITY", gs_fmt_pct(m.value("volatility").toDouble()), this),
            gs_make_card("MAX DRAWDOWN", gs_fmt_pct(mdd), this,
                         mdd <= -0.20 ? ui::colors::NEGATIVE() : ui::colors::WARNING()),
            gs_make_card("BEST DAY", gs_fmt_pct(m.value("best_day").toDouble()), this, ui::colors::POSITIVE()),
            gs_make_card("WORST DAY", gs_fmt_pct(m.value("worst_day").toDouble()), this, ui::colors::NEGATIVE()),
        };
        results_layout_->addWidget(gs_card_row(risk_row, this));

        QList<QWidget*> activity = {
            gs_make_card("TRADES", QString::number(trades), this),
            gs_make_card("WIN RATE", QString::number(m.value("win_rate_pct").toDouble(), 'f', 1) + "%", this,
                         m.value("win_rate_pct").toDouble() > 50 ? ui::colors::POSITIVE() : ui::colors::WARNING()),
            gs_make_card("DAYS", QString::number(m.value("n_days").toInt()), this),
            gs_make_card("PROFIT", gs_fmt_signed_money(final_val - init_cap), this,
                         gs_pos_neg_color(final_val - init_cap)),
        };
        results_layout_->addWidget(gs_card_row(activity, this));

        // Equity curve preview — show first/middle/last sample as ASCII strip + min/max
        if (curve.size() >= 2) {
            double curve_min = init_cap, curve_max = init_cap;
            for (const auto& v : curve) {
                const double val = v.toObject().value("value").toDouble();
                curve_min = std::min(curve_min, val);
                curve_max = std::max(curve_max, val);
            }
            auto* curve_lbl = new QLabel(
                QString("Equity curve: %1 samples  |  Range %2 → %3")
                    .arg(curve.size()).arg(gs_fmt_money(curve_min), gs_fmt_money(curve_max)));
            curve_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-family:'Courier New';"
                                             "padding:6px 10px; background:%2; border-left:3px solid %3;")
                                         .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_SURFACE(),
                                              accent));
            results_layout_->addWidget(curve_lbl);

            // Sampled equity table — show 10 evenly-spaced points
            const int n_show = std::min<int>(10, curve.size());
            const int step = std::max<int>(1, curve.size() / n_show);
            auto* eq_table = new QTableWidget(0, 3, this);
            eq_table->setHorizontalHeaderLabels({"Date", "Portfolio Value", "Return"});
            eq_table->verticalHeader()->setVisible(false);
            eq_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            eq_table->setSelectionMode(QAbstractItemView::NoSelection);
            eq_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            eq_table->setStyleSheet(table_ss());
            eq_table->setMaximumHeight(280);
            int row = 0;
            for (int i = 0; i < curve.size(); i += step) {
                const auto o = curve[i].toObject();
                const double val = o.value("value").toDouble();
                const double ret = init_cap > 0 ? (val / init_cap - 1.0) : 0.0;
                eq_table->insertRow(row);
                eq_table->setItem(row, 0, new QTableWidgetItem(o.value("date").toString()));
                auto* val_item = new QTableWidgetItem(gs_fmt_money(val));
                val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                eq_table->setItem(row, 1, val_item);
                auto* ret_item = new QTableWidgetItem(gs_fmt_pct(ret));
                ret_item->setForeground(QColor(gs_pos_neg_color(ret)));
                ret_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                eq_table->setItem(row, 2, ret_item);
                eq_table->setRowHeight(row, 22);
                ++row;
            }
            results_layout_->addWidget(eq_table);
        }

        status_label_->setText(QString("%1 on %2: %3 over %4 trades  (Sharpe %5)")
                                   .arg(strategy, ticker).arg(gs_fmt_pct(total_ret)).arg(trades)
                                   .arg(sharpe, 0, 'f', 2));
        return;
    }

    // ── 7. STATISTICS ────────────────────────────────────────────────────────
    if (command == "statistics") {
        QList<QWidget*> central = {
            gs_make_card("MEAN", gs_fmt_num(d.value("mean").toDouble(), 4), this),
            gs_make_card("MEDIAN", gs_fmt_num(d.value("median").toDouble(), 4), this),
            gs_make_card("STD DEV", gs_fmt_num(d.value("std").toDouble(), 4), this),
            gs_make_card("VARIANCE", gs_fmt_num(d.value("variance").toDouble(), 4), this),
        };
        results_layout_->addWidget(gs_card_row(central, this));

        QList<QWidget*> spread = {
            gs_make_card("MIN", gs_fmt_num(d.value("min").toDouble(), 4), this, ui::colors::NEGATIVE()),
            gs_make_card("MAX", gs_fmt_num(d.value("max").toDouble(), 4), this, ui::colors::POSITIVE()),
            gs_make_card("RANGE", gs_fmt_num(d.value("range").toDouble(), 4), this),
            gs_make_card("SUM", gs_fmt_num(d.value("sum").toDouble(), 2), this),
        };
        results_layout_->addWidget(gs_card_row(spread, this));

        QList<QWidget*> shape = {
            gs_make_card("SKEWNESS", gs_fmt_num(d.value("skewness").toDouble(), 3), this,
                         gs_pos_neg_color(d.value("skewness").toDouble())),
            gs_make_card("KURTOSIS", gs_fmt_num(d.value("kurtosis").toDouble(), 3), this),
            gs_make_card("SEMI-VAR", gs_fmt_num(d.value("semi_variance").toDouble(), 4), this),
            gs_make_card("REALIZED VAR", gs_fmt_num(d.value("realized_variance").toDouble(), 2), this),
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
            table->setHorizontalHeaderLabels({"Percentile", "Value"});
            table->verticalHeader()->setVisible(false);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setStyleSheet(table_ss());
            table->setMaximumHeight(qMin(ordered_keys.size() * 24 + 32, 280));
            for (int i = 0; i < ordered_keys.size(); ++i) {
                const int k = ordered_keys[i];
                table->setItem(i, 0, new QTableWidgetItem(QString("p%1").arg(k)));
                auto* v_item = new QTableWidgetItem(gs_fmt_num(pcts.value(QString::number(k)).toDouble(), 4));
                v_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(i, 1, v_item);
                table->setRowHeight(i, 22);
            }
            results_layout_->addWidget(table);
        }

        status_label_->setText(QString("μ=%1  σ=%2  n=%3")
                                   .arg(d.value("mean").toDouble(), 0, 'f', 3)
                                   .arg(d.value("std").toDouble(), 0, 'f', 3)
                                   .arg(d.value("count").toInt()));
        return;
    }

    // ── Fallback for any unknown command — generic table ─────────────────────
    display_result(d);
}

} // namespace fincept::screens
