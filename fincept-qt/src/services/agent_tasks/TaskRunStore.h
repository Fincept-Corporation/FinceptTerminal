#pragma once
// TaskRunStore.h — SQLite persistence of runs, steps, events, artefacts,
// templates and schedules (agent_task_* tables, migration v059; artefacts,
// templates and the added run / schedule columns, migration v063).
//
// Thread-agnostic: every method goes through Database::execute(), which uses the
// CALLING thread's connection. TaskRunService runs all writes (and its loads) on
// its private single-thread writer pool, so they stay ordered — a run row is
// always written before its steps and events — and never touch the GUI thread.
// Never call these on the GUI thread.

#include "services/agent_tasks/TaskRunTypes.h"
#include "storage/repositories/BaseRepository.h"

#include <QHash>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::agent_tasks {

/// (run_id, event) — one row of a batched event write.
using RunEventRow = QPair<QString, RunEvent>;

/// A user task template as stored (agent_run_templates, v063): the template itself
/// is JSON (RunTemplates.h run_template_to_json / run_template_from_json).
struct StoredRunTemplate {
    QString id;
    QString title;
    QJsonObject json;
    qint64 created_at_ms = 0;
    qint64 updated_at_ms = 0;
};

class TaskRunStore : public BaseRepository<TaskRun> {
  public:
    static TaskRunStore& instance();

    // ── Runs ─────────────────────────────────────────────────────────────────
    /// Newest first.
    Result<QVector<TaskRun>> load_runs(int limit) const;
    Result<void> upsert_run(const TaskRun& run) const;
    /// Deletes the runs with their steps, events and artefact rows.
    Result<void> delete_runs(const QStringList& ids) const;
    /// Keep the newest `keep_newest` runs, delete the rest. Returns runs deleted.
    Result<int> prune_runs(int keep_newest) const;
    /// Restart recovery: runs created before `before_ms` that were Queued /
    /// Planning / Running / WaitingApproval cannot be alive any more — mark them
    /// Interrupted (status_after_restart()). WaitingInput and Paused stay.
    Result<int> mark_unfinished_interrupted(qint64 before_ms, qint64 now_ms, const QString& note) const;

    // ── Steps ────────────────────────────────────────────────────────────────
    /// Steps of the newest `run_limit` runs, ordered by run then index.
    Result<QVector<RunStep>> load_steps(int run_limit) const;
    /// Replace all steps of `run_id` (a new or re-made plan) in one transaction.
    Result<void> replace_steps(const QString& run_id, const QVector<RunStep>& steps) const;
    Result<void> upsert_step(const RunStep& step) const;

    // ── Events ───────────────────────────────────────────────────────────────
    /// Insert a batch in ONE transaction. A row that fails (its run was deleted
    /// meanwhile) is skipped, never aborting the batch. Returns rows written.
    Result<int> insert_events(const QVector<RunEventRow>& rows) const;
    /// Events in seq order; `last_n` > 0 keeps only the newest N.
    Result<QVector<RunEvent>> load_events(const QString& run_id, int last_n) const;
    /// run_id -> highest event seq, for the newest `run_limit` runs that have events.
    Result<QHash<QString, int>> load_max_event_seq(int run_limit) const;

    // ── Artefacts (agent_run_artefacts, v063) ────────────────────────────────
    /// Every artefact row, oldest first — pinned ones of deleted runs and orphans
    /// included (ArtefactService sweeps those). `run_limit` is not used any more.
    Result<QVector<RunArtefact>> load_artefacts(int run_limit) const;
    Result<void> upsert_artefact(const RunArtefact& artefact) const;
    Result<void> delete_artefact(const QString& id) const;
    Result<void> delete_artefacts(const QStringList& ids) const;

    // ── Templates (agent_run_templates, v063) ────────────────────────────────
    Result<QVector<StoredRunTemplate>> load_templates() const;
    Result<void> upsert_template(const StoredRunTemplate& t) const;
    Result<void> delete_template(const QString& id) const;

    // ── Schedules ────────────────────────────────────────────────────────────
    Result<QVector<RunSchedule>> load_schedules() const;
    Result<void> upsert_schedule(const RunSchedule& schedule) const;
    Result<void> delete_schedule(const QString& id) const;

  private:
    TaskRunStore() = default;
};

} // namespace fincept::agent_tasks
