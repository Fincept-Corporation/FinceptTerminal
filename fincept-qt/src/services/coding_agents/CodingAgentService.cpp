// CodingAgentService.cpp — run registry, FIFO queue, batched persistence,
// schedules, notifications and shutdown. Process launching, stream handling,
// CLI detection and external-client registration live in
// CodingAgentService_Process.cpp.

#include "services/coding_agents/CodingAgentService.h"

#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "services/coding_agents/CodingAgentLiveRun.h"
#include "services/coding_agents/CodingAgentRepository.h"
#include "ui/notifications/DesktopNotifier.h"
#include "ui/notifications/NotificationService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace fincept::services::coding_agents {

namespace coding_agent_service_detail {

constexpr const char* kCodingAgentSvcTag = "CodingAgents";
constexpr int kFlushIntervalMs = 500;
constexpr int kFlushBatchSize = 50;
constexpr int kScheduleTickMs = 30000;
constexpr int kEventCacheRuns = 12;
constexpr const char* kCfgMaxConcurrent = "coding_agents/max_concurrent";
constexpr const char* kCfgNotify = "coding_agents/notify_on_finish";
constexpr const char* kCfgClaudePath = "coding_agents/claude_path";
constexpr const char* kCfgCodexPath = "coding_agents/codex_path";
constexpr const char* kWorkspaceMarker = "<!-- fincept-agent-workbench -->";

qint64 now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

QString fmt_local(qint64 ms) {
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString fmt_duration(qint64 ms) {
    const qint64 s = qMax<qint64>(0, ms) / 1000;
    if (s < 60)
        return QStringLiteral("%1s").arg(s);
    if (s < 3600)
        return QStringLiteral("%1m %2s").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1h %2m").arg(s / 3600).arg((s % 3600) / 60, 2, 10, QLatin1Char('0'));
}

/// --smoke-test walks every screen (constructing this service) and --selftest-*
/// suites are headless: neither may start a scheduled — paid — agent run.
bool is_headless_test_run() {
    const QStringList args = QCoreApplication::arguments();
    for (const auto& a : args) {
        if (a == QLatin1String("--smoke-test") || a.startsWith(QLatin1String("--selftest")))
            return true;
    }
    return false;
}

bool is_valid_model_name(const QString& model) {
    static const QRegularExpression rx(QStringLiteral(R"(^[A-Za-z0-9._:/@\[\]-]{1,100}$)"));
    return rx.match(model).hasMatch();
}

QString status_label(RunStatus status) {
    switch (status) {
        case RunStatus::Queued:
            return QStringLiteral("Queued");
        case RunStatus::Running:
            return QStringLiteral("Running");
        case RunStatus::Succeeded:
            return QStringLiteral("Succeeded");
        case RunStatus::Failed:
            return QStringLiteral("Failed");
        case RunStatus::Cancelled:
            return QStringLiteral("Cancelled");
        case RunStatus::Interrupted:
            return QStringLiteral("Interrupted");
    }
    return QStringLiteral("Failed");
}

/// Write `text` to <folder>/<base>.md. A same-named file the workbench did not
/// create (no marker line) is never touched: the write is diverted to
/// <base>-<short_id>.md instead. Runs on the writer thread.
void write_workspace_md(const QString& folder, const QString& base, const QString& short_id, const QString& text,
                        bool append) {
    QString path = folder + QLatin1Char('/') + base + QStringLiteral(".md");
    if (QFileInfo::exists(path)) {
        bool ours = false;
        QFile probe(path);
        if (probe.open(QIODevice::ReadOnly))
            ours = probe.read(64).startsWith(QByteArray(kWorkspaceMarker));
        if (!ours)
            path = folder + QLatin1Char('/') + base + QLatin1Char('-') + short_id + QStringLiteral(".md");
    }
    QFile f(path);
    const bool do_append = append && f.exists();
    if (!f.open(do_append ? (QIODevice::WriteOnly | QIODevice::Append) : (QIODevice::WriteOnly | QIODevice::Truncate))) {
        LOG_WARN(kCodingAgentSvcTag, QStringLiteral("Cannot write %1: %2").arg(path, f.errorString()));
        return;
    }
    QByteArray out;
    if (!do_append)
        out += QByteArray(kWorkspaceMarker) + '\n';
    out += text.toUtf8();
    f.write(out);
}

} // namespace coding_agent_service_detail

using namespace coding_agent_service_detail;

// ── Construction ─────────────────────────────────────────────────────────────

CodingAgentService& CodingAgentService::instance() {
    static CodingAgentService s;
    return s;
}

CodingAgentService::CodingAgentService() : QObject(nullptr) {
    session_start_ms_ = now_ms();
    claude_cli_.kind = AgentKind::Claude;
    codex_cli_.kind = AgentKind::Codex;
    max_concurrent_ = std::clamp(
        AppConfig::instance().get(QString::fromLatin1(kCfgMaxConcurrent), kDefaultMaxConcurrent).toInt(), 1,
        kMaxConcurrentLimit);
    notify_on_finish_ = AppConfig::instance().get(QString::fromLatin1(kCfgNotify), true).toBool();

    writer_ = new QThreadPool(this);
    writer_->setMaxThreadCount(1); // one thread ⇒ FIFO ⇒ a run row is always written before its events

    flush_timer_ = new QTimer(this);
    flush_timer_->setSingleShot(true);
    flush_timer_->setInterval(kFlushIntervalMs);
    connect(flush_timer_, &QTimer::timeout, this, &CodingAgentService::flush_events);

    schedule_timer_ = new QTimer(this);
    schedule_timer_->setInterval(kScheduleTickMs);
    connect(schedule_timer_, &QTimer::timeout, this, &CodingAgentService::on_schedule_tick);

    if (auto* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &CodingAgentService::on_about_to_quit);
        // Timers and QProcess need GUI-thread affinity. Every in-tree caller
        // constructs the service on the GUI thread; this guards a future one that does not.
        if (QThread::currentThread() != app->thread()) {
            LOG_WARN(kCodingAgentSvcTag, "Service constructed off the GUI thread — moving it to the GUI thread");
            moveToThread(app->thread());
        }
    }
}

