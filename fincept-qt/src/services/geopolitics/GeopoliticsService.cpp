// src/services/geopolitics/GeopoliticsService.cpp
#include "services/geopolitics/GeopoliticsService.h"

#include "auth/AuthManager.h"
#include "services/billing/PlanAccess.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "network/http/HttpClient.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>
#include <QUrlQuery>
#include <QVariant>

#include <utility>

namespace fincept::services::geo {

static QString geo_api_base() {
    return fincept::AppConfig::instance().api_base_url() + QStringLiteral("/research/news-events");
}

namespace {
inline void publish_to_hub(const QString& topic, const QVariant& value) {
    fincept::datahub::DataHub::instance().publish(topic, value);
}
// A refusal that waiting does not fix (signed out, no credits, not allowed) backs
// off for hours instead of the hub's 5-minute ladder cap; a sign-in, a balance
// increase or the user's refresh lifts it (GeopoliticsService::release_hub_refusal).
constexpr std::chrono::milliseconds kGeoSvcRefusedRetry = fincept::datahub::DataHub::kNegativeCacheRetry * 24;
constexpr std::chrono::milliseconds kGeoSvcIpBlockRetry = fincept::datahub::DataHub::kNegativeCacheRetry * 2;

// research.py NEWS_EVENTS_CREDIT_COST: every news-events call.
constexpr double kGeoSvcCreditCost = 3.0;

// A hub-driven refresh that fails must say so: the hub marks the topic in-flight and
// otherwise waits out refresh_timeout_ms before it can retry. Returns the kind of
// refusal (see above), or None for a transient failure the hub's ladder retries.
inline fincept::auth::RefusalLatch::Kind geo_svc_publish_error(const QString& topic, const QString& message) {
    using Kind = fincept::auth::RefusalLatch::Kind;
    const int status = HttpClient::status_from_error(message.toStdString());
    auto& hub = fincept::datahub::DataHub::instance();
    if (status == 401) {
        hub.publish_error(topic, QStringLiteral("Sign in to Fincept to load geopolitics events."), kGeoSvcRefusedRetry);
        return Kind::Access;
    }
    if (status == 402) {
        // On Free the message also says what Pro changes for events (included).
        hub.publish_error(topic,
                          fincept::billing::PlanAccess::instance().with_upgrade_note(
                              QStringLiteral("Out of Fincept credits (3 per update). Top up or upgrade in Plans & "
                                             "Pricing, then refresh — paused until then."),
                              QStringLiteral("news_events")),
                          kGeoSvcRefusedRetry);
        return Kind::Credits;
    }
    if (status == 403) {
        const bool ip = HttpClient::instance().is_ip_blocked();
        hub.publish_error(topic, HttpClient::message_from_error(message.toStdString()).left(200),
                          ip ? kGeoSvcIpBlockRetry : kGeoSvcRefusedRetry);
        return ip ? Kind::None : Kind::Access;
    }
    hub.publish_error(topic, message.left(200));
    return Kind::None;
}
/// Credit figures arrive as JSON numbers (fractional — the balance is NUMERIC) or,
/// from some paths, strings. toInt() rejected every fractional balance, so the
/// screen never showed one. Returns `def` when absent / unparsable.
inline double geo_svc_credit_value(const QJsonValue& v, double def) {
    if (v.isDouble())
        return v.toDouble();
    bool ok = false;
    const double d = v.toString().trimmed().toDouble(&ok);
    return ok ? d : def;
}
} // namespace

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

void GeopoliticsService::fetch_events(const QString& country, const QString& city, const QString& category, int limit,
                                      int page, const QString& source, const QString& date_from,
                                      const QString& date_to) {
    QUrl url(geo_api_base());
    QUrlQuery q;
    if (!country.isEmpty())
        q.addQueryItem("country", country);
    if (!city.isEmpty())
        q.addQueryItem("city", city);
    if (!category.isEmpty())
        q.addQueryItem("event_category", category);
    if (!source.isEmpty())
        q.addQueryItem("source", source);
    if (!date_from.isEmpty())
        q.addQueryItem("date_from", date_from);
    if (!date_to.isEmpty())
        q.addQueryItem("date_to", date_to);
    q.addQueryItem("limit", QString::number(limit));
    if (page > 1)
        q.addQueryItem("page", QString::number(page));
    url.setQuery(q);

    // Only an unfiltered first-page request may feed the shared geopolitics:events
    // hub topic. The dashboard events widget subscribes to it and expects the
    // global latest-events list — a country/category-filtered fetch from the
    // Conflict Monitor used to overwrite it with that filtered page.
    const bool hub_eligible = country.trimmed().isEmpty() && city.trimmed().isEmpty() && category.trimmed().isEmpty() &&
                              source.trimmed().isEmpty() && date_from.trimmed().isEmpty() &&
                              date_to.trimmed().isEmpty() && page <= 1;
    const QString request_key = events_request_key(country, city, category, limit, page, source, date_from, date_to);

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(
        url.toString(),
        [self, limit, page, hub_eligible, request_key](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                {
                    // Expected refusals (signed out, out of credits, route not offered by this
                    // server) are INFO; anything else is a degraded source: WARN (policy B.1).
                    const int status = HttpClient::status_from_error(result.error());
                    if (status == 401 || status == 402 || status == 404)
                        LOG_INFO("Geopolitics", logf("Events fetch failed", {{"status", status},
                                                                             {"error", QString::fromStdString(result.error())}}));
                    else
                        LOG_WARN("Geopolitics", logf("Events fetch failed", {{"status", status},
                                                                             {"error", QString::fromStdString(result.error())}}));
                }
                emit self->error_occurred("events", QString::fromStdString(result.error()));
                if (self->hub_registered_ && hub_eligible)
                    self->note_hub_refusal(QStringLiteral("geopolitics:events"),
                                           geo_svc_publish_error(QStringLiteral("geopolitics:events"),
                                                                 QString::fromStdString(result.error())));
                return;
            }
            // Response envelope:
            //   {success, message, data: {events:[...], pagination:{...},
            //                             filters_applied:{...},
            //                             credits_used:N, remaining_credits:N}}
            const auto root = result.value().object();
            const auto obj = root.contains("data") ? root["data"].toObject() : root;
            const auto arr = obj["events"].toArray();

            EventsPage page_data;
            page_data.request_key = request_key;
            page_data.events.reserve(arr.size());
            for (const auto& v : arr) {
                const auto e = v.toObject();
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
                page_data.events.append(ev);
            }

            // Newest first — extracted_date is "YYYY-MM-DD HH:MM:SS" so plain
            // string comparison is lexicographically correct.
            std::sort(page_data.events.begin(), page_data.events.end(),
                      [](const NewsEvent& a, const NewsEvent& b) { return a.extracted_date > b.extracted_date; });

            if (obj.contains("pagination")) {
                const auto p = obj["pagination"].toObject();
                page_data.total_events = p["total_events"].toInt(page_data.events.size());
                page_data.current_page = p["current_page"].toInt(page);
                page_data.total_pages = p["total_pages"].toInt(0);
                page_data.events_per_page = p["events_per_page"].toInt(limit);
                page_data.has_next = p["has_next"].toBool(false);
                page_data.has_prev = p["has_prev"].toBool(false);
            } else {
                page_data.total_events = obj["total"].toInt(page_data.events.size());
                page_data.current_page = page;
                page_data.events_per_page = limit;
            }
            page_data.credits_used = geo_svc_credit_value(obj["credits_used"], 0.0);
            page_data.remaining_credits = geo_svc_credit_value(obj["remaining_credits"], -1.0);
            // (A CacheManager copy of every page used to be written here "for offline
            // replay", but nothing ever read it back — the hub keeps the last page.)

            LOG_DEBUG("Geopolitics", QString("Loaded %1 events (page %2/%3, total %4, %5 credits left)")
                                        .arg(page_data.events.size())
                                        .arg(page_data.current_page)
                                        .arg(page_data.total_pages)
                                        .arg(page_data.total_events)
                                        .arg(page_data.remaining_credits, 0, 'f', 2));

            emit self->events_loaded(page_data);
            if (self->hub_registered_ && hub_eligible)
                publish_to_hub(QStringLiteral("geopolitics:events"), QVariant::fromValue(page_data));
        });
}

