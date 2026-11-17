#pragma once
#include "services/news/NewsService.h"
#include "services/news/NewsSignalDetector.h"

#include <QHash>
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
    /// The feed topic (ticker / entity) this market was found for; empty for
    /// the unfiltered list.
    QString matched_term;
};

// ── Service ─────────────────────────────────────────────────────────────────

/// Feed-level intelligence for the News screen's intel drawer.
///
/// * detect_signals() runs in-process (NewsSignalDetector) — no Python.
/// * fetch_predictions() queries the MANIFOLD Markets public API through HttpClient
///   (a play-money site; Polymarket is geo-blocked in places). Results are cached
///   for kPredictionsTtlSec and concurrent requests share one HTTP request.
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

    /// Open binary markets about what the feed is discussing: one Manifold
    /// search per term (the first kMaxPredictionTerms, each cached
    /// kPredictionsTtlSec), keeping only markets whose question names the term.
    /// Each result carries the term it matched; the callback gets an empty list
    /// (ok) when nothing matches — unrelated markets are never shown.
    void fetch_predictions_for(const QStringList& terms, PredictionCallback cb);
    static constexpr int kMaxPredictionTerms = 3;

    /// Human label of where fetch_predictions() data comes from.
    static QString predictions_source_label();

  private:
    NewsCorrelationService();
    struct TermCache {
        QVector<PredictionMarket> markets;
        qint64 fetched_at = 0;
        qint64 failed_at = 0;
        bool inflight = false;
    };
    QHash<QString, TermCache> term_cache_; // lower-cased term -> search result

    static constexpr int kPredictionsTtlSec = 10 * 60;
    static constexpr int kPredictionsRetrySec = 2 * 60;

    QVector<PredictionMarket> predictions_cache_;
    qint64 predictions_fetched_at_ = 0; // unix secs; 0 = never
    qint64 predictions_failed_at_ = 0;  // unix secs of the last failed fetch; 0 = none
    bool predictions_inflight_ = false;
    QVector<PredictionCallback> predictions_waiters_;
};

} // namespace fincept::services
