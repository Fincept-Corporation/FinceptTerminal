// BrokerHttp — synchronous HTTP for broker API calls

#include "trading/brokers/BrokerHttp.h"

#include "core/logging/Logger.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QThreadStorage>
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

BrokerHttpResponse BrokerHttp::post_json_array(const QString& url, const QJsonArray& payload,
                                               const QMap<QString, QString>& headers) {
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return execute("POST", url, body, "application/json", headers);
}

BrokerHttpResponse BrokerHttp::put_json(const QString& url, const QJsonObject& payload,
                                        const QMap<QString, QString>& headers) {
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return execute("PUT", url, body, "application/json", headers);
}

BrokerHttpResponse BrokerHttp::patch_json(const QString& url, const QJsonObject& payload,
                                          const QMap<QString, QString>& headers) {
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return execute("PATCH", url, body, "application/json", headers);
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
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        query.addQueryItem(it.key(), it.value());
    QByteArray body = query.toString(QUrl::FullyEncoded).toUtf8();
    return execute("POST", url, body, "application/x-www-form-urlencoded", headers);
}

BrokerHttpResponse BrokerHttp::put_form(const QString& url, const QMap<QString, QString>& params,
                                        const QMap<QString, QString>& headers) {
    QUrlQuery query;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        query.addQueryItem(it.key(), it.value());
    QByteArray body = query.toString(QUrl::FullyEncoded).toUtf8();
    return execute("PUT", url, body, "application/x-www-form-urlencoded", headers);
}

BrokerHttpResponse BrokerHttp::post_raw(const QString& url, const QByteArray& body,
                                        const QMap<QString, QString>& headers) {
    QString ct = "application/x-www-form-urlencoded";
    auto it = headers.find("Content-Type");
    if (it != headers.end())
        ct = it.value();
    QMap<QString, QString> h = headers;
    h.remove("Content-Type");
    return execute("POST", url, body, ct, h);
}

BrokerHttpResponse BrokerHttp::put_raw(const QString& url, const QByteArray& body,
                                       const QMap<QString, QString>& headers) {
    QString ct = "application/x-www-form-urlencoded";
    auto it = headers.find("Content-Type");
    if (it != headers.end())
        ct = it.value();
    QMap<QString, QString> h = headers;
    h.remove("Content-Type");
    return execute("PUT", url, body, ct, h);
}

BrokerHttpResponse BrokerHttp::send(const QString& method, const QString& url, const QByteArray& body,
                                    const QString& content_type, const QMap<QString, QString>& headers) {
    return execute(method, url, body, content_type, headers);
}

BrokerHttpResponse BrokerHttp::execute(const QString& method, const QString& url, const QByteArray& body,
                                       const QString& content_type, const QMap<QString, QString>& headers) {
    // One QNetworkAccessManager per worker thread so TLS/TCP connections are
    // reused across calls (keep-alive), instead of being created and torn down
    // for each request. Removed the global mutex — QNAM handles its own queueing
    // and the previous serialization was causing broker calls to block one
    // another, which made token exchange feel like it took "minutes".
    static QThreadStorage<QNetworkAccessManager*> tls_nam;
    if (!tls_nam.hasLocalData())
        tls_nam.setLocalData(new QNetworkAccessManager);
    QNetworkAccessManager& nam = *tls_nam.localData();

    QUrl req_url(url);
    QNetworkRequest req(req_url);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    // Explicit per-request transfer timeout so a dead socket aborts fast instead
    // of quietly draining the 8s budget. Qt >= 6.2.
    req.setTransferTimeout(timeout_ms_);

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
        // Some broker REST APIs (e.g. ICICI Breeze) send a JSON body on GET.
        // QNetworkAccessManager::get() can't carry a body, so fall back to a
        // custom request when one is present. Empty-body GET keeps the fast path.
        if (body.isEmpty())
            reply = nam.get(req);
        else
            reply = nam.sendCustomRequest(req, "GET", body);
    } else if (method == "POST") {
        reply = nam.post(req, body);
    } else if (method == "PUT") {
        reply = nam.put(req, body);
    } else if (method == "PATCH") {
        reply = nam.sendCustomRequest(req, "PATCH", body);
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

    // Block until finished (with timeout). Measure the actual network round-trip
    // by bracketing only the event-loop wait — this excludes request setup and
    // JSON parsing so rtt_ms is comparable to what Postman/Bruno report.
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QElapsedTimer rtt;
    rtt.start();
    timer.start(timeout_ms_);
    loop.exec();
    const double elapsed_ms = static_cast<double>(rtt.nsecsElapsed()) / 1e6;

    BrokerHttpResponse result;
    result.rtt_ms = elapsed_ms;

    if (timer.isActive()) {
        timer.stop();
    } else {
        // Timeout
        reply->abort();
        reply->deleteLater();
        BrokerHttpResponse timeout_result;
        timeout_result.error = "Request timed out";
        timeout_result.rtt_ms = elapsed_ms;
        return timeout_result;
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