bool GeopoliticsService::fetch_unique_countries(bool cache_only) {
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
        // A hub-driven refresh that lands on a cache hit must still fill the topic,
        // otherwise its subscribers wait for the cache entry to expire.
        if (hub_registered_)
            publish_to_hub(QStringLiteral("geopolitics:countries"), QVariant::fromValue(countries));
        return true;
    }
    if (cache_only)
        return false;

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(
        geo_api_base() + "?get_unique_countries=true&limit=100", [self](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                emit self->error_occurred("countries", QString::fromStdString(result.error()));
                if (self->hub_registered_)
                    self->note_hub_refusal(QStringLiteral("geopolitics:countries"),
                                           geo_svc_publish_error(QStringLiteral("geopolitics:countries"),
                                                                 QString::fromStdString(result.error())));
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
                "geo:countries", QVariant(QString::fromUtf8(QJsonDocument(to_cache).toJson(QJsonDocument::Compact))),
                kRefDataTtlSec, "geopolitics");
            emit self->countries_loaded(countries);
            if (self->hub_registered_)
                publish_to_hub(QStringLiteral("geopolitics:countries"), QVariant::fromValue(countries));
        });
    return false;
}

bool GeopoliticsService::fetch_unique_categories(bool cache_only) {
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
        if (hub_registered_)
            publish_to_hub(QStringLiteral("geopolitics:categories"), QVariant::fromValue(cats));
        return true;
    }
    if (cache_only)
        return false;

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(geo_api_base() + "?get_unique_categories=true", [self](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            emit self->error_occurred("categories", QString::fromStdString(result.error()));
            if (self->hub_registered_)
                self->note_hub_refusal(QStringLiteral("geopolitics:categories"),
                                       geo_svc_publish_error(QStringLiteral("geopolitics:categories"),
                                                             QString::fromStdString(result.error())));
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
            "geo:categories", QVariant(QString::fromUtf8(QJsonDocument(to_cache).toJson(QJsonDocument::Compact))),
            kRefDataTtlSec, "geopolitics");
        emit self->categories_loaded(cats);
        if (self->hub_registered_)
            publish_to_hub(QStringLiteral("geopolitics:categories"), QVariant::fromValue(cats));
    });
    return false;
}

