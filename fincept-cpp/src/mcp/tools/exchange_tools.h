#pragma once
// Exchange MCP Tools — live market data from crypto exchanges
// Wraps fincept::trading::ExchangeService for AI/agent access.

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Get all exchange data tool definitions
std::vector<ToolDef> get_exchange_tools();

} // namespace fincept::mcp::tools
