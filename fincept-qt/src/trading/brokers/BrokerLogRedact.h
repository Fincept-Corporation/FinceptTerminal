#pragma once
// BrokerLogRedact — strip credentials out of broker HTTP bodies before logging.
//
// Several broker auth/profile endpoints answer with the very secret we just
// asked for: Kite's /session/token returns access_token + public_token, ICICI
// Breeze's /customerdetails returns session_token, and most of them also carry
// the account holder's email. Logging those bodies verbatim writes a live
// trading credential into fincept.log in plaintext — a file users routinely
// attach to bug reports and support tickets.
//
// Dropping the log line instead would cost real diagnostic value: response
// shape and status are how a broken session actually gets debugged. So redact
// by key rather than by line — secret-named fields become "<redacted:N>"
// (N = original length, enough to distinguish "broker returned an empty token"
// from "token present but rejected"), and everything else survives intact.

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1String>
#include <QSet>
#include <QString>

namespace fincept::trading {

namespace redact_detail {

// True when a JSON key names something that must never reach the log file.
inline bool is_secret_key(const QString& key) {
    const QString k = key.toLower();

    // Non-secret metadata that merely mentions a credential. These are the
    // fields you need when debugging an expiry bug, so keep them readable.
    static const QSet<QString> kKeep = {
        QStringLiteral("tokenvalidity"),
        QStringLiteral("token_expires_at"),
        QStringLiteral("token_type"),
        QStringLiteral("expires_in"),
    };
    if (kKeep.contains(k))
        return false;

    // Direct PII — not a credential, but still not log material.
    static const QSet<QString> kExact = {
        QStringLiteral("email"),  QStringLiteral("emailid"),  QStringLiteral("email_id"),  QStringLiteral("phone"),
        QStringLiteral("mobile"), QStringLiteral("mobileno"), QStringLiteral("mobile_no"), QStringLiteral("pan"),
        QStringLiteral("otp"),    QStringLiteral("sid"),
    };
    if (kExact.contains(k))
        return true;

    for (const char* frag : {"token", "secret", "password", "passwd", "apikey", "api_key", "appkey", "app_key",
                             "private", "session", "auth", "signature", "checksum"}) {
        if (k.contains(QLatin1String(frag)))
            return true;
    }
    return false;
}

inline QJsonValue redact_value(const QJsonValue& v);

inline QJsonObject redact_object(const QJsonObject& in) {
    QJsonObject out;
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        if (is_secret_key(it.key())) {
            if (it.value().isString())
                out.insert(it.key(), QStringLiteral("<redacted:%1>").arg(it.value().toString().size()));
            else
                out.insert(it.key(), QStringLiteral("<redacted>"));
        } else {
            out.insert(it.key(), redact_value(it.value()));
        }
    }
    return out;
}

inline QJsonValue redact_value(const QJsonValue& v) {
    if (v.isObject())
        return redact_object(v.toObject());
    if (v.isArray()) {
        QJsonArray out;
        for (const QJsonValue& e : v.toArray())
            out.append(redact_value(e));
        return out;
    }
    return v;
}

} // namespace redact_detail

// Redact a broker response body for logging. JSON bodies keep their structure
// with secret-named fields masked; non-JSON bodies are reported by length only,
// because without a parse we cannot tell which bytes are sensitive.
inline QString redact_body(const QString& raw_body, int max_chars = 500) {
    if (raw_body.isEmpty())
        return QStringLiteral("<empty>");

    const QJsonDocument doc = QJsonDocument::fromJson(raw_body.toUtf8());
    if (doc.isObject()) {
        return QString::fromUtf8(
                   QJsonDocument(redact_detail::redact_object(doc.object())).toJson(QJsonDocument::Compact))
            .left(max_chars);
    }
    if (doc.isArray()) {
        return QString::fromUtf8(
                   QJsonDocument(redact_detail::redact_value(doc.array()).toArray()).toJson(QJsonDocument::Compact))
            .left(max_chars);
    }
    return QStringLiteral("<non-JSON body, %1 bytes>").arg(raw_body.size());
}

} // namespace fincept::trading
