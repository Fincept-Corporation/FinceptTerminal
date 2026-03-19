#include "database.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace fincept::db {
namespace ops {


void save_setting(const std::string& key, const std::string& value, const std::string& category) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO settings (setting_key, setting_value, category, updated_at) "
        "VALUES (?1, ?2, ?3, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, key);
    stmt.bind_text(2, value);
    stmt.bind_text(3, category);
    stmt.execute();
}

std::optional<std::string> get_setting(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT setting_value FROM settings WHERE setting_key = ?1");
    stmt.bind_text(1, key);
    if (stmt.step()) return stmt.col_text(0);
    return std::nullopt;
}

std::vector<Setting> get_all_settings() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT setting_key, setting_value, category, updated_at FROM settings");
    std::vector<Setting> result;
    while (stmt.step()) {
        result.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3)});
    }
    return result;
}

void delete_setting(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM settings WHERE setting_key = ?1");
    stmt.bind_text(1, key);
    stmt.execute();
}

// ============================================================================
// Operations — Key-Value Storage
// ============================================================================
void storage_set(const std::string& key, const std::string& value) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO key_value_storage (key, value, updated_at) VALUES (?1, ?2, ?3)");
    stmt.bind_text(1, key);
    stmt.bind_text(2, value);
    stmt.bind_int(3, now_timestamp());
    stmt.execute();
}

std::optional<std::string> storage_get(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT value FROM key_value_storage WHERE key = ?1");
    stmt.bind_text(1, key);
    if (stmt.step()) return stmt.col_text(0);
    return std::nullopt;
}

void storage_remove(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM key_value_storage WHERE key = ?1");
    stmt.bind_text(1, key);
    stmt.execute();
}

bool storage_has(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT 1 FROM key_value_storage WHERE key = ?1 LIMIT 1");
    stmt.bind_text(1, key);
    return stmt.step();
}

std::vector<std::string> storage_keys() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT key FROM key_value_storage ORDER BY key");
    std::vector<std::string> keys;
    while (stmt.step()) keys.push_back(stmt.col_text(0));
    return keys;
}

void storage_set_many(const std::vector<std::pair<std::string, std::string>>& pairs) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    db.transaction([&]() {
        int64_t ts = now_timestamp();
        for (auto& [k, v] : pairs) {
            auto stmt = db.prepare(
                "INSERT OR REPLACE INTO key_value_storage (key, value, updated_at) VALUES (?1, ?2, ?3)");
            stmt.bind_text(1, k);
            stmt.bind_text(2, v);
            stmt.bind_int(3, ts);
            stmt.execute();
        }
        return true;
    });
}

// ============================================================================
// Operations — Credentials
// ============================================================================
void save_credential(const Credential& cred) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO credentials "
        "(service_name, username, password, api_key, api_secret, additional_data, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, cred.service_name);
    cred.username    ? stmt.bind_text(2, *cred.username)    : stmt.bind_null(2);
    cred.password    ? stmt.bind_text(3, *cred.password)    : stmt.bind_null(3);
    cred.api_key     ? stmt.bind_text(4, *cred.api_key)     : stmt.bind_null(4);
    cred.api_secret  ? stmt.bind_text(5, *cred.api_secret)  : stmt.bind_null(5);
    cred.additional_data ? stmt.bind_text(6, *cred.additional_data) : stmt.bind_null(6);
    stmt.execute();
}

std::vector<Credential> get_credentials() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, service_name, username, password, api_key, api_secret, "
        "additional_data, created_at, updated_at FROM credentials ORDER BY service_name");
    std::vector<Credential> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_int(0), stmt.col_text(1), stmt.col_text_opt(2),
            stmt.col_text_opt(3), stmt.col_text_opt(4), stmt.col_text_opt(5),
            stmt.col_text_opt(6), stmt.col_text(7), stmt.col_text(8)
        });
    }
    return result;
}

std::optional<Credential> get_credential_by_service(const std::string& service_name) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, service_name, username, password, api_key, api_secret, "
        "additional_data, created_at, updated_at FROM credentials WHERE service_name = ?1");
    stmt.bind_text(1, service_name);
    if (!stmt.step()) return std::nullopt;
    return Credential{
        stmt.col_int(0), stmt.col_text(1), stmt.col_text_opt(2),
        stmt.col_text_opt(3), stmt.col_text_opt(4), stmt.col_text_opt(5),
        stmt.col_text_opt(6), stmt.col_text(7), stmt.col_text(8)
    };
}

void delete_credential(int64_t id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM credentials WHERE id = ?1");
    stmt.bind_int(1, id);
    stmt.execute();
}

// ============================================================================
// Operations — LLM Config
// ============================================================================
std::vector<LLMConfig> get_llm_configs() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT provider, api_key, base_url, model, is_active, created_at, updated_at FROM llm_configs");
    std::vector<LLMConfig> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_text(0), stmt.col_text_opt(1), stmt.col_text_opt(2),
            stmt.col_text(3), stmt.col_bool(4), stmt.col_text(5), stmt.col_text(6)
        });
    }
    return result;
}

void save_llm_config(const LLMConfig& config) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO llm_configs (provider, api_key, base_url, model, is_active, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, config.provider);
    config.api_key  ? stmt.bind_text(2, *config.api_key)  : stmt.bind_null(2);
    config.base_url ? stmt.bind_text(3, *config.base_url) : stmt.bind_null(3);
    stmt.bind_text(4, config.model);
    stmt.bind_int(5, config.is_active ? 1 : 0);
    stmt.execute();
}

LLMGlobalSettings get_llm_global_settings() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT temperature, max_tokens, system_prompt FROM llm_global_settings WHERE id = 1");
    if (stmt.step()) {
        return {stmt.col_double(0), stmt.col_int(1), stmt.col_text(2)};
    }
    return {};
}

void save_llm_global_settings(const LLMGlobalSettings& s) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE llm_global_settings SET temperature = ?1, max_tokens = ?2, system_prompt = ?3 WHERE id = 1");
    stmt.bind_double(1, s.temperature);
    stmt.bind_int(2, s.max_tokens);
    stmt.bind_text(3, s.system_prompt);
    stmt.execute();
}

void delete_llm_config(const std::string& provider) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM llm_configs WHERE provider = ?1");
    stmt.bind_text(1, provider);
    stmt.execute();
}

void set_active_llm_provider(const std::string& provider) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    db.exec("UPDATE llm_configs SET is_active = 0");
    auto stmt = db.prepare("UPDATE llm_configs SET is_active = 1 WHERE provider = ?1");
    stmt.bind_text(1, provider);
    stmt.execute();
}

// ============================================================================

} // namespace ops
} // namespace fincept::db
