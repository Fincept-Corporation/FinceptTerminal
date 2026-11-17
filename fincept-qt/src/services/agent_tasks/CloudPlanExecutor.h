#pragma once
// CloudPlanExecutor.h — a planned multi-step cloud task (WP3). engine_id() "cloud_plan".
//
//   1. A plan turn asks the cloud agent for a fenced JSON plan
//      {"steps":[{title, instruction, deliverable}]} (RunPlanPrompts.h); a reply
//      that does not parse becomes a single step carrying the whole goal.
//   2. One /chat/agent/stream turn per step, all in the run's conversation, so the
//      server's checkpoint carries the context from step to step.
//   3. A final synthesis turn whose answer is the run's final text.
//   Budget is checked between turns; request_pause() takes effect at a step
//   boundary; a step interrupted by an app quit is resumed by adopting the stored
//   answer (or re-sending the step); WaitingInput goes through resume_turn with a
//   fallback to an ordinary message. Estimated cost: (steps + 2) × 5 credits.
//
// Every turn is a CloudTurnDriver (shared with CloudTurnExecutor), reached through
// the ICloudBackend seam (CloudTurnPort.h) so the state machine is unit-tested
// with a fake backend. The default constructor (the app's, network backend) is
// defined in CloudTurnDriver.cpp; this class's own .cpp is Qt Core only.
//
// Registered by TaskRunService::initialize_deferred().

#include "services/agent_tasks/CloudTurnPort.h"
#include "services/agent_tasks/IRunExecutor.h"

#include <memory>

namespace fincept::agent_tasks {

class CloudPlanExecutor : public IRunExecutor {
    Q_OBJECT
  public:
    explicit CloudPlanExecutor(QObject* parent = nullptr);
    /// WP3 (additive): runs its turns through `backend` (unit tests pass a fake).
    explicit CloudPlanExecutor(std::unique_ptr<ICloudBackend> backend, QObject* parent = nullptr);
    ~CloudPlanExecutor() override;

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
