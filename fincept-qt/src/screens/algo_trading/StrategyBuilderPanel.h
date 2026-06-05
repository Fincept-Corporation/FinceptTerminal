// src/screens/algo_trading/StrategyBuilderPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"
#include "ui/widgets/algo/BacktestReportPanel.h"
#include "ui/widgets/algo/ConditionSection.h"
#include "ui/widgets/algo/RiskManagementPanel.h"
#include "ui/widgets/algo/SymbolSearchCombo.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QString>
#include <QWidget>

namespace fincept::screens {

class StrategyBuilderPanel : public QWidget {
    Q_OBJECT
public:
    explicit StrategyBuilderPanel(QWidget* parent = nullptr);

    QVariantMap save_draft() const;
    void restore_draft(const QVariantMap& draft);
    // Loads an existing strategy for editing (keeps its id so Save updates in place).
    void load_strategy(const services::algo::AlgoStrategy& strategy);
    // Loads a strategy, sets symbol/date range, and runs a backtest immediately.
    void load_and_backtest(const services::algo::AlgoStrategy& strategy, const QString& symbol,
                           const QString& start_date, const QString& end_date);

signals:
    // Emitted after a strategy is successfully deployed, so the parent screen can
    // switch to the Dashboard tab and refresh the deployment list.
    void deployed();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void on_save();
    void on_backtest();
    void on_deploy();
    void on_backtest_result(const QJsonObject& data);
    void on_error(const QString& context, const QString& msg);

private:
    void build_ui();
    void retranslateUi();
    void connect_service();
    QWidget* build_top_toolbar();
    QWidget* build_left_panel();
    QWidget* build_right_panel();
    void display_backtest_result(const QJsonObject& data);
    void clear_results();
    // Builds an AlgoStrategy from the current UI state, reusing the same id for
    // the lifetime of this editor so repeated Save/Backtest/Deploy update one
    // strategy instead of spawning duplicates.
    services::algo::AlgoStrategy build_strategy();
    void load_template(int index);
    // Returns a human-readable error if the strategy is unrunnable, else empty.
    QString validate() const;

    // Top toolbar
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* desc_edit_ = nullptr;
    ui::algo::SymbolSearchCombo* symbol_combo_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QComboBox* template_combo_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* backtest_btn_ = nullptr;
    QPushButton* deploy_btn_ = nullptr;
    // Small pill showing whether this is a fresh draft or an existing strategy.
    QLabel* state_chip_ = nullptr;

    // Left panel — condition builder
    ui::algo::ConditionSection* entry_section_ = nullptr;
    ui::algo::ConditionSection* exit_section_ = nullptr;
    ui::algo::RiskManagementPanel* risk_panel_ = nullptr;

    // Right panel — backtest setup card
    QLabel* bt_header_ = nullptr;
    QLabel* bt_symbol_label_ = nullptr;
    QLabel* bt_capital_label_ = nullptr;
    QLabel* bt_start_label_ = nullptr;
    QLabel* bt_end_label_ = nullptr;
    QLabel* results_header_ = nullptr;
    QDoubleSpinBox* bt_capital_ = nullptr;
    QDateEdit* bt_start_date_ = nullptr;
    QDateEdit* bt_end_date_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Identity of the strategy currently being edited (empty => not yet minted).
    QString loaded_strategy_id_;

    // Right panel — backtest performance report (KPIs, equity, drawdown, trades)
    ui::algo::BacktestReportPanel* report_panel_ = nullptr;

    // Inline validation banner shown above the builder when a guard fails.
    QLabel* validation_banner_ = nullptr;
};

} // namespace fincept::screens
