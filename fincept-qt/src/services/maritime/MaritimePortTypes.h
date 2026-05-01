// src/services/maritime/MaritimePortTypes.h
#pragma once
#include <QMetaType>
#include <QString>
#include <QVector>

namespace fincept::services::maritime {

/// Where a port record came from. Used by the UI to display a small badge so
/// the user can tell that, e.g., a hit with no UN/LOCODE came from OSM rather
/// than the canonical Wikidata catalogue.
enum class PortSource {
    Unknown = 0,
    Wikidata,
    MarineRegions,
    OSM,
};

inline QString port_source_name(PortSource s) {
    switch (s) {
        case PortSource::Wikidata:      return QStringLiteral("Wikidata");
        case PortSource::MarineRegions: return QStringLiteral("MarineRegions");
        case PortSource::OSM:           return QStringLiteral("OSM");
        default:                        return QStringLiteral("?");
    }
}

/// One port record. `locode` is the 5-letter UN/LOCODE (e.g. "SGSIN") and is
/// optional — Marine Regions and OSM never provide it. `country` is also
/// optional for the same reason.
struct PortRecord {
    QString name;
    QString country;
    QString locode;
    double  latitude  = 0.0;
    double  longitude = 0.0;
    PortSource source = PortSource::Unknown;
};

} // namespace fincept::services::maritime

Q_DECLARE_METATYPE(fincept::services::maritime::PortRecord)
Q_DECLARE_METATYPE(QVector<fincept::services::maritime::PortRecord>)
