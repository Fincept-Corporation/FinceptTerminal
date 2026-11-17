// TaskRunTypes.cpp — ids, labels, the run state machine, budget checks and JSON
// for the run model. Qt Core only (linked by tests/tst_agent_task_runs).

#include "services/agent_tasks/TaskRunTypes.h"

#include <QCoreApplication>
#include <QJsonDocument>

#include <array>
#include <utility>

namespace fincept::agent_tasks {

namespace task_run_types_detail {

struct StatusName {
    RunStatus status;
    const char* id;
};
constexpr std::array<StatusName, 11> kRunStatusNames{{
    {RunStatus::Queued, "queued"},
    {RunStatus::Planning, "planning"},
    {RunStatus::Running, "running"},
    {RunStatus::WaitingInput, "waiting_input"},
    {RunStatus::WaitingApproval, "waiting_approval"},
    {RunStatus::Paused, "paused"},
    {RunStatus::Completed, "completed"},
    {RunStatus::Failed, "failed"},
    {RunStatus::Cancelled, "cancelled"},
    {RunStatus::BudgetStopped, "budget_stopped"},
    {RunStatus::Interrupted, "interrupted"},
}};

struct StepName {
    StepStatus status;
    const char* id;
};
constexpr std::array<StepName, 5> kStepStatusNames{{
    {StepStatus::Pending, "pending"},
    {StepStatus::Running, "running"},
    {StepStatus::Done, "done"},
    {StepStatus::Failed, "failed"},
    {StepStatus::Skipped, "skipped"},
}};

struct KindName {
    RunEventKind kind;
    const char* id;
};
constexpr std::array<KindName, 17> kEventKindNames{{
    {RunEventKind::Plan, "plan"},
    {RunEventKind::StepStart, "step_start"},
    {RunEventKind::StepEnd, "step_end"},
    {RunEventKind::Thinking, "thinking"},
    {RunEventKind::ToolCall, "tool_call"},
    {RunEventKind::ToolResult, "tool_result"},
    {RunEventKind::Delegation, "delegation"},
    {RunEventKind::Question, "question"},
    {RunEventKind::Answer, "answer"},
    {RunEventKind::Approval, "approval"},
    {RunEventKind::ApprovalDecided, "approval_decided"},
    {RunEventKind::Usage, "usage"},
    {RunEventKind::Note, "note"},
    {RunEventKind::Artefact, "artefact"},
    {RunEventKind::Error, "error"},
    {RunEventKind::Final, "final"},
    {RunEventKind::Status, "status"},
}};

QString trt_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::TaskRun", text);
}

QString trt_minutes(int minutes) {
    return minutes == 1 ? trt_tr("1 minute") : trt_tr("%1 minutes").arg(minutes);
}

} // namespace task_run_types_detail

using namespace task_run_types_detail;

// ── TaskRun ──────────────────────────────────────────────────────────────────

QString TaskRun::executor_id() const {
    return executor_for(engine, plan_first);
}

bool TaskRun::is_unattended() const {
    return is_unattended_origin(origin);
}

// ── Ids and labels ───────────────────────────────────────────────────────────

QString run_status_id(RunStatus s) {
    for (const auto& n : kRunStatusNames)
        if (n.status == s)
            return QString::fromLatin1(n.id);
    return QStringLiteral("failed");
}

std::optional<RunStatus> run_status_from_id(const QString& id) {
    for (const auto& n : kRunStatusNames)
        if (id == QLatin1String(n.id))
            return n.status;
    return std::nullopt;
}

QString run_status_label(RunStatus s) {
    switch (s) {
        case RunStatus::Queued:
            return trt_tr("Queued");
        case RunStatus::Planning:
            return trt_tr("Planning");
        case RunStatus::Running:
            return trt_tr("Running");
        case RunStatus::WaitingInput:
            return trt_tr("Waiting for input");
        case RunStatus::WaitingApproval:
            return trt_tr("Waiting for approval");
        case RunStatus::Paused:
            return trt_tr("Paused");
        case RunStatus::Completed:
            return trt_tr("Completed");
        case RunStatus::Failed:
            return trt_tr("Failed");
        case RunStatus::Cancelled:
            return trt_tr("Cancelled");
        case RunStatus::BudgetStopped:
            return trt_tr("Budget reached");
        case RunStatus::Interrupted:
            return trt_tr("Interrupted");
    }
    return trt_tr("Failed");
}

