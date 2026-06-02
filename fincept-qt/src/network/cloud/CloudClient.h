#pragma once
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkReply;

namespace fincept::cloud {

/// Parsed finceptgo response envelope: { success, message, data, error, hint }.
struct CloudResponse {
    bool ok = false;     // transport succeeded AND envelope success AND 2xx
    int status = 0;      // HTTP status code
    QJsonValue data;     // envelope "data" (object or array)
    QString error;       // envelope "error" code, or a transport error token
    QString message;     // human-readable message
    QString hint;        // optional fix-up hint

    bool is_insufficient_credits() const { return error == QLatin1String("insufficient_credits"); }
    bool is_auth_error() const { return status == 401 || status == 403; }
    bool is_rate_limited() const { return status == 429 || error == QLatin1String("rate_limit_exceeded"); }
};

/// Async client for the Fincept Cloud API (finceptgo, mounted under /v1).
///
/// Sends `Authorization: Bearer <api_key>` (+ optional `X-Session-Token`). This is
/// deliberately separate from the legacy `HttpClient`, which sends `X-API-Key`
/// (rejected by finceptgo) on the same host. Credentials are pushed in by
/// `CloudSyncEngine` from `AuthManager`.
class CloudClient : public QObject {
    Q_OBJECT
  public:
    static CloudClient& instance();

    using Callback = std::function<void(CloudResponse)>;

    /// `context` scopes callback lifetime (Qt auto-disconnects on its destruction).
    /// Pass a long-lived object; nullptr is safe only for app-lifetime callers.
    void get(const QString& endpoint, Callback cb, const QObject* context = nullptr);
    void post(const QString& endpoint, const QJsonObject& body, Callback cb, const QObject* context = nullptr);
    void put(const QString& endpoint, const QJsonObject& body, Callback cb, const QObject* context = nullptr);
    void del(const QString& endpoint, Callback cb, const QObject* context = nullptr);

    void set_credentials(const QString& api_key, const QString& session_token);
    void clear_credentials();
    void set_base_url(const QString& base); // e.g. https://api.fincept.in/v1
    QString base_url() const { return base_url_; }
    bool has_credentials() const { return !api_key_.isEmpty(); }

  private:
    CloudClient();
    Q_DISABLE_COPY(CloudClient)

    QNetworkRequest build_request(const QString& endpoint) const;
    void finish(QNetworkReply* reply, Callback cb, const QObject* context);

    QNetworkAccessManager* nam_ = nullptr;
    QString base_url_;
    QString api_key_;
    QString session_token_;
};

} // namespace fincept::cloud
