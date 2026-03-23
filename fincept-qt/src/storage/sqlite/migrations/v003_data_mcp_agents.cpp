// v003_data_mcp_agents — Data sources, MCP servers, agents, and context recording.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v003(QSqlDatabase& db) {

    // ── Data Sources ─────────────────────────────────────────────────────────
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS data_sources ("
                     "  id TEXT PRIMARY KEY,"
                     "  alias TEXT NOT NULL UNIQUE,"
                     "  display_name TEXT NOT NULL,"
                     "  description TEXT,"
                     "  type TEXT NOT NULL CHECK (type IN ('websocket', 'rest_api')),"
                     "  provider TEXT NOT NULL,"
                     "  category TEXT,"
                     "  config TEXT NOT NULL DEFAULT '{}',"
                     "  enabled INTEGER DEFAULT 1,"
                     "  tags TEXT,"
                     "  created_at TEXT DEFAULT (datetime('now')),"
                     "  updated_at TEXT DEFAULT (datetime('now'))"
                     ")");
    if (r.is_err())
        return r;

    // ── WebSocket Provider Configs ───────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS ws_provider_configs ("
                "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  provider_name TEXT NOT NULL UNIQUE,"
                "  enabled INTEGER DEFAULT 1,"
                "  api_key TEXT,"
                "  api_secret TEXT,"
                "  endpoint TEXT,"
                "  config_data TEXT,"
                "  created_at TEXT DEFAULT (datetime('now')),"
                "  updated_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    // ── Data Source Connections ───────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS data_source_connections ("
                "  id TEXT PRIMARY KEY,"
                "  name TEXT NOT NULL,"
                "  type TEXT NOT NULL,"
                "  category TEXT NOT NULL,"
                "  config TEXT NOT NULL DEFAULT '{}',"
                "  status TEXT NOT NULL DEFAULT 'disconnected',"
                "  last_tested TEXT,"
                "  error_message TEXT,"
                "  created_at TEXT DEFAULT (datetime('now')),"
                "  updated_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    // ── MCP Servers ──────────────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS mcp_servers ("
                "  id TEXT PRIMARY KEY,"
                "  name TEXT NOT NULL,"
                "  description TEXT,"
                "  command TEXT NOT NULL,"
                "  args TEXT,"
                "  env TEXT,"
                "  category TEXT,"
                "  icon TEXT,"
                "  enabled INTEGER DEFAULT 1,"
                "  auto_start INTEGER DEFAULT 0,"
                "  status TEXT DEFAULT 'stopped',"
                "  created_at TEXT DEFAULT (datetime('now')),"
                "  updated_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    // ── MCP Tools ────────────────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS mcp_tools ("
                "  id TEXT PRIMARY KEY,"
                "  server_id TEXT NOT NULL REFERENCES mcp_servers(id) ON DELETE CASCADE,"
                "  name TEXT NOT NULL,"
                "  description TEXT,"
                "  input_schema TEXT,"
                "  created_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    sql(db, "CREATE INDEX IF NOT EXISTS idx_mcp_tools_server ON mcp_tools(server_id)");

    // ── MCP Tool Usage Logs ──────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS mcp_tool_usage_logs ("
                "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  server_id TEXT NOT NULL,"
                "  tool_name TEXT NOT NULL,"
                "  args TEXT,"
                "  result TEXT,"
                "  success INTEGER,"
                "  execution_time INTEGER,"
                "  error TEXT,"
                "  timestamp TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    // ── Internal MCP Tool Settings ───────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS internal_mcp_tool_settings ("
                "  tool_name TEXT PRIMARY KEY,"
                "  category TEXT NOT NULL,"
                "  is_enabled INTEGER DEFAULT 1,"
                "  updated_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    // ── Recorded Contexts (tab data snapshots for AI) ────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS recorded_contexts ("
                "  id TEXT PRIMARY KEY,"
                "  tab_name TEXT NOT NULL,"
                "  data_type TEXT NOT NULL,"
                "  label TEXT,"
                "  raw_data TEXT NOT NULL,"
                "  metadata TEXT,"
                "  data_size INTEGER,"
                "  tags TEXT,"
                "  created_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    sql(db, "CREATE INDEX IF NOT EXISTS idx_contexts_tab ON recorded_contexts(tab_name)");

    // ── Recording Sessions ───────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS recording_sessions ("
                "  id TEXT PRIMARY KEY,"
                "  tab_name TEXT NOT NULL,"
                "  is_active INTEGER DEFAULT 1,"
                "  auto_record INTEGER DEFAULT 0,"
                "  filters TEXT,"
                "  started_at TEXT DEFAULT (datetime('now')),"
                "  ended_at TEXT"
                ")");
    if (r.is_err())
        return r;

    // ── Agent Configs ────────────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS agent_configs ("
                "  id TEXT PRIMARY KEY,"
                "  name TEXT NOT NULL,"
                "  description TEXT,"
                "  config_json TEXT NOT NULL DEFAULT '{}',"
                "  category TEXT DEFAULT 'general',"
                "  is_default INTEGER DEFAULT 0,"
                "  is_active INTEGER DEFAULT 0,"
                "  created_at TEXT DEFAULT (datetime('now')),"
                "  updated_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v003() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({3, "data_mcp_agents", apply_v003});
}
} // namespace fincept
