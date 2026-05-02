// src/services/maritime/MaritimeService.cpp
#include "services/maritime/MaritimeService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

namespace fincept::services::maritime {

namespace {
inline void publish_to_hub(const QString& topic, const QVariant& value) {
    fincept::datahub::DataHub::instance().publish(topic, value);
}
}  // namespace

static constexpr const char* kMarineBase = "https://api.fincept.in/marine";
static constexpr int kVesselTtlSec = 60;      // position data: 1 min
static constexpr int kHistoryTtlSec = 5 * 60; // history: 5 min

// ── Singleton ────────────────────────────────────────────────────────────────
MaritimeService& MaritimeService::instance() {
    static MaritimeService inst;
    return inst;
}

MaritimeService::MaritimeService(QObject* parent) : QObject(parent) {}

// ── Parse vessel from JSON ───────────────────────────────────────────────────
VesselData MaritimeService::parse_vessel(const QJsonObject& obj) const {
    VesselData v;
    v.id = obj["id"].toInt();
    v.imo = obj["imo"].toString();
    v.name = obj["name"].toString();
    v.latitude = obj["last_pos_latitude"].toString().toDouble();
    v.longitude = obj["last_pos_longitude"].toString().toDouble();
    v.speed = obj["last_pos_speed"].toString().toDouble();
    v.angle = obj["last_pos_angle"].toString().toDouble();
    v.from_port = obj["route_from_port_name"].toString();
    v.to_port = obj["route_to_port_name"].toString();
    v.from_date = obj["route_from_date"].toString();
    v.to_date = obj["route_to_date"].toString();
    v.route_progress = obj["route_progress"].toString().toDouble();
    v.draught = obj["current_draught"].toString().toDouble();
    v.last_updated = obj["last_pos_updated_at"].toString();
    v.fetched_at = obj["fetched_at"].toString();
    return v;
}

// ── Helper: unwrap {success, data: {...}} envelope ──────────────────────────
static QJsonObject unwrap(const QJsonObject& root) {
    if (root.contains("data") && root["data"].isObject())
        return root["data"].toObject();
    return root;
}

// ── Area search — POST /marine/vessel/area-search (8 credits/call) ──────────
//
// Returns every vessel whose last known position lies inside the bounding box.
// Costly (8 credits) and unbounded — large boxes can return 10k+ vessels.
// The response shape is:
//   {data: {search_area:{...}, vessels:[...], vessel_count:N,
//           credits_used:N, remaining_credits:N}}

void MaritimeService::search_vessels_by_area(const AreaSearchParams& params) {
    QJsonObject body;
    body["min_lat"] = params.min_lat;
    body["max_lat"] = params.max_lat;
    body["min_lng"] = params.min_lng;
    body["max_lng"] = params.max_lng;
    if (params.days_ago > 0)
        body["days_ago"] = params.days_ago;

    const QString cache_key = QString("maritime:area:%1:%2:%3:%4:%5")
                                  .arg(params.min_lat).arg(params.max_lat)
                                  .arg(params.min_lng).arg(params.max_lng)
                                  .arg(params.days_ago);
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const QJsonObject root = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
        VesselsPage page;
        const QJsonArray arr = root["vessels"].toArray();
        page.vessels.reserve(arr.size());
        for (const auto& v : arr)
            page.vessels.append(parse_vessel(v.toObject()));
        page.total_count       = root["total_count"].toInt(page.vessels.size());
        page.credits_used      = root["credits_used"].toDouble(0.0);
        page.remaining_credits = root["remaining_credits"].toInt(-1);
        emit vessels_loaded(page);
        return;
    }

