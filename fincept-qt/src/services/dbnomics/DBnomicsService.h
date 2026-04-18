// src/services/dbnomics/DBnomicsService.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

#include <QHash>
#include <QJsonDocument>
#include <QObject>
#include <QTimer>

#    include "datahub/Producer.h"

namespace fincept::services {

/// Phase 6 — DataHub producer for `dbnomics:<provider>:<dataset>:<series>`
/// observation topics. Provider/dataset/series catalogue endpoints stay
/// signal-driven (structural, not price-like); only observation fetches
/// (the hot path for chart panels) fan out to the hub. Dual-fire — the
/// existing `observations_loaded` signal still emits alongside hub
/// publishes, so panels can migrate incrementally.
class DBnomicsService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static DBnomicsService& instance();

    /// Register with the hub + install dbnomics:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;  // 3 — DBnomics REST

    // ── API methods (all async, result via signals) ───────────────────────────
    void fetch_providers();
    void fetch_datasets(const QString& provider_code, int offset = 0);
    void fetch_series(const QString& provider_code, const QString& dataset_code, const QString& query = {},
                      int offset = 0);
    void fetch_observations(const QString& provider_code, const QString& dataset_code, const QString& series_code);
    void global_search(const QString& query, int offset = 0);

    // ── Debounce entry points (called by UI on each keystroke) ────────────────
    void schedule_series_search(const QString& provider_code, const QString& dataset_code, const QString& query);
    void schedule_global_search(const QString& query);

    // ── Chart color palette ───────────────────────────────────────────────────
    static QColor chart_color(int index);

  signals:
    void providers_loaded(QVector<DbnProvider> providers);
    void datasets_loaded(QVector<DbnDataset> datasets, DbnPagination page, bool append);
    void series_loaded(QVector<DbnSeriesInfo> series, DbnPagination page, bool append);
    void observations_loaded(DbnDataPoint data);
    void search_results_loaded(QVector<DbnSearchResult> results, DbnPagination page, bool append);
    void error_occurred(const QString& context, const QString& message);

  private:
    explicit DBnomicsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(DBnomicsService)

    static constexpr const char* kBaseUrl = "https://api.db.nomics.world/v22";
    static constexpr int kProviderCacheSec = 10 * 60; // 10 min
    static constexpr int kDatasetCacheSec = 5 * 60;   //  5 min
    static constexpr int kSeriesCacheSec = 5 * 60;    //  5 min
    static constexpr int kDebounceMs = 400;

    // ── Debounce timers ───────────────────────────────────────────────────────
    QTimer* series_debounce_ = nullptr;
    QTimer* search_debounce_ = nullptr;
    QString pending_series_prov_;
    QString pending_series_ds_;
    QString pending_series_q_;
    QString pending_search_q_;

    QString build_url(const QString& path) const;

    static QString hub_topic(const QString& provider, const QString& dataset, const QString& series);
    bool hub_registered_ = false;
};

} // namespace fincept::services
