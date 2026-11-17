#pragma once
#include "core/result/Result.h"
#include "storage/sqlite/SqliteSupport.h"

#include <QMutex>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <atomic>

class QThread;

namespace fincept {

/// SQLite database wrapper with WAL mode, prepared statements, and transaction support.
///
/// Thread-safety: QSqlDatabase is single-threaded by design — a connection can
/// only be used on the thread that created it (Qt docs). To support background
/// queries (QtConcurrent::run lambdas in repositories) without silent query
/// failures, this class hands out a CLONED per-thread connection on demand via
/// `connection()`. The clone is registered with `QSqlDatabase` under a unique
/// name and torn down automatically when the thread terminates, or on that
/// thread's next `connection()` call after `close()`.
class Database {
  public:
    /// Error tags for `open()` failures that do not come from MigrationRunner
    /// (which tags its own: kErrSchemaNewer, kErrSchemaIncompatible,
    /// kErrMigrationFailed). Text after the tag is for the log, not for users.
    static constexpr const char* kErrOpenFailed = "[db-open-failed]";      ///< see open_error_kind()
    static constexpr const char* kErrSqliteTooOld = "[db-sqlite-too-old]"; ///< see sqlite_version()
    /// Oldest SQLite library this build runs on: VACUUM INTO needs 3.27,
    /// ON CONFLICT ... DO UPDATE needs 3.24.
    static constexpr int kMinSqliteMajor = 3;
    static constexpr int kMinSqliteMinor = 27;

    static Database& instance();

    /// Open (or reopen) the database at `path`, verify it and migrate it.
    /// Order: open → SQLite version check → connection PRAGMAs → readability,
    /// write-access and `quick_check` probes → read-only schema inspection →
    /// journal_mode=WAL (read back; TRUNCATE fallback) → migrations → FTS repair.
    /// Nothing is written to the file before the inspection accepts it. On any
    /// failure the connection is closed again; the error is tagged with
    /// kErrOpenFailed (classified by open_error_kind()), kErrSqliteTooOld or one of
    /// MigrationRunner's tags. Main thread only.
    Result<void> open(const QString& path);

    /// Close the main connection and remove it from Qt's registry. Worker-thread
    /// clones are dropped on that thread's next `connection()` call, which then
    /// returns an invalid handle, so late calls fail cleanly instead of touching
    /// a closed (or renamed) file. Main thread only; safe to call twice.
    void close();

    /// Orderly end-of-session close: PRAGMA optimize, wal_checkpoint(TRUNCATE)
    /// (WAL mode only), then close(). Busy results are logged and ignored.
    void shutdown();

    bool is_open() const { return open_.load(std::memory_order_acquire); }

    /// Returns a `QSqlDatabase` handle valid for the current thread.
    /// On the main thread, returns the primary connection opened by `open()`.
    /// On any other thread, lazy-creates a thread-local cloned connection on
    /// first call and reuses it for subsequent calls. The thread_local guard
    /// removes the cloned connection from the registry on thread exit.
    QSqlDatabase connection();

    Result<QSqlQuery> execute(const QString& sql, const QVariantList& params = {});
    Result<void> exec(const QString& sql);

    /// Prepare `sql` ONCE and execute it for every parameter row.
    /// Use for bulk INSERT/UPDATE loops — `execute()` re-prepares per call, so an
    /// N-row loop pays N statement compilations of an identical statement.
    /// Does NOT open a transaction: call inside the caller's
    /// begin_transaction()/commit() so the whole batch is one commit.
    /// Stops and returns on the first failing row (the row index is in the error).
    Result<void> execute_many(const QString& sql, const QVector<QVariantList>& rows);

    // Transaction support (per-thread — operates on the calling thread's connection).
    Result<void> begin_transaction();
    Result<void> commit();
    Result<void> rollback();

    /// The owning (main-thread) connection object.
    /// MAIN THREAD ONLY — this bypasses `connection()`, so using it from any
    /// other thread is the cross-thread QSqlDatabase misuse the per-thread
    /// scheme exists to prevent. Prefer `execute()` / `exec()` / `connection()`
    /// everywhere; this accessor exists for the SQL console and diagnostics.
    QSqlDatabase& raw_db() { return db_; }

    /// Returns the file path of the open database (empty if not open).
    QString path() const;

    // ── Details of the last open() (main thread) ────────────────────────────
    /// Classified cause of the last kErrOpenFailed (None after success).
    db::SqlErrorKind open_error_kind() const { return open_error_kind_; }
    /// MAX(schema_version) found in the file by the last open() (0 = none).
    int found_schema_version() const { return found_schema_version_; }
    /// SQLite library version seen by the last open() ("3.45.1").
    QString sqlite_version() const { return sqlite_version_; }
    /// Journal mode in effect ("wal", or "truncate" when WAL is unavailable).
    QString journal_mode() const { return journal_mode_; }
    /// Pre-migration backup taken or reused by the last open(), or empty.
    QString migration_backup_path() const { return migration_backup_; }

    // ── File-level recovery helpers (the database must be closed) ───────────
    /// Move a damaged database aside: renames `path` and its -wal / -shm to
    /// `<path>.corrupt-<yyyyMMdd-hhmmss>` (+ "-wal" / "-shm"). Returns the new
    /// path of the main file. A -wal that cannot be moved is deleted, never left
    /// next to a new database (SQLite would replay it into the new file); if
    /// that also fails the main file is put back and an error returned.
    static Result<QString> quarantine_files(const QString& path);
    /// Newest pre-migration backup of `path` (`<path>.pre-v*.bak`), or empty.
    static QString latest_backup(const QString& path);
    /// Copy `backup` to `path` (the backup itself is kept). Fails if `path`
    /// or its -wal already exists.
    static Result<void> restore_backup(const QString& backup, const QString& path);

  private:
    Database() = default;
    /// Close and unregister the main connection without touching session state.
    void drop_main_connection();

    QSqlDatabase db_;                            // main-thread (owning) connection
    QThread* main_thread_ = nullptr;             // captured at open()
    mutable QMutex path_mutex_;                  // guards db_path_ (read by worker threads)
    QString db_path_;                            // path of the open database
    std::atomic<bool> open_{false};              // read by worker threads
    std::atomic<int> generation_{0};             // bumped by close(): stale clones are dropped
    std::atomic<int> per_thread_connections_{0}; // diagnostic counter

    db::SqlErrorKind open_error_kind_ = db::SqlErrorKind::None;
    int found_schema_version_ = 0;
    QString sqlite_version_;
    QString journal_mode_;
    QString migration_backup_;
};

} // namespace fincept
