// MCPManager — External MCP server lifecycle management

#include "mcp_manager.h"
#include "../core/logger.h"
#include "../storage/database.h"
#include <algorithm>

namespace fincept::mcp {

static constexpr const char* TAG_MANAGER = "MCPManager";

MCPManager& MCPManager::instance() {
    static MCPManager s;
    return s;
}

// ============================================================================
// Initialization
// ============================================================================

void MCPManager::initialize() {
    create_mcp_schema();
    load_configs_from_db();
    LOG_INFO(TAG_MANAGER, "Loaded %zu MCP server configs", configs_.size());
}

void MCPManager::shutdown() {
    stop_health_check();
    stop_all();
    LOG_INFO(TAG_MANAGER, "MCPManager shut down");
}

// ============================================================================
// Server Configuration
// ============================================================================

Result<void> MCPManager::save_server(const MCPServerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    configs_[config.id] = config;
    save_config_to_db(config);
    LOG_INFO(TAG_MANAGER, "Saved server config: %s", config.name.c_str());
    return {};
}

Result<void> MCPManager::remove_server(const std::string& server_id) {
    // Stop if running
    stop_server(server_id);

    std::lock_guard<std::mutex> lock(mutex_);
    configs_.erase(server_id);
    tool_cache_.erase(server_id);
    restart_info_.erase(server_id);
    delete_config_from_db(server_id);
    LOG_INFO(TAG_MANAGER, "Removed server: %s", server_id.c_str());
    return {};
}

std::vector<MCPServerConfig> MCPManager::get_servers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MCPServerConfig> result;
    result.reserve(configs_.size());
    for (const auto& [id, cfg] : configs_) {
        auto copy = cfg;
        // Update runtime status
        auto it = clients_.find(id);
        copy.status = (it != clients_.end() && it->second->is_running())
            ? ServerStatus::Running : ServerStatus::Stopped;
        result.push_back(std::move(copy));
    }
    return result;
}

std::optional<MCPServerConfig> MCPManager::get_server(const std::string& server_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = configs_.find(server_id);
    if (it == configs_.end()) return std::nullopt;
    return it->second;
}

// ============================================================================
// Server Lifecycle
// ============================================================================

Result<void> MCPManager::start_server(const std::string& server_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto cfg_it = configs_.find(server_id);
    if (cfg_it == configs_.end()) return Error("Server not found: " + server_id);

    // Already running?
    auto client_it = clients_.find(server_id);
    if (client_it != clients_.end() && client_it->second->is_running()) {
        return {}; // Already running
    }

    auto client = std::make_unique<MCPClient>(cfg_it->second);
    auto result = client->start();
    if (result.failed()) {
        LOG_ERROR(TAG_MANAGER, "Failed to start server %s: %s", server_id.c_str(), result.error().message.c_str());
        update_server_status_in_db(server_id, ServerStatus::Error);
        return result;
    }

    // Initialize MCP handshake
    auto init_result = client->initialize();
    if (init_result.failed()) {
        LOG_ERROR(TAG_MANAGER, "MCP init failed for %s: %s", server_id.c_str(), init_result.error().message.c_str());
        client->stop();
        update_server_status_in_db(server_id, ServerStatus::Error);
        return Error("MCP initialization failed: " + init_result.error().message);
    }

    // Fetch tools
    auto tools_result = client->list_tools();
    if (tools_result.ok()) {
        tool_cache_[server_id] = tools_result.value();
        LOG_INFO(TAG_MANAGER, "Server %s has %zu tools", server_id.c_str(), tools_result.value().size());
    }

    clients_[server_id] = std::move(client);
    update_server_status_in_db(server_id, ServerStatus::Running);
    restart_info_.erase(server_id); // Reset restart counter on successful start

    return {};
}

Result<void> MCPManager::stop_server(const std::string& server_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = clients_.find(server_id);
    if (it == clients_.end()) return {};

    it->second->stop();
    clients_.erase(it);
    tool_cache_.erase(server_id);
    update_server_status_in_db(server_id, ServerStatus::Stopped);

    return {};
}

Result<void> MCPManager::restart_server(const std::string& server_id) {
    stop_server(server_id);
    return start_server(server_id);
}

