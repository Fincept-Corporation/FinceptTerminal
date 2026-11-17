// v060_symbol_master — the local instrument index of the symbol system (plan E7 / WP2).
//
// One row per instrument, keyed by the stable id of core/instrument/Instrument.h
// ("XNSE:RELIANCE", "US:AAPL", "FX:EURUSD", "CRYPTO:BTC-USD", "IDX:NSEI", "FUT:GC").
// Written only by SymbolMasterRepository (seed, learned resolutions, broker-master
// import, backend master deltas); read by InstrumentIndex on worker threads.
//
//   symbol_master       the instrument record. `symbol` is the canonical market-data key
//                       (Yahoo form, '' when Yahoo has none); `name_norm` is
//                       normalize_name(name) for indexed name-prefix scans and
//                       name-contains search; providers_json / aliases_json /
//                       former_json hold the record's provider map and lists verbatim
//                       (the two tables below are their lookup indexes, rebuilt with
//                       every write); `source` / `source_rank` say which writer owns the
//                       fields (master 4 > seed 3 > learned 2 > broker 1).
//   symbol_alias        lookup keys -> id. alias_norm = alias_key(text) (upper case,
//                       letters and digits only: "S&P 500" -> "SP500"). kind:
//                         code    listing codes (Yahoo code / base, id code, broker symbols)
//                         alias   common names ("NIFTY 50", "gold", "Google")
//                         former  former tickers of THIS instrument ("TATAMOTORS" on
//                                 TMPV.NS); valid_to = last day the code was used
//                         name    the whole name; lead = its first word (typo search)
//                         isin    ISIN (current, and former ISINs a master sends)
//                       Exact, prefix (range on the primary key) and fuzzy candidate
//                       scans all run on the primary-key index.
//   symbol_provider     provider / broker symbol -> id (reverse lookup: which instrument
//                       is zerodha's NSE "RELIANCE"). `value` is the provider_symbols
//                       string as stored on the record ("NSE|RELIANCE|738561|RELIANCE-EQ").
//   symbol_master_meta  key/value state: seed version, master sync cursor, last
//                       imported broker-master timestamp per broker.
//
// No FTS5: whether the SQLite library has it depends on the build (Qt's bundled
// SQLite vs a distro one), and name-contains over the index is a short scan of one
// narrow column (measured in tst_symbol_master_repo). Prefix searches use range scans
// on normalised columns, the same technique as trading/instruments/InstrumentSearchSql.h;
// a dense prefix walks idx_symbol_master_weight heaviest first instead.
//
// SAFE ON A POPULATED DATABASE: new tables and indexes only (CREATE ... IF NOT
// EXISTS); nothing existing is read or changed. Idempotent.
//
// REGISTRATION: declared in MigrationRunner.h, called from main.cpp after
// register_migration_v059(), listed in LegacyRootAdoption.cpp
// register_known_migrations().

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v060();

