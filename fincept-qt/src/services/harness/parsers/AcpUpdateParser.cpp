// AcpUpdateParser.cpp — one ACP session update per line → AgentEvents (see
// AcpUpdateParser.h). Field names follow the Agent Client Protocol schema
// (agentclientprotocol.com/protocol/schema): SessionNotification {sessionId,
// update}, SessionUpdate.sessionUpdate, ContentChunk {content, messageId},
// ToolCall {toolCallId, title, kind, status, content, locations, rawInput,
// rawOutput}, ToolCallContent {type: content | diff | terminal}, PlanEntry
// {content, priority, status}, PromptResponse {stopReason, usage}, Usage
// {inputTokens, outputTokens, totalTokens, thoughtTokens, cachedReadTokens}.
// Everything is read defensively; QtCore only.

#include "services/harness/parsers/AcpUpdateParser.h"

#include <QJsonArray>
#include <QJsonValue>

namespace fincept::harness {

namespace acp_update_parser_detail {

namespace aup_cad = fincept::services::coding_agents;
namespace aup_sps = fincept::harness::stream_support;

/// Updates that describe the session rather than the conversation: they never
/// end a message being streamed.
bool is_metadata_update(const QString& kind) {
    return kind == QLatin1String("available_commands_update") || kind == QLatin1String("current_mode_update") ||
           kind == QLatin1String("config_option_update") || kind == QLatin1String("session_info_update") ||
           kind == QLatin1String("usage_update");
}

/// A ContentChunk's content: one ContentBlock (an array, or a block without a
/// type but with text, is tolerated).
QString chunk_text(const QJsonValue& content) {
    if (content.isObject()) {
        const QJsonObject b = content.toObject();
        if (!b.contains(QStringLiteral("type")) && b.value(QStringLiteral("text")).isString())
            return b.value(QStringLiteral("text")).toString();
    }
    return aup_sps::content_text(content);
}

QString stop_reason_label(const QString& reason) {
    if (reason == QLatin1String("max_tokens"))
        return QStringLiteral("the model's output token limit was reached");
    if (reason == QLatin1String("max_turn_requests"))
        return QStringLiteral("the maximum number of model requests for this turn was reached");
    if (reason == QLatin1String("refusal"))
        return QStringLiteral("the model refused to continue");
    if (reason == QLatin1String("cancelled"))
        return QStringLiteral("the turn was cancelled");
    return QStringLiteral("stopped (%1)").arg(reason.left(60));
}

/// The readable body of a tool call's ToolCallContent[].
QString tool_content_text(const QJsonValue& content) {
    QStringList parts;
    for (const QJsonValue& v : content.toArray()) {
        const QJsonObject item = v.toObject();
        const QString type = item.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("content")) {
            const QString t = aup_sps::content_text(item.value(QStringLiteral("content")));
            if (!t.isEmpty())
                parts << t;
        } else if (type == QLatin1String("diff")) {
            const bool is_new = !item.contains(QStringLiteral("oldText")) || item.value(QStringLiteral("oldText")).isNull();
            parts << QStringLiteral("diff  %1%2").arg(item.value(QStringLiteral("path")).toString(),
                                                    is_new ? QStringLiteral("  (new file)") : QString());
        } else if (type == QLatin1String("terminal")) {
            parts << QStringLiteral("[terminal %1]").arg(item.value(QStringLiteral("terminalId")).toString());
        } else if (!item.isEmpty()) {
            parts << aup_sps::compact_json(item);
        }
    }
    return parts.join(QLatin1Char('\n'));
}

void acp_add_unique(QStringList& list, const QString& value) {
    if (!value.isEmpty() && !list.contains(value))
        list << value;
}

} // namespace acp_update_parser_detail

using namespace acp_update_parser_detail;

