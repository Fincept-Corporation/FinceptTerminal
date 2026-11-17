// src/screens/portfolio/views/PortfolioOptimizationView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAnalyticsService.h"

#include <QChartView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

class QHBoxLayout;

namespace fincept::ui {
class AutoLoadPane;
class BarChart;
class EmptyState;
class InputMemory;
class KpiStrip;
class MatrixHeatmap;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

class PortfolioSharedRun;

/// Portfolio Optimization view — 9 tabs.
/// OPTIMIZE: run Python optimizer (max_sharpe / min_vol / risk_parity / HRP etc.)
/// FRONTIER: efficient frontier scatter chart (real points from optimizer)
/// ALLOCATION: current holdings donut + table
/// STRATEGIES: comparison of 5 methods (same run)
/// COMPARE: side-by-side weights of all 5 methods (same run)
/// BACKTEST: buy-and-hold run of current / optimal weights (or hand-off to the Backtesting screen)
/// RISK: per-asset risk contribution from the optimizer's covariance matrix (same run)
/// STRESS: crisis shocks applied to the current asset-class mix (no optimization needed)
/// B-L MODEL: market-implied equilibrium returns + Black-Litterman weights (same run)
///
/// One optimiser run fills OPTIMIZE, FRONTIER, STRATEGIES, COMPARE, RISK and B-L:
/// each of those tabs has its own ui::AutoLoadPane, all routed to one
/// PortfolioSharedRun — the run starts on the first show of any of them (class B:
/// free, ~5 s), every tab opens on the last result at once ("as of"), and leaving
/// the view cancels it. BACKTEST has its own pane (current weights, 1 year by default).
class PortfolioOptimizationView : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioOptimizationView(QWidget* parent = nullptr);
    ~PortfolioOptimizationView() override;

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    /// Per-holding betas from the market-analytics run, so the STRESS tab moves each
    /// equity line by its own beta (same model as the RISK view).
    void set_analytics(const portfolio::MarketAnalytics& analytics);

    /// "Optimize for this return" hand-off from the Planning view: switch to the
    /// Target Return method at @p annual_return (fraction, e.g. 0.08) and run it.
    /// Ignored for non-positive targets (the plan is already funded).
    void set_target_return(double annual_return);

  signals:
    /// The BUY action of the "no positions" state (the screen opens Add Asset).
    void add_position_requested();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    /// The six tabs one optimiser run fills.
    enum ResultTab { kOptimizeTab = 0, kFrontierTab, kStrategiesTab, kCompareTab, kRiskTab, kBlTab, kResultTabCount };

    void build_ui();
    void retranslateUi();
    /// Texts of the empty / preview states and the loading captions (build + language change).
    void apply_state_texts();
    /// Re-renders the tables and charts from the data on screen (they carry their colours).
    void refresh_theme();

    // Tab builders
    QWidget* build_optimize_tab();
    QWidget* build_frontier_tab();
    QWidget* build_backtest_tab();
    QWidget* build_allocation_tab();
    QWidget* build_risk_tab();
    QWidget* build_strategies_tab();
    QWidget* build_stress_tab();
    QWidget* build_compare_tab();
    QWidget* build_black_litterman_tab();

    /// Title + "as of" header row of a result tab.
    QHBoxLayout* make_tab_header(QWidget* page, QLabel*& title, const QString& text, ResultTab tab);
    /// Body of a result tab: a stack of [its AutoLoadPane | the "no positions" state].
    /// The pane is attached to the shared optimiser run.
    ui::AutoLoadPane* make_result_body(ResultTab tab, QWidget* page);

    // ── The shared optimiser run ──
    /// Args JSON of a run with the CURRENT inputs (empty + `why` when nothing can be optimised).
    QString optimizer_args(QString* why) const;
    /// Signature of the user's optimiser inputs (method, models, target, bounds).
    QString optimizer_signature() const;
    /// The portfolio key + inputs signature every result pane (and the backtest pane) follows.
    void apply_view_key();
    void launch_optimizer(quint64 run_id);
    /// Render one tab from an optimiser payload (also remembers the optimal weights).
    void render_tab(ResultTab tab, const QJsonObject& root);
    void render_optimize(const QJsonObject& root);
    void update_frontier(const QJsonObject& root);
    void update_strategies(const QJsonObject& root);
    void update_compare(const QJsonObject& root);
    void update_risk(const QJsonObject& root);
    void update_black_litterman(const QJsonObject& root);

    void update_allocation();
    void update_stress();
    /// Every result body shows its "no positions" state for an empty book (no run ever).
    void update_empty_states();

    // ── BACKTEST ──
    void start_backtest(quint64 request_id);
    void cancel_backtest(quint64 request_id);
    void render_backtest(const QJsonObject& payload);
    /// View key / signature of the backtest pane (portfolio + current|optimal, years, weights).
    void apply_backtest_view();
    /// Backtest the current (false) or the optimal (true) weights now (user run).
    void run_backtest(bool optimal);

    QTabWidget* tabs_ = nullptr;
    int optimize_tab_index_ = -1;
    int frontier_tab_index_ = -1;
    int allocation_tab_index_ = -1;
    int strategies_tab_index_ = -1;
    int compare_tab_index_ = -1;
    int backtest_tab_index_ = -1;
    int risk_tab_index_ = -1;
    int stress_tab_index_ = -1;
    int bl_tab_index_ = -1;

