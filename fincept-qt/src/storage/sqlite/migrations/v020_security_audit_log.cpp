// v020_security_audit_log — Persistent audit trail for PIN / lock-screen
// security events. Used by SecurityAuditService to record PIN setup, change,
// failed unlock attempts, lockout transitions, inactivity locks, and
// resume-from-sleep locks. Read back by Settings → Security → Audit log.

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

Result<void> apply_v020(QSqlDatabase& db) {
    // `event` is a short stable identifier (e.g. "pin_set", "pin_verify_fail").
    // `detail` holds free-form context (attempt number, lockout seconds, etc.).
    // `ts` is Unix epoch seconds — indexed for recent-events queries.
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS security_audit_log ("
                     "  id     INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "  ts     INTEGER NOT NULL,"
                     "  event  TEXT    NOT NULL,"
                     "  detail TEXT    DEFAULT ''"
                     ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_security_audit_ts "
                "ON security_audit_log(ts DESC)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v020() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({20, "security_audit_log", apply_v020});
}

} // namespace fincept
