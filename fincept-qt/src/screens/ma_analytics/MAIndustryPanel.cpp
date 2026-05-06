// MAIndustryPanel.cpp — industry-metric snapshot sub-tab builder for MAModulePanel.
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

using namespace fincept::services::ma;

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 6: INDUSTRY METRICS
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_industry_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Technology ──
    auto* tech = new QWidget(this);
    auto* tech_vl = new QVBoxLayout(tech);
    tech_vl->setContentsMargins(12, 12, 12, 12);
    tech_vl->setSpacing(8);

    auto* tech_sector = new QComboBox(tech);
    tech_sector->addItems({"saas", "marketplace", "semiconductor"});
    tech_sector->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                       "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::fonts::SMALL));
    combo_inputs_["tech_sector"] = tech_sector;
    tech_vl->addWidget(build_input_row("Sector", tech_sector, tech));

    auto* tech_arr = make_double_spin(0, 1e15, 100e6, 0, "", tech);
    double_inputs_["tech_arr"] = tech_arr;
    tech_vl->addWidget(build_input_row("ARR/Revenue ($)", tech_arr, tech));

    auto* tech_growth = make_double_spin(-100, 500, 40, 1, "%", tech);
    double_inputs_["tech_growth"] = tech_growth;
    tech_vl->addWidget(build_input_row("Growth Rate", tech_growth, tech));

    auto* tech_gm = make_double_spin(0, 100, 75, 1, "%", tech);
    double_inputs_["tech_gross_margin"] = tech_gm;
    tech_vl->addWidget(build_input_row("Gross Margin", tech_gm, tech));

    auto* tech_nrr = make_double_spin(50, 200, 115, 0, "%", tech);
    double_inputs_["tech_nrr"] = tech_nrr;
    tech_vl->addWidget(build_input_row("Net Revenue Retention", tech_nrr, tech));

    auto* tech_cac = make_double_spin(0, 120, 18, 0, " mo", tech);
    double_inputs_["tech_cac_payback"] = tech_cac;
    tech_vl->addWidget(build_input_row("CAC Payback (months)", tech_cac, tech));

    auto* tech_ltv = make_double_spin(0, 20, 3.5, 1, "x", tech);
    double_inputs_["tech_ltv_cac"] = tech_ltv;
    tech_vl->addWidget(build_input_row("LTV/CAC Ratio", tech_ltv, tech));

    auto* tech_r40 = make_double_spin(-50, 200, 55, 0, "", tech);
    double_inputs_["tech_rule_of_40"] = tech_r40;
    tech_vl->addWidget(build_input_row("Rule of 40 Score", tech_r40, tech));

    auto* tech_run = make_run_button("CALCULATE TECH METRICS", tech);
    connect(tech_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating Tech Metrics...");
        QJsonObject params;
        params["sector"] = combo_inputs_["tech_sector"]->currentText();
        params["arr"] = double_inputs_["tech_arr"]->value();
        params["growth"] = double_inputs_["tech_growth"]->value() / 100.0;
        params["gross_margin"] = double_inputs_["tech_gross_margin"]->value() / 100.0;
        params["nrr"] = double_inputs_["tech_nrr"]->value() / 100.0;
        params["cac_payback"] = double_inputs_["tech_cac_payback"]->value();
        params["ltv_cac"] = double_inputs_["tech_ltv_cac"]->value();
        params["rule_of_40"] = double_inputs_["tech_rule_of_40"]->value();
        MAAnalyticsService::instance().calculate_tech_metrics(params);
    });
    tech_vl->addWidget(tech_run);
    tech_vl->addStretch();
    sub_tabs_->addTab(tech, "Technology");

    // ── Healthcare ──
    auto* hc = new QWidget(this);
    auto* hc_vl = new QVBoxLayout(hc);
    hc_vl->setContentsMargins(12, 12, 12, 12);
    hc_vl->setSpacing(8);

    auto* hc_sector = new QComboBox(hc);
    hc_sector->addItems({"pharma", "biotech", "devices", "services"});
    hc_sector->setStyleSheet(tech_sector->styleSheet());
    combo_inputs_["hc_sector"] = hc_sector;
    hc_vl->addWidget(build_input_row("Sector", hc_sector, hc));

    auto* hc_rev = make_double_spin(0, 1e15, 5e9, 0, "", hc);
    double_inputs_["hc_revenue"] = hc_rev;
    hc_vl->addWidget(build_input_row("Revenue ($)", hc_rev, hc));

    auto* hc_margin = make_double_spin(0, 100, 35, 1, "%", hc);
    double_inputs_["hc_ebitda_margin"] = hc_margin;
    hc_vl->addWidget(build_input_row("EBITDA Margin", hc_margin, hc));

    auto* hc_pipeline = make_double_spin(0, 1e15, 3e9, 0, "", hc);
    double_inputs_["hc_pipeline_npv"] = hc_pipeline;
    hc_vl->addWidget(build_input_row("Pipeline NPV ($)", hc_pipeline, hc));

    auto* hc_p3 = make_int_spin(0, 50, 5, hc);
    int_inputs_["hc_phase3_count"] = hc_p3;
    hc_vl->addWidget(build_input_row("Phase 3 Candidates", hc_p3, hc));

    auto* hc_patent = make_double_spin(0, 100, 25, 0, "%", hc);
    double_inputs_["hc_patent_expiry_rev"] = hc_patent;
    hc_vl->addWidget(build_input_row("Patent Expiry Rev %", hc_patent, hc));

    auto* hc_rd = make_double_spin(0, 1e15, 800e6, 0, "", hc);
    double_inputs_["hc_rd_spend"] = hc_rd;
    hc_vl->addWidget(build_input_row("R&D Spend ($)", hc_rd, hc));

    auto* hc_run = make_run_button("CALCULATE HC METRICS", hc);
    connect(hc_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating Healthcare Metrics...");
        QJsonObject params;
        params["sector"] = combo_inputs_["hc_sector"]->currentText();
        params["revenue"] = double_inputs_["hc_revenue"]->value();
        params["ebitda_margin"] = double_inputs_["hc_ebitda_margin"]->value() / 100.0;
        params["pipeline_npv"] = double_inputs_["hc_pipeline_npv"]->value();
        params["phase3_candidates"] = int_inputs_["hc_phase3_count"]->value();
        params["patent_expiry_revenue"] = double_inputs_["hc_patent_expiry_rev"]->value() / 100.0;
        params["rd_spend"] = double_inputs_["hc_rd_spend"]->value();
        MAAnalyticsService::instance().calculate_healthcare_metrics(params);
    });
    hc_vl->addWidget(hc_run);
    hc_vl->addStretch();
    sub_tabs_->addTab(hc, "Healthcare");

    // ── Financial Services ──
    auto* fs = new QWidget(this);
    auto* fs_vl = new QVBoxLayout(fs);
    fs_vl->setContentsMargins(12, 12, 12, 12);
    fs_vl->setSpacing(8);

    auto* fs_sector = new QComboBox(fs);
    fs_sector->addItems({"banking", "insurance", "asset_management"});
    fs_sector->setStyleSheet(tech_sector->styleSheet());
    combo_inputs_["fs_sector"] = fs_sector;
    fs_vl->addWidget(build_input_row("Sector", fs_sector, fs));

    auto* fs_assets = make_double_spin(0, 1e18, 50e9, 0, "", fs);
    double_inputs_["fs_total_assets"] = fs_assets;
    fs_vl->addWidget(build_input_row("Total Assets ($)", fs_assets, fs));

    auto* fs_roe = make_double_spin(-100, 200, 12, 1, "%", fs);
    double_inputs_["fs_roe"] = fs_roe;
    fs_vl->addWidget(build_input_row("ROE", fs_roe, fs));

    auto* fs_deposits = make_double_spin(0, 1e18, 40e9, 0, "", fs);
    double_inputs_["fs_deposits"] = fs_deposits;
    fs_vl->addWidget(build_input_row("Deposits ($)", fs_deposits, fs));

    auto* fs_nim = make_double_spin(0, 20, 3.2, 2, "%", fs);
    double_inputs_["fs_nim"] = fs_nim;
    fs_vl->addWidget(build_input_row("Net Interest Margin", fs_nim, fs));

    auto* fs_efficiency = make_double_spin(0, 200, 58, 1, "%", fs);
    double_inputs_["fs_efficiency_ratio"] = fs_efficiency;
    fs_vl->addWidget(build_input_row("Efficiency Ratio", fs_efficiency, fs));

    auto* fs_cet1 = make_double_spin(0, 30, 12.5, 1, "%", fs);
    double_inputs_["fs_cet1"] = fs_cet1;
    fs_vl->addWidget(build_input_row("CET1 Ratio", fs_cet1, fs));

    auto* fs_npl = make_double_spin(0, 30, 0.8, 2, "%", fs);
    double_inputs_["fs_npl_ratio"] = fs_npl;
    fs_vl->addWidget(build_input_row("NPL Ratio", fs_npl, fs));

    auto* fs_run = make_run_button("CALCULATE FINSERV METRICS", fs);
    connect(fs_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating FinServ Metrics...");
        QJsonObject params;
        params["sector"] = combo_inputs_["fs_sector"]->currentText();
        params["total_assets"] = double_inputs_["fs_total_assets"]->value();
        params["roe"] = double_inputs_["fs_roe"]->value() / 100.0;
        params["deposits"] = double_inputs_["fs_deposits"]->value();
        params["nim"] = double_inputs_["fs_nim"]->value() / 100.0;
        params["efficiency_ratio"] = double_inputs_["fs_efficiency_ratio"]->value() / 100.0;
        params["cet1_ratio"] = double_inputs_["fs_cet1"]->value() / 100.0;
        params["npl_ratio"] = double_inputs_["fs_npl_ratio"]->value() / 100.0;
        MAAnalyticsService::instance().calculate_financial_services_metrics(params);
    });
    fs_vl->addWidget(fs_run);
    fs_vl->addStretch();
    sub_tabs_->addTab(fs, "Financial Services");

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
