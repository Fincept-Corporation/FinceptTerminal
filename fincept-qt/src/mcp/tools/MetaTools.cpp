// MetaTools.cpp — Self-introspection tools (tool.list, tool.describe, mcp.health).

#include "mcp/tools/MetaTools.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"
#include "mcp/ToolSchemaBuilder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MetaTools";

std::vector<ToolDef> get_meta_tools() {
    std::vector<ToolDef> tools;

    // ── tool.list ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "tool.list";
        t.description = "List MCP tools available to the LLM, optionally filtered by category "
                        "and/or a regex search over name+description. Returns name + 1-line "
                        "description only — call tool.describe(name) for full schemas.";
        t.category = "meta";
        t.input_schema = ToolSchemaBuilder()
            .string("category", "Filter by tool category (e.g. 'markets', 'news', 'paper-trading')")
            .string("search", "Regex matched against name or description (case-insensitive)")
            .integer("limit", "Max tools returned").default_int(50).between(1, 500)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString category = args["category"].toString();
            const QString search = args["search"].toString();
            const int limit = args["limit"].toInt(50);

            QRegularExpression rx;
            if (!search.isEmpty())
                rx = QRegularExpression(search, QRegularExpression::CaseInsensitiveOption);

            const auto all = McpProvider::instance().list_tools();
            QJsonArray out;
            int total = 0;
            for (const auto& tool : all) {
                if (!category.isEmpty() && tool.category != category) continue;
                if (rx.isValid() && !search.isEmpty()) {
                    if (!rx.match(tool.name).hasMatch() && !rx.match(tool.description).hasMatch())
                        continue;
                }
                ++total;
                if (out.size() < limit) {
                    out.append(QJsonObject{
                        {"name", tool.name},
                        {"category", tool.category},
                        {"description", tool.description},
                    });
                }
            }
            return ToolResult::ok_data(QJsonObject{
                {"count", out.size()},
                {"total_matched", total},
                {"limit", limit},
                {"tools", out}});
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
                {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)},
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 meta tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
