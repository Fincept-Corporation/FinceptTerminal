// src/screens/ma_analytics/MAModulePanel.cpp
#include "screens/ma_analytics/MAModulePanel.h"

#include "core/logging/Logger.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QScrollArea>
#include <QTableWidget>

namespace fincept::screens {

using namespace fincept::services::ma;

// ── Helpers: create styled input widgets ─────────────────────────────────────

QDoubleSpinBox* MAModulePanel::make_double_spin(double min, double max, double val, int decimals, const QString& suffix,
                                                QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setDecimals(decimals);
    if (!suffix.isEmpty())
        spin->setSuffix(suffix);
    spin->setStyleSheet(QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; }"
                                "QDoubleSpinBox:focus { border-color:%6; }")
                            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL)
                            .arg(module_.color.name()));
    return spin;
}

QSpinBox* MAModulePanel::make_int_spin(int min, int max, int val, QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; }"
                                "QSpinBox:focus { border-color:%6; }")
                            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL)
                            .arg(module_.color.name()));
    return spin;
}

QPushButton* MAModulePanel::make_run_button(const QString& text, QWidget* parent) {
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                               "font-weight:700; border:none; padding:8px 20px;"
                               "letter-spacing:1px; }"
                               "QPushButton:hover { background:%5; }"
                               "QPushButton:pressed { background:%6; }")
                           .arg(module_.color.name())
                           .arg(ui::colors::BG_BASE())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(module_.color.lighter(120).name())
                           .arg(module_.color.darker(120).name()));
    return btn;
}

QWidget* MAModulePanel::build_input_row(const QString& label, QWidget* input, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 2, 0, 2);
    hl->setSpacing(8);
    auto* lbl = new QLabel(label, row);
    lbl->setFixedWidth(160);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                           .arg(ui::colors::TEXT_SECONDARY())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(lbl);
    hl->addWidget(input, 1);
    return row;
}

QWidget* MAModulePanel::build_metric_card(const QString& label, const QString& value, const QString& color,
                                          QWidget* parent) {
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString("background:%1; border:1px solid %2; padding:10px;")
                            .arg(ui::colors::BG_RAISED())
                            .arg(ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);
    auto* lbl = new QLabel(label, card);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; letter-spacing:1px;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY));
    auto* val = new QLabel(value, card);
    val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                           .arg(color)
                           .arg(ui::fonts::HEADER)
                           .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(lbl);
    vl->addWidget(val);
    return card;
}

// ── Constructor ──────────────────────────────────────────────────────────────

MAModulePanel::MAModulePanel(const ModuleInfo& info, QWidget* parent) : QWidget(parent), module_(info) {
    build_ui();
    connect_service();
    refresh_theme();
}

