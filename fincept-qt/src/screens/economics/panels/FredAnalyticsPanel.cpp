// src/screens/economics/panels/FredAnalyticsPanel.cpp
// FRED Analytics panel — composite dataset views built from multiple FRED series.
// Requires FRED_API_KEY (same key as FredPanel).
//
// Response shapes vary per command:
//   yield_curve:  { curve:[{maturity, yield, date}], spread_2_10 }
//   money_supply: { observations:[{date,value}], measure, ... }
//   credit:       { observations:[{date,value}], series_id }
//   stress:       { observations:[{date,value}] }
//   sentiment:    { observations:[{date,value}] }
//   pce:          { observations:[{date,value}] }
#include "screens/economics/panels/FredAnalyticsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {

static constexpr const char* kScript   = "fred_economic_data.py";
static constexpr const char* kSourceId = "fred_analytics";
static constexpr const char* kColor    = "#F97316";  // orange (distinct from FredPanel red)

struct FredAnalDataset {
    QString label;
    QString command;
    QStringList args;
};

static const QList<FredAnalDataset> kDatasets = {
    { "Yield Curve (Treasury Rates)",   "yield_curve",   {}              },
    { "M1 Money Supply",                "money_supply",  {"m1"}          },
    { "M2 Money Supply",                "money_supply",  {"m2"}          },
    { "M3 Money Supply",                "money_supply",  {"m3"}          },
    { "Fed Funds Rate (Credit)",        "credit",        {"fed_funds_rate"}  },
    { "Prime Rate (Credit)",            "credit",        {"prime_rate"}       },
    { "Financial Stress Index",         "stress",        {}              },
    { "Consumer Sentiment",             "sentiment",     {}              },
    { "PCE Price Index",                "pce",           {}              },
};

FredAnalyticsPanel::FredAnalyticsPanel(QWidget* parent)
    : EconPanelBase(kSourceId, kColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &FredAnalyticsPanel::on_result);
}

void FredAnalyticsPanel::activate() {
    show_empty("Set FRED_API_KEY environment variable, then select a dataset and click FETCH\n"
               "Uses the same key as the FRED panel — fred.stlouisfed.org/docs/api/api_key.html");
}

void FredAnalyticsPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("DATASET");
    lbl->setStyleSheet(
        "color:#525252; font-size:9px; font-weight:700; background:transparent;");

    dataset_combo_ = new QComboBox;
    for (const auto& d : kDatasets)
        dataset_combo_->addItem(d.label, d.command);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(240);

    thl->addWidget(lbl);
    thl->addWidget(dataset_combo_);
}

void FredAnalyticsPanel::on_fetch() {
    const int   idx     = dataset_combo_->currentIndex();
    const auto& dataset = kDatasets[idx];

    show_loading("Fetching FRED Analytics: " + dataset.label + "…");

    QStringList args = { dataset.command };
    args << dataset.args;

    const QString arg_suffix = dataset.args.isEmpty() ? "" : "_" + dataset.args.join("_");
    services::EconomicsService::instance().execute(
        kSourceId, kScript, dataset.command, args,
        "fredan_" + dataset.command + arg_suffix);
}

// Extract rows from various FRED Analytics response shapes.
static QJsonArray extract_fred_analytics_rows(const QJsonObject& data) {
    // Standard observations array
    QJsonArray obs = data["observations"].toArray();
    if (!obs.isEmpty()) return obs;

    // Yield curve: {curve:[{maturity, yield, date}]}
    obs = data["curve"].toArray();
    if (!obs.isEmpty()) return obs;

    // Fallback service normalisation
    obs = data["data"].toArray();
    return obs;
}

void FredAnalyticsPanel::on_result(const QString& request_id,
                                    const services::EconomicsResult& result) {
    if (result.source_id != kSourceId) return;
    if (!request_id.startsWith("fredan_")) return;

    if (!result.success) {
        const QString msg = result.error;
        if (msg.contains("FRED_API_KEY") || msg.contains("api_key") || msg.contains("API key")) {
            show_error("FRED API key not configured.\n"
                       "Set FRED_API_KEY environment variable.\n"
                       "Free key at: fred.stlouisfed.org/docs/api/api_key.html");
        } else {
            show_error(msg);
        }
        return;
    }

    // Check inline error
    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) {
        if (inline_err.contains("FRED_API_KEY") || inline_err.contains("not set")) {
            show_error("FRED API key not configured.\n"
                       "Set FRED_API_KEY environment variable.\n"
                       "Free key at: fred.stlouisfed.org/docs/api/api_key.html");
        } else {
            show_error(inline_err);
        }
        return;
    }

    // Filter string sentinel values (FRED uses "." for missing)
    QJsonArray raw = extract_fred_analytics_rows(result.data);
    QJsonArray rows;
    for (const auto& rv : raw) {
        const QJsonObject r = rv.toObject();
        const QString val_str = r["value"].toString();
        if (val_str == "." || (val_str.isEmpty() && !r["value"].isDouble())) continue;
        rows.append(r);
    }

    if (rows.isEmpty()) {
        show_error("No data returned — check FRED_API_KEY is set");
        return;
    }

    const int idx = dataset_combo_->currentIndex();
    const QString title = "FRED Analytics: "
        + (idx >= 0 && idx < kDatasets.size() ? kDatasets[idx].label : request_id.mid(7));

    display(rows, title);
    LOG_INFO("FredAnalyticsPanel", QString("Displayed %1 rows: %2")
                                       .arg(rows.size()).arg(title));
}

} // namespace fincept::screens
