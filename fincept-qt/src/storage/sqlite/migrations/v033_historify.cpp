// v033_historify — Persistent historical OHLCV+OI data store (Historify), Phase 3 §13.
//
// Backs `fincept::storage::HistoricalDataStore`. Mirrors OpenAlgo's Historify
// (database/historify_db.py) but on the EXISTING SQLite stack rather than DuckDB.
//
// DECISION: SQLite, not DuckDB. The project has no DuckDB / vcpkg / conan
// dependency; pulling DuckDB in via FetchContent is too heavy/risky. The Phase 3
// doc explicitly lists SQLite as the acceptable fallback (Option B). DuckDB
// remains a possible future optimization for large-scale OLAP / backtesting
// (millions of rows) — see the TODO in HistoricalDataStore.h.
//
// Tables:
//   market_data        — one row per (symbol, exchange, interval, timestamp_ms).
//                        Base storage intervals (e.g. 1h, 1d) are stored; higher
//                        timeframes (4h/8h, 1w/1mo) are computed on-demand by
//                        HistoricalDataStore::get_resampled().
//   historify_watchlist — symbols flagged for auto-download (the fetch wiring is
//                        a documented TODO hook; this migration only stores the
//                        list).
//
// timestamp_ms is epoch MILLISECONDS (matches trading::BrokerCandle::timestamp
// which the broker historical endpoints populate). NOTE: OpenAlgo uses epoch
// SECONDS — the Fincept store standardises on ms throughout.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v033(QSqlDatabase& db) {
    // OHLCV+OI store. Composite primary key makes store_candles idempotent on
    // re-download (INSERT OR REPLACE collapses duplicate timestamps).
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS market_data ("
                     "  symbol TEXT NOT NULL,"
                     "  exchange TEXT NOT NULL,"
                     "  interval TEXT NOT NULL,"
                     "  timestamp_ms INTEGER NOT NULL,"
                     "  open REAL NOT NULL DEFAULT 0,"
                     "  high REAL NOT NULL DEFAULT 0,"
                     "  low REAL NOT NULL DEFAULT 0,"
                     "  close REAL NOT NULL DEFAULT 0,"
                     "  volume REAL NOT NULL DEFAULT 0,"
                     "  oi REAL NOT NULL DEFAULT 0,"
                     "  PRIMARY KEY (symbol, exchange, interval, timestamp_ms)"
                     ")");
    if (r.is_err())
        return r;

    // Range-scan index for get_candles / get_resampled (series + time window).
    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_market_data_series_time "
                "ON market_data (symbol, exchange, interval, timestamp_ms)");
    if (r.is_err())
        return r;

    // Watchlist of (symbol, exchange, interval) to auto-download.
    r = sql(db, "CREATE TABLE IF NOT EXISTS historify_watchlist ("
                "  symbol TEXT NOT NULL,"
                "  exchange TEXT NOT NULL,"
                "  interval TEXT NOT NULL,"
                "  added_at TEXT DEFAULT (datetime('now')),"
                "  PRIMARY KEY (symbol, exchange, interval)"
                ")");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v033() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({33, "historify", apply_v033});
}

} // namespace fincept
