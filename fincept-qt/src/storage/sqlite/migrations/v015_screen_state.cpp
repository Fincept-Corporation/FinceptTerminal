// v015_screen_state — Add screen_state table to cache.db (via CacheDatabase).
// This is NOT a fincept.db migration — it runs against cache.db directly.
// The MigrationRunner here only creates the table once in the main DB's
// schema_version tracking, but the actual DDL executes on CacheDatabase.
// See ScreenStateManager::ensure_table() which calls this at init time.

#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v015(QSqlDatabase& /*db*/) {
    // This migration runs its DDL on cache.db, not fincept.db.
    // The version record is still tracked in fincept.db schema_version
    // so we don't re-run it. The actual table creation is idempotent
    // (CREATE TABLE IF NOT EXISTS) and also happens in CacheDatabase::create_tables().
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<void>::ok(); // cache.db not open yet — table created on open

    auto r = cdb.exec("CREATE TABLE IF NOT EXISTS screen_state ("
                      "  screen_key    TEXT PRIMARY KEY,"
                      "  state_version INTEGER NOT NULL DEFAULT 1,"
                      "  state_json    TEXT    NOT NULL DEFAULT '{}',"
                      "  updated_at    TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
                      "  session_id    TEXT    NOT NULL DEFAULT ''"
                      ")");
    if (r.is_err())
        return r;

    cdb.exec("CREATE INDEX IF NOT EXISTS idx_screen_state_updated "
             "ON screen_state(updated_at)");

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v015() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({15, "screen_state", apply_v015});
}

} // namespace fincept
