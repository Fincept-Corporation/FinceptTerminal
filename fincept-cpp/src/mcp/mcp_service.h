#pragma once
// MCPService — Unified tool interface merging internal + external MCP tools
// Single entry point for AI chat, agents, and node editor to discover and call tools.

#include "mcp_types.h"
#include "../core/result.h"
#include <mutex>
#include <chrono>
#include <vector>

namespace fincept::mcp {

using core::Result;
using core::Error;

class MCPService {
public:
    static MCPService& instance();

    // ── Unified Tool Discovery ──────────────────────────────────────────

    /// Get all available tools (internal + external, cached for 5s)
    std::vector<UnifiedTool> get_all_tools();

    /// Get tools formatted for OpenAI function calling
    json format_tools_for_openai();

    /// Get tool count
    size_t tool_count();

    // ── Unified Tool Execution ──────────────────────────────────────────

    /// Execute a tool by unified name (routes to internal or external)
    ToolResult execute_tool(const std::string& server_id,
                           const std::string& tool_name,
                           const json& args);

    /// Execute from OpenAI function call format (serverId__toolName)
    ToolResult execute_openai_function(const std::string& function_name,
                                       const json& args);

    // ── Validation ──────────────────────────────────────────────────────

    /// Validate tool parameters against its JSON schema
    Result<void> validate_params(const std::string& tool_name, const json& args);

    // ── Lifecycle ───────────────────────────────────────────────────────

    /// Initialize internal tools and external manager
    void initialize();

    /// Shutdown everything
    void shutdown();

    MCPService(const MCPService&) = delete;
    MCPService& operator=(const MCPService&) = delete;

private:
    MCPService() = default;

    mutable std::mutex mutex_;

    // Tool cache
    std::vector<UnifiedTool> cached_tools_;
    std::chrono::steady_clock::time_point cache_time_;
    uint64_t cached_generation_ = 0;   // provider generation at last refresh
    static constexpr int CACHE_TTL_MS = 5000;

    void refresh_cache();
    bool is_cache_valid() const;
};

} // namespace fincept::mcp
