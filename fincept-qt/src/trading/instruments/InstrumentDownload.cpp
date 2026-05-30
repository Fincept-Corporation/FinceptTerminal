#include "trading/instruments/InstrumentDownload.h"

#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace fincept::trading {

namespace {

// Run one request to completion on a private QNAM and return the raw body.
QByteArray run_request(const QString& method, const QString& url, const QByteArray& body,
                       const QString& content_type, const QMap<QString, QString>& headers, int timeout_ms) {
    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    QNetworkReply* reply = nullptr;
    if (method == "POST") {
        if (!content_type.isEmpty())
            req.setHeader(QNetworkRequest::ContentTypeHeader, content_type);
        reply = nam->post(req, body);
    } else {
        reply = nam->get(req);
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeout_ms);
    loop.exec();

    auto drain = [&]() {
        reply->deleteLater();
        nam->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };

    if (!timer.isActive()) {
        reply->abort();
        LOG_ERROR("InstrumentDownload", QString("Timeout fetching %1").arg(url));
        drain();
        return {};
    }
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_ERROR("InstrumentDownload", QString("Failed %1: %2").arg(url, reply->errorString()));
        drain();
        return {};
    }

    QByteArray data = reply->readAll();
    drain();
    LOG_INFO("InstrumentDownload", QString("Fetched %1 (%2 bytes)").arg(url).arg(data.size()));
    return data;
}

} // namespace

QByteArray http_get_blocking(const QString& url, const QMap<QString, QString>& headers, int timeout_ms) {
    return run_request("GET", url, {}, {}, headers, timeout_ms);
}

QByteArray http_post_blocking(const QString& url, const QByteArray& body, const QString& content_type,
                              const QMap<QString, QString>& headers, int timeout_ms) {
    return run_request("POST", url, body, content_type, headers, timeout_ms);
}

} // namespace fincept::trading
