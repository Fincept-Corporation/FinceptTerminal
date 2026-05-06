// MAStartupPanel.cpp — startup valuation (Berkus / Scorecard / VC) sub-tab builder for MAModulePanel.
// Method definition split from MAModulePanel.cpp.

#include "screens/ma_analytics/MAModulePanel.h"

#include "services/ma_analytics/MAAnalyticsService.h"
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

using namespace fincept::services::ma;

namespace fincept::screens {

// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_startup_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Berkus ──
    auto* berkus = new QWidget(this);
    auto* bvl = new QVBoxLayout(berkus);
    bvl->setContentsMargins(12, 12, 12, 12);
    bvl->setSpacing(8);

    QStringList berkus_factors = {"Sound Idea", "Prototype", "Quality Team", "Strategic Relationships",
                                  "Product Rollout"};
    for (int i = 0; i < berkus_factors.size(); ++i) {
        auto* spin = make_double_spin(0, 100, 50, 0, "%", berkus);
        double_inputs_[QString("berkus_%1").arg(i)] = spin;
        bvl->addWidget(build_input_row(berkus_factors[i] + " (0-100%)", spin, berkus));
    }

    auto* berkus_run = make_run_button("CALCULATE BERKUS", berkus);
    connect(berkus_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating Berkus...");
        QJsonObject params;
        QJsonArray scores;
        for (int i = 0; i < 5; ++i)
            scores.append(double_inputs_[QString("berkus_%1").arg(i)]->value() / 100.0);
        params["scores"] = scores;
        MAAnalyticsService::instance().calculate_berkus(params);
    });
    bvl->addWidget(berkus_run);
    bvl->addStretch();
    sub_tabs_->addTab(berkus, "Berkus");

    // ── Scorecard ──
    auto* sc = new QWidget(this);
    auto* scvl = new QVBoxLayout(sc);
    scvl->setContentsMargins(12, 12, 12, 12);
    scvl->setSpacing(8);

    auto* sc_stage = new QComboBox(sc);
    sc_stage->addItems({"seed", "early", "growth"});
    sc_stage->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::SMALL));
    combo_inputs_["sc_stage"] = sc_stage;
    scvl->addWidget(build_input_row("Stage", sc_stage, sc));

    QStringList sc_factors = {"Management Team",  "Market Size", "Product/Technology", "Competition", "Marketing/Sales",
                              "Need for Funding", "Other"};
    for (int i = 0; i < sc_factors.size(); ++i) {
        auto* spin = make_double_spin(0.5, 2.0, 1.0, 1, "x", sc);
        double_inputs_[QString("sc_%1").arg(i)] = spin;
        scvl->addWidget(build_input_row(sc_factors[i], spin, sc));
    }

    auto* sc_run = make_run_button("CALCULATE SCORECARD", sc);
    connect(sc_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating Scorecard...");
        QJsonObject params;
        params["stage"] = combo_inputs_["sc_stage"]->currentText();
        QJsonArray assessments;
        for (int i = 0; i < 7; ++i)
            assessments.append(double_inputs_[QString("sc_%1").arg(i)]->value());
        params["assessments"] = assessments;
        MAAnalyticsService::instance().calculate_scorecard(params);
    });
    scvl->addWidget(sc_run);
    scvl->addStretch();
    sub_tabs_->addTab(sc, "Scorecard");

    // ── VC Method ──
    auto* vc = new QWidget(this);
    auto* vc_vl = new QVBoxLayout(vc);
    vc_vl->setContentsMargins(12, 12, 12, 12);
    vc_vl->setSpacing(8);

    auto* vc_exit = make_double_spin(0, 1e12, 50e6, 0, "", vc);
    double_inputs_["vc_exit_metric"] = vc_exit;
    vc_vl->addWidget(build_input_row("Exit Year Metric ($)", vc_exit, vc));

    auto* vc_mult = make_double_spin(0, 100, 8, 1, "x", vc);
    double_inputs_["vc_multiple"] = vc_mult;
    vc_vl->addWidget(build_input_row("Exit Multiple", vc_mult, vc));

    auto* vc_years = make_int_spin(1, 20, 5, vc);
    int_inputs_["vc_years"] = vc_years;
    vc_vl->addWidget(build_input_row("Years to Exit", vc_years, vc));

    auto* vc_invest = make_double_spin(0, 1e12, 5e6, 0, "", vc);
    double_inputs_["vc_investment"] = vc_invest;
    vc_vl->addWidget(build_input_row("Investment ($)", vc_invest, vc));

    auto* vc_run = make_run_button("CALCULATE VC METHOD", vc);
    connect(vc_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating VC Method...");
        QJsonObject params;
        params["exit_metric"] = double_inputs_["vc_exit_metric"]->value();
        params["exit_multiple"] = double_inputs_["vc_multiple"]->value();
        params["years"] = int_inputs_["vc_years"]->value();
        params["investment"] = double_inputs_["vc_investment"]->value();
        MAAnalyticsService::instance().calculate_vc_method(params);
    });
    vc_vl->addWidget(vc_run);
    vc_vl->addStretch();
    sub_tabs_->addTab(vc, "VC Method");

    // ── First Chicago ──
    auto* fc = new QWidget(this);
    auto* fc_vl = new QVBoxLayout(fc);
    fc_vl->setContentsMargins(12, 12, 12, 12);
    fc_vl->setSpacing(8);

    QStringList scenarios = {"Bull", "Base", "Bear"};
    double defaults_prob[] = {25, 50, 25};
    double defaults_val[] = {100e6, 40e6, 10e6};
    for (int i = 0; i < 3; ++i) {
        auto* lbl = new QLabel(scenarios[i].toUpper() + " CASE", fc);
        lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                   "letter-spacing:1px;")
                               .arg(module_.color.name())
                               .arg(ui::fonts::DATA_FAMILY));
        fc_vl->addWidget(lbl);

        auto* prob = make_double_spin(0, 100, defaults_prob[i], 0, "%", fc);
        double_inputs_[QString("fc_prob_%1").arg(i)] = prob;
        fc_vl->addWidget(build_input_row("Probability", prob, fc));

        auto* val = make_double_spin(0, 1e15, defaults_val[i], 0, "", fc);
        double_inputs_[QString("fc_value_%1").arg(i)] = val;
        fc_vl->addWidget(build_input_row("Exit Value ($)", val, fc));
    }

    auto* fc_run = make_run_button("CALCULATE FIRST CHICAGO", fc);
    connect(fc_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating First Chicago...");
        QJsonObject params;
        QJsonArray scenarios;
        for (int i = 0; i < 3; ++i) {
            QJsonObject s;
            s["probability"] = double_inputs_[QString("fc_prob_%1").arg(i)]->value() / 100.0;
            s["exit_value"] = double_inputs_[QString("fc_value_%1").arg(i)]->value();
            scenarios.append(s);
        }
        params["scenarios"] = scenarios;
        MAAnalyticsService::instance().calculate_first_chicago(params);
    });
    fc_vl->addWidget(fc_run);
    fc_vl->addStretch();
    sub_tabs_->addTab(fc, "First Chicago");

    // ── Risk Factor ──
    auto* rf = new QWidget(this);
    auto* rf_vl = new QVBoxLayout(rf);
    rf_vl->setContentsMargins(12, 12, 12, 12);
    rf_vl->setSpacing(8);

    auto* rf_base = make_double_spin(0, 1e12, 3e6, 0, "", rf);
    double_inputs_["rf_base"] = rf_base;
    rf_vl->addWidget(build_input_row("Base Valuation ($)", rf_base, rf));

    QStringList risk_factors = {"Management",        "Stage",         "Legislation", "Manufacturing",
                                "Sales & Marketing", "Funding",       "Competition", "Technology",
                                "Litigation",        "International", "Reputation",  "Exit Opportunity"};
    for (int i = 0; i < risk_factors.size(); ++i) {
        auto* spin = make_int_spin(-2, 2, 0, rf);
        int_inputs_[QString("rf_%1").arg(i)] = spin;
        rf_vl->addWidget(build_input_row(risk_factors[i] + " (-2 to +2)", spin, rf));
    }

    auto* rf_run = make_run_button("CALCULATE RISK FACTOR", rf);
    connect(rf_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating Risk Factor...");
        QJsonObject params;
        params["base_valuation"] = double_inputs_["rf_base"]->value();
        QJsonArray assessments;
        for (int i = 0; i < 12; ++i)
            assessments.append(int_inputs_[QString("rf_%1").arg(i)]->value());
        params["assessments"] = assessments;
        MAAnalyticsService::instance().calculate_risk_factor(params);
    });
    rf_vl->addWidget(rf_run);
    rf_vl->addStretch();
    sub_tabs_->addTab(rf, "Risk Factor");

    // ── Comprehensive ──
    auto* comp = new QWidget(this);
    auto* comp_vl = new QVBoxLayout(comp);
    comp_vl->setContentsMargins(12, 12, 12, 12);
    comp_vl->setSpacing(8);

    auto* comp_hint = new QLabel("Runs all 5 methods with current inputs and returns a consensus range.", comp);
    comp_hint->setWordWrap(true);
    comp_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                 .arg(ui::colors::TEXT_SECONDARY())
                                 .arg(ui::fonts::SMALL)
                                 .arg(ui::fonts::DATA_FAMILY));
    comp_vl->addWidget(comp_hint);

    auto* comp_run = make_run_button("RUN ALL METHODS", comp);
    connect(comp_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running Comprehensive...");
        QJsonObject params;
        // Gather all startup inputs
        QJsonArray berkus_scores;
        for (int i = 0; i < 5; ++i)
            berkus_scores.append(double_inputs_[QString("berkus_%1").arg(i)]->value() / 100.0);
        params["berkus_scores"] = berkus_scores;
        params["vc_exit_metric"] = double_inputs_["vc_exit_metric"]->value();
        params["vc_multiple"] = double_inputs_["vc_multiple"]->value();
        params["vc_years"] = int_inputs_["vc_years"]->value();
        params["vc_investment"] = double_inputs_["vc_investment"]->value();
        params["rf_base"] = double_inputs_["rf_base"]->value();
        MAAnalyticsService::instance().calculate_comprehensive_startup(params);
    });
    comp_vl->addWidget(comp_run);
    comp_vl->addStretch();
    sub_tabs_->addTab(comp, "All Methods");

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
