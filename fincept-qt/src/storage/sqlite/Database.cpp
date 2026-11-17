#include "storage/sqlite/Database.h"
#include <QCoreApplication>

#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlError>
#include <QThread>

namespace fincept {

namespace {
constexpr const char* kMainConnectionName = "fincept_main";
constexpr const char* kDbTag = "DB";

// fincept.db connection settings: 20 MB page cache, 5 s lock wait, cascades
// on, -wal truncated back to 64 MB after checkpoints (a bulk instrument
// import otherwise leaves it at its high-water mark for the life of the install).
const db::PragmaProfile kFinceptDbPragmas{20000, 5000, true, 64LL * 1024 * 1024};

QString thread_label(QThread* t) {
    if (!t)
        return QStringLiteral("?");
    const QString name = t->objectName();
    return name.isEmpty() ? QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(t), 0, 16) : name;
}

bool fincept_db_sqlite_at_least(const QString& version, int major, int minor) {
    const QStringList parts = version.split(QLatin1Char('.'));
    const int a = parts.value(0).toInt();
    const int b = parts.value(1).toInt();
    return a > major || (a == major && b >= minor);
}

/// Version of the SQLite library behind the QSQLITE driver, asked on a private
/// in-memory connection: preparing ANY statement on the real file reads its
/// schema first, so on a damaged file even `SELECT sqlite_version()` fails.
QString fincept_db_library_version(QString* error_out) {
    const QString name = QStringLiteral("fincept_sqlite_version_probe");
    QString version;
    {
        QSqlDatabase probe = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        probe.setDatabaseName(QStringLiteral(":memory:"));
        if (probe.open()) {
            version = db::sqlite_version(probe);
            probe.close();
            if (version.isEmpty() && error_out)
                *error_out = QStringLiteral("SELECT sqlite_version() returned nothing");
        } else if (error_out) {
            *error_out = probe.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return version;
}

// ── Statement failure / slow statement lines ────────────────────────────────
// The central wrapper is the transport layer for SQL: it writes the ONE line a
// failing statement gets (policy: callers stay silent or log at DEBUG). The line
// names the statement by its first keyword and table only (db::statement_summary)
// — never literals, never bound values. A statement that keeps failing (a
// schema mismatch inside a poll) is written once per minute per statement and
// kind, with the number of lines held back.

constexpr qint64 kDbRepeatWindowMs = 60 * 1000;
constexpr qint64 kDbSlowGuiMs = 500;     // a statement on the GUI thread
constexpr qint64 kDbSlowWorkerMs = 2000; // on any other thread

/// True when a line for `key` may be written now; `*suppressed` receives the
/// number of lines held back for that key since the last one written.
bool db_log_gate(const QString& key, int* suppressed) {
    struct Gate {
        qint64 last_ms = 0;
        int held = 0;
    };
    static QMutex mutex;
    static QHash<QString, Gate> gates;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QMutexLocker lock(&mutex);
    if (gates.size() > 512)
        gates.clear(); // bounded: a flood of distinct statements cannot grow it without limit
    Gate& g = gates[key];
    if (g.last_ms != 0 && now - g.last_ms < kDbRepeatWindowMs) {
        ++g.held;
        return false;
    }
    *suppressed = g.held;
    g.held = 0;
    g.last_ms = now;
    return true;
}

QString db_suppressed_suffix(int suppressed) {
    return suppressed > 0 ? QStringLiteral(" suppressed=%1").arg(suppressed) : QString();
}

/// One line for a statement the driver refused.
/// `stage`: prepare | exec | batch | raw | begin | commit | rollback.
void db_log_statement_failure(const char* stage, const QString& sql, const QSqlError& error, const QString& thread,
                              int row = -1) {
    const db::SqlErrorKind kind = db::classify(error);
    const QString text = error.text();
    // ALTER TABLE ... ADD COLUMN run again on an upgraded file: expected, not a fault.
    const bool expected = text.contains(QLatin1String("duplicate column"), Qt::CaseInsensitive);
    const bool transient = kind == db::SqlErrorKind::Busy || kind == db::SqlErrorKind::Locked;
    const LogLevel level = expected ? LogLevel::Debug : (transient ? LogLevel::Warn : LogLevel::Error);
    if (!Logger::instance().should_log(level, kDbTag))
        return;
    const QString statement = db::statement_summary(sql);
    int suppressed = 0;
    if (!db_log_gate(QString::fromLatin1(stage) + QLatin1Char('|') + statement + QLatin1Char('|') +
                         QLatin1String(db::kind_name(kind)),
                     &suppressed))
        return;
    QString msg = QStringLiteral("SQL statement failed statement=\"%1\" stage=%2 kind=%3 thread=%4")
                      .arg(statement, QLatin1String(stage), QLatin1String(db::kind_name(kind)), thread);
    if (row >= 0)
        msg += QStringLiteral(" row=%1").arg(row);
    msg += QStringLiteral(" error=\"%1\"").arg(privacy::redact_text(text.simplified()).left(200));
    msg += db_suppressed_suffix(suppressed);
    if (level == LogLevel::Debug)
        LOG_DEBUG(kDbTag, msg);
    else if (level == LogLevel::Warn)
        LOG_WARN(kDbTag, msg);
    else
        LOG_ERROR(kDbTag, msg);
}

void db_log_no_connection(const char* stage, const QString& sql, const QString& thread) {
    if (!Logger::instance().should_log(LogLevel::Error, kDbTag))
        return;
    const QString statement = db::statement_summary(sql);
    int suppressed = 0;
    if (!db_log_gate(QStringLiteral("noconn|") + statement, &suppressed))
        return;
    LOG_ERROR(kDbTag,
              QStringLiteral("SQL skipped, no open connection on this thread statement=\"%1\" stage=%2 thread=%3%4")
                  .arg(statement, QLatin1String(stage), thread, db_suppressed_suffix(suppressed)));
}

void db_log_if_slow(const QElapsedTimer& timer, bool gui_thread, const QString& sql, const QString& thread) {
    const qint64 ms = timer.elapsed();
    const qint64 threshold = gui_thread ? kDbSlowGuiMs : kDbSlowWorkerMs;
    if (ms < threshold || !Logger::instance().should_log(LogLevel::Warn, kDbTag))
        return;
    const QString statement = db::statement_summary(sql);
    int suppressed = 0;
    if (!db_log_gate(QStringLiteral("slow|") + statement, &suppressed))
        return;
    LOG_WARN(kDbTag, QStringLiteral("Slow query statement=\"%1\" duration_ms=%2 threshold_ms=%3 thread=%4%5")
                         .arg(statement)
                         .arg(ms)
                         .arg(threshold)
                         .arg(gui_thread ? QStringLiteral("gui") : thread, db_suppressed_suffix(suppressed)));
}
} // namespace

Database& Database::instance() {
    static Database s;
    return s;
}

QString Database::path() const {
    QMutexLocker lock(&path_mutex_);
    return db_path_;
}

void Database::drop_main_connection() {
    if (db_.isOpen())
        db_.close();
    db_ = QSqlDatabase();
    if (QSqlDatabase::contains(QString::fromLatin1(kMainConnectionName)))
        QSqlDatabase::removeDatabase(QString::fromLatin1(kMainConnectionName));
}

Result<void> Database::open(const QString& path) {
    // Reopen path (Retry, "separate database", restored backup): drop whatever
    // the previous attempt left behind first.
    close();
    main_thread_ = QThread::currentThread();
    open_error_kind_ = db::SqlErrorKind::None;
    found_schema_version_ = 0;
    sqlite_version_.clear();
    journal_mode_.clear();
    migration_backup_.clear();

    auto fail_open = [this, &path](db::SqlErrorKind kind, const QString& what) {
        open_error_kind_ = (kind == db::SqlErrorKind::None) ? db::SqlErrorKind::Other : kind;
        LOG_ERROR(kDbTag, QString("Database could not be opened path=%1 kind=%2 error=\"%3\"")
                              .arg(privacy::redact_home(path), QLatin1String(db::kind_name(open_error_kind_)),
                                   privacy::redact_text(what).left(300)));
        drop_main_connection();
        return Result<void>::err(std::string(kErrOpenFailed) + " " + what.toStdString());
    };

    // SQLite silently opens an unwritable file read-only and fails much later,
    // inside a migration, with "attempt to write a readonly database". Say so now.
    const QFileInfo file_info(path);
    if (file_info.exists() && !file_info.isWritable())
        return fail_open(db::SqlErrorKind::ReadOnly, QStringLiteral("the file is read-only"));

    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QString::fromLatin1(kMainConnectionName));
    db_.setDatabaseName(path);
    if (!db_.open())
        return fail_open(db::classify(db_.lastError()), db_.lastError().text());

    // Library check before the file is read at all. Qt's bundled SQLite is far
    // newer; a system Qt (distro / Flatpak runtime) may not be. An unknown
    // version (the probe itself failed) does not block: the file probes below
    // will report whatever is actually wrong.
    QString version_error;
    sqlite_version_ = fincept_db_library_version(&version_error);
    if (sqlite_version_.isEmpty()) {
        LOG_WARN(kDbTag,
                 QStringLiteral("Could not read the SQLite library version error=\"%1\"").arg(version_error.left(200)));
    } else if (!fincept_db_sqlite_at_least(sqlite_version_, kMinSqliteMajor, kMinSqliteMinor)) {
        LOG_ERROR(kDbTag, QString("SQLite %1 is too old: this build needs %2.%3 or newer")
                              .arg(sqlite_version_.isEmpty() ? QStringLiteral("(unknown)") : sqlite_version_)
                              .arg(kMinSqliteMajor)
                              .arg(kMinSqliteMinor));
        drop_main_connection();
        return Result<void>::err(std::string(kErrSqliteTooOld) + " SQLite " + sqlite_version_.toStdString());
    }

    // SQLite opens lazily: a missing, foreign, damaged or locked file only shows
    // up on the first read. Probe now, so the cause is classified here instead of
    // surfacing as raw driver text from the first migration. (The driver's own
    // 5 s busy timeout already applies to this first read.)
    {
        QSqlQuery q(db_);
        if (!q.exec(QStringLiteral("SELECT count(*) FROM sqlite_master")))
            return fail_open(db::classify(q.lastError()), q.lastError().text());
    }

    // Connection settings only — the journal mode is a property of the FILE and
    // is not changed until the inspection below accepts the file.
    db::apply_pragmas(db_, kFinceptDbPragmas, /*set_journal_mode=*/false);

    {
        // Write access, without writing: a RESERVED lock is refused with
        // SQLITE_READONLY on a read-only file and SQLITE_BUSY when another
        // process holds the write lock past busy_timeout.
        QSqlQuery q(db_);
        if (!q.exec(QStringLiteral("BEGIN IMMEDIATE")))
            return fail_open(db::classify(q.lastError()), q.lastError().text());
        q.exec(QStringLiteral("ROLLBACK"));
    }
    {
        QElapsedTimer t;
        t.start();
        db::SqlErrorKind kind = db::SqlErrorKind::None;
        auto qc = db::quick_check(db_, &kind);
        if (qc.is_err())
            return fail_open(kind, QString::fromStdString(qc.error()));
        LOG_DEBUG(kDbTag, QString("quick_check ok duration_ms=%1").arg(t.elapsed()));
    }

    // Does the file belong to this build? Read-only; Newer / Incompatible files
    // are refused before a single byte is written to them.
    MigrationRunner runner(db_);
    auto inspection = runner.inspect();
    if (inspection.is_err()) {
        const QString raw = QString::fromStdString(inspection.error());
        return fail_open(db::classify_text(raw), raw);
    }
    found_schema_version_ = inspection.value().current;
    if (inspection.value().state == SchemaState::Newer || inspection.value().state == SchemaState::Incompatible) {
        auto refused = runner.run(); // returns the tagged refusal; applies nothing
        drop_main_connection();
        return refused;
    }

    // Accepted: now the file-level setting. Read back — on a filesystem without
    // shared memory SQLite keeps the old mode silently.
    journal_mode_ = db::apply_pragmas(db_, kFinceptDbPragmas, /*set_journal_mode=*/true);
    const bool fts5 = db::has_fts5(db_);
    LOG_INFO(kDbTag, QString("Database opened path=%1 sqlite=%2 journal_mode=%3 fts5=%4")
                         .arg(privacy::redact_home(path), sqlite_version_,
                              journal_mode_.isEmpty() ? QStringLiteral("?") : journal_mode_,
                              fts5 ? QStringLiteral("yes") : QStringLiteral("no")));

    // Run versioned migrations on the main connection.
    auto migrated = runner.run();
    migration_backup_ = runner.backup_path();
    if (migrated.is_err()) {
        drop_main_connection();
        return migrated;
    }

    // Converge the optional news search index with this SQLite library.
    auto fts = ensure_news_fts(db_, fts5);
    if (fts.is_err())
        LOG_WARN(kDbTag, "News search index repair failed (non-fatal): " + QString::fromStdString(fts.error()));

    {
        QMutexLocker lock(&path_mutex_);
        db_path_ = path;
    }
    open_.store(true, std::memory_order_release);
    return Result<void>::ok();
}

void Database::close() {
    open_.store(false, std::memory_order_release);
    generation_.fetch_add(1, std::memory_order_acq_rel);
    {
        QMutexLocker lock(&path_mutex_);
        db_path_.clear();
    }
    drop_main_connection();
}

void Database::shutdown() {
    if (open_.load(std::memory_order_acquire) && db_.isOpen()) {
        {
            // Bounded: analysis_limit keeps PRAGMA optimize from running a full
            // ANALYZE over a large table at quit on older SQLite versions.
            QSqlQuery q(db_);
            q.exec(QStringLiteral("PRAGMA analysis_limit = 400"));
            if (!q.exec(QStringLiteral("PRAGMA optimize")))
                LOG_WARN(kDbTag, "PRAGMA optimize at shutdown failed: " + q.lastError().text());
        }
        db::checkpoint_truncate(db_);
        LOG_DEBUG(kDbTag, "Database closed");
    }
    close();
}

QSqlDatabase Database::connection() {
    QThread* t = QThread::currentThread();

    // Main-thread fast path — the connection opened by `open()` (an invalid
    // handle after close(), so callers see "not open" rather than a stale file).
    if (t == main_thread_)
        return db_;

    // Per-thread cloned connection. The thread_local Guard owns the
    // connection-name lifetime so the registry entry is removed when the
    // thread exits. This handles QtConcurrent thread-pool workers correctly:
    // they keep their connection across tasks and clean up at pool shutdown.
    struct Guard {
        QString name;
        int generation = -1;
        ~Guard() {
            // Worker threads can exit after QCoreApplication (and Qt SQL's connection
            // registry) are gone; removeDatabase() then dereferenced freed state and
            // crashed every quit. The process is ending, so the OS reclaims the handle.
            if (QCoreApplication::instance() == nullptr || QCoreApplication::closingDown())
                return;
            if (!name.isEmpty()) {
                // Remove from Qt's connection registry so file descriptors
                // and the QSqlDriver are released. Logs an inert warning if
                // any QSqlDatabase handle is still alive — harmless.
                QSqlDatabase::removeDatabase(name);
            }
        }
    };
    thread_local Guard guard;

    // A clone made before the last close() points at a database that is no
    // longer the open one (shut down, quarantined, or replaced). Drop it.
    const int generation = generation_.load(std::memory_order_acquire);
    if (!guard.name.isEmpty() && guard.generation != generation) {
        QSqlDatabase::removeDatabase(guard.name);
        guard.name.clear();
    }

    if (guard.name.isEmpty()) {
        if (!open_.load(std::memory_order_acquire)) {
            LOG_ERROR(kDbTag, "connection() called while the database is not open — refusing to clone");
            return {};
        }

        // Unique connection name per thread. Encode pointer + objectName so
        // log messages are debuggable.
        guard.name = QStringLiteral("fincept_thread_%1_%2_g%3")
                         .arg(thread_label(t))
                         .arg(reinterpret_cast<quintptr>(t), 0, 16)
                         .arg(generation);
        guard.generation = generation;

        // Clone BY CONNECTION NAME, not by QSqlDatabase object. The object
        // overload dereferences `db_` — the main thread's handle and its live
        // QSqlDriver — from this worker thread, which is exactly the cross-thread
        // access the per-thread scheme exists to avoid. Qt added this overload
        // for precisely this case: it resolves the source inside the registry
        // under Qt's own lock.
        QSqlDatabase clone = QSqlDatabase::cloneDatabase(QString::fromLatin1(kMainConnectionName), guard.name);
        if (!clone.open()) {
            const QString err = clone.lastError().text();
            LOG_ERROR(kDbTag, QString("Failed to open per-thread connection on [%1]: %2").arg(thread_label(t), err));
            // Clean up the registry entry we created via cloneDatabase.
            clone = QSqlDatabase();
            QSqlDatabase::removeDatabase(guard.name);
            guard.name.clear();
            return {};
        }
        // The journal mode is a property of the file, already set by the main
        // connection — only per-connection settings are applied here.
        db::apply_pragmas(clone, kFinceptDbPragmas, /*set_journal_mode=*/false);
        // Once per thread for the life of the thread — but thread-pool threads come
        // and go, so at INFO this was the single largest source of log lines.
        const int n = ++per_thread_connections_;
        LOG_DEBUG(kDbTag, QString("Database connection opened thread=%1 live=%2").arg(thread_label(t)).arg(n));
    }
    return QSqlDatabase::database(guard.name, /*open=*/false);
}

Result<QSqlQuery> Database::execute(const QString& sql, const QVariantList& params) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        db_log_no_connection("exec", sql, thread_label(QThread::currentThread()));
        return Result<QSqlQuery>::err("DB connection unavailable on this thread");
    }
    QElapsedTimer timer;
    timer.start();
    QSqlQuery query(conn);

    // Check prepare(). Ignoring it hid the real error on every failing
    // statement in the app: when prepare() fails, bindValue() silently does
    // nothing and the subsequent exec() reports the *symptom* — "Parameter
    // count mismatch", or "No query Unable to fetch row" when results are then
    // read — instead of the actual cause, which is usually "no such table" or
    // "no such column". Those two useless strings were what every repository
    // logged, so a schema mismatch was undiagnosable from the log alone.
    if (!query.prepare(sql)) {
        db_log_statement_failure("prepare", sql, query.lastError(), thread_label(QThread::currentThread()));
        return Result<QSqlQuery>::err("prepare failed: " + query.lastError().text().toStdString());
    }

    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    if (!query.exec()) {
        db_log_statement_failure("exec", sql, query.lastError(), thread_label(QThread::currentThread()));
        return Result<QSqlQuery>::err(query.lastError().text().toStdString());
    }
    db_log_if_slow(timer, QThread::currentThread() == main_thread_, sql, thread_label(QThread::currentThread()));
    return Result<QSqlQuery>::ok(std::move(query));
}

