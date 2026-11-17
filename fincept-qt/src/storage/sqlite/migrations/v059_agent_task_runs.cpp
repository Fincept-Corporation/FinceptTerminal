// v059_agent_task_runs — persistence for Chat Mode's run engine
// (services/agent_tasks: TaskRunService + TaskRunStore): long-running agentic
// finance tasks driven turn by turn against the cloud agent or by the local
// agentic engine.
//
//   agent_task_runs       one row per run (a retry-from-step is a new row that
//                         points at its parent through parent_run_id)
//   agent_task_steps      the run's plan, one row per step, primary key (run_id, idx)
//   agent_task_events     the run's activity, written in batches (text deltas are
//                         not stored — the step's final text is)
//   agent_task_artefacts  reports / tables / files a run produced
//   agent_task_schedules  recurring runs (same shape as coding_agent_schedules,
//                         plus template_id, request_json and paused_for_review)
//
// Timestamps are epoch milliseconds (UTC). parent_run_id / schedule_id are plain
// TEXT on purpose: deleting a parent run or a schedule must not delete the runs
// that reference it. Steps, events and artefacts cascade with their run.
//
// SAFE ON A POPULATED DATABASE: only CREATE TABLE / CREATE INDEX IF NOT EXISTS.
// Idempotent.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v059();

