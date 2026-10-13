// AgentStreamParsers.cpp — Claude Code stream-json / Codex exec --json → AgentEvents.
//
// Field names follow the CLIs' documented event shapes (claude 2.1.x, codex-cli
// 0.15x). Everything is read defensively: a missing field degrades to an empty
// value, an unknown type to a generic status event — the transcript must never
// lose a line, and the terminal must never crash on a CLI upgrade.

#include "services/coding_agents/AgentStreamParsers.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

namespace fincept::services::coding_agents {

namespace agent_stream_parsers_detail {

AgentEvent make_event(EventKind kind, const QString& title, const QString& body = {}, bool is_error = false,
                      const QString& ref_id = {}, const QString& raw = {}) {
    AgentEvent ev;
    ev.kind = kind;
    ev.title = title.left(300).simplified();
    ev.body = clip_utf8(body, kAgentEventBodyMaxBytes);
    ev.raw_json = raw.isEmpty() ? QString() : clip_utf8(raw, kAgentEventRawMaxBytes);
    ev.is_error = is_error;
    ev.ref_id = ref_id;
    return ev;
}

QString compact(const QJsonValue& v) {
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isString())
        return v.toString();
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 15);
    return {};
}

bool has_value(const QJsonValue& v) {
    if (v.isUndefined() || v.isNull())
        return false;
    if (v.isString())
        return !v.toString().isEmpty();
    if (v.isArray())
        return !v.toArray().isEmpty();
    if (v.isObject())
        return !v.toObject().isEmpty();
    return true;
}

/// Tool-result content: a string, an array of content blocks, or an MCP
/// CallToolResult object ({content:[…], structuredContent?, isError?}).
QString content_text(const QJsonValue& content) {
    if (content.isString())
        return content.toString();
    if (content.isArray()) {
        QStringList parts;
        for (const auto& v : content.toArray()) {
            if (v.isString()) {
                parts << v.toString();
                continue;
            }
            const QJsonObject b = v.toObject();
            const QString type = b.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("text"))
                parts << b.value(QStringLiteral("text")).toString();
            else if (type == QLatin1String("image"))
                parts << QStringLiteral("[image]");
            else if (type == QLatin1String("resource") || type == QLatin1String("resource_link")) {
                QString uri = b.value(QStringLiteral("uri")).toString();
                if (uri.isEmpty())
                    uri = b.value(QStringLiteral("resource")).toObject().value(QStringLiteral("uri")).toString();
                parts << QStringLiteral("[resource] ") + uri;
            } else
                parts << compact(v);
        }
        return parts.join(QLatin1Char('\n'));
    }
    if (content.isObject()) {
        const QJsonObject o = content.toObject();
        if (o.contains(QStringLiteral("content")))
            return content_text(o.value(QStringLiteral("content")));
        return compact(content);
    }
    return compact(content);
}

