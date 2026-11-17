// src/screens/portfolio/PortfolioFFNView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAnalyticsService.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

#include <functional>

namespace fincept::ui {
class AutoLoadPane;
class BarChart;
class EmptyState;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

class PortfolioSharedRun;

/// FFN (Financial Functions) analytics view with 6 sub-tabs: OVERVIEW,
/// BENCHMARK, OPTIMISATION, REBASED, DRAWDOWNS, ROLLING. One ffn_analysis.py run
/// fills all six (PortfolioSharedRun: whichever tab is shown first starts it on
/// first show — class B, free, local Python + a 1-year price download — the
/// others join it or open on its result). Results are remembered per portfolio
/// and holdings ("as of") and refreshed quietly when older than
/// kPortfolioScriptMaxAgeS. Python goes through PortfolioAnalyticsService (D1/P6).
class PortfolioFFNView : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioFFNView(QWidget* parent = nullptr);
    ~PortfolioFFNView() override;

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

    /// Benchmark daily closes (the portfolio currency's index, e.g. SPY / ^NSEI)
    /// and the live annual risk-free rate — feed the BENCHMARK figures (shown at
    /// once, before the ffn run lands) and every benchmark comparison on this view.
    void set_benchmark(const QString& symbol, const QVector<double>& closes, double risk_free_rate);

  signals:
    void back_requested();
    /// The empty-book state's BUY action.
    void add_position_requested();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    /// One result tab: its AutoLoadPane, and the empty-book state beside it.
    struct ResultTab {
        QStackedWidget* stack = nullptr; ///< pane | empty-book state
        ui::AutoLoadPane* pane = nullptr;
        ui::EmptyState* empty = nullptr; ///< created on first need
        std::function<QString()> empty_text;
        std::function<void()> render; ///< draws ffn_data_ into the tab
    };

    void build_ui();
    void retranslateUi();
    /// Table items and built series carry their colours: re-render the visible tab
    /// after an appearance change, the others on their next show.
    void on_appearance_settled();
    void rerender_if_stale(ResultTab& tab);

    ResultTab& new_tab(ResultTab& tab, QWidget* parent, int shape, std::function<void()> render);
    void set_empty_book(bool empty);
    void on_tab_changed(int index);
    ResultTab* tab_for_index(int index);
    ui::AutoLoadPane* current_pane();

    // Per-tab renderers — read ffn_data_
    void update_overview();
    void update_benchmark();
    void update_bench_strip(); ///< benchmark-only figures from the local closes (no run needed)
    void update_optimization();
    void update_rebased();
    void update_drawdowns();
    void update_rolling();

    void launch_ffn(quint64 run_id);
    QString benchmark_info_text() const;

    // ── Header ────────────────────────────────────────────────────────────────
    QPushButton* back_btn_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QLabel* status_label_ = nullptr; ///< "as of …" of the tab on screen
    QLabel* title_label_ = nullptr;

    // ── Tab container ─────────────────────────────────────────────────────────
    QTabWidget* tabs_ = nullptr;
    ResultTab overview_tab_;
    ResultTab benchmark_tab_;
    ResultTab optimization_tab_;
    ResultTab rebased_tab_;
    ResultTab drawdowns_tab_;
    ResultTab rolling_tab_;

    // Section headers — kept so retranslateUi can refresh them after a language
    // switch without rebuilding the entire view.
    QLabel* overview_hdr_ = nullptr;
    QLabel* benchmark_hdr_ = nullptr;
    QLabel* optimization_hdr_ = nullptr;
    QLabel* weights_hdr_ = nullptr;
    QLabel* stats_hdr_ = nullptr;

    // ── Overview tab ──────────────────────────────────────────────────────────
    ui::KpiStrip* overview_kpis_ = nullptr;
    QTableWidget* overview_table_ = nullptr;

    // ── Benchmark tab ─────────────────────────────────────────────────────────
    ui::KpiStrip* bench_kpis_ = nullptr; ///< benchmark-only, from set_benchmark()
    QTableWidget* benchmark_table_ = nullptr;
    QLabel* benchmark_info_label_ = nullptr;

    // ── Optimisation tab ──────────────────────────────────────────────────────
    ui::BarChart* opt_bars_ = nullptr;
    QTableWidget* opt_weights_table_ = nullptr;
    QTableWidget* opt_stats_table_ = nullptr;

    // ── Rebased / drawdowns / rolling tabs ────────────────────────────────────
    ui::TimeSeriesChart* rebased_chart_ = nullptr;
    ui::TimeSeriesChart* drawdowns_chart_ = nullptr;
    QStackedWidget* rolling_content_ = nullptr; ///< chart | "needs two holdings"
    ui::TimeSeriesChart* rolling_chart_ = nullptr;
    ui::EmptyState* rolling_empty_ = nullptr;

    // ── Runs ──────────────────────────────────────────────────────────────────
    PortfolioSharedRun* shared_ = nullptr;
    QHash<quint64, services::AnalyticsRequest> requests_; ///< shared-run id -> script request
    QSet<ui::AutoLoadPane*> theme_stale_;

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    QString currency_;
    QJsonObject ffn_data_;             // the ffn_analysis.py result being rendered
    QVector<double> benchmark_closes_; // benchmark daily closes (1y), via set_benchmark()
    QString benchmark_symbol_ = QStringLiteral("SPY");
    double risk_free_rate_ = 0.04; // replaced by the live rate via set_benchmark()
    bool has_data_ = false;
    bool empty_book_ = false;
};

} // namespace fincept::screens
