// src/services/maritime/GeocodingTypes.h
#pragma once
#include <QMetaType>
#include <QString>
#include <QVector>

namespace fincept::services::maritime {

/// One geocoding result from a place-name search. `bbox` is the suggested
/// area extent (min/max lat/lng) that the maritime area-search uses to load
/// vessels. Nominatim returns this as its `boundingbox` field; for points
/// without a real extent it is a small box centred on (latitude, longitude).
struct GeoPlace {
    QString display_name;  // human-readable, e.g. "Singapore Strait, Singapore"
    QString type;          // OSM class/type hint, e.g. "strait", "bay", "city"
    double  latitude  = 0.0;
    double  longitude = 0.0;
    double  min_lat   = 0.0;
    double  max_lat   = 0.0;
    double  min_lng   = 0.0;
    double  max_lng   = 0.0;
};

} // namespace fincept::services::maritime

Q_DECLARE_METATYPE(fincept::services::maritime::GeoPlace)
Q_DECLARE_METATYPE(QVector<fincept::services::maritime::GeoPlace>)
