#include "database.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace fincept::db {
namespace ops {

// Operations — Tab Sessions
// ============================================================================
std::optional<TabSession> tab_session_get(const std::string& tab_id) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT tab_id, tab_name, state, scroll_position, active_filters, "
        "selected_items, updated_at, created_at FROM tab_sessions WHERE tab_id = ?1");
    stmt.bind_text(1, tab_id);
    if (!stmt.step()) return std::nullopt;
    return TabSession{
        stmt.col_text(0), stmt.col_text(1), stmt.col_text(2),
        stmt.col_text_opt(3), stmt.col_text_opt(4), stmt.col_text_opt(5),
        stmt.col_int(6), stmt.col_int(7)
    };
}

void tab_session_set(const std::string& tab_id, const std::string& tab_name,
                     const std::string& state, const std::string& scroll_position,
                     const std::string& active_filters, const std::string& selected_items) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t now = now_timestamp();
    auto stmt = db.prepare(
        "INSERT INTO tab_sessions (tab_id, tab_name, state, scroll_position, active_filters, selected_items, updated_at, created_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?7) "
        "ON CONFLICT(tab_id) DO UPDATE SET "
        "tab_name = ?2, state = ?3, scroll_position = ?4, active_filters = ?5, selected_items = ?6, updated_at = ?7");
    stmt.bind_text(1, tab_id);
    stmt.bind_text(2, tab_name);
    stmt.bind_text(3, state);
    stmt.bind_text(4, scroll_position);
    stmt.bind_text(5, active_filters);
    stmt.bind_text(6, selected_items);
    stmt.bind_int(7, now);
    stmt.execute();
}

void tab_session_delete(const std::string& tab_id) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM tab_sessions WHERE tab_id = ?1");
    stmt.bind_text(1, tab_id);
    stmt.execute();
}

std::vector<TabSession> tab_session_get_all() {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT tab_id, tab_name, state, scroll_position, active_filters, "
        "selected_items, updated_at, created_at FROM tab_sessions ORDER BY updated_at DESC");
    std::vector<TabSession> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_text(0), stmt.col_text(1), stmt.col_text(2),
            stmt.col_text_opt(3), stmt.col_text_opt(4), stmt.col_text_opt(5),
            stmt.col_int(6), stmt.col_int(7)
        });
    }
    return result;
}

int64_t tab_session_cleanup(int64_t max_age_days) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t cutoff = now_timestamp() - (max_age_days * 24 * 60 * 60);
    int64_t count = 0;
    {
        auto cnt = db.prepare("SELECT COUNT(*) FROM tab_sessions WHERE updated_at < ?1");
        cnt.bind_int(1, cutoff);
        if (cnt.step()) count = cnt.col_int(0);
    }
    auto stmt = db.prepare("DELETE FROM tab_sessions WHERE updated_at < ?1");
    stmt.bind_int(1, cutoff);
    stmt.execute();
    return count;
}

// ============================================================================
// Agent Configs
// ============================================================================
void save_agent_config(const AgentConfigRow& config) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO agent_configs (id, name, description, category, config_json, "
        "created_at, updated_at) VALUES (?1, ?2, ?3, ?4, ?5, "
        "COALESCE((SELECT created_at FROM agent_configs WHERE id = ?1), datetime('now')), "
        "datetime('now'))");
    stmt.bind_text(1, config.id);
    stmt.bind_text(2, config.name);
    stmt.bind_text(3, config.description);
    stmt.bind_text(4, config.category);
    stmt.bind_text(5, config.config_json);
    stmt.execute();
}

std::vector<AgentConfigRow> get_agent_configs() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, category, config_json, created_at, updated_at "
        "FROM agent_configs ORDER BY name");
    std::vector<AgentConfigRow> result;
    while (stmt.step()) {
        AgentConfigRow row;
        row.id = stmt.col_text(0);
        row.name = stmt.col_text(1);
        row.description = stmt.col_text(2);
        row.category = stmt.col_text(3);
        row.config_json = stmt.col_text(4);
        row.created_at = stmt.col_text(5);
        row.updated_at = stmt.col_text(6);
        result.push_back(std::move(row));
    }
    return result;
}

