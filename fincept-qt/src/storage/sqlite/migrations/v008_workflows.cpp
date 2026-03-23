// v008_workflows — Workflow persistence tables for Node Editor.

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

Result<void> apply_v008(QSqlDatabase& db) {

    // ── Workflows ──────────────────────────────────────────────────────────────
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS workflows ("
                     "  id           TEXT PRIMARY KEY,"
                     "  name         TEXT NOT NULL DEFAULT 'Untitled Workflow',"
                     "  description  TEXT DEFAULT '',"
                     "  status       TEXT NOT NULL DEFAULT 'draft',"
                     "  static_data  TEXT DEFAULT '{}',"
                     "  created_at   TEXT DEFAULT (datetime('now')),"
                     "  updated_at   TEXT DEFAULT (datetime('now'))"
                     ")");
    if (r.is_err())
        return r;

    // ── Workflow nodes ─────────────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS workflow_nodes ("
                "  id               TEXT PRIMARY KEY,"
                "  workflow_id      TEXT NOT NULL REFERENCES workflows(id) ON DELETE CASCADE,"
                "  type             TEXT NOT NULL,"
                "  name             TEXT NOT NULL,"
                "  type_version     INTEGER NOT NULL DEFAULT 1,"
                "  pos_x            REAL NOT NULL DEFAULT 0,"
                "  pos_y            REAL NOT NULL DEFAULT 0,"
                "  parameters       TEXT DEFAULT '{}',"
                "  credentials      TEXT DEFAULT '{}',"
                "  disabled         INTEGER NOT NULL DEFAULT 0,"
                "  continue_on_fail INTEGER NOT NULL DEFAULT 0,"
                "  retry_on_fail    INTEGER NOT NULL DEFAULT 0,"
                "  max_tries        INTEGER NOT NULL DEFAULT 1,"
                "  sort_order       INTEGER NOT NULL DEFAULT 0"
                ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_wn_workflow ON workflow_nodes(workflow_id)");
    if (r.is_err())
        return r;

    // ── Workflow edges ─────────────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS workflow_edges ("
                "  id           TEXT PRIMARY KEY,"
                "  workflow_id  TEXT NOT NULL REFERENCES workflows(id) ON DELETE CASCADE,"
                "  source_node  TEXT NOT NULL,"
                "  target_node  TEXT NOT NULL,"
                "  source_port  TEXT NOT NULL,"
                "  target_port  TEXT NOT NULL,"
                "  animated     INTEGER NOT NULL DEFAULT 0"
                ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_we_workflow ON workflow_edges(workflow_id)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v008() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({8, "workflows", apply_v008});
}

} // namespace fincept
