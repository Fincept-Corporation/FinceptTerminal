#pragma once
// Portfolio MCP Tools — notes CRUD, chat session management

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Get all portfolio/notes tool definitions
std::vector<ToolDef> get_portfolio_tools();

} // namespace fincept::mcp::tools