    QPointer<MaritimeService> self = this;
    HttpClient::instance().post(
        QString(kMarineBase) + "/vessel/area-search", body,
        [self, cache_key](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                LOG_ERROR("Maritime", "Area search failed: " + QString::fromStdString(result.error()));
                emit self->error_occurred("area_search", QString::fromStdString(result.error()));
                return;
            }
            const auto data = unwrap(result.value().object());
            const auto vessels_arr = data["vessels"].toArray();

            VesselsPage page;
            page.vessels.reserve(vessels_arr.size());
            for (const auto& v : vessels_arr)
                page.vessels.append(self->parse_vessel(v.toObject()));

            // Newest position first — last_pos_updated_at is ISO-8601 so plain
            // string compare gives correct lexicographic ordering.
            std::sort(page.vessels.begin(), page.vessels.end(),
                      [](const VesselData& a, const VesselData& b) {
                          return a.last_updated > b.last_updated;
                      });

            page.total_count       = data["vessel_count"].toInt(page.vessels.size());
            page.credits_used      = data["credits_used"].toDouble(0.0);
            page.remaining_credits = data["remaining_credits"].toInt(-1);

            // Cache the (already-sorted) vessels + envelope.
            QJsonArray cached_arr;
            for (const auto& v : page.vessels) {
                QJsonObject o;
                o["id"] = v.id;
                o["imo"] = v.imo;
                o["name"] = v.name;
                o["last_pos_latitude"]  = QString::number(v.latitude, 'f', 8);
                o["last_pos_longitude"] = QString::number(v.longitude, 'f', 8);
                o["last_pos_speed"]     = QString::number(v.speed);
                o["last_pos_angle"]     = QString::number(v.angle);
                o["route_from_port_name"] = v.from_port;
                o["route_to_port_name"]   = v.to_port;
                o["route_from_date"]    = v.from_date;
                o["route_to_date"]      = v.to_date;
                o["route_progress"]     = QString::number(v.route_progress);
                o["current_draught"]    = QString::number(v.draught);
                o["last_pos_updated_at"] = v.last_updated;
                o["fetched_at"]         = v.fetched_at;
                cached_arr.append(o);
            }
            QJsonObject cached_root;
            cached_root["vessels"]           = cached_arr;
            cached_root["total_count"]       = page.total_count;
            cached_root["credits_used"]      = page.credits_used;
            cached_root["remaining_credits"] = page.remaining_credits;
            fincept::CacheManager::instance().put(
                cache_key,
                QVariant(QString::fromUtf8(QJsonDocument(cached_root).toJson(QJsonDocument::Compact))),
                kVesselTtlSec, "maritime");

            LOG_INFO("Maritime", QString("Area search: %1 vessels (total %2, %3 credits left)")
                                     .arg(page.vessels.size())
                                     .arg(page.total_count)
                                     .arg(page.remaining_credits));

            emit self->vessels_loaded(page);
            if (self->hub_registered_)
                publish_to_hub(QStringLiteral("maritime:vessels:area"), QVariant::fromValue(page));
        });
}

// ── Single vessel position ───────────────────────────────────────────────────
void MaritimeService::get_vessel_position(const QString& imo) {
    if (imo.trimmed().isEmpty())
        return;

    const QString cache_key = "maritime:vessel:" + imo.trimmed();
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto vessel = parse_vessel(QJsonDocument::fromJson(cached.toString().toUtf8()).object());
        emit vessel_found(vessel);
        return;
    }

    QJsonObject body;
    body["imo"] = imo.trimmed();

    QPointer<MaritimeService> self = this;
    const QString imo_trimmed = imo.trimmed();
    HttpClient::instance().post(
        QString(kMarineBase) + "/vessel/position", body,
        [self, cache_key, imo_trimmed](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                LOG_ERROR("Maritime", "Vessel position failed: " + QString::fromStdString(result.error()));
                emit self->error_occurred("vessel_position", QString::fromStdString(result.error()));
                return;
            }
            auto data = unwrap(result.value().object());
            auto vessel_obj = data["vessel"].toObject();
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(vessel_obj).toJson(QJsonDocument::Compact))),
                kVesselTtlSec, "maritime");
            auto vessel = self->parse_vessel(vessel_obj);
            LOG_INFO("Maritime", QString("Found vessel: %1 [%2]").arg(vessel.name, vessel.imo));
            emit self->vessel_found(vessel);
            if (self->hub_registered_)
                publish_to_hub(QStringLiteral("maritime:vessel:") + imo_trimmed, QVariant::fromValue(vessel));
        });
}

