#pragma once
// System MCP Tools — auth status, cache management, app info

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Get all system/utility tool definitions
std::vector<ToolDef> get_system_tools();

} // namespace fincept::mcp::tools
