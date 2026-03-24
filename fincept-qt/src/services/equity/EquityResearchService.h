// src/services/equity/EquityResearchService.h
#pragma once
#include "services/equity/EquityResearchModels.h"

#include <QHash>
#include <QObject>
#include <QTimer>

#include <functional>

namespace fincept::services::equity {

class EquityResearchService : public QObject {
    Q_OBJECT
  public:
    static EquityResearchService& instance();

    // ── Async API — results delivered via signals ─────────────────────────────
    void search_symbols(const QString& query);
    void schedule_search(const QString& query); // debounced entry point

    /// Fetches quote + info + historical in parallel (three Python calls)
    void load_symbol(const QString& symbol, const QString& period = "1y");

    void fetch_financials(const QString& symbol);
    void fetch_technicals(const QString& symbol, const QString& period = "1y");
    void fetch_peers(const QString& symbol, const QStringList& peer_symbols);
    void fetch_news(const QString& symbol, int count = 20);

    /// TALIpp tab: fetch historical then run equity_talipp.py
    void compute_talipp(const QString& symbol, const QString& indicator, const QVariantMap& params,
                        const QString& period = "2y");

  signals:
    void search_results_loaded(QVector<fincept::services::equity::SearchResult> results);
    void quote_loaded(fincept::services::equity::QuoteData quote);
    void info_loaded(fincept::services::equity::StockInfo info);
    void historical_loaded(QString symbol, QVector<fincept::services::equity::Candle> candles);
    void financials_loaded(fincept::services::equity::FinancialsData data);
    void technicals_loaded(fincept::services::equity::TechnicalsData data);
    void peers_loaded(QVector<fincept::services::equity::PeerData> peers);
    void news_loaded(QString symbol, QVector<fincept::services::equity::NewsArticle> articles);
    void talipp_result(QString indicator, QVector<double> values, QVector<qint64> timestamps);
    void error_occurred(QString context, QString message);

  private:
    explicit EquityResearchService(QObject* parent = nullptr);
    Q_DISABLE_COPY(EquityResearchService)

    // ── Python helpers ────────────────────────────────────────────────────────
    void run_python(const QString& script, const QStringList& args, std::function<void(bool, const QString&)> cb);

    // ── Parsers ───────────────────────────────────────────────────────────────
    QuoteData parse_quote(const QJsonObject& obj) const;
    StockInfo parse_info(const QJsonObject& obj) const;
    QVector<Candle> parse_candles(const QJsonArray& arr) const;
    FinancialsData parse_financials(const QJsonObject& obj) const;
    TechnicalsData parse_technicals(const QString& symbol, const QJsonArray& rows) const;
    TechSignal score_indicator(const QString& name, double value, double sma20, double sma50) const;
    QVector<PeerData> parse_peers(const QJsonArray& arr) const;
    QVector<NewsArticle> parse_news(const QJsonArray& arr) const;

    // ── Cache structures ──────────────────────────────────────────────────────
    static constexpr qint64 kQuoteTtlSec = 30;
    static constexpr qint64 kInfoTtlSec = 300;
    static constexpr qint64 kHistoricalTtlSec = 120;
    static constexpr qint64 kNewsTtlSec = 180;

    struct CachedQuote {
        QuoteData data;
        qint64 ts = 0;
    };
    struct CachedInfo {
        StockInfo data;
        qint64 ts = 0;
    };
    struct CachedCandles {
        QVector<Candle> data;
        qint64 ts = 0;
    };
    struct CachedNews {
        QVector<NewsArticle> data;
        qint64 ts = 0;
    };

    QHash<QString, CachedQuote> quote_cache_;
    QHash<QString, CachedInfo> info_cache_;
    QHash<QString, CachedCandles> candle_cache_;
    QHash<QString, CachedNews> news_cache_;

    // ── Debounce ──────────────────────────────────────────────────────────────
    static constexpr int kDebounceMs = 350;
    QTimer* search_debounce_ = nullptr;
    QString pending_query_;
};

} // namespace fincept::services::equity
