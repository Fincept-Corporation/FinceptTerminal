// LlmRequestBuilders.cpp — per-provider JSON request shape builders.
// Each method translates the conversation + active config into the wire
// format the provider's chat-completion endpoint expects.

#include "ai_chat/LlmService.h"

#include "ai_chat/LlmRequestPolicy.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace fincept::ai_chat {

namespace { constexpr const char* kLlmBuildersTag = "LlmService"; }

QJsonObject LlmService::build_openai_request(const QString& user_message,
                                             const std::vector<ConversationMessage>& history, bool stream,
                                             bool with_tools) {
    const QString model_lower = model_.toLower();

    // deepseek-reasoner doesn't accept `tools`.
    const bool is_ds_reasoner = (provider_ == "deepseek" && model_lower.contains("reasoner"));

    // Groq's whisper-* (audio) and llama-guard-* (safety classifier) do not accept tools.
    const bool groq_no_tools =
        (provider_ == "groq" && (model_lower.startsWith("whisper-") || model_lower.contains("llama-guard")));

    QJsonArray messages;
    if (!system_prompt_.isEmpty())
        messages.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["model"]    = model_;
    req["messages"] = messages;
    // Temperature intentionally omitted — each provider uses its own default.
    // OpenAI deprecated max_tokens; gpt-5 / o-series require max_completion_tokens.
    // xAI also prefers max_completion_tokens. Other OpenAI-compatible providers
    // still expect max_tokens.
    const int mx = resolved_max_tokens();
    if (provider_ == "openai" || provider_ == "xai")
        req["max_completion_tokens"] = mx;
    else
        req["max_tokens"] = mx;
    if (stream) {
        req["stream"] = true;
        // Streamed OpenAI / xAI responses omit usage unless we opt in
        if (provider_ == "openai" || provider_ == "xai")
            req["stream_options"] = QJsonObject{{"include_usage", true}};
    }

    // Send tools on BOTH streaming and non-streaming requests. The streaming
    // path detects a tool-call response (finish_reason="tool_calls" or
    // delta.tool_calls) and falls back to non-streaming do_request, which
    // executes the tools and follows up. Without sending tools on stream,
    // the model has no idea they exist and answers from training data —
    // which silently breaks live tool calling for OpenAI/Kimi/Groq/etc.
    // deepseek-reasoner rejects tools entirely; some Groq models also.
    const bool tools_effectively_on = detail::effective_tools_enabled(tools_enabled_);
    if (with_tools && tools_effectively_on && !is_ds_reasoner && !groq_no_tools) {
        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai(detail::apply_request_policy(tool_filter_));
        if (!tools.isEmpty())
            req["tools"] = tools;
        LOG_INFO(kLlmBuildersTag, QString("OpenAI request: stream=%1 provider=%2 tools=%3 (count=%4)")
                          .arg(stream ? "true" : "false", provider_,
                               tools.isEmpty() ? "none" : "attached")
                          .arg(tools.size()));
    } else {
        LOG_WARN(kLlmBuildersTag, QString("OpenAI request: stream=%1 provider=%2 NO TOOLS — "
                              "with_tools=%3 tools_effectively_on=%4 ds_reasoner=%5 groq_no_tools=%6")
                          .arg(stream ? "true" : "false", provider_)
                          .arg(with_tools ? "true" : "false")
                          .arg(tools_effectively_on ? "true" : "false")
                          .arg(is_ds_reasoner ? "true" : "false")
                          .arg(groq_no_tools ? "true" : "false"));
    }
    return req;
}

QJsonObject LlmService::build_anthropic_request(const QString& user_message,
                                                const std::vector<ConversationMessage>& history, bool stream) {
    QJsonArray messages;
    for (const auto& m : history) {
        if (m.role != "system")
            messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["model"]      = model_;
    req["messages"]   = messages;
    req["max_tokens"] = resolved_max_tokens();
    // Temperature intentionally omitted — Anthropic defaults to 1.0.
    if (!system_prompt_.isEmpty())
        req["system"] = system_prompt_;
    if (stream)
        req["stream"] = true;

    // Anthropic tool format: array of {name, description, input_schema}
    // (no "type":"function" wrapper like OpenAI). Tools are sent for both
    // streaming and non-streaming requests so the model can request a
    // tool_use even mid-stream — the streaming code path detects this and
    // falls back to do_request to execute and follow up.
    if (detail::effective_tools_enabled(tools_enabled_)) {
        QJsonArray ant_tools;
        auto all_tools = mcp::McpService::instance().get_all_tools(detail::apply_request_policy(tool_filter_));
        for (const auto& tool : all_tools) {
            QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
            QJsonObject schema = tool.input_schema;
            if (schema.isEmpty()) {
                schema["type"]       = "object";
                schema["properties"] = QJsonObject();
            }
            ant_tools.append(
                QJsonObject{{"name", fn_name}, {"description", tool.description}, {"input_schema", schema}});
        }
        if (!ant_tools.isEmpty())
            req["tools"] = ant_tools;
    }
    return req;
}

QJsonObject LlmService::build_gemini_request(const QString& user_message,
                                             const std::vector<ConversationMessage>& history) {
    QJsonArray contents;
    for (const auto& m : history) {
        if (m.role == "system")
            continue;
        QString role = (m.role == "assistant") ? "model" : "user";
        contents.append(QJsonObject{{"role", role}, {"parts", QJsonArray{QJsonObject{{"text", m.content}}}}});
    }
    contents.append(QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", user_message}}}}});

    QJsonObject gen_cfg;
    // Temperature intentionally omitted — Gemini defaults to 1.0.
    gen_cfg["maxOutputTokens"] = resolved_max_tokens();

    QJsonObject req;
    req["contents"]         = contents;
    req["generationConfig"] = gen_cfg;
    if (!system_prompt_.isEmpty()) {
        req["systemInstruction"] = QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", system_prompt_}}}}};
    }

    // Gemini tool format: tools[{functionDeclarations:[{name, description, parameters}]}]
    auto all_tools = detail::effective_tools_enabled(tools_enabled_)
                         ? mcp::McpService::instance().get_all_tools(detail::apply_request_policy(tool_filter_))
                         : std::vector<mcp::UnifiedTool>{};
    if (!all_tools.empty()) {
        QJsonArray fn_decls;
        for (const auto& tool : all_tools) {
            QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
            QJsonObject schema = tool.input_schema;
            if (schema.isEmpty()) {
                schema["type"]       = "object";
                schema["properties"] = QJsonObject();
            }
            fn_decls.append(QJsonObject{{"name", fn_name}, {"description", tool.description}, {"parameters", schema}});
        }
        if (!fn_decls.isEmpty())
            req["tools"] = QJsonArray{QJsonObject{{"functionDeclarations", fn_decls}}};
    }

    return req;
}

QJsonObject LlmService::build_fincept_request(const QString& user_message,
                                              const std::vector<ConversationMessage>& history, bool with_tools) {
    // Fincept /research/chat uses the OpenAI messages array format
    QJsonArray messages;
    if (!system_prompt_.isEmpty())
        messages.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["messages"] = messages;
    // Pass model if set to a real model name (not the legacy placeholder)
    if (!model_.isEmpty() && model_ != "fincept-llm")
        req["model"] = model_;

    Q_UNUSED(with_tools) // Fincept /research/chat does not support tools yet
    return req;
}

} // namespace fincept::ai_chat
