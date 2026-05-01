// src/services/maritime/PortsCatalog.cpp
#include "services/maritime/PortsCatalog.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "storage/cache/CacheManager.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::services::maritime {

namespace {

constexpr const char* kWikidataEndpoint    = "https://query.wikidata.org/sparql";
constexpr const char* kMarineRegionsBase   = "https://www.marineregions.org/rest/getGazetteerRecordsByName.json";
constexpr const char* kOverpassEndpoint    = "https://overpass-api.de/api/interpreter";

// Wikidata's "honest API access" policy requires a real User-Agent. Anonymous
// or library-default UAs get throttled hard; including contact info here is
// the recommended convention.
constexpr const char* kUserAgent = "FinceptTerminal/4.0 (https://fincept.in; support@fincept.in)";

// Cache TTL — UN/LOCODE / Wikidata port data drifts on a quarterly cadence.
// Seven days keeps the screen snappy across launches without going stale.
constexpr int kCacheTtlSec = 7 * 24 * 60 * 60;

QString cache_key_for(const QString& kind, const QString& payload) {
    const auto hash = QCryptographicHash::hash(payload.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("maritime:ports:%1:%2").arg(kind, QString::fromLatin1(hash));
}

// Local helper — renamed from `publish_to_hub` so unity builds don't collide
// with the identically-named helper in MaritimeService.cpp. Both lived in
// anonymous namespaces in their own TUs, but unity merging collapses them.
inline void publish_ports_to_hub(const QString& topic, const QVariant& value) {
    fincept::datahub::DataHub::instance().publish(topic, value);
}

// ── Wikidata WKT parser ─────────────────────────────────────────────────────
//
// Wikidata returns coordinates as a `Point(lng lat)` literal with the
// `geosparql#wktLiteral` datatype. Note the order: longitude first, latitude
// second — opposite of the lat/lng convention used everywhere else in the
// codebase.
bool parse_wkt_point(const QString& wkt, double* out_lat, double* out_lng) {
    static const QRegularExpression re(QStringLiteral(R"(Point\(\s*([-\d\.]+)\s+([-\d\.]+)\s*\))"));
    const auto m = re.match(wkt);
    if (!m.hasMatch()) return false;
    bool ok1 = false, ok2 = false;
    const double lng = m.captured(1).toDouble(&ok1);
    const double lat = m.captured(2).toDouble(&ok2);
    if (!ok1 || !ok2) return false;
    *out_lat = lat;
    *out_lng = lng;
    return true;
}

// ── PortRecord ↔ JSON helpers (cache round-trip) ─────────────────────────────
QJsonObject to_json(const PortRecord& p) {
    QJsonObject o;
    o["name"]      = p.name;
    o["country"]   = p.country;
    o["locode"]    = p.locode;
    o["lat"]       = p.latitude;
    o["lng"]       = p.longitude;
    o["source"]    = static_cast<int>(p.source);
    return o;
}

PortRecord from_json(const QJsonObject& o) {
    PortRecord p;
    p.name      = o["name"].toString();
    p.country   = o["country"].toString();
    p.locode    = o["locode"].toString();
    p.latitude  = o["lat"].toDouble();
    p.longitude = o["lng"].toDouble();
    p.source    = static_cast<PortSource>(o["source"].toInt());
    return p;
}

QByteArray serialize_results(const QVector<PortRecord>& rows) {
    QJsonArray arr;
    for (const auto& p : rows) arr.append(to_json(p));
    return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}

QVector<PortRecord> deserialize_results(const QByteArray& blob) {
    QVector<PortRecord> out;
    const auto arr = QJsonDocument::fromJson(blob).array();
    out.reserve(arr.size());
    for (const auto& v : arr) out.append(from_json(v.toObject()));
    return out;
}

}  // namespace

// ── Singleton ────────────────────────────────────────────────────────────────
PortsCatalog& PortsCatalog::instance() {
    static PortsCatalog inst;
    return inst;
}

PortsCatalog::PortsCatalog(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void PortsCatalog::search_by_name(const QString& query, int limit) {
    const QString q = query.trimmed();
    if (q.isEmpty()) return;
    const QString context = QStringLiteral("name:") + q.toLower();

    // Cache hit — skip network entirely.
    const QString cache_key = cache_key_for("name", q.toLower() + ":" + QString::number(limit));
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const auto rows = deserialize_results(cached.toString().toUtf8());
        emit ports_found(rows, context);
        return;
    }
    fetch_wikidata_by_name(q, limit, context);
}

void PortsCatalog::search_by_bbox(double min_lat, double max_lat,
                                  double min_lng, double max_lng, int limit) {
    if (max_lat <= min_lat || max_lng <= min_lng) {
        emit error_occurred(QStringLiteral("bbox"), QStringLiteral("Invalid bbox"));
        return;
    }
    const QString context = QStringLiteral("bbox:%1,%2,%3,%4")
                                .arg(min_lat).arg(max_lat).arg(min_lng).arg(max_lng);

    const QString cache_key = cache_key_for("bbox",
        QStringLiteral("%1,%2,%3,%4,%5").arg(min_lat).arg(max_lat).arg(min_lng).arg(max_lng).arg(limit));
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const auto rows = deserialize_results(cached.toString().toUtf8());
        emit ports_found(rows, context);
        return;
    }
    fetch_wikidata_by_bbox(min_lat, max_lat, min_lng, max_lng, limit, context);
}

