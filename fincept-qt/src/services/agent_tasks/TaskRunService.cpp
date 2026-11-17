// TaskRunService.cpp — run registry, FIFO queue, budgets, batched persistence,
// notifications and shutdown for Chat Mode's run engine.

#include "services/agent_tasks/TaskRunService.h"

#include "core/logging/Logger.h"
#include "services/agent_tasks/ArtefactService.h"
#include "services/agent_tasks/CloudPlanExecutor.h"
#include "services/agent_tasks/CloudTurnExecutor.h"
#include "services/agent_tasks/HarnessRunExecutor.h"
#include "services/agent_tasks/LocalAgenticExecutor.h"
#include "services/agent_tasks/TaskRunScheduler.h"
#include "services/agent_tasks/TaskRunStore.h"
#include "ui/notifications/DesktopNotifier.h"
#include "ui/notifications/NotificationService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QPointer>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace fincept::agent_tasks {

namespace task_run_service_detail {

constexpr const char* kTaskRunSvcTag = "TaskRuns";
constexpr int kTrsFlushIntervalMs = 500;
constexpr int kTrsFlushBatchSize = 50;
constexpr int kTrsBudgetTickMs = 10000;
constexpr int kTrsEventCacheRuns = 16;
constexpr int kTrsLiveTextCapChars = 4 * 1024 * 1024;
constexpr int kTrsErrorCapChars = 8000;

qint64 trs_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

QString trs_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::TaskRunService", text);
}

/// A title for a run started without one: the prompt's first line, shortened.
QString trs_title_from_prompt(const QString& prompt) {
    QString first = prompt.trimmed().section(QLatin1Char('\n'), 0, 0).simplified();
    if (first.size() > 80)
        first = first.left(79) + QChar(0x2026);
    return first.isEmpty() ? trs_tr("Untitled run") : first;
}

QString trs_duration(qint64 ms) {
    const qint64 s = qMax<qint64>(0, ms) / 1000;
    if (s < 60)
        return QStringLiteral("%1s").arg(s);
    if (s < 3600)
        return QStringLiteral("%1m %2s").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1h %2m").arg(s / 3600).arg((s % 3600) / 60, 2, 10, QLatin1Char('0'));
}

bool trs_is_essential(const RunEvent& e) {
    return e.kind == RunEventKind::Final || e.kind == RunEventKind::Error || e.kind == RunEventKind::Question ||
           e.kind == RunEventKind::Approval || e.kind == RunEventKind::ApprovalDecided ||
           (e.kind == RunEventKind::Usage && e.data.value(QStringLiteral("turn_end")).toBool()) ||
           (e.kind == RunEventKind::Status && e.is_error);
}

} // namespace task_run_service_detail

using namespace task_run_service_detail;

// ── Construction ─────────────────────────────────────────────────────────────

TaskRunService& TaskRunService::instance() {
    static TaskRunService s;
    return s;
}

TaskRunService::TaskRunService() : QObject(nullptr) {
    session_start_ms_ = trs_now_ms();
    qRegisterMetaType<fincept::agent_tasks::RunStatus>();
    qRegisterMetaType<fincept::agent_tasks::StepStatus>();
    qRegisterMetaType<fincept::agent_tasks::RunEvent>();
    qRegisterMetaType<fincept::agent_tasks::RunStep>();
    qRegisterMetaType<fincept::agent_tasks::RunArtefact>();
    qRegisterMetaType<QVector<fincept::agent_tasks::RunStep>>();

    writer_ = new QThreadPool(this);
    writer_->setMaxThreadCount(1); // one thread => FIFO => a run row is always written before its rows

    flush_timer_ = new QTimer(this);
    flush_timer_->setSingleShot(true);
    flush_timer_->setInterval(kTrsFlushIntervalMs);
    connect(flush_timer_, &QTimer::timeout, this, &TaskRunService::flush_events);

    budget_timer_ = new QTimer(this);
    budget_timer_->setInterval(kTrsBudgetTickMs);
    connect(budget_timer_, &QTimer::timeout, this, &TaskRunService::on_budget_tick);

    if (auto* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &TaskRunService::on_about_to_quit);
        if (QThread::currentThread() != app->thread()) {
            LOG_WARN(kTaskRunSvcTag, "Service constructed off the GUI thread — moving it to the GUI thread");
            moveToThread(app->thread());
        }
    }
}

TaskRunService::~TaskRunService() = default;

// ── Lifecycle ────────────────────────────────────────────────────────────────

void TaskRunService::initialize_deferred() {
    if (initialized_)
        return;
    initialized_ = true;

    register_executor(new CloudTurnExecutor);
    register_executor(new CloudPlanExecutor);
    register_executor(new LocalAgenticExecutor);
    register_executor(new HarnessRunExecutor); // engine "harness": Claude Code / Codex (API key only)
    ArtefactService::instance().initialize_deferred();

    const qint64 session_start = session_start_ms_;
    QPointer<TaskRunService> self(this);
    submit_write([self, session_start]() {
        auto& store = TaskRunStore::instance();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        auto marked = store.mark_unfinished_interrupted(
            session_start, now, QStringLiteral("The terminal closed while this run was in progress."));
        auto pruned = store.prune_runs(kRetainRuns);
        auto runs = store.load_runs(kRetainRuns);
        auto steps = store.load_steps(kRetainRuns);
        auto artefacts = store.load_artefacts(kRetainRuns);
        auto seqs = store.load_max_event_seq(kRetainRuns);
        QVector<TaskRun> run_list = runs.is_ok() ? runs.value() : QVector<TaskRun>{};
        QVector<RunStep> step_list = steps.is_ok() ? steps.value() : QVector<RunStep>{};
        QVector<RunArtefact> artefact_list = artefacts.is_ok() ? artefacts.value() : QVector<RunArtefact>{};
        QHash<QString, int> seq_map = seqs.is_ok() ? seqs.value() : QHash<QString, int>{};
        if (runs.is_err())
            LOG_WARN(kTaskRunSvcTag, QStringLiteral("Loading runs failed: %1").arg(QString::fromStdString(runs.error())));
        LOG_DEBUG(kTaskRunSvcTag, QStringLiteral("Loaded %1 runs, %2 steps, %3 artefacts (interrupted %4, pruned %5)")
                                     .arg(run_list.size())
                                     .arg(step_list.size())
                                     .arg(artefact_list.size())
                                     .arg(marked.is_ok() ? marked.value() : 0)
                                     .arg(pruned.is_ok() ? pruned.value() : 0));
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, run_list, step_list, artefact_list, seq_map]() {
                if (self)
                    self->on_loaded(run_list, step_list, artefact_list, seq_map);
            },
            Qt::QueuedConnection);
    });
}

