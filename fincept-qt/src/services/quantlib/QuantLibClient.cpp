// QuantLibClient.cpp — HTTP client for the api.fincept.in/quantlib/* routes.

#include "services/quantlib/QuantLibClient.h"

#include "auth/AuthManager.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "core/telemetry/ClientInfo.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

namespace fincept::services {

namespace {

constexpr const char* kQuantLibClientTag = "QuantLibClient";
constexpr int kRefDataTtlSec = 60 * 60; // 1 hour — static reference data

// A hung connection otherwise leaves the screen computing forever (Qt has no
// default transfer timeout). The server's own calculation limit is shorter.
constexpr int kQuantLibTransferTimeoutMs = 60 * 1000;

// Bodies at least this large are parsed on a worker thread: QJsonDocument
// costs ~10-20 ms per MB, and a capped simulation answer runs to several MB.
constexpr qsizetype kOffThreadParseBytes = 256 * 1024;

// Set on a reply aborted by cancel(), to tell it apart from a transfer timeout
// (Qt reports both as an aborted transfer).
constexpr const char* kCancelledProperty = "ql_cancelled";

// Endpoints that use GET (no request body).
const QStringList& get_endpoints() {
    static const QStringList k = {
        "core/types/currencies",           "core/types/frequencies",        "scheduling/calendar/list",
        "scheduling/daycount/conventions", "scheduling/adjustment/methods",
    };
    return k;
}

// Endpoints that take body fields as URL query params (POST with empty body).
const QStringList& query_param_endpoints() {
    static const QStringList k = {
        "core/types/spread/from-bps",
        // Declared in the OpenAPI spec as `parameters: in: query` with no request body — a JSON body is
        // ignored by the server and every call 422'd on the missing `pd` / `pre_money`.
        "analysis/valuation/credit/expected-loss",
        "analysis/valuation/credit/rating-pd",
        "analysis/valuation/startup/dilution",
    };
    return k;
}

QString ql_compact(const QJsonObject& o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

// ── Request builder ──────────────────────────────────────────────────────────

QNetworkRequest ql_build_request(const QString& endpoint, const QJsonObject& query_params = {}) {
    QString url = fincept::AppConfig::instance().api_base_url() + "/quantlib/" + endpoint;

    if (!query_params.isEmpty()) {
        QStringList parts;
        for (auto it = query_params.begin(); it != query_params.end(); ++it)
            parts << (QString::fromLatin1(QUrl::toPercentEncoding(it.key())) + "=" +
                      QString::fromLatin1(QUrl::toPercentEncoding(it.value().toVariant().toString())));
        url += "?" + parts.join("&");
    }

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    telemetry::ClientInfo::apply_headers(req);
    req.setTransferTimeout(kQuantLibTransferTimeoutMs);
    // The QNAM is shared across calls; keep its cookie jar out of the exchange.
    req.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    req.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);

    auto& auth_mgr = auth::AuthManager::instance();
    if (auth_mgr.is_authenticated())
        req.setRawHeader("X-API-Key", auth_mgr.session().api_key.toUtf8());

    return req;
}

// ── Failure text ─────────────────────────────────────────────────────────────

// One FastAPI location ("body -> legs -> 0 -> rate") as a field path ("legs[0].rate").
QString ql_field_path(const QStringList& loc) {
    QString path;
    for (const QString& part : loc) {
        bool is_index = false;
        part.toInt(&is_index);
        if (is_index)
            path += QLatin1Char('[') + part + QLatin1Char(']');
        else
            path += (path.isEmpty() ? QString() : QStringLiteral(".")) + part;
    }
    return path;
}

QString ql_strip_value_error(QString msg) {
    // Model-level validators read "Value error, <text>".
    if (msg.startsWith(QLatin1String("Value error, ")))
        msg = msg.mid(13);
    return msg.trimmed();
}

// main.py turns a 422 into {"message": "body -> n: Input should be a valid integer; body -> x: Field
// required"}. One line per rejected field, named as the form names it: "n: Input should be ...".
QString ql_validation_lines(const QString& message) {
    static const QStringList kRoots = {"body", "query", "path", "header"};
    QStringList lines;
    for (const QString& raw_part : message.split(QStringLiteral("; "), Qt::SkipEmptyParts)) {
        const QString part = raw_part.trimmed();
        const int colon = part.indexOf(QStringLiteral(": "));
        QStringList loc = colon > 0 ? part.left(colon).split(QStringLiteral(" -> ")) : QStringList{};
        if (loc.isEmpty() || !kRoots.contains(loc.first())) {
            // Not a location: the rest of a message that itself contains "; " ("... per request; reduce one of
            // them."), or a free-text validator message.
            if (lines.isEmpty())
                lines << ql_strip_value_error(part);
            else
                lines.last() += QStringLiteral("; ") + part;
            continue;
        }
        loc.removeFirst();
        const QString field = ql_field_path(loc);
        const QString msg = ql_strip_value_error(part.mid(colon + 2));
        lines << (field.isEmpty() ? msg : field + QStringLiteral(": ") + msg);
    }
    if (lines.size() <= 1)
        return lines.isEmpty() ? message : lines.first();
    for (QString& l : lines)
        l.prepend(QStringLiteral("• "));
    return lines.join(QLatin1Char('\n'));
}

struct QlServerMessage {
    QString text;
    QString error_code;
};

// The server's own explanation from an error body (main.py's {"success": false, "message", "error_code"};
// FastAPI's default {"detail": ...} when a proxy or another app answers).
QlServerMessage ql_server_message(const QByteArray& raw) {
    QlServerMessage out;
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return out;
    const QJsonObject obj = doc.object();
    out.error_code = obj.value("error_code").toString();

    const QJsonValue detail = obj.value("detail");
    if (detail.isArray()) {
        QStringList parts;
        for (const auto& item : detail.toArray()) {
            const QJsonObject d = item.toObject();
            QStringList loc;
            for (const auto& l : d.value("loc").toArray())
                loc << l.toVariant().toString();
            parts << (loc.join(QStringLiteral(" -> ")) + QStringLiteral(": ") + d.value("msg").toString());
        }
        out.text = parts.join(QStringLiteral("; "));
        return out;
    }
    if (detail.isString()) {
        out.text = detail.toString();
        return out;
    }
    // Auth / billing refusals carry {"message": {"message": "...", "hint": ...}}.
    const QJsonValue mv = obj.value("message");
    out.text = mv.isObject() ? mv.toObject().value("message").toString() : mv.toString();
    return out;
}

QString ql_network_text(QNetworkReply::NetworkError err, const QNetworkReply* reply) {
    switch (err) {
        case QNetworkReply::HostNotFoundError:
            return QuantLibClient::tr("Cannot reach the Fincept API: %1 could not be resolved. Check the internet "
                                      "connection.")
                .arg(reply->url().host());
        case QNetworkReply::ConnectionRefusedError:
            return QuantLibClient::tr("The Fincept API refused the connection — the service may be restarting.");
        case QNetworkReply::RemoteHostClosedError:
            return QuantLibClient::tr("The connection to the Fincept API closed before an answer arrived.");
        case QNetworkReply::SslHandshakeFailedError:
            return QuantLibClient::tr("The secure (TLS) connection to the Fincept API could not be established.");
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::NetworkSessionFailedError:
        case QNetworkReply::UnknownNetworkError:
            return QuantLibClient::tr("The network connection is unavailable.");
        case QNetworkReply::ProxyConnectionRefusedError:
        case QNetworkReply::ProxyConnectionClosedError:
        case QNetworkReply::ProxyNotFoundError:
        case QNetworkReply::ProxyTimeoutError:
        case QNetworkReply::ProxyAuthenticationRequiredError:
            return QuantLibClient::tr("The proxy server refused or dropped the connection to the Fincept API.");
        default:
            break;
    }
    return reply->errorString();
}

QString ql_status_text(int status) {
    switch (status) {
        case 401:
            return QuantLibClient::tr("Sign-in required.");
        case 402:
            return QuantLibClient::tr("Not enough credits for this call.");
        case 403:
            return QuantLibClient::tr("Your plan does not include this endpoint.");
        case 429:
            return QuantLibClient::tr("Too many requests.");
        case 503:
            return QuantLibClient::tr("The QuantLib service is busy.");
        case 504:
            return QuantLibClient::tr("The calculation took longer than the server allows.");
        default:
            break;
    }
    if (status >= 500)
        return QuantLibClient::tr("The QuantLib service failed while computing this request (HTTP %1).").arg(status);
    return QuantLibClient::tr("The server answered HTTP %1.").arg(status);
}

mcp::ToolResult ql_failure(const QNetworkReply* reply, int status, const QByteArray& body, bool cancelled,
                           bool timed_out) {
    QJsonObject info{{"http_status", status}};
    QString text;
    if (cancelled) {
        info["cancelled"] = true;
        text = QuantLibClient::tr("Cancelled.");
    } else if (status == 0) {
        info["network_error"] = int(reply->error());
        if (timed_out) {
            info["timed_out"] = true;
            text = QuantLibClient::tr("No answer from the QuantLib service within %1 seconds.")
                       .arg(QuantLibClient::transfer_timeout_seconds());
        } else {
            text = ql_network_text(reply->error(), reply);
        }
    } else {
        const QlServerMessage server = ql_server_message(body);
        if (!server.error_code.isEmpty())
            info["error_code"] = server.error_code;
        if (status == 404)
            text = QuantLibClient::tr("The server does not offer this endpoint (HTTP 404).");
        else if (status == 422 && !server.text.isEmpty())
            text = ql_validation_lines(server.text);
        else
            text = server.text.trimmed().isEmpty() ? ql_status_text(status) : server.text.trimmed();
    }
    auto fail = mcp::ToolResult::fail(text);
    fail.data = info;
    return fail;
}

// Every call goes to the Fincept API through this client's own QNAM, so
// HttpClient never sees these replies. Report a refusal the way it would: a
// 401 lets AuthManager re-validate the key (and sign out if it was revoked), a
// 403 IP block pauses the app's pollers. (No X-Session-Token is sent here, so
// a 401 is always about the key.)
void ql_report_refusal(const QNetworkReply* reply, int http_status, const QByteArray& body) {
    if (http_status == 401) {
        auth::AuthManager::instance().handle_unauthorized(
            QString::fromUtf8(reply->request().rawHeader("X-Session-Token")),
            reply->rawHeader("X-Session-Expired").trimmed().compare("true", Qt::CaseInsensitive) == 0);
    } else if (http_status == 403) {
        fincept::HttpClient::instance().report_forbidden_response(body);
    }
}

// Billed answers (a Free plan's 1-credit call) and 402 refusals carry the
// balance after the charge — keep the header credit chip current, exactly as
// HttpClient::inspect_credit_headers() does for its own replies.
void ql_report_credits(const QNetworkReply* reply, int http_status) {
    const bool billed_or_refused = (http_status >= 200 && http_status < 300) || http_status == 402;
    if (!billed_or_refused || !reply->hasRawHeader("X-Credits-Cost") || !reply->hasRawHeader("X-Credits-Balance"))
        return;
    auto& auth_mgr = auth::AuthManager::instance();
    if (!auth_mgr.is_authenticated() || reply->request().rawHeader("X-API-Key") != auth_mgr.session().api_key.toUtf8())
        return; // sent with another account's key
    bool ok = false;
    const double balance = reply->rawHeader("X-Credits-Balance").trimmed().toDouble(&ok);
    if (ok)
        auth_mgr.report_credit_balance(balance);
}

} // namespace

bool QuantLibClient::is_get_endpoint(const QString& endpoint) {
    return get_endpoints().contains(endpoint);
}

bool QuantLibClient::is_query_param_endpoint(const QString& endpoint) {
    return query_param_endpoints().contains(endpoint);
}

int QuantLibClient::transfer_timeout_seconds() {
    return kQuantLibTransferTimeoutMs / 1000;
}

// ── Singleton ────────────────────────────────────────────────────────────────

QuantLibClient::QuantLibClient(QObject* parent) : QObject(parent) {}

QuantLibClient& QuantLibClient::instance() {
    static QuantLibClient inst;
    return inst;
}

QNetworkAccessManager* QuantLibClient::network() {
    // Created lazily on this object's thread (call() guarantees that), owned via
    // parent: one HTTP thread and one keep-alive connection pool for every call.
    if (!nam_)
        nam_ = new QNetworkAccessManager(this);
    return nam_;
}

// ── Response parser ──────────────────────────────────────────────────────────

mcp::ToolResult QuantLibClient::parse_response(int http_status, const QByteArray& raw) {
    QJsonParseError parse_err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parse_err);

