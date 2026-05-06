// MAAdvancedPanel.cpp — advanced analytics (Monte Carlo / scenario) sub-tab builder for MAModulePanel.
// Method definition split from MAModulePanel.cpp.

#include "screens/ma_analytics/MAModulePanel.h"

#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 7: ADVANCED ANALYTICS
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_advanced_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Monte Carlo ──
    auto* mc = new QWidget(this);
    auto* mc_vl = new QVBoxLayout(mc);
    mc_vl->setContentsMargins(12, 12, 12, 12);
    mc_vl->setSpacing(8);

    auto* mc_base = make_double_spin(0, 1e15, 1e9, 0, "", mc);
    double_inputs_["mc_base"] = mc_base;
    mc_vl->addWidget(build_input_row("Base Valuation ($)", mc_base, mc));

    auto* mc_rg_mean = make_double_spin(-100, 500, 15, 1, "%", mc);
    double_inputs_["mc_rev_growth_mean"] = mc_rg_mean;
    mc_vl->addWidget(build_input_row("Rev Growth Mean", mc_rg_mean, mc));

    auto* mc_rg_std = make_double_spin(0, 100, 5, 1, "%", mc);
    double_inputs_["mc_rev_growth_std"] = mc_rg_std;
    mc_vl->addWidget(build_input_row("Rev Growth Std Dev", mc_rg_std, mc));

    auto* mc_mg_mean = make_double_spin(-100, 100, 25, 1, "%", mc);
    double_inputs_["mc_margin_mean"] = mc_mg_mean;
    mc_vl->addWidget(build_input_row("EBITDA Margin Mean", mc_mg_mean, mc));

    auto* mc_mg_std = make_double_spin(0, 100, 3, 1, "%", mc);
    double_inputs_["mc_margin_std"] = mc_mg_std;
    mc_vl->addWidget(build_input_row("EBITDA Margin Std Dev", mc_mg_std, mc));

    auto* mc_disc = make_double_spin(0, 50, 10, 1, "%", mc);
    double_inputs_["mc_discount"] = mc_disc;
    mc_vl->addWidget(build_input_row("Discount Rate", mc_disc, mc));

    auto* mc_sims = make_int_spin(100, 100000, 10000, mc);
    int_inputs_["mc_simulations"] = mc_sims;
    mc_vl->addWidget(build_input_row("Simulations", mc_sims, mc));

    auto* mc_run = make_run_button("RUN MONTE CARLO", mc);
    connect(mc_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running Monte Carlo...");
        QJsonObject params;
        params["base_valuation"] = double_inputs_["mc_base"]->value();
        params["rev_growth_mean"] = double_inputs_["mc_rev_growth_mean"]->value() / 100.0;
        params["rev_growth_std"] = double_inputs_["mc_rev_growth_std"]->value() / 100.0;
        params["margin_mean"] = double_inputs_["mc_margin_mean"]->value() / 100.0;
        params["margin_std"] = double_inputs_["mc_margin_std"]->value() / 100.0;
        params["discount_rate"] = double_inputs_["mc_discount"]->value() / 100.0;
        params["simulations"] = int_inputs_["mc_simulations"]->value();
        MAAnalyticsService::instance().run_monte_carlo(params);
    });
    mc_vl->addWidget(mc_run);
    mc_vl->addStretch();
    sub_tabs_->addTab(mc, "Monte Carlo");

    // ── Regression ──
    auto* reg = new QWidget(this);
    auto* reg_vl = new QVBoxLayout(reg);
    reg_vl->setContentsMargins(12, 12, 12, 12);
    reg_vl->setSpacing(8);

    auto* reg_type = new QComboBox(reg);
    reg_type->addItems({"OLS", "Multiple"});
    reg_type->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::SMALL));
    combo_inputs_["reg_type"] = reg_type;
    reg_vl->addWidget(build_input_row("Regression Type", reg_type, reg));

    auto* subj_rev = make_double_spin(0, 1e15, 1e9, 0, "", reg);
    double_inputs_["reg_subj_revenue"] = subj_rev;
    reg_vl->addWidget(build_input_row("Subject Revenue ($)", subj_rev, reg));

    auto* subj_ebitda = make_double_spin(0, 1e15, 300e6, 0, "", reg);
    double_inputs_["reg_subj_ebitda"] = subj_ebitda;
    reg_vl->addWidget(build_input_row("Subject EBITDA ($)", subj_ebitda, reg));

    auto* subj_growth = make_double_spin(-100, 500, 14, 1, "%", reg);
    double_inputs_["reg_subj_growth"] = subj_growth;
    reg_vl->addWidget(build_input_row("Subject Growth", subj_growth, reg));

    auto* reg_run = make_run_button("RUN REGRESSION", reg);
    connect(reg_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running Regression...");
        QJsonObject params;
        params["type"] = combo_inputs_["reg_type"]->currentText().toLower();
        QJsonObject subject;
        subject["revenue"] = double_inputs_["reg_subj_revenue"]->value();
        subject["ebitda"] = double_inputs_["reg_subj_ebitda"]->value();
        subject["growth"] = double_inputs_["reg_subj_growth"]->value() / 100.0;
        params["subject"] = subject;
        MAAnalyticsService::instance().run_regression(params);
    });
    reg_vl->addWidget(reg_run);
    reg_vl->addStretch();
    sub_tabs_->addTab(reg, "Regression");

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
