#pragma once
#include "core/result/Result.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>

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
/// name and torn down automatically when the thread terminates.
class Database {
  public:
    static Database& instance();

    Result<void> open(const QString& path);
    void close();
    bool is_open() const;

    /// Returns a `QSqlDatabase` handle valid for the current thread.
    /// On the main thread, returns the primary connection opened by `open()`.
    /// On any other thread, lazy-creates a thread-local cloned connection on
    /// first call and reuses it for subsequent calls. The thread_local guard
    /// removes the cloned connection from the registry on thread exit.
    QSqlDatabase connection();

    Result<QSqlQuery> execute(const QString& sql, const QVariantList& params = {});
    Result<void> exec(const QString& sql);

    // Transaction support (per-thread — operates on the calling thread's connection).
    Result<void> begin_transaction();
    Result<void> commit();
    Result<void> rollback();

    QSqlDatabase& raw_db() { return db_; }

    /// Returns the file path of the open database (empty if not open).
    QString path() const { return db_path_; }

  private:
    Database() = default;
    Result<void> apply_pragmas(QSqlDatabase& conn, bool include_database_wide);

    QSqlDatabase db_;                         // main-thread (owning) connection
    QThread* main_thread_ = nullptr;          // captured at open()
    QString db_path_;                         // for cloning per-thread connections
    std::atomic<int> per_thread_connections_{0}; // diagnostic counter
};

} // namespace fincept
