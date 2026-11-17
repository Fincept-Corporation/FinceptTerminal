// CloudPlanExecutor.cpp — a planned multi-step cloud task: a plan turn, one turn
// per step and a synthesis turn, all in the run's conversation (WP3).
//
// Qt Core only: every turn comes from the ICloudBackend seam (CloudTurnDriver in
// the app, a fake in tests/tst_agent_task_executors). All state is per run id;
// everything runs on the GUI thread's event loop. Rules kept throughout:
//   * finished() is emitted once per run (Run::ended guards it); a pause parks
//     the run with waiting(Paused) and forgets it (resume = start() again).
//   * Any emit may re-enter (TaskRunService's slots can cancel the run), so the
//     run is looked up again after every emit.
//   * The next turn never starts inside the previous turn's callbacks: the step
//     boundary runs from a zero-delay timer, after the previous stream closed.

#include "services/agent_tasks/CloudPlanExecutor.h"

#include "services/agent_tasks/RunPlanPrompts.h"

#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <map>

namespace fincept::agent_tasks {

namespace cloud_plan_executor_detail {

constexpr int kCpeSummaryChars = 12000;     ///< a step's result as kept (the store keeps up to 20 000)
constexpr int kCpePartialChars = 60000;     ///< the finished steps' results handed back as partial results
constexpr int kCpeRetryDelayMs = 5000;      ///< before the one retry of a failed turn
constexpr int kCpeBusyRetryDelayMs = 30000; ///< ... when the server said it is busy
constexpr int kCpeMaxRetries = 1;
qint64 cpe_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

QString cpe_number(double v) {
    return QString::number(v, 'g', 10);
}

RunEvent cpe_event(RunEventKind kind, const QString& title, const QString& body = {}, const QJsonObject& data = {}) {
    RunEvent e;
    e.kind = kind;
    e.title = title;
    e.body = body;
    e.data = data;
    return e;
}

bool cpe_settled(const RunStep& s) {
    return s.status == StepStatus::Done || s.status == StepStatus::Skipped;
}

/// Failures a second attempt cannot fix.
bool cpe_retryable(const QString& code) {
    static const char* const kFinal[] = {"insufficient_credits", "session_gone",    "no_session",
                                         "not_waiting",          "invalid_request", "session_create"};
    for (const char* c : kFinal)
        if (code == QLatin1String(c))
            return false;
    return true;
}

int cpe_retry_delay_ms(const QString& code) {
    return code == QLatin1String("agent_busy") || code == QLatin1String("agent_unavailable") ? kCpeBusyRetryDelayMs
                                                                                             : kCpeRetryDelayMs;
}

/// The plan's step cap: the run's step budget (or the hard cap), and no more
/// steps than the credit budget pays for besides the plan and synthesis turns.
/// Depends only on the budget, so a resumed run asks for the same plan.
int cpe_plan_cap(const TaskRun& run) {
    int cap = run.budget.max_steps > 0 ? run.budget.max_steps : run_plan_prompts::kPlanHardMaxSteps;
    if (run.budget.max_credits > 0) {
        const int affordable = static_cast<int>(std::floor(run.budget.max_credits / kCloudTurnCredits + 1e-9)) - 2;
        cap = std::min(cap, std::max(1, affordable));
    }
    return std::max(1, cap);
}

} // namespace cloud_plan_executor_detail

using namespace cloud_plan_executor_detail;
namespace rpp = run_plan_prompts;

// ── Per-run state and the state machine ──────────────────────────────────────

struct CloudPlanExecutor::Impl final : ICloudTurnListener {
    enum class Phase { Idle, Session, Plan, Step, Synthesis, Waiting };

