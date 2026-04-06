// src/services/maritime/MaritimeService.cpp
#include "services/maritime/MaritimeService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

namespace fincept::services::maritime {

static constexpr const char* kMarineBase = "https://api.fincept.in/marine";
static constexpr int kVesselTtlSec  = 60;       // position data: 1 min
static constexpr int kHistoryTtlSec = 5 * 60;   // history: 5 min

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

// ── Area search (uses multi-vessel with well-known port IMOs) ────────────────
void MaritimeService::search_vessels_by_area(const AreaSearchParams& params) {
    Q_UNUSED(params);
    // The API doesn't have an area-search endpoint.
    // Use multi-vessel with a set of well-known container ship IMOs instead.
    static const QStringList known_imos = {
        "9893890", // EVER ACE
        "9461867", // APL CHONGQING
        "9811000", // MSC GULSUN
        "9839430", // MSC TESSA
        "9795622", // HMM ALGECIRAS
        "9484525", // COSCO SHIPPING UNIVERSE
        "9706891", // ONE APRICOT
        "9732319", // MAERSK EDINBURGH
        "9758098", // CMA CGM ANTOINE
        "9778791", // EVER GOLDEN
    };
    get_multi_vessel_positions(known_imos);
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
    HttpClient::instance().post(QString(kMarineBase) + "/vessel/position", body, [self, cache_key](Result<QJsonDocument> result) {
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
            cache_key,
            QVariant(QString::fromUtf8(QJsonDocument(vessel_obj).toJson(QJsonDocument::Compact))),
            kVesselTtlSec, "maritime");
        auto vessel = self->parse_vessel(vessel_obj);
        LOG_INFO("Maritime", QString("Found vessel: %1 [%2]").arg(vessel.name, vessel.imo));
        emit self->vessel_found(vessel);
    });
}

// ── Multi vessel positions ───────────────────────────────────────────────────
void MaritimeService::get_multi_vessel_positions(const QStringList& imos) {
    QStringList sorted = imos;
    std::sort(sorted.begin(), sorted.end());
    const QString cache_key = "maritime:multi:" + sorted.join(",");
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        QJsonArray vessels_arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        QVector<VesselData> vessels;
        vessels.reserve(vessels_arr.size());
        for (const auto& v : vessels_arr)
            vessels.append(parse_vessel(v.toObject()));
        emit vessels_loaded(vessels, vessels.size());
        return;
    }

    QJsonObject body;
    QJsonArray arr;
    for (const auto& imo : imos)
        arr.append(imo.trimmed());
    body["imos"] = arr;

    QPointer<MaritimeService> self = this;
    HttpClient::instance().post(QString(kMarineBase) + "/vessel/multi", body, [self, cache_key](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            LOG_ERROR("Maritime", "Multi vessel failed: " + QString::fromStdString(result.error()));
            emit self->error_occurred("multi_vessel", QString::fromStdString(result.error()));
            return;
        }
        auto data = unwrap(result.value().object());
        auto vessels_arr = data["vessels"].toArray();
        fincept::CacheManager::instance().put(
            cache_key,
            QVariant(QString::fromUtf8(QJsonDocument(vessels_arr).toJson(QJsonDocument::Compact))),
            kVesselTtlSec, "maritime");
        QVector<VesselData> vessels;
        vessels.reserve(vessels_arr.size());
        for (const auto& v : vessels_arr)
            vessels.append(self->parse_vessel(v.toObject()));
        int total = data["found_count"].toInt(vessels.size());
        LOG_INFO("Maritime", QString("Multi vessel: %1 found").arg(vessels.size()));
        emit self->vessels_loaded(vessels, total);
    });
}

// ── Vessel history ───────────────────────────────────────────────────────────
void MaritimeService::get_vessel_history(const QString& imo) {
    const QString cache_key = "maritime:history:" + imo.trimmed();
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        QJsonArray history_arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        QVector<VesselData> history;
        history.reserve(history_arr.size());
        for (const auto& v : history_arr)
            history.append(parse_vessel(v.toObject()));
        emit vessel_history_loaded(history);
        return;
    }

    QJsonObject body;
    body["imo"] = imo.trimmed();

    QPointer<MaritimeService> self = this;
    HttpClient::instance().post(QString(kMarineBase) + "/vessel/history", body, [self, cache_key](Result<QJsonDocument> result) {
        if (!self)
            return;
        if (!result.is_ok()) {
            emit self->error_occurred("vessel_history", QString::fromStdString(result.error()));
            return;
        }
        auto data = unwrap(result.value().object());
        auto arr = data["positions"].toArray();
        if (arr.isEmpty())
            arr = data["history"].toArray();
        fincept::CacheManager::instance().put(
            cache_key,
            QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
            kHistoryTtlSec, "maritime");
        QVector<VesselData> history;
        history.reserve(arr.size());
        for (const auto& v : arr)
            history.append(self->parse_vessel(v.toObject()));
        emit self->vessel_history_loaded(history);
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
        emit self->health_loaded(result.value().object());
    });
}

} // namespace fincept::services::maritime
