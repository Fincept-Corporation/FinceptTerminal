// HarnessRunExecutor.cpp — Chat Mode's "harness" engine (see the header).
//
// All run state lives on the GUI thread. Each call to the harness runs on a
// private worker pool (HarnessChatBackend::complete blocks for the whole run);
// its transcript events and its result are posted back to the GUI thread in
// order, and every IRunExecutor signal is emitted there. finished() is emitted
// once per run (Run::ended guards it).

#include "services/agent_tasks/HarnessRunExecutor.h"

#include "core/logging/Logger.h"
#include "services/harness/HarnessChatBackend.h"
#include "services/llm/ModelRouter.h"

#include <QCoreApplication>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
#include <limits>
#include <memory>

namespace fincept::agent_tasks {

namespace harness_run_executor_detail {

namespace hre_map = harness_run_mapping;

constexpr const char* kHreTag = "HarnessRun";
constexpr int kHreDefaultMinutes = 30;
constexpr int kHreMaxMinutes = 4 * 60;
constexpr int kHreFirstEventMs = 60 * 1000;
constexpr int kHrePoolThreads = 4; // the chat process pool (2) is the real limit; the rest queue there

QString hre_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::HarnessRunExecutor", text);
}

/// What the harness is told about a Chat Mode task.
QString hre_system_prompt() {
    return QStringLiteral(
        "You are running a long task inside Fincept Terminal, a professional financial terminal, for its user. "
        "Work through the terminal's MCP tools: ui_overview first (what is open), ui_open to open a screen "
        "(optionally on a symbol), ui_snapshot / ui_read to read panels and tables, ui_act to work controls, "
        "ui_wait until something loads, python to find and run the bundled analytics scripts.\n"
        "Rules:\n"
        "- Start by writing a short plan as a todo list and keep it up to date as you work.\n"
        "- Every figure you report must come from what the terminal shows or a script returns. Never invent "
        "numbers.\n"
        "- This run is PAPER ONLY: never place, change or cancel a real order or move money; such actions are "
        "refused. Never read or type secrets (API keys, passwords, PINs).\n"
        "- You have no file system or shell of your own; everything goes through the terminal's tools.\n"
        "- Finish with a concise report: the findings, the numbers and where they came from.");
}

/// Why no harness may run this task (from the route's skipped reasons).
QString hre_unavailable_reason(const ai_chat::RouteResult& route) {
    QStringList parts;
    for (const ai_chat::SkippedChoice& s : route.skipped) {
        if (!s.harness_id.isEmpty())
            parts << s.what + QStringLiteral(" — ") + s.reason;
    }
    QString text = hre_tr("No harness may run Chat Mode tasks.");
    if (!parts.isEmpty())
        text += QLatin1Char(' ') + parts.join(QStringLiteral("; ")) + QLatin1Char('.');
    text += QLatin1Char(' ') + hre_tr("Choose a harness for Chat Mode tasks, or allow the connected harness for them, "
                                      "in Settings › LLM Configuration › Terminal AI.");
    return text;
}

int hre_timeout_ms(const TaskRun& run) {
    const int minutes = run.budget.max_minutes > 0 ? qMin(run.budget.max_minutes, kHreMaxMinutes) : kHreDefaultMinutes;
    return minutes * 60 * 1000;
}

} // namespace harness_run_executor_detail

using namespace harness_run_executor_detail;

// ── Per-run state ────────────────────────────────────────────────────────────

struct HarnessRunExecutor::Impl {
    struct Run {
        QString id;
        TaskRun snapshot;          // as handed to start()
        ai_chat::ModelChoice choice;
        std::shared_ptr<std::atomic<bool>> cancel_flag = std::make_shared<std::atomic<bool>>(false);
        bool busy = false;         // a harness call is in flight
        bool ended = false;        // finished() was emitted
        int cancels = 0;
        int step = 0;              // the step text deltas belong to
        hre_map::DeltaTracker text;
        QStringList todo_titles;   // the plan as last reported
        QVector<StepStatus> todo_status;
        bool session_bound = false;
        QString session_id;        // this harness's session (resume)
        quint64 call = 0;          // the call whose events are current
    };

    explicit Impl(HarnessRunExecutor* owner) : q(owner), pool(new QThreadPool) {
        pool->setMaxThreadCount(kHrePoolThreads);
        pool->setObjectName(QStringLiteral("fincept-harness-runs"));
    }