// ── Multi vessel positions ───────────────────────────────────────────────────
void MaritimeService::get_multi_vessel_positions(const QStringList& imos) {
    QStringList sorted = imos;
    std::sort(sorted.begin(), sorted.end());
    const QString cache_key = "maritime:multi:" + sorted.join(",");
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const QJsonObject root = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
        VesselsPage page;
        const QJsonArray arr = root["vessels"].toArray();
        page.vessels.reserve(arr.size());
        for (const auto& v : arr)
            page.vessels.append(parse_vessel(v.toObject()));
        page.total_count       = root["found_count"].toInt(page.vessels.size());
        page.found_count       = page.total_count;
        for (const auto& nf : root["not_found"].toArray())
            page.not_found.append(nf.toString());
        page.credits_used      = root["credits_used"].toDouble(0.0);
        page.remaining_credits = root["remaining_credits"].toInt(-1);
        emit vessels_loaded(page);
        return;
    }

    QJsonObject body;
    QJsonArray arr;
    for (const auto& imo : imos)
        arr.append(imo.trimmed());
    body["imos"] = arr;

    QPointer<MaritimeService> self = this;
    HttpClient::instance().post(
        QString(kMarineBase) + "/vessel/multi", body, [self, cache_key](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                LOG_ERROR("Maritime", "Multi vessel failed: " + QString::fromStdString(result.error()));
                emit self->error_occurred("multi_vessel", QString::fromStdString(result.error()));
                return;
            }
            const auto data = unwrap(result.value().object());
            const auto vessels_arr = data["vessels"].toArray();

            VesselsPage page;
            page.vessels.reserve(vessels_arr.size());
            for (const auto& v : vessels_arr)
                page.vessels.append(self->parse_vessel(v.toObject()));

            std::sort(page.vessels.begin(), page.vessels.end(),
                      [](const VesselData& a, const VesselData& b) {
                          return a.last_updated > b.last_updated;
                      });

            page.found_count       = data["found_count"].toInt(page.vessels.size());
            page.total_count       = page.found_count;
            for (const auto& nf : data["not_found"].toArray())
                page.not_found.append(nf.toString());
            page.credits_used      = data["credits_used"].toDouble(0.0);
            page.remaining_credits = data["remaining_credits"].toInt(-1);

            // Cache the full envelope so cached replays carry not_found etc.
            QJsonArray cached_arr;
            for (const auto& v : page.vessels) {
                QJsonObject o;
                o["id"] = v.id;
                o["imo"] = v.imo;
                o["name"] = v.name;
                o["last_pos_latitude"]  = QString::number(v.latitude, 'f', 8);
                o["last_pos_longitude"] = QString::number(v.longitude, 'f', 8);
                o["last_pos_speed"]     = QString::number(v.speed);
                o["last_pos_angle"]     = QString::number(v.angle);
                o["route_from_port_name"] = v.from_port;
                o["route_to_port_name"]   = v.to_port;
                o["route_progress"]     = QString::number(v.route_progress);
                o["last_pos_updated_at"] = v.last_updated;
                cached_arr.append(o);
            }
            QJsonObject cached_root;
            cached_root["vessels"]           = cached_arr;
            cached_root["found_count"]       = page.found_count;
            cached_root["not_found"]         = QJsonArray::fromStringList(page.not_found);
            cached_root["credits_used"]      = page.credits_used;
            cached_root["remaining_credits"] = page.remaining_credits;
            fincept::CacheManager::instance().put(
                cache_key,
                QVariant(QString::fromUtf8(QJsonDocument(cached_root).toJson(QJsonDocument::Compact))),
                kVesselTtlSec, "maritime");

            LOG_INFO("Maritime", QString("Multi vessel: %1 found, %2 missing (%3 credits left)")
                                     .arg(page.found_count)
                                     .arg(page.not_found.size())
                                     .arg(page.remaining_credits));

            emit self->vessels_loaded(page);
            if (self->hub_registered_)
                publish_to_hub(QStringLiteral("maritime:vessels:multi"), QVariant::fromValue(page));
        });
}

