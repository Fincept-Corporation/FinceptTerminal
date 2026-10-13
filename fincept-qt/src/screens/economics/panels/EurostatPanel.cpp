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

#include <algorithm>

namespace fincept::screens {
namespace {

static constexpr const char* kEurostatScript = "eurostat_extra_data.py";
static constexpr const char* kEurostatSourceId = "eurostat";
static constexpr const char* kEurostatColor = "#0369A1"; // EU blue

struct EurostatDataset {
    QString label;
    QString command;
    bool has_country; // whether this command takes a country arg
};

static const QList<EurostatDataset> kEurostatDatasets = {
    {"Industrial Production", "industrial", true},
    {"Retail Trade", "retail", true},
    {"Energy Balance", "energy", true},
    {"Trade in Goods", "trade", true},
    {"Construction Output", "construction", true},
    {"Tourism Statistics", "tourism", true},
};

static const QList<QPair<QString, QString>> kEurostatCountries = {
    {"Germany", "DE"},     {"France", "FR"},      {"Italy", "IT"},          {"Spain", "ES"},   {"Netherlands", "NL"},
    {"Poland", "PL"},      {"Belgium", "BE"},     {"Sweden", "SE"},         {"Austria", "AT"}, {"Denmark", "DK"},
    {"Finland", "FI"},     {"Portugal", "PT"},    {"Czech Republic", "CZ"}, {"Romania", "RO"}, {"Hungary", "HU"},
    {"EU27", "EU27_2020"}, {"Euro Area", "EA20"},
};

} // namespace

// ── SDMX-JSON flattener ───────────────────────────────────────────────────────

QJsonArray EurostatPanel::flatten_sdmx(const QJsonObject& response) {
    // Check for error
    if (response.contains("error"))
        return {};

    const QJsonArray id_arr = response["id"].toArray();
    const QJsonArray size_arr = response["size"].toArray();
    const QJsonObject dims = response["dimension"].toObject();
    const QJsonObject vals = response["value"].toObject();

    if (id_arr.isEmpty() || size_arr.isEmpty() || vals.isEmpty() || id_arr.size() != size_arr.size())
        return {};

    // JSON-stat: the flat value index is row-major over the dimensions in `id` order. Decode every
    // coordinate (not just time) — when a non-time dimension holds several categories (several units
    // or adjustments), each row must say which series it belongs to; the old "index % time_size"
    // decoding interleaved them under identical periods.
    struct Dim {
        QString id;
        int size = 1;
        QStringList labels; // position -> category label
    };
    QVector<Dim> dim_list;
    for (int d = 0; d < id_arr.size(); ++d) {
        Dim dim;
        dim.id = id_arr[d].toString();
        dim.size = std::max(1, size_arr[d].toInt(1));
        const QJsonObject cat = dims[dim.id].toObject()["category"].toObject();
        const QJsonObject index = cat["index"].toObject();
        const QJsonObject label = cat["label"].toObject();
        dim.labels = QStringList(dim.size, QString());
        for (auto it = index.constBegin(); it != index.constEnd(); ++it) {
            const int pos = it.value().toInt(-1);
            if (pos >= 0 && pos < dim.size)
                dim.labels[pos] = dim.id == QLatin1String("time") ? it.key() : label.value(it.key()).toString(it.key());
        }
        dim_list.append(dim);
    }

    QJsonArray rows;
    for (auto it = vals.constBegin(); it != vals.constEnd(); ++it) {
        qint64 flat = it.key().toLongLong();
        QJsonObject row;
        for (int d = static_cast<int>(dim_list.size()) - 1; d >= 0; --d) {
            const Dim& dim = dim_list[d];
            const int pos = static_cast<int>(flat % dim.size);
            flat /= dim.size;
            if (dim.id == QLatin1String("time"))
                row["period"] = dim.labels.value(pos, QString::number(pos));
            else if (dim.size > 1)
                row[dim.id] = dim.labels.value(pos);
        }
        row["value"] = it.value().toDouble();
        rows.append(row);
    }

    // Sort by period string — copy to QList to avoid QJsonValueRef swap issue on MSVC
    QList<QJsonValue> sorted(rows.begin(), rows.end());
    std::sort(sorted.begin(), sorted.end(), [](const QJsonValue& a, const QJsonValue& b) {
        return a.toObject()["period"].toString() < b.toObject()["period"].toString();
    });
    rows = QJsonArray();
    for (const auto& v : sorted)
        rows.append(v);

    return rows;
}

// ── Panel ─────────────────────────────────────────────────────────────────────

EurostatPanel::EurostatPanel(QWidget* parent) : EconPanelBase(kEurostatSourceId, kEurostatColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &EurostatPanel::on_result);
}

