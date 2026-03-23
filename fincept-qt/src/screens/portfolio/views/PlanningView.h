// src/screens/portfolio/views/PlanningView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QWidget>

namespace fincept::screens {

/// Financial Planning view with retirement planning, goal-based allocation,
/// and savings rate analysis.
class PlanningView : public QWidget {
    Q_OBJECT
  public:
    explicit PlanningView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  private:
    void build_ui();
    QWidget* build_retirement_tab();
    QWidget* build_goals_tab();
    QWidget* build_savings_tab();
    void recalculate();

    QTabWidget* tabs_ = nullptr;

    // Retirement inputs
    QDoubleSpinBox* current_age_     = nullptr;
    QDoubleSpinBox* retire_age_      = nullptr;
    QDoubleSpinBox* annual_expense_  = nullptr;
    QDoubleSpinBox* monthly_contrib_ = nullptr;
    QDoubleSpinBox* expected_return_ = nullptr;
    QDoubleSpinBox* inflation_       = nullptr;

    // Results
    QLabel* years_label_       = nullptr;
    QLabel* target_label_      = nullptr;
    QLabel* projected_label_   = nullptr;
    QLabel* gap_label_         = nullptr;
    QLabel* status_label_      = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
};

} // namespace fincept::screens
