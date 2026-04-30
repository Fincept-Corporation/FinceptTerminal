#pragma once
// MetaTools.h — Self-introspection MCP tools (Phase 6).
//
// Three always-available tools the LLM can call to explore the catalogue:
//
//   tool.list(category?, search?, limit?)
//     One-line summary per matching tool (no schemas). Cheap to embed in
//     prompts. Lets the LLM discover specialised tools without the full
//     237-tool catalogue ballooning every request.
//
//   tool.describe(name)
//     Full schema + description for a single tool. Called when the LLM has
//     identified which tool it wants and now needs the parameter shape.
//
//   mcp.health
//     Snapshot of the MCP system: internal tool count by category, external
//     server statuses + last errors, DataHub topic count. Lets agents
//     self-diagnose ("why is my tool failing?") and surfaces ops state to
//     debug consoles without requiring access to the desktop UI.

#include "mcp/McpTypes.h"

#include <vector>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_meta_tools();

} // namespace fincept::mcp::tools