    struct Run {
        QString id;
        TaskRun run;            ///< snapshot from start(): prompt, mode, profile, budget, context, started_at
        QString session_uuid;
        QVector<RunStep> steps; ///< the plan with each step's status and result
        Phase phase = Phase::Idle;
        Phase wait_phase = Phase::Idle; ///< the turn a question interrupted
        int step = -1;                  ///< the step a Step turn works on
        ICloudTurn* turn = nullptr;     ///< the current / last turn (release(), never delete)
        bool turn_live = false;         ///< a turn was started and its outcome is pending
        CloudTurnRequest last_request;  ///< for the one retry
        int retries = 0;                ///< retries of the current turn
        double credits = 0;             ///< run.credits_spent at start + credits reported since
        double assumed_credits = 0;     ///< adopted turns: charged by the server, bill never streamed
        bool pause_requested = false;
        bool cancel_requested = false;
        bool resumed = false;       ///< a paused / interrupted run: adopt stored answers before re-sending
        bool ended = false;         ///< finished() / waiting(Paused) reported: nothing more is signalled
        bool waiting_close = false; ///< the turn answered; the boundary waits for its stream to close
        bool advance_pending = false;
        bool retry_pending = false;
        int retry_token = 0;
    };

    CloudPlanExecutor* q;
    std::unique_ptr<ICloudBackend> backend;
    std::map<QString, std::unique_ptr<Run>> runs;

    Impl(CloudPlanExecutor* owner, std::unique_ptr<ICloudBackend> b) : q(owner), backend(std::move(b)) {}

    ~Impl() {
        for (auto& [id, r] : runs)
            if (r->turn)
                r->turn->release(); // children of the executor: deleted with it, nothing is emitted
        runs.clear();
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
        r->run = run;
        r->session_uuid = run.session_uuid;
        r->steps = steps;
        r->credits = run.credits_spent;
        Run* raw = r.get();
        runs.emplace(run.id, std::move(r));
        return raw;
    }

    void drop(const QString& id) {
        const auto it = runs.find(id);
        if (it == runs.end())
            return;
        if (it->second->turn)
            it->second->turn->release();
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
        if (backend)
            backend->log_info(QStringLiteral("Run %1: %2").arg(id.left(8), what));
    }

    static int settled_count(const Run* r) {
        return static_cast<int>(std::count_if(r->steps.cbegin(), r->steps.cend(), cpe_settled));
    }

    /// The finished steps' results, as the run's partial result.
    static QString partial_results(const Run* r) {
        QString out;
        for (const RunStep& s : r->steps) {
            if (s.status != StepStatus::Done || s.summary.trimmed().isEmpty())
                continue;
            const QString block = QStringLiteral("## ") + tr("Step %1 — %2").arg(s.idx + 1).arg(s.title) +
                                  QStringLiteral("\n\n") + s.summary.trimmed() + QStringLiteral("\n\n");
            if (out.size() + block.size() > kCpePartialChars) {
                out += tr("(More results are in the run's steps.)");
                break;
            }
            out += block;
        }
        return out.trimmed();
    }

    /// Why the next turn may not start; empty when it may. `starts_step`: the turn
    /// starts a step that has not run yet.
    QString budget_block(const Run* r, bool starts_step) const {
        const RunBudget& b = r->run.budget;
        const double spent = r->credits + r->assumed_credits;
        if (b.max_credits > 0 && spent + kCloudTurnCredits > b.max_credits + 1e-9)
            return tr("Credit budget reached: %1 of %2 credits spent, and the next turn costs about %3.")
                .arg(cpe_number(spent), cpe_number(b.max_credits), cpe_number(kCloudTurnCredits));
        TaskRun probe = r->run;
        probe.credits_spent = spent;
        int started = 0;
        for (const RunStep& s : r->steps)
            if (s.status != StepStatus::Pending && s.status != StepStatus::Skipped)
                ++started;
        if (starts_step)
            ++started;
        const BudgetVerdict v = check_budget(probe, started, cpe_now_ms());
        return v.exceeded ? v.reason : QString();
    }

    // ── The step boundary ────────────────────────────────────────────────────

    void schedule_advance(const QString& id) {
        Run* r = find(id);
        if (!r || r->ended || r->advance_pending)
            return;
        r->advance_pending = true;
        QTimer::singleShot(0, q, [this, id]() {
            Run* rr = find(id);
            if (!rr)
                return;
            rr->advance_pending = false;
            advance(rr);
        });
    }