CodingAgentService::~CodingAgentService() {
    // on_about_to_quit() normally ran already; this only matters when the app
    // ended without an event loop. Kill whatever is still running.
    const QStringList ids = live_.keys();
    for (const auto& id : ids)
        cleanup_live(id, /*immediate=*/true);
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void CodingAgentService::initialize_deferred() {
    if (initialized_)
        return;
    initialized_ = true;

    const qint64 session_start = session_start_ms_;
    QPointer<CodingAgentService> self(this);
    submit_write([self, session_start]() {
        auto& repo = CodingAgentRepository::instance();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        auto marked = repo.mark_unfinished_interrupted(
            session_start, now, QStringLiteral("The terminal closed while this run was in progress."));
        auto pruned = repo.prune_runs(kRetainRuns);
        auto runs = repo.load_runs(kRetainRuns);
        auto schedules = repo.load_schedules();
        QVector<CodingAgentRun> run_list = runs.is_ok() ? runs.value() : QVector<CodingAgentRun>{};
        QVector<CodingAgentSchedule> schedule_list =
            schedules.is_ok() ? schedules.value() : QVector<CodingAgentSchedule>{};
        LOG_INFO(kCodingAgentSvcTag,
                 QStringLiteral("Loaded %1 runs, %2 schedules (interrupted %3, pruned %4)")
                     .arg(run_list.size())
                     .arg(schedule_list.size())
                     .arg(marked.is_ok() ? marked.value() : 0)
                     .arg(pruned.is_ok() ? pruned.value() : 0));
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, run_list, schedule_list]() {
                if (self)
                    self->on_loaded(run_list, schedule_list);
            },
            Qt::QueuedConnection);
    });
}

void CodingAgentService::on_loaded(const QVector<CodingAgentRun>& runs,
                                   const QVector<CodingAgentSchedule>& schedules) {
    for (const auto& r : runs) {
        if (!runs_.contains(r.id))
            runs_.insert(r.id, r);
    }
    order_ = runs_.keys();
    std::sort(order_.begin(), order_.end(), [this](const QString& a, const QString& b) {
        return runs_.constFind(a)->created_at_ms > runs_.constFind(b)->created_at_ms;
    });

    for (const auto& s : schedules) {
        if (schedules_.contains(s.id))
            continue;
        schedules_.insert(s.id, s);
        schedule_order_.append(s.id);
    }
    loaded_ = true;

    if (is_headless_test_run()) {
        LOG_INFO(kCodingAgentSvcTag, "Headless test run — schedules are not armed");
        emit runs_reset();
        emit schedules_changed();
        return;
    }

    // Missed occurrences while the terminal was closed: run once if missed by
    // < 12 h, otherwise skip ahead to the next occurrence.
    const qint64 now = now_ms();
    const QStringList ids = schedule_order_;
    for (const auto& id : ids) {
        auto it = schedules_.find(id);
        if (it == schedules_.end() || !it->enabled)
            continue;
        if (it->next_run_at_ms <= 0) {
            it->next_run_at_ms = compute_next_run_ms(*it, now);
            persist_schedule(*it);
        } else if (it->next_run_at_ms <= now) {
            if (now - it->next_run_at_ms < kScheduleCatchUpWindowMs) {
                LOG_INFO(kCodingAgentSvcTag, QStringLiteral("Schedule '%1' missed at %2 — running it now")
                                                 .arg(it->name, fmt_local(it->next_run_at_ms)));
                fire_schedule(id, now);
            } else {
                LOG_INFO(kCodingAgentSvcTag, QStringLiteral("Schedule '%1' missed by > 12 h — skipping to next occurrence")
                                                 .arg(it->name));
                it->next_run_at_ms = compute_next_run_ms(*it, now);
                persist_schedule(*it);
            }
        }
    }
    rearm_schedule_timer();
    emit runs_reset();
    emit schedules_changed();
}

