// McpService.cpp — Unified tool interface (Qt port)

#include "mcp/McpService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "mcp/GeminiSchema.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"
#include "mcp/TerminalMcpBridge.h"
#include "mcp/ToolRetriever.h"
#include "storage/repositories/SettingsRepository.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPromise>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::mcp {

static constexpr const char* TAG = "McpService";

// Per-request declaration ceilings — a backstop, not the primary limit.
//
// kHardMaxTools (50, below) already bounds the non-Tier-0 path, and a Tier-0
// turn ships ~12. These exist so that a caller who raises ToolFilter::max_tools
// can't silently walk a provider past a hard API limit: Gemini rejects a
// request carrying more than 128 function declarations, and does so without
// naming the offender, which is indistinguishable from "tools are broken".
static constexpr int kGeminiMaxToolsPerRequest = 128;
static constexpr int kMaxToolsPerRequest = 256;

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
        // The Tool RAG index covers external tools too (ToolRetriever builds
        // over get_all_tools()), so it must follow this cache. Queued, never
        // inline: servers_changed can fire while McpManager's mutex is held
        // (error paths), and the retriever rebuild takes ToolRetriever →
        // McpService → McpManager locks — invalidating inline here would
        // invert that order.
        QMetaObject::invokeMethod(qApp, []() { ToolRetriever::instance().invalidate(); }, Qt::QueuedConnection);
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

    // The user's per-tool on/off choices (MCP Servers screen) — the catalogue
    // is fully registered by now (McpInit calls initialize() last).
    apply_persisted_tool_states();

    LOG_INFO(TAG, QString("McpService initialized — %1 internal tools, %2 external servers queued")
                      .arg(McpProvider::instance().tool_count())
                      .arg(to_start.size()));
}

// ============================================================================
// Persisted tool enable/disable
// ============================================================================

namespace {
constexpr const char* kMcpDisabledToolsKey = "mcp.disabled_tools";

// The persisted disabled set. `ok` is false when the read itself failed — the
// caller must then not write the set back (SettingsRepository::get contract).
QStringList mcp_svc_read_disabled_tools(bool* ok) {
    const auto r = SettingsRepository::instance().get(QString::fromLatin1(kMcpDisabledToolsKey), QStringLiteral("[]"));
    if (r.is_err()) {
        if (ok)
            *ok = false;
        return {};
    }
    if (ok)
        *ok = true;
    QStringList names;
    for (const auto& v : QJsonDocument::fromJson(r.value().toUtf8()).array()) {
        const QString n = v.toString().trimmed();
        if (!n.isEmpty() && !names.contains(n))
            names.append(n);
    }
    return names;
}
} // namespace

void McpService::set_tool_enabled_persistent(const QString& tool_name, bool enabled) {
    if (tool_name.trimmed().isEmpty())
        return;
    McpProvider::instance().set_tool_enabled(tool_name, enabled);

    bool ok = false;
    QStringList disabled = mcp_svc_read_disabled_tools(&ok);
    if (!ok) {
        // Writing now would replace the user's stored set with this one entry.
        LOG_WARN(TAG, QString("Tool '%1' %2 for this session only — could not read the saved tool states")
                          .arg(tool_name, enabled ? "enabled" : "disabled"));
        return;
    }
    if (enabled)
        disabled.removeAll(tool_name);
    else if (!disabled.contains(tool_name))
        disabled.append(tool_name);

    QJsonArray arr;
    for (const auto& n : disabled)
        arr.append(n);
    const auto w = SettingsRepository::instance().set(
        QString::fromLatin1(kMcpDisabledToolsKey), QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)),
        QStringLiteral("mcp"));
    if (w.is_err())
        LOG_WARN(
            TAG,
            QString("Could not persist tool state for '%1': %2").arg(tool_name, QString::fromStdString(w.error())));
}

