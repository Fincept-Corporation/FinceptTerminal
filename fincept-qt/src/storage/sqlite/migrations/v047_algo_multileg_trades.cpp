// v047_algo_multileg_trades — adds per-leg columns to algo_trades so a multi-leg
// F&O basket fill records one trade row per leg (leg_symbol = broker-native
// contract, leg_index = its slot in the basket). Both default to the equity
// behavior (empty / -1), so existing rows and the single-symbol path are
// unaffected. Idempotent on re-run (ignores the duplicate-column error, matching
// v044/v045/v046). See docs/superpowers/plans/2026-06-08-fno-algo-p3-execution.md.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> add_column(QSqlDatabase& db, const QString& sql) {
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        const QString err = q.lastError().text();
        if (!err.contains("duplicate column", Qt::CaseInsensitive))
            return Result<void>::err(err.toStdString());
    }
    return Result<void>::ok();
}

Result<void> apply_v047(QSqlDatabase& db) {
    const char* stmts[] = {
        "ALTER TABLE algo_trades ADD COLUMN leg_symbol TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE algo_trades ADD COLUMN leg_index INTEGER NOT NULL DEFAULT -1",
    };
    for (const char* s : stmts) {
        auto r = add_column(db, QString::fromUtf8(s));
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v047() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({47, "algo_multileg_trades", apply_v047});
}

} // namespace fincept
