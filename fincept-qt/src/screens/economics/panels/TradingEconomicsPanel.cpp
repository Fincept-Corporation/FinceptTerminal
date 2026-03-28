// src/screens/economics/panels/TradingEconomicsPanel.cpp
// Trading Economics panel. Requires TRADING_ECONOMICS_API_KEY env var.
//
// Working commands (with key): ratings, ratings_by_agency, bonds,
//   us_treasuries, european_bonds, yield_curve, country_data, calendar
// Response shape varies per command — normalised via extract_te_rows().
#include "screens/economics/panels/TradingEconomicsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {

static constexpr const char* kScript   = "trading_economics_data.py";
static constexpr const char* kSourceId = "trading_economics";
static constexpr const char* kColor    = "#F59E0B";  // amber

struct TeDataset {
    QString label;
    QString command;
    QString country_arg;  // "required", "optional", or ""
};

static const QList<TeDataset> kDatasets = {
    { "Credit Ratings (All)",           "ratings",           ""         },
    { "Credit Ratings by Agency",       "ratings_by_agency", ""         },
    { "US Treasuries",                  "us_treasuries",     ""         },
    { "European Bonds",                 "european_bonds",    ""         },
    { "Bond Yield Curve",               "yield_curve",       "required" },
    { "Country Economic Data",          "country_data",      "required" },
    { "Economic Calendar",              "calendar",          ""         },
};

static const QList<QPair<QString,QString>> kCountries = {
    { "United States", "US" }, { "Germany",        "DE" }, { "Japan",          "JP" },
    { "United Kingdom","GB" }, { "France",         "FR" }, { "Italy",          "IT" },
    { "China",         "CN" }, { "Brazil",         "BR" }, { "India",          "IN" },
    { "Australia",     "AU" }, { "Canada",         "CA" }, { "Spain",          "ES" },
};

TradingEconomicsPanel::TradingEconomicsPanel(QWidget* parent)
    : EconPanelBase(kSourceId, kColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &TradingEconomicsPanel::on_result);
}

void TradingEconomicsPanel::activate() {
    show_empty("Set TRADING_ECONOMICS_API_KEY environment variable, then select a dataset and click FETCH\n"
               "Get a key at: tradingeconomics.com/api");
}

void TradingEconomicsPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    dataset_combo_ = new QComboBox;
    for (const auto& d : kDatasets)
        dataset_combo_->addItem(d.label, d.command);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(200);

    country_combo_ = new QComboBox;
    for (const auto& c : kCountries)
        country_combo_->addItem(c.first, c.second);
    country_combo_->setFixedHeight(26);
    country_combo_->setMinimumWidth(130);
    country_combo_->setToolTip("Required for yield_curve and country_data");

    thl->addWidget(lbl("DATASET"));
    thl->addWidget(dataset_combo_);
    thl->addWidget(lbl("COUNTRY"));
    thl->addWidget(country_combo_);
}

void TradingEconomicsPanel::on_fetch() {
    const int   idx     = dataset_combo_->currentIndex();
    const auto& dataset = kDatasets[idx];
    const QString country = country_combo_->currentData().toString();

    show_loading("Fetching Trading Economics: " + dataset.label + "…");

    QStringList args = { dataset.command };
    if (dataset.country_arg == "required" || dataset.country_arg == "optional")
        args << country;

    services::EconomicsService::instance().execute(
        kSourceId, kScript, dataset.command, args,
        "te_" + dataset.command + (args.size() > 1 ? "_" + country : ""));
}

// Normalise various Trading Economics response shapes into displayable rows.
static QJsonArray extract_te_rows(const QJsonObject& data) {
    // Shape 1: direct array in "data" key
    QJsonArray arr = data["data"].toArray();
    if (!arr.isEmpty()) return arr;

    // Shape 2: ratings array
    arr = data["ratings"].toArray();
    if (!arr.isEmpty()) return arr;

    // Shape 3: bonds / treasuries / yield_curve
    arr = data["bonds"].toArray();
    if (!arr.isEmpty()) return arr;

    arr = data["treasuries"].toArray();
    if (!arr.isEmpty()) return arr;

    arr = data["yield_curve"].toArray();
    if (!arr.isEmpty()) return arr;

    // Shape 4: events / calendar
    arr = data["events"].toArray();
    if (!arr.isEmpty()) return arr;

    // Shape 5: single object with country data fields — wrap in array
    if (data.contains("country") || data.contains("gdp") || data.contains("inflation")) {
        return QJsonArray{ data };
    }

    return {};
}

void TradingEconomicsPanel::on_result(const QString& request_id,
                                       const services::EconomicsResult& result) {
    if (result.source_id != kSourceId) return;
    if (!request_id.startsWith("te_")) return;

    if (!result.success) {
        const QString msg = result.error;
        if (msg.contains("API key") || msg.contains("TRADING_ECONOMICS_API_KEY")) {
            show_error("Trading Economics API key not configured.\n"
                       "Set TRADING_ECONOMICS_API_KEY environment variable.\n"
                       "Get a key at: tradingeconomics.com/api");
        } else {
            show_error(msg);
        }
        return;
    }

    // Also check for inline error field
    if (result.data.contains("error") && !result.data["error"].toBool(false)) {
        const QString err = result.data["error"].toString();
        if (!err.isEmpty() && err != "false") {
            if (err.contains("API key") || err.contains("TRADING_ECONOMICS")) {
                show_error("Trading Economics API key not configured.\n"
                           "Set TRADING_ECONOMICS_API_KEY environment variable.\n"
                           "Get a key at: tradingeconomics.com/api");
            } else {
                show_error(err);
            }
            return;
        }
    }

    const QJsonArray rows = extract_te_rows(result.data);
    if (rows.isEmpty()) {
        show_error("No data returned — API key may be required");
        return;
    }

    const int idx = dataset_combo_->currentIndex();
    const QString title = "Trading Economics: "
        + (idx >= 0 && idx < kDatasets.size() ? kDatasets[idx].label : request_id.mid(3));
    display(rows, title);
    LOG_INFO("TradingEconomicsPanel", QString("Displayed %1 rows: %2")
                                          .arg(rows.size()).arg(title));
}

} // namespace fincept::screens