bool looks_like_error(const QString& line) {
    static const QRegularExpression rx(
        QStringLiteral(R"(\b(error|failed|failure|fatal|exception|denied|unauthori[sz]ed|forbidden|invalid api key|not logged in|panic)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(line).hasMatch();
}

/// Compact JSON of `o` minus bookkeeping keys — the generic rendering for
/// event types this build does not know.
QString object_summary(const QJsonObject& o, const QStringList& skip_keys, int max_bytes) {
    QJsonObject copy = o;
    for (const auto& k : skip_keys)
        copy.remove(k);
    return clip_utf8(compact(copy), max_bytes);
}

QString group_digits(qint64 v) {
    QString s = QString::number(v < 0 ? -v : v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(i, QLatin1Char(','));
    return v < 0 ? QStringLiteral("-") + s : s;
}

qint64 json_int(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? static_cast<qint64>(v.toDouble()) : 0;
}

QString claude_result_subtype_label(const QString& subtype) {
    if (subtype == QLatin1String("error_max_turns"))
        return QStringLiteral("maximum number of turns reached");
    if (subtype == QLatin1String("error_during_execution"))
        return QStringLiteral("error during execution");
    if (subtype == QLatin1String("error_max_budget_usd"))
        return QStringLiteral("budget cap reached");
    if (subtype.isEmpty())
        return QStringLiteral("error");
    return subtype;
}

/// Shared non-JSON line handling: the CLIs occasionally print plain text on
/// stdout (warnings, update notices) — keep it, flag it when it reads like an error.
void plain_line(const QString& trimmed, StreamFacts& facts, QVector<AgentEvent>& out) {
    const bool err = looks_like_error(trimmed);
    if (err)
        facts.last_error = trimmed.left(2000);
    out.append(make_event(err ? EventKind::Error : EventKind::Status,
                          err ? QStringLiteral("CLI error output") : QStringLiteral("CLI output"), trimmed, err));
}

const QStringList& bookkeeping_keys() {
    static const QStringList keys = {QStringLiteral("type"), QStringLiteral("subtype"), QStringLiteral("session_id"),
                                     QStringLiteral("uuid")};
    return keys;
}

} // namespace agent_stream_parsers_detail

using namespace agent_stream_parsers_detail;

// ── LineBuffer ───────────────────────────────────────────────────────────────

QList<QByteArray> LineBuffer::feed(const QByteArray& chunk) {
    QList<QByteArray> lines;
    buf_.append(chunk);
    qsizetype start = 0;
    while (true) {
        const qsizetype nl = buf_.indexOf('\n', start);
        if (nl < 0)
            break;
        qsizetype end = nl;
        if (end > start && buf_.at(end - 1) == '\r')
            --end;
        lines.append(buf_.mid(start, end - start));
        start = nl + 1;
    }
    if (start > 0)
        buf_.remove(0, start);
    if (buf_.size() > kMaxLineBytes) {
        lines.append(buf_);
        buf_.clear();
    }
    return lines;
}

QByteArray LineBuffer::take_rest() {
    QByteArray rest = buf_;
    buf_.clear();
    if (rest.endsWith('\r'))
        rest.chop(1);
    return rest;
}

QString display_tool_name(const QString& name) {
    if (name.startsWith(QLatin1String("mcp__"))) {
        const QString rest = name.mid(5);
        const qsizetype sep = rest.indexOf(QLatin1String("__"));
        if (sep > 0)
            return rest.left(sep) + QLatin1Char('.') + rest.mid(sep + 2);
        return rest;
    }
    return name;
}

// ── Claude Code stream-json ──────────────────────────────────────────────────

QVector<AgentEvent> ClaudeStreamParser::parse_line(const QString& line) {
    QVector<AgentEvent> out;
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return out;

    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        plain_line(trimmed, facts_, out);
        return out;
    }

    const QJsonObject o = doc.object();
    const QString sid = o.value(QStringLiteral("session_id")).toString();
    if (!sid.isEmpty())
        facts_.session_id = sid;

    const QString type = o.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("system"))
        on_system(o, trimmed, out);
    else if (type == QLatin1String("assistant"))
        on_assistant(o, out);
    else if (type == QLatin1String("user"))
        on_user(o, out);
    else if (type == QLatin1String("result"))
        on_result(o, trimmed, out);
    else if (type == QLatin1String("stream_event") || type == QLatin1String("keep_alive"))
        return out; // partial deltas (not requested) / heartbeats — nothing to show
    else
        out.append(make_event(EventKind::Status, type.isEmpty() ? QStringLiteral("Event") : type,
                              object_summary(o, bookkeeping_keys(), 4096), false, {}, trimmed));
    return out;
}