void TaskRunService::register_executor(IRunExecutor* executor) {
    if (!executor)
        return;
    const QString id = executor->engine_id();
    if (id.isEmpty() || executors_.contains(id)) {
        LOG_WARN(kTaskRunSvcTag, QStringLiteral("Executor '%1' not registered (empty id or already registered)").arg(id));
        executor->deleteLater();
        return;
    }
    executor->setParent(this);
    executors_.insert(id, executor);
    connect(executor, &IRunExecutor::event, this, &TaskRunService::on_executor_event);
    connect(executor, &IRunExecutor::text_delta, this, &TaskRunService::on_executor_text);
    connect(executor, &IRunExecutor::plan_ready, this, &TaskRunService::on_executor_plan);
    connect(executor, &IRunExecutor::step_changed, this, &TaskRunService::on_executor_step);
    connect(executor, &IRunExecutor::usage, this, &TaskRunService::on_executor_usage);
    connect(executor, &IRunExecutor::waiting, this, &TaskRunService::on_executor_waiting);
    connect(executor, &IRunExecutor::finished, this, &TaskRunService::on_executor_finished);
}

void TaskRunService::on_loaded(const QVector<TaskRun>& runs, const QVector<RunStep>& steps,
                               const QVector<RunArtefact>& artefacts, const QHash<QString, int>& max_seq) {
    int interrupted = 0;
    for (const TaskRun& r : runs) {
        if (runs_.contains(r.id))
            continue; // started in this session before the load landed
        runs_.insert(r.id, r);
        if (r.status == RunStatus::Interrupted)
            ++interrupted;
        // Events recorded later (an artefact added after a restart) continue the sequence.
        if (!next_seq_.contains(r.id))
            next_seq_.insert(r.id, max_seq.value(r.id, 0) + 1);
    }
    for (const RunStep& s : steps) {
        if (!runs_.contains(s.run_id))
            continue;
        auto& vec = steps_[s.run_id];
        if (std::none_of(vec.cbegin(), vec.cend(), [&s](const RunStep& x) { return x.idx == s.idx; }))
            vec.append(s);
    }
    order_ = runs_.keys();
    std::sort(order_.begin(), order_.end(), [this](const QString& a, const QString& b) {
        return runs_.constFind(a)->created_at_ms > runs_.constFind(b)->created_at_ms;
    });
    loaded_ = true;
    LOG_DEBUG(kTaskRunSvcTag, QStringLiteral("Run list ready: %1 runs (%2 interrupted)").arg(order_.size()).arg(interrupted));
    emit runs_reset();
    report_attention();
    // Artefacts (pinned ones of deleted runs too) and their retention sweep, which
    // needs the run list just loaded; then the schedules.
    ArtefactService::instance().adopt_loaded(artefacts);
    TaskRunScheduler::instance().initialize_deferred();
}

void TaskRunService::on_about_to_quit() {
    if (shutting_down_)
        return;
    shutting_down_ = true;
    budget_timer_->stop();
    const qint64 now = trs_now_ms();
    int stopped = 0;
    for (auto it = runs_.begin(); it != runs_.end(); ++it) {
        if (!is_active(it->status))
            continue;
        const bool started = it->status != RunStatus::Queued;
        account_active_time(*it, RunStatus::Interrupted, now);
        it->status = RunStatus::Interrupted;
        it->finished_at_ms = now;
        it->updated_at_ms = now;
        it->detached = false;
        it->recovering = false;
        if (it->error.isEmpty())
            it->error = started ? trs_tr("The terminal was closed while this run was in progress.")
                                : trs_tr("The terminal was closed before this run started.");
        persist_run(it.key());
        ++stopped;
    }
    queue_.clear();
    flush_events();
    // Executors hold network replies and timers: release them while the event
    // loop machinery still exists, without letting them signal into a half-shut service.
    const auto executors = executors_.values();
    executors_.clear();
    for (IRunExecutor* ex : executors) {
        disconnect(ex, nullptr, this, nullptr);
        delete ex;
    }
    // Teardown is the one place a bounded wait on the GUI thread is acceptable.
    writer_->waitForDone(5000);
    if (stopped > 0)
        LOG_INFO(kTaskRunSvcTag, QStringLiteral("Shutdown: %1 run(s) interrupted").arg(stopped));
}

void TaskRunService::set_view_visible(bool visible) {
    view_visible_ = qMax(0, view_visible_ + (visible ? 1 : -1));
}

// ── Queries ──────────────────────────────────────────────────────────────────

QStringList TaskRunService::run_ids() const {
    return order_;
}

const TaskRun* TaskRunService::find_run(const QString& id) const {
    const auto it = runs_.constFind(id);
    return it == runs_.cend() ? nullptr : &it.value();
}

QVector<RunStep> TaskRunService::steps(const QString& id) const {
    return steps_.value(id);
}

QVector<RunArtefact> TaskRunService::artefacts(const QString& id) const {
    return ArtefactService::instance().artefacts(id);
}

QString TaskRunService::live_text(const QString& id) const {
    return live_text_.value(id);
}

QVector<RunEvent> TaskRunService::cached_events(const QString& id) const {
    return events_.value(id);
}

QString TaskRunService::open_run_for_session(const QString& session_uuid) const {
    if (session_uuid.isEmpty())
        return {};
    for (const QString& id : order_) {
        const auto it = runs_.constFind(id);
        if (it != runs_.cend() && it->session_uuid == session_uuid && !is_terminal(it->status))
            return id;
    }
    return {};
}

QString TaskRunService::latest_run_for_session(const QString& session_uuid) const {
    if (session_uuid.isEmpty())
        return {};
    for (const QString& id : order_) {
        const auto it = runs_.constFind(id);
        if (it != runs_.cend() && it->session_uuid == session_uuid)
            return id;
    }
    return {};
}

