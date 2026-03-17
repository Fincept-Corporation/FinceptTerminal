#pragma once
// MCPProvider — Internal tool registry and executor
// Singleton that manages all built-in terminal tools.
// Tools register handlers that are invoked by AI chat, agents, or node editor.

#include "mcp_types.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace fincept::mcp {

class MCPProvider {
public:
    static MCPProvider& instance();

    // ── Tool Registration ──────────────────────────────────────────────────

    /// Register a single tool
    void register_tool(ToolDef tool);

    /// Register multiple tools at once
    void register_tools(std::vector<ToolDef> tools);

    /// Unregister a tool by name
    void unregister_tool(const std::string& name);

    // ── Tool Enable/Disable ────────────────────────────────────────────────

    void set_tool_enabled(const std::string& name, bool enabled);
    bool is_tool_enabled(const std::string& name) const;

    // ── Tool Discovery ─────────────────────────────────────────────────────

    /// List all enabled tools as UnifiedTool (for LLM consumption)
    std::vector<UnifiedTool> list_tools() const;

    /// List ALL tools including disabled (for management UI)
    std::vector<UnifiedTool> list_all_tools() const;

    /// Get tool count (enabled only)
    size_t tool_count() const;

    /// Check if a tool exists
    bool has_tool(const std::string& name) const;

    // ── Tool Execution ─────────────────────────────────────────────────────

    /// Execute a tool by name with JSON arguments
    ToolResult call_tool(const std::string& name, const json& args);

    // ── LLM Integration ────────────────────────────────────────────────────

    /// Format all enabled tools for OpenAI function calling
    json format_tools_for_openai() const;

    /// Parse an OpenAI function name "serverId__toolName" back to components
    static std::pair<std::string, std::string> parse_openai_function_name(const std::string& fn_name);

    // ── Lifecycle ──────────────────────────────────────────────────────────

    /// Clear all registered tools
    void clear();

    // ── Generation Counter ─────────────────────────────────────────────────

    /// Monotonically increasing counter — incremented on any mutation
    /// (register, unregister, enable, disable, clear).
    /// Consumers (MCPService) use this to detect when their cache is stale.
    uint64_t generation() const;

    MCPProvider(const MCPProvider&) = delete;
    MCPProvider& operator=(const MCPProvider&) = delete;

private:
    MCPProvider() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ToolDef> tools_;
    std::unordered_set<std::string> disabled_tools_;
    uint64_t generation_ = 0;
};

} // namespace fincept::mcp
