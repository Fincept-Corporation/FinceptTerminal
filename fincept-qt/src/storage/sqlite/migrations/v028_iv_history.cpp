// v028_iv_history — Daily ATM IV history per underlying (Phase 10 polish).
//
// Driven by `OptionChainService::publish_atm_iv` which UPSERTs (underlying,
// today) on every chain Greeks-enrichment publish. Last value of the day
// wins. The IV percentile pill on FnoHeaderBar reads a trailing 90-day
// window from this table.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> v028_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v028(QSqlDatabase& db) {
    {
        auto r = v028_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS iv_history_daily (
                underlying  TEXT NOT NULL,
                date_iso    TEXT NOT NULL,
                atm_iv      REAL NOT NULL,
                updated_at  TEXT NOT NULL DEFAULT (datetime('now')),
                PRIMARY KEY (underlying, date_iso)
            ) WITHOUT ROWID
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v028_sql(db, "CREATE INDEX IF NOT EXISTS idx_iv_history_underlying_date "
                              "ON iv_history_daily(underlying, date_iso DESC)");
        if (r.is_err()) return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v028() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({28, "iv_history", apply_v028});
}

} // namespace fincept
