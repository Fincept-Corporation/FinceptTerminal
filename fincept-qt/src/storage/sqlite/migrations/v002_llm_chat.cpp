// v002_llm_chat — LLM configurations and chat persistence tables.

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

Result<void> apply_v002(QSqlDatabase& db) {

    // ── LLM Provider Configs ─────────────────────────────────────────────────
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS llm_configs ("
                     "  provider TEXT PRIMARY KEY,"
                     "  api_key TEXT,"
                     "  base_url TEXT,"
                     "  model TEXT NOT NULL,"
                     "  is_active INTEGER DEFAULT 0,"
                     "  created_at TEXT DEFAULT (datetime('now')),"
                     "  updated_at TEXT DEFAULT (datetime('now'))"
                     ")");
    if (r.is_err())
        return r;

    // Insert default Fincept LLM
    sql(db, "INSERT OR IGNORE INTO llm_configs (provider, api_key, base_url, model, is_active) "
            "VALUES ('fincept', '', 'https://api.fincept.in/research/llm', 'fincept-llm', 1)");

    // ── LLM Global Settings (singleton row) ──────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS llm_global_settings ("
                "  id INTEGER PRIMARY KEY CHECK (id = 1),"
                "  temperature REAL DEFAULT 0.7,"
                "  max_tokens INTEGER DEFAULT 2000,"
                "  system_prompt TEXT DEFAULT ''"
                ")");
    if (r.is_err())
        return r;

    sql(db, "INSERT OR IGNORE INTO llm_global_settings (id, temperature, max_tokens, system_prompt) "
            "VALUES (1, 0.7, 4096, "
            "'You are a helpful AI assistant specialized in financial analysis and market data.')");

    // ── LLM Custom Model Configs ─────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS llm_model_configs ("
                "  id TEXT PRIMARY KEY,"
                "  provider TEXT NOT NULL,"
                "  model_id TEXT NOT NULL,"
                "  display_name TEXT NOT NULL,"
                "  api_key TEXT,"
                "  base_url TEXT,"
                "  is_enabled INTEGER DEFAULT 1,"
                "  is_default INTEGER DEFAULT 0,"
                "  created_at TEXT DEFAULT (datetime('now')),"
                "  updated_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    sql(db, "CREATE INDEX IF NOT EXISTS idx_llm_models_provider ON llm_model_configs(provider)");

    // ── Chat Sessions ────────────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS chat_sessions ("
                "  id TEXT PRIMARY KEY,"
                "  title TEXT NOT NULL,"
                "  provider TEXT,"
                "  model TEXT,"
                "  message_count INTEGER DEFAULT 0,"
                "  created_at TEXT DEFAULT (datetime('now')),"
                "  updated_at TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    // ── Chat Messages ────────────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS chat_messages ("
                "  id TEXT PRIMARY KEY,"
                "  session_id TEXT NOT NULL REFERENCES chat_sessions(id) ON DELETE CASCADE,"
                "  role TEXT NOT NULL CHECK (role IN ('user', 'assistant', 'system')),"
                "  content TEXT NOT NULL,"
                "  provider TEXT,"
                "  model TEXT,"
                "  tokens_used INTEGER,"
                "  timestamp TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    sql(db, "CREATE INDEX IF NOT EXISTS idx_chat_msg_session ON chat_messages(session_id)");

    // ── Chat Context Links (link recorded contexts to chat sessions) ─────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS chat_context_links ("
                "  session_id TEXT NOT NULL REFERENCES chat_sessions(id) ON DELETE CASCADE,"
                "  context_id TEXT NOT NULL,"
                "  is_active INTEGER DEFAULT 1,"
                "  added_at TEXT DEFAULT (datetime('now')),"
                "  PRIMARY KEY (session_id, context_id)"
                ")");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v002() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({2, "llm_chat", apply_v002});
}
} // namespace fincept