    void advance(Run* r) {
        if (r->ended || r->turn_live || r->waiting_close || r->retry_pending || r->phase == Phase::Waiting ||
            r->phase == Phase::Session)
            return;
        if (r->cancel_requested) {
            end_run(r, RunStatus::Cancelled, partial_results(r), {});
            return;
        }
        if (r->steps.isEmpty()) {
            // Pause takes effect once there is a plan to resume.
            const QString blocked = budget_block(r, false);
            if (!blocked.isEmpty()) {
                budget_stop(r, blocked);
                return;
            }
            begin_plan(r);
            return;
        }
        int next = -1;
        for (int i = 0; i < r->steps.size(); ++i) {
            if (!cpe_settled(r->steps.at(i))) {
                next = i;
                break;
            }
        }
        if (r->pause_requested && next >= 0) {
            pause(r);
            return;
        }
        r->pause_requested = false; // nothing left to pause before: the report follows
        const QString blocked = budget_block(r, next >= 0 && r->steps.at(next).status == StepStatus::Pending);
        if (!blocked.isEmpty()) {
            budget_stop(r, blocked);
            return;
        }
        if (next >= 0)
            begin_step(r, next);
        else
            begin_synthesis(r);
    }

    void begin_plan(Run* r) {
        const QString id = r->id;
        r->phase = Phase::Plan;
        r->step = -1;
        const int cap = cpe_plan_cap(r->run);
        QJsonObject d;
        d["max_steps"] = cap;
        emit q->event(id, cpe_event(RunEventKind::Status, tr("Planning the task"),
                                    tr("Asking the agent for a plan of at most %1 step(s).").arg(cap), d));
        if (!(r = find(id)) || r->ended)
            return;
        log(id, QStringLiteral("plan turn (at most %1 steps)").arg(cap));
        CloudTurnRequest req;
        req.text = rpp::plan_request(r->run.prompt, r->run.context, cap);
        req.step = -1;
        req.check_stored_first = r->resumed;
        send_turn(r, req);
    }

    void begin_step(Run* r, int idx) {
        const QString id = r->id;
        r->phase = Phase::Step;
        r->step = idx;
        RunStep& s = r->steps[idx];
        // Started before (an app quit caught it): adopt its stored answer, if any.
        const bool started_before = s.attempts > 0;
        s.status = StepStatus::Running;
        ++s.attempts;
        emit q->step_changed(id, idx, StepStatus::Running, {});
        if (!(r = find(id)) || r->ended)
            return; // cancelled meanwhile (a budget check on the step's start)
        const int n = static_cast<int>(r->steps.size());
        QJsonObject d;
        d["step"] = idx;
        d["turn"] = QStringLiteral("step"); // the transcript starts a new turn here
        emit q->event(id, cpe_event(RunEventKind::Status,
                                    tr("Step %1 of %2: %3").arg(idx + 1).arg(n).arg(r->steps.at(idx).title), {}, d));
        if (!(r = find(id)) || r->ended)
            return;
        log(id, QStringLiteral("step %1/%2%3")
                    .arg(idx + 1)
                    .arg(n)
                    .arg(started_before ? QStringLiteral(" (resumed)") : QString()));
        CloudTurnRequest req;
        req.text = rpp::step_request(r->run, r->steps.at(idx), n, r->steps);
        req.step = idx;
        req.check_stored_first = started_before;
        send_turn(r, req);
    }

    void begin_synthesis(Run* r) {
        const QString id = r->id;
        r->phase = Phase::Synthesis;
        r->step = -1;
        QJsonObject d;
        d["turn"] = QStringLiteral("synthesis"); // the transcript starts a new turn here
        emit q->event(id, cpe_event(RunEventKind::Status, tr("Writing the final report"), {}, d));
        if (!(r = find(id)) || r->ended)
            return;
        log(id, QStringLiteral("synthesis turn"));
        CloudTurnRequest req;
        req.text = rpp::synthesis_request(r->run, r->steps);
        req.step = -1;
        req.check_stored_first = r->resumed;
        send_turn(r, req);
    }