void ClaudeStreamParser::on_system(const QJsonObject& o, const QString& raw, QVector<AgentEvent>& out) {
    const QString sub = o.value(QStringLiteral("subtype")).toString();

    if (sub == QLatin1String("init")) {
        const QString model = o.value(QStringLiteral("model")).toString();
        if (!model.isEmpty())
            facts_.model = model;
        const QJsonArray tools = o.value(QStringLiteral("tools")).toArray();
        int mcp_tools = 0;
        for (const auto& t : tools) {
            if (t.toString().startsWith(QLatin1String("mcp__")))
                ++mcp_tools;
        }
        QStringList lines;
        if (!model.isEmpty())
            lines << QStringLiteral("model: ") + model;
        if (!facts_.session_id.isEmpty())
            lines << QStringLiteral("session: ") + facts_.session_id;
        const QString pm = o.value(QStringLiteral("permissionMode")).toString();
        if (!pm.isEmpty())
            lines << QStringLiteral("permission mode: ") + pm;
        const QString cwd = o.value(QStringLiteral("cwd")).toString();
        if (!cwd.isEmpty())
            lines << QStringLiteral("working folder: ") + cwd;
        lines << QStringLiteral("tools: %1 (%2 from MCP)").arg(tools.size()).arg(mcp_tools);

        QStringList bad;
        for (const auto& sv : o.value(QStringLiteral("mcp_servers")).toArray()) {
            const QJsonObject s = sv.toObject();
            const QString name = s.value(QStringLiteral("name")).toString();
            const QString status = s.value(QStringLiteral("status")).toString();
            lines << QStringLiteral("MCP %1: %2").arg(name, status.isEmpty() ? QStringLiteral("?") : status);
            if (!status.isEmpty() && status != QLatin1String("connected"))
                bad << QStringLiteral("%1 (%2)").arg(name, status);
        }
        out.append(make_event(EventKind::Status,
                              model.isEmpty() ? QStringLiteral("Session started")
                                              : QStringLiteral("Session started · ") + model,
                              lines.join(QLatin1Char('\n')), false, {}, raw));
        if (!bad.isEmpty()) {
            facts_.mcp_problems = bad;
            out.append(make_event(EventKind::Error, QStringLiteral("MCP server not connected"),
                                  QStringLiteral("Not reachable: %1\nTerminal tools are unavailable for this run — check "
                                                 "Agent Workbench › CONNECT.")
                                      .arg(bad.join(QStringLiteral(", "))),
                                  true));
        }
        const QJsonValue errs = o.value(QStringLiteral("mcp_server_errors"));
        if (has_value(errs))
            out.append(make_event(EventKind::Error, QStringLiteral("MCP server errors"), compact(errs), true));
        return;
    }

    if (sub == QLatin1String("api_retry")) {
        const qint64 attempt = json_int(o, "attempt");
        const qint64 max_retries = json_int(o, "max_retries");
        const qint64 delay_ms = json_int(o, "retry_delay_ms");
        const QString status = compact(o.value(QStringLiteral("error_status")));
        const QString error = compact(o.value(QStringLiteral("error")));
        QStringList parts;
        if (!status.isEmpty())
            parts << QStringLiteral("status ") + status;
        if (delay_ms > 0)
            parts << QStringLiteral("retrying in %1 s").arg(static_cast<double>(delay_ms) / 1000.0, 0, 'f', 1);
        if (!error.isEmpty())
            parts << error;
        if (!error.isEmpty())
            facts_.last_error = error.left(2000);
        const QString title = attempt > 0 ? (max_retries > 0 ? QStringLiteral("API retry %1/%2").arg(attempt).arg(max_retries)
                                                             : QStringLiteral("API retry %1").arg(attempt))
                                          : QStringLiteral("API retry");
        out.append(make_event(EventKind::Status, title, parts.join(QStringLiteral(" · ")), false, {}, raw));
        return;
    }

    if (sub == QLatin1String("permission_denied")) {
        QString tool = o.value(QStringLiteral("tool_name")).toString();
        if (tool.isEmpty())
            tool = o.value(QStringLiteral("tool")).toString();
        out.append(make_event(EventKind::Error,
                              tool.isEmpty() ? QStringLiteral("Permission denied")
                                             : QStringLiteral("Permission denied: ") + display_tool_name(tool),
                              object_summary(o, bookkeeping_keys(), 4096), true, {}, raw));
        return;
    }

    if (sub == QLatin1String("compact_boundary")) {
        out.append(make_event(EventKind::Status, QStringLiteral("Context compacted"),
                              object_summary(o, bookkeeping_keys(), 1024), false, {}, raw));
        return;
    }

    out.append(make_event(EventKind::Status,
                          sub.isEmpty() ? QStringLiteral("System") : QStringLiteral("System · ") + sub,
                          object_summary(o, bookkeeping_keys(), 2048), false, {}, raw));
}

