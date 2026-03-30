// src/screens/portfolio/PortfolioFFNView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QAreaSeries>
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
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

/// FFN (Financial Functions) analytics view with 6 sub-tabs.
/// Uses Python ffn library via PythonRunner for calculations.
class PortfolioFFNView : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioFFNView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  signals:
    void back_requested();

  private:
    void build_ui();

    // Per-tab update methods
    void update_overview();
    void update_benchmark();
    void update_optimization();
    void update_rebased();
    void update_drawdowns();
    void update_rolling();

    void run_ffn();

    /// Creates a dark-themed QChartView with an amber title.
    QChartView* make_chart_view(const QString& title);

    // ── Header ────────────────────────────────────────────────────────────────
    QPushButton* back_btn_    = nullptr;
    QPushButton* run_btn_     = nullptr;
    QLabel*      status_label_ = nullptr;

    // ── Tab container ─────────────────────────────────────────────────────────
    QTabWidget* tabs_ = nullptr;

    // ── Overview tab ──────────────────────────────────────────────────────────
    QTableWidget* overview_table_ = nullptr;

    // ── Benchmark tab ─────────────────────────────────────────────────────────
    QWidget*      benchmark_panel_      = nullptr;
    QTableWidget* benchmark_table_      = nullptr;
    QLabel*       benchmark_info_label_ = nullptr;

    // ── Optimisation tab ──────────────────────────────────────────────────────
    QWidget*       optimization_panel_      = nullptr;
    QStackedWidget* opt_stack_             = nullptr;   // 0=placeholder, 1=tables
    QTableWidget*  opt_weights_table_      = nullptr;
    QTableWidget*  opt_stats_table_        = nullptr;

    // ── Rebased tab ───────────────────────────────────────────────────────────
    QWidget*           rebased_panel_      = nullptr;
    QStackedWidget*    rebased_stack_      = nullptr;   // 0=placeholder, 1=chart
    QChartView* rebased_chart_view_ = nullptr;

    // ── Drawdowns tab ─────────────────────────────────────────────────────────
    QWidget*           drawdowns_panel_        = nullptr;
    QStackedWidget*    drawdowns_stack_        = nullptr;
    QChartView* drawdowns_chart_view_ = nullptr;

    // ── Rolling correlations tab ──────────────────────────────────────────────
    QWidget*           rolling_panel_      = nullptr;
    QStackedWidget*    rolling_stack_      = nullptr;
    QChartView* rolling_chart_view_ = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    QString     currency_;
    QJsonObject ffn_data_;
};

} // namespace fincept::screens
