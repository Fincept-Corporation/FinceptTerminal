#pragma once
// Market Data MCP Tools — quote lookup, watchlist management, symbol search

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Get all market data tool definitions
std::vector<ToolDef> get_market_data_tools();

} // namespace fincept::mcp::tools
