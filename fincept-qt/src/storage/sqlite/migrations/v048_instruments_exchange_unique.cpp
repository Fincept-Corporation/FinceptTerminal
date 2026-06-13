// v048_instruments_exchange_unique — fix Dhan securityId collisions.
//
// Dhan reuses the same numeric securityId across exchange segments: e.g. 2885 is
// both RELIANCE on NSE_EQ and an (expired) EURINR currency option on CDS; 11536 is
// TCS and a GBPUSD option. ~20k ids in the scrip master are shared by >1 segment.
// The instruments table enforced UNIQUE(instrument_token, broker_id) with INSERT
// OR IGNORE, and the master lists currency/commodity options before equities, so
// the option won and the colliding equity row was silently dropped — wiping
// RELIANCE/TCS/INFY/HDFCBANK/… from the Dhan cache and breaking every quote/chart/
// orderbook resolution (only non-colliding ids like JSWSTEEL=11723 worked).
//
// The securityId IS unique per (exchange-segment), so widen the uniqueness key to
// include `exchange`. SQLite can't ALTER a UNIQUE constraint, so rebuild the table.
// Rows for numeric-globally-unique brokers (Fyers/Zerodha/…) are preserved
// verbatim; Dhan rows are intentionally NOT copied so the next focus re-downloads a
// complete master under the corrected key (the cache only refreshes when empty).

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

Result<void> apply_v048(QSqlDatabase& db) {
    // 1. Move the existing table aside.
    auto r = sql(db, "ALTER TABLE instruments RENAME TO instruments_old_v048");
    if (r.is_err())
        return r;

    // 2. Recreate with the widened uniqueness key (instrument_token, exchange, broker_id).
    r = sql(db,
            "CREATE TABLE instruments ("
            "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  instrument_token INTEGER NOT NULL,"
            "  exchange_token   INTEGER NOT NULL DEFAULT 0,"
            "  symbol           TEXT    NOT NULL,"
            "  brsymbol         TEXT    NOT NULL,"
            "  name             TEXT    NOT NULL DEFAULT '',"
            "  exchange         TEXT    NOT NULL,"
            "  brexchange       TEXT    NOT NULL DEFAULT '',"
            "  expiry           TEXT    NOT NULL DEFAULT '',"
            "  strike           REAL    NOT NULL DEFAULT 0,"
            "  lot_size         INTEGER NOT NULL DEFAULT 1,"
            "  instrument_type  TEXT    NOT NULL DEFAULT 'EQ',"
            "  tick_size        REAL    NOT NULL DEFAULT 0.05,"
            "  broker_id        TEXT    NOT NULL,"
            "  updated_at       TEXT    DEFAULT (datetime('now')),"
            "  broker_token     TEXT    NOT NULL DEFAULT '',"
            "  UNIQUE(instrument_token, exchange, broker_id)"
            ")");
    if (r.is_err())
        return r;

    // 3. Copy every broker EXCEPT dhan (whose old rows are collision-corrupted —
    //    let it re-download a fresh, complete master under the new key).
    r = sql(db,
            "INSERT OR IGNORE INTO instruments "
            "(instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
            " expiry, strike, lot_size, instrument_type, tick_size, broker_id, updated_at, broker_token) "
            "SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
            " expiry, strike, lot_size, instrument_type, tick_size, broker_id, updated_at, broker_token "
            "FROM instruments_old_v048 WHERE broker_id <> 'dhan'");
    if (r.is_err())
        return r;

    // 4. Drop the old table.
    r = sql(db, "DROP TABLE instruments_old_v048");
    if (r.is_err())
        return r;

    // 5. Recreate the indices (they vanished with the old table).
    for (const char* idx : {
             "CREATE INDEX IF NOT EXISTS idx_inst_symbol   ON instruments(symbol, exchange, broker_id)",
             "CREATE INDEX IF NOT EXISTS idx_inst_token    ON instruments(instrument_token, broker_id)",
             "CREATE INDEX IF NOT EXISTS idx_inst_brsymbol ON instruments(brsymbol, brexchange, broker_id)",
             "CREATE INDEX IF NOT EXISTS idx_inst_exchange ON instruments(exchange, broker_id, instrument_type)",
             "CREATE INDEX IF NOT EXISTS idx_inst_name     ON instruments(name, broker_id)",
         }) {
        r = sql(db, idx);
        if (r.is_err())
            return r;
    }

    return Result<void>::ok();
}

} // namespace

void register_migration_v048() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({48, "instruments_exchange_unique", apply_v048});
}

} // namespace fincept
