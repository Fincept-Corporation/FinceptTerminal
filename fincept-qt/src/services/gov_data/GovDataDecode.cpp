// src/services/gov_data/GovDataDecode.cpp
// Pure: QtCore + python::extract_json only (see GovDataDecode.h). Runs on GovDataService's decode
// thread; nothing here may log, touch the cache, the hub, the runner or a widget.
#include "services/gov_data/GovDataDecode.h"

#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

namespace fincept::services::gov_decode {

namespace {

/// A connector script signals failure as {"success": false, "error": "..."} or a bare {"error": "..."};
/// a good payload may still carry "error": null. Returns the failure message, or an empty string for a
/// normal result. (The panels only ever looked at result.success, which was true for every script that
/// exits 0 — so a rejected request was rendered as an empty table instead of an error.)
QString gov_dec_script_error(const QJsonObject& obj) {
    const QJsonValue err = obj.value(QStringLiteral("error"));
    QString msg;
    if (err.isString())
        msg = err.toString();
    else if (err.isObject())
        msg = err.toObject().value(QStringLiteral("message")).toString();

    const QJsonValue ok = obj.value(QStringLiteral("success"));
    const bool explicit_success = ok.isBool() && ok.toBool();
    const bool explicit_failure = ok.isBool() && !ok.toBool();

    if (explicit_failure) {
        if (msg.isEmpty())
            msg = obj.value(QStringLiteral("message")).toString();
        return msg.isEmpty() ? QStringLiteral("Request failed") : msg;
    }
    return explicit_success ? QString() : msg;
}

/// Why a request failed, as far as the script's JSON and the message tell (GovDataResult::error_code).
struct GovDecFailure {
    QString code;
    QString key;
    int retry_after_s = 0;
};

/// The closed set of codes the shared connector wrapper writes (fincept_connector.ERROR_CODES:
/// missing_key, auth_failed, rate_limited, upstream_down, bad_args, no_data, not_supported, unknown)
/// and the service's own names, mapped to GovDataResult::error_code. Empty = not one of them.
QString gov_dec_known_code(const QString& lower) {
    if (lower == QLatin1String("missing_key"))
        return QStringLiteral("missing_key");
    // A key is saved but the source refused it ("auth_failed" from fincept_connector).
    if (lower == QLatin1String("auth_failed") || lower == QLatin1String("invalid_key") ||
        lower == QLatin1String("unauthorized") || lower == QLatin1String("unauthorised"))
        return QStringLiteral("invalid_key");
    if (lower == QLatin1String("rate_limited"))
        return QStringLiteral("rate_limited");
    if (lower == QLatin1String("upstream_down") || lower == QLatin1String("unreachable"))
        return QStringLiteral("unreachable");
    if (lower == QLatin1String("timeout") || lower == QLatin1String("timed_out"))
        return QStringLiteral("timeout");
    if (lower == QLatin1String("no_data"))
        return QStringLiteral("no_data");
    if (lower == QLatin1String("not_supported"))
        return QStringLiteral("not_supported");
    if (lower == QLatin1String("bad_args"))
        return QStringLiteral("bad_args");
    return {};
}

QString gov_dec_code_from_text(const QString& raw) {
    const QString c = raw.trimmed().toLower();
    if (c.isEmpty() || c == QLatin1String("unknown"))
        return {};
    const QString known = gov_dec_known_code(c);
    if (!known.isEmpty())
        return known;
    // Other spellings: not_supported (HTTP 402 / not offered), bad_args (HTTP 400, "Provide …"),
    // upstream_down (non-JSON / DNS), no_data ("No … found").
    if (c.contains(QLatin1String("not_supported")) || c.contains(QLatin1String("unsupported")) || c == QLatin1String("402"))
        return QStringLiteral("not_supported");
    if (c.contains(QLatin1String("bad_arg")) || c.contains(QLatin1String("bad_request")) || c == QLatin1String("400"))
        return QStringLiteral("bad_args");
    if (c.contains(QLatin1String("invalid")) || c.contains(QLatin1String("auth")) || c == QLatin1String("401"))
        return QStringLiteral("invalid_key");
    if (c.contains(QLatin1String("key")) || c.contains(QLatin1String("credential")))
        return QStringLiteral("missing_key");
    if (c.contains(QLatin1String("rate")) || c.contains(QLatin1String("429")) || c.contains(QLatin1String("throttl")))
        return QStringLiteral("rate_limited");
    if (c.contains(QLatin1String("timeout")) || c.contains(QLatin1String("timed_out")))
        return QStringLiteral("timeout");
    if (c.contains(QLatin1String("network")) || c.contains(QLatin1String("connection")) ||
        c.contains(QLatin1String("unreachable")) || c.contains(QLatin1String("upstream")))
        return QStringLiteral("unreachable");
    if (c.contains(QLatin1String("no_data")) || c.contains(QLatin1String("empty")) || c.contains(QLatin1String("not_found")))
        return QStringLiteral("no_data");
    return {};
}

/// The credential a missing / rejected-key failure is about: the script's "env_key" (or its only
/// "env_keys" entry), else the first FOO_API_KEY-style name in the message.
QString gov_dec_key_name(const QJsonObject& obj, const QString& text) {
    const QString env = obj.value(QStringLiteral("env_key")).toString().trimmed();
    if (!env.isEmpty())
        return env;
    const QJsonArray keys = obj.value(QStringLiteral("env_keys")).toArray();
    if (keys.size() == 1 && keys.first().isString() && !keys.first().toString().trimmed().isEmpty())
        return keys.first().toString().trimmed();
    static const QRegularExpression kKeyName(QStringLiteral("\\b([A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*_(?:KEY|TOKEN))\\b"));
    const QRegularExpressionMatch m = kKeyName.match(text);
    return m.hasMatch() ? m.captured(1) : QString();
}

GovDecFailure gov_dec_classify(const QJsonObject& obj, const QString& message, const QString& runner_error) {
    GovDecFailure f;
    if (runner_error == QLatin1String("Cancelled")) {
        f.code = QStringLiteral("cancelled");
        return f;
    }
    // 1. An explicit code from the script ("error_code" / "code"; a number is an HTTP status).
    for (const char* name : {"error_code", "code"}) {
        const QJsonValue v = obj.value(QLatin1String(name));
        if (v.isString())
            f.code = gov_dec_code_from_text(v.toString());
        else if (v.isDouble() && v.toInt() == 429)
            f.code = QStringLiteral("rate_limited");
        if (!f.code.isEmpty())
            break;
    }
    // 2. The flags the gov-data scripts set.
    if (f.code.isEmpty()) {
        if (obj.value(QStringLiteral("api_key_required")).toBool(false))
            f.code = QStringLiteral("missing_key");
        else if (obj.value(QStringLiteral("auth_error")).toBool(false) || obj.value(QStringLiteral("status_code")).toInt(0) == 401)
            f.code = QStringLiteral("invalid_key");
        else if (obj.value(QStringLiteral("rate_limit_error")).toBool(false) ||
                 obj.value(QStringLiteral("status_code")).toInt(0) == 429)
            f.code = QStringLiteral("rate_limited");
        else if (obj.value(QStringLiteral("timeout")).toBool(false))
            f.code = QStringLiteral("timeout");
        else if (obj.value(QStringLiteral("connection_error")).toBool(false))
            f.code = QStringLiteral("unreachable");
    }
    // 3. The message itself. Scripts that wrap their failure in their own error object lose the
    // code on the way out (congress_gov_data.py's CongressGovError keeps only "error" and
    // "status_code"), so the shared connector's wording is recognised too: "<Service> needs an API
    // key: … save it as FOO_KEY …", "<Service> rejected the API key (HTTP 401) …", "<Service>
    // refused the request (HTTP 403): the key saved as FOO_KEY … is invalid or its plan does not
    // include this data".
    static const QRegularExpression kMissingKey(
        QStringLiteral("(api[ _-]?key|credential)[^.]{0,40}(not configured|missing|required|not set|not found)|"
                       "(no|missing|requires? an?)\\s+[^.]{0,30}api[ _-]?key|"
                       "needs (an?|your|the) [^.]{0,40}?(api[ _-]?key|key|token)|"
                       "save it as [A-Z][A-Z0-9_]*_(KEY|TOKEN)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kRate(QStringLiteral("rate.?limit|too many requests|\\b429\\b"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kTimeout(QStringLiteral("timed out|timeout"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kUnreachable(
        QStringLiteral("connection error|connection refused|connection reset|failed to establish|name resolution|"
                       "max retries exceeded|network is unreachable|getaddrinfo|ssl error"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kNoData(
        QStringLiteral("no (price )?data available|no data (found|for)|no results|\\bno [\\w ]{1,30} found"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kInvalidKey(
        QStringLiteral("invalid api[ _-]?key|api[ _-]?key (is )?(invalid|rejected|not valid)|unauthori[sz]ed|\\b401\\b|"
                       "rejected the (api )?key|credentials rejected|authentication failed|plan does not include|"
                       "(api[ _-]?key|key saved as)[^.]{0,80}?\\b(is invalid|is not valid|was rejected|has expired|is expired)"),
        QRegularExpression::CaseInsensitiveOption);
    const QString text = message.isEmpty() ? runner_error : message;
    if (f.code.isEmpty()) {
        if (kInvalidKey.match(text).hasMatch())
            f.code = QStringLiteral("invalid_key");
        else if (kMissingKey.match(text).hasMatch())
            f.code = QStringLiteral("missing_key");
        else if (kRate.match(text).hasMatch())
            f.code = QStringLiteral("rate_limited");
        else if (kTimeout.match(text).hasMatch())
            f.code = QStringLiteral("timeout");
        else if (kUnreachable.match(text).hasMatch())
            f.code = QStringLiteral("unreachable");
        else if (kNoData.match(text).hasMatch())
            f.code = QStringLiteral("no_data");
    }
    if (f.code == QLatin1String("missing_key") || f.code == QLatin1String("invalid_key"))
        f.key = gov_dec_key_name(obj, text);
    if (f.code == QLatin1String("rate_limited")) {
        for (const char* name : {"retry_after", "retry_after_s"}) {
            const QJsonValue v = obj.value(QLatin1String(name));
            const int s = v.isDouble() ? v.toInt() : v.toString().toInt();
            if (s > 0) {
                f.retry_after_s = s;
                break;
            }
        }
    }
    return f;
}

} // namespace

QString redact_secrets(QString text) {
    static const QRegularExpression kQueryKey(
        QStringLiteral("\\b(api[_-]?key|apikey|access[_-]?token|token|key|subscription-key|password|secret)=[^&\\s\"']+"),
        QRegularExpression::CaseInsensitiveOption);
    text.replace(kQueryKey, QStringLiteral("\\1=***"));
    return text;
}

GovDataResult cancelled_result() {
    GovDataResult r;
    r.success = false;
    r.error = QStringLiteral("Cancelled");
    r.error_code = QStringLiteral("cancelled");
    return r;
}

GovDataResult decode_reply(const python::PythonResult& py) {
    GovDataResult result;

    // PythonRunner already handed over the extracted JSON text, so it normally parses as it is;
    // only when it does not (a non-JSON stdout handed over whole) is the last JSON value looked for.
    // Also on a non-zero exit: scripts print {"success": false, "error": "..."} before exiting 1,
    // which is far more useful than "exited with code 1".
    QString json_str;
    QJsonDocument doc;
    QJsonParseError parse_err{};
    if (!py.output.isEmpty()) {
        doc = QJsonDocument::fromJson(py.output.toUtf8(), &parse_err);
        if (parse_err.error == QJsonParseError::NoError && !doc.isNull()) {
            json_str = py.output;
        } else {
            json_str = python::extract_json(py.output);
            doc = json_str.isEmpty() ? QJsonDocument() : QJsonDocument::fromJson(json_str.toUtf8(), &parse_err);
        }
    }

    const auto fail = [&](const QString& message) {
        result.success = false;
        result.error = redact_secrets(message);
        const GovDecFailure why = gov_dec_classify(doc.isObject() ? doc.object() : QJsonObject(), result.error, py.error);
        result.error_code = why.code;
        result.missing_key = why.key;
        result.retry_after_s = why.retry_after_s;
        result.data = QJsonObject();
        return result;
    };

    if (!py.success) {
        QString msg;
        if (doc.isObject())
            msg = gov_dec_script_error(doc.object());
        if (msg.isEmpty())
            msg = py.error.isEmpty() ? QStringLiteral("Script exited with code %1").arg(py.exit_code) : py.error;
        return fail(msg);
    }
    if (json_str.isEmpty())
        return fail(QStringLiteral("No JSON output from script"));
    if (doc.isNull())
        return fail(QStringLiteral("JSON parse error: %1").arg(parse_err.errorString()));

    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QString script_error = gov_dec_script_error(obj);
        if (!script_error.isEmpty())
            return fail(script_error);
        result.data = obj;
    } else {
        // A bare JSON array: expose it under "data" (the key the panels read) instead of
        // collapsing it to an empty object.
        result.data = QJsonObject{{QStringLiteral("success"), true}, {QStringLiteral("data"), doc.array()}};
    }
    result.success = true;
    return result;
}

GovDataResult decode_cached(const QString& cached_text) {
    GovDataResult result;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(cached_text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        result.success = false;
        result.error = QStringLiteral("Cached answer unreadable");
        return result;
    }
    result.success = true;
    result.data = doc.object();
    return result;
}

QString cache_text(const GovDataResult& ok_result) {
    return QString::fromUtf8(QJsonDocument(ok_result.data).toJson(QJsonDocument::Compact));
}

} // namespace fincept::services::gov_decode