QStringList TaskRunService::active_run_ids(const QString& engine) const {
    QStringList out;
    for (const QString& id : order_) {
        const auto it = runs_.constFind(id);
        if (it != runs_.cend() && is_active(it->status) && (engine.isEmpty() || it->engine == engine))
            out << id;
    }
    return out;
}

int TaskRunService::active_count() const {
    int n = 0;
    for (auto it = runs_.cbegin(); it != runs_.cend(); ++it)
        if (is_active(it->status))
            ++n;
    return n;
}

int TaskRunService::attention_count() const {
    int n = 0;
    for (auto it = runs_.cbegin(); it != runs_.cend(); ++it)
        if (needs_attention(it->status))
            ++n;
    return n;
}

int TaskRunService::queue_position(const QString& id) const {
    const int i = static_cast<int>(queue_.indexOf(id));
    return i < 0 ? 0 : i + 1;
}

int TaskRunService::steps_started(const QString& run_id) const {
    int n = 0;
    for (const RunStep& s : steps_.value(run_id))
        if (s.status != StepStatus::Pending && s.status != StepStatus::Skipped)
            ++n;
    return n;
}

IRunExecutor* TaskRunService::executor_of(const TaskRun& run) const {
    return executors_.value(run.executor_id(), nullptr);
}

// ── Starting runs ────────────────────────────────────────────────────────────

QString TaskRunService::start_run(const RunRequest& request, QString* error) {
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        LOG_WARN(kTaskRunSvcTag, QStringLiteral("Run not started: %1").arg(message));
        return QString();
    };
    initialize_deferred();
    if (shutting_down_)
        return fail(trs_tr("The terminal is shutting down."));

    const QString prompt = request.prompt.trimmed();
    if (prompt.isEmpty())
        return fail(trs_tr("The message is empty."));
    if (prompt.size() > kRunPromptMaxChars)
        return fail(trs_tr("The message is too long (%1 characters; at most %2).").arg(prompt.size()).arg(kRunPromptMaxChars));

    const QString engine = request.engine.isEmpty() ? QString::fromLatin1(kEngineCloud) : request.engine;
    const QString executor_id = executor_for(engine, request.plan_first);
    if (executor_id.isEmpty() || !executors_.contains(executor_id))
        return fail(trs_tr("The '%1' engine is not available in this build.").arg(engine));

    // One conversation answers one message at a time: a second send while a reply
    // is still being written (or recovered after Stop) would interleave two runs of
    // the same server-side agent thread.
    if (!request.session_uuid.isEmpty()) {
        const QStringList ids = order_; // events below emit signals: iterate a copy
        for (const QString& id : ids) {
            auto it = runs_.find(id);
            if (it == runs_.end() || it->session_uuid != request.session_uuid || is_terminal(it->status))
                continue;
            if (is_active(it->status))
                return fail(trs_tr("A reply is still being written in this conversation. Wait for it to finish "
                                   "(after Stop, for its stored answer to arrive) before sending another message."));
            if (it->status == RunStatus::WaitingInput && it->executor_id() == QLatin1String(kExecutorCloudTurn)) {
                // The user asked something else instead of answering: that question is closed.
                IRunExecutor* ex = executor_of(*it);
                RunEvent note;
                note.kind = RunEventKind::Note;
                note.title = trs_tr("Not answered: a new message was sent instead");
                record_event(id, note);
                if (ex)
                    ex->cancel(id); // drops the executor's state; it reports nothing more
                finish(id, RunStatus::Completed, {}, {});
                continue;
            }
            return fail(trs_tr("Another task of this conversation is %1. Resume or cancel it first.")
                            .arg(run_status_label(it->status).toLower()));
        }
    }

    if (queue_.size() >= kMaxQueuedRuns)
        return fail(trs_tr("%1 runs are already waiting to start. Wait for some to finish, or cancel queued runs.")
                        .arg(queue_.size()));

    const qint64 now = trs_now_ms();
    TaskRun run;
    run.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    run.title = request.title.trimmed().isEmpty() ? trs_title_from_prompt(prompt) : request.title.trimmed().left(200);
    run.prompt = prompt;
    run.engine = engine;
    run.plan_first = request.plan_first;
    run.mode = request.mode.isEmpty() && engine == QLatin1String(kEngineCloud) ? QStringLiteral("lite") : request.mode;
    run.template_id = request.template_id;
    run.session_uuid = request.session_uuid;
    run.profile_id = qMax(0, request.profile_id);
    run.context = request.context;
    run.budget.max_credits = qMax(0.0, request.budget.max_credits);
    run.budget.max_steps = qMax(0, request.budget.max_steps);
    run.budget.max_minutes = qMax(0, request.budget.max_minutes);
    run.origin = request.origin.isEmpty() ? QString::fromLatin1(kOriginUser) : request.origin;
    run.parent_run_id = request.parent_run_id;
    run.schedule_id = request.schedule_id;
    run.status = RunStatus::Queued;
    run.created_at_ms = now;
    run.updated_at_ms = now;
    LOG_INFO(kTaskRunSvcTag, QStringLiteral("Run queued run=%1 engine=%2 executor=%3 mode=%4 session=%5 query_chars=%6")
                                 .arg(run.id.left(8), run.engine, executor_id, run.mode.isEmpty() ? QStringLiteral("-") : run.mode,
                                      run.session_uuid.isEmpty() ? QStringLiteral("-") : run.session_uuid.left(8))
                                 .arg(run.prompt.size()));
    return enqueue(run, {});
}

QString TaskRunService::enqueue(TaskRun run, const QVector<RunStep>& steps) {
    const QString id = run.id;
    runs_.insert(id, run);
    order_.prepend(id);
    queue_.append(id);
    if (!steps.isEmpty()) {
        steps_.insert(id, steps);
        persist_run(id);
        persist_steps(id);
    } else {
        persist_run(id);
    }
    events_.insert(id, {});
    touch_event_cache(id);
    emit run_added(id);
    report_attention();
    schedule_pump();
    return id;
}

void TaskRunService::schedule_pump() {
    if (pump_scheduled_ || shutting_down_)
        return;
    pump_scheduled_ = true;
    // Queued: callers get their run id (and connect) before the run starts, and
    // nothing re-enters start_run() from inside a signal handler.
    QTimer::singleShot(0, this, [this]() {
        pump_scheduled_ = false;
        pump_queue();
    });
}

