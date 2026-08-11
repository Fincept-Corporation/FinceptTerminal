#pragma once
#include "core/result/Result.h"

#include <QMutex>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <atomic>

class QThread;

namespace fincept {

/// Per-profile SQLite database for live workspace state, snapshot ring, and
/// session-history rows.
///
/// Design choices:
///   - WAL mode + synchronous=NORMAL — same combo CacheDatabase uses; safe
///     under power loss with one fsync per checkpoint, low write amplification.
///   - Per-thread connections, same scheme as `Database` / `CacheDatabase`.
///     QSqlDatabase has thread AFFINITY, not merely a data race: a connection
///     may only be used on the thread that created it. The previous design held
///     one shared connection behind a QMutex and then RETURNED a live QSqlQuery
///     after releasing the lock, so a caller stepped a statement on a connection
///     another thread could already be preparing on. `connection()` now hands
///     out a per-thread cloned connection; under WAL those need no
///     serialisation. This matters because LocalTelemetrySink::record() writes
///     here from arbitrary threads. The remaining mutex guards only the owning
///     connection's identity (open/close/profile swap), never a live statement.
///   - Consequence for callers: the `QSqlQuery` returned by `execute()` belongs
///     to the CALLING thread's connection and must be stepped on that same
///     thread — which is what every call site already does. To hand results to
///     another thread use `query_rows()`, which materialises them first.
///   - Connection name is profile-scoped (`fincept_workspace_<profile_uuid>`)
///     so a future profile-switch path can have two WorkspaceDbs open
///     concurrently during the swap. A reopen bumps a generation counter so
///     per-thread clones of the previous profile are retired lazily instead of
///     silently writing to the old file.
///   - Path is taken from ProfilePaths::workspace_db() — TerminalShell::initialise()
///     opens the db immediately after that path resolver runs.
///
/// Schema (all tables created in create_tables()):
///
///   workspace_snapshot
///     snapshot_id INTEGER PRIMARY KEY AUTOINCREMENT
///     created_at  INTEGER NOT NULL              -- unix ms
///     kind        TEXT    NOT NULL              -- 'auto' | 'named_save' | 'crash_recovery'
///     payload     BLOB    NOT NULL              -- workspace JSON, opaque to this layer
///     name        TEXT                          -- non-null for kind='named_save'
///
///   session_history
///     session_id  TEXT PRIMARY KEY              -- a fresh UUID per shell init
///     profile_id  TEXT NOT NULL
///     started_at  INTEGER NOT NULL
///     ended_at    INTEGER                       -- NULL until shutdown(), else unix ms
///     exit_kind   TEXT                          -- NULL while running; 'clean' | 'crashed' | 'killed'
///     frame_count INTEGER DEFAULT 0
///     panel_count INTEGER DEFAULT 0
///
///   _meta
///     key   TEXT PRIMARY KEY
///     value TEXT
///   Used keys:
///     "schema_version"           — bumped when migrations add columns
///     "last_clean_shutdown_at"   — unix ms; absence ⇒ unclean shutdown ⇒ recovery candidate
class WorkspaceDb {
  public:
    /// Singleton. Per the multi-profile plan, profile switch tears down +
    /// reopens (Phase 1b territory) — for v0 there's exactly one active
    /// WorkspaceDb at a time and it's owned by TerminalShell.
    static WorkspaceDb& instance();

    /// Open the db at `path`. Creates schema + applies pragmas. Idempotent
    /// for the same path. Returns err if SQLite refuses to open (disk full,
    /// permissions, corruption past recovery).
    Result<void> open(const QString& path);

    void close();
    bool is_open() const;

    /// Returns a `QSqlDatabase` handle valid for the current thread.
    /// Main thread: the primary connection opened by `open()`. Any other thread:
    /// a lazily-created thread-local clone, removed from Qt's registry when that
    /// thread exits (or when a profile switch reopens the database).
    QSqlDatabase connection();

    Result<QSqlQuery> execute(const QString& sql, const QVariantList& params = {});
    Result<void> exec(const QString& sql);

    /// Like `execute()`, but copies every row out before the statement dies.
    /// The result is a plain value type, so it is safe to move across threads.
    Result<QVector<QVariantList>> query_rows(const QString& sql, const QVariantList& params = {});

    Result<void> begin_transaction();
    Result<void> commit();
    Result<void> rollback();

    /// Convenience: read/write the _meta key/value table. Reads return empty
    /// string on miss (the recovery code uses absence as a signal).
    QString meta(const QString& key) const;
    Result<void> set_meta(const QString& key, const QString& value);

    QString path() const;

    /// Schema version stored in `_meta.schema_version`. Bumped only when
    /// the loader needs migrations beyond what the CREATE-IF-NOT-EXISTS
    /// statements cover.
    static constexpr int kSchemaVersion = 1;

  private:
    WorkspaceDb() = default;
    Result<void> apply_pragmas(QSqlDatabase& conn, bool include_database_wide);
    Result<void> create_tables();

    QSqlDatabase db_;                // main-thread (owning) connection
    QString connection_name_;        // registry name of the owning connection
    QString db_path_;                // for cloning per-thread connections
    QThread* main_thread_ = nullptr; // captured at open()

    // Bumped on every open()/close(). A per-thread clone stamps the generation
    // it was made under and is discarded when it no longer matches, so a
    // profile switch cannot leave a worker writing into the previous profile.
    std::atomic<quint64> generation_{0};
    // is_open() is called from arbitrary threads (LocalTelemetrySink::record).
    // Reading db_.isOpen() there would touch the main thread's driver, so track
    // the state in an atomic instead.
    std::atomic<bool> open_{false};
    std::atomic<int> per_thread_connections_{0}; // diagnostic counter

    // Recursive: open() holds the mutex while calling create_tables(), which
    // routes through exec(). exec() no longer takes the mutex, but keep the
    // recursive type so re-entrancy through this class can never deadlock the
    // main thread before any window is shown — see ee4e946e regression.
    // Guards only the owning connection's identity, never a live statement.
    mutable QRecursiveMutex mutex_;
};

} // namespace fincept
