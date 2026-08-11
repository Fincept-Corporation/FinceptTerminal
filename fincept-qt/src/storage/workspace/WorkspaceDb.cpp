#include "storage/workspace/WorkspaceDb.h"

#include "core/config/ProfileManager.h"
#include "core/logging/Logger.h"

#include <QMutexLocker>
#include <QSqlError>
#include <QSqlRecord>
#include <QThread>
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

QString workspace_thread_label(QThread* t) {
    if (!t)
        return QStringLiteral("?");
    const QString name = t->objectName();
    return name.isEmpty() ? QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(t), 0, 16) : name;
}
} // namespace

WorkspaceDb& WorkspaceDb::instance() {
    static WorkspaceDb s;
    return s;
}

Result<void> WorkspaceDb::open(const QString& path) {
    QMutexLocker<QRecursiveMutex> lock(&mutex_);

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

    main_thread_ = QThread::currentThread();
    db_path_ = path;
    connection_name_ = connection_name_for_profile();
    db_ = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
    db_.setDatabaseName(path);
    if (!db_.open()) {
        const QString err = db_.lastError().text();
        LOG_ERROR(kWorkspaceDbTag, QString("Failed to open %1: %2").arg(path, err));
        return Result<void>::err(err.toStdString());
    }
    // Retire any per-thread clone made against a previous profile/path before
    // declaring the database open.
    ++generation_;
    open_ = true;
    LOG_INFO(kWorkspaceDbTag, "Opened workspace database: " + path);

    // Pragmas first so subsequent CREATE statements run under WAL.
    auto pr = apply_pragmas(db_, /*include_database_wide=*/true);
    if (pr.is_err())
        return pr;

    auto cr = create_tables();
    if (cr.is_err())
        return cr;

    return Result<void>::ok();
}

void WorkspaceDb::close() {
    QMutexLocker<QRecursiveMutex> lock(&mutex_);
    open_ = false;
    ++generation_; // invalidate per-thread clones
    if (db_.isOpen())
        db_.close();
    if (!connection_name_.isEmpty()) {
        QSqlDatabase::removeDatabase(connection_name_);
        connection_name_.clear();
    }
    db_path_.clear();
}

bool WorkspaceDb::is_open() const {
    // Deliberately does NOT touch db_: this is called from worker threads and
    // QSqlDatabase::isOpen() would dereference the main thread's driver.
    return open_.load();
}

QSqlDatabase WorkspaceDb::connection() {
    QThread* t = QThread::currentThread();

    // Main-thread fast path — the connection opened by `open()`.
    if (t == main_thread_)
        return db_;

    // Per-thread cloned connection. The thread_local Guard owns the
    // connection-name lifetime so the registry entry is removed when the thread
    // exits — this handles QtConcurrent pool workers, which keep their
    // connection across tasks and clean up at pool shutdown.
    struct Guard {
        QString name;
        quint64 generation = 0;
        ~Guard() {
            if (!name.isEmpty())
                QSqlDatabase::removeDatabase(name);
        }
    };
    thread_local Guard guard;

    // Snapshot the owning connection's identity under the lock — open()/close()
    // can swap it underneath us on a profile switch.
    QString source_name;
    quint64 gen = 0;
    {
        QMutexLocker<QRecursiveMutex> lock(&mutex_);
        if (!open_.load() || connection_name_.isEmpty() || db_path_.isEmpty()) {
            LOG_ERROR(kWorkspaceDbTag, "connection() called while closed — refusing to clone");
            return {};
        }
        source_name = connection_name_;
        gen = generation_.load();
    }

    // A clone from a previous profile must not be reused against the new file.
    if (!guard.name.isEmpty() && guard.generation != gen) {
        QSqlDatabase::removeDatabase(guard.name);
        guard.name.clear();
    }

    if (guard.name.isEmpty()) {
        // Generation is part of the name so a retired clone's registry entry can
        // never collide with its replacement.
        guard.name = QStringLiteral("fincept_workspace_thread_%1_%2_%3")
                         .arg(workspace_thread_label(t))
                         .arg(reinterpret_cast<quintptr>(t), 0, 16)
                         .arg(gen);

        // Clone by connection NAME: the QSqlDatabase-object overload would read
        // the main thread's handle and its live driver from this thread, which
        // is exactly the cross-thread access this scheme exists to avoid.
        QSqlDatabase clone = QSqlDatabase::cloneDatabase(source_name, guard.name);
        if (!clone.open()) {
            const QString err = clone.lastError().text();
            LOG_ERROR(kWorkspaceDbTag, QString("Failed to open per-thread workspace connection on [%1]: %2")
                                           .arg(workspace_thread_label(t), err));
            QSqlDatabase::removeDatabase(guard.name);
            guard.name.clear();
            return {};
        }
        // journal_mode is file-global and already set by the owning connection —
        // only re-apply the per-connection PRAGMAs.
        apply_pragmas(clone, /*include_database_wide=*/false);
        guard.generation = gen;
        const int n = ++per_thread_connections_;
        LOG_INFO(kWorkspaceDbTag, QString("Opened per-thread workspace connection on [%1] (total=%2)")
                                      .arg(workspace_thread_label(t))
                                      .arg(n));
    }
    return QSqlDatabase::database(guard.name, /*open=*/false);
}

