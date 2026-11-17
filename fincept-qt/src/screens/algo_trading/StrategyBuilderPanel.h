// src/screens/algo_trading/StrategyBuilderPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"
#include "services/options/OptionChainTypes.h"
#include "ui/widgets/algo/BacktestReportPanel.h"
#include "ui/widgets/algo/ExecutionCostPanel.h"
#include "ui/widgets/algo/RiskManagementPanel.h"
#include "ui/widgets/algo/RuleTableEditor.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QString>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QResizeEvent;

// Forward-declare F&O widgets to avoid pulling heavy headers into every TU
// that includes this header.
namespace fincept::ui::algo {
class FnoLegRuleEditor;
}
namespace fincept::ui {
class PanelHeader;
class ScreenCommandBar;
class ScreenStatusBar;
class SymbolSearchBox;
} // namespace fincept::ui
namespace fincept::screens::fno {
class PayoffChartWidget;
class BuilderAnalyticsRibbon;
} // namespace fincept::screens::fno

namespace fincept::screens {

/// BUILDER sub-tab — one workbench, three zones under the command bar:
///   command bar   ALGO [1 BUILDER|…]  OPTIMISE  SAVE AS  SAVE  DEPLOY (PAPER)  RUN BACKTEST
///   header strip  NAME  DESC  INSTRUMENT  TYPE  TF  SIDE  CAPITAL  PERIOD [1Y|3Y|5Y|MAX]  state chip
///   templates     one-click rule sets (Golden Cross, RSI, MACD, Bollinger, SuperTrend, Donchian …)
///   left  (definition, scrolls): ENTRY RULES / EXIT RULES (rule tables, side by side when wide),
///         RISK & SIZING (compact fields), EXECUTION & COSTS (one summary line → editor),
///         OPTION LEGS for option / future strategies
///   right (result): the backtest report (KPI ribbon, equity vs buy & hold with drawdown and
///         trade markers, TRADES · MONTHLY · COSTS · ASSUMPTIONS / WARNINGS …) — or the payoff
///         preview of an option strategy
///   status bar    engine · data source · what is defined · as of · state
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
    // Sets the backtest symbol (e.g. from a scanner result).
    void set_backtest_symbol(const QString& symbol);
    /// The symbol a draft with none is first backtested on: a broad, liquid fund of the
    /// market the user's number-format locale points to, else SPY.
    static QString default_backtest_symbol();

  signals:
    // Emitted after a strategy is successfully deployed, so the parent screen can
    // switch to the Dashboard tab and refresh the deployment list.
    void deployed();
    /// A backtest result was remembered for this draft (save_draft() changed): the
    /// parent screen persists its state.
    void backtest_remembered();

  protected:
    void changeEvent(QEvent* event) override;
    // First show with nothing on the result zone runs the default backtest (the restored
    // draft, else the Golden Cross template on the locale's default symbol) — automatic runs
    // pass the app-wide AutoLoadGate and are cancelled on hide.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private slots:
    void on_save();
    void on_save_as();
    void on_backtest();
    void on_optimize();
    void on_deploy();
    void on_backtest_result(const QJsonObject& data);
    void on_error(const QString& context, const QString& msg);

