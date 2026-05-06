// Fincept async LLM path: POST /research/llm/async then poll status every 3s up to 120s.
// Submit takes a flat prompt, so system + tool catalog + history are flattened here.

#include "ai_chat/LlmService.h"

#include "ai_chat/LlmRequestPolicy.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

namespace fincept::ai_chat {

namespace { constexpr const char* kFinceptAsyncTag = "LlmService"; }

/// Tool catalog injected into the system prompt for models without structured tool_calls.
/// RAG mode (default filter): Tier-0 tools only + tool.list discovery hint.
/// Otherwise: up to 60 filtered tools inline.
static QString build_tool_catalog_for_prompt(const mcp::ToolFilter& filter) {
    auto all_tools = mcp::McpService::instance().get_all_tools(filter);
    if (all_tools.empty())
        return {};

    const bool default_filter = filter.categories.isEmpty() &&
                                filter.exclude_categories.isEmpty() &&
                                filter.name_patterns.isEmpty() &&
                                filter.exclude_name_patterns.isEmpty() &&
                                filter.max_tools == 0;
    const bool use_rag = default_filter &&
                         fincept::AppConfig::instance()
                             .get("mcp/use_tool_rag", QVariant(true))
                             .toBool();

    QString catalog;
    catalog += "You have access to tools. To use one, emit a <tool_call> block:\n";
    catalog += "<tool_call>{\"name\": \"TOOL_NAME\", \"arguments\": {\"param\": \"value\"}}</tool_call>\n\n";

    if (use_rag) {
        // Mirrors McpService::tier_0_tool_names() — kept in sync manually.
        static const QSet<QString> kTier0 = {
            "tool.list", "tool.describe", "navigate_to_tab", "list_tabs",
            "get_current_tab", "get_auth_status",
        };
        catalog += "Always-available tools:\n";
        for (const auto& tool : all_tools) {
            if (!kTier0.contains(tool.name))
                continue;
            QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
            catalog += "- " + fn_name + ": " + tool.description + "\n";
        }
        const QString wire_list     = mcp::McpProvider::encode_tool_name_for_wire("tool.list");
        const QString wire_describe = mcp::McpProvider::encode_tool_name_for_wire("tool.describe");
        catalog += QStringLiteral(
            "\nFor any other capability, call fincept-terminal__%1 with a natural-language "
            "query (e.g. {\"query\": \"draft a research report\"}). It returns the top 5 most "
            "relevant tools. Then call fincept-terminal__%2(name) for the full schema, "
            "then invoke the tool. For multi-intent requests, call %1 multiple times.\n")
            .arg(wire_list, wire_describe);
        return catalog;
    }

    catalog += "Available tools:\n";
    int count = 0;
    for (const auto& tool : all_tools) {
        QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
        catalog += "- " + fn_name + ": " + tool.description;
        QJsonObject props = tool.input_schema["properties"].toObject();
        if (!props.isEmpty()) {
            QStringList params;
            for (auto it = props.constBegin(); it != props.constEnd(); ++it)
                params.append(it.key());
            catalog += " (params: " + params.join(", ") + ")";
        }
        catalog += "\n";
        ++count;
        if (count >= 60) {
            catalog += "... and " + QString::number(all_tools.size() - 60) + " more tools available.\n";
            break;
        }
    }
    return catalog;
}

LlmResponse LlmService::fincept_async_request(const QString& user_message,
                                              const std::vector<ConversationMessage>& history) {
    LlmResponse resp;

    // Endpoint takes a plain prompt, not OpenAI messages.
    QString prompt;
    if (!system_prompt_.isEmpty())
        prompt += system_prompt_ + "\n\n";

    if (detail::effective_tools_enabled(tools_enabled_)) {
        QString tool_catalog = build_tool_catalog_for_prompt(detail::apply_request_policy(tool_filter_));
        if (!tool_catalog.isEmpty())
            prompt += tool_catalog + "\n";
    }

    for (const auto& m : history) {
        if (m.role == "user")
            prompt += "User: " + m.content + "\n\n";
        else if (m.role == "assistant")
            prompt += "Assistant: " + m.content + "\n\n";
    }
    prompt += "User: " + user_message;

    QJsonObject submit_body;
    submit_body["prompt"]     = prompt;
    submit_body["max_tokens"] = resolved_max_tokens();

    auto hdr = get_headers();
    const QString async_url   = "https://api.fincept.in/research/llm/async";
    const QString status_base = "https://api.fincept.in/research/llm/status/";

    LOG_INFO(kFinceptAsyncTag, QString("Fincept async: submitting to %1 (api_key=%2, prompt_len=%3)")
                      .arg(async_url)
                      .arg(api_key_.isEmpty() ? "EMPTY" : api_key_.left(12) + "...")
                      .arg(prompt.length()));

    QByteArray json_data = QJsonDocument(submit_body).toJson(QJsonDocument::Compact);
    auto submit = eventloop_request("POST", async_url, json_data, hdr, 30000);
    if (!submit.success) {
        resp.error = "Fincept async submit failed: " + submit.error;
        LOG_ERROR(kFinceptAsyncTag, resp.error);
        return resp;
    }

    auto submit_doc = QJsonDocument::fromJson(submit.body);
    if (submit_doc.isNull()) {
        resp.error = "Fincept async: failed to parse submit response";
        return resp;
    }

    // task_id may be top-level or under data.
    QJsonObject sj = submit_doc.object();
    QString task_id = sj["task_id"].toString();
    if (task_id.isEmpty())
        task_id = sj["data"].toObject()["task_id"].toString();
    if (task_id.isEmpty()) {
        resp.error = "Fincept async: no task_id in submit response";
        return resp;
    }
    LOG_INFO(kFinceptAsyncTag, "Fincept async task_id: " + task_id);

    const QString poll_url = status_base + task_id;
    constexpr int MAX_POLLS = 40;
    for (int i = 0; i < MAX_POLLS; ++i) {
        QThread::msleep(3000);

        auto poll = eventloop_request("GET", poll_url, {}, hdr, 15000);
        if (!poll.success) {
            LOG_WARN(kFinceptAsyncTag, "Fincept async poll failed: " + poll.error);
            continue;
        }

        auto poll_doc = QJsonDocument::fromJson(poll.body);
        if (poll_doc.isNull())
            continue;

        QJsonObject pj = poll_doc.object();
        QString status = pj["status"].toString();
        QJsonObject data_obj = pj["data"].toObject();
        if (status.isEmpty())
            status = data_obj["status"].toString();

        LOG_INFO(kFinceptAsyncTag, QString("Fincept async poll %1 status=%2").arg(i + 1).arg(status));

        if (status == "completed") {
            QString response = data_obj["data"].toObject()["response"].toString();
            if (response.isEmpty())
                response = data_obj["response"].toString();
            if (response.isEmpty()) {
                resp.error = "Fincept async completed but response is empty";
                LOG_WARN(kFinceptAsyncTag, "Fincept async task completed with empty response field");
                return resp;
            }
            resp.content = response;
            resp.success = true;

            QJsonObject usage = data_obj["data"].toObject()["usage"].toObject();
            if (!usage.isEmpty()) {
                resp.prompt_tokens     = usage["input_tokens"].toInt();
                resp.completion_tokens = usage["output_tokens"].toInt();
                resp.total_tokens      = usage["total_tokens"].toInt();
            }

            // Detect <tool_call>...</tool_call> blocks the model may have emitted; follow up via sync /research/chat.
            if (!resp.content.isEmpty()) {
                LOG_INFO(kFinceptAsyncTag, "Fincept: checking response for text-based tool calls");
                QString followup_url = get_endpoint_url();
                auto followup_hdr    = get_headers();
                auto tool_result =
                    try_extract_and_execute_text_tool_calls(resp.content, user_message, followup_url, followup_hdr);
                if (tool_result.has_value()) {
                    LOG_INFO(kFinceptAsyncTag, "Fincept: text tool calls detected and executed");
                    return tool_result.value();
                }
            }
            return resp;
        }

        if (status == "failed") {
            QString err = pj["error"].toString();
            if (err.isEmpty())
                err = data_obj["error"].toString();
            resp.error = "Fincept async task failed: " + (err.isEmpty() ? "unknown error" : err);
            return resp;
        }
    }

    resp.error = "Fincept async timed out waiting for response";
    return resp;
}

} // namespace fincept::ai_chat
