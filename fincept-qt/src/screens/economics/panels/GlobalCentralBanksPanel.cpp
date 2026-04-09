// src/screens/economics/panels/GlobalCentralBanksPanel.cpp
// Global Central Banks — BOE, RBA, Bank of Canada, Riksbank, SNB, Norges Bank.
// No API key required for any source.
//
// Response shapes (all use {success, data:[{date, <value_col>}]} or similar):
//   BOE:     { success, data:[{date:"02 Jan 1975", <CDID>:value}] }
//   RBA:     { success, table, data:[{date:"04-Jan-2011", <col>:value}] }
//   BOC:     { success, series, data:[{date:"2026-02-01", <series_id>:value}] }
//   Riksbank:{ success, series_id, data:[{date:"2025-03-28", value:2.25}] }
//   SNB:     { success, cube, data:[{date:"2026-03-16", <col>:value}] }
//   Norges:  { success, flow, data:[{date:"2026-03-17", <col>:value}] }
#include "screens/economics/panels/GlobalCentralBanksPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kGlobalCentralBanksSourceId = "global_cb";
static constexpr const char* kGlobalCentralBanksColor = "#6366F1"; // indigo
} // namespace

// ── Per-bank descriptor ──────────────────────────────────────────────────────

struct CbSeries {
    QString label;
    QString command;
};

struct CbBank {
    QString label;      // display name
    QString script;     // python script filename
    QString req_prefix; // request_id prefix
    QList<CbSeries> series;
};

static const QList<CbBank> kBanks = {
    {"BOE — Bank of England",
     "boe_data.py",
     "boe",
     {
         {"Bank Rate", "bank_rate"},
         {"SONIA Overnight Rate", "sonia"},
         {"Exchange Rates (GBP)", "exchange_rates"},
         {"Monetary Aggregates (M0/M4)", "monetary_aggregates"},
         {"Quoted Interest Rates", "quoted_rates"},
     }},
    {"RBA — Reserve Bank of Australia",
     "rba_data.py",
     "rba",
     {
         {"Cash Rate (F1)", "cash_rate"},
         {"Bond Yields (F2)", "bond_yields"},
         {"Exchange Rates (F11)", "exchange_rates"},
         {"Inflation / CPI", "inflation"},
         {"Lending Rates", "lending_rates"},
         {"Monetary Aggregates", "monetary"},
     }},
    {"BOC — Bank of Canada",
     "boc_data.py",
     "boc",
     {
         {"Overnight Policy Rate", "policy_rate"},
         {"CORRA", "corra"},
         {"Prime Rate", "prime"},
         {"USD/CAD", "usd"},
         {"EUR/CAD", "eur"},
     }},
    {"Riksbank — Sweden",
     "riksbank_data.py",
     "riksbank",
     {
         {"Policy Rate", "policy_rate"},
         {"Policy + Deposit + Lending", "policy_all"},
         {"T-Bills (1M-6M)", "tbills"},
         {"Mortgage Bond Yields", "mortgage"},
     }},
    {"SNB — Swiss National Bank",
     "snb_data.py",
     "snb",
     {
         {"Policy Rate + SARON", "policy_rate"},
         {"Bond Yields (Monthly)", "bond_yields"},
         {"Bond Yields (Daily)", "bond_yields_d"},
         {"CHF Exchange Rates", "exchange_rates"},
     }},
    {"Norges Bank — Norway",
     "norges_bank_data.py",
     "norges",
     {
         {"Policy Rate Announcements", "policy_rate"},
         {"Exchange Rates (NOK)", "exchange_rates"},
         {"NIBOR / Interest Rates", "interest_rates"},
     }},
};

// ── Flatten helpers ──────────────────────────────────────────────────────────

// All bank response shapes have a "data" array of objects with a "date" key
// and one or more numeric value columns. We keep all columns as-is.
static QJsonArray extract_cb_rows(const QJsonObject& data) {
    return data["data"].toArray();
}

// ── Panel ────────────────────────────────────────────────────────────────────

