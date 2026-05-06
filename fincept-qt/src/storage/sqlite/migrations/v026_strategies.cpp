// v026_strategies — Saved F&O strategies (Phase 5 of the F&O work).
//
// One row per saved Strategy. Legs are stored as a JSON array under
// `legs_json` to keep the schema flat (legs are read/written wholesale —
// per-leg queries aren't a use case the Builder needs).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> v026_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v026(QSqlDatabase& db) {
    {
        auto r = v026_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS strategies (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                name         TEXT    NOT NULL,
                underlying   TEXT    NOT NULL DEFAULT '',
                expiry       TEXT    NOT NULL DEFAULT '',
                created_at   TEXT    NOT NULL DEFAULT (datetime('now')),
                modified_at  TEXT    NOT NULL DEFAULT (datetime('now')),
                notes        TEXT    NOT NULL DEFAULT '',
                legs_json    TEXT    NOT NULL DEFAULT '[]'
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v026_sql(db, "CREATE INDEX IF NOT EXISTS idx_strategies_underlying "
                              "ON strategies(underlying, modified_at DESC)");
        if (r.is_err()) return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v026() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({26, "strategies", apply_v026});
}

} // namespace fincept
