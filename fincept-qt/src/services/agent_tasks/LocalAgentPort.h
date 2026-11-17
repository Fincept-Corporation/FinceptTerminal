#pragma once
// LocalAgentPort.h — the narrow seam between LocalAgenticExecutor and the
// terminal's local agentic engine (AgentService + Python finagent_core's
// AgenticRunner). Header-only, Qt Core only, no moc: the executor is unit-tested
// against a fake engine fed with recorded AGENTIC_EVENT payloads.
//
// The app's implementation (LocalAgentEngine.cpp) forwards to AgentService's
// start_task / resume_task / reply_to_question / pause_task / cancel_task and its
// task_event signal, and checks that Agentic Mode, Python and an LLM are set up.
//
// AGENTIC_EVENT payloads (scripts/agents/finagent_core/agentic/runner.py; every
// event also carries "task_id" and AgentService adds "request_id"):
//   task_started {query}                     task_resumed {from_step, with_user_input}
//   plan_ready   {plan:{name, steps:[{id, name, config:{query}, …}]}}
//   step_start   {step_index, step, budget}  step_end {step_index, result:{response, error?, success?}, budget}
//   reflection   {step_index, decision:{decision, reason, …}}
//   replanned    {plan, reason}              question {step_index, question, reason}
//   paused       {at_step}                   cancelled {at_step}
//   budget_stop  {at_step, breach, budget}   done {final, steps_completed}
//   error        {error}                     control_ack {signal} (from pause_task / cancel_task)
//   skill_saved / skill_skipped / reusability_score (informational)
// budget = {tokens_used, max_tokens, cost_used, max_cost_usd, wall_s, max_wall_s,
// steps_used, max_steps} — counted per Python process (a resume starts at 0).

#include "services/agent_tasks/TaskRunTypes.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::agent_tasks {

/// Whether the local engine can take a task now; when not, why and where to fix it.
struct LocalAgentReadiness {
    bool ready = true;
    QString code;      ///< "agentic_mode_disabled" | "python_not_ready" | "llm_not_configured" | …
    QString message;   ///< plain language
    QString llm_label; ///< "provider · model" the agent will use (informational)
};

class ILocalAgentListener {
  public:
    /// One AGENTIC_EVENT (AgentService::task_event). `task_id` may be empty for a
    /// task that has not reported its id yet; then event["request_id"] names the
    /// start request (and a failed spawn may carry neither, synchronously).
    virtual void local_agent_event(const QString& task_id, const QJsonObject& event) = 0;

  protected:
    ~ILocalAgentListener() = default;
};

class ILocalAgentEngine {
  public:
    virtual ~ILocalAgentEngine() = default;
    virtual void set_listener(ILocalAgentListener* listener) = 0;
    virtual LocalAgentReadiness readiness() const = 0;
    /// Each returns the request id the process's events carry ("request_id").
    virtual QString start_task(const QString& query, const QJsonObject& config) = 0;
    virtual QString resume_task(const QString& task_id, const QJsonObject& config) = 0;
    virtual QString reply_to_question(const QString& task_id, const QString& answer, const QJsonObject& config) = 0;
    /// Cooperative: the running task stops at its next step boundary.
    virtual void pause_task(const QString& task_id) = 0;
    virtual void cancel_task(const QString& task_id) = 0;
    /// The run as TaskRunService holds it (answer() / cancel() after a restart), or null.
    virtual const TaskRun* find_run(const QString& run_id) const = 0;
    virtual QVector<RunStep> run_steps(const QString& run_id) const = 0;
    virtual void log_info(const QString& message) = 0;
};

// ── Payload helpers (pure; unit-tested) ──────────────────────────────────────

/// The steps of an AgenticRunner plan ({name, steps:[{name, description?,
/// config:{query}}]}), all Pending, idx from 0.
inline QVector<RunStep> local_agent_plan_steps(const QJsonObject& plan) {
    QVector<RunStep> out;
    const QJsonArray steps = plan.value(QStringLiteral("steps")).toArray();
    for (const QJsonValue& v : steps) {
        const QJsonObject s = v.toObject();
        const QString query = s.value(QStringLiteral("config")).toObject().value(QStringLiteral("query")).toString().trimmed();
        QString title = s.value(QStringLiteral("name")).toString().simplified();
        if (title.isEmpty())
            title = s.value(QStringLiteral("title")).toString().simplified();
        QString instruction = query.isEmpty() ? s.value(QStringLiteral("description")).toString().trimmed() : query;
        if (title.isEmpty())
            title = instruction.simplified().left(80);
        if (title.isEmpty())
            title = QCoreApplication::translate("fincept::agent_tasks::LocalAgenticExecutor", "Step %1")
                        .arg(out.size() + 1);
        if (instruction.isEmpty())
            instruction = title;
        RunStep r;
        r.idx = static_cast<int>(out.size());
        r.title = title.left(120);
        r.instruction = instruction.left(2000);
        out.append(r);
    }
    return out;
}

