// src/services/geopolitics/GeopoliticsTypes.h
#pragma once
#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::services::geo {

// ── News event from conflict monitor ────────────────────────────────────────

struct NewsEvent {
    QString url;
    QString source;
    QString event_category;
    QString title;
    QString city;
    QString country;
    double latitude = 0;
    double longitude = 0;
    bool has_coords = false;
    QString extracted_date;
    QString created_at;
};

/// Convert an API category id ("armed_conflict_unclassified") into a display
/// label ("Armed Conflict (Unclassified)"). Used everywhere the category is
/// shown to the user — table cells, detail panel, legend, dropdown.
inline QString pretty_category(const QString& cat) {
    QString base = cat;
    bool unclassified = false;
    if (base.endsWith(QStringLiteral("_unclassified"))) {
        base.chop(13);
        unclassified = true;
    }
    QStringList parts = base.split(QLatin1Char('_'), Qt::SkipEmptyParts);
    for (auto& p : parts)
        if (!p.isEmpty()) p[0] = p[0].toUpper();
    QString out = parts.join(QLatin1Char(' '));
    if (unclassified) out += QStringLiteral(" (Unclassified)");
    return out;
}

inline QColor category_color(const QString& cat) {
    // API returns *_unclassified variants — map them to the same colour as the
    // base category so the legend / map / table stay coherent.
    QString base = cat;
    if (base.endsWith(QStringLiteral("_unclassified")))
        base.chop(13);

    if (base == QStringLiteral("armed_conflict"))     return QColor(255,   0,   0);
    if (base == QStringLiteral("terrorism"))          return QColor(255,  69,   0);
    if (base == QStringLiteral("protests"))           return QColor(255, 215,   0);
    if (base == QStringLiteral("civilian_violence"))  return QColor(255, 100, 100);
    if (base == QStringLiteral("riots"))              return QColor(255, 165,   0);
    if (base == QStringLiteral("political_violence")) return QColor(147,  51, 234);
    if (base == QStringLiteral("crisis"))             return QColor(  0, 229, 255);
    if (base == QStringLiteral("explosions"))         return QColor(255,  20, 147);
    if (base == QStringLiteral("strategic"))          return QColor(100, 149, 237);
    if (base == QStringLiteral("diplomacy") ||
        base == QStringLiteral("diplomatic"))         return QColor(167, 139, 250);
    if (base == QStringLiteral("maritime"))           return QColor( 56, 189, 248);
    if (base == QStringLiteral("elections"))          return QColor( 34, 197,  94);
    if (base == QStringLiteral("sanctions"))          return QColor(251, 146,  60);
    if (base == QStringLiteral("nuclear"))            return QColor(250, 204,  21);
    if (base == QStringLiteral("cyber"))              return QColor( 20, 184, 166);
    if (base == QStringLiteral("not_geopol"))         return QColor(100, 100, 100);
    return QColor(136, 136, 136);
}

// ── HDX dataset ─────────────────────────────────────────────────────────────

struct HDXDataset {
    QString id;
    QString title;
    QString organization;
    QString notes;
    QString date;
    int num_resources = 0;
    QStringList tags;
    QJsonObject raw;
};

// ── Reference data ──────────────────────────────────────────────────────────

struct UniqueCountry {
    QString country;
    int event_count = 0;
};

struct UniqueCategory {
    QString category;
    int event_count = 0;
};

// ── Events page (response envelope from /research/news-events) ──────────────
//
// The Fincept API now wraps every events response with pagination and credit
// metering. Consumers should treat this struct as the unit of delivery instead
// of separate (events, total) tuples.

struct EventsPage {
    QVector<NewsEvent> events;
    int total_events = 0;
    int current_page = 1;
    int total_pages = 0;
    int events_per_page = 0;
    bool has_next = false;
    bool has_prev = false;
    double credits_used = 0.0;
    int remaining_credits = -1;  // -1 = unknown / not reported
};

// ── Relationship map node ───────────────────────────────────────────────────

struct RelationshipNode {
    QString id;
    QString label;
    QString type;     // country, conflict, organization, crisis
    QString severity; // low, medium, high, critical
    int dataset_count = 0;
    QStringList connections;
};

// ── Critical regions for HDX monitoring ─────────────────────────────────────

inline QStringList critical_regions() {
    return {"Ukraine", "Gaza",     "Sudan", "Yemen",   "Syria",    "Afghanistan",
            "Myanmar", "Ethiopia", "Haiti", "Somalia", "Venezuela"};
}

} // namespace fincept::services::geo

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::geo::NewsEvent)
Q_DECLARE_METATYPE(fincept::services::geo::HDXDataset)
Q_DECLARE_METATYPE(fincept::services::geo::UniqueCountry)
Q_DECLARE_METATYPE(fincept::services::geo::UniqueCategory)
Q_DECLARE_METATYPE(fincept::services::geo::EventsPage)
Q_DECLARE_METATYPE(QVector<fincept::services::geo::NewsEvent>)
Q_DECLARE_METATYPE(QVector<fincept::services::geo::HDXDataset>)
Q_DECLARE_METATYPE(QVector<fincept::services::geo::UniqueCountry>)
Q_DECLARE_METATYPE(QVector<fincept::services::geo::UniqueCategory>)
