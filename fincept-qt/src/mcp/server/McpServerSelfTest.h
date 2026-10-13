#pragma once
// McpServerSelfTest.h — headless self-test of the external MCP server
// (McpHttpServer), wired to `--selftest-mcp-server`.
//
// Starts the server on an OS-assigned loopback port with in-memory settings
// (nothing persisted, a throwaway token, actions NOT allowed), drives it with a
// raw HTTP/1.1 client in-process — pumping the event loop, since it runs before
// QApplication::exec() — and checks the protocol, auth, origin and tool-policy
// paths. No network beyond loopback, no LLM, no window; a few seconds.

namespace fincept::mcp {

/// Prints a PASS/FAIL report to stdout. Returns 0 when every check passes.
int run_mcp_server_selftest();

} // namespace fincept::mcp
