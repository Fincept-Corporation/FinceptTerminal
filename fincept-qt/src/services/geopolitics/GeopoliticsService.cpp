// src/services/geopolitics/GeopoliticsService.cpp
#include "services/geopolitics/GeopoliticsService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QUrlQuery>
#include <QVariant>

namespace fincept::services::geo {

static constexpr const char* kApiBase = "https://api.fincept.in/research/news-events";

namespace {
inline void publish_to_hub(const QString& topic, const QVariant& value) {
    fincept::datahub::DataHub::instance().publish(topic, value);
}
}  // namespace

// ── Singleton ────────────────────────────────────────────────────────────────
GeopoliticsService& GeopoliticsService::instance() {
    static GeopoliticsService inst;
    return inst;
}

GeopoliticsService::GeopoliticsService(QObject* parent) : QObject(parent) {}

// ── Python helper ────────────────────────────────────────────────────────────
void GeopoliticsService::run_python(const QString& script, const QStringList& args, const QString& context,
                                    std::function<void(bool, const QString&)> cb) {
    QPointer<GeopoliticsService> self = this;
    python::PythonRunner::instance().run(script, args, [self, context, cb](python::PythonResult result) {
        if (!self)
            return;
        cb(result.success, result.success ? result.output : result.error);
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONFLICT MONITOR — HTTP API
// ═══════════════════════════════════════════════════════════════════════════════

void GeopoliticsService::fetch_events(const QString& country, const QString& city, const QString& category, int limit) {
    // Build URL with query params
    QUrl url(kApiBase);
    QUrlQuery q;
    if (!country.isEmpty())
        q.addQueryItem("country", country);
    if (!city.isEmpty())
        q.addQueryItem("city", city);
    if (!category.isEmpty())
        q.addQueryItem("event_category", category);
    q.addQueryItem("limit", QString::number(limit));
    url.setQuery(q);

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(url.toString(), [self, country, city, category, limit](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            LOG_ERROR("Geopolitics", "Events fetch failed: " + QString::fromStdString(result.error()));
            emit self->error_occurred("events", QString::fromStdString(result.error()));
            return;
        }
        auto root = result.value().object();
        // API returns {success, message, data: {events: [...], pagination: {...}}}
        auto obj = root.contains("data") ? root["data"].toObject() : root;
        auto arr = obj["events"].toArray();
        QVector<NewsEvent> events;
        events.reserve(arr.size());
        for (const auto& v : arr) {
            auto e = v.toObject();
            NewsEvent ev;
            ev.url = e["url"].toString();
            ev.source = e["source"].toString();
            ev.event_category = e["event_category"].toString();
            ev.title = e["title"].toString();
            ev.city = e["city"].toString();
            ev.country = e["country"].toString();
            const auto lat_v = e["latitude"];
            const auto lng_v = e["longitude"];
            ev.has_coords = lat_v.isDouble() && lng_v.isDouble();
            ev.latitude = ev.has_coords ? lat_v.toDouble() : 0.0;
            ev.longitude = ev.has_coords ? lng_v.toDouble() : 0.0;
            ev.extracted_date = e["extracted_date"].toString();
            ev.created_at = e["created_at"].toString();
            events.append(ev);
        }
        // Pagination envelope replaces the old flat "total" field
        int total = events.size();
        if (obj.contains("pagination"))
            total = obj["pagination"].toObject()["total_events"].toInt(total);
        else if (obj.contains("total"))
            total = obj["total"].toInt(total);
        // Serialize events to JSON for CacheManager persistence
        QJsonArray cached_arr;
        for (const auto& ev : events) {
            QJsonObject o;
            o["url"] = ev.url;
            o["source"] = ev.source;
            o["event_category"] = ev.event_category;
            o["title"] = ev.title;
            o["city"] = ev.city;
            o["country"] = ev.country;
            if (ev.has_coords) {
                o["latitude"] = ev.latitude;
                o["longitude"] = ev.longitude;
            }
            o["extracted_date"] = ev.extracted_date;
            o["created_at"] = ev.created_at;
            cached_arr.append(o);
        }
        QJsonObject cached_root;
        cached_root["events"] = cached_arr;
        cached_root["total"] = total;
        const QString cache_key = QString("geo:events:%1:%2:%3:%4").arg(country, city, category).arg(limit);
        fincept::CacheManager::instance().put(cache_key,
                                              QVariant(QJsonDocument(cached_root).toJson(QJsonDocument::Compact)),
                                              kEventsTtlSec, "geopolitics");
        LOG_INFO("Geopolitics", QString("Loaded %1 events").arg(events.size()));
        emit self->events_loaded(events, total);
        if (self->hub_registered_)
            publish_to_hub(QStringLiteral("geopolitics:events"), QVariant::fromValue(events));
    });
}

void GeopoliticsService::fetch_unique_countries() {
    // Cache hit — deserialize and emit, otherwise go to network.
    const QVariant cached = fincept::CacheManager::instance().get("geo:countries");
    if (!cached.isNull()) {
        const QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        QVector<UniqueCountry> countries;
        countries.reserve(arr.size());
        for (const auto& v : arr) {
            const auto o = v.toObject();
            countries.append({o["country"].toString(), o["event_count"].toInt()});
        }
        emit countries_loaded(countries);
        return;
    }

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(
        QString(kApiBase) + "?get_unique_countries=true&limit=100", [self](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                emit self->error_occurred("countries", QString::fromStdString(result.error()));
                return;
            }
            auto root = result.value().object();
            auto data = root.contains("data") ? root["data"].toObject() : root;
            auto arr = data["unique_countries"].toArray();
            QVector<UniqueCountry> countries;
            countries.reserve(arr.size());
            QJsonArray to_cache;
            for (const auto& v : arr) {
                auto o = v.toObject();
                const QString name = o["country"].toString();
                const int count = o["event_count"].toInt();
                countries.append({name, count});
                QJsonObject entry;
                entry["country"] = name;
                entry["event_count"] = count;
                to_cache.append(entry);
            }
            fincept::CacheManager::instance().put(
                "geo:countries",
                QVariant(QString::fromUtf8(QJsonDocument(to_cache).toJson(QJsonDocument::Compact))), kRefDataTtlSec,
                "geopolitics");
            emit self->countries_loaded(countries);
            if (self->hub_registered_)
                publish_to_hub(QStringLiteral("geopolitics:countries"), QVariant::fromValue(countries));
        });
}

void GeopoliticsService::fetch_unique_categories() {
    const QVariant cached = fincept::CacheManager::instance().get("geo:categories");
    if (!cached.isNull()) {
        const QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        QVector<UniqueCategory> cats;
        cats.reserve(arr.size());
        for (const auto& v : arr) {
            const auto o = v.toObject();
            cats.append({o["event_category"].toString(), o["event_count"].toInt()});
        }
        emit categories_loaded(cats);
        return;
    }

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(QString(kApiBase) + "?get_unique_categories=true", [self](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            emit self->error_occurred("categories", QString::fromStdString(result.error()));
            return;
        }
        auto root = result.value().object();
        auto data = root.contains("data") ? root["data"].toObject() : root;
        auto arr = data["unique_categories"].toArray();
        QVector<UniqueCategory> cats;
        cats.reserve(arr.size());
        QJsonArray to_cache;
        for (const auto& v : arr) {
            auto o = v.toObject();
            const QString name = o["event_category"].toString();
            const int count = o["event_count"].toInt();
            cats.append({name, count});
            QJsonObject entry;
            entry["event_category"] = name;
            entry["event_count"] = count;
            to_cache.append(entry);
        }
        fincept::CacheManager::instance().put(
            "geo:categories",
            QVariant(QString::fromUtf8(QJsonDocument(to_cache).toJson(QJsonDocument::Compact))), kRefDataTtlSec,
            "geopolitics");
        emit self->categories_loaded(cats);
        if (self->hub_registered_)
            publish_to_hub(QStringLiteral("geopolitics:categories"), QVariant::fromValue(cats));
    });
}

void GeopoliticsService::fetch_unique_cities() {
    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(QString(kApiBase) + "?get_unique_cities=true", [self](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            emit self->error_occurred("cities", QString::fromStdString(result.error()));
            return;
        }
        // Response: {success, message, data: {unique_cities: [{city, country}, ...]}}
        auto root = result.value().object();
        auto data = root.contains("data") ? root["data"].toObject() : root;
        auto arr = data["unique_cities"].toArray();
        QStringList cities;
        cities.reserve(arr.size());
        for (const auto& v : arr) {
            const auto city = v.toObject()["city"].toString();
            if (!city.isEmpty())
                cities.append(city);
        }
        emit self->cities_loaded(cities);
        if (self->hub_registered_)
            publish_to_hub(QStringLiteral("geopolitics:cities"), QVariant::fromValue(cities));
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDX HUMANITARIAN DATA — Python
// ═══════════════════════════════════════════════════════════════════════════════

static QVector<HDXDataset> parse_hdx_results(const QString& output) {
    auto json_str = python::extract_json(output);
    auto doc = QJsonDocument::fromJson(json_str.toUtf8());
    QVector<HDXDataset> datasets;

    // Script returns: {"success": true, "data": {"datasets": [...]}}
    // Try unwrapping the data envelope first, then fall back to bare array/object
    auto root = doc.object();
    QJsonArray arr;
    if (root.contains("data")) {
        auto data = root["data"].toObject();
        arr = data["datasets"].toArray();
    } else if (root.contains("datasets")) {
        arr = root["datasets"].toArray();
    } else {
        arr = doc.array();
    }

    datasets.reserve(arr.size());
    for (const auto& v : arr) {
        auto o = v.toObject();
        HDXDataset d;
        d.id = o["id"].toString();
        d.title = o["title"].toString();
        d.organization = o["organization"].toString();
        d.notes = o["notes"].toString();
        d.date = o["dataset_date"].toString();
        d.num_resources = o["num_resources"].toInt();
        // Tags in summary are plain strings; in full detail they are objects with "name"
        for (const auto& t : o["tags"].toArray()) {
            if (t.isString())
                d.tags.append(t.toString());
            else
                d.tags.append(t.toObject()["name"].toString());
        }
        d.raw = o;
        datasets.append(d);
    }
    return datasets;
}

void GeopoliticsService::search_hdx_conflicts() {
    run_python("hdx_data.py", {"search_conflict", "", "20"}, "hdx_conflicts", [this](bool ok, const QString& out) {
        if (!ok) {
            emit error_occurred("hdx_conflicts", out);
            return;
        }
        const auto datasets = parse_hdx_results(out);
        emit hdx_results_loaded("conflicts", datasets);
        publish_hdx_result(QStringLiteral("conflicts"), datasets);
    });
}

void GeopoliticsService::search_hdx_humanitarian() {
    run_python("hdx_data.py", {"search_humanitarian", "", "20"}, "hdx_humanitarian",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("hdx_humanitarian", out);
                       return;
                   }
                   const auto datasets = parse_hdx_results(out);
                   emit hdx_results_loaded("humanitarian", datasets);
                   publish_hdx_result(QStringLiteral("humanitarian"), datasets);
               });
}

void GeopoliticsService::search_hdx_by_country(const QString& country) {
    run_python("hdx_data.py", {"search_by_country", country}, "hdx_country", [this, country](bool ok, const QString& out) {
        if (!ok) {
            emit error_occurred("hdx_country", out);
            return;
        }
        const auto datasets = parse_hdx_results(out);
        emit hdx_results_loaded("country", datasets);
        publish_hdx_result(QStringLiteral("country:") + country, datasets);
    });
}

void GeopoliticsService::search_hdx_by_topic(const QString& topic) {
    run_python("hdx_data.py", {"search_by_topic", topic}, "hdx_topic", [this, topic](bool ok, const QString& out) {
        if (!ok) {
            emit error_occurred("hdx_topic", out);
            return;
        }
        const auto datasets = parse_hdx_results(out);
        emit hdx_results_loaded("topic", datasets);
        publish_hdx_result(QStringLiteral("topic:") + topic, datasets);
    });
}

void GeopoliticsService::search_hdx_advanced(const QString& query) {
    // Use search_datasets for free-text queries (advanced_search expects key:value pairs)
    run_python("hdx_data.py", {"search_datasets", query, "20"}, "hdx_search",
               [this, query](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("hdx_search", out);
                       return;
                   }
                   const auto datasets = parse_hdx_results(out);
                   emit hdx_results_loaded("search", datasets);
                   publish_hdx_result(QStringLiteral("search:") + query, datasets);
               });
}

