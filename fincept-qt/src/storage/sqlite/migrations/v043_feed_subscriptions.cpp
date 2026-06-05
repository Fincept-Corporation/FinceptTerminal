// v043_feed_subscriptions — generalized feed monitor subscriptions.
//
// Separate from rss_feeds (news catalog) so arbitrary user feeds never leak into
// the News screen. Managed by FeedSubscriptionRepository / FeedMonitor.

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

Result<void> apply_v043(QSqlDatabase& db) {
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS feed_subscriptions ("
                     "  id TEXT PRIMARY KEY,"
                     "  name TEXT NOT NULL,"
                     "  url TEXT NOT NULL,"
                     "  refresh_sec INTEGER NOT NULL DEFAULT 300,"
                     "  parse_mode TEXT NOT NULL DEFAULT 'auto',"
                     "  format TEXT NOT NULL DEFAULT 'auto',"
                     "  field_map TEXT NOT NULL DEFAULT '{}',"
                     "  display_mode TEXT NOT NULL DEFAULT 'cards',"
                     "  headers TEXT NOT NULL DEFAULT '{}',"
                     "  enabled INTEGER NOT NULL DEFAULT 1,"
                     "  sort_order INTEGER NOT NULL DEFAULT 0,"
                     "  created_at TEXT DEFAULT (datetime('now')),"
                     "  updated_at TEXT DEFAULT (datetime('now'))"
                     ")");
    if (r.is_err())
        return r;

    return sql(db, "CREATE INDEX IF NOT EXISTS idx_feed_subs_order "
                   "ON feed_subscriptions(sort_order)");
}

} // anonymous namespace

void register_migration_v043() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({43, "feed_subscriptions", apply_v043});
}

} // namespace fincept
