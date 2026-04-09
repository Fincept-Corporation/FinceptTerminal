// v013_news_articles — Persistent news article storage.
// Stores all fetched RSS articles so history is retained across refreshes.

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

Result<void> apply_v013(QSqlDatabase& db) {

    // ── Persistent article store ───────────────────────────────────────────────
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS news_articles ("
                     "  id           TEXT PRIMARY KEY,"
                     "  headline     TEXT NOT NULL,"
                     "  summary      TEXT,"
                     "  source       TEXT,"
                     "  region       TEXT,"
                     "  category     TEXT,"
                     "  link         TEXT,"
                     "  sort_ts      INTEGER,"
                     "  priority     TEXT,"
                     "  sentiment    TEXT,"
                     "  impact       TEXT,"
                     "  tickers      TEXT," // JSON array ["AAPL","MSFT"]
                     "  tier         INTEGER DEFAULT 4,"
                     "  lang         TEXT,"
                     "  threat_level TEXT,"
                     "  threat_cat   TEXT,"
                     "  threat_conf  REAL DEFAULT 0,"
                     "  source_flag  INTEGER DEFAULT 0,"
                     "  fetched_at   INTEGER DEFAULT (strftime('%s','now'))"
                     ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_news_articles_sort_ts "
                "ON news_articles(sort_ts DESC)");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_news_articles_category "
                "ON news_articles(category)");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_news_articles_source "
                "ON news_articles(source)");
    if (r.is_err())
        return r;

    // ── FTS5 full-text search over headline + summary ─────────────────────────
    // content= keeps the FTS index in sync with news_articles automatically.
    // tokenize=porter enables stemming (search "inflation" matches "inflationary").
    r = sql(db, "CREATE VIRTUAL TABLE IF NOT EXISTS news_fts USING fts5("
                "  id UNINDEXED,"
                "  headline,"
                "  summary,"
                "  source UNINDEXED,"
                "  content='news_articles',"
                "  content_rowid='rowid',"
                "  tokenize='porter unicode61'"
                ")");
    if (r.is_err())
        return r;

    // Triggers to keep FTS in sync with base table inserts/deletes
    r = sql(db, "CREATE TRIGGER IF NOT EXISTS news_fts_ai AFTER INSERT ON news_articles BEGIN"
                "  INSERT INTO news_fts(rowid, id, headline, summary, source)"
                "  VALUES (new.rowid, new.id, new.headline, new.summary, new.source);"
                "END");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE TRIGGER IF NOT EXISTS news_fts_ad AFTER DELETE ON news_articles BEGIN"
                "  INSERT INTO news_fts(news_fts, rowid, id, headline, summary, source)"
                "  VALUES ('delete', old.rowid, old.id, old.headline, old.summary, old.source);"
                "END");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE TRIGGER IF NOT EXISTS news_fts_au AFTER UPDATE ON news_articles BEGIN"
                "  INSERT INTO news_fts(news_fts, rowid, id, headline, summary, source)"
                "  VALUES ('delete', old.rowid, old.id, old.headline, old.summary, old.source);"
                "  INSERT INTO news_fts(rowid, id, headline, summary, source)"
                "  VALUES (new.rowid, new.id, new.headline, new.summary, new.source);"
                "END");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v013() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({13, "news_articles", apply_v013});
}

} // namespace fincept
