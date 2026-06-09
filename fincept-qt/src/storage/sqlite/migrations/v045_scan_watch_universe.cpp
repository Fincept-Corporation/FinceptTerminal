// v045_scan_watch_universe — adds a `universe` selector column to scan_watches so a
// realtime watch can track a *dynamic* symbol set (all NSE/BSE equities, NIFTY 50)
// resolved live from the instrument master, instead of a frozen symbols list.
//
//   '' / 'CUSTOM'           → use the existing `symbols` column
//   'NSE_EQ' / 'BSE_EQ'     → resolve all EQ instruments for that exchange at runner start
//   'NIFTY50'               → resolve nifty50_symbols()
// Existing rows default to '' so current poll watches are unaffected.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> apply_v045(QSqlDatabase& db) {
    // The column may already exist on re-run; ignore the duplicate-column error so
    // the migration stays idempotent (same approach as v044's `persist` column).
    QSqlQuery alter(db);
    if (!alter.exec("ALTER TABLE scan_watches ADD COLUMN universe TEXT NOT NULL DEFAULT ''")) {
        const QString err = alter.lastError().text();
        if (!err.contains("duplicate column", Qt::CaseInsensitive))
            return Result<void>::err(err.toStdString());
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v045() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({45, "scan_watch_universe", apply_v045});
}

} // namespace fincept