/// A step_end "result": whether it failed and the text to keep as its result.
struct LocalStepResult {
    bool failed = false;
    QString text;
};

inline LocalStepResult local_agent_step_result(const QJsonObject& result) {
    LocalStepResult r;
    const QString error = result.value(QStringLiteral("error")).toString().trimmed();
    r.failed = !error.isEmpty() || !result.value(QStringLiteral("success")).toBool(true);
    r.text = result.value(QStringLiteral("response")).toString().trimmed();
    if (r.failed && r.text.isEmpty())
        r.text = error.isEmpty() ? QCoreApplication::translate("fincept::agent_tasks::LocalAgenticExecutor",
                                                               "The step failed.")
                                 : error;
    return r;
}

/// Plain-language reason of a budget_stop {breach, budget}.
inline QString local_agent_budget_text(const QString& breach, const QJsonObject& budget) {
    const auto tr = [](const char* s) { return QCoreApplication::translate("fincept::agent_tasks::LocalAgenticExecutor", s); };
    QString what;
    if (breach == QLatin1String("tokens"))
        what = tr("its token limit (%1 of %2 tokens)")
                   .arg(static_cast<qint64>(budget.value(QStringLiteral("tokens_used")).toDouble()))
                   .arg(static_cast<qint64>(budget.value(QStringLiteral("max_tokens")).toDouble()));
    else if (breach == QLatin1String("cost"))
        what = tr("its cost limit ($%1 of $%2)")
                   .arg(QString::number(budget.value(QStringLiteral("cost_used")).toDouble(), 'f', 2),
                        QString::number(budget.value(QStringLiteral("max_cost_usd")).toDouble(), 'f', 2));
    else if (breach == QLatin1String("wall_clock"))
        what = tr("its time limit (%1 of %2 s)")
                   .arg(budget.value(QStringLiteral("wall_s")).toInt())
                   .arg(budget.value(QStringLiteral("max_wall_s")).toInt());
    else if (breach == QLatin1String("steps"))
        what = tr("its step limit (%1 of %2 steps)")
                   .arg(budget.value(QStringLiteral("steps_used")).toInt())
                   .arg(budget.value(QStringLiteral("max_steps")).toInt());
    else
        what = breach.isEmpty() ? tr("its budget") : breach;
    return tr("The local agent stopped at %1. The finished steps' results are kept.").arg(what);
}

/// Plain-language text of an "error" event's message.
inline QString local_agent_error_text(const QString& error) {
    const auto tr = [](const char* s) { return QCoreApplication::translate("fincept::agent_tasks::LocalAgenticExecutor", s); };
    const QString e = error.trimmed();
    if (e.isEmpty() || e == QLatin1String("Subprocess exited without terminal event"))
        return tr("The local agent stopped unexpectedly: its process ended without a result.");
    if (e.compare(QLatin1String("Python not available"), Qt::CaseInsensitive) == 0)
        return tr("The local agent needs the terminal's Python environment, which is not set up. Finish the setup in "
                  "Settings › Python Env, then try again.");
    if (e.startsWith(QLatin1String("Process error:")))
        return tr("The local agent could not be started (%1).").arg(e.mid(14).trimmed());
    return tr("The local agent stopped with an error: %1").arg(e.left(1500));
}

/// The machine-readable code of an "error" event's message.
inline QString local_agent_error_code(const QString& error) {
    const QString e = error.trimmed();
    if (e.compare(QLatin1String("Python not available"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("python_not_ready");
    if (e.startsWith(QLatin1String("Process error:")))
        return QStringLiteral("local_agent_spawn");
    if (e.isEmpty() || e == QLatin1String("Subprocess exited without terminal event"))
        return QStringLiteral("local_agent_exited");
    return QStringLiteral("local_agent_error");
}

} // namespace fincept::agent_tasks
