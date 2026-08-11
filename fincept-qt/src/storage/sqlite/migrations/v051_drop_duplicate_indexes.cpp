// v051_drop_duplicate_indexes — Remove two indices that duplicate an existing
// one. Every INSERT into the affected tables was maintaining two identical
// b-trees, doubling write cost and disk for zero query benefit.
//
//  1. news_articles(sort_ts)
//       v013 created `idx_news_articles_sort_ts ON news_articles(sort_ts DESC)`.
//       v017 then created `idx_news_sort_ts ON news_articles(sort_ts)` — its own
//       comment admits the index "already [exists] on sort_ts". SQLite can scan
//       an index in either direction, so the DESC one serves both orders.
//       Dropped: idx_news_sort_ts (the v017 duplicate).
//
//  2. market_data(symbol, exchange, interval, timestamp_ms)
//       v033 declared that exact tuple as the table's PRIMARY KEY and then
//       created `idx_market_data_series_time` on the SAME four columns in the
//       SAME order. On a non-WITHOUT-ROWID table the PK is itself a unique
//       index, so the secondary index is pure overhead on every candle insert.
//       Dropped: idx_market_data_series_time.
//
// Historical migrations are never edited — this is a new forward migration, so
// databases already at v050 converge with fresh installs.
//
// SAFE ON A POPULATED DATABASE: DROP INDEX touches no table rows and cannot
// lose data. Queries that used the dropped indices are served by the survivor
// (the DESC index / the primary key) with the same plan.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

// Uniquely named (not a bare `sql`) — unity builds concatenate ~20 TUs, and two
// anonymous-namespace helpers sharing a name in one batch is a redefinition.
Result<void> sql_v051(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v051(QSqlDatabase& db) {
    auto r = sql_v051(db, "DROP INDEX IF EXISTS idx_news_sort_ts");
    if (r.is_err())
        return r;

    r = sql_v051(db, "DROP INDEX IF EXISTS idx_market_data_series_time");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v051() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({51, "drop_duplicate_indexes", apply_v051});
}

} // namespace fincept