    // ── Shared optimiser run: one pane per result tab ─────────────────────────
    PortfolioSharedRun* shared_ = nullptr;
    ui::AutoLoadPane* result_panes_[kResultTabCount] = {};
    QStackedWidget* result_bodies_[kResultTabCount] = {};
    ui::EmptyState* result_empty_[kResultTabCount] = {};
    QLabel* as_of_labels_[kResultTabCount] = {};
    QJsonObject rendered_[kResultTabCount];                    ///< payload each tab shows (theme re-render)
    QHash<quint64, services::AnalyticsRequest> opt_requests_; ///< shared run id -> optimiser request
    ui::InputMemory* opt_inputs_ = nullptr;                    ///< "inputs/portfolio/optimize"
    QString run_key_;                                          ///< portfolio_run_key(summary_)

    // ── OPTIMIZE tab ──────────────────────────────────────────────────────────
    QComboBox* method_cb_ = nullptr;
    QComboBox* returns_cb_ = nullptr;
    QComboBox* risk_model_cb_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QTableWidget* result_table_ = nullptr;
    QLabel* method_field_label_ = nullptr;
    QLabel* returns_field_label_ = nullptr;
    QLabel* risk_model_field_label_ = nullptr;
    QLabel* target_ret_label_ = nullptr;      // shown only for the "Target Return" method
    QDoubleSpinBox* target_ret_spin_ = nullptr; // annual target, in percent
    QLabel* max_w_label_ = nullptr;
    QLabel* min_w_label_ = nullptr;
    QDoubleSpinBox* max_w_spin_ = nullptr; ///< per-position weight cap, % (100 = none)
    QDoubleSpinBox* min_w_spin_ = nullptr; ///< per-position weight floor, %
    ui::KpiStrip* opt_kpis_ = nullptr;     ///< expected return / volatility / Sharpe / turnover
    QLabel* opt_caption_ = nullptr;        ///< method, models, risk-free rate, history
    QLabel* opt_warn_ = nullptr;           ///< solver notes, symbols left out, unreachable target

    // ── Tab titles ────────────────────────────────────────────────────────────
    QLabel* frontier_title_ = nullptr;
    QLabel* strategies_title_ = nullptr;
    QLabel* compare_title_ = nullptr;
    QLabel* risk_title_ = nullptr;
    QLabel* stress_title_ = nullptr;
    QLabel* bl_title_ = nullptr;
    QLabel* bl_body_ = nullptr; // explanatory note, kept above the B-L table
    QLabel* alloc_title_ = nullptr;

    // ── FRONTIER tab ──────────────────────────────────────────────────────────
    QStackedWidget* frontier_stack_ = nullptr; // 0=chart, 1=no frontier for this run
    ui::EmptyState* frontier_empty_ = nullptr;
    QChartView* frontier_chart_ = nullptr;

    // ── ALLOCATION tab ────────────────────────────────────────────────────────
    QStackedWidget* alloc_body_ = nullptr; // 0=donut+table, 1=no positions
    ui::EmptyState* alloc_empty_ = nullptr;
    QChartView* alloc_chart_ = nullptr;
    QTableWidget* alloc_table_ = nullptr;

    // ── STRATEGIES tab ────────────────────────────────────────────────────────
    ui::BarChart* strategies_chart_ = nullptr;
    QTableWidget* strategies_table_ = nullptr;

    // ── COMPARE tab ───────────────────────────────────────────────────────────
    ui::MatrixHeatmap* compare_heat_ = nullptr;
    QTableWidget* compare_table_ = nullptr;

    // ── RISK tab ──────────────────────────────────────────────────────────────
    ui::BarChart* risk_chart_ = nullptr;
    QTableWidget* risk_table_ = nullptr;

    // ── STRESS tab ────────────────────────────────────────────────────────────
    QStackedWidget* stress_stack_ = nullptr; // 0=no positions, 1=table
    ui::EmptyState* stress_empty_ = nullptr;
    QTableWidget* stress_table_ = nullptr;

    // ── B-L tab ───────────────────────────────────────────────────────────────
    QTableWidget* bl_table_ = nullptr;

    // ── BACKTEST tab ──────────────────────────────────────────────────────────
    struct BacktestRun {
        quint64 request_id = 0; ///< AutoLoadPane request it answers
        bool optimal = false;
        int years = 1;
        QString start;
        QString end;
    };
    QLabel* backtest_history_label_ = nullptr;
    QComboBox* backtest_period_cb_ = nullptr; ///< years of history for the inline backtest
    QPushButton* backtest_current_btn_ = nullptr;
    QPushButton* backtest_optimal_btn_ = nullptr;
    QPushButton* open_terminal_btn_ = nullptr; // OPEN FULL BACKTESTING TERMINAL (disabled on an empty book)
    QLabel* backtest_as_of_ = nullptr;
    QStackedWidget* backtest_body_ = nullptr; // 0=pane, 1=no positions
    ui::EmptyState* backtest_empty_ = nullptr;
    ui::AutoLoadPane* backtest_pane_ = nullptr;
    ui::KpiStrip* backtest_kpis_ = nullptr;
    QLabel* backtest_caption_ = nullptr;
    QLabel* backtest_warn_ = nullptr;
    fincept::ui::TimeSeriesChart* backtest_chart_ = nullptr; ///< equity / drawdown of the inline backtest
    ui::InputMemory* backtest_inputs_ = nullptr;              ///< "inputs/portfolio/opt_backtest"
    QHash<quint64, BacktestRun> backtest_runs_;               ///< BacktestingService run id -> request
    bool backtest_optimal_ = false;                           ///< the pane shows the optimal weights' backtest
    QJsonObject backtest_rendered_;
    QJsonObject last_opt_weights_; ///< symbol -> fraction of the last optimizer result on screen

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    portfolio::MarketAnalytics analytics_;
    QString currency_;
    bool has_data_ = false;
};

} // namespace fincept::screens
