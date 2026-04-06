#pragma once
#include "core/result/Result.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <functional>

namespace fincept::services {

struct QuoteData {
    QString symbol;
    QString name;
    double price = 0;
    double change = 0;
    double change_pct = 0;
    double high = 0;
    double low = 0;
    double volume = 0;
};

struct InfoData {
    QString symbol;
    QString name;
    QString sector;
    QString industry;
    QString country;
    QString currency;
    double market_cap    = 0;
    double pe_ratio      = 0;
    double forward_pe    = 0;
    double price_to_book = 0;
    double dividend_yield= 0;
    double beta          = 0;
    double week52_high   = 0;
    double week52_low    = 0;
    double avg_volume    = 0;
    double eps           = 0;   // revenuePerShare as proxy
    double roe           = 0;
    double profit_margin = 0;
    double debt_to_equity= 0;
    double current_ratio = 0;
};

struct HistoryPoint {
    qint64 timestamp = 0;  // seconds since epoch
    double open  = 0;
    double high  = 0;
    double low   = 0;
    double close = 0;
    qint64 volume = 0;
};

struct TickerDef {
    QString symbol;
    QString name;
};

struct MarketCategory {
    QString category;
    QStringList tickers;
};

struct RegionalMarket {
    QString region;
    QVector<TickerDef> tickers;
};

/// Fetches market quotes via Python/yfinance.
/// Features:
///   - Request batching: collects symbols over a 100ms window, deduplicates, single Python call
///   - Quote caching: returns cached data immediately, refreshes in background
class MarketDataService : public QObject {
    Q_OBJECT
  public:
    using QuoteCallback = std::function<void(bool, QVector<QuoteData>)>;

    static MarketDataService& instance();

    /// Fetch quotes — batched and cached. Callback receives filtered results for requested symbols.
    void fetch_quotes(const QStringList& symbols, QuoteCallback cb);

    using NewsCallback = std::function<void(bool, QJsonArray)>;
    void fetch_news(const QString& symbol, int count, NewsCallback cb);

    /// Fetch full company info (P/E, 52W range, market cap, ratios, etc.)
    using InfoCallback = std::function<void(bool, InfoData)>;
    void fetch_info(const QString& symbol, InfoCallback cb);

    /// Fetch historical OHLCV data. period: "1mo","3mo","6mo","1y","2y","5y"
    /// interval: "1d","1wk","1mo"
    using HistoryCallback = std::function<void(bool, QVector<HistoryPoint>)>;
    void fetch_history(const QString& symbol, const QString& period,
                       const QString& interval, HistoryCallback cb);

    /// Fetch 5-day hourly sparkline data for multiple symbols in one Python call.
    /// Callback: map of symbol -> list of close prices (chronological).
    using SparklineCallback = std::function<void(bool, QHash<QString, QVector<double>>)>;
    void fetch_sparklines(const QStringList& symbols, SparklineCallback cb);


    static QVector<MarketCategory> default_global_markets();
    static QVector<RegionalMarket> default_regional_markets();

    /// Default symbol lists for dashboard widgets
    static QStringList indices_symbols();
    static QStringList forex_symbols();
    static QStringList crypto_symbols();
    static QStringList commodity_symbols();
    static QStringList mover_symbols();
    static QStringList global_snapshot_symbols();

  private:
    MarketDataService();
    void flush_batch();

    // ── Batching ──
    struct PendingRequest {
        QStringList symbols;
        QuoteCallback cb;
    };
    QVector<PendingRequest> pending_;
    bool batch_scheduled_ = false;

    // ── Caching — delegated to CacheManager ──
    static constexpr int kQuoteCacheTtlSec = 30;
};

} // namespace fincept::services