    void send_turn(Run* r, CloudTurnRequest req) {
        req.session_uuid = r->session_uuid;
        req.mode = r->run.mode == QLatin1String("deep") ? QStringLiteral("deep") : QStringLiteral("lite");
        req.profile_id = r->run.profile_id;
        if (r->turn) {
            r->turn->release();
            r->turn = nullptr;
        }
        r->last_request = req;
        r->last_request.check_stored_first = false; // a retry sends
        r->turn_live = true;
        r->waiting_close = false;
        r->turn = backend->create_turn(this, r->id, q);
        if (!r->turn) {
            r->turn_live = false;
            end_run(r, RunStatus::Failed, partial_results(r), tr("The cloud agent is not available in this build."));
            return;
        }
        r->turn->start(req); // may call back at once: `r` is not used after this
    }

    void after_turn(Run* r) {
        r->retries = 0;
        r->phase = Phase::Idle;
        if (r->turn && r->turn->is_open()) {
            // Deep mode sends a self-check after "finish", and the server stores the
            // answer when the stream ends: the next message waits for that.
            r->waiting_close = true;
            return;
        }
        schedule_advance(r->id);
    }

    // ── Outcomes ─────────────────────────────────────────────────────────────

    void on_plan(Run* r, const QString& reply) {
        const QString id = r->id;
        const int cap = cpe_plan_cap(r->run);
        QString why;
        int proposed = 0;
        QVector<RunStep> steps = rpp::parse_plan_reply(reply, r->run.prompt, cap, &why, &proposed);
        for (int i = 0; i < steps.size(); ++i) {
            steps[i].run_id = id;
            steps[i].idx = i;
            steps[i].status = StepStatus::Pending;
        }
        r->steps = steps;
        const int n = static_cast<int>(steps.size());
        log(id, why.isEmpty() ? QStringLiteral("plan ready: %1 step(s)").arg(n)
                              : QStringLiteral("plan not readable (%1): one step").arg(why));
        emit q->plan_ready(id, steps);
        if (!(r = find(id)) || r->ended)
            return;

        QStringList lines;
        for (const RunStep& s : steps) {
            QString line = QStringLiteral("%1. ").arg(s.idx + 1) + s.title;
            if (!s.deliverable.isEmpty())
                line += QStringLiteral(" — ") + s.deliverable;
            lines << line;
        }
        QJsonObject d;
        d["steps"] = n;
        d["source"] = why.isEmpty() ? QStringLiteral("agent") : QStringLiteral("fallback");
        emit q->event(id, cpe_event(RunEventKind::Plan, n == 1 ? tr("Plan: 1 step") : tr("Plan: %1 steps").arg(n),
                                    lines.join(QLatin1Char('\n')), d));
        if (!(r = find(id)) || r->ended)
            return;
        if (!why.isEmpty()) {
            emit q->event(id, cpe_event(RunEventKind::Note,
                                        tr("The agent's plan could not be read (%1), so the task runs as one step.")
                                            .arg(why)));
        } else if (proposed > n) {
            emit q->event(id, cpe_event(RunEventKind::Note,
                                        tr("The agent proposed %1 steps; the budget allows %2, so the first %2 run.")
                                            .arg(proposed)
                                            .arg(n)));
        }
        if (!(r = find(id)) || r->ended)
            return;
        QJsonObject est;
        est["estimate_credits"] = estimate_cloud_plan_credits(n);
        emit q->event(id, cpe_event(RunEventKind::Note,
                                    tr("Estimated cost: about %1 credits (the plan, %2 step(s) and the final report at "
                                       "about %3 credits each).")
                                        .arg(cpe_number(estimate_cloud_plan_credits(n)))
                                        .arg(n)
                                        .arg(cpe_number(kCloudTurnCredits)),
                                    {}, est));
        if (!(r = find(id)) || r->ended)
            return;
        after_turn(r);
    }

    void on_step_answer(Run* r, const CloudTurnOutcome& o) {
        const QString id = r->id;
        const int idx = r->step;
        if (idx < 0 || idx >= r->steps.size()) {
            after_turn(r);
            return;
        }
        const QString summary = rpp::clip(o.text, kCpeSummaryChars);
        RunStep& s = r->steps[idx];
        s.status = StepStatus::Done;
        s.summary = summary;
        if (o.adopted)
            s.message_uuid = o.message_uuid;
        log(id, QStringLiteral("step %1 done (%2 chars%3)")
                    .arg(idx + 1)
                    .arg(summary.size())
                    .arg(o.adopted ? QStringLiteral(", recovered") : QString()));
        emit q->step_changed(id, idx, StepStatus::Done, summary);
        if (!(r = find(id)) || r->ended)
            return;
        after_turn(r);
    }

