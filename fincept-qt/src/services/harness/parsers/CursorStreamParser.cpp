// CursorStreamParser.cpp — Cursor agent CLI stream-json → AgentEvents (see
// CursorStreamParser.h). Only the shapes the output-format page documents are
// interpreted; anything else is shown as a status event. QtCore only.

#include "services/harness/parsers/CursorStreamParser.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace fincept::harness {

namespace cursor_stream_parser_detail {

namespace csp_cad = fincept::services::coding_agents;
namespace csp_sps = fincept::harness::stream_support;

/// The text blocks of message.content (a string is tolerated).
QString message_text(const QJsonObject& o) {
    const QJsonValue content = o.value(QStringLiteral("message")).toObject().value(QStringLiteral("content"));
    if (content.isString())
        return content.toString();
    QStringList parts;
    for (const QJsonValue& v : content.toArray()) {
        const QJsonObject b = v.toObject();
        if (b.value(QStringLiteral("type")).toString() == QLatin1String("text"))
            parts << b.value(QStringLiteral("text")).toString();
    }
    return parts.join(QString());
}

/// The documented call shapes: {"<name>ToolCall": {args, result?}} or
/// {"function": {name, arguments}}.
struct CursorCall {
    QString key;  // "writeToolCall" | "function" | …
    QString name; // "write" | the function name
    QJsonObject body;
};

CursorCall cursor_call(const QJsonObject& tool_call) {
    CursorCall c;
    const QStringList keys = tool_call.keys();
    for (const QString& k : keys) {
        if (k.endsWith(QLatin1String("ToolCall")) && tool_call.value(k).isObject()) {
            c.key = k;
            c.name = k.left(k.size() - 8);
            c.body = tool_call.value(k).toObject();
            return c;
        }
    }
    const QJsonObject fn = tool_call.value(QStringLiteral("function")).toObject();
    if (!fn.isEmpty()) {
        c.key = QStringLiteral("function");
        c.name = fn.value(QStringLiteral("name")).toString();
        c.body = fn;
    }
    return c;
}

} // namespace cursor_stream_parser_detail

using namespace cursor_stream_parser_detail;

QVector<csp_cad::AgentEvent> CursorStreamParser::parse_line(const QString& line) {
    QVector<csp_cad::AgentEvent> out;
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return out;
    QJsonObject o;
    if (!csp_sps::parse_json_object(trimmed, o)) {
        csp_sps::plain_output_line(trimmed, facts_, out);
        return out;
    }
    const QString sid = o.value(QStringLiteral("session_id")).toString();
    if (!sid.isEmpty())
        facts_.session_id = sid.left(200);
    const QString type = o.value(QStringLiteral("type")).toString();
    const QString sub = o.value(QStringLiteral("subtype")).toString();

    if (type == QLatin1String("system")) {
        if (sub == QLatin1String("init")) {
            const QString model = o.value(QStringLiteral("model")).toString();
            if (!model.isEmpty())
                facts_.model = model.left(200);
            QStringList lines;
            if (!model.isEmpty())
                lines << QStringLiteral("model: ") + model;
            if (!facts_.session_id.isEmpty())
                lines << QStringLiteral("session: ") + facts_.session_id;
            const QString key_source = o.value(QStringLiteral("apiKeySource")).toString();
            if (!key_source.isEmpty())
                lines << QStringLiteral("signed in via: ") + key_source.left(40);
            const QString pm = o.value(QStringLiteral("permissionMode")).toString();
            if (!pm.isEmpty())
                lines << QStringLiteral("permission mode: ") + pm.left(40);
            const QString cwd = o.value(QStringLiteral("cwd")).toString();
            if (!cwd.isEmpty())
                lines << QStringLiteral("working folder: ") + cwd;
            out.append(csp_sps::make_event(csp_cad::EventKind::Status,
                                           model.isEmpty() ? QStringLiteral("Session started")
                                                           : QStringLiteral("Session started · ") + model,
                                           lines.join(QLatin1Char('\n')), false, {}, trimmed));
        } else {
            out.append(csp_sps::make_event(csp_cad::EventKind::Status,
                                           sub.isEmpty() ? QStringLiteral("System") : QStringLiteral("System · ") + sub,
                                           csp_sps::json_summary(o, {QStringLiteral("type"), QStringLiteral("subtype"),
                                                                     QStringLiteral("session_id")},
                                                                 2048),
                                           false, {}, trimmed));
        }
    } else if (type == QLatin1String("user")) {
        // The prompt echoed back — nothing new for the transcript.
    } else if (type == QLatin1String("assistant")) {
        const QString text = message_text(o);
        if (!text.trimmed().isEmpty()) {
            facts_.final_text = text;
            out.append(csp_sps::make_event(csp_cad::EventKind::Text, QStringLiteral("Assistant"), text));
        }
    } else if (type == QLatin1String("tool_call")) {
        on_tool_call(o, trimmed, out);
    } else if (type == QLatin1String("result")) {
        on_result(o, trimmed, out);
    } else {
        out.append(csp_sps::make_event(csp_cad::EventKind::Status,
                                       type.isEmpty() ? QStringLiteral("Event") : type.left(60),
                                       csp_sps::json_summary(o, {QStringLiteral("type"), QStringLiteral("session_id")}, 4096),
                                       false, {}, trimmed));
    }
    return out;
}

