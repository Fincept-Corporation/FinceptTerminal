#pragma once
// CodingAgentTools.h — MCP tools for the Agent Workbench (category "coding-agents"):
// let the terminal's own AI start and follow local Claude Code / Codex CLI runs.

#include "mcp/McpTypes.h"

#include <vector>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_coding_agent_tools();

} // namespace fincept::mcp::tools
