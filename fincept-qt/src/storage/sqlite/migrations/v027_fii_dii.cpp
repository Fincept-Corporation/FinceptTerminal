// v027_fii_dii — Daily FII / DII institutional flows (Phase 8 of F&O).
//
// One row per trading day. PRIMARY KEY on date_iso so re-fetching the same
// day is an idempotent UPSERT. Values stored as REAL (₹ Crore) — losing
// the fractional precision NSE publishes (typically 2 decimal places) is
// acceptable for chart/display purposes.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> v027_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v027(QSqlDatabase& db) {
    {
        auto r = v027_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS fii_dii_daily (
                date_iso     TEXT PRIMARY KEY,
                fii_buy      REAL NOT NULL DEFAULT 0,
                fii_sell     REAL NOT NULL DEFAULT 0,
                fii_net      REAL NOT NULL DEFAULT 0,
                dii_buy      REAL NOT NULL DEFAULT 0,
                dii_sell     REAL NOT NULL DEFAULT 0,
                dii_net      REAL NOT NULL DEFAULT 0,
                fetched_at   TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v027_sql(db, "CREATE INDEX IF NOT EXISTS idx_fii_dii_date "
                              "ON fii_dii_daily(date_iso DESC)");
        if (r.is_err()) return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v027() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({27, "fii_dii", apply_v027});
}

} // namespace fincept