Result<void> Database::execute_many(const QString& sql, const QVector<QVariantList>& rows) {
    if (rows.isEmpty())
        return Result<void>::ok();

    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        db_log_no_connection("batch", sql, thread_label(QThread::currentThread()));
        return Result<void>::err("DB connection unavailable on this thread");
    }

    // Prepare ONCE. `execute()` re-prepares per call, which costs one
    // sqlite3_prepare_v2 per row — a 100k-row broker instrument import paid
    // 100k compilations of the same statement.
    QElapsedTimer timer;
    timer.start();
    QSqlQuery query(conn);
    if (!query.prepare(sql)) {
        db_log_statement_failure("prepare", sql, query.lastError(), thread_label(QThread::currentThread()));
        return Result<void>::err(query.lastError().text().toStdString());
    }

    int index = 0;
    for (const auto& row : rows) {
        for (int i = 0; i < row.size(); ++i)
            query.bindValue(i, row[i]);
        if (!query.exec()) {
            db_log_statement_failure("batch", sql, query.lastError(), thread_label(QThread::currentThread()), index);
            return Result<void>::err("row " + std::to_string(index) + ": " +
                                     query.lastError().text().toStdString());
        }
        ++index;
    }
    db_log_if_slow(timer, QThread::currentThread() == main_thread_, sql, thread_label(QThread::currentThread()));
    return Result<void>::ok();
}

