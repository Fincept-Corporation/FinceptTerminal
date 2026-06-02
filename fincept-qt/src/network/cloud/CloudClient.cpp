#include "network/cloud/CloudClient.h"

#include "core/logging/Logger.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QUrl>

namespace fincept::cloud {

CloudClient& CloudClient::instance() {
    static CloudClient s;
    return s;
}

CloudClient::CloudClient() {
    nam_ = new QNetworkAccessManager(this);
}

void CloudClient::set_credentials(const QString& api_key, const QString& session_token) {
    api_key_ = api_key;
    session_token_ = session_token;
}

void CloudClient::clear_credentials() {
    api_key_.clear();
    session_token_.clear();
}

void CloudClient::set_base_url(const QString& base) {
    base_url_ = base;
}

QNetworkRequest CloudClient::build_request(const QString& endpoint) const {
    const QString full = endpoint.startsWith("http") ? endpoint : (base_url_ + endpoint);
    QNetworkRequest req{QUrl(full)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "FinceptTerminal/4.0");
    if (!api_key_.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + api_key_.toUtf8());
    if (!session_token_.isEmpty())
        req.setRawHeader("X-Session-Token", session_token_.toUtf8());
    return req;
}

void CloudClient::finish(QNetworkReply* reply, Callback cb, const QObject* context) {
    const QObject* receiver = context ? context : static_cast<const QObject*>(this);
    connect(reply, &QNetworkReply::finished, receiver, [reply, cb = std::move(cb)]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        CloudResponse out;
        out.status = status;

        QJsonParseError perr;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (perr.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject obj = doc.object();
            out.data = obj.value("data");
            out.error = obj.value("error").toString();
            out.message = obj.value("message").toString();
            out.hint = obj.value("hint").toString();
            // finceptgo sets success=true on 2xx; trust it but still require 2xx.
            const bool envelope_ok = obj.value("success").toBool(status >= 200 && status < 300);
            out.ok = envelope_ok && status >= 200 && status < 300;
        } else {
            out.ok = (status >= 200 && status < 300) && reply->error() == QNetworkReply::NoError;
        }

        if (!out.ok && out.error.isEmpty()) {
            out.error = reply->error() != QNetworkReply::NoError
                            ? QStringLiteral("network_error")
                            : QStringLiteral("http_%1").arg(status);
        }

        if (!out.ok) {
            QUrl sanitized = reply->url();
            sanitized.setQuery(QString{}); // keep tokens/queries out of logs
            LOG_WARN("CloudSync",
                     QString("HTTP %1 %2 — %3").arg(status).arg(sanitized.toString()).arg(out.error));
        }
        cb(out);
    });
}

void CloudClient::get(const QString& endpoint, Callback cb, const QObject* context) {
    LOG_DEBUG("CloudSync", "GET " + endpoint);
    finish(nam_->get(build_request(endpoint)), std::move(cb), context);
}

void CloudClient::post(const QString& endpoint, const QJsonObject& body, Callback cb, const QObject* context) {
    LOG_DEBUG("CloudSync", "POST " + endpoint);
    finish(nam_->post(build_request(endpoint), QJsonDocument(body).toJson(QJsonDocument::Compact)), std::move(cb),
           context);
}

void CloudClient::put(const QString& endpoint, const QJsonObject& body, Callback cb, const QObject* context) {
    LOG_DEBUG("CloudSync", "PUT " + endpoint);
    finish(nam_->put(build_request(endpoint), QJsonDocument(body).toJson(QJsonDocument::Compact)), std::move(cb),
           context);
}

void CloudClient::del(const QString& endpoint, Callback cb, const QObject* context) {
    LOG_DEBUG("CloudSync", "DELETE " + endpoint);
    finish(nam_->deleteResource(build_request(endpoint)), std::move(cb), context);
}

} // namespace fincept::cloud
