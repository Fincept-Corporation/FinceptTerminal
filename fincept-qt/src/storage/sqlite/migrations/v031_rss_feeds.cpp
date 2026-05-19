// v031_rss_feeds — User overlay table for RSS feed sources.
//
// Built-in feeds remain hardcoded in NewsService_Feeds.cpp. This table stores
// user overrides:
//   is_builtin = 1  → patch over a default feed (same id as default); may
//                     override name/url/category/region/source/tier and/or
//                     set enabled = 0 to hide the default.
//   is_builtin = 0  → fully user-added feed (id prefixed "usr-<uuid>").
//
// Effective feed list is computed at runtime in NewsService::effective_feeds().

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

Result<void> apply_v031(QSqlDatabase& db) {
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS rss_feeds ("
                     "  id TEXT PRIMARY KEY,"
                     "  name TEXT NOT NULL,"
                     "  url TEXT NOT NULL,"
                     "  category TEXT NOT NULL DEFAULT 'MARKETS',"
                     "  region TEXT NOT NULL DEFAULT 'GLOBAL',"
                     "  source TEXT NOT NULL,"
                     "  tier INTEGER NOT NULL DEFAULT 3,"
                     "  is_builtin INTEGER NOT NULL DEFAULT 0,"
                     "  enabled INTEGER NOT NULL DEFAULT 1,"
                     "  created_at TEXT DEFAULT (datetime('now')),"
                     "  updated_at TEXT DEFAULT (datetime('now'))"
                     ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_rss_feeds_enabled "
                "ON rss_feeds(enabled)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v031() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({31, "rss_feeds", apply_v031});
}

} // namespace fincept
