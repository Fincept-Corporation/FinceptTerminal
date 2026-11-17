// LocalAgenticExecutor.cpp — a run on the local agentic engine (WP4): the
// AGENTIC_EVENT lifecycle of one AgentService task mapped onto the run model.
//
// Qt Core only: AgentService is reached through ILocalAgentEngine (LocalAgentEngine.cpp
// in the app, a fake in tests/tst_agent_task_executors). All state is per run id,
// on the GUI thread. finished() is emitted once per run (Run::ended guards it);
// pause parks the run with waiting(Paused) and forgets it — resume_run() starts it
// again and the task continues from its last Python checkpoint.

#include "services/agent_tasks/LocalAgenticExecutor.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>

namespace fincept::agent_tasks {

namespace local_agentic_executor_detail {

constexpr int kLaeSummaryChars = 12000; ///< a step's result as kept (the store keeps up to 20 000)
constexpr int kLaePartialChars = 60000; ///< the finished steps' results handed back as partial results

RunEvent lae_event(RunEventKind kind, const QString& title, const QString& body = {}, const QJsonObject& data = {}) {
    RunEvent e;
    e.kind = kind;
    e.title = title;
    e.body = body;
    e.data = data;
    return e;
}

RunEvent lae_verbose(RunEventKind kind, const QString& title, const QString& body = {}, QJsonObject data = {}) {
    data.insert(QStringLiteral("verbose"), true);
    return lae_event(kind, title, body, data);
}

QString lae_clip(const QString& text, int max_chars) {
    const QString t = text.trimmed();
    return t.size() > max_chars ? t.left(max_chars - 1) + QChar(0x2026) : t;
}

/// The engine config of a run: its budget for the Python BudgetGuard, and never
/// the capability to run destructive tools (orders, deletes) — local runs started
/// here, attended or not, stay refused at the bridge.
QJsonObject lae_config(const TaskRun& run) {
    QJsonObject cfg;
    QJsonObject budget;
    if (run.budget.max_steps > 0)
        budget["max_steps"] = run.budget.max_steps;
    if (run.budget.max_minutes > 0)
        budget["max_wall_s"] = run.budget.max_minutes * 60;
    if (!budget.isEmpty())
        cfg["budget"] = budget;
    cfg["allow_destructive_tools"] = false;
    return cfg;
}

} // namespace local_agentic_executor_detail

using namespace local_agentic_executor_detail;

// ── Per-run state and the event mapping ──────────────────────────────────────

struct LocalAgenticExecutor::Impl final : ILocalAgentListener {
    enum class Phase { Starting, Running, Waiting, Failing };

    struct Run {
        QString id;
        QString task_id;        ///< the Python task (TaskRun::remote_task_id)
        QJsonObject config;
        QVector<RunStep> steps; ///< the plan with each step's status and result
        Phase phase = Phase::Starting;
        bool bound = false;     ///< remote_task_id reported to the run
        bool pause_requested = false;
        bool pause_sent = false;
        bool cancel_requested = false;
        bool cancel_sent = false;
        int resume_from = 0;     ///< task_resumed.from_step of the current process
        int last_ended_step = -1;
        qint64 tokens_seen = 0;  ///< budget.tokens_used already reported for the current process
        bool ended = false;      ///< finished() / waiting(Paused) reported, or handed back to the service
    };

    LocalAgenticExecutor* q;
    std::unique_ptr<ILocalAgentEngine> engine;
    std::map<QString, std::unique_ptr<Run>> runs;
    QHash<QString, QString> by_task;    ///< Python task id -> run id
    QHash<QString, QString> by_request; ///< AgentService request id -> run id
    QString sync_run; ///< the run whose engine call is on the stack (events carrying no ids)

    Impl(LocalAgenticExecutor* owner, std::unique_ptr<ILocalAgentEngine> e) : q(owner), engine(std::move(e)) {
        if (engine)
            engine->set_listener(this);
    }

    ~Impl() {
        if (engine)
            engine->set_listener(nullptr);
    }

    // ── Bookkeeping ──────────────────────────────────────────────────────────

    Run* find(const QString& id) const {
        const auto it = runs.find(id);
        return it == runs.end() ? nullptr : it->second.get();
    }