void CursorStreamParser::on_tool_call(const QJsonObject& o, const QString& raw, QVector<csp_cad::AgentEvent>& out) {
    const QString sub = o.value(QStringLiteral("subtype")).toString();
    const QString id = o.value(QStringLiteral("call_id")).toString().left(200);
    const CursorCall call = cursor_call(o.value(QStringLiteral("tool_call")).toObject());
    QString name = call.name.isEmpty() ? tool_names_.value(id) : call.name;
    if (name.isEmpty())
        name = QStringLiteral("Tool call");

    if (sub == QLatin1String("started")) {
        if (!id.isEmpty())
            tool_names_.insert(id, name);
        ++facts_.tool_calls;
        const QJsonValue args = call.key == QLatin1String("function") ? call.body.value(QStringLiteral("arguments"))
                                                                     : call.body.value(QStringLiteral("args"));
        out.append(csp_sps::make_event(csp_cad::EventKind::ToolCall, name, csp_sps::compact_json(args), false, id));
        return;
    }
    if (sub != QLatin1String("completed")) {
        out.append(csp_sps::make_event(csp_cad::EventKind::Status, QStringLiteral("Tool call · ") + sub.left(40),
                                       csp_sps::json_summary(o, {QStringLiteral("type"), QStringLiteral("session_id")}, 2048),
                                       false, id, raw));
        return;
    }
    // Only {result: {success: {…}}} is documented; any other result is shown as
    // a failure with its raw content.
    const QJsonObject result = call.body.value(QStringLiteral("result")).toObject();
    const QJsonValue success = result.value(QStringLiteral("success"));
    const bool failed = !result.isEmpty() && !success.isObject();
    QString body;
    if (success.isObject()) {
        const QJsonObject s = success.toObject();
        body = s.value(QStringLiteral("content")).isString() ? s.value(QStringLiteral("content")).toString()
                                                              : csp_sps::compact_json(s);
    } else if (!result.isEmpty()) {
        body = csp_sps::compact_json(result);
    } else {
        body = QStringLiteral("(completed)");
    }
    out.append(csp_sps::make_event(csp_cad::EventKind::ToolResult, name, body, failed, id));

    if (call.key == QLatin1String("writeToolCall") && success.isObject()) {
        QString path = success.toObject().value(QStringLiteral("path")).toString();
        if (path.isEmpty())
            path = call.body.value(QStringLiteral("args")).toObject().value(QStringLiteral("path")).toString();
        if (!path.isEmpty())
            out.append(csp_sps::make_event(csp_cad::EventKind::FileChange, QStringLiteral("Files changed (1)"),
                                           csp_cad::file_change_line(QStringLiteral("write"), path), false, id));
    }
}

void CursorStreamParser::on_result(const QJsonObject& o, const QString& raw, QVector<csp_cad::AgentEvent>& out) {
    facts_.saw_result = true;
    const QString sub = o.value(QStringLiteral("subtype")).toString();
    const bool is_err = o.value(QStringLiteral("is_error")).toBool(false) || (!sub.isEmpty() && sub != QLatin1String("success"));
    const QString text = o.value(QStringLiteral("result")).toString();
    facts_.result_is_error = is_err;
    if (!is_err) {
        facts_.final_text = text;
        if (!text.trimmed().isEmpty())
            out.append(csp_sps::make_event(csp_cad::EventKind::Final, QStringLiteral("Result"), text));
    } else {
        QString message = text;
        if (message.trimmed().isEmpty())
            message = QStringLiteral("Run ended: %1").arg(sub.isEmpty() ? QStringLiteral("error") : sub.left(60));
        facts_.result_error = message.left(4000);
        out.append(csp_sps::make_event(csp_cad::EventKind::Error, QStringLiteral("Run ended — error"), message, true, {},
                                       raw));
    }
    const qint64 duration_ms = csp_sps::json_count(o, {"duration_ms"});
    if (duration_ms > 0)
        out.append(csp_sps::make_event(csp_cad::EventKind::Status,
                                       QStringLiteral("Finished in %1 s").arg(static_cast<double>(duration_ms) / 1000.0, 0, 'f', 1)));
}

} // namespace fincept::harness
