#include "storage/workspace/WorkspaceDb.h"

#include "core/config/ProfileManager.h"
#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "storage/sqlite/SqliteSupport.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QMutexLocker>
#include <QSet>
#include <QSqlError>
#include <QSqlRecord>
#include <QStringList>
#include <QThread>
#include <QVariant>

#include <functional>

namespace fincept {

namespace {
constexpr const char* kWorkspaceDbTag = "WorkspaceDB";

// Every connection to workspace.db, owning and per-thread clones. busy_timeout
// covers SessionManager's autosave colliding with a closeEvent flush and the
// retention sweeper; journal_size_limit bounds the -wal after a checkpoint.
const db::PragmaProfile kWorkspacePragmas{/*cache_kb=*/4000, /*busy_timeout_ms=*/5000, /*foreign_keys=*/false,
                                          /*journal_size_limit=*/16LL * 1024 * 1024};

// ── Open gate ────────────────────────────────────────────────────────────────
// LOCAL COPY: the same gate (GateVerdict, GateResult, TableSpec, verify_tables,
// run_open_gate) is in CacheDatabase.cpp and LayoutCatalog.cpp, and
// quarantine_db_files() is also in LayoutCatalog.cpp. They belong in
// storage/sqlite/SqliteSupport; fold the copies into one there.
// Its own named namespace because the unity build merges this file's anonymous
// namespace with the other copies; pull it in with a FUNCTION-scope
// `using namespace workspace_gate;` only (a namespace-scope one would leak).
namespace workspace_gate {

enum class GateVerdict {
    Ok,    ///< schema current and verified
    Reset, ///< damaged, or written by another build: replace the file
    Fail,  ///< environmental (busy, read-only, disk full, permissions): leave the file alone
};

struct GateResult {
    GateVerdict verdict = GateVerdict::Fail;
    QString label;  ///< "corrupt" or "incompatible" when verdict == Reset
    QString reason; ///< for the log only (may carry driver text)
    db::SqlErrorKind kind = db::SqlErrorKind::None;
    int found_version = 0;  ///< PRAGMA user_version the file had
    bool was_empty = false; ///< the file had no tables (new file)
};

struct TableSpec {
    QString table;
    QString pk;          ///< column that must be part of the primary key
    QStringList columns; ///< columns that must exist; extra columns are allowed
};

bool is_environmental(db::SqlErrorKind k) {
    switch (k) {
        case db::SqlErrorKind::Busy:
        case db::SqlErrorKind::Locked:
        case db::SqlErrorKind::ReadOnly:
        case db::SqlErrorKind::Full:
        case db::SqlErrorKind::CantOpen:
        case db::SqlErrorKind::IoErr:
            return true;
        default:
            return false;
    }
}

GateResult gate_reset(const QString& label, const QString& reason, db::SqlErrorKind kind = db::SqlErrorKind::None) {
    GateResult g;
    g.verdict = GateVerdict::Reset;
    g.label = label;
    g.reason = reason;
    g.kind = kind;
    return g;
}

/// A driver error while reading the file: damage resets it, anything else
/// (busy, read-only, I/O, permissions) leaves it alone.
GateResult gate_error(db::SqlErrorKind kind, const QString& reason) {
    if (kind == db::SqlErrorKind::Corrupt || kind == db::SqlErrorKind::NotADb)
        return gate_reset(QStringLiteral("corrupt"), reason, kind);
    GateResult g;
    g.verdict = GateVerdict::Fail;
    g.reason = reason;
    g.kind = kind;
    return g;
}

/// Every table in `specs` exists, has at least the listed columns, and has `pk`
/// in its primary key (read through pragma_table_info).
bool verify_tables(QSqlDatabase& db, const QList<TableSpec>& specs, QString* problem) {
    for (const TableSpec& spec : specs) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT name, pk FROM pragma_table_info(?)"));
        q.addBindValue(spec.table);
        if (!q.exec()) {
            *problem = QStringLiteral("cannot read the columns of %1: %2").arg(spec.table, q.lastError().text());
            return false;
        }
        QSet<QString> have;
        bool pk_ok = spec.pk.isEmpty();
        while (q.next()) {
            const QString name = q.value(0).toString().toLower();
            have.insert(name);
            if (name == spec.pk.toLower() && q.value(1).toInt() > 0)
                pk_ok = true;
        }
        if (have.isEmpty()) {
            *problem = QStringLiteral("table %1 is missing").arg(spec.table);
            return false;
        }
        for (const QString& col : spec.columns) {
            if (!have.contains(col.toLower())) {
                *problem = QStringLiteral("table %1 has no column %2").arg(spec.table, col);
                return false;
            }
        }
        if (!pk_ok) {
            *problem = QStringLiteral("%1.%2 is not the primary key").arg(spec.table, spec.pk);
            return false;
        }
    }
    return true;
}

/// Read-only checks first (is it SQLite, is it intact, which schema version),
/// then `build(from_version)` plus the column check in ONE `BEGIN IMMEDIATE`
/// transaction that also stamps PRAGMA user_version = `version`. A file that
/// fails any check is rolled back and never written to. Journal mode and the
/// other PRAGMAs are the caller's job AFTER an Ok verdict, so a file that is
/// about to be replaced is not even switched to WAL. `legacy_check` runs for
/// files with tables but user_version 0 and returns a reason to reject, or "".
GateResult run_open_gate(QSqlDatabase& db, int version, const QList<TableSpec>& specs,
                         const std::function<Result<void>(int from_version)>& build,
                         const std::function<QString()>& legacy_check = {}) {
    auto run = [&db](const QString& sql, QSqlError* error) {
        QSqlQuery q(db);
        if (q.exec(sql))
            return true;
        if (error)
            *error = q.lastError();
        return false;
    };

    // 1. SQLite opens lazily: this is the first read of the file, and where
    //    "file is not a database" shows up.
    int tables = 0;
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("SELECT count(*) FROM sqlite_master")) || !q.next())
            return gate_error(db::classify(q.lastError()), "cannot read the schema: " + q.lastError().text());
        tables = q.value(0).toInt();
    }

    // 2. Damaged pages.
    if (tables > 0) {
        db::SqlErrorKind kind = db::SqlErrorKind::None;
        auto qc = db::quick_check(db, &kind);
        if (qc.is_err())
            return gate_error(kind, QString::fromStdString(qc.error()));
    }

    // 3. Schema version. 0 = new file or one from before versioning.
    int found = 0;
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next())
            return gate_error(db::classify(q.lastError()), "cannot read user_version: " + q.lastError().text());
        found = q.value(0).toInt();
    }
    if (found < 0 || found > version) {
        return gate_reset(QStringLiteral("incompatible"),
                          QStringLiteral("schema version %1 is newer than or unknown to this build (it supports up to %2)")
                              .arg(found)
                              .arg(version));
    }
    if (found == 0 && tables > 0 && legacy_check) {
        const QString why = legacy_check();
        if (!why.isEmpty())
            return gate_reset(QStringLiteral("incompatible"), why);
    }

    // 4. Create, upgrade, verify, stamp: all or nothing.
    QSqlError error;
    if (!run(QStringLiteral("BEGIN IMMEDIATE"), &error))
        return gate_error(db::classify(error), "cannot start the schema transaction: " + error.text());
    auto rolled_back = [&run](GateResult g) {
        run(QStringLiteral("ROLLBACK"), nullptr);
        return g;
    };

    auto built = build(found);
    if (built.is_err()) {
        const QString text = QString::fromStdString(built.error());
        const db::SqlErrorKind kind = db::classify_text(text);
        if (is_environmental(kind))
            return rolled_back(gate_error(kind, "schema setup failed: " + text));
        const bool damaged = (kind == db::SqlErrorKind::Corrupt || kind == db::SqlErrorKind::NotADb);
        return rolled_back(gate_reset(damaged ? QStringLiteral("corrupt") : QStringLiteral("incompatible"),
                                      "schema setup failed: " + text, kind));
    }
    QString problem;
    if (!verify_tables(db, specs, &problem))
        return rolled_back(
            gate_reset(QStringLiteral("incompatible"), "column check failed: " + problem, db::SqlErrorKind::Schema));
    if (found != version && !run(QStringLiteral("PRAGMA user_version = %1").arg(version), &error))
        return rolled_back(gate_error(db::classify(error), "cannot stamp user_version: " + error.text()));
    if (!run(QStringLiteral("COMMIT"), &error))
        return rolled_back(gate_error(db::classify(error), "cannot commit the schema: " + error.text()));

    GateResult ok;
    ok.verdict = GateVerdict::Ok;
    ok.found_version = found;
    ok.was_empty = (tables == 0);
    return ok;
}

