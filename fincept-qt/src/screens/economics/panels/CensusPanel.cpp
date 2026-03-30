// src/screens/economics/panels/CensusPanel.cpp
// US Census Bureau data panel.
//
// Response shape: { headers: ["NAME","B01003_001E","state"], data: [{...}, ...], count: N }
// Flatten: each data row is already a dict — pass directly to display().
// Headers are Census variable codes; we rename known ones for readability.
#include "screens/economics/panels/CensusPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kCensusScript   = "census_data.py";
static constexpr const char* kCensusSourceId = "census";
static constexpr const char* kCensusColor    = "#7C3AED";  // purple
} // namespace

struct CensusDataset {
    QString label;
    QString command;
    QString description;
};

static const QList<CensusDataset> kCensusDatasets = {
    { "Population by State (ACS)",      "acs",     "Total population estimate by US state" },
    { "Housing by State (ACS)",         "housing", "Housing units, occupancy & tenure by state" },
};

// Rename Census variable codes to human-readable labels
static const QMap<QString, QString> kVarNames = {
    { "NAME",          "State"           },
    { "B01003_001E",   "Total Population"},
    { "state",         "State FIPS"      },
    { "B25001_001E",   "Total Housing Units"   },
    { "B25002_002E",   "Occupied Units"        },
    { "B25002_003E",   "Vacant Units"          },
    { "B25003_002E",   "Owner Occupied"        },
    { "B25003_003E",   "Renter Occupied"       },
};

QJsonArray CensusPanel::flatten_census(const QJsonObject& response) {
    const QJsonArray headers = response["headers"].toArray();
    const QJsonArray data    = response["data"].toArray();

    if (headers.isEmpty() || data.isEmpty())
        return {};

    // Each data row is already a flat object — rename keys to friendly labels
    QJsonArray rows;
    for (const auto& rv : data) {
        const QJsonObject orig = rv.toObject();
        QJsonObject renamed;
        for (auto it = orig.constBegin(); it != orig.constEnd(); ++it) {
            const QString friendly = kVarNames.value(it.key(), it.key());
            renamed[friendly] = it.value();
        }
        rows.append(renamed);
    }
    return rows;
}

CensusPanel::CensusPanel(QWidget* parent)
    : EconPanelBase(kCensusSourceId, kCensusColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &CensusPanel::on_result);
}

void CensusPanel::activate() {
    show_empty("Select a dataset and click FETCH\n"
               "Source: US Census Bureau — American Community Survey (ACS 5-year)");
}

void CensusPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("DATASET");
    lbl->setStyleSheet(
        "color:#525252; font-size:9px; font-weight:700; background:transparent;");

    dataset_combo_ = new QComboBox;
    for (const auto& d : kCensusDatasets)
        dataset_combo_->addItem(d.label, d.command);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(220);
    dataset_combo_->setToolTip("ACS 5-year estimates, state level");

    thl->addWidget(lbl);
    thl->addWidget(dataset_combo_);
}

void CensusPanel::on_fetch() {
    const int     idx     = dataset_combo_->currentIndex();
    const auto&   dataset = kCensusDatasets[idx];

    show_loading("Fetching Census: " + dataset.label + "…");
    services::EconomicsService::instance().execute(
        kCensusSourceId, kCensusScript, dataset.command, {},
        "census_" + dataset.command);
}

void CensusPanel::on_result(const QString& request_id,
                             const services::EconomicsResult& result) {
    if (result.source_id != kCensusSourceId) return;
    if (!request_id.startsWith("census_")) return;
    if (!result.success) { show_error(result.error); return; }

    // Try Census-specific flatten first
    QJsonArray rows = flatten_census(result.data);

    // Fallback: service normalised to {data:[...]}
    if (rows.isEmpty())
        rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        const QString err = result.data["error"].toString();
        show_error(err.isEmpty() ? "No data returned" : err);
        return;
    }

    const int idx = dataset_combo_->currentIndex();
    const QString title = "Census: " + (idx >= 0 && idx < kCensusDatasets.size()
                                            ? kCensusDatasets[idx].label
                                            : request_id.mid(7));
    display(rows, title);
    LOG_INFO("CensusPanel", QString("Displayed %1 rows for %2").arg(rows.size()).arg(title));
}

} // namespace fincept::screens
