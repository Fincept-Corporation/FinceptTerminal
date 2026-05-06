// MAMergerPanel.cpp — accretion/dilution + synergy analysis sub-tab builder.
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
// MODULE 2: MERGER ANALYSIS
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_merger_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Accretion/Dilution ──
    auto* ad = new QWidget(this);
    auto* ad_vl = new QVBoxLayout(ad);
    ad_vl->setContentsMargins(12, 12, 12, 12);
    ad_vl->setSpacing(8);

    auto* sec_acq = new QLabel("ACQUIRER", ad);
    sec_acq->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                   "letter-spacing:1px;")
                               .arg(module_.color.name())
                               .arg(ui::fonts::DATA_FAMILY));
    ad_vl->addWidget(sec_acq);

    auto* acq_rev = make_double_spin(0, 1e15, 1e9, 0, "", ad);
    double_inputs_["acq_revenue"] = acq_rev;
    ad_vl->addWidget(build_input_row("Revenue ($)", acq_rev, ad));

    auto* acq_ebitda = make_double_spin(0, 1e15, 250e6, 0, "", ad);
    double_inputs_["acq_ebitda"] = acq_ebitda;
    ad_vl->addWidget(build_input_row("EBITDA ($)", acq_ebitda, ad));

    auto* acq_ni = make_double_spin(-1e15, 1e15, 150e6, 0, "", ad);
    double_inputs_["acq_net_income"] = acq_ni;
    ad_vl->addWidget(build_input_row("Net Income ($)", acq_ni, ad));

    auto* acq_shares = make_double_spin(0, 1e12, 200e6, 0, "", ad);
    double_inputs_["acq_shares"] = acq_shares;
    ad_vl->addWidget(build_input_row("Shares Outstanding", acq_shares, ad));

    auto* sec_tgt = new QLabel("TARGET", ad);
    sec_tgt->setStyleSheet(sec_acq->styleSheet());
    ad_vl->addWidget(sec_tgt);

    auto* tgt_rev = make_double_spin(0, 1e15, 300e6, 0, "", ad);
    double_inputs_["tgt_revenue"] = tgt_rev;
    ad_vl->addWidget(build_input_row("Revenue ($)", tgt_rev, ad));

    auto* tgt_ni = make_double_spin(-1e15, 1e15, 40e6, 0, "", ad);
    double_inputs_["tgt_net_income"] = tgt_ni;
    ad_vl->addWidget(build_input_row("Net Income ($)", tgt_ni, ad));

    auto* sec_deal = new QLabel("DEAL TERMS", ad);
    sec_deal->setStyleSheet(sec_acq->styleSheet());
    ad_vl->addWidget(sec_deal);

    auto* deal_val = make_double_spin(0, 1e15, 500e6, 0, "", ad);
    double_inputs_["deal_value"] = deal_val;
    ad_vl->addWidget(build_input_row("Deal Value ($)", deal_val, ad));

    auto* deal_prem = make_double_spin(0, 200, 30, 1, "%", ad);
    double_inputs_["deal_premium"] = deal_prem;
    ad_vl->addWidget(build_input_row("Premium", deal_prem, ad));

    auto* cash_pct = make_double_spin(0, 100, 60, 0, "%", ad);
    double_inputs_["cash_pct"] = cash_pct;
    ad_vl->addWidget(build_input_row("Cash %", cash_pct, ad));

    auto* ad_run = make_run_button("RUN ACCRETION/DILUTION", ad);
    connect(ad_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running Accretion/Dilution...");
        QJsonObject params;
        QJsonObject acquirer;
        acquirer["revenue"] = double_inputs_["acq_revenue"]->value();
        acquirer["ebitda"] = double_inputs_["acq_ebitda"]->value();
        acquirer["net_income"] = double_inputs_["acq_net_income"]->value();
        acquirer["shares"] = double_inputs_["acq_shares"]->value();
        params["acquirer"] = acquirer;
        QJsonObject target;
        target["revenue"] = double_inputs_["tgt_revenue"]->value();
        target["net_income"] = double_inputs_["tgt_net_income"]->value();
        params["target"] = target;
        params["deal_value"] = double_inputs_["deal_value"]->value();
        params["premium"] = double_inputs_["deal_premium"]->value() / 100.0;
        params["cash_pct"] = double_inputs_["cash_pct"]->value() / 100.0;
        MAAnalyticsService::instance().calculate_accretion_dilution(params);
    });
    ad_vl->addWidget(ad_run);
    ad_vl->addStretch();
    sub_tabs_->addTab(ad, "Accretion/Dilution");

    // ── Synergies ──
    auto* syn = new QWidget(this);
    auto* syn_vl = new QVBoxLayout(syn);
    syn_vl->setContentsMargins(12, 12, 12, 12);
    syn_vl->setSpacing(8);

    auto* rev_syn = make_double_spin(0, 100, 5, 1, "%", syn);
    double_inputs_["rev_synergy_pct"] = rev_syn;
    syn_vl->addWidget(build_input_row("Revenue Synergy %", rev_syn, syn));

    auto* cost_syn = make_double_spin(0, 100, 10, 1, "%", syn);
    double_inputs_["cost_synergy_pct"] = cost_syn;
    syn_vl->addWidget(build_input_row("Cost Synergy %", cost_syn, syn));

    auto* integ_cost = make_double_spin(0, 1e12, 50e6, 0, "", syn);
    double_inputs_["integration_cost"] = integ_cost;
    syn_vl->addWidget(build_input_row("Integration Cost ($)", integ_cost, syn));

    auto* syn_disc = make_double_spin(0, 50, 10, 1, "%", syn);
    double_inputs_["synergy_discount"] = syn_disc;
    syn_vl->addWidget(build_input_row("Discount Rate", syn_disc, syn));

    auto* syn_run = make_run_button("VALUE SYNERGIES", syn);
    connect(syn_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Valuing Synergies...");
        QJsonObject params;
        params["revenue_synergy_pct"] = double_inputs_["rev_synergy_pct"]->value() / 100.0;
        params["cost_synergy_pct"] = double_inputs_["cost_synergy_pct"]->value() / 100.0;
        params["integration_cost"] = double_inputs_["integration_cost"]->value();
        params["discount_rate"] = double_inputs_["synergy_discount"]->value() / 100.0;
        MAAnalyticsService::instance().value_synergies_dcf(params);
    });
    syn_vl->addWidget(syn_run);
    syn_vl->addStretch();
    sub_tabs_->addTab(syn, "Synergies");

    // ── Pro Forma ──
    auto* pf = new QWidget(this);
    auto* pf_vl = new QVBoxLayout(pf);
    pf_vl->setContentsMargins(12, 12, 12, 12);
    pf_vl->setSpacing(8);

    auto* pf_years = make_int_spin(1, 10, 3, pf);
    int_inputs_["pf_years"] = pf_years;
    pf_vl->addWidget(build_input_row("Years to Project", pf_years, pf));

    auto* pf_rev_g = make_double_spin(-50, 200, 8, 1, "%", pf);
    double_inputs_["pf_rev_growth"] = pf_rev_g;
    pf_vl->addWidget(build_input_row("Revenue Growth", pf_rev_g, pf));

    auto* pf_run = make_run_button("BUILD PRO FORMA", pf);
    connect(pf_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Building Pro Forma...");
        QJsonObject params;
        params["years"] = int_inputs_["pf_years"]->value();
        params["revenue_growth"] = double_inputs_["pf_rev_growth"]->value() / 100.0;
        // Re-use acquirer/target inputs from accretion tab
        QJsonObject acquirer;
        acquirer["revenue"] = double_inputs_["acq_revenue"]->value();
        acquirer["ebitda"] = double_inputs_["acq_ebitda"]->value();
        acquirer["net_income"] = double_inputs_["acq_net_income"]->value();
        params["acquirer"] = acquirer;
        QJsonObject target;
        target["revenue"] = double_inputs_["tgt_revenue"]->value();
        target["net_income"] = double_inputs_["tgt_net_income"]->value();
        params["target"] = target;
        MAAnalyticsService::instance().build_pro_forma(params);
    });
    pf_vl->addWidget(pf_run);
    pf_vl->addStretch();
    sub_tabs_->addTab(pf, "Pro Forma");

    // ── Sources & Uses ──
    auto* su = new QWidget(this);
    auto* su_vl = new QVBoxLayout(su);
    su_vl->setContentsMargins(12, 12, 12, 12);
    su_vl->setSpacing(8);

    auto* su_cash = make_double_spin(0, 1e15, 200e6, 0, "", su);
    double_inputs_["su_cash"] = su_cash;
    su_vl->addWidget(build_input_row("Cash on Hand ($)", su_cash, su));

    auto* su_debt = make_double_spin(0, 1e15, 250e6, 0, "", su);
    double_inputs_["su_new_debt"] = su_debt;
    su_vl->addWidget(build_input_row("New Debt ($)", su_debt, su));

    auto* su_stock = make_double_spin(0, 1e15, 50e6, 0, "", su);
    double_inputs_["su_stock"] = su_stock;
    su_vl->addWidget(build_input_row("Stock Issuance ($)", su_stock, su));

    auto* su_run = make_run_button("CALCULATE SOURCES & USES", su);
    connect(su_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating Sources & Uses...");
        QJsonObject params;
        params["deal_value"] = double_inputs_["deal_value"]->value();
        params["cash"] = double_inputs_["su_cash"]->value();
        params["new_debt"] = double_inputs_["su_new_debt"]->value();
        params["stock_issuance"] = double_inputs_["su_stock"]->value();
        MAAnalyticsService::instance().calculate_sources_uses(params);
    });
    su_vl->addWidget(su_run);
    su_vl->addStretch();
    sub_tabs_->addTab(su, "Sources & Uses");

    // ── Contribution Analysis ──
    auto* contrib = new QWidget(this);
    auto* contrib_vl = new QVBoxLayout(contrib);
    contrib_vl->setContentsMargins(12, 12, 12, 12);
    contrib_vl->setSpacing(8);

    auto* contrib_hint = new QLabel("Calculates each party's % contribution to the combined entity.", contrib);
    contrib_hint->setWordWrap(true);
    contrib_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                    .arg(ui::colors::TEXT_SECONDARY())
                                    .arg(ui::fonts::SMALL)
                                    .arg(ui::fonts::DATA_FAMILY));
    contrib_vl->addWidget(contrib_hint);

    auto* contrib_own = make_double_spin(0, 100, 70, 1, "%", contrib);
    double_inputs_["contrib_ownership"] = contrib_own;
    contrib_vl->addWidget(build_input_row("Acquirer Ownership %", contrib_own, contrib));

    auto* contrib_run = make_run_button("ANALYZE CONTRIBUTION", contrib);
    connect(contrib_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing Contribution...");
        QJsonObject params;
        QJsonObject acquirer;
        acquirer["revenue"] = double_inputs_["acq_revenue"]->value();
        acquirer["ebitda"] = double_inputs_["acq_ebitda"]->value();
        acquirer["net_income"] = double_inputs_["acq_net_income"]->value();
        params["acquirer"] = acquirer;
        QJsonObject target;
        target["revenue"] = double_inputs_["tgt_revenue"]->value();
        target["net_income"] = double_inputs_["tgt_net_income"]->value();
        params["target"] = target;
        params["ownership_split"] = double_inputs_["contrib_ownership"]->value() / 100.0;
        MAAnalyticsService::instance().analyze_contribution(params);
    });
    contrib_vl->addWidget(contrib_run);
    contrib_vl->addStretch();
    sub_tabs_->addTab(contrib, "Contribution");

    // ── Payment Structure ──
    auto* pay_str = new QWidget(this);
    auto* pay_vl = new QVBoxLayout(pay_str);
    pay_vl->setContentsMargins(12, 12, 12, 12);
    pay_vl->setSpacing(8);

    auto* pay_price = make_double_spin(0, 1e15, 500e6, 0, "", pay_str);
    double_inputs_["pay_purchase_price"] = pay_price;
    pay_vl->addWidget(build_input_row("Purchase Price ($)", pay_price, pay_str));

    auto* pay_cash = make_double_spin(0, 100, 60, 0, "%", pay_str);
    double_inputs_["pay_cash_pct"] = pay_cash;
    pay_vl->addWidget(build_input_row("Cash %", pay_cash, pay_str));

    auto* pay_acq_cash = make_double_spin(0, 1e15, 200e6, 0, "", pay_str);
    double_inputs_["pay_cash_on_hand"] = pay_acq_cash;
    pay_vl->addWidget(build_input_row("Cash on Hand ($)", pay_acq_cash, pay_str));

    auto* pay_new_debt = make_double_spin(0, 1e15, 100e6, 0, "", pay_str);
    double_inputs_["pay_new_debt"] = pay_new_debt;
    pay_vl->addWidget(build_input_row("New Debt ($)", pay_new_debt, pay_str));

    auto* pay_run = make_run_button("ANALYZE PAYMENT STRUCTURE", pay_str);
    connect(pay_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing Payment...");
        QJsonObject params;
        params["purchase_price"] = double_inputs_["pay_purchase_price"]->value();
        params["cash_pct"] = double_inputs_["pay_cash_pct"]->value() / 100.0;
        params["cash_on_hand"] = double_inputs_["pay_cash_on_hand"]->value();
        params["new_debt"] = double_inputs_["pay_new_debt"]->value();
        MAAnalyticsService::instance().analyze_payment_structure(params);
    });
    pay_vl->addWidget(pay_run);
    pay_vl->addStretch();
    sub_tabs_->addTab(pay_str, "Payment");

    // ── Earnout ──
    auto* earn = new QWidget(this);
    auto* earn_vl = new QVBoxLayout(earn);
    earn_vl->setContentsMargins(12, 12, 12, 12);
    earn_vl->setSpacing(8);

    auto* earn_base = make_double_spin(0, 1e15, 50e6, 0, "", earn);
    double_inputs_["earn_base_amount"] = earn_base;
    earn_vl->addWidget(build_input_row("Earnout Amount ($)", earn_base, earn));

    auto* earn_threshold = make_double_spin(0, 1e15, 100e6, 0, "", earn);
    double_inputs_["earn_threshold"] = earn_threshold;
    earn_vl->addWidget(build_input_row("Revenue Threshold ($)", earn_threshold, earn));

    auto* earn_period = make_int_spin(1, 10, 3, earn);
    int_inputs_["earn_period"] = earn_period;
    earn_vl->addWidget(build_input_row("Earnout Period (yrs)", earn_period, earn));

    auto* earn_prob = make_double_spin(0, 100, 60, 0, "%", earn);
    double_inputs_["earn_probability"] = earn_prob;
    earn_vl->addWidget(build_input_row("Achievement Probability", earn_prob, earn));

    auto* earn_run = make_run_button("VALUE EARNOUT", earn);
    connect(earn_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Valuing Earnout...");
        QJsonObject params;
        params["earnout_amount"] = double_inputs_["earn_base_amount"]->value();
        params["revenue_threshold"] = double_inputs_["earn_threshold"]->value();
        params["period"] = int_inputs_["earn_period"]->value();
        params["probability"] = double_inputs_["earn_probability"]->value() / 100.0;
        MAAnalyticsService::instance().value_earnout(params);
    });
    earn_vl->addWidget(earn_run);
    earn_vl->addStretch();
    sub_tabs_->addTab(earn, "Earnout");

    // ── Exchange Ratio ──
    auto* exch = new QWidget(this);
    auto* exch_vl = new QVBoxLayout(exch);
    exch_vl->setContentsMargins(12, 12, 12, 12);
    exch_vl->setSpacing(8);

    auto* exch_acq_price = make_double_spin(0, 1e6, 100, 2, "", exch);
    double_inputs_["exch_acq_price"] = exch_acq_price;
    exch_vl->addWidget(build_input_row("Acquirer Share Price ($)", exch_acq_price, exch));

    auto* exch_tgt_price = make_double_spin(0, 1e6, 50, 2, "", exch);
    double_inputs_["exch_tgt_price"] = exch_tgt_price;
    exch_vl->addWidget(build_input_row("Target Share Price ($)", exch_tgt_price, exch));

    auto* exch_premium = make_double_spin(0, 200, 30, 1, "%", exch);
    double_inputs_["exch_premium"] = exch_premium;
    exch_vl->addWidget(build_input_row("Offer Premium", exch_premium, exch));

    auto* exch_run = make_run_button("CALCULATE EXCHANGE RATIO", exch);
    connect(exch_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Calculating Exchange Ratio...");
        QJsonObject params;
        params["acquirer_price"] = double_inputs_["exch_acq_price"]->value();
        params["target_price"] = double_inputs_["exch_tgt_price"]->value();
        params["premium"] = double_inputs_["exch_premium"]->value() / 100.0;
        MAAnalyticsService::instance().calculate_exchange_ratio(params);
    });
    exch_vl->addWidget(exch_run);
    exch_vl->addStretch();
    sub_tabs_->addTab(exch, "Exchange Ratio");

    // ── Collar Mechanism ──
    auto* collar = new QWidget(this);
    auto* collar_vl = new QVBoxLayout(collar);
    collar_vl->setContentsMargins(12, 12, 12, 12);
    collar_vl->setSpacing(8);

    auto* collar_floor = make_double_spin(0, 1e6, 85, 2, "", collar);
    double_inputs_["collar_floor"] = collar_floor;
    collar_vl->addWidget(build_input_row("Price Floor ($)", collar_floor, collar));

    auto* collar_cap = make_double_spin(0, 1e6, 115, 2, "", collar);
    double_inputs_["collar_cap"] = collar_cap;
    collar_vl->addWidget(build_input_row("Price Cap ($)", collar_cap, collar));

    auto* collar_ratio = make_double_spin(0, 10, 0.5, 3, "", collar);
    double_inputs_["collar_base_ratio"] = collar_ratio;
    collar_vl->addWidget(build_input_row("Base Exchange Ratio", collar_ratio, collar));

    auto* collar_run = make_run_button("ANALYZE COLLAR", collar);
    connect(collar_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing Collar...");
        QJsonObject params;
        params["floor_price"] = double_inputs_["collar_floor"]->value();
        params["cap_price"] = double_inputs_["collar_cap"]->value();
        params["base_ratio"] = double_inputs_["collar_base_ratio"]->value();
        MAAnalyticsService::instance().analyze_collar_mechanism(params);
    });
    collar_vl->addWidget(collar_run);
    collar_vl->addStretch();
    sub_tabs_->addTab(collar, "Collar");

    // ── CVR (Contingent Value Rights) ──
    auto* cvr = new QWidget(this);
    auto* cvr_vl = new QVBoxLayout(cvr);
    cvr_vl->setContentsMargins(12, 12, 12, 12);
    cvr_vl->setSpacing(8);

    auto* cvr_type = new QComboBox(cvr);
    cvr_type->addItems({"milestone", "revenue", "regulatory"});
    cvr_type->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::SMALL));
    combo_inputs_["cvr_type"] = cvr_type;
    cvr_vl->addWidget(build_input_row("CVR Type", cvr_type, cvr));

    auto* cvr_max = make_double_spin(0, 1e15, 10e6, 0, "", cvr);
    double_inputs_["cvr_max_payout"] = cvr_max;
    cvr_vl->addWidget(build_input_row("Max Payout ($)", cvr_max, cvr));

    auto* cvr_prob = make_double_spin(0, 100, 50, 0, "%", cvr);
    double_inputs_["cvr_probability"] = cvr_prob;
    cvr_vl->addWidget(build_input_row("Achievement Prob", cvr_prob, cvr));

    auto* cvr_expiry = make_int_spin(1, 10, 3, cvr);
    int_inputs_["cvr_expiry"] = cvr_expiry;
    cvr_vl->addWidget(build_input_row("Expiry (yrs)", cvr_expiry, cvr));

    auto* cvr_run = make_run_button("VALUE CVR", cvr);
    connect(cvr_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Valuing CVR...");
        QJsonObject params;
        params["type"] = combo_inputs_["cvr_type"]->currentText();
        params["max_payout"] = double_inputs_["cvr_max_payout"]->value();
        params["probability"] = double_inputs_["cvr_probability"]->value() / 100.0;
        params["expiry_years"] = int_inputs_["cvr_expiry"]->value();
        MAAnalyticsService::instance().value_cvr(params);
    });
    cvr_vl->addWidget(cvr_run);
    cvr_vl->addStretch();
    sub_tabs_->addTab(cvr, "CVR");

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}


} // namespace fincept::screens
