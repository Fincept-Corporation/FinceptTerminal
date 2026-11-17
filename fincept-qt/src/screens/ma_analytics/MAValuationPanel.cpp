// MAValuationPanel.cpp — DCF / valuation toolkit sub-tab builder for MAModulePanel.
// Method definition split from MAModulePanel.cpp; class declaration in
// MAModulePanel.h is unchanged.

#include "screens/ma_analytics/MAModulePanel.h"
#include "screens/ma_analytics/MaDealGrid.h"
#include "services/instruments/InstrumentResolver.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QComboBox>
#include <QRegularExpression>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

using namespace fincept::services::ma;

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

    // Start from a company: fills the DCF, LBO and precedent inputs and the football field's price (MA-4).
    vl->addWidget(build_company_strip(w));

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── DCF Tab ──
    add_lazy_sub_tab(QT_TR_NOOP("DCF"), [this](QWidget* dcf) {
        auto* dcf_vl = new QVBoxLayout(dcf);
        dcf_vl->setContentsMargins(12, 12, 12, 12);
        dcf_vl->setSpacing(8);

        auto* dcf_ebit = make_double_spin(0, 1e12, 500e6, 0, "", dcf);
        double_inputs_["dcf_ebit"] = dcf_ebit;
        dcf_vl->addWidget(build_input_row(tr("EBIT") + " ($)", dcf_ebit, dcf));

        auto* dcf_tax = make_double_spin(0, 100, 21, 1, "%", dcf);
        double_inputs_["dcf_tax"] = dcf_tax;
        dcf_vl->addWidget(build_input_row(tr("Tax Rate"), dcf_tax, dcf));

        auto* dcf_rf = make_double_spin(0, 50, 4.5, 2, "%", dcf);
        double_inputs_["dcf_risk_free"] = dcf_rf;
        dcf_vl->addWidget(build_input_row(tr("Risk-Free Rate"), dcf_rf, dcf));

        auto* dcf_beta = make_double_spin(0, 5, 1.2, 2, "", dcf);
        double_inputs_["dcf_beta"] = dcf_beta;
        dcf_vl->addWidget(build_input_row(tr("Beta"), dcf_beta, dcf));

        auto* dcf_tg = make_double_spin(0, 20, 2.5, 1, "%", dcf);
        double_inputs_["dcf_terminal_growth"] = dcf_tg;
        dcf_vl->addWidget(build_input_row(tr("Terminal Growth"), dcf_tg, dcf));

        auto* dcf_shares = make_double_spin(0, 1e12, 100e6, 0, "", dcf);
        double_inputs_["dcf_shares"] = dcf_shares;
        dcf_vl->addWidget(build_input_row(tr("Shares Outstanding"), dcf_shares, dcf));

        auto* dcf_mktcap = make_double_spin(0, 1e15, 5e9, 0, "", dcf);
        double_inputs_["dcf_mktcap"] = dcf_mktcap;
        dcf_vl->addWidget(build_input_row(tr("Market Cap") + " ($)", dcf_mktcap, dcf));

        auto* dcf_debt = make_double_spin(0, 1e15, 1e9, 0, "", dcf);
        double_inputs_["dcf_debt"] = dcf_debt;
        dcf_vl->addWidget(build_input_row(tr("Total Debt") + " ($)", dcf_debt, dcf));

        auto* dcf_cod = make_double_spin(0, 50, 5, 2, "%", dcf);
        double_inputs_["dcf_cost_debt"] = dcf_cod;
        dcf_vl->addWidget(build_input_row(tr("Cost of Debt"), dcf_cod, dcf));

        auto* dcf_cash = make_double_spin(0, 1e15, 500e6, 0, "", dcf);
        double_inputs_["dcf_cash"] = dcf_cash;
        dcf_vl->addWidget(build_input_row(tr("Cash") + " ($)", dcf_cash, dcf));

        // The model's drivers, shown instead of assumed (the defaults are the values the model used to apply
        // silently: 5% growth fading to the terminal rate over 5 years, a 5.5% equity risk premium, no D&A,
        // capex or working-capital change).
        auto* drivers = new QLabel(tr("FORECAST DRIVERS"), dcf);
        drivers->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:1px;")
                                   .arg(ui::colors::legible(module_.color))
                                   .arg(ui::fonts::px(-3)));
        dcf_vl->addWidget(drivers);
        auto* dcf_growth = make_double_spin(-50, 100, 5, 1, "%", dcf);
        dcf_growth->setToolTip(tr("Free-cash-flow growth in year 1, fading in a straight line to the terminal "
                                  "growth by the last projected year"));
        double_inputs_["dcf_growth"] = dcf_growth;
        dcf_vl->addWidget(build_input_row(tr("Near-term FCF Growth"), dcf_growth, dcf));
        auto* dcf_years = make_int_spin(1, 15, 5, dcf);
        int_inputs_["dcf_years"] = dcf_years;
        dcf_vl->addWidget(build_input_row(tr("Projection Years"), dcf_years, dcf));
        auto* dcf_erp = make_double_spin(0, 20, 5.5, 2, "%", dcf);
        double_inputs_["dcf_erp"] = dcf_erp;
        dcf_vl->addWidget(build_input_row(tr("Equity Risk Premium"), dcf_erp, dcf));
        auto* dcf_da = make_double_spin(0, 1e15, 0, 0, "", dcf);
        double_inputs_["dcf_depreciation"] = dcf_da;
        dcf_vl->addWidget(build_input_row(tr("Depreciation & Amortisation") + " ($)", dcf_da, dcf));
        auto* dcf_capex = make_double_spin(0, 1e15, 0, 0, "", dcf);
        double_inputs_["dcf_capex"] = dcf_capex;
        dcf_vl->addWidget(build_input_row(tr("Capital Expenditure") + " ($)", dcf_capex, dcf));
        auto* dcf_nwc = make_double_spin(-1e15, 1e15, 0, 0, "", dcf);
        double_inputs_["dcf_nwc"] = dcf_nwc;
        dcf_vl->addWidget(build_input_row(tr("Increase in Working Capital") + " ($)", dcf_nwc, dcf));

        auto* sens = new QLabel(tr("SENSITIVITY GRID"), dcf);
        sens->setStyleSheet(drivers->styleSheet());
        dcf_vl->addWidget(sens);
        auto* sens_w = make_double_spin(0.1, 5, 1, 2, "%", dcf);
        double_inputs_["dcf_sens_wacc_step"] = sens_w;
        dcf_vl->addWidget(build_input_row(tr("WACC Step"), sens_w, dcf));
        auto* sens_g = make_double_spin(0.05, 2, 0.5, 2, "%", dcf);
        double_inputs_["dcf_sens_growth_step"] = sens_g;
        dcf_vl->addWidget(build_input_row(tr("Growth Step"), sens_g, dcf));
        auto* sens_n = make_int_spin(3, 9, 5, dcf);
        int_inputs_["dcf_sens_steps"] = sens_n;
        dcf_vl->addWidget(build_input_row(tr("Grid Size (rows = columns)"), sens_n, dcf));

        auto* dcf_run = make_run_button(tr("RUN DCF ANALYSIS"), dcf);
        connect(dcf_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Running DCF..."));
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
            params["growth_rate"] = double_inputs_["dcf_growth"]->value() / 100.0;
            params["projection_years"] = int_inputs_["dcf_years"]->value();
            params["market_risk_premium"] = double_inputs_["dcf_erp"]->value() / 100.0;
            params["depreciation"] = double_inputs_["dcf_depreciation"]->value();
            params["capex"] = double_inputs_["dcf_capex"]->value();
            params["change_in_nwc"] = double_inputs_["dcf_nwc"]->value();
            // The WACC x terminal-growth grid is computed in the same run (MA-5).
            params["sens_wacc_step"] = double_inputs_["dcf_sens_wacc_step"]->value() / 100.0;
            params["sens_growth_step"] = double_inputs_["dcf_sens_growth_step"]->value() / 100.0;
            params["sens_steps"] = int_inputs_["dcf_sens_steps"]->value();
            MAAnalyticsService::instance().calculate_dcf(params);
        });
        dcf_vl->addWidget(dcf_run);
        dcf_vl->addStretch();
    });

    // ── LBO Tab ──
    add_lazy_sub_tab(QT_TR_NOOP("LBO Returns"), [this](QWidget* lbo) {
        auto* lbo_vl = new QVBoxLayout(lbo);
        lbo_vl->setContentsMargins(12, 12, 12, 12);
        lbo_vl->setSpacing(8);

        auto* lbo_entry = make_double_spin(0, 1e15, 1e9, 0, "", lbo);
        double_inputs_["lbo_entry"] = lbo_entry;
        lbo_vl->addWidget(build_input_row(tr("Entry Valuation") + " ($)", lbo_entry, lbo));

        auto* lbo_exit = make_double_spin(0, 1e15, 1.5e9, 0, "", lbo);
        double_inputs_["lbo_exit"] = lbo_exit;
        lbo_vl->addWidget(build_input_row(tr("Exit Valuation") + " ($)", lbo_exit, lbo));

        // Default equity funds the default entry valuation with the default debt: 600M + 300M + 100M = 1B.
        auto* lbo_equity = make_double_spin(0, 1e15, 600e6, 0, "", lbo);
        double_inputs_["lbo_equity"] = lbo_equity;
        lbo_vl->addWidget(build_input_row(tr("Equity Invested") + " ($)", lbo_equity, lbo));

        auto* lbo_period = make_int_spin(1, 30, 5, lbo);
        int_inputs_["lbo_period"] = lbo_period;
        lbo_vl->addWidget(build_input_row(tr("Holding Period (yrs)"), lbo_period, lbo));

        auto* lbo_sr_debt = make_double_spin(0, 1e15, 300e6, 0, "", lbo);
        double_inputs_["lbo_sr_debt"] = lbo_sr_debt;
        lbo_vl->addWidget(build_input_row(tr("Senior Debt") + " ($)", lbo_sr_debt, lbo));

        auto* lbo_sr_rate = make_double_spin(0, 50, 6, 2, "%", lbo);
        double_inputs_["lbo_sr_rate"] = lbo_sr_rate;
        lbo_vl->addWidget(build_input_row(tr("Senior Rate"), lbo_sr_rate, lbo));

        auto* lbo_sub_debt = make_double_spin(0, 1e15, 100e6, 0, "", lbo);
        double_inputs_["lbo_sub_debt"] = lbo_sub_debt;
        lbo_vl->addWidget(build_input_row(tr("Sub Debt") + " ($)", lbo_sub_debt, lbo));

        auto* lbo_sub_rate = make_double_spin(0, 50, 10, 2, "%", lbo);
        double_inputs_["lbo_sub_rate"] = lbo_sub_rate;
        lbo_vl->addWidget(build_input_row(tr("Sub Rate"), lbo_sub_rate, lbo));

        auto* lbo_run = make_run_button(tr("RUN LBO ANALYSIS"), lbo);
        connect(lbo_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Running LBO..."));
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
    });

    // ── LBO Full Model Tab ──
    add_lazy_sub_tab(QT_TR_NOOP("LBO Model"), [this](QWidget* lbo_full) {
        auto* lbof_vl = new QVBoxLayout(lbo_full);
        lbof_vl->setContentsMargins(12, 12, 12, 12);
        lbof_vl->setSpacing(8);

        auto* lbof_ebitda = make_double_spin(0, 1e15, 200e6, 0, "", lbo_full);
        double_inputs_["lbof_ebitda"] = lbof_ebitda;
        lbof_vl->addWidget(build_input_row(tr("EBITDA") + " ($)", lbof_ebitda, lbo_full));

        auto* lbof_multiple = make_double_spin(0, 50, 8, 1, "x", lbo_full);
        double_inputs_["lbof_entry_multiple"] = lbof_multiple;
        lbof_vl->addWidget(build_input_row(tr("Entry Multiple"), lbof_multiple, lbo_full));

        auto* lbof_exit_mult = make_double_spin(0, 50, 7, 1, "x", lbo_full);
        double_inputs_["lbof_exit_multiple"] = lbof_exit_mult;
        lbof_vl->addWidget(build_input_row(tr("Exit Multiple"), lbof_exit_mult, lbo_full));

        auto* lbof_rev_growth = make_double_spin(-50, 200, 10, 1, "%", lbo_full);
        double_inputs_["lbof_rev_growth"] = lbof_rev_growth;
        lbof_vl->addWidget(build_input_row(tr("Revenue Growth"), lbof_rev_growth, lbo_full));

        auto* lbof_margin = make_double_spin(0, 100, 25, 1, "%", lbo_full);
        double_inputs_["lbof_margin"] = lbof_margin;
        lbof_vl->addWidget(build_input_row(tr("EBITDA Margin"), lbof_margin, lbo_full));

        auto* lbof_years = make_int_spin(1, 15, 5, lbo_full);
        int_inputs_["lbof_years"] = lbof_years;
        lbof_vl->addWidget(build_input_row(tr("Hold Period (yrs)"), lbof_years, lbo_full));

        auto* lbof_run = make_run_button(tr("BUILD LBO MODEL"), lbo_full);
        connect(lbof_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Building LBO Model..."));
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
    });

    // ── LBO Debt Schedule Tab ──
    add_lazy_sub_tab(QT_TR_NOOP("Debt Schedule"), [this](QWidget* lbo_ds) {
        auto* ds_vl = new QVBoxLayout(lbo_ds);
        ds_vl->setContentsMargins(12, 12, 12, 12);
        ds_vl->setSpacing(8);

        auto* ds_revolver = make_double_spin(0, 1e15, 50e6, 0, "", lbo_ds);
        double_inputs_["ds_revolver"] = ds_revolver;
        ds_vl->addWidget(build_input_row(tr("Revolver") + " ($)", ds_revolver, lbo_ds));

        auto* ds_revolver_rate = make_double_spin(0, 50, 5.5, 2, "%", lbo_ds);
        double_inputs_["ds_revolver_rate"] = ds_revolver_rate;
        ds_vl->addWidget(build_input_row(tr("Revolver Rate"), ds_revolver_rate, lbo_ds));

        auto* ds_capex = make_double_spin(0, 1e15, 30e6, 0, "", lbo_ds);
        double_inputs_["ds_capex"] = ds_capex;
        ds_vl->addWidget(build_input_row(tr("Annual CapEx") + " ($)", ds_capex, lbo_ds));

        auto* ds_sweep = make_double_spin(0, 100, 75, 0, "%", lbo_ds);
        double_inputs_["ds_sweep_pct"] = ds_sweep;
        ds_vl->addWidget(build_input_row(tr("Debt Sweep %"), ds_sweep, lbo_ds));

        auto* ds_ebitda = make_double_spin(0, 1e15, 200e6, 0, "", lbo_ds);
        double_inputs_["ds_ebitda"] = ds_ebitda;
        ds_vl->addWidget(build_input_row(tr("EBITDA") + " ($)", ds_ebitda, lbo_ds));

        auto* ds_run = make_run_button(tr("ANALYZE DEBT SCHEDULE"), lbo_ds);
        connect(ds_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Analyzing Debt Schedule..."));
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
            // The schedule runs over the hold period set on the LBO Model tab (it used to be 7 years always).
            params["holding_period"] = int_inputs_["lbof_years"]->value();
            MAAnalyticsService::instance().analyze_lbo_debt_schedule(params);
        });
        ds_vl->addWidget(ds_run);
        ds_vl->addStretch();
    });

    // ── LBO Sensitivity Tab ──
    add_lazy_sub_tab(QT_TR_NOOP("Sensitivity"), [this](QWidget* lbo_sens) {
        auto* sens_vl = new QVBoxLayout(lbo_sens);
        sens_vl->setContentsMargins(12, 12, 12, 12);
        sens_vl->setSpacing(8);

        auto* sens_hint =
            new QLabel(tr("Sensitivity analysis varies entry multiple and exit multiple around base case."), lbo_sens);
        sens_hint->setWordWrap(true);
        sens_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                     .arg(ui::colors::TEXT_SECONDARY())
                                     .arg(ui::fonts::SMALL.get())
                                     .arg(ui::fonts::DATA_FAMILY));
        sens_vl->addWidget(sens_hint);

        auto* sens_entry_base = make_double_spin(0, 50, 8, 1, "x", lbo_sens);
        double_inputs_["sens_entry_base"] = sens_entry_base;
        sens_vl->addWidget(build_input_row(tr("Entry Multiple (base)"), sens_entry_base, lbo_sens));

        auto* sens_exit_base = make_double_spin(0, 50, 7, 1, "x", lbo_sens);
        double_inputs_["sens_exit_base"] = sens_exit_base;
        sens_vl->addWidget(build_input_row(tr("Exit Multiple (base)"), sens_exit_base, lbo_sens));

        auto* sens_range = make_double_spin(0.5, 5, 2, 1, "x", lbo_sens);
        double_inputs_["sens_range"] = sens_range;
        sens_vl->addWidget(build_input_row(tr("Range (+/-)"), sens_range, lbo_sens));

        auto* sens_steps = make_int_spin(3, 11, 5, lbo_sens);
        int_inputs_["sens_steps"] = sens_steps;
        sens_vl->addWidget(build_input_row(tr("Steps"), sens_steps, lbo_sens));

        auto* sens_run = make_run_button(tr("RUN LBO SENSITIVITY"), lbo_sens);
        connect(sens_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Running LBO Sensitivity..."));
            QJsonObject params;
            params["entry_base"] = double_inputs_["sens_entry_base"]->value();
            params["exit_base"] = double_inputs_["sens_exit_base"]->value();
            params["range"] = double_inputs_["sens_range"]->value();
            params["steps"] = int_inputs_["sens_steps"]->value();
            // The base case is the LBO Model tab's operating case and debt; equity is the entry EV less that debt.
            params["ebitda"] = double_inputs_["lbof_ebitda"]->value();
            params["revenue_growth"] = double_inputs_["lbof_rev_growth"]->value() / 100.0;
            params["ebitda_margin"] = double_inputs_["lbof_margin"]->value() / 100.0;
            params["holding_period"] = int_inputs_["lbof_years"]->value();
            params["senior_debt"] = double_inputs_["lbo_sr_debt"]->value();
            params["senior_rate"] = double_inputs_["lbo_sr_rate"]->value() / 100.0;
            params["sub_debt"] = double_inputs_["lbo_sub_debt"]->value();
            params["sub_rate"] = double_inputs_["lbo_sub_rate"]->value() / 100.0;
            MAAnalyticsService::instance().calculate_lbo_sensitivity(params);
        });
        sens_vl->addWidget(sens_run);
        sens_vl->addStretch();
    });

    // ── Trading Comps Tab ──
    add_lazy_sub_tab(QT_TR_NOOP("Trading Comps"), [this](QWidget* comps) {
        auto* comps_vl = new QVBoxLayout(comps);
        comps_vl->setContentsMargins(12, 12, 12, 12);
        comps_vl->setSpacing(8);

        auto* comps_hint = new QLabel(tr("Pick the target company and enter comparable tickers (comma-separated):"),
                                      comps);
        comps_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                      .arg(ui::colors::TEXT_SECONDARY())
                                      .arg(ui::fonts::SMALL.get())
                                      .arg(ui::fonts::DATA_FAMILY));
        comps_vl->addWidget(comps_hint);

        // The shared symbol picker (companies only). Its object name is the InputMemory /
        // preview key (MAModulePanel::attach_memory, configure_preview) — a new key: the old
        // "comps_target" entry holds a combo value, which would blank a line edit.
        services::ResolveOptions comps_opts;
        comps_opts.ctx.allowed_classes = {core::instrument::AssetClass::Equity};
        auto* comps_target = new ui::SymbolSearchBox(comps);
        comps_target->setObjectName(QStringLiteral("comps_target_symbol"));
        comps_target->set_compact(true);
        comps_target->set_clear_on_pick(false);
        comps_target->set_recent_key(QStringLiteral("ma_trading_comps"));
        comps_target->set_options(comps_opts);
        comps_target->set_placeholder(tr("Target: ticker, company name or ISIN"));
        // Sample tickers, editable: the preview's RUN works at once. Not run automatically (each run
        // fetches every ticker's market data from a rate-limited source).
        comps_target->set_text(QStringLiteral("AAPL"));
        comps_vl->addWidget(build_input_row(tr("Target Ticker"), comps_target, comps));

        auto* comps_peers = new QComboBox(comps);
        comps_peers->setEditable(true);
        comps_peers->setPlaceholderText("MSFT,GOOG,AMZN");
        comps_peers->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                           "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                       .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                       .arg(ui::fonts::DATA_FAMILY)
                                       .arg(ui::fonts::SMALL.get()));
        comps_peers->setEditText(QStringLiteral("MSFT,GOOGL,AMZN,META"));
        combo_inputs_["comps_peers"] = comps_peers;
        comps_vl->addWidget(build_input_row(tr("Comp Tickers"), comps_peers, comps));
        auto* suggest = new QPushButton(tr("SUGGEST PEERS"), comps);
        suggest->setCursor(Qt::PointingHandCursor);
        suggest->setToolTip(tr("Fill the comparable tickers with companies of the target's industry (same market "
                               "first); check them before running"));
        suggest->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                       " padding:4px 12px; font-size:%3px; }")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED())
                                   .arg(ui::fonts::px(-2)));
        connect(suggest, &QPushButton::clicked, this, &MAModulePanel::suggest_peers);
        comps_vl->addWidget(suggest, 0, Qt::AlignLeft);

        auto* comps_run = make_run_button(tr("RUN TRADING COMPS"), comps);
        // Every ticker goes through the shared resolver first (asynchronously, never blocking):
        // "RELIANCE" or "Toyota" become their market-data symbols; an unknown or ambiguous one
        // stops the run with a plain message instead of fetching another company's data.
        connect(comps_run, &QPushButton::clicked, this, [this, comps_target, comps_peers, comps_opts]() {
            const auto picked = comps_target->current_instrument();
            const QString target_text = picked.is_valid() && !picked.symbol.isEmpty()
                                            ? picked.symbol
                                            : comps_target->text().trimmed();
            static const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
            const QStringList peer_texts = comps_peers->currentText().split(sep, Qt::SkipEmptyParts);
            const quint64 run_id = run_ids_.value(QStringLiteral("trading_comps"));
            if (target_text.isEmpty() || peer_texts.isEmpty()) {
                if (pane_)
                    pane_->fail(run_id, target_text.isEmpty() ? tr("Pick the target company.")
                                                              : tr("Enter at least one comparable ticker."),
                                tr("Check the inputs"));
                status_label_->setText(tr("Error"));
                return;
            }
            status_label_->setText(tr("Checking the tickers..."));
            QStringList texts{target_text};
            texts += peer_texts;
            services::InstrumentResolver::instance().resolve_many(
                texts, comps_opts, this, [this, texts, run_id](const QVector<services::ResolveResult>& results) {
                    if (pane_ && run_id != 0 && !pane_->is_current(run_id))
                        return; // superseded or cancelled while resolving
                    QStringList symbols, unclear;
                    for (int i = 0; i < texts.size(); ++i) {
                        const services::ResolveResult r = i < results.size() ? results.at(i) : services::ResolveResult{};
                        if (r.exact && !r.matches.isEmpty() && services::is_quotable(r.matches.first()))
                            symbols << r.matches.first().symbol;
                        else
                            unclear << texts.at(i);
                    }
                    if (!unclear.isEmpty()) {
                        if (pane_)
                            pane_->fail(run_id,
                                        tr("Not found, or more than one listing: %1. Use the exchange form "
                                           "(e.g. TCS.NS, 7203.T) or pick the target from the list.")
                                            .arg(unclear.join(QStringLiteral(", "))),
                                        tr("Check the tickers"));
                        status_label_->setText(tr("Error"));
                        return;
                    }
                    // Say what runs (the inputs stay as typed: rewriting them would change the
                    // inputs signature and mark this very result "inputs changed").
                    status_label_->setText(tr("Running Trading Comps: %1 vs %2...")
                                               .arg(symbols.first(), symbols.mid(1).join(QStringLiteral(", "))));
                    QJsonObject params;
                    params["target_ticker"] = symbols.first();
                    params["comp_tickers"] = symbols.mid(1).join(QStringLiteral(","));
                    MAAnalyticsService::instance().calculate_trading_comps(params);
                });
        });
        comps_vl->addWidget(comps_run);
        comps_vl->addStretch();
    });

    // ── Precedent Transactions Tab ──
    add_lazy_sub_tab(QT_TR_NOOP("Precedent Txns"), [this](QWidget* prec) {
        auto* prec_vl = new QVBoxLayout(prec);
        prec_vl->setContentsMargins(12, 12, 12, 12);
        prec_vl->setSpacing(8);

        auto* prec_rev = make_double_spin(0, 1e15, 500e6, 0, "", prec);
        double_inputs_["prec_revenue"] = prec_rev;
        prec_vl->addWidget(build_input_row(tr("Target Revenue") + " ($)", prec_rev, prec));

        auto* prec_ebitda = make_double_spin(0, 1e15, 100e6, 0, "", prec);
        double_inputs_["prec_ebitda"] = prec_ebitda;
        prec_vl->addWidget(build_input_row(tr("Target EBITDA") + " ($)", prec_ebitda, prec));

        // Comparable deals: typed, pasted, imported or loaded from the Deal Database. An empty grid uses the deals
        // the user verified there (auto-extracted, unverified rows are never used) and says what to add if none.
        auto* prec_hint = new QLabel(tr("Comparable deals - leave the grid empty to use the deals you verified in the "
                                        "Deal Database."),
                                     prec);
        prec_hint->setWordWrap(true);
        prec_hint->setStyleSheet(QString("color:%1; font-size:%2px;").arg(ui::colors::TEXT_SECONDARY()).arg(ui::fonts::px(-2)));
        prec_vl->addWidget(prec_hint);
        auto* prec_grid = new MaDealGrid(MaDealGrid::Kind::Precedent, QStringLiteral("prec_deals_grid"), prec);
        prec_vl->addWidget(prec_grid);
        deal_grids_["precedent_txns"] = prec_grid; // filled by USE SAMPLE COMPARABLES

        auto* prec_run = make_run_button(tr("RUN PRECEDENT ANALYSIS"), prec);
        connect(prec_run, &QPushButton::clicked, this, [this, prec_grid]() {
            QJsonObject params;
            QJsonArray comps;
            if (!grid_deals(prec_grid, &comps, /*allow_empty=*/prec_grid->row_count() == 0))
                return;
            if (!comps.isEmpty())
                params["comparables"] = comps;
            status_label_->setText(tr("Running Precedent Txns..."));
            params["target_revenue"] = double_inputs_["prec_revenue"]->value();
            params["target_ebitda"] = double_inputs_["prec_ebitda"]->value();
            MAAnalyticsService::instance().calculate_precedent_transactions(params);
        });
        prec_vl->addWidget(prec_run);
        prec_vl->addStretch();
    });

    // ── Football Field (MA-6): every method's range on one axis, from this module's own runs ──
    add_lazy_sub_tab(QT_TR_NOOP("Football Field"), [this](QWidget* ff) { build_football_field_tab(ff); });

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
