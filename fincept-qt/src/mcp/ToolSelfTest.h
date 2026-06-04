#pragma once
// ToolSelfTest.h — headless self-test for the MCP tool system.
//
// Runs WITHOUT a GUI, an LLM, or any network/API key. Exercises the two
// deterministic halves of agentic reliability:
//
//   1. Retrieval — can ToolRetriever (the BM25 index behind tool_list) surface
//      the right tool for a realistic natural-language intent? This is the #1
//      cause of "the model couldn't find/call the tool" failures.
//   2. Registry integrity — does every registered tool have a handler, a valid
//      schema, and a usable description?
//
// Invoked from main() via `--selftest-tools` / `--dump-tools`, after
// initialize_all_tools() registers the real catalog but before any window or
// network init, so it tests exactly what ships.

namespace fincept::mcp {

/// Run the retrieval eval + registry audit. Prints a scored report to stdout.
/// Returns a process exit code: 0 = passed, non-zero = failures (no-handler
/// tools, schema errors, or retrieval recall below threshold).
int run_tool_selftest();

/// Print every registered tool as JSON (name, category, description, flags) to
/// stdout. Returns 0. Used to ground the eval corpus in the real catalog.
int dump_tools_json();

} // namespace fincept::mcp