    Run* make_run(const TaskRun& run, QVector<RunStep> steps) {
        drop(run.id);
        std::sort(steps.begin(), steps.end(), [](const RunStep& a, const RunStep& b) { return a.idx < b.idx; });
        for (int i = 0; i < steps.size(); ++i) {
            steps[i].run_id = run.id;
            steps[i].idx = i;
        }
        auto r = std::make_unique<Run>();
        r->id = run.id;
        r->steps = steps;
        r->config = lae_config(run);
        if (!run.remote_task_id.isEmpty()) {
            r->task_id = run.remote_task_id;
            r->bound = true;
            by_task.insert(r->task_id, run.id);
        }
        Run* raw = r.get();
        runs.emplace(run.id, std::move(r));
        return raw;
    }

    void drop(const QString& id) {
        const auto it = runs.find(id);
        if (it == runs.end())
            return;
        for (auto t = by_task.begin(); t != by_task.end();)
            t = t.value() == id ? by_task.erase(t) : std::next(t);
        for (auto t = by_request.begin(); t != by_request.end();)
            t = t.value() == id ? by_request.erase(t) : std::next(t);
        runs.erase(it);
    }

    void drop_later(const QString& id) {
        QTimer::singleShot(0, q, [this, id]() {
            Run* r = find(id);
            if (r && r->ended)
                drop(id);
        });
    }

    void log(const QString& id, const QString& what) {
        if (engine)
            engine->log_info(QStringLiteral("Local run %1: %2").arg(id.left(8), what));
    }

    static QString partial_results(const Run* r) {
        QString out;
        for (const RunStep& s : r->steps) {
            if (s.status != StepStatus::Done || s.summary.trimmed().isEmpty())
                continue;
            const QString block = QStringLiteral("## ") + tr("Step %1 — %2").arg(s.idx + 1).arg(s.title) +
                                  QStringLiteral("\n\n") + s.summary.trimmed() + QStringLiteral("\n\n");
            if (out.size() + block.size() > kLaePartialChars) {
                out += tr("(More results are in the run's steps.)");
                break;
            }
            out += block;
        }
        return out.trimmed();
    }

    /// Run an engine call that starts a Python process; route its events to `r`.
    void call_engine(Run* r, const std::function<QString()>& fn) {
        const QString id = r->id;
        const QString previous = sync_run;
        sync_run = id;
        const QString request_id = fn();
        sync_run = previous;
        if (!request_id.isEmpty() && find(id))
            by_request.insert(request_id, id);
    }

    void end(Run* r, RunStatus status, const QString& final_text, const QString& error) {
        if (r->ended)
            return;
        r->ended = true;
        const QString id = r->id;
        log(id, QStringLiteral("ended: %1").arg(run_status_id(status)));
        emit q->finished(id, status, final_text, error);
        drop_later(id);
    }

    void fail_not_ready(const QString& id, const LocalAgentReadiness& ready) {
        Run* r = find(id);
        if (!r || r->ended)
            return;
        RunEvent e = lae_event(RunEventKind::Error, tr("The local agent cannot run this task"), ready.message);
        e.is_error = true;
        e.data["code"] = ready.code;
        emit q->event(id, e);
        if ((r = find(id)) && !r->ended)
            end(r, RunStatus::Failed, partial_results(r), ready.message);
    }

    void report_tokens(Run* r, const QJsonObject& budget) {
        if (!budget.contains(QStringLiteral("tokens_used")))
            return;
        const qint64 used = static_cast<qint64>(budget.value(QStringLiteral("tokens_used")).toDouble());
        const qint64 delta = used - r->tokens_seen;
        if (delta <= 0)
            return;
        r->tokens_seen = used;
        emit q->usage(r->id, 0, static_cast<int>(delta)); // the user's own LLM: tokens, never credits
    }

    void send_pending_controls(Run* r) {
        if (!engine || r->task_id.isEmpty())
            return;
        if (r->cancel_requested && !r->cancel_sent) {
            r->cancel_sent = true;
            engine->cancel_task(r->task_id);
        } else if (r->pause_requested && !r->pause_sent && !r->cancel_requested) {
            r->pause_sent = true;
            engine->pause_task(r->task_id);
        }
    }

    // ── Steps ────────────────────────────────────────────────────────────────

