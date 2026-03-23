// src/services/geopolitics/GeopoliticsService.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QHash>
#include <QObject>
#include <functional>

namespace fincept::services::geo {

/// Singleton service for geopolitics data — news events API + HDX Python + trade analysis.
class GeopoliticsService : public QObject {
    Q_OBJECT
  public:
    static GeopoliticsService& instance();

    // ── Conflict Monitor (HTTP API) ─────────────────────────────────────────
    void fetch_events(const QString& country = {}, const QString& city = {},
                      const QString& category = {}, int limit = 100);
    void fetch_unique_countries();
    void fetch_unique_categories();
    void fetch_unique_cities();

    // ── HDX Humanitarian Data (Python) ──────────────────────────────────────
    void search_hdx_conflicts();
    void search_hdx_humanitarian();
    void search_hdx_by_country(const QString& country);
    void search_hdx_by_topic(const QString& topic);
    void search_hdx_advanced(const QString& query);

    // ── Trade Analysis (Python) ─────────────────────────────────────────────
    void analyze_trade_benefits(const QJsonObject& params);
    void analyze_trade_restrictions(const QJsonObject& params);

    // ── Geolocation (Python) ────────────────────────────────────────────────
    void extract_geolocations(const QStringList& headlines);

  signals:
    void events_loaded(QVector<fincept::services::geo::NewsEvent> events, int total);
    void countries_loaded(QVector<fincept::services::geo::UniqueCountry> countries);
    void categories_loaded(QVector<fincept::services::geo::UniqueCategory> categories);
    void cities_loaded(QStringList cities);
    void hdx_results_loaded(QString context, QVector<fincept::services::geo::HDXDataset> datasets);
    void trade_result_ready(QString context, QJsonObject data);
    void geolocation_ready(QJsonObject data);
    void error_occurred(QString context, QString message);

  private:
    explicit GeopoliticsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(GeopoliticsService)

    void run_python(const QString& script, const QStringList& args,
                    const QString& context,
                    std::function<void(bool, const QString&)> cb);

    // Cache
    static constexpr qint64 kEventsTtlSec = 120;
    static constexpr qint64 kRefDataTtlSec = 600;
    struct CachedEvents { QVector<NewsEvent> data; int total = 0; qint64 ts = 0; };
    CachedEvents events_cache_;
    qint64 countries_ts_ = 0;
    qint64 categories_ts_ = 0;
};

} // namespace fincept::services::geo
