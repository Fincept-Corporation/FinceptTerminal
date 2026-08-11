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
class CacheDatabase {
  public:
    static CacheDatabase& instance();

    Result<void> open(const QString& path);
    void close();
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
    Result<void> apply_pragmas(QSqlDatabase& conn, bool include_database_wide);
    Result<void> create_tables();

    QSqlDatabase db_;                            // main-thread (owning) connection
    QThread* main_thread_ = nullptr;             // captured at open()
    QString db_path_;                            // for cloning per-thread connections
    std::atomic<int> per_thread_connections_{0}; // diagnostic counter
};

} // namespace fincept
