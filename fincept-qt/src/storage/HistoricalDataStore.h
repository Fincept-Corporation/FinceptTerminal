#pragma once
// HistoricalDataStore — persistent historical OHLCV+OI data store (Historify).
//
// Phase 3 §13. Port of OpenAlgo's Historify (services/historify_service.py +
// database/historify_db.py) onto Fincept's EXISTING SQLite stack.
//
// DECISION — SQLite, not DuckDB:
//   OpenAlgo's Historify uses DuckDB (columnar OLAP). Fincept has NO DuckDB
//   dependency and no vcpkg/conan; adding DuckDB via FetchContent is too heavy
//   and risky for this pass. The Phase 3 doc explicitly lists SQLite as the
//   acceptable fallback (Option B). This store therefore lives on the same
//   SQLite database as every other repository (see migration v033_historify).
//
//   TODO(perf): for very large-scale OLAP / multi-year backtests over millions
//   of rows, a DuckDB-backed store (or a DuckDB read replica fed from this
//   SQLite table) would be materially faster for columnar aggregation. Revisit
//   if/when the project gains a package manager and the row counts justify it.
//
// Conventions:
//   - namespace fincept::storage (matches the src/storage/ directory).
//   - Timestamps are epoch MILLISECONDS throughout (trading::BrokerCandle uses
//     int64 ms). OpenAlgo uses epoch seconds; this store standardises on ms.
//   - Schema lives in migration v033_historify.cpp.
//   - Singleton, accessed on the main thread. Uses Database::instance() which
//     hands out a per-thread connection, so background callers are tolerated.

#include "trading/TradingTypes.h"

#include <QString>
#include <QVector>

#include <cstdint>

namespace fincept::storage {

class HistoricalDataStore {
  public:
    static HistoricalDataStore& instance();

    // ── OHLCV storage ────────────────────────────────────────────────────────

    /// Upsert candles for (symbol, exchange, interval). Idempotent on timestamp
    /// (INSERT OR REPLACE keyed on the candle's epoch-ms timestamp). Runs in a
    /// single transaction. Returns false on any write error.
    bool store_candles(const QString& symbol, const QString& exchange, const QString& interval,
                       const QVector<trading::BrokerCandle>& candles);

    /// Query stored candles in [from_ms, to_ms] inclusive, ordered ascending by
    /// timestamp. Pass from_ms<=0 / to_ms<=0 to leave that bound open.
    QVector<trading::BrokerCandle> get_candles(const QString& symbol, const QString& exchange,
                                               const QString& interval, qint64 from_ms, qint64 to_ms) const;

    /// Compute a higher-timeframe series by resampling a stored base series:
    ///   4h / 8h  ← aggregated from stored "1h"
    ///   1w / 1mo ← aggregated from stored "1d"
    /// OHLC aggregation: open=first, high=max, low=min, close=last, volume=sum,
    /// oi=last in each target bucket. Returns empty if target_interval is not a
    /// supported resample target or the base series is missing.
    QVector<trading::BrokerCandle> get_resampled(const QString& symbol, const QString& exchange,
                                                 const QString& target_interval, qint64 from_ms,
                                                 qint64 to_ms) const;

    // ── Watchlist (symbols flagged for auto-download) ────────────────────────

    /// Add a (symbol, exchange, interval) to the auto-download watchlist.
    /// Idempotent (INSERT OR IGNORE on the composite key).
    bool add_to_watchlist(const QString& symbol, const QString& exchange, const QString& interval);

    /// Remove a (symbol, exchange, interval) from the watchlist.
    bool remove_from_watchlist(const QString& symbol, const QString& exchange, const QString& interval);

    struct WatchEntry {
        QString symbol;
        QString exchange;
        QString interval;
    };
    /// All watchlist entries, ordered by symbol/exchange/interval.
    QVector<WatchEntry> watchlist() const;

    // ── Catalog metadata ─────────────────────────────────────────────────────

    struct CatalogEntry {
        QString symbol;
        QString exchange;
        QString interval;
        qint64 first_ts = 0; // epoch ms of earliest stored candle
        qint64 last_ts = 0;  // epoch ms of latest stored candle
        int record_count = 0;
    };
    /// Per-series summary (MIN/MAX timestamp + COUNT) derived from market_data.
    QVector<CatalogEntry> catalog() const;

    // ── Export ───────────────────────────────────────────────────────────────

    /// Export a stored series to CSV: header `timestamp,open,high,low,close,
    /// volume,oi` then one row per candle (timestamp in epoch ms). [from_ms,
    /// to_ms] inclusive; pass 0 for either bound to leave it open. Returns false
    /// on query / file-write error.
    bool export_csv(const QString& symbol, const QString& exchange, const QString& interval,
                    const QString& file_path, qint64 from_ms = 0, qint64 to_ms = 0) const;

    /// TODO(parquet): true columnar Parquet export requires a parquet/arrow lib
    /// (none is vendored). For now this writes CSV to `file_path` and logs a
    /// warning that Parquet was downgraded to CSV. Wire a real Parquet writer
    /// when Arrow/Parquet is added to the build.
    bool export_parquet(const QString& symbol, const QString& exchange, const QString& interval,
                        const QString& file_path, qint64 from_ms = 0, qint64 to_ms = 0) const;

    // ── Auto-download hook ───────────────────────────────────────────────────

    /// Entry point for callers (a future scheduler / broker-fetch service) to
    /// push freshly fetched candles into the store for a watchlist series.
    /// Equivalent to store_candles() but named to document intent at the call
    /// site. This pass intentionally does NOT wire broker fetches — the actual
    /// download is a TODO hook (see refresh_watchlist()).
    bool refresh_now(const QString& symbol, const QString& exchange, const QString& interval,
                     const QVector<trading::BrokerCandle>& candles);

    /// Refresh every watchlist entry: fetches historical candles from the first
    /// connected active broker account (on a worker thread, sequentially) and
    /// stores them via refresh_now(). No-ops gracefully (returns 0) when the
    /// watchlist is empty or no broker is connected. Returns the number of
    /// entries dispatched for refresh. A periodic scheduler (app/main.cpp) calls
    /// this; it is also safe to call on demand.
    int refresh_watchlist();

  private:
    HistoricalDataStore() = default;
    HistoricalDataStore(const HistoricalDataStore&) = delete;
    HistoricalDataStore& operator=(const HistoricalDataStore&) = delete;
};

} // namespace fincept::storage