QString step_status_id(StepStatus s) {
    for (const auto& n : kStepStatusNames)
        if (n.status == s)
            return QString::fromLatin1(n.id);
    return QStringLiteral("pending");
}

std::optional<StepStatus> step_status_from_id(const QString& id) {
    for (const auto& n : kStepStatusNames)
        if (id == QLatin1String(n.id))
            return n.status;
    return std::nullopt;
}

QString step_status_label(StepStatus s) {
    switch (s) {
        case StepStatus::Pending:
            return trt_tr("Pending");
        case StepStatus::Running:
            return trt_tr("Running");
        case StepStatus::Done:
            return trt_tr("Done");
        case StepStatus::Failed:
            return trt_tr("Failed");
        case StepStatus::Skipped:
            return trt_tr("Skipped");
    }
    return trt_tr("Pending");
}

QString event_kind_id(RunEventKind k) {
    for (const auto& n : kEventKindNames)
        if (n.kind == k)
            return QString::fromLatin1(n.id);
    return QStringLiteral("note");
}

std::optional<RunEventKind> event_kind_from_id(const QString& id) {
    for (const auto& n : kEventKindNames)
        if (id == QLatin1String(n.id))
            return n.kind;
    return std::nullopt;
}

QString executor_for(const QString& engine, bool plan_first) {
    if (engine == QLatin1String(kEngineCloud))
        return QString::fromLatin1(plan_first ? kExecutorCloudPlan : kExecutorCloudTurn);
    if (engine == QLatin1String(kEngineLocal))
        return QString::fromLatin1(kExecutorLocal);
    if (engine == QLatin1String("harness")) // HarnessRunExecutor (kEngineHarness / kExecutorHarness)
        return QStringLiteral("harness");
    return {};
}

bool is_unattended_origin(const QString& origin) {
    return origin == QLatin1String(kOriginSchedule);
}

// ── State machine ────────────────────────────────────────────────────────────

bool is_terminal(RunStatus s) {
    return s == RunStatus::Completed || s == RunStatus::Failed || s == RunStatus::Cancelled ||
           s == RunStatus::BudgetStopped || s == RunStatus::Interrupted;
}

bool is_active(RunStatus s) {
    return s == RunStatus::Queued || s == RunStatus::Planning || s == RunStatus::Running;
}

bool holds_slot(RunStatus s) {
    return s == RunStatus::Planning || s == RunStatus::Running;
}

bool needs_attention(RunStatus s) {
    return s == RunStatus::WaitingInput || s == RunStatus::WaitingApproval;
}

bool can_resume(RunStatus s) {
    return s == RunStatus::Paused || s == RunStatus::Interrupted;
}

bool can_transition(RunStatus from, RunStatus to) {
    if (from == to)
        return true;
    switch (from) {
        case RunStatus::Queued:
            // Started, held back (paused before it started), cancelled before it
            // started, failed to launch, or caught by a quit.
            return to == RunStatus::Planning || to == RunStatus::Running || to == RunStatus::Paused ||
                   to == RunStatus::Cancelled || to == RunStatus::Failed || to == RunStatus::Interrupted;
        case RunStatus::Planning:
        case RunStatus::Running:
            // Planning → Running when the plan is ready; Running → Planning when a
            // resumed plan run re-plans. Either can wait, pause or end.
            return to != RunStatus::Queued;
        case RunStatus::WaitingInput:
        case RunStatus::WaitingApproval:
            // Answered / decided → Running; a new message instead → Completed.
            return to == RunStatus::Running || to == RunStatus::Planning || to == RunStatus::Completed ||
                   to == RunStatus::Cancelled || to == RunStatus::Failed || to == RunStatus::Interrupted ||
                   to == RunStatus::BudgetStopped;
        case RunStatus::Paused:
            // Resumed at once, or queued behind other runs.
            return to == RunStatus::Running || to == RunStatus::Planning || to == RunStatus::Queued ||
                   to == RunStatus::Cancelled || to == RunStatus::Failed;
        case RunStatus::Interrupted:
            // resume_run(): back in the queue (or straight to work).
            return to == RunStatus::Queued || to == RunStatus::Running || to == RunStatus::Planning ||
                   to == RunStatus::Cancelled;
        case RunStatus::Completed:
        case RunStatus::Failed:
        case RunStatus::Cancelled:
        case RunStatus::BudgetStopped:
            return false; // final; a retry is a new (child) run
    }
    return false;
}

