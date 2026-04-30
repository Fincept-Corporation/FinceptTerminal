// OpenAiAdapter.cpp — OpenAI-compatible tool-call dispatch.

#include "mcp/dispatch/OpenAiAdapter.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace fincept::mcp::dispatch {

// ── Helpers ──────────────────────────────────────────────────────────────────

namespace {

/// Extract user-visible text from an OpenAI-compatible `message` object.
/// Mirrors the helper in LlmService.cpp — handles plain content, content-as-parts
/// arrays, reasoning_content (kimi/deepseek-reasoner), and refusal messages.
QString extract_openai_message_text(const QJsonObject& msg) {
    QJsonValue cv = msg["content"];
    if (cv.isString()) {
        QString s = cv.toString();
        if (!s.isEmpty()) return s;
    } else if (cv.isArray()) {
        QString joined;
        for (const auto& pv : cv.toArray()) {
            QJsonObject p = pv.toObject();
            QString type = p["type"].toString();
            if (type == "text" || type == "output_text")
                joined += p["text"].toString();
        }
        if (!joined.isEmpty()) return joined;
    }
    QString rc = msg["reasoning_content"].toString();
    if (!rc.isEmpty()) return rc;
    QString refusal = msg["refusal"].toString();
    if (!refusal.isEmpty()) return refusal;
    return {};
}

} // anonymous namespace

// ── ProviderAdapter implementation ───────────────────────────────────────────

QJsonObject OpenAiAdapter::build_initial_request(
    const QString& user_message,
    const std::vector<fincept::ai_chat::ConversationMessage>& history,
    const QJsonArray& openai_tools_array) const {

    QJsonArray messages;
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject body;
    body["messages"] = messages;
    if (!openai_tools_array.isEmpty()) {
        body["tools"] = openai_tools_array;
        body["tool_choice"] = "auto";
    }
    return body;
}

std::vector<ToolInvocation> OpenAiAdapter::parse_tool_invocations(
    const QJsonObject& response) const {

    std::vector<ToolInvocation> out;
    QJsonArray choices = response["choices"].toArray();
    if (choices.isEmpty()) return out;

    QJsonObject msg = choices[0].toObject()["message"].toObject();
    QJsonArray tcs = msg["tool_calls"].toArray();
    if (tcs.isEmpty()) return out;

    out.reserve(static_cast<std::size_t>(tcs.size()));
    for (const auto& tc_val : tcs) {
        QJsonObject tc = tc_val.toObject();
        ToolInvocation inv;
        inv.call_id = tc["id"].toString();
        inv.function_name = tc["function"].toObject()["name"].toString();
        // `arguments` is a JSON-encoded string per OpenAI spec.
        const QString args_str = tc["function"].toObject()["arguments"].toString("{}");
        inv.arguments = QJsonDocument::fromJson(args_str.toUtf8()).object();
        inv.origin = "openai";
        out.push_back(std::move(inv));
    }
    return out;
}

QJsonObject OpenAiAdapter::build_followup_request(
    const QJsonObject& original_response,
    const std::vector<ToolResultEnvelope>& results,
    const QString& user_message,
    const std::vector<fincept::ai_chat::ConversationMessage>& history) const {

    // Reconstruct the messages array: history + user + assistant (the original
    // response's message, including tool_calls) + one role:"tool" entry per result.
    QJsonArray messages;
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    // Echo the assistant's tool_calls so the next request has the matching id.
    QJsonArray choices = original_response["choices"].toArray();
    if (!choices.isEmpty()) {
        QJsonObject assistant_msg = choices[0].toObject()["message"].toObject();
        messages.append(assistant_msg);
    }

    for (const auto& env : results) {
        const QString content =
            QString::fromUtf8(QJsonDocument(env.result.to_json()).toJson(QJsonDocument::Compact));
        messages.append(QJsonObject{
            {"role", "tool"},
            {"tool_call_id", env.call_id},
            {"content", content}});
    }

    QJsonObject body;
    body["messages"] = messages;
    return body;
}

QString OpenAiAdapter::extract_text(const QJsonObject& response) const {
    QJsonArray choices = response["choices"].toArray();
    if (choices.isEmpty()) return {};
    return extract_openai_message_text(choices[0].toObject()["message"].toObject());
}

} // namespace fincept::mcp::dispatch
