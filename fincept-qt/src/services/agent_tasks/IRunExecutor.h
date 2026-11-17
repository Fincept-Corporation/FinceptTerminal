#pragma once
// IRunExecutor.h — what TaskRunService needs from an engine that executes runs.
//
// One executor instance serves every run of its engine_id() (see executor_for()
// in TaskRunTypes.h). TaskRunService owns the instances (register_executor()
// re-parents them), calls them on the GUI thread, and records everything they
// signal: it assigns event seq / timestamps, persists, applies budgets and moves
// the run's state. An executor never writes to the database itself.
//
// Contract
//   * start(run, steps): begin or resume `run`. A resume starts at
//     run.current_step; `steps` holds the steps known so far (Done ones keep
//     their summary). Never block: report progress through the signals.
//   * Every run that start() accepted ends with exactly one finished() — or
//     parks in waiting() until answer() / start() continues it.
//   * answer(): the user's reply while the run is WaitingInput.
//   * request_pause(): take effect at the next step boundary, then
//     waiting(run, RunStatus::Paused, {}). can_pause() says whether it can.
//   * cancel(): stop as far as the engine can. Cloud turns cannot be cancelled on
//     the server: they detach and recover the stored answer instead.
//   * usage(): AMOUNTS ADDED since the previous usage() of that run (never
//     negative) — credits charged and tokens used.
//   * All signals carry the run id: several runs (and windows) share one executor.
//
// Event conventions TaskRunService understands
//   * Status event whose data carries "session_uuid" / "remote_task_id": binds
//     that id to the run (first value wins).
//   * Usage event with data "turn_end": true counts one agent turn (TaskRun::turns).
//   * data "verbose": true marks detail rows a transcript may leave out.
//   * Error event data "code": machine-readable reason ("insufficient_credits",
//     "not_waiting", "agent_busy", …).

#include "services/agent_tasks/TaskRunTypes.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace fincept::agent_tasks {

class IRunExecutor : public QObject {
    Q_OBJECT
  public:
    explicit IRunExecutor(QObject* parent = nullptr) : QObject(parent) {}
    ~IRunExecutor() override = default;

    virtual QString engine_id() const = 0;
    virtual void start(const TaskRun& run, const QVector<RunStep>& steps) = 0; // resume = start at run.current_step
    virtual void answer(const QString& run_id, const QString& text) = 0;
    virtual void request_pause(const QString& run_id) = 0;
    virtual void cancel(const QString& run_id) = 0;
    /// Whether request_pause() can take effect for `run_id` (a single cloud reply
    /// cannot pause). Not in the plan's interface block; added by WP1.
    virtual bool can_pause(const QString& run_id) const {
        Q_UNUSED(run_id)
        return true;
    }

  signals:
    // This signal shares its name with QObject::event(QEvent*), which clang and GCC report
    // as hiding the virtual (-Woverloaded-virtual, an error in CI). The name is the plan's
    // interface and is connected by pointer in several places (a `using QObject::event;`
    // would make `&IRunExecutor::event` ambiguous there), so the warning is switched off
    // for this one declaration.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif
    void event(const QString& run_id, const fincept::agent_tasks::RunEvent& ev);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    void text_delta(const QString& run_id, int step, const QString& chunk);
    void plan_ready(const QString& run_id, const QVector<fincept::agent_tasks::RunStep>& steps);
    void step_changed(const QString& run_id, int idx, fincept::agent_tasks::StepStatus s, const QString& summary);
    void usage(const QString& run_id, double credits, int tokens);
    void waiting(const QString& run_id, fincept::agent_tasks::RunStatus why, const QJsonObject& payload);
    void finished(const QString& run_id, fincept::agent_tasks::RunStatus terminal, const QString& final_text,
                  const QString& error);
};

} // namespace fincept::agent_tasks
