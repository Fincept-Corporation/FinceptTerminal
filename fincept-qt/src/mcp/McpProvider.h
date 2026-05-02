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

    /// Parse "serverId__toolName" → { server_id, tool_name }.
    /// Reverses any wire-encoding applied by `encode_tool_name_for_wire` (e.g.
    /// `tool-dot-list` → `tool.list`) so the returned tool_name matches the
    /// registry key.
    static QPair<QString, QString> parse_openai_function_name(const QString& fn_name);

    /// Encode an internal tool name into a wire-safe form acceptable to every
    /// supported provider. The tightest common subset (intersection of every
    /// provider's published validation) is:
    ///
    ///   ^[a-zA-Z][a-zA-Z0-9_-]{0,63}$      total length 1..64
    ///
    /// Provider-specific rules surveyed (Apr 2026):
    ///   - OpenAI / Anthropic / Groq / OpenRouter / DeepSeek / xAI / MiniMax:
    ///     ^[a-zA-Z0-9_-]{1,64}$  (no dots, no leading symbol-only required).
    ///   - Kimi (Moonshot):
    ///     ^[a-zA-Z_][a-zA-Z0-9_-]{2,63}$  (must start with a letter or '_';
    ///     total length 3..64).
    ///   - Gemini:
    ///     [a-zA-Z0-9_:.-]{1,128}  (permissive; dots/colons allowed).
    ///
    /// Encoding rules:
    ///   1. Each '.' in the internal name → "-dot-" (no internal tool name uses
    ///      hyphens, so the round-trip is unambiguous).
    ///   2. If the resulting name fails the common-subset regex (e.g. starts
    ///      with a digit or contains other illegal chars), the encoder prefixes
    ///      it with `t_` and replaces every illegal byte with `_`.
    ///   3. The combined `<server_id>__<encoded_tool>` must fit in 64 chars.
    ///      Server prefix `fincept-terminal__` is 18 chars; longer tool names
    ///      are tail-truncated and a 4-char hash suffix preserves uniqueness.
    ///
    /// `decode_tool_name_from_wire` reverses step 1 only — steps 2 and 3 are
    /// effectively irreversible, so any tool whose name needs them must be
    /// looked up by its hash via `find_tool_by_wire_name` (registered when
    /// `format_tools_for_openai` is called) rather than by string round-trip.
    static QString encode_tool_name_for_wire(const QString& tool_name);
    static QString decode_tool_name_from_wire(const QString& wire_name);

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