void TaskRunService::pump_queue() {
    if (shutting_down_)
        return;
    int busy = 0;
    for (auto it = runs_.cbegin(); it != runs_.cend(); ++it)
        if (holds_slot(it->status))
            ++busy;
    while (busy < kMaxConcurrentRuns && !queue_.isEmpty()) {
        const QString id = queue_.takeFirst();
        const auto it = runs_.constFind(id);
        if (it == runs_.cend() || it->status != RunStatus::Queued)
            continue;
        launch(id);
        ++busy;
    }
    const QStringList waiting = queue_;
    for (const QString& id : waiting)
        emit run_updated(id); // queue positions moved
}

void TaskRunService::launch(const QString& run_id) {
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return;
    IRunExecutor* ex = executor_of(*it);
    if (!ex) {
        finish(run_id, RunStatus::Failed, {}, trs_tr("The '%1' engine is not available in this build.").arg(it->engine));
        return;
    }
    const bool plans = it->executor_id() != QLatin1String(kExecutorCloudTurn);
    if (!set_status(run_id, plans && steps_.value(run_id).isEmpty() ? RunStatus::Planning : RunStatus::Running))
        return;
    it = runs_.find(run_id);
    if (it == runs_.end())
        return;
    if (it->started_at_ms == 0)
        it->started_at_ms = trs_now_ms();
    it->detached = false;
    it->recovering = false;
    it->activity.clear();
    live_text_.remove(run_id);
    persist_run(run_id);
    if (!budget_timer_->isActive())
        budget_timer_->start();
    const TaskRun snapshot = *it;
    const QVector<RunStep> steps = steps_.value(run_id);
    LOG_INFO(kTaskRunSvcTag, QStringLiteral("Run started run=%1 executor=%2 mode=%3 step=%4 origin=%5")
                                 .arg(run_id.left(8), ex->engine_id(),
                                      snapshot.mode.isEmpty() ? QStringLiteral("-") : snapshot.mode)
                                 .arg(snapshot.current_step + 1)
                                 .arg(snapshot.origin.isEmpty() ? QStringLiteral("-") : snapshot.origin));
    if (auto again = runs_.find(run_id); again != runs_.end())
        again->resume_after_restart = false; // handed to the executor once
    ex->start(snapshot, steps);
}

// ── Control ──────────────────────────────────────────────────────────────────

bool TaskRunService::can_pause(const QString& id) const {
    const auto it = runs_.constFind(id);
    if (it == runs_.cend())
        return false;
    const IRunExecutor* ex = executor_of(*it);
    return ex && ex->can_pause(id);
}

bool TaskRunService::pause_run(const QString& id) {
    auto it = runs_.find(id);
    if (it == runs_.end())
        return false;
    if (it->status == RunStatus::Queued) {
        queue_.removeAll(id);
        return set_status(id, RunStatus::Paused, trs_tr("Paused before it started"));
    }
    if (!holds_slot(it->status))
        return false;
    IRunExecutor* ex = executor_of(*it);
    if (!ex || !ex->can_pause(id))
        return false;
    it->activity = trs_tr("Pausing after the current step…");
    emit run_updated(id);
    ex->request_pause(id);
    return true;
}

bool TaskRunService::cancel_run(const QString& id) {
    auto it = runs_.find(id);
    if (it == runs_.end() || is_terminal(it->status))
        return false;
    if (it->status == RunStatus::Queued) {
        finish(id, RunStatus::Cancelled, {}, trs_tr("Cancelled before it started; nothing was charged."));
        return true;
    }
    IRunExecutor* ex = executor_of(*it);
    if (holds_slot(it->status)) {
        if (!ex) {
            finish(id, RunStatus::Cancelled, {}, {});
            return true;
        }
        // The executor reports the outcome (a cloud turn detaches and recovers the
        // answer the server still stores; others stop and report Cancelled).
        ex->cancel(id);
        return true;
    }
    // Waiting / paused: nothing is in flight.
    if (ex)
        ex->cancel(id);
    it = runs_.find(id);
    if (it != runs_.end() && !is_terminal(it->status))
        finish(id, RunStatus::Cancelled, {}, {});
    return true;
}

bool TaskRunService::answer(const QString& id, const QString& text) {
    auto it = runs_.find(id);
    const QString reply = text.trimmed();
    if (it == runs_.end() || it->status != RunStatus::WaitingInput || reply.isEmpty() ||
        reply.size() > kChatMessageMaxChars)
        return false;
    IRunExecutor* ex = executor_of(*it);
    if (!ex)
        return false;
    RunEvent ev;
    ev.kind = RunEventKind::Answer;
    ev.title = trs_tr("Your answer");
    ev.body = reply;
    record_event(id, ev);
    live_text_.remove(id);
    it = runs_.find(id);
    it->waiting_payload = {};
    if (!set_status(id, RunStatus::Running))
        return false;
    ex->answer(id, reply);
    return true;
}

QString TaskRunService::resume_run(const QString& id, QString* error) {
    auto fail = [error](const QString& m) {
        if (error)
            *error = m;
        return QString();
    };
    initialize_deferred();
    auto it = runs_.find(id);
    if (it == runs_.end())
        return fail(trs_tr("Run not found."));
    if (!can_resume(it->status))
        return fail(trs_tr("Only a paused or interrupted run can be resumed (this one is %1).")
                        .arg(run_status_label(it->status).toLower()));
    if (!executor_of(*it))
        return fail(trs_tr("The '%1' engine is not available in this build.").arg(it->engine));
    if (queue_.size() >= kMaxQueuedRuns)
        return fail(trs_tr("Too many runs are waiting to start."));
    // Only a run the terminal's closing caught can have a turn the server finished
    // and stored meanwhile; a paused run stopped at a step boundary.
    const bool after_restart = it->status == RunStatus::Interrupted;
    it->finished_at_ms = 0;
    it->error.clear();
    if (!set_status(id, RunStatus::Queued, trs_tr("Waiting for a free slot")))
        return fail(trs_tr("This run cannot be resumed."));
    if (auto again = runs_.find(id); again != runs_.end())
        again->resume_after_restart = after_restart;
    queue_.append(id);
    schedule_pump();
    return id;
}