namespace {

// Uniquely named — unity builds concatenate translation units.
Result<void> sql_v059(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(QString::fromLatin1(stmt)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v059(QSqlDatabase& db) {
    static const char* const kStatements[] = {
        "CREATE TABLE IF NOT EXISTS agent_task_runs ("
        "  id             TEXT PRIMARY KEY,"
        "  title          TEXT NOT NULL DEFAULT '',"
        "  prompt         TEXT NOT NULL DEFAULT '',"
        "  engine         TEXT NOT NULL DEFAULT 'cloud',"   // cloud | local
        "  plan_first     INTEGER NOT NULL DEFAULT 0,"      // cloud: 0 = one reply, 1 = planned steps
        "  mode           TEXT NOT NULL DEFAULT '',"        // cloud: lite | deep
        "  template_id    TEXT NOT NULL DEFAULT '',"
        "  session_uuid   TEXT NOT NULL DEFAULT '',"        // cloud conversation
        "  remote_task_id TEXT NOT NULL DEFAULT '',"
        "  profile_id     INTEGER NOT NULL DEFAULT 0,"
        "  context_json   TEXT NOT NULL DEFAULT '{}',"
        "  status         TEXT NOT NULL DEFAULT 'queued',"
        "  current_step   INTEGER NOT NULL DEFAULT 0,"
        "  plan_json      TEXT NOT NULL DEFAULT '',"
        "  max_credits    REAL NOT NULL DEFAULT 0,"         // 0 = unlimited
        "  max_steps      INTEGER NOT NULL DEFAULT 0,"
        "  max_minutes    INTEGER NOT NULL DEFAULT 0,"
        "  credits_spent  REAL NOT NULL DEFAULT 0,"
        "  tokens_used    INTEGER NOT NULL DEFAULT 0,"
        "  turns          INTEGER NOT NULL DEFAULT 0,"
        "  final_text     TEXT NOT NULL DEFAULT '',"
        "  error          TEXT NOT NULL DEFAULT '',"
        "  origin         TEXT NOT NULL DEFAULT 'user',"    // chat | user | schedule | retry
        "  parent_run_id  TEXT,"
        "  schedule_id    TEXT,"
        "  created_at     INTEGER NOT NULL,"
        "  started_at     INTEGER,"
        "  finished_at    INTEGER,"
        "  updated_at     INTEGER NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_agent_task_runs_created ON agent_task_runs(created_at DESC)",
        "CREATE INDEX IF NOT EXISTS idx_agent_task_runs_status ON agent_task_runs(status)",
        "CREATE INDEX IF NOT EXISTS idx_agent_task_runs_session ON agent_task_runs(session_uuid)",

        "CREATE TABLE IF NOT EXISTS agent_task_steps ("
        "  run_id       TEXT NOT NULL REFERENCES agent_task_runs(id) ON DELETE CASCADE,"
        "  idx          INTEGER NOT NULL,"
        "  title        TEXT NOT NULL DEFAULT '',"
        "  instruction  TEXT NOT NULL DEFAULT '',"
        "  deliverable  TEXT NOT NULL DEFAULT '',"
        "  status       TEXT NOT NULL DEFAULT 'pending',"   // pending | running | done | failed | skipped
        "  summary      TEXT NOT NULL DEFAULT '',"
        "  attempts     INTEGER NOT NULL DEFAULT 0,"
        "  credits      REAL NOT NULL DEFAULT 0,"
        "  message_uuid TEXT NOT NULL DEFAULT '',"
        "  started_at   INTEGER,"
        "  finished_at  INTEGER,"
        "  PRIMARY KEY (run_id, idx)"
        ")",

        "CREATE TABLE IF NOT EXISTS agent_task_events ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id    TEXT NOT NULL REFERENCES agent_task_runs(id) ON DELETE CASCADE,"
        "  seq       INTEGER NOT NULL,"
        "  ts        INTEGER NOT NULL,"
        "  step_idx  INTEGER NOT NULL DEFAULT -1,"
        "  kind      TEXT NOT NULL,"
        "  title     TEXT NOT NULL DEFAULT '',"
        "  body      TEXT NOT NULL DEFAULT '',"
        "  data_json TEXT NOT NULL DEFAULT '',"
        "  is_error  INTEGER NOT NULL DEFAULT 0,"
        "  ref_id    TEXT NOT NULL DEFAULT ''"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_agent_task_events_run_seq ON agent_task_events(run_id, seq)",

        "CREATE TABLE IF NOT EXISTS agent_task_artefacts ("
        "  id          TEXT PRIMARY KEY,"
        "  run_id      TEXT NOT NULL REFERENCES agent_task_runs(id) ON DELETE CASCADE,"
        "  step_idx    INTEGER NOT NULL DEFAULT -1,"
        "  kind        TEXT NOT NULL DEFAULT '',"
        "  title       TEXT NOT NULL DEFAULT '',"
        "  mime        TEXT NOT NULL DEFAULT '',"
        "  stored_name TEXT NOT NULL DEFAULT '',"
        "  inline_json TEXT NOT NULL DEFAULT '',"
        "  bytes       INTEGER NOT NULL DEFAULT 0,"
        "  created_at  INTEGER NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_agent_task_artefacts_run ON agent_task_artefacts(run_id)",

        "CREATE TABLE IF NOT EXISTS agent_task_schedules ("
        "  id                TEXT PRIMARY KEY,"
        "  name              TEXT NOT NULL,"
        "  template_id       TEXT NOT NULL DEFAULT '',"
        "  prompt            TEXT NOT NULL DEFAULT '',"
        "  engine            TEXT NOT NULL DEFAULT 'cloud',"
        "  mode              TEXT NOT NULL DEFAULT '',"
        "  request_json      TEXT NOT NULL DEFAULT '{}',"
        "  max_credits       REAL NOT NULL DEFAULT 0,"
        "  kind              TEXT NOT NULL DEFAULT 'daily',"      // interval | daily
        "  interval_minutes  INTEGER NOT NULL DEFAULT 60,"
        "  daily_time        TEXT NOT NULL DEFAULT '08:00',"      // HH:mm local
        "  weekdays_mask     INTEGER NOT NULL DEFAULT 31,"        // bit0 = Monday … bit6 = Sunday
        "  enabled           INTEGER NOT NULL DEFAULT 1,"
        "  paused_for_review INTEGER NOT NULL DEFAULT 0,"
        "  last_run_at       INTEGER,"
        "  next_run_at       INTEGER,"
        "  created_at        INTEGER NOT NULL,"
        "  updated_at        INTEGER NOT NULL"
        ")",
    };
    for (const char* stmt : kStatements) {
        auto r = sql_v059(db, stmt);
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v059() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({59, "agent_task_runs", apply_v059});
}

} // namespace fincept