/// Move a database that must not be used out of the way WITHOUT deleting it:
/// `<path>` becomes `<path>.<label>-<yyyyMMdd-hhmmss>`, and its -wal/-journal
/// get the same name plus their suffix so the moved copy stays openable. -shm
/// is a rebuildable index and is deleted. Side files move first: a -wal left
/// next to the new, empty file would be replayed into it. Returns the new path,
/// or "" with `problem` set (everything put back). The connection must already
/// be closed — Windows cannot rename an open file.
QString quarantine_db_files(const QString& path, const QString& label, QString* problem) {
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    QString target = QStringLiteral("%1.%2-%3").arg(path, label, stamp);
    for (int n = 2; QFile::exists(target) && n < 100; ++n)
        target = QStringLiteral("%1.%2-%3-%4").arg(path, label, stamp).arg(n);

    const QStringList side_suffixes = {QStringLiteral("-wal"), QStringLiteral("-journal")};
    QStringList moved;
    auto put_back = [&]() {
        for (const QString& suffix : moved)
            QFile::rename(target + suffix, path + suffix);
    };
    for (const QString& suffix : side_suffixes) {
        const QString side = path + suffix;
        if (!QFile::exists(side))
            continue;
        if (!QFile::rename(side, target + suffix)) {
            *problem = QStringLiteral("cannot move %1 aside").arg(side);
            put_back();
            return {};
        }
        moved << suffix;
    }
    QFile::remove(path + QStringLiteral("-shm"));
    if (QFile::exists(path) && !QFile::rename(path, target)) {
        *problem = QStringLiteral("cannot rename %1 to %2").arg(path, target);
        put_back();
        return {};
    }
    return target;
}
} // namespace workspace_gate
// ── end of open gate ─────────────────────────────────────────────────────────

