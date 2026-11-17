#include "storage/sqlite/CacheDatabase.h"
#include <QCoreApplication>

#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "storage/sqlite/SqliteSupport.h"

#include <QFile>
#include <QSet>
#include <QSqlRecord>
#include <QStringList>
#include <QThread>

#include <functional>

namespace fincept {

namespace {
constexpr const char* kCacheConnectionName = "fincept_cache";
constexpr const char* kCacheDbTag = "CacheDB";

// Every connection to cache.db, owning and per-thread clones. The schema has no
// foreign keys; journal_size_limit bounds the -wal after a checkpoint (without it
// a one-off burst leaves the file at its high-water mark).
const db::PragmaProfile kCachePragmas{/*cache_kb=*/10000, /*busy_timeout_ms=*/5000, /*foreign_keys=*/false,
                                      /*journal_size_limit=*/64LL * 1024 * 1024};

QString cache_thread_label(QThread* t) {
    if (!t)
        return QStringLiteral("?");
    const QString name = t->objectName();
    return name.isEmpty() ? QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(t), 0, 16) : name;
}

// ── Open gate ────────────────────────────────────────────────────────────────
// LOCAL COPY: the same gate (GateVerdict, GateResult, TableSpec, verify_tables,
// run_open_gate) is in WorkspaceDb.cpp and LayoutCatalog.cpp. It belongs in
// storage/sqlite/SqliteSupport; fold the three copies into one there.
// Its own named namespace because the unity build merges this file's anonymous
// namespace with the other copies; pull it in with a FUNCTION-scope
// `using namespace cache_gate;` only (a namespace-scope one would leak).
namespace cache_gate {

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
} // namespace cache_gate
// ── end of open gate ─────────────────────────────────────────────────────────

/// Columns every cache.db table must have at kSchemaVersion.
QList<cache_gate::TableSpec> expected_cache_tables() {
    return {
        {QStringLiteral("unified_cache"),
         QStringLiteral("key"),
         {"key", "value", "category", "content_type", "ttl_seconds", "expires_at", "created_at", "hit_count",
          "size_bytes"}},
        {QStringLiteral("tab_sessions"),
         QStringLiteral("tab_id"),
         {"tab_id", "screen_name", "scroll_position", "filters_json", "selections_json", "last_accessed",
          "updated_at"}},
        {QStringLiteral("screen_state"),
         QStringLiteral("screen_key"),
         {"screen_key", "state_version", "state_json", "updated_at", "session_id", "instance_uuid"}},
    };
}

/// Delete cache.db and its side files. The -wal goes first: one left behind
/// next to a new, empty cache.db would be replayed into it. False (with
/// `problem`) when a file survives — on Windows, another process holding it.
bool remove_cache_files(const QString& path, QString* problem) {
    const QString wal = path + QStringLiteral("-wal");
    if (QFile::exists(wal) && !QFile::remove(wal)) {
        *problem = QStringLiteral("cannot delete %1").arg(wal);
        return false;
    }
    QFile::remove(path + QStringLiteral("-shm"));
    QFile::remove(path + QStringLiteral("-journal"));
    if (QFile::exists(path) && !QFile::remove(path)) {
        *problem = QStringLiteral("cannot delete %1").arg(path);
        return false;
    }
    return true;
}
} // namespace

CacheDatabase& CacheDatabase::instance() {
    static CacheDatabase s;
    return s;
}

