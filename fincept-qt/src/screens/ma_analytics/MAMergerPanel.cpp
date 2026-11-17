// MAMergerPanel.cpp — accretion/dilution + synergy analysis sub-tab builder.
// Method definition split from MAModulePanel.cpp.

#include "screens/ma_analytics/MAModulePanel.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

using namespace fincept::services::ma;

namespace fincept::screens {

using namespace fincept::services::ma;

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 2: MERGER ANALYSIS
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_merger_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // Start from two companies: fills both sides of the accretion model and the share prices of the deal
    // structure tabs (MA-4).
    vl->addWidget(build_company_strip(w));

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Accretion/Dilution ──
    add_lazy_sub_tab(QT_TR_NOOP("Accretion/Dilution"), [this](QWidget* ad) {
        auto* ad_vl = new QVBoxLayout(ad);
        ad_vl->setContentsMargins(12, 12, 12, 12);
        ad_vl->setSpacing(8);

        auto* sec_acq = new QLabel(tr("ACQUIRER"), ad);
        sec_acq->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; font-family:%2;"
                                       "letter-spacing:1px;")
                                   .arg(ui::colors::legible(module_.color))
                                   .arg(ui::fonts::DATA_FAMILY));
        ad_vl->addWidget(sec_acq);

        auto* acq_rev = make_double_spin(0, 1e15, 1e9, 0, "", ad);
        double_inputs_["acq_revenue"] = acq_rev;
        ad_vl->addWidget(build_input_row(tr("Revenue") + " ($)", acq_rev, ad));

        auto* acq_ebitda = make_double_spin(0, 1e15, 250e6, 0, "", ad);
        double_inputs_["acq_ebitda"] = acq_ebitda;
        ad_vl->addWidget(build_input_row(tr("EBITDA") + " ($)", acq_ebitda, ad));

        auto* acq_ni = make_double_spin(-1e15, 1e15, 150e6, 0, "", ad);
        double_inputs_["acq_net_income"] = acq_ni;
        ad_vl->addWidget(build_input_row(tr("Net Income") + " ($)", acq_ni, ad));

        auto* acq_shares = make_double_spin(0, 1e12, 200e6, 0, "", ad);
        double_inputs_["acq_shares"] = acq_shares;
        ad_vl->addWidget(build_input_row(tr("Shares Outstanding"), acq_shares, ad));

        // The price new shares are issued at: needed whenever part of the price is paid in stock.
        auto* acq_price = make_double_spin(0, 1e7, 15, 2, "", ad);
        acq_price->setSpecialValueText(tr("not set"));
        double_inputs_["acq_share_price"] = acq_price;
        ad_vl->addWidget(build_input_row(tr("Share Price") + " ($)", acq_price, ad));

        auto* sec_tgt = new QLabel(tr("TARGET"), ad);
        sec_tgt->setStyleSheet(sec_acq->styleSheet());
        ad_vl->addWidget(sec_tgt);

        auto* tgt_rev = make_double_spin(0, 1e15, 300e6, 0, "", ad);
        double_inputs_["tgt_revenue"] = tgt_rev;
        ad_vl->addWidget(build_input_row(tr("Revenue") + " ($)", tgt_rev, ad));

        auto* tgt_ni = make_double_spin(-1e15, 1e15, 40e6, 0, "", ad);
        double_inputs_["tgt_net_income"] = tgt_ni;
        ad_vl->addWidget(build_input_row(tr("Net Income") + " ($)", tgt_ni, ad));

        auto* sec_deal = new QLabel(tr("DEAL TERMS"), ad);
        sec_deal->setStyleSheet(sec_acq->styleSheet());
        ad_vl->addWidget(sec_deal);

        auto* deal_val = make_double_spin(0, 1e15, 500e6, 0, "", ad);
        double_inputs_["deal_value"] = deal_val;
        ad_vl->addWidget(build_input_row(tr("Deal Value") + " ($)", deal_val, ad));

