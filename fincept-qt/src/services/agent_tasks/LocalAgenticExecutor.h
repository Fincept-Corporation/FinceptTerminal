#pragma once
// LocalAgenticExecutor.h — runs a task on the terminal's local agentic engine
// (WP4). engine_id() "local".
//
// Wraps AgentService's task API (start_task / resume_task / pause_task /
// cancel_task / reply_to_question) and its task_event stream, mapping the
// AGENTIC_EVENT kinds (plan_ready, step_start, step_end, question, paused,
// budget_stop, done, error …) to run events. Steps are opaque (no tokens or tool
// calls inside a step) and use the user's own LLM profile, not Fincept credits.
// Python checkpoints per step, so resume = AgentService::resume_task. An
// unattended run (TaskRun::is_unattended()) is paper-only. Does not edit
// src/services/agents.
//
// AgentService is reached through the ILocalAgentEngine seam (LocalAgentPort.h) so
// the mapping is unit-tested with a fake engine. The default constructor (the
// app's, AgentService-backed) is defined in LocalAgentEngine.cpp; this class's own
// .cpp is Qt Core only.
//
// Registered by TaskRunService::initialize_deferred().

#include "services/agent_tasks/IRunExecutor.h"
#include "services/agent_tasks/LocalAgentPort.h"

#include <memory>

namespace fincept::agent_tasks {

class LocalAgenticExecutor : public IRunExecutor {
    Q_OBJECT
  public:
    explicit LocalAgenticExecutor(QObject* parent = nullptr);
    /// WP4 (additive): drives `engine` instead of AgentService (unit tests pass a fake).
    explicit LocalAgenticExecutor(std::unique_ptr<ILocalAgentEngine> engine, QObject* parent = nullptr);
    ~LocalAgenticExecutor() override;

    QString engine_id() const override;
    void start(const TaskRun& run, const QVector<RunStep>& steps) override;
    void answer(const QString& run_id, const QString& text) override;
    void request_pause(const QString& run_id) override;
    void cancel(const QString& run_id) override;
    bool can_pause(const QString& run_id) const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::agent_tasks
