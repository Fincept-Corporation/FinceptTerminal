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

namespace fincept::mcp {

static constexpr const char* TAG = "McpInit";

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

    LOG_INFO(TAG, QString("Registered %1 internal MCP tools").arg(provider.tool_count()));

    // Initialize unified service (starts external servers in background)
    McpService::instance().initialize();
}

void shutdown_mcp() {
    McpService::instance().shutdown();
    LOG_INFO(TAG, "MCP system shut down");
}

} // namespace fincept::mcp
