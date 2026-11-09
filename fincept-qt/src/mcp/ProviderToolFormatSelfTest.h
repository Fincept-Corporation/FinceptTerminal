#pragma once
// ProviderToolFormatSelfTest.h — headless self-test for the per-provider tool
// payloads (OpenAI / Anthropic / Gemini).
//
// Runs WITHOUT a GUI, an LLM, or any network/API key. It builds the tool array
// each dialect would actually be sent and checks it against that provider's
// documented constraints — the class of defect that is otherwise invisible
// until a user with that provider selected finds that tool calling silently
// does nothing.
//
// Why this exists: the three dialects once drifted apart — Anthropic and Gemini
// hand-rolled their arrays from a different selection than OpenAI, and Gemini
// forwarded raw JSON Schema into a field that only accepts an OpenAPI subset,
// where one unrecognised key or one empty `properties` object fails the whole
// request. All three now go through McpService::format_tools_for_*, and this
// suite keeps them honest.
//
// Invoked from main() via `--selftest-llm-tools`, after initialize_all_tools()
// registers the real catalog, so it tests exactly what ships.

namespace fincept::mcp {

/// Validate the OpenAI / Anthropic / Gemini tool payloads against each
/// provider's wire constraints. Prints a report to stdout. Returns a process
/// exit code: 0 = passed, non-zero = at least one payload would be rejected.
int run_provider_tool_format_selftest();

} // namespace fincept::mcp
