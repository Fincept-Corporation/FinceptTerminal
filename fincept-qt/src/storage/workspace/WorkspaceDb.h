#pragma once
#include "core/result/Result.h"

#include <QMutex>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>

namespace fincept {

/// Per-profile SQLite database for live workspace state, snapshot ring, and
/// session-history rows.
///
/// Design choices:
///   - WAL mode + synchronous=NORMAL — same combo CacheDatabase uses; safe
///     under power loss with one fsync per checkpoint, low write amplification.
///   - QMutex around execute() — Qt's QSqlDatabase connection is not
///     thread-safe and the workspace writer may be invoked from the UI
///     thread (frame closeEvent) and from a worker (autosave debounce).
///   - Connection name is profile-scoped (`fincept_workspace_<profile_uuid>`)
///     so a future profile-switch path can have two WorkspaceDbs open
///     concurrently during the swap.
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

    Result<QSqlQuery> execute(const QString& sql, const QVariantList& params = {});
    Result<void> exec(const QString& sql);

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
    Result<void> apply_pragmas();
    Result<void> create_tables();

    QSqlDatabase db_;
    QString connection_name_;
    // Recursive: open() holds the mutex while calling create_tables(),
    // which routes through exec() that re-acquires the same mutex. With a
    // non-recursive QMutex this deadlocks the main thread before any window
    // is shown — see ee4e946e regression.
    mutable QRecursiveMutex mutex_;
};

} // namespace fincept