Result<void> CacheDatabase::open(const QString& path) {
    if (open_.load(std::memory_order_acquire)) {
        if (path == db_path_)
            return Result<void>::ok();
        shutdown(); // reopening at another path
    }
    main_thread_ = QThread::currentThread();

    using namespace cache_gate; // function scope only — see the gate's comment
    const QList<TableSpec> expected = expected_cache_tables();
    auto attempt = [&]() -> GateResult {
        release_connection();
        db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QString::fromLatin1(kCacheConnectionName));
        db_.setDatabaseName(path);
        if (!db_.open())
            return gate_error(db::classify(db_.lastError()), "cannot open: " + db_.lastError().text());
        GateResult g =
            run_open_gate(db_, kSchemaVersion, expected, [this](int from) { return create_tables(from); });
        // WAL (with read-back and fallback), mmap off, busy timeout — only once
        // the file is known to be ours.
        if (g.verdict == GateVerdict::Ok)
            journal_mode_ = db::apply_pragmas(db_, kCachePragmas, /*set_journal_mode=*/true);
        return g;
    };

    GateResult g = attempt();
    if (g.verdict == GateVerdict::Reset) {
        // A cache: deleting it costs a refetch; keeping a broken or foreign file
        // would fail every cache call for the whole session.
        LOG_WARN(kCacheDbTag, QString("Cache database unusable, deleting it and starting empty state=\"%1\" "
                                      "reason=\"%2\" path=%3")
                                  .arg(g.label, privacy::redact_text(g.reason).left(200), privacy::redact_home(path)));
        release_connection();
        QString problem;
        if (remove_cache_files(path, &problem)) {
            g = attempt();
            if (g.verdict == GateVerdict::Reset) // recreate once, never loop
                g.verdict = GateVerdict::Fail;
        } else {
            g.verdict = GateVerdict::Fail;
            g.reason = problem;
        }
    }
    if (g.verdict != GateVerdict::Ok) {
        // Never leave a wrapper "open" on a broken connection.
        release_connection();
        db_path_.clear();
        const QString msg =
            QString("cache.db unavailable [%1]: %2").arg(QString::fromLatin1(db::kind_name(g.kind)), g.reason);
        LOG_ERROR(kCacheDbTag, QString("Cache database could not be opened kind=%1 path=%2 error=\"%3\"")
                                   .arg(QString::fromLatin1(db::kind_name(g.kind)), privacy::redact_home(path),
                                        privacy::redact_text(g.reason).left(200)));
        return Result<void>::err(msg.toStdString());
    }

    db_path_ = path;
    open_.store(true, std::memory_order_release);

    QString schema_note;
    if (g.was_empty)
        schema_note = QStringLiteral("created schema v%1").arg(kSchemaVersion);
    else if (g.found_version == 0)
        schema_note = QStringLiteral("stamped unversioned file as schema v%1").arg(kSchemaVersion);
    else if (g.found_version != kSchemaVersion)
        schema_note = QStringLiteral("upgraded schema v%1 -> v%2").arg(g.found_version).arg(kSchemaVersion);
    else
        schema_note = QStringLiteral("schema v%1").arg(kSchemaVersion);
    LOG_DEBUG(kCacheDbTag, QString("Cache database opened path=%1 schema=\"%2\" journal_mode=%3 sqlite=%4")
                              .arg(privacy::redact_home(path), schema_note, journal_mode_.isEmpty() ? QStringLiteral("?") : journal_mode_,
                                   db::sqlite_version(db_)));

    // Startup sweep of expired rows. StorageManager::start_retention_sweeper()
    // repeats this on a ~15 min timer — a terminal left open for days used to
    // accumulate expired rows until the next relaunch.
    const int removed = sweep_expired();
    if (removed > 0)
        LOG_DEBUG(kCacheDbTag, QString("Startup sweep removed expired cache rows count=%1").arg(removed));

    return Result<void>::ok();
}

void CacheDatabase::close() {
    shutdown();
}

void CacheDatabase::shutdown() {
    if (main_thread_ && QThread::currentThread() != main_thread_) {
        // db_ belongs to the opening thread; touching it here is the cross-thread
        // access the per-thread scheme exists to avoid.
        LOG_WARN(kCacheDbTag, "shutdown() called off the thread that opened cache.db — ignored");
        return;
    }
    // Flip the flag first: from here every worker's connection() refuses.
    const bool was_open = open_.exchange(false, std::memory_order_acq_rel);
    if (was_open && db_.isOpen()) {
        {
            QSqlQuery q(db_);
            if (!q.exec(QStringLiteral("PRAGMA optimize")))
                LOG_WARN(kCacheDbTag, "PRAGMA optimize failed: " + q.lastError().text());
        }
        // Fold the -wal back into cache.db and truncate it (no-op unless WAL).
        db::checkpoint_truncate(db_);
    }
    release_connection();
    db_path_.clear();
    if (was_open)
        LOG_DEBUG(kCacheDbTag, "Cache database closed");
}

