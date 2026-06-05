// src/trading/HistoricalDataService.h
#pragma once
// HistoricalDataService — one place for broker OHLCV history.
//
// Both the equity chart (AccountDataStream::fetch_candles) and the algo engine
// (CandleDataFetcher) need historical candles from the connected broker. This
// service unifies that path: broker symbol resolution (token brokers), the
// get_history call with a bare-symbol fallback, and a short TTL cache so the
// same series isn't fetched twice when a chart + an algo open it together.
//
// It returns the trading-layer BrokerCandle. The algo side converts to its own
// OhlcvCandle and layers a Yahoo-Finance fallback on top (in CandleDataFetcher)
// for symbols/brokers the broker history endpoint can't serve.
#include "trading/TradingTypes.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::trading {

class HistoricalDataService : public QObject {
    Q_OBJECT
  public:
    static HistoricalDataService& instance();

    using Callback =
        std::function<void(bool ok, const QVector<BrokerCandle>& candles, const QString& error)>;

    // Fetch `lookback_days` of OHLCV history for `symbol` at `timeframe`
    // ("1m"/"3m"/"5m"/"15m"/"30m"/"1h"/"4h"/"1d") from `broker_id`/`account_id`.
    // Served from a short TTL cache when a fresh copy exists. Must be called on
    // the main thread (loads credentials there for SQLite thread-safety);
    // `callback` is invoked on the main thread.
    void fetch(const QString& symbol, const QString& timeframe, int lookback_days,
               const QString& broker_id, const QString& account_id, Callback callback);

  private:
    HistoricalDataService() = default;
    Q_DISABLE_COPY(HistoricalDataService)

    struct CacheEntry {
        qint64 fetched_ms = 0;
        QVector<BrokerCandle> candles;
    };
    QHash<QString, CacheEntry> cache_; // key: broker|symbol|tf|lookback
};

} // namespace fincept::trading
