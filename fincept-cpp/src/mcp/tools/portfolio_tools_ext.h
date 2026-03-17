#pragma once
// Investment Portfolio MCP Tools — long-term portfolio management
// Wraps fincept::portfolio::* functions for AI/agent access.

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Get all investment portfolio tool definitions
std::vector<ToolDef> get_investment_portfolio_tools();

} // namespace fincept::mcp::tools
