// v042_scan_watch_events — one row per fired alert (a watch condition met). Lets
// the UI show alert history and lets a watch's events be cascade-removed when the
// watch is deleted. UUID primary key avoids collisions across watches/fires.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> apply_v042(QSqlDatabase& db) {
    QSqlQuery q(db);
    const char* sql =
        "CREATE TABLE IF NOT EXISTS scan_watch_events ("
        " id TEXT PRIMARY KEY,"
        " watch_id TEXT NOT NULL DEFAULT '',"
        " symbol TEXT NOT NULL DEFAULT '',"
        " detail TEXT NOT NULL DEFAULT '',"
        " fired_at INTEGER NOT NULL DEFAULT 0"
        ")";
    if (!q.exec(QString::fromUtf8(sql)))
        return Result<void>::err(q.lastError().text().toStdString());
    QSqlQuery idx(db);
    if (!idx.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_scan_watch_events_watch"
                                 " ON scan_watch_events(watch_id)")))
        return Result<void>::err(idx.lastError().text().toStdString());
    return Result<void>::ok();
}

} // namespace

void register_migration_v042() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({42, "scan_watch_events", apply_v042});
}

} // namespace fincept
