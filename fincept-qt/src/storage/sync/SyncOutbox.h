#pragma once
#include "core/result/Result.h"

#include <QString>
#include <QVector>

#include <functional>

namespace fincept {

/// One pending local mutation awaiting cloud push.
struct OutboxRow {
    qint64 id = 0;
    QString entity;   // 'watchlist','note','portfolio',...
    QString local_id; // stringified local PK
    QString op;       // 'create' | 'update' | 'delete' | domain-specific op
    QString payload;  // optional hint (e.g. stock symbol)
    int attempts = 0;
};

/// Durable write-ahead log of local mutations to mirror to the cloud.
///
/// Repositories call `record()` immediately after a successful local write;
/// `CloudSyncEngine` drains it asynchronously (push), with retry/backoff, and the
/// rows survive app restart and offline periods. All methods operate on the
/// calling thread's `Database` connection.
class SyncOutbox {
  public:
    /// Append a change. Returns ok() but does NOTHING (no row inserted) when:
    ///   (a) an `ApplyGuard` is active on the current thread — a pull is writing
    ///       remote rows into the local cache, so recording would echo-loop; or
    ///   (b) `CloudSyncSettings::should_sync(entity)` is false (master off, or the
    ///       domain is excluded).
    /// This lets repository hooks call `record()` unconditionally — the gate lives
    /// here, in one place.
    static Result<void> record(const QString& entity, const QString& local_id, const QString& op,
                               const QString& payload = {});

    /// Like record(), but a no-op when an identical pending (entity, local_id, op)
    /// row already exists. Used to coalesce portfolio-level "sync" markers so a
    /// burst of edits collapses into a single reconcile (which reads live state).
    static Result<void> record_unique(const QString& entity, const QString& local_id, const QString& op,
                                      const QString& payload = {});

    /// Pending rows across all entities, FIFO by id, capped at `limit`.
    static Result<QVector<OutboxRow>> pending_all(int limit = 500);

    /// Count of pending rows for one entity.
    static Result<int> count(const QString& entity);

    /// Remove a row once its push succeeded.
    static Result<void> mark_done(qint64 id);

    /// Record a failed push attempt (increments attempts, stores last_error).
    static Result<void> bump_attempt(qint64 id, const QString& error);

    /// True while a pull is applying remote rows on the current thread.
    static bool applying_remote();

    /// `CloudSyncEngine` registers a callback invoked (with the entity) after each
    /// successful `record()`, so it can schedule a debounced drain. Stored as a
    /// plain std::function so the storage layer stays decoupled from services.
    static void set_notifier(std::function<void(QString)> cb);

    /// RAII: suppress `record()` on the current thread for the guard's lifetime.
    /// Wrap remote-apply (pull) writes in this to prevent the echo loop.
    class ApplyGuard {
      public:
        ApplyGuard();
        ~ApplyGuard();
        ApplyGuard(const ApplyGuard&) = delete;
        ApplyGuard& operator=(const ApplyGuard&) = delete;
    };
};

} // namespace fincept