QVector<aup_cad::AgentEvent> AcpUpdateParser::parse_line(const QString& line) {
    QVector<aup_cad::AgentEvent> out;
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return out;

    QJsonObject o;
    if (!aup_sps::parse_json_object(trimmed, o)) {
        aup_sps::plain_output_line(trimmed, facts_, out);
        return out;
    }
    note_session(o);

    // JSON-RPC notification or request.
    const QString method = o.value(QStringLiteral("method")).toString();
    if (!method.isEmpty()) {
        const QJsonObject params = o.value(QStringLiteral("params")).toObject();
        note_session(params);
        if (method == QLatin1String("session/update") && params.value(QStringLiteral("update")).isObject()) {
            on_update(params.value(QStringLiteral("update")).toObject(), trimmed, out);
            return out;
        }
        // e.g. session/request_permission — a client request this stream cannot answer.
        flush(out);
        out.append(aup_sps::make_event(aup_cad::EventKind::Status, QStringLiteral("ACP · ") + method,
                                       aup_sps::json_summary(params, {}, 2048), false, {}, trimmed));
        return out;
    }
    // The bare notification params, or a bare update.
    if (o.value(QStringLiteral("update")).isObject()) {
        on_update(o.value(QStringLiteral("update")).toObject(), trimmed, out);
        return out;
    }
    if (o.contains(QStringLiteral("sessionUpdate"))) {
        on_update(o, trimmed, out);
        return out;
    }
    // A JSON-RPC response (the prompt's result or an error).
    if (o.contains(QStringLiteral("result")) || o.contains(QStringLiteral("error"))) {
        on_response(o, trimmed, out);
        return out;
    }
    // A bare PromptResponse.
    if (o.contains(QStringLiteral("stopReason"))) {
        on_stop(o.value(QStringLiteral("stopReason")).toString(), o.value(QStringLiteral("usage")).toObject(), trimmed,
                out);
        return out;
    }
    flush(out);
    out.append(aup_sps::make_event(aup_cad::EventKind::Status, QStringLiteral("Event"),
                                   aup_sps::json_summary(o, {QStringLiteral("jsonrpc")}, 4096), false, {}, trimmed));
    return out;
}

QVector<aup_cad::AgentEvent> AcpUpdateParser::finish() {
    QVector<aup_cad::AgentEvent> out;
    flush(out);
    return out;
}

void AcpUpdateParser::note_session(const QJsonObject& o) {
    const QString sid = aup_sps::json_string(o, {"sessionId", "session_id"});
    if (!sid.isEmpty())
        facts_.session_id = sid.left(200);
}

void AcpUpdateParser::on_update(const QJsonObject& update, const QString& raw, QVector<aup_cad::AgentEvent>& out) {
    const QString kind = update.value(QStringLiteral("sessionUpdate")).toString();
    note_session(update);

    if (kind == QLatin1String("agent_message_chunk")) {
        on_chunk(aup_cad::EventKind::Text, update, out);
        return;
    }
    if (kind == QLatin1String("agent_thought_chunk")) {
        on_chunk(aup_cad::EventKind::Thinking, update, out);
        return;
    }
    if (is_metadata_update(kind)) {
        if (kind == QLatin1String("usage_update")) {
            // {used, size, cost?{amount, currency}} — context size, not per-turn tokens.
            const QJsonObject cost = update.value(QStringLiteral("cost")).toObject();
            if (cost.value(QStringLiteral("amount")).isDouble() &&
                cost.value(QStringLiteral("currency")).toString().compare(QLatin1String("USD"), Qt::CaseInsensitive) == 0)
                facts_.cost_usd = cost.value(QStringLiteral("amount")).toDouble();
        } else if (kind == QLatin1String("current_mode_update")) {
            const QString mode = update.value(QStringLiteral("currentModeId")).toString();
            if (!mode.isEmpty())
                out.append(aup_sps::make_event(aup_cad::EventKind::Status, QStringLiteral("Mode · ") + mode.left(60)));
        }
        return; // commands / config options / session title: nothing for the transcript
    }

    flush(out); // every other update ends the message being streamed
    if (kind == QLatin1String("user_message_chunk"))
        return; // the prompt echoed back
    if (kind == QLatin1String("tool_call")) {
        on_tool(update, true, raw, out);
        return;
    }
    if (kind == QLatin1String("tool_call_update")) {
        on_tool(update, false, raw, out);
        return;
    }
    if (kind == QLatin1String("plan")) {
        QStringList lines;
        for (const QJsonValue& v : update.value(QStringLiteral("entries")).toArray()) {
            const QJsonObject e = v.toObject();
            const QString status = e.value(QStringLiteral("status")).toString();
            const QString mark = status == QLatin1String("completed")     ? QStringLiteral("[x] ")
                                 : status == QLatin1String("in_progress") ? QStringLiteral("[~] ")
                                                                          : QStringLiteral("[ ] ");
            QString text = mark + e.value(QStringLiteral("content")).toString();
            if (e.value(QStringLiteral("priority")).toString() == QLatin1String("high"))
                text += QStringLiteral("  (high priority)");
            lines << text;
        }
        out.append(aup_sps::make_event(aup_cad::EventKind::Status, QStringLiteral("Plan"), lines.join(QLatin1Char('\n')),
                                       false, QStringLiteral("plan")));
        return;
    }
    if (kind == QLatin1String("notice")) {
        const QString severity = update.value(QStringLiteral("severity")).toString();
        const QString title = update.value(QStringLiteral("title")).toString();
        const QString description = update.value(QStringLiteral("description")).toString();
        const bool err = severity == QLatin1String("error");
        if (err)
            facts_.last_error = (title + QLatin1Char('\n') + description).trimmed().left(2000);
        out.append(aup_sps::make_event(err ? aup_cad::EventKind::Error : aup_cad::EventKind::Status,
                                       title.isEmpty() ? QStringLiteral("Notice") : title, description, err, {}, raw));
        return;
    }
    out.append(aup_sps::make_event(aup_cad::EventKind::Status,
                                   kind.isEmpty() ? QStringLiteral("Update") : QStringLiteral("Update · ") + kind.left(60),
                                   aup_sps::json_summary(update, {QStringLiteral("sessionUpdate")}, 2048), false, {}, raw));
}