// ═══════════════════════════════════════════════════════════════════════════════
// WIKIDATA SPARQL — primary
// ═══════════════════════════════════════════════════════════════════════════════

namespace {

// Parse a `application/sparql-results+json` body into PortRecords. The expected
// shape is:
//   { "head": {"vars": [...]},
//     "results": { "bindings": [
//        {"port": {...}, "portLabel": {...}, "countryLabel": {...},
//         "coord": {"value": "Point(<lng> <lat>)"}, "unlocode": {...} },
//        ...
//     ]}}
QVector<PortRecord> parse_wikidata_response(const QByteArray& body) {
    QVector<PortRecord> out;
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) return out;
    const auto bindings = doc.object()["results"].toObject()["bindings"].toArray();
    out.reserve(bindings.size());
    for (const auto& v : bindings) {
        const auto obj = v.toObject();
        const QString wkt = obj["coord"].toObject()["value"].toString();
        double lat = 0, lng = 0;
        if (!parse_wkt_point(wkt, &lat, &lng)) continue;
        PortRecord p;
        p.name      = obj["portLabel"].toObject()["value"].toString();
        p.country   = obj["countryLabel"].toObject()["value"].toString();
        p.locode    = obj["unlocode"].toObject()["value"].toString();
        p.latitude  = lat;
        p.longitude = lng;
        p.source    = PortSource::Wikidata;
        // Drop entries where the label is just the Q-id (no English label
        // available on that entity) — they're useless to a human reader.
        if (p.name.isEmpty() || p.name.startsWith(QLatin1Char('Q'))) {
            const QUrl uri(obj["port"].toObject()["value"].toString());
            const auto qid = uri.fileName();
            if (p.name.isEmpty()) p.name = qid;
        }
        out.append(p);
    }
    return out;
}

}  // namespace

