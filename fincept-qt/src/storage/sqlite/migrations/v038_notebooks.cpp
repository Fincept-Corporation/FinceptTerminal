// v038_notebooks — local store for Jupyter-style notebooks, enabling cloud sync
// of /v1/notebooks. There is no local notebook editor UI yet; this table is the
// sync target (populated via cloud pull, and ready for a future local editor).
// See fincept-qt/CLOUD_SYNC_PLAN.md.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> apply_v038(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec("CREATE TABLE IF NOT EXISTS notebooks ("
                "id TEXT PRIMARY KEY, "                                  // client UUID; maps to nbk_
                "name TEXT NOT NULL DEFAULT 'Untitled', "
                "description TEXT, "
                "cells TEXT NOT NULL DEFAULT '[]', "                     // JSON array (Jupyter cells)
                "metadata TEXT NOT NULL DEFAULT '{}', "                  // JSON object
                "execution_counter INTEGER NOT NULL DEFAULT 0, "
                "created_at TEXT NOT NULL DEFAULT (datetime('now')), "
                "updated_at TEXT NOT NULL DEFAULT (datetime('now')))"))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

} // namespace

void register_migration_v038() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({38, "notebooks", apply_v038});
}

} // namespace fincept
