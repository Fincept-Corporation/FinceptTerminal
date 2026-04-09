// src/screens/economics/panels/BcbPanel.cpp
// Brazil Central Bank (Banco Central do Brasil) data panel.
// No API key required.
//
// Response shape: { success:true, series, series_id, name, unit, frequency,
//   data:[{date:"DD/MM/YYYY", <value_key>:float}], count, source }
// Normalise: rename value key to "value", parse date to ISO format.
#include "screens/economics/panels/BcbPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kBcbScript   = "bcb_data.py";
static constexpr const char* kBcbSourceId = "bcb";
static constexpr const char* kBcbColor    = "#16A34A";  // green (Brazil flag)
} // namespace

struct BcbSeries {
    QString label;
    QString command;
    QString value_key;  // field name in each data record
    QString unit;
};

static const QList<BcbSeries> kBcbSeries = {
    { "Selic Target Rate",          "selic",        "selic_target",  "% p.a."    },
    { "IPCA Inflation",             "ipca",         "ipca",          "% monthly" },
    { "GDP Growth Rate",            "gdp",          "gdp_growth",    "% annual"  },
    { "Unemployment Rate",          "unemployment", "unemployment",  "%"         },
    { "Total Credit (BRL M)",       "credit",       "credit_total",  "BRL M"     },
    { "International Reserves",     "reserves",     "reserves",      "USD M"     },
};

// Convert BCB date "DD/MM/YYYY" to ISO "YYYY-MM-DD"
static QString bcb_date_to_iso(const QString& d) {
    // d is "29/03/2021"
    if (d.length() == 10 && d[2] == '/' && d[5] == '/') {
        return d.mid(6, 4) + "-" + d.mid(3, 2) + "-" + d.mid(0, 2);
    }
    return d;
}

BcbPanel::BcbPanel(QWidget* parent)
    : EconPanelBase(kBcbSourceId, kBcbColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &BcbPanel::on_result);
}

void BcbPanel::activate() {
    show_empty("Select a series and click FETCH\n"
               "Source: Banco Central do Brasil — no API key required");
}

void BcbPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("SERIES");
    lbl->setStyleSheet(ctrl_label_style());

    series_combo_ = new QComboBox;
    for (const auto& s : kBcbSeries)
        series_combo_->addItem(s.label + " (" + s.unit + ")", s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(260);

    thl->addWidget(lbl);
    thl->addWidget(series_combo_);
}

void BcbPanel::on_fetch() {
    const int     idx    = series_combo_->currentIndex();
    const auto&   series = kBcbSeries[idx];

    show_loading("Fetching BCB: " + series.label + "…");
    services::EconomicsService::instance().execute(
        kBcbSourceId, kBcbScript, series.command, {},
        "bcb_" + series.command);
}

void BcbPanel::on_result(const QString& request_id,
                          const services::EconomicsResult& result) {
    if (result.source_id != kBcbSourceId) return;
    if (!request_id.startsWith("bcb_")) return;
    if (!result.success) { show_error(result.error); return; }

    // Find series descriptor from command suffix
    const QString cmd = request_id.mid(4);  // strip "bcb_"
    const BcbSeries* series_ptr = nullptr;
    for (const auto& s : kBcbSeries)
        if (s.command == cmd) { series_ptr = &s; break; }

    // Extract data array — may be under "data" key directly or wrapped by service
    QJsonArray raw = result.data["data"].toArray();

    if (raw.isEmpty()) {
        show_error("No data returned for this series");
        return;
    }

    // Normalise: rename value_key -> "value", convert date format
    QJsonArray rows;
    for (const auto& rv : raw) {
        const QJsonObject r = rv.toObject();
        QJsonObject row;
        row["date"] = bcb_date_to_iso(r["date"].toString());
        // value may be under the specific key or already normalised
        if (series_ptr && r.contains(series_ptr->value_key)) {
            row["value"] = r[series_ptr->value_key];
        } else {
            // Pick first numeric field that isn't "date"
            for (auto it = r.constBegin(); it != r.constEnd(); ++it) {
                if (it.key() != "date" && it.value().isDouble()) {
                    row["value"] = it.value();
                    break;
                }
            }
        }
        rows.append(row);
    }

    const int idx = series_combo_->currentIndex();
    const QString unit = (idx >= 0 && idx < kBcbSeries.size())
                             ? " (" + kBcbSeries[idx].unit + ")" : "";
    const QString title = "BCB: " + (series_ptr ? series_ptr->label : cmd) + unit;

    display(rows, title);
    LOG_INFO("BcbPanel", QString("Displayed %1 records: %2").arg(rows.size()).arg(title));
}

} // namespace fincept::screens