Result<void> Database::exec(const QString& sql) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        db_log_no_connection("raw", sql, thread_label(QThread::currentThread()));
        return Result<void>::err("DB connection unavailable on this thread");
    }
    QElapsedTimer timer;
    timer.start();
    QSqlQuery query(conn);
    if (!query.exec(sql)) {
        db_log_statement_failure("raw", sql, query.lastError(), thread_label(QThread::currentThread()));
        return Result<void>::err(query.lastError().text().toStdString());
    }
    db_log_if_slow(timer, QThread::currentThread() == main_thread_, sql, thread_label(QThread::currentThread()));
    return Result<void>::ok();
}

Result<void> Database::begin_transaction() {
    QSqlDatabase conn = connection();
    if (!conn.isOpen())
        return Result<void>::err("DB connection unavailable on this thread");
    QSqlQuery q(conn);
    if (!q.exec("BEGIN IMMEDIATE")) {
        db_log_statement_failure("begin", QStringLiteral("BEGIN"), q.lastError(), thread_label(QThread::currentThread()));
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::commit() {
    QSqlDatabase conn = connection();
    if (!conn.isOpen())
        return Result<void>::err("DB connection unavailable on this thread");
    QSqlQuery q(conn);
    if (!q.exec("COMMIT")) {
        db_log_statement_failure("commit", QStringLiteral("COMMIT"), q.lastError(),
                                 thread_label(QThread::currentThread()));
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::rollback() {
    QSqlDatabase conn = connection();
    if (!conn.isOpen())
        return Result<void>::err("DB connection unavailable on this thread");
    QSqlQuery q(conn);
    if (!q.exec("ROLLBACK")) {
        db_log_statement_failure("rollback", QStringLiteral("ROLLBACK"), q.lastError(),
                                 thread_label(QThread::currentThread()));
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

// ── File-level recovery helpers ──────────────────────────────────────────────

Result<QString> Database::quarantine_files(const QString& path) {
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    QString target = path + QStringLiteral(".corrupt-") + stamp;
    for (int n = 2; QFile::exists(target) || QFile::exists(target + QStringLiteral("-wal")); ++n)
        target = path + QStringLiteral(".corrupt-") + stamp + QLatin1Char('-') + QString::number(n);

    if (QFile::exists(path) && !QFile::rename(path, target))
        return Result<QString>::err(QString("cannot rename %1 to %2").arg(path, target).toStdString());

    for (const char* suffix : {"-wal", "-shm"}) {
        const QString side = path + QLatin1String(suffix);
        if (!QFile::exists(side))
            continue;
        if (QFile::rename(side, target + QLatin1String(suffix)))
            continue;
        // A -wal left next to a NEW database would be replayed into it by
        // SQLite's recovery. Delete it rather than leave it; the -shm is only an
        // index and is rebuilt.
        if (QFile::remove(side)) {
            LOG_WARN(kDbTag, "Database side file could not be moved aside, deleted instead path=" +
                                 privacy::redact_home(side));
            continue;
        }
        if (QString::fromLatin1(suffix) == QLatin1String("-wal")) {
            QFile::rename(target, path); // put the main file back: never split it from its WAL
            return Result<QString>::err(
                QString("cannot move or delete %1 (is another program using it?)").arg(side).toStdString());
        }
    }
    LOG_WARN(kDbTag, QString("Database damaged, quarantined path=%1 moved_to=%2")
                         .arg(privacy::redact_home(path), privacy::redact_home(target)));
    return Result<QString>::ok(target);
}

QString Database::latest_backup(const QString& path) {
    return MigrationRunner::list_backups(path).value(0);
}

Result<void> Database::restore_backup(const QString& backup, const QString& path) {
    if (QFile::exists(path) || QFile::exists(path + QStringLiteral("-wal")))
        return Result<void>::err(QString("%1 still exists; move it aside first").arg(path).toStdString());
    QFile::remove(path + QStringLiteral("-shm"));
    const QString partial = path + QStringLiteral(".restoring");
    QFile::remove(partial);
    if (!QFile::copy(backup, partial)) {
        QFile::remove(partial);
        return Result<void>::err(QString("cannot copy %1 to %2").arg(backup, partial).toStdString());
    }
    // QFile::copy keeps the source's permissions; a backup must become writable.
    QFile::setPermissions(partial, QFile::permissions(partial) | QFileDevice::WriteOwner | QFileDevice::ReadOwner);
    if (!QFile::rename(partial, path)) {
        QFile::remove(partial);
        return Result<void>::err(QString("cannot rename %1 to %2").arg(partial, path).toStdString());
    }
    LOG_WARN(kDbTag, QString("Database restored from backup path=%1 backup=%2")
                         .arg(privacy::redact_home(path), privacy::redact_home(backup)));
    return Result<void>::ok();
}

} // namespace fincept
