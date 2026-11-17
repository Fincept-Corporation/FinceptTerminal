// src/services/dbnomics/DBnomicsService.cpp
#include "services/dbnomics/DBnomicsService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "network/http/HttpClient.h"
#include "services/dbnomics/DBnomicsDecode.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"

#include <QFuture>
#include <QJsonArray>
#include <QJsonObject>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <optional>

namespace {

/// The decode thread: the cached catalogue lists are read and decoded, fresh answers converted (and
/// written to the cache), observations turned into a plottable series — never on the GUI thread.
/// One thread, so answers are handed back (and the signals emitted) in the order they arrived. A
/// child of the process-lifetime service: its destructor waits for a decode still in flight.
QThreadPool* dbn_svc_pool() {
    static QThreadPool* const pool = [] {
        // The decode thread reads and writes the cache (CacheDatabase gives each thread its own
        // connection). The cache singleton is made here, on the GUI thread, never first on a worker
        // (a QObject belongs to the thread that creates it).
        (void)fincept::CacheManager::instance();
        auto* p = new QThreadPool(&fincept::services::DBnomicsService::instance());
        p->setObjectName(QStringLiteral("DBnomicsDecode"));
        p->setMaxThreadCount(1);
        return p;
    }();
    return pool;
}

/// A REST answer after the decode thread: the converted value, or the transport / HTTP error.
template <typename T>
struct DbnSvcAnswer {
    bool ok = false;
    QString error;
    T value{};
};

/// A REST answer as it leaves the GUI thread: the 2xx body exactly as received (HttpClient::get_raw —
/// a dataset list reaches 3 MB, so it is parsed once, on the decode thread, never on the GUI thread),
/// or the transport / HTTP error.
struct DbnSvcReply {
    bool failed = false;
    QString error;
    QByteArray body;
};

DbnSvcReply dbn_svc_reply(const fincept::Result<QByteArray>& result) {
    DbnSvcReply r;
    r.failed = result.is_err();
    if (r.failed)
        r.error = QString::fromStdString(result.error());
    else
        r.body = result.value();
    return r;
}

/// On the decode thread: the body as JSON — false with `error` set for a failed request or a body
/// that is not JSON (the same text HttpClient::get() would have reported).
bool dbn_svc_json(const DbnSvcReply& reply, QJsonDocument& doc, QString& error) {
    if (reply.failed) {
        error = reply.error;
        return false;
    }
    const fincept::Result<QJsonDocument> parsed = fincept::HttpClient::parse_body(reply.body);
    if (parsed.is_err()) {
        error = QString::fromStdString(parsed.error());
        return false;
    }
    doc = parsed.value();
    return true;
}

} // namespace

namespace fincept::services {

namespace dbn = fincept::services::dbnomics;

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
    const QString url = build_url("/providers");
    // The cached list is read and decoded on the decode thread; a miss starts the network fetch.
    (void)QtConcurrent::run(dbn_svc_pool(),
                            []() -> std::optional<QVector<DbnProvider>> {
                                const QVariant cached =
                                    fincept::CacheManager::instance().get(QStringLiteral("dbnomics:providers"));
                                if (cached.isNull())
                                    return std::nullopt;
                                return dbn::decode_providers_cache(cached.toString());
                            })
        .then(this, [this, url](std::optional<QVector<DbnProvider>> cached) {
            if (cached) {
                LOG_DEBUG("DBnomicsService", "providers: serving from cache");
                emit providers_loaded(*cached);
                return;
            }
            LOG_DEBUG("DBnomicsService", "Fetching providers from API");
            fincept::HttpClient::instance().get_raw(
                url,
                [this](fincept::Result<QByteArray> result) {
                    (void)QtConcurrent::run(dbn_svc_pool(),
                                            [reply = dbn_svc_reply(result)]() {
                                                DbnSvcAnswer<QVector<DbnProvider>> a;
                                                QJsonDocument doc;
                                                if (!dbn_svc_json(reply, doc, a.error))
                                                    return a;
                                                a.value = dbn::decode_providers(doc);
                                                fincept::CacheManager::instance().put(
                                                    QStringLiteral("dbnomics:providers"),
                                                    QVariant(dbn::encode_providers_cache(a.value)), kProviderCacheSec,
                                                    QStringLiteral("dbnomics"));
                                                a.ok = true;
                                                return a;
                                            })
                        .then(this, [this](DbnSvcAnswer<QVector<DbnProvider>> a) {
                            if (!a.ok) {
                                LOG_ERROR("DBnomicsService", QString("providers fetch failed: %1").arg(a.error));
                                emit error_occurred("providers", a.error);
                                return;
                            }
                            LOG_DEBUG("DBnomicsService", QString("Loaded %1 providers").arg(a.value.size()));
                            emit providers_loaded(a.value);
                        });
                },
                this);
        });
}

