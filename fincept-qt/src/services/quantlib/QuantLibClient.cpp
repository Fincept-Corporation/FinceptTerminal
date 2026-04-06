// QuantLibClient.cpp — Shared QuantLib API HTTP client.

#include "services/quantlib/QuantLibClient.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "storage/cache/CacheManager.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

static constexpr int kRefDataTtlSec = 60 * 60; // 1 hour — static reference data

namespace fincept::services {

static constexpr const char* kQuantLibClientTag = "QuantLibClient";

const QString QuantLibClient::API_BASE = QStringLiteral("https://api.fincept.in");

// Endpoints that use GET (no request body).
static const QStringList GET_ENDPOINTS = {
    "core/types/currencies",
    "core/types/frequencies",
    "scheduling/calendar/list",
    "scheduling/daycount/conventions",
    "scheduling/adjustment/methods",
};

// Endpoints that take body fields as URL query params (POST with empty body).
static const QStringList QUERY_PARAM_ENDPOINTS = {
    "core/types/spread/from-bps",
};

bool QuantLibClient::is_get_endpoint(const QString& endpoint) {
    return GET_ENDPOINTS.contains(endpoint);
}

bool QuantLibClient::is_query_param_endpoint(const QString& endpoint) {
    return QUERY_PARAM_ENDPOINTS.contains(endpoint);
}

// ── Singleton ────────────────────────────────────────────────────────────────

QuantLibClient::QuantLibClient(QObject* parent) : QObject(parent) {}

QuantLibClient& QuantLibClient::instance() {
    static QuantLibClient inst;
    return inst;
}

// ── Request builder ──────────────────────────────────────────────────────────

static QNetworkRequest build_request(const QString& endpoint, const QJsonObject& query_params = {}) {
    QString url = QuantLibClient::API_BASE + "/quantlib/" + endpoint;

    if (!query_params.isEmpty()) {
        QStringList parts;
        for (auto it = query_params.begin(); it != query_params.end(); ++it)
            parts << (it.key() + "=" + it.value().toVariant().toString());
        url += "?" + parts.join("&");
    }

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "FinceptTerminal/4.0.0");

    auto& auth_mgr = auth::AuthManager::instance();
    if (auth_mgr.is_authenticated())
        req.setRawHeader("X-API-Key", auth_mgr.session().api_key.toUtf8());

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
    // Cache GET endpoints (static reference data) and query-param endpoints
    const bool cacheable = is_get_endpoint(endpoint) || is_query_param_endpoint(endpoint);
    if (cacheable) {
        QString cache_key = "quantlib:" + endpoint;
        if (!body.isEmpty())
            cache_key += ":" + QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact));

        const QVariant cached = fincept::CacheManager::instance().get(cache_key);
        if (!cached.isNull()) {
            auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
            mcp::ToolResult result = doc.isObject()
                ? mcp::ToolResult::ok_data(doc.object())
                : mcp::ToolResult::ok_data(doc.array());
            callback(result);
            return;
        }

        auto* nam = new QNetworkAccessManager(this);
        QNetworkReply* reply = nullptr;
        if (is_get_endpoint(endpoint)) {
            reply = nam->get(build_request(endpoint));
        } else {
            reply = nam->post(build_request(endpoint, body), QByteArray("{}"));
        }

        QPointer<QuantLibClient> self = this;
        connect(reply, &QNetworkReply::finished, this, [self, reply, nam, endpoint, cache_key, callback]() {
            reply->deleteLater();
            nam->deleteLater();
            if (!self) return;
            if (reply->error() != QNetworkReply::NoError) {
                LOG_ERROR(kQuantLibClientTag, "Network error on " + endpoint + ": " + reply->errorString());
                callback(mcp::ToolResult::fail(reply->errorString()));
                return;
            }
            int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            auto raw = reply->readAll();
            auto result = parse_response(http_status, raw);
            if (result.success) {
                // Serialize the data payload for caching
                QString to_cache;
                if (result.data.isObject())
                    to_cache = QString::fromUtf8(QJsonDocument(result.data.toObject()).toJson(QJsonDocument::Compact));
                else if (result.data.isArray())
                    to_cache = QString::fromUtf8(QJsonDocument(result.data.toArray()).toJson(QJsonDocument::Compact));
                if (!to_cache.isEmpty())
                    fincept::CacheManager::instance().put(cache_key, QVariant(to_cache), kRefDataTtlSec, "quantlib");
            }
            LOG_DEBUG(kQuantLibClientTag, QString("HTTP %1 — %2 — %3")
                               .arg(http_status).arg(endpoint)
                               .arg(result.success ? "OK" : result.error));
            callback(result);
        });
        return;
    }

    // Non-cacheable POST endpoints
    auto* nam = new QNetworkAccessManager(this);
    auto req = build_request(endpoint);
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = nam->post(req, data);

    QPointer<QuantLibClient> self = this;
    connect(reply, &QNetworkReply::finished, this, [self, reply, nam, endpoint, callback]() {
        reply->deleteLater();
        nam->deleteLater();

        if (!self) return;

        if (reply->error() != QNetworkReply::NoError) {
            LOG_ERROR(kQuantLibClientTag, "Network error on " + endpoint + ": " + reply->errorString());
            callback(mcp::ToolResult::fail(reply->errorString()));
            return;
        }

        int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto raw = reply->readAll();
        auto result = parse_response(http_status, raw);

        LOG_DEBUG(kQuantLibClientTag, QString("HTTP %1 — %2 — %3")
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
