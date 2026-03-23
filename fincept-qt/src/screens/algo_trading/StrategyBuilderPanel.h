// src/screens/algo_trading/StrategyBuilderPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Strategy builder — condition-based entry/exit rule editor with backtest.
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
    QWidget* build_condition_row(QWidget* parent);
    QJsonArray gather_conditions();

    QLineEdit* name_edit_ = nullptr;
    QLineEdit* desc_edit_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QComboBox* entry_logic_combo_ = nullptr;
    QComboBox* exit_logic_combo_ = nullptr;
    QDoubleSpinBox* stop_loss_spin_ = nullptr;
    QDoubleSpinBox* take_profit_spin_ = nullptr;
    QDoubleSpinBox* trailing_stop_spin_ = nullptr;

    QVBoxLayout* entry_conditions_layout_ = nullptr;
    QVBoxLayout* exit_conditions_layout_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Backtest inputs
    QLineEdit* bt_symbol_ = nullptr;
    QDoubleSpinBox* bt_capital_ = nullptr;
};

} // namespace fincept::screens
