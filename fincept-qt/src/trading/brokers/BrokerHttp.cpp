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

static constexpr const char* kBrokerHttpTag = "BrokerHttp";

// URL path for a log line: never the query (tokens travel there), and path segments that
// look like identifiers (account numbers, order ids — they contain digits) cut to their
// last 4 characters. Host is logged separately.
static QString broker_http_log_path(const QUrl& url) {
    QStringList parts = url.path().split(QLatin1Char('/'));
    for (QString& part : parts) {
        bool has_digit = false;
        for (const QChar c : part)
            has_digit = has_digit || c.isDigit();
        if (has_digit && part.size() > 4)
            part = log_mask(part);
    }
    return parts.join(QLatin1Char('/'));
}

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
        LOG_ERROR(kBrokerHttpTag, logf("Broker request could not be created",
                                       {{"method", method},
                                        {"host", req_url.host()},
                                        {"path", broker_http_log_path(req_url)}}));
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

    // Timeout. The abort is client-side only: the broker may already have
    // accepted the request, so this is "outcome unknown", not "did not
    // happen". A blind retry of an order placement here creates a second
    // live order — order payloads therefore carry a unique client order
    // reference (BrokerClientOrderId.h) so the broker can reject the
    // duplicate. Say so in the message rather than implying nothing ran.
    const QString timeout_msg = QString("Request timed out after %1 ms — outcome unknown, the request may have "
                                        "been accepted; verify before retrying")
                                    .arg(timeout_ms_);

    // One line per failed request, written here so every broker adapter is covered. Host and
    // masked path only — never the query, headers or body. Keyed per endpoint so a poller that
    // keeps timing out writes one line a minute while an order endpoint keeps its own key
    // (order outcomes are also in the audit trail written by UnifiedTrading).
    auto log_timeout = [&]() {
        LOG_ERROR_RL(kBrokerHttpTag,
                     method + QLatin1Char(' ') + req_url.host() + broker_http_log_path(req_url) +
                         QStringLiteral("|timeout"),
                     60000,
                     logf("Broker request timed out, outcome unknown",
                          {{"method", method},
                           {"host", req_url.host()},
                           {"path", broker_http_log_path(req_url)},
                           {"timeout_ms", timeout_ms_},
                           {"duration_ms", static_cast<qint64>(elapsed_ms)}}));
    };

    if (timer.isActive()) {
        timer.stop();
    } else {
        log_timeout();
        reply->abort();
        reply->deleteLater();
        BrokerHttpResponse timeout_result;
        timeout_result.error = timeout_msg;
        timeout_result.rtt_ms = elapsed_ms;
        return timeout_result;
    }

    result.status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString response_content_type = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    result.raw_body = QString::fromUtf8(reply->readAll());

    if (reply->error() != QNetworkReply::NoError && result.status_code == 0) {
        // Qt's own per-request transfer timeout (setTransferTimeout above) is armed
        // with the same budget as the QTimer and usually wins the race: it finishes
        // the reply with TimeoutError / OperationCanceledError, which used to surface
        // as a bare "Operation timed out" and lose the "outcome unknown" warning that
        // guards against a blind order retry. Nothing else aborts this reply.
        if (reply->error() == QNetworkReply::TimeoutError || reply->error() == QNetworkReply::OperationCanceledError)
            result.error = timeout_msg;
        else
            result.error = reply->errorString();
        if (result.error == timeout_msg) {
            log_timeout();
        } else {
            // Offline: every poll fails the same way — one line a minute per endpoint and error.
            LOG_ERROR_RL(kBrokerHttpTag,
                         method + QLatin1Char(' ') + req_url.host() + broker_http_log_path(req_url) +
                             QLatin1Char('|') + QString::number(static_cast<int>(reply->error())),
                         60000,
                         logf("Broker request failed at transport level",
                              {{"method", method},
                               {"host", req_url.host()},
                               {"path", broker_http_log_path(req_url)},
                               {"qt_error", static_cast<int>(reply->error())},
                               {"duration_ms", static_cast<qint64>(elapsed_ms)},
                               {"error", result.error}}));
        }
        reply->deleteLater();
        return result;
    }

    // Parse JSON
    const QByteArray body_bytes = result.raw_body.toUtf8();
    QJsonParseError parseErr;
    auto doc = QJsonDocument::fromJson(body_bytes, &parseErr);
    if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
        result.json = doc.object();
    }

    result.success = (result.status_code >= 200 && result.status_code < 300);

    // A 2xx whose body we could not parse is NOT a success. The parse failure
    // used to be dropped on the floor, leaving `json` default-constructed while
    // `success` stayed true — Alpaca then read an empty `id` off it and reported
    // "Order placed: " with no order id when nothing had been placed.
    //
    // Only a body the server itself labelled as JSON is held to this: an empty
    // body is legitimate (204 on DELETE/cancel), a top-level JSON array is
    // legitimate (Alpaca, IBKR and Shoonya return arrays and parse raw_body
    // themselves — those leave `json` empty by design and parse cleanly), and
    // genuinely non-JSON endpoints must keep working (Motilal's
    // /getscripmastercsv and Kite's /instruments both return CSV through this
    // same client). A missing or non-JSON content type therefore falls through
    // to the previous behaviour rather than inventing a failure.
    if (result.success && parseErr.error != QJsonParseError::NoError && !body_bytes.trimmed().isEmpty() &&
        response_content_type.contains(QLatin1String("json"), Qt::CaseInsensitive)) {
        result.success = false;
        result.error = QString("Malformed JSON response (HTTP %1): %2 at offset %3")
                           .arg(result.status_code)
                           .arg(parseErr.errorString())
                           .arg(parseErr.offset);
        // Never the body — its size and where parsing stopped.
        LOG_ERROR_RL(kBrokerHttpTag,
                     method + QLatin1Char(' ') + req_url.host() + broker_http_log_path(req_url) +
                         QStringLiteral("|json"),
                     60000,
                     logf("Broker returned malformed JSON", {{"method", method},
                                                             {"host", req_url.host()},
                                                             {"path", broker_http_log_path(req_url)},
                                                             {"status", result.status_code},
                                                             {"offset", parseErr.offset},
                                                             {"bytes", body_bytes.size()},
                                                             {"error", parseErr.errorString()}}));
    }

    if (!result.success && result.error.isEmpty()) {
        result.error = result.json.value("message").toString(
            result.json.value("error").toString(QString("HTTP %1").arg(result.status_code)));
    }

    // A 429 with no usable message used to surface as a bare "HTTP 429". Name it as a rate limit and,
    // when the broker gives a numeric Retry-After, say when to come back.
    if (result.status_code == 429 && result.error == QLatin1String("HTTP 429")) {
        bool numeric = false;
        const int retry_after = QString::fromLatin1(reply->rawHeader("Retry-After")).trimmed().toInt(&numeric);
        result.error = (numeric && retry_after > 0)
                           ? QString("Rate limited by the broker (HTTP 429) — retry after %1 s").arg(retry_after)
                           : QString("Rate limited by the broker (HTTP 429) — wait a moment and retry");
    }

    // Non-2xx answer (including 429). Pollers repeat the same refusal (an expired token
    // answers 401 on every poll): one line a minute per endpoint and status.
    if (result.status_code < 200 || result.status_code >= 300) {
        LOG_WARN_RL(kBrokerHttpTag,
                    method + QLatin1Char(' ') + req_url.host() + broker_http_log_path(req_url) + QLatin1Char('|') +
                        QString::number(result.status_code),
                    60000,
                    logf("Broker request rejected", {{"method", method},
                                                     {"host", req_url.host()},
                                                     {"path", broker_http_log_path(req_url)},
                                                     {"status", result.status_code},
                                                     {"duration_ms", static_cast<qint64>(elapsed_ms)},
                                                     {"error", result.error}}));
    }

    reply->deleteLater();
    return result;
}

} // namespace fincept::trading
