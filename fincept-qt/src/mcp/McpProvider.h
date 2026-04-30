#pragma once
// McpProvider.h — Internal tool registry and executor (Qt port)
// Singleton managing all built-in terminal tools.
// Tools register handlers invoked by AI chat, agents, or node editor.

#include "mcp/McpTypes.h"

#include <QFuture>
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

    /// Synchronous execution. For tools registered with `handler` (legacy),
    /// invokes directly and returns. For tools registered with `async_handler`,
    /// dispatches asynchronously and waits for the future to complete (with
    /// the tool's default_timeout_ms applied). Validation runs first via
    /// mcp::validate_args; failures short-circuit before the handler.
    ToolResult call_tool(const QString& name, const QJsonObject& args);

    /// Phase 4: asynchronous execution. Returns a QFuture that resolves with
    /// the ToolResult. For sync handlers, returns an already-resolved future
    /// (the handler runs on the calling thread). For async handlers, builds
    /// a ToolContext (cancellation hook + timeout from the per-call ctx or
    /// the tool's default), invokes the handler, and arms a timeout timer.
    ///
    /// Pass an empty ToolContext to get the tool's defaults. Pass a populated
    /// one to override timeout / inject a cancellation hook (Phase 5 will
    /// thread cancellation tokens through here).
    QFuture<ToolResult> call_tool_async(const QString& name, const QJsonObject& args,
                                         ToolContext ctx = {});

    // ── Phase 6.3: Authorization hook ──────────────────────────────────────
    /// Caller-supplied predicate that returns true iff the call should
    /// proceed. The hook receives the tool's required AuthLevel and
    /// is_destructive flag; it is responsible for checking the active
    /// session (AuthManager), prompting the user (modal dialog), and
    /// returning the verdict synchronously. Hook lives in the app layer to
    /// avoid pulling auth/UI headers into McpTypes.h.
    ///
    /// When unset, tools with auth_required > Authenticated or
    /// is_destructive=true fail closed; lesser tools pass through.
    using AuthChecker = std::function<bool(AuthLevel required, bool is_destructive)>;
    void set_auth_checker(AuthChecker checker);

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

    // Phase 6.3 — guarded by mutex_ so set_auth_checker / call_tool_async
    // see consistent state across threads.
    AuthChecker auth_checker_;
};

} // namespace fincept::mcp
