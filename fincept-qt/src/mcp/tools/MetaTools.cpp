// MetaTools.cpp — Self-introspection tools (tool.list, tool.describe, mcp.health).

#include "mcp/tools/MetaTools.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"
#include "mcp/ToolRetriever.h"
#include "mcp/ToolSchemaBuilder.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QRegularExpression>

#include <atomic>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MetaTools";

// ─────────────────────────────────────────────────────────────────────────
// Telemetry counters — Tool RAG observability.
//
// Why file-local statics rather than a singleton service: the MCP system
// already has too many singletons. These counters are read by exactly one
// consumer (mcp.health) and written by exactly two writers (tool.list and
// tool.describe). A class wrapper would be overhead.
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
    static QHash<QString, qint64> g_surfaces; // tool name → times surfaced by tool.list

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
        std::sort(ranked.begin(), ranked.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

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

    // ── tool.list ───────────────────────────────────────────────────────
    //
    // Tool RAG entry point. Replaces the old regex-over-everything variant
    // with BM25 semantic ranking — proven by Anthropic's own Tool Search
    // (Nov 2025) to lift tool-pick accuracy by 9-25 percentage points across
    // model classes once the catalog exceeds ~30 tools.
    //
    // The LLM is expected to call this with a NATURAL LANGUAGE query
    // describing the action it wants to perform. Multi-intent requests
    // ("get news on AAPL and add to watchlist") should issue MULTIPLE
    // tool.list calls, one per intent — Anthropic's recommended pattern.
    {
        ToolDef t;
        t.name = "tool.list";
        t.description =
            "Search the tool catalog by natural-language query (BM25). Returns the top-K most "
            "relevant tools as {name, category, description, destructive, score}. "
            "Use this whenever you need a capability you don't already have a tool for. "
            "For multi-intent requests, call multiple times — once per intent. "
            "After picking a tool, call tool.describe(name) for the full input schema.";
        t.category = "meta";
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Natural-language description of the capability you need "
                             "(e.g. 'draft a research report', 'place a paper trade', "
                             "'fetch latest news for AAPL')").required().length(1, 256)
            .string("category", "Optional category filter (e.g. 'markets', 'report-builder', "
                                "'paper-trading'). Leave empty to search all categories.")
            .integer("top_k", "Maximum tools returned (1-20)").default_int(5).between(1, 20)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString query = args["query"].toString().trimmed();
            const QString category = args["category"].toString().trimmed();
            const int top_k = args["top_k"].toInt(5);

            telemetry::g_tool_list_calls.fetch_add(1);

            if (query.isEmpty())
                return ToolResult::fail("Empty query — pass a natural-language description.");

            const auto matches =
                ToolRetriever::instance().search(query, top_k, category);

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

    // ── tool.describe ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "tool.describe";
        t.description = "Return the full description and JSON Schema for one MCP tool. "
                        "Use after tool.list to fetch the parameter shape before calling.";
        t.category = "meta";
        t.input_schema = ToolSchemaBuilder()
            .string("name", "Bare tool name (without server_id__ prefix)").required().length(1, 128)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString name = args["name"].toString();
            telemetry::g_tool_describe_calls.fetch_add(1);

            const auto all = McpProvider::instance().list_tools();
            for (const auto& tool : all) {
                if (tool.name == name) {
                    return ToolResult::ok_data(QJsonObject{
                        {"name", tool.name},
                        {"category", tool.category},
                        {"description", tool.description},
                        {"input_schema", tool.input_schema},
                        {"server_id", tool.server_id},
                    });
                }
            }
            telemetry::g_tool_describe_misses.fetch_add(1);
            return ToolResult::fail("Tool not found: " + name);
        };
        tools.push_back(std::move(t));
    }

    // ── mcp.health ──────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "mcp.health";
        t.description = "Snapshot of MCP system health: internal tool counts by category, "
                        "external server statuses + last errors, and DataHub topic activity. "
                        "Useful for agents to self-diagnose ('why is my tool failing?').";
        t.category = "meta";
        t.handler = [](const QJsonObject&) -> ToolResult {
            const auto all = McpProvider::instance().list_all_tools();
            const auto enabled_count = McpProvider::instance().tool_count();

            QHash<QString, int> by_cat;
            for (const auto& t : all) by_cat[t.category]++;
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
