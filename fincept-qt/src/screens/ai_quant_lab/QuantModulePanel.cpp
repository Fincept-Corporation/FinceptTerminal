// src/screens/ai_quant_lab/QuantModulePanel.cpp
#include "screens/ai_quant_lab/QuantModulePanel.h"

#include "core/logging/Logger.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>

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

// ── LLM picker helpers ────────────────────────────────────────────────────────

QWidget* QuantModulePanel::build_llm_picker(QWidget* parent, QComboBox** out_combo) {
    auto* combo = new QComboBox(parent);
    combo->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                 "font-family:%4; font-size:%5px; padding:4px 8px; }"
                                 "QComboBox::drop-down { border:none; }"
                                 "QComboBox QAbstractItemView { background:%1; color:%2; }")
                             .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::fonts::SMALL));

    // Populate from saved profiles
    auto profiles_result = LlmProfileRepository::instance().list_profiles();
    if (profiles_result.is_ok()) {
        for (const auto& p : profiles_result.value()) {
            combo->addItem(
                QString("%1  [%2 / %3]").arg(p.name, p.provider, p.model_id),
                p.id);
        }
    }
    if (combo->count() == 0)
        combo->addItem("No LLM profiles — configure in Settings → LLM Config", QString());

    // Pre-select the global default if present
    auto resolved = LlmProfileRepository::instance().resolve_for_context("ai_quant_lab");
    if (!resolved.profile_id.isEmpty()) {
        int idx = combo->findData(resolved.profile_id);
        if (idx >= 0)
            combo->setCurrentIndex(idx);
    }

    if (out_combo) *out_combo = combo;
    return build_input_row("LLM Profile", combo, parent);
}

QJsonObject QuantModulePanel::llm_config_from_combo(QComboBox* combo) const {
    if (!combo) return {};
    QString profile_id = combo->currentData().toString();
    if (profile_id.isEmpty()) return {};

    auto resolved = LlmProfileRepository::instance().resolve_for_context("ai_quant_lab", profile_id);
    // resolve_for_context uses context_id as a profile_id hint — use get_profile directly
    auto result = LlmProfileRepository::instance().get_profile(profile_id);
    if (!result.is_ok()) return {};

    const auto& p = result.value();
    QJsonObject cfg;
    cfg["llm_provider"] = p.provider;
    cfg["llm_api_key"]  = p.api_key;
    cfg["llm_model"]    = p.model_id;
    cfg["llm_base_url"] = p.base_url;
    return cfg;
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
    else if (module_.id == "deep_agent")
        content = build_deep_agent_panel();
    else if (module_.id == "factor_discovery")
        content = build_factor_discovery_panel();
    else if (module_.id == "model_library")
        content = build_model_library_panel();
    else if (module_.id == "live_signals")
        content = build_live_signals_panel();
    else if (module_.id == "online_learning")
        content = build_online_learning_panel();
    else if (module_.id == "meta_learning")
        content = build_meta_learning_panel();
    else if (module_.id == "hft")
        content = build_hft_panel();
    else if (module_.id == "rolling_retraining")
        content = build_rolling_retraining_panel();
    else if (module_.id == "feature_engineering")
        content = build_feature_engineering_panel();
    else if (module_.id == "portfolio_opt")
        content = build_portfolio_opt_panel();
    else if (module_.id == "factor_evaluation")
        content = build_factor_evaluation_panel();
    else if (module_.id == "strategy_builder")
        content = build_strategy_builder_panel();
    else if (module_.id == "data_processors")
        content = build_data_processors_panel();
    else if (module_.id == "quant_reporting")
        content = build_quant_reporting_panel();
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
// ═══════════════════════════════════════════════════════════════════════════════
// DEEP AGENT PANEL — tabbed: LangGraph Deep Analysis + RD-Agent Research
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_deep_agent_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Shared LLM profile picker (used by both Deep Analysis and RD-Agent) ──
    QComboBox* llm_combo = nullptr;
    auto* llm_bar = new QWidget(w);
    llm_bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* llm_bar_l = new QHBoxLayout(llm_bar);
    llm_bar_l->setContentsMargins(16, 8, 16, 8);
    llm_bar_l->setSpacing(0);
    llm_bar_l->addWidget(build_llm_picker(llm_bar, &llm_combo));
    vl->addWidget(llm_bar);

    auto tab_style = QString(
        "QTabWidget::pane { border:none; background:%1; }"
        "QTabBar::tab { background:%2; color:%3; padding:7px 16px;"
        "font-family:%4; font-size:%5px; border:1px solid %6; border-bottom:none; margin-right:2px; }"
        "QTabBar::tab:selected { background:%1; color:%7; font-weight:700;"
        "border-bottom:2px solid %7; }"
        "QTabBar::tab:hover { color:%3; background:%2; }")
        .arg(ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
        .arg(ui::colors::TEXT_SECONDARY)
        .arg(ui::fonts::DATA_FAMILY)
        .arg(ui::fonts::SMALL)
        .arg(ui::colors::BORDER_DIM)
        .arg(module_.color.name());

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_style);

    // ── Tab 1: LangGraph Deep Analysis ──────────────────────────────────────
    auto* da_w = new QWidget(tabs);
    auto* da_vl = new QVBoxLayout(da_w);
    da_vl->setContentsMargins(16, 16, 16, 16);
    da_vl->setSpacing(12);

    auto* desc = new QLabel(
        "Multi-agent financial analysis powered by LangGraph. Delegates to specialist subagents "
        "(research, data-analyst, trading, risk-analyzer, portfolio-optimizer, backtester, reporter).", da_w);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                            .arg(ui::colors::TEXT_SECONDARY)
                            .arg(ui::fonts::SMALL)
                            .arg(ui::fonts::DATA_FAMILY));
    da_vl->addWidget(desc);

    auto* task_edit = new QTextEdit(da_w);
    task_edit->setPlaceholderText(
        "Describe your analysis task...\n"
        "e.g. \"Conduct a full investment analysis of NVDA: research fundamentals, "
        "assess risks, and give a buy/sell/hold recommendation with price target\"");
    task_edit->setFixedHeight(90);
    task_edit->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                     "font-family:%4; font-size:%5px; padding:6px; }")
                                 .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::SMALL));
    da_vl->addWidget(build_input_row("Task", task_edit, da_w));

    auto* agent_type = new QComboBox(da_w);
    agent_type->addItems({"general", "research", "trading_strategy", "portfolio_management", "risk_assessment"});
    agent_type->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                      "font-family:%4; font-size:%5px; padding:4px 8px; }"
                                      "QComboBox::drop-down { border:none; }"
                                      "QComboBox QAbstractItemView { background:%1; color:%2; }")
                                  .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::fonts::SMALL));
    combo_inputs_["agent_type"] = agent_type;
    da_vl->addWidget(build_input_row("Agent Type", agent_type, da_w));

    auto* thread_id = new QLineEdit(da_w);
    thread_id->setPlaceholderText("Optional — leave blank to auto-generate");
    thread_id->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                                     "font-family:%4; font-size:%5px; padding:6px 8px; }")
                                 .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::SMALL));
    text_inputs_["thread_id"] = thread_id;
    da_vl->addWidget(build_input_row("Thread ID", thread_id, da_w));

    auto* da_run = make_run_button("RUN DEEP ANALYSIS", da_w);
    connect(da_run, &QPushButton::clicked, this, [this, task_edit, agent_type, thread_id, llm_combo]() {
        auto task = task_edit->toPlainText().trimmed();
        if (task.isEmpty()) { display_error("Please enter an analysis task."); return; }
        auto cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) { display_error("Select an LLM profile before running."); return; }
        status_label_->setText("Running...");
        clear_results();
        QJsonObject params;
        params["task"]       = task;
        params["agent_type"] = agent_type->currentText();
        params["config"]     = cfg;
        auto tid = thread_id->text().trimmed();
        if (!tid.isEmpty()) params["thread_id"] = tid;
        AIQuantLabService::instance().run_deep_agent(params);
    });
    da_vl->addWidget(da_run);

    agent_output_ = new QTextEdit(da_w);
    agent_output_->setReadOnly(true);
    agent_output_->setPlaceholderText("Analysis results will appear here...");
    agent_output_->setMinimumHeight(300);
    agent_output_->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                         "font-family:%4; font-size:%5px; padding:12px; }")
                                     .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::fonts::SMALL));
    da_vl->addWidget(agent_output_, 1);

    auto* rc = new QWidget(da_w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 0, 0, 0);
    results_layout_->setSpacing(0);
    da_vl->addWidget(rc);

    tabs->addTab(da_w, "Deep Analysis");

    // ── Tab 2: RD-Agent ─────────────────────────────────────────────────────
    tabs->addTab(build_rd_agent_tab(llm_combo), "RD-Agent");

    vl->addWidget(tabs, 1);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RD-AGENT TAB — Autonomous factor/model research (Microsoft RD-Agent)
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_rd_agent_tab(QComboBox* llm_combo) {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // shared styles
    auto input_style = QString("QLineEdit,QTextEdit,QSpinBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus,QTextEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(module_.color.name());

    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 8px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    auto sub_tab_style = QString(
        "QTabWidget::pane { border:none; background:%1; }"
        "QTabBar::tab { background:%2; color:%3; padding:5px 12px;"
        "font-family:%4; font-size:%5px; border:1px solid %6; border-bottom:none; margin-right:1px; }"
        "QTabBar::tab:selected { background:%1; color:%7; font-weight:700; }"
        "QTabBar { background:%2; }")
        .arg(ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
        .arg(ui::colors::TEXT_SECONDARY)
        .arg(ui::fonts::DATA_FAMILY)
        .arg(ui::fonts::TINY)
        .arg(ui::colors::BORDER_DIM)
        .arg(module_.color.name());

    auto* sub = new QTabWidget(w);
    sub->setStyleSheet(sub_tab_style);

    // ── Status bar ───────────────────────────────────────────────────────────
    auto* status_bar = new QWidget(w);
    status_bar->setFixedHeight(28);
    status_bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                  .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* sbl = new QHBoxLayout(status_bar);
    sbl->setContentsMargins(12, 0, 12, 0);
    sbl->setSpacing(8);
    auto* status_dot = new QLabel("●", status_bar);
    status_dot->setStyleSheet(QString("color:%1; font-size:8px;").arg(ui::colors::TEXT_TERTIARY));
    sbl->addWidget(status_dot);
    auto* status_txt = new QLabel("RD-Agent ready", status_bar);
    status_txt->setObjectName("rdStatusTxt");
    status_txt->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                  .arg(ui::colors::TEXT_TERTIARY)
                                  .arg(ui::fonts::TINY)
                                  .arg(ui::fonts::DATA_FAMILY));
    sbl->addWidget(status_txt, 1);
    auto* check_btn = new QPushButton("CHECK STATUS", status_bar);
    check_btn->setCursor(Qt::PointingHandCursor);
    check_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                     "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                                     "QPushButton:hover { background:%1; color:%4; }")
                                 .arg(module_.color.name())
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::TINY)
                                 .arg(ui::colors::BG_BASE));
    connect(check_btn, &QPushButton::clicked, this, [this, status_txt]() {
        status_txt->setText("Checking...");
        AIQuantLabService::instance().rd_agent_check_status();
    });
    sbl->addWidget(check_btn);

    auto* ui_btn = new QPushButton("OPEN LOG VIEWER", status_bar);
    ui_btn->setCursor(Qt::PointingHandCursor);
    ui_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                  "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                                  "QPushButton:hover { background:%1; color:%4; }")
                              .arg(ui::colors::TEXT_TERTIARY)
                              .arg(ui::fonts::DATA_FAMILY)
                              .arg(ui::fonts::TINY)
                              .arg(ui::colors::BG_BASE));
    connect(ui_btn, &QPushButton::clicked, this, [this, status_txt]() {
        status_txt->setText("Starting log viewer...");
        AIQuantLabService::instance().rd_agent_start_ui();
    });
    sbl->addWidget(ui_btn);

    auto* mcp_btn = new QPushButton("MCP TOOLS", status_bar);
    mcp_btn->setCursor(Qt::PointingHandCursor);
    mcp_btn->setCheckable(true);
    mcp_btn->setToolTip("Start/stop the Fincept MCP tool server\n"
                        "Gives RD-Agent loops access to market data,\n"
                        "financial news and economics tools.");
    mcp_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                "QPushButton:checked { background:%4; color:%5; border-color:%4; }"
                "QPushButton:hover { background:%1; color:%5; }")
            .arg(ui::colors::TEXT_TERTIARY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::TINY)
            .arg(module_.color.name())
            .arg(ui::colors::BG_BASE));
    connect(mcp_btn, &QPushButton::toggled, this, [this, mcp_btn, status_txt](bool checked) {
        if (checked) {
            status_txt->setText("Starting MCP tool server...");
            AIQuantLabService::instance().rd_agent_start_mcp_server();
        } else {
            status_txt->setText("MCP tool server stopped");
            mcp_btn->setText("MCP TOOLS");
        }
    });
    sbl->addWidget(mcp_btn);
    vl->addWidget(status_bar);

    // ── Sub-tab 1: Factor Mining ─────────────────────────────────────────────
    auto* fm_w = new QWidget(sub);
    auto* fm_vl = new QVBoxLayout(fm_w);
    fm_vl->setContentsMargins(14, 14, 14, 14);
    fm_vl->setSpacing(10);

    auto* fm_desc = new QLabel(
        "Autonomous alpha factor discovery via FactorRDLoop. The agent proposes, codes, runs "
        "and evaluates factors iteratively until the target IC is reached.", fm_w);
    fm_desc->setWordWrap(true);
    fm_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY)
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    fm_vl->addWidget(fm_desc);

    auto* fm_task = new QTextEdit(fm_w);
    fm_task->setPlaceholderText("Describe the factor hypothesis...\ne.g. \"Discover momentum-based alpha factors for US equities\"");
    fm_task->setFixedHeight(70);
    fm_task->setStyleSheet(input_style);
    fm_vl->addWidget(build_input_row("Task Description", fm_task, fm_w));

    auto* fm_market = new QComboBox(fm_w);
    fm_market->addItems({"US", "CN", "EU", "IN", "JP", "HK", "GLOBAL"});
    fm_market->setStyleSheet(combo_style);
    fm_vl->addWidget(build_input_row("Target Market", fm_market, fm_w));

    auto* fm_iter = new QSpinBox(fm_w);
    fm_iter->setRange(1, 100);
    fm_iter->setValue(10);
    fm_iter->setStyleSheet(input_style);
    fm_vl->addWidget(build_input_row("Max Iterations", fm_iter, fm_w));

    auto* fm_ic = make_double_spin(0.01, 0.50, 0.05, 3, "", fm_w);
    fm_vl->addWidget(build_input_row("Target IC", fm_ic, fm_w));

    auto* fm_run = make_run_button("START FACTOR MINING", fm_w);
    connect(fm_run, &QPushButton::clicked, this, [this, fm_task, fm_market, fm_iter, fm_ic, status_txt, llm_combo]() {
        auto desc_text = fm_task->toPlainText().trimmed();
        if (desc_text.isEmpty()) { display_error("Enter a task description."); return; }
        auto cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) { display_error("Select an LLM profile before running."); return; }
        status_label_->setText("Starting...");
        status_txt->setText("Factor mining started...");
        QJsonObject params;
        params["task_description"] = desc_text;
        params["target_market"]    = fm_market->currentText();
        params["max_iterations"]   = fm_iter->value();
        params["target_ic"]        = fm_ic->value();
        params["config"]           = cfg;
        AIQuantLabService::instance().rd_agent_start_factor_mining(params);
    });
    fm_vl->addWidget(fm_run);
    fm_vl->addStretch();
    sub->addTab(fm_w, "Factor Mining");

    // ── Sub-tab 2: Model Optimization ────────────────────────────────────────
    auto* mo_w = new QWidget(sub);
    auto* mo_vl = new QVBoxLayout(mo_w);
    mo_vl->setContentsMargins(14, 14, 14, 14);
    mo_vl->setSpacing(10);

    auto* mo_desc = new QLabel(
        "ML model hyperparameter optimization via ModelRDLoop. Supports LightGBM, XGBoost, "
        "LSTM, GRU, Transformer and TCN. Optimizes for Sharpe, IC, max drawdown, or win rate.", mo_w);
    mo_desc->setWordWrap(true);
    mo_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY)
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    mo_vl->addWidget(mo_desc);

    auto* mo_model = new QComboBox(mo_w);
    mo_model->addItems({"lightgbm", "xgboost", "lstm", "gru", "transformer", "tcn"});
    mo_model->setStyleSheet(combo_style);
    mo_vl->addWidget(build_input_row("Model Type", mo_model, mo_w));

    auto* mo_target = new QComboBox(mo_w);
    mo_target->addItems({"sharpe", "ic", "max_drawdown", "win_rate", "calmar_ratio"});
    mo_target->setStyleSheet(combo_style);
    mo_vl->addWidget(build_input_row("Optimize For", mo_target, mo_w));

    auto* mo_iter = new QSpinBox(mo_w);
    mo_iter->setRange(1, 100);
    mo_iter->setValue(10);
    mo_iter->setStyleSheet(input_style);
    mo_vl->addWidget(build_input_row("Max Iterations", mo_iter, mo_w));

    auto* mo_run = make_run_button("START MODEL OPTIMIZATION", mo_w);
    connect(mo_run, &QPushButton::clicked, this, [this, mo_model, mo_target, mo_iter, status_txt, llm_combo]() {
        auto cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) { display_error("Select an LLM profile before running."); return; }
        status_label_->setText("Starting...");
        status_txt->setText("Model optimization started...");
        QJsonObject params;
        params["model_type"]          = mo_model->currentText();
        params["optimization_target"] = mo_target->currentText();
        params["max_iterations"]      = mo_iter->value();
        params["config"]              = cfg;
        AIQuantLabService::instance().rd_agent_start_model_optimization(params);
    });
    mo_vl->addWidget(mo_run);
    mo_vl->addStretch();
    sub->addTab(mo_w, "Model Optimization");

    // ── Sub-tab 3: Quant Research ─────────────────────────────────────────────
    auto* qr_w = new QWidget(sub);
    auto* qr_vl = new QVBoxLayout(qr_w);
    qr_vl->setContentsMargins(14, 14, 14, 14);
    qr_vl->setSpacing(10);

    auto* qr_desc = new QLabel(
        "Combined factor discovery + model optimization via QuantRDLoop. Runs the full "
        "research pipeline end-to-end: propose factors, code them, backtest, refine.", qr_w);
    qr_desc->setWordWrap(true);
    qr_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY)
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    qr_vl->addWidget(qr_desc);

    auto* qr_goal = new QTextEdit(qr_w);
    qr_goal->setPlaceholderText("Research goal...\ne.g. \"Build a quantitative equity strategy for mid-cap US stocks\"");
    qr_goal->setFixedHeight(70);
    qr_goal->setStyleSheet(input_style);
    qr_vl->addWidget(build_input_row("Research Goal", qr_goal, qr_w));

    auto* qr_market = new QComboBox(qr_w);
    qr_market->addItems({"US", "CN", "EU", "IN", "JP", "HK", "GLOBAL"});
    qr_market->setStyleSheet(combo_style);
    qr_vl->addWidget(build_input_row("Target Market", qr_market, qr_w));

    auto* qr_iter = new QSpinBox(qr_w);
    qr_iter->setRange(1, 100);
    qr_iter->setValue(15);
    qr_iter->setStyleSheet(input_style);
    qr_vl->addWidget(build_input_row("Max Iterations", qr_iter, qr_w));

    auto* qr_run = make_run_button("START QUANT RESEARCH", qr_w);
    connect(qr_run, &QPushButton::clicked, this, [this, qr_goal, qr_market, qr_iter, status_txt, llm_combo]() {
        auto goal = qr_goal->toPlainText().trimmed();
        if (goal.isEmpty()) { display_error("Enter a research goal."); return; }
        auto cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) { display_error("Select an LLM profile before running."); return; }
        status_label_->setText("Starting...");
        status_txt->setText("Quant research started...");
        QJsonObject params;
        params["research_goal"]  = goal;
        params["target_market"]  = qr_market->currentText();
        params["max_iterations"] = qr_iter->value();
        params["config"]         = cfg;
        AIQuantLabService::instance().rd_agent_start_quant_research(params);
    });
    qr_vl->addWidget(qr_run);
    qr_vl->addStretch();
    sub->addTab(qr_w, "Quant Research");

    // ── Sub-tab 4: Task Monitor ───────────────────────────────────────────────
    auto* tm_w = new QWidget(sub);
    auto* tm_vl = new QVBoxLayout(tm_w);
    tm_vl->setContentsMargins(14, 14, 14, 14);
    tm_vl->setSpacing(8);

    // Toolbar: filter + buttons
    auto* tm_toolbar = new QWidget(tm_w);
    auto* tm_hl = new QHBoxLayout(tm_toolbar);
    tm_hl->setContentsMargins(0, 0, 0, 0);
    tm_hl->setSpacing(6);

    auto* filter_combo = new QComboBox(tm_toolbar);
    filter_combo->addItems({"All", "running", "completed", "stopped", "failed"});
    filter_combo->setFixedWidth(120);
    filter_combo->setStyleSheet(combo_style);
    tm_hl->addWidget(filter_combo);

    auto* refresh_btn = new QPushButton("REFRESH", tm_toolbar);
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       "font-family:%3; font-size:%4px; font-weight:700; padding:5px 12px; border-radius:2px; }"
                                       "QPushButton:hover { background:%5; }")
                                   .arg(module_.color.name())
                                   .arg(ui::colors::BG_BASE)
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::fonts::TINY)
                                   .arg(module_.color.lighter(120).name()));
    tm_hl->addWidget(refresh_btn);

    auto* task_id_input = new QLineEdit(tm_toolbar);
    task_id_input->setPlaceholderText("Task ID...");
    task_id_input->setStyleSheet(input_style);
    task_id_input->setFixedWidth(200);
    tm_hl->addWidget(task_id_input);

    auto* stop_btn = new QPushButton("STOP", tm_toolbar);
    stop_btn->setCursor(Qt::PointingHandCursor);
    stop_btn->setStyleSheet(QString("QPushButton { background:#c0392b; color:#fff; border:none;"
                                    "font-family:%1; font-size:%2px; font-weight:700; padding:5px 12px; border-radius:2px; }"
                                    "QPushButton:hover { background:#e74c3c; }")
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::TINY));
    tm_hl->addWidget(stop_btn);

    auto* resume_btn = new QPushButton("RESUME", tm_toolbar);
    resume_btn->setCursor(Qt::PointingHandCursor);
    resume_btn->setStyleSheet(QString("QPushButton { background:#27ae60; color:#fff; border:none;"
                                      "font-family:%1; font-size:%2px; font-weight:700; padding:5px 12px; border-radius:2px; }"
                                      "QPushButton:hover { background:#2ecc71; }")
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::fonts::TINY));
    tm_hl->addWidget(resume_btn);

    auto* factors_btn = new QPushButton("GET FACTORS", tm_toolbar);
    factors_btn->setCursor(Qt::PointingHandCursor);
    factors_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                       "font-family:%2; font-size:%3px; font-weight:700; padding:5px 10px; border-radius:2px; }"
                                       "QPushButton:hover { background:%1; color:%4; }")
                                   .arg(module_.color.name())
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::colors::BG_BASE));
    tm_hl->addWidget(factors_btn);
    tm_hl->addStretch();
    tm_vl->addWidget(tm_toolbar);

    // Task table
    rd_task_table_ = new QTableWidget(0, 6, tm_w);
    rd_task_table_->setHorizontalHeaderLabels({"Task ID", "Type", "Status", "Progress", "Best IC", "Elapsed"});
    rd_task_table_->horizontalHeader()->setStretchLastSection(true);
    rd_task_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rd_task_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rd_task_table_->setAlternatingRowColors(true);
    rd_task_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:1px solid %3; gridline-color:%3;"
                "font-family:%4; font-size:%5px; alternate-background-color:%6; }"
                "QHeaderView::section { background:%7; color:%8; border:none; border-bottom:1px solid %3;"
                "padding:4px 8px; font-weight:700; font-family:%4; font-size:%5px; }"
                "QTableWidget::item:selected { background:%9; color:%2; }")
            .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL)
            .arg(ui::colors::BG_RAISED)
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY)
            .arg(module_.color.darker(150).name()));
    tm_vl->addWidget(rd_task_table_, 1);

    // Output area for factor results
    rd_agent_output_ = new QTextEdit(tm_w);
    rd_agent_output_->setReadOnly(true);
    rd_agent_output_->setPlaceholderText("Select a task and click GET FACTORS / GET MODEL to view results...");
    rd_agent_output_->setFixedHeight(160);
    rd_agent_output_->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                            "font-family:%4; font-size:%5px; padding:8px; }")
                                        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
                                        .arg(ui::fonts::DATA_FAMILY)
                                        .arg(ui::fonts::SMALL));
    tm_vl->addWidget(rd_agent_output_);

    // Wire toolbar buttons
    connect(refresh_btn, &QPushButton::clicked, this, [this, filter_combo]() {
        auto filter = filter_combo->currentText();
        AIQuantLabService::instance().rd_agent_list_tasks(filter == "All" ? QString{} : filter);
    });

    connect(stop_btn, &QPushButton::clicked, this, [this, task_id_input]() {
        auto tid = task_id_input->text().trimmed();
        if (tid.isEmpty() && rd_task_table_->currentRow() >= 0)
            tid = rd_task_table_->item(rd_task_table_->currentRow(), 0)->text();
        if (tid.isEmpty()) { display_error("Select or enter a task ID."); return; }
        AIQuantLabService::instance().rd_agent_stop_task(tid);
    });

    connect(resume_btn, &QPushButton::clicked, this, [this, task_id_input]() {
        auto tid = task_id_input->text().trimmed();
        if (tid.isEmpty() && rd_task_table_->currentRow() >= 0)
            tid = rd_task_table_->item(rd_task_table_->currentRow(), 0)->text();
        if (tid.isEmpty()) { display_error("Select or enter a task ID."); return; }
        AIQuantLabService::instance().rd_agent_resume_task(tid);
    });

    connect(factors_btn, &QPushButton::clicked, this, [this, task_id_input]() {
        auto tid = task_id_input->text().trimmed();
        if (tid.isEmpty() && rd_task_table_->currentRow() >= 0)
            tid = rd_task_table_->item(rd_task_table_->currentRow(), 0)->text();
        if (tid.isEmpty()) { display_error("Select or enter a task ID."); return; }
        AIQuantLabService::instance().rd_agent_get_discovered_factors(tid);
    });

    // Populate task_id_input on row click
    connect(rd_task_table_, &QTableWidget::currentCellChanged, this,
            [task_id_input, this](int row, int, int, int) {
                if (row >= 0 && rd_task_table_->item(row, 0))
                    task_id_input->setText(rd_task_table_->item(row, 0)->text());
            });

    sub->addTab(tm_w, "Task Monitor");

    vl->addWidget(sub, 1);
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

    // Export button — saves raw JSON to File Manager
    auto* export_btn = new QPushButton("EXPORT RESULTS");
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setFixedHeight(28);
    export_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                "font-size:%3px; font-family:%4; padding:0 12px; }"
                "QPushButton:hover { color:%1; background:rgba(255,255,255,0.05); }")
            .arg(module_.color.name(), ui::colors::BORDER_DIM)
            .arg(ui::fonts::SMALL)
            .arg(ui::fonts::DATA_FAMILY));
    QString json_str = QJsonDocument(data).toJson(QJsonDocument::Indented);
    connect(export_btn, &QPushButton::clicked, this, [this, json_str]() {
        QString safe_name = module_.label;
        safe_name.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        QString stored_name = safe_name + "_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".json";
        QString dest = services::FileManagerService::instance().storage_dir() + "/" + stored_name;

        QFile f(dest);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(json_str.toUtf8());
            f.close();
            services::FileManagerService::instance().register_file(
                stored_name, module_.label + "_result.json",
                QFileInfo(dest).size(), "application/json", "ai_quant_lab");
            LOG_INFO("AIQuantLab", "Exported result: " + stored_name);
        }
    });
    results_layout_->addWidget(export_btn);

    status_label_->setText("Done");
}

