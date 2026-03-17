#pragma once
// Trading MCP Tools — paper trading operations
// Wraps fincept::trading::pt_* functions for AI/agent access.

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Get all paper trading tool definitions
std::vector<ToolDef> get_trading_tools();

} // namespace fincept::mcp::tools
