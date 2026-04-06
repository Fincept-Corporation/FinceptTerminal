#pragma once
#include "core/result/Result.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include <functional>

namespace fincept {

/// Async HTTP client wrapping QNetworkAccessManager.
/// All responses delivered via callbacks on the Qt event loop.
class HttpClient : public QObject {
    Q_OBJECT
  public:
    static HttpClient& instance();

    using JsonCallback = std::function<void(Result<QJsonDocument>)>;

    void get(const QString& url, JsonCallback callback);
    void post(const QString& url, const QJsonObject& body, JsonCallback callback);
    void put(const QString& url, const QJsonObject& body, JsonCallback callback);
    void del(const QString& url, JsonCallback callback);

    void set_auth_header(const QString& api_key);
    void set_session_token(const QString& token);
    void clear_session_token();
    void set_base_url(const QString& base);

  private:
    HttpClient();
    QNetworkRequest build_request(const QString& url) const;
    void handle_reply(QNetworkReply* reply, JsonCallback callback);

    QNetworkAccessManager* nam_ = nullptr;
    QString base_url_;
    QString api_key_;
    QString session_token_;
};

} // namespace fincept
