#pragma once
// MCPManager — Manages external MCP server lifecycle
// Stores configs in SQLite, spawns/stops servers, health checks,
// auto-restart with backoff, tool aggregation from all running servers.

#include "mcp_types.h"
#include "mcp_client.h"
#include "../core/result.h"
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace fincept::mcp {

using core::Result;
using core::Error;

class MCPManager {
public:
    static MCPManager& instance();

    // ── Server Configuration (SQLite persistence) ───────────────────────

    /// Add or update a server config
    Result<void> save_server(const MCPServerConfig& config);

    /// Remove a server config and stop if running
    Result<void> remove_server(const std::string& server_id);

    /// Get all configured servers
    std::vector<MCPServerConfig> get_servers() const;

    /// Get a specific server config
    std::optional<MCPServerConfig> get_server(const std::string& server_id) const;

    // ── Server Lifecycle ────────────────────────────────────────────────

    /// Start a specific server
    Result<void> start_server(const std::string& server_id);

    /// Stop a specific server
    Result<void> stop_server(const std::string& server_id);

    /// Restart a server
    Result<void> restart_server(const std::string& server_id);

    /// Start all auto-start servers
    void start_auto_servers();

    /// Stop all running servers
    void stop_all();

    /// Get status of a specific server
    ServerStatus get_server_status(const std::string& server_id) const;

    // ── Tool Aggregation ────────────────────────────────────────────────

    /// Get all tools from all running external servers
    std::vector<ExternalTool> get_all_external_tools() const;

    /// Call a tool on a specific external server
    Result<json> call_external_tool(const std::string& server_id,
                                    const std::string& tool_name,
                                    const json& args);

    // ── Health Check ────────────────────────────────────────────────────

    /// Start background health check thread
    void start_health_check(int interval_seconds = 60);

    /// Stop health check thread
    void stop_health_check();

    // ── Lifecycle ───────────────────────────────────────────────────────

    /// Initialize — load configs from DB, create schema
    void initialize();

    /// Shutdown — stop all servers, stop health check
    void shutdown();

    MCPManager(const MCPManager&) = delete;
    MCPManager& operator=(const MCPManager&) = delete;

private:
    MCPManager() = default;

    mutable std::mutex mutex_;

    // Running server clients
    std::unordered_map<std::string, std::unique_ptr<MCPClient>> clients_;

    // Cached tools per server
    mutable std::unordered_map<std::string, std::vector<ExternalTool>> tool_cache_;
    mutable std::chrono::steady_clock::time_point cache_time_;

    // Server configs (loaded from DB)
    std::unordered_map<std::string, MCPServerConfig> configs_;

    // Health check
    std::thread health_thread_;
    std::atomic<bool> health_running_{false};
    int health_interval_seconds_ = 60;
    void health_check_loop();

    // Restart tracking
    struct RestartInfo {
        int attempts = 0;
        std::chrono::steady_clock::time_point last_attempt;
    };
    std::unordered_map<std::string, RestartInfo> restart_info_;
    static constexpr int MAX_RESTART_ATTEMPTS = 3;
    static constexpr int RESTART_COOLDOWN_SECONDS = 300; // 5 minutes

    // DB operations
    void create_mcp_schema();
    void load_configs_from_db();
    void save_config_to_db(const MCPServerConfig& config);
    void delete_config_from_db(const std::string& server_id);
    void update_server_status_in_db(const std::string& server_id, ServerStatus status);

    // Tool cache refresh
    void refresh_tool_cache(const std::string& server_id);
};

} // namespace fincept::mcp
