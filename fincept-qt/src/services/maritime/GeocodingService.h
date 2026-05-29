// src/services/maritime/GeocodingService.h
#pragma once
#include "services/maritime/GeocodingTypes.h"

#include <QObject>
#include <QString>
#include <QVector>

class QNetworkAccessManager;

namespace fincept::services::maritime {

/// Singleton place-name geocoder backed by OpenStreetMap Nominatim.
///
/// One-shot, user-initiated lookup (not a stream): the user types a place
/// name, presses Search, and picks a result whose bounding box drives the
/// maritime area-search. Per the DataHub D4 rule, one-shot catalog-style
/// lookups stay as callback/signal fetches rather than hub topics.
///
/// Mirrors PortsCatalog: async HTTP via its own QNetworkAccessManager, results
/// posted back through `places_found(results, context)` where `context` echoes
/// the original query so concurrent searches can be disambiguated.
class GeocodingService : public QObject {
    Q_OBJECT
  public:
    static GeocodingService& instance();

    /// Free-text place search. `limit` caps the number of suggestions.
    void search(const QString& query, int limit = 8);

  signals:
    /// `context` is the original query string so handlers can match results
    /// back to the search that triggered them.
    void places_found(QVector<fincept::services::maritime::GeoPlace> places, QString context);
    void error_occurred(QString context, QString message);

  private:
    explicit GeocodingService(QObject* parent = nullptr);
    Q_DISABLE_COPY(GeocodingService)

    QNetworkAccessManager* nam_ = nullptr;
};

} // namespace fincept::services::maritime
