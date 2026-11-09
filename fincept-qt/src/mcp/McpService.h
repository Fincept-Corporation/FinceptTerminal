#pragma once
// McpService.h — Unified tool interface merging internal + external MCP tools (Qt port)
// Single entry point for AI chat, agents, and node editor to discover and call tools.

#include "core/result/Result.h"
#include "mcp/McpTypes.h"

#include <QDateTime>
#include <QFuture>
#include <QMutex>
#include <QSet>

#include <vector>

namespace fincept::mcp {

class McpService {
  public:
    static McpService& instance();

    // ── Unified Tool Discovery ──────────────────────────────────────────

    /// Get all available tools (internal + external, cached 5 s)
    std::vector<UnifiedTool> get_all_tools();

    /// Filtered variant — same `ToolFilter` semantics as
    /// `format_tools_for_openai(filter)`, including the 50-tool safety cap when
    /// `max_tools` is 0. Lets non-OpenAI builders (Anthropic, Gemini, Fincept
    /// text catalog) honour the same scope.
    std::vector<UnifiedTool> get_all_tools(const ToolFilter& filter);

    /// Catalogue for an internal agent run (TerminalMcpBridge / finagent_core).
    ///
    /// Same include/exclude predicates as get_all_tools(filter), but `max_tools`
    /// is the agent author's own cap — 0 means EVERY matching tool — and the
    /// order puts read-only tools first (then the caller's category order, then
    /// name), so a capped catalogue keeps the most useful tools. The hidden
    /// 50-tool cap only exists for the LLM provider formatters, whose payload
    /// rides on every request of a turn; applying it here silently cut agents
    /// down to an alphabetical slice (add_*, agent_*, alt_*…) with no quotes,
    /// news or filings while the UI promised "0 = no cap".
    std::vector<UnifiedTool> get_agent_tools(const ToolFilter& filter);

    /// Tool arrays for the three provider dialects. Every one is built by the
    /// same selection (select_tools_for_llm_locked), so OpenAI, Anthropic and
    /// Gemini always see the same tools: with a default filter that is the
    /// whole internal catalogue (the ten core tools) followed by the user's
    /// external MCP servers' tools, capped per request.
    QJsonArray format_tools_for_openai(const ToolFilter& filter);

    /// Anthropic `/v1/messages` — `[{name, description, input_schema}]`.
    QJsonArray format_tools_for_anthropic(const ToolFilter& filter);

    /// Gemini `generateContent` — `[{functionDeclarations:[{name, description, parameters}]}]`.
    /// Parameters are translated into Gemini's OpenAPI subset (see
    /// mcp/GeminiSchema.h); declarations whose name or schema cannot be made
    /// valid are dropped rather than allowed to fail the whole request.
    QJsonArray format_tools_for_gemini(const ToolFilter& filter);

    std::size_t tool_count();

    /// The UNSANITISED input schema for a wire function name ("serverId__tool"),
    /// or an empty object when no such tool is registered.
    ///
    /// Needed by the Gemini path: its declarations carry a lossy translation of
    /// the schema (see mcp/GeminiSchema.h), so the arguments that come back have
    /// to be reconciled against the original before dispatch.
    QJsonObject input_schema_for_function(const QString& function_name);

    // ── Unified Tool Execution ──────────────────────────────────────────

    /// Where a bare tool name lives (internal, or which external server).
    struct ToolTarget {
        QString server_id;           // INTERNAL_SERVER_ID or an external server id
        QString category;            // empty for external tools
        bool is_destructive = false; // external tools: true (no metadata on the wire)
        QString error;               // non-empty ⇒ unresolved; human/LLM-readable reason
    };

    /// Resolve `name` (a bare tool name). An empty `server_id`
    /// means "built-in tool if one exists, otherwise the external tool with
    /// that name" — ambiguous external names are refused with the candidate
    /// server ids, never guessed.
    ToolTarget resolve_tool_target(const QString& name, const QString& server_id = {});

    /// Route to internal or external server.
    ToolResult execute_tool(const QString& server_id, const QString& tool_name, const QJsonObject& args);

    /// Execute from OpenAI function-call format ("serverId__toolName")
    ToolResult execute_openai_function(const QString& function_name, const QJsonObject& args);

    /// Phase 4: async equivalent of execute_openai_function. Returns a
    /// QFuture<ToolResult> so callers (LlmService dispatch loops in
    /// Phase 5) can fan out multiple tool calls concurrently and join
    /// with QtFuture::whenAll. Internal tools resolve via
    /// McpProvider::call_tool_async; external tools dispatch through a
    /// QtConcurrent::run wrapper since McpManager's RPC client is
    /// blocking by design.
    QFuture<ToolResult> execute_openai_function_async(const QString& function_name, const QJsonObject& args);

    // ── Validation ──────────────────────────────────────────────────────
    // Phase 3: removed. McpProvider::call_tool now invokes
    // mcp::validate_args automatically; SchemaValidator.h is the single
    // entry point for input checks.

    // ── Tool enable/disable (persisted) ─────────────────────────────────

    /// Enable/disable an INTERNAL tool and remember the choice across restarts
    /// (SettingsRepository key `mcp.disabled_tools`, a JSON array of names).
    /// McpProvider::set_tool_enabled alone is in-memory only, which is why the
    /// MCP Servers screen's per-tool toggle used to reset on every launch.
    /// Call from the GUI thread (SettingsRepository's connection lives there).
    void set_tool_enabled_persistent(const QString& tool_name, bool enabled);

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

    // Filter-aware caches. Both keyed by a stable signature of ToolFilter
    // (categories, exclude_categories, name_patterns, exclude_name_patterns,
    // max_tools). Cleared whenever refresh_cache() rebuilds cached_tools_,
    // so they automatically follow the same TTL + generation invalidation.
    // Avoids re-running the regex/category sweep AND re-stringifying the
    // ~150 KB OpenAI JSON schema on every LLM turn.
    QHash<QByteArray, std::vector<UnifiedTool>> filtered_tools_cache_;
    QHash<QByteArray, QJsonArray> openai_format_cache_;
    QHash<QByteArray, QJsonArray> anthropic_format_cache_;
    QHash<QByteArray, QJsonArray> gemini_format_cache_;
    // Keyed by filter signature — a handful of entries in practice. Cleared
    // wholesale on overflow: it is a memo, not a store.
    static constexpr int kMaxFormatCacheEntries = 64;

    void refresh_cache();        // requires mutex_ held
    bool is_cache_valid() const; // requires mutex_ held

    /// Re-apply the persisted disabled-tool set (see set_tool_enabled_persistent).
    /// Called from initialize(), i.e. after every internal tool is registered.
    void apply_persisted_tool_states();

    // Snapshot the unfiltered cached tool list (refreshing if stale). Caller
    // must hold mutex_. Splitting this out lets get_all_tools(filter) and
    // format_tools_for_openai(filter) reuse the cache without re-locking.
    const std::vector<UnifiedTool>& cached_tools_locked();

    /// Tool selection shared by every provider formatter. Caller must hold
    /// mutex_. `out_key` receives the cache key the caller should memo under.
    std::vector<UnifiedTool> select_tools_for_llm_locked(const ToolFilter& filter, QByteArray* out_key);

    // Stable byte signature of a ToolFilter — usable as a QHash key.
    static QByteArray filter_signature(const ToolFilter& filter);
};

} // namespace fincept::mcp
