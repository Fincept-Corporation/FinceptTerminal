// McpInit.cpp — Register all internal MCP tools and start the system

#include "mcp/McpInit.h"

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/tools/ExchangeTools.h"
#include "mcp/tools/MarketDataTools.h"
#include "mcp/tools/NavigationTools.h"
#include "mcp/tools/PortfolioTools.h"
#include "mcp/tools/PortfolioToolsExt.h"
#include "mcp/tools/PythonTools.h"
#include "mcp/tools/SettingsTools.h"
#include "mcp/tools/SystemTools.h"
#include "mcp/tools/TradingTools.h"

namespace fincept::mcp {

static constexpr const char* TAG = "McpInit";

void initialize_all_tools() {
    auto& provider = McpProvider::instance();

    // Register tool modules
    provider.register_tools(tools::get_navigation_tools());
    provider.register_tools(tools::get_market_data_tools());
    provider.register_tools(tools::get_settings_tools());
    provider.register_tools(tools::get_portfolio_tools());
    provider.register_tools(tools::get_trading_tools());
    provider.register_tools(tools::get_investment_portfolio_tools());
    provider.register_tools(tools::get_python_tools());
    provider.register_tools(tools::get_exchange_tools());
    provider.register_tools(tools::get_system_tools());

    LOG_INFO(TAG, QString("Registered %1 internal MCP tools").arg(provider.tool_count()));

    // Initialize unified service (starts external servers in background)
    McpService::instance().initialize();
}

void shutdown_mcp() {
    McpService::instance().shutdown();
    LOG_INFO(TAG, "MCP system shut down");
}

} // namespace fincept::mcp
