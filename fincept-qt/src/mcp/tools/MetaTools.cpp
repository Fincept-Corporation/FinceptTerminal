// MetaTools.cpp — Self-introspection tools (tool_list, tool_describe, tool_invoke,
// mcp_health).

#include "mcp/tools/MetaTools.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/ToolRetriever.h"
#include "mcp/ToolSchemaBuilder.h"

#include <QDateTime>
#include <QFuture>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QPromise>
#include <QRegularExpression>

#include <atomic>
#include <memory>
#include <optional>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MetaTools";

// ─────────────────────────────────────────────────────────────────────────
// Telemetry counters — Tool RAG observability.
//
// Why file-local statics rather than a singleton service: the MCP system
// already has too many singletons. These counters are read by exactly one
// consumer (mcp_health) and written by exactly two writers (tool_list and
// tool_describe). A class wrapper would be overhead.
//
// Thread safety: tool handlers run on QtConcurrent worker threads. Atomics
// for scalar counters; mutex+QHash for the per-tool surface map.
// ─────────────────────────────────────────────────────────────────────────
namespace telemetry {
static std::atomic<qint64> g_tool_list_calls{0};
static std::atomic<qint64> g_tool_list_empty_results{0};
static std::atomic<qint64> g_tool_describe_calls{0};
static std::atomic<qint64> g_tool_describe_misses{0};

static QMutex g_surface_mutex;
static QHash<QString, qint64> g_surfaces; // tool name → times surfaced by tool_list

static void record_surfaces(const QStringList& names) {
    if (names.isEmpty())
        return;
    QMutexLocker lock(&g_surface_mutex);
    for (const auto& n : names)
        g_surfaces[n]++;
}

static QJsonObject snapshot() {
    QJsonObject out;
    out["tool_list_calls"] = g_tool_list_calls.load();
    out["tool_list_empty_results"] = g_tool_list_empty_results.load();
    out["tool_describe_calls"] = g_tool_describe_calls.load();
    out["tool_describe_misses"] = g_tool_describe_misses.load();

    // Top-20 most-surfaced tools — useful for spotting which tools the
    // LLM keeps finding (= valuable) vs. which never surface (= dead
    // descriptions that need rewriting).
    QMutexLocker lock(&g_surface_mutex);
    QList<QPair<QString, qint64>> ranked;
    ranked.reserve(g_surfaces.size());
    for (auto it = g_surfaces.constBegin(); it != g_surfaces.constEnd(); ++it)
        ranked.append({it.key(), it.value()});
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    QJsonArray top;
    const int kCap = 20;
    for (int i = 0; i < ranked.size() && i < kCap; ++i)
        top.append(QJsonObject{{"name", ranked[i].first}, {"surfaces", ranked[i].second}});
    out["top_surfaced"] = top;
    out["unique_tools_ever_surfaced"] = static_cast<int>(g_surfaces.size());
    return out;
}
} // namespace telemetry

