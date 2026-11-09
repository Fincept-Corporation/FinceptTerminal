#pragma once
// McpProvider.h — Internal tool registry and executor (Qt port)
// Singleton managing all built-in terminal tools.
// Tools register handlers invoked by AI chat, agents, or node editor.

#include "mcp/McpTypes.h"

#include <QFuture>
#include <QHash>
#include <QMutex>
#include <QSet>

#include <optional>
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

    /// O(1) lookup of a single tool's UnifiedTool snapshot by canonical name.
    /// Returns `std::nullopt` if the name is unknown or disabled. O(1), unlike
    /// list_tools() + a linear scan.
    std::optional<UnifiedTool> find_tool(const QString& name) const;

    /// Audit-friendly snapshot of one tool. Carries the bits the self-test /
    /// management UI need to verify wiring — crucially handler-presence, which
    /// the LLM-facing UnifiedTool snapshot omits. Does not expose the handler
    /// std::functions themselves (kept inside the registry).
    struct ToolAuditInfo {
        QString name;
        QString category;
        QString description;
        bool has_handler = false; // sync OR async handler is set
        bool is_async = false;    // async_handler is set
        int default_timeout_ms = kMcpDefaultTimeoutMs;
        bool enabled = true;
        bool is_destructive = false;
        AuthLevel auth_required = AuthLevel::None;
        QJsonObject input_schema; // serialised JSON Schema
    };

    /// One ToolAuditInfo per registered tool (enabled and disabled). Used by
    /// the headless tool self-test to verify every tool has a handler, a valid
    /// schema, and a usable description without invoking anything.
    std::vector<ToolAuditInfo> audit_all_tools() const;

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
    QFuture<ToolResult> call_tool_async(const QString& name, const QJsonObject& args, ToolContext ctx = {});

    /// The tool's declared hard budget, or the 30 s default if unknown. The
    /// agent bridge sizes its callers' HTTP timeouts from it.
    ///
    /// (Long work is no longer backgrounded generically: the core tools answer
    /// within their budget, and the `python` tool runs its own jobs.)
    int effective_timeout_ms(const QString& name) const;

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

    // ── Destructive-tool capability (fail-closed) ─────────────────────────
    //
    // `is_destructive` used to gate nothing on the chat path. The only consumer
    // was the AuthChecker installed by AgentService, which denies a destructive
    // tool ONLY when TerminalMcpBridge::is_call_in_progress() is true. The
    // interactive LLM tool loop calls McpService::execute_openai_function with
    // no ScopedCallFlags, so that flag was false and every destructive tool
    // below AuthLevel::Verified executed with no prompt of any kind.
    //
    // The gate now fails closed: a tool that declares `is_destructive` is
    // refused unless destructive capability has been granted for the current
    // context. Three ways to grant it, all explicit:
    //   1. Per call — the agent bridge's destructive capability token
    //      (TerminalMcpBridge::destructive_token(), echoed back as
    //      X-MCP-Allow-Destructive and surfaced via is_destructive_allowed()).
    //   2. Per call — the user approved THIS call in the terminal UI
    //      (ToolApprovalDialog wraps the dispatch in mcp::ScopedApprovedCall,
    //      see mcp/server/CallApproval.h). A per-call human approval is a
    //      stronger grant than the session switch below.
    //   3. Per session — set_destructive_allowed(true), persisted as
    //      `mcp/allow_destructive_tools`. Defaults to false.
    //
    // AuthLevel::ExplicitConfirm (and Verified/Subscribed) tools additionally
    // go through the AuthChecker AgentService installs, which lets them run
    // only inside a user-approved call (ScopedApprovedCall::active()).

    /// Grant/revoke destructive-tool capability for this session. Persists to
    /// `mcp/allow_destructive_tools` so the choice survives a restart.
    static void set_destructive_allowed(bool allowed);

    /// True when destructive tools may run in the CURRENT context — i.e. the
    /// per-call capability token is present, the user approved this call in
    /// the UI, or the session grant is on.
    static bool destructive_allowed();

    /// Run the Phase 6.3 authorization gate for one call WITHOUT executing
    /// anything. Returns std::nullopt when the call may proceed, or a
    /// ready-to-return failure ToolResult when blocked. Shared by
    /// call_tool_async (internal tools) and McpService::execute_tool
    /// (external tools, gated destructive-by-default) so both paths apply
    /// identical auth/destructive-confirmation rules. `name` is used for
    /// logging and error messages only.
    ///
    /// `destructive_declared` — true when `is_destructive` comes from the
    /// tool's own ToolDef. Pass false when the caller is merely applying a
    /// conservative default to a tool that carries no destructiveness metadata
    /// (external MCP servers over the wire). Only DECLARED destructive tools
    /// hit the fail-closed capability gate; undeclared ones stay on the
    /// checker-only path so user-configured external servers keep working.
    std::optional<ToolResult> check_authorization(const QString& name, AuthLevel auth_required, bool is_destructive,
                                                  bool destructive_declared = true) const;

    // ── LLM Integration ────────────────────────────────────────────────────
    //
    // Provider tool arrays are built by McpService::format_tools_for_* only —
    // they share Tool RAG / Tier-0 selection, which a registry-wide dump here
    // would bypass.

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
    /// Encoding: each '.' in the internal name → "-dot-" (no internal tool name
    /// uses hyphens, so the round-trip is unambiguous). Nothing else is
    /// rewritten — a name that still fails the common-subset regex is logged
    /// once so it can be renamed at source, because a silently mangled name
    /// breaks the model's ability to call the tool by its advertised string.
    /// `decode_tool_name_from_wire` reverses the '.' encoding.
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
    // Pre-serialised view of each tool, kept in lockstep with `tools_`. Built
    // once at registration so list_tools()/find_tool() never re-serialise the
    // input_schema. `disabled_tools_` is still consulted at read time.
    QHash<QString, UnifiedTool> snapshots_;
    QSet<QString> disabled_tools_;
    quint64 generation_ = 0;

    // Phase 6.3 — guarded by mutex_ so set_auth_checker / call_tool_async
    // see consistent state across threads.
    AuthChecker auth_checker_;
};

} // namespace fincept::mcp
