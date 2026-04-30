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

// ── Trade route corridor (derived from current vessel set, not hardcoded) ──
//
// Built at runtime by aggregating loaded vessels on (from_port, to_port).
// `value` is left empty since the API does not expose trade-volume figures —
// the column is kept for future enrichment.

struct TradeRoute {
    QString name;
    QString value;
    QString status;  // active, delayed, critical (left empty when not derivable)
    int vessels = 0;
    double start_lat = 0, start_lng = 0;
    double end_lat = 0, end_lng = 0;
};

inline QColor route_status_color(const QString& status) {
    if (status == QStringLiteral("critical"))
        return QColor("#FF0000");
    if (status == QStringLiteral("delayed"))
        return QColor("#FFD700");
    if (status == QStringLiteral("active"))
        return QColor("#00FF00");
    return QColor("#888888");
}

// ── Page envelope for area-search / multi-vessel responses ──────────────────
//
// Wraps the parsed vessel list with the new credit metering + result counts
// the API returns. `not_found` is populated by multi-vessel when caller
// requested IMOs that aren't in the database.

struct VesselsPage {
    QVector<VesselData> vessels;
    int total_count = 0;       // server-reported total ("vessel_count" / "found_count")
    int found_count = 0;       // multi-vessel only
    QStringList not_found;     // multi-vessel only — IMOs missing from DB
    double credits_used = 0.0;
    int remaining_credits = -1; // -1 = unknown / not reported
};

struct VesselHistoryPage {
    QString imo;
    QVector<VesselData> history; // sorted newest-first
    int total_records = 0;
    double credits_used = 0.0;
    int remaining_credits = -1;
};

// ── Area search params ──────────────────────────────────────────────────────

struct AreaSearchParams {
    double min_lat = 0;
    double max_lat = 0;
    double min_lng = 0;
    double max_lng = 0;
    int days_ago = 0;
};

} // namespace fincept::services::maritime

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::maritime::VesselData)
Q_DECLARE_METATYPE(fincept::services::maritime::VesselsPage)
Q_DECLARE_METATYPE(fincept::services::maritime::VesselHistoryPage)
Q_DECLARE_METATYPE(QVector<fincept::services::maritime::VesselData>)