QString TaskRunService::retry_from_step(const QString& id, int step, QString* error) {
    auto fail = [error](const QString& m) {
        if (error)
            *error = m;
        return QString();
    };
    initialize_deferred();
    const auto src = runs_.constFind(id);
    if (src == runs_.cend())
        return fail(trs_tr("Run not found."));
    if (is_active(src->status))
        return fail(trs_tr("Stop the run before retrying it."));
    if (queue_.size() >= kMaxQueuedRuns)
        return fail(trs_tr("Too many runs are waiting to start."));
    const QVector<RunStep> old_steps = steps_.value(id);
    const int from = old_steps.isEmpty() ? 0 : std::clamp(step, 0, static_cast<int>(old_steps.size()) - 1);

    const qint64 now = trs_now_ms();
    TaskRun run = *src;
    run.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    run.title = trs_tr("Retry: %1").arg(src->title).left(200);
    run.status = RunStatus::Queued;
    run.current_step = from;
    run.credits_spent = 0;
    run.tokens_used = 0;
    run.turns = 0;
    run.final_text.clear();
    run.error.clear();
    run.origin = QString::fromLatin1(kOriginRetry);
    run.parent_run_id = id;
    run.remote_task_id.clear();
    run.created_at_ms = now;
    run.started_at_ms = 0;
    run.finished_at_ms = 0;
    run.updated_at_ms = now;
    run.activity.clear();
    run.waiting_payload = {};
    run.detached = false;
    run.recovering = false;
    run.active_ms = 0;
    run.active_since_ms = 0;
    run.credits_unreported = 0;
    run.resume_after_restart = false;

    QVector<RunStep> steps;
    steps.reserve(old_steps.size());
    for (RunStep s : old_steps) {
        s.run_id = run.id;
        if (s.idx >= from) {
            s.status = StepStatus::Pending;
            s.summary.clear();
            s.attempts = 0;
            s.credits = 0;
            s.message_uuid.clear();
            s.started_at_ms = 0;
            s.finished_at_ms = 0;
        }
        steps.append(s);
    }
    LOG_INFO(kTaskRunSvcTag, QStringLiteral("Run %1 retried from step %2 as %3").arg(id.left(8)).arg(from + 1).arg(run.id.left(8)));
    return enqueue(run, steps);
}

int TaskRunService::delete_runs(const QStringList& ids) {
    QStringList gone;
    for (const QString& id : ids) {
        const auto it = runs_.constFind(id);
        if (it == runs_.cend() || !is_terminal(it->status))
            continue; // never delete a run that is still going (or waiting / paused)
        gone << id;
    }
    if (gone.isEmpty())
        return 0;
    flush_events(); // its pending rows go first; the FIFO writer then deletes them
    for (const QString& id : std::as_const(gone)) {
        runs_.remove(id);
        order_.removeAll(id);
        steps_.remove(id);
        events_.remove(id);
        events_lru_.removeAll(id);
        next_seq_.remove(id);
        live_text_.remove(id);
    }
    submit_write([gone]() {
        auto r = TaskRunStore::instance().delete_runs(gone);
        if (r.is_err())
            LOG_WARN(kTaskRunSvcTag, QStringLiteral("delete_runs failed: %1").arg(QString::fromStdString(r.error())));
    });
    // Their unpinned artefacts go with them (rows and files); pinned ones stay.
    ArtefactService::instance().on_runs_deleted(gone);
    for (const QString& id : std::as_const(gone))
        emit run_updated(id); // find_run(id) is null now: views drop the row
    report_attention();
    return static_cast<int>(gone.size());
}

QString TaskRunService::add_artefact(const QString& run_id, const RunArtefact& artefact) {
    if (!runs_.contains(run_id))
        return {};
    // ArtefactService keeps the index and the row; it calls note_artefact() once
    // the row is stored.
    return ArtefactService::instance().add_row(run_id, artefact);
}

void TaskRunService::note_artefact(const RunArtefact& a) {
    if (!runs_.contains(a.run_id))
        return;
    RunEvent ev;
    ev.kind = RunEventKind::Artefact;
    ev.step = a.step_idx;
    ev.title = a.title;
    ev.ref_id = a.id;
    ev.data["kind"] = a.kind;
    ev.data["mime"] = a.mime;
    ev.data["bytes"] = static_cast<double>(a.bytes);
    if (!a.source.isEmpty())
        ev.data["source"] = a.source;
    record_event(a.run_id, ev);
    emit artefact_added(a.run_id, a.id);
}

// ── Events ───────────────────────────────────────────────────────────────────

void TaskRunService::record_event(const QString& run_id, const RunEvent& in) {
    auto run = runs_.find(run_id);
    if (run == runs_.end())
        return;
    RunEvent ev = in;

    // Conventions (IRunExecutor.h): bind ids, count turns.
    bool binding_changed = false;
    if (ev.kind == RunEventKind::Status) {
        const QString session = ev.data.value(QStringLiteral("session_uuid")).toString();
        if (!session.isEmpty() && run->session_uuid.isEmpty()) {
            run->session_uuid = session;
            binding_changed = true;
        }
        const QString remote = ev.data.value(QStringLiteral("remote_task_id")).toString();
        if (!remote.isEmpty() && run->remote_task_id.isEmpty()) {
            run->remote_task_id = remote;
            binding_changed = true;
        }
        if (binding_changed)
            persist_run(run_id);
    } else if (ev.kind == RunEventKind::Usage && ev.data.value(QStringLiteral("turn_end")).toBool()) {
        ++run->turns;
        // An answer recovered from the stored conversation: the server charged the
        // turn but the bill never streamed — credits_spent is a lower bound now.
        if (ev.data.value(QStringLiteral("credits_unreported")).toBool())
            ++run->credits_unreported;
        persist_run(run_id);
        binding_changed = true; // the views show the turn / unreported count
    }
    const int current_step = run->current_step;

    auto& vec = events_[run_id];
    int& seq = next_seq_[run_id];
    if (seq <= 0)
        seq = vec.isEmpty() ? 1 : vec.constLast().seq + 1;
    RunEvent emitted;
    if (vec.size() >= kMaxEventsPerRun && !trs_is_essential(ev)) {
        if (dropped_events_[run_id]++ != 0) {
            if (binding_changed)
                emit run_updated(run_id);
            return;
        }
        emitted.kind = RunEventKind::Note;
        emitted.title = trs_tr("Activity limit reached");
        emitted.body = trs_tr("%1 events recorded — from now on only questions, approvals, errors and the result "
                              "are kept. The run continues.")
                           .arg(kMaxEventsPerRun);
    } else {
        emitted = ev;
    }
    emitted.seq = seq++;
    if (emitted.ts_ms <= 0)
        emitted.ts_ms = trs_now_ms();
    if (emitted.step < 0)
        emitted.step = current_step;
    vec.append(emitted);
    pending_events_.append({run_id, emitted});
    if (pending_events_.size() >= kTrsFlushBatchSize)
        flush_events();
    else if (!flush_timer_->isActive())
        flush_timer_->start();
    // Signals last: a slot may re-enter and change runs_ / events_.
    if (binding_changed)
        emit run_updated(run_id);
    emit run_event(run_id, emitted);
}