Result<QSqlQuery> WorkspaceDb::execute(const QString& sql, const QVariantList& params) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen())
        return Result<QSqlQuery>::err("Workspace DB connection unavailable on this thread");
    QSqlQuery query(conn);
    query.prepare(sql);
    for (int i = 0; i < params.size(); ++i)
        query.bindValue(i, params[i]);
    if (!query.exec())
        return Result<QSqlQuery>::err(query.lastError().text().toStdString());
    return Result<QSqlQuery>::ok(std::move(query));
}

Result<void> WorkspaceDb::exec(const QString& sql) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen())
        return Result<void>::err("Workspace DB connection unavailable on this thread");
    QSqlQuery query(conn);
    if (!query.exec(sql))
        return Result<void>::err(query.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<QVector<QVariantList>> WorkspaceDb::query_rows(const QString& sql, const QVariantList& params) {
    auto r = execute(sql, params);
    if (r.is_err())
        return Result<QVector<QVariantList>>::err(r.error());

    QVector<QVariantList> rows;
    auto& q = r.value();
    const int cols = q.record().count();
    while (q.next()) {
        QVariantList row;
        row.reserve(cols);
        for (int c = 0; c < cols; ++c)
            row.append(q.value(c));
        rows.append(std::move(row));
    }
    return Result<QVector<QVariantList>>::ok(std::move(rows));
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
    // Logically const, but it must go through execute() so the read lands on the
    // CALLING thread's connection — building a QSqlQuery from db_ here was the
    // cross-thread misuse this class no longer does. connection() lazily creates
    // that per-thread clone, hence the const_cast.
    auto r = const_cast<WorkspaceDb*>(this)->execute("SELECT value FROM _meta WHERE key = ?", {key});
    if (r.is_err()) {
        LOG_WARN(kWorkspaceDbTag, QString("meta(%1) read failed: %2").arg(key, QString::fromStdString(r.error())));
        return {};
    }
    auto& query = r.value();
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
    // Cached string, not db_.databaseName() — the latter reads the owning
    // connection's driver, which callers on other threads must not touch.
    QMutexLocker<QRecursiveMutex> lock(&mutex_);
    return db_path_;
}

Result<void> WorkspaceDb::apply_pragmas(QSqlDatabase& conn, bool include_database_wide) {
    // Same combo as CacheDatabase: WAL + NORMAL gives durable-on-power-loss
    // writes with one fsync per checkpoint, not per write. busy_timeout
    // covers the brief overlap when SessionManager's autosave race-collides
    // with a closeEvent flush.
    //
    // journal_mode is a property of the FILE — one connection sets it for all.
    // Everything else is per-connection and must be re-applied to every clone,
    // or worker threads silently run with SQLite defaults (synchronous=FULL,
    // 0 ms busy timeout → spurious SQLITE_BUSY under concurrent writers).
    static const char* kDatabaseWide[] = {
        "PRAGMA journal_mode = WAL",
    };
    static const char* kPerConnection[] = {
        "PRAGMA synchronous = NORMAL", "PRAGMA cache_size = -4000",  "PRAGMA temp_store = MEMORY",
        "PRAGMA mmap_size = 67108864", "PRAGMA busy_timeout = 3000",
    };

    auto run = [&conn](const char* sql) {
        QSqlQuery q(conn);
        if (!q.exec(QLatin1String(sql))) {
            LOG_WARN(kWorkspaceDbTag, QString("PRAGMA failed: %1 — %2").arg(sql, q.lastError().text()));
        }
    };

    if (include_database_wide) {
        for (const char* p : kDatabaseWide)
            run(p);
    }
    for (const char* p : kPerConnection)
        run(p);
    return Result<void>::ok();
}

Result<void> WorkspaceDb::create_tables() {
    // _meta first so version-tagging works inside this very method.
    auto r = exec("CREATE TABLE IF NOT EXISTS _meta ("
                  "  key   TEXT PRIMARY KEY,"
                  "  value TEXT"
                  ")");
    if (r.is_err())
        return r;

    r = exec("CREATE TABLE IF NOT EXISTS workspace_snapshot ("
             "  snapshot_id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "  created_at  INTEGER NOT NULL,"
             "  kind        TEXT NOT NULL,"
             "  payload     BLOB NOT NULL,"
             "  name        TEXT"
             ")");
    if (r.is_err())
        return r;

    // Index on (kind, created_at) makes the ring-trim query (delete oldest
    // 'auto' rows when count > 5) and recovery query (latest non-named
    // snapshot) both index lookups.
    r = exec("CREATE INDEX IF NOT EXISTS idx_snapshot_kind_created "
             "ON workspace_snapshot(kind, created_at)");
    if (r.is_err())
        return r;

    r = exec("CREATE TABLE IF NOT EXISTS session_history ("
             "  session_id  TEXT PRIMARY KEY,"
             "  profile_id  TEXT NOT NULL,"
             "  started_at  INTEGER NOT NULL,"
             "  ended_at    INTEGER,"
             "  exit_kind   TEXT,"
             "  frame_count INTEGER DEFAULT 0,"
             "  panel_count INTEGER DEFAULT 0"
             ")");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_session_started "
             "ON session_history(started_at)");
    if (r.is_err())
        return r;

    // Stamp schema version. Idempotent — INSERT OR IGNORE on first run,
    // future migrations bump via UPDATE.
    auto v = execute("INSERT OR IGNORE INTO _meta(key, value) VALUES('schema_version', ?)",
                     {QString::number(kSchemaVersion)});
    if (v.is_err())
        return Result<void>::err(v.error());

    LOG_INFO(kWorkspaceDbTag, "Workspace tables initialised");
    return Result<void>::ok();
}

} // namespace fincept
