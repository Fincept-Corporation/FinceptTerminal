// MAIndustryPanel.cpp — industry-metric snapshot sub-tab builder for MAModulePanel.
// Method definition split from MAModulePanel.cpp.
//
// Each tab's Sector combo switches the input rows to that sector's own inputs
// (the analyzers of technology.py / healthcare.py / financial_services.py take
// different fields per sector), and the run sends exactly those keys. The
// scripts no longer fill a missing sector input with an assumption — that used
// to come back looking like a result (a GMV of ARR / 15%, a combined ratio that
// was the Efficiency Ratio field).

#include "screens/ma_analytics/MAModulePanel.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <functional>

using namespace fincept::services::ma;

namespace fincept::screens {

using namespace fincept::services::ma;

namespace {

#define MA_IND_TR(text) QT_TRANSLATE_NOOP("fincept::screens::MAModulePanel", text)

/// One input of a sector's analyzer.
struct MaSectorField {
    enum Kind { Number, Integer, YesNo };
    const char* label;   ///< translated at build time (MAModulePanel context)
    const char* key;     ///< JSON key sent to the script
    double value;        ///< default
    double min;
    double max;
    int decimals;
    const char* suffix;
    double send_scale;   ///< multiplied into the sent value (0.01: a percent input sent as a fraction)
    bool money;          ///< label gets the live currency hint
    Kind kind;
};

struct MaSector {
    const char* id; ///< the script's sector name
    QVector<MaSectorField> fields;
};

using F = MaSectorField;

const QVector<MaSector>& ma_tech_sectors() {
    static const QVector<MaSector> s = {
        {"saas",
         {{MA_IND_TR("ARR"), "arr", 100e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Growth Rate"), "growth", 40, -100, 500, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Gross Margin"), "gross_margin", 75, 0, 100, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Net Revenue Retention"), "nrr", 115, 50, 200, 0, "%", 0.01, false, F::Number},
          {MA_IND_TR("CAC Payback (months)"), "cac_payback", 18, 0, 120, 0, " mo", 1, false, F::Number},
          {MA_IND_TR("LTV/CAC Ratio"), "ltv_cac", 3.5, 0, 20, 1, "x", 1, false, F::Number},
          {MA_IND_TR("Rule of 40 Score"), "rule_of_40", 55, -50, 200, 0, "", 1, false, F::Number}}},
        {"marketplace",
         {{MA_IND_TR("Gross Merchandise Value"), "gmv", 800e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Take Rate"), "take_rate", 15, 0, 100, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Net Revenue"), "revenue", 120e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Liquidity Score (0-100)"), "liquidity_score", 70, 0, 100, 0, "", 1, false, F::Number},
          {MA_IND_TR("Supply-side Retention"), "supply_side_retention", 80, 0, 100, 0, "%", 1, false, F::Number},
          {MA_IND_TR("Demand-side Retention (repeat rate)"), "demand_side_retention", 75, 0, 100, 0, "%", 1, false,
           F::Number}}},
        {"semiconductor",
         {{MA_IND_TR("Revenue"), "revenue", 500e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Gross Margin"), "gross_margin", 55, 0, 100, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("R&D Intensity"), "r_and_d_pct", 18, 0, 100, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Design-win Backlog"), "design_win_backlog", 750e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Process Node (nm)"), "process_node", 7, 1, 250, 0, "", 1, false, F::Integer},
          {MA_IND_TR("Fabless"), "fabless", 1, 0, 1, 0, "", 1, false, F::YesNo}}},
    };
    return s;
}

const QVector<MaSector>& ma_healthcare_sectors() {
    static const QVector<MaSector> s = {
        {"pharma",
         {{MA_IND_TR("Revenue"), "revenue", 5e9, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("EBITDA Margin"), "ebitda_margin", 35, 0, 100, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Pipeline NPV"), "pipeline_npv", 3e9, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Phase 3 Candidates"), "phase3_candidates", 5, 0, 50, 0, "", 1, false, F::Integer},
          {MA_IND_TR("Patent Expiry Rev %"), "patent_expiry_revenue", 25, 0, 100, 0, "%", 0.01, false, F::Number},
          {MA_IND_TR("R&D Spend"), "rd_spend", 800e6, 0, 1e15, 0, "", 1, true, F::Number}}},
        {"biotech",
         {{MA_IND_TR("Cash Position"), "cash_position", 350e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Monthly Burn"), "burn_rate", 15e6, 0, 1e13, 0, "", 1, true, F::Number},
          {MA_IND_TR("Pipeline Value"), "pipeline_value", 900e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Lead Program Phase (1-3)"), "lead_program_phase", 2, 1, 3, 0, "", 1, false, F::Integer},
          {MA_IND_TR("Platform Value"), "platform_value", 0, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Partnership Revenue"), "partnership_revenue", 0, 0, 1e15, 0, "", 1, true, F::Number}}},
        {"devices",
         {{MA_IND_TR("Revenue"), "revenue", 400e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("Gross Margin"), "gross_margin", 65, 0, 100, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Clinical Evidence (0-100)"), "clinical_evidence_strength", 60, 0, 100, 0, "", 1, false,
           F::Integer},
          {MA_IND_TR("Regulatory Clearances (FDA, CE)"), "fda_clearances", 1, 0, 2, 0, "", 1, false, F::Integer},
          {MA_IND_TR("Reimbursement Coverage"), "reimbursement_coverage", 70, 0, 100, 0, "%", 1, false, F::Number},
          {MA_IND_TR("Installed Base (units)"), "installed_base", 5000, 0, 1e9, 0, "", 1, false, F::Integer},
          {MA_IND_TR("Recurring Revenue"), "recurring_revenue_pct", 35, 0, 100, 0, "%", 1, false, F::Number}}},
        {"services",
         {{MA_IND_TR("Revenue"), "revenue", 700e6, 0, 1e15, 0, "", 1, true, F::Number},
          {MA_IND_TR("EBITDA Margin"), "ebitda_margin", 16, 0, 100, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Same-store Growth"), "same_store_growth", 4, -50, 100, 1, "%", 1, false, F::Number},
          {MA_IND_TR("Commercial Payor Mix"), "payor_mix_commercial", 55, 0, 100, 0, "%", 1, false, F::Number}}},
    };
    return s;
}

const QVector<MaSector>& ma_finserv_sectors() {
    static const QVector<MaSector> s = {
        {"banking",
         {{MA_IND_TR("Total Assets"), "total_assets", 50e9, 0, 1e18, 0, "", 1, true, F::Number},
          {MA_IND_TR("Deposits"), "deposits", 40e9, 0, 1e18, 0, "", 1, true, F::Number},
          {MA_IND_TR("Loans"), "loans", 30e9, 0, 1e18, 0, "", 1, true, F::Number},
          {MA_IND_TR("ROE"), "roe", 12, -100, 200, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Net Interest Margin"), "nim", 3.2, 0, 20, 2, "%", 0.01, false, F::Number},
          {MA_IND_TR("Efficiency Ratio"), "efficiency_ratio", 58, 0, 200, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("CET1 Ratio"), "cet1_ratio", 12.5, 0, 30, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("NPL Ratio"), "npl_ratio", 0.8, 0, 30, 2, "%", 0.01, false, F::Number}}},
        {"insurance",
         {{MA_IND_TR("Gross Written Premium"), "gross_written_premium", 5e9, 0, 1e18, 0, "", 1, true, F::Number},
          {MA_IND_TR("Combined Ratio"), "combined_ratio", 95, 0, 200, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Loss Ratio"), "loss_ratio", 65, 0, 200, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Expense Ratio"), "expense_ratio", 30, 0, 200, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Investment Yield"), "investment_yield", 4.2, 0, 30, 2, "%", 0.01, false, F::Number},
          {MA_IND_TR("ROE"), "roe", 12, -100, 200, 1, "%", 0.01, false, F::Number},
          {MA_IND_TR("Book Value"), "book_value", 6e9, 0, 1e18, 0, "", 1, true, F::Number}}},
        {"asset_management",
         {{MA_IND_TR("Assets Under Management"), "aum", 80e9, 0, 1e18, 0, "", 1, true, F::Number},
          {MA_IND_TR("Net Flows"), "net_flows", 2e9, -1e18, 1e18, 0, "", 1, true, F::Number},
          {MA_IND_TR("Fee Rate (bps)"), "fee_rate", 45, 0, 500, 1, " bps", 1, false, F::Number},
          {MA_IND_TR("Client Retention"), "client_retention", 92, 0, 100, 1, "%", 1, false, F::Number},
          {MA_IND_TR("Revenue per Advisor"), "advisor_productivity", 1.5e6, 0, 1e12, 0, "", 1, true, F::Number},
          {MA_IND_TR("Operating Margin"), "operating_margin", 32, -100, 100, 1, "%", 0.01, false, F::Number}}},
    };
    return s;
}

#undef MA_IND_TR

} // namespace

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

    const QString combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                        "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                    .arg(ui::fonts::DATA_FAMILY)
                                    .arg(ui::fonts::SMALL.get());

    // One tab: Sector combo + one page of input rows per sector + a run button
    // that sends the selected sector's own keys.
    auto build_tab = [this, &combo_style](const QString& prefix, const QVector<MaSector>& sectors,
                                          const QString& run_text, const QString& running_text,
                                          std::function<void(const QJsonObject&)> run) {
        auto* tab = new QWidget(this);
        auto* tvl = new QVBoxLayout(tab);
        tvl->setContentsMargins(12, 12, 12, 12);
        tvl->setSpacing(8);

        auto* sector_combo = new QComboBox(tab);
        for (const MaSector& s : sectors)
            sector_combo->addItem(QString::fromLatin1(s.id));
        sector_combo->setStyleSheet(combo_style);
        combo_inputs_[prefix + "_sector"] = sector_combo;
        tvl->addWidget(build_input_row(tr("Sector"), sector_combo, tab));

        auto* pages = new QStackedWidget(tab);
        for (const MaSector& s : sectors) {
            auto* page = new QWidget(pages);
            auto* pvl = new QVBoxLayout(page);
            pvl->setContentsMargins(0, 0, 0, 0);
            pvl->setSpacing(8);
            const QString base = prefix + "_" + QString::fromLatin1(s.id) + "_";
            for (const MaSectorField& f : s.fields) {
                const QString label = tr(f.label) + (f.money ? QStringLiteral(" ($)") : QString());
                const QString id = base + QString::fromLatin1(f.key);
                if (f.kind == MaSectorField::YesNo) {
                    auto* cb = new QComboBox(page);
                    cb->addItem(tr("Yes"), true);
                    cb->addItem(tr("No"), false);
                    cb->setCurrentIndex(f.value != 0 ? 0 : 1);
                    cb->setStyleSheet(combo_style);
                    combo_inputs_[id] = cb;
                    pvl->addWidget(build_input_row(label, cb, page));
                } else if (f.kind == MaSectorField::Integer) {
                    auto* sp = make_int_spin(int(f.min), int(f.max), int(f.value), page);
                    int_inputs_[id] = sp;
                    pvl->addWidget(build_input_row(label, sp, page));
                } else {
                    auto* sp = make_double_spin(f.min, f.max, f.value, f.decimals, QString::fromLatin1(f.suffix), page);
                    double_inputs_[id] = sp;
                    pvl->addWidget(build_input_row(label, sp, page));
                }
            }
            pvl->addStretch();
            pages->addWidget(page);
        }
        connect(sector_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), pages,
                &QStackedWidget::setCurrentIndex);
        tvl->addWidget(pages);

        auto* run_btn = make_run_button(run_text, tab);
        connect(run_btn, &QPushButton::clicked, this, [this, prefix, sectors, running_text, run]() {
            status_label_->setText(running_text);
            const int idx = std::clamp(combo_inputs_[prefix + "_sector"]->currentIndex(), 0, int(sectors.size()) - 1);
            const MaSector& s = sectors.at(idx);
            const QString base = prefix + "_" + QString::fromLatin1(s.id) + "_";
            QJsonObject params;
            params["sector"] = QString::fromLatin1(s.id);
            for (const MaSectorField& f : s.fields) {
                const QString id = base + QString::fromLatin1(f.key);
                const QString key = QString::fromLatin1(f.key);
                if (f.kind == MaSectorField::YesNo)
                    params[key] = combo_inputs_[id]->currentData().toBool();
                else if (f.kind == MaSectorField::Integer)
                    params[key] = int_inputs_[id]->value();
                else
                    params[key] = double_inputs_[id]->value() * f.send_scale;
            }
            run(params);
        });
        tvl->addWidget(run_btn);
        tvl->addStretch();
        return tab;
    };

    add_sub_tab(build_tab(QStringLiteral("tech"), ma_tech_sectors(), tr("CALCULATE TECH METRICS"),
                          tr("Calculating Tech Metrics..."),
                          [](const QJsonObject& p) { MAAnalyticsService::instance().calculate_tech_metrics(p); }),
                QT_TR_NOOP("Technology"));
    add_sub_tab(build_tab(QStringLiteral("hc"), ma_healthcare_sectors(), tr("CALCULATE HC METRICS"),
                          tr("Calculating Healthcare Metrics..."),
                          [](const QJsonObject& p) { MAAnalyticsService::instance().calculate_healthcare_metrics(p); }),
                QT_TR_NOOP("Healthcare"));
    add_sub_tab(build_tab(QStringLiteral("fs"), ma_finserv_sectors(), tr("CALCULATE FINSERV METRICS"),
                          tr("Calculating FinServ Metrics..."),
                          [](const QJsonObject& p) {
                              MAAnalyticsService::instance().calculate_financial_services_metrics(p);
                          }),
                QT_TR_NOOP("Financial Services"));

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
