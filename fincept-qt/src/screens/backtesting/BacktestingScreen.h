// src/screens/backtesting/BacktestingScreen.h
//
// The Backtesting tab: "Portfolio for a simulated account". Bands, top to bottom:
//   ui::ScreenCommandBar   BACKTEST · ENGINE ▾ · STRATEGY (F4) · 1Y 3Y 5Y 10Y MAX · PIN · EXPORT ▾ · SETUP · RUN
//   progress line          determinate when the engine streams done / total, else indeterminate
//   BtInputStrip           symbols · period · bars · capital · benchmark │ run state · warnings
//   results ribbon         10 KPI tiles of the ACTIVE backtest (ui::KpiStrip), click → the tab that explains it
//   SETUP panel │ tabs     OVERVIEW PERFORMANCE RISK TRADES EXPOSURE COSTS │ OPTIMISE WALK-FWD MONTE CARLO │
//                          COMPARE ASSUMPTIONS RUNS RESEARCH (pages built on first activation)
//   ui::ScreenStatusBar    engine + implementation + version · data source · bars · fill · costs · rf · ccy · clock
//
// No empty screens: the run tabs share ONE ui::AutoLoadPane (AutoRun) whose remembered result is the newest
// saved backtest (run store) — shown at once with "as of"; on a fresh profile the default backtest (locale
// index, SMA crossover 20/50, 5 years, venue cost preset) runs by itself behind a skeleton. OPTIMISE /
// WALK-FWD / MONTE CARLO never run by themselves (remembered result or designed preview). Every result states
// its engine, implementation, fill model, cost model, risk-free source and the data range actually used.
#pragma once

#include "core/symbol/IGroupLinked.h"
#include "screens/backtesting/BtHost.h"
#include "screens/backtesting/BtRunController.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/backtesting/BacktestingTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QVector>
#include <QWidget>

class QLabel;
class QMenu;
class QProgressBar;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QTimer;

namespace fincept::ui {
class AutoLoadPane;
class InputMemory;
class KpiStrip;
class ScreenCommandBar;
class ScreenStatusBar;
} // namespace fincept::ui

namespace fincept::screens {

namespace bt {
class BtAnalysisTab;
class BtAssumptionsView;
class BtCompareTab;
class BtInputStrip;
class BtResearchTab;
class BtRunView;
class BtRunsTab;
class BtSelection;
class BtSetupPanel;
} // namespace bt

class BacktestingScreen : public QWidget,
                          public IStatefulScreen,
                          public IGroupLinked,
                          public bt::BtHost,
                          public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit BacktestingScreen(QWidget* parent = nullptr);
    ~BacktestingScreen() override;

