#pragma once
// McpService.h — Unified tool interface merging internal + external MCP tools (Qt port)
// Single entry point for AI chat, agents, and node editor to discover and call tools.

#include "core/result/Result.h"
#include "mcp/McpTypes.h"

#include <QDateTime>
#include <QFuture>
#include <QMutex>

#include <vector>

namespace fincept::mcp {

class McpService {
  public:
    static McpService& instance();

    // ── Unified Tool Discovery ──────────────────────────────────────────

    /// Get all available tools (internal + external, cached 5 s)
    std::vector<UnifiedTool> get_all_tools();

    /// Filtered variant — same `ToolFilter` semantics as
    /// `format_tools_for_openai(filter)`. Lets non-OpenAI builders
    /// (Anthropic, Gemini, Fincept text catalog) honour the same scope.
    std::vector<UnifiedTool> get_all_tools(const ToolFilter& filter);

    /// Format for OpenAI function calling — full catalogue.
    QJsonArray format_tools_for_openai();

    /// Phase 6: format with a filter applied. Lets callers scope the
    /// catalogue per-LLM-request to reduce prompt size and tool-pick noise.
    /// Equivalent to format_tools_for_openai() when filter is default-constructed.
    QJsonArray format_tools_for_openai(const ToolFilter& filter);

    std::size_t tool_count();

    // ── Unified Tool Execution ──────────────────────────────────────────

    /// Route to internal or external server
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

    void refresh_cache();             // requires mutex_ held
    bool is_cache_valid() const;      // requires mutex_ held

    // Snapshot the unfiltered cached tool list (refreshing if stale). Caller
    // must hold mutex_. Splitting this out lets get_all_tools(filter) and
    // format_tools_for_openai(filter) reuse the cache without re-locking.
    const std::vector<UnifiedTool>& cached_tools_locked();

    // Stable byte signature of a ToolFilter — usable as a QHash key.
    static QByteArray filter_signature(const ToolFilter& filter);
};

} // namespace fincept::mcp