/// Columns every workspace.db table must have at kSchemaVersion.
/// telemetry_events (left in some databases by older builds) is not checked.
QList<workspace_gate::TableSpec> expected_workspace_tables() {
    return {
        {QStringLiteral("_meta"), QStringLiteral("key"), {"key", "value"}},
        {QStringLiteral("workspace_snapshot"),
         QStringLiteral("snapshot_id"),
         {"snapshot_id", "created_at", "kind", "payload", "name"}},
        {QStringLiteral("session_history"),
         QStringLiteral("session_id"),
         {"session_id", "profile_id", "started_at", "ended_at", "exit_kind", "frame_count", "panel_count"}},
    };
}

/// Builds before user_version existed stamped `_meta.schema_version` = "1".
/// A file with tables, user_version 0 and any OTHER value there came from a
/// build with a different workspace schema. No _meta (or another shape) is
/// left to the column check.
QString workspace_legacy_check(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT value FROM _meta WHERE key = 'schema_version'")) || !q.next())
        return {};
    const QString v = q.value(0).toString().trimmed();
    bool ok = false;
    const int n = v.toInt(&ok);
    if (v.isEmpty() || (ok && n >= 1 && n <= WorkspaceDb::kSchemaVersion))
        return {};
    return QStringLiteral("_meta.schema_version is '%1' but user_version is 0 (this build writes %2)")
        .arg(v)
        .arg(WorkspaceDb::kSchemaVersion);
}

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
    if (open_.load() && db_path_ == path)
        return Result<void>::ok();
    if (open_.load() || !connection_name_.isEmpty()) {
        // Path changed (profile switch), or a previous close() ran off-thread:
        // orderly close first — Qt's registry complains if a name is reused
        // without removing it. Recursive mutex, so this nests.
        close();
    }

    main_thread_ = QThread::currentThread();
    connection_name_ = connection_name_for_profile();

    using namespace workspace_gate; // function scope only — see the gate's comment
    const QList<TableSpec> expected = expected_workspace_tables();
    auto attempt = [&]() -> GateResult {
        release_connection_locked();
        db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name_);
        db_.setDatabaseName(path);
        if (!db_.open())
            return gate_error(db::classify(db_.lastError()), "cannot open: " + db_.lastError().text());
        GateResult g = run_open_gate(
            db_, kSchemaVersion, expected, [this](int from) { return create_tables(from); },
            [this]() { return workspace_legacy_check(db_); });
        // WAL (with read-back and fallback), mmap off, busy timeout — only once
        // the file is known to be ours.
        if (g.verdict == GateVerdict::Ok)
            journal_mode_ = db::apply_pragmas(db_, kWorkspacePragmas, /*set_journal_mode=*/true);
        return g;
    };

    GateResult g = attempt();
    if (g.verdict == GateVerdict::Reset) {
        // Never delete: the file may hold another build's workspace, or be
        // salvageable. Move it aside and start a fresh one.
        release_connection_locked();
        QString problem;
        const QString moved = quarantine_db_files(path, g.label, &problem);
        if (moved.isEmpty()) {
            g.verdict = GateVerdict::Fail;
            g.reason = QStringLiteral("file is %1 (%2) and could not be moved aside: %3").arg(g.label, g.reason, problem);
        } else {
            LOG_WARN(kWorkspaceDbTag,
                     QString("Workspace database unusable, moved aside and starting empty state=\"%1\" reason=\"%2\" "
                             "moved_to=%3")
                         .arg(g.label, privacy::redact_text(g.reason).left(200), privacy::redact_home(moved)));
            g = attempt();
            if (g.verdict == GateVerdict::Reset) // quarantine once, never loop
                g.verdict = GateVerdict::Fail;
        }
    }
    if (g.verdict != GateVerdict::Ok) {
        // Never leave the wrapper "open" on a broken connection.
        release_connection_locked();
        connection_name_.clear();
        db_path_.clear();
        const QString msg =
            QString("workspace.db unavailable [%1]: %2").arg(QString::fromLatin1(db::kind_name(g.kind)), g.reason);
        LOG_ERROR(kWorkspaceDbTag, QString("Workspace database could not be opened kind=%1 path=%2 error=\"%3\"")
                                       .arg(QString::fromLatin1(db::kind_name(g.kind)), privacy::redact_home(path),
                                            privacy::redact_text(g.reason).left(200)));
        return Result<void>::err(msg.toStdString());
    }

    db_path_ = path;
    // Retire any per-thread clone made against a previous profile/path before
    // declaring the database open.
    ++generation_;
    open_ = true;

    QString schema_note;
    if (g.was_empty)
        schema_note = QStringLiteral("created schema v%1").arg(kSchemaVersion);
    else if (g.found_version == 0)
        schema_note = QStringLiteral("stamped unversioned file as schema v%1").arg(kSchemaVersion);
    else if (g.found_version != kSchemaVersion)
        schema_note = QStringLiteral("upgraded schema v%1 -> v%2").arg(g.found_version).arg(kSchemaVersion);
    else
        schema_note = QStringLiteral("schema v%1").arg(kSchemaVersion);
    LOG_DEBUG(kWorkspaceDbTag,
             QString("Workspace database opened path=%1 schema=\"%2\" journal_mode=%3")
                 .arg(privacy::redact_home(path), schema_note, journal_mode_.isEmpty() ? QStringLiteral("?") : journal_mode_));
    return Result<void>::ok();
}