    auto unreadable = [http_status]() {
        auto fail = mcp::ToolResult::fail(
            tr("The QuantLib service sent an unreadable answer (HTTP %1).").arg(http_status));
        fail.data = QJsonObject{{"http_status", http_status}, {"unreadable", true}};
        return fail;
    };
    if (parse_err.error != QJsonParseError::NoError)
        return unreadable();

    if (!doc.isObject()) {
        if (doc.isArray())
            return mcp::ToolResult::ok_data(doc.array());
        return unreadable();
    }

    const QJsonObject obj = doc.object();

    // API envelope: {"success": bool, "message": "...", "data": <payload>}
    if (obj.contains("success")) {
        if (!obj["success"].toBool()) {
            // HTTP 200 with success == false: the handler's own refusal.
            QString msg = obj["message"].isObject() ? obj["message"].toObject().value("message").toString()
                                                    : obj["message"].toString();
            if (msg.trimmed().isEmpty())
                msg = tr("The calculation failed without an explanation (HTTP %1).").arg(http_status);
            auto fail = mcp::ToolResult::fail(msg);
            fail.data = QJsonObject{{"http_status", http_status}};
            return fail;
        }
        // A success envelope without "data" is an empty result, not "undefined".
        const QJsonValue payload = obj.contains("data") ? obj.value("data") : QJsonValue(QJsonObject{});
        return mcp::ToolResult::ok(obj.value("message").toString(), payload);
    }

