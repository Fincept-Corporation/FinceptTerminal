#pragma once
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QVector>
#include <QHash>
#include <QDateTime>
#include <functional>
#include "core/result/Result.h"

namespace fincept::services {

struct QuoteData {
    QString symbol;
    QString name;
    double  price = 0;
    double  change = 0;
    double  change_pct = 0;
    double  high = 0;
    double  low = 0;
    double  volume = 0;
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

    /// Cache TTL in seconds (default 30s)
    void set_cache_ttl(int seconds) { cache_ttl_sec_ = seconds; }

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

    // ── Caching ──
    struct CachedQuote {
        QuoteData data;
        qint64 timestamp = 0;  // seconds since epoch
    };
    QHash<QString, CachedQuote> cache_;
    int cache_ttl_sec_ = 30;
};

} // namespace fincept::services