void DBnomicsService::fetch_datasets(const QString& provider_code, int offset) {
    const bool first_page = (offset == 0);
    const QString ds_key = "dbnomics:datasets:" + provider_code;
    const QString url = build_url(QString("/datasets/%1?limit=50&offset=%2").arg(provider_code).arg(offset));
    // The first page may be cached: read and decoded on the decode thread; a miss (or a later page)
    // starts the network fetch.
    (void)QtConcurrent::run(dbn_svc_pool(),
                            [first_page, ds_key]() -> std::optional<dbn::DbnDatasetPage> {
                                if (!first_page)
                                    return std::nullopt;
                                const QVariant cached = fincept::CacheManager::instance().get(ds_key);
                                if (cached.isNull())
                                    return std::nullopt;
                                return dbn::decode_datasets_cache(cached.toString());
                            })
        .then(this, [this, provider_code, offset, ds_key, url](std::optional<dbn::DbnDatasetPage> cached) {
            if (cached) {
                LOG_DEBUG("DBnomicsService", "datasets: serving from cache");
                emit datasets_loaded(provider_code, cached->datasets, DbnPagination{0, 50, cached->total}, false);
                return;
            }
            LOG_DEBUG("DBnomicsService", QString("Fetching datasets for %1 offset=%2").arg(provider_code).arg(offset));
            fincept::HttpClient::instance().get_raw(
                url,
                [this, provider_code, offset, ds_key](fincept::Result<QByteArray> result) {
                    const bool append = (offset > 0);
                    (void)QtConcurrent::run(dbn_svc_pool(),
                                            [reply = dbn_svc_reply(result), append, ds_key]() {
                                                DbnSvcAnswer<dbn::DbnDatasetPage> a;
                                                QJsonDocument doc;
                                                if (!dbn_svc_json(reply, doc, a.error))
                                                    return a;
                                                a.value = dbn::decode_datasets(doc);
                                                if (!append)
                                                    fincept::CacheManager::instance().put(
                                                        ds_key, QVariant(dbn::encode_datasets_cache(a.value)), kDatasetCacheSec,
                                                        QStringLiteral("dbnomics"));
                                                a.ok = true;
                                                return a;
                                            })
                        .then(this, [this, provider_code, offset, append](DbnSvcAnswer<dbn::DbnDatasetPage> a) {
                            if (!a.ok) {
                                LOG_ERROR("DBnomicsService", QString("datasets fetch failed: %1").arg(a.error));
                                emit error_occurred("datasets", a.error);
                                return;
                            }
                            LOG_DEBUG("DBnomicsService", QString("Loaded %1 datasets (total=%2)")
                                                             .arg(a.value.datasets.size())
                                                             .arg(a.value.total));
                            emit datasets_loaded(provider_code, a.value.datasets,
                                                 DbnPagination{offset, 50, a.value.total}, append);
                        });
                },
                this);
        });
}

