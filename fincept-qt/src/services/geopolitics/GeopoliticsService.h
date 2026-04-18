// src/services/geopolitics/GeopoliticsService.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QHash>
#include <QObject>

#include <functional>

#    include "datahub/Producer.h"

namespace fincept::services::geo {

/// Singleton service for geopolitics data — news events API + HDX Python + trade analysis.
class GeopoliticsService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static GeopoliticsService& instance();

    /// Register with the hub + install geopolitics:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ─────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

    // ── Conflict Monitor (HTTP API) ─────────────────────────────────────────
    void fetch_events(const QString& country = {}, const QString& city = {}, const QString& category = {},
                      int limit = 100);
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

    void run_python(const QString& script, const QStringList& args, const QString& context,
                    std::function<void(bool, const QString&)> cb);

    // Cache TTLs (used as CacheManager ttl_seconds)
    static constexpr int kEventsTtlSec = 120;
    static constexpr int kRefDataTtlSec = 600;

    void publish_hdx_result(const QString& context, const QVector<fincept::services::geo::HDXDataset>& datasets);
    bool hub_registered_ = false;
};

} // namespace fincept::services::geo
