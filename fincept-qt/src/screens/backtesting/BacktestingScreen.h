// src/screens/backtesting/BacktestingScreen.h
#pragma once
#include "services/backtesting/BacktestingTypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QHideEvent>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Multi-provider backtesting terminal with 6 providers, 9 commands, 50+ strategies.
class BacktestingScreen : public QWidget {
    Q_OBJECT
  public:
    explicit BacktestingScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_provider_changed(int index);
    void on_command_changed(int index);
    void on_strategy_changed(int index);
    void on_run();
    void on_result(const QString& provider, const QString& command, const QJsonObject& data);
    void on_error(const QString& context, const QString& message);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_left_panel();
    QWidget* build_center_panel();
    QWidget* build_right_panel();
    QWidget* build_status_bar();
    void connect_service();
    void update_provider_buttons();
    void update_command_buttons();
    void populate_strategies();
    void rebuild_strategy_params();
    void display_result(const QJsonObject& data);
    void display_error(const QString& msg);
    void clear_results();
    QJsonObject gather_args();
    QJsonObject gather_strategy_params();

    // Provider state
    QVector<services::backtest::Provider> providers_;
    QVector<services::backtest::Strategy> strategies_;
    int active_provider_ = 0;
    int active_command_ = 0;

    // Top bar
    QVector<QPushButton*> provider_buttons_;
    QPushButton* run_button_ = nullptr;
    QLabel* status_dot_ = nullptr;

    // Left panel - commands + strategies
    QVector<QPushButton*> command_buttons_;
    QComboBox* strategy_category_combo_ = nullptr;
    QComboBox* strategy_combo_ = nullptr;
    QWidget* strategy_params_container_ = nullptr;
    QVBoxLayout* strategy_params_layout_ = nullptr;
    QVector<QDoubleSpinBox*> param_spinboxes_;

    // Right panel - config
    QLineEdit* symbols_edit_ = nullptr;
    QDoubleSpinBox* capital_spin_ = nullptr;
    QDateEdit* start_date_ = nullptr;
    QDateEdit* end_date_ = nullptr;
    QDoubleSpinBox* commission_spin_ = nullptr;
    QDoubleSpinBox* slippage_spin_ = nullptr;
    QDoubleSpinBox* leverage_spin_ = nullptr;
    QDoubleSpinBox* stop_loss_spin_ = nullptr;
    QDoubleSpinBox* take_profit_spin_ = nullptr;
    QComboBox* pos_sizing_combo_ = nullptr;
    QCheckBox* allow_short_check_ = nullptr;
    QLineEdit* benchmark_edit_ = nullptr;

    // Right panel - command-specific config (stacked)
    QStackedWidget* cmd_config_stack_ = nullptr;
    // Optimize config
    QComboBox* opt_objective_combo_ = nullptr;
    QComboBox* opt_method_combo_ = nullptr;
    QSpinBox* opt_iterations_spin_ = nullptr;
    // Walk-forward config
    QSpinBox* wf_splits_spin_ = nullptr;
    QDoubleSpinBox* wf_train_ratio_spin_ = nullptr;
    // Indicator config
    QComboBox* indicator_combo_ = nullptr;
    // Indicator signals config
    QComboBox* ind_signal_mode_combo_ = nullptr;
    // Labels config
    QComboBox* labels_type_combo_ = nullptr;
    QSpinBox* labels_horizon_spin_ = nullptr;
    QDoubleSpinBox* labels_threshold_spin_ = nullptr;
    // Splitters config
    QComboBox* splitter_type_combo_ = nullptr;
    QSpinBox* splitter_window_spin_ = nullptr;
    QSpinBox* splitter_step_spin_ = nullptr;
    // Returns config
    QComboBox* returns_type_combo_ = nullptr;
    QSpinBox* returns_window_spin_ = nullptr;
    // Signals config
    QComboBox* signal_gen_combo_ = nullptr;

    // Center - results
    QTabWidget* result_tabs_ = nullptr;
    QVBoxLayout* summary_layout_ = nullptr;
    QWidget* summary_container_ = nullptr;
    QTableWidget* metrics_table_ = nullptr;
    QTableWidget* trades_table_ = nullptr;
    QTextEdit* raw_json_edit_ = nullptr;
    QLabel* status_label_ = nullptr;

    bool first_show_ = true;
    bool is_running_ = false;
};

} // namespace fincept::screens
