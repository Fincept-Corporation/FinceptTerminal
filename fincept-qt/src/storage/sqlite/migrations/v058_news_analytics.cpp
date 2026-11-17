// v058_news_analytics — News: persisted tone, validated tags, volume history,
// symbol aliases, saved searches and feed health.
//
//   news_articles      + sentiment_score REAL (NULL = not scored / unknown — rows
//                        stored before this migration stay unknown, never ±0.5),
//                        event_type (market-impact event: RATES, EARNINGS, M&A ...),
//                        ticker_conf (JSON confidences parallel to `tickers`; ''
//                        marks a row tagged by the old capitalised-word guesser),
//                        unverified_tickers (JSON), and the runtime-added `saved`
//                        / `seen_at` columns when missing; partial index on saved
//                        bookmarks
//   news_volume_hourly (new) — articles per hour per category / region / language,
//                        written for every hour the app observed, so volume
//                        baselines (DEVIATIONS) survive restarts
//   news_symbol_aliases (new) — company name <-> symbol learned from watchlists,
//                        holdings and symbol searches (the tagger's local table)
//   news_saved_searches (new) — full filter state + alert rule (ticker /
//                        sentiment threshold / volume spike, quiet hours)
//   news_feed_health    (new) — per-feed last success / failure, so failing
//                        feeds back off across restarts and are shown to the user
//
// The v007 tables news_baselines / news_entity_cache / news_instability were
// never read or written; they are left in place (dropping user tables is not
// worth the risk) and superseded by news_volume_hourly.
//
// SAFE ON A POPULATED DATABASE: ADD COLUMN with defaults, new tables and
// indexes only; anything that already exists is skipped. Idempotent.
//
// REGISTRATION: declare `void register_migration_v058();` in MigrationRunner.h
// and call it from main.cpp after register_migration_v057().

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v058();

namespace {

Result<void> sql_v058(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(QString::fromUtf8(stmt)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

bool v058_has_table(QSqlDatabase& db, const QString& table) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?"));
    q.addBindValue(table);
    return q.exec() && q.next();
}

bool v058_has_column(QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return false;
    while (q.next())
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

Result<void> v058_add_column(QSqlDatabase& db, const char* table, const char* column, const char* decl) {
    const QString t = QString::fromUtf8(table);
    const QString c = QString::fromUtf8(column);
    if (v058_has_column(db, t, c))
        return Result<void>::ok();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(t, c, QString::fromUtf8(decl)))) {
        const QString err = q.lastError().text();
        if (!err.contains(QStringLiteral("duplicate column"), Qt::CaseInsensitive))
            return Result<void>::err(err.toStdString());
    }
    return Result<void>::ok();
}

Result<void> apply_v058(QSqlDatabase& db) {
    // news_articles is created by v013; a database without it has nothing to extend.
    if (v058_has_table(db, QStringLiteral("news_articles"))) {
        struct Col {
            const char* column;
            const char* decl;
        };
        const Col cols[] = {
            {"sentiment_score", "REAL"}, // NULL = unknown / not scored
            {"event_type", "TEXT DEFAULT ''"},
            {"ticker_conf", "TEXT DEFAULT ''"},
            {"unverified_tickers", "TEXT DEFAULT ''"},
            {"saved", "INTEGER DEFAULT 0"},
            {"seen_at", "INTEGER"},
        };
        for (const auto& c : cols) {
            auto r = v058_add_column(db, "news_articles", c.column, c.decl);
            if (r.is_err())
                return r;
        }
        auto r = sql_v058(db, "CREATE INDEX IF NOT EXISTS idx_news_articles_saved ON news_articles(sort_ts) "
                              "WHERE saved = 1");
        if (r.is_err())
            return r;
    }

    auto r = sql_v058(db, "CREATE TABLE IF NOT EXISTS news_volume_hourly ("
                          "  hour_ts INTEGER NOT NULL," // unix seconds, start of the hour (UTC)
                          "  dim     TEXT NOT NULL,"    // 'all' | 'cat' | 'region' | 'lang'
                          "  key     TEXT NOT NULL,"    // '' for dim 'all'
                          "  count   INTEGER NOT NULL DEFAULT 0,"
                          "  scored  INTEGER NOT NULL DEFAULT 0,"
                          "  score_sum REAL NOT NULL DEFAULT 0,"
                          "  PRIMARY KEY (hour_ts, dim, key)"
                          ")");
    if (r.is_err())
        return r;

    r = sql_v058(db, "CREATE TABLE IF NOT EXISTS news_symbol_aliases ("
                     "  alias      TEXT NOT NULL,"
                     "  symbol     TEXT NOT NULL,"
                     "  name       TEXT NOT NULL DEFAULT '',"
                     "  source     TEXT NOT NULL DEFAULT '',"
                     "  updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
                     "  PRIMARY KEY (alias, symbol)"
                     ")");
    if (r.is_err())
        return r;

    r = sql_v058(db, "CREATE TABLE IF NOT EXISTS news_saved_searches ("
                     "  id         TEXT PRIMARY KEY,"
                     "  name       TEXT NOT NULL,"
                     "  state_json TEXT NOT NULL DEFAULT '{}',"
                     "  alert_json TEXT NOT NULL DEFAULT '{}',"
                     "  created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
                     "  updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
                     ")");
    if (r.is_err())
        return r;

    return sql_v058(db, "CREATE TABLE IF NOT EXISTS news_feed_health ("
                        "  feed_id              TEXT PRIMARY KEY,"
                        "  last_ok_at           INTEGER NOT NULL DEFAULT 0,"
                        "  last_fail_at         INTEGER NOT NULL DEFAULT 0,"
                        "  consecutive_failures INTEGER NOT NULL DEFAULT 0,"
                        "  last_error           TEXT NOT NULL DEFAULT '',"
                        "  last_http            INTEGER NOT NULL DEFAULT 0,"
                        "  last_count           INTEGER NOT NULL DEFAULT 0"
                        ")");
}

} // anonymous namespace

void register_migration_v058() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({58, "news_analytics", apply_v058});
}

} // namespace fincept
