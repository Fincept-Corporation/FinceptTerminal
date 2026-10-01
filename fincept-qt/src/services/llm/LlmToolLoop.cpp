// LlmToolLoop.cpp — tool-execution layer for LlmService.
//
// Two entry points:
//   - do_tool_loop: structured OpenAI-compatible tool-call follow-up loop
//     (handles up to 15 reasoning rounds with parallel tool execution).
//   - try_extract_and_execute_text_tool_calls: detects XML/text tool calls
//     emitted by models that don't support structured tool_calls (minimax,
//     some OpenRouter models). Executes them and asks the LLM to summarise.

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/ResultStore.h"
#include "services/llm/LlmContentExtractors.h"
#include "services/llm/LlmRequestPolicy.h"
#include "services/llm/LlmService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>

#include <algorithm>
#include <vector>

namespace fincept::ai_chat {

namespace {
constexpr const char* kLlmToolLoopTag = "LlmService";
} // namespace

LlmResponse LlmService::do_tool_loop(QJsonArray loop_messages, const QString& url,
                                     const QMap<QString, QString>& headers, QSet<QString> activated_tools) {
    LlmResponse resp;
    // Bounded, LRU-evicting view over the discovered-tool set — see
    // ActivationTracker for why an unbounded set defeats Tier-0.
    detail::ActivationTracker activated(activated_tools);
    // MiniMax-style models spend 3 rounds per action (tool_list → tool_describe
    // → actual call), so the default 40 gives ~13 real actions — enough for a
    // full report template fill. Below ~12 silently cripples report-builder flows.
    const int MAX_ROUNDS = active_max_tool_rounds();
    // Rounds bound the token spend; the deadline bounds the wait. A round that
    // long-polls a background job can take 30 s, so MAX_ROUNDS alone leaves the
    // turn unbounded in wall-clock terms.
    detail::ToolLoopBudget budget(MAX_ROUNDS);
    LOG_INFO(kLlmToolLoopTag, QString("TOOL LOOP: starting (max %1 rounds, %2 s deadline, model=%3)")
                                  .arg(MAX_ROUNDS)
                                  .arg(detail::tool_loop_deadline_ms() / 1000)
                                  .arg(model_));

    for (int round = 0; !budget.exhausted(); ++round) {
        // Stop pressed: leave before issuing another request or running more tools.
        if (detail::cancel_requested()) {
            LOG_INFO(kLlmToolLoopTag, QString("TOOL LOOP: cancelled by user before round %1").arg(round));
            resp.cancelled = true;
            resp.error = "Request cancelled";
            return resp;
        }
        budget.note_round();
        QJsonObject fu;
        fu["model"] = model_;
        fu["messages"] = loop_messages;
        // Temperature intentionally omitted — provider default.
        apply_openai_token_limit(fu);

        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai(
            detail::apply_request_policy(tool_filter_), activated.names());
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
            // Another round
            loop_messages.append(msg);
            // Collect the whole round before executing any of it: the calls in
            // one assistant turn are independent by construction, so they are
            // dispatched together (read-only ones concurrently, writes as
            // barriers — see detail::execute_tool_calls).
            std::vector<detail::PendingToolCall> calls;
            calls.reserve(static_cast<std::size_t>(tcs.size()));
            for (const auto& tc_val : tcs) {
                const QJsonObject tc = tc_val.toObject();
                detail::PendingToolCall c;
                c.call_id = tc["id"].toString();
                c.wire_name = tc["function"].toObject()["name"].toString();
                c.args =
                    QJsonDocument::fromJson(tc["function"].toObject()["arguments"].toString("{}").toUtf8()).object();
                // Strip "<server>__" prefix for user-visible label; decode any wire encoding.
                c.display = c.wire_name;
                const int sep = c.display.indexOf(QStringLiteral("__"));
                if (sep > 0)
                    c.display = c.display.mid(sep + 2);
                c.display.replace(QStringLiteral("-dot-"), QStringLiteral("."));

                LOG_INFO(kLlmToolLoopTag,
                         QString("TOOL LOOP r%1: executing %2 args=%3")
                             .arg(round)
                             .arg(c.wire_name,
                                  QString::fromUtf8(QJsonDocument(c.args).toJson(QJsonDocument::Compact)).left(200)));
                // Progress lines are emitted here, on the loop thread, in the
                // model's order — not from the workers, whose completion order
                // is arbitrary and whose thread has no business touching the UI.
                detail::emit_tool_progress(c.display, c.args);
                calls.push_back(std::move(c));
            }

            // allow_defer is set inside execute_tool_calls: this is the one call
            // site that knows about the job_* tools, so it is the one allowed to
            // receive a receipt instead of a result for a long-running tool.
            const auto results = detail::execute_tool_calls(calls);
            for (std::size_t k = 0; k < calls.size(); ++k) {
                const auto& c = calls[k];
                const auto& tr = results[k];
                LOG_INFO(kLlmToolLoopTag,
                         QString("TOOL LOOP r%1: %2 -> %3 (msg=%4 err=%5)")
                             .arg(round)
                             .arg(c.wire_name, tr.success ? "OK" : "FAIL", tr.message.left(120), tr.error.left(120)));
                // If this was a discovery call (tool_list / tool_describe), declare
                // what it surfaced so the model can actually call it next round.
                // Applied in the model's order so the activation LRU evicts the
                // same way it did when execution was sequential.
                detail::note_tool_activations(c.display, c.args, tr, activated);
                loop_messages.append(QJsonObject{{"role", "tool"},
                                                 {"tool_call_id", c.call_id},
                                                 {"content", detail::encode_tool_result_for_llm(c.display, tr)}});
            }
            continue;
        }

        // Final text response
        resp.content = strip_think_blocks(extract_openai_message_text(msg));
        parse_usage(resp, rj, provider_);
        resp.success = !resp.content.isEmpty();
        // An empty final turn used to come back as a failure with NO message, which
        // the Quick Chat bubble rendered as a bare "Error: ".
        if (!resp.success)
            resp.error = "The model returned an empty reply after using tools";
        LOG_INFO(kLlmToolLoopTag, QString("TOOL LOOP: finished after %1 round(s) — %2 chars of text")
                                      .arg(round + 1)
                                      .arg(resp.content.length()));

        // The "final" text may actually contain text-based tool markup
        // (<minimax:tool_call>, <invoke name=…>, etc.) that the model emitted
        // in place of structured tool_calls. If so, route through the
        // text-extraction path so the markup gets executed instead of leaking
        // into the chat bubble as raw XML.
        static const QRegularExpression rx_has_text_markup(
            QStringLiteral("<\\s*/?\\s*(?:\\w+\\s*:\\s*)?tool_call\\b|<\\s*invoke\\s+name="),
            QRegularExpression::CaseInsensitiveOption);
        if (resp.success && rx_has_text_markup.match(resp.content).hasMatch()) {
            LOG_INFO(kLlmToolLoopTag,
                     "TOOL LOOP: final content has text-tool markup — re-routing through text-extraction");
            QString user_msg;
            for (const auto& v : loop_messages) {
                QJsonObject o = v.toObject();
                if (o["role"].toString() == "user") {
                    user_msg = o["content"].toString();
                    break; // first user msg is the original prompt
                }
            }
            auto text_result = try_extract_and_execute_text_tool_calls(resp.content, user_msg, url, headers);
            if (text_result.has_value() && (text_result->success || text_result->cancelled))
                return text_result.value();
        }
        return resp;
    }

