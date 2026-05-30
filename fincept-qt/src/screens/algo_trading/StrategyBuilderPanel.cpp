// src/screens/algo_trading/StrategyBuilderPanel.cpp
#include "screens/algo_trading/StrategyBuilderPanel.h"

#include "algo_engine/AlgoEngine.h"
#include "core/currency/Currency.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/AlgoDeployDialog.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QDateEdit>
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

    validation_banner_ = new QLabel(this);
    validation_banner_->setObjectName(QStringLiteral("builderValidationBanner"));
    validation_banner_->setWordWrap(true);
    validation_banner_->setStyleSheet(QStringLiteral(
        "background:#3A1A1A;color:#FCA5A5;border:1px solid #7F1D1D;"
        "padding:5px 10px;font-size:11px;font-weight:600;"));
    validation_banner_->setVisible(false);
    main_layout->addWidget(validation_banner_);

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

    template_combo_ = new QComboBox(this);
    template_combo_->setObjectName(QStringLiteral("builderTemplateCombo"));
    template_combo_->addItem(tr("Templates…"));
    for (const auto& t : services::algo::algo_strategy_templates())
        template_combo_->addItem(t.name);

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
    layout->addWidget(template_combo_);
    layout->addStretch();
    layout->addWidget(save_btn_);
    layout->addWidget(backtest_btn_);
    layout->addWidget(deploy_btn_);

    connect(save_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_save);
    connect(backtest_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_backtest);
    connect(deploy_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_deploy);
    connect(template_combo_, QOverload<int>::of(&QComboBox::activated), this,
            &StrategyBuilderPanel::load_template);

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
    cur::bindPrefix(bt_capital_); // backtest capital is user-denominated

    const QDate today = QDate::currentDate();
    bt_start_date_ = new QDateEdit(this);
    bt_start_date_->setObjectName(QStringLiteral("builderBtStart"));
    bt_start_date_->setCalendarPopup(true);
    bt_start_date_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    bt_start_date_->setMinimumDate(QDate(2000, 1, 1));
    bt_start_date_->setMaximumDate(today);
    bt_start_date_->setDate(today.addYears(-1));

    bt_end_date_ = new QDateEdit(this);
    bt_end_date_->setObjectName(QStringLiteral("builderBtEnd"));
    bt_end_date_->setCalendarPopup(true);
    bt_end_date_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    bt_end_date_->setMinimumDate(QDate(2000, 1, 1));
    bt_end_date_->setMaximumDate(today);
    bt_end_date_->setDate(today);

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

    report_panel_ = new ui::algo::BacktestReportPanel(this);
    layout->addWidget(report_panel_, 1);

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

services::algo::AlgoStrategy StrategyBuilderPanel::build_strategy() {
    if (loaded_strategy_id_.isEmpty())
        loaded_strategy_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);

    services::algo::AlgoStrategy strat;
    strat.id = loaded_strategy_id_;
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
    strat.position_size_pct = risk_panel_->capital_pct();
    return strat;
}

void StrategyBuilderPanel::on_save() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText(tr("Please enter a strategy name."));
        return;
    }

    services::algo::AlgoTradingService::instance().save_strategy(build_strategy());
    status_label_->setText(tr("Saving strategy..."));
}

void StrategyBuilderPanel::on_backtest() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText(tr("Save strategy first."));
        return;
    }
    if (bt_start_date_->date() >= bt_end_date_->date()) {
        status_label_->setText(tr("Start date must be before end date."));
        return;
    }
    if (const QString err = validate(); !err.isEmpty()) {
        validation_banner_->setText(err);
        validation_banner_->setVisible(true);
        return;
    }
    validation_banner_->setVisible(false);

    // Backtest the current UI state directly, without persisting — backtesting is
    // non-mutating; the user saves explicitly via Save. (Avoids silently rewriting
    // library rows / bumping updated_at on every run.)
    auto strat = build_strategy();

    QString symbol = symbol_combo_->currentText().trimmed();
    if (symbol.isEmpty()) symbol = QStringLiteral("RELIANCE");

    services::algo::AlgoTradingService::instance().run_backtest(
        strat, symbol, bt_start_date_->date().toString(QStringLiteral("yyyy-MM-dd")),
        bt_end_date_->date().toString(QStringLiteral("yyyy-MM-dd")), bt_capital_->value());
    status_label_->setText(tr("Running backtest..."));
}

