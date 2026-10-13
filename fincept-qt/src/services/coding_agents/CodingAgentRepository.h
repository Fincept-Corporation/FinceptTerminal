#pragma once
// CodingAgentRepository.h — SQLite persistence for Agent Workbench runs, their
// transcripts and recurring schedules (tables created by migration v052).
//
// Thread-agnostic: every method goes through Database::execute(), which hands
// out the CALLING thread's connection. CodingAgentService runs all writes on
// its private single-thread writer pool (so they stay ordered and off the GUI
// thread) and only cheap reads elsewhere.

#include "services/coding_agents/CodingAgentTypes.h"
#include "storage/repositories/BaseRepository.h"

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::coding_agents {

/// (run_id, event) — one row of a batched transcript write.
using RunEventRow = QPair<QString, AgentEvent>;

class CodingAgentRepository : public BaseRepository<CodingAgentRun> {
  public:
    static CodingAgentRepository& instance();

    /// Newest first.
    Result<QVector<CodingAgentRun>> load_runs(int limit) const;
    Result<void> upsert_run(const CodingAgentRun& run) const;
    /// Deletes the runs and their transcripts.
    Result<void> delete_runs(const QStringList& ids) const;
    /// Keep the newest `keep_newest` runs, delete the rest. Returns rows deleted.
    Result<int> prune_runs(int keep_newest) const;
    /// Crash recovery: runs created before `before_ms` that are still queued or
    /// running cannot be alive any more — mark them interrupted.
    Result<int> mark_unfinished_interrupted(qint64 before_ms, qint64 now_ms, const QString& note) const;

    /// Insert a batch in ONE transaction. A row that fails (its run was deleted
    /// meanwhile) is logged and skipped — it never aborts the batch. Returns rows written.
    Result<int> insert_events(const QVector<RunEventRow>& rows) const;
    /// Transcript in seq order; `last_n` > 0 keeps only the newest N events.
    Result<QVector<AgentEvent>> load_events(const QString& run_id, int last_n) const;

    Result<QVector<CodingAgentSchedule>> load_schedules() const;
    Result<void> upsert_schedule(const CodingAgentSchedule& schedule) const;
    Result<void> delete_schedule(const QString& id) const;

  private:
    CodingAgentRepository() = default;
};

} // namespace fincept::services::coding_agents