    HarnessRunExecutor* q;
    QHash<QString, Run> runs;
    /// Private (never the global pool, which other subsystems block on). No
    /// parent: a call still running at exit is abandoned, never waited for without bound.
    QThreadPool* pool;
    quint64 next_call = 1;

    Run* find(const QString& id) {
        auto it = runs.find(id);
        return it == runs.end() ? nullptr : &it.value();
    }

    void update_available() {
        bool any = false;
        for (const ai_chat::RouterHarness& h : ai_chat::ModelRouter::instance().harness_states())
            any = any || h.eligibility.usable;
        q->setProperty("available", any);
    }

    void end(Run* r, RunStatus status, const QString& final_text, const QString& error) {
        if (!r || r->ended)
            return;
        r->ended = true;
        r->cancel_flag->store(true); // anything still running stops
        const QString id = r->id;
        LOG_INFO(kHreTag, QStringLiteral("Run %1 via %2: %3").arg(id.left(8), r->choice.harness_id, run_status_id(status)));
        runs.remove(id);
        emit q->finished(id, status, final_text, error);
    }

    void fail_unavailable(const QString& id, const QString& reason) {
        Run* r = find(id);
        if (!r || r->ended)
            return;
        RunEvent e = hre_map::make_event(RunEventKind::Error, hre_tr("No harness can run this task"), reason,
                                         QJsonObject{{QStringLiteral("code"), QStringLiteral("harness_unavailable")}});
        e.is_error = true;
        emit q->event(id, e);
        if ((r = find(id)))
            end(r, RunStatus::Failed, {}, reason);
    }

    /// One harness call for `r` (the task, or a continuation). GUI thread.
    void launch(Run* r, const QString& message, const QString& resume_session) {
        harness::HarnessChatRequest req;
        req.harness_id = r->choice.harness_id;
        req.model = r->choice.model;
        req.effort = r->choice.effort;
        req.system_prompt = hre_system_prompt();
        req.user_message = message;
        req.chat_session_id = r->id;
        req.feature = QString::fromLatin1(ai_chat::kFeatureChatModeHarness);
        req.tools = harness::ChatTools::All;
        req.paper_only = true; // owner: Chat Mode harness tasks never touch a real order
        req.persist_session = true;
        req.resume_session_id = resume_session;
        req.first_event_timeout_ms = kHreFirstEventMs;
        req.timeout_ms = hre_timeout_ms(r->snapshot);
        req.require_api_key_source = r->choice.require_api_key_source;

        r->busy = true;
        r->cancel_flag->store(false);
        r->call = next_call++;
        const QString id = r->id;
        const quint64 call = r->call;
        const auto flag = r->cancel_flag;
        QPointer<HarnessRunExecutor> self(q);
        pool->start([self, id, call, req, flag]() {
            harness::HarnessChatSink sink;
            sink.event = [self, id, call](const services::coding_agents::AgentEvent& ev) {
                QCoreApplication* app = QCoreApplication::instance();
                if (!app)
                    return;
                QMetaObject::invokeMethod(
                    app,
                    [self, id, call, ev]() {
                        if (self)
                            self->d_->on_event(id, call, ev);
                    },
                    Qt::QueuedConnection);
            };
            sink.cancelled = [flag]() { return flag->load(); };
            const harness::HarnessChatResult result = harness::HarnessChatBackend::complete(req, sink);
            QCoreApplication* app = QCoreApplication::instance();
            if (!app)
                return;
            QMetaObject::invokeMethod(
                app,
                [self, id, call, result]() {
                    if (self)
                        self->d_->on_done(id, call, result);
                },
                Qt::QueuedConnection);
        });
    }

    void bind_session(Run* r, const QString& session_id) {
        if (!r || session_id.isEmpty() || r->session_bound)
            return;
        r->session_bound = true;
        r->session_id = session_id;
        const QString remote = hre_map::remote_id(r->choice.harness_id, session_id);
        emit q->event(r->id, hre_map::make_event(RunEventKind::Status, hre_tr("Harness session started"), {},
                                                 QJsonObject{{QStringLiteral("remote_task_id"), remote},
                                                             {QStringLiteral("verbose"), true}}));
    }