// ═══════════════════════════════════════════════════════════════════════════════
// TRADE ANALYSIS — Python
// ═══════════════════════════════════════════════════════════════════════════════

void GeopoliticsService::analyze_trade_benefits(const QJsonObject& params) {
    auto json_str = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/economics/trade_geopolitics.py", {"benefits_costs", json_str}, "trade_benefits",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("trade_benefits", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   const auto obj = doc.object();
                   emit trade_result_ready("trade_benefits", obj);
                   if (hub_registered_)
                       publish_to_hub(QStringLiteral("geopolitics:trade:benefits"), QVariant(obj));
               });
}

void GeopoliticsService::analyze_trade_restrictions(const QJsonObject& params) {
    auto json_str = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/economics/trade_geopolitics.py", {"restrictions", json_str}, "trade_restrictions",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("trade_restrictions", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   const auto obj = doc.object();
                   emit trade_result_ready("trade_restrictions", obj);
                   if (hub_registered_)
                       publish_to_hub(QStringLiteral("geopolitics:trade:restrictions"), QVariant(obj));
               });
}

// ═══════════════════════════════════════════════════════════════════════════════
// GEOLOCATION — Python
// ═══════════════════════════════════════════════════════════════════════════════

void GeopoliticsService::extract_geolocations(const QStringList& headlines) {
    auto json = QJsonDocument(QJsonArray::fromStringList(headlines)).toJson(QJsonDocument::Compact);
    run_python("news_geolocation.py", {"extract_and_geocode", json}, "geolocation",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("geolocation", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   const auto obj = doc.object();
                   emit geolocation_ready(obj);
                   if (hub_registered_)
                       publish_to_hub(QStringLiteral("geopolitics:geolocation"), QVariant(obj));
               });
}