void ClaudeStreamParser::on_assistant(const QJsonObject& o, QVector<AgentEvent>& out) {
    const QJsonObject msg = o.value(QStringLiteral("message")).toObject();
    const QJsonValue parent = o.value(QStringLiteral("parent_tool_use_id"));
    const QString prefix = (parent.isString() && !parent.toString().isEmpty()) ? QStringLiteral("↳ ") : QString();
    if (facts_.model.isEmpty())
        facts_.model = msg.value(QStringLiteral("model")).toString();

    // API failures (auth, rate limit, overload) are reported on the assistant message.
    const QJsonValue api_error = o.value(QStringLiteral("error"));
    if (has_value(api_error)) {
        const QString e = compact(api_error);
        facts_.last_error = e.left(2000);
        out.append(make_event(EventKind::Error, QStringLiteral("API error"), e, true));
    }

    const QJsonValue content = msg.value(QStringLiteral("content"));
    if (content.isString()) {
        if (!content.toString().trimmed().isEmpty())
            out.append(make_event(EventKind::Text, prefix + QStringLiteral("Assistant"), content.toString()));
        return;
    }
    for (const auto& bv : content.toArray()) {
        const QJsonObject b = bv.toObject();
        const QString bt = b.value(QStringLiteral("type")).toString();
        if (bt == QLatin1String("text")) {
            const QString t = b.value(QStringLiteral("text")).toString();
            if (!t.trimmed().isEmpty())
                out.append(make_event(EventKind::Text, prefix + QStringLiteral("Assistant"), t));
        } else if (bt == QLatin1String("thinking")) {
            const QString t = b.value(QStringLiteral("thinking")).toString();
            if (!t.trimmed().isEmpty())
                out.append(make_event(EventKind::Thinking, prefix + QStringLiteral("Thinking"), t));
        } else if (bt == QLatin1String("redacted_thinking")) {
            out.append(make_event(EventKind::Thinking, prefix + QStringLiteral("Thinking"),
                                  QStringLiteral("[redacted by the provider]")));
        } else if (bt == QLatin1String("tool_use") || bt == QLatin1String("server_tool_use")) {
            const QString id = b.value(QStringLiteral("id")).toString();
            const QString name = b.value(QStringLiteral("name")).toString();
            if (!id.isEmpty())
                tool_names_.insert(id, name);
            ++facts_.tool_calls;
            out.append(make_event(EventKind::ToolCall, prefix + display_tool_name(name),
                                  compact(b.value(QStringLiteral("input"))), false, id));
        } else if (bt.endsWith(QLatin1String("_tool_result"))) {
            // Server-side tool results (e.g. web_search_tool_result) arrive on the assistant message.
            const QString id = b.value(QStringLiteral("tool_use_id")).toString();
            const QString name = tool_names_.value(id, bt);
            out.append(make_event(EventKind::ToolResult, prefix + display_tool_name(name),
                                  content_text(b.value(QStringLiteral("content"))), false, id));
        } else if (!bt.isEmpty()) {
            out.append(make_event(EventKind::Status, prefix + QStringLiteral("Assistant · ") + bt,
                                  clip_utf8(compact(b), 2048)));
        }
    }
}

void ClaudeStreamParser::on_user(const QJsonObject& o, QVector<AgentEvent>& out) {
    const QJsonObject msg = o.value(QStringLiteral("message")).toObject();
    const QJsonValue content = msg.value(QStringLiteral("content"));
    if (!content.isArray())
        return; // echoed prompt text — nothing new for the transcript
    const QJsonValue parent = o.value(QStringLiteral("parent_tool_use_id"));
    const QString prefix = (parent.isString() && !parent.toString().isEmpty()) ? QStringLiteral("↳ ") : QString();
    for (const auto& bv : content.toArray()) {
        const QJsonObject b = bv.toObject();
        const QString bt = b.value(QStringLiteral("type")).toString();
        if (bt == QLatin1String("tool_result")) {
            const QString id = b.value(QStringLiteral("tool_use_id")).toString();
            const QString name = tool_names_.value(id);
            const bool is_err = b.value(QStringLiteral("is_error")).toBool(false);
            out.append(make_event(EventKind::ToolResult,
                                  prefix + (name.isEmpty() ? QStringLiteral("Tool result") : display_tool_name(name)),
                                  content_text(b.value(QStringLiteral("content"))), is_err, id));
        } else if (bt == QLatin1String("text")) {
            const QString t = b.value(QStringLiteral("text")).toString();
            if (!t.trimmed().isEmpty())
                out.append(make_event(EventKind::Status, prefix + QStringLiteral("Message"), t));
        }
    }
}