GlobalCentralBanksPanel::GlobalCentralBanksPanel(QWidget* parent)
    : EconPanelBase(kGlobalCentralBanksSourceId, kGlobalCentralBanksColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &GlobalCentralBanksPanel::on_result);
}

void GlobalCentralBanksPanel::activate() {
    show_empty("Select a central bank and series, then click FETCH\n"
               "Sources: BOE, RBA, Bank of Canada, Riksbank, SNB, Norges Bank\n"
               "No API key required for any source");
}

void GlobalCentralBanksPanel::build_controls(QHBoxLayout* thl) {
    auto* blbl = new QLabel("BANK");
    blbl->setStyleSheet(ctrl_label_style());

    bank_combo_ = new QComboBox;
    for (const auto& b : kBanks)
        bank_combo_->addItem(b.label);
    bank_combo_->setFixedHeight(26);
    bank_combo_->setMinimumWidth(230);

    auto* slbl = new QLabel("SERIES");
    slbl->setStyleSheet(ctrl_label_style());

    series_combo_ = new QComboBox;
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(200);

    // Populate series for the initial bank
    update_series_for_bank(0);

    connect(bank_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &GlobalCentralBanksPanel::update_series_for_bank);

    thl->addWidget(blbl);
    thl->addWidget(bank_combo_);
    thl->addSpacing(8);
    thl->addWidget(slbl);
    thl->addWidget(series_combo_);
}

void GlobalCentralBanksPanel::update_series_for_bank(int bank_idx) {
    if (bank_idx < 0 || bank_idx >= kBanks.size())
        return;
    series_combo_->clear();
    for (const auto& s : kBanks[bank_idx].series)
        series_combo_->addItem(s.label, s.command);
}

void GlobalCentralBanksPanel::on_fetch() {
    const int bi = bank_combo_->currentIndex();
    const int si = series_combo_->currentIndex();
    if (bi < 0 || bi >= kBanks.size())
        return;
    if (si < 0 || si >= kBanks[bi].series.size())
        return;

    const auto& bank = kBanks[bi];
    const auto& series = bank.series[si];

    show_loading("Fetching " + bank.label + ": " + series.label + "…");
    services::EconomicsService::instance().execute(kGlobalCentralBanksSourceId, bank.script, series.command, {},
                                                   bank.req_prefix + "_" + series.command);
}

void GlobalCentralBanksPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kGlobalCentralBanksSourceId)
        return;

    // Check this result belongs to one of our banks
    bool matched = false;
    int matched_bank = -1;
    int matched_series = -1;
    for (int bi = 0; bi < kBanks.size(); ++bi) {
        const auto& bank = kBanks[bi];
        if (!request_id.startsWith(bank.req_prefix + "_"))
            continue;
        matched = true;
        matched_bank = bi;
        const QString cmd = request_id.mid(bank.req_prefix.size() + 1);
        for (int si = 0; si < bank.series.size(); ++si) {
            if (bank.series[si].command == cmd) {
                matched_series = si;
                break;
            }
        }
        break;
    }
    if (!matched)
        return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) {
        show_error(inline_err);
        return;
    }

    QJsonArray raw = extract_cb_rows(result.data);

    // Filter rows: keep only those with at least one numeric value
    QJsonArray rows;
    for (const auto& rv : raw) {
        const QJsonObject r = rv.toObject();
        bool has_value = false;
        for (const auto& key : r.keys()) {
            if (key == "date")
                continue;
            if (r[key].isDouble()) {
                has_value = true;
                break;
            }
        }
        if (has_value)
            rows.append(r);
    }

    if (rows.isEmpty()) {
        show_error("No data returned");
        return;
    }

    const QString bank_name = matched_bank >= 0 ? kBanks[matched_bank].label : "";
    const QString series_name =
        (matched_bank >= 0 && matched_series >= 0) ? kBanks[matched_bank].series[matched_series].label : request_id;
    const QString title = bank_name + ": " + series_name;

    display(rows, title);
    LOG_INFO("GlobalCentralBanksPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

} // namespace fincept::screens
