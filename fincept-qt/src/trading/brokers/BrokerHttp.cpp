// BrokerHttp — synchronous HTTP for broker API calls

#include "trading/brokers/BrokerHttp.h"

#include "core/logging/Logger.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::trading {

BrokerHttp& BrokerHttp::instance() {
    static BrokerHttp s;
    return s;
}

BrokerHttpResponse BrokerHttp::get(const QString& url, const QMap<QString, QString>& headers) {
    return execute("GET", url, {}, "", headers);
}

BrokerHttpResponse BrokerHttp::post_json(const QString& url, const QJsonObject& payload,
                                         const QMap<QString, QString>& headers) {
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return execute("POST", url, body, "application/json", headers);
}

BrokerHttpResponse BrokerHttp::put_json(const QString& url, const QJsonObject& payload,
                                        const QMap<QString, QString>& headers) {
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return execute("PUT", url, body, "application/json", headers);
}

BrokerHttpResponse BrokerHttp::del(const QString& url, const QMap<QString, QString>& headers,
                                   const QJsonObject& payload) {
    QByteArray body;
    if (!payload.isEmpty()) {
        body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    }
    return execute("DELETE", url, body, body.isEmpty() ? "" : "application/json", headers);
}

BrokerHttpResponse BrokerHttp::post_form(const QString& url, const QMap<QString, QString>& params,
                                         const QMap<QString, QString>& headers) {
    QUrlQuery query;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        query.addQueryItem(it.key(), it.value());
    }
    QByteArray body = query.toString(QUrl::FullyEncoded).toUtf8();
    return execute("POST", url, body, "application/x-www-form-urlencoded", headers);
}

BrokerHttpResponse BrokerHttp::execute(const QString& method, const QString& url, const QByteArray& body,
                                       const QString& content_type, const QMap<QString, QString>& headers) {
    QNetworkAccessManager nam;
    QUrl req_url(url);
    QNetworkRequest req(req_url);

    // Set headers
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }
    if (!content_type.isEmpty()) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, content_type);
    }
    req.setRawHeader("Accept", "application/json");

    // Send request
    QNetworkReply* reply = nullptr;
    if (method == "GET") {
        reply = nam.get(req);
    } else if (method == "POST") {
        reply = nam.post(req, body);
    } else if (method == "PUT") {
        reply = nam.put(req, body);
    } else if (method == "DELETE") {
        if (body.isEmpty()) {
            reply = nam.deleteResource(req);
        } else {
            reply = nam.sendCustomRequest(req, "DELETE", body);
        }
    }

    if (!reply) {
        return {false, 0, {}, "", "Failed to create network request"};
    }

    // Block until finished (with timeout)
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeout_ms_);
    loop.exec();

    BrokerHttpResponse result;

    if (timer.isActive()) {
        timer.stop();
    } else {
        // Timeout
        reply->abort();
        reply->deleteLater();
        return {false, 0, {}, "", "Request timed out"};
    }

    result.status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.raw_body = QString::fromUtf8(reply->readAll());

    if (reply->error() != QNetworkReply::NoError && result.status_code == 0) {
        result.error = reply->errorString();
        reply->deleteLater();
        return result;
    }

    // Parse JSON
    QJsonParseError parseErr;
    auto doc = QJsonDocument::fromJson(result.raw_body.toUtf8(), &parseErr);
    if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
        result.json = doc.object();
    }

    result.success = (result.status_code >= 200 && result.status_code < 300);
    if (!result.success && result.error.isEmpty()) {
        result.error = result.json.value("message").toString(
            result.json.value("error").toString(QString("HTTP %1").arg(result.status_code)));
    }

    reply->deleteLater();
    return result;
}

} // namespace fincept::trading
