#pragma once
#include "services/news/NewsService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services {

// ── Signal Types ────────────────────────────────────────────────────────────

struct CorrelationSignal {
    QString type; // velocity_spike, keyword_spike, triangulation, etc.
    QString category;
    QString detail;
    QString severity; // critical, high, medium, low
    double value = 0;
    QStringList sources;
};

struct InstabilityScore {
    QString country;
    int cii_score = 0; // 0-100
    QString level;     // CRITICAL, HIGH, ELEVATED, STABLE
    int baseline = 0;
    QMap<QString, double> signal_contributions;
};

struct FocalPoint {
    double lat = 0;
    double lon = 0;
    int event_count = 0;
    QMap<QString, int> categories;
    int source_count = 0;
    QString severity;
    QStringList headlines;
};

struct PredictionMarket {
    QString id;
    QString question;
    double yes_price = 0;
    double no_price = 0;
    double volume = 0;
};

struct CategoryBaseline {
    QVector<int> hourly_counts;
    double mean = 0;
    double stddev = 0;
    int sample_size = 0;
};

// ── Service ─────────────────────────────────────────────────────────────────

class NewsCorrelationService : public QObject {
    Q_OBJECT
  public:
    using SignalsCallback = std::function<void(bool, QVector<CorrelationSignal>)>;
    using InstabilityCallback = std::function<void(bool, InstabilityScore)>;
    using FocalCallback = std::function<void(bool, QVector<FocalPoint>)>;
    using PredictionCallback = std::function<void(bool, QVector<PredictionMarket>)>;
    using BaselineCallback = std::function<void(bool, QMap<QString, CategoryBaseline>)>;
    using DeviationCallback = std::function<void(bool, QVector<QPair<QString, double>>)>;

    static NewsCorrelationService& instance();

    /// Detect correlation signals from articles.
    void detect_signals(const QVector<NewsArticle>& articles, SignalsCallback cb);

    /// Compute Country Instability Index for a country.
    void compute_instability(const QString& country_code, const QVector<CorrelationSignal>& sigs,
                             InstabilityCallback cb);

    /// Detect geographic focal points from geolocated articles.
    void detect_focal_points(const QJsonArray& geolocated_articles, FocalCallback cb);

    /// Fetch prediction market odds.
    void fetch_predictions(PredictionCallback cb);

    /// Baseline management (persist to SQLite via Python).
    void update_baseline(const QMap<QString, int>& current_counts, BaselineCallback cb);
    void detect_deviations(const QMap<QString, int>& current_counts, DeviationCallback cb);

    /// Cached data.
    const QVector<CorrelationSignal>& cached_signals() const { return signals_cache_; }
    const QVector<PredictionMarket>& cached_predictions() const { return predictions_cache_; }

  private:
    NewsCorrelationService();

    QVector<CorrelationSignal> signals_cache_;
    QVector<PredictionMarket> predictions_cache_;
    QMap<QString, CategoryBaseline> baselines_;
};

} // namespace fincept::services