  private: // NOLINT(readability-redundant-access-specifiers) — needed to end private slots: section
    void build_ui();
    void build_command_bar();
    QWidget* build_header_strip();
    QWidget* build_templates_strip();
    QWidget* build_definition_zone(QWidget* parent);
    QWidget* build_result_zone(QWidget* parent);
    void install_shortcuts();
    void retranslateUi();
    void connect_service();
    // ── Default run on first show (class B: Yahoo bars + the native engine, ~1-4 s) ──
    /// Decide on the next event-loop turn (after a restored draft is applied).
    void schedule_default_backtest();
    /// Seeds a blank draft (Golden Cross template, locale default symbol, the template's
    /// look-back) and runs it through the gate when the result zone is empty.
    void run_default_backtest();
    /// Validates and starts a run; `automatic` = the default run (gate slot held).
    void start_backtest(bool automatic);
    /// Releases the gate slot / queue place of the automatic run, if any.
    void release_auto_slot();
    /// Inputs of a backtest (rules + symbol + range + capital) — one auto run per visit and inputs.
    QString backtest_inputs_key();
    // ── Remembered results (one per draft, persisted with the draft) ──
    /// The draft a result belongs to: the strategy id (minted on the first run / save
    /// and kept in the draft), else its name.
    QString draft_memory_key() const;
    /// Keeps `payload` (the result of the run described by pending_memory_entry_) for
    /// its draft; serialised off the GUI thread.
    void remember_backtest(const QJsonObject& payload);
    /// Shows this draft's remembered result instead of running (see the .cpp). True when one
    /// is being shown.
    bool offer_remembered_backtest(bool same_inputs);
    void display_backtest_result(const QJsonObject& data);
    void clear_results();
    // The backtest symbol changed: suggest the venue's cost preset (until the user
    // picks one) and show capital in the instrument's currency.
    void on_symbol_changed();
    /// The instrument as typed / picked ("SPY", "RELIANCE.NS").
    QString symbol_text() const;
    void set_symbol_text(const QString& symbol);
    // Builds an AlgoStrategy from the current UI state, reusing the same id for
    // the lifetime of this editor so repeated Save/Backtest/Deploy update one
    // strategy instead of spawning duplicates.
    services::algo::AlgoStrategy build_strategy();
    /// Fill the rules, timeframe and risk from template `index` (algo_strategy_templates()).
    void load_template(int index);
    // Returns a human-readable error if the strategy is unrunnable, else empty.
    QString validate() const;
    /// Live feedback after an edit: the validation note, the summary lines, the status
    /// bar's "info", the template chips' active look.
    void refresh_definition_feedback();
    /// Show / hide the validation note (left zone) with `text`.
    void show_validation(const QString& text);
    /// A transient message in the status bar.
    void notify(const QString& text, int tone);
    /// RUN BACKTEST ⇄ CANCEL on the command bar.
    void set_run_busy(bool busy);
    /// Entry and exit tables side by side (wide) or stacked.
    void apply_rules_arrangement();
    void apply_range_preset(int years);
    QString data_source_text() const;

    // F&O section builder + helpers
    void build_fno_section(QWidget* left_parent, QWidget* right_parent);
    void populate_underlyings();
    void refresh_fno_preview();
    void on_instrument_type_changed();
    QString fno_broker_id() const;
    // Expiry rule chosen in the F&O header (stamped on every leg by build_strategy):
    // mode is the engine token (WEEKLY|MONTHLY|NEAREST_DTE|ABSOLUTE), value the
    // min-DTE / "DD-MMM-YY" for the last two.
    QString expiry_mode() const;
    QString expiry_value() const;
    // Human-readable rule + the expiry it resolves to today (deploy dialog / LIVE confirm).
    QString expiry_rule_label() const;
    // Shows the DTE / absolute-expiry input for the selected rule (refills the expiry list).
    void sync_expiry_inputs();
    void apply_expiry_rule(const QString& mode, const QString& value);
    // Subscribes the option chain of the SELECTED underlying + resolved expiry so the
    // payoff preview follows the combos (not whichever chain was published last).
    void ensure_preview_chain();
    void release_preview_chain();

    // Bars
    ui::ScreenCommandBar* bar_ = nullptr;
    ui::ScreenStatusBar* status_ = nullptr;

    // Header strip
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* desc_edit_ = nullptr;
    ui::SymbolSearchBox* symbol_box_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QComboBox* instrument_type_combo_ = nullptr;
    QDoubleSpinBox* bt_capital_ = nullptr;
    QDateEdit* bt_start_date_ = nullptr;
    QDateEdit* bt_end_date_ = nullptr;
    QWidget* range_seg_ = nullptr;
    QWidget* equity_only_fields_ = nullptr; ///< capital + period (hidden for option / future)
    QLabel* state_chip_ = nullptr;
    QVector<QPushButton*> template_buttons_;
    QLabel* templates_caption_ = nullptr;

