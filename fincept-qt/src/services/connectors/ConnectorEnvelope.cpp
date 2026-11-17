// ConnectorEnvelope.cpp — see ConnectorEnvelope.h.

#include "services/connectors/ConnectorEnvelope.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>

namespace fincept::services::connectors {

namespace ds = fincept::services::datasources;

namespace {

QString env_tr(const char* text) {
    return QCoreApplication::translate("ConnectorEnvelope", text);
}

/// Last line of `text` that parses as a JSON object (whole text first — a pretty-printed reply).
bool env_find_object(const QByteArray& text, QJsonObject* out) {
    QJsonParseError pe{};
    const QJsonDocument whole = QJsonDocument::fromJson(text.trimmed(), &pe);
    if (pe.error == QJsonParseError::NoError && whole.isObject()) {
        *out = whole.object();
        return true;
    }
    const QList<QByteArray> lines = text.split('\n');
    for (int i = int(lines.size()) - 1; i >= 0; --i) {
        const QByteArray line = lines[i].trimmed();
        if (!line.startsWith('{'))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
        if (pe.error == QJsonParseError::NoError && doc.isObject()) {
            *out = doc.object();
            return true;
        }
    }
    return false;
}

/// Project a value without columns (old payloads, single records) onto tables.
void env_project_value(const QJsonValue& v, ds::TabularResult& r) {
    if (v.isArray()) {
        ds::DataTable t = ds::table_from_json_array(v.toArray());
        if (!t.is_empty())
            r.tables << t;
        return;
    }
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        // A nested list of records under a common key is the table; else key/value pairs.
        for (const char* key : {"data", "results", "result", "rows", "items", "records", "observations", "values"}) {
            const QJsonValue inner = o.value(QLatin1String(key));
            if (inner.isArray() && !inner.toArray().isEmpty()) {
                ds::DataTable t = ds::table_from_json_array(inner.toArray(), QString());
                if (!t.is_empty()) {
                    r.tables << t;
                    return;
                }
            }
        }
        ds::DataTable t = ds::table_from_key_values(o);
        if (!t.is_empty())
            r.tables << t;
        return;
    }
    if (!v.isNull() && !v.isUndefined()) {
        ds::DataTable t;
        t.columns = {QStringLiteral("value")};
        t.types = {ds::cell_type(v)};
        t.rows.append(QJsonArray{v});
        r.tables << t;
    }
}

} // namespace

EnvelopeResult parse_connector_envelope(const QByteArray& text) {
    QJsonObject obj;
    if (!env_find_object(text, &obj)) {
        EnvelopeResult r;
        r.bytes = text.size();
        const QByteArray tail = text.trimmed().right(200);
        r.error = ds::SourceError::make(
            ds::ErrorCode::Unknown,
            tail.isEmpty() ? env_tr("The connector printed no result")
                           : env_tr("The connector printed no JSON result: %1").arg(QString::fromUtf8(tail)));
        return r;
    }
    EnvelopeResult r = parse_connector_envelope_object(obj);
    r.bytes = text.size();
    return r;
}

EnvelopeResult parse_connector_envelope_object(const QJsonObject& envelope) {
    EnvelopeResult r;
    const bool has_ok = envelope.contains(QStringLiteral("ok"));
    const bool ok = has_ok ? envelope.value(QStringLiteral("ok")).toBool()
                           : !(envelope.value(QStringLiteral("error")).isString() &&
                               !envelope.value(QStringLiteral("error")).toString().trimmed().isEmpty());
    if (!ok) {
        r.error = ds::SourceError::from_json(envelope);
        if (r.error.code == ds::ErrorCode::None)
            r.error.code = ds::ErrorCode::Unknown;
        if (r.error.message.trimmed().isEmpty())
            r.error.message = env_tr("The connector reported a failure without a message");
        return r;
    }

    const QJsonObject meta = envelope.value(QStringLiteral("meta")).toObject();
    const QJsonObject extra = meta.value(QStringLiteral("extra")).toObject();
    r.shape = extra.value(QStringLiteral("shape")).toString();
    // A bare (pre-envelope) payload is the data itself.
    r.payload = has_ok ? envelope.value(QStringLiteral("data")) : QJsonValue(envelope);
    r.data.source = meta.value(QStringLiteral("source")).toString();
    r.data.as_of = QDateTime::fromString(meta.value(QStringLiteral("as_of")).toString(), Qt::ISODate);
    r.data.truncated = meta.value(QStringLiteral("truncated")).toBool();
    const QJsonValue next = meta.value(QStringLiteral("next"));
    if (next.isString())
        r.data.next = next.toString();
    else if (next.isDouble())
        r.data.next = QString::number(next.toDouble(), 'g', 15);
    r.data.meta = meta;

    const QJsonValue tables = envelope.value(QStringLiteral("tables"));
    if (tables.isObject()) {
        const QJsonObject t = tables.toObject();
        for (auto it = t.constBegin(); it != t.constEnd(); ++it) {
            const QJsonObject spec = it.value().toObject();
            QStringList cols;
            for (const QJsonValue& c : spec.value(QStringLiteral("columns")).toArray())
                cols << c.toString();
            ds::DataTable table = ds::table_from_records(spec.value(QStringLiteral("rows")).toArray(), cols,
                                                         spec.value(QStringLiteral("types")).toObject(), it.key());
            if (!table.columns.isEmpty())
                r.data.tables << table;
        }
    } else {
        QStringList cols;
        for (const QJsonValue& c : envelope.value(QStringLiteral("columns")).toArray())
            cols << c.toString();
        if (!cols.isEmpty() && r.payload.isArray()) {
            r.data.tables << ds::table_from_records(r.payload.toArray(), cols,
                                                    envelope.value(QStringLiteral("types")).toObject(), QString());
        } else {
            env_project_value(r.payload, r.data);
        }
    }

    if (r.data.is_empty()) {
        r.error = ds::SourceError::make(ds::ErrorCode::NoData, env_tr("The connector returned no rows for this request"));
        return r;
    }
    r.ok = true;
    return r;
}

} // namespace fincept::services::connectors
