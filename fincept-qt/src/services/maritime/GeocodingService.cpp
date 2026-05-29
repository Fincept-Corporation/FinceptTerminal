// src/services/maritime/GeocodingService.cpp
#include "services/maritime/GeocodingService.h"

#include "core/logging/Logger.h"
#include "storage/cache/CacheManager.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::services::maritime {

namespace {

// NOTE: unity builds merge this TU with PortsCatalog.cpp, whose anonymous
// namespace defines identically-named helpers (kUserAgent, kCacheTtlSec,
// from_json, deserialize_results, ...). Anonymous namespaces don't isolate
// across a unity merge, so every symbol here is prefixed `geo_` to avoid the
// ODR collision (same reason PortsCatalog renamed its publish helper).
constexpr const char* kNominatimEndpoint = "https://nominatim.openstreetmap.org/search";

// Nominatim's usage policy mandates a valid identifying User-Agent with
// contact info; anonymous/library-default UAs are blocked. Matches the
// convention already used by PortsCatalog for Wikidata.
constexpr const char* kGeoUserAgent = "FinceptTerminal/4.0 (https://fincept.in; support@fincept.in)";

// Place geometry is stable; a week keeps repeat searches instant across
// launches without going stale. Mirrors PortsCatalog's cache horizon.
constexpr int kGeoCacheTtlSec = 7 * 24 * 60 * 60;

QString geo_cache_key_for(const QString& query, int limit) {
    const QString payload = QStringLiteral("%1|%2").arg(query.toLower().trimmed()).arg(limit);
    const auto hash = QCryptographicHash::hash(payload.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("maritime:geocode:%1").arg(QString::fromLatin1(hash));
}

// ── GeoPlace ↔ JSON (cache round-trip) ───────────────────────────────────────
QJsonObject geo_to_json(const GeoPlace& p) {
    QJsonObject o;
    o["display_name"] = p.display_name;
    o["type"]         = p.type;
    o["lat"]          = p.latitude;
    o["lng"]          = p.longitude;
    o["min_lat"]      = p.min_lat;
    o["max_lat"]      = p.max_lat;
    o["min_lng"]      = p.min_lng;
    o["max_lng"]      = p.max_lng;
    return o;
}

GeoPlace geo_from_json(const QJsonObject& o) {
    GeoPlace p;
    p.display_name = o["display_name"].toString();
    p.type         = o["type"].toString();
    p.latitude     = o["lat"].toDouble();
    p.longitude    = o["lng"].toDouble();
    p.min_lat      = o["min_lat"].toDouble();
    p.max_lat      = o["max_lat"].toDouble();
    p.min_lng      = o["min_lng"].toDouble();
    p.max_lng      = o["max_lng"].toDouble();
    return p;
}

QByteArray geo_serialize_results(const QVector<GeoPlace>& rows) {
    QJsonArray arr;
    for (const auto& p : rows) arr.append(geo_to_json(p));
    return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}

QVector<GeoPlace> geo_deserialize_results(const QByteArray& blob) {
    QVector<GeoPlace> out;
    const auto arr = QJsonDocument::fromJson(blob).array();
    out.reserve(arr.size());
    for (const auto& v : arr) out.append(geo_from_json(v.toObject()));
    return out;
}

// Parse one Nominatim result object into a GeoPlace. `boundingbox` is an array
// of 4 strings: [min_lat, max_lat, min_lng, max_lng]. When absent (rare), fall
// back to a small box around the point so area-search still has an extent.
GeoPlace parse_nominatim(const QJsonObject& o) {
    GeoPlace p;
    p.display_name = o["display_name"].toString();
    p.type         = o["type"].toString();
    p.latitude     = o["lat"].toString().toDouble();
    p.longitude    = o["lon"].toString().toDouble();

    const QJsonArray bb = o["boundingbox"].toArray();
    if (bb.size() == 4) {
        p.min_lat = bb[0].toString().toDouble();
        p.max_lat = bb[1].toString().toDouble();
        p.min_lng = bb[2].toString().toDouble();
        p.max_lng = bb[3].toString().toDouble();
    } else {
        constexpr double kPad = 0.25;  // ~25-30 km half-extent fallback
        p.min_lat = p.latitude - kPad;
        p.max_lat = p.latitude + kPad;
        p.min_lng = p.longitude - kPad;
        p.max_lng = p.longitude + kPad;
    }
    return p;
}

}  // namespace

// ── Singleton ────────────────────────────────────────────────────────────────
GeocodingService& GeocodingService::instance() {
    static GeocodingService inst;
    return inst;
}

GeocodingService::GeocodingService(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

// ── Place search ─────────────────────────────────────────────────────────────
void GeocodingService::search(const QString& query, int limit) {
    const QString q = query.trimmed();
    if (q.isEmpty())
        return;

    const QString context = q;
    const QString cache_key = geo_cache_key_for(q, limit);

    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        emit places_found(geo_deserialize_results(cached.toString().toUtf8()), context);
        return;
    }

    QUrl url(QString::fromLatin1(kNominatimEndpoint));
    QUrlQuery params;
    params.addQueryItem("format", "jsonv2");
    params.addQueryItem("addressdetails", "0");
    params.addQueryItem("limit", QString::number(limit));
    params.addQueryItem("q", q);
    url.setQuery(params);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", kGeoUserAgent);
    req.setRawHeader("Accept", "application/json");

    QPointer<GeocodingService> self = this;
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [self, reply, context, cache_key]() {
        reply->deleteLater();
        if (!self)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("Geocoding", QString("Nominatim [%1]: %2").arg(context, reply->errorString()));
            emit self->error_occurred(context, reply->errorString());
            return;
        }
        const QByteArray body = reply->readAll();
        const QJsonArray arr = QJsonDocument::fromJson(body).array();

        QVector<GeoPlace> places;
        places.reserve(arr.size());
        for (const auto& v : arr)
            places.append(parse_nominatim(v.toObject()));

        fincept::CacheManager::instance().put(
            cache_key, QVariant(QString::fromUtf8(geo_serialize_results(places))),
            kGeoCacheTtlSec, "maritime");

        LOG_INFO("Geocoding", QString("Place search '%1': %2 result(s)").arg(context).arg(places.size()));
        emit self->places_found(places, context);
    });
}

} // namespace fincept::services::maritime