void CodingAgentService::on_about_to_quit() {
    if (shutting_down_)
        return;
    shutting_down_ = true;
    schedule_timer_->stop();
    const qint64 now = now_ms();

    const QStringList live_ids = live_.keys();
    for (const auto& id : live_ids) {
        auto it = runs_.find(id);
        if (it != runs_.end()) {
            it->status = RunStatus::Interrupted;
            it->finished_at_ms = now;
            it->error = QStringLiteral("The terminal was closed while this run was in progress.");
            persist_run(id);
        }
        cleanup_live(id, /*immediate=*/true); // kills the process tree
    }
    for (const auto& id : queue_) {
        auto it = runs_.find(id);
        if (it == runs_.end())
            continue;
        it->status = RunStatus::Interrupted;
        it->finished_at_ms = now;
        it->error = QStringLiteral("The terminal was closed before this run started.");
        persist_run(id);
    }
    queue_.clear();
    flush_events();
    // Teardown is the one place a bounded wait on the GUI thread is acceptable (P1).
    writer_->waitForDone(5000);
    if (!live_ids.isEmpty())
        LOG_INFO(kCodingAgentSvcTag, QStringLiteral("Shutdown: %1 running agent(s) stopped").arg(live_ids.size()));
}

// ── Settings ─────────────────────────────────────────────────────────────────

QString CodingAgentService::cli_path_override(AgentKind kind) const {
    return AppConfig::instance()
        .get(QString::fromLatin1(kind == AgentKind::Codex ? kCfgCodexPath : kCfgClaudePath), QString())
        .toString()
        .trimmed();
}

void CodingAgentService::set_cli_path_override(AgentKind kind, const QString& path) {
    const QString key = QString::fromLatin1(kind == AgentKind::Codex ? kCfgCodexPath : kCfgClaudePath);
    if (path.trimmed().isEmpty())
        AppConfig::instance().remove(key);
    else
        AppConfig::instance().set(key, path.trimmed());
    refresh_cli_detection();
}

QString CodingAgentService::install_guide_url(AgentKind kind) {
    return kind == AgentKind::Codex ? QStringLiteral("https://developers.openai.com/codex/cli")
                                    : QStringLiteral("https://docs.claude.com/en/docs/claude-code/setup");
}

void CodingAgentService::set_max_concurrent(int n) {
    max_concurrent_ = std::clamp(n, 1, kMaxConcurrentLimit);
    AppConfig::instance().set(QString::fromLatin1(kCfgMaxConcurrent), max_concurrent_);
    schedule_pump();
}

void CodingAgentService::set_notify_on_finish(bool on) {
    notify_on_finish_ = on;
    AppConfig::instance().set(QString::fromLatin1(kCfgNotify), on);
}

QString CodingAgentService::workspace_root() {
    return AppPaths::root() + QStringLiteral("/agent-workspace");
}

void CodingAgentService::set_view_visible(bool visible) {
    view_visible_ = qMax(0, view_visible_ + (visible ? 1 : -1));
}

// ── Runs ─────────────────────────────────────────────────────────────────────

QString CodingAgentService::start_run(const RunRequest& request, QString* error) {
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return QString();
    };
    initialize_deferred();
    if (shutting_down_)
        return fail(QStringLiteral("The terminal is shutting down."));

    const QString prompt = request.prompt.trimmed();
    if (prompt.isEmpty())
        return fail(QStringLiteral("The prompt is empty."));
    if (prompt.size() > kAgentPromptMaxChars)
        return fail(QStringLiteral("The prompt is too long (max %1 characters).").arg(kAgentPromptMaxChars));
    const QString model = request.model.trimmed();
    if (!model.isEmpty() && !is_valid_model_name(model))
        return fail(QStringLiteral("Model names may only contain letters, digits and . _ - : / @ [ ]"));
    if (request.max_budget_usd < 0.0 || request.max_budget_usd > 1000.0)
        return fail(QStringLiteral("The budget must be between 0 and 1000 USD."));
    if (queue_.size() >= kMaxQueuedRuns)
        return fail(QStringLiteral("%1 runs are already queued — wait for some to finish or stop them.")
                        .arg(queue_.size()));

    CodingAgentRun run;
    run.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    run.created_at_ms = now_ms();
    run.title = request.title.trimmed().isEmpty() ? run_title_from_prompt(prompt)
                                                  : request.title.trimmed().left(kAgentRunTitleMaxChars);
    run.agent = request.agent;
    run.prompt = prompt;
    run.model = model;
    run.mode = request.mode;
    run.use_fincept_tools = request.use_fincept_tools;
    run.max_budget_usd = request.agent == AgentKind::Claude ? request.max_budget_usd : 0.0;
    run.schedule_id = request.schedule_id;
    run.origin = request.origin.isEmpty() ? QStringLiteral("user") : request.origin;

    const QString chosen = request.workdir.trimmed();
    if (chosen.isEmpty()) {
        const QString name = QDateTime::fromMSecsSinceEpoch(run.created_at_ms).toString(QStringLiteral("yyyyMMdd-HHmmss")) +
                             QLatin1Char('-') + run.id.left(8);
        run.workdir = workspace_root() + QLatin1Char('/') + name;
        run.workdir_auto = true;
    } else {
        run.workdir = QDir(chosen).absolutePath();
        run.workdir_auto = false;
    }
    if (!QDir().mkpath(run.workdir))
        return fail(QStringLiteral("Cannot create the working folder %1").arg(QDir::toNativeSeparators(run.workdir)));

    return enqueue_run(std::move(run));
}