    void on_failure(Run* r, const CloudTurnOutcome& o) {
        const QString id = r->id;
        const Phase phase = r->phase;
        const int idx = r->step;
        const QString text = cloud_turn_failure_text(o.error, o.code);
        log(id, QStringLiteral("turn failed (%1): %2").arg(o.code, text.left(200)));
        const bool is_step = phase == Phase::Step && idx >= 0 && idx < r->steps.size();
        if (is_step) {
            r->steps[idx].status = StepStatus::Failed;
            emit q->step_changed(id, idx, StepStatus::Failed, text);
            if (!(r = find(id)) || r->ended)
                return;
        }
        if (r->cancel_requested) {
            end_run(r, RunStatus::Cancelled, partial_results(r), {});
            return;
        }
        if (r->retries < kCpeMaxRetries && cpe_retryable(o.code)) {
            ++r->retries;
            const int delay = cpe_retry_delay_ms(o.code);
            QString title;
            if (is_step)
                title = tr("Step %1 failed — trying it once more").arg(idx + 1);
            else if (phase == Phase::Plan)
                title = tr("Planning failed — trying once more");
            else
                title = tr("The final report failed — trying once more");
            QJsonObject d;
            d["retry"] = true;
            if (!o.code.isEmpty())
                d["code"] = o.code;
            r->retry_pending = true;
            const int token = ++r->retry_token;
            emit q->event(id, cpe_event(RunEventKind::Note, title,
                                        text + QLatin1Char(' ') + tr("Retrying in %1 s.").arg(delay / 1000), d));
            if (!(r = find(id)) || r->ended)
                return;
            QTimer::singleShot(delay, q, [this, id, token]() { retry(id, token); });
            return;
        }
        if (!o.error_reported) {
            RunEvent e = cpe_event(RunEventKind::Error,
                                   is_step                 ? tr("Step %1 failed").arg(idx + 1)
                                   : phase == Phase::Plan ? tr("Planning failed")
                                                          : tr("The final report failed"),
                                   text);
            e.is_error = true;
            if (!o.code.isEmpty())
                e.data["code"] = o.code;
            emit q->event(id, e);
            if (!(r = find(id)) || r->ended)
                return;
        }
        QString error;
        if (is_step)
            error = tr("Step %1 (%2) failed: %3").arg(idx + 1).arg(r->steps.at(idx).title, text);
        else if (phase == Phase::Plan)
            error = tr("The task could not be planned: %1").arg(text);
        else
            error = tr("The final report could not be written: %1").arg(text);
        end_run(r, RunStatus::Failed, partial_results(r), error);
    }

    void retry(const QString& id, int token) {
        Run* r = find(id);
        if (!r || r->ended || !r->retry_pending || r->retry_token != token)
            return;
        r->retry_pending = false;
        if (r->cancel_requested) {
            end_run(r, RunStatus::Cancelled, partial_results(r), {});
            return;
        }
        const QString blocked = budget_block(r, false); // the retry is a turn too
        if (!blocked.isEmpty()) {
            budget_stop(r, blocked);
            return;
        }
        if (r->phase == Phase::Step && r->step >= 0 && r->step < r->steps.size()) {
            RunStep& s = r->steps[r->step];
            s.status = StepStatus::Running;
            ++s.attempts;
            emit q->step_changed(id, r->step, StepStatus::Running, {});
            if (!(r = find(id)) || r->ended)
                return;
        }
        log(id, QStringLiteral("retrying the failed turn"));
        send_turn(r, r->last_request);
    }