    // No envelope — return raw object
    return mcp::ToolResult::ok_data(obj);
}

// ── Async call ───────────────────────────────────────────────────────────────

quint64 QuantLibClient::call(const QString& endpoint, const QJsonObject& body, QuantLibCallback callback) {
    // The shared QNAM lives on this object's thread and must only be used there.
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, [this, endpoint, body, callback]() { call(endpoint, body, callback); }, Qt::QueuedConnection);
        return 0;
    }

    const bool get = is_get_endpoint(endpoint);
    const bool query = is_query_param_endpoint(endpoint);

    // GET endpoints (static reference data) and query-param endpoints are cached.
    QString cache_key;
    if (get || query) {
        cache_key = "quantlib:" + endpoint;
        if (!body.isEmpty())
            cache_key += ":" + ql_compact(body);
        const QVariant cached = fincept::CacheManager::instance().get(cache_key);
        if (!cached.isNull()) {
            const QJsonDocument doc = QJsonDocument::fromJson(cached.toString().toUtf8());
            callback(doc.isObject() ? mcp::ToolResult::ok_data(doc.object()) : mcp::ToolResult::ok_data(doc.array()));
            return 0;
        }
    }

    QNetworkReply* reply = nullptr;
    if (get)
        reply = network()->get(ql_build_request(endpoint));
    else if (query)
        reply = network()->post(ql_build_request(endpoint, body), QByteArray("{}"));
    else
        reply = network()->post(ql_build_request(endpoint), QJsonDocument(body).toJson(QJsonDocument::Compact));

    const quint64 id = ++next_id_;
    in_flight_.insert(id, reply);
    connect(reply, &QNetworkReply::finished, this,
            [this, id, reply, endpoint, cache_key, callback]() { on_finished(id, reply, endpoint, cache_key, callback); });
    return id;
}

