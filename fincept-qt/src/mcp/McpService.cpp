// McpService.cpp — Unified tool interface (Qt port)

#include "mcp/McpService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"

#include <QJsonDocument>
#include <QPromise>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

namespace fincept::mcp {

static constexpr const char* TAG = "McpService";

McpService& McpService::instance() {
    static McpService s;
    return s;
}

// ============================================================================
// Lifecycle
// ============================================================================

void McpService::initialize() {
    // Load external server configs from DB (fast, synchronous)
    McpManager::instance().initialize();

    // Invalidate tool cache whenever external servers change (start/stop/add/remove)
    QObject::connect(&McpManager::instance(), &McpManager::servers_changed, [this]() {
        QMutexLocker lock(&mutex_);
        cache_time_ = QDateTime(); // force refresh on next get_all_tools()
        LOG_INFO(TAG, "Tool cache invalidated — external servers changed");
    });

    // Auto-start enabled servers in a background thread so the UI never
    // freezes. Only servers with both enabled AND auto_start flags are
    // started at launch. Users can manually enable others from the MCP tab.
    const auto servers = McpManager::instance().get_servers();
    QStringList to_start;
    for (const auto& srv : servers) {
        if (srv.enabled && srv.auto_start)
            to_start.append(srv.id);
    }

    if (!to_start.isEmpty()) {
        QThread* t = QThread::create([to_start]() {
            for (const auto& id : to_start) {
                LOG_INFO("McpService", "Auto-starting MCP server: " + id);
                auto r = McpManager::instance().start_server(id);
                if (r.is_err())
                    LOG_WARN("McpService", "Auto-start failed for " + id + ": " + QString::fromStdString(r.error()));
            }
            LOG_INFO("McpService", "All external MCP servers started");
        });
        t->setObjectName("mcp-autostart");
        t->start();
        QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    }

    McpManager::instance().start_health_check();

    LOG_INFO(TAG, QString("McpService initialized — %1 internal tools, %2 external servers queued")
                      .arg(McpProvider::instance().tool_count())
                      .arg(to_start.size()));
}

void McpService::shutdown() {
    McpManager::instance().shutdown();
    McpProvider::instance().clear();
    LOG_INFO(TAG, "McpService shut down");
}

// ============================================================================
// Tool Discovery
// ============================================================================

std::vector<UnifiedTool> McpService::get_all_tools() {
    QMutexLocker lock(&mutex_);
    return cached_tools_locked();
}

const std::vector<UnifiedTool>& McpService::cached_tools_locked() {
    if (!is_cache_valid())
        refresh_cache();
    return cached_tools_;
}

QByteArray McpService::filter_signature(const ToolFilter& f) {
    // Deterministic, compact, mutation-free. The component prefixes ('c=',
    // 'x=', etc.) prevent collisions when one field is empty and another
    // contains a literal pipe. max_tools=0 means "no cap" — kept in the key
    // so cap changes invalidate.
    QByteArray key;
    key.reserve(256);
    key += "c=" + f.categories.join('|').toUtf8() + ';';
    key += "xc=" + f.exclude_categories.join('|').toUtf8() + ';';
    key += "n=" + f.name_patterns.join('|').toUtf8() + ';';
    key += "xn=" + f.exclude_name_patterns.join('|').toUtf8() + ';';
    key += "m=" + QByteArray::number(f.max_tools);
    return key;
}

// Production safety cap: when a caller leaves max_tools at 0 (= unlimited),
// we still refuse to dump the full catalogue at the LLM. Tool-pick accuracy
// degrades sharply past ~40 tools; ~150 KB of schema also costs prompt
// tokens. Tool RAG / Tier-0 mode renders this mostly historical — kept as a
// defence-in-depth backstop for callers that bypass Tier-0 by passing a
// non-default ToolFilter.
static constexpr int kHardMaxTools = 50;

// Apply ToolFilter (categories include/exclude, name regex include/exclude,
// max_tools cap) to a list of UnifiedTools. Pulled out as a free function so
// both the OpenAI-format path and the Anthropic / Gemini / Fincept catalog
// builders can share identical filter semantics.
//
// Telemetry: counts how many candidates were truncated by the max_tools
// cap and emits a warning so we can spot screens whose Tier-2 categories
// are too wide. Truncation isn't an error — but it is a signal.
static std::vector<UnifiedTool> apply_tool_filter(std::vector<UnifiedTool> tools, const ToolFilter& filter) {
    QList<QRegularExpression> include_rx;
    for (const auto& p : filter.name_patterns) include_rx.append(QRegularExpression(p));
    QList<QRegularExpression> exclude_rx;
    for (const auto& p : filter.exclude_name_patterns) exclude_rx.append(QRegularExpression(p));

    const int effective_cap = filter.max_tools > 0 ? filter.max_tools : kHardMaxTools;

    // Stage 1 — keep candidates that pass include/exclude predicates.
    // Cap is applied after sort, not during iteration, so a stable order
    // is preserved regardless of registration order.
    std::vector<UnifiedTool> kept;
    kept.reserve(tools.size());
    for (auto& tool : tools) {
        if (!filter.categories.isEmpty()) {
            if (tool.category.isEmpty() || !filter.categories.contains(tool.category))
                continue;
        }
        if (!filter.exclude_categories.isEmpty() && filter.exclude_categories.contains(tool.category))
            continue;

        if (!include_rx.isEmpty()) {
            bool any_match = false;
            for (const auto& rx : include_rx) {
                if (rx.match(tool.name).hasMatch()) { any_match = true; break; }
            }
            if (!any_match) continue;
        }
        bool excluded = false;
        for (const auto& rx : exclude_rx) {
            if (rx.match(tool.name).hasMatch()) { excluded = true; break; }
        }
        if (excluded) continue;

        kept.push_back(std::move(tool));
    }

    // Stage 2 — deterministic sort. Anthropic's prompt cache only hits on
    // a byte-identical prefix, so the tool block order MUST be stable.
    // Primary key: index in filter.categories (categories listed first by
    // the caller cluster at the top). Tools whose category isn't in the
    // include list (external tools with empty category) sort to the end
    // with a sentinel rank.
    // Secondary key: tool name alphabetical.
    QHash<QString, int> cat_rank;
    for (int i = 0; i < filter.categories.size(); ++i)
        cat_rank.insert(filter.categories[i], i);
    const int sentinel = filter.categories.size();

    std::stable_sort(kept.begin(), kept.end(),
                     [&cat_rank, sentinel](const UnifiedTool& a, const UnifiedTool& b) {
                         const int ra = cat_rank.value(a.category, sentinel);
                         const int rb = cat_rank.value(b.category, sentinel);
                         if (ra != rb) return ra < rb;
                         return a.name < b.name;
                     });

    // Stage 3 — apply cap.
    const int kept_before_cap = static_cast<int>(kept.size());
    if (static_cast<int>(kept.size()) > effective_cap)
        kept.resize(effective_cap);

    if (kept_before_cap > effective_cap) {
        LOG_WARN(TAG, QString("apply_tool_filter: truncated %1 → %2 (cap=%3, configured_cap=%4) — "
                              "consider tightening categories for this screen/agent")
                          .arg(kept_before_cap).arg(kept.size())
                          .arg(effective_cap).arg(filter.max_tools));
    }
    return kept;
}

std::vector<UnifiedTool> McpService::get_all_tools(const ToolFilter& filter) {
    QMutexLocker lock(&mutex_);
    // Refresh cached_tools_ if stale — also clears filter caches.
    cached_tools_locked();

    const QByteArray key = filter_signature(filter);
    auto it = filtered_tools_cache_.constFind(key);
    if (it != filtered_tools_cache_.constEnd())
        return it.value();

    auto filtered = apply_tool_filter(cached_tools_, filter);
    filtered_tools_cache_.insert(key, filtered);
    return filtered;
}

QJsonArray McpService::format_tools_for_openai() {
    return format_tools_for_openai(ToolFilter{});
}

// Tool RAG Tier-0 — the always-loaded set when mcp_use_tool_rag is on.
//
// Rationale: when Tool RAG is active we send only this 6-tool prefix to the
// LLM each turn (~3 KB of schema vs ~25 KB previously). Everything else is
// discoverable via tool.list. Anthropic's recommended band is 3-5 always-on
// tools — we go to 6 to keep navigation working without a search round-trip.
//
// Picked because they're (1) high-frequency, (2) safe (no destructive ops),
// (3) needed before a search would even make sense:
//   tool.list           — entry point to discover everything else
//   tool.describe       — fetch full schema for a discovered tool
//   navigate_to_tab     — UI navigation is the LLM's primary side-effect
//   list_tabs           — what tabs exist?
//   get_current_tab     — where is the user?
//   get_auth_status     — guest vs signed-in changes valid actions
static const QSet<QString>& tier_0_tool_names() {
    static const QSet<QString> kTier0 = {
        "tool.list",
        "tool.describe",
        "navigate_to_tab",
        "list_tabs",
        "get_current_tab",
        "get_auth_status",
    };
    return kTier0;
}

// Whether the user has Tool RAG mode enabled. Flag persists in QSettings via
// AppConfig.
//
// Default = TRUE.
//
// Rationale: Tool RAG (BM25 retrieval over the catalog via tool.list) lifts
// tool-pick accuracy from ~49 → ~74% on Opus 4-class models per Anthropic's
// published numbers, and from ~80 → ~88% on 4.5-class. Sending only ~6
// Tier-0 tools per turn (vs. previously ~50) reduces prompt tokens by ~85%.
// The kill switch is one settings flip away if a specific
// provider/model combo regresses.
static bool tool_rag_enabled() {
    return fincept::AppConfig::instance()
        .get("mcp/use_tool_rag", QVariant(true))
        .toBool();
}

QJsonArray McpService::format_tools_for_openai(const ToolFilter& filter) {
    QMutexLocker lock(&mutex_);
    cached_tools_locked();

    // ── Tool RAG Tier-0 mode ──
    // Engaged only when (a) the setting is on AND (b) the caller passed a
    // default-constructed ToolFilter (i.e. didn't ask for a specific scope).
    // Per-agent / per-screen explicit filters bypass Tier-0 — those callers
    // know what they want and shouldn't be silently overridden.
    const bool default_filter = filter.categories.isEmpty() &&
                                filter.exclude_categories.isEmpty() &&
                                filter.name_patterns.isEmpty() &&
                                filter.exclude_name_patterns.isEmpty() &&
                                filter.max_tools == 0;
    const bool use_rag = default_filter && tool_rag_enabled();

    const QByteArray key = use_rag
        ? QByteArrayLiteral("__tier0__")
        : filter_signature(filter);

    auto cached = openai_format_cache_.constFind(key);
    if (cached != openai_format_cache_.constEnd()) {
        LOG_INFO(TAG, QString("format_tools_for_openai: %1 tools sent to LLM (cached, %2)")
                          .arg(cached.value().size())
                          .arg(use_rag ? "tier-0" : "filtered"));
        return cached.value();
    }

    const int total_seen = static_cast<int>(cached_tools_.size());
    std::vector<UnifiedTool> tools;
    if (use_rag) {
        const auto& tier0 = tier_0_tool_names();
        for (const auto& t : cached_tools_) {
            if (tier0.contains(t.name))
                tools.push_back(t);
        }
    } else {
        tools = apply_tool_filter(cached_tools_, filter);
    }

    QJsonArray result;
    for (const auto& tool : tools) {
        // Encode tool name for the wire so dotted internal names like
        // `tool.list` become `tool-dot-list` — Kimi / OpenAI / Groq reject
        // dots in function names. parse_openai_function_name reverses this
        // when the model invokes the tool.
        QString fn_name = tool.server_id + "__" + McpProvider::encode_tool_name_for_wire(tool.name);

        QJsonObject schema = tool.input_schema;
        if (schema.isEmpty()) {
            schema["type"] = "object";
            schema["properties"] = QJsonObject();
        }

        QJsonObject fn;
        fn["name"] = fn_name;
        fn["description"] = tool.description;
        fn["parameters"] = schema;

        QJsonObject entry;
        entry["type"] = "function";
        entry["function"] = fn;
        result.append(entry);
    }

    openai_format_cache_.insert(key, result);

    if (use_rag) {
        LOG_INFO(TAG, QString("format_tools_for_openai: %1/%2 tools sent to LLM "
                              "(tier-0 / Tool RAG mode, fresh)")
                          .arg(result.size()).arg(total_seen));
    } else if (total_seen != result.size()) {
        LOG_INFO(TAG, QString("format_tools_for_openai: %1/%2 tools sent to LLM (filtered, fresh)")
                          .arg(result.size()).arg(total_seen));
    } else {
        LOG_INFO(TAG, QString("format_tools_for_openai: %1 tools sent to LLM (fresh)").arg(result.size()));
    }
    return result;
}

std::size_t McpService::tool_count() {
    return get_all_tools().size();
}

// ============================================================================
// Tool Execution
// ============================================================================

ToolResult McpService::execute_tool(const QString& server_id, const QString& tool_name, const QJsonObject& args) {
    // Route to internal provider
    if (server_id == INTERNAL_SERVER_ID)
        return McpProvider::instance().call_tool(tool_name, args);

    // Route to external server
    auto result = McpManager::instance().call_external_tool(server_id, tool_name, args);
    if (result.is_err())
        return ToolResult::fail(QString::fromStdString(result.error()));

    const QJsonObject& data = result.value();

    bool is_error = data["isError"].toBool(false);
    QJsonArray content = data["content"].toArray();

    QString text;
    for (const auto& item : content) {
        QJsonObject obj = item.toObject();
        if (obj["type"].toString() == "text")
            text += obj["text"].toString();
    }

    if (is_error)
        return ToolResult::fail(text.isEmpty() ? "External tool error" : text);

    // Try to parse text as JSON data
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isNull()) {
        if (doc.isObject())
            return ToolResult::ok(text, doc.object());
        if (doc.isArray())
            return ToolResult::ok(text, doc.array());
    }