void EurostatPanel::activate() {
    show_empty(tr("Select a dataset and country, then click FETCH\n"
                  "Source: Eurostat — EU statistical office"));
}

void EurostatPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
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

    thl->addWidget(dataset_lbl_ = lbl(tr("DATASET")));
    thl->addWidget(dataset_combo_);
    thl->addWidget(country_lbl_ = lbl(tr("COUNTRY")));
    thl->addWidget(country_combo_);
}

void EurostatPanel::on_fetch() {
    const int ds_idx = dataset_combo_->currentIndex();
    const auto& dataset = kEurostatDatasets[ds_idx];
    const QString country = country_combo_->currentData().toString();

    show_loading(tr("Fetching Eurostat: %1 — %2…").arg(dataset.label, country_combo_->currentText()));

    // EconomicsService::execute() prepends `command` to the argv itself. Passing
    // the command again here made the script read the command name as the
    // country code (`industrial industrial DE` → country="industrial",
    // nace="DE"), so the COUNTRY selector never reached Eurostat.
    QStringList args;
    if (dataset.has_country)
        args << country;

    const QString req_id = "eurostat_" + dataset.command + "_" + country;

    services::EconomicsService::instance().execute(kEurostatSourceId, kEurostatScript, dataset.command, args, req_id);
}

void EurostatPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kEurostatSourceId)
        return;
    if (!request_id.startsWith("eurostat_"))
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }

    // Dataset from the request id ("eurostat_<command>_<country>"), not the combo.
    const EurostatDataset* found = nullptr;
    for (const auto& d : kEurostatDatasets)
        if (request_id.startsWith("eurostat_" + d.command + "_"))
            found = &d;
    if (!found)
        return;
    const auto& dataset = *found;

    // Try SDMX flatten first (raw Eurostat format)
    QJsonArray rows = flatten_sdmx(result.data);

    // Fallback: service may have wrapped as {data:[...]}
    if (rows.isEmpty())
        rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        // Check for explicit error in data
        const QString err = result.data["error"].toString();
        show_error(err.isEmpty() ? tr("No data returned for this selection") : err);
        return;
    }

    // Eurostat states what the series is: a single-category unit / s_adj / freq dimension is the
    // series' unit, seasonal adjustment and frequency; `updated` is the dataset's last revision.
    const QJsonObject dims = result.data["dimension"].toObject();
    auto single_label = [&dims](const char* id) -> QString {
        const QJsonObject labels = dims.value(QLatin1String(id)).toObject()["category"].toObject()["label"].toObject();
        return labels.size() == 1 ? labels.begin().value().toString() : QString();
    };
    SeriesMeta meta;
    meta.units = single_label("unit");
    meta.seasonal_adj = single_label("s_adj");
    meta.frequency = single_label("freq");
    meta.last_updated = result.data["updated"].toString().left(10);
    const QString geo = single_label("geo");
    const QString ds_label = result.data["label"].toString();
    set_series_meta(meta);
    const QString title = "Eurostat: " + (ds_label.isEmpty() ? dataset.label : ds_label) + " — " +
                          (geo.isEmpty() ? country_combo_->currentText() : geo);
    display(rows, title);
    LOG_INFO("EurostatPanel", QString("Displayed %1 rows for %2").arg(rows.size()).arg(title));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void EurostatPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void EurostatPanel::retranslateUi() {
    if (dataset_lbl_)
        dataset_lbl_->setText(tr("DATASET"));
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