void MCPManager::start_auto_servers() {
    std::vector<std::string> auto_ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, cfg] : configs_) {
            if (cfg.auto_start && cfg.enabled) {
                auto_ids.push_back(id);
            }
        }
    }

    for (const auto& id : auto_ids) {
        auto result = start_server(id);
        if (result.failed()) {
            LOG_WARN(TAG_MANAGER, "Auto-start failed for %s: %s", id.c_str(), result.error().message.c_str());
        }
    }
}

void MCPManager::stop_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, client] : clients_) {
        client->stop();
        update_server_status_in_db(id, ServerStatus::Stopped);
    }
    clients_.clear();
    tool_cache_.clear();
}

ServerStatus MCPManager::get_server_status(const std::string& server_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(server_id);
    if (it == clients_.end()) return ServerStatus::Stopped;
    return it->second->is_running() ? ServerStatus::Running : ServerStatus::Error;
}

// ============================================================================
// Tool Aggregation
// ============================================================================

std::vector<ExternalTool> MCPManager::get_all_external_tools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ExternalTool> result;
    for (const auto& [server_id, tools] : tool_cache_) {
        // Only include tools from running servers
        auto it = clients_.find(server_id);
        if (it != clients_.end() && it->second->is_running()) {
            result.insert(result.end(), tools.begin(), tools.end());
        }
    }
    return result;
}

Result<json> MCPManager::call_external_tool(const std::string& server_id,
                                             const std::string& tool_name,
                                             const json& args) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = clients_.find(server_id);
    if (it == clients_.end() || !it->second->is_running()) {
        return Error("Server not running: " + server_id);
    }

    return it->second->call_tool(tool_name, args);
}

// ============================================================================
// Health Check
// ============================================================================

void MCPManager::start_health_check(int interval_seconds) {
    if (health_running_.load()) return;

    health_interval_seconds_ = interval_seconds;
    health_running_ = true;
    health_thread_ = std::thread(&MCPManager::health_check_loop, this);
    LOG_INFO(TAG_MANAGER, "Health check started (interval: %ds)", interval_seconds);
}

void MCPManager::stop_health_check() {
    health_running_ = false;
    if (health_thread_.joinable()) {
        health_thread_.join();
    }
}

void MCPManager::health_check_loop() {
    while (health_running_.load()) {
        // Sleep in small increments so we can exit quickly
        for (int i = 0; i < health_interval_seconds_ * 10 && health_running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!health_running_.load()) break;

        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> dead_servers;

        for (auto& [id, client] : clients_) {
            if (!client->is_running()) {
                dead_servers.push_back(id);
                continue;
            }

            auto ping_result = client->ping();
            if (ping_result.failed()) {
                LOG_WARN(TAG_MANAGER, "Ping failed for %s: %s", id.c_str(), ping_result.error().message.c_str());
                dead_servers.push_back(id);
            }
        }

        // Attempt restart for dead servers
        for (const auto& id : dead_servers) {
            auto& info = restart_info_[id];
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - info.last_attempt).count();

            if (info.attempts >= MAX_RESTART_ATTEMPTS && elapsed < RESTART_COOLDOWN_SECONDS) {
                LOG_WARN(TAG_MANAGER, "Server %s exceeded max restart attempts, cooldown active", id.c_str());
                continue;
            }

            if (elapsed >= RESTART_COOLDOWN_SECONDS) {
                info.attempts = 0; // Reset after cooldown
            }

            info.attempts++;
            info.last_attempt = now;

            LOG_INFO(TAG_MANAGER, "Attempting restart of %s (attempt %d/%d)",
                     id.c_str(), info.attempts, MAX_RESTART_ATTEMPTS);

            // Clean up the dead client
            clients_.erase(id);
            tool_cache_.erase(id);

            // We need to release the lock to call start_server which also locks
            // So we just mark for restart and do it outside
            // Actually, let's restart inline since we already have the lock
            auto cfg_it = configs_.find(id);
            if (cfg_it == configs_.end()) continue;

            auto client = std::make_unique<MCPClient>(cfg_it->second);
            auto result = client->start();
            if (result.ok()) {
                auto init_result = client->initialize();
                if (init_result.ok()) {
                    auto tools_result = client->list_tools();
                    if (tools_result.ok()) {
                        tool_cache_[id] = tools_result.value();
                    }
                    clients_[id] = std::move(client);
                    update_server_status_in_db(id, ServerStatus::Running);
                    LOG_INFO(TAG_MANAGER, "Successfully restarted %s", id.c_str());
                    restart_info_.erase(id);
                } else {
                    client->stop();
                    update_server_status_in_db(id, ServerStatus::Error);
                }
            } else {
                update_server_status_in_db(id, ServerStatus::Error);
            }
        }
    }
}

