// src/screens/economics/panels/EiaPanel.cpp
// EIA U.S. Energy Information Administration
//
// Two data sources with different commands and response shapes:
//
// WPSR (Weekly Petroleum Status Report) — NO API KEY NEEDED
//   Command: get_petroleum <category>
//   Response: { "success": true, "category": "...", "data": [...], "count": N }
//   data[] rows: { "date": "YYYY-MM-DD", "category": "...", "table": "...",
//                  "symbol": "...", "value": 1.23, "source": "petroleum_status_report" }
//
// STEO (Short-Term Energy Outlook) — REQUIRES EIA_API_KEY
//   Command: get_steo <table>
//   Response: { "success": true, "table": "...", "data": [...] }
//   data[] rows: { "date": "YYYY-MM", "symbol": "WTIPUUS", "title": "WTI Crude...",
//                  "value": 78.5, "units": "$/barrel", "table": "STEO-01: ...",
//                  "source": "short_term_energy_outlook" }
#include "screens/economics/panels/EiaPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {

static constexpr const char* kScript   = "eia_data.py";
static constexpr const char* kSourceId = "eia";
static constexpr const char* kColor    = "#4CAF50";  // green

// ── WPSR categories (no API key needed, downloads public XLS)
static const QList<QPair<QString,QString>> kWpsrCategories = {
    {"Balance Sheet",                        "balance_sheet"},
    {"Inputs & Production",                  "inputs_and_production"},
    {"Crude Petroleum Stocks",               "crude_petroleum_stocks"},
    {"Gasoline & Fuel Stocks",               "gasoline_fuel_stocks"},
    {"Gasoline by Sub-PADD",                 "total_gasoline_by_sub_padd"},
    {"Distillate Fuel Oil Stocks",           "distillate_fuel_oil_stocks"},
    {"Imports",                              "imports"},
    {"Imports by Country",                   "imports_by_country"},
    {"Weekly Estimates",                     "weekly_estimates"},
    {"Spot Prices (Crude/Gas/Heating Oil)",  "spot_prices_crude_gas_heating"},
    {"Spot Prices (Diesel/Jet/Propane)",     "spot_prices_diesel_jet_fuel_propane"},
    {"Retail Prices",                        "retail_prices"},
    {"Refiner/Blender Net Production",       "refiner_blender_net_production"},
};

// ── STEO tables (requires EIA_API_KEY)
static const QList<QPair<QString,QString>> kSteoTables = {
    {"01 — US Energy Markets Summary",           "01"},
    {"02 — Nominal Energy Prices",               "02"},
    {"04a — Petroleum & Liquid Fuels",           "04a"},
    {"05a — Natural Gas Supply & Consumption",   "05a"},
    {"07a — US Electricity Industry Overview",   "07a"},
    {"09a — Macroeconomic Indicators & CO2",     "09a"},
};

// ── Constructor ───────────────────────────────────────────────────────────────

EiaPanel::EiaPanel(QWidget* parent)
    : EconPanelBase(kSourceId, kColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &EiaPanel::on_result);
}

void EiaPanel::activate() {
    show_empty("Select a data source and category, then click FETCH\n"
               "Weekly Petroleum Status Report requires no API key\n"
               "Short-Term Energy Outlook requires EIA_API_KEY env var");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void EiaPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    source_combo_ = new QComboBox;
    source_combo_->addItem("Weekly Petroleum (WPSR)",      "wpsr");
    source_combo_->addItem("Short-Term Outlook (STEO)",    "steo");
    source_combo_->setFixedHeight(26);
    source_combo_->setMinimumWidth(200);
    connect(source_combo_, &QComboBox::currentIndexChanged,
            this, &EiaPanel::on_source_changed);

    category_combo_ = new QComboBox;
    category_combo_->setFixedHeight(26);
    category_combo_->setMinimumWidth(260);

    // Populate with WPSR categories initially
    for (const auto& c : kWpsrCategories)
        category_combo_->addItem(c.first, c.second);

    apikey_notice_ = new QLabel("No API key needed");
    apikey_notice_->setStyleSheet(
        "color:#16a34a; font-size:9px; background:transparent;");

    thl->addWidget(make_lbl("SOURCE"));
    thl->addWidget(source_combo_);
    thl->addWidget(make_lbl("CATEGORY"));
    thl->addWidget(category_combo_);
    thl->addWidget(apikey_notice_);
}

void EiaPanel::on_source_changed(int index) {
    category_combo_->clear();
    if (index == 0) {
        // WPSR
        for (const auto& c : kWpsrCategories)
            category_combo_->addItem(c.first, c.second);
        apikey_notice_->setText("No API key needed");
        apikey_notice_->setStyleSheet(
            "color:#16a34a; font-size:9px; background:transparent;");
    } else {
        // STEO
        for (const auto& t : kSteoTables)
            category_combo_->addItem(t.first, t.second);
        apikey_notice_->setText("Requires EIA_API_KEY");
        apikey_notice_->setStyleSheet(
            "color:#f59e0b; font-size:9px; background:transparent;");
    }
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void EiaPanel::on_fetch() {
    const QString source   = source_combo_->currentData().toString();
    const QString category = category_combo_->currentData().toString();

    if (category.isEmpty()) {
        show_empty("Select a category");
        return;
    }

    if (source == "wpsr") {
        show_loading("Fetching EIA Petroleum Report: " +
                     category_combo_->currentText() + "…\n"
                     "(Downloads public XLS file — may take a few seconds)");
        services::EconomicsService::instance().execute(
            kSourceId, kScript, "get_petroleum",
            {category},
            "eia_wpsr_" + category);
    } else {
        show_loading("Fetching EIA STEO Table " +
                     category_combo_->currentText() + "…");
        services::EconomicsService::instance().execute(
            kSourceId, kScript, "get_steo",
            {category},
            "eia_steo_" + category);
    }
}

// ── Result ────────────────────────────────────────────────────────────────────

void EiaPanel::on_result(const QString& request_id,
                         const services::EconomicsResult& result) {
    if (result.source_id != kSourceId) return;

    if (!result.success) {
        // Detect API key error for STEO
        if (result.error.contains("API key") || result.error.contains("api_key")) {
            show_error("EIA API key not configured.\n"
                       "Set EIA_API_KEY environment variable.\n"
                       "Free key at: www.eia.gov/opendata/register.php");
        } else {
            show_error(result.error);
        }
        return;
    }

    // Both WPSR and STEO return { "success": true, "data": [...] }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_empty("No data returned for this selection");
        return;
    }

    // WPSR rows: { date, category, table, symbol, value, source }
    // STEO rows: { date, symbol, title, value, units, table, source }
    // Both have "date" and "value" — base display() handles both correctly.
    // For WPSR, promote "symbol" as a useful column; for STEO "title" is descriptive.
    // The base display() auto-detects date→col0, value→col1, rest follows.

    const QString title = "EIA: " + category_combo_->currentText();
    display(rows, title);

    LOG_INFO("EiaPanel", QString("Displayed %1 rows — %2")
             .arg(rows.size()).arg(request_id));
}

} // namespace fincept::screens
