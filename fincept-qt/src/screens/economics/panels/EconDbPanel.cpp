// src/screens/economics/panels/EconDbPanel.cpp
// EconDB macroeconomic indicators panel. No API key required.
//
// Response shape:
//   { indicator, country, transform, observation_count,
//     metadata: { description, frequency, country, ticker, dataset },
//     observations: [ {date:"YYYY-MM-DD", value:float}, ... ] }
#include "screens/economics/panels/EconDbPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kEconDbScript   = "econdb_data.py";
static constexpr const char* kEconDbSourceId = "econdb";
static constexpr const char* kEconDbColor    = "#059669";  // green
} // namespace

static const QList<QPair<QString,QString>> kEconDbIndicators = {
    { "GDP (Nominal)",              "gdp"   },
    { "Real GDP",                   "rgdp"  },
    { "CPI Inflation",              "cpi"   },
    { "PPI Inflation",              "ppi"   },
    { "Core Inflation",             "core"  },
    { "Unemployment Rate",          "urate" },
    { "Employment",                 "emp"   },
    { "Industrial Production",      "ip"    },
    { "Retail Sales",               "reta"  },
    { "Consumer Confidence",        "conf"  },
    { "Policy Interest Rate",       "polir" },
    { "10-Year Yield",              "y10yd" },
    { "3-Month Yield",              "m3yd"  },
    { "Government Debt (% GDP)",    "gdebt" },
    { "Current Account (% GDP)",    "ca"    },
    { "Trade Balance",              "tb"    },
    { "Population",                 "pop"   },
    { "Consumer Prices",            "prc"   },
    { "Gross Fixed Capital Fmn",    "gfcf"  },
    { "Exports",                    "exp"   },
    { "Imports",                    "imp"   },
};

// 49 countries available in EconDB (ISO2 codes)
static const QList<QPair<QString,QString>> kEconDbCountries = {
    { "United States",    "US" },
    { "China",            "CN" },
    { "Germany",          "DE" },
    { "Japan",            "JP" },
    { "United Kingdom",   "GB" },
    { "France",           "FR" },
    { "India",            "IN" },
    { "Brazil",           "BR" },
    { "Canada",           "CA" },
    { "South Korea",      "KR" },
    { "Australia",        "AU" },
    { "Russia",           "RU" },
    { "Mexico",           "MX" },
    { "Spain",            "ES" },
    { "Italy",            "IT" },
    { "Netherlands",      "NL" },
    { "Switzerland",      "CH" },
    { "Turkey",           "TR" },
    { "Poland",           "PL" },
    { "Sweden",           "SE" },
    { "Belgium",          "BE" },
    { "Argentina",        "AR" },
    { "Austria",          "AT" },
    { "Bangladesh",       "BD" },
    { "Chile",            "CL" },
    { "Colombia",         "CO" },
    { "Czech Republic",   "CZ" },
    { "Denmark",          "DK" },
    { "Egypt",            "EG" },
    { "Finland",          "FI" },
    { "Greece",           "GR" },
    { "Hong Kong",        "HK" },
    { "Hungary",          "HU" },
    { "Indonesia",        "ID" },
    { "Israel",           "IL" },
    { "Malaysia",         "MY" },
    { "Nigeria",          "NG" },
    { "Norway",           "NO" },
    { "Pakistan",         "PK" },
    { "Peru",             "PE" },
    { "Philippines",      "PH" },
    { "Portugal",         "PT" },
    { "Romania",          "RO" },
    { "Saudi Arabia",     "SA" },
    { "Singapore",        "SG" },
    { "South Africa",     "ZA" },
    { "Taiwan",           "TW" },
    { "Thailand",         "TH" },
    { "Ukraine",          "UA" },
    { "Vietnam",          "VN" },
};

EconDbPanel::EconDbPanel(QWidget* parent)
    : EconPanelBase(kEconDbSourceId, kEconDbColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &EconDbPanel::on_result);
}

void EconDbPanel::activate() {
    show_empty("Select an indicator and country, then click FETCH\n"
               "Source: EconDB — macroeconomic data, no API key required");
}

void EconDbPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    indicator_combo_ = new QComboBox;
    for (const auto& p : kEconDbIndicators)
        indicator_combo_->addItem(p.first, p.second);
    indicator_combo_->setFixedHeight(26);
    indicator_combo_->setMinimumWidth(180);

    country_combo_ = new QComboBox;
    for (const auto& c : kEconDbCountries)
        country_combo_->addItem(c.first, c.second);
    country_combo_->setFixedHeight(26);
    country_combo_->setMinimumWidth(130);

    thl->addWidget(lbl("INDICATOR"));
    thl->addWidget(indicator_combo_);
    thl->addWidget(lbl("COUNTRY"));
    thl->addWidget(country_combo_);
}

void EconDbPanel::on_fetch() {
    const QString indicator = indicator_combo_->currentData().toString();
    const QString country   = country_combo_->currentData().toString();

    show_loading("Fetching EconDB: " + indicator_combo_->currentText()
                 + " — " + country_combo_->currentText() + "…");

    services::EconomicsService::instance().execute(
        kEconDbSourceId, kEconDbScript, "indicator",
        { indicator, country },
        "econdb_" + indicator + "_" + country);
}

void EconDbPanel::on_result(const QString& request_id,
                             const services::EconomicsResult& result) {
    if (result.source_id != kEconDbSourceId) return;
    if (!request_id.startsWith("econdb_")) return;
    if (!result.success) { show_error(result.error); return; }

    // Primary shape: {observations:[{date,value}], metadata:{description,...}}
    QJsonArray obs = result.data["observations"].toArray();

    // Fallback: service may wrap as {data:[...]}
    if (obs.isEmpty())
        obs = result.data["data"].toArray();

    if (obs.isEmpty()) {
        show_error("No observations returned");
        return;
    }

    // Build descriptive title from metadata
    const QJsonObject meta = result.data["metadata"].toObject();
    const QString desc = meta["description"].toString();
    const QString freq = meta["frequency"].toString();
    const QString title = desc.isEmpty()
        ? "EconDB: " + indicator_combo_->currentText()
          + " — " + country_combo_->currentText()
        : "EconDB: " + desc
          + (freq.isEmpty() ? "" : " (" + freq + ")");

    display(obs, title);
    LOG_INFO("EconDbPanel", QString("Displayed %1 observations: %2")
                                .arg(obs.size()).arg(title));
}

} // namespace fincept::screens
