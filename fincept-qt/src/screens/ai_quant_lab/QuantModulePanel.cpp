// src/screens/ai_quant_lab/QuantModulePanel.cpp
#include "screens/ai_quant_lab/QuantModulePanel.h"

#include "core/logging/Logger.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>

#include <cmath>

namespace fincept::screens {

using namespace fincept::services::quant;

// ── Helpers ──────────────────────────────────────────────────────────────────

QDoubleSpinBox* QuantModulePanel::make_double_spin(double min, double max, double val, int decimals,
                                                   const QString& suffix, QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setDecimals(decimals);
    if (!suffix.isEmpty())
        spin->setSuffix(suffix);
    spin->setStyleSheet(QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                                "QDoubleSpinBox:focus { border-color:%6; }")
                            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL)
                            .arg(module_.color.name()));
    return spin;
}

QPushButton* QuantModulePanel::make_run_button(const QString& text, QWidget* parent) {
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                               "font-weight:700; border:none; padding:8px 20px; border-radius:2px;"
                               "letter-spacing:1px; }"
                               "QPushButton:hover { background:%5; }")
                           .arg(module_.color.name())
                           .arg(ui::colors::BG_BASE)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(module_.color.lighter(120).name()));
    return btn;
}

QWidget* QuantModulePanel::build_input_row(const QString& label, QWidget* input, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 2, 0, 2);
    hl->setSpacing(8);
    auto* lbl = new QLabel(label, row);
    lbl->setFixedWidth(160);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                           .arg(ui::colors::TEXT_SECONDARY)
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(lbl);
    hl->addWidget(input, 1);
    return row;
}

// ── Constructor ──────────────────────────────────────────────────────────────

QuantModulePanel::QuantModulePanel(const QuantModule& mod, QWidget* parent) : QWidget(parent), module_(mod) {
    build_ui();
    connect_service();
}

void QuantModulePanel::connect_service() {
    auto& svc = AIQuantLabService::instance();
    connect(&svc, &AIQuantLabService::result_ready, this, &QuantModulePanel::on_result);
    connect(&svc, &AIQuantLabService::error_occurred, this, &QuantModulePanel::on_error);
}

void QuantModulePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Module header
    auto* header = new QWidget(this);
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel(module_.label.toUpper(), header);
    title->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                 "letter-spacing:1px;")
                             .arg(module_.color.name())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    auto* div = new QWidget(header);
    div->setFixedSize(1, 14);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    hhl->addWidget(div);
    auto* cat = new QLabel(module_.category, header);
    cat->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(cat);
    hhl->addStretch();
    status_label_ = new QLabel(header);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(status_label_);
    root->addWidget(header);

    // Content
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    QWidget* content = nullptr;
    if (module_.id == "gs_quant")
        content = build_gs_quant_panel();
    else if (module_.id == "cfa_quant")
        content = build_cfa_quant_panel();
    else if (module_.id == "backtesting")
        content = build_backtesting_panel();
    else if (module_.id == "rl_trading")
        content = build_rl_trading_panel();
    else if (module_.id == "advanced_models")
        content = build_advanced_models_panel();
    else
        content = build_generic_panel();

    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// GS QUANT PANEL — 7 analysis modes
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_gs_quant_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700;"
                                "border-bottom:2px solid %7; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY)
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL)
                            .arg(module_.color.name()));

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(module_.color.name());

    // ── Risk Metrics ──
    auto* risk = new QWidget;
    auto* rvl = new QVBoxLayout(risk);
    rvl->setContentsMargins(12, 12, 12, 12);
    rvl->setSpacing(8);
    auto* risk_returns = new QLineEdit(risk);
    risk_returns->setPlaceholderText("Daily returns (comma-separated, e.g. 0.01,-0.02,0.005)");
    risk_returns->setStyleSheet(input_style);
    text_inputs_["gs_risk_returns"] = risk_returns;
    rvl->addWidget(build_input_row("Daily Returns", risk_returns, risk));
    auto* risk_rf = make_double_spin(0, 20, 4.5, 2, "%", risk);
    double_inputs_["gs_risk_rf"] = risk_rf;
    rvl->addWidget(build_input_row("Risk-Free Rate", risk_rf, risk));
    auto* risk_run = make_run_button("CALCULATE RISK METRICS", risk);
    connect(risk_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        params["returns"] = text_inputs_["gs_risk_returns"]->text();
        params["risk_free_rate"] = double_inputs_["gs_risk_rf"]->value() / 100.0;
        AIQuantLabService::instance().gs_risk_metrics(params);
    });
    rvl->addWidget(risk_run);
    rvl->addStretch();
    tabs->addTab(risk, "Risk Metrics");

    // ── Portfolio Analytics ──
    auto* port = new QWidget;
    auto* pvl = new QVBoxLayout(port);
    pvl->setContentsMargins(12, 12, 12, 12);
    pvl->setSpacing(8);
    auto* port_ret = new QLineEdit(port);
    port_ret->setPlaceholderText("Portfolio returns (comma-separated)");
    port_ret->setStyleSheet(input_style);
    text_inputs_["gs_port_returns"] = port_ret;
    pvl->addWidget(build_input_row("Portfolio Returns", port_ret, port));
    auto* bench_ret = new QLineEdit(port);
    bench_ret->setPlaceholderText("Benchmark returns (comma-separated)");
    bench_ret->setStyleSheet(input_style);
    text_inputs_["gs_bench_returns"] = bench_ret;
    pvl->addWidget(build_input_row("Benchmark Returns", bench_ret, port));
    auto* port_run = make_run_button("ANALYZE PORTFOLIO", port);
    connect(port_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing...");
        QJsonObject params;
        params["returns"] = text_inputs_["gs_port_returns"]->text();
        params["benchmark_returns"] = text_inputs_["gs_bench_returns"]->text();
        AIQuantLabService::instance().gs_portfolio_analytics(params);
    });
    pvl->addWidget(port_run);
    pvl->addStretch();
    tabs->addTab(port, "Portfolio");

    // ── Options Greeks ──
    auto* greeks = new QWidget;
    auto* gvl = new QVBoxLayout(greeks);
    gvl->setContentsMargins(12, 12, 12, 12);
    gvl->setSpacing(8);
    auto* g_spot = make_double_spin(0, 1e6, 100, 2, "", greeks);
    double_inputs_["gs_spot"] = g_spot;
    gvl->addWidget(build_input_row("Spot Price ($)", g_spot, greeks));
    auto* g_strike = make_double_spin(0, 1e6, 100, 2, "", greeks);
    double_inputs_["gs_strike"] = g_strike;
    gvl->addWidget(build_input_row("Strike Price ($)", g_strike, greeks));
    auto* g_expiry = make_double_spin(0.01, 120, 6, 1, " mo", greeks);
    double_inputs_["gs_expiry"] = g_expiry;
    gvl->addWidget(build_input_row("Expiry (months)", g_expiry, greeks));
    auto* g_rate = make_double_spin(0, 30, 5, 2, "%", greeks);
    double_inputs_["gs_rate"] = g_rate;
    gvl->addWidget(build_input_row("Risk-Free Rate", g_rate, greeks));
    auto* g_vol = make_double_spin(1, 200, 25, 1, "%", greeks);
    double_inputs_["gs_vol"] = g_vol;
    gvl->addWidget(build_input_row("Volatility", g_vol, greeks));
    auto* g_type = new QComboBox(greeks);
    g_type->addItems({"call", "put"});
    g_type->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                  "font-family:%4; font-size:%5px; padding:4px 6px; }")
                              .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                              .arg(ui::fonts::DATA_FAMILY)
                              .arg(ui::fonts::SMALL));
    combo_inputs_["gs_option_type"] = g_type;
    gvl->addWidget(build_input_row("Option Type", g_type, greeks));
    auto* greeks_run = make_run_button("CALCULATE GREEKS", greeks);
    connect(greeks_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
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
    auto* var = new QWidget;
    auto* vvl = new QVBoxLayout(var);
    vvl->setContentsMargins(12, 12, 12, 12);
    vvl->setSpacing(8);
    auto* var_ret = new QLineEdit(var);
    var_ret->setPlaceholderText("Daily returns (comma-separated)");
    var_ret->setStyleSheet(input_style);
    text_inputs_["gs_var_returns"] = var_ret;
    vvl->addWidget(build_input_row("Daily Returns", var_ret, var));
    auto* var_pos = make_double_spin(0, 1e12, 1e6, 0, "", var);
    double_inputs_["gs_var_position"] = var_pos;
    vvl->addWidget(build_input_row("Position Value ($)", var_pos, var));
    auto* var_conf = make_double_spin(0.90, 0.99, 0.95, 2, "", var);
    double_inputs_["gs_var_confidence"] = var_conf;
    vvl->addWidget(build_input_row("Confidence Level", var_conf, var));
    auto* var_run = make_run_button("CALCULATE VaR", var);
    connect(var_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        params["returns"] = text_inputs_["gs_var_returns"]->text();
        params["position_value"] = double_inputs_["gs_var_position"]->value();
        params["confidence"] = double_inputs_["gs_var_confidence"]->value();
        AIQuantLabService::instance().gs_var_analysis(params);
    });
    vvl->addWidget(var_run);
    vvl->addStretch();
    tabs->addTab(var, "VaR");

    // ── Stress Test ──
    auto* stress = new QWidget;
    auto* svl = new QVBoxLayout(stress);
    svl->setContentsMargins(12, 12, 12, 12);
    svl->setSpacing(8);
    auto* st_ret = new QLineEdit(stress);
    st_ret->setPlaceholderText("Daily returns (comma-separated)");
    st_ret->setStyleSheet(input_style);
    text_inputs_["gs_stress_returns"] = st_ret;
    svl->addWidget(build_input_row("Daily Returns", st_ret, stress));
    auto* st_pos = make_double_spin(0, 1e12, 1e6, 0, "", stress);
    double_inputs_["gs_stress_position"] = st_pos;
    svl->addWidget(build_input_row("Position Value ($)", st_pos, stress));
    auto* st_hint = new QLabel("Tests 9 historical crisis scenarios: 2008, COVID-19, etc.", stress);
    st_hint->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    svl->addWidget(st_hint);
    auto* stress_run = make_run_button("RUN STRESS TEST", stress);
    connect(stress_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Stress testing...");
        QJsonObject params;
        params["returns"] = text_inputs_["gs_stress_returns"]->text();
        params["position_value"] = double_inputs_["gs_stress_position"]->value();
        AIQuantLabService::instance().gs_stress_test(params);
    });
    svl->addWidget(stress_run);
    svl->addStretch();
    tabs->addTab(stress, "Stress Test");

    // ── Backtest ──
    auto* bt = new QWidget;
    auto* bvl = new QVBoxLayout(bt);
    bvl->setContentsMargins(12, 12, 12, 12);
    bvl->setSpacing(8);
    auto* bt_strat = new QComboBox(bt);
    bt_strat->addItems({"buy_hold", "momentum", "mean_reversion", "rebalancing"});
    bt_strat->setStyleSheet(g_type->styleSheet());
    combo_inputs_["gs_bt_strategy"] = bt_strat;
    bvl->addWidget(build_input_row("Strategy", bt_strat, bt));
    auto* bt_ticker = new QLineEdit(bt);
    bt_ticker->setPlaceholderText("AAPL");
    bt_ticker->setStyleSheet(input_style);
    text_inputs_["gs_bt_ticker"] = bt_ticker;
    bvl->addWidget(build_input_row("Ticker", bt_ticker, bt));
    auto* bt_capital = make_double_spin(1000, 1e12, 100000, 0, "", bt);
    double_inputs_["gs_bt_capital"] = bt_capital;
    bvl->addWidget(build_input_row("Initial Capital ($)", bt_capital, bt));
    auto* bt_lookback = new QSpinBox(bt);
    bt_lookback->setRange(5, 252);
    bt_lookback->setValue(20);
    bt_lookback->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                       "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                   .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::fonts::SMALL));
    int_inputs_["gs_bt_lookback"] = bt_lookback;
    bvl->addWidget(build_input_row("Lookback Period", bt_lookback, bt));
    auto* bt_run = make_run_button("RUN BACKTEST", bt);
    connect(bt_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Backtesting...");
        QJsonObject params;
        params["strategy"] = combo_inputs_["gs_bt_strategy"]->currentText();
        params["ticker"] = text_inputs_["gs_bt_ticker"]->text();
        params["initial_capital"] = double_inputs_["gs_bt_capital"]->value();
        params["lookback"] = int_inputs_["gs_bt_lookback"]->value();
        AIQuantLabService::instance().gs_backtest(params);
    });
    bvl->addWidget(bt_run);
    bvl->addStretch();
    tabs->addTab(bt, "Backtest");

    // ── Statistics ──
    auto* stats = new QWidget;
    auto* stvl = new QVBoxLayout(stats);
    stvl->setContentsMargins(12, 12, 12, 12);
    stvl->setSpacing(8);
    auto* st_vals = new QLineEdit(stats);
    st_vals->setPlaceholderText("Values (comma-separated, e.g. 10.5,11.2,9.8)");
    st_vals->setStyleSheet(input_style);
    text_inputs_["gs_stats_values"] = st_vals;
    stvl->addWidget(build_input_row("Values", st_vals, stats));
    auto* stats_run = make_run_button("CALCULATE STATISTICS", stats);
    connect(stats_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        params["values"] = text_inputs_["gs_stats_values"]->text();
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
// CFA QUANT PANEL — 8 analysis types
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_cfa_quant_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    // Data input
    auto* data_lbl = new QLabel("DATA INPUT", w);
    data_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                    "letter-spacing:1px;")
                                .arg(module_.color.name())
                                .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(data_lbl);

    auto* symbol = new QLineEdit(w);
    symbol->setPlaceholderText("Ticker symbol (e.g. AAPL) or comma-separated values");
    symbol->setStyleSheet(input_style);
    text_inputs_["cfa_symbol"] = symbol;
    vl->addWidget(build_input_row("Symbol / Data", symbol, w));

    // Analysis type selector
    auto* analysis_type = new QComboBox(w);
    analysis_type->addItems({"trend_analysis", "stationarity_test", "arima_model", "forecasting", "supervised_learning",
                             "unsupervised_learning", "resampling_methods", "validate_data"});
    analysis_type->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                         "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                     .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::fonts::SMALL));
    combo_inputs_["cfa_analysis"] = analysis_type;
    vl->addWidget(build_input_row("Analysis Type", analysis_type, w));

    auto* run = make_run_button("RUN ANALYSIS", w);
    connect(run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running...");
        QJsonObject params;
        params["data"] = text_inputs_["cfa_symbol"]->text();
        auto cmd = combo_inputs_["cfa_analysis"]->currentText();
        AIQuantLabService::instance().run_module("cfa_quant", cmd, params);
    });
    vl->addWidget(run);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BACKTESTING PANEL
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_backtesting_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    auto* strat = new QComboBox(w);
    strat->addItems({"topk_dropout", "weight_based", "enhanced_indexing"});
    strat->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                 "font-family:%4; font-size:%5px; padding:4px 6px; }")
                             .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::fonts::SMALL));
    combo_inputs_["bt_strategy"] = strat;
    vl->addWidget(build_input_row("Strategy", strat, w));

    auto* instruments = new QLineEdit(w);
    instruments->setPlaceholderText("AAPL,MSFT,GOOG,AMZN");
    instruments->setStyleSheet(input_style);
    text_inputs_["bt_instruments"] = instruments;
    vl->addWidget(build_input_row("Instruments", instruments, w));

    auto* start_date = new QLineEdit(w);
    start_date->setPlaceholderText("2020-01-01");
    start_date->setStyleSheet(input_style);
    text_inputs_["bt_start"] = start_date;
    vl->addWidget(build_input_row("Start Date", start_date, w));

    auto* end_date = new QLineEdit(w);
    end_date->setPlaceholderText("2024-01-01");
    end_date->setStyleSheet(input_style);
    text_inputs_["bt_end"] = end_date;
    vl->addWidget(build_input_row("End Date", end_date, w));

    auto* capital = make_double_spin(1000, 1e12, 1e6, 0, "", w);
    double_inputs_["bt_capital"] = capital;
    vl->addWidget(build_input_row("Initial Capital ($)", capital, w));

    auto* topk = new QSpinBox(w);
    topk->setRange(1, 100);
    topk->setValue(10);
    topk->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; }")
                            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL));
    int_inputs_["bt_topk"] = topk;
    vl->addWidget(build_input_row("Top K Positions", topk, w));

    auto* benchmark = new QLineEdit(w);
    benchmark->setPlaceholderText("SH000300 (CSI300)");
    benchmark->setStyleSheet(input_style);
    text_inputs_["bt_benchmark"] = benchmark;
    vl->addWidget(build_input_row("Benchmark", benchmark, w));

    auto* run = make_run_button("RUN BACKTEST", w);
    connect(run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Backtesting...");
        QJsonObject params;
        QJsonObject strategy_config;
        strategy_config["type"] = combo_inputs_["bt_strategy"]->currentText();
        strategy_config["topk"] = int_inputs_["bt_topk"]->value();
        params["strategy_config"] = strategy_config;
        QJsonObject dataset;
        dataset["instruments"] = text_inputs_["bt_instruments"]->text();
        dataset["start_date"] = text_inputs_["bt_start"]->text();
        dataset["end_date"] = text_inputs_["bt_end"]->text();
        params["dataset_config"] = dataset;
        QJsonObject portfolio;
        portfolio["initial_capital"] = double_inputs_["bt_capital"]->value();
        portfolio["benchmark"] = text_inputs_["bt_benchmark"]->text();
        params["portfolio_config"] = portfolio;
        AIQuantLabService::instance().run_backtest(params);
    });
    vl->addWidget(run);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RL TRADING PANEL
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_rl_trading_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* algo = new QComboBox(w);
    algo->addItems({"PPO", "DQN", "A2C", "SAC", "TD3"});
    algo->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; }")
                            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL));
    combo_inputs_["rl_algo"] = algo;
    vl->addWidget(build_input_row("RL Algorithm", algo, w));

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    auto* ticker = new QLineEdit(w);
    ticker->setPlaceholderText("AAPL");
    ticker->setStyleSheet(input_style);
    text_inputs_["rl_ticker"] = ticker;
    vl->addWidget(build_input_row("Ticker", ticker, w));

    auto* episodes = new QSpinBox(w);
    episodes->setRange(10, 10000);
    episodes->setValue(100);
    episodes->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::SMALL));
    int_inputs_["rl_episodes"] = episodes;
    vl->addWidget(build_input_row("Training Episodes", episodes, w));

    auto* lr = make_double_spin(0.00001, 1.0, 0.0003, 5, "", w);
    double_inputs_["rl_learning_rate"] = lr;
    vl->addWidget(build_input_row("Learning Rate", lr, w));

    auto* capital = make_double_spin(1000, 1e12, 100000, 0, "", w);
    double_inputs_["rl_capital"] = capital;
    vl->addWidget(build_input_row("Initial Capital ($)", capital, w));

    auto* train_run = make_run_button("TRAIN RL AGENT", w);
    connect(train_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Training RL Agent...");
        QJsonObject params;
        params["algorithm"] = combo_inputs_["rl_algo"]->currentText();
        params["ticker"] = text_inputs_["rl_ticker"]->text();
        params["episodes"] = int_inputs_["rl_episodes"]->value();
        params["learning_rate"] = double_inputs_["rl_learning_rate"]->value();
        params["initial_capital"] = double_inputs_["rl_capital"]->value();
        AIQuantLabService::instance().train_rl_agent(params);
    });
    vl->addWidget(train_run);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ADVANCED MODELS PANEL
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_advanced_models_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* model_type = new QComboBox(w);
    model_type->addItems(
        {"LSTM", "GRU", "Transformer", "Localformer", "HIST", "GAT", "LightGBM", "XGBoost", "CatBoost"});
    model_type->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                      "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                  .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::fonts::SMALL));
    combo_inputs_["adv_model"] = model_type;
    vl->addWidget(build_input_row("Model Type", model_type, w));

    auto* hidden = new QSpinBox(w);
    hidden->setRange(16, 1024);
    hidden->setValue(64);
    hidden->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                  "font-family:%4; font-size:%5px; padding:4px 6px; }")
                              .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                              .arg(ui::fonts::DATA_FAMILY)
                              .arg(ui::fonts::SMALL));
    int_inputs_["adv_hidden"] = hidden;
    vl->addWidget(build_input_row("Hidden Size", hidden, w));

    auto* layers = new QSpinBox(w);
    layers->setRange(1, 12);
    layers->setValue(2);
    layers->setStyleSheet(hidden->styleSheet());
    int_inputs_["adv_layers"] = layers;
    vl->addWidget(build_input_row("Num Layers", layers, w));

    auto* dropout = make_double_spin(0, 0.9, 0.1, 2, "", w);
    double_inputs_["adv_dropout"] = dropout;
    vl->addWidget(build_input_row("Dropout", dropout, w));

    auto* epochs = new QSpinBox(w);
    epochs->setRange(1, 1000);
    epochs->setValue(50);
    epochs->setStyleSheet(hidden->styleSheet());
    int_inputs_["adv_epochs"] = epochs;
    vl->addWidget(build_input_row("Epochs", epochs, w));

    auto* run = make_run_button("CREATE & TRAIN MODEL", w);
    connect(run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Training...");
        QJsonObject params;
        params["model_type"] = combo_inputs_["adv_model"]->currentText();
        QJsonObject config;
        config["hidden_size"] = int_inputs_["adv_hidden"]->value();
        config["num_layers"] = int_inputs_["adv_layers"]->value();
        config["dropout"] = double_inputs_["adv_dropout"]->value();
        config["epochs"] = int_inputs_["adv_epochs"]->value();
        params["model_config"] = config;
        AIQuantLabService::instance().train_advanced_model(params);
    });
    vl->addWidget(run);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GENERIC PANEL — for modules without specialized UI
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_generic_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* desc = new QLabel(module_.description, w);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; line-height:1.5;")
                            .arg(ui::colors::TEXT_PRIMARY)
                            .arg(ui::fonts::SMALL)
                            .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(desc);

    auto* script_lbl = new QLabel(QString("Python script: %1").arg(module_.script), w);
    script_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;"
                                      "padding:6px; background:%3; border:1px solid %4; border-radius:2px;")
                                  .arg(ui::colors::TEXT_TERTIARY)
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::colors::BG_RAISED)
                                  .arg(ui::colors::BORDER_DIM));
    vl->addWidget(script_lbl);

    // Command input
    auto* cmd = new QLineEdit(w);
    cmd->setPlaceholderText("Command (e.g. analyze, train, list_models)");
    cmd->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:6px 8px; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL));
    text_inputs_["gen_command"] = cmd;
    vl->addWidget(build_input_row("Command", cmd, w));

    // JSON params
    auto* params_edit = new QTextEdit(w);
    params_edit->setPlaceholderText("JSON parameters (optional)\ne.g. {\"ticker\":\"AAPL\"}");
    params_edit->setMaximumHeight(100);
    params_edit->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                       "font-family:%4; font-size:%5px; padding:6px; }")
                                   .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::fonts::SMALL));
    vl->addWidget(params_edit);

    auto* run = make_run_button("EXECUTE", w);
    connect(run, &QPushButton::clicked, this, [this, params_edit]() {
        status_label_->setText("Running...");
        auto cmd_text = text_inputs_["gen_command"]->text().trimmed();
        if (cmd_text.isEmpty())
            cmd_text = "analyze";
        auto json_text = params_edit->toPlainText().trimmed();
        QJsonObject params;
        if (!json_text.isEmpty()) {
            auto doc = QJsonDocument::fromJson(json_text.toUtf8());
            if (!doc.isNull())
                params = doc.object();
        }
        AIQuantLabService::instance().run_module(module_.id, cmd_text, params);
    });
    vl->addWidget(run);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RESULT DISPLAY