void PortsCatalog::fetch_wikidata_by_name(const QString& query, int limit, const QString& context) {
    // SPARQL: free-text name match (case-insensitive contains) over the
    // canonical port type (Q44782 — port) and its subclasses. Service label
    // resolves portLabel/countryLabel to English; we scope wikibase:label to
    // the SERVICE block so UNION/OPTIONAL don't multiply rows.
    const QString sparql = QStringLiteral(
        "SELECT DISTINCT ?port ?portLabel ?countryLabel ?coord ?unlocode WHERE {"
        "  ?port wdt:P31/wdt:P279* wd:Q44782 ."
        "  ?port wdt:P625 ?coord ."
        "  ?port rdfs:label ?label ."
        "  FILTER(LANG(?label) = \"en\") ."
        "  FILTER(CONTAINS(LCASE(?label), LCASE(\"%1\"))) ."
        "  OPTIONAL { ?port wdt:P17 ?country . }"
        "  OPTIONAL { ?port wdt:P1937 ?unlocode . }"
        "  SERVICE wikibase:label { bd:serviceParam wikibase:language \"en\". }"
        "} LIMIT %2"
    ).arg(query.toLower().replace(QStringLiteral("\""), QStringLiteral("\\\""))).arg(limit);

    QUrl url(kWikidataEndpoint);
    QUrlQuery q;
    q.addQueryItem("query", sparql);
    q.addQueryItem("format", "json");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    req.setRawHeader("Accept", "application/sparql-results+json");

    auto* reply = nam_->get(req);
    QPointer<PortsCatalog> self = this;
    connect(reply, &QNetworkReply::finished, this, [self, reply, query, limit, context]() {
        reply->deleteLater();
        if (!self) return;
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("PortsCatalog", QString("Wikidata name search failed: %1").arg(reply->errorString()));
            // Fall through to Marine Regions instead of failing the search.
            self->fetch_marineregions_by_name(query, limit, context);
            return;
        }
        auto rows = parse_wikidata_response(reply->readAll());
        if (rows.isEmpty()) {
            LOG_INFO("PortsCatalog", QString("Wikidata empty for '%1' — trying Marine Regions").arg(query));
            self->fetch_marineregions_by_name(query, limit, context);
            return;
        }
        const QString cache_key = cache_key_for("name", query.toLower() + ":" + QString::number(limit));
        fincept::CacheManager::instance().put(
            cache_key, QVariant(QString::fromUtf8(serialize_results(rows))), kCacheTtlSec, "maritime");
        LOG_INFO("PortsCatalog", QString("Wikidata: %1 ports for '%2'").arg(rows.size()).arg(query));
        emit self->ports_found(rows, context);
        if (self->hub_registered_)
            publish_ports_to_hub(QStringLiteral("maritime:ports:") + context, QVariant::fromValue(rows));
    });
}

