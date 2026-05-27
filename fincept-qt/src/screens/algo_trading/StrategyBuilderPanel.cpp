// src/screens/algo_trading/StrategyBuilderPanel.cpp
#include "screens/algo_trading/StrategyBuilderPanel.h"

#include "algo_engine/AlgoEngine.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/AlgoDeployDialog.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QScrollArea>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {
namespace algo_ns = fincept::algo;

StrategyBuilderPanel::StrategyBuilderPanel(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("strategyBuilderPanel"));
    build_ui();
    connect_service();
}

void StrategyBuilderPanel::build_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    main_layout->addWidget(build_top_toolbar());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("builderSplitter"));
    splitter->addWidget(build_left_panel());
    splitter->addWidget(build_right_panel());
    splitter->setStretchFactor(0, 45);
    splitter->setStretchFactor(1, 55);

    main_layout->addWidget(splitter, 1);
}

QWidget* StrategyBuilderPanel::build_top_toolbar() {
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("builderToolbar"));
    toolbar->setFixedHeight(44);
    auto* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(12, 4, 12, 4);
    layout->setSpacing(8);

    name_edit_ = new QLineEdit(this);
    name_edit_->setObjectName(QStringLiteral("builderNameEdit"));
    name_edit_->setPlaceholderText(tr("Strategy Name"));
    name_edit_->setMinimumWidth(160);

    desc_edit_ = new QLineEdit(this);
    desc_edit_->setObjectName(QStringLiteral("builderDescEdit"));
    desc_edit_->setPlaceholderText(tr("Description (optional)"));
    desc_edit_->setMinimumWidth(160);

    symbol_combo_ = new ui::algo::SymbolSearchCombo(this);
    symbol_combo_->setMinimumWidth(120);

    timeframe_combo_ = new QComboBox(this);
    timeframe_combo_->setObjectName(QStringLiteral("builderTimeframe"));
    for (const auto& tf : services::algo::algo_timeframes())
        timeframe_combo_->addItem(tf);
    timeframe_combo_->setCurrentText(QStringLiteral("5m"));

    save_btn_ = new QPushButton(tr("Save"), this);
    save_btn_->setObjectName(QStringLiteral("builderSaveBtn"));
    backtest_btn_ = new QPushButton(tr("Backtest"), this);
    backtest_btn_->setObjectName(QStringLiteral("builderBacktestBtn"));
    deploy_btn_ = new QPushButton(tr("Deploy"), this);
    deploy_btn_->setObjectName(QStringLiteral("builderDeployBtn"));

    layout->addWidget(name_edit_);
    layout->addWidget(desc_edit_);
    layout->addWidget(symbol_combo_);
    layout->addWidget(timeframe_combo_);
    layout->addStretch();
    layout->addWidget(save_btn_);
    layout->addWidget(backtest_btn_);
    layout->addWidget(deploy_btn_);

    connect(save_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_save);
    connect(backtest_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_backtest);
    connect(deploy_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_deploy);

    return toolbar;
}

