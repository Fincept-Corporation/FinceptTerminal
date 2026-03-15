#pragma once
// Settings MCP Tools — read/write application settings, LLM config management

#include "../mcp_types.h"
#include <vector>

namespace fincept::mcp::tools {

/// Get all settings tool definitions
std::vector<ToolDef> get_settings_tools();

} // namespace fincept::mcp::tools