        auto* deal_prem = make_double_spin(0, 200, 30, 1, "%", ad);
        double_inputs_["deal_premium"] = deal_prem;
        ad_vl->addWidget(build_input_row(tr("Premium"), deal_prem, ad));

        auto* cash_pct = make_double_spin(0, 100, 60, 0, "%", ad);
        double_inputs_["cash_pct"] = cash_pct;
        ad_vl->addWidget(build_input_row(tr("Cash %"), cash_pct, ad));

        // Financing and synergies: everything the pro forma EPS depends on is on screen (MA-7).
        auto* sec_fin = new QLabel(tr("FINANCING"), ad);
        sec_fin->setStyleSheet(sec_acq->styleSheet());
        ad_vl->addWidget(sec_fin);
        auto* cash_used = make_double_spin(0, 1e15, 0, 0, "", ad);
        cash_used->setToolTip(tr("Part of the cash consideration paid from the acquirer's own cash; the rest is "
                                 "borrowed at the cost of new debt"));
        double_inputs_["acq_cash_on_hand"] = cash_used;
        ad_vl->addWidget(build_input_row(tr("Acquirer Cash Used") + " ($)", cash_used, ad));
        auto* cod = make_double_spin(0, 50, 6, 2, "%", ad);
        double_inputs_["acq_cost_of_debt"] = cod;
        ad_vl->addWidget(build_input_row(tr("Cost of New Debt"), cod, ad));
        auto* yield = make_double_spin(0, 50, 4, 2, "%", ad);
        yield->setToolTip(tr("Pre-tax interest the acquirer's cash was earning (given up when it is spent)"));
        double_inputs_["acq_cash_yield"] = yield;
        ad_vl->addWidget(build_input_row(tr("Interest Earned on Cash"), yield, ad));
        auto* tax = make_double_spin(0, 60, 21, 1, "%", ad);
        double_inputs_["acq_tax_rate"] = tax;
        ad_vl->addWidget(build_input_row(tr("Tax Rate"), tax, ad));

        auto* sec_syn = new QLabel(tr("SYNERGIES AND DEAL COSTS"), ad);
        sec_syn->setStyleSheet(sec_acq->styleSheet());
        ad_vl->addWidget(sec_syn);
        auto* syn = make_double_spin(0, 1e15, 0, 0, "", ad);
        syn->setToolTip(tr("Pre-tax synergies a year once fully realised"));
        double_inputs_["acq_synergies"] = syn;
        ad_vl->addWidget(build_input_row(tr("Run-rate Synergies") + " ($)", syn, ad));
        const char* phase_labels[] = {QT_TR_NOOP("Synergies Realised Year 1"), QT_TR_NOOP("Synergies Realised Year 2"),
                                      QT_TR_NOOP("Synergies Realised Year 3")};
        const double phase_defaults[] = {25, 75, 100};
        for (int i = 0; i < 3; ++i) {
            auto* ph = make_double_spin(0, 100, phase_defaults[i], 0, "%", ad);
            double_inputs_[QStringLiteral("acq_syn_phase_%1").arg(i + 1)] = ph;
            ad_vl->addWidget(build_input_row(tr(phase_labels[i]), ph, ad));
        }
        auto* integ1 = make_double_spin(0, 1e15, 0, 0, "", ad);
        double_inputs_["acq_integration_y1"] = integ1;
        ad_vl->addWidget(build_input_row(tr("Integration Costs Year 1") + " ($)", integ1, ad));
        auto* integ2 = make_double_spin(0, 1e15, 0, 0, "", ad);
        double_inputs_["acq_integration_y2"] = integ2;
        ad_vl->addWidget(build_input_row(tr("Integration Costs Year 2") + " ($)", integ2, ad));
        auto* amort = make_double_spin(0, 1e15, 0, 0, "", ad);
        amort->setToolTip(tr("Yearly amortisation of the intangible assets written up in the deal (pre-tax)"));
        double_inputs_["acq_amortization"] = amort;
        ad_vl->addWidget(build_input_row(tr("New Intangibles Amortisation") + " ($)", amort, ad));
        auto* growth = make_double_spin(-50, 100, 0, 1, "%", ad);
        growth->setToolTip(tr("Yearly net income growth of both companies for years 2 and 3 (0 = flat)"));
        double_inputs_["acq_earnings_growth"] = growth;
        ad_vl->addWidget(build_input_row(tr("Earnings Growth (years 2-3)"), growth, ad));