// ── Signal handlers ──────────────────────────────────────────────────────────

void QuantModulePanel::on_result(const QString& module_id, const QString& command, const QJsonObject& data) {
    if (module_id != module_.id)
        return;

    if (module_id != "deep_agent") {
        display_result(data);
        return;
    }

    // ── LangGraph deep analysis result ───────────────────────────────────────
    if (command == "execute_task") {
        if (!data["success"].toBool()) {
            display_error(data["error"].toString("Unknown error"));
            return;
        }
        auto text = data["result"].toString();
        if (text.isEmpty())
            text = QJsonDocument(data).toJson(QJsonDocument::Indented);
        if (agent_output_) agent_output_->setPlainText(text);
        auto tid = data["thread_id"].toString();
        status_label_->setText(tid.isEmpty() ? "Done" : QString("Done — thread: %1").arg(tid));
        return;
    }

    // ── RD-Agent: check_status ────────────────────────────────────────────────
    if (command == "check_status") {
        auto ver = data["rdagent_version"].toString("?");
        auto avail = data["availability"].toObject();
        QStringList flags;
        for (auto it = avail.begin(); it != avail.end(); ++it)
            if (it.value().toBool()) flags << it.key();
        status_label_->setText(QString("rdagent %1 — %2").arg(ver, flags.join(", ")));
        return;
    }

    // ── RD-Agent: task started (factor_mining / model_optimization / quant_research) ──
    if (command == "start_factor_mining" || command == "start_model_optimization" ||
        command == "start_quant_research") {
        if (!data["success"].toBool()) {
            display_error(data["error"].toString("Unknown error"));
            return;
        }
        auto task_id = data["task_id"].toString();
        auto est     = data["estimated_time"].toString();
        status_label_->setText(QString("Task %1 started").arg(task_id));
        if (rd_agent_output_)
            rd_agent_output_->setPlainText(
                QString("Task started: %1\nEstimated time: %2\n\nUse Task Monitor → REFRESH to track progress.")
                    .arg(task_id, est));
        return;
    }

    // ── RD-Agent: list_tasks ──────────────────────────────────────────────────
    if (command == "list_tasks" && rd_task_table_) {
        auto tasks = data["tasks"].toArray();
        rd_task_table_->setRowCount(0);
        for (const auto& t : tasks) {
            auto obj = t.toObject();
            int row = rd_task_table_->rowCount();
            rd_task_table_->insertRow(row);
            rd_task_table_->setItem(row, 0, new QTableWidgetItem(obj["task_id"].toString()));
            rd_task_table_->setItem(row, 1, new QTableWidgetItem(obj["type"].toString()));
            auto status_str = obj["status"].toString();
            auto* status_item = new QTableWidgetItem(status_str);
            if (status_str == "running")
                status_item->setForeground(QColor("#f39c12"));
            else if (status_str == "completed")
                status_item->setForeground(QColor("#27ae60"));
            else if (status_str == "failed" || status_str == "stopped")
                status_item->setForeground(QColor("#c0392b"));
            rd_task_table_->setItem(row, 2, status_item);
            rd_task_table_->setItem(row, 3, new QTableWidgetItem(
                QString::number(obj["progress"].toDouble() * 100, 'f', 0) + "%"));
            auto ic = obj["best_ic"];
            rd_task_table_->setItem(row, 4, new QTableWidgetItem(
                ic.isNull() ? "-" : QString::number(ic.toDouble(), 'f', 4)));
            rd_task_table_->setItem(row, 5, new QTableWidgetItem(obj["elapsed_time"].toString("-")));
        }
        status_label_->setText(QString("%1 task(s)").arg(tasks.size()));
        return;
    }

    // ── RD-Agent: get_task_status ─────────────────────────────────────────────
    if (command == "get_task_status") {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        auto task_id  = data["task_id"].toString();
        auto progress = data["progress"].toDouble() * 100;
        auto step     = data["current_step"].toString();
        auto ic       = data["best_ic"];
        status_label_->setText(QString("Task %1 — %2% — %3")
                                   .arg(task_id)
                                   .arg(progress, 0, 'f', 0)
                                   .arg(step));
        if (rd_agent_output_) {
            rd_agent_output_->setPlainText(
                QString("Task ID:    %1\nStatus:     %2\nProgress:   %3%\nStep:       %4\nBest IC:    %5\nElapsed:    %6")
                    .arg(task_id, data["status"].toString())
                    .arg(progress, 0, 'f', 0)
                    .arg(step)
                    .arg(ic.isNull() ? "N/A" : QString::number(ic.toDouble(), 'f', 4))
                    .arg(data["elapsed_time"].toString("-")));
        }
        return;
    }

    // ── RD-Agent: get_discovered_factors ──────────────────────────────────────
    if (command == "get_discovered_factors" && rd_agent_output_) {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        auto factors = data["factors"].toArray();
        auto best_ic = data["best_ic"];
        QString out = QString("Discovered Factors: %1  |  Best IC: %2\n\n")
                          .arg(factors.size())
                          .arg(best_ic.isNull() ? "N/A" : QString::number(best_ic.toDouble(), 'f', 4));
        int idx = 1;
        for (const auto& f : factors) {
            auto obj = f.toObject();
            out += QString("[%1] %2\n  IC: %3  Sharpe: %4\n  %5\n\n")
                       .arg(idx++)
                       .arg(obj["name"].toString("Factor"))
                       .arg(obj["ic"].isNull() ? "N/A" : QString::number(obj["ic"].toDouble(), 'f', 4))
                       .arg(obj["sharpe"].isNull() ? "N/A" : QString::number(obj["sharpe"].toDouble(), 'f', 3))
                       .arg(obj["description"].toString());
        }
        rd_agent_output_->setPlainText(out);
        status_label_->setText(QString("Found %1 factor(s)").arg(factors.size()));
        return;
    }

    // ── RD-Agent: get_optimized_model ─────────────────────────────────────────
    if (command == "get_optimized_model" && rd_agent_output_) {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        auto models = data["models"].toArray();
        QString out = QString("Optimized Models: %1\n\n").arg(models.size());
        int idx = 1;
        for (const auto& m : models) {
            auto obj = m.toObject();
            out += QString("[%1] %2\n  Sharpe: %3  IC: %4\n")
                       .arg(idx++)
                       .arg(obj["model_id"].toString("Model"))
                       .arg(obj["sharpe"].isNull() ? "N/A" : QString::number(obj["sharpe"].toDouble(), 'f', 3))
                       .arg(obj["ic"].isNull() ? "N/A" : QString::number(obj["ic"].toDouble(), 'f', 4));
        }
        rd_agent_output_->setPlainText(out);
        return;
    }

    // ── RD-Agent: stop_task / resume_task ────────────────────────────────────
    if (command == "stop_task" || command == "resume_task") {
        status_label_->setText(data["message"].toString(command));
        if (rd_agent_output_)
            rd_agent_output_->setPlainText(data["message"].toString());
        return;
    }

    // ── RD-Agent: start_ui ───────────────────────────────────────────────────
    if (command == "start_ui") {
        auto url = data["url"].toString();
        if (!url.isEmpty()) {
            status_label_->setText(QString("Log viewer: %1").arg(url));
            QDesktopServices::openUrl(QUrl(url));
        }
        return;
    }

    // ── RD-Agent: start_mcp_server ────────────────────────────────────────────
    if (command == "start_mcp_server") {
        if (!data["success"].toBool()) {
            status_label_->setText("MCP server failed");
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(
                    QString("MCP server failed to start:\n%1\n\nInstall: %2")
                        .arg(data["error"].toString())
                        .arg(data["install"].toString("pip install 'mcp[cli]' 'pydantic-ai[mcp]' yfinance")));
            return;
        }
        auto url  = data["url"].toString();
        auto port = data["port"].toInt();
        auto tools = data["tools"].toArray();
        QStringList tool_names;
        for (const auto& t : tools) tool_names << t.toString();
        status_label_->setText(QString("MCP ready on port %1").arg(port));
        if (rd_agent_output_)
            rd_agent_output_->setPlainText(
                QString("MCP tool server running at %1\n\nAvailable tools:\n  %2\n\n"
                        "Enable 'enable_mcp: true' in factor/model/quant research params\n"
                        "to give RD-Agent loops access to these tools.")
                    .arg(url, tool_names.join("\n  ")));
        return;
    }

    // ── RD-Agent: mcp_status ─────────────────────────────────────────────────
    if (command == "mcp_status") {
        auto avail   = data["mcp_server_available"].toBool();
        auto pai_mcp = data["pydantic_ai_mcp"].toBool();
        auto ports   = data["running_ports"].toArray();
        QString info = QString("MCP server available: %1\npydantic-ai MCP: %2\nRunning ports: %3")
                           .arg(avail ? "yes" : "no")
                           .arg(pai_mcp ? "yes" : "no")
                           .arg(ports.isEmpty() ? "none" : [&]() {
                               QStringList ps;
                               for (const auto& p : ports) ps << QString::number(p.toInt());
                               return ps.join(", ");
                           }());
        status_label_->setText(avail ? "MCP available" : "MCP not installed");
        if (rd_agent_output_) rd_agent_output_->setPlainText(info);
        return;
    }

    // ── Factor Discovery ─────────────────────────────────────────────────────
    if (module_id == "factor_discovery") {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        clear_results();
        if (command == "get_factor_library") {
            auto factors = data["factors"].toArray();
            QString out = QString("Factor Library — %1 factors\n\n").arg(factors.size());
            for (const auto& f : factors) {
                auto obj = f.toObject();
                out += QString("• %1\n  Category: %2  |  Expression: %3\n")
                           .arg(obj["name"].toString(), obj["category"].toString("-"), obj["expression"].toString("-"));
            }
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; white-space:pre;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 factors").arg(factors.size()));
            return;
        }
        if (command == "get_data") {
            auto total = data["total_records"].toInt();
            auto range = data["date_range"].toObject();
            auto warning = data["warning"].toString();
            QString summary = QString("Data fetched — %1 records  |  %2 → %3")
                                  .arg(total).arg(range["start"].toString()).arg(range["end"].toString());
            if (!warning.isEmpty()) summary += QString("\n⚠ %1").arg(warning);
            auto* lbl = new QLabel(summary, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 records").arg(total));
            return;
        }
        if (command == "get_instruments") {
            auto instruments = data["instruments"].toArray();
            QStringList names;
            for (const auto& i : instruments) names << i.toString();
            auto* lbl = new QLabel(QString("Instruments (%1): %2").arg(names.size()).arg(names.join(", ")), this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 instruments").arg(names.size()));
            return;
        }
        display_result(data); return;
    }

    // ── Model Library ────────────────────────────────────────────────────────
    if (module_id == "model_library") {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        clear_results();
        if (command == "list_models") {
            auto models = data["models"].toArray();
            QString out = QString("Available Models (%1)\n\n").arg(models.size());
            for (const auto& m : models) {
                auto obj = m.toObject();
                out += QString("• %1  [%2]\n  %3\n")
                           .arg(obj["name"].toString(), obj["id"].toString(), obj["description"].toString("-"));
            }
            if (models.isEmpty()) out += "No models listed (check qlib availability)";
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 models").arg(models.size()));
            return;
        }
        if (command == "check_status") {
            auto qlib_ok = data["qlib_available"].toBool();
            auto models_avail = data["models_available"].toObject();
            QStringList available, unavailable;
            for (auto it = models_avail.begin(); it != models_avail.end(); ++it)
                (it.value().toBool() ? available : unavailable) << it.key();
            auto* lbl = new QLabel(
                QString("Qlib: %1\nAvailable: %2\nUnavailable: %3")
                    .arg(qlib_ok ? "✓ Ready" : "✗ Not installed")
                    .arg(available.join(", "))
                    .arg(unavailable.join(", ")), this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(qlib_ok ? "Qlib ready" : "Qlib unavailable");
            return;
        }
        if (command == "train_model") {
            auto model_id = data["model_id"].toString();
            auto metrics  = data["metrics"].toObject();
            QString out = QString("Model Trained: %1\n").arg(model_id);
            for (auto it = metrics.begin(); it != metrics.end(); ++it)
                out += QString("  %1: %2\n").arg(it.key()).arg(it.value().toDouble(), 0, 'f', 4);
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("Trained: %1").arg(model_id));
            return;
        }
        display_result(data); return;
    }

    // ── Live Signals ─────────────────────────────────────────────────────────
    if (module_id == "live_signals") {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        clear_results();
        if (command == "get_data") {
            auto total   = data["total_records"].toInt();
            auto range   = data["date_range"].toObject();
            auto warning = data["warning"].toString();
            auto instr   = data["instruments"].toArray();
            QStringList names;
            for (const auto& i : instr) names << i.toString();
            QString out = QString("Signal Data — %1 records\nInstruments: %2\nRange: %3 → %4")
                              .arg(total).arg(names.join(", "))
                              .arg(range["start"].toString()).arg(range["end"].toString());
            if (!warning.isEmpty()) out += QString("\n⚠ %1").arg(warning);
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 records").arg(total));
            return;
        }
        if (command == "get_factor_analysis") {
            auto factors = data["factors"].toArray();
            QString out = QString("Factor Analysis — %1 factors\n\n").arg(factors.size());
            for (const auto& f : factors) {
                auto obj = f.toObject();
                out += QString("• %1  IC: %2  Sharpe: %3\n")
                           .arg(obj["name"].toString("-"))
                           .arg(obj["ic"].isNull() ? "N/A" : QString::number(obj["ic"].toDouble(), 'f', 4))
                           .arg(obj["sharpe"].isNull() ? "N/A" : QString::number(obj["sharpe"].toDouble(), 'f', 3));
            }
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 factors").arg(factors.size()));
            return;
        }
        if (command == "get_feature_importance") {
            auto features = data["feature_importance"].toObject();
            QString out = "Feature Importance\n\n";
            QVector<QPair<QString, double>> sorted;
            for (auto it = features.begin(); it != features.end(); ++it)
                sorted.append({it.key(), it.value().toDouble()});
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
            for (const auto& p : sorted)
                out += QString("• %-20s %1\n").arg(p.second, 0, 'f', 4).arg(p.first, -20);
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText("Feature importance loaded");
            return;
        }
        display_result(data); return;
    }

    // ── Online Learning ──────────────────────────────────────────────────────
    if (module_id == "online_learning") {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        clear_results();
        if (command == "list_models") {
            auto models = data["models"].toArray();
            auto river  = data["river_available"].toBool();
            QString out = QString("Online Models (%1)  |  River: %2\n\n")
                              .arg(models.size()).arg(river ? "available" : "not installed");
            for (const auto& m : models) {
                auto obj = m.toObject();
                out += QString("• %1  [%2]\n  Trained: %3 samples  |  Updated: %4\n")
                           .arg(obj["model_id"].toString(), obj["type"].toString())
                           .arg(obj["samples_trained"].toInt())
                           .arg(obj["last_updated"].isNull() ? "never" : obj["last_updated"].toString());
            }
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 model(s)").arg(models.size()));
            return;
        }
        if (command == "create_model") {
            auto* lbl = new QLabel(
                QString("Created: %1  [%2]")
                    .arg(data["model_id"].toString(), data["model_type"].toString()), this);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText("Model created");
            return;
        }
        if (command == "train") {
            auto mae     = data["current_mae"].toDouble();
            auto samples = data["samples_trained"].toInt();
            auto drift   = data["drift_detected"].toBool();
            auto pred    = data["prediction"];
            QString out = QString("Trained — Samples: %1  |  MAE: %2\nDrift: %3")
                              .arg(samples).arg(mae, 0, 'f', 6)
                              .arg(drift ? "DETECTED" : "none");
            if (!pred.isNull())
                out += QString("\nPrediction: %1  →  Actual: %2  |  Error: %3")
                           .arg(pred.toDouble(), 0, 'f', 6)
                           .arg(data["actual"].toDouble(), 0, 'f', 6)
                           .arg(data["error"].toDouble(), 0, 'f', 6);
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(drift ? QString("#EF5350") : ui::colors::TEXT_PRIMARY)
                                   .arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(drift ? "⚠ Drift detected" : QString("MAE: %1").arg(mae, 0, 'f', 4));
            return;
        }
        if (command == "predict") {
            auto pred = data["prediction"];
            auto* lbl = new QLabel(
                QString("Prediction: %1").arg(pred.isNull() ? "N/A" : QString::number(pred.toDouble(), 'f', 6)), this);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; font-weight:700;")
                                   .arg(module_.color.name()).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText("Prediction ready");
            return;
        }
        if (command == "performance") {
            auto mae     = data["current_mae"].toDouble();
            auto samples = data["samples_trained"].toInt();
            auto drift   = data["drift_detected"].toBool();
            auto* lbl = new QLabel(
                QString("Model: %1  [%2]\nSamples: %3  |  MAE: %4\nDrift: %5\nLast updated: %6")
                    .arg(data["model_id"].toString(), data["model_type"].toString())
                    .arg(samples).arg(mae, 0, 'f', 6)
                    .arg(drift ? "DETECTED" : "none")
                    .arg(data["last_updated"].isNull() ? "never" : data["last_updated"].toString()), this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("MAE: %1").arg(mae, 0, 'f', 4));
            return;
        }
        display_result(data); return;
    }

    // ── Meta Learning ────────────────────────────────────────────────────────
    if (module_id == "meta_learning") {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        clear_results();
        if (command == "list_models") {
            auto models = data["models"].toArray();
            QString out = QString("Available Models (%1)\n\n").arg(models.size());
            for (const auto& m : models) {
                auto obj = m.toObject();
                out += QString("• %1  [%2 / %3]\n  %4\n")
                           .arg(obj["name"].toString(), obj["type"].toString(),
                                obj["library"].toString(), obj["description"].toString("-"));
            }
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 models").arg(models.size()));
            return;
        }
        if (command == "run_selection") {
            auto best    = data["best_model"].toString();
            auto trained = data["trained_count"].toInt();
            auto ranking = data["ranking"].toArray();
            QString out = QString("Best Model: %1  |  Trained: %2\n\nRanking:\n").arg(best).arg(trained);
            int rank = 1;
            for (const auto& r : ranking) {
                auto obj = r.toObject();
                auto metrics = obj["metrics"].toObject();
                out += QString("  %1. %2  R²: %3  RMSE: %4\n")
                           .arg(rank++).arg(obj["model_id"].toString())
                           .arg(metrics["r2_score"].toDouble(), 0, 'f', 4)
                           .arg(metrics["rmse"].toDouble(), 0, 'f', 4);
            }
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("Best: %1").arg(best));
            return;
        }
        if (command == "create_ensemble") {
            auto* lbl = new QLabel(
                QString("Ensemble: %1  [%2]\nModels: %3")
                    .arg(data["ensemble_id"].toString("-"), data["method"].toString("-"))
                    .arg(data["models"].toArray().size()), this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText("Ensemble created");
            return;
        }
        if (command == "tune_hyperparameters") {
            auto best_params = data["best_params"].toObject();
            auto best_score  = data["best_score"].toDouble();
            QString out = QString("Best score: %1\nBest params:\n").arg(best_score, 0, 'f', 4);
            for (auto it = best_params.begin(); it != best_params.end(); ++it)
                out += QString("  %1: %2\n").arg(it.key()).arg(it.value().toVariant().toString());
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("Best score: %1").arg(best_score, 0, 'f', 4));
            return;
        }
        if (command == "get_results") {
            auto results = data["results"].toObject();
            QString out = QString("All Results (%1)\n\n").arg(results.size());
            for (auto it = results.begin(); it != results.end(); ++it) {
                auto obj = it.value().toObject();
                auto metrics = obj["metrics"].toObject();
                out += QString("• %1\n  R²: %2  RMSE: %3\n")
                           .arg(it.key())
                           .arg(metrics["r2_score"].toDouble(), 0, 'f', 4)
                           .arg(metrics["rmse"].toDouble(), 0, 'f', 4);
            }
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 result(s)").arg(results.size()));
            return;
        }
        display_result(data); return;
    }

    // ── HFT ──────────────────────────────────────────────────────────────────
    if (module_id == "hft") {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        clear_results();
        if (command == "create_orderbook") {
            auto* lbl = new QLabel(
                QString("Order book created for %1  (depth: %2)")
                    .arg(data["symbol"].toString()).arg(data["depth"].toInt(10)), this);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText("Order book ready");
            return;
        }
        if (command == "snapshot") {
            auto bid = data["bids"].toArray();
            auto ask = data["asks"].toArray();
            auto mid = data["mid_price"];
            auto spread = data["spread"];
            QString out = QString("Symbol: %1\nMid: %2  |  Spread: %3\n\nTop Bids:\n")
                              .arg(data["symbol"].toString())
                              .arg(mid.isNull() ? "N/A" : QString::number(mid.toDouble(), 'f', 4))
                              .arg(spread.isNull() ? "N/A" : QString::number(spread.toDouble(), 'f', 4));
            for (int i = 0; i < qMin(5, (int)bid.size()); ++i) {
                auto b = bid[i].toObject();
                out += QString("  %1 x %2\n").arg(b["price"].toDouble(), 0, 'f', 4).arg(b["size"].toDouble(), 0, 'f', 0);
            }
            out += "\nTop Asks:\n";
            for (int i = 0; i < qMin(5, (int)ask.size()); ++i) {
                auto a = ask[i].toObject();
                out += QString("  %1 x %2\n").arg(a["price"].toDouble(), 0, 'f', 4).arg(a["size"].toDouble(), 0, 'f', 0);
            }
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("Spread: %1").arg(spread.isNull() ? "N/A" : QString::number(spread.toDouble(), 'f', 4)));
            return;
        }
        if (command == "market_making_quotes") {
            auto bid_p = data["bid_price"].toDouble();
            auto ask_p = data["ask_price"].toDouble();
            auto spread = data["spread"].toDouble();
            auto* lbl = new QLabel(
                QString("Market Making Quotes\nBid: %1  |  Ask: %2\nSpread: %3  |  Mid: %4")
                    .arg(bid_p, 0, 'f', 4).arg(ask_p, 0, 'f', 4)
                    .arg(spread, 0, 'f', 4).arg(data["mid_price"].toDouble(), 0, 'f', 4), this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("Bid: %1 / Ask: %2").arg(bid_p, 0, 'f', 4).arg(ask_p, 0, 'f', 4));
            return;
        }
        if (command == "detect_toxic") {
            auto is_toxic = data["is_toxic"].toBool();
            auto pin      = data["pin_score"].toDouble();
            auto* lbl = new QLabel(
                QString("Toxic Flow: %1\nPIN Score: %2\nClassification: %3")
                    .arg(is_toxic ? "DETECTED" : "none")
                    .arg(pin, 0, 'f', 4)
                    .arg(data["classification"].toString("-")), this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(is_toxic ? QString("#EF5350") : ui::colors::TEXT_PRIMARY)
                                   .arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(is_toxic ? "⚠ Toxic flow detected" : "Flow normal");
            return;
        }
        if (command == "execute_order") {
            auto fills = data["fills"].toArray();
            auto filled_qty  = data["filled_quantity"].toDouble();
            auto avg_price   = data["average_price"].toDouble();
            auto* lbl = new QLabel(
                QString("Order Executed\nFilled: %1  |  Avg Price: %2\nFills: %3")
                    .arg(filled_qty, 0, 'f', 0).arg(avg_price, 0, 'f', 4).arg(fills.size()), this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("Filled %1 @ %2").arg(filled_qty, 0, 'f', 0).arg(avg_price, 0, 'f', 4));
            return;
        }
        display_result(data); return;
    }

    // ── Rolling Retraining ───────────────────────────────────────────────────
    if (module_id == "rolling_retraining") {
        if (!data["success"].toBool()) { display_error(data["error"].toString()); return; }
        clear_results();
        if (command == "list") {
            auto schedules = data["schedules"].toObject();
            QString out = schedules.isEmpty()
                ? "No schedules configured."
                : QString("Schedules (%1)\n\n").arg(schedules.size());
            for (auto it = schedules.begin(); it != schedules.end(); ++it) {
                auto obj = it.value().toObject();
                out += QString("• %1\n  Freq: %2  |  Window: %3 days  |  Next: %4\n")
                           .arg(it.key(), obj["freq"].toString(), QString::number(obj["window"].toInt()))
                           .arg(obj["next_run"].toString("-"));
            }
            auto* lbl = new QLabel(out, this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText(QString("%1 schedule(s)").arg(schedules.size()));
            return;
        }
        if (command == "create") {
            auto sched = data["schedule"].toObject();
            auto* lbl = new QLabel(
                QString("Schedule created for %1\nFreq: %2  |  Window: %3 days\nNext run: %4")
                    .arg(data["model_id"].toString(), sched["freq"].toString())
                    .arg(sched["window"].toInt())
                    .arg(sched["next_run"].toString()), this);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText("Schedule created");
            return;
        }
        if (command == "retrain") {
            auto* lbl = new QLabel(
                QString("Retrain complete: %1\n%2").arg(data["model_id"].toString(), data["message"].toString()), this);
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(lbl);
            status_label_->setText("Retrain complete");
            return;
        }
        display_result(data); return;
    }

    display_result(data);
}

void QuantModulePanel::on_error(const QString& module_id, const QString& message) {
    if (module_id == module_.id)
        display_error(message);
}

// ═══════════════════════════════════════════════════════════════════════════════
// FEATURE ENGINEERING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_feature_engineering_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());

    // ── Indicators tab ──
    auto* ind = new QWidget;
    auto* ivl = new QVBoxLayout(ind);
    ivl->setContentsMargins(12, 12, 12, 12);
    ivl->setSpacing(8);

    auto* ind_data = new QLineEdit(ind);
    ind_data->setPlaceholderText("Price data (comma-separated, e.g. 100,102,101,105,108)");
    ind_data->setStyleSheet(input_style);
    text_inputs_["fe_data"] = ind_data;
    ivl->addWidget(build_input_row("Price Data", ind_data, ind));

    auto* ind_indicator = new QComboBox(ind);
    ind_indicator->addItems({"moving_average", "rsi", "macd", "bollinger_bands",
                              "momentum", "volatility", "returns", "log_returns", "drawdown"});
    ind_indicator->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                         "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                     .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                     .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    combo_inputs_["fe_indicator"] = ind_indicator;
    ivl->addWidget(build_input_row("Indicator", ind_indicator, ind));

    auto* ind_window = new QSpinBox(ind);
    ind_window->setRange(2, 200);
    ind_window->setValue(14);
    ind_window->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                      "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                  .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                  .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    int_inputs_["fe_window"] = ind_window;
    ivl->addWidget(build_input_row("Window", ind_window, ind));

    auto* ind_run = make_run_button("COMPUTE INDICATOR", ind);
    connect(ind_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Computing...");
        QJsonObject params;
        params["data"]      = text_inputs_["fe_data"]->text();
        params["indicator"] = combo_inputs_["fe_indicator"]->currentText();
        params["window"]    = int_inputs_["fe_window"]->value();
        AIQuantLabService::instance().feature_compute(params);
    });
    ivl->addWidget(ind_run);
    ivl->addStretch();
    tabs->addTab(ind, "Indicators");

    // ── Feature Selection tab ──
    auto* sel = new QWidget;
    auto* svl = new QVBoxLayout(sel);
    svl->setContentsMargins(12, 12, 12, 12);
    svl->setSpacing(8);

    auto* sel_features = new QLineEdit(sel);
    sel_features->setPlaceholderText("Feature values JSON: {\"rsi\":[...],\"macd\":[...]}");
    sel_features->setStyleSheet(input_style);
    text_inputs_["fe_sel_features"] = sel_features;
    svl->addWidget(build_input_row("Features (JSON)", sel_features, sel));

    auto* sel_returns = new QLineEdit(sel);
    sel_returns->setPlaceholderText("Target returns (comma-separated)");
    sel_returns->setStyleSheet(input_style);
    text_inputs_["fe_sel_returns"] = sel_returns;
    svl->addWidget(build_input_row("Returns", sel_returns, sel));

    auto* sel_topk = new QSpinBox(sel);
    sel_topk->setRange(1, 50);
    sel_topk->setValue(5);
    sel_topk->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    int_inputs_["fe_topk"] = sel_topk;
    svl->addWidget(build_input_row("Top-K Features", sel_topk, sel));

    auto* sel_run = make_run_button("SELECT FEATURES BY IC", sel);
    connect(sel_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Selecting...");
        QJsonObject params;
        auto feat_text = text_inputs_["fe_sel_features"]->text();
        auto doc = QJsonDocument::fromJson(feat_text.toUtf8());
        if (!doc.isNull()) params["features"] = doc.object();
        params["returns"] = text_inputs_["fe_sel_returns"]->text();
        params["top_k"]   = int_inputs_["fe_topk"]->value();
        AIQuantLabService::instance().feature_select_by_ic(params);
    });
    svl->addWidget(sel_run);
    svl->addStretch();
    tabs->addTab(sel, "Feature Selection");

    // ── Expression Engine tab ──
    auto* expr = new QWidget;
    auto* evl = new QVBoxLayout(expr);
    evl->setContentsMargins(12, 12, 12, 12);
    evl->setSpacing(8);

    auto* expr_data = new QLineEdit(expr);
    expr_data->setPlaceholderText("{\"close\":[100,102,...],\"volume\":[1000,1200,...]}");
    expr_data->setStyleSheet(input_style);
    text_inputs_["fe_expr_data"] = expr_data;
    evl->addWidget(build_input_row("OHLCV Data (JSON)", expr_data, expr));

    auto* expr_expr = new QLineEdit(expr);
    expr_expr->setPlaceholderText("e.g. Mean(close, 5) / Std(close, 20)");
    expr_expr->setStyleSheet(input_style);
    text_inputs_["fe_expression"] = expr_expr;
    evl->addWidget(build_input_row("Expression", expr_expr, expr));

    auto* expr_run = make_run_button("EVALUATE EXPRESSION", expr);
    connect(expr_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Evaluating...");
        QJsonObject params;
        auto data_text = text_inputs_["fe_expr_data"]->text();
        auto doc = QJsonDocument::fromJson(data_text.toUtf8());
        if (!doc.isNull()) params["data"] = doc.object();
        params["expression"] = text_inputs_["fe_expression"]->text();
        AIQuantLabService::instance().feature_evaluate_expression(params);
    });
    evl->addWidget(expr_run);
    evl->addStretch();
    tabs->addTab(expr, "Expression Engine");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PORTFOLIO OPTIMIZATION PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_portfolio_opt_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());

    // ── Shared covariance / returns helper ──
    auto make_cov_tab = [&](const QString& method_id, const QString& btn_label,
                             bool needs_returns) -> QWidget* {
        auto* t = new QWidget;
        auto* tvl = new QVBoxLayout(t);
        tvl->setContentsMargins(12, 12, 12, 12);
        tvl->setSpacing(8);

        auto* assets_in = new QLineEdit(t);
        assets_in->setPlaceholderText("Asset names (comma-separated, e.g. AAPL,GOOG,MSFT)");
        assets_in->setStyleSheet(input_style);
        text_inputs_[method_id + "_assets"] = assets_in;
        tvl->addWidget(build_input_row("Assets", assets_in, t));

        auto* cov_in = new QLineEdit(t);
        cov_in->setPlaceholderText("Covariance matrix JSON: [[0.04,0.01],[0.01,0.09]]");
        cov_in->setStyleSheet(input_style);
        text_inputs_[method_id + "_cov"] = cov_in;
        tvl->addWidget(build_input_row("Covariance Matrix", cov_in, t));

        if (needs_returns) {
            auto* ret_in = new QLineEdit(t);
            ret_in->setPlaceholderText("Expected returns (comma-separated, e.g. 0.10,0.15,0.12)");
            ret_in->setStyleSheet(input_style);
            text_inputs_[method_id + "_returns"] = ret_in;
            tvl->addWidget(build_input_row("Expected Returns", ret_in, t));
        }

        auto* rf_spin = make_double_spin(0, 20, 2.0, 2, "%", t);
        double_inputs_[method_id + "_rf"] = rf_spin;
        tvl->addWidget(build_input_row("Risk-Free Rate", rf_spin, t));

        auto* run = make_run_button(btn_label, t);
        connect(run, &QPushButton::clicked, this, [this, method_id, needs_returns]() {
            status_label_->setText("Optimizing...");
            QJsonObject params;
            auto assets_str = text_inputs_[method_id + "_assets"]->text().split(',');
            QJsonArray assets_arr;
            for (auto& a : assets_str) assets_arr.append(a.trimmed());
            params["assets"] = assets_arr;
            auto cov_doc = QJsonDocument::fromJson(text_inputs_[method_id + "_cov"]->text().toUtf8());
            if (!cov_doc.isNull()) params["cov_matrix"] = cov_doc.array();
            if (needs_returns) {
                QJsonArray ret_arr;
                for (auto& r : text_inputs_[method_id + "_returns"]->text().split(','))
                    ret_arr.append(r.trimmed().toDouble());
                params["expected_returns"] = ret_arr;
            }
            params["risk_free_rate"] = double_inputs_[method_id + "_rf"]->value() / 100.0;
            AIQuantLabService::instance().portopt_run(method_id, params);
        });
        tvl->addWidget(run);
        tvl->addStretch();
        return t;
    };

    tabs->addTab(make_cov_tab("hierarchical_risk_parity", "RUN HRP", false),        "HRP");
    tabs->addTab(make_cov_tab("minimum_variance",         "MIN VARIANCE",    false), "Min Variance");
    tabs->addTab(make_cov_tab("maximum_sharpe",           "MAX SHARPE",      true),  "Max Sharpe");
    tabs->addTab(make_cov_tab("efficient_frontier",       "EFFICIENT FRONTIER", true), "Eff. Frontier");

    // ── Black-Litterman ──
    auto* bl = new QWidget;
    auto* blvl = new QVBoxLayout(bl);
    blvl->setContentsMargins(12, 12, 12, 12);
    blvl->setSpacing(8);
    auto* bl_assets = new QLineEdit(bl);
    bl_assets->setPlaceholderText("Asset names (comma-separated)");
    bl_assets->setStyleSheet(input_style);
    text_inputs_["bl_assets"] = bl_assets;
    blvl->addWidget(build_input_row("Assets", bl_assets, bl));
    auto* bl_caps = new QLineEdit(bl);
    bl_caps->setPlaceholderText("Market caps (comma-separated, e.g. 2000,1500,800)");
    bl_caps->setStyleSheet(input_style);
    text_inputs_["bl_caps"] = bl_caps;
    blvl->addWidget(build_input_row("Market Caps ($B)", bl_caps, bl));
    auto* bl_cov = new QLineEdit(bl);
    bl_cov->setPlaceholderText("Covariance matrix JSON: [[0.04,0.01],[0.01,0.09]]");
    bl_cov->setStyleSheet(input_style);
    text_inputs_["bl_cov"] = bl_cov;
    blvl->addWidget(build_input_row("Covariance Matrix", bl_cov, bl));
    auto* bl_views = new QLineEdit(bl);
    bl_views->setPlaceholderText("Views (comma-separated, e.g. 0.05,0.10)");
    bl_views->setStyleSheet(input_style);
    text_inputs_["bl_views"] = bl_views;
    blvl->addWidget(build_input_row("Views", bl_views, bl));
    auto* bl_conf = new QLineEdit(bl);
    bl_conf->setPlaceholderText("View confidences (e.g. 0.8,0.6)");
    bl_conf->setStyleSheet(input_style);
    text_inputs_["bl_conf"] = bl_conf;
    blvl->addWidget(build_input_row("View Confidences", bl_conf, bl));
    auto* bl_run = make_run_button("RUN BLACK-LITTERMAN", bl);
    connect(bl_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running BL...");
        QJsonObject params;
        QJsonArray caps, views, confs, assets;
        for (auto& v : text_inputs_["bl_caps"]->text().split(','))   caps.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["bl_views"]->text().split(','))  views.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["bl_conf"]->text().split(','))   confs.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["bl_assets"]->text().split(',')) assets.append(v.trimmed());
        params["market_caps"]      = caps;
        params["views"]            = views;
        params["view_confidences"] = confs;
        params["assets"]           = assets;
        auto cov_doc = QJsonDocument::fromJson(text_inputs_["bl_cov"]->text().toUtf8());
        if (!cov_doc.isNull()) params["cov_matrix"] = cov_doc.array();
        AIQuantLabService::instance().portopt_run("black_litterman", params);
    });
    blvl->addWidget(bl_run);
    blvl->addStretch();
    tabs->addTab(bl, "Black-Litterman");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FACTOR EVALUATION PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_factor_evaluation_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());

    // ── IC Metrics tab ──
    auto* ic = new QWidget;
    auto* icvl = new QVBoxLayout(ic);
    icvl->setContentsMargins(12, 12, 12, 12);
    icvl->setSpacing(8);
    auto* ic_preds = new QLineEdit(ic);
    ic_preds->setPlaceholderText("Predictions (comma-separated, e.g. 0.1,0.2,-0.1,0.3)");
    ic_preds->setStyleSheet(input_style);
    text_inputs_["ev_predictions"] = ic_preds;
    icvl->addWidget(build_input_row("Predictions", ic_preds, ic));
    auto* ic_rets = new QLineEdit(ic);
    ic_rets->setPlaceholderText("Actual returns (comma-separated)");
    ic_rets->setStyleSheet(input_style);
    text_inputs_["ev_returns"] = ic_rets;
    icvl->addWidget(build_input_row("Returns", ic_rets, ic));
    auto* ic_method = new QComboBox(ic);
    ic_method->addItems({"pearson", "spearman"});
    ic_method->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                      "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                 .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                 .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    combo_inputs_["ev_ic_method"] = ic_method;
    icvl->addWidget(build_input_row("Method", ic_method, ic));
    auto* ic_run = make_run_button("CALCULATE IC METRICS", ic);
    connect(ic_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        QJsonArray preds, rets;
        for (auto& v : text_inputs_["ev_predictions"]->text().split(',')) preds.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["ev_returns"]->text().split(','))     rets.append(v.trimmed().toDouble());
        params["predictions"] = preds;
        params["returns"]     = rets;
        params["method"]      = combo_inputs_["ev_ic_method"]->currentText();
        AIQuantLabService::instance().evaluation_ic(params);
    });
    icvl->addWidget(ic_run);
    icvl->addStretch();
    tabs->addTab(ic, "IC Metrics");

    // ── Full Report tab ──
    auto* rep = new QWidget;
    auto* repvl = new QVBoxLayout(rep);
    repvl->setContentsMargins(12, 12, 12, 12);
    repvl->setSpacing(8);
    auto* rep_name = new QLineEdit(rep);
    rep_name->setPlaceholderText("Factor name (e.g. momentum)");
    rep_name->setStyleSheet(input_style);
    text_inputs_["ev_factor_name"] = rep_name;
    repvl->addWidget(build_input_row("Factor Name", rep_name, rep));
    auto* rep_preds = new QLineEdit(rep);
    rep_preds->setPlaceholderText("Predictions (comma-separated)");
    rep_preds->setStyleSheet(input_style);
    text_inputs_["ev_rep_preds"] = rep_preds;
    repvl->addWidget(build_input_row("Predictions", rep_preds, rep));
    auto* rep_rets = new QLineEdit(rep);
    rep_rets->setPlaceholderText("Returns (comma-separated)");
    rep_rets->setStyleSheet(input_style);
    text_inputs_["ev_rep_returns"] = rep_rets;
    repvl->addWidget(build_input_row("Returns", rep_rets, rep));
    auto* rep_run = make_run_button("GENERATE EVALUATION REPORT", rep);
    connect(rep_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Generating...");
        QJsonObject params;
        QJsonArray preds, rets;
        for (auto& v : text_inputs_["ev_rep_preds"]->text().split(','))   preds.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["ev_rep_returns"]->text().split(',')) rets.append(v.trimmed().toDouble());
        params["factor_name"]  = text_inputs_["ev_factor_name"]->text();
        params["predictions"]  = preds;
        params["returns"]      = rets;
        AIQuantLabService::instance().evaluation_report(params);
    });
    repvl->addWidget(rep_run);
    repvl->addStretch();
    tabs->addTab(rep, "Full Report");

    // ── Risk Metrics tab ──
    auto* risk = new QWidget;
    auto* riskvl = new QVBoxLayout(risk);
    riskvl->setContentsMargins(12, 12, 12, 12);
    riskvl->setSpacing(8);
    auto* risk_rets = new QLineEdit(risk);
    risk_rets->setPlaceholderText("Daily returns (comma-separated)");
    risk_rets->setStyleSheet(input_style);
    text_inputs_["ev_risk_returns"] = risk_rets;
    riskvl->addWidget(build_input_row("Returns", risk_rets, risk));
    auto* risk_bench = new QLineEdit(risk);
    risk_bench->setPlaceholderText("Benchmark returns (optional, comma-separated)");
    risk_bench->setStyleSheet(input_style);
    text_inputs_["ev_risk_bench"] = risk_bench;
    riskvl->addWidget(build_input_row("Benchmark (opt.)", risk_bench, risk));
    auto* risk_conf = make_double_spin(0.8, 0.999, 0.95, 3, "", risk);
    double_inputs_["ev_risk_conf"] = risk_conf;
    riskvl->addWidget(build_input_row("Confidence Level", risk_conf, risk));
    auto* risk_run = make_run_button("CALCULATE RISK METRICS", risk);
    connect(risk_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        QJsonArray rets;
        for (auto& v : text_inputs_["ev_risk_returns"]->text().split(',')) rets.append(v.trimmed().toDouble());
        params["returns"]          = rets;
        params["confidence_level"] = double_inputs_["ev_risk_conf"]->value();
        auto bench_text = text_inputs_["ev_risk_bench"]->text().trimmed();
        if (!bench_text.isEmpty()) {
            QJsonArray bench;
            for (auto& v : bench_text.split(',')) bench.append(v.trimmed().toDouble());
            params["benchmark_returns"] = bench;
        }
        AIQuantLabService::instance().evaluation_risk_metrics(params);
    });
    riskvl->addWidget(risk_run);
    riskvl->addStretch();
    tabs->addTab(risk, "Risk Metrics");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// STRATEGY BUILDER PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_strategy_builder_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());

    // ── TopK-Dropout ──
    auto* topk = new QWidget;
    auto* tkvl = new QVBoxLayout(topk);
    tkvl->setContentsMargins(12, 12, 12, 12);
    tkvl->setSpacing(8);
    auto* tk_signal = new QLineEdit(topk);
    tk_signal->setPlaceholderText("Signal values (comma-separated)");
    tk_signal->setStyleSheet(input_style);
    text_inputs_["st_tk_signal"] = tk_signal;
    tkvl->addWidget(build_input_row("Signal", tk_signal, topk));
    auto* tk_topk = new QSpinBox(topk);
    tk_topk->setRange(1, 500); tk_topk->setValue(50);
    tk_topk->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                   "font-family:%4; font-size:%5px; padding:4px 6px; }")
                               .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                               .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    int_inputs_["st_topk"] = tk_topk;
    tkvl->addWidget(build_input_row("Top-K", tk_topk, topk));
    auto* tk_drop = new QSpinBox(topk);
    tk_drop->setRange(1, 100); tk_drop->setValue(5);
    tk_drop->setStyleSheet(tk_topk->styleSheet());
    int_inputs_["st_ndrop"] = tk_drop;
    tkvl->addWidget(build_input_row("N-Drop", tk_drop, topk));
    auto* tk_run = make_run_button("CREATE TOPK-DROPOUT STRATEGY", topk);
    connect(tk_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        QJsonArray signal;
        for (auto& v : text_inputs_["st_tk_signal"]->text().split(',')) signal.append(v.trimmed().toDouble());
        params["signal"] = signal;
        params["topk"]   = int_inputs_["st_topk"]->value();
        params["n_drop"] = int_inputs_["st_ndrop"]->value();
        AIQuantLabService::instance().strategy_create("create_topk_dropout", params);
    });
    tkvl->addWidget(tk_run);
    tkvl->addStretch();
    tabs->addTab(topk, "TopK-Dropout");

    // ── Risk Parity ──
    auto* rp = new QWidget;
    auto* rpvl = new QVBoxLayout(rp);
    rpvl->setContentsMargins(12, 12, 12, 12);
    rpvl->setSpacing(8);
    auto* rp_returns = new QLineEdit(rp);
    rp_returns->setPlaceholderText("Asset returns matrix JSON: [[0.01,-0.02,...],[...]]");
    rp_returns->setStyleSheet(input_style);
    text_inputs_["st_rp_returns"] = rp_returns;
    rpvl->addWidget(build_input_row("Returns Matrix", rp_returns, rp));
    auto* rp_target = make_double_spin(0.01, 1.0, 0.10, 2, "", rp);
    double_inputs_["st_rp_target"] = rp_target;
    rpvl->addWidget(build_input_row("Target Risk", rp_target, rp));
    auto* rp_freq = new QComboBox(rp);
    rp_freq->addItems({"monthly", "weekly", "daily", "quarterly"});
    rp_freq->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:4px 6px; }")
                               .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                               .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    combo_inputs_["st_rp_freq"] = rp_freq;
    rpvl->addWidget(build_input_row("Rebalance", rp_freq, rp));
    auto* rp_run = make_run_button("CREATE RISK PARITY STRATEGY", rp);
    connect(rp_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        auto ret_doc = QJsonDocument::fromJson(text_inputs_["st_rp_returns"]->text().toUtf8());
        if (!ret_doc.isNull()) params["returns"] = ret_doc.array();
        params["target_risk"]          = double_inputs_["st_rp_target"]->value();
        params["rebalance_frequency"]  = combo_inputs_["st_rp_freq"]->currentText();
        AIQuantLabService::instance().strategy_create("create_risk_parity", params);
    });
    rpvl->addWidget(rp_run);
    rpvl->addStretch();
    tabs->addTab(rp, "Risk Parity");

    // ── Portfolio Metrics ──
    auto* pm = new QWidget;
    auto* pmvl = new QVBoxLayout(pm);
    pmvl->setContentsMargins(12, 12, 12, 12);
    pmvl->setSpacing(8);
    auto* pm_rets = new QLineEdit(pm);
    pm_rets->setPlaceholderText("Portfolio returns (comma-separated)");
    pm_rets->setStyleSheet(input_style);
    text_inputs_["st_pm_returns"] = pm_rets;
    pmvl->addWidget(build_input_row("Returns", pm_rets, pm));
    auto* pm_bench = new QLineEdit(pm);
    pm_bench->setPlaceholderText("Benchmark returns (optional)");
    pm_bench->setStyleSheet(input_style);
    text_inputs_["st_pm_bench"] = pm_bench;
    pmvl->addWidget(build_input_row("Benchmark (opt.)", pm_bench, pm));
    auto* pm_rf = make_double_spin(0, 20, 2.0, 2, "%", pm);
    double_inputs_["st_pm_rf"] = pm_rf;
    pmvl->addWidget(build_input_row("Risk-Free Rate", pm_rf, pm));
    auto* pm_run = make_run_button("CALCULATE PORTFOLIO METRICS", pm);
    connect(pm_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        QJsonArray rets;
        for (auto& v : text_inputs_["st_pm_returns"]->text().split(',')) rets.append(v.trimmed().toDouble());
        params["returns"]       = rets;
        params["risk_free_rate"] = double_inputs_["st_pm_rf"]->value() / 100.0;
        auto bench_text = text_inputs_["st_pm_bench"]->text().trimmed();
        if (!bench_text.isEmpty()) {
            QJsonArray bench;
            for (auto& v : bench_text.split(',')) bench.append(v.trimmed().toDouble());
            params["benchmark_returns"] = bench;
        }
        AIQuantLabService::instance().strategy_portfolio_metrics(params);
    });
    pmvl->addWidget(pm_run);
    pmvl->addStretch();
    tabs->addTab(pm, "Portfolio Metrics");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// DATA PROCESSORS PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_data_processors_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    // ── List Processors ──
    auto* list_tab = new QWidget;
    auto* ltvl = new QVBoxLayout(list_tab);
    ltvl->setContentsMargins(12, 12, 12, 12);
    ltvl->setSpacing(8);
    auto* lt_info = new QLabel("Browse all available data normalizers and transformation processors.", list_tab);
    lt_info->setWordWrap(true);
    lt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
    ltvl->addWidget(lt_info);
    auto* lt_run = make_run_button("LIST ALL PROCESSORS", list_tab);
    connect(lt_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().dataproc_list_processors();
    });
    ltvl->addWidget(lt_run);
    ltvl->addStretch();
    tabs->addTab(list_tab, "Browse");

    // ── Create Pipeline ──
    auto* pipe_tab = new QWidget;
    auto* ptvl = new QVBoxLayout(pipe_tab);
    ptvl->setContentsMargins(12, 12, 12, 12);
    ptvl->setSpacing(8);
    auto* pipe_id = new QLineEdit(pipe_tab);
    pipe_id->setPlaceholderText("Pipeline ID (e.g. my_pipeline)");
    pipe_id->setStyleSheet(input_style);
    text_inputs_["dp_pipeline_id"] = pipe_id;
    ptvl->addWidget(build_input_row("Pipeline ID", pipe_id, pipe_tab));
    auto* pipe_procs = new QLineEdit(pipe_tab);
    pipe_procs->setPlaceholderText(R"([{"type":"zscore"},{"type":"winsorize","lower":0.01,"upper":0.99}])");
    pipe_procs->setStyleSheet(input_style);
    text_inputs_["dp_processors"] = pipe_procs;
    ptvl->addWidget(build_input_row("Processors (JSON)", pipe_procs, pipe_tab));
    auto* pipe_run = make_run_button("CREATE PIPELINE", pipe_tab);
    connect(pipe_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        params["pipeline_id"] = text_inputs_["dp_pipeline_id"]->text();
        auto doc = QJsonDocument::fromJson(text_inputs_["dp_processors"]->text().toUtf8());
        if (!doc.isNull()) params["processors"] = doc.array();
        AIQuantLabService::instance().dataproc_create_pipeline(params);
    });
    ptvl->addWidget(pipe_run);
    ptvl->addStretch();
    tabs->addTab(pipe_tab, "Create Pipeline");

    // ── Process Data ──
    auto* proc_tab = new QWidget;
    auto* procvl = new QVBoxLayout(proc_tab);
    procvl->setContentsMargins(12, 12, 12, 12);
    procvl->setSpacing(8);
    auto* proc_pid = new QLineEdit(proc_tab);
    proc_pid->setPlaceholderText("Pipeline ID (must be created first)");
    proc_pid->setStyleSheet(input_style);
    text_inputs_["dp_proc_pid"] = proc_pid;
    procvl->addWidget(build_input_row("Pipeline ID", proc_pid, proc_tab));
    auto* proc_data = new QLineEdit(proc_tab);
    proc_data->setPlaceholderText(R"({"feature_close":[100,102,...],"feature_volume":[1000,1200,...]})");
    proc_data->setStyleSheet(input_style);
    text_inputs_["dp_proc_data"] = proc_data;
    procvl->addWidget(build_input_row("Data (JSON)", proc_data, proc_tab));
    auto* proc_run = make_run_button("PROCESS DATA", proc_tab);
    connect(proc_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Processing...");
        QJsonObject params;
        params["pipeline_id"] = text_inputs_["dp_proc_pid"]->text();
        auto doc = QJsonDocument::fromJson(text_inputs_["dp_proc_data"]->text().toUtf8());
        if (!doc.isNull()) params["data"] = doc.object();
        AIQuantLabService::instance().dataproc_process_data(params);
    });
    procvl->addWidget(proc_run);
    procvl->addStretch();
    tabs->addTab(proc_tab, "Process Data");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// QUANT REPORTING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_quant_reporting_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());

    // ── IC Analysis ──
    auto* ic_tab = new QWidget;
    auto* icvl = new QVBoxLayout(ic_tab);
    icvl->setContentsMargins(12, 12, 12, 12);
    icvl->setSpacing(8);
    auto* ic_preds = new QLineEdit(ic_tab);
    ic_preds->setPlaceholderText("Predictions (comma-separated)");
    ic_preds->setStyleSheet(input_style);
    text_inputs_["rp_ic_preds"] = ic_preds;
    icvl->addWidget(build_input_row("Predictions", ic_preds, ic_tab));
    auto* ic_rets = new QLineEdit(ic_tab);
    ic_rets->setPlaceholderText("Returns (comma-separated)");
    ic_rets->setStyleSheet(input_style);
    text_inputs_["rp_ic_rets"] = ic_rets;
    icvl->addWidget(build_input_row("Returns", ic_rets, ic_tab));
    auto* ic_method = new QComboBox(ic_tab);
    ic_method->addItems({"both", "pearson", "spearman"});
    ic_method->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                      "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                 .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                                 .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL));
    combo_inputs_["rp_ic_method"] = ic_method;
    icvl->addWidget(build_input_row("Method", ic_method, ic_tab));
    auto* ic_run = make_run_button("GENERATE IC ANALYSIS", ic_tab);
    connect(ic_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing...");
        QJsonObject params;
        QJsonArray preds, rets;
        for (auto& v : text_inputs_["rp_ic_preds"]->text().split(',')) preds.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["rp_ic_rets"]->text().split(','))  rets.append(v.trimmed().toDouble());
        params["predictions"] = preds;
        params["returns"]     = rets;
        params["method"]      = combo_inputs_["rp_ic_method"]->currentText();
        AIQuantLabService::instance().reporting_ic_analysis(params);
    });
    icvl->addWidget(ic_run);
    icvl->addStretch();
    tabs->addTab(ic_tab, "IC Analysis");

    // ── Model Performance ──
    auto* mp_tab = new QWidget;
    auto* mpvl = new QVBoxLayout(mp_tab);
    mpvl->setContentsMargins(12, 12, 12, 12);
    mpvl->setSpacing(8);
    auto* mp_preds = new QLineEdit(mp_tab);
    mp_preds->setPlaceholderText("Model predictions (comma-separated)");
    mp_preds->setStyleSheet(input_style);
    text_inputs_["rp_mp_preds"] = mp_preds;
    mpvl->addWidget(build_input_row("Predictions", mp_preds, mp_tab));
    auto* mp_rets = new QLineEdit(mp_tab);
    mp_rets->setPlaceholderText("Actual returns (comma-separated)");
    mp_rets->setStyleSheet(input_style);
    text_inputs_["rp_mp_rets"] = mp_rets;
    mpvl->addWidget(build_input_row("Returns", mp_rets, mp_tab));
    auto* mp_name = new QLineEdit(mp_tab);
    mp_name->setPlaceholderText("Model name (e.g. LightGBM)");
    mp_name->setStyleSheet(input_style);
    text_inputs_["rp_mp_name"] = mp_name;
    mpvl->addWidget(build_input_row("Model Name", mp_name, mp_tab));
    auto* mp_run = make_run_button("GENERATE MODEL PERFORMANCE REPORT", mp_tab);
    connect(mp_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Generating...");
        QJsonObject params;
        QJsonArray preds, rets;
        for (auto& v : text_inputs_["rp_mp_preds"]->text().split(',')) preds.append(v.trimmed().toDouble());
        for (auto& v : text_inputs_["rp_mp_rets"]->text().split(','))  rets.append(v.trimmed().toDouble());
        params["predictions"] = preds;
        params["returns"]     = rets;
        params["model_name"]  = text_inputs_["rp_mp_name"]->text().isEmpty()
                                   ? QString("Model") : text_inputs_["rp_mp_name"]->text();
        AIQuantLabService::instance().reporting_model_performance(params);
    });
    mpvl->addWidget(mp_run);
    mpvl->addStretch();
    tabs->addTab(mp_tab, "Model Performance");

    // ── Cumulative Returns ──
    auto* cr_tab = new QWidget;
    auto* crvl = new QVBoxLayout(cr_tab);
    crvl->setContentsMargins(12, 12, 12, 12);
    crvl->setSpacing(8);
    auto* cr_rets = new QLineEdit(cr_tab);
    cr_rets->setPlaceholderText("Portfolio returns (comma-separated)");
    cr_rets->setStyleSheet(input_style);
    text_inputs_["rp_cr_rets"] = cr_rets;
    crvl->addWidget(build_input_row("Returns", cr_rets, cr_tab));
    auto* cr_bench = new QLineEdit(cr_tab);
    cr_bench->setPlaceholderText("Benchmark returns (optional, comma-separated)");
    cr_bench->setStyleSheet(input_style);
    text_inputs_["rp_cr_bench"] = cr_bench;
    crvl->addWidget(build_input_row("Benchmark (opt.)", cr_bench, cr_tab));
    auto* cr_title = new QLineEdit(cr_tab);
    cr_title->setPlaceholderText("Chart title (e.g. Strategy vs Benchmark)");
    cr_title->setStyleSheet(input_style);
    text_inputs_["rp_cr_title"] = cr_title;
    crvl->addWidget(build_input_row("Title", cr_title, cr_tab));
    auto* cr_run = make_run_button("GENERATE CUMULATIVE RETURN CHART", cr_tab);
    connect(cr_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Generating...");
        QJsonObject params;
        QJsonArray rets;
        for (auto& v : text_inputs_["rp_cr_rets"]->text().split(',')) rets.append(v.trimmed().toDouble());
        params["returns"] = rets;
        auto bench_text = text_inputs_["rp_cr_bench"]->text().trimmed();
        if (!bench_text.isEmpty()) {
            QJsonArray bench;
            for (auto& v : bench_text.split(',')) bench.append(v.trimmed().toDouble());
            params["benchmark_returns"] = bench;
        }
        auto title = text_inputs_["rp_cr_title"]->text().trimmed();
        params["title"] = title.isEmpty() ? QString("Cumulative Returns") : title;
        AIQuantLabService::instance().reporting_cumulative_returns(params);
    });
    crvl->addWidget(cr_run);
    crvl->addStretch();
    tabs->addTab(cr_tab, "Cumulative Returns");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FACTOR DISCOVERY PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_factor_discovery_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    // ── Factor Library ──
    auto* lib_tab = new QWidget;
    auto* libvl = new QVBoxLayout(lib_tab);
    libvl->setContentsMargins(12, 12, 12, 12);
    libvl->setSpacing(8);
    auto* lib_info = new QLabel("Browse all built-in Qlib alpha factors and expressions.", lib_tab);
    lib_info->setWordWrap(true);
    lib_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
    libvl->addWidget(lib_info);
    auto* lib_run = make_run_button("BROWSE FACTOR LIBRARY", lib_tab);
    connect(lib_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().factor_get_library();
    });
    libvl->addWidget(lib_run);
    auto* inst_run = make_run_button("LIST INSTRUMENTS", lib_tab);
    connect(inst_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().factor_get_instruments();
    });
    libvl->addWidget(inst_run);
    libvl->addStretch();
    tabs->addTab(lib_tab, "Factor Library");

    // ── Fetch Data ──
    auto* data_tab = new QWidget;
    auto* datavl = new QVBoxLayout(data_tab);
    datavl->setContentsMargins(12, 12, 12, 12);
    datavl->setSpacing(8);
    auto* fd_instr = new QLineEdit(data_tab);
    fd_instr->setPlaceholderText("Instruments (comma-separated, e.g. aapl,msft)");
    fd_instr->setStyleSheet(input_style);
    text_inputs_["fd_instruments"] = fd_instr;
    datavl->addWidget(build_input_row("Instruments", fd_instr, data_tab));
    auto* fd_fields = new QLineEdit(data_tab);
    fd_fields->setPlaceholderText("Fields (comma-separated, e.g. $close,$volume,$open)");
    fd_fields->setStyleSheet(input_style);
    text_inputs_["fd_fields"] = fd_fields;
    datavl->addWidget(build_input_row("Fields", fd_fields, data_tab));
    auto* fd_start = new QLineEdit(data_tab);
    fd_start->setPlaceholderText("Start date (YYYY-MM-DD, e.g. 2019-01-01)");
    fd_start->setStyleSheet(input_style);
    text_inputs_["fd_start"] = fd_start;
    datavl->addWidget(build_input_row("Start Date", fd_start, data_tab));
    auto* fd_end = new QLineEdit(data_tab);
    fd_end->setPlaceholderText("End date (YYYY-MM-DD, e.g. 2020-11-10)");
    fd_end->setStyleSheet(input_style);
    text_inputs_["fd_end"] = fd_end;
    datavl->addWidget(build_input_row("End Date", fd_end, data_tab));
    auto* fd_run = make_run_button("FETCH DATA", data_tab);
    connect(fd_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching...");
        QJsonObject params;
        QJsonArray instr;
        for (auto& s : text_inputs_["fd_instruments"]->text().split(','))
            if (!s.trimmed().isEmpty()) instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        QJsonArray fields;
        for (auto& s : text_inputs_["fd_fields"]->text().split(','))
            if (!s.trimmed().isEmpty()) fields.append(s.trimmed());
        params["fields"] = fields;
        if (!text_inputs_["fd_start"]->text().isEmpty()) params["start_date"] = text_inputs_["fd_start"]->text().trimmed();
        if (!text_inputs_["fd_end"]->text().isEmpty())   params["end_date"]   = text_inputs_["fd_end"]->text().trimmed();
        AIQuantLabService::instance().factor_get_data(params);
    });
    datavl->addWidget(fd_run);
    datavl->addStretch();
    tabs->addTab(data_tab, "Fetch Data");

    // ── Calendar ──
    auto* cal_tab = new QWidget;
    auto* calvl = new QVBoxLayout(cal_tab);
    calvl->setContentsMargins(12, 12, 12, 12);
    calvl->setSpacing(8);
    auto* cal_start = new QLineEdit(cal_tab);
    cal_start->setPlaceholderText("Start date (YYYY-MM-DD)");
    cal_start->setStyleSheet(input_style);
    text_inputs_["fd_cal_start"] = cal_start;
    calvl->addWidget(build_input_row("Start Date", cal_start, cal_tab));
    auto* cal_end = new QLineEdit(cal_tab);
    cal_end->setPlaceholderText("End date (YYYY-MM-DD)");
    cal_end->setStyleSheet(input_style);
    text_inputs_["fd_cal_end"] = cal_end;
    calvl->addWidget(build_input_row("End Date", cal_end, cal_tab));
    auto* cal_run = make_run_button("GET TRADING CALENDAR", cal_tab);
    connect(cal_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        QJsonObject params;
        if (!text_inputs_["fd_cal_start"]->text().isEmpty()) params["start_date"] = text_inputs_["fd_cal_start"]->text().trimmed();
        if (!text_inputs_["fd_cal_end"]->text().isEmpty())   params["end_date"]   = text_inputs_["fd_cal_end"]->text().trimmed();
        AIQuantLabService::instance().factor_get_calendar(params);
    });
    calvl->addWidget(cal_run);
    calvl->addStretch();
    tabs->addTab(cal_tab, "Calendar");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MODEL LIBRARY PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_model_library_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());
    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:3px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    // ── Model Browser ──
    auto* browse_tab = new QWidget;
    auto* btvl = new QVBoxLayout(browse_tab);
    btvl->setContentsMargins(12, 12, 12, 12);
    btvl->setSpacing(8);
    auto* bt_info = new QLabel("List all available Qlib models (LightGBM, XGBoost, LSTM, Transformer, etc.).", browse_tab);
    bt_info->setWordWrap(true);
    bt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
    btvl->addWidget(bt_info);
    auto* bt_list = make_run_button("LIST ALL MODELS", browse_tab);
    connect(bt_list, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().model_list();
    });
    btvl->addWidget(bt_list);
    auto* bt_status = make_run_button("CHECK QLIB STATUS", browse_tab);
    connect(bt_status, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Checking...");
        AIQuantLabService::instance().model_check_status();
    });
    btvl->addWidget(bt_status);
    btvl->addStretch();
    tabs->addTab(browse_tab, "Browse");

    // ── Train Model ──
    auto* train_tab = new QWidget;
    auto* ttvl = new QVBoxLayout(train_tab);
    ttvl->setContentsMargins(12, 12, 12, 12);
    ttvl->setSpacing(8);
    auto* ml_type = new QComboBox(train_tab);
    ml_type->setStyleSheet(combo_style);
    ml_type->addItems({"lightgbm", "xgboost", "catboost", "linear", "lstm", "gru", "transformer"});
    combo_inputs_["ml_model_type"] = ml_type;
    ttvl->addWidget(build_input_row("Model Type", ml_type, train_tab));
    auto* ml_instr = new QLineEdit(train_tab);
    ml_instr->setPlaceholderText("Instruments (comma-separated, e.g. aapl,msft)");
    ml_instr->setStyleSheet(input_style);
    text_inputs_["ml_instruments"] = ml_instr;
    ttvl->addWidget(build_input_row("Instruments", ml_instr, train_tab));
    auto* ml_start = new QLineEdit(train_tab);
    ml_start->setPlaceholderText("Train start (YYYY-MM-DD)");
    ml_start->setStyleSheet(input_style);
    text_inputs_["ml_start"] = ml_start;
    ttvl->addWidget(build_input_row("Start Date", ml_start, train_tab));
    auto* ml_end = new QLineEdit(train_tab);
    ml_end->setPlaceholderText("Train end (YYYY-MM-DD)");
    ml_end->setStyleSheet(input_style);
    text_inputs_["ml_end"] = ml_end;
    ttvl->addWidget(build_input_row("End Date", ml_end, train_tab));
    auto* ml_run = make_run_button("TRAIN MODEL", train_tab);
    connect(ml_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Training...");
        QJsonObject params;
        params["model_type"] = combo_inputs_["ml_model_type"]->currentText();
        QJsonArray instr;
        for (auto& s : text_inputs_["ml_instruments"]->text().split(','))
            if (!s.trimmed().isEmpty()) instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        if (!text_inputs_["ml_start"]->text().isEmpty()) params["start_date"] = text_inputs_["ml_start"]->text().trimmed();
        if (!text_inputs_["ml_end"]->text().isEmpty())   params["end_date"]   = text_inputs_["ml_end"]->text().trimmed();
        AIQuantLabService::instance().model_train(params);
    });
    ttvl->addWidget(ml_run);
    ttvl->addStretch();
    tabs->addTab(train_tab, "Train Model");

    // ── Backtest ──
    auto* bt_tab = new QWidget;
    auto* btvl2 = new QVBoxLayout(bt_tab);
    btvl2->setContentsMargins(12, 12, 12, 12);
    btvl2->setSpacing(8);
    auto* bt_model = new QLineEdit(bt_tab);
    bt_model->setPlaceholderText("Model ID (from training output)");
    bt_model->setStyleSheet(input_style);
    text_inputs_["ml_bt_model"] = bt_model;
    btvl2->addWidget(build_input_row("Model ID", bt_model, bt_tab));
    auto* bt_instr = new QLineEdit(bt_tab);
    bt_instr->setPlaceholderText("Instruments (comma-separated)");
    bt_instr->setStyleSheet(input_style);
    text_inputs_["ml_bt_instr"] = bt_instr;
    btvl2->addWidget(build_input_row("Instruments", bt_instr, bt_tab));
    auto* bt_start2 = new QLineEdit(bt_tab);
    bt_start2->setPlaceholderText("Backtest start (YYYY-MM-DD)");
    bt_start2->setStyleSheet(input_style);
    text_inputs_["ml_bt_start"] = bt_start2;
    btvl2->addWidget(build_input_row("Start Date", bt_start2, bt_tab));
    auto* bt_end2 = new QLineEdit(bt_tab);
    bt_end2->setPlaceholderText("Backtest end (YYYY-MM-DD)");
    bt_end2->setStyleSheet(input_style);
    text_inputs_["ml_bt_end"] = bt_end2;
    btvl2->addWidget(build_input_row("End Date", bt_end2, bt_tab));
    auto* bt_run2 = make_run_button("RUN BACKTEST", bt_tab);
    connect(bt_run2, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running backtest...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ml_bt_model"]->text().trimmed();
        QJsonArray instr;
        for (auto& s : text_inputs_["ml_bt_instr"]->text().split(','))
            if (!s.trimmed().isEmpty()) instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        if (!text_inputs_["ml_bt_start"]->text().isEmpty()) params["start_date"] = text_inputs_["ml_bt_start"]->text().trimmed();
        if (!text_inputs_["ml_bt_end"]->text().isEmpty())   params["end_date"]   = text_inputs_["ml_bt_end"]->text().trimmed();
        AIQuantLabService::instance().model_backtest(params);
    });
    btvl2->addWidget(bt_run2);
    btvl2->addStretch();
    tabs->addTab(bt_tab, "Backtest");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// LIVE SIGNALS PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_live_signals_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    // ── Signal Data ──
    auto* sig_tab = new QWidget;
    auto* sigvl = new QVBoxLayout(sig_tab);
    sigvl->setContentsMargins(12, 12, 12, 12);
    sigvl->setSpacing(8);
    auto* sig_instr = new QLineEdit(sig_tab);
    sig_instr->setPlaceholderText("Instruments (comma-separated, e.g. aapl,msft,goog)");
    sig_instr->setStyleSheet(input_style);
    text_inputs_["ls_instruments"] = sig_instr;
    sigvl->addWidget(build_input_row("Instruments", sig_instr, sig_tab));
    auto* sig_fields = new QLineEdit(sig_tab);
    sig_fields->setPlaceholderText("Fields (e.g. $close,$open,$high,$low,$volume)");
    sig_fields->setStyleSheet(input_style);
    text_inputs_["ls_fields"] = sig_fields;
    sigvl->addWidget(build_input_row("Fields", sig_fields, sig_tab));
    auto* sig_start = new QLineEdit(sig_tab);
    sig_start->setPlaceholderText("Start date (YYYY-MM-DD)");
    sig_start->setStyleSheet(input_style);
    text_inputs_["ls_start"] = sig_start;
    sigvl->addWidget(build_input_row("Start Date", sig_start, sig_tab));
    auto* sig_end = new QLineEdit(sig_tab);
    sig_end->setPlaceholderText("End date (YYYY-MM-DD)");
    sig_end->setStyleSheet(input_style);
    text_inputs_["ls_end"] = sig_end;
    sigvl->addWidget(build_input_row("End Date", sig_end, sig_tab));
    auto* sig_run = make_run_button("FETCH SIGNALS", sig_tab);
    connect(sig_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching...");
        QJsonObject params;
        QJsonArray instr;
        for (auto& s : text_inputs_["ls_instruments"]->text().split(','))
            if (!s.trimmed().isEmpty()) instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        QJsonArray fields;
        for (auto& s : text_inputs_["ls_fields"]->text().split(','))
            if (!s.trimmed().isEmpty()) fields.append(s.trimmed());
        params["fields"] = fields;
        if (!text_inputs_["ls_start"]->text().isEmpty()) params["start_date"] = text_inputs_["ls_start"]->text().trimmed();
        if (!text_inputs_["ls_end"]->text().isEmpty())   params["end_date"]   = text_inputs_["ls_end"]->text().trimmed();
        AIQuantLabService::instance().signals_get_data(params);
    });
    sigvl->addWidget(sig_run);
    sigvl->addStretch();
    tabs->addTab(sig_tab, "Signal Data");

    // ── Factor Analysis ──
    auto* fa_tab = new QWidget;
    auto* favl = new QVBoxLayout(fa_tab);
    favl->setContentsMargins(12, 12, 12, 12);
    favl->setSpacing(8);
    auto* fa_instr = new QLineEdit(fa_tab);
    fa_instr->setPlaceholderText("Instruments (comma-separated)");
    fa_instr->setStyleSheet(input_style);
    text_inputs_["ls_fa_instr"] = fa_instr;
    favl->addWidget(build_input_row("Instruments", fa_instr, fa_tab));
    auto* fa_start = new QLineEdit(fa_tab);
    fa_start->setPlaceholderText("Start date (YYYY-MM-DD)");
    fa_start->setStyleSheet(input_style);
    text_inputs_["ls_fa_start"] = fa_start;
    favl->addWidget(build_input_row("Start Date", fa_start, fa_tab));
    auto* fa_end = new QLineEdit(fa_tab);
    fa_end->setPlaceholderText("End date (YYYY-MM-DD)");
    fa_end->setStyleSheet(input_style);
    text_inputs_["ls_fa_end"] = fa_end;
    favl->addWidget(build_input_row("End Date", fa_end, fa_tab));
    auto* fa_run = make_run_button("RUN FACTOR ANALYSIS", fa_tab);
    connect(fa_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing...");
        QJsonObject params;
        QJsonArray instr;
        for (auto& s : text_inputs_["ls_fa_instr"]->text().split(','))
            if (!s.trimmed().isEmpty()) instr.append(s.trimmed().toLower());
        params["instruments"] = instr;
        if (!text_inputs_["ls_fa_start"]->text().isEmpty()) params["start_date"] = text_inputs_["ls_fa_start"]->text().trimmed();
        if (!text_inputs_["ls_fa_end"]->text().isEmpty())   params["end_date"]   = text_inputs_["ls_fa_end"]->text().trimmed();
        AIQuantLabService::instance().signals_get_factor_analysis(params);
    });
    favl->addWidget(fa_run);
    favl->addStretch();
    tabs->addTab(fa_tab, "Factor Analysis");

    // ── Feature Importance ──
    auto* fi_tab = new QWidget;
    auto* fivl = new QVBoxLayout(fi_tab);
    fivl->setContentsMargins(12, 12, 12, 12);
    fivl->setSpacing(8);
    auto* fi_model = new QLineEdit(fi_tab);
    fi_model->setPlaceholderText("Trained model ID");
    fi_model->setStyleSheet(input_style);
    text_inputs_["ls_fi_model"] = fi_model;
    fivl->addWidget(build_input_row("Model ID", fi_model, fi_tab));
    auto* fi_run = make_run_button("GET FEATURE IMPORTANCE", fi_tab);
    connect(fi_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ls_fi_model"]->text().trimmed();
        AIQuantLabService::instance().signals_get_feature_importance(params);
    });
    fivl->addWidget(fi_run);
    fivl->addStretch();
    tabs->addTab(fi_tab, "Feature Importance");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ONLINE LEARNING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_online_learning_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());
    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:3px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    // ── Models ──
    auto* models_tab = new QWidget;
    auto* mtvl = new QVBoxLayout(models_tab);
    mtvl->setContentsMargins(12, 12, 12, 12);
    mtvl->setSpacing(8);
    auto* mt_info = new QLabel("Incrementally trained models that update on each new data point.", models_tab);
    mt_info->setWordWrap(true);
    mt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
    mtvl->addWidget(mt_info);
    auto* mt_list = make_run_button("LIST ALL MODELS", models_tab);
    connect(mt_list, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().online_list_models();
    });
    mtvl->addWidget(mt_list);
    mtvl->addStretch();
    tabs->addTab(models_tab, "Models");

    // ── Create Model ──
    auto* create_tab = new QWidget;
    auto* ctvl = new QVBoxLayout(create_tab);
    ctvl->setContentsMargins(12, 12, 12, 12);
    ctvl->setSpacing(8);
    auto* ol_model_type = new QComboBox(create_tab);
    ol_model_type->setStyleSheet(combo_style);
    ol_model_type->addItems({"linear", "bayesian_linear", "pa", "tree", "adaptive_tree", "bagging", "ewa", "srp"});
    combo_inputs_["ol_model_type"] = ol_model_type;
    ctvl->addWidget(build_input_row("Model Type", ol_model_type, create_tab));
    auto* ol_model_id = new QLineEdit(create_tab);
    ol_model_id->setPlaceholderText("Model ID (optional, auto-generated if blank)");
    ol_model_id->setStyleSheet(input_style);
    text_inputs_["ol_model_id"] = ol_model_id;
    ctvl->addWidget(build_input_row("Model ID", ol_model_id, create_tab));
    auto* ol_create = make_run_button("CREATE MODEL", create_tab);
    connect(ol_create, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        params["model_type"] = combo_inputs_["ol_model_type"]->currentText();
        auto mid = text_inputs_["ol_model_id"]->text().trimmed();
        if (!mid.isEmpty()) params["model_id"] = mid;
        AIQuantLabService::instance().online_create_model(params);
    });
    ctvl->addWidget(ol_create);
    ctvl->addStretch();
    tabs->addTab(create_tab, "Create Model");

    // ── Incremental Train ──
    auto* train_tab = new QWidget;
    auto* ttvl = new QVBoxLayout(train_tab);
    ttvl->setContentsMargins(12, 12, 12, 12);
    ttvl->setSpacing(8);
    auto* ol_tid = new QLineEdit(train_tab);
    ol_tid->setPlaceholderText("Model ID");
    ol_tid->setStyleSheet(input_style);
    text_inputs_["ol_train_id"] = ol_tid;
    ttvl->addWidget(build_input_row("Model ID", ol_tid, train_tab));
    auto* ol_feats = new QLineEdit(train_tab);
    ol_feats->setPlaceholderText(R"({"close":94.0,"volume":1000000,"rsi":55.0})");
    ol_feats->setStyleSheet(input_style);
    text_inputs_["ol_features"] = ol_feats;
    ttvl->addWidget(build_input_row("Features (JSON)", ol_feats, train_tab));
    auto* ol_target = new QLineEdit(train_tab);
    ol_target->setPlaceholderText("Target value (e.g. 0.02)");
    ol_target->setStyleSheet(input_style);
    text_inputs_["ol_target"] = ol_target;
    ttvl->addWidget(build_input_row("Target", ol_target, train_tab));
    auto* ol_train = make_run_button("TRAIN ONE STEP", train_tab);
    connect(ol_train, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Training...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ol_train_id"]->text().trimmed();
        auto doc = QJsonDocument::fromJson(text_inputs_["ol_features"]->text().toUtf8());
        if (!doc.isNull()) params["features"] = doc.object();
        params["target"] = text_inputs_["ol_target"]->text().trimmed().toDouble();
        AIQuantLabService::instance().online_train(params);
    });
    ttvl->addWidget(ol_train);
    ttvl->addStretch();
    tabs->addTab(train_tab, "Incremental Train");

    // ── Predict ──
    auto* pred_tab = new QWidget;
    auto* predvl = new QVBoxLayout(pred_tab);
    predvl->setContentsMargins(12, 12, 12, 12);
    predvl->setSpacing(8);
    auto* ol_pid = new QLineEdit(pred_tab);
    ol_pid->setPlaceholderText("Model ID");
    ol_pid->setStyleSheet(input_style);
    text_inputs_["ol_pred_id"] = ol_pid;
    predvl->addWidget(build_input_row("Model ID", ol_pid, pred_tab));
    auto* ol_pfeats = new QLineEdit(pred_tab);
    ol_pfeats->setPlaceholderText(R"({"close":95.0,"volume":1100000,"rsi":58.0})");
    ol_pfeats->setStyleSheet(input_style);
    text_inputs_["ol_pred_feats"] = ol_pfeats;
    predvl->addWidget(build_input_row("Features (JSON)", ol_pfeats, pred_tab));
    auto* ol_pred = make_run_button("PREDICT", pred_tab);
    connect(ol_pred, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Predicting...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ol_pred_id"]->text().trimmed();
        auto doc = QJsonDocument::fromJson(text_inputs_["ol_pred_feats"]->text().toUtf8());
        if (!doc.isNull()) params["features"] = doc.object();
        AIQuantLabService::instance().online_predict(params);
    });
    predvl->addWidget(ol_pred);
    auto* ol_perf = make_run_button("GET PERFORMANCE", pred_tab);
    connect(ol_perf, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        QJsonObject params;
        params["model_id"] = text_inputs_["ol_pred_id"]->text().trimmed();
        AIQuantLabService::instance().online_performance(params);
    });
    predvl->addWidget(ol_perf);
    predvl->addStretch();
    tabs->addTab(pred_tab, "Predict");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// META LEARNING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_meta_learning_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());
    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:3px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    // ── Model Selection ──
    auto* sel_tab = new QWidget;
    auto* selvl = new QVBoxLayout(sel_tab);
    selvl->setContentsMargins(12, 12, 12, 12);
    selvl->setSpacing(8);
    auto* sel_info = new QLabel("Automatically select the best model from a set of candidates.", sel_tab);
    sel_info->setWordWrap(true);
    sel_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
    selvl->addWidget(sel_info);
    auto* sel_list_btn = make_run_button("LIST AVAILABLE MODELS", sel_tab);
    connect(sel_list_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().meta_list_models();
    });
    selvl->addWidget(sel_list_btn);
    auto* sel_models = new QLineEdit(sel_tab);
    sel_models->setPlaceholderText("Model IDs (comma-separated, e.g. lightgbm,xgboost,random_forest)");
    sel_models->setStyleSheet(input_style);
    text_inputs_["ml_sel_models"] = sel_models;
    selvl->addWidget(build_input_row("Models", sel_models, sel_tab));
    auto* sel_task = new QComboBox(sel_tab);
    sel_task->setStyleSheet(combo_style);
    sel_task->addItems({"regression", "classification"});
    combo_inputs_["ml_sel_task"] = sel_task;
    selvl->addWidget(build_input_row("Task Type", sel_task, sel_tab));
    auto* sel_run = make_run_button("RUN MODEL SELECTION", sel_tab);
    connect(sel_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Selecting...");
        QJsonObject params;
        QJsonArray model_ids;
        for (auto& s : text_inputs_["ml_sel_models"]->text().split(','))
            if (!s.trimmed().isEmpty()) model_ids.append(s.trimmed());
        params["model_ids"] = model_ids;
        params["task_type"] = combo_inputs_["ml_sel_task"]->currentText();
        AIQuantLabService::instance().meta_run_selection(params);
    });
    selvl->addWidget(sel_run);
    selvl->addStretch();
    tabs->addTab(sel_tab, "Model Selection");

    // ── Ensemble ──
    auto* ens_tab = new QWidget;
    auto* ensvl = new QVBoxLayout(ens_tab);
    ensvl->setContentsMargins(12, 12, 12, 12);
    ensvl->setSpacing(8);
    auto* ens_keys = new QLineEdit(ens_tab);
    ens_keys->setPlaceholderText("Model keys (from selection output, comma-separated)");
    ens_keys->setStyleSheet(input_style);
    text_inputs_["ml_ens_keys"] = ens_keys;
    ensvl->addWidget(build_input_row("Model Keys", ens_keys, ens_tab));
    auto* ens_method = new QComboBox(ens_tab);
    ens_method->setStyleSheet(combo_style);
    ens_method->addItems({"voting", "stacking", "averaging"});
    combo_inputs_["ml_ens_method"] = ens_method;
    ensvl->addWidget(build_input_row("Ensemble Method", ens_method, ens_tab));
    auto* ens_run = make_run_button("CREATE ENSEMBLE", ens_tab);
    connect(ens_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating ensemble...");
        QJsonObject params;
        QJsonArray keys;
        for (auto& s : text_inputs_["ml_ens_keys"]->text().split(','))
            if (!s.trimmed().isEmpty()) keys.append(s.trimmed());
        params["model_keys"] = keys;
        params["method"] = combo_inputs_["ml_ens_method"]->currentText();
        AIQuantLabService::instance().meta_create_ensemble(params);
    });
    ensvl->addWidget(ens_run);
    ensvl->addStretch();
    tabs->addTab(ens_tab, "Ensemble");

    // ── Hyperparameter Tuning ──
    auto* tune_tab = new QWidget;
    auto* tunevl = new QVBoxLayout(tune_tab);
    tunevl->setContentsMargins(12, 12, 12, 12);
    tunevl->setSpacing(8);
    auto* tune_model = new QComboBox(tune_tab);
    tune_model->setStyleSheet(combo_style);
    tune_model->addItems({"lightgbm", "xgboost", "random_forest", "catboost"});
    combo_inputs_["ml_tune_model"] = tune_model;
    tunevl->addWidget(build_input_row("Model", tune_model, tune_tab));
    auto* tune_grid = new QLineEdit(tune_tab);
    tune_grid->setPlaceholderText(R"({"n_estimators":[50,100,200],"max_depth":[3,5,7]})");
    tune_grid->setStyleSheet(input_style);
    text_inputs_["ml_tune_grid"] = tune_grid;
    tunevl->addWidget(build_input_row("Param Grid (JSON)", tune_grid, tune_tab));
    auto* tune_method = new QComboBox(tune_tab);
    tune_method->setStyleSheet(combo_style);
    tune_method->addItems({"grid", "random"});
    combo_inputs_["ml_tune_method"] = tune_method;
    tunevl->addWidget(build_input_row("Search Method", tune_method, tune_tab));
    auto* tune_run = make_run_button("TUNE HYPERPARAMETERS", tune_tab);
    connect(tune_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Tuning...");
        QJsonObject params;
        params["model_id"] = combo_inputs_["ml_tune_model"]->currentText();
        auto doc = QJsonDocument::fromJson(text_inputs_["ml_tune_grid"]->text().toUtf8());
        if (!doc.isNull()) params["param_grid"] = doc.object();
        params["search_method"] = combo_inputs_["ml_tune_method"]->currentText();
        AIQuantLabService::instance().meta_tune_hyperparameters(params);
    });
    tunevl->addWidget(tune_run);
    auto* results_btn = make_run_button("GET ALL RESULTS", tune_tab);
    connect(results_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().meta_get_results();
    });
    tunevl->addWidget(results_btn);
    tunevl->addStretch();
    tabs->addTab(tune_tab, "Hyperparameter Tuning");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HFT PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_hft_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());
    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:3px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    // ── Order Book ──
    auto* ob_tab = new QWidget;
    auto* obvl = new QVBoxLayout(ob_tab);
    obvl->setContentsMargins(12, 12, 12, 12);
    obvl->setSpacing(8);
    auto* hft_symbol = new QLineEdit(ob_tab);
    hft_symbol->setPlaceholderText("Symbol (e.g. AAPL)");
    hft_symbol->setStyleSheet(input_style);
    text_inputs_["hft_symbol"] = hft_symbol;
    obvl->addWidget(build_input_row("Symbol", hft_symbol, ob_tab));
    auto* ob_create = make_run_button("CREATE ORDER BOOK", ob_tab);
    connect(ob_create, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        params["symbol"] = text_inputs_["hft_symbol"]->text().trimmed().toUpper();
        AIQuantLabService::instance().hft_create_orderbook(params);
    });
    obvl->addWidget(ob_create);
    auto* ob_snap = make_run_button("GET SNAPSHOT", ob_tab);
    connect(ob_snap, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching...");
        QJsonObject params;
        params["symbol"] = text_inputs_["hft_symbol"]->text().trimmed().toUpper();
        AIQuantLabService::instance().hft_snapshot(params);
    });
    obvl->addWidget(ob_snap);
    obvl->addStretch();
    tabs->addTab(ob_tab, "Order Book");

    // ── Market Making ──
    auto* mm_tab = new QWidget;
    auto* mmvl = new QVBoxLayout(mm_tab);
    mmvl->setContentsMargins(12, 12, 12, 12);
    mmvl->setSpacing(8);
    auto* mm_sym = new QLineEdit(mm_tab);
    mm_sym->setPlaceholderText("Symbol (e.g. AAPL)");
    mm_sym->setStyleSheet(input_style);
    text_inputs_["hft_mm_symbol"] = mm_sym;
    mmvl->addWidget(build_input_row("Symbol", mm_sym, mm_tab));
    auto* mm_mid = new QLineEdit(mm_tab);
    mm_mid->setPlaceholderText("Mid price (e.g. 150.0)");
    mm_mid->setStyleSheet(input_style);
    text_inputs_["hft_mm_mid"] = mm_mid;
    mmvl->addWidget(build_input_row("Mid Price", mm_mid, mm_tab));
    auto* mm_spread = new QLineEdit(mm_tab);
    mm_spread->setPlaceholderText("Spread multiplier (e.g. 0.02)");
    mm_spread->setStyleSheet(input_style);
    text_inputs_["hft_mm_spread"] = mm_spread;
    mmvl->addWidget(build_input_row("Spread Multiplier", mm_spread, mm_tab));
    auto* mm_run = make_run_button("GET MARKET MAKING QUOTES", mm_tab);
    connect(mm_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating...");
        QJsonObject params;
        params["symbol"] = text_inputs_["hft_mm_symbol"]->text().trimmed().toUpper();
        params["mid_price"] = text_inputs_["hft_mm_mid"]->text().trimmed().toDouble();
        params["spread_multiplier"] = text_inputs_["hft_mm_spread"]->text().trimmed().toDouble();
        AIQuantLabService::instance().hft_market_making_quotes(params);
    });
    mmvl->addWidget(mm_run);
    mmvl->addStretch();
    tabs->addTab(mm_tab, "Market Making");

    // ── Toxic Flow Detection ──
    auto* toxic_tab = new QWidget;
    auto* toxicvl = new QVBoxLayout(toxic_tab);
    toxicvl->setContentsMargins(12, 12, 12, 12);
    toxicvl->setSpacing(8);
    auto* tox_sym = new QLineEdit(toxic_tab);
    tox_sym->setPlaceholderText("Symbol (e.g. AAPL)");
    tox_sym->setStyleSheet(input_style);
    text_inputs_["hft_tox_symbol"] = tox_sym;
    toxicvl->addWidget(build_input_row("Symbol", tox_sym, toxic_tab));
    auto* tox_price = new QLineEdit(toxic_tab);
    tox_price->setPlaceholderText("Trade price (e.g. 150.5)");
    tox_price->setStyleSheet(input_style);
    text_inputs_["hft_tox_price"] = tox_price;
    toxicvl->addWidget(build_input_row("Trade Price", tox_price, toxic_tab));
    auto* tox_size = new QLineEdit(toxic_tab);
    tox_size->setPlaceholderText("Trade size (e.g. 100)");
    tox_size->setStyleSheet(input_style);
    text_inputs_["hft_tox_size"] = tox_size;
    toxicvl->addWidget(build_input_row("Trade Size", tox_size, toxic_tab));
    auto* tox_run = make_run_button("DETECT TOXIC FLOW", toxic_tab);
    connect(tox_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Detecting...");
        QJsonObject params;
        params["symbol"] = text_inputs_["hft_tox_symbol"]->text().trimmed().toUpper();
        params["trade_price"] = text_inputs_["hft_tox_price"]->text().trimmed().toDouble();
        params["trade_size"] = text_inputs_["hft_tox_size"]->text().trimmed().toDouble();
        AIQuantLabService::instance().hft_detect_toxic(params);
    });
    toxicvl->addWidget(tox_run);
    toxicvl->addStretch();
    tabs->addTab(toxic_tab, "Toxic Flow");

    // ── Order Execution ──
    auto* exec_tab = new QWidget;
    auto* execvl = new QVBoxLayout(exec_tab);
    execvl->setContentsMargins(12, 12, 12, 12);
    execvl->setSpacing(8);
    auto* exec_sym = new QLineEdit(exec_tab);
    exec_sym->setPlaceholderText("Symbol (e.g. AAPL)");
    exec_sym->setStyleSheet(input_style);
    text_inputs_["hft_exec_symbol"] = exec_sym;
    execvl->addWidget(build_input_row("Symbol", exec_sym, exec_tab));
    auto* exec_side = new QComboBox(exec_tab);
    exec_side->setStyleSheet(combo_style);
    exec_side->addItems({"buy", "sell"});
    combo_inputs_["hft_exec_side"] = exec_side;
    execvl->addWidget(build_input_row("Side", exec_side, exec_tab));
    auto* exec_qty = new QLineEdit(exec_tab);
    exec_qty->setPlaceholderText("Quantity (e.g. 100)");
    exec_qty->setStyleSheet(input_style);
    text_inputs_["hft_exec_qty"] = exec_qty;
    execvl->addWidget(build_input_row("Quantity", exec_qty, exec_tab));
    auto* exec_price = new QLineEdit(exec_tab);
    exec_price->setPlaceholderText("Limit price (e.g. 150.0)");
    exec_price->setStyleSheet(input_style);
    text_inputs_["hft_exec_price"] = exec_price;
    execvl->addWidget(build_input_row("Limit Price", exec_price, exec_tab));
    auto* exec_run = make_run_button("EXECUTE ORDER", exec_tab);
    connect(exec_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Executing...");
        QJsonObject params;
        params["symbol"] = text_inputs_["hft_exec_symbol"]->text().trimmed().toUpper();
        params["side"] = combo_inputs_["hft_exec_side"]->currentText();
        params["quantity"] = text_inputs_["hft_exec_qty"]->text().trimmed().toDouble();
        params["price"] = text_inputs_["hft_exec_price"]->text().trimmed().toDouble();
        AIQuantLabService::instance().hft_execute_order(params);
    });
    execvl->addWidget(exec_run);
    execvl->addStretch();
    tabs->addTab(exec_tab, "Order Execution");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ROLLING RETRAINING PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_rolling_retraining_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto input_style = QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QLineEdit:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL).arg(module_.color.name());
    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:3px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY).arg(ui::fonts::SMALL);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                "QTabBar::tab { background:%3; color:%4; padding:6px 12px;"
                                "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                "QTabBar::tab:selected { background:%2; color:%7; font-weight:700; }")
                            .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                            .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL).arg(module_.color.name()));

    // ── Schedules ──
    auto* list_tab = new QWidget;
    auto* ltvl = new QVBoxLayout(list_tab);
    ltvl->setContentsMargins(12, 12, 12, 12);
    ltvl->setSpacing(8);
    auto* lt_info = new QLabel("View all configured rolling retraining schedules.", list_tab);
    lt_info->setWordWrap(true);
    lt_info->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::SMALL).arg(ui::fonts::DATA_FAMILY));
    ltvl->addWidget(lt_info);
    auto* lt_run = make_run_button("LIST SCHEDULES", list_tab);
    connect(lt_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading...");
        AIQuantLabService::instance().rolling_list_schedules();
    });
    ltvl->addWidget(lt_run);
    ltvl->addStretch();
    tabs->addTab(list_tab, "Schedules");

    // ── Create Schedule ──
    auto* create_tab = new QWidget;
    auto* ctvl = new QVBoxLayout(create_tab);
    ctvl->setContentsMargins(12, 12, 12, 12);
    ctvl->setSpacing(8);
    auto* rr_model_id = new QLineEdit(create_tab);
    rr_model_id->setPlaceholderText("Model ID (e.g. model_aapl)");
    rr_model_id->setStyleSheet(input_style);
    text_inputs_["rr_model_id"] = rr_model_id;
    ctvl->addWidget(build_input_row("Model ID", rr_model_id, create_tab));
    auto* rr_freq = new QComboBox(create_tab);
    rr_freq->setStyleSheet(combo_style);
    rr_freq->addItems({"daily", "weekly", "monthly"});
    combo_inputs_["rr_frequency"] = rr_freq;
    ctvl->addWidget(build_input_row("Frequency", rr_freq, create_tab));
    auto* rr_window = new QLineEdit(create_tab);
    rr_window->setPlaceholderText("Retrain window (trading days, e.g. 252)");
    rr_window->setStyleSheet(input_style);
    text_inputs_["rr_window"] = rr_window;
    ctvl->addWidget(build_input_row("Window (days)", rr_window, create_tab));
    auto* rr_create = make_run_button("CREATE SCHEDULE", create_tab);
    connect(rr_create, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Creating...");
        QJsonObject params;
        params["model_id"]   = text_inputs_["rr_model_id"]->text().trimmed();
        params["frequency"]  = combo_inputs_["rr_frequency"]->currentText();
        auto w_text = text_inputs_["rr_window"]->text().trimmed();
        if (!w_text.isEmpty()) params["window"] = w_text.toInt();
        AIQuantLabService::instance().rolling_create_schedule(params);
    });
    ctvl->addWidget(rr_create);
    ctvl->addStretch();
    tabs->addTab(create_tab, "Create Schedule");

    // ── Execute Retrain ──
    auto* retrain_tab = new QWidget;
    auto* retrainvl = new QVBoxLayout(retrain_tab);
    retrainvl->setContentsMargins(12, 12, 12, 12);
    retrainvl->setSpacing(8);
    auto* rr_exec_id = new QLineEdit(retrain_tab);
    rr_exec_id->setPlaceholderText("Model ID to retrain");
    rr_exec_id->setStyleSheet(input_style);
    text_inputs_["rr_exec_id"] = rr_exec_id;
    retrainvl->addWidget(build_input_row("Model ID", rr_exec_id, retrain_tab));
    auto* rr_exec = make_run_button("EXECUTE RETRAIN NOW", retrain_tab);
    connect(rr_exec, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Retraining...");
        QJsonObject params;
        params["model_id"] = text_inputs_["rr_exec_id"]->text().trimmed();
        AIQuantLabService::instance().rolling_execute_retrain(params);
    });
    retrainvl->addWidget(rr_exec);
    retrainvl->addStretch();
    tabs->addTab(retrain_tab, "Execute Retrain");

    vl->addWidget(tabs, 1);
    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

} // namespace fincept::screens
