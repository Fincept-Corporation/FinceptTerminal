#pragma once
// McpService.h — Unified tool interface merging internal + external MCP tools (Qt port)
// Single entry point for AI chat, agents, and node editor to discover and call tools.

#include "core/result/Result.h"
#include "mcp/McpTypes.h"

#include <QDateTime>
#include <QMutex>

#include <vector>

namespace fincept::mcp {

class McpService {
  public:
    static McpService& instance();

    // ── Unified Tool Discovery ──────────────────────────────────────────

    /// Get all available tools (internal + external, cached 5 s)
    std::vector<UnifiedTool> get_all_tools();

    /// Format for OpenAI function calling
    QJsonArray format_tools_for_openai();

    std::size_t tool_count();

    // ── Unified Tool Execution ──────────────────────────────────────────

    /// Route to internal or external server
    ToolResult execute_tool(const QString& server_id, const QString& tool_name, const QJsonObject& args);

    /// Execute from OpenAI function-call format ("serverId__toolName")
    ToolResult execute_openai_function(const QString& function_name, const QJsonObject& args);

    // ── Validation ──────────────────────────────────────────────────────

    /// Validate args against the tool's JSON schema
    Result<void> validate_params(const QString& tool_name, const QJsonObject& args);

    // ── Lifecycle ───────────────────────────────────────────────────────

    void initialize();
    void shutdown();

    McpService(const McpService&) = delete;
    McpService& operator=(const McpService&) = delete;

  private:
    McpService() = default;

    mutable QMutex mutex_;

    std::vector<UnifiedTool> cached_tools_;
    QDateTime cache_time_;
    quint64 cached_generation_ = 0;
    static constexpr int CACHE_TTL_MS = 5000;

    void refresh_cache();
    bool is_cache_valid() const;
};

} // namespace fincept::mcp
