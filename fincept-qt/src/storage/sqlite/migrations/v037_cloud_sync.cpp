// v037_cloud_sync — durable scaffolding for Fincept Cloud mirror sync.
//   sync_outbox : write-ahead log of local mutations awaiting cloud push (survives
//                 restart/offline; drained by CloudSyncEngine with retry/backoff).
//   sync_map    : local PK <-> server public_id mapping, plus remote_updated_at for
//                 last-write-wins on pull.
// See fincept-qt/CLOUD_SYNC_PLAN.md.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> apply_v037(QSqlDatabase& db) {
    QSqlQuery q(db);

    if (!q.exec("CREATE TABLE IF NOT EXISTS sync_outbox ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "entity TEXT NOT NULL, "       // 'watchlist','note','portfolio',...
                "local_id TEXT NOT NULL, "     // stringified local PK (uuid or int)
                "op TEXT NOT NULL, "           // 'create' | 'update' | 'delete' | domain op
                "payload TEXT, "               // optional op hint (e.g. stock symbol)
                "created_at TEXT NOT NULL DEFAULT (datetime('now')), "
                "attempts INTEGER NOT NULL DEFAULT 0, "
                "last_error TEXT)"))
        return Result<void>::err(q.lastError().text().toStdString());

    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_sync_outbox_entity ON sync_outbox(entity, id)"))
        return Result<void>::err(q.lastError().text().toStdString());

    if (!q.exec("CREATE TABLE IF NOT EXISTS sync_map ("
                "entity TEXT NOT NULL, "
                "local_id TEXT NOT NULL, "
                "remote_id TEXT NOT NULL, "    // server public_id (wl_, nte_, pfl_, ...)
                "remote_updated_at TEXT, "     // for last-write-wins
                "PRIMARY KEY (entity, local_id))"))
        return Result<void>::err(q.lastError().text().toStdString());

    if (!q.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_sync_map_remote ON sync_map(entity, remote_id)"))
        return Result<void>::err(q.lastError().text().toStdString());

    return Result<void>::ok();
}

} // namespace

void register_migration_v037() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({37, "cloud_sync", apply_v037});
}

} // namespace fincept