RunStatus status_after_restart(RunStatus s) {
    switch (s) {
        case RunStatus::Queued:
        case RunStatus::Planning:
        case RunStatus::Running:
        case RunStatus::WaitingApproval: // the approval prompt lived in the closed session
            return RunStatus::Interrupted;
        default:
            return s;
    }
}

// ── Budget ───────────────────────────────────────────────────────────────────

BudgetVerdict check_budget(const TaskRun& run, int steps_started, qint64 now_ms) {
    BudgetVerdict v;
    const RunBudget& b = run.budget;
    if (b.max_credits > 0 && run.credits_spent > b.max_credits + 1e-9) {
        v.exceeded = true;
        v.reason = trt_tr("Credit budget reached: %1 of %2 credits spent.")
                       .arg(QString::number(run.credits_spent, 'f', 2), QString::number(b.max_credits, 'f', 2));
        return v;
    }
    if (b.max_steps > 0 && steps_started > b.max_steps) {
        v.exceeded = true;
        v.reason = trt_tr("Step budget reached: the run wanted step %1, the budget allows %2.")
                       .arg(steps_started)
                       .arg(b.max_steps);
        return v;
    }
    if (b.max_minutes > 0 && run_active_ms(run, now_ms) > qint64(b.max_minutes) * 60 * 1000) {
        v.exceeded = true;
        v.reason = trt_tr("Time budget reached: the run has been working for more than %1.").arg(trt_minutes(b.max_minutes));
        return v;
    }
    return v;
}

double estimate_cloud_plan_credits(int steps) {
    return (qMax(1, steps) + 2) * kCloudTurnCredits;
}

RunCostEstimate estimate_run_cost(const QString& executor_id, const RunBudget& budget, int hard_max_steps) {
    RunCostEstimate e;
    const int hard = qMax(1, hard_max_steps);
    if (executor_id == QLatin1String(kExecutorLocal) || executor_id == QLatin1String("harness")) {
        // The user's own model (or harness, on its own API key): no Fincept credits; steps only.
        e.uses_credits = false;
        e.steps = budget.max_steps > 0 ? budget.max_steps : hard;
        return e;
    }
    if (executor_id == QLatin1String(kExecutorCloudTurn)) {
        e.credits = kCloudTurnCredits;
        e.needs_confirmation = e.credits > kConfirmAboveCredits + 1e-9;
        return e;
    }
    // A planned cloud run: a plan turn, one turn per step and a synthesis turn.
    // Same cap as CloudPlanExecutor: the step budget (or the hard cap), and no more
    // steps than the credit budget pays for besides the plan and synthesis turns.
    int cap = budget.max_steps > 0 ? budget.max_steps : hard;
    if (budget.max_credits > 0) {
        const int affordable = static_cast<int>(budget.max_credits / kCloudTurnCredits + 1e-9) - 2;
        cap = qMin(cap, qMax(1, affordable));
    }
    e.steps = qMax(1, cap);
    e.credits = estimate_cloud_plan_credits(e.steps);
    if (budget.max_credits > 0)
        e.credits = qMin(e.credits, qMax(budget.max_credits, estimate_cloud_plan_credits(1)));
    e.unlimited = budget.max_credits <= 0;
    e.needs_confirmation = e.credits > kConfirmAboveCredits + 1e-9;
    return e;
}

// ── Working time ─────────────────────────────────────────────────────────────

qint64 run_active_ms(const TaskRun& run, qint64 now_ms) {
    if (run.active_ms > 0 || run.active_since_ms > 0) {
        qint64 total = qMax<qint64>(0, run.active_ms);
        if (run.active_since_ms > 0 && holds_slot(run.status) && now_ms > run.active_since_ms)
            total += now_ms - run.active_since_ms;
        return total;
    }
    // Recorded before the working clock existed: started → finished, and for a run
    // that stands still (paused, waiting) up to the moment it stopped moving.
    if (run.started_at_ms <= 0)
        return 0;
    qint64 end = run.finished_at_ms > 0 ? run.finished_at_ms : now_ms;
    if (run.finished_at_ms <= 0 && !is_active(run.status) && run.updated_at_ms > run.started_at_ms)
        end = qMin(end, run.updated_at_ms);
    return qMax<qint64>(0, end - run.started_at_ms);
}