    void wait_for_answer(Run* r, const CloudTurnOutcome& o) {
        const QString id = r->id;
        r->wait_phase = r->phase == Phase::Waiting ? r->wait_phase : r->phase;
        r->phase = Phase::Waiting;
        QJsonObject payload;
        payload["question"] = o.question_text;
        payload["resumable"] = true;
        if (!o.question_mode.isEmpty())
            payload["mode"] = o.question_mode;
        payload["phase"] = r->wait_phase == Phase::Plan        ? QStringLiteral("plan")
                           : r->wait_phase == Phase::Synthesis ? QStringLiteral("synthesis")
                                                               : QStringLiteral("step");
        if (r->wait_phase == Phase::Step)
            payload["step"] = r->step;
        log(id, QStringLiteral("waits for the user's answer"));
        emit q->waiting(id, RunStatus::WaitingInput, payload);
    }

    void pause(Run* r) {
        const QString id = r->id;
        const int done = settled_count(r);
        const int n = static_cast<int>(r->steps.size());
        r->ended = true; // parked: resume_run() starts the run again
        r->pause_requested = false;
        log(id, QStringLiteral("paused after %1/%2 steps").arg(done).arg(n));
        emit q->event(id, cpe_event(RunEventKind::Status, done == 0 ? tr("Paused before the first step")
                                                                    : tr("Paused after step %1 of %2").arg(done).arg(n)));
        emit q->waiting(id, RunStatus::Paused, {});
        drop_later(id);
    }

    void budget_stop(Run* r, const QString& reason) {
        const QString id = r->id;
        log(id, reason);
        RunEvent e = cpe_event(RunEventKind::Status, tr("Budget reached — stopping before the next turn"), reason);
        e.is_error = true;
        emit q->event(id, e);
        if (!(r = find(id)) || r->ended)
            return;
        end_run(r, RunStatus::BudgetStopped, partial_results(r), reason);
    }

    void end_run(Run* r, RunStatus status, const QString& final_text, const QString& error) {
        if (r->ended)
            return;
        r->ended = true;
        r->pause_requested = false;
        r->retry_pending = false;
        ++r->retry_token;
        const QString id = r->id;
        if (r->turn_live && r->turn) {
            r->turn->release(); // nothing more is wanted from it
            r->turn = nullptr;
            r->turn_live = false;
        }
        // A stream still open after its answer keeps reporting (deep mode's self-
        // check) until it closes; then the run's state goes.
        const bool lingering = r->turn && r->turn->is_open();
        log(id, QStringLiteral("ended: %1").arg(run_status_id(status)));
        emit q->finished(id, status, final_text, error);
        if (!lingering)
            drop_later(id);
    }

    // ── ICloudTurnListener ───────────────────────────────────────────────────

    void cloud_turn_event(const QString& run_id, const RunEvent& e) override {
        if (find(run_id))
            emit q->event(run_id, e);
    }

    void cloud_turn_text(const QString& run_id, int step, const QString& chunk) override {
        Run* r = find(run_id);
        if (!r || r->phase == Phase::Plan)
            return; // the plan's JSON is shown as the parsed plan, not as reply text
        emit q->text_delta(run_id, step, chunk);
    }

    void cloud_turn_usage(const QString& run_id, double credits, int tokens) override {
        Run* r = find(run_id);
        if (!r)
            return;
        if (credits > 0)
            r->credits += credits;
        emit q->usage(run_id, credits, tokens);
    }

    void cloud_turn_outcome(const QString& run_id, const CloudTurnOutcome& o) override {
        Run* r = find(run_id);
        if (!r || r->ended || !r->turn_live)
            return;
        r->turn_live = false;
        if (o.adopted)
            r->assumed_credits += kCloudTurnCredits; // for the budget only; usage() reports what streamed
        switch (o.kind) {
            case CloudTurnOutcome::Kind::Answered:
                if (o.question && o.resumable) {
                    wait_for_answer(r, o);
                    return;
                }
                if (r->phase == Phase::Plan)
                    on_plan(r, o.text);
                else if (r->phase == Phase::Synthesis)
                    end_run(r, RunStatus::Completed, o.text.trimmed().isEmpty() ? partial_results(r) : o.text, {});
                else
                    on_step_answer(r, o);
                return;
            case CloudTurnOutcome::Kind::Failed:
                on_failure(r, o);
                return;
            case CloudTurnOutcome::Kind::Stopped:
                end_run(r, RunStatus::Cancelled, partial_results(r), o.error);
                return;
        }
    }