void McpService::apply_persisted_tool_states() {
    bool ok = false;
    const QStringList disabled = mcp_svc_read_disabled_tools(&ok);
    if (!ok) {
        LOG_WARN(TAG, "Could not read saved tool states — every tool starts enabled this session");
        return;
    }
    int applied = 0;
    auto& provider = McpProvider::instance();
    for (const auto& name : disabled) {
        // A name that no longer exists (renamed/removed tool) is kept in the
        // saved set harmlessly; it simply has nothing to disable.
        if (!provider.has_tool(name))
            continue;
        provider.set_tool_enabled(name, false);
        ++applied;
    }
    if (applied > 0)
        LOG_INFO(TAG, QString("Applied %1 user-disabled tool(s)").arg(applied));
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

// How a filtered selection is ordered and capped. The two consumers differ on
// purpose: an LLM provider payload must stay small and byte-stable (prompt
// caching), an agent catalogue must not lose tools the user asked for.
enum class McpToolSelection {
    LlmPayload,     // category rank → name; max_tools 0 ⇒ kHardMaxTools
    AgentCatalogue, // read-only first → category rank → name; max_tools 0 ⇒ no cap
};

// Apply ToolFilter (categories include/exclude, name regex include/exclude,
// max_tools cap) to a list of UnifiedTools. Pulled out as a free function so
// the OpenAI-format path, the Anthropic / Gemini / Fincept catalog builders and
// the agent catalogue share identical predicate semantics.
//
// Telemetry: counts how many candidates were truncated by the max_tools
// cap and emits a warning so we can spot screens whose Tier-2 categories
// are too wide. Truncation isn't an error — but it is a signal.
static std::vector<UnifiedTool> apply_tool_filter(std::vector<UnifiedTool> tools, const ToolFilter& filter,
                                                  McpToolSelection mode = McpToolSelection::LlmPayload) {
    QList<QRegularExpression> include_rx;
    for (const auto& p : filter.name_patterns)
        include_rx.append(QRegularExpression(p));
    QList<QRegularExpression> exclude_rx;
    for (const auto& p : filter.exclude_name_patterns)
        exclude_rx.append(QRegularExpression(p));

    const int effective_cap = filter.max_tools > 0                       ? filter.max_tools
                              : mode == McpToolSelection::AgentCatalogue ? 0
                                                                         : kHardMaxTools;

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
                if (rx.match(tool.name).hasMatch()) {
                    any_match = true;
                    break;
                }
            }
            if (!any_match)
                continue;
        }
        bool excluded = false;
        for (const auto& rx : exclude_rx) {
            if (rx.match(tool.name).hasMatch()) {
                excluded = true;
                break;
            }
        }
        if (excluded)
            continue;

        kept.push_back(std::move(tool));
    }

    // Stage 2 — deterministic sort. Anthropic's prompt cache only hits on
    // a byte-identical prefix, so the tool block order MUST be stable.
    // Primary key: index in filter.categories (categories listed first by
    // the caller cluster at the top). Tools whose category isn't in the
    // include list (external tools with empty category) sort to the end
    // with a sentinel rank.
    // Secondary key: tool name alphabetical.
    // Agent catalogues put read-only tools ahead of everything else so that a
    // user-capped catalogue keeps the research tools rather than the mutators.
    QHash<QString, int> cat_rank;
    for (int i = 0; i < filter.categories.size(); ++i)
        cat_rank.insert(filter.categories[i], i);
    const int sentinel = filter.categories.size();
    const bool read_only_first = mode == McpToolSelection::AgentCatalogue;

    std::stable_sort(kept.begin(), kept.end(),
                     [&cat_rank, sentinel, read_only_first](const UnifiedTool& a, const UnifiedTool& b) {
                         if (read_only_first && a.is_destructive != b.is_destructive)
                             return !a.is_destructive;
                         const int ra = cat_rank.value(a.category, sentinel);
                         const int rb = cat_rank.value(b.category, sentinel);
                         if (ra != rb)
                             return ra < rb;
                         return a.name < b.name;
                     });

    // Stage 3 — apply cap (0 = uncapped, agent catalogues only).
    const int kept_before_cap = static_cast<int>(kept.size());
    if (effective_cap > 0 && kept_before_cap > effective_cap)
        kept.resize(effective_cap);

    if (effective_cap > 0 && kept_before_cap > effective_cap) {
        LOG_WARN(TAG, QString("apply_tool_filter: truncated %1 → %2 (cap=%3, configured_cap=%4) — "
                              "consider tightening categories for this screen/agent")
                          .arg(kept_before_cap)
                          .arg(kept.size())
                          .arg(effective_cap)
                          .arg(filter.max_tools));
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

std::vector<UnifiedTool> McpService::get_agent_tools(const ToolFilter& filter) {
    QMutexLocker lock(&mutex_);
    cached_tools_locked();

    // Separate key space from the LLM selection above: same filter, different
    // ordering and cap semantics.
    const QByteArray key = QByteArrayLiteral("agent|") + filter_signature(filter);
    auto it = filtered_tools_cache_.constFind(key);
    if (it != filtered_tools_cache_.constEnd())
        return it.value();

    auto selected = apply_tool_filter(cached_tools_, filter, McpToolSelection::AgentCatalogue);
    filtered_tools_cache_.insert(key, selected);
    return selected;
}

// Tool RAG Tier-0 — the always-loaded set when mcp_use_tool_rag is on.
//
// Rationale: when Tool RAG is active we send only this 6-tool prefix to the
// LLM each turn (~3 KB of schema vs ~25 KB previously). Everything else is
// discoverable via tool_list. Anthropic's recommended band is 3-5 always-on
// tools — we go to 6 to keep navigation working without a search round-trip.
//
// Picked because they're (1) high-frequency, (2) safe (no destructive ops),
// (3) needed before a search would even make sense:
//   tool_list           — entry point to discover everything else
//   tool_describe       — fetch full schema for a discovered tool
//   navigate_to_tab     — UI navigation is the LLM's primary side-effect
//   list_tabs           — what tabs exist?
//   get_current_tab     — where is the user?
//   get_auth_status     — guest vs signed-in changes valid actions
static const QSet<QString>& tier_0_tool_names() {
    static const QSet<QString> kTier0 = {
        "tool_list",
        "tool_describe",
        // Generic dispatcher: calls any tool found via tool_list by name. The
        // activation path re-declares discovered tools, but it is capped
        // (kMaxActivatedTools) and some models will not call a function that
        // only appeared mid-turn; tool_invoke is the always-available route.
        "tool_invoke",
        "navigate_to_tab",
        "list_tabs",
        "get_current_tab",
        "get_auth_status",
        // Always-visible so the model checks chat→report linkage before any
        // report-builder work — prevents new chats from accidentally appending
        // to the previous chat's report.
        "report_session_context",
        // Redemption tools for the two deferral protocols. A model that has
        // been handed a `{job_id, status:"running"}` receipt, or a truncated
        // result envelope carrying a `result_id`, must be able to act on it
        // immediately — forcing a tool_list round-trip first would cost more
        // tokens than these five small schemas do.
        "job_status",
        "job_result",
        "job_cancel",
        "job_list",
        "result_fetch",
    };
    return kTier0;
}

// Whether the user has Tool RAG mode enabled. Flag persists in QSettings via
// AppConfig.
//
// Default = TRUE.
//
// Rationale: Tool RAG (BM25 retrieval over the catalog via tool_list) lifts
// tool-pick accuracy from ~49 → ~74% on Opus 4-class models per Anthropic's
// published numbers, and from ~80 → ~88% on 4.5-class. Sending only ~6
// Tier-0 tools per turn (vs. previously ~50) reduces prompt tokens by ~85%.
// The kill switch is one settings flip away if a specific
// provider/model combo regresses.
static bool tool_rag_enabled() {
    return fincept::AppConfig::instance().get("mcp/use_tool_rag", QVariant(true)).toBool();
}

QJsonArray McpService::format_tools_for_openai(const ToolFilter& filter) {
    return format_tools_for_openai(filter, QSet<QString>{});
}

std::vector<UnifiedTool> McpService::select_tools_for_llm_locked(const ToolFilter& filter,
                                                                 const QSet<QString>& extra_tool_names,
                                                                 QByteArray* out_key, bool* out_used_rag) {
    cached_tools_locked();

    // ── Tool RAG Tier-0 mode ──
    // Engaged only when (a) the setting is on AND (b) the caller passed a
    // default-constructed ToolFilter (i.e. didn't ask for a specific scope).
    // Per-agent / per-screen explicit filters bypass Tier-0 — those callers
    // know what they want and shouldn't be silently overridden.
    const bool default_filter = filter.categories.isEmpty() && filter.exclude_categories.isEmpty() &&
                                filter.name_patterns.isEmpty() && filter.exclude_name_patterns.isEmpty() &&
                                filter.max_tools == 0;
    const bool use_rag = default_filter && tool_rag_enabled();

    QByteArray key;
    if (use_rag) {
        key = QByteArrayLiteral("__tier0__");
        // Activated tools widen the Tier-0 set, so they must vary the cache key
        // — otherwise round 2 would be served round 1's cached 7-tool array and
        // the model would never see the tool it just discovered.
        if (!extra_tool_names.isEmpty()) {
            QStringList sorted(extra_tool_names.constBegin(), extra_tool_names.constEnd());
            sorted.sort();
            key += '|';
            key += sorted.join(QLatin1Char(',')).toUtf8();
        }
    } else {
        key = filter_signature(filter);
    }

    std::vector<UnifiedTool> tools;
    if (use_rag) {
        const auto& tier0 = tier_0_tool_names();
        for (const auto& t : cached_tools_) {
            if (tier0.contains(t.name) || extra_tool_names.contains(t.name))
                tools.push_back(t);
        }
    } else {
        tools = apply_tool_filter(cached_tools_, filter);
    }

    if (out_key)
        *out_key = key;
    if (out_used_rag)
        *out_used_rag = use_rag;
    return tools;
}

// Bound any provider's tool array. In a Tier-0 turn this never binds (12-ish
// declarations); it only matters when Tool RAG is switched off, where the raw
// catalogue is ~900 tools — past every provider's declaration ceiling and
// enough prompt to crowd out the conversation. Dropping silently would read as
// "the model can't see my tool", so overflow is always logged.
static std::vector<UnifiedTool> cap_tool_list(std::vector<UnifiedTool> tools, int cap, const char* dialect) {
    if (cap <= 0 || static_cast<int>(tools.size()) <= cap)
        return tools;
    LOG_WARN(TAG, QString("%1: %2 tools selected but the dialect accepts at most %3 — sending the first %3. "
                          "Enable Tool RAG (mcp/use_tool_rag) or pass a narrower ToolFilter.")
                      .arg(QString::fromUtf8(dialect))
                      .arg(tools.size())
                      .arg(cap));
    tools.resize(static_cast<std::size_t>(cap));
    return tools;
}

QJsonArray McpService::format_tools_for_openai(const ToolFilter& filter, const QSet<QString>& extra_tool_names) {
    QMutexLocker lock(&mutex_);

    QByteArray key;
    bool use_rag = false;
    std::vector<UnifiedTool> tools = select_tools_for_llm_locked(filter, extra_tool_names, &key, &use_rag);

    auto cached = openai_format_cache_.constFind(key);
    if (cached != openai_format_cache_.constEnd()) {
        LOG_INFO(TAG, QString("format_tools_for_openai: %1 tools sent to LLM (cached, %2)")
                          .arg(cached.value().size())
                          .arg(use_rag ? "tier-0" : "filtered"));
        return cached.value();
    }

    const int total_seen = static_cast<int>(cached_tools_.size());
    tools = cap_tool_list(std::move(tools), kMaxToolsPerRequest, "openai");

    QJsonArray result;
    for (const auto& tool : tools) {
        // Encode tool name for the wire so dotted internal names like
        // `tool.list` becomes `tool-dot-list` — Kimi / OpenAI / Groq reject
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

    // Bound the cache. In Tool RAG mode the key encodes the activated-tool set,
    // which is different on almost every round of every turn — left unbounded
    // this map grows for the life of the process, each entry holding a full
    // serialised tool array. It's a memo, not a store: dropping it wholesale
    // when it gets big costs one rebuild.
    if (openai_format_cache_.size() >= kMaxFormatCacheEntries) {
        LOG_DEBUG(TAG, QString("openai_format_cache_ hit %1 entries — clearing").arg(openai_format_cache_.size()));
        openai_format_cache_.clear();
    }
    openai_format_cache_.insert(key, result);

    if (use_rag) {
        LOG_INFO(TAG, QString("format_tools_for_openai: %1/%2 tools sent to LLM "
                              "(tier-0 + %3 activated / Tool RAG mode, fresh)")
                          .arg(result.size())
                          .arg(total_seen)
                          .arg(extra_tool_names.size()));
    } else if (total_seen != result.size()) {
        LOG_INFO(TAG, QString("format_tools_for_openai: %1/%2 tools sent to LLM (filtered, fresh)")
                          .arg(result.size())
                          .arg(total_seen));
    } else {
        LOG_INFO(TAG, QString("format_tools_for_openai: %1 tools sent to LLM (fresh)").arg(result.size()));
    }
    return result;
}

QJsonArray McpService::format_tools_for_anthropic(const ToolFilter& filter, const QSet<QString>& extra_tool_names) {
    QMutexLocker lock(&mutex_);

    QByteArray key;
    bool use_rag = false;
    std::vector<UnifiedTool> tools = select_tools_for_llm_locked(filter, extra_tool_names, &key, &use_rag);

    auto cached = anthropic_format_cache_.constFind(key);
    if (cached != anthropic_format_cache_.constEnd())
        return cached.value();

    tools = cap_tool_list(std::move(tools), kMaxToolsPerRequest, "anthropic");

    QJsonArray result;
    for (const auto& tool : tools) {
        QJsonObject schema = tool.input_schema;
        // Anthropic requires input_schema to be a JSON Schema object. A
        // parameterless tool is legal here (unlike Gemini) as long as the
        // object is well-formed.
        if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object")) {
            schema[QStringLiteral("type")] = QStringLiteral("object");
        }
        if (!schema.contains(QStringLiteral("properties")))
            schema[QStringLiteral("properties")] = QJsonObject();

        result.append(QJsonObject{
            {QStringLiteral("name"), tool.server_id + QStringLiteral("__") + McpProvider::encode_tool_name_for_wire(tool.name)},
            {QStringLiteral("description"), tool.description},
            {QStringLiteral("input_schema"), schema},
        });
    }

    if (anthropic_format_cache_.size() >= kMaxFormatCacheEntries)
        anthropic_format_cache_.clear();
    anthropic_format_cache_.insert(key, result);

    LOG_INFO(TAG, QString("format_tools_for_anthropic: %1 tools sent to LLM (%2)")
                      .arg(result.size())
                      .arg(use_rag ? "tier-0" : "filtered"));
    return result;
}

QJsonArray McpService::format_tools_for_gemini(const ToolFilter& filter, const QSet<QString>& extra_tool_names) {
    QMutexLocker lock(&mutex_);

    QByteArray key;
    bool use_rag = false;
    std::vector<UnifiedTool> tools = select_tools_for_llm_locked(filter, extra_tool_names, &key, &use_rag);

    auto cached = gemini_format_cache_.constFind(key);
    if (cached != gemini_format_cache_.constEnd())
        return cached.value();

    tools = cap_tool_list(std::move(tools), kGeminiMaxToolsPerRequest, "gemini");

    QJsonArray fn_decls;
    int dropped_names = 0;
    for (const auto& tool : tools) {
        const QString fn_name =
            tool.server_id + QStringLiteral("__") + McpProvider::encode_tool_name_for_wire(tool.name);
        if (!is_valid_gemini_function_name(fn_name)) {
            // One invalid name fails the entire generateContent call, taking
            // every other tool down with it. Drop this one instead.
            ++dropped_names;
            LOG_WARN(TAG, QString("format_tools_for_gemini: dropping '%1' — not a valid Gemini function name "
                                  "(must match ^[a-zA-Z_][a-zA-Z0-9_.:-]{0,63}$)")
                              .arg(fn_name));
            continue;
        }

        QJsonObject decl{{QStringLiteral("name"), fn_name}, {QStringLiteral("description"), tool.description}};
        // `parameters` is OMITTED for a parameterless tool. Sending
        // {"type":"object","properties":{}} — which is what the OpenAI-shaped
        // builders emit — is rejected with "properties: should be non-empty
        // for OBJECT type" and fails the whole request.
        if (auto params = sanitize_schema_for_gemini(tool.input_schema))
            decl[QStringLiteral("parameters")] = *params;
        fn_decls.append(decl);
    }

    QJsonArray result;
    if (!fn_decls.isEmpty())
        result.append(QJsonObject{{QStringLiteral("functionDeclarations"), fn_decls}});

    if (gemini_format_cache_.size() >= kMaxFormatCacheEntries)
        gemini_format_cache_.clear();
    gemini_format_cache_.insert(key, result);

    LOG_INFO(TAG, QString("format_tools_for_gemini: %1 function declarations sent to LLM (%2%3)")
                      .arg(fn_decls.size())
                      .arg(use_rag ? "tier-0" : "filtered")
                      .arg(dropped_names > 0 ? QString(", %1 dropped on name").arg(dropped_names) : QString()));
    return result;
}

std::size_t McpService::tool_count() {
    return get_all_tools().size();
}

QJsonObject McpService::input_schema_for_function(const QString& function_name) {
    auto [server_id, tool_name] = McpProvider::parse_openai_function_name(function_name);
    if (server_id.isEmpty() || tool_name.isEmpty())
        return {};
    QMutexLocker lock(&mutex_);
    for (const auto& t : cached_tools_locked()) {
        if (t.server_id == server_id && t.name == tool_name)
            return t.input_schema;
    }
    return {};
}

// ============================================================================
// Tool Execution
// ============================================================================

McpService::ToolTarget McpService::resolve_tool_target(const QString& name, const QString& server_id) {
    ToolTarget t;
    const QString bare = name.trimmed();
    const QString sid = server_id.trimmed();
    if (bare.isEmpty()) {
        t.error = QStringLiteral("No tool name given.");
        return t;
    }

    auto& provider = McpProvider::instance();
    if (sid.isEmpty() || sid == QLatin1String(INTERNAL_SERVER_ID)) {
        if (const auto info = provider.find_tool(bare)) {
            t.server_id = QString::fromLatin1(INTERNAL_SERVER_ID);
            t.category = info->category;
            t.is_destructive = info->is_destructive;
            return t;
        }
        if (provider.has_tool(bare)) {
            t.error = QStringLiteral("Tool '%1' is disabled by the user.").arg(bare);
            return t;
        }
        if (!sid.isEmpty()) {
            t.error = QStringLiteral("No built-in tool named '%1'. Use tool_list to search for tools.").arg(bare);
            return t;
        }
    }

    QStringList servers;
    for (const auto& u : get_all_tools()) {
        if (u.is_internal || u.name != bare)
            continue;
        if (!sid.isEmpty() && u.server_id != sid)
            continue;
        servers << u.server_id;
    }
    if (servers.isEmpty()) {
        t.error = QStringLiteral("No tool named '%1'%2. Use tool_list to search for tools.")
                      .arg(bare, sid.isEmpty() ? QString() : QStringLiteral(" on server '%1'").arg(sid));
        return t;
    }
    if (servers.size() > 1) {
        t.error = QStringLiteral("Tool '%1' exists on several servers (%2) — pass server_id.")
                      .arg(bare, servers.join(QStringLiteral(", ")));
        return t;
    }
    t.server_id = servers.front();
    t.is_destructive = true; // MCP wire carries no destructiveness metadata (see refresh_cache)
    return t;
}

ToolResult McpService::execute_tool(const QString& server_id, const QString& tool_name, const QJsonObject& args,
                                    bool allow_defer) {
    // Route to internal provider
    if (server_id == INTERNAL_SERVER_ID) {
        if (!allow_defer)
            return McpProvider::instance().call_tool(tool_name, args);
        // How long a supports_async tool may run inline before it backgrounds
        // itself. Clamped: below ~250 ms almost everything would background
        // (costing an extra round-trip for nothing), above ~30 s the deferral
        // stops buying anything over just blocking.
        const int grace_ms =
            std::clamp(AppConfig::instance().get("mcp/job_grace_ms", QVariant(kMcpJobGraceMs)).toInt(), 250, 30000);
        return McpProvider::instance().call_tool_or_defer(tool_name, args, grace_ms);
    }

    // Route to external server — through the same Phase 6.3 auth/destructive
    // gate internal tools get inside call_tool_async (previously this path
    // bypassed it entirely). The MCP wire carries no auth/destructiveness
    // metadata, so external tools are gated destructive-by-default: the
    // installed checker must approve them exactly like a destructive internal
    // tool (agent-originated calls are denied unless the agent opts in).
    // `destructive_declared=false`: the `true` above is OUR conservative
    // assumption, not something the external tool declared. Marking it as
    // undeclared keeps these on the checker-only path instead of the
    // fail-closed capability gate, so a user who configured a Notion/Postgres
    // server in the MCP Servers tab does not have to also flip the destructive
    // switch. Agent-originated calls are still denied by the checker unless the
    // agent opted in with the destructive token.
    if (auto denied = McpProvider::instance().check_authorization(server_id + "__" + tool_name, AuthLevel::None,
                                                                  /*is_destructive=*/true,
                                                                  /*destructive_declared=*/false))
        return *denied;

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
    // Parsed payloads go in `data` ONLY. Passing the raw text as `message` as well put every
    // JSON result in the transcript twice, and each later round re-bills both copies (§M3);
    // consumers (workflow MCP node, the tool loop) read `data` first.
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isNull()) {
        if (doc.isObject())
            return ToolResult::ok_data(doc.object());
        if (doc.isArray())
            return ToolResult::ok_data(doc.array());
    }

    return ToolResult::ok(text);
}

