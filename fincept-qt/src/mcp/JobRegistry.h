#pragma once
// JobRegistry.h — Async job store for long-running work started by a tool call.
//
// Problem this solves
// -------------------
// A tool call that blocks holds the model's HTTP turn open: a 5-minute script
// run means five minutes of dead air and a whole tool round spent waiting, with
// no way to report progress or give up. This registry is the receipt book that
// lets a tool start work, answer with a job id, and be asked about it later.
//
// Its one client today is the `python` core tool (mcp/core/PythonScriptTool.cpp):
//   python(action="run", …)        — waits up to wait_s (≤25 s) inline; a run
//                                    still going after that answers with its
//                                    job id and keeps running in the background
//   python(action="status", job_id, wait_s) — long-polls; one call covers most
//                                    of a run, and the result arrives with it
//   python(action="cancel", job_id) — kills the script (PythonRunner cancel_token)
//
// Long-polling matters for token cost: without it the model burns one tool
// round per poll. With it, a 90 s job costs ~4 rounds instead of ~30.
//
// Thread safety: every method is callable from any thread. Work resolves on
// service/worker threads; tool calls arrive on pool threads; the watchdog runs on
// the GUI thread. A single QMutex guards the map and a QWaitCondition backs
// `wait_for` so long-polling costs no event loop.

#include "mcp/McpTypes.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QWaitCondition>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace fincept::mcp {

enum class JobState { Running, Succeeded, Failed, Cancelled };

inline const char* job_state_str(JobState s) {
    switch (s) {
        case JobState::Running:
            return "running";
        case JobState::Succeeded:
            return "succeeded";
        case JobState::Failed:
            return "failed";
        case JobState::Cancelled:
            return "cancelled";
    }
    return "unknown";
}

/// Immutable view of a job — safe to copy out from under the lock.
struct JobSnapshot {
    QString id;
    QString tool;
    /// Who started it (the caller label: "claude-code", "AI Chat", "agent: …").
    /// Only that caller sees, waits on or cancels the job — one agent must not
    /// read or stop another's work. Empty = no owner (visible to all).
    QString owner;
    JobState state = JobState::Running;
    double progress = 0.0; // [0,1]; 0 when the handler reports no progress
    QString message;       // last status line from the handler
    qint64 started_ms = 0; // epoch ms
    qint64 finished_ms = 0;
    // The handler's hard budget. Reported alongside elapsed_ms because elapsed
    // alone is undecidable: "running, 40 s" is healthy for a 300 s backtest and
    // nearly dead for a 45 s quote fetch, and the model has no other way to
    // tell which it is holding. Together they answer the only question a poll
    // is really asking — keep waiting, or give up.
    int timeout_ms = 0;
    bool result_collected = false;

    bool is_finished() const { return state != JobState::Running; }
    qint64 elapsed_ms() const;

    /// Compact wire form for a job's status answer. Deliberately small —
    /// this is polled repeatedly, so every field costs tokens on every poll.
    QJsonObject to_json() const;
};

class JobRegistry {
  public:
    static JobRegistry& instance();

    /// Register a new running job. Returns its id (e.g. "job_4f21ab").
    /// `timeout_ms` is the handler's hard budget, echoed back in the status answer
    /// so a poller can tell a slow job from a stuck one.
    QString create(const QString& tool, int timeout_ms = 0, const QString& owner = {});

    /// Handler progress hook. `progress` is clamped to [0,1]; an empty
    /// `message` leaves the previous one in place.
    void set_progress(const QString& id, double progress, const QString& message);

    /// Move the job to a terminal state and store its payload. No-op if the
    /// job already finished (the timeout watchdog and the handler race).
    void complete(const QString& id, const ToolResult& result);

    /// Raise the cancellation flag the handler polls via `ToolContext::is_cancelled`.
    /// Returns false if the id is unknown or the job already finished. The job
    /// is NOT moved to Cancelled here — that happens when the handler actually
    /// unwinds and calls complete(), so status keeps reporting "running"
    /// until the work has genuinely stopped.
    bool request_cancel(const QString& id);

    bool is_cancelled(const QString& id) const;

    /// The flag itself — handed to ToolContext so the polling path doesn't
    /// re-lock the registry on every check inside a tight handler loop.
    std::shared_ptr<std::atomic<bool>> cancel_flag(const QString& id) const;

    std::optional<JobSnapshot> status(const QString& id) const;

    /// The stored payload. `std::nullopt` while the job is still running or if
    /// the id is unknown. Marks the job as collected (it becomes eligible for a
    /// shorter sweep TTL, so a polled-and-read job doesn't linger for 15 min).
    std::optional<ToolResult> take_result(const QString& id);

    /// Block up to `wait_ms` for the job to leave Running, then return its
    /// snapshot. This is the long-poll that keeps a status check from costing one
    /// tool round per second. `wait_ms <= 0` returns immediately.
    std::optional<JobSnapshot> wait_for(const QString& id, int wait_ms);

    std::vector<JobSnapshot> list(bool include_finished) const;

    /// Drop finished jobs past their TTL and trim the map back to kMaxJobs.
    /// Called opportunistically from create(); no timer needed.
    void sweep();

    int running_count() const;

    JobRegistry(const JobRegistry&) = delete;
    JobRegistry& operator=(const JobRegistry&) = delete;

  private:
    JobRegistry() = default;

    struct Job {
        JobSnapshot snap;
        ToolResult result;
        std::shared_ptr<std::atomic<bool>> cancel_flag;
    };

    void sweep_locked();

    mutable QMutex mutex_;
    mutable QWaitCondition finished_cv_;
    QHash<QString, Job> jobs_;
    quint64 next_id_ = 0;

    // A finished job is kept this long so the model can still collect it after
    // a slow round-trip; once collected it only needs to survive a retry.
    static constexpr qint64 kFinishedTtlMs = 15 * 60 * 1000;
    static constexpr qint64 kCollectedTtlMs = 60 * 1000;
    // Hard cap so a runaway agent can't grow the map without bound. Oldest
    // finished jobs are evicted first; running jobs are never evicted.
    static constexpr int kMaxJobs = 128;
    // Upper bound on a single status long-poll. Longer than this and the
    // provider-side HTTP request risks timing out mid-wait.
    static constexpr int kMaxWaitMs = 30000;

  public:
    static constexpr int max_wait_ms() { return kMaxWaitMs; }
};

} // namespace fincept::mcp