QWidget* StrategyBuilderPanel::build_left_panel() {
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("builderLeftScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

    entry_section_ = new ui::algo::ConditionSection(
        ui::algo::ConditionSection::Type::Entry, this);
    exit_section_ = new ui::algo::ConditionSection(
        ui::algo::ConditionSection::Type::Exit, this);
    risk_panel_ = new ui::algo::RiskManagementPanel(this);

    layout->addWidget(entry_section_);
    layout->addWidget(exit_section_);
    layout->addWidget(risk_panel_);
    layout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

QWidget* StrategyBuilderPanel::build_right_panel() {
    auto* right = new QWidget(this);
    auto* layout = new QVBoxLayout(right);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    // Chart placeholder
    auto* chart_placeholder = new QWidget(this);
    chart_placeholder->setObjectName(QStringLiteral("builderChartPlaceholder"));
    chart_placeholder->setMinimumHeight(200);
    auto* chart_layout = new QVBoxLayout(chart_placeholder);
    auto* chart_label = new QLabel(tr("Select a symbol and deploy to view live chart"), this);
    chart_label->setObjectName(QStringLiteral("builderChartLabel"));
    chart_label->setAlignment(Qt::AlignCenter);
    chart_layout->addWidget(chart_label);
    layout->addWidget(chart_placeholder, 1);

    // Backtest params
    auto* bt_section = new QLabel(tr("BACKTEST"), this);
    bt_section->setObjectName(QStringLiteral("builderBtHeader"));
    layout->addWidget(bt_section);

    auto* bt_grid = new QGridLayout();
    bt_grid->setSpacing(6);

    bt_capital_ = new QDoubleSpinBox(this);
    bt_capital_->setObjectName(QStringLiteral("builderBtCapital"));
    bt_capital_->setRange(1000, 100000000);
    bt_capital_->setDecimals(0);
    bt_capital_->setValue(100000);
    bt_capital_->setPrefix(QStringLiteral("$ "));

    bt_start_date_ = new QLineEdit(this);
    bt_start_date_->setObjectName(QStringLiteral("builderBtStart"));
    bt_start_date_->setPlaceholderText(QStringLiteral("2024-01-01"));
    bt_start_date_->setText(QStringLiteral("2024-01-01"));

    bt_end_date_ = new QLineEdit(this);
    bt_end_date_->setObjectName(QStringLiteral("builderBtEnd"));
    bt_end_date_->setPlaceholderText(QStringLiteral("2024-12-31"));
    bt_end_date_->setText(QStringLiteral("2024-12-31"));

    bt_grid->addWidget(new QLabel(tr("Capital"), this), 0, 0);
    bt_grid->addWidget(bt_capital_, 0, 1);
    bt_grid->addWidget(new QLabel(tr("Start"), this), 1, 0);
    bt_grid->addWidget(bt_start_date_, 1, 1);
    bt_grid->addWidget(new QLabel(tr("End"), this), 2, 0);
    bt_grid->addWidget(bt_end_date_, 2, 1);
    layout->addLayout(bt_grid);

    status_label_ = new QLabel(this);
    status_label_->setObjectName(QStringLiteral("builderStatus"));
    layout->addWidget(status_label_);

    // KPI grid (hidden until backtest runs)
    kpi_grid_widget_ = new QWidget(this);
    kpi_grid_widget_->setObjectName(QStringLiteral("builderKpiGrid"));
    kpi_grid_widget_->setVisible(false);

    auto* kpi_grid = new QGridLayout(kpi_grid_widget_);
    kpi_grid->setSpacing(8);

    auto make_kpi = [&](int row, int col, const QString& title) -> std::pair<QLabel*, QLabel*> {
        auto* card = new QWidget(kpi_grid_widget_);
        card->setObjectName(QStringLiteral("kpiCard"));
        auto* card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(8, 6, 8, 6);
        card_layout->setSpacing(2);

        auto* title_lbl = new QLabel(title, card);
        title_lbl->setObjectName(QStringLiteral("kpiTitle"));
        auto* val_lbl = new QLabel(QStringLiteral("--"), card);
        val_lbl->setObjectName(QStringLiteral("kpiValue"));
        auto* sub_lbl = new QLabel(card);
        sub_lbl->setObjectName(QStringLiteral("kpiSub"));

        card_layout->addWidget(title_lbl);
        card_layout->addWidget(val_lbl);
        card_layout->addWidget(sub_lbl);
        kpi_grid->addWidget(card, row, col);
        return {val_lbl, sub_lbl};
    };

    auto [tr_v, tr_s] = make_kpi(0, 0, tr("TOTAL RETURN"));
    kpi_total_return_val_ = tr_v; kpi_total_return_sub_ = tr_s;
    auto [sh_v, sh_s] = make_kpi(0, 1, tr("SHARPE RATIO"));
    kpi_sharpe_val_ = sh_v; kpi_sharpe_sub_ = sh_s;
    auto [dd_v, dd_s] = make_kpi(0, 2, tr("MAX DRAWDOWN"));
    kpi_max_dd_val_ = dd_v; kpi_max_dd_sub_ = dd_s;
    auto [wr_v, wr_s] = make_kpi(1, 0, tr("WIN RATE"));
    kpi_win_rate_val_ = wr_v; kpi_win_rate_sub_ = wr_s;
    auto [tt_v, tt_s] = make_kpi(1, 1, tr("TOTAL TRADES"));
    kpi_trades_val_ = tt_v; kpi_trades_sub_ = tt_s;
    auto [pf_v, pf_s] = make_kpi(1, 2, tr("PROFIT FACTOR"));
    kpi_profit_factor_val_ = pf_v; kpi_profit_factor_sub_ = pf_s;

    layout->addWidget(kpi_grid_widget_);
    layout->addStretch();

    return right;
}

void StrategyBuilderPanel::connect_service() {
    auto& svc = services::algo::AlgoTradingService::instance();
    connect(&svc, &services::algo::AlgoTradingService::backtest_result,
            this, &StrategyBuilderPanel::on_backtest_result);
    connect(&svc, &services::algo::AlgoTradingService::error_occurred,
            this, &StrategyBuilderPanel::on_error);
    connect(&svc, &services::algo::AlgoTradingService::strategy_saved,
            this, [this](const QString& id) {
                status_label_->setText(tr("Strategy saved: %1").arg(id));
            });
}

void StrategyBuilderPanel::on_save() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText(tr("Please enter a strategy name."));
        return;
    }

    services::algo::AlgoStrategy strat;
    strat.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    strat.name = name_edit_->text().trimmed();
    strat.description = desc_edit_->text().trimmed();
    strat.timeframe = timeframe_combo_->currentText();
    strat.entry_conditions = entry_section_->conditions();
    strat.exit_conditions = exit_section_->conditions();
    strat.entry_logic = entry_section_->combined_logic();
    strat.exit_logic = exit_section_->combined_logic();
    strat.stop_loss = risk_panel_->stop_loss();
    strat.take_profit = risk_panel_->take_profit();
    strat.trailing_stop = risk_panel_->trailing_stop();

    services::algo::AlgoTradingService::instance().save_strategy(strat);
    status_label_->setText(tr("Saving strategy..."));
}