void QuantLibClient::cancel(quint64 request_id) {
    const auto it = in_flight_.constFind(request_id);
    if (it == in_flight_.constEnd())
        return;
    const QPointer<QNetworkReply> reply = it.value();
    if (reply && reply->isRunning()) {
        reply->setProperty(kCancelledProperty, true);
        reply->abort(); // finished() → on_finished() → callback with cancelled == true
    }
}

void QuantLibClient::on_finished(quint64 id, QNetworkReply* reply, const QString& endpoint, const QString& cache_key,
                                 const QuantLibCallback& callback) {
    in_flight_.remove(id);
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->isOpen() ? reply->readAll() : QByteArray(); // an aborted reply is closed
    ql_report_credits(reply, status);

    if (reply->error() != QNetworkReply::NoError) {
        const bool cancelled = reply->property(kCancelledProperty).toBool();
        // Qt reports an expired transfer timeout as an aborted transfer.
        const bool timed_out = !cancelled && status == 0 &&
                               (reply->error() == QNetworkReply::OperationCanceledError ||
                                reply->error() == QNetworkReply::TimeoutError);
        if (!cancelled)
            ql_report_refusal(reply, status, raw);
        mcp::ToolResult fail = ql_failure(reply, status, raw, cancelled, timed_out);
        if (cancelled)
            LOG_DEBUG(kQuantLibClientTag, "Cancelled " + endpoint);
        else
            LOG_ERROR(kQuantLibClientTag, QString("%1 failed (HTTP %2): %3").arg(endpoint).arg(status).arg(fail.error));
        callback(fail);
        return;
    }

    auto deliver = [endpoint, cache_key, callback, status](const mcp::ToolResult& result) {
        if (result.success && !cache_key.isEmpty()) {
            QString to_cache;
            if (result.data.isObject())
                to_cache = ql_compact(result.data.toObject());
            else if (result.data.isArray())
                to_cache = QString::fromUtf8(QJsonDocument(result.data.toArray()).toJson(QJsonDocument::Compact));
            if (!to_cache.isEmpty())
                fincept::CacheManager::instance().put(cache_key, QVariant(to_cache), kRefDataTtlSec, "quantlib");
        }
        LOG_DEBUG(kQuantLibClientTag,
                  QString("HTTP %1 — %2 — %3").arg(status).arg(endpoint).arg(result.success ? "OK" : result.error));
        callback(result);
    };

    if (raw.size() < kOffThreadParseBytes) {
        deliver(parse_response(status, raw));
        return;
    }
    // A multi-MB answer (capped simulations, long series) is parsed on a worker
    // thread so the GUI keeps painting; the callback still runs on this thread.
    auto* watcher = new QFutureWatcher<mcp::ToolResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [watcher, deliver]() {
        const mcp::ToolResult result = watcher->result();
        watcher->deleteLater();
        deliver(result);
    });
    watcher->setFuture(QtConcurrent::run([status, raw]() { return parse_response(status, raw); }));
}

} // namespace fincept::services
