#pragma once
#include "core/result/Result.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

namespace fincept {

class WorkspaceDb;

/// Manages the ring of recent workspace snapshots used by the crash recovery
/// dialog (Phase 6 lands the UI; Phase 2 only ships the storage primitive).
///
/// Three categories of snapshot live in `workspace_snapshot`:
///
///   kind='auto'           — written by SessionManager's auto-save path on a
///                           rate limit (default: 60s minimum interval). Old
///                           ones are trimmed when the count exceeds the
///                           ring size (default: 5). These are the entries
///                           the crash-recovery dialog offers.
///   kind='named_save'     — written by the user via `layout save`. NEVER
///                           trimmed by the ring; lifetime is user-controlled.
///   kind='crash_recovery' — written by the crash handler itself if it can.
///                           NEVER trimmed; cleared by the user explicitly.
///
/// Threading: WorkspaceDb's mutex covers the SQL writes; this class is a
/// thin policy layer above the db. Multiple call sites (UI thread for
/// closeEvent flushes, autosave debounce thread for periodic snapshots) can
/// invoke `add_auto()` concurrently without external locking.
class WorkspaceSnapshotRing {
  public:
    /// Default ring size — the recovery dialog typically lists "last 5"
    /// to give the user options if the latest snapshot is the corrupted one.
    static constexpr int kDefaultMaxAuto = 5;

    /// Minimum interval between consecutive `auto` writes. Pure auto-save
    /// floods on a busy session (resize → write, navigate → write, etc.)
    /// otherwise. 60s is the plan default; SessionManager + closeEvent
    /// flushes still fire at their own cadence and can bypass via
    /// `add_with_kind('auto', ..., force=true)`.
    static constexpr qint64 kDefaultMinIntervalMs = 60'000;

    explicit WorkspaceSnapshotRing(WorkspaceDb* db,
                                   int max_auto = kDefaultMaxAuto,
                                   qint64 min_interval_ms = kDefaultMinIntervalMs);

    /// Insert a snapshot of the given kind. Returns the new snapshot_id on
    /// success, or err on SQL failure. For kind='auto' with force=false (the
    /// default), enforces min_interval_ms — call returns ok() with id=0 if
    /// the rate limit suppressed the write. force=true bypasses for explicit
    /// flushes (closeEvent, named-save, etc.).
    Result<qint64> add(const QByteArray& payload, const QString& kind, bool force = false);

    /// Convenience: kind='auto', force=false. The hot path for autosave.
    Result<qint64> add_auto(const QByteArray& payload) {
        return add(payload, "auto", /*force=*/false);
    }

    /// Convenience: kind='named_save', force=true, with a name attached.
    /// Returns the new snapshot_id. Name is stored in `workspace_snapshot.name`.
    Result<qint64> add_named(const QByteArray& payload, const QString& name);

    /// Convenience: kind='crash_recovery', force=true.
    Result<qint64> add_crash_recovery(const QByteArray& payload);

    /// Light-weight metadata for one snapshot row. The payload is excluded
    /// because the recovery dialog shows a list before any restore happens.
    struct Entry {
        qint64 snapshot_id = 0;
        qint64 created_at_ms = 0;
        QString kind;
        QString name; // empty for non-named
    };

    /// Most recent N snapshots of any kind, ordered newest-first.
    Result<QList<Entry>> latest(int limit = kDefaultMaxAuto) const;

    /// Most recent N entries of the 'auto' kind, ordered newest-first.
    /// Used by the recovery dialog (named saves are listed separately).
    Result<QList<Entry>> latest_auto(int limit = kDefaultMaxAuto) const;

    /// Load a snapshot's payload by id. nullopt-equivalent: empty QByteArray.
    Result<QByteArray> load_payload(qint64 snapshot_id) const;

    /// Drop one snapshot. Used when the user dismisses a stale recovery row.
    Result<void> erase(qint64 snapshot_id);

    /// Set or clear the user-visible name on a snapshot. Pass an empty string
    /// to clear (stored as NULL). Used by the recovery dialog's rename action.
    Result<void> rename(qint64 snapshot_id, const QString& name);

    /// Trim auto rows so at most `max_auto` remain. Called automatically
    /// inside `add()` after each successful auto insert. Exposed for tests.
    Result<void> trim_auto();

  private:
    WorkspaceDb* db_;
    int max_auto_;
    qint64 min_interval_ms_;

    // Cached timestamp of the last successful auto write. Used to enforce
    // min_interval_ms_ without round-tripping to SQL on every add() call.
    qint64 last_auto_ms_ = 0;
};

} // namespace fincept