// ═══════════════════════════════════════════════════════════════════════════════
// DATAHUB PRODUCER — geopolitics:*
// ═══════════════════════════════════════════════════════════════════════════════

void GeopoliticsService::publish_hdx_result(const QString& context, const QVector<HDXDataset>& datasets) {
    if (!hub_registered_) return;
    publish_to_hub(QStringLiteral("geopolitics:hdx:") + context, QVariant::fromValue(datasets));
}

QStringList GeopoliticsService::topic_patterns() const {
    return {QStringLiteral("geopolitics:*")};
}

void GeopoliticsService::refresh(const QStringList& topics) {
    // Hub-driven refresh for the lightweight reference endpoints. Anything
    // parameterised (events:<filters>, hdx:*, trade:*, geolocation) is
    // user-invoked and not driven through the hub scheduler.
    for (const auto& topic : topics) {
        if (topic == QStringLiteral("geopolitics:events")) {
            fetch_events();
        } else if (topic == QStringLiteral("geopolitics:countries")) {
            fetch_unique_countries();
        } else if (topic == QStringLiteral("geopolitics:categories")) {
            fetch_unique_categories();
        } else if (topic == QStringLiteral("geopolitics:cities")) {
            fetch_unique_cities();
        }
    }
}

int GeopoliticsService::max_requests_per_sec() const {
    return 2;  // Fincept research API + HDX Python — conservative
}