    void cloud_turn_closed(const QString& run_id) override {
        Run* r = find(run_id);
        if (!r)
            return;
        if (r->ended) {
            drop_later(run_id);
            return;
        }
        if (r->waiting_close) {
            r->waiting_close = false;
            schedule_advance(run_id);
        }
    }
};

// ── CloudPlanExecutor ────────────────────────────────────────────────────────
// The default constructor (network backend) is in CloudTurnDriver.cpp.

CloudPlanExecutor::CloudPlanExecutor(std::unique_ptr<ICloudBackend> backend, QObject* parent)
    : IRunExecutor(parent), d_(std::make_unique<Impl>(this, std::move(backend))) {
    // The Chat Mode UI offers an engine unless its executor says otherwise
    // (rv::kExecutorAvailableProperty in screens/chat_mode/RunViewWidgets.h).
    setProperty("available", true);
}

CloudPlanExecutor::~CloudPlanExecutor() = default;

QString CloudPlanExecutor::engine_id() const {
    return QString::fromLatin1(kExecutorCloudPlan);
}

void CloudPlanExecutor::start(const TaskRun& run, const QVector<RunStep>& steps) {
    const QString id = run.id;
    Impl::Run* r = d_->make_run(run, steps);
    if (!d_->backend) {
        // Fails after start() returned; the state makes a cancel meanwhile the one finished().
        r->phase = Impl::Phase::Session;
        QTimer::singleShot(0, this, [this, id]() {
            if (Impl::Run* rr = d_->find(id); rr && !rr->ended)
                d_->end_run(rr, RunStatus::Failed, {}, tr("Multi-step cloud runs are not available in this build."));
        });
        return;
    }
    // Only a run the terminal's closing interrupted can have a turn the server
    // finished and stored meanwhile. A paused run stopped at a step boundary: its
    // next turn was never sent, so nothing is looked up (and the transcript does
    // not claim the terminal was closed).
    r->resumed = run.resume_after_restart;
    d_->log(id, QStringLiteral("start (%1 known steps%2)")
                    .arg(r->steps.size())
                    .arg(r->resumed ? QStringLiteral(", resumed") : QString()));

    if (r->steps.isEmpty() && !r->resumed) {
        const int cap = cpe_plan_cap(run);
        QJsonObject est;
        est["estimate_credits"] = estimate_cloud_plan_credits(cap);
        est["max_steps"] = cap;
        emit event(id, cpe_event(RunEventKind::Note,
                                 tr("Estimated cost: up to %1 credits — a plan, up to %2 step(s) and a final report "
                                    "at about %3 credits each.")
                                     .arg(cpe_number(estimate_cloud_plan_credits(cap)))
                                     .arg(cap)
                                     .arg(cpe_number(kCloudTurnCredits)),
                                 {}, est));
        if (!(r = d_->find(id)) || r->ended)
            return;
    }

    if (!r->session_uuid.isEmpty()) {
        d_->schedule_advance(id);
        return;
    }
    // A run without a conversation (NEW RUN / schedule): create one; every turn of
    // the run then talks in it.
    r->phase = Impl::Phase::Session;
    d_->backend->create_session(this, [this, id](bool ok, QString uuid, QString err) {
        Impl::Run* rr = d_->find(id);
        if (!rr || rr->ended || rr->phase != Impl::Phase::Session)
            return;
        if (!ok) {
            const QString text = tr("Could not create a conversation for this run: %1").arg(err);
            RunEvent e = cpe_event(RunEventKind::Error, tr("The run could not start"), text);
            e.is_error = true;
            e.data["code"] = QStringLiteral("session_create");
            emit event(id, e);
            if ((rr = d_->find(id)) && !rr->ended)
                d_->end_run(rr, RunStatus::Failed, {}, text);
            return;
        }
        rr->session_uuid = uuid;
        rr->phase = Impl::Phase::Idle;
        QJsonObject d;
        d["session_uuid"] = uuid;
        d["verbose"] = true;
        emit event(id, cpe_event(RunEventKind::Status, tr("Conversation created"), {}, d));
        d_->schedule_advance(id);
    });
}

void CloudPlanExecutor::answer(const QString& run_id, const QString& text) {
    Impl::Run* r = d_->find(run_id);
    if (r && r->ended) {
        d_->drop(run_id); // only a closing stream was left of it
        r = nullptr;
    }
    if (!r) {
        // Waiting since before a restart: rebuild the run from the record.
        const TaskRun* run = d_->backend ? d_->backend->find_run(run_id) : nullptr;
        if (!run) {
            emit finished(run_id, RunStatus::Failed, {}, tr("This run no longer exists."));
            return;
        }
        r = d_->make_run(*run, d_->backend->run_steps(run_id));
        r->phase = Impl::Phase::Waiting;
        r->wait_phase = Impl::Phase::Synthesis;
        if (r->steps.isEmpty()) {
            r->wait_phase = Impl::Phase::Plan;
        } else {
            for (int i = 0; i < r->steps.size(); ++i) {
                if (!cpe_settled(r->steps.at(i))) {
                    r->wait_phase = Impl::Phase::Step;
                    r->step = i;
                    break;
                }
            }
        }
    }
    if (r->ended || r->phase != Impl::Phase::Waiting)
        return; // not waiting for an answer (TaskRunService only answers WaitingInput runs)
    r->phase = r->wait_phase;
    const QString blocked = d_->budget_block(r, false); // the answer starts a turn
    if (!blocked.isEmpty()) {
        d_->budget_stop(r, blocked);
        return;
    }
    d_->log(run_id, QStringLiteral("answer sent"));
    CloudTurnRequest req;
    req.text = text;
    req.as_answer = true; // through /chat/agent/stream/resume, else as a new message
    req.step = r->phase == Impl::Phase::Step ? r->step : -1;
    d_->send_turn(r, req);
}

void CloudPlanExecutor::request_pause(const QString& run_id) {
    Impl::Run* r = d_->find(run_id);
    if (!can_pause(run_id) || !r)
        return;
    r->pause_requested = true;
    d_->log(run_id, QStringLiteral("pause requested"));
    if (!r->turn_live && !r->waiting_close && !r->retry_pending && r->phase == Impl::Phase::Idle)
        d_->schedule_advance(run_id);
}

bool CloudPlanExecutor::can_pause(const QString& run_id) const {
    const Impl::Run* r = d_->find(run_id);
    if (!r || r->ended || r->cancel_requested)
        return false;
    if (r->phase == Impl::Phase::Synthesis || r->phase == Impl::Phase::Waiting)
        return false;
    // Steps remain (or the plan is still to come): there is a boundary to stop at.
    if (r->phase == Impl::Phase::Step || r->steps.isEmpty())
        return true;
    return std::any_of(r->steps.cbegin(), r->steps.cend(), [](const RunStep& s) { return !cpe_settled(s); });
}

void CloudPlanExecutor::cancel(const QString& run_id) {
    Impl::Run* r = d_->find(run_id);
    if (!r || r->ended) {
        // Nothing here (a restart) or only a closing stream: a run the service
        // still counts as running must not hang — report it cancelled.
        const TaskRun* run = d_->backend ? d_->backend->find_run(run_id) : nullptr;
        if (!r && run && holds_slot(run->status))
            emit finished(run_id, RunStatus::Cancelled, {}, {});
        return;
    }
    if (r->phase == Impl::Phase::Waiting) {
        // Nothing in flight: forget it; the service ends the run itself.
        r->ended = true;
        d_->drop_later(run_id);
        return;
    }
    r->cancel_requested = true;
    r->pause_requested = false;
    d_->log(run_id, QStringLiteral("cancel requested"));
    if (r->turn_live && r->turn) {
        // The server cannot be stopped: the turn detaches and its stored answer is
        // recovered (kept as the step's result), then the run ends Cancelled.
        // Cancel again while recovering: stop waiting at once.
        r->turn->detach();
        return;
    }
    d_->end_run(r, RunStatus::Cancelled, Impl::partial_results(r), {});
}

} // namespace fincept::agent_tasks
