#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> sql_v032(QSqlDatabase& db, const char* statement) {
    QSqlQuery query(db);
    if (!query.exec(statement))
        return Result<void>::err(query.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v032(QSqlDatabase& db) {
    auto result = sql_v032(db, "CREATE TABLE IF NOT EXISTS users ("
                          "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "  username TEXT NOT NULL,"
                          "  password_hash TEXT DEFAULT NULL,"
                          "  role TEXT NOT NULL DEFAULT 'standard' CHECK(role IN ('admin', 'standard')),"
                          "  status TEXT NOT NULL DEFAULT 'active' CHECK(status IN ('active', 'disabled')),"
                          "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                          "  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
                          ")");
    if (result.is_err())
        return result;

    result = sql_v032(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_users_username ON users(username)");
    if (result.is_err())
        return result;

    result = sql_v032(db,
                 "CREATE UNIQUE INDEX IF NOT EXISTS idx_users_single_active_admin "
                 "ON users(role) WHERE role = 'admin' AND status = 'active'");
    if (result.is_err())
        return result;

    result = sql_v032(db, "CREATE TABLE IF NOT EXISTS sessions ("
                          "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "  session_id TEXT NOT NULL,"
                          "  user_id INTEGER NOT NULL,"
                          "  issued_at TEXT NOT NULL DEFAULT (datetime('now')),"
                          "  last_activity TEXT NOT NULL DEFAULT (datetime('now')),"
                          "  expires_at TEXT NOT NULL,"
                          "  invalidated INTEGER NOT NULL DEFAULT 0 CHECK(invalidated IN (0, 1)),"
                          "  FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
                          ")");
    if (result.is_err())
        return result;

    result = sql_v032(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_session_id ON sessions(session_id)");
    if (result.is_err())
        return result;

    result = sql_v032(db,
                 "CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_active_user "
                 "ON sessions(user_id) WHERE invalidated = 0");
    if (result.is_err())
        return result;

    result = sql_v032(db, "CREATE TABLE IF NOT EXISTS audit_events ("
                          "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "  timestamp TEXT NOT NULL DEFAULT (datetime('now')),"
                          "  user_identity TEXT NOT NULL,"
                          "  action_type TEXT NOT NULL,"
                          "  target TEXT NOT NULL,"
                          "  result_status TEXT NOT NULL CHECK(result_status IN ('success', 'failure'))"
                          ")");
    if (result.is_err())
        return result;

    result = sql_v032(db, "CREATE INDEX IF NOT EXISTS idx_audit_events_user_identity ON audit_events(user_identity)");
    if (result.is_err())
        return result;

    result = sql_v032(db, "CREATE INDEX IF NOT EXISTS idx_audit_events_action_type ON audit_events(action_type)");
    if (result.is_err())
        return result;

    // The active-user cap (maximum three active named users) is deferred to runtime enforcement in a later task.
    // Usernames are stored as lowercase canonical values by later auth/bootstrap flows; the schema enforces uniqueness.
    return Result<void>::ok();
}

} // namespace

void register_migration_v032() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({32, "multiuser_phase_one", apply_v032});
}

} // namespace fincept