void StrategyBuilderPanel::on_deploy() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText(tr("Save strategy first."));
        return;
    }
    if (const QString err = validate(); !err.isEmpty()) {
        validation_banner_->setText(err);
        validation_banner_->setVisible(true);
        return;
    }
    validation_banner_->setVisible(false);

    // Build strategy from current UI state
    auto strat = build_strategy();

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

void StrategyBuilderPanel::load_template(int index) {
    const auto tpls = services::algo::algo_strategy_templates();
    const int i = index - 1; // index 0 is the "Templates…" placeholder
    if (i < 0 || i >= tpls.size())
        return;
    const auto& t = tpls[i];

    loaded_strategy_id_.clear(); // a template seeds a brand-new strategy
    name_edit_->setText(t.name);
    desc_edit_->setText(t.description);
    timeframe_combo_->setCurrentText(t.timeframe);
    entry_section_->set_conditions(t.entry, t.entry_logic);
    exit_section_->set_conditions(t.exit, t.exit_logic);
    risk_panel_->set_values(t.stop_loss, t.take_profit, 0.0, 1.0, 0.0);
    report_panel_->clear();
    status_label_->setText(tr("Loaded template: %1").arg(t.name));
    template_combo_->setCurrentIndex(0); // reset to placeholder
}

void StrategyBuilderPanel::load_strategy(const services::algo::AlgoStrategy& s) {
    loaded_strategy_id_ = s.id; // keep id → Save updates this strategy in place
    name_edit_->setText(s.name);
    desc_edit_->setText(s.description);
    timeframe_combo_->setCurrentText(s.timeframe.isEmpty() ? QStringLiteral("5m") : s.timeframe);
    entry_section_->set_conditions(s.entry_conditions, s.entry_logic.isEmpty() ? QStringLiteral("AND") : s.entry_logic);
    exit_section_->set_conditions(s.exit_conditions, s.exit_logic.isEmpty() ? QStringLiteral("AND") : s.exit_logic);
    risk_panel_->set_values(s.stop_loss, s.take_profit, s.trailing_stop, 1.0, 0.0,
                            s.position_size_pct > 0 ? s.position_size_pct : 100.0);
    // Size the backtest range to the strategy's timeframe (daily needs years for
    // long indicators; intraday must stay under Yahoo's history cap).
    const QDate today = QDate::currentDate();
    bt_start_date_->setDate(today.addDays(-services::algo::algo_default_lookback_days(s.timeframe)));
    bt_end_date_->setDate(today);
    report_panel_->clear();
    validation_banner_->setVisible(false);
    status_label_->setText(tr("Editing: %1").arg(s.name));
}

void StrategyBuilderPanel::load_and_backtest(const services::algo::AlgoStrategy& s,
                                             const QString& symbol, const QString& start_date,
                                             const QString& end_date) {
    load_strategy(s);
    if (!symbol.isEmpty())
        symbol_combo_->setCurrentText(symbol);
    const QDate sd = QDate::fromString(start_date, QStringLiteral("yyyy-MM-dd"));
    const QDate ed = QDate::fromString(end_date, QStringLiteral("yyyy-MM-dd"));
    if (sd.isValid())
        bt_start_date_->setDate(sd);
    if (ed.isValid())
        bt_end_date_->setDate(ed);
    on_backtest(); // reuses validation + service call; report renders in the right panel
}

QString StrategyBuilderPanel::validate() const {
    if (entry_section_->conditions().isEmpty())
        return tr("Add at least one entry condition.");
    const bool no_exit = exit_section_->conditions().isEmpty();
    if (no_exit && risk_panel_->stop_loss() <= 0.0 && risk_panel_->take_profit() <= 0.0)
        return tr("Strategy never exits: add an exit condition or set a stop-loss / take-profit.");
    return QString();
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
    report_panel_->set_result(payload);
}

void StrategyBuilderPanel::clear_results() {
    report_panel_->clear();
}

QVariantMap StrategyBuilderPanel::save_draft() const {
    QVariantMap draft;
    draft["id"] = loaded_strategy_id_;
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
    draft["capital_pct"] = risk_panel_->capital_pct();
    return draft;
}

void StrategyBuilderPanel::restore_draft(const QVariantMap& draft) {
    loaded_strategy_id_ = draft.value("id").toString();
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
        draft.value("max_order_value", 0.0).toDouble(),
        draft.value("capital_pct", 100.0).toDouble());
}

} // namespace fincept::screens
