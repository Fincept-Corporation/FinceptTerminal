// Per-provider chat-completion request shape builders.

#include "ai_chat/LlmService.h"

#include "ai_chat/LlmRequestPolicy.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace fincept::ai_chat {

namespace { constexpr const char* kRequestBuildersTag = "LlmService"; }

QJsonObject LlmService::build_openai_request(const QString& user_message,
                                             const std::vector<ConversationMessage>& history, bool stream,
                                             bool with_tools) {
    const QString model_lower = model_.toLower();

    // Models that reject `tools`: deepseek-reasoner, groq whisper-* (audio), groq llama-guard-* (classifier).
    const bool is_ds_reasoner = (provider_ == "deepseek" && model_lower.contains("reasoner"));
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
    // Temperature omitted — each provider's default. OpenAI/xAI use max_completion_tokens; others max_tokens.
    const int mx = resolved_max_tokens();
    if (provider_ == "openai" || provider_ == "xai")
        req["max_completion_tokens"] = mx;
    else
        req["max_tokens"] = mx;
    if (stream) {
        req["stream"] = true;
        // OpenAI/xAI omit usage on streamed responses unless we opt in.
        if (provider_ == "openai" || provider_ == "xai")
            req["stream_options"] = QJsonObject{{"include_usage", true}};
    }

    // Send tools on streaming requests too — the stream path detects tool_calls and falls back
    // to do_request to execute. Otherwise the model answers from training and tool calling silently breaks.
    const bool tools_effectively_on = detail::effective_tools_enabled(tools_enabled_);
    if (with_tools && tools_effectively_on && !is_ds_reasoner && !groq_no_tools) {
        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai(detail::apply_request_policy(tool_filter_));
        if (!tools.isEmpty())
            req["tools"] = tools;
        LOG_INFO(kRequestBuildersTag, QString("OpenAI request: stream=%1 provider=%2 tools=%3 (count=%4)")
                          .arg(stream ? "true" : "false", provider_,
                               tools.isEmpty() ? "none" : "attached")
                          .arg(tools.size()));
    } else {
        LOG_WARN(kRequestBuildersTag, QString("OpenAI request: stream=%1 provider=%2 NO TOOLS — "
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
    if (!system_prompt_.isEmpty())
        req["system"] = system_prompt_;
    if (stream)
        req["stream"] = true;

    // Anthropic tools: bare {name, description, input_schema} — no OpenAI-style "type":"function" wrapper.
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
    gen_cfg["maxOutputTokens"] = resolved_max_tokens();

    QJsonObject req;
    req["contents"]         = contents;
    req["generationConfig"] = gen_cfg;
    if (!system_prompt_.isEmpty()) {
        req["systemInstruction"] = QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", system_prompt_}}}}};
    }

    // Gemini tools: tools[{functionDeclarations:[{name, description, parameters}]}].
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
    // /research/chat uses OpenAI messages format.
    QJsonArray messages;
    if (!system_prompt_.isEmpty())
        messages.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["messages"] = messages;
    // Skip the legacy "fincept-llm" placeholder.
    if (!model_.isEmpty() && model_ != "fincept-llm")
        req["model"] = model_;

    Q_UNUSED(with_tools) // /research/chat does not support tools yet.
    return req;
}

} // namespace fincept::ai_chat