bool CodingAgentService::can_follow_up(const QString& run_id, QString* why) const {
    auto deny = [why](const QString& message) {
        if (why)
            *why = message;
        return false;
    };
    const CodingAgentRun* parent = find_run(run_id);
    if (!parent)
        return deny(QStringLiteral("Run not found."));
    if (!is_terminal_status(parent->status))
        return deny(QStringLiteral("Wait for the run to finish before following up."));
    if (parent->session_id.isEmpty())
        return deny(QStringLiteral("This run has no session to resume (it never started a session)."));
    if (!QFileInfo(parent->workdir).isDir())
        return deny(QStringLiteral("The run's working folder no longer exists."));
    for (auto it = runs_.cbegin(); it != runs_.cend(); ++it) {
        if (!is_terminal_status(it->status) && it->session_id == parent->session_id)
            return deny(QStringLiteral("A follow-up on this session is already in progress."));
    }
    return true;
}

QString CodingAgentService::follow_up(const QString& run_id, const QString& prompt, QString* error) {
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return QString();
    };
    QString why;
    if (!can_follow_up(run_id, &why))
        return fail(why);
    const QString text = prompt.trimmed();
    if (text.isEmpty())
        return fail(QStringLiteral("The follow-up prompt is empty."));
    if (text.size() > kAgentPromptMaxChars)
        return fail(QStringLiteral("The prompt is too long."));
    if (queue_.size() >= kMaxQueuedRuns)
        return fail(QStringLiteral("Too many queued runs."));

    const CodingAgentRun parent = *find_run(run_id);
    CodingAgentRun run;
    run.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    run.created_at_ms = now_ms();
    run.title = QStringLiteral("↳ ") + run_title_from_prompt(text);
    run.agent = parent.agent;
    run.prompt = text;
    run.model = parent.model;
    run.mode = parent.mode;
    run.workdir = parent.workdir; // resume needs the same cwd (Claude stores sessions per project folder)
    run.workdir_auto = parent.workdir_auto;
    run.use_fincept_tools = parent.use_fincept_tools;
    run.max_budget_usd = parent.max_budget_usd;
    run.session_id = parent.session_id; // the session to resume
    run.parent_run_id = parent.id;
    run.origin = QStringLiteral("follow_up");
    return enqueue_run(std::move(run));
}

QString CodingAgentService::enqueue_run(CodingAgentRun run) {
    const QString id = run.id;
    runs_.insert(id, run);
    order_.prepend(id);
    next_seq_.insert(id, 1);
    events_.insert(id, {});
    persist_run(id);
    queue_.append(id);
    emit run_added(id);
    LOG_INFO(kCodingAgentSvcTag, QStringLiteral("Run %1 queued (%2, origin %3)")
                                     .arg(id.left(8), agent_display_name(run.agent), run.origin));
    pump_queue();
    const auto it = runs_.constFind(id);
    if (it != runs_.cend() && it->status == RunStatus::Queued)
        record_status(id, QStringLiteral("Queued — position %1").arg(queue_position(id)),
                      QStringLiteral("Waiting for a free slot (%1 of %2 agents running).")
                          .arg(running_count())
                          .arg(max_concurrent_));
    return id;
}

bool CodingAgentService::stop_run(const QString& run_id) {
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return false;
    if (it->status == RunStatus::Queued) {
        queue_.removeAll(run_id);
        finish_run(run_id, RunStatus::Cancelled, QStringLiteral("Cancelled before it started."));
        return true;
    }
    if (it->status != RunStatus::Running)
        return false;

    LiveRun* live = live_.value(run_id);
    if (!live || !live->proc) {
        finish_run(run_id, RunStatus::Cancelled, QStringLiteral("Stopped by the user."));
        return true;
    }
    if (live->stop_requested) {
        record_status(run_id, QStringLiteral("Force stop"), QStringLiteral("Killing the agent process tree now."));
        kill_tree(live);
        return true;
    }
    live->stop_requested = true;
    record_status(run_id, QStringLiteral("Stop requested"),
                  QStringLiteral("The agent is terminated in %1 s if it has not exited by then.")
                      .arg(kCodingAgentStopGraceMs / 1000));
    live->proc->closeWriteChannel();
    terminate_tree(live);
    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    live->kill_timer = timer;
    connect(timer, &QTimer::timeout, this, [this, run_id]() {
        LiveRun* l = live_.value(run_id);
        if (l && l->proc && l->proc->state() != QProcess::NotRunning)
            kill_tree(l);
    });
    timer->start(kCodingAgentStopGraceMs);
    return true;
}

