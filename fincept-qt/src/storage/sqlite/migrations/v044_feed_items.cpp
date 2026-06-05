// v044_feed_items — persisted feed item history + per-feed "store history" flag.
//
// feed_items accumulates fetched items per subscription so the panel can show
// cached data when a feed is down and query historical entries. feed_subscriptions
// gains a `persist` flag (default on) controlling whether a feed is stored.

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

Result<void> apply_v044(QSqlDatabase& db) {
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS feed_items ("
                     "  feed_id TEXT NOT NULL,"
                     "  item_id TEXT NOT NULL,"
                     "  title TEXT,"
                     "  summary TEXT,"
                     "  link TEXT,"
                     "  source TEXT,"
                     "  sort_ts INTEGER NOT NULL DEFAULT 0,"
                     "  time TEXT,"
                     "  fields_json TEXT NOT NULL DEFAULT '{}',"
                     "  fetched_at INTEGER NOT NULL DEFAULT 0,"
                     "  PRIMARY KEY (feed_id, item_id)"
                     ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_feed_items_feed_ts "
                "ON feed_items(feed_id, sort_ts DESC)");
    if (r.is_err())
        return r;

    // Per-feed storage toggle. The column may already exist on re-run; ignore the
    // duplicate-column error so the migration stays idempotent.
    QSqlQuery alter(db);
    if (!alter.exec("ALTER TABLE feed_subscriptions ADD COLUMN persist INTEGER NOT NULL DEFAULT 1")) {
        const QString err = alter.lastError().text();
        if (!err.contains("duplicate column", Qt::CaseInsensitive))
            return Result<void>::err(err.toStdString());
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v044() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({44, "feed_items", apply_v044});
}

} // namespace fincept