    void apply_todos(Run* r, const QVector<hre_map::TodoItem>& todos) {
        if (!r || todos.isEmpty())
            return;
        const QString id = r->id;
        const QStringList titles = hre_map::todo_titles(todos);
        if (titles != r->todo_titles) {
            r->todo_titles = titles;
            r->todo_status = QVector<StepStatus>(todos.size(), StepStatus::Pending);
            emit q->plan_ready(id, hre_map::steps_from_todos(todos));
            if (!(r = find(id)))
                return;
        }
        for (int i = 0; i < todos.size() && i < r->todo_status.size(); ++i) {
            if (r->todo_status[i] == todos[i].status)
                continue;
            r->todo_status[i] = todos[i].status;
            if (todos[i].status == StepStatus::Running)
                r->step = i;
            emit q->step_changed(id, i, todos[i].status, {});
            if (!(r = find(id)))
                return;
        }
    }

    void on_event(const QString& id, quint64 call, const services::coding_agents::AgentEvent& ev) {
        Run* r = find(id);
        if (!r || r->ended || r->call != call)
            return;
        const hre_map::Mapped m = hre_map::map_event(ev, r->text);
        if (!m.session_id.isEmpty())
            bind_session(r, m.session_id);
        if (!(r = find(id)))
            return;
        if (!m.text_delta.isEmpty())
            emit q->text_delta(id, r->step, m.text_delta);
        for (const RunEvent& e : m.events) {
            if (!(r = find(id)))
                return;
            emit q->event(id, e);
        }
        if (m.todos && (r = find(id)))
            apply_todos(r, *m.todos);
    }

    void on_done(const QString& id, quint64 call, const harness::HarnessChatResult& res) {
        Run* r = find(id);
        if (!r || r->ended || r->call != call)
            return;
        r->busy = false;
        bind_session(r, res.session_id);
        if (!(r = find(id)))
            return;
        const qint64 tokens = qMax<qint64>(0, res.input_tokens) + qMax<qint64>(0, res.output_tokens);
        if (tokens > 0 || res.success) {
            emit q->usage(id, 0.0, static_cast<int>(qMin<qint64>(tokens, std::numeric_limits<int>::max())));
            if (!(r = find(id)))
                return;
            emit q->event(id, hre_map::usage_event(res.input_tokens, res.output_tokens, r->choice.vendor, res.cost_usd));
            if (!(r = find(id)))
                return;
        }
        if (res.cancelled || r->cancels > 0) {
            end(r, RunStatus::Cancelled, res.content, {});
            return;
        }
        if (res.success) {
            end(r, RunStatus::Completed, res.content, {});
            return;
        }
        QString code = harness::failure_id(res.failure);
        if (code.isEmpty())
            code = QStringLiteral("harness_failed");
        RunEvent e = hre_map::make_event(RunEventKind::Error, hre_tr("The harness stopped"), res.error,
                                         QJsonObject{{QStringLiteral("code"), code}});
        e.is_error = true;
        emit q->event(id, e);
        if ((r = find(id)))
            end(r, RunStatus::Failed, res.content, res.error);
    }
};

// ── HarnessRunExecutor ───────────────────────────────────────────────────────

HarnessRunExecutor::HarnessRunExecutor(QObject* parent) : IRunExecutor(parent), d_(std::make_unique<Impl>(this)) {
    // Offered only when a harness may serve terminal AI at all (API-key
    // authentication, consent); a run started otherwise fails with the reason.
    d_->update_available();
    connect(&ai_chat::ModelRouter::instance(), &ai_chat::ModelRouter::routes_changed, this,
            [this]() { d_->update_available(); });
}

HarnessRunExecutor::~HarnessRunExecutor() {
    for (auto it = d_->runs.begin(); it != d_->runs.end(); ++it)
        it->cancel_flag->store(true); // the calls stop; their results find no executor
    d_->pool->clear();
    // Teardown only: a cancelled call ends within the adapter's grace period;
    // one that does not is abandoned (the pool is leaked, never waited for).
    if (d_->pool->waitForDone(5000))
        delete d_->pool;
    d_->pool = nullptr;
}

QString HarnessRunExecutor::engine_id() const {
    return QString::fromLatin1(kExecutorHarness);
}