int CodingAgentService::delete_runs(const QStringList& ids) {
    QStringList victims;
    for (const auto& id : ids) {
        const CodingAgentRun* r = find_run(id);
        if (r && is_terminal_status(r->status) && !victims.contains(id))
            victims << id;
    }
    if (victims.isEmpty())
        return 0;
    // Drop not-yet-written transcript rows of these runs: inserting them after the
    // DELETE would only produce orphan-row failures.
    pending_events_.erase(std::remove_if(pending_events_.begin(), pending_events_.end(),
                                         [&victims](const QPair<QString, AgentEvent>& row) {
                                             return victims.contains(row.first);
                                         }),
                          pending_events_.end());
    for (const auto& id : victims) {
        runs_.remove(id);
        order_.removeAll(id);
        events_.remove(id);
        events_lru_.removeAll(id);
        next_seq_.remove(id);
        stderr_tails_.remove(id);
    }
    submit_write([victims]() {
        auto r = CodingAgentRepository::instance().delete_runs(victims);
        if (r.is_err())
            LOG_WARN(kCodingAgentSvcTag, QStringLiteral("delete_runs failed: %1").arg(QString::fromStdString(r.error())));
    });
    LOG_INFO(kCodingAgentSvcTag, QStringLiteral("Deleted %1 run(s)").arg(victims.size()));
    emit runs_reset();
    return static_cast<int>(victims.size());
}

int CodingAgentService::clear_finished() {
    QStringList ids;
    for (const auto& id : order_) {
        const CodingAgentRun* r = find_run(id);
        if (r && is_terminal_status(r->status))
            ids << id;
    }
    return delete_runs(ids);
}

const CodingAgentRun* CodingAgentService::find_run(const QString& run_id) const {
    const auto it = runs_.constFind(run_id);
    return it == runs_.cend() ? nullptr : &it.value();
}

int CodingAgentService::running_count() const {
    return static_cast<int>(live_.size());
}

int CodingAgentService::queue_position(const QString& run_id) const {
    const qsizetype idx = queue_.indexOf(run_id);
    return idx < 0 ? 0 : static_cast<int>(idx) + 1;
}

bool CodingAgentService::has_active_tool_runs() const {
    for (auto it = runs_.cbegin(); it != runs_.cend(); ++it) {
        if (!is_terminal_status(it->status) && it->use_fincept_tools)
            return true;
    }
    return false;
}

void CodingAgentService::schedule_pump() {
    if (pump_scheduled_)
        return;
    pump_scheduled_ = true;
    QTimer::singleShot(0, this, [this]() {
        pump_scheduled_ = false;
        pump_queue();
    });
}

void CodingAgentService::pump_queue() {
    if (shutting_down_)
        return;
    while (running_count() < max_concurrent_ && !queue_.isEmpty()) {
        const QString id = queue_.takeFirst();
        const auto it = runs_.constFind(id);
        if (it == runs_.cend() || it->status != RunStatus::Queued)
            continue;
        QString err;
        if (!launch(id, &err))
            finish_run(id, RunStatus::Failed, err);
    }
    // Queue positions moved — refresh the ids still waiting.
    for (const auto& id : std::as_const(queue_))
        emit run_updated(id);
}

void CodingAgentService::finish_run(const QString& run_id, RunStatus status, const QString& error) {
    auto it = runs_.find(run_id);
    if (it == runs_.end() || is_terminal_status(it->status))
        return;
    queue_.removeAll(run_id);

    CodingAgentRun& run = *it;
    run.status = status;
    run.finished_at_ms = now_ms();
    if (!error.isEmpty())
        run.error = error.left(8000);

    QStringList parts;
    if (run.started_at_ms > 0)
        parts << fmt_duration(run.finished_at_ms - run.started_at_ms);
    if (run.cost_usd.has_value())
        parts << QStringLiteral("$%1").arg(*run.cost_usd, 0, 'f', 4);
    if (run.input_tokens > 0 || run.output_tokens > 0)
        parts << QStringLiteral("%1 in / %2 out tok").arg(run.input_tokens).arg(run.output_tokens);
    const QString title =
        status_label(status) + (parts.isEmpty() ? QString() : QStringLiteral(" · ") + parts.join(QStringLiteral(" · ")));
    AgentEvent ev;
    ev.kind = (status == RunStatus::Failed || status == RunStatus::Interrupted) ? EventKind::Error : EventKind::Status;
    ev.is_error = status == RunStatus::Failed || status == RunStatus::Interrupted;
    ev.title = title;
    ev.body = run.error;
    record_event(run_id, ev);

    if (status == RunStatus::Succeeded)
        write_result_file(run);

    cleanup_live(run_id, /*immediate=*/false);
    flush_events();
    persist_run(run_id);
    LOG_INFO(kCodingAgentSvcTag, QStringLiteral("Run %1 %2").arg(run_id.left(8), title));

    const CodingAgentRun snapshot = run; // signals may re-enter and mutate runs_
    emit run_updated(run_id);
    emit run_finished(run_id);
    notify_finished(snapshot);
    touch_event_cache(run_id);
    schedule_pump();
}

void CodingAgentService::notify_finished(const CodingAgentRun& run) {
    if (!notify_on_finish_ || shutting_down_)
        return;
    const bool app_active = QGuiApplication::applicationState() == Qt::ApplicationActive;
    if (view_visible_ > 0 && app_active)
        return; // the user is looking at the workbench
    QString verb;
    int severity = static_cast<int>(ui::ToastService::Severity::Info);
    switch (run.status) {
        case RunStatus::Succeeded:
            verb = QStringLiteral("finished");
            severity = static_cast<int>(ui::ToastService::Severity::Success);
            break;
        case RunStatus::Cancelled:
            verb = QStringLiteral("was cancelled");
            severity = static_cast<int>(ui::ToastService::Severity::Warning);
            break;
        case RunStatus::Interrupted:
            verb = QStringLiteral("was interrupted");
            severity = static_cast<int>(ui::ToastService::Severity::Warning);
            break;
        default:
            verb = QStringLiteral("failed");
            severity = static_cast<int>(ui::ToastService::Severity::Error);
            break;
    }
    ui::DesktopNotifier::instance().notify(
        QStringLiteral("Agent Workbench"),
        QStringLiteral("%1 %2: %3").arg(agent_display_name(run.agent), verb, run.title), severity);
}

