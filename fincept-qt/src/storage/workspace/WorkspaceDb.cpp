#include "storage/workspace/WorkspaceDb.h"

#include "core/config/ProfileManager.h"
#include "core/logging/Logger.h"

#include <QMutexLocker>
#include <QSqlError>
#include <QVariant>

namespace fincept {

namespace {
constexpr const char* kWorkspaceDbTag = "WorkspaceDB";

QString connection_name_for_profile() {
    // Scoped per profile so a future profile-switch path can briefly hold
    // two open connections (old profile + new profile) without Qt's
    // singleton registry colliding.
    const QString prof = ProfileManager::instance().active_profile_id().to_string();
    return "fincept_workspace_" + prof;
}
} // namespace

WorkspaceDb& WorkspaceDb::instance() {
    static WorkspaceDb s;
    return s;
}

Result<void> WorkspaceDb::open(const QString& path) {
    QMutexLocker lock(&mutex_);

    // Idempotent: if already open at the same path, nothing to do.
    if (db_.isOpen() && db_.databaseName() == path) {
        return Result<void>::ok();
    }
    if (db_.isOpen()) {
        // Path changed (profile switch). Close before reopening — Qt's
        // QSqlDatabase complains if a name is reused without removing it.
        db_.close();
        QSqlDatabase::removeDatabase(connection_name_);
    }

    connection_name_ = connection_name_for_profile();
    db_ = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
    db_.setDatabaseName(path);
    if (!db_.open()) {
        const QString err = db_.lastError().text();
        LOG_ERROR(kWorkspaceDbTag, QString("Failed to open %1: %2").arg(path, err));
        return Result<void>::err(err.toStdString());
    }
    LOG_INFO(kWorkspaceDbTag, "Opened workspace database: " + path);

    // Pragmas first so subsequent CREATE statements run under WAL.
    auto pr = apply_pragmas();
    if (pr.is_err())
        return pr;

    auto cr = create_tables();
    if (cr.is_err())
        return cr;

    return Result<void>::ok();
}

void WorkspaceDb::close() {
    QMutexLocker lock(&mutex_);
    if (db_.isOpen())
        db_.close();
    if (!connection_name_.isEmpty()) {
        QSqlDatabase::removeDatabase(connection_name_);
        connection_name_.clear();
    }
}

bool WorkspaceDb::is_open() const {
    QMutexLocker lock(&mutex_);
    return db_.isOpen();
}

Result<QSqlQuery> WorkspaceDb::execute(const QString& sql, const QVariantList& params) {
    QMutexLocker lock(&mutex_);
    QSqlQuery query(db_);
    query.prepare(sql);
    for (int i = 0; i < params.size(); ++i)
        query.bindValue(i, params[i]);
    if (!query.exec())
        return Result<QSqlQuery>::err(query.lastError().text().toStdString());
    return Result<QSqlQuery>::ok(std::move(query));
}

Result<void> WorkspaceDb::exec(const QString& sql) {
    QMutexLocker lock(&mutex_);
    QSqlQuery query(db_);
    if (!query.exec(sql))
        return Result<void>::err(query.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> WorkspaceDb::begin_transaction() {
    return exec("BEGIN");
}

Result<void> WorkspaceDb::commit() {
    return exec("COMMIT");
}

Result<void> WorkspaceDb::rollback() {
    return exec("ROLLBACK");
}

QString WorkspaceDb::meta(const QString& key) const {
    // Re-acquire the lock here; execute() locks too, so we go through it.
    // This is one of the rare const methods that mutates internal state via
    // the mutex; mutable QMutex makes that explicit.
    QMutexLocker lock(&mutex_);
    QSqlQuery query(db_);
    query.prepare("SELECT value FROM _meta WHERE key = ?");
    query.bindValue(0, key);
    if (!query.exec()) {
        LOG_WARN(kWorkspaceDbTag, QString("meta(%1) read failed: %2").arg(key, query.lastError().text()));
        return {};
    }
    if (query.next())
        return query.value(0).toString();
    return {};
}

Result<void> WorkspaceDb::set_meta(const QString& key, const QString& value) {
    auto r = execute("INSERT INTO _meta(key, value) VALUES(?, ?) "
                     "ON CONFLICT(key) DO UPDATE SET value = excluded.value",
                     {key, value});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

QString WorkspaceDb::path() const {
    QMutexLocker lock(&mutex_);
    return db_.databaseName();
}

Result<void> WorkspaceDb::apply_pragmas() {
    // Same combo as CacheDatabase: WAL + NORMAL gives durable-on-power-loss
    // writes with one fsync per checkpoint, not per write. busy_timeout
    // covers the brief overlap when SessionManager's autosave race-collides
    // with a closeEvent flush.
    const char* pragmas[] = {
        "PRAGMA journal_mode = WAL",
        "PRAGMA synchronous = NORMAL",
        "PRAGMA cache_size = -4000",
        "PRAGMA temp_store = MEMORY",
        "PRAGMA mmap_size = 67108864",
        "PRAGMA busy_timeout = 3000",
    };
    for (const char* p : pragmas) {
        QSqlQuery q(db_);
        if (!q.exec(p)) {
            LOG_WARN(kWorkspaceDbTag, QString("PRAGMA failed: %1 — %2").arg(p, q.lastError().text()));
        }
    }
    return Result<void>::ok();
}

Result<void> WorkspaceDb::create_tables() {
    // _meta first so version-tagging works inside this very method.
    auto r = exec("CREATE TABLE IF NOT EXISTS _meta ("
                  "  key   TEXT PRIMARY KEY,"
                  "  value TEXT"
                  ")");
    if (r.is_err()) return r;

    r = exec("CREATE TABLE IF NOT EXISTS workspace_snapshot ("
             "  snapshot_id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "  created_at  INTEGER NOT NULL,"
             "  kind        TEXT NOT NULL,"
             "  payload     BLOB NOT NULL,"
             "  name        TEXT"
             ")");
    if (r.is_err()) return r;

    // Index on (kind, created_at) makes the ring-trim query (delete oldest
    // 'auto' rows when count > 5) and recovery query (latest non-named
    // snapshot) both index lookups.
    r = exec("CREATE INDEX IF NOT EXISTS idx_snapshot_kind_created "
             "ON workspace_snapshot(kind, created_at)");
    if (r.is_err()) return r;

    r = exec("CREATE TABLE IF NOT EXISTS session_history ("
             "  session_id  TEXT PRIMARY KEY,"
             "  profile_id  TEXT NOT NULL,"
             "  started_at  INTEGER NOT NULL,"
             "  ended_at    INTEGER,"
             "  exit_kind   TEXT,"
             "  frame_count INTEGER DEFAULT 0,"
             "  panel_count INTEGER DEFAULT 0"
             ")");
    if (r.is_err()) return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_session_started "
             "ON session_history(started_at)");
    if (r.is_err()) return r;

    // Stamp schema version. Idempotent — INSERT OR IGNORE on first run,
    // future migrations bump via UPDATE.
    auto v = execute("INSERT OR IGNORE INTO _meta(key, value) VALUES('schema_version', ?)",
                     {QString::number(kSchemaVersion)});
    if (v.is_err()) return Result<void>::err(v.error());

    LOG_INFO(kWorkspaceDbTag, "Workspace tables initialised");
    return Result<void>::ok();
}

} // namespace fincept
