// src/screens/backtesting/BacktestingScreen.h
#pragma once
#include "core/symbol/IGroupLinked.h"
#include "screens/IStatefulScreen.h"
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
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Multi-provider backtesting terminal with 6 providers, 9 commands, 50+ strategies.
class BacktestingScreen : public QWidget, public IStatefulScreen, public IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit BacktestingScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "backtesting"; }
    int state_version() const override { return 1; }

    // IGroupLinked — backtest is multi-symbol, but the *first* symbol in the
    // line edit acts as the active ticker for group linking. Receiving a
    // group symbol overwrites the entire field with that one symbol; users
    // who want a multi-symbol backtest add more after publishing.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

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
    void on_command_options_loaded(const QString& provider, const QJsonObject& options);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_left_panel();
    QWidget* build_center_panel();
    QWidget* build_right_panel();
    QWidget* build_status_bar();
    void connect_service();
    void set_status_state(const QString& text, const QString& color, const QString& bg_rgba);
    void update_provider_buttons();
    void update_command_buttons();
    void populate_strategies();
    void rebuild_strategy_params();
    void display_result(const QString& command, const QJsonObject& data);
    /// Toggle the right/left-panel section containers based on the active command.
    /// Each command consumes a different subset of the global config (e.g. ML
    /// Labels doesn't use commission/slippage/leverage/benchmark/strategy).
    void update_section_visibility();
    void display_error(const QString& msg);
    void clear_results();
    QJsonObject gather_args();
    QJsonObject gather_strategy_params();

    // Provider state (cached to avoid repeated heap allocation)
    QVector<services::backtest::Provider> providers_;
    QVector<services::backtest::CommandDef> commands_;
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
    // Per-param explicit-range editor (one trio per strategy param). Always built
    // alongside param_spinboxes_, but only made visible when the active command
    // is `optimize`. Indexed in lockstep with param_spinboxes_.
    QVector<QDoubleSpinBox*> param_min_spinboxes_;
    QVector<QDoubleSpinBox*> param_max_spinboxes_;
    QVector<QDoubleSpinBox*> param_step_spinboxes_;
    // Wrapper widgets for each (min/max/step) row so we toggle them as a group.
    QVector<QWidget*> param_range_rows_;

    // Right panel - section containers (toggled per-command).
    // Each wraps a labeled visual block (title + inputs) so the whole
    // section can be hidden when the active command doesn't consume it.
    QWidget* market_data_section_ = nullptr;
    QWidget* execution_section_ = nullptr;
    QWidget* advanced_section_ = nullptr;
    QWidget* benchmark_section_ = nullptr;
    // Left panel strategy block — only meaningful for backtest/optimize/walk_forward.
    QWidget* strategy_section_ = nullptr;

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
    QSpinBox* indicator_period_spin_ = nullptr;
    // Indicator signals config — params shown depend on the chosen mode:
    //   crossover_signals: fast_period + slow_period + ma_type
    //   threshold_signals: period + lower + upper
    //   breakout_signals:  channel + period
    //   mean_reversion_signals: period + z_entry + z_exit
    //   signal_filter:     base_indicator + base_period + filter_indicator
    //                       + filter_period + filter_threshold + filter_type
    QComboBox* ind_signal_indicator_combo_ = nullptr;
    QComboBox* ind_signal_mode_combo_ = nullptr;
    // crossover_signals
    QWidget* is_fast_period_row_ = nullptr;
    QSpinBox* is_fast_period_spin_ = nullptr;
    QWidget* is_slow_period_row_ = nullptr;
    QSpinBox* is_slow_period_spin_ = nullptr;
    QWidget* is_ma_type_row_ = nullptr;
    QComboBox* is_ma_type_combo_ = nullptr;
    // threshold_signals
    QWidget* is_period_row_ = nullptr;
    QSpinBox* is_period_spin_ = nullptr;
    QWidget* is_lower_row_ = nullptr;
    QDoubleSpinBox* is_lower_spin_ = nullptr;
    QWidget* is_upper_row_ = nullptr;
    QDoubleSpinBox* is_upper_spin_ = nullptr;
    // breakout_signals
    QWidget* is_channel_row_ = nullptr;
    QComboBox* is_channel_combo_ = nullptr;
    // mean_reversion_signals
    QWidget* is_z_entry_row_ = nullptr;
    QDoubleSpinBox* is_z_entry_spin_ = nullptr;
    QWidget* is_z_exit_row_ = nullptr;
    QDoubleSpinBox* is_z_exit_spin_ = nullptr;
    // signal_filter
    QWidget* is_filter_indicator_row_ = nullptr;
    QComboBox* is_filter_indicator_combo_ = nullptr;
    QWidget* is_filter_period_row_ = nullptr;
    QSpinBox* is_filter_period_spin_ = nullptr;
    QWidget* is_filter_threshold_row_ = nullptr;
    QDoubleSpinBox* is_filter_threshold_spin_ = nullptr;
    QWidget* is_filter_type_row_ = nullptr;
    QComboBox* is_filter_type_combo_ = nullptr;
    // Labels config — different label generators use different params:
    //   FIXLB:   horizon, threshold (forward-return classification)
    //   MEANLB:  window, threshold (z-score mean reversion)
    //   LEXLB:   window only (local extrema)
    //   TRENDLB: window, threshold (rolling regression slope)
    //   BOLB:    window, alpha     (Bollinger Band zones)
    // Each row is wrapped in a container so labels_type_combo's change handler
    // can show/hide rows that don't apply to the chosen generator.
    QComboBox* labels_type_combo_ = nullptr;
    QWidget* labels_horizon_row_ = nullptr;
    QSpinBox* labels_horizon_spin_ = nullptr;
    QWidget* labels_window_row_ = nullptr;
    QSpinBox* labels_window_spin_ = nullptr;
    QWidget* labels_threshold_row_ = nullptr;
    QDoubleSpinBox* labels_threshold_spin_ = nullptr;
    QWidget* labels_alpha_row_ = nullptr;
    QDoubleSpinBox* labels_alpha_spin_ = nullptr;
    // Splitters config — different splitters consume different params:
    //   RollingSplitter:     window_len + test_len + step
    //   ExpandingSplitter:   min_len + test_len + step
    //   PurgedKFoldSplitter: n_splits + purge_len + embargo_len
    QComboBox* splitter_type_combo_ = nullptr;
    QWidget* splitter_window_row_ = nullptr;
    QSpinBox* splitter_window_spin_ = nullptr;
    QWidget* splitter_min_row_ = nullptr;
    QSpinBox* splitter_min_spin_ = nullptr;
    QWidget* splitter_test_row_ = nullptr;
    QSpinBox* splitter_test_spin_ = nullptr;
    QWidget* splitter_step_row_ = nullptr;
    QSpinBox* splitter_step_spin_ = nullptr;
    QWidget* splitter_n_splits_row_ = nullptr;
    QSpinBox* splitter_n_splits_spin_ = nullptr;
    QWidget* splitter_purge_row_ = nullptr;
    QSpinBox* splitter_purge_spin_ = nullptr;
    QWidget* splitter_embargo_row_ = nullptr;
    QSpinBox* splitter_embargo_spin_ = nullptr;
    // Returns config — different analysis types consume different params:
    //   returns_stats: (no extra knobs beyond benchmark + risk_free)
    //   drawdowns:     (no extra)
    //   ranges:        threshold
    //   rolling:       window + metric
    QComboBox* returns_type_combo_ = nullptr;
    QWidget* returns_window_row_ = nullptr;
    QSpinBox* returns_window_spin_ = nullptr;
    QWidget* returns_metric_row_ = nullptr;
    QComboBox* returns_metric_combo_ = nullptr;
    QWidget* returns_threshold_row_ = nullptr;
    QDoubleSpinBox* returns_threshold_spin_ = nullptr;
    // Signals config — params shown depend on the chosen generator:
    //   RAND:    n
    //   RANDX:   n
    //   RANDNX:  n + min_hold + max_hold
    //   RPROB:   entry_prob
    //   RPROBX:  entry_prob + exit_prob
    QComboBox* signal_gen_combo_ = nullptr;
    QWidget* signal_count_row_ = nullptr;
    QSpinBox* signal_count_spin_ = nullptr;
    QWidget* signal_entry_prob_row_ = nullptr;
    QDoubleSpinBox* signal_entry_prob_spin_ = nullptr;
    QWidget* signal_exit_prob_row_ = nullptr;
    QDoubleSpinBox* signal_exit_prob_spin_ = nullptr;
    QWidget* signal_min_hold_row_ = nullptr;
    QSpinBox* signal_min_hold_spin_ = nullptr;
    QWidget* signal_max_hold_row_ = nullptr;
    QSpinBox* signal_max_hold_spin_ = nullptr;
    // Labels-to-signals config
    QComboBox* l2s_label_type_combo_ = nullptr;
    QSpinBox* l2s_entry_label_spin_ = nullptr;
    QSpinBox* l2s_exit_label_spin_ = nullptr;
    // Indicator sweep config
    QComboBox* sweep_indicator_combo_ = nullptr;
    QSpinBox* sweep_min_spin_ = nullptr;
    QSpinBox* sweep_max_spin_ = nullptr;
    QSpinBox* sweep_step_spin_ = nullptr;

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

    // Symbol-group link (None when unlinked).
    SymbolGroup link_group_ = SymbolGroup::None;
    void publish_first_symbol_to_group();
};

} // namespace fincept::screens