void CodingAgentService::write_prompt_file(const CodingAgentRun& run) {
    // Follow-ups share the root run's folder; divert-name collisions use the root's id.
    QString root_id = run.id;
    const CodingAgentRun* cursor = &run;
    for (int guard = 0; guard < 64 && cursor && !cursor->parent_run_id.isEmpty(); ++guard) {
        root_id = cursor->parent_run_id;
        cursor = find_run(cursor->parent_run_id);
    }
    const QString short_id = root_id.left(8);
    const bool follow_up = !run.parent_run_id.isEmpty();
    QString text;
    if (!follow_up) {
        text = QStringLiteral("# %1\n\n- Agent: %2%3\n- Mode: %4 · Terminal tools: %5\n- Run: %6 · started %7\n\n"
                              "## Prompt\n\n%8\n")
                   .arg(run.title, agent_display_name(run.agent),
                        run.model.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(run.model),
                        run.mode == RunMode::WorkspaceEdits ? QStringLiteral("Workspace edits")
                                                            : QStringLiteral("Analysis (read-only)"),
                        run.use_fincept_tools ? QStringLiteral("on") : QStringLiteral("off"), run.id,
                        fmt_local(now_ms()), run.prompt);
    } else {
        text = QStringLiteral("\n## Follow-up · %1 (run %2)\n\n%3\n").arg(fmt_local(now_ms()), run.id.left(8), run.prompt);
    }
    const QString folder = run.workdir;
    submit_write([folder, short_id, text, follow_up]() {
        write_workspace_md(folder, QStringLiteral("PROMPT"), short_id, text, follow_up);
    });
}

void CodingAgentService::write_result_file(const CodingAgentRun& run) {
    if (run.final_text.trimmed().isEmpty())
        return;
    QString root_id = run.id;
    const CodingAgentRun* cursor = &run;
    for (int guard = 0; guard < 64 && cursor && !cursor->parent_run_id.isEmpty(); ++guard) {
        root_id = cursor->parent_run_id;
        cursor = find_run(cursor->parent_run_id);
    }
    const QString short_id = root_id.left(8);
    const bool follow_up = !run.parent_run_id.isEmpty();
    const QString meta =
        QStringLiteral("%1%2 · finished %3%4")
            .arg(agent_display_name(run.agent), run.model.isEmpty() ? QString() : QStringLiteral(" · ") + run.model,
                 fmt_local(run.finished_at_ms),
                 run.cost_usd.has_value() ? QStringLiteral(" · $%1").arg(*run.cost_usd, 0, 'f', 4) : QString());
    const QString text =
        follow_up ? QStringLiteral("\n\n---\n\n## Follow-up answer · %1 (run %2)\n\n%3\n")
                        .arg(fmt_local(run.finished_at_ms), run.id.left(8), run.final_text)
                  : QStringLiteral("# %1\n\n_%2_\n\n%3\n").arg(run.title, meta, run.final_text);
    const QString folder = run.workdir;
    submit_write([folder, short_id, text, follow_up]() {
        write_workspace_md(folder, QStringLiteral("RESULT"), short_id, text, follow_up);
    });
}

// ── Events ───────────────────────────────────────────────────────────────────

void CodingAgentService::record_event(const QString& run_id, AgentEvent ev) {
    if (!runs_.contains(run_id))
        return;
    auto& vec = events_[run_id];
    int& seq = next_seq_[run_id];
    if (seq <= 0)
        seq = vec.isEmpty() ? 1 : vec.constLast().seq + 1;

    auto push = [&](AgentEvent e) {
        e.seq = seq++;
        e.ts_ms = now_ms();
        vec.append(e);
        pending_events_.append({run_id, e});
        if (pending_events_.size() >= kFlushBatchSize)
            flush_events();
        else if (!flush_timer_->isActive())
            flush_timer_->start();
        emit run_event(run_id, e);
    };

    if (vec.size() >= kCodingAgentMaxEventsPerRun) {
        const bool essential = ev.kind == EventKind::Final || ev.kind == EventKind::Usage || ev.kind == EventKind::Error;
        if (!essential) {
            LiveRun* live = live_.value(run_id);
            if (live && live->dropped_events++ == 0) {
                AgentEvent note;
                note.kind = EventKind::Status;
                note.title = QStringLiteral("Transcript limit reached");
                note.body = QStringLiteral("%1 events recorded — from now on only errors, usage and the final result "
                                           "are kept. The run continues.")
                                .arg(kCodingAgentMaxEventsPerRun);
                push(note);
            }
            return;
        }
    }
    push(std::move(ev));
}

