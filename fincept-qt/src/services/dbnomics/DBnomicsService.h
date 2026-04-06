// src/services/dbnomics/DBnomicsService.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

#include <QJsonDocument>
#include <QObject>
#include <QTimer>

namespace fincept::services {

class DBnomicsService : public QObject {
    Q_OBJECT
  public:
    static DBnomicsService& instance();

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

    static constexpr const char* kBaseUrl       = "https://api.db.nomics.world/v22";
    static constexpr int kProviderCacheSec = 10 * 60; // 10 min
    static constexpr int kDatasetCacheSec  =  5 * 60; //  5 min
    static constexpr int kSeriesCacheSec   =  5 * 60; //  5 min
    static constexpr int kDebounceMs       = 400;

    // ── Debounce timers ───────────────────────────────────────────────────────
    QTimer* series_debounce_ = nullptr;
    QTimer* search_debounce_ = nullptr;
    QString pending_series_prov_;
    QString pending_series_ds_;
    QString pending_series_q_;
    QString pending_search_q_;

    QString build_url(const QString& path) const;
};

} // namespace fincept::services
