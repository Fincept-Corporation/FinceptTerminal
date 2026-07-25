#pragma once
// JobTools.h — Meta-tools for long-running jobs and oversized results.

#include "mcp/McpTypes.h"

#include <vector>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_job_tools();

} // namespace fincept::mcp::tools