void TaskRunService::fetch_events(const QString& id, QObject* ctx, std::function<void(const QVector<RunEvent>&)> cb) {
    if (!cb)
        return;
    if (events_.contains(id)) {
        cb(events_.value(id));
        return;
    }
    flush_events(); // FIFO writer: anything still pending lands before the read
    QPointer<QObject> context(ctx);
    QPointer<TaskRunService> self(this);
    submit_write([self, context, id, cb]() {
        auto r = TaskRunStore::instance().load_events(id, 0);
        QVector<RunEvent> events = r.is_ok() ? r.value() : QVector<RunEvent>{};
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, context, id, events, cb]() {
                if (!self)
                    return;
                self->cache_loaded_events(id, events);
                if (context)
                    cb(self->events_.value(id, events));
            },
            Qt::QueuedConnection);
    });
}

void TaskRunService::cache_loaded_events(const QString& run_id, const QVector<RunEvent>& events) {
    if (!runs_.contains(run_id) || events_.contains(run_id))
        return; // deleted meanwhile, or live (the live copy is authoritative)
    events_.insert(run_id, events);
    touch_event_cache(run_id);
}

void TaskRunService::touch_event_cache(const QString& run_id) {
    events_lru_.removeAll(run_id);
    events_lru_.append(run_id);
    for (int guard = 0; events_lru_.size() > kTrsEventCacheRuns && guard < events_lru_.size(); ++guard) {
        const QString victim = events_lru_.first();
        const TaskRun* r = find_run(victim);
        if (r && !is_terminal(r->status)) {
            events_lru_.move(0, events_lru_.size() - 1); // never evict a live run's events
            continue;
        }
        events_lru_.removeFirst();
        events_.remove(victim); // next_seq_ stays: a later event must not reuse a seq
    }
}

// ── Executor signals ─────────────────────────────────────────────────────────

void TaskRunService::on_executor_event(const QString& run_id, const RunEvent& ev) {
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return;
    // A connection / detach state change is shown live on the run (not persisted).
    if (ev.kind == RunEventKind::Status && !is_terminal(it->status)) {
        bool changed = false;
        if (ev.data.contains(QStringLiteral("recovering"))) {
            it->recovering = ev.data.value(QStringLiteral("recovering")).toBool();
            changed = true;
        }
        if (ev.data.contains(QStringLiteral("detached"))) {
            it->detached = ev.data.value(QStringLiteral("detached")).toBool();
            changed = true;
        }
        if (!ev.data.value(QStringLiteral("verbose")).toBool() && !ev.title.isEmpty()) {
            it->activity = ev.title;
            changed = true;
        }
        if (changed)
            emit run_updated(run_id);
    }
    // A planned run's next turn (a step, or the final report) starts: the text kept for
    // a transcript that attaches later is this turn's only — the earlier turns are stored
    // messages of the conversation, and joined to them they showed as one reply.
    if (ev.kind == RunEventKind::Status && ev.data.contains(QStringLiteral("turn")))
        live_text_.remove(run_id);
    record_event(run_id, ev);
}

void TaskRunService::on_executor_text(const QString& run_id, int step, const QString& chunk) {
    if (!runs_.contains(run_id) || chunk.isEmpty())
        return;
    QString& text = live_text_[run_id];
    if (text.size() < kTrsLiveTextCapChars)
        text += chunk;
    emit run_text_delta(run_id, step, chunk);
}

void TaskRunService::on_executor_plan(const QString& run_id, const QVector<RunStep>& plan) {
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return;
    QVector<RunStep> steps = plan;
    for (int i = 0; i < steps.size(); ++i) {
        steps[i].run_id = run_id;
        steps[i].idx = i;
    }
    steps_.insert(run_id, steps);
    it->plan = steps_to_plan_json(steps);
    const bool was_planning = it->status == RunStatus::Planning;
    persist_run(run_id);
    persist_steps(run_id);
    emit steps_changed(run_id);
    if (was_planning)
        set_status(run_id, RunStatus::Running);
    enforce_budget(run_id);
}

void TaskRunService::on_executor_step(const QString& run_id, int idx, StepStatus status, const QString& summary) {
    auto it = runs_.find(run_id);
    if (it == runs_.end() || idx < 0)
        return;
    auto& steps = steps_[run_id];
    while (steps.size() <= idx) {
        RunStep s;
        s.run_id = run_id;
        s.idx = static_cast<int>(steps.size());
        s.title = trs_tr("Step %1").arg(s.idx + 1);
        steps.append(s);
    }
    RunStep& step = steps[idx];
    const StepStatus before = step.status;
    const qint64 now = trs_now_ms();
    step.status = status;
    if (status == StepStatus::Running) {
        if (before != StepStatus::Running) {
            ++step.attempts;
            step.started_at_ms = now;
            step.finished_at_ms = 0;
        }
        it->current_step = idx;
    } else if (status != StepStatus::Pending) {
        step.finished_at_ms = now;
    }
    if (!summary.isEmpty())
        step.summary = summary.left(20000);
    const RunStep snapshot = step;
    const bool single = steps.size() <= 1;
    persist_step(snapshot);
    persist_run(run_id);

    if (before != status && (status == StepStatus::Running || status == StepStatus::Done ||
                             status == StepStatus::Failed || status == StepStatus::Skipped)) {
        RunEvent ev;
        ev.kind = status == StepStatus::Running ? RunEventKind::StepStart : RunEventKind::StepEnd;
        ev.step = idx;
        ev.title = snapshot.title.isEmpty() ? trs_tr("Step %1").arg(idx + 1) : snapshot.title;
        ev.is_error = status == StepStatus::Failed;
        ev.data["status"] = step_status_id(status);
        if (single)
            ev.data["verbose"] = true; // a one-step run: the step IS the run
        record_event(run_id, ev);
    }
    emit steps_changed(run_id);
    emit run_updated(run_id);
    if (status == StepStatus::Running)
        enforce_budget(run_id);
}