        auto* ad_run = make_run_button(tr("RUN ACCRETION/DILUTION"), ad);
        connect(ad_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Running Accretion/Dilution..."));
            QJsonObject params;
            QJsonObject acquirer;
            acquirer["revenue"] = double_inputs_["acq_revenue"]->value();
            acquirer["ebitda"] = double_inputs_["acq_ebitda"]->value();
            acquirer["net_income"] = double_inputs_["acq_net_income"]->value();
            acquirer["shares"] = double_inputs_["acq_shares"]->value();
            if (double_inputs_["acq_share_price"]->value() > 0)
                acquirer["share_price"] = double_inputs_["acq_share_price"]->value();
            params["acquirer"] = acquirer;
            QJsonObject target;
            target["revenue"] = double_inputs_["tgt_revenue"]->value();
            target["net_income"] = double_inputs_["tgt_net_income"]->value();
            params["target"] = target;
            params["deal_value"] = double_inputs_["deal_value"]->value();
            params["premium"] = double_inputs_["deal_premium"]->value() / 100.0;
            params["cash_pct"] = double_inputs_["cash_pct"]->value() / 100.0;
            params["cash_from_balance_sheet"] = double_inputs_["acq_cash_on_hand"]->value();
            params["cost_of_debt"] = double_inputs_["acq_cost_of_debt"]->value() / 100.0;
            params["cash_yield"] = double_inputs_["acq_cash_yield"]->value() / 100.0;
            params["tax_rate"] = double_inputs_["acq_tax_rate"]->value() / 100.0;
            params["synergies"] = double_inputs_["acq_synergies"]->value();
            params["synergy_phase"] = QJsonArray{double_inputs_["acq_syn_phase_1"]->value() / 100.0,
                                                 double_inputs_["acq_syn_phase_2"]->value() / 100.0,
                                                 double_inputs_["acq_syn_phase_3"]->value() / 100.0};
            params["integration_costs_y1"] = double_inputs_["acq_integration_y1"]->value();
            params["integration_costs_y2"] = double_inputs_["acq_integration_y2"]->value();
            params["amortization"] = double_inputs_["acq_amortization"]->value();
            params["earnings_growth"] = double_inputs_["acq_earnings_growth"]->value() / 100.0;
            MAAnalyticsService::instance().calculate_accretion_dilution(params);
        });
        ad_vl->addWidget(ad_run);
        ad_vl->addStretch();
    });

    // ── Synergies ──
    add_lazy_sub_tab(QT_TR_NOOP("Synergies"), [this](QWidget* syn) {
        auto* syn_vl = new QVBoxLayout(syn);
        syn_vl->setContentsMargins(12, 12, 12, 12);
        syn_vl->setSpacing(8);

        auto* rev_syn = make_double_spin(0, 100, 5, 1, "%", syn);
        double_inputs_["rev_synergy_pct"] = rev_syn;
        syn_vl->addWidget(build_input_row(tr("Revenue Synergy %"), rev_syn, syn));

        auto* cost_syn = make_double_spin(0, 100, 10, 1, "%", syn);
        double_inputs_["cost_synergy_pct"] = cost_syn;
        syn_vl->addWidget(build_input_row(tr("Cost Synergy %"), cost_syn, syn));

        auto* integ_cost = make_double_spin(0, 1e12, 50e6, 0, "", syn);
        double_inputs_["integration_cost"] = integ_cost;
        syn_vl->addWidget(build_input_row(tr("Integration Cost") + " ($)", integ_cost, syn));

        auto* syn_disc = make_double_spin(0, 50, 10, 1, "%", syn);
        double_inputs_["synergy_discount"] = syn_disc;
        syn_vl->addWidget(build_input_row(tr("Discount Rate"), syn_disc, syn));

        auto* syn_run = make_run_button(tr("VALUE SYNERGIES"), syn);
        connect(syn_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Valuing Synergies..."));
            QJsonObject params;
            params["revenue_synergy_pct"] = double_inputs_["rev_synergy_pct"]->value() / 100.0;
            params["cost_synergy_pct"] = double_inputs_["cost_synergy_pct"]->value() / 100.0;
            params["integration_cost"] = double_inputs_["integration_cost"]->value();
            params["discount_rate"] = double_inputs_["synergy_discount"]->value() / 100.0;
            // The synergy percentages need a base to apply to: the target's revenue from the Accretion/Dilution tab.
            params["target_revenue"] = double_inputs_["tgt_revenue"]->value();
            MAAnalyticsService::instance().value_synergies_dcf(params);
        });
        syn_vl->addWidget(syn_run);
        syn_vl->addStretch();
    });

    // ── Pro Forma ──
    add_lazy_sub_tab(QT_TR_NOOP("Pro Forma"), [this](QWidget* pf) {
        auto* pf_vl = new QVBoxLayout(pf);
        pf_vl->setContentsMargins(12, 12, 12, 12);
        pf_vl->setSpacing(8);

        auto* pf_years = make_int_spin(1, 10, 3, pf);
        int_inputs_["pf_years"] = pf_years;
        pf_vl->addWidget(build_input_row(tr("Years to Project"), pf_years, pf));

        auto* pf_rev_g = make_double_spin(-50, 200, 8, 1, "%", pf);
        double_inputs_["pf_rev_growth"] = pf_rev_g;
        pf_vl->addWidget(build_input_row(tr("Revenue Growth"), pf_rev_g, pf));

        auto* pf_run = make_run_button(tr("BUILD PRO FORMA"), pf);
        connect(pf_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Building Pro Forma..."));
            QJsonObject params;
            params["years"] = int_inputs_["pf_years"]->value();
            params["revenue_growth"] = double_inputs_["pf_rev_growth"]->value() / 100.0;
            // Re-use the acquirer/target and deal-term inputs from the accretion tab; without them the script
            // assumes its own price, cash mix and share count.
            params["deal_value"] = double_inputs_["deal_value"]->value();
            params["cash_pct"] = double_inputs_["cash_pct"]->value() / 100.0;
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
            MAAnalyticsService::instance().build_pro_forma(params);
        });
        pf_vl->addWidget(pf_run);
        pf_vl->addStretch();
    });

    // ── Sources & Uses ──
    add_lazy_sub_tab(QT_TR_NOOP("Sources & Uses"), [this](QWidget* su) {
        auto* su_vl = new QVBoxLayout(su);
        su_vl->setContentsMargins(12, 12, 12, 12);
        su_vl->setSpacing(8);

        auto* su_cash = make_double_spin(0, 1e15, 200e6, 0, "", su);
        double_inputs_["su_cash"] = su_cash;
        su_vl->addWidget(build_input_row(tr("Cash on Hand") + " ($)", su_cash, su));

        // Defaults balance: 200M + 262.5M + 50M = 500M price + 7.5M transaction fees + 5M financing fees.
        auto* su_debt = make_double_spin(0, 1e15, 262.5e6, 0, "", su);
        double_inputs_["su_new_debt"] = su_debt;
        su_vl->addWidget(build_input_row(tr("New Debt") + " ($)", su_debt, su));

        auto* su_stock = make_double_spin(0, 1e15, 50e6, 0, "", su);
        double_inputs_["su_stock"] = su_stock;
        su_vl->addWidget(build_input_row(tr("Stock Issuance") + " ($)", su_stock, su));

        // The fees are uses of funds too; shown (and sent) instead of the script's silent 1.5% / 2% estimates.
        auto* su_tx_fees = make_double_spin(0, 1e15, 7.5e6, 0, "", su);
        double_inputs_["su_transaction_fees"] = su_tx_fees;
        su_vl->addWidget(build_input_row(tr("Transaction Fees") + " ($)", su_tx_fees, su));

        auto* su_fin_fees = make_double_spin(0, 1e15, 5e6, 0, "", su);
        double_inputs_["su_financing_fees"] = su_fin_fees;
        su_vl->addWidget(build_input_row(tr("Financing Fees") + " ($)", su_fin_fees, su));

        auto* su_run = make_run_button(tr("CALCULATE SOURCES & USES"), su);
        connect(su_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Calculating Sources & Uses..."));
            QJsonObject params;
            params["deal_value"] = double_inputs_["deal_value"]->value();
            params["cash"] = double_inputs_["su_cash"]->value();
            params["new_debt"] = double_inputs_["su_new_debt"]->value();
            params["stock_issuance"] = double_inputs_["su_stock"]->value();
            params["transaction_fees"] = double_inputs_["su_transaction_fees"]->value();
            params["financing_fees"] = double_inputs_["su_financing_fees"]->value();
            MAAnalyticsService::instance().calculate_sources_uses(params);
        });
        su_vl->addWidget(su_run);
        su_vl->addStretch();
    });

    // ── Contribution Analysis ──
    add_lazy_sub_tab(QT_TR_NOOP("Contribution"), [this](QWidget* contrib) {
        auto* contrib_vl = new QVBoxLayout(contrib);
        contrib_vl->setContentsMargins(12, 12, 12, 12);
        contrib_vl->setSpacing(8);

        auto* contrib_hint = new QLabel(tr("Calculates each party's % contribution to the combined entity."), contrib);
        contrib_hint->setWordWrap(true);
        contrib_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                        .arg(ui::colors::TEXT_SECONDARY())
                                        .arg(ui::fonts::SMALL.get())
                                        .arg(ui::fonts::DATA_FAMILY));
        contrib_vl->addWidget(contrib_hint);

        auto* contrib_own = make_double_spin(1, 100, 70, 1, "%", contrib);
        double_inputs_["contrib_ownership"] = contrib_own;
        contrib_vl->addWidget(build_input_row(tr("Acquirer Ownership %"), contrib_own, contrib));

        auto* contrib_run = make_run_button(tr("ANALYZE CONTRIBUTION"), contrib);
        connect(contrib_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Analyzing Contribution..."));
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
            // The script's ownership_split is the TARGET shareholders' share of the combined company.
            params["ownership_split"] = 1.0 - double_inputs_["contrib_ownership"]->value() / 100.0;
            MAAnalyticsService::instance().analyze_contribution(params);
        });
        contrib_vl->addWidget(contrib_run);
        contrib_vl->addStretch();
    });

    // ── Payment Structure ──
    add_lazy_sub_tab(QT_TR_NOOP("Payment"), [this](QWidget* pay_str) {
        auto* pay_vl = new QVBoxLayout(pay_str);
        pay_vl->setContentsMargins(12, 12, 12, 12);
        pay_vl->setSpacing(8);

        auto* pay_price = make_double_spin(0, 1e15, 500e6, 0, "", pay_str);
        double_inputs_["pay_purchase_price"] = pay_price;
        pay_vl->addWidget(build_input_row(tr("Purchase Price") + " ($)", pay_price, pay_str));

        auto* pay_cash = make_double_spin(0, 100, 60, 0, "%", pay_str);
        double_inputs_["pay_cash_pct"] = pay_cash;
        pay_vl->addWidget(build_input_row(tr("Cash %"), pay_cash, pay_str));

        auto* pay_acq_cash = make_double_spin(0, 1e15, 200e6, 0, "", pay_str);
        double_inputs_["pay_cash_on_hand"] = pay_acq_cash;
        pay_vl->addWidget(build_input_row(tr("Cash on Hand") + " ($)", pay_acq_cash, pay_str));

        auto* pay_new_debt = make_double_spin(0, 1e15, 100e6, 0, "", pay_str);
        double_inputs_["pay_new_debt"] = pay_new_debt;
        pay_vl->addWidget(build_input_row(tr("New Debt") + " ($)", pay_new_debt, pay_str));

        // Share issuance, dilution and the exchange ratio need real share data: optional, never defaulted.
        auto* pay_acq_price = make_double_spin(0, 1e7, 0, 2, "", pay_str);
        pay_acq_price->setSpecialValueText(tr("not set"));
        double_inputs_["pay_acq_price"] = pay_acq_price;
        pay_vl->addWidget(build_input_row(tr("Acquirer Share Price") + " ($)", pay_acq_price, pay_str));

        auto* pay_acq_shares = make_double_spin(0, 1e13, 0, 0, "", pay_str);
        pay_acq_shares->setSpecialValueText(tr("not set"));
        double_inputs_["pay_acq_shares"] = pay_acq_shares;
        pay_vl->addWidget(build_input_row(tr("Acquirer Shares Outstanding"), pay_acq_shares, pay_str));

        auto* pay_tgt_shares = make_double_spin(0, 1e13, 0, 0, "", pay_str);
        pay_tgt_shares->setSpecialValueText(tr("not set"));
        double_inputs_["pay_tgt_shares"] = pay_tgt_shares;
        pay_vl->addWidget(build_input_row(tr("Target Shares Outstanding"), pay_tgt_shares, pay_str));

        auto* pay_run = make_run_button(tr("ANALYZE PAYMENT STRUCTURE"), pay_str);
        connect(pay_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Analyzing Payment..."));
            QJsonObject params;
            params["purchase_price"] = double_inputs_["pay_purchase_price"]->value();
            params["cash_pct"] = double_inputs_["pay_cash_pct"]->value() / 100.0;
            params["cash_on_hand"] = double_inputs_["pay_cash_on_hand"]->value();
            params["new_debt"] = double_inputs_["pay_new_debt"]->value();
            if (double_inputs_["pay_acq_price"]->value() > 0)
                params["acquirer_share_price"] = double_inputs_["pay_acq_price"]->value();
            if (double_inputs_["pay_acq_shares"]->value() > 0)
                params["acquirer_shares_outstanding"] = double_inputs_["pay_acq_shares"]->value();
            if (double_inputs_["pay_tgt_shares"]->value() > 0)
                params["target_shares_outstanding"] = double_inputs_["pay_tgt_shares"]->value();
            MAAnalyticsService::instance().analyze_payment_structure(params);
        });
        pay_vl->addWidget(pay_run);
        pay_vl->addStretch();
    });

    // ── Earnout ──
    add_lazy_sub_tab(QT_TR_NOOP("Earnout"), [this](QWidget* earn) {
        auto* earn_vl = new QVBoxLayout(earn);
        earn_vl->setContentsMargins(12, 12, 12, 12);
        earn_vl->setSpacing(8);

        auto* earn_base = make_double_spin(0, 1e15, 50e6, 0, "", earn);
        double_inputs_["earn_base_amount"] = earn_base;
        earn_vl->addWidget(build_input_row(tr("Earnout Amount") + " ($)", earn_base, earn));

        auto* earn_threshold = make_double_spin(0, 1e15, 100e6, 0, "", earn);
        double_inputs_["earn_threshold"] = earn_threshold;
        earn_vl->addWidget(build_input_row(tr("Revenue Threshold") + " ($)", earn_threshold, earn));

        auto* earn_period = make_int_spin(1, 10, 3, earn);
        int_inputs_["earn_period"] = earn_period;
        earn_vl->addWidget(build_input_row(tr("Earnout Period (yrs)"), earn_period, earn));

        auto* earn_prob = make_double_spin(0, 100, 60, 0, "%", earn);
        double_inputs_["earn_probability"] = earn_prob;
        earn_vl->addWidget(build_input_row(tr("Achievement Probability"), earn_prob, earn));

        auto* earn_run = make_run_button(tr("VALUE EARNOUT"), earn);
        connect(earn_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Valuing Earnout..."));
            QJsonObject params;
            // The upfront price is the Deal Value of the Accretion tab, as on the other merger tabs.
            params["base_price"] = double_inputs_["deal_value"]->value();
            params["earnout_amount"] = double_inputs_["earn_base_amount"]->value();
            params["revenue_threshold"] = double_inputs_["earn_threshold"]->value();
            params["period"] = int_inputs_["earn_period"]->value();
            params["probability"] = double_inputs_["earn_probability"]->value() / 100.0;
            MAAnalyticsService::instance().value_earnout(params);
        });
        earn_vl->addWidget(earn_run);
        earn_vl->addStretch();
    });

    // ── Exchange Ratio ──
    add_lazy_sub_tab(QT_TR_NOOP("Exchange Ratio"), [this](QWidget* exch) {
        auto* exch_vl = new QVBoxLayout(exch);
        exch_vl->setContentsMargins(12, 12, 12, 12);
        exch_vl->setSpacing(8);

        auto* exch_acq_price = make_double_spin(0, 1e6, 100, 2, "", exch);
        double_inputs_["exch_acq_price"] = exch_acq_price;
        exch_vl->addWidget(build_input_row(tr("Acquirer Share Price") + " ($)", exch_acq_price, exch));

        auto* exch_tgt_price = make_double_spin(0, 1e6, 50, 2, "", exch);
        double_inputs_["exch_tgt_price"] = exch_tgt_price;
        exch_vl->addWidget(build_input_row(tr("Target Share Price") + " ($)", exch_tgt_price, exch));

        auto* exch_premium = make_double_spin(0, 200, 30, 1, "%", exch);
        double_inputs_["exch_premium"] = exch_premium;
        exch_vl->addWidget(build_input_row(tr("Offer Premium"), exch_premium, exch));

        auto* exch_run = make_run_button(tr("CALCULATE EXCHANGE RATIO"), exch);
        connect(exch_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Calculating Exchange Ratio..."));
            QJsonObject params;
            params["acquirer_price"] = double_inputs_["exch_acq_price"]->value();
            params["target_price"] = double_inputs_["exch_tgt_price"]->value();
            params["premium"] = double_inputs_["exch_premium"]->value() / 100.0;
            MAAnalyticsService::instance().calculate_exchange_ratio(params);
        });
        exch_vl->addWidget(exch_run);
        exch_vl->addStretch();
    });

    // ── Collar Mechanism ──
    add_lazy_sub_tab(QT_TR_NOOP("Collar"), [this](QWidget* collar) {
        auto* collar_vl = new QVBoxLayout(collar);
        collar_vl->setContentsMargins(12, 12, 12, 12);
        collar_vl->setSpacing(8);

        auto* collar_floor = make_double_spin(0, 1e6, 85, 2, "", collar);
        double_inputs_["collar_floor"] = collar_floor;
        collar_vl->addWidget(build_input_row(tr("Price Floor") + " ($)", collar_floor, collar));

        auto* collar_cap = make_double_spin(0, 1e6, 115, 2, "", collar);
        double_inputs_["collar_cap"] = collar_cap;
        collar_vl->addWidget(build_input_row(tr("Price Cap") + " ($)", collar_cap, collar));

        auto* collar_ratio = make_double_spin(0, 10, 0.5, 3, "", collar);
        double_inputs_["collar_base_ratio"] = collar_ratio;
        collar_vl->addWidget(build_input_row(tr("Base Exchange Ratio"), collar_ratio, collar));

        // The acquirer price the terms were set at (the scenarios run from 70% to 130% of it).
        auto* collar_announce = make_double_spin(0, 1e6, 100, 2, "", collar);
        double_inputs_["collar_announce_price"] = collar_announce;
        collar_vl->addWidget(build_input_row(tr("Acquirer Price at Announcement") + " ($)", collar_announce, collar));
        // Optional: with the target's share count the scenarios also show the total deal value and shares issued.
        auto* collar_shares = make_double_spin(0, 1e13, 0, 0, "", collar);
        collar_shares->setSpecialValueText(tr("not set"));
        double_inputs_["collar_tgt_shares"] = collar_shares;
        collar_vl->addWidget(build_input_row(tr("Target Shares Outstanding"), collar_shares, collar));

        auto* collar_run = make_run_button(tr("ANALYZE COLLAR"), collar);
        connect(collar_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Analyzing Collar..."));
            QJsonObject params;
            params["floor_price"] = double_inputs_["collar_floor"]->value();
            params["cap_price"] = double_inputs_["collar_cap"]->value();
            params["base_ratio"] = double_inputs_["collar_base_ratio"]->value();
            params["announcement_price"] = double_inputs_["collar_announce_price"]->value();
            if (double_inputs_["collar_tgt_shares"]->value() > 0)
                params["target_shares"] = double_inputs_["collar_tgt_shares"]->value();
            MAAnalyticsService::instance().analyze_collar_mechanism(params);
        });
        collar_vl->addWidget(collar_run);
        collar_vl->addStretch();
    });

    // ── CVR (Contingent Value Rights) ──
    add_lazy_sub_tab(QT_TR_NOOP("CVR"), [this](QWidget* cvr) {
        auto* cvr_vl = new QVBoxLayout(cvr);
        cvr_vl->setContentsMargins(12, 12, 12, 12);
        cvr_vl->setSpacing(8);

        auto* cvr_type = new QComboBox(cvr);
        cvr_type->addItems({"milestone", "revenue", "regulatory"});
        cvr_type->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                        "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                    .arg(ui::fonts::DATA_FAMILY)
                                    .arg(ui::fonts::SMALL.get()));
        combo_inputs_["cvr_type"] = cvr_type;
        cvr_vl->addWidget(build_input_row(tr("CVR Type"), cvr_type, cvr));

        auto* cvr_max = make_double_spin(0, 1e15, 10e6, 0, "", cvr);
        double_inputs_["cvr_max_payout"] = cvr_max;
        cvr_vl->addWidget(build_input_row(tr("Max Payout") + " ($)", cvr_max, cvr));

        auto* cvr_prob = make_double_spin(0, 100, 50, 0, "%", cvr);
        double_inputs_["cvr_probability"] = cvr_prob;
        cvr_vl->addWidget(build_input_row(tr("Achievement Prob"), cvr_prob, cvr));

        auto* cvr_expiry = make_int_spin(1, 10, 3, cvr);
        int_inputs_["cvr_expiry"] = cvr_expiry;
        cvr_vl->addWidget(build_input_row(tr("Expiry (yrs)"), cvr_expiry, cvr));

        auto* cvr_run = make_run_button(tr("VALUE CVR"), cvr);
        connect(cvr_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Valuing CVR..."));
            QJsonObject params;
            params["type"] = combo_inputs_["cvr_type"]->currentText();
            params["max_payout"] = double_inputs_["cvr_max_payout"]->value();
            params["probability"] = double_inputs_["cvr_probability"]->value() / 100.0;
            params["expiry_years"] = int_inputs_["cvr_expiry"]->value();
            MAAnalyticsService::instance().value_cvr(params);
        });
        cvr_vl->addWidget(cvr_run);
        cvr_vl->addStretch();
    });

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
