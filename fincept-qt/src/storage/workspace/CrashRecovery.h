#pragma once
#include "core/result/Result.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QList>

namespace fincept {

class WorkspaceDb;

/// Determines whether a crash recovery prompt should appear at startup, and
/// hands the recovery dialog (Phase 6 UI) the candidate snapshots to choose
/// from.
///
/// Detection logic:
///   - Last clean shutdown writes `_meta.last_clean_shutdown_at = <unix_ms>`.
///   - Startup reads it. If the value is missing OR is older than the most
///     recent `auto` snapshot's `created_at`, the previous session ended
///     uncleanly (crashed, killed, power loss). Recovery is warranted.
///   - If no `auto` snapshots exist at all (first run, or every snapshot
///     was wiped), the prompt is skipped — there's nothing to recover from.
///
/// Phase 2 ships only the API + the marker write/read. Phase 6 wires the
/// modal dialog UI. Phase 9 (Launchpad) surfaces it as a Launchpad card.
///
/// Threading: read-only side (needs_recovery, latest_snapshots) is safe to
/// call from any thread because it goes through WorkspaceDb's mutex.
/// mark_clean_shutdown is meant to be called from main.cpp's aboutToQuit
/// handler — UI thread only by convention, though the lock makes it safe.
class CrashRecovery {
  public:
    explicit CrashRecovery(WorkspaceDb* db, WorkspaceSnapshotRing* ring);

    /// True when the previous session's exit was unclean AND there is at
    /// least one auto snapshot available to restore from. False on first
    /// run (no auto snapshots), and false after a clean shutdown.
    bool needs_recovery() const;

    /// The candidate snapshots the recovery dialog should offer, newest
    /// first. Combines auto + crash_recovery rows (named saves are listed
    /// elsewhere). Capped at `limit` to keep the dialog tractable.
    Result<QList<WorkspaceSnapshotRing::Entry>> latest_snapshots(int limit = 5) const;

    /// Mark this session as having shut down cleanly. Call from main.cpp's
    /// aboutToQuit handler (after the final autosave flush). Failing to
    /// call this is the signal the next-launch recovery uses.
    Result<void> mark_clean_shutdown();

    /// For debugging / tests: the unix-ms timestamp of the most recent auto
    /// snapshot. Returns 0 if none.
    qint64 latest_auto_snapshot_ms() const;

    /// For debugging / tests: the value of _meta.last_clean_shutdown_at, or
    /// 0 if missing.
    qint64 last_clean_shutdown_ms() const;

  private:
    WorkspaceDb* db_;
    WorkspaceSnapshotRing* ring_;
};

} // namespace fincept