void CacheDatabase::release_connection() {
    if (db_.isValid() && db_.isOpen())
        db_.close();
    // Drop our handle BEFORE removeDatabase, or Qt warns the connection is
    // still in use.
    db_ = QSqlDatabase();
    const QString name = QString::fromLatin1(kCacheConnectionName);
    if (QSqlDatabase::contains(name))
        QSqlDatabase::removeDatabase(name);
}

bool CacheDatabase::is_open() const {
    // Called from worker threads (StorageManager sweeper, CacheManager): never
    // read db_ here, it would touch the main thread's driver.
    return open_.load(std::memory_order_acquire);
}

QSqlDatabase CacheDatabase::connection() {
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
        ~Guard() {
            // Worker threads can exit after QCoreApplication (and Qt SQL's connection
            // registry) are gone; removeDatabase() then dereferenced freed state and
            // crashed every quit. The process is ending, so the OS reclaims the handle.
            if (QCoreApplication::instance() == nullptr || QCoreApplication::closingDown())
                return;
            if (!name.isEmpty())
                QSqlDatabase::removeDatabase(name);
        }
    };
    thread_local Guard guard;

    // Not open (never opened, open failed, or shut down at quit): refuse. A
    // clone made or used now would write to a file that is being torn down and
    // re-grow its -wal after the final checkpoint. Not logged: callers get the
    // "unavailable" error from execute()/exec() and decide.
    if (!open_.load(std::memory_order_acquire))
        return {};

    if (guard.name.isEmpty()) {
        guard.name = QStringLiteral("fincept_cache_thread_%1_%2")
                         .arg(cache_thread_label(t))
                         .arg(reinterpret_cast<quintptr>(t), 0, 16);

        // Clone by connection NAME: the QSqlDatabase-object overload would read
        // the main thread's handle and its live driver from this thread.
        QSqlDatabase clone = QSqlDatabase::cloneDatabase(QString::fromLatin1(kCacheConnectionName), guard.name);
        if (!clone.open()) {
            const QString err = clone.lastError().text();
            LOG_ERROR(kCacheDbTag,
                      QString("Failed to open per-thread cache connection on [%1]: %2").arg(cache_thread_label(t), err));
            QSqlDatabase::removeDatabase(guard.name);
            guard.name.clear();
            return {};
        }
        // journal_mode is file-global and already set by the main connection —
        // only re-apply the per-connection PRAGMAs (synchronous matched to it).
        db::apply_pragmas(clone, kCachePragmas, /*set_journal_mode=*/false);
        const int n = ++per_thread_connections_;
        LOG_DEBUG(kCacheDbTag,
                  QString("Cache connection opened thread=%1 live=%2").arg(cache_thread_label(t)).arg(n));
    }
    return QSqlDatabase::database(guard.name, /*open=*/false);
}

Result<QSqlQuery> CacheDatabase::execute(const QString& sql, const QVariantList& params) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        return Result<QSqlQuery>::err("Cache DB connection unavailable on this thread");
    }
    QSqlQuery query(conn);

    // Check prepare() — see the identical note in Database::execute(). An
    // unchecked prepare turns "no such column" into a misleading "Parameter
    // count mismatch" at exec time, because bindValue() no-ops on a statement
    // that was never prepared.
    if (!query.prepare(sql)) {
        return Result<QSqlQuery>::err("prepare failed: " + query.lastError().text().toStdString());
    }

    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    if (!query.exec()) {
        return Result<QSqlQuery>::err(query.lastError().text().toStdString());
    }
    return Result<QSqlQuery>::ok(std::move(query));
}

