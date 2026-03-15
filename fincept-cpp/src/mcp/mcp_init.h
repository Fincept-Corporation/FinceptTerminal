#pragma once
// MCP Initialization — registers all internal tools with MCPProvider
// Call mcp::initialize_all_tools() during App::initialize()

namespace fincept::mcp {

/// Register all built-in tools and initialize the MCP system
void initialize_all_tools();

/// Shutdown the MCP system
void shutdown_mcp();

} // namespace fincept::mcp
