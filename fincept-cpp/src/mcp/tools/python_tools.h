#pragma once
// Python MCP Tools — run analytics scripts, check Python status

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Get all Python analytics tool definitions
std::vector<ToolDef> get_python_tools();

} // namespace fincept::mcp::tools