void CodingAgentService::record_status(const QString& run_id, const QString& title, const QString& body, bool is_error) {
    AgentEvent ev;
    ev.kind = is_error ? EventKind::Error : EventKind::Status;
    ev.title = title;
    ev.body = body;
    ev.is_error = is_error;
    record_event(run_id, std::move(ev));
}

bool CodingAgentService::has_cached_events(const QString& run_id) const {
    return events_.contains(run_id);
}

QVector<AgentEvent> CodingAgentService::cached_events(const QString& run_id) const {
    return events_.value(run_id);
}

QStringList CodingAgentService::stderr_tail(const QString& run_id) const {
    if (const LiveRun* live = live_.value(run_id))
        return live->stderr_tail;
    return stderr_tails_.value(run_id);
}

void CodingAgentService::fetch_events(const QString& run_id, QObject* context,
                                      std::function<void(const QVector<AgentEvent>&)> callback) {
    if (!callback)
        return;
    if (events_.contains(run_id)) {
        callback(events_.value(run_id));
        return;
    }
    flush_events(); // FIFO writer: anything still pending lands before the read
    QPointer<QObject> ctx(context);
    QPointer<CodingAgentService> self(this);
    submit_write([self, ctx, run_id, callback]() {
        auto r = CodingAgentRepository::instance().load_events(run_id, 0);
        QVector<AgentEvent> events = r.is_ok() ? r.value() : QVector<AgentEvent>{};
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, ctx, run_id, events, callback]() {
                if (!self)
                    return;
                self->cache_loaded_events(run_id, events);
                if (ctx)
                    callback(self->events_.value(run_id, events));
            },
            Qt::QueuedConnection);
    });
}

void CodingAgentService::cache_loaded_events(const QString& run_id, const QVector<AgentEvent>& events) {
    if (!runs_.contains(run_id) || events_.contains(run_id))
        return; // deleted meanwhile, or live (the live copy is authoritative)
    events_.insert(run_id, events);
    touch_event_cache(run_id);
}

void CodingAgentService::touch_event_cache(const QString& run_id) {
    events_lru_.removeAll(run_id);
    events_lru_.append(run_id);
    while (events_lru_.size() > kEventCacheRuns) {
        const QString victim = events_lru_.takeFirst();
        const CodingAgentRun* r = find_run(victim);
        if (r && !is_terminal_status(r->status))
            continue; // never evict an active run's transcript
        events_.remove(victim);
    }
}

// ── Persistence ──────────────────────────────────────────────────────────────

void CodingAgentService::submit_write(std::function<void()> job) {
    if (!job)
        return;
    writer_->start(std::move(job));
}

void CodingAgentService::persist_run(const QString& run_id) {
    const auto it = runs_.constFind(run_id);
    if (it == runs_.cend())
        return;
    const CodingAgentRun copy = it.value();
    submit_write([copy]() {
        auto r = CodingAgentRepository::instance().upsert_run(copy);
        if (r.is_err())
            LOG_WARN(kCodingAgentSvcTag, QStringLiteral("upsert_run failed: %1").arg(QString::fromStdString(r.error())));
    });
}

void CodingAgentService::persist_schedule(const CodingAgentSchedule& schedule) {
    const CodingAgentSchedule copy = schedule;
    submit_write([copy]() {
        auto r = CodingAgentRepository::instance().upsert_schedule(copy);
        if (r.is_err())
            LOG_WARN(kCodingAgentSvcTag,
                     QStringLiteral("upsert_schedule failed: %1").arg(QString::fromStdString(r.error())));
    });
}

void CodingAgentService::flush_events() {
    flush_timer_->stop();
    if (pending_events_.isEmpty())
        return;
    QVector<RunEventRow> batch;
    batch.swap(pending_events_);
    submit_write([batch]() {
        auto r = CodingAgentRepository::instance().insert_events(batch);
        if (r.is_err())
            LOG_WARN(kCodingAgentSvcTag, QStringLiteral("insert_events failed (%1 rows): %2")
                                             .arg(batch.size())
                                             .arg(QString::fromStdString(r.error())));
    });
}

// ── Schedules ────────────────────────────────────────────────────────────────

QVector<CodingAgentSchedule> CodingAgentService::schedules() const {
    QVector<CodingAgentSchedule> out;
    out.reserve(schedule_order_.size());
    for (const auto& id : schedule_order_) {
        const auto it = schedules_.constFind(id);
        if (it != schedules_.cend())
            out.append(it.value());
    }
    return out;
}

const CodingAgentSchedule* CodingAgentService::find_schedule(const QString& id) const {
    const auto it = schedules_.constFind(id);
    return it == schedules_.cend() ? nullptr : &it.value();
}

