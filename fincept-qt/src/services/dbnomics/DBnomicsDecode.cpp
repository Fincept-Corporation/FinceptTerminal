// src/services/dbnomics/DBnomicsDecode.cpp
#include "services/dbnomics/DBnomicsDecode.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>

namespace {

using fincept::services::DbnDataset;
using fincept::services::DbnProvider;
using fincept::services::DbnSeriesInfo;

/// {"code", "name"} rows → (code, name) pairs of type T (providers, datasets, series in the cache).
template <typename T>
QVector<T> dbndec_code_name_rows(const QJsonArray& docs, const char* code_key, const char* name_key) {
    QVector<T> out;
    out.reserve(docs.size());
    for (const QJsonValue& v : docs) {
        const QJsonObject o = v.toObject();
        out.push_back({o.value(QLatin1String(code_key)).toString(), o.value(QLatin1String(name_key)).toString()});
    }
    return out;
}

/// (code, name) rows as the cache keeps them: [{"code", "name"}, …].
template <typename T>
QJsonArray dbndec_code_name_array(const QVector<T>& rows) {
    QJsonArray arr;
    for (const T& r : rows) {
        QJsonObject o;
        o.insert(QStringLiteral("code"), r.code);
        o.insert(QStringLiteral("name"), r.name);
        arr.append(o);
    }
    return arr;
}

QString dbndec_compact(const QJsonDocument& doc) {
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

} // namespace

namespace fincept::services::dbnomics {

// ── /providers ───────────────────────────────────────────────────────────────

QVector<DbnProvider> decode_providers(const QJsonDocument& response) {
    const QJsonArray docs = response.object().value(QLatin1String("providers")).toObject().value(QLatin1String("docs")).toArray();
    return dbndec_code_name_rows<DbnProvider>(docs, "code", "name");
}

QVector<DbnProvider> decode_providers_cache(const QString& cached) {
    return dbndec_code_name_rows<DbnProvider>(QJsonDocument::fromJson(cached.toUtf8()).array(), "code", "name");
}

QString encode_providers_cache(const QVector<DbnProvider>& providers) {
    return dbndec_compact(QJsonDocument(dbndec_code_name_array(providers)));
}

// ── /datasets/<provider> ─────────────────────────────────────────────────────

DbnDatasetPage decode_datasets(const QJsonDocument& response) {
    const QJsonObject ds_root = response.object().value(QLatin1String("datasets")).toObject();
    DbnDatasetPage page;
    page.datasets = dbndec_code_name_rows<DbnDataset>(ds_root.value(QLatin1String("docs")).toArray(), "code", "name");
    page.total = ds_root.value(QLatin1String("num_found")).toInt();
    return page;
}

DbnDatasetPage decode_datasets_cache(const QString& cached) {
    const QJsonObject root = QJsonDocument::fromJson(cached.toUtf8()).object();
    DbnDatasetPage page;
    page.datasets = dbndec_code_name_rows<DbnDataset>(root.value(QLatin1String("docs")).toArray(), "code", "name");
    page.total = root.value(QLatin1String("total")).toInt();
    return page;
}

QString encode_datasets_cache(const DbnDatasetPage& page) {
    QJsonObject root;
    root.insert(QStringLiteral("docs"), dbndec_code_name_array(page.datasets));
    root.insert(QStringLiteral("total"), page.total);
    return dbndec_compact(QJsonDocument(root));
}

// ── /series/<provider>/<dataset> ─────────────────────────────────────────────

DbnSeriesPage decode_series(const QJsonDocument& response) {
    DbnSeriesPage page;
    const QJsonObject root = response.object();
    // A 2xx without a `series` envelope is a failure, not an empty result: num_found would default
    // to 0 and the UI would render a rate-limit or outage as an honest "no series found".
    if (!root.value(QLatin1String("series")).isObject()) {
        page.ok = false;
        return page;
    }
    const QJsonObject s = root.value(QLatin1String("series")).toObject();
    page.series = dbndec_code_name_rows<DbnSeriesInfo>(s.value(QLatin1String("docs")).toArray(), "series_code",
                                                       "series_name");
    page.total = s.value(QLatin1String("num_found")).toInt();
    return page;
}

DbnSeriesPage decode_series_cache(const QString& cached) {
    const QJsonObject root = QJsonDocument::fromJson(cached.toUtf8()).object();
    DbnSeriesPage page;
    page.series = dbndec_code_name_rows<DbnSeriesInfo>(root.value(QLatin1String("docs")).toArray(), "code", "name");
    page.total = root.value(QLatin1String("total")).toInt();
    return page;
}

QString encode_series_cache(const DbnSeriesPage& page) {
    QJsonObject root;
    root.insert(QStringLiteral("docs"), dbndec_code_name_array(page.series));
    root.insert(QStringLiteral("total"), page.total);
    return dbndec_compact(QJsonDocument(root));
}

// ── /search ──────────────────────────────────────────────────────────────────

DbnSearchPage decode_search(const QJsonDocument& response) {
    const QJsonObject res = response.object().value(QLatin1String("results")).toObject();
    const QJsonArray docs = res.value(QLatin1String("docs")).toArray();
    DbnSearchPage page;
    page.total = res.value(QLatin1String("num_found")).toInt();
    page.results.reserve(docs.size());
    for (const QJsonValue& v : docs) {
        const QJsonObject o = v.toObject();
        // /search docs describe a dataset as "code" / "name"; older payloads used "dataset_code" /
        // "dataset_name".
        const QString ds_code = o.value(QLatin1String("code")).toString(o.value(QLatin1String("dataset_code")).toString());
        const QString ds_name = o.value(QLatin1String("name")).toString(o.value(QLatin1String("dataset_name")).toString());
        page.results.push_back({o.value(QLatin1String("provider_code")).toString(),
                                o.value(QLatin1String("provider_name")).toString(), ds_code,
                                ds_name.isEmpty() ? ds_code : ds_name, o.value(QLatin1String("nb_series")).toInt()});
    }
    return page;
}

// ── /series/<provider>/<dataset>/<series>?observations=1 ─────────────────────

DbnObservationsResult decode_observations(const QJsonDocument& response, const QString& series_id,
                                          const QString& series_code) {
    DbnObservationsResult out;
    const QJsonObject root = response.object();
    const QJsonArray docs = root.value(QLatin1String("series")).toObject().value(QLatin1String("docs")).toArray();
    if (docs.isEmpty()) {
        out.error = QStringLiteral("No data returned for series");
        return out;
    }
    const QJsonObject doc = docs.at(0).toObject();
    const QJsonArray periods = doc.value(QLatin1String("period")).toArray();
    const QJsonArray starts = doc.value(QLatin1String("period_start_day")).toArray();
    const QJsonArray values = doc.value(QLatin1String("value")).toArray();

    QVector<DbnObservation> obs;
    const qsizetype n = std::min(periods.size(), values.size());
    obs.reserve(n);
    // DBnomics answers in period order: when every start day is known and they never go back, the
    // sort below would change nothing and is skipped.
    bool sorted = true;
    for (qsizetype i = 0; i < n; ++i) {
        DbnObservation o;
        o.period = periods.at(i).toString();
        o.start = QDate::fromString(starts.at(i).toString(), Qt::ISODate);
        // DBnomics marks gaps with the string "NA"; anything not numeric is a gap, never a 0.
        const QJsonValue v = values.at(i);
        if (v.isDouble()) {
            o.value = v.toDouble();
            o.valid = true;
        } else {
            o.value = 0.0;
            o.valid = false;
        }
        if (!o.start.isValid() || (!obs.isEmpty() && o.start < obs.constLast().start))
            sorted = false;
        obs.push_back(std::move(o));
    }
    // Chronological by period start (period strings of mixed frequency do not sort lexically).
    if (!sorted) {
        std::stable_sort(obs.begin(), obs.end(), [](const DbnObservation& a, const DbnObservation& b) {
            if (a.start.isValid() && b.start.isValid())
                return a.start < b.start;
            return a.period < b.period;
        });
    }

    DbnDataPoint& dp = out.point;
    dp.series_id = series_id;
    dp.series_name = doc.value(QLatin1String("series_name")).toString(series_code);
    dp.observations = std::move(obs);
    dp.frequency = doc.value(QLatin1String("@frequency")).toString();
    dp.indexed_at = doc.value(QLatin1String("indexed_at")).toString();
    dp.provider_name = root.value(QLatin1String("provider")).toObject().value(QLatin1String("name")).toString();
    // Unit / seasonal adjustment: the label of the series' value in a unit-like / adjustment-like
    // dimension (names differ per provider: unit, UNIT_MEASURE, s_adj, seasonal, ADJUSTMENT …).
    const QJsonObject dims = doc.value(QLatin1String("dimensions")).toObject();
    const QJsonObject value_labels = root.value(QLatin1String("dataset"))
                                         .toObject()
                                         .value(QLatin1String("dimensions_values_labels"))
                                         .toObject();
    const auto dim_label = [&dims, &value_labels](const QStringList& names) -> QString {
        for (auto it = dims.constBegin(); it != dims.constEnd(); ++it) {
            if (!names.contains(it.key().toLower()))
                continue;
            const QString code = it.value().toString();
            const QString label = value_labels.value(it.key()).toObject().value(code).toString();
            return label.isEmpty() ? code : label;
        }
        return {};
    };
    dp.unit = dim_label({QStringLiteral("unit"), QStringLiteral("units"), QStringLiteral("unit_measure"),
                         QStringLiteral("unit_of_measure")});
    dp.seasonal = dim_label({QStringLiteral("s_adj"), QStringLiteral("seasonal"), QStringLiteral("seasonal_adjustment"),
                             QStringLiteral("adjustment"), QStringLiteral("sa")});
    // The colour is assigned by the screen (DBnomicsScreen::assign_series_colors()).
    out.ok = true;
    return out;
}

} // namespace fincept::services::dbnomics
