#pragma once
// ToolSelfTest.h — headless self-test for the MCP tool system.
//
// Runs WITHOUT a GUI, an LLM, or any network/API key. Checks the deterministic
// properties the core tool catalogue depends on:
//
//   1. Registry integrity — exactly the ten core tools are registered, each
//      with a handler, a valid schema, a usable description and a known
//      category.
//   2. Token budget — the whole catalogue stays small enough to ride on every
//      request of every turn (no discovery tier exists to fall back on).
//   3. Job lifecycle — JobRegistry, which the `python` tool runs on.
//   4. Batch planning — UI-changing tools are barriers in a tool round.
//   5. Python script policy — path normalisation refuses anything outside the
//      scripts tree; risky scripts / commands are classified for approval.
//
// Invoked from main() via `--selftest-tools` / `--dump-tools`, after
// initialize_all_tools() registers the real catalog but before any window or
// network init, so it tests exactly what ships.

namespace fincept::mcp {

/// Run the audit. Prints a report to stdout. Returns a process exit code:
/// 0 = passed, non-zero = failures.
int run_tool_selftest();

/// Print every registered tool as JSON (name, category, description, flags) to
/// stdout. Returns 0.
int dump_tools_json();

} // namespace fincept::mcp