    return ToolResult::ok(text);
}

ToolResult McpService::execute_openai_function(const QString& function_name, const QJsonObject& args) {
    auto [server_id, tool_name] = McpProvider::parse_openai_function_name(function_name);

    if (server_id.isEmpty() || tool_name.isEmpty()) {
        LOG_WARN(TAG, "Invalid function name format: " + function_name);
        return ToolResult::fail("Invalid function name format: " + function_name);
    }

    LOG_INFO(TAG, QString("Dispatch: %1 -> server=%2 tool=%3").arg(function_name, server_id, tool_name));
    auto result = execute_tool(server_id, tool_name, args);
    LOG_INFO(TAG, QString("Dispatch result: %1 success=%2").arg(tool_name, result.success ? "true" : "false"));
    return result;
}

QFuture<ToolResult> McpService::execute_openai_function_async(const QString& function_name, const QJsonObject& args) {
    auto [server_id, tool_name] = McpProvider::parse_openai_function_name(function_name);

    auto fail_now = [](const QString& msg) {
        QPromise<ToolResult> p;
        p.start();
        p.addResult(ToolResult::fail(msg));
        p.finish();
        return p.future();
    };

    if (server_id.isEmpty() || tool_name.isEmpty()) {
        LOG_WARN(TAG, "Invalid function name format: " + function_name);
        return fail_now("Invalid function name format: " + function_name);
    }

    LOG_INFO(TAG, QString("Async dispatch: %1 -> server=%2 tool=%3").arg(function_name, server_id, tool_name));

    // Internal tools — use the native async path. Validation, timeout, and
    // cancellation are handled by McpProvider::call_tool_async.
    if (server_id == INTERNAL_SERVER_ID) {
        return McpProvider::instance().call_tool_async(tool_name, args);
    }

    // External tools — McpManager's JSON-RPC client is blocking by design
    // (it sleeps on a QWaitCondition for the response). Wrap in
    // QtConcurrent::run so multiple external calls can fan out concurrently
    // even if each one blocks its own pool thread. Phase 5's dispatcher
    // unification will use these futures with QtFuture::whenAll to join
    // a round of tool calls in parallel.
    return QtConcurrent::run([server_id, tool_name, args]() -> ToolResult {
        return McpService::instance().execute_tool(server_id, tool_name, args);
    });
}