void ClaudeStreamParser::on_result(const QJsonObject& o, const QString& raw, QVector<AgentEvent>& out) {
    facts_.saw_result = true;
    const QString sub = o.value(QStringLiteral("subtype")).toString();
    const bool is_err =
        o.value(QStringLiteral("is_error")).toBool(false) || (!sub.isEmpty() && sub != QLatin1String("success"));

    const QJsonValue cost = o.value(QStringLiteral("total_cost_usd"));
    if (cost.isDouble())
        facts_.cost_usd = cost.toDouble();
    else if (o.value(QStringLiteral("cost_usd")).isDouble())
        facts_.cost_usd = o.value(QStringLiteral("cost_usd")).toDouble();

    if (o.value(QStringLiteral("num_turns")).isDouble())
        facts_.num_turns = o.value(QStringLiteral("num_turns")).toInt();

    const QJsonObject usage = o.value(QStringLiteral("usage")).toObject();
    if (!usage.isEmpty()) {
        facts_.input_tokens = json_int(usage, "input_tokens") + json_int(usage, "cache_creation_input_tokens") +
                              json_int(usage, "cache_read_input_tokens");
        facts_.output_tokens = json_int(usage, "output_tokens");
    }

    const QString result_text = o.value(QStringLiteral("result")).toString();
    if (!is_err) {
        facts_.result_is_error = false;
        facts_.final_text = result_text;
        if (!result_text.trimmed().isEmpty())
            out.append(make_event(EventKind::Final, QStringLiteral("Result"), result_text));
    } else {
        facts_.result_is_error = true;
        QString message = result_text;
        if (message.trimmed().isEmpty()) {
            QStringList errs;
            for (const auto& e : o.value(QStringLiteral("errors")).toArray())
                errs << compact(e);
            message = errs.join(QLatin1Char('\n'));
        }
        if (message.trimmed().isEmpty())
            message = QStringLiteral("Run ended: ") + claude_result_subtype_label(sub);
        facts_.result_error = message.left(4000);
        out.append(make_event(EventKind::Error, QStringLiteral("Run ended — ") + claude_result_subtype_label(sub), message,
                              true, {}, raw));
    }

    const QJsonArray denials = o.value(QStringLiteral("permission_denials")).toArray();
    if (!denials.isEmpty()) {
        QStringList names;
        for (const auto& d : denials) {
            const QString n = display_tool_name(d.toObject().value(QStringLiteral("tool_name")).toString());
            if (!n.isEmpty() && !names.contains(n))
                names << n;
        }
        out.append(make_event(
            EventKind::Status, QStringLiteral("Tool calls denied (%1)").arg(denials.size()),
            QStringLiteral("Denied by this run's permission policy: %1\nSwitch to Workspace edits mode if the agent needs to "
                           "write files.")
                .arg(names.join(QStringLiteral(", "))),
            true));
    }

    QStringList parts;
    if (facts_.cost_usd.has_value())
        parts << QStringLiteral("cost $%1").arg(*facts_.cost_usd, 0, 'f', 4);
    parts << QStringLiteral("input %1 tok").arg(group_digits(facts_.input_tokens));
    parts << QStringLiteral("output %1 tok").arg(group_digits(facts_.output_tokens));
    if (facts_.num_turns > 0)
        parts << QStringLiteral("%1 turns").arg(facts_.num_turns);
    const qint64 duration_ms = json_int(o, "duration_ms");
    if (duration_ms > 0)
        parts << QStringLiteral("%1 s").arg(static_cast<double>(duration_ms) / 1000.0, 0, 'f', 1);
    out.append(make_event(EventKind::Usage, QStringLiteral("Usage"), parts.join(QStringLiteral(" · ")), false, {},
                          object_summary(o, {QStringLiteral("result")}, kAgentEventRawMaxBytes)));
}

// ── Codex exec --json ────────────────────────────────────────────────────────