void MAModulePanel::connect_service() {
    auto& svc = MAAnalyticsService::instance();
    connect(&svc, &MAAnalyticsService::result_ready, this, &MAModulePanel::on_result_ready);
    connect(&svc, &MAAnalyticsService::error_occurred, this, &MAModulePanel::on_error);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void MAModulePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Module header bar
    header_bar_ = new QWidget(this);
    header_bar_->setFixedHeight(36);
    auto* hhl = new QHBoxLayout(header_bar_);
    hhl->setContentsMargins(16, 0, 16, 0);
    header_title_ = new QLabel(module_.label.toUpper(), header_bar_);
    hhl->addWidget(header_title_);

    auto* div = new QWidget(header_bar_);
    div->setFixedSize(1, 14);
    div->setObjectName("maPanelDivider");
    hhl->addWidget(div);

    header_category_ = new QLabel(module_.category.toUpper(), header_bar_);
    hhl->addWidget(header_category_);
    hhl->addStretch();

    status_label_ = new QLabel(header_bar_);
    hhl->addWidget(status_label_);
    root->addWidget(header_bar_);

    // Scrollable content area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    QWidget* panel_content = nullptr;
    switch (module_.id) {
        case ModuleId::Valuation:
            panel_content = build_valuation_panel();
            break;
        case ModuleId::Merger:
            panel_content = build_merger_panel();
            break;
        case ModuleId::Deals:
            panel_content = build_deals_panel();
            break;
        case ModuleId::Startup:
            panel_content = build_startup_panel();
            break;
        case ModuleId::Fairness:
            panel_content = build_fairness_panel();
            break;
        case ModuleId::Industry:
            panel_content = build_industry_panel();
            break;
        case ModuleId::Advanced:
            panel_content = build_advanced_panel();
            break;
        case ModuleId::Comparison:
            panel_content = build_comparison_panel();
            break;
    }

    scroll->setWidget(panel_content);
    root->addWidget(scroll, 1);
}

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

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 3: DEAL DATABASE
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_deals_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // Controls row
    auto* ctl = new QWidget(w);
    auto* ctl_hl = new QHBoxLayout(ctl);
    ctl_hl->setContentsMargins(0, 0, 0, 0);
    ctl_hl->setSpacing(8);

    auto* scan_days = make_int_spin(1, 365, 30, ctl);
    int_inputs_["scan_days"] = scan_days;
    ctl_hl->addWidget(new QLabel("Scan Days:", ctl));
    ctl_hl->addWidget(scan_days);

    auto* scan_btn = make_run_button("SCAN SEC FILINGS", ctl);
    connect(scan_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Scanning SEC EDGAR...");
        MAAnalyticsService::instance().scan_filings(int_inputs_["scan_days"]->value());
    });
    ctl_hl->addWidget(scan_btn);

    auto* load_btn = make_run_button("LOAD ALL DEALS", ctl);
    connect(load_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading deals...");
        MAAnalyticsService::instance().get_all_deals();
    });
    ctl_hl->addWidget(load_btn);
    ctl_hl->addStretch();
    vl->addWidget(ctl);

    // Search
    auto* search_box = new QComboBox(w);
    search_box->setEditable(true);
    search_box->setPlaceholderText("Search deals by target, acquirer, or industry...");
    search_box->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                      "font-family:%4; font-size:%5px; padding:6px 10px; }")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::fonts::SMALL));
    combo_inputs_["deal_search"] = search_box;
    // Search on enter
    connect(search_box->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
        auto q = combo_inputs_["deal_search"]->currentText();
        if (!q.isEmpty()) {
            status_label_->setText("Searching deals...");
            MAAnalyticsService::instance().search_deals(q);
        }
    });
    vl->addWidget(search_box);

    // Results area
    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 4: STARTUP VALUATION
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

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 8: DEAL COMPARISON
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_comparison_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Compare ──
    auto* cmp = new QWidget(this);
    auto* cmp_vl = new QVBoxLayout(cmp);
    cmp_vl->setContentsMargins(12, 12, 12, 12);
    cmp_vl->setSpacing(8);

    auto* cmp_hint = new QLabel("Enter deal data as JSON array. Each deal: "
                                "{\"acquirer\":\"...\",\"target\":\"...\",\"deal_value\":N,\"premium\":N,"
                                "\"ev_revenue\":N,\"ev_ebitda\":N}",
                                cmp);
    cmp_hint->setWordWrap(true);
    cmp_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY));
    cmp_vl->addWidget(cmp_hint);

    auto* cmp_text = new QTextEdit(cmp);
    cmp_text->setPlaceholderText("[{\"acquirer\":\"MSFT\",\"target\":\"ATVI\",\"deal_value\":68700,\"premium\":45.3,"
                                 "\"ev_revenue\":8.7,\"ev_ebitda\":23.1}]");
    cmp_text->setMaximumHeight(120);
    cmp_text->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:8px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::SMALL));
    cmp_vl->addWidget(cmp_text);

    auto* cmp_run = make_run_button("COMPARE DEALS", cmp);
    connect(cmp_run, &QPushButton::clicked, this, [this, cmp_text]() {
        status_label_->setText("Comparing Deals...");
        auto doc = QJsonDocument::fromJson(cmp_text->toPlainText().toUtf8());
        QJsonObject params;
        params["deals"] = doc.array();
        MAAnalyticsService::instance().compare_deals(params);
    });
    cmp_vl->addWidget(cmp_run);
    cmp_vl->addStretch();
    sub_tabs_->addTab(cmp, "Compare");

    // ── Rank ──
    auto* rank = new QWidget(this);
    auto* rank_vl = new QVBoxLayout(rank);
    rank_vl->setContentsMargins(12, 12, 12, 12);
    rank_vl->setSpacing(8);

    auto* rank_criteria = new QComboBox(rank);
    rank_criteria->addItems({"premium", "deal_value", "ev_revenue", "ev_ebitda", "synergies"});
    rank_criteria->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                         "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                     .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::fonts::SMALL));
    combo_inputs_["rank_criteria"] = rank_criteria;
    rank_vl->addWidget(build_input_row("Rank By", rank_criteria, rank));

    auto* rank_text = new QTextEdit(rank);
    rank_text->setPlaceholderText("Same JSON array format as Compare tab...");
    rank_text->setMaximumHeight(120);
    rank_text->setStyleSheet(cmp_text->styleSheet());
    rank_vl->addWidget(rank_text);

    auto* rank_run = make_run_button("RANK DEALS", rank);
    connect(rank_run, &QPushButton::clicked, this, [this, rank_text]() {
        status_label_->setText("Ranking Deals...");
        auto doc = QJsonDocument::fromJson(rank_text->toPlainText().toUtf8());
        QJsonObject params;
        params["deals"] = doc.array();
        params["criteria"] = combo_inputs_["rank_criteria"]->currentText();
        MAAnalyticsService::instance().rank_deals(params);
    });
    rank_vl->addWidget(rank_run);
    rank_vl->addStretch();
    sub_tabs_->addTab(rank, "Rank");

    // ── Benchmark ──
    auto* bench = new QWidget(this);
    auto* bench_vl = new QVBoxLayout(bench);
    bench_vl->setContentsMargins(12, 12, 12, 12);
    bench_vl->setSpacing(8);

    auto* bench_premium = make_double_spin(0, 200, 30, 1, "%", bench);
    double_inputs_["bench_premium"] = bench_premium;
    bench_vl->addWidget(build_input_row("Target Premium %", bench_premium, bench));

    auto* bench_text = new QTextEdit(bench);
    bench_text->setPlaceholderText("Comparable deals JSON array...");
    bench_text->setMaximumHeight(120);
    bench_text->setStyleSheet(cmp_text->styleSheet());
    bench_vl->addWidget(bench_text);

    auto* bench_run = make_run_button("BENCHMARK PREMIUM", bench);
    connect(bench_run, &QPushButton::clicked, this, [this, bench_text]() {
        status_label_->setText("Benchmarking Premium...");
        auto doc = QJsonDocument::fromJson(bench_text->toPlainText().toUtf8());
        QJsonObject params;
        params["target_premium"] = double_inputs_["bench_premium"]->value() / 100.0;
        params["comparables"] = doc.array();
        MAAnalyticsService::instance().benchmark_deal_premium(params);
    });
    bench_vl->addWidget(bench_run);
    bench_vl->addStretch();
    sub_tabs_->addTab(bench, "Benchmark");

    // ── Payment Structure ──
    auto* pay = new QWidget(this);
    auto* pay_vl = new QVBoxLayout(pay);
    pay_vl->setContentsMargins(12, 12, 12, 12);
    pay_vl->setSpacing(8);

    auto* pay_text = new QTextEdit(pay);
    pay_text->setPlaceholderText("Deals JSON with cash_pct and stock_pct fields...");
    pay_text->setMaximumHeight(120);
    pay_text->setStyleSheet(cmp_text->styleSheet());
    pay_vl->addWidget(pay_text);

    auto* pay_run = make_run_button("ANALYZE PAYMENT STRUCTURES", pay);
    connect(pay_run, &QPushButton::clicked, this, [this, pay_text]() {
        status_label_->setText("Analyzing Payment Structures...");
        auto doc = QJsonDocument::fromJson(pay_text->toPlainText().toUtf8());
        QJsonObject params;
        params["deals"] = doc.array();
        MAAnalyticsService::instance().analyze_payment_structures(params);
    });
    pay_vl->addWidget(pay_run);
    pay_vl->addStretch();
    sub_tabs_->addTab(pay, "Payment");

    // ── Industry ──
    auto* ind = new QWidget(this);
    auto* ind_vl = new QVBoxLayout(ind);
    ind_vl->setContentsMargins(12, 12, 12, 12);
    ind_vl->setSpacing(8);

    auto* ind_text = new QTextEdit(ind);
    ind_text->setPlaceholderText("Deals JSON with industry field...");
    ind_text->setMaximumHeight(120);
    ind_text->setStyleSheet(cmp_text->styleSheet());
    ind_vl->addWidget(ind_text);

    auto* ind_run = make_run_button("ANALYZE BY INDUSTRY", ind);
    connect(ind_run, &QPushButton::clicked, this, [this, ind_text]() {
        status_label_->setText("Analyzing Industry Deals...");
        auto doc = QJsonDocument::fromJson(ind_text->toPlainText().toUtf8());
        QJsonObject params;
        params["deals"] = doc.array();
        MAAnalyticsService::instance().analyze_industry_deals(params);
    });
    ind_vl->addWidget(ind_run);
    ind_vl->addStretch();
    sub_tabs_->addTab(ind, "Industry");

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RESULT DISPLAY
// ═══════════════════════════════════════════════════════════════════════════════