// ═══════════════════════════════════════════════════════════════════════════════

void QuantModulePanel::clear_results() {
    if (!results_layout_)
        return;
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void QuantModulePanel::display_error(const QString& msg) {
    clear_results();
    auto* err = new QLabel(msg);
    err->setWordWrap(true);
    err->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:12px;"
                               "background:rgba(220,38,38,0.08); border:1px solid rgba(220,38,38,0.3);"
                               "border-radius:2px;")
                           .arg(ui::colors::NEGATIVE)
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(err);
    status_label_->setText("Error");
}

static QString format_val(const QJsonValue& val) {
    if (val.isDouble()) {
        double v = val.toDouble();
        if (std::abs(v) >= 1e9)
            return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
        if (std::abs(v) >= 1e6)
            return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
        if (std::abs(v) < 1.0 && std::abs(v) > 0.0001)
            return QString("%1%").arg(v * 100, 0, 'f', 2);
        return QString::number(v, 'f', 4);
    }
    if (val.isBool())
        return val.toBool() ? "YES" : "NO";
    if (val.isString())
        return val.toString();
    return QString::fromUtf8("\u2014");
}

void QuantModulePanel::display_result(const QJsonObject& data) {
    clear_results();

    auto* header = new QLabel("RESULTS");
    header->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                              .arg(module_.color.name())
                              .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(header);

    // Key-value table for scalar fields
    QStringList keys;
    for (auto it = data.begin(); it != data.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            keys.append(it.key());

    if (!keys.isEmpty()) {
        auto* table = new QTableWidget(keys.size(), 2);
        table->setHorizontalHeaderLabels({"Metric", "Value"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        table->setMaximumHeight(qMin(keys.size() * 30 + 30, 400));
        table->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                     "font-family:%4; font-size:%5px; border:1px solid %3; }"
                                     "QTableWidget::item { padding:4px 8px; }"
                                     "QHeaderView::section { background:%6; color:%7; font-weight:700;"
                                     "padding:4px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                     "QTableWidget::item:alternate { background:%8; }")
                                 .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::SMALL)
                                 .arg(ui::colors::BG_RAISED)
                                 .arg(ui::colors::TEXT_SECONDARY)
                                 .arg(ui::colors::ROW_ALT));

        for (int r = 0; r < keys.size(); ++r) {
            auto label = keys[r];
            label.replace('_', ' ');
            table->setItem(r, 0, new QTableWidgetItem(label.toUpper()));
            auto* vi = new QTableWidgetItem(format_val(data[keys[r]]));
            vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, 1, vi);
        }
        table->resizeColumnsToContents();
        results_layout_->addWidget(table);
    }

    // Raw JSON
    auto* raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setMaximumHeight(200);
    raw->setPlainText(QJsonDocument(data).toJson(QJsonDocument::Indented));
    raw->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:8px; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL));
    results_layout_->addWidget(raw);

    status_label_->setText("Done");
}

// ── Signal handlers ──────────────────────────────────────────────────────────

void QuantModulePanel::on_result(const QString& module_id, const QString& command, const QJsonObject& data) {
    if (module_id == module_.id)
        display_result(data);
}

void QuantModulePanel::on_error(const QString& module_id, const QString& message) {
    if (module_id == module_.id)
        display_error(message);
}

} // namespace fincept::screens