bool GeopoliticsService::fetch_unique_cities(bool cache_only) {
    // Reference data — same cache treatment as countries/categories so opening
    // the screen doesn't re-download the full city list every time. The list has
    // no limit (every city the events database knows), so — as for the maritime
    // area search — the GUI thread only reads the cache row, writes the copy and
    // emits; parsing, shaping and serialising run on a worker.
    const QVariant cached = fincept::CacheManager::instance().get("geo:cities");
    if (!cached.isNull()) {
        QPointer<GeopoliticsService> self = this;
        const quint64 shown_at_read = cities_fetch_shown_;
        QThreadPool::globalInstance()->start([self, shown_at_read, text = cached.toString()]() {
            QStringList cached_cities;
            const QJsonArray arr = QJsonDocument::fromJson(text.toUtf8()).array();
            cached_cities.reserve(arr.size());
            for (const auto& v : arr)
                cached_cities.append(v.toString());
            // Back on the GUI thread: signal, hub.
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [self, shown_at_read, cached_cities = std::move(cached_cities)]() {
                    if (!self)
                        return;
                    if (self->cities_fetch_shown_ != shown_at_read)
                        return; // a freshly fetched list was emitted meanwhile: this copy is older
                    emit self->cities_loaded(cached_cities);
                    // A hub-driven refresh that lands on a cache hit must still fill the topic.
                    if (self->hub_registered_)
                        publish_to_hub(QStringLiteral("geopolitics:cities"), QVariant::fromValue(cached_cities));
                },
                Qt::QueuedConnection);
        });
        return true;
    }
    if (cache_only)
        return false;

    // Every city the events database knows (no limit): its size grows with the
    // data, so the body is parsed on HttpClient's worker, not the GUI thread.
    QPointer<GeopoliticsService> self = this;
    const quint64 fetch_seq = ++cities_fetch_seq_;
    const QString url = geo_api_base() + "?get_unique_cities=true";
    HttpClient::instance().get_large(url, [self, fetch_seq](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            emit self->error_occurred("cities", QString::fromStdString(result.error()));
            if (self->hub_registered_)
                self->note_hub_refusal(QStringLiteral("geopolitics:cities"),
                                       geo_svc_publish_error(QStringLiteral("geopolitics:cities"),
                                                             QString::fromStdString(result.error())));
            return;
        }
        // Response: {success, message, data: {unique_cities: [{city, country}, ...]}}
        // The list and its cache JSON are built on a worker.
        QThreadPool::globalInstance()->start([self, fetch_seq, doc = result.value()]() {
            const QJsonObject root = doc.object();
            const QJsonObject data = root.contains("data") ? root["data"].toObject() : root;
            const QJsonArray arr = data["unique_cities"].toArray();
            QStringList cities;
            cities.reserve(arr.size());
            for (const auto& v : arr) {
                const auto city = v.toObject()["city"].toString();
                if (!city.isEmpty())
                    cities.append(city);
            }
            QString cities_json =
                QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(cities)).toJson(QJsonDocument::Compact));

            // Back on the GUI thread: cache write, signal, hub.
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [self, fetch_seq, cities = std::move(cities), cities_json = std::move(cities_json)]() {
                    if (!self)
                        return;
                    if (fetch_seq < self->cities_fetch_shown_)
                        return; // a newer fetch's list was already emitted (and cached)
                    self->cities_fetch_shown_ = fetch_seq;
                    fincept::CacheManager::instance().put("geo:cities", QVariant(cities_json), kRefDataTtlSec,
                                                          "geopolitics");
                    emit self->cities_loaded(cities);
                    if (self->hub_registered_)
                        publish_to_hub(QStringLiteral("geopolitics:cities"), QVariant::fromValue(cities));
                },
                Qt::QueuedConnection);
        });
    }, this);
    return false;
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

