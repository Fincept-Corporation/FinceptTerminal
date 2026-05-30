// src/screens/portfolio/views/PlanningView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Financial Planning view with retirement planning, goal-based allocation,
/// and savings rate analysis.
class PlanningView : public QWidget {
    Q_OBJECT
  public:
    explicit PlanningView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    /// Daily NAV snapshots — used to derive the portfolio's real historical CAGR
    /// and volatility, which seed the planning assumptions.
    void set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots);
    /// Computed risk metrics — used for the drawdown/VaR stress line.
    void set_metrics(const portfolio::ComputedMetrics& metrics);

  signals:
    /// Emitted by the "Optimize for this return" action — asks the host to open
    /// the Optimization sub-tab targeting the given annualised return (decimal).
    void optimize_for_return(double target_annual_return);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    QWidget* build_retirement_tab();
    QWidget* build_goals_tab();
    QWidget* build_savings_tab();
    void recalculate();
    void recalculate_goals();
    void recalculate_savings();

    // Derive historical CAGR + annualised volatility from snapshots_.
    void recompute_assumptions();

    // Monte Carlo wealth-path simulation calibrated to the portfolio's own
    // mean/vol. Simulates `years` of annual returns ~Normal(mean, vol) on a
    // balance that also receives `monthly`*12 each year.
    struct McResult {
        double p5 = 0, p50 = 0, p95 = 0; // ending-balance percentiles
        double success_prob = 0;         // fraction of paths reaching `target`
        bool valid = false;
    };
    McResult monte_carlo(double start, double monthly, int years, double mean_annual, double vol_annual,
                         double target) const;

    // Goals persistence (per-portfolio, SettingsRepository).
    void load_goals();
    void save_goals();
    void refresh_goals_list();
    void add_or_update_goal();
    void remove_selected_goal();

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

    // Goals tab
    QLabel* goals_title_ = nullptr;
    QLabel* goals_desc_ = nullptr;
    QLineEdit* goal_name_ = nullptr;
    QDoubleSpinBox* goal_target_ = nullptr;
    QDoubleSpinBox* goal_years_ = nullptr;
    QDoubleSpinBox* goal_return_ = nullptr;
    QDoubleSpinBox* goal_monthly_ = nullptr;
    QDoubleSpinBox* goal_alloc_pct_ = nullptr; // % of current portfolio earmarked for this goal
    QLabel* l_goal_name_ = nullptr;
    QLabel* l_goal_target_ = nullptr;
    QLabel* l_goal_years_ = nullptr;
    QLabel* l_goal_return_ = nullptr;
    QLabel* l_goal_monthly_ = nullptr;
    QLabel* l_goal_alloc_ = nullptr;
    QPushButton* goals_calc_btn_ = nullptr;
    QLabel* goals_res_title_ = nullptr;
    QLabel* goal_required_card_label_ = nullptr;
    QLabel* goal_projected_card_label_ = nullptr;
    QLabel* goal_target_card_label_ = nullptr;
    QLabel* goal_progress_card_label_ = nullptr;
    QLabel* goal_required_label_ = nullptr;
    QLabel* goal_projected_label_ = nullptr;
    QLabel* goal_target_value_label_ = nullptr;
    QLabel* goal_progress_label_ = nullptr;
    QLabel* goal_status_label_ = nullptr;

    // Savings tab
    QLabel* savings_title_ = nullptr;
    QLabel* savings_desc_ = nullptr;
    QDoubleSpinBox* sav_income_ = nullptr;
    QDoubleSpinBox* sav_rate_ = nullptr;
    QDoubleSpinBox* sav_return_ = nullptr;
    QDoubleSpinBox* sav_years_ = nullptr;
    QLabel* l_sav_income_ = nullptr;
    QLabel* l_sav_rate_ = nullptr;
    QLabel* l_sav_return_ = nullptr;
    QLabel* l_sav_years_ = nullptr;
    QPushButton* savings_calc_btn_ = nullptr;
    QLabel* savings_res_title_ = nullptr;
    QTableWidget* savings_table_ = nullptr;
    QLabel* savings_status_label_ = nullptr;

    // Results
    QLabel* years_label_ = nullptr;
    QLabel* target_label_ = nullptr;
    QLabel* projected_label_ = nullptr;
    QLabel* gap_label_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Retirement: Monte Carlo + stress + assumptions note
    QLabel* retire_assump_note_ = nullptr;
    QLabel* retire_mc_label_ = nullptr;    // "Success: 72% | P5 … P95 …"
    QLabel* retire_stress_label_ = nullptr; // drawdown/VaR stress line
    QPushButton* retire_optimize_btn_ = nullptr;

    // Goals: saved-goals list + funding
    QTableWidget* goals_list_ = nullptr;
    QPushButton* goal_add_btn_ = nullptr;
    QPushButton* goal_remove_btn_ = nullptr;
    QLabel* goal_mc_label_ = nullptr;

    // Savings: assumptions note
    QLabel* sav_assump_note_ = nullptr;

    // A persisted goal.
    struct Goal {
        QString name;
        double target = 0;
        int years = 0;
        double monthly = 0;
        double alloc_pct = 0; // % of portfolio earmarked
    };
    QVector<Goal> goals_;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    QVector<portfolio::PortfolioSnapshot> snapshots_;
    portfolio::ComputedMetrics metrics_;
    double hist_cagr_ = 0.0;  // derived annualised return (decimal), 0 if unknown
    double hist_vol_ = 0.0;   // derived annualised volatility (decimal), 0 if unknown
    bool have_history_ = false;
    bool has_data_ = false;
};

} // namespace fincept::screens
