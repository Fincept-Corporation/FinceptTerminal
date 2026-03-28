// QuantLibClient.cpp — Shared QuantLib API HTTP client.

#include "services/quantlib/QuantLibClient.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

namespace fincept::services {

static constexpr const char* TAG = "QuantLibClient";

const QString QuantLibClient::API_BASE = QStringLiteral("https://api.fincept.in");

// Endpoints that use GET (no request body) — everything else is POST.
static const QStringList GET_ENDPOINTS = {
    "core/types/currencies",
    "core/types/frequencies",
    "scheduling/calendar/list",
    "scheduling/daycount/conventions",
    "scheduling/adjustment/methods",
};

bool QuantLibClient::is_get_endpoint(const QString& endpoint) {
    return GET_ENDPOINTS.contains(endpoint);
}

// ── Singleton ────────────────────────────────────────────────────────────────

QuantLibClient::QuantLibClient(QObject* parent) : QObject(parent) {}

QuantLibClient& QuantLibClient::instance() {
    static QuantLibClient inst;
    return inst;
}

// ── Request builder ──────────────────────────────────────────────────────────

static QNetworkRequest build_request(const QString& endpoint) {
    QString url = QuantLibClient::API_BASE + "/quantlib/" + endpoint;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "FinceptTerminal/4.0.0");

    auto& auth = auth::AuthManager::instance();
    if (auth.is_authenticated())
        req.setRawHeader("X-API-Key", auth.session().api_key.toUtf8());

    return req;
}

// ── Response parser ──────────────────────────────────────────────────────────

mcp::ToolResult QuantLibClient::parse_response(int http_status, const QByteArray& raw) {
    QJsonParseError parse_err;
    auto doc = QJsonDocument::fromJson(raw, &parse_err);

    if (parse_err.error != QJsonParseError::NoError) {
        return mcp::ToolResult::fail(
            QString("JSON parse error (HTTP %1): %2").arg(http_status).arg(parse_err.errorString()));
    }

    // FastAPI 422 validation error: {"detail": [...]}
    if (http_status == 422 && doc.isObject()) {
        auto obj = doc.object();
        if (obj.contains("detail")) {
            QString msg;
            auto detail = obj["detail"];
            if (detail.isArray()) {
                for (const auto& item : detail.toArray()) {
                    auto d = item.toObject();
                    QString loc = d["loc"].toArray().last().toString();
                    msg += loc + ": " + d["msg"].toString() + "\n";
                }
            } else {
                msg = detail.toString();
            }
            return mcp::ToolResult::fail("Validation error: " + msg.trimmed());
        }
    }

    if (!doc.isObject()) {
        // Top-level array or scalar — return as-is in data
        if (doc.isArray())
            return mcp::ToolResult::ok_data(doc.array());
        return mcp::ToolResult::fail(QString("Unexpected response (HTTP %1)").arg(http_status));
    }

    auto obj = doc.object();

    // API envelope: {"success": bool, "message": "...", "data": <payload>}
    if (obj.contains("success")) {
        if (!obj["success"].toBool()) {
            QString msg = obj["message"].toString();
            if (msg.isEmpty())
                msg = QString("API error (HTTP %1)").arg(http_status);
            return mcp::ToolResult::fail(msg);
        }
        // Unwrap data payload
        auto payload = obj.value("data");
        QString message = obj.value("message").toString();
        return mcp::ToolResult::ok(message, payload);
    }

    // No envelope — return raw object
    return mcp::ToolResult::ok_data(obj);
}

// ── Async call ───────────────────────────────────────────────────────────────

void QuantLibClient::call(const QString& endpoint, const QJsonObject& body, QuantLibCallback callback) {
    auto* nam = new QNetworkAccessManager(this);
    auto req = build_request(endpoint);

    QNetworkReply* reply = nullptr;
    if (is_get_endpoint(endpoint)) {
        reply = nam->get(req);
    } else {
        QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
        reply = nam->post(req, data);
    }

    QPointer<QuantLibClient> self = this;
    connect(reply, &QNetworkReply::finished, this, [self, reply, nam, endpoint, callback]() {
        reply->deleteLater();
        nam->deleteLater();

        if (!self) return;

        if (reply->error() != QNetworkReply::NoError) {
            LOG_ERROR(TAG, "Network error on " + endpoint + ": " + reply->errorString());
            callback(mcp::ToolResult::fail(reply->errorString()));
            return;
        }

        int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto raw = reply->readAll();
        auto result = parse_response(http_status, raw);

        LOG_DEBUG(TAG, QString("HTTP %1 — %2 — %3")
                           .arg(http_status)
                           .arg(endpoint)
                           .arg(result.success ? "OK" : result.error));

        callback(result);
    });
}

// ── Sync call (MCP tool handlers only — never call from UI thread) ───────────

mcp::ToolResult QuantLibClient::call_sync(const QString& endpoint, const QJsonObject& body) {
    mcp::ToolResult result = mcp::ToolResult::fail("Timeout");

    QEventLoop loop;
    call(endpoint, body, [&](mcp::ToolResult r) {
        result = std::move(r);
        loop.quit();
    });
    loop.exec();

    return result;
}

} // namespace fincept::services
