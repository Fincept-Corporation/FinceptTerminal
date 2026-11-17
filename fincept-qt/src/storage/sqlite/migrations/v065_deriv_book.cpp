// v065_deriv_book — the Derivatives tab's position book and saved strategies:
//
//   deriv_positions   one row per saved position (a single option, stock, future or swap, or a
//                     multi-leg strategy) in a named book; its legs (type, signed quantity, strike,
//                     expiry, entry price, entry IV, futures contract, swap terms) are JSON in
//                     legs_json (services/options/BookTypes.h); quantity / price units as there.
//   deriv_strategies  named strategies saved from the STRATEGY page (legs as JSON, the template
//                     they came from, the underlying); saving under an existing name replaces it.
//
// Local only: not part of cloud sync or the workspace export. Timestamps are epoch milliseconds;
// entry_date is an ISO date (yyyy-MM-dd).
//
// SAFE ON A POPULATED DATABASE AND IDEMPOTENT: only CREATE TABLE / INDEX IF NOT EXISTS; nothing
// existing is altered. MigrationRunner::inspect() builds its reference schema with this too.
//
// REGISTRATION: declared in MigrationRunner.h, called from main.cpp after register_migration_v064(),
// listed in LegacyRootAdoption.cpp register_known_migrations() (kKnownHighestMigration).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v065();

namespace {

Result<void> apply_v065(QSqlDatabase& db) {
    static const char* const kStatements[] = {
        "CREATE TABLE IF NOT EXISTS deriv_positions ("
        "  id          TEXT PRIMARY KEY,"
        "  book        TEXT NOT NULL DEFAULT 'Main',"
        "  kind        TEXT NOT NULL,"
        "  name        TEXT NOT NULL DEFAULT '',"
        "  underlying  TEXT NOT NULL DEFAULT '',"
        "  currency    TEXT NOT NULL DEFAULT 'USD',"
        "  multiplier  REAL NOT NULL DEFAULT 1,"
        "  entry_date  TEXT NOT NULL DEFAULT '',"
        "  legs_json   TEXT NOT NULL DEFAULT '[]',"
        "  strategy_id TEXT NOT NULL DEFAULT '',"
        "  notes       TEXT NOT NULL DEFAULT '',"
        "  created_at  INTEGER NOT NULL,"
        "  updated_at  INTEGER NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_deriv_positions_book ON deriv_positions(book, underlying)",

        "CREATE TABLE IF NOT EXISTS deriv_strategies ("
        "  id          TEXT PRIMARY KEY,"
        "  name        TEXT NOT NULL,"
        "  underlying  TEXT NOT NULL DEFAULT '',"
        "  template_id TEXT NOT NULL DEFAULT '',"
        "  currency    TEXT NOT NULL DEFAULT 'USD',"
        "  multiplier  REAL NOT NULL DEFAULT 100,"
        "  legs_json   TEXT NOT NULL DEFAULT '[]',"
        "  notes       TEXT NOT NULL DEFAULT '',"
        "  created_at  INTEGER NOT NULL,"
        "  updated_at  INTEGER NOT NULL"
        ")",
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_deriv_strategies_name ON deriv_strategies(name COLLATE NOCASE)",
    };
    for (const char* stmt : kStatements) {
        QSqlQuery q(db);
        if (!q.exec(QString::fromLatin1(stmt)))
            return Result<void>::err(
                QStringLiteral("v065: %1 — %2").arg(QString::fromLatin1(stmt).left(80), q.lastError().text()).toStdString());
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v065() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({65, "deriv_book", apply_v065});
}

} // namespace fincept
