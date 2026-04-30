#pragma once
// ToolInvocation.h — Provider-neutral tool-call types.
//
// Phase 5 of the MCP refactor (see plans/mcp-refactor-phase-5-dispatch-unification.md).
//
// Five LLM provider paths in LlmService.cpp each parse tool-call wire shapes
// differently:
//
//   OpenAI / Fincept-OpenAI-compat: choices[0].message.tool_calls[]
//   Anthropic:                       content blocks where type == "tool_use"
//   Gemini:                          parts[].functionCall
//   Fincept text-catalog:            tool calls extracted from prompt response text
//   Text-regex fallback:             <tool_call>...</tool_call>, <invoke name=...>...,
//                                    ```tool_call ... ```, minimax:tool_call ... patterns
//
// `ToolInvocation` normalises all five into one shape so the dispatcher can
// fan them out via `McpService::execute_openai_function_async` regardless of
// which provider produced them.

#include "mcp/McpTypes.h"

#include <QJsonObject>
#include <QString>

namespace fincept::mcp {

/// One tool call as emitted by an LLM, in provider-neutral form.
struct ToolInvocation {
    /// Provider-supplied call id (OpenAI tool_call_id, Anthropic tool_use id,
    /// Gemini synthetic id, etc.). The dispatcher echoes this back in the
    /// follow-up message so the LLM can match results to its requests.
    /// May be empty for text-regex extraction; the adapter generates one.
    QString call_id;

    /// "serverId__toolName" form (the canonical wire identifier — see
    /// McpProvider::parse_openai_function_name). Adapters that produce
    /// invocations from non-OpenAI shapes must convert into this form.
    QString function_name;

    /// Parsed arguments — JSON object the handler will receive.
    QJsonObject arguments;

    /// Source provider, for logging / telemetry. Not used for routing.
    /// Values: "openai" | "anthropic" | "gemini" | "fincept-text" | "text-regex".
    QString origin;
};

/// One tool call's result, ready to be folded back into a provider follow-up.
struct ToolResultEnvelope {
    /// Echoes ToolInvocation::call_id so the adapter can pair result with
    /// invocation in the follow-up message format.
    QString call_id;

    /// Echoes ToolInvocation::function_name — used by formatters that need
    /// the name in the follow-up payload (Gemini's functionResponse.name).
    QString function_name;

    /// The tool's actual return value. Adapters serialise this differently:
    /// OpenAI/Fincept put it in role:"tool" content as a JSON string;
    /// Anthropic puts it in tool_result blocks; Gemini wraps it as a
    /// functionResponse object.
    ToolResult result;
};

} // namespace fincept::mcp