    // Definition zone
    QWidget* rules_box_ = nullptr;
    QGridLayout* rules_grid_ = nullptr;
    QWidget* entry_panel_ = nullptr;
    QWidget* exit_panel_ = nullptr;
    QWidget* risk_box_ = nullptr;
    QWidget* exec_box_ = nullptr;
    ui::PanelHeader* entry_header_ = nullptr;
    ui::PanelHeader* exit_header_ = nullptr;
    ui::PanelHeader* risk_header_ = nullptr;
    ui::PanelHeader* exec_header_ = nullptr;
    bool rules_side_by_side_ = false;
    ui::algo::RuleTableEditor* entry_rules_ = nullptr;
    ui::algo::RuleTableEditor* exit_rules_ = nullptr;
    ui::algo::RiskManagementPanel* risk_panel_ = nullptr;
    ui::algo::ExecutionCostPanel* cost_panel_ = nullptr;
    QLabel* exec_summary_ = nullptr;
    QPushButton* exec_toggle_ = nullptr;
    QLabel* validation_note_ = nullptr;
    QSplitter* splitter_ = nullptr;
    bool splitter_sized_ = false;

    // Identity of the strategy currently being edited (empty => not yet minted).
    QString loaded_strategy_id_;
    // Persisted last-backtest summary of the loaded strategy (shown at deploy time
    // when no backtest ran in this session).
    QJsonObject loaded_last_backtest_;
    // Rules/costs fingerprint of the backtest in flight and of the one on screen:
    // Save attaches the on-screen result only while the definition still matches.
    QString pending_backtest_def_;
    QString shown_backtest_def_;

    // Re-entrancy guard for on_deploy(). The deploy path opens a modal dialog and then
    // continues after an asynchronous broker probe, during which a second click on DEPLOY
    // would otherwise re-enter and start a second deployment of the same strategy.
    bool deploying_ = false;

    // Result zone — the backtest report (KPIs, equity, drawdown, trades)
    ui::algo::BacktestReportPanel* report_panel_ = nullptr;

    // Default-run bookkeeping.
    bool default_scheduled_ = false; ///< a run_default_backtest() is queued for the next turn
    bool run_in_flight_ = false;     ///< a backtest (automatic or not) is running
    bool auto_run_ = false;          ///< the run in flight / queued is the automatic default run
    bool auto_queued_ = false;       ///< waiting for an AutoLoadGate slot
    quint64 auto_ticket_ = 0;        ///< AutoLoadGate ticket (0 = none)
    quint64 visit_ = 0;              ///< bumped on every show
    quint64 auto_visit_ = 0;         ///< visit of the last automatic attempt
    QString auto_inputs_;            ///< inputs of the last automatic attempt

    // Remembered results: draft key → {def, inputs, symbol, start, end, capital,
    // ran_at (ISO UTC), duration_ms, payload (compact JSON)}; saved in the draft.
    QVariantMap backtest_memory_;
    /// The run in flight: its draft key ("key") and inputs (def, inputs, symbol, start,
    /// end, capital) — what remember_backtest() files its result under.
    QVariantMap pending_memory_entry_;
    bool remembered_loading_ = false; ///< a remembered payload is being parsed
    quint64 memory_token_ = 0;        ///< bumped to drop a parse that no longer applies

    // F&O section (hidden for Equity instrument type)
    QWidget* fno_section_ = nullptr; ///< left zone: underlying, expiry rule, legs
    QWidget* fno_result_ = nullptr;  ///< right zone: payoff preview
    QComboBox* underlying_combo_ = nullptr;
    QComboBox* expiry_mode_combo_ = nullptr;
    QSpinBox* expiry_dte_spin_ = nullptr;   // NEAREST_DTE: minimum days to expiry
    QComboBox* expiry_abs_combo_ = nullptr; // ABSOLUTE: concrete expiry
    QLabel* fno_preview_note_ = nullptr;    // "Loading NIFTY 27-NOV-26 chain…" etc.
    QString preview_topic_;
    QString preview_broker_;
    QString preview_underlying_;
    QString preview_expiry_;
    fincept::services::options::OptionChain preview_chain_;
    fincept::ui::algo::FnoLegRuleEditor* leg_editor_ = nullptr;
    fincept::screens::fno::PayoffChartWidget* payoff_chart_ = nullptr;
    fincept::screens::fno::BuilderAnalyticsRibbon* analytics_ribbon_ = nullptr;
};

} // namespace fincept::screens
