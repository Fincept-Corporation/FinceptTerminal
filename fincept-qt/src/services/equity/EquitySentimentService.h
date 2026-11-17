#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QTimer;

namespace fincept::services::equity {

struct LoadFailure; // EquityResearchService.h

// Computes the default, keyless Sentiment-tab content by blending free signals:
//   • News headline sentiment (GNews/yfinance → NewsNlpService, finance-tuned VADER)
//   • Price/technical momentum (the Technicals tab's aggregate indicator rating)
//   • Adanos Market Sentiment — only when an API key is configured
//
// Reuses EquityResearchService's cached fetchers rather than duplicating them;
// resolves the async legs with per-source done flags guarded by a request id
// (so a symbol change mid-flight drops stale callbacks), plus a timeout so a
// dead Python leg never wedges the spinner.
class EquitySentimentService : public QObject {
    Q_OBJECT
  public:
    static EquitySentimentService& instance();

    /// Kick off all source fetches for `symbol` and emit sentiment_loaded once
    /// every awaited leg resolves (or the timeout fires; a leg that lands later for the
    /// same request emits it again). `force` (REFRESH) bypasses the legs' caches.
    void fetch_sentiment(const QString& symbol, bool force = false);

  signals:
    void sentiment_loaded(QString symbol, fincept::services::equity::EquitySentimentSnapshot snapshot);

  private:
    explicit EquitySentimentService(QObject* parent = nullptr);
    Q_DISABLE_COPY(EquitySentimentService)

    void on_news_loaded(const QString& symbol, const QVector<NewsArticle>& articles);
    void on_technicals_loaded(const TechnicalsData& data);
    void on_adanos_loaded(const QString& symbol, const MarketSentimentSnapshot& snapshot);
    /// A failed News / Technicals fetch for the active symbol resolves that leg as unavailable.
    void on_leg_failed(const LoadFailure& failure);
    void try_finalize();
    void finalize();

    // Accumulated state for one contributing signal in the in-flight request.
    struct SourceState {
        bool done = false;
        bool available = false;
        double score = 0.0;      // -1..1
        double confidence = 0.0; // 0..1
    };

    quint64 active_request_id_ = 0;
    QString active_symbol_;
    bool finalized_ = false;

    bool need_adanos_ = false;
    bool news_consumed_ = false;
    bool tech_consumed_ = false;
    bool adanos_consumed_ = false;

    SourceState news_;
    SourceState price_;
    SourceState adanos_;

    int bullish_ = 0;
    int bearish_ = 0;
    int neutral_ = 0;
    QString engine_;
    QVector<ArticleSentiment> articles_;
    QStringList failures_; // "<leg>: <reason>" of the legs that failed in this request

    QTimer* timeout_timer_ = nullptr;
};

} // namespace fincept::services::equity
