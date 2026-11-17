// GeminiStreamParser.cpp — Gemini CLI stream-json → AgentEvents (see
// GeminiStreamParser.h; field names from gemini-cli's output/types.ts).
// QtCore only.

#include "services/harness/parsers/GeminiStreamParser.h"

#include <QJsonObject>
#include <QJsonValue>

namespace fincept::harness {

namespace gemini_stream_parser_detail {

namespace gsp_cad = fincept::services::coding_agents;
namespace gsp_sps = fincept::harness::stream_support;

/// {type, message} → "message (type)".
QString error_text(const QJsonValue& err) {
    if (!err.isObject())
        return gsp_sps::compact_json(err);
    const QJsonObject e = err.toObject();
    const QString message = e.value(QStringLiteral("message")).toString();
    const QString type = e.value(QStringLiteral("type")).toString();
    if (message.isEmpty())
        return type;
    return type.isEmpty() ? message : QStringLiteral("%1 (%2)").arg(message, type);
}

} // namespace gemini_stream_parser_detail

using namespace gemini_stream_parser_detail;

QVector<gsp_cad::AgentEvent> GeminiStreamParser::parse_line(const QString& line) {
    QVector<gsp_cad::AgentEvent> out;
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return out;
    QJsonObject o;
    if (!gsp_sps::parse_json_object(trimmed, o)) {
        gsp_sps::plain_output_line(trimmed, facts_, out);
        return out;
    }
    const QString type = o.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("message")) {
        on_message(o, trimmed, out);
        return out;
    }
    flush(out); // every other event ends the assistant message being streamed

    if (type == QLatin1String("init")) {
        const QString sid = o.value(QStringLiteral("session_id")).toString();
        if (!sid.isEmpty())
            facts_.session_id = sid.left(200);
        const QString model = o.value(QStringLiteral("model")).toString();
        if (!model.isEmpty())
            facts_.model = model.left(200);
        QStringList lines;
        if (!model.isEmpty())
            lines << QStringLiteral("model: ") + model;
        if (!sid.isEmpty())
            lines << QStringLiteral("session: ") + sid;
        out.append(gsp_sps::make_event(gsp_cad::EventKind::Status,
                                       model.isEmpty() ? QStringLiteral("Session started")
                                                       : QStringLiteral("Session started · ") + model,
                                       lines.join(QLatin1Char('\n')), false, {}, trimmed));
    } else if (type == QLatin1String("tool_use")) {
        const QString id = o.value(QStringLiteral("tool_id")).toString().left(200);
        const QString name = o.value(QStringLiteral("tool_name")).toString();
        if (!id.isEmpty())
            tool_names_.insert(id, name);
        ++facts_.tool_calls;
        out.append(gsp_sps::make_event(gsp_cad::EventKind::ToolCall, name.isEmpty() ? QStringLiteral("Tool call") : name,
                                       gsp_sps::compact_json(o.value(QStringLiteral("parameters"))), false, id));
    } else if (type == QLatin1String("tool_result")) {
        const QString id = o.value(QStringLiteral("tool_id")).toString().left(200);
        const bool failed = o.value(QStringLiteral("status")).toString() == QLatin1String("error");
        QString body = o.value(QStringLiteral("output")).toString();
        if (body.isEmpty() && gsp_sps::has_json_value(o.value(QStringLiteral("error"))))
            body = error_text(o.value(QStringLiteral("error")));
        if (body.isEmpty())
            body = failed ? QStringLiteral("(failed)") : QStringLiteral("(no output)");
        const QString name = tool_names_.value(id);
        out.append(gsp_sps::make_event(gsp_cad::EventKind::ToolResult,
                                       name.isEmpty() ? QStringLiteral("Tool result") : name, body, failed, id));
    } else if (type == QLatin1String("error")) {
        const QString message = o.value(QStringLiteral("message")).toString();
        const bool warning = o.value(QStringLiteral("severity")).toString() == QLatin1String("warning");
        if (!warning)
            facts_.last_error = message.left(2000);
        out.append(gsp_sps::make_event(warning ? gsp_cad::EventKind::Status : gsp_cad::EventKind::Error,
                                       warning ? QStringLiteral("Warning") : QStringLiteral("Error"), message, !warning, {},
                                       trimmed));
    } else if (type == QLatin1String("result")) {
        on_result(o, trimmed, out);
    } else {
        out.append(gsp_sps::make_event(gsp_cad::EventKind::Status,
                                       type.isEmpty() ? QStringLiteral("Event") : type.left(60),
                                       gsp_sps::json_summary(o, {QStringLiteral("type"), QStringLiteral("timestamp")}, 4096),
                                       false, {}, trimmed));
    }
    return out;
}

