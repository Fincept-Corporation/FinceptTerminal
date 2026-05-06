// MAValuationPanel.cpp — DCF / valuation toolkit sub-tab builder for MAModulePanel.
// Method definition split from MAModulePanel.cpp; class declaration in
// MAModulePanel.h is unchanged.

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
// MODULE 1: VALUATION TOOLKIT
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_valuation_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── DCF Tab ──
    auto* dcf = new QWidget(this);
    auto* dcf_vl = new QVBoxLayout(dcf);
    dcf_vl->setContentsMargins(12, 12, 12, 12);
    dcf_vl->setSpacing(8);

    auto* dcf_ebit = make_double_spin(0, 1e12, 500e6, 0, "", dcf);
    double_inputs_["dcf_ebit"] = dcf_ebit;
    dcf_vl->addWidget(build_input_row("EBIT ($)", dcf_ebit, dcf));

    auto* dcf_tax = make_double_spin(0, 100, 21, 1, "%", dcf);
    double_inputs_["dcf_tax"] = dcf_tax;
    dcf_vl->addWidget(build_input_row("Tax Rate", dcf_tax, dcf));

    auto* dcf_rf = make_double_spin(0, 50, 4.5, 2, "%", dcf);
    double_inputs_["dcf_risk_free"] = dcf_rf;
    dcf_vl->addWidget(build_input_row("Risk-Free Rate", dcf_rf, dcf));

    auto* dcf_beta = make_double_spin(0, 5, 1.2, 2, "", dcf);
    double_inputs_["dcf_beta"] = dcf_beta;
    dcf_vl->addWidget(build_input_row("Beta", dcf_beta, dcf));

    auto* dcf_tg = make_double_spin(0, 20, 2.5, 1, "%", dcf);
    double_inputs_["dcf_terminal_growth"] = dcf_tg;
    dcf_vl->addWidget(build_input_row("Terminal Growth", dcf_tg, dcf));

    auto* dcf_shares = make_double_spin(0, 1e12, 100e6, 0, "", dcf);
    double_inputs_["dcf_shares"] = dcf_shares;
    dcf_vl->addWidget(build_input_row("Shares Outstanding", dcf_shares, dcf));

    auto* dcf_mktcap = make_double_spin(0, 1e15, 5e9, 0, "", dcf);
    double_inputs_["dcf_mktcap"] = dcf_mktcap;
    dcf_vl->addWidget(build_input_row("Market Cap ($)", dcf_mktcap, dcf));

    auto* dcf_debt = make_double_spin(0, 1e15, 1e9, 0, "", dcf);
    double_inputs_["dcf_debt"] = dcf_debt;
    dcf_vl->addWidget(build_input_row("Total Debt ($)", dcf_debt, dcf));

    auto* dcf_cod = make_double_spin(0, 50, 5, 2, "%", dcf);
    double_inputs_["dcf_cost_debt"] = dcf_cod;
    dcf_vl->addWidget(build_input_row("Cost of Debt", dcf_cod, dcf));

    auto* dcf_cash = make_double_spin(0, 1e15, 500e6, 0, "", dcf);
    double_inputs_["dcf_cash"] = dcf_cash;
    dcf_vl->addWidget(build_input_row("Cash ($)", dcf_cash, dcf));

    auto* dcf_run = make_run_button("RUN DCF ANALYSIS", dcf);
    connect(dcf_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running DCF...");
        QJsonObject params;
        params["ebit"] = double_inputs_["dcf_ebit"]->value();
        params["tax_rate"] = double_inputs_["dcf_tax"]->value() / 100.0;
        params["risk_free_rate"] = double_inputs_["dcf_risk_free"]->value() / 100.0;
        params["beta"] = double_inputs_["dcf_beta"]->value();
        params["terminal_growth"] = double_inputs_["dcf_terminal_growth"]->value() / 100.0;
        params["shares"] = double_inputs_["dcf_shares"]->value();
        params["market_cap"] = double_inputs_["dcf_mktcap"]->value();
        params["debt"] = double_inputs_["dcf_debt"]->value();
        params["cost_of_debt"] = double_inputs_["dcf_cost_debt"]->value() / 100.0;
        params["cash"] = double_inputs_["dcf_cash"]->value();
        MAAnalyticsService::instance().calculate_dcf(params);
    });
    dcf_vl->addWidget(dcf_run);
    dcf_vl->addStretch();
    sub_tabs_->addTab(dcf, "DCF");

    // ── LBO Tab ──
    auto* lbo = new QWidget(this);
    auto* lbo_vl = new QVBoxLayout(lbo);
    lbo_vl->setContentsMargins(12, 12, 12, 12);
    lbo_vl->setSpacing(8);

    auto* lbo_entry = make_double_spin(0, 1e15, 1e9, 0, "", lbo);
    double_inputs_["lbo_entry"] = lbo_entry;
    lbo_vl->addWidget(build_input_row("Entry Valuation ($)", lbo_entry, lbo));

    auto* lbo_exit = make_double_spin(0, 1e15, 1.5e9, 0, "", lbo);
    double_inputs_["lbo_exit"] = lbo_exit;
    lbo_vl->addWidget(build_input_row("Exit Valuation ($)", lbo_exit, lbo));

    auto* lbo_equity = make_double_spin(0, 1e15, 300e6, 0, "", lbo);
    double_inputs_["lbo_equity"] = lbo_equity;
    lbo_vl->addWidget(build_input_row("Equity Invested ($)", lbo_equity, lbo));

    auto* lbo_period = make_int_spin(1, 30, 5, lbo);
    int_inputs_["lbo_period"] = lbo_period;
    lbo_vl->addWidget(build_input_row("Holding Period (yrs)", lbo_period, lbo));

    auto* lbo_sr_debt = make_double_spin(0, 1e15, 300e6, 0, "", lbo);
    double_inputs_["lbo_sr_debt"] = lbo_sr_debt;
    lbo_vl->addWidget(build_input_row("Senior Debt ($)", lbo_sr_debt, lbo));

    auto* lbo_sr_rate = make_double_spin(0, 50, 6, 2, "%", lbo);
    double_inputs_["lbo_sr_rate"] = lbo_sr_rate;
    lbo_vl->addWidget(build_input_row("Senior Rate", lbo_sr_rate, lbo));

    auto* lbo_sub_debt = make_double_spin(0, 1e15, 100e6, 0, "", lbo);
    double_inputs_["lbo_sub_debt"] = lbo_sub_debt;
    lbo_vl->addWidget(build_input_row("Sub Debt ($)", lbo_sub_debt, lbo));

    auto* lbo_sub_rate = make_double_spin(0, 50, 10, 2, "%", lbo);
    double_inputs_["lbo_sub_rate"] = lbo_sub_rate;
    lbo_vl->addWidget(build_input_row("Sub Rate", lbo_sub_rate, lbo));

    auto* lbo_run = make_run_button("RUN LBO ANALYSIS", lbo);
    connect(lbo_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running LBO...");
        QJsonObject params;
        params["entry_valuation"] = double_inputs_["lbo_entry"]->value();
        params["exit_valuation"] = double_inputs_["lbo_exit"]->value();
        params["equity_invested"] = double_inputs_["lbo_equity"]->value();
        params["holding_period"] = int_inputs_["lbo_period"]->value();
        params["senior_debt"] = double_inputs_["lbo_sr_debt"]->value();
        params["senior_rate"] = double_inputs_["lbo_sr_rate"]->value() / 100.0;
        params["sub_debt"] = double_inputs_["lbo_sub_debt"]->value();
        params["sub_rate"] = double_inputs_["lbo_sub_rate"]->value() / 100.0;
        MAAnalyticsService::instance().calculate_lbo_returns(params);
    });
    lbo_vl->addWidget(lbo_run);
    lbo_vl->addStretch();
    sub_tabs_->addTab(lbo, "LBO Returns");

    // ── LBO Full Model Tab ──
    auto* lbo_full = new QWidget(this);
    auto* lbof_vl = new QVBoxLayout(lbo_full);
    lbof_vl->setContentsMargins(12, 12, 12, 12);
    lbof_vl->setSpacing(8);

    auto* lbof_ebitda = make_double_spin(0, 1e15, 200e6, 0, "", lbo_full);
    double_inputs_["lbof_ebitda"] = lbof_ebitda;
    lbof_vl->addWidget(build_input_row("EBITDA ($)", lbof_ebitda, lbo_full));

    auto* lbof_multiple = make_double_spin(0, 50, 8, 1, "x", lbo_full);
    double_inputs_["lbof_entry_multiple"] = lbof_multiple;
    lbof_vl->addWidget(build_input_row("Entry Multiple", lbof_multiple, lbo_full));

    auto* lbof_exit_mult = make_double_spin(0, 50, 7, 1, "x", lbo_full);
    double_inputs_["lbof_exit_multiple"] = lbof_exit_mult;
    lbof_vl->addWidget(build_input_row("Exit Multiple", lbof_exit_mult, lbo_full));

    auto* lbof_rev_growth = make_double_spin(-50, 200, 10, 1, "%", lbo_full);
    double_inputs_["lbof_rev_growth"] = lbof_rev_growth;
    lbof_vl->addWidget(build_input_row("Revenue Growth", lbof_rev_growth, lbo_full));

    auto* lbof_margin = make_double_spin(0, 100, 25, 1, "%", lbo_full);
    double_inputs_["lbof_margin"] = lbof_margin;
    lbof_vl->addWidget(build_input_row("EBITDA Margin", lbof_margin, lbo_full));

    auto* lbof_years = make_int_spin(1, 15, 5, lbo_full);
    int_inputs_["lbof_years"] = lbof_years;
    lbof_vl->addWidget(build_input_row("Hold Period (yrs)", lbof_years, lbo_full));

    auto* lbof_run = make_run_button("BUILD LBO MODEL", lbo_full);
    connect(lbof_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Building LBO Model...");
        QJsonObject params;
        params["ebitda"] = double_inputs_["lbof_ebitda"]->value();
        params["entry_multiple"] = double_inputs_["lbof_entry_multiple"]->value();
        params["exit_multiple"] = double_inputs_["lbof_exit_multiple"]->value();
        params["revenue_growth"] = double_inputs_["lbof_rev_growth"]->value() / 100.0;
        params["ebitda_margin"] = double_inputs_["lbof_margin"]->value() / 100.0;
        params["holding_period"] = int_inputs_["lbof_years"]->value();
        params["senior_debt"] = double_inputs_["lbo_sr_debt"]->value();
        params["senior_rate"] = double_inputs_["lbo_sr_rate"]->value() / 100.0;
        params["sub_debt"] = double_inputs_["lbo_sub_debt"]->value();
        params["sub_rate"] = double_inputs_["lbo_sub_rate"]->value() / 100.0;
        MAAnalyticsService::instance().build_lbo_model(params);
    });
    lbof_vl->addWidget(lbof_run);
    lbof_vl->addStretch();
    sub_tabs_->addTab(lbo_full, "LBO Model");

    // ── LBO Debt Schedule Tab ──
    auto* lbo_ds = new QWidget(this);
    auto* ds_vl = new QVBoxLayout(lbo_ds);
    ds_vl->setContentsMargins(12, 12, 12, 12);
    ds_vl->setSpacing(8);

    auto* ds_revolver = make_double_spin(0, 1e15, 50e6, 0, "", lbo_ds);
    double_inputs_["ds_revolver"] = ds_revolver;
    ds_vl->addWidget(build_input_row("Revolver ($)", ds_revolver, lbo_ds));

    auto* ds_revolver_rate = make_double_spin(0, 50, 5.5, 2, "%", lbo_ds);
    double_inputs_["ds_revolver_rate"] = ds_revolver_rate;
    ds_vl->addWidget(build_input_row("Revolver Rate", ds_revolver_rate, lbo_ds));

    auto* ds_capex = make_double_spin(0, 1e15, 30e6, 0, "", lbo_ds);
    double_inputs_["ds_capex"] = ds_capex;
    ds_vl->addWidget(build_input_row("Annual CapEx ($)", ds_capex, lbo_ds));

    auto* ds_sweep = make_double_spin(0, 100, 75, 0, "%", lbo_ds);
    double_inputs_["ds_sweep_pct"] = ds_sweep;
    ds_vl->addWidget(build_input_row("Debt Sweep %", ds_sweep, lbo_ds));

    auto* ds_ebitda = make_double_spin(0, 1e15, 200e6, 0, "", lbo_ds);
    double_inputs_["ds_ebitda"] = ds_ebitda;
    ds_vl->addWidget(build_input_row("EBITDA ($)", ds_ebitda, lbo_ds));

    auto* ds_run = make_run_button("ANALYZE DEBT SCHEDULE", lbo_ds);
    connect(ds_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Analyzing Debt Schedule...");
        QJsonObject params;
        params["senior_debt"] = double_inputs_["lbo_sr_debt"]->value();
        params["senior_rate"] = double_inputs_["lbo_sr_rate"]->value() / 100.0;
        params["sub_debt"] = double_inputs_["lbo_sub_debt"]->value();
        params["sub_rate"] = double_inputs_["lbo_sub_rate"]->value() / 100.0;
        params["revolver"] = double_inputs_["ds_revolver"]->value();
        params["revolver_rate"] = double_inputs_["ds_revolver_rate"]->value() / 100.0;
        params["ebitda"] = double_inputs_["ds_ebitda"]->value();
        params["capex"] = double_inputs_["ds_capex"]->value();
        params["sweep_pct"] = double_inputs_["ds_sweep_pct"]->value() / 100.0;
        MAAnalyticsService::instance().analyze_lbo_debt_schedule(params);
    });
    ds_vl->addWidget(ds_run);
    ds_vl->addStretch();
    sub_tabs_->addTab(lbo_ds, "Debt Schedule");

    // ── LBO Sensitivity Tab ──
    auto* lbo_sens = new QWidget(this);
    auto* sens_vl = new QVBoxLayout(lbo_sens);
    sens_vl->setContentsMargins(12, 12, 12, 12);
    sens_vl->setSpacing(8);

    auto* sens_hint =
        new QLabel("Sensitivity analysis varies entry multiple and exit multiple around base case.", lbo_sens);
    sens_hint->setWordWrap(true);
    sens_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                 .arg(ui::colors::TEXT_SECONDARY())
                                 .arg(ui::fonts::SMALL)
                                 .arg(ui::fonts::DATA_FAMILY));
    sens_vl->addWidget(sens_hint);

    auto* sens_entry_base = make_double_spin(0, 50, 8, 1, "x", lbo_sens);
    double_inputs_["sens_entry_base"] = sens_entry_base;
    sens_vl->addWidget(build_input_row("Entry Multiple (base)", sens_entry_base, lbo_sens));

    auto* sens_exit_base = make_double_spin(0, 50, 7, 1, "x", lbo_sens);
    double_inputs_["sens_exit_base"] = sens_exit_base;
    sens_vl->addWidget(build_input_row("Exit Multiple (base)", sens_exit_base, lbo_sens));

    auto* sens_range = make_double_spin(0.5, 5, 2, 1, "x", lbo_sens);
    double_inputs_["sens_range"] = sens_range;
    sens_vl->addWidget(build_input_row("Range (+/-)", sens_range, lbo_sens));

    auto* sens_steps = make_int_spin(3, 11, 5, lbo_sens);
    int_inputs_["sens_steps"] = sens_steps;
    sens_vl->addWidget(build_input_row("Steps", sens_steps, lbo_sens));

    auto* sens_run = make_run_button("RUN LBO SENSITIVITY", lbo_sens);
    connect(sens_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running LBO Sensitivity...");
        QJsonObject params;
        params["entry_base"] = double_inputs_["sens_entry_base"]->value();
        params["exit_base"] = double_inputs_["sens_exit_base"]->value();
        params["range"] = double_inputs_["sens_range"]->value();
        params["steps"] = int_inputs_["sens_steps"]->value();
        params["equity_invested"] = double_inputs_["lbo_equity"]->value();
        params["holding_period"] = int_inputs_["lbo_period"]->value();
        MAAnalyticsService::instance().calculate_lbo_sensitivity(params);
    });
    sens_vl->addWidget(sens_run);
    sens_vl->addStretch();
    sub_tabs_->addTab(lbo_sens, "Sensitivity");

    // ── Trading Comps Tab ──
    auto* comps = new QWidget(this);
    auto* comps_vl = new QVBoxLayout(comps);
    comps_vl->setContentsMargins(12, 12, 12, 12);
    comps_vl->setSpacing(8);

    auto* comps_hint = new QLabel("Enter target ticker and comparable tickers (comma-separated):", comps);
    comps_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                  .arg(ui::colors::TEXT_SECONDARY())
                                  .arg(ui::fonts::SMALL)
                                  .arg(ui::fonts::DATA_FAMILY));
    comps_vl->addWidget(comps_hint);

    auto* comps_target = new QComboBox(comps);
    comps_target->setEditable(true);
    comps_target->setPlaceholderText("Target ticker (e.g. AAPL)");
    comps_target->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                        "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                    .arg(ui::fonts::DATA_FAMILY)
                                    .arg(ui::fonts::SMALL));
    combo_inputs_["comps_target"] = comps_target;
    comps_vl->addWidget(build_input_row("Target Ticker", comps_target, comps));

    auto* comps_peers = new QComboBox(comps);
    comps_peers->setEditable(true);
    comps_peers->setPlaceholderText("MSFT,GOOG,AMZN");
    comps_peers->setStyleSheet(comps_target->styleSheet());
    combo_inputs_["comps_peers"] = comps_peers;
    comps_vl->addWidget(build_input_row("Comp Tickers", comps_peers, comps));

    auto* comps_run = make_run_button("RUN TRADING COMPS", comps);
    connect(comps_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running Trading Comps...");
        QJsonObject params;
        params["target_ticker"] = combo_inputs_["comps_target"]->currentText();
        params["comp_tickers"] = combo_inputs_["comps_peers"]->currentText();
        MAAnalyticsService::instance().calculate_trading_comps(params);
    });
    comps_vl->addWidget(comps_run);
    comps_vl->addStretch();
    sub_tabs_->addTab(comps, "Trading Comps");

    // ── Precedent Transactions Tab ──
    auto* prec = new QWidget(this);
    auto* prec_vl = new QVBoxLayout(prec);
    prec_vl->setContentsMargins(12, 12, 12, 12);
    prec_vl->setSpacing(8);

    auto* prec_rev = make_double_spin(0, 1e15, 500e6, 0, "", prec);
    double_inputs_["prec_revenue"] = prec_rev;
    prec_vl->addWidget(build_input_row("Target Revenue ($)", prec_rev, prec));

    auto* prec_ebitda = make_double_spin(0, 1e15, 100e6, 0, "", prec);
    double_inputs_["prec_ebitda"] = prec_ebitda;
    prec_vl->addWidget(build_input_row("Target EBITDA ($)", prec_ebitda, prec));

    auto* prec_run = make_run_button("RUN PRECEDENT ANALYSIS", prec);
    connect(prec_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running Precedent Txns...");
        QJsonObject params;
        params["target_revenue"] = double_inputs_["prec_revenue"]->value();
        params["target_ebitda"] = double_inputs_["prec_ebitda"]->value();
        MAAnalyticsService::instance().calculate_precedent_transactions(params);
    });
    prec_vl->addWidget(prec_run);
    prec_vl->addStretch();
    sub_tabs_->addTab(prec, "Precedent Txns");

    vl->addWidget(sub_tabs_);

    // Results area
    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