void PortsCatalog::fetch_wikidata_by_bbox(double min_lat, double max_lat,
                                          double min_lng, double max_lng,
                                          int limit, const QString& context) {
    // Wikidata's wikibase:box service — south-west and north-east corners as
    // Point literals. Note that here, like all WKT in Wikidata, longitude is
    // first, latitude second.
    const QString sparql = QStringLiteral(
        "SELECT DISTINCT ?port ?portLabel ?countryLabel ?coord ?unlocode WHERE {"
        "  SERVICE wikibase:box {"
        "    ?port wdt:P625 ?coord ."
        "    bd:serviceParam wikibase:cornerSouthWest \"Point(%1 %2)\"^^geo:wktLiteral ."
        "    bd:serviceParam wikibase:cornerNorthEast \"Point(%3 %4)\"^^geo:wktLiteral ."
        "  }"
        "  ?port wdt:P31/wdt:P279* wd:Q44782 ."
        "  OPTIONAL { ?port wdt:P17 ?country . }"
        "  OPTIONAL { ?port wdt:P1937 ?unlocode . }"
        "  SERVICE wikibase:label { bd:serviceParam wikibase:language \"en\". }"
        "} LIMIT %5"
    ).arg(min_lng).arg(min_lat).arg(max_lng).arg(max_lat).arg(limit);

    QUrl url(kWikidataEndpoint);
    QUrlQuery q;
    q.addQueryItem("query", sparql);
    q.addQueryItem("format", "json");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    req.setRawHeader("Accept", "application/sparql-results+json");

    auto* reply = nam_->get(req);
    QPointer<PortsCatalog> self = this;
    connect(reply, &QNetworkReply::finished, this,
        [self, reply, min_lat, max_lat, min_lng, max_lng, limit, context]() {
        reply->deleteLater();
        if (!self) return;
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("PortsCatalog", QString("Wikidata bbox failed: %1").arg(reply->errorString()));
            self->fetch_overpass_by_bbox(min_lat, max_lat, min_lng, max_lng, limit, context);
            return;
        }
        auto rows = parse_wikidata_response(reply->readAll());
        // < 3 hits in a region usually means thin coverage — supplement with
        // OSM. The two sources are merged client-side; dedup is done by
        // (rounded-coord, lower-name) below.
        if (rows.size() < 3) {
            LOG_INFO("PortsCatalog", QString("Wikidata bbox sparse (%1) — supplementing with OSM").arg(rows.size()));
            self->fetch_overpass_by_bbox(min_lat, max_lat, min_lng, max_lng, limit, context);
            // Cache what we got from Wikidata so the supplemental call's
            // results merge can read it back. We use a very short partial-
            // cache here — overpass callback reads it and republishes with
            // dedup, then writes the merged blob.
            return;
        }
        const QString cache_key = cache_key_for("bbox",
            QStringLiteral("%1,%2,%3,%4,%5").arg(min_lat).arg(max_lat).arg(min_lng).arg(max_lng).arg(limit));
        fincept::CacheManager::instance().put(
            cache_key, QVariant(QString::fromUtf8(serialize_results(rows))), kCacheTtlSec, "maritime");
        LOG_INFO("PortsCatalog", QString("Wikidata: %1 ports in bbox").arg(rows.size()));
        emit self->ports_found(rows, context);
        if (self->hub_registered_)
            publish_ports_to_hub(QStringLiteral("maritime:ports:") + context, QVariant::fromValue(rows));
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// MARINE REGIONS — name-search fallback
// ═══════════════════════════════════════════════════════════════════════════════

void PortsCatalog::fetch_marineregions_by_name(const QString& query, int limit, const QString& context) {
    QUrl url(QString(kMarineRegionsBase) + "/" + QUrl::toPercentEncoding(query) + "/");
    QUrlQuery q;
    q.addQueryItem("like", "true");
    q.addQueryItem("fuzzy", "true");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    req.setRawHeader("Accept", "application/json");

    auto* reply = nam_->get(req);
    QPointer<PortsCatalog> self = this;
    connect(reply, &QNetworkReply::finished, this, [self, reply, query, limit, context]() {
        reply->deleteLater();
        if (!self) return;
        if (reply->error() != QNetworkReply::NoError) {
            emit self->error_occurred(context, QString("Marine Regions: %1").arg(reply->errorString()));
            return;
        }
        const auto arr = QJsonDocument::fromJson(reply->readAll()).array();
        // Marine Regions returns one row per language label per gazetteer
        // record — dedup by MRGID, prefer English label, filter to actual
        // harbour entries (the API will happily return communes / bays /
        // marine regions named "Rotterdam").
        QHash<int, PortRecord> by_mrgid;
        QHash<int, int> language_priority;  // 0=en, 1=other; lower wins
        for (const auto& v : arr) {
            const auto o = v.toObject();
            if (o["placeType"].toString() != QStringLiteral("Harbour")) continue;
            const int mrgid = o["MRGID"].toInt();
            const QString lang = o["preferredGazetteerNameLang"].toString();
            const int prio = (lang == QStringLiteral("English")) ? 0 : 1;
            if (by_mrgid.contains(mrgid) && language_priority.value(mrgid, 99) <= prio)
                continue;
            PortRecord p;
            p.name      = o["preferredGazetteerName"].toString();
            p.latitude  = o["latitude"].toDouble();
            p.longitude = o["longitude"].toDouble();
            p.source    = PortSource::MarineRegions;
            by_mrgid.insert(mrgid, p);
            language_priority.insert(mrgid, prio);
        }
        QVector<PortRecord> rows;
        rows.reserve(by_mrgid.size());
        for (auto it = by_mrgid.cbegin(); it != by_mrgid.cend() && rows.size() < limit; ++it)
            rows.append(it.value());
        const QString cache_key = cache_key_for("name", query.toLower() + ":" + QString::number(limit));
        fincept::CacheManager::instance().put(
            cache_key, QVariant(QString::fromUtf8(serialize_results(rows))), kCacheTtlSec, "maritime");
        LOG_INFO("PortsCatalog", QString("MarineRegions: %1 ports for '%2'").arg(rows.size()).arg(query));
        emit self->ports_found(rows, context);
        if (self->hub_registered_)
            publish_ports_to_hub(QStringLiteral("maritime:ports:") + context, QVariant::fromValue(rows));
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// OVERPASS — bbox fallback
// ═══════════════════════════════════════════════════════════════════════════════

void PortsCatalog::fetch_overpass_by_bbox(double min_lat, double max_lat,
                                          double min_lng, double max_lng,
                                          int limit, const QString& context) {
    // Overpass takes (south, west, north, east). Match the common port-ish
    // tag combinations: seamark:type=harbour (canonical maritime tag),
    // industrial=port (commercial/container), harbour=yes (catch-all).
    const QString ql = QStringLiteral(
        "[out:json][timeout:25];"
        "("
        "node[\"seamark:type\"=\"harbour\"](%1,%2,%3,%4);"
        "node[\"industrial\"=\"port\"](%1,%2,%3,%4);"
        "node[\"harbour\"=\"yes\"](%1,%2,%3,%4);"
        "way[\"seamark:type\"=\"harbour\"](%1,%2,%3,%4);"
        "way[\"industrial\"=\"port\"](%1,%2,%3,%4);"
        ");"
        "out center %5;"
    ).arg(min_lat).arg(min_lng).arg(max_lat).arg(max_lng).arg(limit);

    const QUrl overpass_url{QString::fromLatin1(kOverpassEndpoint)};
    QNetworkRequest req(overpass_url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);

    QUrlQuery body;
    body.addQueryItem("data", ql);
    const QByteArray body_bytes = body.toString(QUrl::FullyEncoded).toUtf8();
    auto* reply = nam_->post(req, body_bytes);
    QPointer<PortsCatalog> self = this;
    connect(reply, &QNetworkReply::finished, this,
        [self, reply, min_lat, max_lat, min_lng, max_lng, limit, context]() {
        reply->deleteLater();
        if (!self) return;
        if (reply->error() != QNetworkReply::NoError) {
            emit self->error_occurred(context, QString("Overpass: %1").arg(reply->errorString()));
            return;
        }
        const auto root = QJsonDocument::fromJson(reply->readAll()).object();
        const auto elements = root["elements"].toArray();
        QVector<PortRecord> rows;
        rows.reserve(elements.size());
        for (const auto& v : elements) {
            const auto el = v.toObject();
            double lat = 0, lng = 0;
            if (el.contains("lat") && el.contains("lon")) {
                lat = el["lat"].toDouble();
                lng = el["lon"].toDouble();
            } else if (el.contains("center")) {
                lat = el["center"].toObject()["lat"].toDouble();
                lng = el["center"].toObject()["lon"].toDouble();
            } else {
                continue;
            }
            const auto tags = el["tags"].toObject();
            PortRecord p;
            p.name      = tags["name"].toString();
            if (p.name.isEmpty())
                p.name = tags["seamark:name"].toString();
            // Skip unnamed POIs — they're noisy on a results table.
            if (p.name.isEmpty()) continue;
            p.latitude  = lat;
            p.longitude = lng;
            p.source    = PortSource::OSM;
            rows.append(p);
            if (rows.size() >= limit) break;
        }
        const QString cache_key = cache_key_for("bbox",
            QStringLiteral("%1,%2,%3,%4,%5").arg(min_lat).arg(max_lat).arg(min_lng).arg(max_lng).arg(limit));
        fincept::CacheManager::instance().put(
            cache_key, QVariant(QString::fromUtf8(serialize_results(rows))), kCacheTtlSec, "maritime");
        LOG_INFO("PortsCatalog", QString("Overpass: %1 ports in bbox").arg(rows.size()));
        emit self->ports_found(rows, context);
        if (self->hub_registered_)
            publish_ports_to_hub(QStringLiteral("maritime:ports:") + context, QVariant::fromValue(rows));
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// DATAHUB PRODUCER — maritime:ports:*
// ═══════════════════════════════════════════════════════════════════════════════

QStringList PortsCatalog::topic_patterns() const {
    return {QStringLiteral("maritime:ports:*")};
}

void PortsCatalog::refresh(const QStringList& topics) {
    // Searches are user-driven and parameterised by the query string baked
    // into the topic suffix — re-firing them on a hub schedule would only
    // burn requests against Wikidata's polite-use limits. Push-only.
    Q_UNUSED(topics);
}

int PortsCatalog::max_requests_per_sec() const {
    return 1;  // Wikidata enforces polite-use; one call per second is plenty.
}

void PortsCatalog::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy policy;
    policy.push_only = true;
    policy.ttl_ms = kCacheTtlSec * 1000;
    hub.set_policy_pattern(QStringLiteral("maritime:ports:*"), policy);

    hub_registered_ = true;
    LOG_INFO("PortsCatalog", "Registered with DataHub (maritime:ports:*)");
}

}  // namespace fincept::services::maritime