// ============================================================================
// Database Operations
// ============================================================================

void MCPManager::create_mcp_schema() {
    auto& db = fincept::db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    db.exec(R"(
        CREATE TABLE IF NOT EXISTS mcp_servers (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            command TEXT NOT NULL,
            args TEXT DEFAULT '[]',
            env TEXT DEFAULT '{}',
            category TEXT DEFAULT '',
            enabled INTEGER DEFAULT 1,
            auto_start INTEGER DEFAULT 0,
            status TEXT DEFAULT 'stopped',
            created_at TEXT DEFAULT (datetime('now')),
            updated_at TEXT DEFAULT (datetime('now'))
        )
    )");
}

void MCPManager::load_configs_from_db() {
    auto& db = fincept::db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare("SELECT id, name, description, command, args, env, category, enabled, auto_start, status, created_at, updated_at FROM mcp_servers");

    std::lock_guard<std::mutex> my_lock(mutex_);
    configs_.clear();

    while (stmt.step()) {
        MCPServerConfig cfg;
        cfg.id = stmt.col_text(0);
        cfg.name = stmt.col_text(1);
        cfg.description = stmt.col_text(2);
        cfg.command = stmt.col_text(3);

        // Parse args JSON array
        try {
            auto args_json = json::parse(stmt.col_text(4));
            if (args_json.is_array()) {
                for (const auto& a : args_json) {
                    cfg.args.push_back(a.get<std::string>());
                }
            }
        } catch (...) {}

        // Parse env JSON object
        try {
            auto env_json = json::parse(stmt.col_text(5));
            if (env_json.is_object()) {
                for (auto& [key, val] : env_json.items()) {
                    cfg.env[key] = val.get<std::string>();
                }
            }
        } catch (...) {}

        cfg.category = stmt.col_text(6);
        cfg.enabled = stmt.col_bool(7);
        cfg.auto_start = stmt.col_bool(8);

        std::string status_str = stmt.col_text(9);
        if (status_str == "running") cfg.status = ServerStatus::Running;
        else if (status_str == "error") cfg.status = ServerStatus::Error;
        else cfg.status = ServerStatus::Stopped;

        cfg.created_at = stmt.col_text(10);
        cfg.updated_at = stmt.col_text(11);

        configs_[cfg.id] = std::move(cfg);
    }
}

void MCPManager::save_config_to_db(const MCPServerConfig& config) {
    auto& db = fincept::db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    json args_json = json::array();
    for (const auto& a : config.args) args_json.push_back(a);

    json env_json = json::object();
    for (const auto& [k, v] : config.env) env_json[k] = v;

    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO mcp_servers
        (id, name, description, command, args, env, category, enabled, auto_start, status, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))
    )");

    stmt.bind_text(1, config.id);
    stmt.bind_text(2, config.name);
    stmt.bind_text(3, config.description);
    stmt.bind_text(4, config.command);
    stmt.bind_text(5, args_json.dump());
    stmt.bind_text(6, env_json.dump());
    stmt.bind_text(7, config.category);
    stmt.bind_int(8, config.enabled ? 1 : 0);
    stmt.bind_int(9, config.auto_start ? 1 : 0);
    stmt.bind_text(10, server_status_str(config.status));
    stmt.execute();
}

void MCPManager::delete_config_from_db(const std::string& server_id) {
    auto& db = fincept::db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare("DELETE FROM mcp_servers WHERE id = ?");
    stmt.bind_text(1, server_id);
    stmt.execute();
}

void MCPManager::update_server_status_in_db(const std::string& server_id, ServerStatus status) {
    auto& db = fincept::db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare("UPDATE mcp_servers SET status = ?, updated_at = datetime('now') WHERE id = ?");
    stmt.bind_text(1, server_status_str(status));
    stmt.bind_text(2, server_id);
    stmt.execute();
}

void MCPManager::refresh_tool_cache(const std::string& server_id) {
    auto it = clients_.find(server_id);
    if (it == clients_.end() || !it->second->is_running()) return;

    auto result = it->second->list_tools();
    if (result.ok()) {
        tool_cache_[server_id] = result.value();
    }
}

} // namespace fincept::mcp