std::optional<AgentConfigRow> get_agent_config(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, category, config_json, created_at, updated_at "
        "FROM agent_configs WHERE id = ?1");
    stmt.bind_text(1, id);
    if (stmt.step()) {
        AgentConfigRow row;
        row.id = stmt.col_text(0);
        row.name = stmt.col_text(1);
        row.description = stmt.col_text(2);
        row.category = stmt.col_text(3);
        row.config_json = stmt.col_text(4);
        row.created_at = stmt.col_text(5);
        row.updated_at = stmt.col_text(6);
        return row;
    }
    return std::nullopt;
}

void delete_agent_config(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM agent_configs WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// --- LLM Model Configs ---

std::vector<LLMModelConfig> get_llm_model_configs() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, provider, model_id, display_name, api_key, base_url, "
        "is_enabled, is_default, created_at, updated_at FROM llm_model_configs");
    std::vector<LLMModelConfig> result;
    while (stmt.step()) {
        LLMModelConfig cfg;
        cfg.id = stmt.col_text(0);
        cfg.provider = stmt.col_text(1);
        cfg.model_id = stmt.col_text(2);
        cfg.display_name = stmt.col_text(3);
        cfg.api_key = stmt.col_text_opt(4);
        cfg.base_url = stmt.col_text_opt(5);
        cfg.is_enabled = stmt.col_bool(6);
        cfg.is_default = stmt.col_bool(7);
        cfg.created_at = stmt.col_text(8);
        cfg.updated_at = stmt.col_text(9);
        result.push_back(std::move(cfg));
    }
    return result;
}

void save_llm_model_config(const LLMModelConfig& config) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO llm_model_configs "
        "(id, provider, model_id, display_name, api_key, base_url, is_enabled, is_default, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, config.id);
    stmt.bind_text(2, config.provider);
    stmt.bind_text(3, config.model_id);
    stmt.bind_text(4, config.display_name);
    if (config.api_key) stmt.bind_text(5, *config.api_key); else stmt.bind_null(5);
    if (config.base_url) stmt.bind_text(6, *config.base_url); else stmt.bind_null(6);
    stmt.bind_int(7, config.is_enabled ? 1 : 0);
    stmt.bind_int(8, config.is_default ? 1 : 0);
    stmt.execute();
}

void delete_llm_model_config(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM llm_model_configs WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

void toggle_llm_model_config_enabled(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE llm_model_configs SET is_enabled = NOT is_enabled, "
        "updated_at = CURRENT_TIMESTAMP WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// --- Data Sources ---

void save_data_source(const DataSource& source) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO data_sources "
        "(id, alias, display_name, description, type, provider, category, config, enabled, tags, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, source.id);
    stmt.bind_text(2, source.alias);
    stmt.bind_text(3, source.display_name);
    if (source.description) stmt.bind_text(4, *source.description); else stmt.bind_null(4);
    stmt.bind_text(5, source.type);
    stmt.bind_text(6, source.provider);
    if (source.category) stmt.bind_text(7, *source.category); else stmt.bind_null(7);
    stmt.bind_text(8, source.config);
    stmt.bind_int(9, source.enabled ? 1 : 0);
    if (source.tags) stmt.bind_text(10, *source.tags); else stmt.bind_null(10);
    stmt.execute();
}

std::vector<DataSource> get_all_data_sources() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, alias, display_name, description, type, provider, "
        "category, config, enabled, tags, created_at, updated_at FROM data_sources");
    std::vector<DataSource> result;
    while (stmt.step()) {
        DataSource ds;
        ds.id = stmt.col_text(0);
        ds.alias = stmt.col_text(1);
        ds.display_name = stmt.col_text(2);
        ds.description = stmt.col_text_opt(3);
        ds.type = stmt.col_text(4);
        ds.provider = stmt.col_text(5);
        ds.category = stmt.col_text_opt(6);
        ds.config = stmt.col_text(7);
        ds.enabled = stmt.col_bool(8);
        ds.tags = stmt.col_text_opt(9);
        ds.created_at = stmt.col_text(10);
        ds.updated_at = stmt.col_text(11);
        result.push_back(std::move(ds));
    }
    return result;
}

void delete_data_source(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM data_sources WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// ============================================================================

} // namespace ops
} // namespace fincept::db