void TaskRunService::on_executor_usage(const QString& run_id, double credits, int tokens) {
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return;
    bool changed = false;
    if (credits > 0) {
        it->credits_spent += credits;
        auto& steps = steps_[run_id];
        const int cur = it->current_step;
        if (cur >= 0 && cur < steps.size()) {
            steps[cur].credits += credits;
            persist_step(steps[cur]);
        }
        changed = true;
    }
    if (tokens > 0) {
        it->tokens_used += tokens;
        changed = true;
    }
    if (!changed)
        return;
    persist_run(run_id);
    emit run_updated(run_id);
    enforce_budget(run_id);
}

void TaskRunService::on_executor_waiting(const QString& run_id, RunStatus why, const QJsonObject& payload) {
    auto it = runs_.find(run_id);
    if (it == runs_.end() || is_terminal(it->status))
        return;
    if (why != RunStatus::WaitingInput && why != RunStatus::WaitingApproval && why != RunStatus::Paused) {
        LOG_WARN(kTaskRunSvcTag, QStringLiteral("Executor reported waiting with %1 — ignored").arg(run_status_id(why)));
        return;
    }
    it->waiting_payload = payload;
    it->detached = false;
    it->recovering = false;
    if (!set_status(run_id, why, QString()))
        return;
    const TaskRun snapshot = runs_.value(run_id);
    notify(snapshot);
    schedule_pump(); // its slot is free while it waits
}

void TaskRunService::on_executor_finished(const QString& run_id, RunStatus terminal, const QString& final_text,
                                          const QString& error) {
    if (!runs_.contains(run_id))
        return;
    RunStatus status = is_terminal(terminal) ? terminal : RunStatus::Failed;
    QString err = error;
    const auto budget = budget_stop_.constFind(run_id);
    if (budget != budget_stop_.cend() && (status == RunStatus::Cancelled || status == RunStatus::BudgetStopped)) {
        status = RunStatus::BudgetStopped;
        if (err.isEmpty())
            err = budget.value();
    }
    finish(run_id, status, final_text, err);
}

void TaskRunService::finish(const QString& run_id, RunStatus status, const QString& final_text, const QString& error) {
    auto it = runs_.find(run_id);
    if (it == runs_.end() || is_terminal(it->status))
        return;
    queue_.removeAll(run_id);
    if (!is_terminal(status))
        status = RunStatus::Failed;

    const qint64 now = trs_now_ms();
    {
        TaskRun& run = *it;
        account_active_time(run, status, now);
        run.status = status;
        run.finished_at_ms = now;
        run.updated_at_ms = now;
        run.detached = false;
        run.recovering = false;
        run.activity.clear();
        run.waiting_payload = {};
        if (!final_text.isEmpty())
            run.final_text = final_text;
        if (!error.isEmpty())
            run.error = error.left(kTrsErrorCapChars);
    }
    // Steps still marked running end with the run.
    for (RunStep& s : steps_[run_id]) {
        if (s.status != StepStatus::Running)
            continue;
        s.status = status == RunStatus::Completed ? StepStatus::Done : StepStatus::Failed;
        s.finished_at_ms = now;
        persist_step(s);
    }
    persist_run(run_id);
    budget_stop_.remove(run_id);
    dropped_events_.remove(run_id);
    const TaskRun snapshot = *it; // events and signals below may re-enter and mutate runs_

    if (!snapshot.final_text.isEmpty() && !final_text.isEmpty()) {
        RunEvent fin;
        fin.kind = RunEventKind::Final;
        fin.title = trs_tr("Result");
        fin.body = snapshot.final_text;
        record_event(run_id, fin);
    }
    QStringList parts;
    if (snapshot.started_at_ms > 0)
        parts << trs_duration(now - snapshot.started_at_ms);
    if (snapshot.credits_spent > 0)
        parts << trs_tr("%1 credits").arg(QString::number(snapshot.credits_spent, 'g', 10));
    if (snapshot.tokens_used > 0)
        parts << trs_tr("%1 tokens").arg(snapshot.tokens_used);
    RunEvent outcome;
    outcome.kind = RunEventKind::Status;
    outcome.is_error = status == RunStatus::Failed || status == RunStatus::BudgetStopped;
    outcome.title = run_status_label(status) +
                    (parts.isEmpty() ? QString() : QStringLiteral(" · ") + parts.join(QStringLiteral(" · ")));
    outcome.body = status == RunStatus::Completed ? QString() : snapshot.error;
    outcome.data["status"] = run_status_id(status);
    record_event(run_id, outcome);
    flush_events();
    // Ids, status and numbers only: the error text may quote the task.
    LOG_INFO(kTaskRunSvcTag, QStringLiteral("Run finished run=%1 executor=%2 status=%3 ms=%4 credits=%5 tokens=%6 "
                                            "error_chars=%7")
                                 .arg(run_id.left(8), snapshot.executor_id(), run_status_id(status))
                                 .arg(snapshot.started_at_ms > 0 ? now - snapshot.started_at_ms : 0)
                                 .arg(snapshot.credits_spent)
                                 .arg(snapshot.tokens_used)
                                 .arg(status == RunStatus::Completed ? 0 : snapshot.error.size()));

    emit run_updated(run_id);
    emit run_finished(run_id);
    live_text_.remove(run_id);
    notify(snapshot);
    touch_event_cache(run_id);
    report_attention();
    schedule_pump();
    if (active_count() == 0)
        budget_timer_->stop();
}