namespace {

// Uniquely named — unity builds concatenate translation units.
Result<void> sql_v060(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(QString::fromLatin1(stmt)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v060(QSqlDatabase& db) {
    static const char* const kV060Statements[] = {
        "CREATE TABLE IF NOT EXISTS symbol_master ("
        "  id              TEXT PRIMARY KEY,"
        "  symbol          TEXT NOT NULL DEFAULT '',"      // canonical Yahoo form; '' = none
        "  display         TEXT NOT NULL DEFAULT '',"
        "  name            TEXT NOT NULL DEFAULT '',"
        "  name_norm       TEXT NOT NULL DEFAULT '',"      // normalize_name(name)
        "  asset_class     TEXT NOT NULL DEFAULT '',"      // asset_class_slug
        "  quote_type      TEXT NOT NULL DEFAULT '',"
        "  venue           TEXT NOT NULL DEFAULT '',"
        "  mic             TEXT NOT NULL DEFAULT '',"
        "  country         TEXT NOT NULL DEFAULT '',"
        "  currency        TEXT NOT NULL DEFAULT '',"      // as quoted, minor units kept (GBp)
        "  isin            TEXT NOT NULL DEFAULT '',"
        "  figi            TEXT NOT NULL DEFAULT '',"
        "  status          TEXT NOT NULL DEFAULT 'active',"// active | delisted | renamed
        "  successor_id    TEXT NOT NULL DEFAULT '',"
        "  primary_listing INTEGER NOT NULL DEFAULT 0,"
        "  rank_weight     INTEGER NOT NULL DEFAULT 0,"
        "  providers_json  TEXT NOT NULL DEFAULT '{}',"
        "  aliases_json    TEXT NOT NULL DEFAULT '[]',"
        "  former_json     TEXT NOT NULL DEFAULT '[]',"
        "  source          TEXT NOT NULL DEFAULT '',"      // master | seed | learned | broker:<id>
        "  source_rank     INTEGER NOT NULL DEFAULT 0,"
        "  updated_at      INTEGER NOT NULL DEFAULT 0"     // epoch ms (UTC)
        ")",
        "CREATE INDEX IF NOT EXISTS idx_symbol_master_symbol ON symbol_master(symbol)",
        "CREATE INDEX IF NOT EXISTS idx_symbol_master_name_norm ON symbol_master(name_norm)",
        "CREATE INDEX IF NOT EXISTS idx_symbol_master_isin ON symbol_master(isin) WHERE isin <> ''",
        "CREATE INDEX IF NOT EXISTS idx_symbol_master_successor ON symbol_master(successor_id) "
        "WHERE successor_id <> ''",
        // Heaviest-first walk for dense prefixes ("A", "S"): stops after the first few hundred
        // matches instead of sorting a range of tens of thousands of keys.
        "CREATE INDEX IF NOT EXISTS idx_symbol_master_weight ON symbol_master(rank_weight)",

        "CREATE TABLE IF NOT EXISTS symbol_alias ("
        "  alias_norm TEXT NOT NULL,"                      // alias_key(text)
        "  id         TEXT NOT NULL,"
        "  kind       TEXT NOT NULL,"                      // code | alias | former | name | lead | isin
        "  text       TEXT NOT NULL DEFAULT '',"           // as given ("formerly TATAMOTORS")
        "  valid_to   TEXT NOT NULL DEFAULT '',"           // former: last day used (YYYY-MM-DD), '' unknown
        "  PRIMARY KEY (alias_norm, id, kind)"
        ") WITHOUT ROWID",
        "CREATE INDEX IF NOT EXISTS idx_symbol_alias_id ON symbol_alias(id)",

        "CREATE TABLE IF NOT EXISTS symbol_provider ("
        "  id       TEXT NOT NULL,"
        "  provider TEXT NOT NULL,"                        // yahoo | tv | binance | ... | broker:<id>
        "  symbol   TEXT NOT NULL DEFAULT '',"             // provider's own symbol (broker: normalised SYMBOL)
        "  exchange TEXT NOT NULL DEFAULT '',"             // broker exchange code ("NSE"); '' otherwise
        "  token    TEXT NOT NULL DEFAULT '',"
        "  native   TEXT NOT NULL DEFAULT '',"             // broker-native spelling when it differs
        "  value    TEXT NOT NULL DEFAULT '',"             // provider_symbols value as stored
        "  PRIMARY KEY (id, provider)"
        ") WITHOUT ROWID",
        "CREATE INDEX IF NOT EXISTS idx_symbol_provider_lookup ON symbol_provider(provider, symbol, exchange)",

        "CREATE TABLE IF NOT EXISTS symbol_master_meta ("
        "  key        TEXT PRIMARY KEY,"
        "  value      TEXT NOT NULL DEFAULT '',"
        "  updated_at INTEGER NOT NULL DEFAULT 0"
        ")",
    };
    for (const char* stmt : kV060Statements) {
        auto r = sql_v060(db, stmt);
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v060() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({60, "symbol_master", apply_v060});
}

} // namespace fincept