void DBnomicsService::fetch_series(const QString& provider_code, const QString& dataset_code, const QString& query,
                                   int offset) {
    const QString cache_key = QString("%1/%2").arg(provider_code).arg(dataset_code);
    const QString series_ckey = "dbnomics:series:" + cache_key;
    const bool first_page = (offset == 0);
    const bool no_query = query.isEmpty();
    const bool cacheable = first_page && no_query;

    QString path =
        QString("/series/%1/%2?limit=50&offset=%3&observations=false").arg(provider_code).arg(dataset_code).arg(offset);
    if (!query.isEmpty())
        path += "&q=" + QString::fromUtf8(QUrl::toPercentEncoding(query));
    const QString url = build_url(path);

    // The unfiltered first page may be cached: read and decoded on the decode thread; a miss (a
    // search, a later page) starts the network fetch.
    (void)QtConcurrent::run(dbn_svc_pool(),
                            [cacheable, series_ckey]() -> std::optional<dbn::DbnSeriesPage> {
                                if (!cacheable)
                                    return std::nullopt;
                                const QVariant cv = fincept::CacheManager::instance().get(series_ckey);
                                if (cv.isNull())
                                    return std::nullopt;
                                return dbn::decode_series_cache(cv.toString());
                            })
        .then(this, [this, provider_code, dataset_code, query, offset, cache_key, series_ckey, cacheable, url](std::optional<dbn::DbnSeriesPage> cached) {
            if (cached) {
                LOG_DEBUG("DBnomicsService", "series: serving from cache");
                emit series_loaded(provider_code, dataset_code, query, cached->series, DbnPagination{0, 50, cached->total},
                                   false);
                return;
            }
            LOG_DEBUG("DBnomicsService",
                      QString("Fetching series %1 q='%2' offset=%3").arg(cache_key).arg(query).arg(offset));
            fincept::HttpClient::instance().get_raw(
                url,
                [this, provider_code, dataset_code, query, offset, cache_key, series_ckey, cacheable](fincept::Result<QByteArray> result) {
                    (void)QtConcurrent::run(dbn_svc_pool(),
                                            [reply = dbn_svc_reply(result), cacheable, series_ckey]() {
                                                DbnSvcAnswer<dbn::DbnSeriesPage> a;
                                                QJsonDocument doc;
                                                if (!dbn_svc_json(reply, doc, a.error))
                                                    return a;
                                                a.value = dbn::decode_series(doc);
                                                if (a.value.ok && cacheable)
                                                    fincept::CacheManager::instance().put(
                                                        series_ckey, QVariant(dbn::encode_series_cache(a.value)), kSeriesCacheSec,
                                                        QStringLiteral("dbnomics"));
                                                a.ok = true;
                                                return a;
                                            })
                        .then(this, [this, provider_code, dataset_code, query, offset,
                                     cache_key](DbnSvcAnswer<dbn::DbnSeriesPage> a) {
                            if (!a.ok) {
                                LOG_ERROR("DBnomicsService", QString("series fetch failed: %1").arg(a.error));
                                emit error_occurred("series", a.error);
                                return;
                            }
                            if (!a.value.ok) {
                                LOG_ERROR("DBnomicsService",
                                          QString("series fetch: malformed response for %1").arg(cache_key));
                                emit error_occurred("series", QStringLiteral("Malformed response from DBnomics"));
                                return;
                            }
                            LOG_DEBUG("DBnomicsService", QString("Loaded %1 series (total=%2)")
                                                             .arg(a.value.series.size())
                                                             .arg(a.value.total));
                            emit series_loaded(provider_code, dataset_code, query, a.value.series,
                                               DbnPagination{offset, 50, a.value.total}, offset > 0);
                        });
                },
                this);
        });
}

void DBnomicsService::fetch_observations(const QString& provider_code, const QString& dataset_code,
                                         const QString& series_code) {
    // Series codes may carry characters that are not legal in a URL path segment.
    const auto seg = [](const QString& s) { return QString::fromUtf8(QUrl::toPercentEncoding(s)); };
    // metadata=true (the default) adds the dataset's dimension labels — the source of the unit and
    // seasonal-adjustment labels.
    const QString path = QString("/series/%1/%2/%3?observations=1&format=json")
                             .arg(seg(provider_code), seg(dataset_code), seg(series_code));
    const QString full_id = QString("%1/%2/%3").arg(provider_code).arg(dataset_code).arg(series_code);
    LOG_DEBUG("DBnomicsService", QString("Fetching observations for %1").arg(full_id));

    fincept::HttpClient::instance().get_raw(
        build_url(path),
        [this, full_id, provider_code, dataset_code, series_code](fincept::Result<QByteArray> result) {
            // The body is parsed and the observations (QDate per period, the sort, the unit / seasonal
            // labels) converted on the decode thread; the signals and the hub publish stay on this
            // thread, in arrival order.
            (void)QtConcurrent::run(dbn_svc_pool(),
                                    [reply = dbn_svc_reply(result), full_id, series_code]() {
                                        QJsonDocument doc;
                                        dbn::DbnObservationsResult r;
                                        if (!dbn_svc_json(reply, doc, r.error))
                                            return r;
                                        return dbn::decode_observations(doc, full_id, series_code);
                                    })
                .then(this, [this, full_id, provider_code, dataset_code, series_code](dbn::DbnObservationsResult r) {
                    if (!r.ok) {
                        // A failed hub refresh must clear the topic's in_flight flag (publish_error), or
                        // retries are refused until the scheduler's refresh_timeout fires. The signal is
                        // still emitted for the screen's spinner and for the MCP tools that wait on it.
                        emit observations_failed(full_id, r.error);
                        emit error_occurred("observations", r.error);
                        if (hub_registered_)
                            fincept::datahub::DataHub::instance().publish_error(
                                hub_topic(provider_code, dataset_code, series_code), r.error);
                        return;
                    }
                    LOG_DEBUG("DBnomicsService",
                              QString("Loaded %1 observations for %2").arg(r.point.observations.size()).arg(full_id));
                    emit observations_loaded(r.point);
                    if (hub_registered_) {
                        const QString topic = hub_topic(provider_code, dataset_code, series_code);
                        fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(r.point));
                    }
                });
        },
        this);
}