/// The search's total match count ("count" in the script's data envelope);
/// -1 when the output carries none.
static int parse_hdx_count(const QString& output) {
    const auto doc = QJsonDocument::fromJson(python::extract_json(output).toUtf8());
    const auto root = doc.object();
    const QJsonValue count =
        root.contains("data") ? root["data"].toObject().value("count") : root.value("count");
    return count.isDouble() ? count.toInt() : -1;
}

/// Shared tail of every HDX search: error, or total count + datasets + hub publish.
void GeopoliticsService::finish_hdx_search(bool ok, const QString& out, const QString& error_context,
                                          const QString& result_context, const QString& hub_context) {
    if (!ok) {
        emit error_occurred(error_context, out);
        return;
    }
    const auto datasets = parse_hdx_results(out);
    const int total = parse_hdx_count(out);
    emit hdx_total_count(result_context, total >= 0 ? total : static_cast<int>(datasets.size()));
    emit hdx_results_loaded(result_context, datasets);
    publish_hdx_result(hub_context, datasets);
}

void GeopoliticsService::search_hdx_conflicts(int limit) {
    run_python("hdx_data.py", {"search_conflict", "", QString::number(limit)}, "hdx_conflicts",
               [this](bool ok, const QString& out) {
                   finish_hdx_search(ok, out, QStringLiteral("hdx_conflicts"), QStringLiteral("conflicts"),
                                     QStringLiteral("conflicts"));
               });
}

