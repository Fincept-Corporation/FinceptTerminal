#pragma once
// McpProvider.h — Internal tool registry and executor (Qt port)
// Singleton managing all built-in terminal tools.
// Tools register handlers invoked by AI chat, agents, or node editor.

#include "mcp/McpTypes.h"

#include <QHash>
#include <QMutex>
#include <QSet>

#include <vector>

namespace fincept::mcp {

class McpProvider {
  public:
    static McpProvider& instance();

    // ── Tool Registration ──────────────────────────────────────────────────

    void register_tool(ToolDef tool);
    void register_tools(std::vector<ToolDef> tools);
    void unregister_tool(const QString& name);

    // ── Tool Enable/Disable ────────────────────────────────────────────────

    void set_tool_enabled(const QString& name, bool enabled);
    bool is_tool_enabled(const QString& name) const;

    // ── Tool Discovery ─────────────────────────────────────────────────────

    /// List all enabled tools as UnifiedTool (for LLM consumption)
    std::vector<UnifiedTool> list_tools() const;

    /// List ALL tools including disabled (for management UI)
    std::vector<UnifiedTool> list_all_tools() const;

    std::size_t tool_count() const;
    bool has_tool(const QString& name) const;

    // ── Tool Execution ─────────────────────────────────────────────────────

    ToolResult call_tool(const QString& name, const QJsonObject& args);

    // ── LLM Integration ────────────────────────────────────────────────────

    /// Format all enabled tools for OpenAI function calling
    QJsonArray format_tools_for_openai() const;

    /// Parse "serverId__toolName" → { server_id, tool_name }
    static QPair<QString, QString> parse_openai_function_name(const QString& fn_name);

    // ── Lifecycle ──────────────────────────────────────────────────────────

    void clear();

    // ── Generation Counter ─────────────────────────────────────────────────

    /// Monotonically increasing — incremented on any mutation.
    /// McpService uses this to detect stale cache.
    quint64 generation() const;

    McpProvider(const McpProvider&) = delete;
    McpProvider& operator=(const McpProvider&) = delete;

  private:
    McpProvider() = default;

    mutable QMutex mutex_;
    QHash<QString, ToolDef> tools_;
    QSet<QString> disabled_tools_;
    quint64 generation_ = 0;
};

} // namespace fincept::mcp