void StrategyBuilderPanel::on_backtest() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText(tr("Save strategy first."));
        return;
    }

    // Save first, then backtest
    on_save();
    QString symbol = symbol_combo_->currentText().trimmed();
    if (symbol.isEmpty()) symbol = QStringLiteral("RELIANCE");

    services::algo::AlgoTradingService::instance().run_backtest(
        QString(), symbol, bt_start_date_->text(), bt_end_date_->text(),
        bt_capital_->value());
    status_label_->setText(tr("Running backtest..."));
}

void StrategyBuilderPanel::on_deploy() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText(tr("Save strategy first."));
        return;
    }

    // Build strategy from current UI state
    services::algo::AlgoStrategy strat;
    strat.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    strat.name = name_edit_->text().trimmed();
    strat.description = desc_edit_->text().trimmed();
    strat.timeframe = timeframe_combo_->currentText();
    strat.entry_conditions = entry_section_->conditions();
    strat.exit_conditions = exit_section_->conditions();
    strat.entry_logic = entry_section_->combined_logic();
    strat.exit_logic = exit_section_->combined_logic();
    strat.stop_loss = risk_panel_->stop_loss();
    strat.take_profit = risk_panel_->take_profit();
    strat.trailing_stop = risk_panel_->trailing_stop();

    auto* dialog = new AlgoDeployDialog(strat, this);
    if (dialog->exec() == QDialog::Accepted) {
        auto deployment = dialog->deployment();
        deployment.quantity = risk_panel_->quantity();
        deployment.max_order_value = risk_panel_->max_order_value();

        // Save strategy first
        services::algo::AlgoTradingService::instance().save_strategy(strat);

        // Deploy via C++ engine
        algo_ns::AlgoEngine::instance().start_deployment(deployment, strat);
        status_label_->setText(tr("Deploying strategy..."));

        // Switch to deployments tab handled by AlgoTradingScreen
    }
    dialog->deleteLater();
}

void StrategyBuilderPanel::on_backtest_result(const QJsonObject& payload) {
    status_label_->setText(tr("Backtest complete."));
    display_backtest_result(payload);
    LOG_INFO("AlgoTrading", "Backtest result displayed");
}

