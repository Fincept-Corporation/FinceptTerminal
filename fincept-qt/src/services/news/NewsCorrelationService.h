#pragma once
#include "services/news/NewsService.h"
#include "services/news/NewsSignalDetector.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services {

// ── Prediction-market odds ─────────────────────────────────────────────────

/// One binary market. `yes_price` is the implied YES probability in [0, 1].
struct PredictionMarket {
    QString id;
    QString question;
    QString url;
    double yes_price = 0;
    double no_price = 0;
    double volume = 0;
};

// ── Service ─────────────────────────────────────────────────────────────────

/// Feed-level intelligence for the News screen's intel drawer.
///
/// * detect_signals() runs in-process (NewsSignalDetector) — no Python.
/// * fetch_predictions() reads scripts/polymarket.py, which serves MANIFOLD
///   Markets (a play-money site; Polymarket is geo-blocked in places). Results are
///   cached for kPredictionsTtlSec and concurrent requests share one process.
class NewsCorrelationService : public QObject {
    Q_OBJECT
  public:
    using SignalsCallback = std::function<void(bool, QVector<CorrelationSignal>)>;
    using PredictionCallback = std::function<void(bool, QVector<PredictionMarket>)>;

    static NewsCorrelationService& instance();

    /// Detect correlation signals over `articles` (synchronous; the callback runs
    /// before this returns).
    void detect_signals(const QVector<NewsArticle>& articles, const SignalsCallback& cb);

    /// Prediction-market odds (Manifold, play money). Served from cache when fresh.
    void fetch_predictions(PredictionCallback cb);

    /// Human label of where fetch_predictions() data comes from.
    static QString predictions_source_label();

  private:
    NewsCorrelationService();

    static constexpr int kPredictionsTtlSec = 10 * 60;
    static constexpr int kPredictionsRetrySec = 2 * 60;

    QVector<PredictionMarket> predictions_cache_;
    qint64 predictions_fetched_at_ = 0; // unix secs; 0 = never
    qint64 predictions_failed_at_ = 0;  // unix secs of the last failed fetch; 0 = none
    bool predictions_inflight_ = false;
    QVector<PredictionCallback> predictions_waiters_;
};

} // namespace fincept::services
