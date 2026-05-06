// Tool-execution layer for LlmService.
// do_tool_loop: structured OpenAI-style follow-up (15 rounds of parallel calls).
// try_extract_and_execute_text_tool_calls: XML/text fallback for models without structured tool_calls.

#include "ai_chat/LlmService.h"

#include "ai_chat/LlmContentExtractors.h"
#include "ai_chat/LlmRequestPolicy.h"
#include "core/logging/Logger.h"
#include "mcp/McpService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>

#include <vector>

namespace fincept::ai_chat {

namespace { constexpr const char* kToolLoopTag = "LlmService"; }

LlmResponse LlmService::do_tool_loop(QJsonArray loop_messages, const QString& url,
                                     const QMap<QString, QString>& headers) {
    LlmResponse resp;
    // 15 reasoning steps (each may issue many parallel tool calls).
    static constexpr int MAX_ROUNDS = 15;
    LOG_INFO(kToolLoopTag, QString("TOOL LOOP: starting (max %1 rounds, model=%2)").arg(MAX_ROUNDS).arg(model_));

    for (int round = 0; round < MAX_ROUNDS; ++round) {
        QJsonObject fu;
        fu["model"]      = model_;
        fu["messages"]   = loop_messages;
        fu["max_tokens"] = resolved_max_tokens();

        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai(detail::apply_request_policy(tool_filter_));
        if (!tools.isEmpty())
            fu["tools"] = tools;

        auto http = blocking_post(url, fu, headers);
        if (!http.success) {
            resp.error = "Tool follow-up failed: " + http.error;
            return resp;
        }

        auto doc = QJsonDocument::fromJson(http.body);
        if (doc.isNull()) {
            resp.error = "Parse error in tool follow-up";
            return resp;
        }
        QJsonObject rj = doc.object();

        QJsonArray choices = rj["choices"].toArray();
        if (choices.isEmpty()) {
            resp.error = "No choices in tool follow-up";
            return resp;
        }

        QJsonObject msg = choices[0].toObject()["message"].toObject();
        QJsonArray tcs = msg["tool_calls"].toArray();

        if (!tcs.isEmpty()) {
            loop_messages.append(msg);
            for (const auto& tc_val : tcs) {
                QJsonObject tc = tc_val.toObject();
                QString cid = tc["id"].toString();
                QString fname = tc["function"].toObject()["name"].toString();
                QJsonObject fa =
                    QJsonDocument::fromJson(tc["function"].toObject()["arguments"].toString("{}").toUtf8()).object();

                LOG_INFO(kToolLoopTag, QString("TOOL LOOP r%1: executing %2 args=%3").arg(round).arg(fname,
                              QString::fromUtf8(QJsonDocument(fa).toJson(QJsonDocument::Compact)).left(200)));
                auto tr = mcp::McpService::instance().execute_openai_function(fname, fa);
                LOG_INFO(kToolLoopTag, QString("TOOL LOOP r%1: %2 -> %3 (msg=%4 err=%5)")
                                  .arg(round).arg(fname,
                                       tr.success ? "OK" : "FAIL", tr.message.left(120), tr.error.left(120)));
                loop_messages.append(QJsonObject{
                    {"role", "tool"},
                    {"tool_call_id", cid},
                    {"content", QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact))}});
            }
            continue;
        }

        resp.content = extract_openai_message_text(msg);
        parse_usage(resp, rj, provider_);
        resp.success = !resp.content.isEmpty();
        LOG_INFO(kToolLoopTag, QString("TOOL LOOP: finished after %1 round(s) — %2 chars of text")
                          .arg(round + 1).arg(resp.content.length()));
        return resp;
    }

    // Force one tool-less summary turn so the user doesn't see an empty bubble.
    LOG_WARN(kToolLoopTag, "TOOL LOOP: exceeded max rounds — forcing summary turn (no tools)");
    {
        loop_messages.append(QJsonObject{
            {"role", "system"},
            {"content", "You have used your tool-call budget. Reply now with a final answer to the user "
                        "summarizing what you accomplished and what (if anything) is incomplete. Do not "
                        "request any more tools."}});

        QJsonObject fu;
        fu["model"]      = model_;
        fu["messages"]   = loop_messages;
        fu["max_tokens"] = resolved_max_tokens();
        // No tools field — forces a text response.

        auto http = blocking_post(url, fu, headers);
        if (http.success) {
            auto doc = QJsonDocument::fromJson(http.body);
            if (!doc.isNull()) {
                QJsonObject rj = doc.object();
                QJsonArray choices = rj["choices"].toArray();
                if (!choices.isEmpty()) {
                    QJsonObject msg = choices[0].toObject()["message"].toObject();
                    resp.content = extract_openai_message_text(msg);
                    parse_usage(resp, rj, provider_);
                    resp.success = !resp.content.isEmpty();
                    LOG_INFO(kToolLoopTag, QString("TOOL LOOP: summary fallback produced %1 chars")
                                      .arg(resp.content.length()));
                    if (resp.success)
                        return resp;
                }
            }
        }
    }
    resp.error = "Tool call loop exceeded maximum rounds";
    return resp;
}

