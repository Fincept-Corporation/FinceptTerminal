// src/services/maritime/MaritimeTypes.h
#pragma once
#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::maritime {

// ── Vessel data from API ────────────────────────────────────────────────────

struct VesselData {
    int id = 0;
    QString imo;
    QString name;
    double latitude = 0;
    double longitude = 0;
    double speed = 0;
    double angle = 0;
    QString from_port;
    QString to_port;
    QString from_date;
    QString to_date;
    double route_progress = 0;
    double draught = 0;
    QString last_updated;
    QString fetched_at;
};

// ── Trade route corridor ────────────────────────────────────────────────────

struct TradeRoute {
    QString name;
    QString value;          // e.g. "$45B"
    QString status;         // active, delayed, critical
    int vessels = 0;
    double start_lat = 0, start_lng = 0;
    double end_lat = 0, end_lng = 0;
};

inline QColor route_status_color(const QString& status) {
    if (status == "critical") return QColor("#FF0000");
    if (status == "delayed")  return QColor("#FFD700");
    return QColor("#00FF00"); // active
}

// ── Intelligence stats ──────────────────────────────────────────────────────

struct IntelligenceData {
    QString threat_level;  // low, medium, high, critical
    int active_vessels = 0;
    int monitored_routes = 48;
    QString trade_volume = "$847.3B";
};

inline QColor threat_color(const QString& level) {
    if (level == "critical") return QColor("#FF0000");
    if (level == "high")     return QColor("#FF6600");
    if (level == "medium")   return QColor("#FFD700");
    return QColor("#00FF00"); // low
}

// ── Preset port locations ───────────────────────────────────────────────────

struct PresetPort {
    QString name;
    double lat;
    double lng;
};

inline QVector<PresetPort> preset_ports() {
    return {
        {"Mumbai Port",    18.9388, 72.8354},
        {"Shanghai Port",  31.3548, 121.6431},
        {"Singapore Port", 1.2644,  103.8224},
        {"Hong Kong Port", 22.2855, 114.1577},
        {"Rotterdam Port", 51.9553, 4.1392},
        {"Dubai Port",     24.9857, 55.0272},
    };
}

// ── Default trade routes ────────────────────────────────────────────────────

inline QVector<TradeRoute> default_trade_routes() {
    return {
        {"Mumbai - Rotterdam",   "$45B",  "active",   23, 18.94, 72.84, 51.96, 4.14},
        {"Mumbai - Shanghai",    "$156B", "active",   89, 18.94, 72.84, 31.35, 121.64},
        {"Mumbai - Singapore",   "$89B",  "active",   45, 18.94, 72.84, 1.26,  103.82},
        {"Chennai - Tokyo",      "$67B",  "delayed",  34, 13.08, 80.27, 35.65, 139.84},
        {"Kolkata - Hong Kong",  "$45B",  "active",   28, 22.57, 88.37, 22.29, 114.16},
        {"Mumbai - Dubai",       "$78B",  "critical", 12, 18.94, 72.84, 24.99, 55.03},
        {"Mumbai - New York",    "$123B", "active",   56, 18.94, 72.84, 40.68, -74.04},
        {"Chennai - Sydney",     "$54B",  "active",   31, 13.08, 80.27, -33.86, 151.21},
        {"Cochin - Port Klang",  "$34B",  "active",   19, 9.97,  76.27, 3.00,  101.39},
        {"Mumbai - Cape Town",   "$28B",  "delayed",  8,  18.94, 72.84, -33.92, 18.42},
    };
}

// ── Area search params ──────────────────────────────────────────────────────

struct AreaSearchParams {
    double min_lat = 18.5;
    double max_lat = 19.5;
    double min_lng = 72.0;
    double max_lng = 73.5;
    int days_ago = 0;
};

} // namespace fincept::services::maritime
