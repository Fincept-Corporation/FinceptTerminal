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

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    QWidget* build_retirement_tab();
    QWidget* build_goals_tab();
    QWidget* build_savings_tab();
    void recalculate();

    QTabWidget* tabs_ = nullptr;
    int retirement_tab_index_ = -1;
    int goals_tab_index_ = -1;
    int savings_tab_index_ = -1;

    // Retirement inputs
    QDoubleSpinBox* current_age_ = nullptr;
    QDoubleSpinBox* retire_age_ = nullptr;
    QDoubleSpinBox* annual_expense_ = nullptr;
    QDoubleSpinBox* monthly_contrib_ = nullptr;
    QDoubleSpinBox* expected_return_ = nullptr;
    QDoubleSpinBox* inflation_ = nullptr;
    QDoubleSpinBox* withdrawal_rate_ = nullptr;

    // Retirement labels/buttons (persistent)
    QLabel* input_title_ = nullptr;
    QLabel* l_current_age_ = nullptr;
    QLabel* l_retire_age_ = nullptr;
    QLabel* l_annual_expense_ = nullptr;
    QLabel* l_monthly_contrib_ = nullptr;
    QLabel* l_expected_return_ = nullptr;
    QLabel* l_inflation_ = nullptr;
    QLabel* l_withdrawal_rate_ = nullptr;
    QPushButton* calc_btn_ = nullptr;
    QLabel* res_title_ = nullptr;
    QLabel* years_card_label_ = nullptr;
    QLabel* target_card_label_ = nullptr;
    QLabel* projected_card_label_ = nullptr;
    QLabel* gap_card_label_ = nullptr;

    // Goals tab labels
    QLabel* goals_title_ = nullptr;
    QLabel* goals_desc_ = nullptr;

    // Savings tab labels
    QLabel* savings_title_ = nullptr;
    QLabel* savings_desc_ = nullptr;

    // Results
    QLabel* years_label_ = nullptr;
    QLabel* target_label_ = nullptr;
    QLabel* projected_label_ = nullptr;
    QLabel* gap_label_ = nullptr;
    QLabel* status_label_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    bool has_data_ = false;
};

} // namespace fincept::screens
