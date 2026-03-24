// src/screens/portfolio/views/PortfolioOptimizationView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Portfolio Optimization detail view with 9 sub-tabs for various optimization methods.
/// Uses PythonRunner for pyportfolioopt/skfolio calculations (P4-compliant).
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
    void update_strategies();

    QTabWidget* tabs_ = nullptr;

    // Optimize tab widgets
    QComboBox* method_cb_ = nullptr;
    QComboBox* returns_cb_ = nullptr;
    QComboBox* risk_model_cb_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* result_table_ = nullptr;

    // Frontier tab
    QChartView* frontier_chart_ = nullptr;

    // Allocation tab
    QChartView* alloc_chart_ = nullptr;
    QTableWidget* alloc_table_ = nullptr;

    // Strategies tab
    QTableWidget* strategies_table_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    bool running_ = false;
};

} // namespace fincept::screens
