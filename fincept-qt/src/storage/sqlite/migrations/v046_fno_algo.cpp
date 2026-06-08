// v046_fno_algo — adds F&O algo columns so an algo strategy/deployment can target
// options or futures (multi-leg). The strategy carries leg *rules*; the deployment
// carries the concrete contracts resolved at entry. Every column defaults to the
// equity behavior, so existing rows are unaffected. Idempotent on re-run (ignores
// the duplicate-column error, matching v044/v045). See
// docs/superpowers/specs/2026-06-08-fno-algo-design.md.

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

Result<void> apply_v046(QSqlDatabase& db) {
    const char* stmts[] = {
        "ALTER TABLE algo_strategies ADD COLUMN instrument_type TEXT NOT NULL DEFAULT 'equity'",
        "ALTER TABLE algo_strategies ADD COLUMN legs_json TEXT NOT NULL DEFAULT '[]'",
        "ALTER TABLE algo_deployments ADD COLUMN instrument_type TEXT NOT NULL DEFAULT 'equity'",
        "ALTER TABLE algo_deployments ADD COLUMN underlying TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE algo_deployments ADD COLUMN resolved_expiry TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE algo_deployments ADD COLUMN resolved_legs_json TEXT NOT NULL DEFAULT '[]'",
    };
    for (const char* s : stmts) {
        auto r = add_column(db, QString::fromUtf8(s));
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v046() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({46, "fno_algo", apply_v046});
}

} // namespace fincept
