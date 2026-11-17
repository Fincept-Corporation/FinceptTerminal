// SourceError.cpp — see SourceError.h.

#include "services/data_sources/SourceError.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>

namespace fincept::services::datasources {

namespace {

QString se_tr(const char* text) {
    return QCoreApplication::translate("SourceError", text);
}

} // namespace

QString error_code_name(ErrorCode code) {
    switch (code) {
        case ErrorCode::None:
            return QString();
        case ErrorCode::MissingKey:
            return QStringLiteral("missing_key");
        case ErrorCode::AuthFailed:
            return QStringLiteral("auth_failed");
        case ErrorCode::RateLimited:
            return QStringLiteral("rate_limited");
        case ErrorCode::UpstreamDown:
            return QStringLiteral("upstream_down");
        case ErrorCode::BadArgs:
            return QStringLiteral("bad_args");
        case ErrorCode::NoData:
            return QStringLiteral("no_data");
        case ErrorCode::NotSupported:
            return QStringLiteral("not_supported");
        case ErrorCode::Unknown:
            return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

ErrorCode error_code_from_name(const QString& name) {
    const QString n = name.trimmed().toLower();
    if (n.isEmpty())
        return ErrorCode::None;
    if (n == QLatin1String("missing_key"))
        return ErrorCode::MissingKey;
    if (n == QLatin1String("auth_failed"))
        return ErrorCode::AuthFailed;
    if (n == QLatin1String("rate_limited"))
        return ErrorCode::RateLimited;
    if (n == QLatin1String("upstream_down"))
        return ErrorCode::UpstreamDown;
    if (n == QLatin1String("bad_args"))
        return ErrorCode::BadArgs;
    if (n == QLatin1String("no_data"))
        return ErrorCode::NoData;
    if (n == QLatin1String("not_supported"))
        return ErrorCode::NotSupported;
    return ErrorCode::Unknown;
}

SourceError SourceError::make(ErrorCode code, const QString& message, const QString& field) {
    SourceError e;
    e.code = code;
    e.message = message;
    e.field = field;
    return e;
}

SourceError SourceError::cancel() {
    SourceError e;
    e.code = ErrorCode::Unknown;
    e.cancelled = true;
    e.message = se_tr("Cancelled");
    return e;
}

QString SourceError::title() const {
    if (cancelled)
        return se_tr("Cancelled");
    switch (code) {
        case ErrorCode::None:
            return QString();
        case ErrorCode::MissingKey:
            return env_keys.isEmpty() ? se_tr("Needs an API key")
                                      : se_tr("Needs %1").arg(env_keys.join(QStringLiteral(" / ")));
        case ErrorCode::AuthFailed:
            return se_tr("Credentials rejected");
        case ErrorCode::RateLimited:
            return se_tr("Rate limited");
        case ErrorCode::UpstreamDown:
            return se_tr("Source not reachable");
        case ErrorCode::BadArgs:
            return se_tr("Check the inputs");
        case ErrorCode::NoData:
            return se_tr("No data for this request");
        case ErrorCode::NotSupported:
            return se_tr("Not available");
        case ErrorCode::Unknown:
            return se_tr("Could not load");
    }
    return se_tr("Could not load");
}

QJsonObject SourceError::to_json() const {
    QJsonObject o{{"code", error_code_name(code)}, {"error", message}};
    if (retry_after_s >= 0)
        o.insert(QStringLiteral("retry_after_s"), retry_after_s);
    if (env_keys.size() == 1)
        o.insert(QStringLiteral("env_key"), env_keys.first());
    else if (!env_keys.isEmpty())
        o.insert(QStringLiteral("env_keys"), QJsonArray::fromStringList(env_keys));
    if (!docs_url.isEmpty())
        o.insert(QStringLiteral("docs_url"), docs_url);
    if (!field.isEmpty())
        o.insert(QStringLiteral("field"), field);
    if (!fix.isEmpty())
        o.insert(QStringLiteral("fix"), fix);
    if (cancelled)
        o.insert(QStringLiteral("cancelled"), true);
    return o;
}

SourceError SourceError::from_json(const QJsonObject& obj) {
    SourceError e;
    e.code = error_code_from_name(obj.value(QStringLiteral("code")).toString());
    e.message = obj.value(QStringLiteral("error")).toString();
    if (e.code == ErrorCode::None && !e.message.isEmpty())
        e.code = ErrorCode::Unknown;
    const QJsonValue retry = obj.value(QStringLiteral("retry_after_s"));
    if (retry.isDouble() && retry.toDouble() >= 0)
        e.retry_after_s = int(retry.toDouble());
    if (e.code == ErrorCode::RateLimited && e.retry_after_s < 0)
        e.retry_after_s = 60;
    const QString one = obj.value(QStringLiteral("env_key")).toString();
    for (const QJsonValue& k : obj.value(QStringLiteral("env_keys")).toArray())
        if (!k.toString().isEmpty() && !e.env_keys.contains(k.toString()))
            e.env_keys << k.toString();
    if (!one.isEmpty() && !e.env_keys.contains(one))
        e.env_keys.prepend(one);
    e.docs_url = obj.value(QStringLiteral("docs_url")).toString();
    e.field = obj.value(QStringLiteral("field")).toString();
    e.fix = obj.value(QStringLiteral("fix")).toString();
    e.cancelled = obj.value(QStringLiteral("cancelled")).toBool();
    return e;
}

SourceError error_from_http_status(int status, const QString& what, const QString& retry_after,
                                   const QString& server_message) {
    SourceError e;
    const QString detail = server_message.trimmed().left(200);
    const QString suffix = detail.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(detail);
    if (status == 401 || status == 403) {
        e.code = ErrorCode::AuthFailed;
        e.message = status == 401 ? se_tr("%1 rejected the credentials (HTTP 401)%2").arg(what, suffix)
                                  : se_tr("%1 refused access with these credentials (HTTP 403)%2").arg(what, suffix);
    } else if (status == 429) {
        e.code = ErrorCode::RateLimited;
        bool ok = false;
        const int secs = retry_after.trimmed().toInt(&ok);
        e.retry_after_s = ok && secs >= 0 ? secs : 60;
        e.message = se_tr("%1 is rate limiting requests (HTTP 429)%2").arg(what, suffix);
    } else if (status == 404 || status == 410) {
        e.code = ErrorCode::BadArgs;
        e.message = se_tr("%1 has no such endpoint or resource (HTTP %2)%3").arg(what).arg(status).arg(suffix);
    } else if (status == 400 || status == 405 || status == 406 || status == 415 || status == 422) {
        e.code = ErrorCode::BadArgs;
        e.message = se_tr("%1 rejected the request (HTTP %2)%3").arg(what).arg(status).arg(suffix);
    } else if (status == 204) {
        e.code = ErrorCode::NoData;
        e.message = se_tr("%1 answered with no content (HTTP 204)").arg(what);
    } else if (status >= 500) {
        e.code = ErrorCode::UpstreamDown;
        e.message = se_tr("%1 failed on its side (HTTP %2)%3").arg(what).arg(status).arg(suffix);
    } else {
        e.code = ErrorCode::Unknown;
        e.message = se_tr("%1 answered HTTP %2%3").arg(what).arg(status).arg(suffix);
    }
    return e;
}

QString redact_secrets(QString text, const QStringList& secrets) {
    for (const QString& s : secrets) {
        if (s.size() >= 4)
            text.replace(s, QStringLiteral("***"));
    }
    static const QRegularExpression kQueryKey(
        QStringLiteral("\\b(api[_-]?key|apikey|api[_-]?token|access[_-]?key|access[_-]?token|auth[_-]?token|token|"
                       "key|secret|client[_-]?secret|password|passwd|signature|sig|app[_-]?id|appid)=([^&#\\s\"'<>]+)"),
        QRegularExpression::CaseInsensitiveOption);
    text.replace(kQueryKey, QStringLiteral("\\1=***"));
    static const QRegularExpression kUserInfo(QStringLiteral("([a-z][a-z0-9+.-]*://)[^/@\\s:]+:[^/@\\s]+@"),
                                              QRegularExpression::CaseInsensitiveOption);
    text.replace(kUserInfo, QStringLiteral("\\1***@"));
    static const QRegularExpression kBearer(QStringLiteral("(Bearer|Basic)\\s+[A-Za-z0-9._~+/=-]{6,}"),
                                            QRegularExpression::CaseInsensitiveOption);
    text.replace(kBearer, QStringLiteral("\\1 ***"));
    return text;
}

} // namespace fincept::services::datasources
