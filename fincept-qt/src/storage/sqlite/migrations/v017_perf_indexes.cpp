// v017_perf_indexes — Add performance indexes for startup-critical queries.
//
// 1. idx_inst_broker   on instruments(broker_id)   — needed by load_from_db_async
//    single-broker SELECT (replacing 9 per-exchange queries).
// 2. idx_news_sort_ts  on news_articles(sort_ts)   — already on sort_ts per
//    prune_older_than query; CREATE IF NOT EXISTS is safe if it already exists.

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

Result<void> apply_v017(QSqlDatabase& db) {
    auto r = sql(db, "CREATE INDEX IF NOT EXISTS idx_inst_broker "
                     "ON instruments(broker_id)");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_news_sort_ts "
                "ON news_articles(sort_ts)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v017() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({17, "perf_indexes", apply_v017});
}

} // namespace fincept