void WorkspaceDb::close() {
    QMutexLocker<QRecursiveMutex> lock(&mutex_);
    const bool was_open = open_.exchange(false);
    ++generation_; // invalidate per-thread clones
    if (main_thread_ && QThread::currentThread() != main_thread_) {
        // db_ belongs to the opening thread. Marked closed above, so every
        // connection() refuses; the next open() on the owning thread releases it.
        LOG_WARN(kWorkspaceDbTag, "close() called off the thread that opened workspace.db — marked closed only");
        return;
    }
    if (was_open && db_.isOpen()) {
        {
            QSqlQuery q(db_);
            if (!q.exec(QStringLiteral("PRAGMA optimize")))
                LOG_WARN(kWorkspaceDbTag, "PRAGMA optimize failed: " + q.lastError().text());
        }
        // Fold the -wal back into workspace.db and truncate it (no-op unless WAL).
        db::checkpoint_truncate(db_);
    }
    release_connection_locked();
    connection_name_.clear();
    db_path_.clear();
    if (was_open)
        LOG_DEBUG(kWorkspaceDbTag, "Workspace database closed");
}

void WorkspaceDb::release_connection_locked() {
    if (db_.isValid() && db_.isOpen())
        db_.close();
    // Drop our handle BEFORE removeDatabase, or Qt warns the connection is
    // still in use.
    db_ = QSqlDatabase();
    if (!connection_name_.isEmpty() && QSqlDatabase::contains(connection_name_))
        QSqlDatabase::removeDatabase(connection_name_);
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
            // Same guard as Database/CacheDatabase: pool threads (the retention
            // sweeper writes here from QThreadPool::globalInstance()) can exit
            // after QCoreApplication and Qt SQL's connection registry are gone,
            // and removeDatabase() then dereferences freed state — a crash on
            // quit. The process is ending; the OS reclaims the handle.
            if (QCoreApplication::instance() == nullptr || QCoreApplication::closingDown())
                return;
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
        // only re-apply the per-connection PRAGMAs (synchronous matched to it).
        db::apply_pragmas(clone, kWorkspacePragmas, /*set_journal_mode=*/false);
        guard.generation = gen;
        const int n = ++per_thread_connections_;
        LOG_DEBUG(kWorkspaceDbTag, QString("Opened per-thread workspace connection on [%1] (total=%2)")
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
    // Check prepare() — see Database::execute(). Unchecked, "no such table" or
    // "no such column" surfaced as a misleading "Parameter count mismatch" at
    // exec time, because bindValue() no-ops on an unprepared statement.
    if (!query.prepare(sql))
        return Result<QSqlQuery>::err("prepare failed: " + query.lastError().text().toStdString());
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
    // IMMEDIATE, like Database: a deferred BEGIN takes the write lock only at
    // the first write, and a read-then-write transaction then fails with
    // SQLITE_BUSY at once instead of waiting on busy_timeout.
    return exec("BEGIN IMMEDIATE");
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

Result<void> WorkspaceDb::create_tables(int from_version) {
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

    // ── In-file upgrade steps ────────────────────────────────────────────────
    // Each step runs only for files older than the version that introduced it,
    // inside open()'s transaction (a failure rolls the whole open back), and must
    // be idempotent. To add one: bump kSchemaVersion, add `if (from_version < N)`
    // here, and add the new columns to expected_workspace_tables().
    //
    // v1 is the first versioned schema: unversioned files (user_version 0) from
    // earlier builds already have exactly these tables, so they need no step.
    Q_UNUSED(from_version);

    // Mirror the version in _meta for builds that predate user_version. Inside
    // the open transaction, so it lands together with PRAGMA user_version.
    auto v = execute("INSERT INTO _meta(key, value) VALUES('schema_version', ?) "
                     "ON CONFLICT(key) DO UPDATE SET value = excluded.value",
                     {QString::number(kSchemaVersion)});
    if (v.is_err())
        return Result<void>::err(v.error());

    LOG_DEBUG(kWorkspaceDbTag, "Workspace tables initialised");
    return Result<void>::ok();
}

} // namespace fincept