    /// The run's steps after a (re)plan: the first `keep` known steps as they are,
    /// then the plan's steps from `from` on.
    static QVector<RunStep> merged(const Run* r, const QVector<RunStep>& planned, int keep, int from) {
        QVector<RunStep> out;
        for (int i = 0; i < keep; ++i) {
            RunStep s;
            if (i < r->steps.size())
                s = r->steps.at(i);
            else if (i < planned.size())
                s = planned.at(i);
            else
                break;
            // Python holds a checkpoint for every step before `keep`: a step the
            // terminal never saw finish (it quit first) is done all the same.
            if (s.status != StepStatus::Done && s.status != StepStatus::Failed && s.status != StepStatus::Skipped)
                s.status = StepStatus::Done;
            out.append(s);
        }
        for (int j = std::max(0, from); j < planned.size(); ++j)
            out.append(planned.at(j));
        for (int i = 0; i < out.size(); ++i) {
            out[i].run_id = r->id;
            out[i].idx = i;
        }
        return out;
    }

    void publish_plan(Run* r, const QVector<RunStep>& steps, const QString& title) {
        const QString id = r->id;
        r->steps = steps;
        emit q->plan_ready(id, steps);
        if (!(r = find(id)) || r->ended)
            return;
        QStringList lines;
        for (const RunStep& s : steps)
            lines << QStringLiteral("%1. ").arg(s.idx + 1) + s.title;
        QJsonObject d;
        d["steps"] = static_cast<int>(steps.size());
        d["source"] = QStringLiteral("local");
        emit q->event(id, lae_event(RunEventKind::Plan, title, lines.join(QLatin1Char('\n')), d));
    }

    /// Make sure step `idx` exists (a step beyond the known plan gets a row).
    void ensure_step(Run* r, int idx, const QJsonObject& step) {
        if (idx < r->steps.size())
            return;
        QVector<RunStep> steps = r->steps;
        while (steps.size() <= idx) {
            RunStep s;
            s.run_id = r->id;
            s.idx = static_cast<int>(steps.size());
            s.title = tr("Step %1").arg(s.idx + 1);
            if (s.idx == idx) {
                const QVector<RunStep> one = local_agent_plan_steps(QJsonObject{{QStringLiteral("steps"), QJsonArray{step}}});
                if (!one.isEmpty()) {
                    s.title = one.first().title;
                    s.instruction = one.first().instruction;
                }
            }
            steps.append(s);
        }
        r->steps = steps;
        emit q->plan_ready(r->id, steps);
    }

    // ── ILocalAgentListener ──────────────────────────────────────────────────

    void local_agent_event(const QString& task_id_in, const QJsonObject& ev) override {
        const QString task_id = task_id_in.isEmpty() ? ev.value(QStringLiteral("task_id")).toString() : task_id_in;
        const QString request_id = ev.value(QStringLiteral("request_id")).toString();
        QString id;
        if (!task_id.isEmpty())
            id = by_task.value(task_id);
        if (id.isEmpty() && !request_id.isEmpty())
            id = by_request.value(request_id);
        if (id.isEmpty() && task_id.isEmpty() && request_id.isEmpty())
            id = sync_run; // a spawn that failed inside the engine call
        Run* r = find(id);
        if (!r || r->ended)
            return; // another panel's task, or a run already handed back
        if (!task_id.isEmpty() && r->task_id.isEmpty()) {
            r->task_id = task_id;
            by_task.insert(task_id, id);
            if (!r->bound) {
                r->bound = true;
                QJsonObject d;
                d["remote_task_id"] = task_id;
                d["verbose"] = true;
                emit q->event(id, lae_event(RunEventKind::Status, tr("Local task %1").arg(task_id.left(8)), {}, d));
                if (!(r = find(id)) || r->ended)
                    return;
            }
            send_pending_controls(r);
            if (!(r = find(id)) || r->ended)
                return;
        }
        on_event(r, ev.value(QStringLiteral("kind")).toString(), ev);
    }

