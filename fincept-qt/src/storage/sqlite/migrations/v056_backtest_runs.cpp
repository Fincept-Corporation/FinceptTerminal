// v056_backtest_runs — saved Backtesting runs (inputs, summary metrics, payload).
//
// One row per completed backtest / optimisation / walk-forward / Monte Carlo run
// of the Backtesting screen, written by services/backtesting/BacktestRunStore.
// The payload is the provider's full result JSON, qCompress'ed (a two-year daily
// backtest with trades and OHLC is ~170 KB of JSON, ~25 KB compressed);
// summary_json holds the handful of headline metrics the RUNS list and the
// comparison table read without decompressing payloads.
//
// SAFE ON A POPULATED DATABASE: creates a new table and an index only.
//
// REGISTRATION: declare `void register_migration_v056();` in MigrationRunner.h
// and call it from main.cpp after register_migration_v055(). If another
// migration has taken version 56 by then, renumber this file (version number,
// function name and the static `done` guard) to the next free version.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> sql_v056(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v056(QSqlDatabase& db) {
    auto r = sql_v056(db, "CREATE TABLE IF NOT EXISTS backtest_runs ("
                          "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "  created_at    TEXT    NOT NULL,"   // ISO-8601 UTC
                          "  provider      TEXT    NOT NULL,"   // engine slug (vectorbt, zipline, ...)
                          "  command       TEXT    NOT NULL,"   // backtest | optimize | walk_forward | monte_carlo
                          "  strategy      TEXT    NOT NULL DEFAULT '',"
                          "  symbols       TEXT    NOT NULL DEFAULT '',"  // comma-separated
                          "  start_date    TEXT    NOT NULL DEFAULT '',"
                          "  end_date      TEXT    NOT NULL DEFAULT '',"
                          "  interval      TEXT    NOT NULL DEFAULT '',"
                          "  currency      TEXT    NOT NULL DEFAULT '',"  // account currency of the money values
                          "  label         TEXT    NOT NULL DEFAULT '',"  // user's name for the run
                          "  synthetic     INTEGER NOT NULL DEFAULT 0,"
                          "  summary_json  TEXT    NOT NULL DEFAULT '{}',"
                          "  inputs_json   TEXT    NOT NULL DEFAULT '{}',"
                          "  payload       BLOB,"                         // qCompress(JSON)
                          "  payload_bytes INTEGER NOT NULL DEFAULT 0"    // uncompressed size
                          ")");
    if (r.is_err())
        return r;
    return sql_v056(db, "CREATE INDEX IF NOT EXISTS idx_backtest_runs_created ON backtest_runs(created_at)");
}

} // anonymous namespace

void register_migration_v056() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({56, "backtest_runs", apply_v056});
}

} // namespace fincept