void AcpUpdateParser::on_chunk(aup_cad::EventKind kind, const QJsonObject& update, QVector<aup_cad::AgentEvent>& out) {
    const QString piece = chunk_text(update.value(QStringLiteral("content")));
    const QString mid = update.value(QStringLiteral("messageId")).toString().left(200);
    const bool same_message = acc_.active() && acc_.kind() == kind &&
                              (mid.isEmpty() || acc_.message_id().isEmpty() || mid == acc_.message_id());
    if (!same_message) {
        flush(out);
        const bool text = kind == aup_cad::EventKind::Text;
        QString key;
        if (mid.isEmpty())
            key = QStringLiteral("%1-%2").arg(text ? QStringLiteral("msg") : QStringLiteral("thought")).arg(++message_counter_);
        else
            key = text ? mid : QStringLiteral("thought:") + mid;
        acc_.start(kind, key, text ? QStringLiteral("Assistant") : QStringLiteral("Thinking"), mid);
    }
    if (auto ev = acc_.append(piece))
        out.append(*ev);
}

void AcpUpdateParser::on_tool(const QJsonObject& update, bool is_new, const QString& raw,
                              QVector<aup_cad::AgentEvent>& out) {
    Q_UNUSED(is_new); // a tool_call_update for an id never announced is announced first
    const QString id = update.value(QStringLiteral("toolCallId")).toString().left(200);
    if (id.isEmpty()) {
        out.append(aup_sps::make_event(aup_cad::EventKind::Status, QStringLiteral("Tool call"),
                                       aup_sps::json_summary(update, {QStringLiteral("sessionUpdate")}, 2048), false,
                                       {}, raw));
        return;
    }
    ToolState& st = tools_[id];
    const QString title = update.value(QStringLiteral("title")).toString();
    if (!title.isEmpty())
        st.title = title;
    else if (st.title.isEmpty())
        st.title = update.value(QStringLiteral("name")).toString();
    const QString kind = update.value(QStringLiteral("kind")).toString();
    if (!kind.isEmpty())
        st.kind = kind;
    for (const QJsonValue& v : update.value(QStringLiteral("locations")).toArray())
        acp_add_unique(st.locations, v.toObject().value(QStringLiteral("path")).toString());
    const QJsonValue content = update.value(QStringLiteral("content"));
    for (const QJsonValue& v : content.toArray()) {
        const QJsonObject item = v.toObject();
        if (item.value(QStringLiteral("type")).toString() != QLatin1String("diff"))
            continue;
        const QString path = item.value(QStringLiteral("path")).toString();
        acp_add_unique(st.diff_paths, path);
        if (!item.contains(QStringLiteral("oldText")) || item.value(QStringLiteral("oldText")).isNull())
            acp_add_unique(st.new_files, path);
    }

    const QString display = !st.title.isEmpty() ? st.title : (!st.kind.isEmpty() ? st.kind : QStringLiteral("Tool call"));
    if (!st.announced) {
        st.announced = true;
        ++facts_.tool_calls;
        QString body;
        const QJsonValue input = update.value(QStringLiteral("rawInput"));
        if (aup_sps::has_json_value(input))
            body = aup_sps::compact_json(input);
        else if (!st.locations.isEmpty())
            body = st.locations.join(QLatin1Char('\n'));
        else
            body = tool_content_text(content);
        out.append(aup_sps::make_event(aup_cad::EventKind::ToolCall, display, body, false, id));
    }

    const QString status = update.value(QStringLiteral("status")).toString();
    const bool failed = status == QLatin1String("failed");
    if ((status != QLatin1String("completed") && !failed) || st.finished)
        return; // pending / in_progress: the call is already shown
    st.finished = true;
    QString body = tool_content_text(content);
    if (body.isEmpty()) {
        const QJsonValue output = update.value(QStringLiteral("rawOutput"));
        if (aup_sps::has_json_value(output))
            body = aup_sps::compact_json(output);
    }
    if (body.isEmpty())
        body = failed ? QStringLiteral("(failed)") : QStringLiteral("(completed)");
    out.append(aup_sps::make_event(aup_cad::EventKind::ToolResult, display, body, failed, id));
    if (failed)
        return;

    QStringList lines;
    if (!st.diff_paths.isEmpty()) {
        for (const QString& p : std::as_const(st.diff_paths)) {
            const QString k = st.kind == QLatin1String("delete") ? QStringLiteral("delete")
                              : st.new_files.contains(p)         ? QStringLiteral("add")
                              : st.kind == QLatin1String("move") ? QStringLiteral("move")
                                                                 : QStringLiteral("update");
            lines << aup_cad::file_change_line(k, p);
        }
    } else if (st.kind == QLatin1String("edit") || st.kind == QLatin1String("delete") ||
               st.kind == QLatin1String("move")) {
        const QString k = st.kind == QLatin1String("edit") ? QStringLiteral("update") : st.kind;
        for (const QString& p : std::as_const(st.locations))
            lines << aup_cad::file_change_line(k, p);
    }
    if (!lines.isEmpty())
        out.append(aup_sps::make_event(aup_cad::EventKind::FileChange,
                                       QStringLiteral("Files changed (%1)").arg(lines.size()),
                                       lines.join(QLatin1Char('\n')), false, id));
}