    // Budget exhausted. Force one final turn so the chat doesn't go silent, and
    // tell the model WHICH budget ran out — "you are out of rounds" and "you are
    // out of time" call for different summaries, and a model told only "budget"
    // tends to relitigate the plan instead of reporting what it has.
    const QString exhaustion = budget.exhaustion_note();
    LOG_WARN(kLlmToolLoopTag, "TOOL LOOP: " + exhaustion + " — forcing summary turn");
    {
        loop_messages.append(QJsonObject{
            {"role", "system"},
            {"content", "You have used your tool budget (" + exhaustion +
                            "). Reply now with a final summary of what you accomplished and what (if anything) is "
                            "incomplete. If there are critical remaining tool calls, you may make them, but prefer "
                            "summarising."}});

        QJsonObject fu;
        fu["model"] = model_;
        fu["messages"] = loop_messages;
        apply_openai_token_limit(fu);
        // Keep tools available so structured tool_calls still work. Stripping
        // them used to force text-markup mode (raw <minimax:tool_call> blobs)
        // which leaked into the chat bubble. Carry the activated set so a
        // last-ditch tool call can still reference a discovered tool.
        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai(
            detail::apply_request_policy(tool_filter_), activated.names());
        if (!tools.isEmpty())
            fu["tools"] = tools;

        auto http = blocking_post(url, fu, headers);
        if (http.success) {
            auto doc = QJsonDocument::fromJson(http.body);
            if (!doc.isNull()) {
                QJsonObject rj = doc.object();
                QJsonArray choices = rj["choices"].toArray();
                if (!choices.isEmpty()) {
                    QJsonObject msg = choices[0].toObject()["message"].toObject();
                    resp.content = strip_think_blocks(extract_openai_message_text(msg));
                    parse_usage(resp, rj, provider_);
                    resp.success = !resp.content.isEmpty();
                    LOG_INFO(kLlmToolLoopTag,
                             QString("TOOL LOOP: summary fallback produced %1 chars").arg(resp.content.length()));

                    // If the model emitted text-tool markup instead of structured
                    // tool_calls, run it through extraction so the markup is
                    // executed instead of dumped raw into the chat bubble.
                    static const QRegularExpression rx_has_text_markup(
                        QStringLiteral("<\\s*/?\\s*(?:\\w+\\s*:\\s*)?tool_call\\b|<\\s*invoke\\s+name="),
                        QRegularExpression::CaseInsensitiveOption);
                    if (resp.success && rx_has_text_markup.match(resp.content).hasMatch()) {
                        LOG_INFO(kLlmToolLoopTag, "TOOL LOOP: summary fallback has text-tool markup — re-routing");
                        QString user_msg;
                        for (const auto& v : loop_messages) {
                            QJsonObject o = v.toObject();
                            if (o["role"].toString() == "user") {
                                user_msg = o["content"].toString();
                                break;
                            }
                        }
                        auto text_result =
                            try_extract_and_execute_text_tool_calls(resp.content, user_msg, url, headers);
                        if (text_result.has_value() && (text_result->success || text_result->cancelled))
                            return text_result.value();
                    }
                    if (resp.success)
                        return resp;
                }
            }
        }
    }
    resp.error = "Tool call loop exceeded maximum rounds";
    return resp;
}