void account_active_time(TaskRun& run, RunStatus to, qint64 now_ms) {
    const bool was_working = holds_slot(run.status) && run.active_since_ms > 0;
    const bool will_work = holds_slot(to);
    if (was_working && !will_work) {
        if (now_ms > run.active_since_ms)
            run.active_ms += now_ms - run.active_since_ms;
        run.active_since_ms = 0;
    } else if (will_work && run.active_since_ms <= 0) {
        // A run recorded before the clock existed (resumed after an upgrade): keep
        // what it worked so far.
        if (run.active_ms <= 0 && run.started_at_ms > 0)
            run.active_ms = run_active_ms(run, now_ms);
        run.active_since_ms = now_ms;
    }
}

// ── JSON ─────────────────────────────────────────────────────────────────────

QJsonObject run_request_to_json(const RunRequest& r) {
    QJsonObject o;
    o["title"] = r.title;
    o["prompt"] = r.prompt;
    o["engine"] = r.engine;
    o["mode"] = r.mode;
    o["template_id"] = r.template_id;
    o["session_uuid"] = r.session_uuid;
    o["origin"] = r.origin;
    o["parent_run_id"] = r.parent_run_id;
    o["schedule_id"] = r.schedule_id;
    o["profile_id"] = r.profile_id;
    o["context"] = r.context;
    o["plan_first"] = r.plan_first;
    QJsonObject b;
    b["max_credits"] = r.budget.max_credits;
    b["max_steps"] = r.budget.max_steps;
    b["max_minutes"] = r.budget.max_minutes;
    o["budget"] = b;
    return o;
}

RunRequest run_request_from_json(const QJsonObject& o) {
    RunRequest r;
    r.title = o.value("title").toString();
    r.prompt = o.value("prompt").toString();
    r.engine = o.value("engine").toString();
    r.mode = o.value("mode").toString();
    r.template_id = o.value("template_id").toString();
    r.session_uuid = o.value("session_uuid").toString();
    r.origin = o.value("origin").toString();
    r.parent_run_id = o.value("parent_run_id").toString();
    r.schedule_id = o.value("schedule_id").toString();
    r.profile_id = o.value("profile_id").toInt();
    r.context = o.value("context").toObject();
    r.plan_first = o.value("plan_first").toBool();
    const QJsonObject b = o.value("budget").toObject();
    r.budget.max_credits = b.value("max_credits").toDouble();
    r.budget.max_steps = b.value("max_steps").toInt();
    r.budget.max_minutes = b.value("max_minutes").toInt();
    return r;
}

QJsonArray steps_to_plan_json(const QVector<RunStep>& steps) {
    QJsonArray arr;
    for (const RunStep& s : steps) {
        QJsonObject o;
        o["title"] = s.title;
        o["instruction"] = s.instruction;
        if (!s.deliverable.isEmpty())
            o["deliverable"] = s.deliverable;
        arr.append(o);
    }
    return arr;
}

QVector<RunStep> steps_from_plan_json(const QString& run_id, const QJsonArray& plan) {
    QVector<RunStep> out;
    out.reserve(plan.size());
    for (const QJsonValue& v : plan) {
        const QJsonObject o = v.toObject();
        RunStep s;
        s.run_id = run_id;
        s.idx = static_cast<int>(out.size());
        s.title = o.value("title").toString();
        s.instruction = o.value("instruction").toString();
        s.deliverable = o.value("deliverable").toString();
        out.append(s);
    }
    return out;
}

QJsonObject run_event_to_json(const RunEvent& e) {
    QJsonObject o;
    o["seq"] = e.seq;
    o["ts"] = QString::number(e.ts_ms);
    o["step"] = e.step;
    o["kind"] = event_kind_id(e.kind);
    o["title"] = e.title;
    o["body"] = e.body;
    if (!e.data.isEmpty())
        o["data"] = e.data;
    if (e.is_error)
        o["is_error"] = true;
    if (!e.ref_id.isEmpty())
        o["ref_id"] = e.ref_id;
    return o;
}

RunEvent run_event_from_json(const QJsonObject& o) {
    RunEvent e;
    e.seq = o.value("seq").toInt();
    e.ts_ms = o.value("ts").toString().toLongLong();
    e.step = o.value("step").toInt(-1);
    e.kind = event_kind_from_id(o.value("kind").toString()).value_or(RunEventKind::Note);
    e.title = o.value("title").toString();
    e.body = o.value("body").toString();
    e.data = o.value("data").toObject();
    e.is_error = o.value("is_error").toBool();
    e.ref_id = o.value("ref_id").toString();
    return e;
}

} // namespace fincept::agent_tasks