QVector<gsp_cad::AgentEvent> GeminiStreamParser::finish() {
    QVector<gsp_cad::AgentEvent> out;
    flush(out);
    return out;
}

void GeminiStreamParser::on_message(const QJsonObject& o, const QString& raw, QVector<gsp_cad::AgentEvent>& out) {
    const QString role = o.value(QStringLiteral("role")).toString();
    const QString content = o.value(QStringLiteral("content")).toString();
    if (role == QLatin1String("user")) {
        flush(out);
        return; // the prompt echoed back
    }
    if (role != QLatin1String("assistant")) {
        flush(out);
        out.append(gsp_sps::make_event(gsp_cad::EventKind::Status, QStringLiteral("Message · ") + role.left(40), content,
                                       false, {}, raw));
        return;
    }
    if (o.value(QStringLiteral("delta")).toBool(false)) {
        if (!acc_.active())
            acc_.start(gsp_cad::EventKind::Text, QStringLiteral("msg-%1").arg(++message_counter_),
                       QStringLiteral("Assistant"));
        if (auto ev = acc_.append(content))
            out.append(*ev);
        return;
    }
    // A whole assistant message. When it repeats what was just streamed it is
    // that message's complete event; otherwise the streamed one ends first.
    QString key;
    if (acc_.active()) {
        if (!acc_.text().isEmpty() && content.startsWith(acc_.text())) {
            key = acc_.key();
            acc_.clear();
        } else {
            flush(out);
        }
    }
    if (key.isEmpty())
        key = QStringLiteral("msg-%1").arg(++message_counter_);
    if (content.trimmed().isEmpty())
        return;
    facts_.final_text = content;
    out.append(gsp_sps::make_event(gsp_cad::EventKind::Text, QStringLiteral("Assistant"), content, false, key));
}

void GeminiStreamParser::on_result(const QJsonObject& o, const QString& raw, QVector<gsp_cad::AgentEvent>& out) {
    facts_.saw_result = true;
    const QString status = o.value(QStringLiteral("status")).toString();
    const bool ok = status == QLatin1String("success");
    facts_.result_is_error = !ok;
    if (ok) {
        if (!facts_.final_text.trimmed().isEmpty())
            out.append(gsp_sps::make_event(gsp_cad::EventKind::Final, QStringLiteral("Result"), facts_.final_text));
    } else {
        QString message = error_text(o.value(QStringLiteral("error")));
        if (message.trimmed().isEmpty())
            message = QStringLiteral("Gemini CLI reported status “%1”.").arg(status.isEmpty() ? QStringLiteral("?") : status.left(40));
        facts_.result_error = message.left(4000);
        out.append(gsp_sps::make_event(gsp_cad::EventKind::Error, QStringLiteral("Run ended — error"), message, true, {},
                                       raw));
    }

    const QJsonObject stats = o.value(QStringLiteral("stats")).toObject();
    if (stats.isEmpty())
        return;
    // StreamStats: input_tokens includes the cached ones (cached + input).
    facts_.input_tokens = gsp_sps::json_count(stats, {"input_tokens"});
    facts_.output_tokens = gsp_sps::json_count(stats, {"output_tokens"});
    facts_.cached_tokens = gsp_sps::json_count(stats, {"cached"});
    QStringList parts;
    parts << QStringLiteral("input %1 tok").arg(gsp_sps::grouped_number(facts_.input_tokens));
    if (facts_.cached_tokens > 0)
        parts << QStringLiteral("cached %1").arg(gsp_sps::grouped_number(facts_.cached_tokens));
    parts << QStringLiteral("output %1 tok").arg(gsp_sps::grouped_number(facts_.output_tokens));
    const qint64 calls = gsp_sps::json_count(stats, {"tool_calls"});
    if (calls > 0)
        parts << QStringLiteral("%1 tool calls").arg(calls);
    const qint64 duration_ms = gsp_sps::json_count(stats, {"duration_ms"});
    if (duration_ms > 0)
        parts << QStringLiteral("%1 s").arg(static_cast<double>(duration_ms) / 1000.0, 0, 'f', 1);
    out.append(gsp_sps::make_event(gsp_cad::EventKind::Usage, QStringLiteral("Usage"), parts.join(QStringLiteral(" · ")),
                                   false, {}, gsp_sps::compact_json(stats)));
}

void GeminiStreamParser::flush(QVector<gsp_cad::AgentEvent>& out) {
    if (!acc_.active())
        return;
    QString full;
    if (auto ev = acc_.finish_message(&full)) {
        out.append(*ev);
        facts_.final_text = full;
    }
}

} // namespace fincept::harness
