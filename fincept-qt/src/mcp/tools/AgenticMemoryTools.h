#pragma once
// AgenticMemoryTools — exposes the Phase 3 ArchivalMemoryStore as MCP tools
// so agents can call archival_memory_save / archival_memory_search mid-step
// instead of relying solely on plan-time injection (the Letta tier-3 pattern).
//
// Storage: direct QtSql against the same agent_tasks.db Python writes to.
// We deliberately avoid spawning a Python subprocess per tool call — the
// agent process is already Python, so a round-trip would be wasteful.
#include "mcp/McpTypes.h"
#include <vector>

namespace fincept::mcp::tools {
std::vector<ToolDef> get_agentic_memory_tools();
} // namespace fincept::mcp::tools
