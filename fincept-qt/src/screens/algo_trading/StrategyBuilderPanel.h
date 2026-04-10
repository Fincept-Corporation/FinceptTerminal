// src/screens/algo_trading/StrategyBuilderPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Strategy builder — QSplitter workstation layout.
/// Left: strategy editor (definition + conditions + risk).
/// Right: backtest params + Quant Lab-style KPI result cards.
class StrategyBuilderPanel : public QWidget {
    Q_OBJECT
  public:
    explicit StrategyBuilderPanel(QWidget* parent = nullptr);

  private slots:
    void on_save();
    void on_backtest();
    void on_backtest_result(const QJsonObject& data);
    void on_error(const QString& context, const QString& msg);

  private:
    void build_ui();
    void connect_service();
    QWidget* build_left_pane();
    QWidget* build_right_pane();
    QWidget* build_condition_row(QWidget* parent);
    void display_backtest_result(const QJsonObject& data);
    void clear_results();

    // Left pane — strategy editor
    QLineEdit*       name_edit_               = nullptr;
    QLineEdit*       desc_edit_               = nullptr;
    QComboBox*       timeframe_combo_         = nullptr;
    QComboBox*       entry_logic_combo_       = nullptr;
    QComboBox*       exit_logic_combo_        = nullptr;
    QDoubleSpinBox*  stop_loss_spin_          = nullptr;
    QDoubleSpinBox*  take_profit_spin_        = nullptr;
    QDoubleSpinBox*  trailing_stop_spin_      = nullptr;
    QVBoxLayout*     entry_conditions_layout_ = nullptr;
    QVBoxLayout*     exit_conditions_layout_  = nullptr;

    // Right pane — backtest
    QLineEdit*       bt_symbol_              = nullptr;
    QDoubleSpinBox*  bt_capital_             = nullptr;
    QLineEdit*       bt_start_date_          = nullptr;
    QLineEdit*       bt_end_date_            = nullptr;
    QLabel*          status_label_           = nullptr;
    QVBoxLayout*     results_layout_         = nullptr;
    QLabel*          bt_empty_label_         = nullptr;

    // KPI card value labels — updated in display_backtest_result()
    QLabel* kpi_total_return_val_  = nullptr;
    QLabel* kpi_total_return_sub_  = nullptr;
    QLabel* kpi_sharpe_val_        = nullptr;
    QLabel* kpi_sharpe_sub_        = nullptr;
    QLabel* kpi_max_dd_val_        = nullptr;
    QLabel* kpi_max_dd_sub_        = nullptr;
    QLabel* kpi_win_rate_val_      = nullptr;
    QLabel* kpi_win_rate_sub_      = nullptr;
    QLabel* kpi_trades_val_        = nullptr;
    QLabel* kpi_trades_sub_        = nullptr;
    QLabel* kpi_profit_factor_val_ = nullptr;
    QLabel* kpi_profit_factor_sub_ = nullptr;
    QWidget* kpi_grid_widget_      = nullptr; // hidden until first result
};

} // namespace fincept::screens
