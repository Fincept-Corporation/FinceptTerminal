// src/screens/economics/panels/EurostatPanel.cpp
// Eurostat extra data panel.
//
// Response shape (Eurostat SDMX-JSON v2.0):
//   { version, class, label, source, updated,
//     value: {"0": 78.9, "1": 78.0, ...},   <- flat indexed values
//     id:    ["freq","indic_bt",...,"geo","time"],
//     size:  [1, 1, ..., 1, N],               <- last dim is time
//     dimension: {
//       time: { category: { index: {"1953-01":0, ...}, label: {"1953-01":"1953-01",...} } }
//       ...
//     }
//   }
// Flatten: value index i -> time position = i % time_size -> period string via index map.
#include "screens/economics/panels/EurostatPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMap>

namespace fincept::screens {
namespace {

static constexpr const char* kEurostatScript   = "eurostat_extra_data.py";
static constexpr const char* kEurostatSourceId = "eurostat";
static constexpr const char* kEurostatColor    = "#0369A1";  // EU blue

struct EurostatDataset {
    QString label;
    QString command;
    bool    has_country;  // whether this command takes a country arg
};

static const QList<EurostatDataset> kEurostatDatasets = {
    { "Industrial Production",  "industrial",   true  },
    { "Retail Trade",           "retail",       true  },
    { "Energy Balance",         "energy",       true  },
    { "Trade in Goods",         "trade",        true  },
    { "Construction Output",    "construction", true  },
    { "Tourism Statistics",     "tourism",      true  },
};

static const QList<QPair<QString,QString>> kEurostatCountries = {
    { "Germany",        "DE" },
    { "France",         "FR" },
    { "Italy",          "IT" },
    { "Spain",          "ES" },
    { "Netherlands",    "NL" },
    { "Poland",         "PL" },
    { "Belgium",        "BE" },
    { "Sweden",         "SE" },
    { "Austria",        "AT" },
    { "Denmark",        "DK" },
    { "Finland",        "FI" },
    { "Portugal",       "PT" },
    { "Czech Republic", "CZ" },
    { "Romania",        "RO" },
    { "Hungary",        "HU" },
    { "EU27",           "EU27_2020" },
    { "Euro Area",      "EA20" },
};

} // namespace

// ── SDMX-JSON flattener ───────────────────────────────────────────────────────

QJsonArray EurostatPanel::flatten_sdmx(const QJsonObject& response) {
    // Check for error
    if (response.contains("error"))
        return {};

    const QJsonArray  id_arr  = response["id"].toArray();
    const QJsonArray  size_arr = response["size"].toArray();
    const QJsonObject dims    = response["dimension"].toObject();
    const QJsonObject vals    = response["value"].toObject();

    if (id_arr.isEmpty() || size_arr.isEmpty() || vals.isEmpty())
        return {};

    // Time is always the last dimension
    const int time_size = size_arr.last().toInt(1);

    // Build reverse map: int_position -> period_string from time dimension index
    // dimension.time.category.index = {"1953-01": 0, "1953-02": 1, ...}
    const QJsonObject time_cat   = dims["time"].toObject()["category"].toObject();
    const QJsonObject time_index = time_cat["index"].toObject();

    QMap<int, QString> pos_to_period;
    for (auto it = time_index.constBegin(); it != time_index.constEnd(); ++it)
        pos_to_period[it.value().toInt()] = it.key();

    // Flatten: for each value entry, derive time position and look up period
    QJsonArray rows;
    for (auto it = vals.constBegin(); it != vals.constEnd(); ++it) {
        const int flat_idx  = it.key().toInt();
        const int time_pos  = flat_idx % time_size;
        const QString period = pos_to_period.value(time_pos, QString::number(time_pos));
        QJsonObject row;
        row["period"] = period;
        row["value"]  = it.value().toDouble();
        rows.append(row);
    }

    // Sort by period string — copy to QList to avoid QJsonValueRef swap issue on MSVC
    QList<QJsonValue> sorted(rows.begin(), rows.end());
    std::sort(sorted.begin(), sorted.end(), [](const QJsonValue& a, const QJsonValue& b) {
        return a.toObject()["period"].toString() < b.toObject()["period"].toString();
    });
    rows = QJsonArray();
    for (const auto& v : sorted) rows.append(v);

    return rows;
}

// ── Panel ─────────────────────────────────────────────────────────────────────

EurostatPanel::EurostatPanel(QWidget* parent)
    : EconPanelBase(kEurostatSourceId, kEurostatColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &EurostatPanel::on_result);
}

void EurostatPanel::activate() {
    show_empty("Select a dataset and country, then click FETCH\n"
               "Source: Eurostat — EU statistical office");
}

void EurostatPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    dataset_combo_ = new QComboBox;
    for (const auto& d : kEurostatDatasets)
        dataset_combo_->addItem(d.label, d.command);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(180);

    country_combo_ = new QComboBox;
    for (const auto& c : kEurostatCountries)
        country_combo_->addItem(c.first, c.second);
    country_combo_->setFixedHeight(26);

    thl->addWidget(lbl("DATASET"));
    thl->addWidget(dataset_combo_);
    thl->addWidget(lbl("COUNTRY"));
    thl->addWidget(country_combo_);
}

void EurostatPanel::on_fetch() {
    const int     ds_idx  = dataset_combo_->currentIndex();
    const auto&   dataset = kEurostatDatasets[ds_idx];
    const QString country = country_combo_->currentData().toString();

    show_loading("Fetching Eurostat: " + dataset.label + " — "
                 + country_combo_->currentText() + "…");

    QStringList args = { dataset.command };
    if (dataset.has_country) args << country;

    const QString req_id = "eurostat_" + dataset.command + "_" + country;

    services::EconomicsService::instance().execute(
        kEurostatSourceId, kEurostatScript, dataset.command, args, req_id);
}

void EurostatPanel::on_result(const QString& request_id,
                               const services::EconomicsResult& result) {
    if (result.source_id != kEurostatSourceId) return;
    if (!request_id.startsWith("eurostat_")) return;
    if (!result.success) { show_error(result.error); return; }

    const int ds_idx = dataset_combo_->currentIndex();
    if (ds_idx < 0 || ds_idx >= kEurostatDatasets.size()) return;
    const auto& dataset = kEurostatDatasets[ds_idx];

    // Try SDMX flatten first (raw Eurostat format)
    QJsonArray rows = flatten_sdmx(result.data);

    // Fallback: service may have wrapped as {data:[...]}
    if (rows.isEmpty())
        rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        // Check for explicit error in data
        const QString err = result.data["error"].toString();
        show_error(err.isEmpty() ? "No data returned for this selection" : err);
        return;
    }

    const QString title = "Eurostat: " + dataset.label + " — "
                        + country_combo_->currentText();
    display(rows, title);
    LOG_INFO("EurostatPanel", QString("Displayed %1 rows for %2")
                                  .arg(rows.size()).arg(title));
}

} // namespace fincept::screens