QVector<AgentEvent> CodexJsonParser::parse_line(const QString& line) {
    QVector<AgentEvent> out;
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return out;

    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        plain_line(trimmed, facts_, out);
        return out;
    }

    const QJsonObject o = doc.object();
    const QString type = o.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("thread.started")) {
        const QString tid = o.value(QStringLiteral("thread_id")).toString();
        if (!tid.isEmpty())
            facts_.session_id = tid;
        out.append(make_event(EventKind::Status, QStringLiteral("Thread started"),
                              tid.isEmpty() ? QString() : QStringLiteral("thread: ") + tid, false, {}, trimmed));
    } else if (type == QLatin1String("turn.started")) {
        out.append(make_event(EventKind::Status, QStringLiteral("Turn started")));
    } else if (type == QLatin1String("turn.completed")) {
        facts_.saw_result = true;
        ++facts_.num_turns;
        const QJsonObject usage = o.value(QStringLiteral("usage")).toObject();
        const qint64 in = json_int(usage, "input_tokens");
        const qint64 cached = json_int(usage, "cached_input_tokens");
        const qint64 outt = json_int(usage, "output_tokens");
        const qint64 reasoning = json_int(usage, "reasoning_output_tokens");
        facts_.input_tokens += in;
        facts_.output_tokens += outt;
        QStringList parts;
        parts << QStringLiteral("input %1 tok").arg(group_digits(in));
        if (cached > 0)
            parts << QStringLiteral("cached %1").arg(group_digits(cached));
        parts << QStringLiteral("output %1 tok").arg(group_digits(outt));
        if (reasoning > 0)
            parts << QStringLiteral("reasoning %1").arg(group_digits(reasoning));
        out.append(make_event(EventKind::Usage, QStringLiteral("Usage"), parts.join(QStringLiteral(" · ")), false, {},
                              trimmed));
    } else if (type == QLatin1String("turn.failed")) {
        facts_.saw_result = true;
        facts_.result_is_error = true;
        const QJsonValue err = o.value(QStringLiteral("error"));
        QString msg = err.isObject() ? err.toObject().value(QStringLiteral("message")).toString() : compact(err);
        if (msg.isEmpty())
            msg = QStringLiteral("The turn failed without an error message.");
        facts_.result_error = msg.left(4000);
        out.append(make_event(EventKind::Error, QStringLiteral("Turn failed"), msg, true, {}, trimmed));
    } else if (type == QLatin1String("error")) {
        QString msg = o.value(QStringLiteral("message")).toString();
        if (msg.isEmpty())
            msg = compact(o.value(QStringLiteral("error")));
        facts_.last_error = msg.left(2000);
        out.append(make_event(EventKind::Error, QStringLiteral("Error"), msg, true, {}, trimmed));
    } else if (type.startsWith(QLatin1String("item."))) {
        on_item(type.mid(5), o.value(QStringLiteral("item")).toObject(), out);
    } else {
        out.append(make_event(EventKind::Status, type.isEmpty() ? QStringLiteral("Event") : type,
                              object_summary(o, {QStringLiteral("type")}, 4096), false, {}, trimmed));
    }
    return out;
}

