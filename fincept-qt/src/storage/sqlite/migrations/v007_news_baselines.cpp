// v007_news_baselines — News deviation baselines and entity cache tables.

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

Result<void> apply_v007(QSqlDatabase& db) {

    // ── News deviation baselines (rolling 7-day hourly counts) ─────────────
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS news_baselines ("
                     "  category TEXT PRIMARY KEY,"
                     "  hourly_counts TEXT,"
                     "  mean REAL DEFAULT 0,"
                     "  stddev REAL DEFAULT 0,"
                     "  sample_size INTEGER DEFAULT 0,"
                     "  updated_at INTEGER DEFAULT (strftime('%s','now'))"
                     ")");
    if (r.is_err())
        return r;

    // ── News entity cache (NER results per article) ────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS news_entity_cache ("
                "  article_id TEXT PRIMARY KEY,"
                "  entities_json TEXT,"
                "  geo_lat REAL,"
                "  geo_lon REAL,"
                "  cached_at INTEGER DEFAULT (strftime('%s','now'))"
                ")");
    if (r.is_err())
        return r;

    // ── News instability scores (CII per country) ──────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS news_instability ("
                "  country_code TEXT PRIMARY KEY,"
                "  cii_score INTEGER DEFAULT 0,"
                "  level TEXT DEFAULT 'STABLE',"
                "  signal_data TEXT,"
                "  updated_at INTEGER DEFAULT (strftime('%s','now'))"
                ")");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v007() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({7, "news_baselines", apply_v007});
}

} // namespace fincept
