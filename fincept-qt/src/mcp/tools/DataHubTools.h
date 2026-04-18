#pragma once
#include "mcp/McpTypes.h"

#include <vector>

namespace fincept::mcp::tools {

/// Generic DataHub introspection surface for LLMs. See Phase 9 plan.
std::vector<ToolDef> get_datahub_tools();

} // namespace fincept::mcp::tools
