// src/screens/portfolio/views/PortfolioOptimizationView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QComboBox>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Portfolio Optimization view — 9 tabs.
/// OPTIMIZE: run Python optimizer (max_sharpe / min_vol / risk_parity / HRP etc.)
/// FRONTIER: efficient frontier scatter chart (real points from optimizer)
/// ALLOCATION: current holdings donut + table
/// STRATEGIES: comparison of 5 methods — populated after optimization
/// COMPARE: side-by-side weights table for all 5 methods
/// BACKTEST / RISK / STRESS / B-L MODEL: informational stubs
class PortfolioOptimizationView : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioOptimizationView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  private:
    void build_ui();

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

    void run_optimization();
    void update_allocation();
    void update_frontier(const QJsonArray& frontier_pts);
    void update_strategies(const QJsonObject& comparison);
    void update_compare(const QJsonObject& comparison);

    QTabWidget* tabs_ = nullptr;

    // ── OPTIMIZE tab ──────────────────────────────────────────────────────────
    QComboBox*    method_cb_     = nullptr;
    QComboBox*    returns_cb_    = nullptr;
    QComboBox*    risk_model_cb_ = nullptr;
    QPushButton*  run_btn_       = nullptr;
    QLabel*       status_label_  = nullptr;
    QTableWidget* result_table_  = nullptr;

    // ── FRONTIER tab ──────────────────────────────────────────────────────────
    QStackedWidget* frontier_stack_ = nullptr;  // 0=placeholder, 1=chart
    QChartView*     frontier_chart_ = nullptr;

    // ── ALLOCATION tab ────────────────────────────────────────────────────────
    QChartView*   alloc_chart_ = nullptr;
    QTableWidget* alloc_table_ = nullptr;

    // ── STRATEGIES tab ────────────────────────────────────────────────────────
    QStackedWidget* strategies_stack_ = nullptr;  // 0=placeholder, 1=table
    QTableWidget*   strategies_table_ = nullptr;

    // ── COMPARE tab ───────────────────────────────────────────────────────────
    QStackedWidget* compare_stack_ = nullptr;   // 0=placeholder, 1=table
    QTableWidget*   compare_table_ = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    QString  currency_;
    bool     running_ = false;
};

} // namespace fincept::screens
