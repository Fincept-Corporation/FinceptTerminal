#pragma once
// OpenAiAdapter.h — OpenAI-compatible tool-call dispatch.
//
// Phase 5 of the MCP refactor. Reference adapter — captures the canonical
// shape from LlmService.cpp's OpenAI tool-call branch. Other adapters
// (Anthropic, Gemini, Fincept, TextRegex) follow this same interface.

#include "mcp/dispatch/ProviderAdapter.h"

namespace fincept::mcp::dispatch {

/// OpenAI / OpenAI-compatible (Groq, OpenRouter, DeepSeek, Together, etc.)
/// tool-call adapter. The wire shape is OpenAI's chat.completions API:
///   - Tools attached as `tools: [{type:"function", function:{name, description, parameters}}]`
///   - Tool calls in `choices[0].message.tool_calls[]`
///   - Results returned via `{role:"tool", tool_call_id, content}` messages
class OpenAiAdapter : public ProviderAdapter {
  public:
    QString id() const override { return "openai"; }

    QJsonObject build_initial_request(
        const QString& user_message,
        const std::vector<fincept::ai_chat::ConversationMessage>& history,
        const QJsonArray& openai_tools_array) const override;

    std::vector<ToolInvocation> parse_tool_invocations(
        const QJsonObject& response) const override;

    QJsonObject build_followup_request(
        const QJsonObject& original_response,
        const std::vector<ToolResultEnvelope>& results,
        const QString& user_message,
        const std::vector<fincept::ai_chat::ConversationMessage>& history) const override;

    QString extract_text(const QJsonObject& response) const override;
};

} // namespace fincept::mcp::dispatch