// ── Vessel history ───────────────────────────────────────────────────────────
void MaritimeService::get_vessel_history(const QString& imo) {
    const QString imo_trimmed = imo.trimmed();
    const QString cache_key = "maritime:history:" + imo_trimmed;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        const QJsonObject root = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
        VesselHistoryPage page;
        page.imo = root["imo"].toString(imo_trimmed);
        const QJsonArray arr = root["history"].toArray();
        page.history.reserve(arr.size());
        for (const auto& v : arr)
            page.history.append(parse_vessel(v.toObject()));
        page.total_records     = root["total_records"].toInt(page.history.size());
        page.credits_used      = root["credits_used"].toDouble(0.0);
        page.remaining_credits = root["remaining_credits"].toInt(-1);
        emit vessel_history_loaded(page);
        return;
    }

    QJsonObject body;
    body["imo"] = imo_trimmed;

    QPointer<MaritimeService> self = this;
    HttpClient::instance().post(
        QString(kMarineBase) + "/vessel/history", body,
        [self, cache_key, imo_trimmed](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                emit self->error_occurred("vessel_history", QString::fromStdString(result.error()));
                return;
            }
            const auto data = unwrap(result.value().object());
            // API returns either "history" (current) or "positions" (legacy).
            QJsonArray arr = data["history"].toArray();
            if (arr.isEmpty())
                arr = data["positions"].toArray();

            VesselHistoryPage page;
            page.imo = data["imo"].toString(imo_trimmed);
            page.history.reserve(arr.size());
            for (const auto& v : arr)
                page.history.append(self->parse_vessel(v.toObject()));

            std::sort(page.history.begin(), page.history.end(),
                      [](const VesselData& a, const VesselData& b) {
                          return a.last_updated > b.last_updated;
                      });

            page.total_records     = data["total_records"].toInt(page.history.size());
            page.credits_used      = data["credits_used"].toDouble(0.0);
            page.remaining_credits = data["remaining_credits"].toInt(-1);

            QJsonArray cached_arr;
            for (const auto& v : page.history) {
                QJsonObject o;
                o["id"] = v.id;
                o["imo"] = v.imo;
                o["name"] = v.name;
                o["last_pos_latitude"]  = QString::number(v.latitude, 'f', 8);
                o["last_pos_longitude"] = QString::number(v.longitude, 'f', 8);
                o["last_pos_speed"]     = QString::number(v.speed);
                o["last_pos_angle"]     = QString::number(v.angle);
                o["route_from_port_name"] = v.from_port;
                o["route_to_port_name"]   = v.to_port;
                o["last_pos_updated_at"] = v.last_updated;
                cached_arr.append(o);
            }
            QJsonObject cached_root;
            cached_root["imo"]               = page.imo;
            cached_root["history"]           = cached_arr;
            cached_root["total_records"]     = page.total_records;
            cached_root["credits_used"]      = page.credits_used;
            cached_root["remaining_credits"] = page.remaining_credits;
            fincept::CacheManager::instance().put(
                cache_key,
                QVariant(QString::fromUtf8(QJsonDocument(cached_root).toJson(QJsonDocument::Compact))),
                kHistoryTtlSec, "maritime");

            LOG_INFO("Maritime", QString("History [%1]: %2 records (total %3, %4 credits left)")
                                     .arg(page.imo)
                                     .arg(page.history.size())
                                     .arg(page.total_records)
                                     .arg(page.remaining_credits));

            emit self->vessel_history_loaded(page);
            if (self->hub_registered_)
                publish_to_hub(QStringLiteral("maritime:history:") + imo_trimmed,
                               QVariant::fromValue(page));
        });
}

// ── Health check ─────────────────────────────────────────────────────────────
void MaritimeService::check_health() {
    QPointer<MaritimeService> self = this;
    HttpClient::instance().get(QString(kMarineBase) + "/health", [self](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            emit self->error_occurred("health", QString::fromStdString(result.error()));
            return;
        }
        const auto obj = result.value().object();
        emit self->health_loaded(obj);
        if (self->hub_registered_)
            publish_to_hub(QStringLiteral("maritime:health"), QVariant(obj));
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// DATAHUB PRODUCER — maritime:*
// ═══════════════════════════════════════════════════════════════════════════════

QStringList MaritimeService::topic_patterns() const {
    return {QStringLiteral("maritime:*")};
}

void MaritimeService::refresh(const QStringList& topics) {
    // Only refresh topics whose key uniquely determines the request. Area /
    // multi-vessel topics depend on user-supplied bbox / IMO list, so the
    // hub doesn't drive their cadence — those calls are user-invoked.
    for (const auto& topic : topics) {
        const QStringList parts = topic.split(QLatin1Char(':'));
        if (parts.size() >= 3 && parts[1] == QStringLiteral("vessel")) {
            get_vessel_position(parts[2]);
        } else if (parts.size() >= 3 && parts[1] == QStringLiteral("history")) {
            get_vessel_history(parts[2]);
        } else if (topic == QStringLiteral("maritime:health")) {
            check_health();
        }
    }
}

int MaritimeService::max_requests_per_sec() const {
    return 2;  // Fincept marine API — conservative
}

void MaritimeService::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // Position data: 1 min TTL matches kVesselTtlSec; min 30s between refreshes.
    fincept::datahub::TopicPolicy vessel_policy;
    vessel_policy.ttl_ms = kVesselTtlSec * 1000;
    vessel_policy.min_interval_ms = 30 * 1000;
    hub.set_policy_pattern(QStringLiteral("maritime:vessel:*"), vessel_policy);
    hub.set_policy_pattern(QStringLiteral("maritime:vessels:*"), vessel_policy);

    // History: 5 min TTL.
    fincept::datahub::TopicPolicy history_policy;
    history_policy.ttl_ms = kHistoryTtlSec * 1000;
    history_policy.min_interval_ms = 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("maritime:history:*"), history_policy);

    // Health: 5 min TTL, non-urgent.
    fincept::datahub::TopicPolicy health_policy;
    health_policy.ttl_ms = 5 * 60 * 1000;
    health_policy.min_interval_ms = 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("maritime:health"), health_policy);

    hub_registered_ = true;
    LOG_INFO("MaritimeService", "Registered with DataHub (maritime:*)");
}

} // namespace fincept::services::maritime
