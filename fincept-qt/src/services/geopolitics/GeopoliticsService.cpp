// src/services/geopolitics/GeopoliticsService.cpp
#include "services/geopolitics/GeopoliticsService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "python/PythonRunner.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QUrlQuery>

namespace fincept::services::geo {

static constexpr const char* kApiBase = "https://api.fincept.in/research/news-events";

// ── Singleton ────────────────────────────────────────────────────────────────
GeopoliticsService& GeopoliticsService::instance() {
    static GeopoliticsService inst;
    return inst;
}

GeopoliticsService::GeopoliticsService(QObject* parent) : QObject(parent) {}

// ── Python helper ────────────────────────────────────────────────────────────
void GeopoliticsService::run_python(const QString& script, const QStringList& args,
                                     const QString& context,
                                     std::function<void(bool, const QString&)> cb) {
    QPointer<GeopoliticsService> self = this;
    python::PythonRunner::instance().run(script, args,
        [self, context, cb](python::PythonResult result) {
            if (!self) return;
            cb(result.success, result.success ? result.output : result.error);
        });
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONFLICT MONITOR — HTTP API
// ═══════════════════════════════════════════════════════════════════════════════

void GeopoliticsService::fetch_events(const QString& country, const QString& city,
                                       const QString& category, int limit) {
    // Build URL with query params
    QUrl url(kApiBase);
    QUrlQuery q;
    if (!country.isEmpty()) q.addQueryItem("country", country);
    if (!city.isEmpty())    q.addQueryItem("city", city);
    if (!category.isEmpty()) q.addQueryItem("event_category", category);
    q.addQueryItem("limit", QString::number(limit));
    url.setQuery(q);

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(url.toString(),
        [self](Result<QJsonDocument> result) {
            if (!self) return;
            if (!result.is_ok()) {
                LOG_ERROR("Geopolitics", "Events fetch failed: " + QString::fromStdString(result.error()));
                emit self->error_occurred("events", QString::fromStdString(result.error()));
                return;
            }
            auto root = result.value().object();
            // API returns {success, data: {events: [...]}} — unwrap the data envelope
            auto obj = root.contains("data") ? root["data"].toObject() : root;
            auto arr = obj["events"].toArray();
            QVector<NewsEvent> events;
            events.reserve(arr.size());
            for (const auto& v : arr) {
                auto e = v.toObject();
                NewsEvent ev;
                ev.url = e["url"].toString();
                ev.domain = e["domain"].toString();
                ev.event_category = e["event_category"].toString();
                ev.matched_keywords = e["matched_keywords"].toString();
                ev.city = e["city"].toString();
                ev.country = e["country"].toString();
                ev.latitude = e["latitude"].toDouble();
                ev.longitude = e["longitude"].toDouble();
                ev.extracted_date = e["extracted_date"].toString();
                ev.created_at = e["created_at"].toString();
                // Filter out invalid coordinates
                if (std::isnan(ev.latitude) || std::isnan(ev.longitude)) continue;
                events.append(ev);
            }
            int total = obj.contains("total") ? obj["total"].toInt() : events.size();
            self->events_cache_ = {events, total, QDateTime::currentSecsSinceEpoch()};
            LOG_INFO("Geopolitics", QString("Loaded %1 events").arg(events.size()));
            emit self->events_loaded(events, total);
        });
}

void GeopoliticsService::fetch_unique_countries() {
    auto now = QDateTime::currentSecsSinceEpoch();
    if ((now - countries_ts_) < kRefDataTtlSec && countries_ts_ > 0) return;

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(QString(kApiBase) + "?get_unique_countries=true&limit=100",
        [self](Result<QJsonDocument> result) {
            if (!self) return;
            if (!result.is_ok()) {
                emit self->error_occurred("countries", QString::fromStdString(result.error()));
                return;
            }
            auto root = result.value().object();
            auto data = root.contains("data") ? root["data"].toObject() : root;
            auto arr = data["unique_countries"].toArray();
            QVector<UniqueCountry> countries;
            countries.reserve(arr.size());
            for (const auto& v : arr) {
                auto o = v.toObject();
                countries.append({o["country"].toString(), o["event_count"].toInt()});
            }
            self->countries_ts_ = QDateTime::currentSecsSinceEpoch();
            emit self->countries_loaded(countries);
        });
}

void GeopoliticsService::fetch_unique_categories() {
    auto now = QDateTime::currentSecsSinceEpoch();
    if ((now - categories_ts_) < kRefDataTtlSec && categories_ts_ > 0) return;

    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(QString(kApiBase) + "?get_unique_categories=true",
        [self](Result<QJsonDocument> result) {
            if (!self) return;
            if (!result.is_ok()) {
                emit self->error_occurred("categories", QString::fromStdString(result.error()));
                return;
            }
            auto root = result.value().object();
            auto data = root.contains("data") ? root["data"].toObject() : root;
            auto arr = data["unique_categories"].toArray();
            QVector<UniqueCategory> cats;
            cats.reserve(arr.size());
            for (const auto& v : arr) {
                auto o = v.toObject();
                cats.append({o["event_category"].toString(), o["event_count"].toInt()});
            }
            self->categories_ts_ = QDateTime::currentSecsSinceEpoch();
            emit self->categories_loaded(cats);
        });
}

void GeopoliticsService::fetch_unique_cities() {
    QPointer<GeopoliticsService> self = this;
    HttpClient::instance().get(QString(kApiBase) + "/unique-cities",
        [self](Result<QJsonDocument> result) {
            if (!self) return;
            if (!result.is_ok()) {
                emit self->error_occurred("cities", QString::fromStdString(result.error()));
                return;
            }
            auto arr = result.value().array();
            QStringList cities;
            cities.reserve(arr.size());
            for (const auto& v : arr) {
                auto o = v.toObject();
                auto city = o["city"].toString();
                if (!city.isEmpty()) cities.append(city);
            }
            emit self->cities_loaded(cities);
        });
}

// ═══════════════════════════════════════════════════════════════════════════════
// HDX HUMANITARIAN DATA — Python
// ═══════════════════════════════════════════════════════════════════════════════

static QVector<HDXDataset> parse_hdx_results(const QString& output) {
    auto json_str = python::extract_json(output);
    auto doc = QJsonDocument::fromJson(json_str.toUtf8());
    QVector<HDXDataset> datasets;
    auto arr = doc.object()["datasets"].toArray();
    if (arr.isEmpty()) arr = doc.array();
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
        for (const auto& t : o["tags"].toArray())
            d.tags.append(t.toObject()["name"].toString());
        d.raw = o;
        datasets.append(d);
    }
    return datasets;
}

void GeopoliticsService::search_hdx_conflicts() {
    run_python("hdx_data.py", {"search_conflict"}, "hdx_conflicts",
        [this](bool ok, const QString& out) {
            if (!ok) { emit error_occurred("hdx_conflicts", out); return; }
            emit hdx_results_loaded("conflicts", parse_hdx_results(out));
        });
}

void GeopoliticsService::search_hdx_humanitarian() {
    run_python("hdx_data.py", {"search_humanitarian"}, "hdx_humanitarian",
        [this](bool ok, const QString& out) {
            if (!ok) { emit error_occurred("hdx_humanitarian", out); return; }
            emit hdx_results_loaded("humanitarian", parse_hdx_results(out));
        });
}

void GeopoliticsService::search_hdx_by_country(const QString& country) {
    run_python("hdx_data.py", {"search_by_country", country}, "hdx_country",
        [this](bool ok, const QString& out) {
            if (!ok) { emit error_occurred("hdx_country", out); return; }
            emit hdx_results_loaded("country", parse_hdx_results(out));
        });
}

void GeopoliticsService::search_hdx_by_topic(const QString& topic) {
    run_python("hdx_data.py", {"search_by_topic", topic}, "hdx_topic",
        [this](bool ok, const QString& out) {
            if (!ok) { emit error_occurred("hdx_topic", out); return; }
            emit hdx_results_loaded("topic", parse_hdx_results(out));
        });
}

void GeopoliticsService::search_hdx_advanced(const QString& query) {
    run_python("hdx_data.py", {"advanced_search", query}, "hdx_search",
        [this](bool ok, const QString& out) {
            if (!ok) { emit error_occurred("hdx_search", out); return; }
            emit hdx_results_loaded("search", parse_hdx_results(out));
        });
}

// ═══════════════════════════════════════════════════════════════════════════════
// TRADE ANALYSIS — Python
// ═══════════════════════════════════════════════════════════════════════════════

void GeopoliticsService::analyze_trade_benefits(const QJsonObject& params) {
    auto json_str = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/economics/trade_geopolitics.py",
               {"benefits_costs", json_str}, "trade_benefits",
        [this](bool ok, const QString& out) {
            if (!ok) { emit error_occurred("trade_benefits", out); return; }
            auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
            emit trade_result_ready("trade_benefits", doc.object());
        });
}

void GeopoliticsService::analyze_trade_restrictions(const QJsonObject& params) {
    auto json_str = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/economics/trade_geopolitics.py",
               {"restrictions", json_str}, "trade_restrictions",
        [this](bool ok, const QString& out) {
            if (!ok) { emit error_occurred("trade_restrictions", out); return; }
            auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
            emit trade_result_ready("trade_restrictions", doc.object());
        });
}

// ═══════════════════════════════════════════════════════════════════════════════
// GEOLOCATION — Python
// ═══════════════════════════════════════════════════════════════════════════════

void GeopoliticsService::extract_geolocations(const QStringList& headlines) {
    auto json = QJsonDocument(QJsonArray::fromStringList(headlines)).toJson(QJsonDocument::Compact);
    run_python("news_geolocation.py", {"extract_and_geocode", json}, "geolocation",
        [this](bool ok, const QString& out) {
            if (!ok) { emit error_occurred("geolocation", out); return; }
            auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
            emit geolocation_ready(doc.object());
        });
}

} // namespace fincept::services::geo