    void on_event(Run* r, const QString& kind, const QJsonObject& ev) {
        const QString id = r->id;
        if (r->phase == Phase::Starting)
            r->phase = Phase::Running;

        if (kind == QLatin1String("task_started")) {
            r->tokens_seen = 0;
            r->resume_from = 0;
            emit q->event(id, lae_event(RunEventKind::Status, tr("The local agent is planning the task")));
            return;
        }
        if (kind == QLatin1String("task_resumed")) {
            r->tokens_seen = 0; // a new process: its budget counts from zero
            r->resume_from = std::max(0, ev.value(QStringLiteral("from_step")).toInt());
            r->last_ended_step = std::max(r->last_ended_step, r->resume_from - 1); // checkpointed before
            const bool with_answer = ev.value(QStringLiteral("with_user_input")).toBool();
            QJsonObject d;
            d["from_step"] = r->resume_from;
            emit q->event(id, lae_event(RunEventKind::Status,
                                        with_answer ? tr("The local agent continues with your answer")
                                                    : tr("The local agent continues from step %1").arg(r->resume_from + 1),
                                        {}, d));
            return;
        }
        if (kind == QLatin1String("plan_ready")) {
            const QVector<RunStep> planned = local_agent_plan_steps(ev.value(QStringLiteral("plan")).toObject());
            // A resumed task continues its saved plan at `from_step` (Python's step
            // indexes are global): what ran before keeps its known result.
            const int from = r->resume_from;
            const QVector<RunStep> steps = merged(r, planned, from, from);
            log(id, QStringLiteral("plan: %1 step(s), from %2").arg(steps.size()).arg(from));
            publish_plan(r, steps, steps.size() == 1 ? tr("Plan: 1 step") : tr("Plan: %1 steps").arg(steps.size()));
            return;
        }
        if (kind == QLatin1String("replanned")) {
            const QVector<RunStep> planned = local_agent_plan_steps(ev.value(QStringLiteral("plan")).toObject());
            const int keep = r->last_ended_step + 1; // the new plan continues after the step that ended
            const QVector<RunStep> steps = merged(r, planned, keep, 0);
            const QString reason = ev.value(QStringLiteral("reason")).toString().trimmed();
            log(id, QStringLiteral("replanned: %1 step(s)").arg(steps.size()));
            publish_plan(r, steps, tr("Replanned: %1 steps").arg(steps.size()));
            if (!(r = find(id)) || r->ended || reason.isEmpty())
                return;
            emit q->event(id, lae_event(RunEventKind::Note, tr("Why the plan changed"), reason));
            return;
        }
        if (kind == QLatin1String("step_start")) {
            const int idx = ev.value(QStringLiteral("step_index")).toInt(-1);
            report_tokens(r, ev.value(QStringLiteral("budget")).toObject());
            if (idx < 0 || !(r = find(id)) || r->ended)
                return;
            ensure_step(r, idx, ev.value(QStringLiteral("step")).toObject());
            if (!(r = find(id)) || r->ended || idx >= r->steps.size())
                return;
            r->steps[idx].status = StepStatus::Running;
            emit q->step_changed(id, idx, StepStatus::Running, {});
            return;
        }
        if (kind == QLatin1String("step_end")) {
            const int idx = ev.value(QStringLiteral("step_index")).toInt(-1);
            const QJsonObject budget = ev.value(QStringLiteral("budget")).toObject();
            report_tokens(r, budget);
            if (idx < 0 || !(r = find(id)) || r->ended)
                return;
            ensure_step(r, idx, {});
            if (!(r = find(id)) || r->ended || idx >= r->steps.size())
                return;
            const LocalStepResult res = local_agent_step_result(ev.value(QStringLiteral("result")).toObject());
            const QString summary = lae_clip(res.text, kLaeSummaryChars);
            const StepStatus status = res.failed ? StepStatus::Failed : StepStatus::Done;
            r->steps[idx].status = status;
            r->steps[idx].summary = summary;
            r->last_ended_step = std::max(r->last_ended_step, idx);
            emit q->step_changed(id, idx, status, summary);
            if (!(r = find(id)) || r->ended)
                return;
            // One local step is one agent turn; tokens so far, never credits.
            QJsonObject d;
            d["turn_end"] = true;
            d["credits_charged"] = 0;
            d["tokens_total"] = budget.value(QStringLiteral("tokens_used")).toDouble();
            emit q->event(id, lae_verbose(RunEventKind::Usage,
                                          tr("Step %1 finished · %2 tokens so far")
                                              .arg(idx + 1)
                                              .arg(static_cast<qint64>(budget.value(QStringLiteral("tokens_used")).toDouble())),
                                          {}, d));
            return;
        }
        if (kind == QLatin1String("reflection")) {
            const QJsonObject dec = ev.value(QStringLiteral("decision")).toObject();
            const QString decision = dec.value(QStringLiteral("decision")).toString();
            const QString reason = dec.value(QStringLiteral("reason")).toString().trimmed();
            QJsonObject d = dec;
            const bool routine = decision.isEmpty() || decision == QLatin1String("continue");
            if (routine)
                d["verbose"] = true;
            emit q->event(id, lae_event(RunEventKind::Note,
                                        tr("Self-check: %1").arg(decision.isEmpty() ? tr("continue") : decision), reason,
                                        d));
            return;
        }
        if (kind == QLatin1String("question")) {
            const QString question = ev.value(QStringLiteral("question")).toString().trimmed();
            const QString reason = ev.value(QStringLiteral("reason")).toString().trimmed();
            QJsonObject d;
            d["resumable"] = true;
            d["local"] = true;
            if (!reason.isEmpty())
                d["reason"] = reason;
            emit q->event(id, lae_event(RunEventKind::Question, tr("The agent asks"),
                                        question.isEmpty() ? tr("The agent needs more information to continue.") : question,
                                        d));
            if (!(r = find(id)) || r->ended)
                return;
            if (r->cancel_requested) {
                // Cancelled while the step ran: the task now waits for input, which
                // Python cancels outright.
                if (engine && !r->task_id.isEmpty())
                    engine->cancel_task(r->task_id);
                end(r, RunStatus::Cancelled, partial_results(r), {});
                return;
            }
            r->phase = Phase::Waiting;
            r->pause_requested = r->pause_sent = false;
            QJsonObject payload;
            payload["question"] = question;
            payload["resumable"] = true;
            payload["local"] = true;
            if (!reason.isEmpty())
                payload["reason"] = reason;
            if (ev.contains(QStringLiteral("step_index")))
                payload["step"] = ev.value(QStringLiteral("step_index")).toInt();
            log(id, QStringLiteral("waits for the user's answer"));
            emit q->waiting(id, RunStatus::WaitingInput, payload);
            return;
        }
        if (kind == QLatin1String("paused")) {
            if (r->cancel_requested) {
                if (engine && !r->task_id.isEmpty())
                    engine->cancel_task(r->task_id); // paused: Python cancels it outright
                end(r, RunStatus::Cancelled, partial_results(r), {});
                return;
            }
            const int at = ev.value(QStringLiteral("at_step")).toInt();
            r->ended = true; // parked: resume_run() starts the run again (resume_task)
            emit q->event(id, lae_event(RunEventKind::Status, at <= 0 ? tr("Paused before the first step")
                                                                      : tr("Paused after step %1").arg(at)));
            log(id, QStringLiteral("paused at step %1").arg(at));
            emit q->waiting(id, RunStatus::Paused, {});
            drop_later(id);
            return;
        }
        if (kind == QLatin1String("cancelled")) {
            end(r, RunStatus::Cancelled, partial_results(r),
                r->cancel_requested ? QString()
                                    : tr("The local task was cancelled outside Chat Mode (in Agent Studio)."));
            return;
        }
        if (kind == QLatin1String("budget_stop")) {
            const QJsonObject budget = ev.value(QStringLiteral("budget")).toObject();
            report_tokens(r, budget);
            if (!(r = find(id)) || r->ended)
                return;
            const QString reason = local_agent_budget_text(ev.value(QStringLiteral("breach")).toString(), budget);
            RunEvent e = lae_event(RunEventKind::Status, tr("Budget reached — the local agent stopped"), reason);
            e.is_error = true;
            emit q->event(id, e);
            if ((r = find(id)) && !r->ended)
                end(r, RunStatus::BudgetStopped, partial_results(r), reason);
            return;
        }
        if (kind == QLatin1String("done")) {
            const QString final_text = ev.value(QStringLiteral("final")).toString();
            end(r, RunStatus::Completed, final_text.trimmed().isEmpty() ? partial_results(r) : final_text, {});
            return;
        }
        if (kind == QLatin1String("error")) {
            const QString raw = ev.value(QStringLiteral("error")).toString();
            // "explained": the engine already turned the error into what the user can do.
            const bool explained = ev.value(QStringLiteral("explained")).toBool();
            const QString text = explained ? raw : local_agent_error_text(raw);
            RunEvent e = lae_event(RunEventKind::Error, tr("The local agent failed"), text);
            e.is_error = true;
            e.data["code"] = explained ? QStringLiteral("python_library_missing") : local_agent_error_code(raw);
            emit q->event(id, e);
            if ((r = find(id)) && !r->ended)
                end(r, RunStatus::Failed, partial_results(r), text);
            return;
        }
        if (kind == QLatin1String("control_ack")) {
            const QString signal = ev.value(QStringLiteral("signal")).toString();
            QString title;
            if (signal == QLatin1String("pause_requested"))
                title = tr("Pause requested — the local agent stops after its current step");
            else if (signal == QLatin1String("cancel_requested"))
                title = tr("Cancel requested — the local agent stops after its current step");
            else
                title = tr("The local agent acknowledged: %1").arg(signal);
            emit q->event(id, lae_verbose(RunEventKind::Note, title));
            return;
        }
        if (kind == QLatin1String("skill_saved") || kind == QLatin1String("skill_skipped") ||
            kind == QLatin1String("reusability_score")) {
            QJsonObject d = ev;
            d.remove(QStringLiteral("request_id"));
            emit q->event(id, lae_verbose(RunEventKind::Note, tr("Skill library: %1").arg(kind), {}, d));
            return;
        }
        QJsonObject d = ev;
        d.remove(QStringLiteral("request_id"));
        d["event"] = kind;
        emit q->event(id, lae_verbose(RunEventKind::Note, tr("Local agent event: %1").arg(kind), {}, d));
    }
};

// ── LocalAgenticExecutor ─────────────────────────────────────────────────────
// The default constructor (AgentService engine) is in LocalAgentEngine.cpp.

LocalAgenticExecutor::LocalAgenticExecutor(std::unique_ptr<ILocalAgentEngine> engine, QObject* parent)
    : IRunExecutor(parent), d_(std::make_unique<Impl>(this, std::move(engine))) {
    // Implemented: the Chat Mode UI offers the engine (rv::kExecutorAvailableProperty
    // in screens/chat_mode/RunViewWidgets.h). A machine without Python / an LLM is
    // told so when a run starts (fail fast with a code), not by hiding the engine.
    setProperty("available", true);
}

LocalAgenticExecutor::~LocalAgenticExecutor() = default;

QString LocalAgenticExecutor::engine_id() const {
    return QString::fromLatin1(kExecutorLocal);
}

void LocalAgenticExecutor::start(const TaskRun& run, const QVector<RunStep>& steps) {
    const QString id = run.id;
    Impl::Run* r = d_->make_run(run, steps);
    const LocalAgentReadiness ready =
        d_->engine ? d_->engine->readiness()
                   : LocalAgentReadiness{false, QStringLiteral("local_engine_unavailable"),
                                         tr("The local agent is not available in this build."), {}};
    if (!ready.ready) {
        // Fail fast, but after start() returned (the caller is still launching the run).
        r->phase = Impl::Phase::Failing;
        d_->log(id, QStringLiteral("not ready (%1)").arg(ready.code));
        QTimer::singleShot(0, this, [this, id, ready]() { d_->fail_not_ready(id, ready); });
        return;
    }
    QJsonObject d;
    if (!ready.llm_label.isEmpty())
        d["llm"] = ready.llm_label;
    emit event(id, lae_event(RunEventKind::Status, tr("Starting the local agent"),
                             ready.llm_label.isEmpty()
                                 ? tr("It runs on this computer with your own LLM; no Fincept credits are used.")
                                 : tr("It runs on this computer with your own LLM (%1); no Fincept credits are used.")
                                       .arg(ready.llm_label),
                             d));
    if (!(r = d_->find(id)) || r->ended)
        return;
    const QJsonObject config = r->config;
    if (!r->task_id.isEmpty()) {
        // Paused or interrupted: Python continues from its last checkpoint.
        const QString task_id = r->task_id;
        d_->log(id, QStringLiteral("resume task %1").arg(task_id.left(8)));
        emit event(id, lae_event(RunEventKind::Note, tr("Resuming the local task from its last finished step")));
        if (!(r = d_->find(id)) || r->ended)
            return;
        d_->call_engine(r, [this, task_id, config]() { return d_->engine->resume_task(task_id, config); });
        return;
    }
    if (std::any_of(r->steps.cbegin(), r->steps.cend(), [](const RunStep& s) { return s.status == StepStatus::Done; })) {
        // A retry child: its copied steps belong to another local task, which a new
        // task cannot continue — the agent plans and runs the whole task again.
        emit event(id, lae_event(RunEventKind::Note,
                                 tr("The local agent cannot continue another task's steps: this run plans and runs "
                                    "the whole task again.")));
        if (!(r = d_->find(id)) || r->ended)
            return;
    }
    const QString prompt = run.prompt;
    d_->log(id, QStringLiteral("start task"));
    d_->call_engine(r, [this, prompt, config]() { return d_->engine->start_task(prompt, config); });
}

void LocalAgenticExecutor::answer(const QString& run_id, const QString& text) {
    if (!d_->engine) {
        emit finished(run_id, RunStatus::Failed, {}, tr("The local agent is not available in this build."));
        return;
    }
    Impl::Run* r = d_->find(run_id);
    if (r && r->ended) {
        d_->drop(run_id);
        r = nullptr;
    }
    if (!r) {
        // Waiting since before a restart: rebuild the run from its record.
        const TaskRun* run = d_->engine->find_run(run_id);
        if (!run || run->remote_task_id.isEmpty()) {
            emit finished(run_id, RunStatus::Failed, {},
                          tr("This local task can no longer be found, so your answer cannot reach it."));
            return;
        }
        r = d_->make_run(*run, d_->engine->run_steps(run_id));
        r->phase = Impl::Phase::Waiting;
    }
    if (r->phase != Impl::Phase::Waiting)
        return; // not waiting for an answer (TaskRunService only answers WaitingInput runs)
    const LocalAgentReadiness ready = d_->engine->readiness();
    if (!ready.ready) {
        d_->fail_not_ready(run_id, ready);
        return;
    }
    r->phase = Impl::Phase::Running;
    const QString task_id = r->task_id;
    const QJsonObject config = r->config;
    d_->log(run_id, QStringLiteral("answer sent"));
    d_->call_engine(r, [this, task_id, text, config]() { return d_->engine->reply_to_question(task_id, text, config); });
}

void LocalAgenticExecutor::request_pause(const QString& run_id) {
    Impl::Run* r = d_->find(run_id);
    if (!r || !can_pause(run_id))
        return;
    r->pause_requested = true;
    d_->log(run_id, QStringLiteral("pause requested"));
    d_->send_pending_controls(r); // or once the task reports its id
}

bool LocalAgenticExecutor::can_pause(const QString& run_id) const {
    const Impl::Run* r = d_->find(run_id);
    return r && !r->ended && !r->cancel_requested &&
           (r->phase == Impl::Phase::Starting || r->phase == Impl::Phase::Running);
}

void LocalAgenticExecutor::cancel(const QString& run_id) {
    Impl::Run* r = d_->find(run_id);
    if (!r || r->ended) {
        if (r)
            return; // already handed back
        // Nothing here (paused, waiting since a restart, or never started): a paused
        // or waiting Python task is cancelled outright; a run the service still
        // counts as running must not hang.
        const TaskRun* run = d_->engine ? d_->engine->find_run(run_id) : nullptr;
        if (run && !run->remote_task_id.isEmpty())
            d_->engine->cancel_task(run->remote_task_id);
        if (run && holds_slot(run->status))
            emit finished(run_id, RunStatus::Cancelled, {}, {});
        return;
    }
    if (r->phase == Impl::Phase::Failing)
        return; // it ends Failed at once
    if (r->phase == Impl::Phase::Waiting) {
        // Nothing runs while the task waits for an answer: Python cancels it
        // outright; the service ends the run itself.
        if (!r->task_id.isEmpty())
            d_->engine->cancel_task(r->task_id);
        r->ended = true;
        d_->drop_later(run_id);
        return;
    }
    if (r->cancel_requested) {
        // Cancel again: stop waiting for the step the agent is on.
        d_->end(r, RunStatus::Cancelled, Impl::partial_results(r),
                tr("Stopped waiting for the local agent. Its current step may still finish in the background; its "
                   "result is not used."));
        return;
    }
    r->cancel_requested = true;
    r->pause_requested = false;
    d_->log(run_id, QStringLiteral("cancel requested"));
    emit event(run_id, lae_event(RunEventKind::Status, tr("Stopping after the current step…"),
                                 tr("The local agent finishes the step it is on, then stops. Press Cancel again to "
                                    "stop waiting.")));
    if ((r = d_->find(run_id)) && !r->ended)
        d_->send_pending_controls(r); // or once the task reports its id
}

} // namespace fincept::agent_tasks
