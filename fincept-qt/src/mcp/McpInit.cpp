// McpInit.cpp — Register all internal MCP tools and start the system

#include "mcp/McpInit.h"

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/tools/AiChatTools.h"
#include "mcp/tools/AltInvestmentsTools.h"
#include "mcp/tools/CryptoTradingTools.h"
#include "mcp/tools/DataHubTools.h"
#include "mcp/tools/DataSourcesTools.h"
#include "mcp/tools/EdgarTools.h"
#include "mcp/tools/FileManagerTools.h"
#include "mcp/tools/ForumTools.h"
#include "mcp/tools/MAAnalyticsTools.h"
#include "mcp/tools/MarketsTools.h"
#include "mcp/tools/MetaTools.h"
#include "mcp/tools/NavigationTools.h"
#include "mcp/tools/NewsTools.h"
#include "mcp/tools/NotesTools.h"
#include "mcp/tools/PaperTradingTools.h"
#include "mcp/tools/PortfolioTools.h"
#include "mcp/tools/ProfileTools.h"
#include "mcp/tools/PythonTools.h"
#include "mcp/tools/ReportBuilderTools.h"
#include "mcp/tools/SettingsTools.h"
#include "mcp/tools/SystemTools.h"
#include "mcp/tools/WatchlistTools.h"

#include <QJsonDocument>

namespace fincept::mcp {

static constexpr const char* TAG = "McpInit";

// One-shot diagnostic: walk every registered ToolDef and warn on any whose
// serialized schema exceeds kSchemaSizeWarnBytes. Common culprits: huge
// enum lists (e.g. exhaustive country/currency codes), verbose multi-line
// descriptions, or deeply nested object schemas. Every byte of schema
// multiplies across every LLM turn that includes the tool.
//
// Logs at INFO if all schemas are within budget; logs WARN with offenders
// (sorted largest-first) otherwise. Runs once at startup; cheap.
static constexpr int kSchemaSizeWarnBytes = 2048;

static void audit_tool_schema_sizes() {
    const auto tools = McpProvider::instance().list_all_tools();
    struct Offender { QString name; int bytes; };
    QVector<Offender> over_budget;
    int total_bytes = 0;
    for (const auto& t : tools) {
        const int bytes = QJsonDocument(t.input_schema).toJson(QJsonDocument::Compact).size();
        total_bytes += bytes;
        if (bytes > kSchemaSizeWarnBytes)
            over_budget.append({t.name, bytes});
    }
    std::sort(over_budget.begin(), over_budget.end(),
              [](const Offender& a, const Offender& b) { return a.bytes > b.bytes; });

    LOG_INFO(TAG, QString("Tool schema audit: %1 tools, %2 KB total schema bytes")
                      .arg(tools.size()).arg(total_bytes / 1024));
    if (over_budget.isEmpty())
        return;

    LOG_WARN(TAG, QString("Tool schema audit: %1 tools exceed %2 B budget — every "
                          "byte multiplies across every LLM turn. Consider trimming "
                          "enums / descriptions / nested objects.")
                      .arg(over_budget.size()).arg(kSchemaSizeWarnBytes));
    for (const auto& o : over_budget)
        LOG_WARN(TAG, QString("  %1 — %2 B").arg(o.name).arg(o.bytes));
}

void initialize_all_tools() {
    auto& provider = McpProvider::instance();

    // navigation
    provider.register_tools(tools::get_navigation_tools());

    // news
    provider.register_tools(tools::get_news_tools());

    // markets tab (quotes, symbol search)
    provider.register_tools(tools::get_markets_tools());

    // watchlist tab
    provider.register_tools(tools::get_watchlist_tools());

    // portfolio tab (holdings + named portfolios/assets/transactions/snapshots)
    provider.register_tools(tools::get_portfolio_tools());

    // notes tab
    provider.register_tools(tools::get_notes_tools());

    // ai chat tab
    provider.register_tools(tools::get_ai_chat_tools());

    // crypto trading tab
    provider.register_tools(tools::get_crypto_trading_tools());

    // paper trading tab
    provider.register_tools(tools::get_paper_trading_tools());

    // sec edgar (CIK resolution, XBRL financials, filing search)
    provider.register_tools(tools::get_edgar_tools());

    // m&a analytics tab
    provider.register_tools(tools::get_ma_analytics_tools());

    // alt investments tab
    provider.register_tools(tools::get_alt_investments_tools());

    // data sources tab
    provider.register_tools(tools::get_data_sources_tools());

    // forum tab
    provider.register_tools(tools::get_forum_tools());

    // profile tab
    provider.register_tools(tools::get_profile_tools());

    // file manager tab
    provider.register_tools(tools::get_file_manager_tools());

    // report builder tab — live LLM-driven report authoring
    provider.register_tools(tools::get_report_builder_tools());

    // settings, python, system
    provider.register_tools(tools::get_settings_tools());
    provider.register_tools(tools::get_python_tools());
    provider.register_tools(tools::get_system_tools());

    // datahub introspection (Phase 9)
    provider.register_tools(tools::get_datahub_tools());

    // Phase 6: meta tools — tool.list, tool.describe, mcp.health.
    // Always exposed so the LLM can lazy-discover specialised tools.
    provider.register_tools(tools::get_meta_tools());

    LOG_INFO(TAG, QString("Registered %1 internal MCP tools").arg(provider.tool_count()));

    // Audit schema sizes once after registration — surfaces bloated tools
    // before they bleed prompt tokens on every turn.
    audit_tool_schema_sizes();

    // Initialize unified service (starts external servers in background)
    McpService::instance().initialize();
}

void shutdown_mcp() {
    McpService::instance().shutdown();
    LOG_INFO(TAG, "MCP system shut down");
}

} // namespace fincept::mcp