void StrategyBuilderPanel::on_error(const QString& context, const QString& msg) {
    status_label_->setText(QStringLiteral("[%1] %2").arg(context, msg));
}

void StrategyBuilderPanel::display_backtest_result(const QJsonObject& payload) {
    kpi_grid_widget_->setVisible(true);

    double total_return = payload.value("total_return").toDouble();
    double sharpe = payload.value("sharpe_ratio").toDouble();
    double max_dd = payload.value("max_drawdown").toDouble();
    int total_trades = payload.value("total_trades").toInt();
    double win_rate = payload.value("win_rate").toDouble();
    double profit_factor = payload.value("profit_factor").toDouble();
    double final_val = payload.value("final_value").toDouble();

    kpi_total_return_val_->setText(
        QString("%1%2%").arg(total_return >= 0 ? "+" : "").arg(total_return, 0, 'f', 2));
    kpi_total_return_sub_->setText(QString("Final: $%1").arg(final_val, 0, 'f', 0));

    kpi_sharpe_val_->setText(QString::number(sharpe, 'f', 3));
    kpi_sharpe_sub_->setText(sharpe >= 1.0 ? "Excellent" : sharpe >= 0.5 ? "Good" : "Weak");

    kpi_max_dd_val_->setText(QString("-%1%").arg(qAbs(max_dd), 0, 'f', 2));
    kpi_max_dd_sub_->setText("Max Drawdown");

    kpi_win_rate_val_->setText(QString("%1%").arg(win_rate, 0, 'f', 1));
    kpi_win_rate_sub_->setText(win_rate >= 50.0 ? "Above average" : "Below average");

    kpi_trades_val_->setText(QString::number(total_trades));
    kpi_trades_sub_->setText("Total trades");

    kpi_profit_factor_val_->setText(QString::number(profit_factor, 'f', 2));
    kpi_profit_factor_sub_->setText(
        profit_factor >= 1.5 ? "Strong" : profit_factor >= 1.0 ? "Profitable" : "Losing");
}

void StrategyBuilderPanel::clear_results() {
    kpi_grid_widget_->setVisible(false);
}

QVariantMap StrategyBuilderPanel::save_draft() const {
    QVariantMap draft;
    draft["name"] = name_edit_->text();
    draft["description"] = desc_edit_->text();
    draft["symbol"] = symbol_combo_->currentText();
    draft["timeframe"] = timeframe_combo_->currentText();
    draft["entry_conditions"] = QJsonDocument(entry_section_->conditions()).toJson(QJsonDocument::Compact);
    draft["exit_conditions"] = QJsonDocument(exit_section_->conditions()).toJson(QJsonDocument::Compact);
    draft["entry_logic"] = entry_section_->combined_logic();
    draft["exit_logic"] = exit_section_->combined_logic();
    draft["stop_loss"] = risk_panel_->stop_loss();
    draft["take_profit"] = risk_panel_->take_profit();
    draft["trailing_stop"] = risk_panel_->trailing_stop();
    draft["quantity"] = risk_panel_->quantity();
    draft["max_order_value"] = risk_panel_->max_order_value();
    return draft;
}

void StrategyBuilderPanel::restore_draft(const QVariantMap& draft) {
    name_edit_->setText(draft.value("name").toString());
    desc_edit_->setText(draft.value("description").toString());
    symbol_combo_->setCurrentText(draft.value("symbol").toString());
    timeframe_combo_->setCurrentText(draft.value("timeframe", "5m").toString());

    auto entry_json = QJsonDocument::fromJson(draft.value("entry_conditions").toByteArray()).array();
    auto exit_json = QJsonDocument::fromJson(draft.value("exit_conditions").toByteArray()).array();
    entry_section_->set_conditions(entry_json, draft.value("entry_logic", "AND").toString());
    exit_section_->set_conditions(exit_json, draft.value("exit_logic", "AND").toString());

    risk_panel_->set_values(
        draft.value("stop_loss", 2.0).toDouble(),
        draft.value("take_profit", 5.0).toDouble(),
        draft.value("trailing_stop", 0.0).toDouble(),
        draft.value("quantity", 1.0).toDouble(),
        draft.value("max_order_value", 0.0).toDouble());
}

} // namespace fincept::screens
