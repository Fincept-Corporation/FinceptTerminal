// MAFairnessPanel.cpp — fairness opinion (premium / collar) sub-tab builder for MAModulePanel.
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
// MODULE 5: FAIRNESS OPINION
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_fairness_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Fairness Analysis ──
    auto* fa = new QWidget(this);
    auto* fa_vl = new QVBoxLayout(fa);
    fa_vl->setContentsMargins(12, 12, 12, 12);
    fa_vl->setSpacing(8);

    auto* offer = make_double_spin(0, 1e6, 50, 2, "", fa);
    double_inputs_["fo_offer_price"] = offer;
    fa_vl->addWidget(build_input_row("Offer Price ($)", offer, fa));

    auto* dcf_val = make_double_spin(0, 1e6, 48, 2, "", fa);
    double_inputs_["fo_dcf_val"] = dcf_val;
    fa_vl->addWidget(build_input_row("DCF Valuation ($)", dcf_val, fa));

    auto* comps_val = make_double_spin(0, 1e6, 45, 2, "", fa);
    double_inputs_["fo_comps_val"] = comps_val;
    fa_vl->addWidget(build_input_row("Comps Valuation ($)", comps_val, fa));

    auto* prec_val = make_double_spin(0, 1e6, 52, 2, "", fa);
    double_inputs_["fo_prec_val"] = prec_val;
    fa_vl->addWidget(build_input_row("Precedent Valuation ($)", prec_val, fa));

    auto* fa_run = make_run_button("GENERATE FAIRNESS OPINION", fa);
    connect(fa_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Generating Fairness Opinion...");
        QJsonObject params;
        params["offer_price"] = double_inputs_["fo_offer_price"]->value();
        QJsonArray methods;
        QJsonObject dcf, comps, prec;
        dcf["method"] = "DCF";
        dcf["valuation"] = double_inputs_["fo_dcf_val"]->value();
        comps["method"] = "Trading Comps";
        comps["valuation"] = double_inputs_["fo_comps_val"]->value();
        prec["method"] = "Precedent Txns";
        prec["valuation"] = double_inputs_["fo_prec_val"]->value();
        methods.append(dcf);
        methods.append(comps);
        methods.append(prec);
        params["methods"] = methods;
        MAAnalyticsService::instance().generate_fairness_opinion(params);
    });
    fa_vl->addWidget(fa_run);
    fa_vl->addStretch();
    sub_tabs_->addTab(fa, "Fairness Analysis");

    // ── Premium Analysis ──
    auto* pa = new QWidget(this);
    auto* pa_vl = new QVBoxLayout(pa);
    pa_vl->setContentsMargins(12, 12, 12, 12);
    pa_vl->setSpacing(8);

    auto* pa_offer = make_double_spin(0, 1e6, 50, 2, "", pa);
    double_inputs_["pa_offer"] = pa_offer;
    pa_vl->addWidget(build_input_row("Offer Price ($)", pa_offer, pa));

    auto* pa_1d = make_double_spin(0, 1e6, 42, 2, "", pa);
    double_inputs_["pa_price_1d"] = pa_1d;
    pa_vl->addWidget(build_input_row("1-Day Prior Price ($)", pa_1d, pa));

    auto* pa_4w = make_double_spin(0, 1e6, 38, 2, "", pa);
    double_inputs_["pa_price_4w"] = pa_4w;
    pa_vl->addWidget(build_input_row("4-Week Avg Price ($)", pa_4w, pa));

    auto* pa_52w = make_double_spin(0, 1e6, 35, 2, "", pa);
    double_inputs_["pa_price_52w"] = pa_52w;
    pa_vl->addWidget(build_input_row("52-Week High ($)", pa_52w, pa));

    auto* pa_run = make_run_button("ANALYZE PREMIUM", pa);
    connect(pa_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing Premium...");
        QJsonObject params;
        params["offer_price"] = double_inputs_["pa_offer"]->value();
        params["price_1d"] = double_inputs_["pa_price_1d"]->value();
        params["price_4w"] = double_inputs_["pa_price_4w"]->value();
        params["price_52w"] = double_inputs_["pa_price_52w"]->value();
        MAAnalyticsService::instance().analyze_premium(params);
    });
    pa_vl->addWidget(pa_run);
    pa_vl->addStretch();
    sub_tabs_->addTab(pa, "Premium Analysis");

    // ── Process Quality ──
    auto* pq = new QWidget(this);
    auto* pq_vl = new QVBoxLayout(pq);
    pq_vl->setContentsMargins(12, 12, 12, 12);
    pq_vl->setSpacing(8);

    QStringList pq_factors = {"Board Independence",  "Special Committee", "Independent Advisor", "Market Check",
                              "Negotiation Process", "Due Diligence",     "Disclosure Quality",  "Timing Adequacy"};
    for (int i = 0; i < pq_factors.size(); ++i) {
        auto* spin = make_int_spin(1, 5, 3, pq);
        int_inputs_[QString("pq_%1").arg(i)] = spin;
        pq_vl->addWidget(build_input_row(pq_factors[i] + " (1-5)", spin, pq));
    }

    auto* pq_run = make_run_button("ASSESS PROCESS QUALITY", pq);
    connect(pq_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Assessing Process Quality...");
        QJsonObject params;
        QJsonArray factors;
        for (int i = 0; i < 8; ++i)
            factors.append(int_inputs_[QString("pq_%1").arg(i)]->value());
        params["factors"] = factors;
        MAAnalyticsService::instance().assess_process_quality(params);
    });
    pq_vl->addWidget(pq_run);
    pq_vl->addStretch();
    sub_tabs_->addTab(pq, "Process Quality");

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