// ============================================================================
// Text-based tool call extraction
// ============================================================================
// Models that don't support structured tool_calls (e.g. minimax, some
// OpenRouter models) may emit tool invocations as XML/text markup in their
// response. This method detects common patterns and executes the tools.

std::optional<LlmResponse> LlmService::try_extract_and_execute_text_tool_calls(const QString& content,
                                                                               const QString& user_message,
                                                                               const QString& url,
                                                                               const QMap<QString, QString>& headers) {
    // ── Pattern 1: <tool_call>{"name":"...", "arguments":{...}}</tool_call>
    // ── Pattern 2: <invoke name="..." args='{"key":"val"}'>
    // ── Pattern 3: minimax:tool_call  CONTENT /minimax:tool_call
    // ── Pattern 4: ```tool_call\n{"name":"...", "arguments":{...}}\n```
    // ── Pattern 5: function_name(arg1, arg2) style calls embedded in text

    LOG_INFO(kLlmToolLoopTag,
             "Checking for text-based tool calls in response (" + QString::number(content.length()) + " chars)");

    // Pattern matching lives in LlmContentExtractors (shared with the Fincept
    // async path, which is prompt-injected and therefore always text-mode).
    QStringList raw_blocks;
    std::vector<TextToolCall> calls = extract_text_tool_calls(content, &raw_blocks);

    // A `[ns:]tool_call` block whose body isn't JSON is a raw command (the case
    // that motivated this: the model emitting bare SQL inside minimax tags).
    // Map it onto the first external tool with a query/sql/statement parameter.
    if (calls.empty() && !raw_blocks.isEmpty()) {
        auto all_tools = mcp::McpService::instance().get_all_tools();
        for (const auto& body : raw_blocks) {
            for (const auto& tool : all_tools) {
                if (tool.is_internal)
                    continue;
                const QJsonObject props = tool.input_schema["properties"].toObject();
                bool matched = false;
                for (auto pit = props.constBegin(); pit != props.constEnd(); ++pit) {
                    const QString key = pit.key().toLower();
                    if (key == "query" || key == "sql" || key == "statement") {
                        QJsonObject args;
                        args[pit.key()] = body;
                        calls.push_back(
                            {tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name), args});
                        matched = true;
                        break;
                    }
                }
                if (matched)
                    break;
            }
            if (!calls.empty())
                break;
        }
    }

    if (calls.empty())
        return std::nullopt;

    LOG_INFO(kLlmToolLoopTag, QString("Detected %1 text-based tool call(s) in response").arg(calls.size()));

    // Execute each detected tool call
    QString tool_results;
    for (const auto& tc : calls) {
        if (detail::cancel_requested())
            break;
        LOG_INFO(kLlmToolLoopTag, "Executing text-detected tool: " + tc.name);
        int sep_disp = tc.name.indexOf(QStringLiteral("__"));
        QString display = (sep_disp >= 0) ? tc.name.mid(sep_disp + 2) : tc.name;
        display.replace(QStringLiteral("-dot-"), QStringLiteral("."));
        detail::emit_tool_progress(display, tc.args);
        auto tr = mcp::McpService::instance().execute_openai_function(tc.name, tc.args);

        // Shaped rather than clipped at a byte offset. Two reasons: `.left(4000)`
        // cut the JSON mid-token, leaving the model to interpret a malformed
        // fragment; and `tr.data.toObject()` silently rendered array payloads as
        // "{}" — a whole result vanishing without a trace. shrink_json keeps the
        // JSON valid and marks in-place what it dropped.
        //
        // No result_id handoff here: the follow-up request on this path attaches
        // no tools, so pointing the model at result_fetch would be a dead
        // instruction. This path is the fallback for models without structured
        // tool calls; the structured loop above is where the overflow store applies.
        QString result_content;
        if (!tr.message.isEmpty()) {
            result_content = tr.message;
        } else {
            QJsonValue payload = (!tr.data.isNull() && !tr.data.isUndefined()) ? tr.data : QJsonValue(tr.to_json());
            mcp::shrink_json(payload, 4000);
            result_content = payload.isObject()
                                 ? QString::fromUtf8(QJsonDocument(payload.toObject()).toJson(QJsonDocument::Compact))
                             : payload.isArray()
                                 ? QString::fromUtf8(QJsonDocument(payload.toArray()).toJson(QJsonDocument::Compact))
                                 : payload.toVariant().toString();
        }

        int sep = tc.name.indexOf("__");
        QString short_name = (sep >= 0) ? tc.name.mid(sep + 2) : tc.name;
        tool_results += "\n**Tool: " + short_name + "**\n" + result_content + "\n";
    }

    if (detail::cancel_requested()) {
        // Stop pressed while the tools ran: don't spend another request summarising.
        LlmResponse cancelled_resp;
        cancelled_resp.cancelled = true;
        cancelled_resp.error = "Request cancelled";
        return cancelled_resp;
    }

    if (tool_results.isEmpty())
        return std::nullopt;

    // Build follow-up request asking the LLM to summarize the tool results
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
        follow_body["model"] = model_;
        follow_body["messages"] = msgs;
        follow_body["max_tokens"] = resolved_max_tokens();
        // Temperature intentionally omitted — Anthropic default.
        if (!system_prompt_.isEmpty())
            follow_body["system"] = system_prompt_;
    } else if (provider_ == "gemini" || provider_ == "google") {
        // `url` here is the Gemini generateContent endpoint, so the body must
        // be Gemini-shaped. It previously fell through to the OpenAI branch
        // below and posted {model, messages, max_tokens} at :generateContent,
        // which 400s — the user then saw the raw-tool-output fallback instead
        // of an answer.
        follow_body["contents"] =
            QJsonArray{QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", follow_prompt}}}}}};
        follow_body["generationConfig"] = QJsonObject{{"maxOutputTokens", resolved_max_tokens()}};
        if (!system_prompt_.isEmpty())
            follow_body["systemInstruction"] = QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", system_prompt_}}}}};
    } else if (provider_ == "fincept") {
        // /research/chat uses messages array
        QJsonArray msgs;
        if (!system_prompt_.isEmpty())
            msgs.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
        msgs.append(QJsonObject{{"role", "user"}, {"content", follow_prompt}});
        follow_body["messages"] = msgs;
        if (!model_.isEmpty() && model_ != "fincept-llm")
            follow_body["model"] = model_;
    } else {
        // OpenAI-compatible
        QJsonArray msgs;
        if (!system_prompt_.isEmpty())
            msgs.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
        msgs.append(QJsonObject{{"role", "user"}, {"content", follow_prompt}});
        follow_body["model"] = model_;
        follow_body["messages"] = msgs;
        // Temperature intentionally omitted — provider default.
        apply_openai_token_limit(follow_body);
    }

    // Last-resort rendering when the summarisation turn can't be obtained.
    // Raw tool JSON is NOT an answer — label it so the user can tell the
    // difference, and log the reason instead of silently passing it off as one.
    auto raw_fallback = [&](const QString& reason) -> LlmResponse {
        LOG_WARN(kLlmToolLoopTag, "Text-tool follow-up unavailable (" + reason + ") — returning raw tool output");
        resp.content = "*(could not summarise — " + reason + ". Raw tool output below.)*\n" + tool_results;
        resp.success = true;
        return resp;
    };

    // No tools in follow-up to prevent infinite loop
    auto fu = blocking_post(url, follow_body, headers);
    if (!fu.success)
        return raw_fallback(fu.error.isEmpty() ? QStringLiteral("follow-up request failed") : fu.error);

    auto fu_doc = QJsonDocument::fromJson(fu.body);
    if (fu_doc.isNull())
        return raw_fallback(QStringLiteral("follow-up response was not JSON"));

    QJsonObject fu_rj = fu_doc.object();

    // Extract text from follow-up response (provider-aware)
    if (provider_ == "anthropic") {
        resp.content = extract_anthropic_content_text(fu_rj["content"].toArray());
    } else if (provider_ == "gemini" || provider_ == "google") {
        const QJsonArray cands = fu_rj["candidates"].toArray();
        if (!cands.isEmpty())
            resp.content = extract_gemini_parts_text(cands[0].toObject()["content"].toObject()["parts"].toArray());
    } else if (provider_ == "fincept") {
        // /research/chat: {"success":true,"data":{"choices":[{"message":{"content":"..."}}]}}
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
        return raw_fallback(QStringLiteral("follow-up returned no text"));

    resp.success = true;
    parse_usage(resp, fu_rj, provider_);
    return resp;
}

} // namespace fincept::ai_chat