bool TaskRunService::set_status(const QString& run_id, RunStatus status, const QString& activity) {
    auto it = runs_.find(run_id);
    if (it == runs_.end())
        return false;
    if (!can_transition(it->status, status)) {
        LOG_WARN(kTaskRunSvcTag, QStringLiteral("Run %1: %2 -> %3 is not a valid transition — ignored")
                                     .arg(run_id.left(8), run_status_id(it->status), run_status_id(status)));
        return false;
    }
    const qint64 now = trs_now_ms();
    // The working clock: it runs only in Planning / Running (a pause, a question
    // or the queue stop it), so elapsed time and max_minutes leave waits out.
    account_active_time(*it, status, now);
    it->status = status;
    it->updated_at_ms = now;
    it->activity = activity;
    persist_run(run_id);
    emit run_updated(run_id);
    report_attention();
    return true;
}

// ── Budgets ──────────────────────────────────────────────────────────────────

void TaskRunService::enforce_budget(const QString& run_id) {
    const auto it = runs_.constFind(run_id);
    if (it == runs_.cend() || !holds_slot(it->status) || budget_stop_.contains(run_id))
        return;
    const BudgetVerdict v = check_budget(*it, steps_started(run_id), trs_now_ms());
    if (!v.exceeded)
        return;
    IRunExecutor* ex = executor_of(*it);
    budget_stop_.insert(run_id, v.reason);
    if (auto run = runs_.find(run_id); run != runs_.end())
        run->activity = trs_tr("Stopping: budget reached");
    RunEvent ev;
    ev.kind = RunEventKind::Status;
    ev.is_error = true;
    ev.title = trs_tr("Budget reached — stopping");
    ev.body = v.reason;
    record_event(run_id, ev);
    emit run_updated(run_id);
    LOG_INFO(kTaskRunSvcTag, QStringLiteral("Run %1: %2").arg(run_id.left(8), v.reason));
    if (ex)
        ex->cancel(run_id);
    else
        finish(run_id, RunStatus::BudgetStopped, {}, v.reason);
}

void TaskRunService::on_budget_tick() {
    bool any = false;
    const QStringList ids = order_;
    for (const QString& id : ids) {
        const auto it = runs_.constFind(id);
        if (it == runs_.cend() || !holds_slot(it->status))
            continue;
        any = true;
        if (it->budget.max_minutes > 0)
            enforce_budget(id);
    }
    if (!any && queue_.isEmpty())
        budget_timer_->stop();
}

// ── Notifications ────────────────────────────────────────────────────────────

void TaskRunService::notify(const TaskRun& run) {
    if (shutting_down_)
        return;
    QString what;
    auto severity = ui::ToastService::Severity::Info;
    switch (run.status) {
        case RunStatus::Completed:
            what = trs_tr("finished");
            severity = ui::ToastService::Severity::Success;
            break;
        case RunStatus::Failed:
            what = trs_tr("failed");
            severity = ui::ToastService::Severity::Error;
            break;
        case RunStatus::WaitingInput:
            what = trs_tr("needs your answer");
            severity = ui::ToastService::Severity::Warning;
            break;
        case RunStatus::WaitingApproval:
            what = trs_tr("needs your approval");
            severity = ui::ToastService::Severity::Warning;
            break;
        case RunStatus::BudgetStopped:
            what = trs_tr("stopped at its budget");
            severity = ui::ToastService::Severity::Warning;
            break;
        default:
            return; // cancelled / interrupted / in progress: the user did it or sees it
    }
    const bool app_active = QGuiApplication::applicationState() == Qt::ApplicationActive;
    // A scheduled run that needs the user ("needs you") is always announced:
    // nobody started it, so nobody is watching it even with Chat Mode open.
    const bool unattended_attention = run.is_unattended() && needs_attention(run.status);
    if (view_visible_ > 0 && app_active && !unattended_attention)
        return; // the user is looking at Chat Mode
    const QString text = unattended_attention
                             ? trs_tr("Scheduled run %1 — it waits for you in Chat Mode: %2").arg(what, run.title)
                             : trs_tr("Agent run %1: %2").arg(what, run.title);
    if (!app_active)
        ui::DesktopNotifier::instance().notify(trs_tr("Fincept agent"), text, static_cast<int>(severity));
    else
        ui::ToastService::instance().post(severity, text, QStringLiteral("Chat Mode"));
}

void TaskRunService::report_attention() {
    const int active = active_count();
    const int attention = attention_count();
    if (active == last_active_reported_ && attention == last_attention_reported_)
        return;
    last_active_reported_ = active;
    last_attention_reported_ = attention;
    emit attention_changed(active, attention);
}

// ── Persistence ──────────────────────────────────────────────────────────────

void TaskRunService::submit_write(std::function<void()> job) {
    if (!job)
        return;
    writer_->start(std::move(job));
}

void TaskRunService::persist_run(const QString& run_id) {
    const auto it = runs_.constFind(run_id);
    if (it == runs_.cend())
        return;
    const TaskRun copy = it.value();
    submit_write([copy]() {
        auto r = TaskRunStore::instance().upsert_run(copy);
        if (r.is_err())
            LOG_WARN(kTaskRunSvcTag, QStringLiteral("upsert_run failed: %1").arg(QString::fromStdString(r.error())));
    });
}

void TaskRunService::persist_step(const RunStep& step) {
    const RunStep copy = step;
    submit_write([copy]() {
        auto r = TaskRunStore::instance().upsert_step(copy);
        if (r.is_err())
            LOG_WARN(kTaskRunSvcTag, QStringLiteral("upsert_step failed: %1").arg(QString::fromStdString(r.error())));
    });
}

void TaskRunService::persist_steps(const QString& run_id) {
    const QVector<RunStep> copy = steps_.value(run_id);
    submit_write([run_id, copy]() {
        auto r = TaskRunStore::instance().replace_steps(run_id, copy);
        if (r.is_err())
            LOG_WARN(kTaskRunSvcTag, QStringLiteral("replace_steps failed: %1").arg(QString::fromStdString(r.error())));
    });
}

void TaskRunService::flush_events() {
    flush_timer_->stop();
    if (pending_events_.isEmpty())
        return;
    QVector<RunEventRow> batch;
    batch.swap(pending_events_);
    submit_write([batch]() {
        auto r = TaskRunStore::instance().insert_events(batch);
        if (r.is_err())
            LOG_WARN(kTaskRunSvcTag, QStringLiteral("insert_events failed (%1 rows): %2")
                                         .arg(batch.size())
                                         .arg(QString::fromStdString(r.error())));
    });
}

} // namespace fincept::agent_tasks
