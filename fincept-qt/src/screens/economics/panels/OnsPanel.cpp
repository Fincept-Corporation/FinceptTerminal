// src/screens/economics/panels/OnsPanel.cpp
// UK Office for National Statistics — GDP, CPI, unemployment, trade, housing.
// Script: ons_data.py  |  No API key required.
//
// Response shape: { success, series, label, unit, count, data:[{date, value}] }
#include "screens/economics/panels/OnsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {

static constexpr const char* kScript   = "ons_data.py";
static constexpr const char* kSourceId = "ons";
static constexpr const char* kColor    = "#005EB8";  // ONS blue

struct OnsSeries {
    QString label;
    QString command;
};

static const QList<OnsSeries> kSeries = {
    { "GDP (Chained Volume, SA)",      "gdp"           },
    { "CPI All Items",                 "cpi"           },
    { "CPIH (incl. Housing Costs)",    "cpih"          },
    { "RPI",                           "rpi"           },
    { "Unemployment Rate",             "unemployment"  },
    { "Employment Rate",               "employment"    },
    { "Trade Balance (BoP)",           "trade_balance" },
    { "House Prices (HPI)",            "house_prices"  },
    { "Average Earnings",              "avg_earnings"  },
    { "Public Sector Net Debt",        "public_debt"   },
};

OnsPanel::OnsPanel(QWidget* parent)
    : EconPanelBase(kSourceId, kColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &OnsPanel::on_result);
}

void OnsPanel::activate() {
    show_empty("Select a series and click FETCH\n"
               "Source: UK Office for National Statistics (no API key required)\n"
               "Data via api.beta.ons.gov.uk — GDP, CPI, labour market, housing");
}

void OnsPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("SERIES");
    lbl->setStyleSheet(
        "color:#525252; font-size:9px; font-weight:700; background:transparent;");

    series_combo_ = new QComboBox;
    for (const auto& s : kSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(240);

    thl->addWidget(lbl);
    thl->addWidget(series_combo_);
}

void OnsPanel::on_fetch() {
    const int   idx    = series_combo_->currentIndex();
    const auto& series = kSeries[idx];

    show_loading("Fetching ONS: " + series.label + "…");
    services::EconomicsService::instance().execute(
        kSourceId, kScript, series.command, {},
        "ons_" + series.command);
}

void OnsPanel::on_result(const QString& request_id,
                          const services::EconomicsResult& result) {
    if (result.source_id != kSourceId) return;
    if (!request_id.startsWith("ons_")) return;
    if (!result.success) { show_error(result.error); return; }

    // Check inline error
    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) { show_error(inline_err); return; }

    // Response: { success, series, label, unit, count, data:[{date, value}] }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_error("No data returned");
        return;
    }

    const QString label = result.data["label"].toString();
    const QString unit  = result.data["unit"].toString();
    const QString title = "ONS: " + (label.isEmpty()
        ? (series_combo_->currentIndex() >= 0
               ? kSeries[series_combo_->currentIndex()].label
               : request_id.mid(4))
        : label)
        + (unit.isEmpty() ? "" : " (" + unit + ")");

    display(rows, title);
    LOG_INFO("OnsPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

} // namespace fincept::screens
