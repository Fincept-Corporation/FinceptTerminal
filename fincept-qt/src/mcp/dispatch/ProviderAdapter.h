#pragma once
// ProviderAdapter.h — Per-LLM-provider tool-call format adapter.
//
// Phase 5 of the MCP refactor (see plans/mcp-refactor-phase-5-dispatch-unification.md).
//
// Each adapter implements the provider-specific shape of:
//   1. Building the initial chat request body (with tools attached).
//   2. Parsing tool invocations from a response.
//   3. Building a follow-up request body that incorporates tool results.
//   4. Extracting the final user-visible text from a response.
//
// The dispatcher (ToolDispatcher.cpp) drives the multi-round loop using these
// hooks, so all five providers share one piece of orchestration logic — no
// more sequential per-provider tool loops with subtle behaviour drift
// (Anthropic's single-round bug, Gemini's single-round bug, OpenAI's 15-round
// loop, text-regex's own loop, etc.).

#include "services/llm/LlmService.h"  // ConversationMessage, LlmResponse
#include "mcp/dispatch/ToolInvocation.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace fincept::mcp::dispatch {

/// Provider-neutral tool-dispatch interface.
///
/// Adapters are stateless w.r.t. tool calls — the dispatcher passes the
/// running history, the latest LLM response, and the executed tool results
/// on each round. Adapters MAY hold per-conversation config (model name,
/// API key, base URL) constructed via the factory.
class ProviderAdapter {
  public:
    virtual ~ProviderAdapter() = default;

    /// Provider identifier ("openai", "anthropic", "gemini", "fincept",
    /// "fincept-text", "text-regex"). Used for logging only.
    virtual QString id() const = 0;

    /// Maximum number of tool-call rounds the dispatcher will run for this
    /// provider. Phase 5 sets all providers to 15 to fix the historical
    /// Anthropic/Gemini single-round bug. Per-conversation override comes
    /// from settings (Phase 6).
    virtual int default_max_rounds() const { return 15; }

    /// Build the initial request body. `openai_tools_array` is the canonical
    /// MCP tool array as produced by McpService::format_tools_for_openai —
    /// adapters reshape it for their provider (Gemini wraps in
    /// functionDeclarations, Anthropic uses a different schema, etc.).
    /// Pass an empty array to disable tools for this request.
    virtual QJsonObject build_initial_request(
        const QString& user_message,
        const std::vector<fincept::ai_chat::ConversationMessage>& history,
        const QJsonArray& openai_tools_array) const = 0;

    /// Parse tool invocations from a response. Empty vector means the model
    /// produced a final text answer (no tool calls); the dispatcher will
    /// terminate the loop and return `extract_text(response)`.
    virtual std::vector<ToolInvocation> parse_tool_invocations(
        const QJsonObject& response) const = 0;

    /// Build a follow-up request that incorporates `results`. The dispatcher
    /// passes the original response (so the adapter can echo any required
    /// fields — Anthropic needs the assistant's tool_use blocks back, Gemini
    /// needs the model's functionCall parts) and the running history.
    virtual QJsonObject build_followup_request(
        const QJsonObject& original_response,
        const std::vector<ToolResultEnvelope>& results,
        const QString& user_message,
        const std::vector<fincept::ai_chat::ConversationMessage>& history) const = 0;

    /// Extract the user-visible text from a response. Each provider has its
    /// own quirks (Anthropic content blocks, Gemini parts, OpenAI choices,
    /// reasoning models with reasoning_content, etc.). Returns empty if the
    /// response has no usable text.
    virtual QString extract_text(const QJsonObject& response) const = 0;
};

/// Factory: produce the right adapter for a given provider id.
/// Unknown providers return nullptr — caller falls back to legacy code paths.
std::unique_ptr<ProviderAdapter> make_adapter(const QString& provider_id);

} // namespace fincept::mcp::dispatch
