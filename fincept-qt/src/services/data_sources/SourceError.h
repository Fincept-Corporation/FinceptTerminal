#pragma once
// SourceError — one classified, plain-language failure for every data source.
//
// The codes are the connector contract's (scripts/fincept_connector.py ERROR_CODES) so a
// catalogue connector, a saved connection and a mapping run all fail the same way and the
// UI maps each code to one action:
//   missing_key   → "Add key" (Settings > Credentials, deep link naming the key)
//   auth_failed   → "Check credentials" (edit the connection / replace the key)
//   rate_limited  → retry countdown (retry_after_s)
//   upstream_down → RETRY
//   bad_args      → fix the input named in `field`
//   no_data       → "No rows for this request"
//   not_supported → the reason (driver missing in this build, broken upstream, …) + `fix`
//   unknown       → the message
// QtCore only.

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fincept::services::datasources {

enum class ErrorCode { None, MissingKey, AuthFailed, RateLimited, UpstreamDown, BadArgs, NoData, NotSupported, Unknown };

/// "missing_key" … "unknown"; "" for None.
QString error_code_name(ErrorCode code);
/// Inverse of error_code_name(); unrecognised text → Unknown, empty → None.
ErrorCode error_code_from_name(const QString& name);

struct SourceError {
    ErrorCode code = ErrorCode::None;
    QString message;          ///< plain sentence; never contains a secret or a URL with credentials
    int retry_after_s = -1;   ///< rate_limited: seconds to wait (-1 = unknown)
    QStringList env_keys;     ///< missing_key / auth_failed: the credential names
    QString docs_url;         ///< where to get a key / read about the failure
    QString field;            ///< bad_args: the config field or argument concerned
    QString fix;              ///< not_supported: the exact step that makes it work ("Install …")
    bool cancelled = false;   ///< the user cancelled — never shown as an error

    bool ok() const { return code == ErrorCode::None && !cancelled; }
    static SourceError make(ErrorCode code, const QString& message, const QString& field = QString());
    static SourceError cancel();

    /// Short title for the error page ("Needs an API key", "Rate limited", …).
    QString title() const;
    QJsonObject to_json() const;
    static SourceError from_json(const QJsonObject& obj);
};

/// Classify an HTTP status (+ Retry-After header value) into a SourceError with a plain message.
/// `what` names the target ("the server", "Alpha Vantage").
SourceError error_from_http_status(int status, const QString& what, const QString& retry_after = QString(),
                                   const QString& server_message = QString());

/// Mask every literal secret and every key-like query parameter (api_key=…, token=…) in `text`.
QString redact_secrets(QString text, const QStringList& secrets);

} // namespace fincept::services::datasources
