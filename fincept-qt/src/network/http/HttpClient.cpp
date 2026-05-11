#include "network/http/HttpClient.h"

#include "core/logging/Logger.h"

#include <QUrl>

namespace fincept {

HttpClient& HttpClient::instance() {
    static HttpClient s;
    return s;
}

HttpClient::HttpClient() {
    nam_ = new QNetworkAccessManager(this);
}

QNetworkRequest HttpClient::build_request(const QString& url) const {
    const bool is_relative = !url.startsWith("http");
    const QString full_url = is_relative ? (base_url_ + url) : url;
    QUrl qurl(full_url);
    QNetworkRequest req{qurl};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "FinceptTerminal/4.0");

    // Only attach auth on same-host requests — third-party absolute URLs (Slack/Discord/webhooks)
    // share this singleton and must NOT receive X-API-Key / X-Session-Token.
    const bool same_host = is_relative || [&]() {
        const QUrl base(base_url_);
        return !base.host().isEmpty() && qurl.host().compare(base.host(), Qt::CaseInsensitive) == 0;
    }();

    if (same_host) {
        if (!api_key_.isEmpty()) {
            req.setRawHeader("X-API-Key", api_key_.toUtf8());
        }
        if (!session_token_.isEmpty()) {
            req.setRawHeader("X-Session-Token", session_token_.toUtf8());
        }
    }
    return req;
}

void HttpClient::handle_reply(QNetworkReply* reply, JsonCallback callback, const QObject* context) {
    // Connect to caller-provided context so caller destruction auto-disconnects before the lambda fires.
    const QObject* receiver = context ? context : static_cast<const QObject*>(this);
    connect(reply, &QNetworkReply::finished, receiver, [reply, cb = std::move(callback)]() {
        reply->deleteLater();

        QByteArray data = reply->readAll();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        QJsonParseError parse_err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parse_err);

        if (reply->error() != QNetworkReply::NoError) {
            QUrl sanitized = reply->url();
            sanitized.setQuery(QString{}); // strip query to keep tokens out of logs
            LOG_WARN("HTTP",
                     QString("HTTP %1: %2 — %3").arg(status).arg(sanitized.toString()).arg(reply->errorString()));

            // 401/403 must always be err() — session-expiry detection in AuthApi/SessionGuard depends on it.
            if (status == 401 || status == 403) {
                cb(Result<QJsonDocument>::err(QString("HTTP_%1").arg(status).toStdString()));
                return;
            }

            // Pass through parseable JSON bodies so AuthApi can extract server error messages.
            if (!data.isEmpty() && parse_err.error == QJsonParseError::NoError && doc.isObject()) {
                cb(Result<QJsonDocument>::ok(doc));
                return;
            }

            cb(Result<QJsonDocument>::err(QString("HTTP_%1").arg(status).toStdString()));
            return;
        }

        if (parse_err.error != QJsonParseError::NoError) {
            cb(Result<QJsonDocument>::err(parse_err.errorString().toStdString()));
            return;
        }
        cb(Result<QJsonDocument>::ok(doc));
    });
}

void HttpClient::get(const QString& url, JsonCallback callback, const QObject* context) {
    LOG_DEBUG("HTTP", "GET " + url);
    auto* reply = nam_->get(build_request(url));
    handle_reply(reply, std::move(callback), context);
}

void HttpClient::post(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context) {
    LOG_DEBUG("HTTP", "POST " + url);
    QJsonDocument doc(body);
    auto* reply = nam_->post(build_request(url), doc.toJson());
    handle_reply(reply, std::move(callback), context);
}

void HttpClient::put(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context) {
    LOG_DEBUG("HTTP", "PUT " + url);
    QJsonDocument doc(body);
    auto* reply = nam_->put(build_request(url), doc.toJson());
    handle_reply(reply, std::move(callback), context);
}

void HttpClient::del(const QString& url, JsonCallback callback, const QObject* context) {
    LOG_DEBUG("HTTP", "DELETE " + url);
    auto* reply = nam_->deleteResource(build_request(url));
    handle_reply(reply, std::move(callback), context);
}

void HttpClient::del(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context) {
    LOG_DEBUG("HTTP", "DELETE " + url);
    QJsonDocument doc(body);
    auto* reply = nam_->sendCustomRequest(build_request(url), "DELETE", doc.toJson(QJsonDocument::Compact));
    handle_reply(reply, std::move(callback), context);
}

void HttpClient::set_auth_header(const QString& api_key) {
    api_key_ = api_key;
}

void HttpClient::set_session_token(const QString& token) {
    session_token_ = token;
}

void HttpClient::clear_session_token() {
    session_token_.clear();
}

void HttpClient::set_base_url(const QString& base) {
    base_url_ = base;
}

} // namespace fincept
