// v052_coding_agents — persistence for the Agent Workbench (local Claude Code /
// OpenAI Codex CLI runs wired to the terminal's tools over MCP).
//
//   coding_agent_runs       one row per CLI invocation (a follow-up is a new row
//                           pointing at its parent through parent_run_id)
//   coding_agent_events     the run transcript, written in batches; cascades
//                           with its run
//   coding_agent_schedules  recurring briefs (interval or daily HH:mm + weekdays)
//
// Timestamps are epoch milliseconds (UTC) so ordering and "missed by < 12 h"
// arithmetic need no string parsing. parent_run_id / schedule_id are plain
// TEXT on purpose: deleting a parent run or a schedule must not delete the
// runs that reference it.
//
// SAFE ON A POPULATED DATABASE: only CREATE TABLE / CREATE INDEX IF NOT EXISTS.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

// Uniquely named — unity builds concatenate translation units.
Result<void> sql_v052(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(QString::fromLatin1(stmt)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v052(QSqlDatabase& db) {
    static const char* const kStatements[] = {
        "CREATE TABLE IF NOT EXISTS coding_agent_runs ("
        "  id                TEXT PRIMARY KEY,"
        "  title             TEXT NOT NULL DEFAULT '',"
        "  agent             TEXT NOT NULL,"                      // claude | codex
        "  prompt            TEXT NOT NULL DEFAULT '',"
        "  model             TEXT NOT NULL DEFAULT '',"
        "  mode              TEXT NOT NULL DEFAULT 'analysis',"   // analysis | workspace_edits
        "  workdir           TEXT NOT NULL DEFAULT '',"
        "  workdir_auto      INTEGER NOT NULL DEFAULT 1,"
        "  use_fincept_tools INTEGER NOT NULL DEFAULT 1,"
        "  max_budget_usd    REAL,"                               // NULL = no cap
        "  status            TEXT NOT NULL DEFAULT 'queued',"
        "  created_at        INTEGER NOT NULL,"
        "  started_at        INTEGER,"
        "  finished_at       INTEGER,"
        "  session_id        TEXT NOT NULL DEFAULT '',"           // claude session_id / codex thread_id
        "  exit_code         INTEGER,"
        "  cost_usd          REAL,"
        "  input_tokens      INTEGER NOT NULL DEFAULT 0,"
        "  output_tokens     INTEGER NOT NULL DEFAULT 0,"
        "  final_text        TEXT NOT NULL DEFAULT '',"
        "  error             TEXT NOT NULL DEFAULT '',"
        "  parent_run_id     TEXT,"
        "  schedule_id       TEXT,"
        "  origin            TEXT NOT NULL DEFAULT 'user'"         // user | schedule | mcp | follow_up
        ")",
        "CREATE INDEX IF NOT EXISTS idx_coding_agent_runs_created ON coding_agent_runs(created_at DESC)",
        "CREATE INDEX IF NOT EXISTS idx_coding_agent_runs_status ON coding_agent_runs(status)",

        "CREATE TABLE IF NOT EXISTS coding_agent_events ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id    TEXT NOT NULL REFERENCES coding_agent_runs(id) ON DELETE CASCADE,"
        "  seq       INTEGER NOT NULL,"
        "  ts        INTEGER NOT NULL,"
        "  kind      TEXT NOT NULL,"
        "  title     TEXT NOT NULL DEFAULT '',"
        "  body      TEXT NOT NULL DEFAULT '',"
        "  raw_json  TEXT NOT NULL DEFAULT '',"
        "  is_error  INTEGER NOT NULL DEFAULT 0,"
        "  ref_id    TEXT NOT NULL DEFAULT ''"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_coding_agent_events_run_seq ON coding_agent_events(run_id, seq)",

        "CREATE TABLE IF NOT EXISTS coding_agent_schedules ("
        "  id                TEXT PRIMARY KEY,"
        "  name              TEXT NOT NULL,"
        "  agent             TEXT NOT NULL,"
        "  prompt            TEXT NOT NULL,"
        "  model             TEXT NOT NULL DEFAULT '',"
        "  mode              TEXT NOT NULL DEFAULT 'analysis',"
        "  use_fincept_tools INTEGER NOT NULL DEFAULT 1,"
        "  max_budget_usd    REAL,"
        "  kind              TEXT NOT NULL DEFAULT 'daily',"      // interval | daily
        "  interval_minutes  INTEGER NOT NULL DEFAULT 60,"
        "  daily_time        TEXT NOT NULL DEFAULT '08:00',"      // HH:mm local
        "  weekdays_mask     INTEGER NOT NULL DEFAULT 31,"        // bit0 = Monday … bit6 = Sunday
        "  enabled           INTEGER NOT NULL DEFAULT 1,"
        "  last_run_at       INTEGER,"
        "  next_run_at       INTEGER,"
        "  created_at        INTEGER NOT NULL,"
        "  updated_at        INTEGER NOT NULL"
        ")",
    };
    for (const char* stmt : kStatements) {
        auto r = sql_v052(db, stmt);
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v052() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({52, "coding_agents", apply_v052});
}

} // namespace fincept
