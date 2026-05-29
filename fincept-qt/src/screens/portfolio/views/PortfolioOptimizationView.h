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

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();

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
    void update_risk(const QJsonObject& root);
    void update_stress();
    void update_black_litterman(const QJsonObject& root);

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

    // ── OPTIMIZE tab ──────────────────────────────────────────────────────────
    QComboBox* method_cb_ = nullptr;
    QComboBox* returns_cb_ = nullptr;
    QComboBox* risk_model_cb_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* result_table_ = nullptr;
    QLabel* method_field_label_ = nullptr;
    QLabel* returns_field_label_ = nullptr;
    QLabel* risk_model_field_label_ = nullptr;

    // Section titles + placeholders for stub tabs
    QLabel* frontier_title_ = nullptr;
    QLabel* frontier_placeholder_ = nullptr;
    QLabel* strategies_title_ = nullptr;
    QLabel* strategies_placeholder_ = nullptr;
    QLabel* compare_title_ = nullptr;
    QLabel* compare_placeholder_ = nullptr;
    QLabel* backtest_title_ = nullptr;
    QLabel* backtest_body_ = nullptr;
    QPushButton* backtest_current_btn_ = nullptr;
    QPushButton* backtest_optimal_btn_ = nullptr;
    QStackedWidget* backtest_stack_ = nullptr; // 0=buttons, 1=results
    QTableWidget* backtest_metrics_table_ = nullptr;
    QLabel* backtest_status_label_ = nullptr;
    QLabel* risk_title_ = nullptr;
    QLabel* risk_body_ = nullptr; // placeholder (page 0 of risk_stack_)
    QStackedWidget* risk_stack_ = nullptr;
    QTableWidget* risk_table_ = nullptr;
    QLabel* stress_title_ = nullptr;
    QLabel* stress_body_ = nullptr; // placeholder (page 0 of stress_stack_)
    QStackedWidget* stress_stack_ = nullptr;
    QTableWidget* stress_table_ = nullptr;
    QLabel* bl_title_ = nullptr;
    QLabel* bl_body_ = nullptr; // explanatory note, kept above the B-L table
    QStackedWidget* bl_stack_ = nullptr;
    QTableWidget* bl_table_ = nullptr;

    // ── FRONTIER tab ──────────────────────────────────────────────────────────
    QStackedWidget* frontier_stack_ = nullptr; // 0=placeholder, 1=chart
    QChartView* frontier_chart_ = nullptr;

    // ── ALLOCATION tab ────────────────────────────────────────────────────────
    QChartView* alloc_chart_ = nullptr;
    QTableWidget* alloc_table_ = nullptr;

    // ── STRATEGIES tab ────────────────────────────────────────────────────────
    QStackedWidget* strategies_stack_ = nullptr; // 0=placeholder, 1=table
    QTableWidget* strategies_table_ = nullptr;

    // ── COMPARE tab ───────────────────────────────────────────────────────────
    QStackedWidget* compare_stack_ = nullptr; // 0=placeholder, 1=table
    QTableWidget* compare_table_ = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    QString currency_;
    bool running_ = false;
    bool has_data_ = false;
};

} // namespace fincept::screens
