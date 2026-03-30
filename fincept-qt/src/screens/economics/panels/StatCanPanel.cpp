// src/screens/economics/panels/StatCanPanel.cpp
// Statistics Canada — GDP, CPI, unemployment, employment, population, housing starts.
// Script: statcan_data.py  |  No API key required.
//
// Response shape: { success, vector_id, product_id, label, count, data:[{date, value}], series }
#include "screens/economics/panels/StatCanPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kStatCanScript   = "statcan_data.py";
static constexpr const char* kStatCanSourceId = "statcan";
static constexpr const char* kStatCanColor    = "#EF4444";  // Canada red

struct StatCanSeries {
    QString label;
    QString command;
    QString description;
};

static const QList<StatCanSeries> kStatCanSeries = {
    { "Real GDP (Chained 2017 $, SA)",  "gdp",          "Table 36-10-0104-01, v65201210, quarterly"  },
    { "CPI All-Items, Canada",          "cpi",          "Table 18-10-0004-01, v41690973, monthly"    },
    { "Unemployment Rate",              "unemployment", "Table 14-10-0287-01, v2062815, monthly"     },
    { "Employment Rate",                "employment",   "Table 14-10-0287-01, v2062817, monthly"     },
    { "Population Estimate (Canada)",   "population",   "Table 17-10-0005-01, v466668, quarterly"    },
    { "Housing Starts (SA, SAAR)",      "housing",      "Table 34-10-0158-01, v52300157, monthly"    },
};

} // namespace

StatCanPanel::StatCanPanel(QWidget* parent)
    : EconPanelBase(kStatCanSourceId, kStatCanColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &StatCanPanel::on_result);
}

void StatCanPanel::activate() {
    show_empty("Select a series and click FETCH\n"
               "Source: Statistics Canada WDS API (www150.statcan.gc.ca)\n"
               "No API key required — data via vector IDs for reliability");
}

void StatCanPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("SERIES");
    lbl->setStyleSheet(
        "color:#525252; font-size:9px; font-weight:700; background:transparent;");

    series_combo_ = new QComboBox;
    for (const auto& s : kStatCanSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(240);

    // Show description as tooltip
    for (int i = 0; i < kStatCanSeries.size(); ++i)
        series_combo_->setItemData(i, kStatCanSeries[i].description, Qt::ToolTipRole);

    thl->addWidget(lbl);
    thl->addWidget(series_combo_);
}

void StatCanPanel::on_fetch() {
    const int   idx    = series_combo_->currentIndex();
    const auto& series = kStatCanSeries[idx];

    show_loading("Fetching StatCan: " + series.label + "…");
    services::EconomicsService::instance().execute(
        kStatCanSourceId, kStatCanScript, series.command, {},
        "statcan_" + series.command);
}

void StatCanPanel::on_result(const QString& request_id,
                              const services::EconomicsResult& result) {
    if (result.source_id != kStatCanSourceId) return;
    if (!request_id.startsWith("statcan_")) return;
    if (!result.success) { show_error(result.error); return; }

    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) { show_error(inline_err); return; }

    // Response: { success, vector_id, label, count, data:[{date, value}], series }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_error("No data returned");
        return;
    }

    // Build title from label or series name
    QString label = result.data["label"].toString().trimmed();
    const QString series_key = result.data["series"].toString();
    if (label.isEmpty()) {
        // Find display label from kStatCanSeries
        for (const auto& s : kStatCanSeries) {
            if (s.command == series_key) { label = s.label; break; }
        }
    }
    const QString title = "StatCan: " + (label.isEmpty() ? request_id.mid(8) : label);

    display(rows, title);
    LOG_INFO("StatCanPanel",
             QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

} // namespace fincept::screens
