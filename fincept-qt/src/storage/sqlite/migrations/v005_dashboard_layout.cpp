// v005_dashboard_layout — Dashboard widget layout persistence tables.

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

Result<void> apply_v005(QSqlDatabase& db) {

    // ── Dashboard Layout profiles ─────────────────────────────────────────────
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS dashboard_layouts ("
                     "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "  profile_name TEXT NOT NULL DEFAULT 'default',"
                     "  cols         INTEGER NOT NULL DEFAULT 12,"
                     "  row_height   INTEGER NOT NULL DEFAULT 60,"
                     "  margin       INTEGER NOT NULL DEFAULT 4,"
                     "  is_active    INTEGER NOT NULL DEFAULT 0,"
                     "  updated_at   TEXT DEFAULT (datetime('now')),"
                     "  UNIQUE(profile_name)"
                     ")");
    if (r.is_err())
        return r;

    // ── Widget instances within a layout ──────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS dashboard_widget_instances ("
                "  instance_id   TEXT PRIMARY KEY,"
                "  layout_id     INTEGER NOT NULL REFERENCES dashboard_layouts(id) ON DELETE CASCADE,"
                "  widget_type   TEXT NOT NULL,"
                "  grid_x        INTEGER NOT NULL DEFAULT 0,"
                "  grid_y        INTEGER NOT NULL DEFAULT 0,"
                "  grid_w        INTEGER NOT NULL DEFAULT 4,"
                "  grid_h        INTEGER NOT NULL DEFAULT 3,"
                "  min_w         INTEGER NOT NULL DEFAULT 1,"
                "  min_h         INTEGER NOT NULL DEFAULT 2,"
                "  sort_order    INTEGER NOT NULL DEFAULT 0,"
                "  created_at    TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_dwi_layout ON dashboard_widget_instances(layout_id)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v005() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({5, "dashboard_layout", apply_v005});
}

} // namespace fincept
