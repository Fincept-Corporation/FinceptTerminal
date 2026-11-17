#pragma once
#include "core/result/Result.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <atomic>

class QThread;

namespace fincept {

/// Separate SQLite database for ephemeral cache data (cache.db).
/// Uses synchronous=NORMAL under WAL for safe, fast writes.
///
/// Thread-safety: identical scheme to `Database`. QSqlDatabase has thread
/// AFFINITY — a connection may only be used on the thread that created it. A
/// mutex does not make a shared connection legal, and the previous design made
/// that worse by returning a live `QSqlQuery` after the lock was released, so a
/// caller stepped a statement on a connection another thread could already be
/// preparing on. `connection()` now hands out a per-thread cloned connection;
/// under WAL those need no serialisation, so there is no mutex any more.
///
/// Consequence for callers: the `QSqlQuery` returned by `execute()` belongs to
/// the CALLING thread's connection and must be stepped on that same thread —
/// which is what every call site already does. To hand results to another
/// thread, use `query_rows()`, which materialises them.
///
/// Schema and recovery: the file carries `PRAGMA user_version` = kSchemaVersion.
/// open() checks it before writing anything: 0 is stamped (new file, or one from
/// before versioning), an older version is upgraded in place inside one
/// transaction, and the expected columns are verified. A newer/unknown version,
/// a failed column check, a damaged file ("not a database", "malformed") or a
/// failed schema setup deletes cache.db and its -wal/-shm and opens a fresh file
/// once — this is a cache. Busy, read-only, disk-full and permission errors leave
/// the file alone and open() fails. On failure nothing stays half-open.
class CacheDatabase {
  public:
    static CacheDatabase& instance();

    /// PRAGMA user_version this build writes and understands.
    static constexpr int kSchemaVersion = 1;

    Result<void> open(const QString& path);

    /// Same as shutdown(); kept for existing callers.
    void close();

    /// Orderly close for application quit: PRAGMA optimize, wal_checkpoint(TRUNCATE),
    /// close the owning connection and remove it from Qt's registry. Afterwards
    /// is_open() is false and every thread's connection() refuses, so late worker
    /// calls fail cleanly instead of re-growing the -wal. Idempotent, a no-op when
    /// never opened. Call it on the thread that called open() (the GUI thread);
    /// from any other thread it only logs.
    void shutdown();

    /// Thread-safe: reads an atomic flag, never the owning connection.
    bool is_open() const;

    /// Returns a `QSqlDatabase` handle valid for the current thread.
    /// Main thread: the primary connection opened by `open()`. Any other
    /// thread: a lazily-created thread-local clone, removed from Qt's registry
    /// when that thread exits.
    QSqlDatabase connection();

    Result<QSqlQuery> execute(const QString& sql, const QVariantList& params = {});
    Result<void> exec(const QString& sql);

    /// Like `execute()`, but copies every row out before the statement dies.
    /// The result is a plain value type, so it is safe to move across threads.
    Result<QVector<QVariantList>> query_rows(const QString& sql, const QVariantList& params = {});

    /// Delete `unified_cache` rows whose TTL has lapsed.
    /// Returns the number of rows removed, or -1 on error.
    int sweep_expired();

    /// The owning (main-thread) connection object.
    /// MAIN THREAD ONLY — bypasses `connection()`. Kept for the settings SQL
    /// console and diagnostics; everything else must use `execute()`/`exec()`.
    QSqlDatabase& raw_db() { return db_; }

    /// File path of the open database (empty if not open).
    QString path() const { return db_path_; }

  private:
    CacheDatabase() = default;
    /// CREATE … IF NOT EXISTS for every table plus the in-file upgrade steps for
    /// files older than kSchemaVersion. Runs inside open()'s transaction.
    Result<void> create_tables(int from_version);
    /// Close the owning connection and drop it from Qt's registry (main thread).
    void release_connection();

    QSqlDatabase db_;                            // main-thread (owning) connection
    QThread* main_thread_ = nullptr;             // captured at open()
    QString db_path_;                            // path of the open file (empty when closed)
    QString journal_mode_;                       // journal mode SQLite actually chose ("wal", "truncate")
    std::atomic<bool> open_{false};              // read by worker threads; never touch db_ off-thread
    std::atomic<int> per_thread_connections_{0}; // diagnostic counter
};

} // namespace fincept
