// JobRegistry.cpp — Async job store for long-running MCP tool calls.

#include "mcp/JobRegistry.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QJsonObject>

#include <algorithm>

namespace fincept::mcp {

static constexpr const char* TAG = "JobRegistry";

static qint64 now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

qint64 JobSnapshot::elapsed_ms() const {
    const qint64 end = finished_ms > 0 ? finished_ms : now_ms();
    return end - started_ms;
}

QJsonObject JobSnapshot::to_json() const {
    QJsonObject j{
        {"job_id", id},
        {"tool", tool},
        {"status", QString::fromLatin1(job_state_str(state))},
        {"elapsed_ms", elapsed_ms()},
    };
    // Omit zero/empty fields — this object is polled repeatedly and every
    // key costs tokens on every poll.
    if (progress > 0.0)
        j["progress"] = progress;
    if (!message.isEmpty())
        j["message"] = message;
    return j;
}

JobRegistry& JobRegistry::instance() {
    static JobRegistry s;
    return s;
}

QString JobRegistry::create(const QString& tool) {
    QMutexLocker lock(&mutex_);
    sweep_locked();

    // Short monotonic id — a full UUID would be 36 chars the model has to echo
    // back on every poll, for no added value in a per-process registry.
    const QString id = QStringLiteral("job_%1").arg(++next_id_, 6, 16, QLatin1Char('0'));

    Job job;
    job.snap.id = id;
    job.snap.tool = tool;
    job.snap.state = JobState::Running;
    job.snap.started_ms = now_ms();
    job.cancel_flag = std::make_shared<std::atomic<bool>>(false);
    jobs_.insert(id, std::move(job));

    LOG_INFO(TAG, QString("Job %1 started for tool '%2'").arg(id, tool));
    return id;
}

void JobRegistry::set_progress(const QString& id, double progress, const QString& message) {
    QMutexLocker lock(&mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end() || it->snap.is_finished())
        return;
    it->snap.progress = std::clamp(progress, 0.0, 1.0);
    if (!message.isEmpty())
        it->snap.message = message;
}

void JobRegistry::complete(const QString& id, const ToolResult& result) {
    {
        QMutexLocker lock(&mutex_);
        auto it = jobs_.find(id);
        if (it == jobs_.end())
            return;
        // The timeout watchdog and the handler's own callback both land here.
        // First writer wins; the loser is dropped silently.
        if (it->snap.is_finished())
            return;

        // A handler that unwound because we asked it to stop reports Cancelled
        // rather than Failed — the distinction matters to the model, which
        // should not retry a job the user cancelled.
        const bool was_cancel_requested = it->cancel_flag && it->cancel_flag->load();
        it->snap.state =
            result.success ? JobState::Succeeded : (was_cancel_requested ? JobState::Cancelled : JobState::Failed);
        it->snap.finished_ms = now_ms();
        if (result.success)
            it->snap.progress = 1.0;
        it->result = result;

        LOG_INFO(TAG, QString("Job %1 (%2) → %3 after %4 ms")
                          .arg(id, it->snap.tool, QString::fromLatin1(job_state_str(it->snap.state)))
                          .arg(it->snap.elapsed_ms()));
    }
    // Wake every long-poller; each re-checks its own job id.
    finished_cv_.wakeAll();
}

bool JobRegistry::request_cancel(const QString& id) {
    QMutexLocker lock(&mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end() || it->snap.is_finished())
        return false;
    if (it->cancel_flag)
        it->cancel_flag->store(true);
    it->snap.message = QStringLiteral("cancellation requested");
    LOG_INFO(TAG, QString("Job %1 (%2) — cancellation requested").arg(id, it->snap.tool));
    return true;
}

bool JobRegistry::is_cancelled(const QString& id) const {
    QMutexLocker lock(&mutex_);
    auto it = jobs_.constFind(id);
    return it != jobs_.constEnd() && it->cancel_flag && it->cancel_flag->load();
}

std::shared_ptr<std::atomic<bool>> JobRegistry::cancel_flag(const QString& id) const {
    QMutexLocker lock(&mutex_);
    auto it = jobs_.constFind(id);
    return it != jobs_.constEnd() ? it->cancel_flag : nullptr;
}

std::optional<JobSnapshot> JobRegistry::status(const QString& id) const {
    QMutexLocker lock(&mutex_);
    auto it = jobs_.constFind(id);
    if (it == jobs_.constEnd())
        return std::nullopt;
    return it->snap;
}

std::optional<ToolResult> JobRegistry::take_result(const QString& id) {
    QMutexLocker lock(&mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end() || !it->snap.is_finished())
        return std::nullopt;
    it->snap.result_collected = true;
    return it->result;
}

std::optional<JobSnapshot> JobRegistry::wait_for(const QString& id, int wait_ms) {
    QMutexLocker lock(&mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end())
        return std::nullopt;
    if (it->snap.is_finished() || wait_ms <= 0)
        return it->snap;

    const qint64 deadline = now_ms() + std::min(wait_ms, kMaxWaitMs);
    while (true) {
        const qint64 remaining = deadline - now_ms();
        if (remaining <= 0)
            break;
        // wakeAll() fires for every completion, so re-look-up and re-test our
        // own job each time round rather than trusting the wake.
        finished_cv_.wait(&mutex_, static_cast<unsigned long>(remaining));
        it = jobs_.find(id);
        if (it == jobs_.end())
            return std::nullopt; // swept mid-wait
        if (it->snap.is_finished())
            break;
    }
    return it->snap;
}

std::vector<JobSnapshot> JobRegistry::list(bool include_finished) const {
    QMutexLocker lock(&mutex_);
    std::vector<JobSnapshot> out;
    out.reserve(static_cast<std::size_t>(jobs_.size()));
    for (auto it = jobs_.constBegin(); it != jobs_.constEnd(); ++it) {
        if (!include_finished && it->snap.is_finished())
            continue;
        out.push_back(it->snap);
    }
    std::sort(out.begin(), out.end(), [](const JobSnapshot& a, const JobSnapshot& b) {
        return a.started_ms > b.started_ms; // newest first
    });
    return out;
}

int JobRegistry::running_count() const {
    QMutexLocker lock(&mutex_);
    int n = 0;
    for (auto it = jobs_.constBegin(); it != jobs_.constEnd(); ++it)
        if (!it->snap.is_finished())
            ++n;
    return n;
}

void JobRegistry::sweep() {
    QMutexLocker lock(&mutex_);
    sweep_locked();
}

void JobRegistry::sweep_locked() {
    const qint64 now = now_ms();
    for (auto it = jobs_.begin(); it != jobs_.end();) {
        const JobSnapshot& s = it->snap;
        const qint64 ttl = s.result_collected ? kCollectedTtlMs : kFinishedTtlMs;
        if (s.is_finished() && now - s.finished_ms > ttl)
            it = jobs_.erase(it);
        else
            ++it;
    }

    if (jobs_.size() <= kMaxJobs)
        return;

    // Over the cap — evict oldest *finished* jobs first. Running jobs are never
    // evicted: dropping one would strand the handler with a job id nobody can
    // complete, and the model would poll a dead receipt forever.
    std::vector<QPair<qint64, QString>> finished;
    for (auto it = jobs_.constBegin(); it != jobs_.constEnd(); ++it)
        if (it->snap.is_finished())
            finished.push_back({it->snap.finished_ms, it.key()});
    std::sort(finished.begin(), finished.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    int to_drop = static_cast<int>(jobs_.size()) - kMaxJobs;
    for (const auto& entry : finished) {
        if (to_drop-- <= 0)
            break;
        jobs_.remove(entry.second);
    }
    if (to_drop > 0)
        LOG_WARN(TAG, QString("%1 running jobs exceed the %2-job cap — none evicted").arg(to_drop).arg(kMaxJobs));
}

} // namespace fincept::mcp