ToolResult McpService::execute_openai_function(const QString& function_name, const QJsonObject& args,
                                               bool allow_defer) {
    auto [server_id, tool_name] = McpProvider::parse_openai_function_name(function_name);

    if (server_id.isEmpty() || tool_name.isEmpty()) {
        // parse_openai_function_name only fails to resolve when NO registered
        // tool matches, so this is "no such tool", not a syntax problem. Saying
        // "invalid format" sent models off re-spelling a name that simply does
        // not exist — one turn retried a hallucinated create_workbook/add_tab
        // API repeatedly instead of concluding the tools weren't there.
        LOG_WARN(TAG, "No such tool: " + function_name);
        return ToolResult::fail("No tool named '" + function_name +
                                "' exists. Do not retry this name. Call tool_list with a description of what you "
                                "want to do to find the tools that actually exist.");
    }

    LOG_INFO(TAG, QString("Dispatch: %1 -> server=%2 tool=%3").arg(function_name, server_id, tool_name));
    auto result = execute_tool(server_id, tool_name, args, allow_defer);
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
        LOG_WARN(TAG, "No such tool: " + function_name);
        return fail_now("No tool named '" + function_name +
                        "' exists. Do not retry this name. Call tool_list with a description of what you want to "
                        "do to find the tools that actually exist.");
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
    // Carry the agent-gating flags across the thread hop. execute_tool() runs
    // the auth check, which reads TerminalMcpBridge's thread_local flags; those
    // default to false on the pool thread, so without re-establishing them an
    // agent-originated external call would be mis-classified as a chat call and
    // skip the destructive-tool gate.
    const bool call_in_progress = TerminalMcpBridge::is_call_in_progress();
    const bool destructive_allowed = TerminalMcpBridge::is_destructive_allowed();
    return QtConcurrent::run([server_id, tool_name, args, call_in_progress, destructive_allowed]() -> ToolResult {
        TerminalMcpBridge::ScopedCallFlags flags(call_in_progress, destructive_allowed);
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
        // is_destructive=true — the MCP spec carries no destructiveness
        // signal on the wire, so treat external tools as destructive-by-
        // default. Keeps tool_list / Tool RAG surfacing honest and matches
        // execute_tool(), which gates them like destructive internal tools.
        cached_tools_.push_back(
            {ext.server_id, ext.server_name, ext.name, ext.description, ext.input_schema, false, QString{}, true});
    }

    // Invalidate filter-derived caches — they were computed against the
    // previous cached_tools_ snapshot. Memory cost: typically a handful of
    // distinct ToolFilter signatures (one per active screen / agent), each
    // <200 KB, so simple full-clear is fine.
    filtered_tools_cache_.clear();
    openai_format_cache_.clear();
    anthropic_format_cache_.clear();
    gemini_format_cache_.clear();

    cache_time_ = QDateTime::currentDateTime();
    cached_generation_ = McpProvider::instance().generation();

    LOG_INFO(TAG, QString("Refreshed tool cache: %1 total (%2 internal, %3 external)")
                      .arg(cached_tools_.size())
                      .arg(internal.size())
                      .arg(external.size()));
}

} // namespace fincept::mcp