void GeopoliticsService::search_hdx_humanitarian(int limit) {
    run_python("hdx_data.py", {"search_humanitarian", "", QString::number(limit)}, "hdx_humanitarian",
               [this](bool ok, const QString& out) {
                   finish_hdx_search(ok, out, QStringLiteral("hdx_humanitarian"), QStringLiteral("humanitarian"),
                                     QStringLiteral("humanitarian"));
               });
}

void GeopoliticsService::search_hdx_by_country(const QString& country, int limit) {
    run_python("hdx_data.py", {"search_by_country", country, QString::number(limit)}, "hdx_country",
               [this, country](bool ok, const QString& out) {
                   finish_hdx_search(ok, out, QStringLiteral("hdx_country"), QStringLiteral("country"),
                                     QStringLiteral("country:") + country);
               });
}

void GeopoliticsService::search_hdx_by_topic(const QString& topic, int limit) {
    run_python("hdx_data.py", {"search_by_topic", topic, QString::number(limit)}, "hdx_topic",
               [this, topic](bool ok, const QString& out) {
                   finish_hdx_search(ok, out, QStringLiteral("hdx_topic"), QStringLiteral("topic"),
                                     QStringLiteral("topic:") + topic);
               });
}

void GeopoliticsService::search_hdx_advanced(const QString& query, int limit) {
    // Use search_datasets for free-text queries (advanced_search expects key:value pairs)
    run_python("hdx_data.py", {"search_datasets", query, QString::number(limit)}, "hdx_search",
               [this, query](bool ok, const QString& out) {
                   finish_hdx_search(ok, out, QStringLiteral("hdx_search"), QStringLiteral("search"),
                                     QStringLiteral("search:") + query);
               });
}

void GeopoliticsService::search_hdx_by_organization(const QString& org_slug, int limit) {
    run_python("hdx_data.py", {"search_by_organization", org_slug, QString::number(limit)}, "hdx_organization",
               [this, org_slug](bool ok, const QString& out) {
                   finish_hdx_search(ok, out, QStringLiteral("hdx_organization"), QStringLiteral("organization"),
                                     QStringLiteral("organization:") + org_slug);
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

void GeopoliticsService::analyze_trading_blocs(const QJsonObject& params) {
    auto json_str = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/economics/trade_geopolitics.py", {"trading_blocs", json_str}, "trade_blocs",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("trade_blocs", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   const auto obj = doc.object();
                   emit trade_result_ready("trade_blocs", obj);
                   if (hub_registered_)
                       publish_to_hub(QStringLiteral("geopolitics:trade:blocs"), QVariant(obj));
               });
}

void GeopoliticsService::analyze_barrier_removal(const QJsonObject& params) {
    auto json_str = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/economics/trade_geopolitics.py", {"barrier_removal", json_str}, "trade_barrier",
               [this](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("trade_barrier", out);
                       return;
                   }
                   auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   const auto obj = doc.object();
                   emit trade_result_ready("trade_barrier", obj);
                   if (hub_registered_)
                       publish_to_hub(QStringLiteral("geopolitics:trade:barrier"), QVariant(obj));
               });
}

// ═══════════════════════════════════════════════════════════════════════════════
// DATAHUB PRODUCER — geopolitics:*
// ═══════════════════════════════════════════════════════════════════════════════

void GeopoliticsService::publish_hdx_result(const QString& context, const QVector<HDXDataset>& datasets) {
    if (!hub_registered_)
        return;
    publish_to_hub(QStringLiteral("geopolitics:hdx:") + context, QVariant::fromValue(datasets));
}

QStringList GeopoliticsService::topic_patterns() const {
    return {QStringLiteral("geopolitics:*")};
}