void MAModulePanel::clear_results() {
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void MAModulePanel::display_error(const QString& msg) {
    clear_results();
    auto* err = new QLabel(msg);
    err->setWordWrap(true);
    QColor neg(ui::colors::NEGATIVE());
    auto neg_rgb = QString("%1,%2,%3").arg(neg.red()).arg(neg.green()).arg(neg.blue());
    err->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:12px;"
                               "background:rgba(%4,0.08); border:1px solid rgba(%4,0.3);")
                           .arg(ui::colors::NEGATIVE())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY())
                           .arg(neg_rgb));
    results_layout_->addWidget(err);
    status_label_->setText("Error");
}

static QString format_value(const QJsonValue& val) {
    if (val.isDouble()) {
        double v = val.toDouble();
        if (std::abs(v) >= 1e9)
            return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
        if (std::abs(v) >= 1e6)
            return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
        if (std::abs(v) >= 1e3)
            return QString("$%1K").arg(v / 1e3, 0, 'f', 0);
        if (std::abs(v) < 1.0 && std::abs(v) > 0.0001)
            return QString("%1%").arg(v * 100, 0, 'f', 1);
        return QString::number(v, 'f', 2);
    }
    if (val.isBool())
        return val.toBool() ? "YES" : "NO";
    if (val.isString())
        return val.toString();
    return QString::fromUtf8("—");
}

