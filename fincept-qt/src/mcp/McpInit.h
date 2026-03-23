#pragma once
// McpInit.h — Registers all internal tools and initialises the MCP system.
// Call mcp::initialize_all_tools() during application startup (before QApplication::exec()).

namespace fincept::mcp {

/// Register all built-in tools and start the MCP system (external servers in background).
void initialize_all_tools();

/// Shutdown the MCP system cleanly.
void shutdown_mcp();

} // namespace fincept::mcp
