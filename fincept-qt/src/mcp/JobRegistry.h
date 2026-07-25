#pragma once
// JobRegistry.h — Async job store for long-running MCP tool calls.
//
// Problem this solves
// -------------------
// Every MCP tool call was synchronous: `McpProvider::call_tool` dispatches the
// handler and blocks on `QFuture::waitForFinished()`. For a tool with a
// 300 s budget (`kDefaultAgentTimeoutMs`) that means the LLM's HTTP turn stays
// open for five minutes, the chat UI shows nothing, and the whole tool-round
// budget is spent on one call. There was no way to start work, hand the model a
// receipt, and let it come back for the answer.
//
// This registry is that receipt book. A tool that opts in
// (`ToolDef::supports_async`) runs under a job: if it finishes inside the grace
// window the caller gets the real result inline and the job is discarded, so
// fast paths are unchanged. If it doesn't, the caller gets
// `{job_id, status:"running"}` — a ~40-token receipt instead of a five-minute
// stall — and the work continues in the background.
//
// The model then uses the `job_*` meta-tools (see tools/JobTools.cpp):
//   job_status(id, wait_ms)  — long-polls; one call can cover the whole run
//   job_result(id)           — the payload, once finished
//   job_cancel(id)           — sets the cancellation flag the handler polls
//   job_list()               — what's still in flight
//
// `wait_ms` matters for token cost: without it the model burns one tool round
// per poll. With it, a 90 s job costs ~2 rounds instead of ~30.
//
// Thread safety: every method is callable from any thread. Handlers resolve
// their promise on service/worker threads; the meta-tools run on QtConcurrent
// pool threads; the watchdog runs on the GUI thread. A single QMutex guards the
// map and a QWaitCondition backs `wait_for` so long-polling costs no event loop.

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
    JobState state = JobState::Running;
    double progress = 0.0; // [0,1]; 0 when the handler reports no progress
    QString message;       // last status line from the handler
    qint64 started_ms = 0; // epoch ms
    qint64 finished_ms = 0;
    bool result_collected = false;

    bool is_finished() const { return state != JobState::Running; }
    qint64 elapsed_ms() const;

    /// Compact wire form for `job_status` / `job_list`. Deliberately small —
    /// this is polled repeatedly, so every field costs tokens on every poll.
    QJsonObject to_json() const;
};

class JobRegistry {
  public:
    static JobRegistry& instance();

    /// Register a new running job. Returns its id (e.g. "job_4f21ab").
    QString create(const QString& tool);

    /// Handler progress hook. `progress` is clamped to [0,1]; an empty
    /// `message` leaves the previous one in place.
    void set_progress(const QString& id, double progress, const QString& message);

    /// Move the job to a terminal state and store its payload. No-op if the
    /// job already finished (the timeout watchdog and the handler race).
    void complete(const QString& id, const ToolResult& result);

    /// Raise the cancellation flag the handler polls via `ToolContext::is_cancelled`.
    /// Returns false if the id is unknown or the job already finished. The job
    /// is NOT moved to Cancelled here — that happens when the handler actually
    /// unwinds and calls complete(), so `job_status` keeps reporting "running"
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
    /// snapshot. This is the long-poll that keeps `job_status` from costing one
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
    // Upper bound on a single `job_status` long-poll. Longer than this and the
    // provider-side HTTP request risks timing out mid-wait.
    static constexpr int kMaxWaitMs = 30000;

  public:
    static constexpr int max_wait_ms() { return kMaxWaitMs; }
};

} // namespace fincept::mcp
