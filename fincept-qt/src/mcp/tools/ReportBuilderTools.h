#pragma once
#include "mcp/McpTypes.h"

#include <vector>

namespace fincept::mcp::tools {

// LLM-facing tools for live authoring of the active report document.
// All tools dispatch onto the main thread synchronously (BlockingQueuedConnection),
// so by the time a tool returns its `ToolResult`, the change is visible on the
// canvas (if the screen is open) and persisted in the service.
std::vector<ToolDef> get_report_builder_tools();

} // namespace fincept::mcp::tools
