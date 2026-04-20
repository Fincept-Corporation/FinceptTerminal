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

inline QColor category_color(const QString& cat) {
    if (cat == "armed_conflict")
        return QColor(255, 0, 0);
    if (cat == "terrorism")
        return QColor(255, 69, 0);
    if (cat == "protests")
        return QColor(255, 215, 0);
    if (cat == "civilian_violence")
        return QColor(255, 100, 100);
    if (cat == "riots")
        return QColor(255, 165, 0);
    if (cat == "political_violence")
        return QColor(147, 51, 234);
    if (cat == "crisis")
        return QColor(0, 229, 255);
    if (cat == "explosions")
        return QColor(255, 20, 147);
    if (cat == "strategic")
        return QColor(100, 149, 237);
    return QColor(136, 136, 136); // unclassified
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

// ── Event categories ────────────────────────────────────────────────────────

inline QStringList event_categories() {
    return {"armed_conflict",     "terrorism", "protests",   "civilian_violence", "riots",
            "political_violence", "crisis",    "explosions", "strategic"};
}

} // namespace fincept::services::geo

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::geo::NewsEvent)
Q_DECLARE_METATYPE(fincept::services::geo::HDXDataset)
Q_DECLARE_METATYPE(fincept::services::geo::UniqueCountry)
Q_DECLARE_METATYPE(fincept::services::geo::UniqueCategory)
Q_DECLARE_METATYPE(QVector<fincept::services::geo::NewsEvent>)
Q_DECLARE_METATYPE(QVector<fincept::services::geo::HDXDataset>)
Q_DECLARE_METATYPE(QVector<fincept::services::geo::UniqueCountry>)
Q_DECLARE_METATYPE(QVector<fincept::services::geo::UniqueCategory>)