QString CodingAgentService::save_schedule(const CodingAgentSchedule& schedule, QString* error) {
    initialize_deferred();
    CodingAgentSchedule s = schedule;
    s.name = s.name.trimmed();
    s.prompt = s.prompt.trimmed();
    s.model = s.model.trimmed();
    const QString invalid = validate_schedule(s);
    if (!invalid.isEmpty()) {
        if (error)
            *error = invalid;
        return {};
    }
    if (!s.model.isEmpty() && !is_valid_model_name(s.model)) {
        if (error)
            *error = QStringLiteral("Model names may only contain letters, digits and . _ - : / @ [ ]");
        return {};
    }
    if (s.agent != AgentKind::Claude)
        s.max_budget_usd = 0.0;
    const qint64 now = now_ms();
    if (s.id.isEmpty() || !schedules_.contains(s.id)) {
        if (s.id.isEmpty())
            s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.created_at_ms = now;
        s.last_run_at_ms = 0;
        schedule_order_.append(s.id);
    } else {
        const CodingAgentSchedule& old = schedules_[s.id];
        s.created_at_ms = old.created_at_ms;
        s.last_run_at_ms = old.last_run_at_ms;
    }
    s.updated_at_ms = now;
    s.next_run_at_ms = s.enabled ? compute_next_run_ms(s, now) : 0;
    schedules_.insert(s.id, s);
    persist_schedule(s);
    rearm_schedule_timer();
    emit schedules_changed();
    LOG_INFO(kCodingAgentSvcTag, QStringLiteral("Schedule '%1' saved (next %2)")
                                     .arg(s.name, s.next_run_at_ms > 0 ? fmt_local(s.next_run_at_ms) : QStringLiteral("—")));
    return s.id;
}

void CodingAgentService::delete_schedule(const QString& id) {
    if (!schedules_.remove(id))
        return;
    schedule_order_.removeAll(id);
    submit_write([id]() { CodingAgentRepository::instance().delete_schedule(id); });
    rearm_schedule_timer();
    emit schedules_changed();
}

QString CodingAgentService::run_schedule_now(const QString& id, QString* error) {
    auto it = schedules_.find(id);
    if (it == schedules_.end()) {
        if (error)
            *error = QStringLiteral("Schedule not found.");
        return {};
    }
    RunRequest req;
    req.agent = it->agent;
    req.prompt = it->prompt;
    req.model = it->model;
    req.mode = it->mode;
    req.use_fincept_tools = it->use_fincept_tools;
    req.max_budget_usd = it->max_budget_usd;
    req.title = QStringLiteral("%1 · %2").arg(it->name, QDateTime::currentDateTime().toString(QStringLiteral("MMM d HH:mm")));
    req.schedule_id = id;
    req.origin = QStringLiteral("user");
    const QString run_id = start_run(req, error);
    if (!run_id.isEmpty()) {
        it = schedules_.find(id); // start_run may have re-entered
        if (it != schedules_.end()) {
            it->last_run_at_ms = now_ms();
            persist_schedule(*it);
            emit schedules_changed();
        }
    }
    return run_id;
}

void CodingAgentService::rearm_schedule_timer() {
    if (shutting_down_ || is_headless_test_run()) {
        schedule_timer_->stop();
        return;
    }
    bool any_enabled = false;
    for (auto it = schedules_.cbegin(); it != schedules_.cend(); ++it) {
        if (it->enabled) {
            any_enabled = true;
            break;
        }
    }
    if (any_enabled && !schedule_timer_->isActive())
        schedule_timer_->start();
    else if (!any_enabled)
        schedule_timer_->stop();
}

void CodingAgentService::on_schedule_tick() {
    const qint64 now = now_ms();
    const QStringList ids = schedule_order_;
    for (const auto& id : ids) {
        const auto it = schedules_.constFind(id);
        if (it == schedules_.cend() || !it->enabled || it->next_run_at_ms <= 0)
            continue;
        if (it->next_run_at_ms <= now)
            fire_schedule(id, now);
    }
}

void CodingAgentService::fire_schedule(const QString& id, qint64 now) {
    auto it = schedules_.find(id);
    if (it == schedules_.end())
        return;
    // Skip an occurrence while the previous one is still queued/running — a slow
    // brief must not pile up paid runs behind itself.
    bool busy = false;
    for (auto r = runs_.cbegin(); r != runs_.cend(); ++r) {
        if (r->schedule_id == id && !is_terminal_status(r->status)) {
            busy = true;
            break;
        }
    }
    const CodingAgentSchedule s = *it;
    if (busy) {
        LOG_INFO(kCodingAgentSvcTag, QStringLiteral("Schedule '%1': previous run still active — occurrence skipped").arg(s.name));
    } else {
        RunRequest req;
        req.agent = s.agent;
        req.prompt = s.prompt;
        req.model = s.model;
        req.mode = s.mode;
        req.use_fincept_tools = s.use_fincept_tools;
        req.max_budget_usd = s.max_budget_usd;
        req.title = QStringLiteral("%1 · %2").arg(s.name, QDateTime::fromMSecsSinceEpoch(now).toString(QStringLiteral("MMM d HH:mm")));
        req.schedule_id = id;
        req.origin = QStringLiteral("schedule");
        QString err;
        if (start_run(req, &err).isEmpty())
            LOG_WARN(kCodingAgentSvcTag, QStringLiteral("Schedule '%1' could not start: %2").arg(s.name, err));
    }
    it = schedules_.find(id); // start_run emits signals; re-find before writing
    if (it == schedules_.end())
        return;
    it->last_run_at_ms = now;
    it->next_run_at_ms = compute_next_run_ms(*it, now);
    persist_schedule(*it);
    emit schedules_changed();
}

} // namespace fincept::services::coding_agents