    // IStatefulScreen (v2: layout state only; inputs live in ui::InputMemory)
    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "backtesting"; }
    int state_version() const override { return 2; }

    // IGroupLinked — the first symbol is the screen's ticker. A group symbol replaces the symbols and marks
    // the result "Inputs changed"; it never starts a run by itself.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

    // bt::BtHost
    services::backtest::BtSetup current_setup() const override;
    QString setup_problem() const override;
    QVector<services::backtest::StrategyParam> strategy_params() const override;
    QString engine_slug() const override { return engine_; }
    QString engine_label() const override;
    bool engine_offers(const QString& command) const override;
    bt::BtRunController* controller() const override { return controller_; }
    services::backtest::BtRunPtr active_run() const override;
    QString setup_currency() const override;
    void apply_params(const QJsonObject& params) override;
    void run_backtest_with(const QJsonObject& params) override;
    void open_saved_run(qint64 store_id) override;
    void load_saved_inputs(qint64 store_id, bool run) override;
    void notify(const QString& text) override;

    // ISectionLinked — a tab (overview performance risk trades exposure costs optimize walk_forward
    // monte_carlo compare assumptions runs research), "engine:<slug>", "strategy:<id or name>",
    // "run:<store id>" (opens the saved run, never re-runs it). focus "strategy:<text>" (any section
    // or "") selects the best-matching strategy of the engine. Selecting never starts a run.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    enum Tab {
        TabOverview,
        TabPerformance,
        TabRisk,
        TabTrades,
        TabExposure,
        TabCosts,
        TabOptimize,
        TabWalkForward,
        TabMonteCarlo,
        TabCompare,
        TabAssumptions,
        TabRuns,
        TabResearch,
        TabCount
    };

    void refresh_theme();
    void build_ui();
    void build_command_bar();
    void build_ribbon();
    void build_status_bar();
    void build_run_pane();
    void install_shortcuts();
    void retranslate();

    // engine / catalogue
    void set_engine(const QString& slug);
    /// Gate the inputs and the ENGINE menu tooltips by each engine's get_capabilities (else the built-in table).
    void apply_engine_support();
    void on_catalog(const QString& provider, const QJsonObject& data);
    void on_catalog_failed(const QString& context, const QString& message);
    /// A deep link's strategy (id, name or words): selected in the setup once the catalogue is in.
    void select_linked_strategy(const QString& text);

    // run pane
    void start_run_pane(quint64 pane_request);
    void render_run_ticket(const QJsonValue& payload);
    void set_active(const bt::BtBundlePtr& bundle);
    void show_active_in_visible_view();
    QString backtest_signature() const;
    void schedule_signature();
    void update_signature();
    void update_ribbon();
    void update_status_bar();
    void update_run_state();
    void update_tab_badges();
    void apply_regional_defaults();
    void apply_portfolio_config(const QJsonObject& config);

    // tabs
    void on_tab_changed(int index);
    QWidget* page_for(int tab);
    bt::BtRunView* run_view(int tab);
    bool is_run_tab(int tab) const;
    void run_primary();
    void cancel_current();
    bt::BtAnalysisTab* analysis_tab(int tab) const;

    // actions
    void pin_active_run();
    void export_kind(const QString& kind);
    void show_raw_json();
    bool write_file(const QString& suggested, const QString& filter, const QByteArray& bytes);

    // services
    bt::BtRunController* controller_ = nullptr;
    bt::BtSelection* selection_ = nullptr;
    ui::InputMemory* memory_ = nullptr; ///< "setup"

    // bands
    ui::ScreenCommandBar* bar_ = nullptr;
    QMenu* engine_menu_ = nullptr;
    QMenu* export_menu_ = nullptr;
    QProgressBar* progress_ = nullptr;
    bt::BtInputStrip* strip_ = nullptr;
    QLabel* state_chip_ = nullptr;
    QLabel* as_of_label_ = nullptr;
    QPushButton* warnings_btn_ = nullptr;
    QWidget* ribbon_box_ = nullptr;
    ui::KpiStrip* ribbon_ = nullptr;
    QSplitter* splitter_ = nullptr;
    bt::BtSetupPanel* setup_ = nullptr;
    QTabBar* tabs_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    ui::ScreenStatusBar* status_ = nullptr;

    // pages
    QVector<QWidget*> page_slots_;           ///< outer stack page per tab (run tabs share one)
    ui::AutoLoadPane* run_pane_ = nullptr;
    QStackedWidget* run_stack_ = nullptr;    ///< inside the run pane: the run views
    QHash<int, bt::BtRunView*> run_views_;
    QHash<int, QWidget*> run_slots_;         ///< placeholder per run tab inside run_stack_
    QHash<int, quint64> view_shown_;         ///< tab -> generation of the run it shows
    quint64 active_generation_ = 0;
    bt::BtAnalysisTab* optimize_tab_ = nullptr;
    bt::BtAnalysisTab* walk_forward_tab_ = nullptr;
    bt::BtAnalysisTab* monte_carlo_tab_ = nullptr;
    bt::BtCompareTab* compare_tab_ = nullptr;
    bt::BtRunsTab* runs_tab_ = nullptr;
    bt::BtResearchTab* research_tab_ = nullptr;

    // state
    QString engine_ = QStringLiteral("vectorbt");
    QVector<services::backtest::Provider> providers_;
    QHash<QString, QJsonObject> engine_caps_; ///< slug -> get_capabilities reply (engines that answered)
    bt::BtBundlePtr active_;
    quint64 run_token_ = 0;
    quint64 run_request_ = 0;
    quint64 pending_start_ = 0;             ///< pane request waiting for the strategy list
    bool run_auto_ = false;
    qint64 pending_open_id_ = 0;            ///< OPEN of a saved run, delivered through the run pane
    QJsonObject pending_params_run_;        ///< RUN of an optimised set once applied
    bool pending_handoff_run_ = false;
    QJsonObject pending_restore_payload_;   ///< v1 session state result, migrated into the store
    QString pending_link_strategy_;         ///< deep-linked strategy waiting for the catalogue
    QTimer* signature_timer_ = nullptr;
    QTimer* ticker_ = nullptr;
    bool first_show_ = true;
    int restored_tab_ = TabOverview;
    QList<int> restored_sizes_;
    bool setup_collapsed_ = false;
    SymbolGroup link_group_ = SymbolGroup::None;
    QString last_stylesheet_;
    QString run_button_text_;    ///< last text set on the bar's RUN action (set only on change)
    bool ribbon_compact_ = false;
};

} // namespace fincept::screens