void HarnessRunExecutor::start(const TaskRun& run, const QVector<RunStep>& steps) {
    const QString id = run.id;
    if (Impl::Run* old = d_->find(id); old && !old->ended && old->busy) {
        LOG_WARN(kHreTag, QStringLiteral("Run %1 started again while running — ignored").arg(id.left(8)));
        return;
    }
    d_->runs.remove(id);
    Impl::Run fresh;
    fresh.id = id;
    fresh.snapshot = run;
    fresh.step = qMax(0, run.current_step);

    // Which harness: the first harness entry of the Chat Mode route.
    const ai_chat::RouteResult route =
        ai_chat::ModelRouter::instance().route(QString::fromLatin1(ai_chat::kFeatureChatModeHarness));
    bool found = false;
    for (const ai_chat::ModelChoice& c : route.chain) {
        if (c.is_harness()) {
            fresh.choice = c;
            found = true;
            break;
        }
    }
    d_->runs.insert(id, fresh);
    if (!found) {
        // Fail fast, but after start() returned (the caller is still launching the run).
        const QString reason = hre_unavailable_reason(route);
        QTimer::singleShot(0, this, [this, id, reason]() { d_->fail_unavailable(id, reason); });
        return;
    }
    Impl::Run* r = d_->find(id);
    const QString harness_id = r->choice.harness_id;
    const QString previous = hre_map::session_of(run.remote_task_id, harness_id);
    // Claude Code continues its own session; Codex terminal-AI sessions cannot be resumed.
    const bool can_resume = harness_id == QLatin1String("claude");
    QString resume;
    QString message = run.prompt;
    if (!run.remote_task_id.isEmpty()) {
        if (can_resume && !previous.isEmpty()) {
            resume = previous;
            r->session_id = previous;
            r->session_bound = true;
            message = hre_tr("Continue the task where you left off. If it is already complete, give the final report.");
        } else {
            emit event(id, hre_map::make_event(RunEventKind::Note, hre_tr("Starting the task again"),
                                               hre_tr("%1 cannot continue the earlier session of this run, so the "
                                                      "task starts from the beginning.")
                                                   .arg(r->choice.label.section(QStringLiteral(" · "), 0, 0))));
            if (!(r = d_->find(id)))
                return;
        }
    }
    emit event(id, hre_map::make_event(RunEventKind::Status, hre_tr("Starting %1").arg(r->choice.label),
                                       hre_tr("Runs on this computer through your own %1 API key; no Fincept credits "
                                              "are used. Paper only: no real order can be placed.")
                                           .arg(r->choice.vendor),
                                       QJsonObject{{QStringLiteral("harness"), harness_id},
                                                   {QStringLiteral("model"), r->choice.model}}));
    if (!(r = d_->find(id)))
        return;
    // A plan of one step until the harness reports its own todo list.
    if (steps.isEmpty()) {
        RunStep s;
        s.title = run.title.left(200);
        s.instruction = run.prompt;
        emit plan_ready(id, {s});
        if (!(r = d_->find(id)))
            return;
        emit step_changed(id, 0, StepStatus::Running, {});
        if (!(r = d_->find(id)))
            return;
    }
    LOG_INFO(kHreTag, QStringLiteral("Run %1: %2 via %3%4")
                          .arg(id.left(8), resume.isEmpty() ? QStringLiteral("start") : QStringLiteral("resume"),
                               harness_id, r->choice.model.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(r->choice.model)));
    d_->launch(r, message, resume);
}

void HarnessRunExecutor::answer(const QString& run_id, const QString& text) {
    Impl::Run* r = d_->find(run_id);
    if (!r || r->ended || r->busy) {
        if (!r)
            emit finished(run_id, RunStatus::Failed, {},
                          tr("This harness task can no longer be found, so your answer cannot reach it."));
        return;
    }
    if (r->choice.harness_id != QLatin1String("claude") || r->session_id.isEmpty()) {
        d_->end(r, RunStatus::Failed, {},
                tr("%1 cannot continue this task with an answer.").arg(r->choice.label.section(QStringLiteral(" · "), 0, 0)));
        return;
    }
    d_->launch(r, text, r->session_id);
}

void HarnessRunExecutor::request_pause(const QString& run_id) {
    Q_UNUSED(run_id) // a harness's own agent loop cannot stop at a step boundary
}

bool HarnessRunExecutor::can_pause(const QString& run_id) const {
    Q_UNUSED(run_id)
    return false;
}

void HarnessRunExecutor::cancel(const QString& run_id) {
    Impl::Run* r = d_->find(run_id);
    if (!r || r->ended) {
        if (!r)
            emit finished(run_id, RunStatus::Cancelled, {}, {}); // nothing here: a run the service counts must not hang
        return;
    }
    ++r->cancels;
    if (!r->busy || r->cancels > 1) {
        // Not running, or cancelled again: stop waiting for the process now.
        d_->end(r, RunStatus::Cancelled, {}, {});
        return;
    }
    r->cancel_flag->store(true); // the runner kills the process tree; on_done reports Cancelled
    emit event(run_id, harness_run_mapping::make_event(RunEventKind::Status, tr("Stopping the harness…"), {}));
}

} // namespace fincept::agent_tasks
