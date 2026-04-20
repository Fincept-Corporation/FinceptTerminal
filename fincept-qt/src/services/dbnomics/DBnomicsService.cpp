// src/services/dbnomics/DBnomicsService.cpp
#include "services/dbnomics/DBnomicsService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

namespace fincept::services {

DBnomicsService& DBnomicsService::instance() {
    static DBnomicsService inst;
    return inst;
}

DBnomicsService::DBnomicsService(QObject* parent) : QObject(parent) {
    series_debounce_ = new QTimer(this);
    series_debounce_->setSingleShot(true);
    series_debounce_->setInterval(kDebounceMs);
    connect(series_debounce_, &QTimer::timeout, this,
            [this]() { fetch_series(pending_series_prov_, pending_series_ds_, pending_series_q_, 0); });

    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    search_debounce_->setInterval(kDebounceMs);
    connect(search_debounce_, &QTimer::timeout, this, [this]() { global_search(pending_search_q_, 0); });
}

QString DBnomicsService::build_url(const QString& path) const {
    return QString("%1%2").arg(kBaseUrl).arg(path);
}

void DBnomicsService::fetch_providers() {
    {
        const QVariant cached = fincept::CacheManager::instance().get("dbnomics:providers");
        if (!cached.isNull()) {
            LOG_DEBUG("DBnomicsService", "providers: serving from cache");
            const QJsonArray docs = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
            QVector<DbnProvider> out;
            out.reserve(docs.size());
            for (const auto& v : docs) {
                const QJsonObject o = v.toObject();
                out.push_back({o["code"].toString(), o["name"].toString()});
            }
            emit providers_loaded(out);
            return;
        }
    }
    LOG_INFO("DBnomicsService", "Fetching providers from API");
    const QString url = build_url("/providers");
    fincept::HttpClient::instance().get(url, [this](fincept::Result<QJsonDocument> result) {
        if (result.is_err()) {
            LOG_ERROR("DBnomicsService",
                      QString("providers fetch failed: %1").arg(QString::fromStdString(result.error())));
            emit error_occurred("providers", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root = result.value().object();
        QJsonArray docs = root["providers"].toObject()["docs"].toArray();
        QVector<DbnProvider> out;
        out.reserve(docs.size());
        for (const auto& v : docs) {
            QJsonObject o = v.toObject();
            out.push_back({o["code"].toString(), o["name"].toString()});
        }
        QJsonArray arr;
        for (const auto& p : out) {
            QJsonObject o;
            o["code"] = p.code;
            o["name"] = p.name;
            arr.append(o);
        }
        fincept::CacheManager::instance().put(
            "dbnomics:providers", QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
            kProviderCacheSec, "dbnomics");
        LOG_INFO("DBnomicsService", QString("Loaded %1 providers").arg(out.size()));
        emit providers_loaded(out);
    });
}

void DBnomicsService::fetch_datasets(const QString& provider_code, int offset) {
    const bool first_page = (offset == 0);
    if (first_page) {
        const QString ds_key = "dbnomics:datasets:" + provider_code;
        const QVariant cached = fincept::CacheManager::instance().get(ds_key);
        if (!cached.isNull()) {
            LOG_DEBUG("DBnomicsService", "datasets: serving from cache");
            const QJsonObject root = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
            const QJsonArray docs = root["docs"].toArray();
            const int total = root["total"].toInt();
            QVector<DbnDataset> out;
            out.reserve(docs.size());
            for (const auto& v : docs) {
                const QJsonObject o = v.toObject();
                out.push_back({o["code"].toString(), o["name"].toString()});
            }
            DbnPagination page{0, 50, total};
            emit datasets_loaded(out, page, false);
            return;
        }
    }
    const QString url = build_url(QString("/datasets/%1?limit=50&offset=%2").arg(provider_code).arg(offset));
    LOG_INFO("DBnomicsService", QString("Fetching datasets for %1 offset=%2").arg(provider_code).arg(offset));
    fincept::HttpClient::instance().get(url, [this, provider_code, offset](fincept::Result<QJsonDocument> result) {
        if (result.is_err()) {
            LOG_ERROR("DBnomicsService",
                      QString("datasets fetch failed: %1").arg(QString::fromStdString(result.error())));
            emit error_occurred("datasets", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root = result.value().object();
        QJsonObject ds_root = root["datasets"].toObject();
        QJsonArray docs = ds_root["docs"].toArray();
        int total = ds_root["num_found"].toInt();

        QVector<DbnDataset> out;
        out.reserve(docs.size());
        for (const auto& v : docs) {
            QJsonObject o = v.toObject();
            out.push_back({o["code"].toString(), o["name"].toString()});
        }
        DbnPagination page{offset, 50, total};
        const bool append = (offset > 0);

        if (!append) {
            QJsonArray arr;
            for (const auto& d : out) {
                QJsonObject o;
                o["code"] = d.code;
                o["name"] = d.name;
                arr.append(o);
            }
            QJsonObject cache_root;
            cache_root["docs"] = arr;
            cache_root["total"] = total;
            fincept::CacheManager::instance().put(
                "dbnomics:datasets:" + provider_code,
                QVariant(QString::fromUtf8(QJsonDocument(cache_root).toJson(QJsonDocument::Compact))), kDatasetCacheSec,
                "dbnomics");
        }
        LOG_INFO("DBnomicsService", QString("Loaded %1 datasets (total=%2)").arg(out.size()).arg(total));
        emit datasets_loaded(out, page, append);
    });
}

void DBnomicsService::fetch_series(const QString& provider_code, const QString& dataset_code, const QString& query,
                                   int offset) {
    const QString cache_key = QString("%1/%2").arg(provider_code).arg(dataset_code);
    const bool first_page = (offset == 0);
    const bool no_query = query.isEmpty();

    if (first_page && no_query) {
        const QString series_ckey = "dbnomics:series:" + cache_key;
        const QVariant cv = fincept::CacheManager::instance().get(series_ckey);
        if (!cv.isNull()) {
            LOG_DEBUG("DBnomicsService", "series: serving from cache");
            const QJsonObject root = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
            const QJsonArray docs = root["docs"].toArray();
            const int total = root["total"].toInt();
            QVector<DbnSeriesInfo> out;
            out.reserve(docs.size());
            for (const auto& v : docs) {
                const QJsonObject o = v.toObject();
                out.push_back({o["code"].toString(), o["name"].toString()});
            }
            DbnPagination page{0, 50, total};
            emit series_loaded(out, page, false);
            return;
        }
    }
    QString path =
        QString("/series/%1/%2?limit=50&offset=%3&observations=false").arg(provider_code).arg(dataset_code).arg(offset);
    if (!query.isEmpty())
        path += "&q=" + QString::fromUtf8(QUrl::toPercentEncoding(query));

    LOG_INFO("DBnomicsService", QString("Fetching series %1 q='%2' offset=%3").arg(cache_key).arg(query).arg(offset));

    fincept::HttpClient::instance().get(
        build_url(path), [this, cache_key, offset, no_query, first_page](fincept::Result<QJsonDocument> result) {
            if (result.is_err()) {
                LOG_ERROR("DBnomicsService",
                          QString("series fetch failed: %1").arg(QString::fromStdString(result.error())));
                emit error_occurred("series", QString::fromStdString(result.error()));
                return;
            }
            QJsonObject root = result.value().object();
            QJsonObject s = root["series"].toObject();
            QJsonArray docs = s["docs"].toArray();
            int total = s["num_found"].toInt();

            QVector<DbnSeriesInfo> out;
            out.reserve(docs.size());
            for (const auto& v : docs) {
                QJsonObject o = v.toObject();
                out.push_back({o["series_code"].toString(), o["series_name"].toString()});
            }
            DbnPagination page{offset, 50, total};
            const bool append = (offset > 0);

            if (first_page && no_query) {
                QJsonArray arr;
                for (const auto& si : out) {
                    QJsonObject o;
                    o["code"] = si.code;
                    o["name"] = si.name;
                    arr.append(o);
                }
                QJsonObject cache_root;
                cache_root["docs"] = arr;
                cache_root["total"] = total;
                fincept::CacheManager::instance().put(
                    "dbnomics:series:" + cache_key,
                    QVariant(QString::fromUtf8(QJsonDocument(cache_root).toJson(QJsonDocument::Compact))), kSeriesCacheSec,
                    "dbnomics");
            }
            LOG_INFO("DBnomicsService", QString("Loaded %1 series (total=%2)").arg(out.size()).arg(total));
            emit series_loaded(out, page, append);
        });
}

void DBnomicsService::fetch_observations(const QString& provider_code, const QString& dataset_code,
                                         const QString& series_code) {
    const QString path =
        QString("/series/%1/%2/%3?observations=1&format=json").arg(provider_code).arg(dataset_code).arg(series_code);
    const QString full_id = QString("%1/%2/%3").arg(provider_code).arg(dataset_code).arg(series_code);
    LOG_INFO("DBnomicsService", QString("Fetching observations for %1").arg(full_id));

    fincept::HttpClient::instance().get(build_url(path), [this, full_id, provider_code, dataset_code,
                                                          series_code](fincept::Result<QJsonDocument> result) {
        if (result.is_err()) {
            emit error_occurred("observations", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root = result.value().object();
        QJsonArray docs = root["series"].toObject()["docs"].toArray();
        if (docs.isEmpty()) {
            emit error_occurred("observations", "No data returned for series");
            return;
        }
        QJsonObject doc = docs[0].toObject();
        QJsonArray periods = doc["period"].toArray();
        QJsonArray values = doc["value"].toArray();
        QString name = doc["series_name"].toString(series_code);

        QVector<DbnObservation> obs;
        obs.reserve(periods.size());
        for (int i = 0; i < periods.size() && i < values.size(); ++i) {
            DbnObservation o;
            o.period = periods[i].toString();
            if (values[i].isNull() || values[i].toString() == "NA") {
                o.value = 0.0;
                o.valid = false;
            } else {
                o.value = values[i].toDouble();
                o.valid = true;
            }
            obs.push_back(o);
        }
        DbnDataPoint dp;
        dp.series_id = full_id;
        dp.series_name = name;
        dp.observations = std::move(obs);
        // color will be assigned by DBnomicsScreen::assign_series_colors()
        LOG_INFO("DBnomicsService", QString("Loaded %1 observations for %2").arg(dp.observations.size()).arg(full_id));
        emit observations_loaded(dp);
        if (hub_registered_) {
            const QString topic = hub_topic(provider_code, dataset_code, series_code);
            fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(dp));
        }
    });
}

void DBnomicsService::global_search(const QString& query, int offset) {
    if (query.trimmed().isEmpty())
        return;
    const QString path = QString("/search?q=%1&limit=20&offset=%2")
                             .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed())))
                             .arg(offset);
    LOG_INFO("DBnomicsService", QString("Searching: '%1' offset=%2").arg(query).arg(offset));

    fincept::HttpClient::instance().get(build_url(path), [this, offset](fincept::Result<QJsonDocument> result) {
        if (result.is_err()) {
            emit error_occurred("search", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root = result.value().object();
        QJsonObject res = root["results"].toObject();
        QJsonArray docs = res["docs"].toArray();
        int total = res["num_found"].toInt();

        QVector<DbnSearchResult> out;
        out.reserve(docs.size());
        for (const auto& v : docs) {
            QJsonObject o = v.toObject();
            out.push_back({o["provider_code"].toString(), o["provider_name"].toString(), o["dataset_code"].toString(),
                           o["dataset_name"].toString(), o["nb_series"].toInt()});
        }
        DbnPagination page{offset, 20, total};
        emit search_results_loaded(out, page, offset > 0);
    });
}

void DBnomicsService::schedule_series_search(const QString& prov, const QString& ds, const QString& query) {
    pending_series_prov_ = prov;
    pending_series_ds_ = ds;
    pending_series_q_ = query;
    series_debounce_->start();
}

void DBnomicsService::schedule_global_search(const QString& query) {
    pending_search_q_ = query;
    search_debounce_->start();
}

QColor DBnomicsService::chart_color(int index) {
    static const QColor palette[] = {
        QColor("#ea580c"), QColor("#3b82f6"), QColor("#22c55e"), QColor("#eab308"),
        QColor("#a855f7"), QColor("#ec4899"), QColor("#06b6d4"), QColor("#f97316"),
    };
    static const int n = static_cast<int>(std::size(palette));
    return palette[index % n];
}

// ── DataHub producer wiring ─────────────────────────────────────────────────

QString DBnomicsService::hub_topic(const QString& provider, const QString& dataset, const QString& series) {
    return QStringLiteral("dbnomics:") + provider + QLatin1Char(':') + dataset +
           QLatin1Char(':') + series;
}

QStringList DBnomicsService::topic_patterns() const {
    return {QStringLiteral("dbnomics:*")};
}

void DBnomicsService::refresh(const QStringList& topics) {
    // Split `dbnomics:<provider>:<dataset>:<series>` and dispatch to the
    // existing observation fetcher. Anything that doesn't match the
    // 4-segment shape is silently skipped.
    for (const auto& topic : topics) {
        const QStringList parts = topic.split(QLatin1Char(':'));
        if (parts.size() != 4) continue;
        fetch_observations(parts[1], parts[2], parts[3]);
    }
}

int DBnomicsService::max_requests_per_sec() const {
    return 3;  // DBnomics REST — conservative default
}

void DBnomicsService::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // 1-hour TTL, 60s min_interval — DBnomics series are daily-or-slower.
    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 60 * 60 * 1000;
    policy.min_interval_ms = 60 * 1000;
    policy.push_only = false;
    hub.set_policy_pattern(QStringLiteral("dbnomics:*"), policy);

    hub_registered_ = true;
    LOG_INFO("DBnomicsService", "Registered with DataHub (dbnomics:*)");
}

} // namespace fincept::services