Result<QVector<QVariantList>> CacheDatabase::query_rows(const QString& sql, const QVariantList& params) {
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

int CacheDatabase::sweep_expired() {
    if (!is_open())
        return -1;
    // idx_cache_expires makes this a range scan, not a table scan.
    auto r = execute("DELETE FROM unified_cache WHERE expires_at < datetime('now')");
    if (r.is_err()) {
        LOG_WARN(kCacheDbTag, QString("Expiry sweep failed: %1").arg(QString::fromStdString(r.error())));
        return -1;
    }
    return r.value().numRowsAffected();
}

Result<void> CacheDatabase::exec(const QString& sql) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        return Result<void>::err("Cache DB connection unavailable on this thread");
    }
    QSqlQuery query(conn);
    if (!query.exec(sql)) {
        return Result<void>::err(query.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> CacheDatabase::create_tables(int from_version) {
    // Unified cache with TTL
    auto r = exec("CREATE TABLE IF NOT EXISTS unified_cache ("
                  "  key TEXT PRIMARY KEY,"
                  "  value TEXT NOT NULL,"
                  "  category TEXT DEFAULT 'general',"
                  "  content_type TEXT DEFAULT 'json',"
                  "  ttl_seconds INTEGER DEFAULT 300,"
                  "  expires_at TEXT NOT NULL,"
                  "  created_at TEXT DEFAULT (datetime('now')),"
                  "  hit_count INTEGER DEFAULT 0,"
                  "  size_bytes INTEGER DEFAULT 0"
                  ")");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_cache_expires ON unified_cache(expires_at)");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_cache_category ON unified_cache(category)");
    if (r.is_err())
        return r;

    // Tab session state persistence
    r = exec("CREATE TABLE IF NOT EXISTS tab_sessions ("
             "  tab_id TEXT PRIMARY KEY,"
             "  screen_name TEXT NOT NULL,"
             "  scroll_position REAL DEFAULT 0,"
             "  filters_json TEXT DEFAULT '{}',"
             "  selections_json TEXT DEFAULT '{}',"
             "  last_accessed TEXT DEFAULT (datetime('now')),"
             "  updated_at TEXT DEFAULT (datetime('now'))"
             ")");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_tab_sessions_accessed ON tab_sessions(last_accessed)");
    if (r.is_err())
        return r;

    // Screen UI state — survives crashes, restored per screen on next open.
    // instance_uuid (Phase 4b, panel-UUID refactor): pre-Phase-4 rows are keyed
    // only by screen_key (the screen type id, e.g. "watchlist"); new rows also
    // carry a per-panel stable id so two open watchlists never collide on save.
    r = exec("CREATE TABLE IF NOT EXISTS screen_state ("
             "  screen_key    TEXT PRIMARY KEY,"
             "  state_version INTEGER NOT NULL DEFAULT 1,"
             "  state_json    TEXT    NOT NULL DEFAULT '{}',"
             "  updated_at    TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
             "  session_id    TEXT    NOT NULL DEFAULT '',"
             "  instance_uuid TEXT"
             ")");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_screen_state_updated ON screen_state(updated_at)");
    if (r.is_err())
        return r;

    // ── In-file upgrade steps ────────────────────────────────────────────────
    // Each step runs only for files older than the version that introduced it,
    // inside open()'s transaction (a failure rolls the whole open back), and must
    // be idempotent. To add one: bump kSchemaVersion, add `if (from_version < N)`
    // below, and add the new columns to expected_cache_tables().
    //
    // v1 — files from before user_version existed (stamped 0). Their screen_state
    // may predate Phase 4b and lack instance_uuid.
    if (from_version < 1) {
        bool has_instance_uuid = false;
        {
            QSqlDatabase conn = connection();
            QSqlQuery info(conn);
            if (!info.exec("SELECT 1 FROM pragma_table_info('screen_state') WHERE name = 'instance_uuid'"))
                return Result<void>::err(info.lastError().text().toStdString());
            has_instance_uuid = info.next();
        }
        if (!has_instance_uuid) {
            r = exec("ALTER TABLE screen_state ADD COLUMN instance_uuid TEXT");
            if (r.is_err())
                return r;
            LOG_INFO(kCacheDbTag, "screen_state upgraded: added instance_uuid column");
        }
    }

    // Index on instance_uuid for fast UUID-keyed lookups. NOT UNIQUE — old
    // rows have NULL instance_uuid and SQLite treats multiple NULLs as
    // distinct in a UNIQUE index, but we'd rather not rely on that subtlety.
    r = exec("CREATE INDEX IF NOT EXISTS idx_screen_state_instance_uuid "
             "ON screen_state(instance_uuid) WHERE instance_uuid IS NOT NULL");
    if (r.is_err())
        return r;

    LOG_DEBUG(kCacheDbTag, "Cache tables initialized");
    return Result<void>::ok();
}

} // namespace fincept