void GeopoliticsService::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // Events — 2 min TTL (matches kEventsTtlSec), 30s min_interval.
    fincept::datahub::TopicPolicy events_policy;
    events_policy.ttl_ms = kEventsTtlSec * 1000;
    events_policy.min_interval_ms = 30 * 1000;
    hub.set_policy_pattern(QStringLiteral("geopolitics:events"), events_policy);

    // Reference data (countries/categories/cities) — 10 min TTL.
    fincept::datahub::TopicPolicy ref_policy;
    ref_policy.ttl_ms = kRefDataTtlSec * 1000;
    ref_policy.min_interval_ms = 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("geopolitics:countries"), ref_policy);
    hub.set_policy_pattern(QStringLiteral("geopolitics:categories"), ref_policy);
    hub.set_policy_pattern(QStringLiteral("geopolitics:cities"), ref_policy);

    // HDX datasets — 1 hour TTL (humanitarian data refresh cadence).
    fincept::datahub::TopicPolicy hdx_policy;
    hdx_policy.ttl_ms = 60 * 60 * 1000;
    hdx_policy.min_interval_ms = 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("geopolitics:hdx:*"), hdx_policy);

    // Trade analysis + geolocation — user-invoked, treat as push-only so the
    // hub caches the most recent result without scheduling a refresh.
    fincept::datahub::TopicPolicy push_policy;
    push_policy.push_only = true;
    push_policy.ttl_ms = 15 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("geopolitics:trade:*"), push_policy);
    hub.set_policy_pattern(QStringLiteral("geopolitics:geolocation"), push_policy);

    hub_registered_ = true;
    LOG_INFO("GeopoliticsService", "Registered with DataHub (geopolitics:*)");
}

} // namespace fincept::services::geo
