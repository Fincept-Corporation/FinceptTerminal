// LlmHttpHelpers.cpp — synchronous HTTP, SSE chunk parsing, and token-usage
// extraction used by LlmService. These are static methods of LlmService living
// in this TU so the main file stays smaller.

#include "ai_chat/LlmService.h"

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QTimer>
#include <QUrl>

namespace fincept::ai_chat {

// ── Blocking POST ──────────────────────────────────────────────────────────
// Background-thread only. Delegates to eventloop_request which works for
// Cloudflare-protected endpoints (the old waitForReadyRead() path failed
// during TLS negotiation behind Cloudflare).
LlmService::HttpResult LlmService::blocking_post(const QString& url, const QJsonObject& body,
                                                 const QMap<QString, QString>& headers, int timeout_ms) {
    QByteArray json_data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return eventloop_request("POST", url, json_data, headers, timeout_ms);
}

// ── Blocking GET ───────────────────────────────────────────────────────────
LlmService::HttpResult LlmService::blocking_get(const QString& url, const QMap<QString, QString>& headers,
                                                int timeout_ms) {
    return eventloop_request("GET", url, {}, headers, timeout_ms);
}

// ── QEventLoop-based HTTP ──────────────────────────────────────────────────
// Required for endpoints behind Cloudflare (api.fincept.in) because
// QNetworkAccessManager needs an event loop to process TLS/SSL handshakes
// and HTTP redirects.
LlmService::HttpResult LlmService::eventloop_request(const QString& method, const QString& url, const QByteArray& body,
                                                     const QMap<QString, QString>& headers, int timeout_ms) {
    HttpResult result;

    // ── Lifetime note ─────────────────────────────────────────────────────────
    // The QNetworkAccessManager and its QNetworkReply MUST outlive the local
    // QEventLoop *and* drain pending DeferredDelete events before this
    // function returns. A stack-allocated QNAM here used to crash the main
    // thread hours later (issue: KERN_INVALID_ADDRESS @ 0x3f inside
    // QIODevice::channelReadyRead via qt_mac_socket_callback) — Qt's macOS
    // CFSocket bridge fires runloop callbacks against socket QObjects whose
    // d_ptr has been torn down with the NAM. Heap-allocate and explicitly
    // process DeferredDelete so the socket-notifier teardown completes
    // before the function returns.
    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    QNetworkReply* reply = (method == "GET") ? nam->get(req) : nam->post(req, body);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(timeout_ms);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    auto drain = [&]() {
        reply->deleteLater();
        nam->deleteLater();
        // Drain DeferredDelete events on the current thread so the NAM and
        // its socket subscribers tear down while a dispatcher still exists.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };

    if (!reply->isFinished()) {
        reply->abort();
        result.error = "Request timed out";
        drain();
        return result;
    }

    result.status  = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body    = reply->readAll();
    result.success = (result.status >= 200 && result.status < 300);
    if (!result.success) {
        QString server_msg;
        auto err_doc = QJsonDocument::fromJson(result.body);
        if (!err_doc.isNull() && err_doc.isObject()) {
            QJsonObject ej = err_doc.object();
            // Top-level {"message": ...} (OpenAI/Anthropic legacy shape)
            if (ej.contains("message") && ej["message"].isString())
                server_msg = ej["message"].toString();
            // Nested {"error": {"message": ..., "metadata": {"raw": ...}}}
            // (OpenAI current, OpenRouter, DeepSeek, Groq all use this shape)
            if (server_msg.isEmpty() && ej.contains("error") && ej["error"].isObject()) {
                QJsonObject eo = ej["error"].toObject();
                if (eo.contains("message") && eo["message"].isString())
                    server_msg = eo["message"].toString();
                // OpenRouter surfaces upstream errors in metadata.raw
                if (eo.contains("metadata") && eo["metadata"].isObject()) {
                    QJsonObject md = eo["metadata"].toObject();
                    QString raw = md["raw"].toString();
                    QString provider_name = md["provider_name"].toString();
                    if (!raw.isEmpty())
                        server_msg += " (upstream " + provider_name + ": " + raw + ")";
                }
            }
        }
        result.error = server_msg.isEmpty() ? QString("HTTP %1: %2").arg(result.status).arg(reply->errorString())
                                            : QString("HTTP %1: %2").arg(result.status).arg(server_msg);
    }
    drain();
    return result;
}

// ── SSE chunk → text ───────────────────────────────────────────────────────
QString LlmService::parse_sse_chunk(const QString& data, const QString& provider) {
    auto doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isObject())
        return {};
    QJsonObject j = doc.object();

    if (provider == "anthropic") {
        // delta is a tagged union. text_delta carries the final answer;
        // thinking_delta carries the chain-of-thought for extended-thinking
        // models (claude-3-7 / claude-opus-4+ with `thinking` param). Surfacing
        // both keeps the UI responsive during long reasoning phases. Other
        // delta types (input_json_delta, signature_delta) must not be rendered.
        if (j["type"].toString() == "content_block_delta") {
            QJsonObject delta = j["delta"].toObject();
            const QString dtype = delta["type"].toString();
            if (dtype == "text_delta")
                return delta["text"].toString();
            if (dtype == "thinking_delta")
                return delta["thinking"].toString();
        }
        return {};
    }

    // OpenAI-compatible
    QJsonArray choices = j["choices"].toArray();
    if (!choices.isEmpty()) {
        QJsonObject delta = choices[0].toObject()["delta"].toObject();
        if (!delta["content"].isNull() && !delta["content"].isUndefined()) {
            QString s = delta["content"].toString();
            if (!s.isEmpty())
                return s;
        }
        // Reasoning models (kimi-k2.5 / kimi-k2.6 / kimi-k2-thinking*, deepseek-reasoner,
        // grok-4 reasoning variants) stream their chain-of-thought as
        // `delta.reasoning_content` and only emit `delta.content` after reasoning
        // completes. Surface reasoning deltas so the user sees progress instead
        // of a blank bubble for 10+ seconds.
        if (!delta["reasoning_content"].isNull() && !delta["reasoning_content"].isUndefined()) {
            QString s = delta["reasoning_content"].toString();
            if (!s.isEmpty())
                return s;
        }
        // Refusal deltas — newer OpenAI and some Groq safety paths stream a
        // `refusal` field in place of `content` when the model declines.
        if (!delta["refusal"].isNull() && !delta["refusal"].isUndefined())
            return delta["refusal"].toString();
    }
    return {};
}

// ── Token usage extraction ─────────────────────────────────────────────────
void LlmService::parse_usage(LlmResponse& resp, const QJsonObject& rj, const QString& provider) {
    if (!rj.contains("usage"))
        return;
    QJsonObject u = rj["usage"].toObject();
    if (provider == "anthropic") {
        resp.prompt_tokens     = u["input_tokens"].toInt();
        resp.completion_tokens = u["output_tokens"].toInt();
        resp.total_tokens      = resp.prompt_tokens + resp.completion_tokens;
    } else {
        resp.prompt_tokens     = u["prompt_tokens"].toInt();
        resp.completion_tokens = u["completion_tokens"].toInt();
        resp.total_tokens      = u["total_tokens"].toInt();
    }
}

} // namespace fincept::ai_chat