void GeopoliticsService::refresh(const QStringList& topics) {
    // Every news-events call needs a signed-in account (and 3 credits). The
    // dashboard can subscribe — or be pre-warmed — before sign-in completes:
    // don't send those requests just to collect 401s.
    const QString key = auth::AuthManager::instance().session().api_key;
    if (key.isEmpty()) {
        for (const auto& topic : topics) {
            if (topic == QStringLiteral("geopolitics:events") || topic == QStringLiteral("geopolitics:countries") ||
                topic == QStringLiteral("geopolitics:categories") || topic == QStringLiteral("geopolitics:cities")) {
                fincept::datahub::DataHub::instance().publish_error(
                    topic, QStringLiteral("Sign in to Fincept to load geopolitics events."), kGeoSvcRefusedRetry);
                note_hub_refusal(topic, auth::RefusalLatch::Kind::Access);
            }
        }
        return;
    }
    // Hub-driven refresh for the lightweight reference endpoints. Anything
    // parameterised (events:<filters>, hdx:*, trade:*) is
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
    return 2; // Fincept research API + HDX Python — conservative
}

void GeopoliticsService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // Events — every request costs API credits (3 per call) and the source refreshes
    // every ~30 min, so a 2-min TTL made a visible dashboard events widget re-buy the
    // same page ~30 times an hour. Align the TTL with the source cadence; a widget's
    // REFRESH button still forces a fetch.
    fincept::datahub::TopicPolicy events_policy;
    events_policy.ttl_ms = kEventsHubTtlSec * 1000;
    events_policy.min_interval_ms = 5 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("geopolitics:events"), events_policy);

    // Reference data (countries/categories/cities) — slow-moving lists, each call metered.
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

    // Trade analysis — user-invoked, treat as push-only so the hub caches the
    // most recent result without scheduling a refresh.
    fincept::datahub::TopicPolicy push_policy;
    push_policy.push_only = true;
    push_policy.ttl_ms = 15 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("geopolitics:trade:*"), push_policy);

    // What lifts a refusal backoff: a new sign-in / key, or credits again.
    auto& am = auth::AuthManager::instance();
    connect(&am, &auth::AuthManager::auth_state_changed, this, [this]() {
        const auto& s = auth::AuthManager::instance().session();
        if (refusal_.release_on_auth(s.authenticated, s.api_key))
            release_hub_refusal("signed in");
    });
    connect(&am, &auth::AuthManager::credit_balance_changed, this, [this](double balance) {
        if (refusal_.release_on_balance(balance, kGeoSvcCreditCost))
            release_hub_refusal("credits topped up");
    });

    hub_registered_ = true;
    LOG_DEBUG("GeopoliticsService", "Registered with DataHub (geopolitics:*)");
}

void GeopoliticsService::note_hub_refusal(const QString& topic, auth::RefusalLatch::Kind kind) {
    if (kind == auth::RefusalLatch::Kind::None)
        return;
    auto& am = auth::AuthManager::instance();
    refusal_.refuse(kind, am.session().api_key, am.session().user_info.credit_balance);
    refused_topics_.insert(topic);
    // The header chip may still show credits the server says are gone: re-read
    // the account (see RefusalLatch). Once per refusal, not per retry.
    if (kind == auth::RefusalLatch::Kind::Credits)
        am.refresh_user_data(auth::AuthManager::Refresh::Now);
}

void GeopoliticsService::release_hub_refusal(const char* why) {
    if (!refusal_.active())
        return;
    refusal_.clear();
    LOG_INFO("GeopoliticsService", QString("Refused topics released (%1)").arg(QLatin1String(why)));
    auto& hub = fincept::datahub::DataHub::instance();
    hub.clear_backoff(QStringLiteral("geopolitics:*"));
    // Topics on screen fetch now (an event, not a timer — the min-interval would
    // otherwise hold them back); the rest wait for their next subscriber.
    const QSet<QString> topics = std::exchange(refused_topics_, {});
    for (const QString& t : topics)
        if (!hub.subscribers(t).isEmpty())
            hub.request(t, /*force=*/true);
}

} // namespace fincept::services::geo
