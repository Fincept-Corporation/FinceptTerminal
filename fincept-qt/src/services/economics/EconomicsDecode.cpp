// src/services/economics/EconomicsDecode.cpp
#include "services/economics/EconomicsDecode.h"

#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>

namespace fincept::services::econ_decode {

namespace {

/// The failure code of a script answer: "error_code" (older scripts), else "code" (the connector
/// contract: missing_key | auth_failed | rate_limited | upstream_down | bad_args | no_data |
/// not_supported).
QString econ_dec_code(const QJsonObject& o) {
    const QString code = o.value(QStringLiteral("error_code")).toString();
    return code.isEmpty() ? o.value(QStringLiteral("code")).toString() : code;
}

QString econ_dec_prefixed(const QString& code, const QString& message) {
    if (code.isEmpty() || message.startsWith(QLatin1Char('[')))
        return message;
    return QStringLiteral("[") + code + QStringLiteral("] ") + message;
}

} // namespace

QString redact_secrets(QString text) {
    // Query-string keys ("...&api_key=SECRET&...") and BLS-style "The key:SECRET provided by the
    // User is invalid".
    static const QRegularExpression kQueryKey(
        QStringLiteral("\\b(api[_-]?key|apikey|access[_-]?token|token|key|userid|subscription-key|password|secret)=[^&\\s\"']+"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kBlsKey(QStringLiteral("(the key:)\\S+"), QRegularExpression::CaseInsensitiveOption);
    text.replace(kQueryKey, QStringLiteral("\\1=***"));
    text.replace(kBlsKey, QStringLiteral("\\1***"));
    return text;
}

DecodedReply decode_reply(const python::PythonResult& py, const QString& source_id, const QDateTime& fetched_at) {
    DecodedReply out;
    EconomicsResult& res = out.result;
    res.source_id = source_id;
    res.fetched_at = fetched_at;

    if (!py.success) {
        QString message = py.error.isEmpty() ? QStringLiteral("Script exited with code %1").arg(py.exit_code) : py.error;
        // PythonRunner fails an {"error": ..., "error_code" | "code": ...} answer before it reaches the
        // parsing below and keeps only the message. Recover the code (and the credential it names)
        // from the script's own output so panels can branch on the "[CODE] " prefix (FredPanel:
        // MISSING_API_KEY, RATE_LIMITED, …; the connector contract: missing_key, not_supported, …).
        const QString envelope_json = python::extract_json(py.output);
        if (!envelope_json.isEmpty()) {
            const QJsonObject envelope = QJsonDocument::fromJson(envelope_json.toUtf8()).object();
            res.error_code = econ_dec_code(envelope);
            res.env_key = envelope.value(QStringLiteral("env_key")).toString();
            message = econ_dec_prefixed(res.error_code, message);
        }
        res.error = redact_secrets(message);
        out.log_message = QStringLiteral("Script failed: ") + res.error;
        return out;
    }

    QJsonParseError err{};
    QJsonDocument doc;
    {
        const QString json_str = python::extract_json(py.output);
        if (json_str.isEmpty()) {
            res.error = QStringLiteral("No JSON output from script");
            out.log_message = res.error;
            return out;
        }
        doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
    }
    if (doc.isNull()) {
        res.error = QStringLiteral("JSON parse error: ") + err.errorString();
        out.log_message = res.error;
        return out;
    }

    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        // A batch answer (fred_data.py "multiple") whose EVERY element is an error object (no key
        // configured, invalid key, rate limited, offline) is a failure, not data. Caching it would
        // replay the same errors for the cache lifetime even after the user fixed the cause.
        bool all_errors = !arr.isEmpty();
        for (const QJsonValue v : arr) {
            if (!v.isObject() || v.toObject().value(QStringLiteral("error")).toString().isEmpty()) {
                all_errors = false;
                break;
            }
        }
        if (all_errors) {
            const QJsonObject first = arr.first().toObject();
            res.error_code = econ_dec_code(first);
            res.env_key = first.value(QStringLiteral("env_key")).toString();
            res.error = econ_dec_prefixed(res.error_code,
                                          redact_secrets(first.value(QStringLiteral("error")).toString()));
            out.log_message = QStringLiteral("Every element of the batch failed: ") + res.error;
            out.log_warning = true;
            return out;
        }
        res.data = QJsonObject{{QStringLiteral("data"), arr}};
    } else {
        res.data = doc.object();
    }
    doc = QJsonDocument(); // the parsed tree is shared by res.data now

    // Scripts echo provider / HTTP-library errors verbatim — scrub credentials before the text is
    // shown, logged or cached.
    for (const char* field : {"error", "message"}) {
        const QJsonValue v = res.data.value(QLatin1String(field));
        if (v.isString())
            res.data[QLatin1String(field)] = redact_secrets(v.toString());
    }

    // A script-level error field (without data) is a failure; "[CODE] " lets panels branch on it.
    const QJsonValue script_error = res.data.value(QStringLiteral("error"));
    if (!script_error.isNull() && !script_error.isUndefined() && !script_error.toString().isEmpty() &&
        !res.data.contains(QStringLiteral("data"))) {
        res.error_code = econ_dec_code(res.data);
        res.env_key = res.data.value(QStringLiteral("env_key")).toString();
        res.error = econ_dec_prefixed(res.error_code, script_error.toString());
        return out; // not logged: the source's own answer, shown to the user
    }

    res.success = true;
    return out;
}

EconomicsResult decode_cached(const QString& text, const QString& source_id) {
    EconomicsResult res;
    res.source_id = source_id;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isObject())
        return res; // unreadable: a miss
    const QJsonObject wrapper = doc.object();
    res.data = wrapper.value(QStringLiteral("data")).toObject();
    res.fetched_at = QDateTime::fromString(wrapper.value(QStringLiteral("fetched_at")).toString(), Qt::ISODate);
    res.from_cache = true;
    res.success = true;
    return res;
}

QString cache_text(const EconomicsResult& result) {
    const QJsonObject wrapper{{QStringLiteral("fetched_at"), result.fetched_at.toString(Qt::ISODate)},
                              {QStringLiteral("data"), result.data}};
    return QString::fromUtf8(QJsonDocument(wrapper).toJson(QJsonDocument::Compact));
}

} // namespace fincept::services::econ_decode
