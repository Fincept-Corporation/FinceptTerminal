// v013_news_articles — Persistent news article storage.
// Stores all fetched RSS articles so history is retained across refreshes.
//
// The FTS5 search index is optional: Qt's bundled SQLite has FTS5, but a
// system Qt (distro packages, Flatpak runtime) may not. Without it the
// migration still succeeds and NewsArticleRepository falls back to LIKE;
// ensure_news_fts() (called by Database::open on every start) adds the index
// later if an SQLite with FTS5 is used, or removes the sync triggers if the
// file moves to an SQLite without it.

#include "core/logging/Logger.h"
#include "storage/sqlite/SqliteSupport.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSet>
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

// ── FTS5 full-text search over headline + summary ─────────────────────────────
// content= keeps the FTS index in sync with news_articles automatically.
// tokenize=porter enables stemming (search "inflation" matches "inflationary").
Result<void> create_news_fts(QSqlDatabase& db) {
    auto r = sql(db, "CREATE VIRTUAL TABLE IF NOT EXISTS news_fts USING fts5("
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

    if (!db::has_fts5(db)) {
        LOG_WARN("DB", "v013: this SQLite library has no FTS5 — news search uses plain text matching. The search "
                       "index is added automatically when an SQLite with FTS5 is used.");
        return Result<void>::ok();
    }
    return create_news_fts(db);
}

} // anonymous namespace

Result<void> ensure_news_fts(QSqlDatabase& db, bool fts5_available) {
    QSet<QString> present;
    {
        QSqlQuery q(db);
        if (!q.exec("SELECT name FROM sqlite_master WHERE name IN "
                    "('news_articles','news_fts','news_fts_ai','news_fts_ad','news_fts_au')"))
            return Result<void>::err(q.lastError().text().toStdString());
        while (q.next())
            present.insert(q.value(0).toString().toLower());
    }
    if (!present.contains(QStringLiteral("news_articles")))
        return Result<void>::ok();

    const bool triggers_complete = present.contains(QStringLiteral("news_fts_ai")) &&
                                   present.contains(QStringLiteral("news_fts_ad")) &&
                                   present.contains(QStringLiteral("news_fts_au"));
    const bool any_trigger = present.contains(QStringLiteral("news_fts_ai")) ||
                             present.contains(QStringLiteral("news_fts_ad")) ||
                             present.contains(QStringLiteral("news_fts_au"));

    if (fts5_available && present.contains(QStringLiteral("news_fts")) && triggers_complete)
        return Result<void>::ok();
    if (!fts5_available && !any_trigger)
        return Result<void>::ok();

    QSqlQuery tx(db);
    if (!tx.exec("BEGIN IMMEDIATE"))
        return Result<void>::err(tx.lastError().text().toStdString());

    Result<void> r = Result<void>::ok();
    if (fts5_available) {
        // Created late (or its triggers were removed while the file was used
        // without FTS5): build it, then index every existing article.
        r = create_news_fts(db);
        if (r.is_ok())
            r = sql(db, "INSERT INTO news_fts(news_fts) VALUES('rebuild')");
    } else {
        // Without the fts5 module every trigger firing fails ("no such module"),
        // which would make every news insert, update and delete fail.
        r = sql(db, "DROP TRIGGER IF EXISTS news_fts_ai");
        if (r.is_ok())
            r = sql(db, "DROP TRIGGER IF EXISTS news_fts_ad");
        if (r.is_ok())
            r = sql(db, "DROP TRIGGER IF EXISTS news_fts_au");
    }
    if (r.is_err()) {
        tx.exec("ROLLBACK");
        return r;
    }
    if (!tx.exec("COMMIT")) {
        const std::string e = tx.lastError().text().toStdString();
        tx.exec("ROLLBACK");
        return Result<void>::err(e);
    }
    if (fts5_available)
        LOG_INFO("DB", "News search index (FTS5) created and filled from existing articles");
    else
        LOG_WARN("DB", "This SQLite library has no FTS5: removed the news search index triggers so news can still be "
                       "saved; search uses plain text matching");
    return Result<void>::ok();
}

void register_migration_v013() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({13, "news_articles", apply_v013});
}

} // namespace fincept