/// Detect XML/text tool invocations from models without structured tool_calls.
std::optional<LlmResponse> LlmService::try_extract_and_execute_text_tool_calls(const QString& content,
                                                                               const QString& user_message,
                                                                               const QString& url,
                                                                               const QMap<QString, QString>& headers) {
    // Patterns tried in order: <tool_call>JSON</tool_call>, <invoke name="..."/>,
    // (ns:)tool_call ... /(ns:)tool_call, ```tool_call``` fenced blocks.
    struct TextToolCall {
        QString name;
        QJsonObject args;
    };
    std::vector<TextToolCall> calls;

    LOG_INFO(kToolLoopTag, "Checking for text-based tool calls in response (" + QString::number(content.length()) + " chars)");

    {
        static const QRegularExpression rx("<tool_call>\\s*(\\{[\\s\\S]*?\\})\\s*</tool_call>",
                                           QRegularExpression::MultilineOption |
                                               QRegularExpression::DotMatchesEverythingOption);
        auto it = rx.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            auto doc = QJsonDocument::fromJson(m.captured(1).toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString name = obj["name"].toString();
                if (name.isEmpty())
                    name = obj["function"].toString();
                QJsonObject args = obj["arguments"].toObject();
                if (args.isEmpty() && obj["arguments"].isString())
                    args = QJsonDocument::fromJson(obj["arguments"].toString().toUtf8()).object();
                if (!name.isEmpty())
                    calls.push_back({name, args});
            }
        }
    }

    if (calls.empty()) {
        static const QRegularExpression rx_invoke("<invoke\\s+name=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</invoke>",
                                                  QRegularExpression::MultilineOption |
                                                      QRegularExpression::DotMatchesEverythingOption);
        static const QRegularExpression rx_param("<parameter\\s+name=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</parameter>",
                                                 QRegularExpression::MultilineOption |
                                                     QRegularExpression::DotMatchesEverythingOption);

        LOG_INFO(kToolLoopTag, "Pattern 2: invoke regex valid=" + QString(rx_invoke.isValid() ? "yes" : "no"));
        auto dbg_match = rx_invoke.match(content);
        LOG_INFO(kToolLoopTag, "Pattern 2: hasMatch=" + QString(dbg_match.hasMatch() ? "yes" : "no"));

        auto it = rx_invoke.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            QString name = m.captured(1);
            QString body = m.captured(2).trimmed();
            QJsonObject args;

            // <parameter> children first.
            auto pit = rx_param.globalMatch(body);
            bool has_params = false;
            while (pit.hasNext()) {
                auto pm = pit.next();
                QString pname = pm.captured(1);
                QString pval = pm.captured(2).trimmed();
                has_params = true;

                auto pdoc = QJsonDocument::fromJson(pval.toUtf8());
                if (!pdoc.isNull()) {
                    if (pdoc.isArray())
                        args[pname] = pdoc.array();
                    else if (pdoc.isObject())
                        args[pname] = pdoc.object();
                    else
                        args[pname] = pval;
                } else {
                    args[pname] = pval;
                }
            }

            // No <parameter> tags — try the body as a JSON object.
            if (!has_params && !body.isEmpty()) {
                auto jdoc = QJsonDocument::fromJson(body.toUtf8());
                if (jdoc.isObject())
                    args = jdoc.object();
            }

            if (!name.isEmpty())
                calls.push_back({name, args});
        }
    }

    if (calls.empty()) {
        static const QRegularExpression rx("(?:\\w+:)?tool_call\\s+([\\s\\S]*?)\\s*/(?:\\w+:)?tool_call",
                                           QRegularExpression::MultilineOption);
        auto it = rx.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            QString body = m.captured(1).trimmed();

            auto doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString name = obj["name"].toString();
                if (name.isEmpty())
                    name = obj["function"].toString();
                QJsonObject args = obj["arguments"].toObject();
                if (args.isEmpty() && obj["arguments"].isString())
                    args = QJsonDocument::fromJson(obj["arguments"].toString().toUtf8()).object();
                if (!name.isEmpty()) {
                    calls.push_back({name, args});
                    continue;
                }
            }

            // Not JSON — model emitted a raw query/SQL. Route to any external tool whose schema accepts query|sql|statement.
            auto all_tools = mcp::McpService::instance().get_all_tools();
            for (const auto& tool : all_tools) {
                if (tool.is_internal)
                    continue;
                QJsonObject schema = tool.input_schema;
                QJsonObject props  = schema["properties"].toObject();
                for (auto pit = props.constBegin(); pit != props.constEnd(); ++pit) {
                    QString key = pit.key().toLower();
                    if (key == "query" || key == "sql" || key == "statement") {
                        QString fn_name = tool.server_id + "__" + tool.name;
                        QJsonObject args;
                        args[pit.key()] = body;
                        calls.push_back({fn_name, args});
                        break;
                    }
                }
                if (!calls.empty())
                    break;
            }
        }
    }

    if (calls.empty()) {
        static const QRegularExpression rx("```tool_call\\s*\\n([\\s\\S]*?)\\n\\s*```",
                                           QRegularExpression::MultilineOption);
        auto it = rx.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            auto doc = QJsonDocument::fromJson(m.captured(1).trimmed().toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString name = obj["name"].toString();
                QJsonObject args = obj["arguments"].toObject();
                if (args.isEmpty() && obj["arguments"].isString())
                    args = QJsonDocument::fromJson(obj["arguments"].toString().toUtf8()).object();
                if (!name.isEmpty())
                    calls.push_back({name, args});
            }
        }
    }

    if (calls.empty())
        return std::nullopt;

    LOG_INFO(kToolLoopTag, QString("Detected %1 text-based tool call(s) in response").arg(calls.size()));

    QString tool_results;
    for (const auto& tc : calls) {
        LOG_INFO(kToolLoopTag, "Executing text-detected tool: " + tc.name);
        auto tr = mcp::McpService::instance().execute_openai_function(tc.name, tc.args);

        QString result_content;
        if (!tr.message.isEmpty())
            result_content = tr.message;
        else if (!tr.data.isNull() && !tr.data.isUndefined())
            result_content =
                QString::fromUtf8(QJsonDocument(tr.data.toObject()).toJson(QJsonDocument::Compact)).left(4000);
        else
            result_content = QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact)).left(4000);

        int sep = tc.name.indexOf("__");
        QString short_name = (sep >= 0) ? tc.name.mid(sep + 2) : tc.name;
        tool_results += "\n**Tool: " + short_name + "**\n" + result_content + "\n";
    }

    if (tool_results.isEmpty())
        return std::nullopt;

    LlmResponse resp;
    QString follow_prompt = "The user asked: \"" + user_message +
                            "\"\n\n"
                            "I executed the requested tool(s) and obtained these results:\n" +
                            tool_results +
                            "\n\nPlease provide a clear, concise summary of this data in natural language. "
                            "Do NOT emit any tool calls or XML markup in your response.";

    QJsonObject follow_body;
    if (provider_ == "anthropic") {
        QJsonArray msgs;
        msgs.append(QJsonObject{{"role", "user"}, {"content", follow_prompt}});
        follow_body["model"]      = model_;
        follow_body["messages"]   = msgs;
        follow_body["max_tokens"] = resolved_max_tokens();
        if (!system_prompt_.isEmpty())
            follow_body["system"] = system_prompt_;
    } else if (provider_ == "fincept") {
        QJsonArray msgs;
        if (!system_prompt_.isEmpty())
            msgs.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
        msgs.append(QJsonObject{{"role", "user"}, {"content", follow_prompt}});
        follow_body["messages"] = msgs;
        if (!model_.isEmpty() && model_ != "fincept-llm")
            follow_body["model"] = model_;
    } else {
        QJsonArray msgs;
        if (!system_prompt_.isEmpty())
            msgs.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
        msgs.append(QJsonObject{{"role", "user"}, {"content", follow_prompt}});
        follow_body["model"]      = model_;
        follow_body["messages"]   = msgs;
        follow_body["max_tokens"] = resolved_max_tokens();
    }

    // Follow-up has no tools — prevents infinite loop.
    auto fu = blocking_post(url, follow_body, headers);
    if (!fu.success) {
        resp.content = tool_results;
        resp.success = true;
        return resp;
    }

    auto fu_doc = QJsonDocument::fromJson(fu.body);
    if (fu_doc.isNull()) {
        resp.content = tool_results;
        resp.success = true;
        return resp;
    }

    QJsonObject fu_rj = fu_doc.object();

    if (provider_ == "anthropic") {
        resp.content = extract_anthropic_content_text(fu_rj["content"].toArray());
    } else if (provider_ == "fincept") {
        // {"success":..., "data":{"choices":[...]}} or top-level shape.
        QJsonObject data = fu_rj.contains("data") ? fu_rj["data"].toObject() : fu_rj;
        QJsonArray fu_choices = data["choices"].toArray();
        if (!fu_choices.isEmpty())
            resp.content = extract_openai_message_text(fu_choices[0].toObject()["message"].toObject());
    } else {
        QJsonArray choices = fu_rj["choices"].toArray();
        if (!choices.isEmpty())
            resp.content = extract_openai_message_text(choices[0].toObject()["message"].toObject());
    }

    if (resp.content.isEmpty())
        resp.content = tool_results;

    resp.success = true;
    parse_usage(resp, fu_rj, provider_);
    return resp;
}

} // namespace fincept::ai_chat