void DBnomicsService::global_search(const QString& query, int offset) {
    if (query.trimmed().isEmpty())
        return;
    const QString path = QString("/search?q=%1&limit=20&offset=%2")
                             .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed())), QString::number(offset));
    LOG_DEBUG("DBnomicsService", QString("Searching: '%1' offset=%2").arg(query).arg(offset));

    fincept::HttpClient::instance().get_raw(
        build_url(path),
        [this, query, offset](fincept::Result<QByteArray> result) {
            (void)QtConcurrent::run(dbn_svc_pool(),
                                    [reply = dbn_svc_reply(result)]() {
                                        DbnSvcAnswer<dbn::DbnSearchPage> a;
                                        QJsonDocument doc;
                                        if (!dbn_svc_json(reply, doc, a.error))
                                            return a;
                                        a.value = dbn::decode_search(doc);
                                        a.ok = true;
                                        return a;
                                    })
                .then(this, [this, query, offset](DbnSvcAnswer<dbn::DbnSearchPage> a) {
                    if (!a.ok) {
                        emit error_occurred("search", a.error);
                        return;
                    }
                    emit search_results_loaded(query, a.value.results, DbnPagination{offset, 20, a.value.total},
                                               offset > 0);
                });
        },
        this);
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
    return ui::colors::chart_color(index);
}

// ── DataHub producer wiring ─────────────────────────────────────────────────

namespace {
// Topic segments are separated by ':', but dataset codes contain it too (every IMF WEO vintage is
// "WEO:2024-10"). Escape just '%' and ':' so the split in refresh() is reversible; every other
// character stays as is, so topics of ordinary codes are unchanged (other subscribers build them
// as a plain join).
QString topic_segment(const QString& s) {
    QString out = s;
    out.replace(QLatin1Char('%'), QStringLiteral("%25"));
    out.replace(QLatin1Char(':'), QStringLiteral("%3A"));
    return out;
}
} // namespace

QString DBnomicsService::hub_topic(const QString& provider, const QString& dataset, const QString& series) {
    return QStringLiteral("dbnomics:") + topic_segment(provider) + QLatin1Char(':') + topic_segment(dataset) +
           QLatin1Char(':') + topic_segment(series);
}

QStringList DBnomicsService::topic_patterns() const {
    return {QStringLiteral("dbnomics:*")};
}

void DBnomicsService::refresh(const QStringList& topics) {
    // Split `dbnomics:<provider>:<dataset>:<series>` and dispatch to the
    // existing observation fetcher. Segments are escaped by hub_topic() (':' → %3A), so
    // decode each one. Anything that doesn't match the 4-segment shape is silently skipped.
    for (const auto& topic : topics) {
        const QStringList parts = topic.split(QLatin1Char(':'));
        if (parts.size() != 4)
            continue;
        const auto dec = [](const QString& s) { return QUrl::fromPercentEncoding(s.toUtf8()); };
        fetch_observations(dec(parts[1]), dec(parts[2]), dec(parts[3]));
    }
}

int DBnomicsService::max_requests_per_sec() const {
    return 3; // DBnomics REST — conservative default
}

void DBnomicsService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // 1-hour TTL, 60s min_interval — DBnomics series are daily-or-slower.
    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 60 * 60 * 1000;
    policy.min_interval_ms = 60 * 1000;
    policy.push_only = false;
    hub.set_policy_pattern(QStringLiteral("dbnomics:*"), policy);

    hub_registered_ = true;
    LOG_DEBUG("DBnomicsService", "Registered with DataHub (dbnomics:*)");
}

} // namespace fincept::services
