// src/services/maritime/PortsCatalog.h
#pragma once
#include "datahub/Producer.h"
#include "services/maritime/MaritimePortTypes.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

namespace fincept::services::maritime {

/// Singleton port-directory service.
///
/// Live data sources (no API key, no static bundle):
///   • Wikidata SPARQL          — primary; canonical port set with UN/LOCODE
///   • Marine Regions Gazetteer — name-search fallback when Wikidata is empty
///   • OSM Overpass             — bbox fallback when Wikidata returns < 3 hits
///
/// All requests are asynchronous and post results back through the
/// `ports_found(results, context)` signal where `context` echoes the caller's
/// query string so multiple in-flight searches can be disambiguated.
class PortsCatalog : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static PortsCatalog& instance();

    /// Register with DataHub + install policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ─────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

    /// Free-text name search. Falls back to Marine Regions if Wikidata
    /// returns zero rows. `limit` caps the returned vector size.
    void search_by_name(const QString& query, int limit = 50);

    /// Geographic bbox search. Falls back to Overpass if Wikidata returns
    /// fewer than 3 ports — useful in regions where Wikidata coverage is
    /// thin (e.g. inland rivers, small Pacific atolls).
    void search_by_bbox(double min_lat, double max_lat,
                        double min_lng, double max_lng,
                        int limit = 100);

  signals:
    /// `context` is the original query string ("name:rotterdam" or
    /// "bbox:lat0,lat1,lng0,lng1") so handlers can match results back to
    /// the search that triggered them.
    void ports_found(QVector<fincept::services::maritime::PortRecord> ports, QString context);
    void error_occurred(QString context, QString message);

  private:
    explicit PortsCatalog(QObject* parent = nullptr);
    Q_DISABLE_COPY(PortsCatalog)

    // Underlying fetchers. All are non-blocking; each posts to ports_found
    // with the same `context` string the public API received so the caller
    // never has to disambiguate.
    void fetch_wikidata_by_name(const QString& query, int limit, const QString& context);
    void fetch_wikidata_by_bbox(double min_lat, double max_lat,
                                double min_lng, double max_lng,
                                int limit, const QString& context);
    void fetch_marineregions_by_name(const QString& query, int limit, const QString& context);
    void fetch_overpass_by_bbox(double min_lat, double max_lat,
                                double min_lng, double max_lng,
                                int limit, const QString& context);

    QNetworkAccessManager* nam_ = nullptr;
    bool hub_registered_ = false;
};

} // namespace fincept::services::maritime