void CodexJsonParser::on_item(const QString& phase, const QJsonObject& item, QVector<AgentEvent>& out) {
    const QString id = item.value(QStringLiteral("id")).toString();
    QString it = item.value(QStringLiteral("type")).toString();
    if (it.isEmpty())
        it = item.value(QStringLiteral("item_type")).toString(); // older codex builds
    const bool started = phase == QLatin1String("started");
    const bool done = phase == QLatin1String("completed");

    if (it == QLatin1String("agent_message") || it == QLatin1String("assistant_message")) {
        if (!done)
            return; // the full text arrives with item.completed
        const QString text = item.value(QStringLiteral("text")).toString();
        if (!text.trimmed().isEmpty()) {
            facts_.final_text = text;
            out.append(make_event(EventKind::Text, QStringLiteral("Assistant"), text, false, id));
        }
        return;
    }

    if (it == QLatin1String("reasoning")) {
        if (!done)
            return;
        const QString text = item.value(QStringLiteral("text")).toString();
        if (!text.trimmed().isEmpty())
            out.append(make_event(EventKind::Thinking, QStringLiteral("Reasoning"), text, false, id));
        return;
    }

    if (it == QLatin1String("command_execution")) {
        const QJsonValue cmdv = item.value(QStringLiteral("command"));
        QString cmd;
        if (cmdv.isArray()) {
            QStringList parts;
            for (const auto& p : cmdv.toArray())
                parts << p.toString();
            cmd = parts.join(QLatin1Char(' '));
        } else {
            cmd = cmdv.toString();
        }
        cmd = cmd.simplified();
        if (started) {
            out.append(make_event(EventKind::Command, QStringLiteral("$ ") + cmd.left(200), QStringLiteral("(running)"),
                                  false, id));
        } else if (done) {
            const QJsonValue exitv = item.value(QStringLiteral("exit_code"));
            const QString status = item.value(QStringLiteral("status")).toString();
            const bool failed = status == QLatin1String("failed") || status == QLatin1String("declined") ||
                                (exitv.isDouble() && exitv.toInt() != 0);
            QString title = QStringLiteral("$ ") + cmd.left(160);
            if (exitv.isDouble())
                title += QStringLiteral("  → exit %1").arg(exitv.toInt());
            else if (!status.isEmpty())
                title += QStringLiteral("  → ") + status;
            QString output = item.value(QStringLiteral("aggregated_output")).toString();
            if (output.isEmpty())
                output = QStringLiteral("(no output)");
            out.append(make_event(EventKind::Command, title, output, failed, id));
        }
        return;
    }

    if (it == QLatin1String("mcp_tool_call")) {
        const QString server = item.value(QStringLiteral("server")).toString();
        const QString tool = item.value(QStringLiteral("tool")).toString();
        const QString name = server.isEmpty() ? tool : server + QLatin1Char('.') + tool;
        if (started) {
            ++facts_.tool_calls;
            const QJsonValue args = item.value(QStringLiteral("arguments"));
            out.append(make_event(EventKind::ToolCall, name, args.isString() ? args.toString() : compact(args), false,
                                  id));
        } else if (done) {
            const QJsonValue err = item.value(QStringLiteral("error"));
            const QString status = item.value(QStringLiteral("status")).toString();
            const bool failed = has_value(err) || status == QLatin1String("failed");
            QString body;
            if (has_value(err))
                body = err.isObject() ? err.toObject().value(QStringLiteral("message")).toString() : compact(err);
            if (body.isEmpty())
                body = content_text(item.value(QStringLiteral("result")));
            out.append(make_event(EventKind::ToolResult, name, body, failed, id));
        }
        return;
    }

    if (it == QLatin1String("file_change")) {
        if (!done)
            return;
        QStringList lines;
        for (const auto& cv : item.value(QStringLiteral("changes")).toArray()) {
            const QJsonObject c = cv.toObject();
            lines << QStringLiteral("%1  %2").arg(c.value(QStringLiteral("kind")).toString(QStringLiteral("update")),
                                                  c.value(QStringLiteral("path")).toString());
        }
        const bool failed = item.value(QStringLiteral("status")).toString() == QLatin1String("failed");
        out.append(make_event(EventKind::FileChange, QStringLiteral("Files changed (%1)").arg(lines.size()),
                              lines.join(QLatin1Char('\n')), failed, id));
        return;
    }

    if (it == QLatin1String("web_search")) {
        if (done)
            out.append(make_event(EventKind::ToolCall, QStringLiteral("web_search"),
                                  item.value(QStringLiteral("query")).toString(), false, id));
        return;
    }

    if (it == QLatin1String("todo_list")) {
        QStringList lines;
        for (const auto& tv : item.value(QStringLiteral("items")).toArray()) {
            const QJsonObject t = tv.toObject();
            lines << (t.value(QStringLiteral("completed")).toBool(false) ? QStringLiteral("[x] ") : QStringLiteral("[ ] ")) +
                         t.value(QStringLiteral("text")).toString();
        }
        const QString title = started ? QStringLiteral("Plan")
                                      : (done ? QStringLiteral("Plan complete") : QStringLiteral("Plan updated"));
        out.append(make_event(EventKind::Status, title, lines.join(QLatin1Char('\n')), false, id));
        return;
    }

    if (it == QLatin1String("error")) {
        const QString msg = item.value(QStringLiteral("message")).toString();
        facts_.last_error = msg.left(2000);
        out.append(make_event(EventKind::Error, QStringLiteral("Error"), msg, true, id));
        return;
    }

    if (done)
        out.append(make_event(EventKind::Status,
                              it.isEmpty() ? QStringLiteral("Item") : QStringLiteral("Item · ") + it,
                              object_summary(item, {QStringLiteral("id"), QStringLiteral("type")}, 2048), false, id));
}

} // namespace fincept::services::coding_agents