// ============================================================================
// Validation
// ============================================================================
// Phase 3: validation now lives in mcp::validate_args (SchemaValidator.cpp)
// and is invoked automatically by McpProvider::call_tool. The previous
// McpService::validate_params helper was orphaned (never called from any
// execution path) and has been removed. See plans/mcp-refactor-phase-3-schema-validation.md.

// ============================================================================
// Cache
// ============================================================================

bool McpService::is_cache_valid() const {
    if (McpProvider::instance().generation() != cached_generation_)
        return false;
    if (cache_time_.isNull() || cached_tools_.empty())
        return false;
    return cache_time_.msecsTo(QDateTime::currentDateTime()) < CACHE_TTL_MS;
}

void McpService::refresh_cache() {
    cached_tools_.clear();

    // Internal tools
    auto internal = McpProvider::instance().list_tools();
    cached_tools_.insert(cached_tools_.end(), internal.begin(), internal.end());

    // External tools
    auto external = McpManager::instance().get_all_external_tools();
    for (auto& ext : external) {
        // External tools default category="" (their server doesn't tag) and
        // is_destructive=false (we have no signal for it from the wire — the
        // MCP spec doesn't carry it). Treat external tools as non-destructive
        // until they tell us otherwise.
        cached_tools_.push_back({ext.server_id, ext.server_name, ext.name, ext.description,
                                  ext.input_schema, false, QString{}, false});
    }

    // Invalidate filter-derived caches — they were computed against the
    // previous cached_tools_ snapshot. Memory cost: typically a handful of
    // distinct ToolFilter signatures (one per active screen / agent), each
    // <200 KB, so simple full-clear is fine.
    filtered_tools_cache_.clear();
    openai_format_cache_.clear();

    cache_time_ = QDateTime::currentDateTime();
    cached_generation_ = McpProvider::instance().generation();

    LOG_INFO(TAG, QString("Refreshed tool cache: %1 total (%2 internal, %3 external)")
                      .arg(cached_tools_.size())
                      .arg(internal.size())
                      .arg(external.size()));
}

} // namespace fincept::mcp
