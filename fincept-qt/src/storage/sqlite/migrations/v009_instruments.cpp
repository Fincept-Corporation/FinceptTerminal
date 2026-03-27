// v009_instruments — Broker instrument master contract table.
// Stores normalised instruments for all brokers (Zerodha, AngelOne, etc.)
// with O(1)-friendly indices for token lookup, symbol search, and brsymbol mapping.

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

Result<void> apply_v009(QSqlDatabase& db) {

    // ── Instruments master contract ───────────────────────────────────────────
    auto r = sql(db,
        "CREATE TABLE IF NOT EXISTS instruments ("
        "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  instrument_token INTEGER NOT NULL,"          // Zerodha numeric token
        "  exchange_token   INTEGER NOT NULL DEFAULT 0,"
        "  symbol           TEXT    NOT NULL,"          // normalised  e.g. NIFTY28MAR24FUT
        "  brsymbol         TEXT    NOT NULL,"          // broker native e.g. 'NIFTY 50 MAR24 FUT'
        "  name             TEXT    NOT NULL DEFAULT ''," // underlying e.g. NIFTY
        "  exchange         TEXT    NOT NULL,"          // NSE / BSE / NFO / NSE_INDEX …
        "  brexchange       TEXT    NOT NULL DEFAULT '',"
        "  expiry           TEXT    NOT NULL DEFAULT ''," // DD-MMM-YY or empty
        "  strike           REAL    NOT NULL DEFAULT 0,"
        "  lot_size         INTEGER NOT NULL DEFAULT 1,"
        "  instrument_type  TEXT    NOT NULL DEFAULT 'EQ'," // EQ/FUT/CE/PE/INDEX
        "  tick_size        REAL    NOT NULL DEFAULT 0.05,"
        "  broker_id        TEXT    NOT NULL,"          // zerodha / angelone / …
        "  updated_at       TEXT    DEFAULT (datetime('now')),"
        "  UNIQUE(instrument_token, broker_id)"
        ")"
    );
    if (r.is_err()) return r;

    // Primary lookup: (symbol, exchange, broker_id) → token
    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_inst_symbol "
                "ON instruments(symbol, exchange, broker_id)");
    if (r.is_err()) return r;

    // Reverse lookup: instrument_token + broker_id → symbol
    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_inst_token "
                "ON instruments(instrument_token, broker_id)");
    if (r.is_err()) return r;

    // Broker-native symbol lookup (for mapping order book responses)
    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_inst_brsymbol "
                "ON instruments(brsymbol, brexchange, broker_id)");
    if (r.is_err()) return r;

    // Exchange filter (for option chain listing)
    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_inst_exchange "
                "ON instruments(exchange, broker_id, instrument_type)");
    if (r.is_err()) return r;

    // Full-text search index on name (for symbol picker)
    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_inst_name "
                "ON instruments(name, broker_id)");
    if (r.is_err()) return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v009() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({9, "instruments", apply_v009});
}

} // namespace fincept
