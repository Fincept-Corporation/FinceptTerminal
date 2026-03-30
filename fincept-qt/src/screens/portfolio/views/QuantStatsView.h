// src/screens/portfolio/views/QuantStatsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QAreaSeries>
#include <QChart>
#include <QChartView>
#include <QJsonObject>
#include <QLabel>
#include <QLineSeries>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QValueAxis>
#include <QWidget>

namespace fincept::screens {

/// QuantStats analytics view — 5 tabs: METRICS, RETURNS, DRAWDOWN, ROLLING, MONTE CARLO.
/// Calls quantstats_analysis.py automatically on set_data(), and quantstats_monte_carlo.py
/// on user request. All Python calls are async via PythonRunner (P8 compliant).
class QuantStatsView : public QWidget {
    Q_OBJECT
  public:
    explicit QuantStatsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  private:
    void build_ui();

    // Per-tab update methods — called after qs_data_ is populated
    void update_metrics();
    void update_returns();
    void update_drawdown();
    void update_rolling();
    void update_monte_carlo_chart();

    // Python runners
    void run_quantstats();
    void run_monte_carlo();

    /// Creates a dark-themed QChartView with title label. Returns nullptr on first call
    /// (chart is built lazily inside update methods). Used by MC tab.
    QChartView* make_chart_view(const QString& title);

    // ── Tab container ─────────────────────────────────────────────────────────
    QTabWidget* tabs_ = nullptr;

    // ── Header controls ───────────────────────────────────────────────────────
    QPushButton* qs_run_btn_  = nullptr;
    QLabel*      qs_status_  = nullptr;

    // ── METRICS tab ───────────────────────────────────────────────────────────
    QTableWidget* metrics_table_ = nullptr;

    // ── RETURNS tab ───────────────────────────────────────────────────────────
    QStackedWidget* returns_stack_ = nullptr;   // 0 = placeholder, 1 = content widget

    // ── DRAWDOWN tab ──────────────────────────────────────────────────────────
    QStackedWidget* drawdown_stack_ = nullptr;  // 0 = placeholder, 1 = content widget

    // ── ROLLING tab ───────────────────────────────────────────────────────────
    QStackedWidget* rolling_stack_ = nullptr;   // 0 = placeholder, 1 = content widget

    // ── MONTE CARLO tab ───────────────────────────────────────────────────────
    QPushButton*           mc_run_btn_  = nullptr;
    QLabel*                mc_status_  = nullptr;
    QWidget*               mc_results_ = nullptr;  // kept for compat
    QStackedWidget*        mc_stack_   = nullptr;  // 0 = placeholder, 1 = live content
    QChartView*  mc_chart_   = nullptr;

    // ── Unused chart members (reserved for future time-series data) ───────────
    QChartView* returns_chart_  = nullptr;
    QChartView* drawdown_chart_ = nullptr;
    QChartView* rolling_chart_  = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    QString     currency_;
    QJsonObject qs_data_;   // parsed result from quantstats_analysis.py
    QJsonObject mc_data_;   // parsed result from quantstats_monte_carlo.py
    bool        qs_running_ = false;
    bool        mc_running_ = false;
};

} // namespace fincept::screens
