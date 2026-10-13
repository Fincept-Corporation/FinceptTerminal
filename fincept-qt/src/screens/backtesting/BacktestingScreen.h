// src/screens/backtesting/BacktestingScreen.h
#pragma once
#include "core/symbol/IGroupLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/backtesting/BacktestingTypes.h"
#include "ui/tables/LiveTableModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// One row of the DETAILS table (trades, optimisation runs, walk-forward windows,
/// signals ...). `index` is the row's position in the provider payload - its
/// stable key for the model.
struct BtDetailRow {
    int index = 0;
    QJsonObject rec;
};

/// Multi-provider backtesting terminal (6 providers, 11 commands).
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
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_provider_changed(int index);
    void on_command_changed(int index);
    void on_strategy_changed(int index);
    void on_run();
    void on_cancel();
    // Catalogue loads (get_strategies / get_indicators) arrive on the legacy signals.
    void on_result(const QString& provider, const QString& command, const QJsonObject& data);
    void on_error(const QString& context, const QString& message);
    // This screen's runs, correlated by run id.
    void on_run_completed(quint64 run_id, const QString& provider, const QString& command, const QJsonObject& data);
    void on_run_failed(quint64 run_id, const QString& context, const QString& message);
    void on_run_cancelled(quint64 run_id, const QString& provider, const QString& command);
    void on_command_options_loaded(const QString& provider, const QJsonObject& options);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_left_panel();
    QWidget* build_center_panel();
    QWidget* build_right_panel();
    QWidget* build_status_bar();
    void connect_service();
    /// Rebuild the screen's single root stylesheet from the active theme tokens.
    /// Every widget is styled through an objectName / dynamic-property selector here
    /// instead of an inline setStyleSheet call.
    void refresh_theme();
    /// state: ready | running | done | error | cancelled | synthetic
    void set_status_state(const QString& text, const QString& state);
    /// Leave the running state (RUN enabled, CANCEL hidden, ticker stopped).
    void end_run();
    /// Fill the DETAILS model (all rows - no cap) with the given columns.
    void set_detail_rows(const QJsonArray& rows, const QStringList& columns);
    /// CSV exports of the DETAILS rows and of the equity curve. Both carry a
    /// "# ..." metadata header (provider, strategy, period, execution model) and an
    /// unmistakable SYNTHETIC warning when the run used synthetic prices.
    void export_details_csv();
    void export_equity_csv();
    QStringList export_header_lines() const;
    bool write_text_file(const QString& path, const QString& text, const QString& title);
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
    /// Re-apply tr() strings to cached structural widgets on QEvent::LanguageChange.
    void retranslateUi();

    // ── Cached static labels for retranslateUi ──────────────────────────────
    // Top bar
    QLabel* brand_label_ = nullptr;
    // Left panel section/field titles
    QLabel* commands_title_ = nullptr;
    QLabel* strategy_title_ = nullptr;
    QLabel* strategy_cat_label_ = nullptr;
    QLabel* strategy_pick_label_ = nullptr;
    QLabel* params_title_ = nullptr;
    // Center panel
    QLabel* results_title_ = nullptr;
    QPushButton* export_json_btn_ = nullptr;
    QPushButton* export_csv_btn_ = nullptr;
    QLabel* summary_hint_ = nullptr;
    QLabel* equity_hint_ = nullptr;
    // Right panel section/field titles
    QLabel* market_data_title_ = nullptr;
    QLabel* symbols_label_ = nullptr;
    QLabel* start_label_ = nullptr;
    QLabel* end_label_ = nullptr;
    QLabel* interval_label_ = nullptr;
    QLabel* execution_title_ = nullptr;
    QLabel* commission_label_ = nullptr;
    QLabel* slippage_label_ = nullptr;
    QLabel* risk_free_label_ = nullptr;
    QLabel* advanced_title_ = nullptr;
    QLabel* leverage_label_ = nullptr;
    QLabel* stop_loss_label_ = nullptr;
    QLabel* take_profit_label_ = nullptr;
    QLabel* pos_sizing_label_ = nullptr;
    QLabel* benchmark_title_ = nullptr;
    QLabel* benchmark_label_ = nullptr;
    // Status bar
    QLabel* providers_caption_ = nullptr;
    QLabel* strategies_caption_ = nullptr;

    // Provider state (cached to avoid repeated heap allocation)
    QVector<services::backtest::Provider> providers_;
    QVector<services::backtest::CommandDef> commands_;
    QVector<services::backtest::Strategy> strategies_;
    int active_provider_ = 0;
    int active_command_ = 0;

    // Top bar
    QVector<QPushButton*> provider_buttons_;
    QPushButton* run_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
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
    QComboBox* interval_combo_ = nullptr;
    QDoubleSpinBox* capital_spin_ = nullptr;
    QDateEdit* start_date_ = nullptr;
    QDateEdit* end_date_ = nullptr;
    QDoubleSpinBox* commission_spin_ = nullptr;
    QDoubleSpinBox* slippage_spin_ = nullptr;
    QDoubleSpinBox* risk_free_spin_ = nullptr;
    QDoubleSpinBox* leverage_spin_ = nullptr;
    QDoubleSpinBox* stop_loss_spin_ = nullptr;
    QDoubleSpinBox* take_profit_spin_ = nullptr;
    QComboBox* pos_sizing_combo_ = nullptr;
    QDoubleSpinBox* pos_sizing_value_spin_ = nullptr;
    QLabel* pos_sizing_value_label_ = nullptr;
    QCheckBox* allow_short_check_ = nullptr;
    // Off by default and never persisted: a run on random-walk prices must be an
    // explicit, per-session choice.
    QCheckBox* allow_synthetic_check_ = nullptr;
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
    QWidget* equity_chart_tab_ = nullptr;
    QVBoxLayout* summary_layout_ = nullptr;
    QWidget* summary_container_ = nullptr;
    QTableWidget* metrics_table_ = nullptr;
    // DETAILS: model/view so a 100k-trade run renders lazily and sorts numerically.
    QTableView* details_view_ = nullptr;
    ui::LiveTableModel<BtDetailRow>* details_model_ = nullptr;
    QSortFilterProxyModel* details_proxy_ = nullptr;
    QStringList details_cols_;
    QTextEdit* raw_json_edit_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* strategies_count_label_ = nullptr;
    QLabel* providers_count_label_ = nullptr;

    // Last completed result (full payload - the RAW JSON tab may show a capped
    // preview) plus what was asked for, for exports.
    QJsonObject last_payload_;
    QString last_command_;
    QJsonObject last_run_meta_;

    // Custom combo strategy builder (up to 3 indicators)
    struct ComboIndicatorRow {
        QComboBox* type = nullptr;
        QComboBox* cond = nullptr;
        QDoubleSpinBox* val = nullptr;
        QSpinBox* period = nullptr;
    };
    ComboIndicatorRow combo_rows_[3] = {};
    QComboBox* combo_logic_ = nullptr;
    QCheckBox* combo_ind3_enabled_ = nullptr;

    void apply_portfolio_config(const QJsonObject& config);
    /// Dispatch a run on the next event-loop tick. Used by the auto-run path so
    /// freshly-populated strategy combos/params settle before on_run() reads them.
    void trigger_auto_run();

    // Long-run feedback: ticks the status chip with elapsed seconds while a
    // command is in flight so a 3-minute walk-forward never looks frozen.
    // Started in on_run(), stopped on result/error/hide (P3).
    QTimer* run_timer_ = nullptr;
    QElapsedTimer run_elapsed_;
    void start_run_ticker();
    void stop_run_ticker();

    QJsonArray pending_weights_;
    bool first_show_ = true;
    bool is_running_ = false;
    // The in-flight run. Results are matched on current_run_id_ (BacktestingService
    // is a process-wide singleton that also serves Portfolio and workflow runs);
    // provider/command are kept for the cost disclosure and exports.
    QString running_provider_;
    QString running_command_;
    quint64 current_run_id_ = 0; // BacktestingService run id of the in-flight run (0 = none)
    // Set when a caller requests an immediate backtest but strategies are still
    // loading; the get_strategies callback fires the deferred run once ready.
    bool pending_auto_run_ = false;

    // Symbol-group link (None when unlinked).
    SymbolGroup link_group_ = SymbolGroup::None;
    void publish_first_symbol_to_group();
};

} // namespace fincept::screens