std::vector<ToolDef> get_meta_tools() {
    std::vector<ToolDef> tools;

    // ── tool_list ────────────────────────────────────────────────────────
    //
    // Tool RAG entry point. Replaces the old regex-over-everything variant
    // with BM25 semantic ranking — proven by Anthropic's own Tool Search
    // (Nov 2025) to lift tool-pick accuracy by 9-25 percentage points across
    // model classes once the catalog exceeds ~30 tools.
    //
    // The LLM is expected to call this with a NATURAL LANGUAGE query
    // describing the action it wants to perform. Multi-intent requests
    // ("get news on AAPL and add to watchlist") should issue MULTIPLE
    // tool_list calls, one per intent — Anthropic's recommended pattern.
    {
        ToolDef t;
        t.name = "tool_list";
        t.description = "Search the tool catalog by natural-language query (BM25). Returns the top-K most "
                        "relevant tools as {name, category, description, destructive, score}. "
                        "Use this whenever you need a capability you don't already have a tool for. "
                        "For multi-intent requests, call multiple times — once per intent. "
                        "After picking a tool, call tool_describe(name) for the full input schema.";
        t.category = "meta";
        t.input_schema = ToolSchemaBuilder()
                             .string("query", "Natural-language description of the capability you need "
                                              "(e.g. 'draft a research report', 'place a paper trade', "
                                              "'fetch latest news for AAPL')")
                             .required()
                             .length(1, 256)
                             .string("category", "Optional category filter (e.g. 'markets', 'report-builder', "
                                                 "'paper-trading'). Leave empty to search all categories.")
                             .integer("top_k", "Maximum tools returned (1-20)")
                             .default_int(5)
                             .between(1, 20)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString query = args["query"].toString().trimmed();
            const QString category = args["category"].toString().trimmed();
            const int top_k = args["top_k"].toInt(5);

            telemetry::g_tool_list_calls.fetch_add(1);

            if (query.isEmpty())
                return ToolResult::fail("Empty query — pass a natural-language description.");

            const auto matches = ToolRetriever::instance().search(query, top_k, category);

            if (matches.empty())
                telemetry::g_tool_list_empty_results.fetch_add(1);

            QStringList surfaced;
            QJsonArray out;
            for (const auto& m : matches) {
                QJsonObject row{
                    {"name", m.name},
                    {"category", m.category},
                    {"description", m.description},
                    {"score", m.score},
                };
                // Only emit destructive flag when true — keeps output small
                // and signals attention when present.
                if (m.is_destructive)
                    row["destructive"] = true;
                out.append(row);
                surfaced.append(m.name);
            }
            telemetry::record_surfaces(surfaced);

            return ToolResult::ok_data(QJsonObject{
                {"query", query},
                {"category", category},
                {"count", out.size()},
                {"tools", out},
            });
        };
        tools.push_back(std::move(t));
    }

    // ── tool_describe ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "tool_describe";
        t.description = "Return the full description and JSON Schema for one MCP tool. "
                        "Use after tool_list to fetch the parameter shape before calling.";
        t.category = "meta";
        t.input_schema = ToolSchemaBuilder()
                             .string("name", "Bare tool name (without server_id__ prefix)")
                             .required()
                             .length(1, 128)
                             .string("server_id", "Optional server id when the same name exists on several "
                                                  "servers (as returned by an earlier tool_describe)")
                             .length(0, 128)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString name = args["name"].toString().trimmed();
            const QString server_id = args["server_id"].toString().trimmed();
            telemetry::g_tool_describe_calls.fetch_add(1);

            // Internal tools: O(1) snapshot lookup — previously linear-scanned
            // the full catalog while re-serialising every input_schema (~3 ms p95).
            std::optional<UnifiedTool> tool;
            if (server_id.isEmpty() || server_id == QLatin1String(INTERNAL_SERVER_ID))
                tool = McpProvider::instance().find_tool(name);

            // External tools: tool_list surfaces them (the Tool RAG index covers
            // the merged catalogue), so tool_describe has to resolve them too —
            // otherwise the model finds a tool it can never learn to call.
            QStringList other_servers;
            if (!tool) {
                for (const auto& u : McpService::instance().get_all_tools()) {
                    if (u.is_internal || u.name != name)
                        continue;
                    if (!server_id.isEmpty() && u.server_id != server_id)
                        continue;
                    if (!tool)
                        tool = u;
                    else
                        other_servers << u.server_id;
                }
            }

            if (!tool) {
                telemetry::g_tool_describe_misses.fetch_add(1);
                return ToolResult::fail("Tool not found: " + name + ". Use tool_list to search for tools.");
            }

            QJsonObject out{
                {"name", tool->name},
                {"category", tool->category},
                {"description", tool->description},
                {"input_schema", tool->input_schema},
                {"server_id", tool->server_id},
            };
            if (!tool->is_internal) {
                out["server_name"] = tool->server_name;
                out["external"] = true;
                out["how_to_call"] = QStringLiteral("Call it through tool_invoke(tool='%1', server_id='%2', "
                                                    "arguments={...}).")
                                         .arg(tool->name, tool->server_id);
                if (!other_servers.isEmpty()) {
                    QJsonArray others;
                    for (const auto& s : other_servers)
                        others.append(s);
                    out["also_on_servers"] = others;
                }
            }
            // tool_list already flags these; tool_describe is the call a model makes right
            // before using the tool, and a refused destructive call costs it a whole round.
            // Tell it up front that this one needs the user's "destructive tools" grant.
            if (tool->is_destructive && tool->is_internal) {
                out["destructive"] = true;
                out["note"] = QStringLiteral("State-changing tool: it only runs when the user has enabled "
                                             "destructive AI tools in Settings > Security; otherwise the call is "
                                             "refused. Tell the user what you intend to do before calling it.");
            }
            return ToolResult::ok_data(out);
        };
        tools.push_back(std::move(t));
    }

    // ── tool_invoke ──────────────────────────────────────────────────────
    //
    // Generic dispatcher. Agents and clients that are handed a small tool set
    // (Tier-0 chat, the agent "essentials" catalogue, external MCP clients)
    // discover the rest with tool_list / tool_describe; this is how they then
    // CALL what they found without the tool having to be declared up front.
    //
    // Authorization is the INNER tool's: the inner call goes through
    // McpProvider::call_tool_async on the calling thread, so the auth checker,
    // the destructive capability gate, the agent-bridge flags and the
    // user-approval marker (all thread-local / per-call) apply exactly as if
    // the inner tool had been called directly. tool_invoke itself is therefore
    // not destructive. Its 15-minute budget makes it job-eligible (§M1): in
    // the interactive loop a slow inner tool comes back as a job receipt, and
    // job_cancel / progress reach the inner call through the shared context.
    {
        ToolDef t;
        t.name = "tool_invoke";
        // Worded to stay out of Tool RAG's way: tool_invoke is indexed like any
        // tool, so domain verbs (get / find / read / show) would let it compete
        // with the real tools for ordinary intents.
        t.description = "Generic dispatcher: invokes another tool by its exact name with a JSON arguments "
                        "object. Discover tools with tool_list and their input schema with tool_describe "
                        "before invoking. server_id is only needed for a tool on an external MCP server. "
                        "Returns that tool's own result.";
        t.category = "meta";
        t.default_timeout_ms = 900000;
        t.input_schema = ToolSchemaBuilder()
                             .string("tool", "Exact bare tool name, as returned by tool_list")
                             .required()
                             .length(1, 128)
                             .object("arguments", "Arguments object matching the tool's input schema "
                                                  "(see tool_describe). Omit or {} when it takes none.")
                             .string("server_id", "External server id from tool_describe; omit for "
                                                  "built-in tools")
                             .length(0, 128)
                             .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            // Race the provider's own guard (watchdog / cancellation watch), not a
            // private flag — see ToolContext::resolve_guard.
            auto resolved = ctx.resolve_guard ? ctx.resolve_guard : std::make_shared<std::atomic<bool>>(false);
            auto resolve = [promise, resolved](ToolResult r) {
                bool expected = false;
                if (!resolved->compare_exchange_strong(expected, true))
                    return;
                promise->addResult(std::move(r));
                promise->finish();
            };

            const QString inner = args.value("tool").toString().trimmed();
            const QString server_id = args.value("server_id").toString().trimmed();
            const QJsonValue raw_args = args.value("arguments");
            if (!raw_args.isUndefined() && !raw_args.isNull() && !raw_args.isObject()) {
                resolve(ToolResult::fail("'arguments' must be a JSON object"));
                return;
            }
            const QJsonObject inner_args = raw_args.toObject();

            // No recursion: the dispatcher cannot dispatch itself, and the job
            // tools manage the protocol tool_invoke runs under — wrapping them
            // would start a job to watch a job.
            if (inner == QLatin1String("tool_invoke") || inner.startsWith(QLatin1String("job_"))) {
                resolve(ToolResult::fail(QStringLiteral("tool_invoke cannot call '%1'. Call it directly.").arg(inner)));
                return;
            }

            const auto target = McpService::instance().resolve_tool_target(inner, server_id);
            if (!target.error.isEmpty()) {
                resolve(ToolResult::fail(target.error));
                return;
            }

            // Inner context: progress and cancellation flow through; the timeout
            // is left at the default so the provider applies the INNER tool's
            // own budget (ours is only the outer ceiling).
            ToolContext inner_ctx;
            inner_ctx.on_progress = ctx.on_progress;
            inner_ctx.is_cancelled = ctx.is_cancelled;

            QFuture<ToolResult> f;
            if (target.server_id == QLatin1String(INTERNAL_SERVER_ID)) {
                f = McpProvider::instance().call_tool_async(inner, inner_args, inner_ctx);
            } else {
                // External server: the same dispatch the chat loop uses. It carries
                // the agent-gating flags across its pool-thread hop and applies the
                // external-tool authorization gate in execute_tool().
                f = McpService::instance().execute_openai_function_async(
                    target.server_id + QStringLiteral("__") + McpProvider::encode_tool_name_for_wire(inner),
                    inner_args);
            }
            f.then([resolve, inner](QFuture<ToolResult> done) {
                resolve(done.resultCount() > 0 ? done.result()
                                               : ToolResult::fail("Tool '" + inner + "' produced no result"));
            });
        };
        tools.push_back(std::move(t));
    }

    // ── mcp_health ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "mcp_health";
        t.description = "Snapshot of MCP system health: internal tool counts by category, "
                        "external server statuses + last errors, and DataHub topic activity. "
                        "Useful for agents to self-diagnose ('why is my tool failing?').";
        t.category = "meta";
        t.handler = [](const QJsonObject&) -> ToolResult {
            const auto all = McpProvider::instance().list_all_tools();
            const auto enabled_count = McpProvider::instance().tool_count();

            QHash<QString, int> by_cat;
            for (const auto& t : all)
                by_cat[t.category]++;
            QJsonObject categories;
            for (auto it = by_cat.constBegin(); it != by_cat.constEnd(); ++it)
                categories[it.key().isEmpty() ? "(uncategorised)" : it.key()] = it.value();

            QJsonObject internal_section{
                {"tool_count", static_cast<int>(enabled_count)},
                {"all_count", static_cast<int>(all.size())},
                {"disabled_count", static_cast<int>(all.size() - enabled_count)},
                {"categories", categories},
            };

            QJsonArray servers;
            for (const auto& cfg : McpManager::instance().get_servers()) {
                servers.append(QJsonObject{
                    {"id", cfg.id},
                    {"name", cfg.name},
                    {"status", server_status_str(cfg.status)},
                    {"enabled", cfg.enabled},
                    {"auto_start", cfg.auto_start},
                });
            }
            QJsonObject external_section{{"servers", servers}};

            const auto hub_stats = fincept::datahub::DataHub::instance().stats();
            int subscriber_total = 0;
            qint64 publishes_total = 0;
            for (const auto& s : hub_stats) {
                subscriber_total += s.subscriber_count;
                publishes_total += s.total_publishes;
            }
            QJsonObject datahub_section{
                {"topic_count", static_cast<int>(hub_stats.size())},
                {"subscriber_count", subscriber_total},
                {"total_publishes", publishes_total},
            };

            return ToolResult::ok_data(QJsonObject{
                {"internal", internal_section},
                {"external", external_section},
                {"datahub", datahub_section},
                {"tool_rag", telemetry::snapshot()},
                {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)},
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 meta tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
