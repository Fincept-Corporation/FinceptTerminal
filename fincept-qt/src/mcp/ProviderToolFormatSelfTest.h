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
// Why this exists: the three dialects had drifted apart. The OpenAI path went
// through format_tools_for_openai (Tool RAG Tier-0, activation feedback,
// name encoding) while Anthropic and Gemini called get_all_tools() and hand-
// rolled their arrays, so they shipped an arbitrary 50-tool slice of a ~900
// tool catalogue; Gemini additionally forwarded raw JSON Schema into a field
// that only accepts an OpenAPI subset, where one unrecognised key or one
// empty `properties` object fails the whole request. Nothing in the build
// caught any of it.
//
// Invoked from main() via `--selftest-llm-tools`, after initialize_all_tools()
// registers the real catalog, so it tests exactly what ships.

namespace fincept::mcp {

/// Validate the OpenAI / Anthropic / Gemini tool payloads against each
/// provider's wire constraints. Prints a report to stdout. Returns a process
/// exit code: 0 = passed, non-zero = at least one payload would be rejected.
int run_provider_tool_format_selftest();

} // namespace fincept::mcp
