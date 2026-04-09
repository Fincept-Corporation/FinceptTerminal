// v010_llm_profiles — LLM Profile system for multi-provider support.
//
// Introduces two new tables:
//   llm_profiles            — named, reusable LLM configurations
//   llm_profile_assignments — maps a profile to a specific context (ai_chat,
//                             agent:<id>, team:<id>) or a context type default
//
// On first run, creates a default profile from the currently active provider
// so existing users see no behaviour change.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql_v010(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v010(QSqlDatabase& db) {

    // ── llm_profiles ──────────────────────────────────────────────────────────
    // A named, portable LLM configuration.  Decoupled from the raw provider
    // record so users can have "Fast Groq", "Careful Claude", "Coding minimax"
    // all referencing different providers / models.
    auto r = sql_v010(db,
                      "CREATE TABLE IF NOT EXISTS llm_profiles ("
                      "  id               TEXT    PRIMARY KEY,"         // UUID
                      "  name             TEXT    NOT NULL UNIQUE,"     // human label
                      "  provider         TEXT    NOT NULL,"            // fk → llm_configs.provider
                      "  model_id         TEXT    NOT NULL,"            // e.g. claude-3-5-sonnet-20241022
                      "  api_key          TEXT    NOT NULL DEFAULT ''," // copied at save time for portability
                      "  base_url         TEXT    NOT NULL DEFAULT ''," // custom endpoint (minimax etc.)
                      "  temperature      REAL    NOT NULL DEFAULT 0.7,"
                      "  max_tokens       INTEGER NOT NULL DEFAULT 4096,"
                      "  system_prompt    TEXT    NOT NULL DEFAULT ''," // optional override
                      "  is_default       INTEGER NOT NULL DEFAULT 0,"  // one global default
                      "  created_at       TEXT    DEFAULT (datetime('now')),"
                      "  updated_at       TEXT    DEFAULT (datetime('now'))"
                      ")");
    if (r.is_err())
        return r;

    // Only one row may be the global default
    r = sql_v010(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_llm_profiles_default "
                     "ON llm_profiles(is_default) WHERE is_default = 1");
    if (r.is_err())
        return r;

    // ── llm_profile_assignments ───────────────────────────────────────────────
    // Maps a profile to a context.
    //
    // context_type values:
    //   "ai_chat"          — the AI Chat tab
    //   "agent_default"    — default for all agents that have no specific assignment
    //   "team_default"     — default for all teams that have no specific assignment
    //   "agent"            — specific agent (context_id = agent id)
    //   "team"             — specific team  (context_id = team id)
    //   "team_coordinator" — coordinator role within a specific team
    //
    // context_id is NULL for type-level defaults, non-NULL for entity-specific.
    r = sql_v010(db,
                 "CREATE TABLE IF NOT EXISTS llm_profile_assignments ("
                 "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "  context_type     TEXT    NOT NULL,"
                 "  context_id       TEXT," // NULL = type-level default
                 "  profile_id       TEXT    NOT NULL"
                 "    REFERENCES llm_profiles(id) ON DELETE CASCADE,"
                 "  created_at       TEXT    DEFAULT (datetime('now')),"
                 "  updated_at       TEXT    DEFAULT (datetime('now')),"
                 "  UNIQUE(context_type, context_id)" // one profile per context slot
                 ")");
    if (r.is_err())
        return r;

    r = sql_v010(db, "CREATE INDEX IF NOT EXISTS idx_llm_assignments_context "
                     "ON llm_profile_assignments(context_type, context_id)");
    if (r.is_err())
        return r;

    // ── Seed: create default profile from active provider ────────────────────
    // If the user has an active provider already configured, create a "Default"
    // profile from it so the upgrade is transparent.
    {
        QSqlQuery q(db);
        q.prepare("INSERT OR IGNORE INTO llm_profiles "
                  "  (id, name, provider, model_id, api_key, base_url, temperature, max_tokens, is_default) "
                  "SELECT "
                  "  lower(hex(randomblob(16))), " // UUID-like id
                  "  'Default', "
                  "  c.provider, "
                  "  COALESCE(NULLIF(c.model,''), 'gpt-4o'), "
                  "  COALESCE(c.api_key, ''), "
                  "  COALESCE(c.base_url, ''), "
                  "  COALESCE(gs.temperature, 0.7), "
                  "  COALESCE(gs.max_tokens, 4096), "
                  "  1 "
                  "FROM llm_configs c "
                  "LEFT JOIN llm_global_settings gs ON gs.id = 1 "
                  "WHERE c.is_active = 1 "
                  "LIMIT 1");
        if (!q.exec()) {
            // Non-fatal: no active provider yet (fresh install)
        }
    }

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v010() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({10, "llm_profiles", apply_v010});
}

} // namespace fincept
