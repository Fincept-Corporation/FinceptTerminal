// src/screens/algo_trading/StrategyBuilderPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"
#include "ui/widgets/algo/ConditionSection.h"
#include "ui/widgets/algo/RiskManagementPanel.h"
#include "ui/widgets/algo/SymbolSearchCombo.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QWidget>

namespace fincept::screens {

class StrategyBuilderPanel : public QWidget {
    Q_OBJECT
public:
    explicit StrategyBuilderPanel(QWidget* parent = nullptr);

    QVariantMap save_draft() const;
    void restore_draft(const QVariantMap& draft);

private slots:
    void on_save();
    void on_backtest();
    void on_deploy();
    void on_backtest_result(const QJsonObject& data);
    void on_error(const QString& context, const QString& msg);

private:
    void build_ui();
    void connect_service();
    QWidget* build_top_toolbar();
    QWidget* build_left_panel();
    QWidget* build_right_panel();
    void display_backtest_result(const QJsonObject& data);
    void clear_results();

    // Top toolbar
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* desc_edit_ = nullptr;
    ui::algo::SymbolSearchCombo* symbol_combo_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* backtest_btn_ = nullptr;
    QPushButton* deploy_btn_ = nullptr;

    // Left panel — condition builder
    ui::algo::ConditionSection* entry_section_ = nullptr;
    ui::algo::ConditionSection* exit_section_ = nullptr;
    ui::algo::RiskManagementPanel* risk_panel_ = nullptr;

    // Right panel — backtest
    QDoubleSpinBox* bt_capital_ = nullptr;
    QLineEdit* bt_start_date_ = nullptr;
    QLineEdit* bt_end_date_ = nullptr;
    QLabel* status_label_ = nullptr;

    // KPI card labels
    QLabel* kpi_total_return_val_ = nullptr;
    QLabel* kpi_total_return_sub_ = nullptr;
    QLabel* kpi_sharpe_val_ = nullptr;
    QLabel* kpi_sharpe_sub_ = nullptr;
    QLabel* kpi_max_dd_val_ = nullptr;
    QLabel* kpi_max_dd_sub_ = nullptr;
    QLabel* kpi_win_rate_val_ = nullptr;
    QLabel* kpi_win_rate_sub_ = nullptr;
    QLabel* kpi_trades_val_ = nullptr;
    QLabel* kpi_trades_sub_ = nullptr;
    QLabel* kpi_profit_factor_val_ = nullptr;
    QLabel* kpi_profit_factor_sub_ = nullptr;
    QWidget* kpi_grid_widget_ = nullptr;
};

} // namespace fincept::screens