static QTableWidget* build_json_table(const QJsonArray& arr, const QString& accent, QWidget* parent) {
    if (arr.isEmpty())
        return nullptr;
    // Collect all column keys from first object
    auto first = arr[0].toObject();
    QStringList cols;
    for (auto it = first.begin(); it != first.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            cols.append(it.key());
    if (cols.isEmpty())
        return nullptr;

    auto* table = new QTableWidget(arr.size(), cols.size(), parent);
    table->setHorizontalHeaderLabels(cols);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setMaximumHeight(300);
    table->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                 "font-family:%4; font-size:%5px; border:1px solid %3; }"
                                 "QTableWidget::item { padding:4px 8px; }"
                                 "QTableWidget::item:selected { background:%6; }"
                                 "QHeaderView::section { background:%7; color:%8; font-weight:700;"
                                 "padding:4px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                 "QTableWidget::item:alternate { background:%9; }")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::fonts::SMALL)
                             .arg(QString("rgba(%1,0.15)").arg(accent))
                             .arg(ui::colors::BG_RAISED())
                             .arg(ui::colors::TEXT_SECONDARY())
                             .arg(ui::colors::ROW_ALT()));

    for (int r = 0; r < arr.size(); ++r) {
        auto obj = arr[r].toObject();
        for (int c = 0; c < cols.size(); ++c) {
            auto* item = new QTableWidgetItem(format_value(obj[cols[c]]));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, c, item);
        }
    }
    table->resizeColumnsToContents();
    return table;
}

