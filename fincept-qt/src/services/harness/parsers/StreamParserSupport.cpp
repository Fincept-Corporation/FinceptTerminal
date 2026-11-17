// StreamParserSupport.cpp — helpers shared by the harness stream parsers (see
// StreamParserSupport.h). QtCore only.

#include "services/harness/parsers/StreamParserSupport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>

namespace fincept::harness::stream_support {

namespace sps_cad = fincept::services::coding_agents;

sps_cad::AgentEvent make_event(sps_cad::EventKind kind, const QString& title, const QString& body, bool is_error,
                           const QString& ref_id, const QString& raw) {
    sps_cad::AgentEvent ev;
    ev.kind = kind;
    ev.title = title.left(300).simplified();
    ev.body = sps_cad::clip_utf8(body, sps_cad::kAgentEventBodyMaxBytes);
    ev.raw_json = raw.isEmpty() ? QString() : sps_cad::clip_utf8(raw, sps_cad::kAgentEventRawMaxBytes);
    ev.is_error = is_error;
    ev.ref_id = ref_id;
    return ev;
}

QString compact_json(const QJsonValue& v) {
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

bool has_json_value(const QJsonValue& v) {
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

QString content_text(const QJsonValue& content) {
    if (content.isString())
        return content.toString();
    if (content.isArray()) {
        QStringList parts;
        for (const QJsonValue& v : content.toArray()) {
            const QString t = content_text(v);
            if (!t.isEmpty())
                parts << t;
        }
        return parts.join(QLatin1Char('\n'));
    }
    if (!content.isObject())
        return compact_json(content);
    const QJsonObject b = content.toObject();
    const QString type = b.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("text"))
        return b.value(QStringLiteral("text")).toString();
    if (type == QLatin1String("image"))
        return QStringLiteral("[image]");
    if (type == QLatin1String("audio"))
        return QStringLiteral("[audio]");
    if (type == QLatin1String("resource_link") || type == QLatin1String("resource")) {
        QString uri = b.value(QStringLiteral("uri")).toString();
        if (uri.isEmpty())
            uri = b.value(QStringLiteral("resource")).toObject().value(QStringLiteral("uri")).toString();
        return QStringLiteral("[resource] ") + uri;
    }
    if (type.isEmpty() && b.contains(QStringLiteral("content")))
        return content_text(b.value(QStringLiteral("content"))); // MCP CallToolResult
    return compact_json(content);
}

QString json_summary(const QJsonObject& o, const QStringList& skip_keys, int max_bytes) {
    QJsonObject copy = o;
    for (const QString& k : skip_keys)
        copy.remove(k);
    return sps_cad::clip_utf8(compact_json(copy), max_bytes);
}

QString grouped_number(qint64 v) {
    QString s = QString::number(v < 0 ? -v : v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(i, QLatin1Char(','));
    return v < 0 ? QStringLiteral("-") + s : s;
}

qint64 json_count(const QJsonObject& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const QJsonValue v = o.value(QLatin1String(k));
        if (v.isDouble())
            return v.toDouble() > 0 ? static_cast<qint64>(v.toDouble()) : 0;
    }
    return 0;
}

QString json_string(const QJsonObject& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const QString s = o.value(QLatin1String(k)).toString();
        if (!s.isEmpty())
            return s;
    }
    return {};
}

bool parse_json_object(const QString& trimmed, QJsonObject& out) {
    if (!trimmed.startsWith(QLatin1Char('{')))
        return false; // cheap reject for plain text (and huge non-JSON lines)
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    out = doc.object();
    return true;
}

bool looks_like_error(const QString& line) {
    static const QRegularExpression rx(
        QStringLiteral(R"(\b(error|failed|failure|fatal|exception|denied|unauthori[sz]ed|forbidden|invalid api key|not logged in|not authenticated|panic)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(line.left(4096)).hasMatch();
}

void plain_output_line(const QString& trimmed, sps_cad::StreamFacts& facts, QVector<sps_cad::AgentEvent>& out) {
    const bool err = looks_like_error(trimmed);
    if (err)
        facts.last_error = trimmed.left(2000);
    out.append(make_event(err ? sps_cad::EventKind::Error : sps_cad::EventKind::Status,
                          err ? QStringLiteral("CLI error output") : QStringLiteral("CLI output"), trimmed, err));
}

// ── DeltaAccumulator ─────────────────────────────────────────────────────────

void DeltaAccumulator::start(sps_cad::EventKind kind, const QString& key, const QString& title, const QString& message_id) {
    clear();
    active_ = true;
    kind_ = kind;
    key_ = key;
    title_ = title;
    message_id_ = message_id;
}

std::optional<sps_cad::AgentEvent> DeltaAccumulator::append(const QString& delta) {
    if (!active_ || delta.isEmpty())
        return std::nullopt;
    if (text_.size() + delta.size() > kMaxMessageChars) {
        if (!truncated_) {
            text_.append(delta.left(qMax<qsizetype>(0, kMaxMessageChars - text_.size())));
            truncated_ = true;
        }
        return std::nullopt;
    }
    text_.append(delta);
    if (partials_stopped_ || text_.trimmed().isEmpty())
        return std::nullopt;
    // One event body holds at most kAgentEventBodyMaxBytes; past that the live
    // view keeps its last (clipped) partial until the complete event arrives.
    if (text_.size() > sps_cad::kAgentEventBodyMaxBytes)
        partials_stopped_ = true;
    sps_cad::AgentEvent ev = make_event(kind_, title_, partials_stopped_ ? text_.left(sps_cad::kAgentEventBodyMaxBytes) : text_,
                                    false, key_);
    ev.partial = true;
    return ev;
}

std::optional<sps_cad::AgentEvent> DeltaAccumulator::finish_message(QString* full_text) {
    if (!active_) {
        if (full_text)
            full_text->clear();
        return std::nullopt;
    }
    QString text = text_;
    if (truncated_)
        text += QStringLiteral("\n… [message truncated]");
    const bool empty = text_.trimmed().isEmpty();
    const sps_cad::EventKind kind = kind_;
    const QString title = title_;
    const QString key = key_;
    clear();
    if (full_text)
        *full_text = empty ? QString() : text;
    if (empty)
        return std::nullopt;
    return make_event(kind, title, text, false, key);
}

void DeltaAccumulator::clear() {
    active_ = false;
    partials_stopped_ = false;
    truncated_ = false;
    kind_ = sps_cad::EventKind::Text;
    key_.clear();
    title_.clear();
    message_id_.clear();
    text_.clear();
}

} // namespace fincept::harness::stream_support
