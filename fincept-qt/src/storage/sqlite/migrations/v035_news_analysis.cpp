// v035_news_analysis — Persisted AI analysis results for news articles.
//
// Caches the /news/analyze response (the raw `data` JSON object) keyed by the
// article URL so a previously-run analysis re-appears whenever the article is
// reopened, without re-spending credits. Overwritten only when the user clicks
// ANALYZE again (INSERT OR REPLACE).

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

Result<void> apply_v035(QSqlDatabase& db) {
    return sql(db, "CREATE TABLE IF NOT EXISTS news_analysis ("
                   "  url TEXT PRIMARY KEY,"
                   "  analysis_json TEXT NOT NULL,"
                   "  created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
                   ")");
}

} // anonymous namespace

void register_migration_v035() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({35, "news_analysis", apply_v035});
}

} // namespace fincept