void AcpUpdateParser::on_response(const QJsonObject& o, const QString& raw, QVector<aup_cad::AgentEvent>& out) {
    const QJsonValue err = o.value(QStringLiteral("error"));
    if (aup_sps::has_json_value(err)) {
        // In a one-prompt stream the only request whose answer is printed is the
        // prompt itself, so an error response ends the run as failed.
        flush(out);
        QString msg;
        QString title = QStringLiteral("Agent error");
        if (err.isObject()) {
            const QJsonObject e = err.toObject();
            msg = e.value(QStringLiteral("message")).toString();
            const QJsonValue data = e.value(QStringLiteral("data"));
            if (aup_sps::has_json_value(data))
                msg += (msg.isEmpty() ? QString() : QStringLiteral("\n")) + aup_sps::compact_json(data).left(2000);
            const QJsonValue code = e.value(QStringLiteral("code"));
            if (code.isDouble())
                title += QStringLiteral(" %1").arg(static_cast<qint64>(code.toDouble()));
        } else {
            msg = aup_sps::compact_json(err);
        }
        if (msg.trimmed().isEmpty())
            msg = QStringLiteral("The agent returned an error without a message.");
        facts_.last_error = msg.left(2000);
        facts_.saw_result = true;
        facts_.result_is_error = true;
        facts_.result_error = msg.left(4000);
        out.append(aup_sps::make_event(aup_cad::EventKind::Error, title, msg, true, {}, raw));
        return;
    }
    const QJsonObject result = o.value(QStringLiteral("result")).toObject();
    note_session(result);
    if (result.contains(QStringLiteral("stopReason"))) {
        on_stop(result.value(QStringLiteral("stopReason")).toString(), result.value(QStringLiteral("usage")).toObject(),
                raw, out);
        return;
    }
    if (!result.isEmpty()) // initialize / session/new … answers; an empty acknowledgement shows nothing
        out.append(aup_sps::make_event(aup_cad::EventKind::Status, QStringLiteral("ACP response"),
                                       aup_sps::json_summary(result, {}, 2048), false, {}, raw));
}

