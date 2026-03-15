// MCP Initialization — register all tools and start the system

#include "mcp_init.h"
#include "mcp_provider.h"
#include "mcp_service.h"
#include "tools/navigation_tools.h"
#include "tools/market_data_tools.h"
#include "tools/settings_tools.h"
#include "tools/portfolio_tools.h"
#include "../core/logger.h"

namespace fincept::mcp {

static constexpr const char* TAG_INIT = "MCPInit";

void initialize_all_tools() {
    auto& provider = MCPProvider::instance();

    // Register tool modules
    provider.register_tools(tools::get_navigation_tools());
    provider.register_tools(tools::get_market_data_tools());
    provider.register_tools(tools::get_settings_tools());
    provider.register_tools(tools::get_portfolio_tools());

    LOG_INFO(TAG_INIT, "Registered %zu internal MCP tools", provider.tool_count());

    // Initialize the unified service (starts external servers too)
    MCPService::instance().initialize();
}

void shutdown_mcp() {
    MCPService::instance().shutdown();
    LOG_INFO(TAG_INIT, "MCP system shut down");
}

} // namespace fincept::mcp