static QTableWidget* build_kv_table(const QJsonObject& obj, const QString& accent, QWidget* parent) {
    // Collect only scalar key-value pairs
    QStringList keys;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            keys.append(it.key());
    if (keys.isEmpty())
        return nullptr;

    auto* table = new QTableWidget(keys.size(), 2, parent);
    table->setHorizontalHeaderLabels({"Metric", "Value"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setMaximumHeight(400);
    table->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                 "font-family:%4; font-size:%5px; border:1px solid %3; }"
                                 "QTableWidget::item { padding:4px 8px; }"
                                 "QTableWidget::item:selected { background:%6; }"
                                 "QHeaderView::section { background:%7; color:%8; font-weight:700;"
                                 "padding:4px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                 "QTableWidget::item:alternate { background:%9; }")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::fonts::SMALL)
                             .arg(QString("rgba(%1,0.15)").arg(accent))
                             .arg(ui::colors::BG_RAISED())
                             .arg(ui::colors::TEXT_SECONDARY())
                             .arg(ui::colors::ROW_ALT()));

    for (int r = 0; r < keys.size(); ++r) {
        auto label = keys[r];
        label.replace('_', ' ');
        auto* key_item = new QTableWidgetItem(label.toUpper());
        key_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->setItem(r, 0, key_item);
        auto* val_item = new QTableWidgetItem(format_value(obj[keys[r]]));
        val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(r, 1, val_item);
    }
    table->resizeColumnsToContents();
    return table;
}

void MAModulePanel::display_result(const QJsonObject& payload) {
    clear_results();

    QString accent = QString("%1,%2,%3").arg(module_.color.red()).arg(module_.color.green()).arg(module_.color.blue());

    // Section header
    auto* header = new QLabel("RESULTS");
    header->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;"
                                  "padding:4px 0;")
                              .arg(module_.color.name())
                              .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(header);

    // 1. Metric cards for top-level scalar values
    auto* grid = new QWidget(this);
    auto* gl = new QGridLayout(grid);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(8);

    int col = 0, row = 0;
    bool has_scalars = false;
    for (auto it = payload.begin(); it != payload.end(); ++it) {
        if (it.value().isObject() || it.value().isArray())
            continue;
        has_scalars = true;
        QString label = it.key();
        label.replace('_', ' ');
        auto* card = build_metric_card(label.toUpper(), format_value(it.value()), module_.color.name(), grid);
        gl->addWidget(card, row, col);
        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }
    if (has_scalars)
        results_layout_->addWidget(grid);
    else
        grid->deleteLater();

    // 2. Tables for nested objects and arrays
    for (auto it = payload.begin(); it != payload.end(); ++it) {
        if (it.value().isArray()) {
            auto arr = it.value().toArray();
            if (arr.isEmpty())
                continue;

            QString sec_label = it.key();
            sec_label.replace('_', ' ');
            auto* sec = new QLabel(sec_label.toUpper());
            sec->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                       "letter-spacing:1px; padding:8px 0 4px 0;")
                                   .arg(module_.color.name())
                                   .arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(sec);

            if (arr[0].isObject()) {
                auto* table = build_json_table(arr, accent, this);
                if (table)
                    results_layout_->addWidget(table);
            } else {
                // Simple array — display as comma-separated
                QStringList items;
                for (const auto& v : arr)
                    items.append(format_value(v));
                auto* lbl = new QLabel(items.join(", "));
                lbl->setWordWrap(true);
                lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;"
                                           "padding:4px; background:%4; border:1px solid %5;")
                                       .arg(ui::colors::TEXT_PRIMARY())
                                       .arg(ui::fonts::SMALL)
                                       .arg(ui::fonts::DATA_FAMILY)
                                       .arg(ui::colors::BG_RAISED())
                                       .arg(ui::colors::BORDER_DIM()));
                results_layout_->addWidget(lbl);
            }
        } else if (it.value().isObject()) {
            auto obj = it.value().toObject();
            if (obj.isEmpty())
                continue;

            QString sec_label = it.key();
            sec_label.replace('_', ' ');
            auto* sec = new QLabel(sec_label.toUpper());
            sec->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                       "letter-spacing:1px; padding:8px 0 4px 0;")
                                   .arg(module_.color.name())
                                   .arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(sec);

            auto* table = build_kv_table(obj, accent, this);
            if (table)
                results_layout_->addWidget(table);
        }
    }

    // 3. Raw JSON viewer (collapsed)
    auto* raw_btn = new QPushButton("Show Raw JSON", this);
    raw_btn->setCursor(Qt::PointingHandCursor);
    raw_btn->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                   "background:transparent; border:1px solid %4; padding:4px 12px; }"
                                   "QPushButton:hover { background:%5; }")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::TINY)
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(ui::colors::BORDER_DIM())
                               .arg(ui::colors::BG_HOVER()));

    auto* raw_text = new QTextEdit;
    raw_text->setReadOnly(true);
    raw_text->setVisible(false);
    raw_text->setMaximumHeight(300);
    raw_text->setPlainText(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    raw_text->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:8px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::SMALL));

    connect(raw_btn, &QPushButton::clicked, this, [raw_text, raw_btn]() {
        bool showing = raw_text->isVisible();
        raw_text->setVisible(!showing);
        raw_btn->setText(showing ? "Show Raw JSON" : "Hide Raw JSON");
    });

    results_layout_->addWidget(raw_btn);
    results_layout_->addWidget(raw_text);

    status_label_->setText("Done");
}