void AcpUpdateParser::on_stop(const QString& reason, const QJsonObject& usage, const QString& raw,
                              QVector<aup_cad::AgentEvent>& out) {
    flush(out);
    facts_.saw_result = true;
    facts_.stop_reason = reason.left(80);
    const bool ok = reason.isEmpty() || reason == QLatin1String("end_turn");
    facts_.result_is_error = !ok;
    if (ok) {
        if (!facts_.final_text.trimmed().isEmpty())
            out.append(aup_sps::make_event(aup_cad::EventKind::Final, QStringLiteral("Result"), facts_.final_text));
    } else {
        const QString label = stop_reason_label(reason);
        facts_.result_error = QStringLiteral("Run ended: ") + label;
        out.append(aup_sps::make_event(aup_cad::EventKind::Error, QStringLiteral("Run ended — ") + label,
                                       QStringLiteral("Stop reason: ") + reason, true, {}, raw));
    }
    take_usage(usage, raw, out);
}

void AcpUpdateParser::take_usage(const QJsonObject& usage, const QString& raw, QVector<aup_cad::AgentEvent>& out) {
    static const char* const kKeys[] = {"inputTokens",  "input_tokens",    "outputTokens",
                                        "output_tokens", "totalTokens",    "total_tokens"};
    bool any = false;
    for (const char* k : kKeys)
        any = any || usage.value(QLatin1String(k)).isDouble();
    if (!any)
        return;
    const qint64 in = aup_sps::json_count(usage, {"inputTokens", "input_tokens"});
    const qint64 outt = aup_sps::json_count(usage, {"outputTokens", "output_tokens"});
    const qint64 cached = aup_sps::json_count(usage, {"cachedReadTokens", "cached_read_tokens"});
    const qint64 thought = aup_sps::json_count(usage, {"thoughtTokens", "thought_tokens"});
    facts_.input_tokens += in;
    facts_.output_tokens += outt;
    facts_.cached_tokens += cached;
    QStringList parts;
    if (facts_.cost_usd.has_value())
        parts << QStringLiteral("cost $%1").arg(*facts_.cost_usd, 0, 'f', 4);
    parts << QStringLiteral("input %1 tok").arg(aup_sps::grouped_number(in));
    if (cached > 0)
        parts << QStringLiteral("cached %1").arg(aup_sps::grouped_number(cached));
    parts << QStringLiteral("output %1 tok").arg(aup_sps::grouped_number(outt));
    if (thought > 0)
        parts << QStringLiteral("thinking %1").arg(aup_sps::grouped_number(thought));
    out.append(aup_sps::make_event(aup_cad::EventKind::Usage, QStringLiteral("Usage"), parts.join(QStringLiteral(" · ")),
                                   false, {}, raw));
}

void AcpUpdateParser::flush(QVector<aup_cad::AgentEvent>& out) {
    if (!acc_.active())
        return;
    const aup_cad::EventKind kind = acc_.kind();
    QString full;
    if (auto ev = acc_.finish_message(&full)) {
        out.append(*ev);
        if (kind == aup_cad::EventKind::Text)
            facts_.final_text = full;
    }
}

} // namespace fincept::harness