// ── Service signal handlers ──────────────────────────────────────────────────

static const QHash<ModuleId, QStringList>& get_context_map() {
    static const QHash<ModuleId, QStringList> map = {
        {ModuleId::Valuation,
         {"dcf", "dcf_sensitivity", "football_field", "lbo_returns", "lbo_model", "lbo_debt_schedule",
          "lbo_sensitivity", "trading_comps", "precedent_txns"}},
        {ModuleId::Merger,
         {"merger_model", "accretion_dilution", "pro_forma", "sources_uses", "contribution", "revenue_synergies",
          "cost_synergies", "synergy_dcf", "integration_costs", "payment_structure", "earnout", "exchange_ratio",
          "collar", "cvr"}},
        {ModuleId::Deals, {"scan_filings", "all_deals", "search_deals", "create_deal", "update_deal", "parse_filing"}},
        {ModuleId::Startup,
         {"berkus", "scorecard", "vc_method", "first_chicago", "risk_factor", "startup_comprehensive"}},
        {ModuleId::Fairness, {"fairness_opinion", "premium_analysis", "process_quality"}},
        {ModuleId::Industry, {"tech_metrics", "healthcare_metrics", "finserv_metrics"}},
        {ModuleId::Advanced, {"monte_carlo", "regression"}},
        {ModuleId::Comparison,
         {"compare_deals", "rank_deals", "benchmark_premium", "payment_structures", "industry_deals"}},
    };
    return map;
}

void MAModulePanel::on_result_ready(const QString& context, const QJsonObject& payload) {
    auto it = get_context_map().find(module_.id);
    if (it != get_context_map().end() && it->contains(context)) {
        display_result(payload);
    }
}

void MAModulePanel::on_error(const QString& context, const QString& message) {
    auto it = get_context_map().find(module_.id);
    if (it != get_context_map().end() && it->contains(context)) {
        display_error(QString("[%1] %2").arg(context, message));
    }
}

// ── Tab stylesheet helper ───────────────────────────────────────────────────
void MAModulePanel::apply_tab_stylesheet() {
    if (!sub_tabs_)
        return;
    sub_tabs_->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                     "QTabBar::tab { background:%3; color:%4; padding:6px 16px;"
                                     "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                     "QTabBar::tab:selected { background:%2; color:%7; font-weight:700;"
                                     "border-bottom:2px solid %7; }")
                                 .arg(ui::colors::BORDER_DIM(), ui::colors::BG_SURFACE(), ui::colors::BG_RAISED())
                                 .arg(ui::colors::TEXT_SECONDARY())
                                 .arg(ui::fonts::DATA_FAMILY())
                                 .arg(ui::fonts::SMALL)
                                 .arg(module_.color.name()));
}

// ── Theme refresh ───────────────────────────────────────────────────────────
void MAModulePanel::refresh_theme() {
    // Header bar
    if (header_bar_)
        header_bar_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                       .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    // Header title
    if (header_title_)
        header_title_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                .arg(module_.color.name())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY()));

    // Header divider
    if (header_bar_) {
        auto* div = header_bar_->findChild<QWidget*>("maPanelDivider");
        if (div)
            div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    }

    // Header category
    if (header_category_)
        header_category_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                            .arg(ui::colors::TEXT_TERTIARY())
                                            .arg(ui::fonts::TINY)
                                            .arg(ui::fonts::DATA_FAMILY()));

    // Status label
    if (status_label_)
        status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                         .arg(ui::colors::TEXT_TERTIARY())
                                         .arg(ui::fonts::TINY)
                                         .arg(ui::fonts::DATA_FAMILY()));

    // Tab widget
    apply_tab_stylesheet();
}

} // namespace fincept::screens
