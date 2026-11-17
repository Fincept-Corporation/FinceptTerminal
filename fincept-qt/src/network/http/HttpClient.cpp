#include "network/http/HttpClient.h"

#include "core/logging/Logger.h"
#include "core/telemetry/ClientInfo.h"
#include "core/telemetry/UsageReporter.h"
#include "network/http/HttpOffThread.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <chrono>

namespace fincept {

namespace {
/// URL with the query string removed, for logging.
///
/// Several callers put a live secret in the query because HttpClient's auth
/// headers are shared/global and cannot be set per request — Opsgenie
/// (`?apiKey=`), Gotify (`?token=`), ntfy (`?auth=`), and any saved data
/// mapping whose template carries `&apikey=`/`&token=`. Logging the raw URL
/// therefore writes those secrets to fincept.log in plaintext at the default
/// log level. The error path in handle_reply() already strips the query for
/// exactly this reason; the request path must do the same. Fixed here, once,
/// rather than at each call site.
///
/// The query is not the only place a secret lives: a Telegram bot token is in
/// the *path* (`/bot<token>/sendMessage`), and Slack/Discord/Teams/Mattermost
/// webhook paths are themselves the credential. So for any host other than the
/// Fincept API host only `scheme://host[:port]/<redacted>` is logged; the path
/// of a Fincept API call is kept (it carries ids, never secrets). User-info
/// (`https://user:pass@host`) is always dropped.
QString http_log_url(const QString& url, const QString& base_url) {
    const QUrl parsed(url);
    const QString host = parsed.host();
    if (host.isEmpty()) {
        // A relative Fincept API path ("/user/login?x=1"); anything else with no
        // host is not a URL we can reason about — never echo it.
        if (!url.startsWith(QLatin1Char('/')))
            return QStringLiteral("<invalid url>");
        const int q = static_cast<int>(url.indexOf(QLatin1Char('?')));
        return q < 0 ? url : url.left(q) + QStringLiteral("?<redacted>");
    }

    const QString base_host = QUrl(base_url).host();
    if (base_host.isEmpty() || host.compare(base_host, Qt::CaseInsensitive) != 0) {
        return parsed.adjusted(QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
                   .toString() +
               QStringLiteral("/<redacted>");
    }

    const QUrl stripped = parsed.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
    return parsed.hasQuery() ? stripped.toString() + QStringLiteral("?<redacted>") : stripped.toString();
}

/// True for an absolute http(s) URL (the only absolute form HttpClient sends).
bool http_is_absolute_url(const QString& url) {
    return url.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) ||
           url.startsWith(QLatin1String("https://"), Qt::CaseInsensitive);
}

/// A request is only well-formed if it is an absolute http(s) URL or a Fincept
/// API path ("/user/login"). build_request() resolves a relative path against the
/// Fincept base URL and attaches the user's X-API-Key / X-Session-Token to it, so
/// a scheme-less third-party target ("hooks.slack.com/services/..." typed
/// without "https://") must NOT fall into that branch — it would POST the
/// webhook payload plus the user's Fincept credentials to the Fincept backend.
bool http_is_sendable_url(const QString& url) {
    return http_is_absolute_url(url) || url.startsWith(QLatin1Char('/'));
}

/// Deliver a "bad URL" failure through the same async callback path as a real
/// reply (queued, never re-entrant into the caller). Nothing about the URL is
/// echoed: it may be a webhook secret.
void http_reject_bad_url(const QObject* receiver, HttpClient::JsonCallback callback) {
    LOG_WARN("HTTP", "Rejected request: URL must be absolute http(s):// or an API path starting with '/'");
    QTimer::singleShot(0, receiver, [cb = std::move(callback)]() {
        cb(Result<QJsonDocument>::err("Invalid URL: expected http:// or https:// (or an API path starting with '/')"));
    });
}

/// Longest server explanation passed on to the UI. Fincept messages are one or
/// two sentences; anything longer is a dumped page or trace and is cut.
constexpr qsizetype kHttpMaxServerText = 300;

/// A human-readable string held by `v`: the string itself, or — for the
/// Fincept envelope, where an HTTPException detail dict lands under "message"
/// (`{"error": "<code>", "message": "<text>", "hint": ...}`) — its "message"
/// or "detail" field. Empty when there is none.
QString http_text_of(const QJsonValue& v) {
    if (v.isString())
        return v.toString().trimmed();
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        for (const auto* key : {"message", "detail"}) {
            const QJsonValue inner = o.value(QLatin1String(key));
            if (inner.isString() && !inner.toString().trimmed().isEmpty())
                return inner.toString().trimmed();
        }
    }
    return {};
}

/// The Fincept API refuses the client IP: 403 {"error_code": "IP_BLOCKED"}
/// (apiback main.py request_middleware). Never an account problem.
bool http_body_is_ip_blocked(const QJsonObject& body) {
    return body.value(QLatin1String("error_code")).toString() == QLatin1String("IP_BLOCKED");
}

/// 401 from require_auth because the X-Session-Token we sent is no longer the
/// account's (signed in elsewhere, password reset): the response carries
/// `X-Session-Expired: true` and message.error == "session_invalidated"
/// (apiback auth.py require_auth). Either marker is enough.
bool http_is_session_expired(const QNetworkReply* reply, const QJsonObject& body) {
    if (reply->rawHeader("X-Session-Expired").trimmed().toLower() == "true")
        return true;
    const QJsonValue m = body.value(QLatin1String("message"));
    return m.isObject() &&
           m.toObject().value(QLatin1String("error")).toString() == QLatin1String("session_invalidated");
}

/// `Retry-After: <seconds>` of a response, 0 when absent or not in that form.
/// The IP_BLOCKED answer does not send one today; honoured if it ever does.
int http_retry_after_seconds(const QNetworkReply* reply) {
    bool ok = false;
    const int s = reply->rawHeader("Retry-After").trimmed().toInt(&ok);
    return ok && s > 0 ? s : 0;
}

/// HTTP verb of a reply, for log lines.
const char* http_method_name(const QNetworkReply* reply) {
    switch (reply->operation()) {
        case QNetworkAccessManager::GetOperation:
            return "GET";
        case QNetworkAccessManager::PostOperation:
            return "POST";
        case QNetworkAccessManager::PutOperation:
            return "PUT";
        case QNetworkAccessManager::DeleteOperation:
            return "DELETE";
        case QNetworkAccessManager::HeadOperation:
            return "HEAD";
        default:
            return "OTHER";
    }
}

/// User-facing text for an IP block: the server's own sentence (it says only
/// "temporarily … try again later"), plus how long it lasts — Retry-After when
/// the server sends it, otherwise the server's fixed durations (apiback auth.py:
/// 30 min, 24 h after repeated failures).
QString http_ip_block_text(const QString& server_text, int retry_after_s) {
    const QString base = server_text.isEmpty()
                             ? HttpClient::tr("The Fincept server has temporarily blocked your IP address.")
                             : server_text;
    if (retry_after_s > 0) {
        const int minutes = (retry_after_s + 59) / 60;
        return base + QLatin1Char(' ') +
               HttpClient::tr("This is a temporary network block, not a problem with your account: it lifts in "
                              "about %n minute(s).", nullptr, minutes);
    }
    return base + QLatin1Char(' ') +
           HttpClient::tr("This is a temporary network block, not a problem with your account: it usually lifts "
                          "within 30 minutes (up to 24 hours if it repeats).");
}

/// Error text for a failed response: `"HTTP_<status>: <server message>"`, or a
/// bare `"HTTP_<status>"` when the body carries nothing usable. Keeping the
/// status as a machine-readable prefix lets callers keep switching on it (see
/// HttpClient::status_from_error) while the server's own wording still reaches
/// the user — for 401/403 too: "Incorrect password. You have 2 attempt(s)
/// remaining", "Your account is not verified yet", "Invalid password".
std::string http_server_message(const QJsonObject& body, int status, int retry_after_s) {
    const QString prefix = QStringLiteral("HTTP_%1").arg(status);
    QString msg = HttpClient::error_text_from_body(body);
    if (status == 403 && http_body_is_ip_blocked(body))
        msg = http_ip_block_text(msg, retry_after_s);
    if (msg.isEmpty())
        return prefix.toStdString();
    return (prefix + QStringLiteral(": ") + msg).toStdString();
}

/// Case-insensitive lookup of one header name in a caller's extra headers.
const QByteArray* http_find_header(const HttpClient::Headers& headers, const char* name) {
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        if (it.key().compare(name, Qt::CaseInsensitive) == 0)
            return &it.value();
    }
    return nullptr;
}

bool http_is_auth_header(const QByteArray& name) {
    return name.compare("X-API-Key", Qt::CaseInsensitive) == 0 ||
           name.compare("X-Session-Token", Qt::CaseInsensitive) == 0;
}

/// The reply whose callback is running on this thread — what response_status() /
/// response_header() / error_header() read.
struct HttpClientCurrentReply {
    const QList<QNetworkReply::RawHeaderPair>* headers = nullptr;
    int status = 0;
    bool http_error = false; ///< the callback receives an "HTTP_<status>" error
};
/// thread_local because a callback runs on its context object's thread, which
/// need not be this client's.
thread_local HttpClientCurrentReply http_current_reply_tls;

/// Publishes one reply for the duration of its callback. Saves and restores the
/// previous one, so a callback that spins a nested event loop (a modal dialog)
/// in which another reply is delivered stays correct.
class HttpClientReplyScope {
  public:
    HttpClientReplyScope(const QList<QNetworkReply::RawHeaderPair>* headers, int status, bool http_error)
        : previous_(http_current_reply_tls) {
        http_current_reply_tls = HttpClientCurrentReply{headers, status, http_error};
    }
    ~HttpClientReplyScope() { http_current_reply_tls = previous_; }
    HttpClientReplyScope(const HttpClientReplyScope&) = delete;
    HttpClientReplyScope& operator=(const HttpClientReplyScope&) = delete;

  private:
    HttpClientCurrentReply previous_;
};

QByteArray http_current_header(const char* name) {
    if (!http_current_reply_tls.headers || !name)
        return {};
    for (const auto& pair : *http_current_reply_tls.headers) {
        if (pair.first.compare(name, Qt::CaseInsensitive) == 0)
            return pair.second;
    }
    return {};
}

/// The JsonCallback a raw form (post_raw(), put_raw(), …) hands to handle_reply():
/// errors take the JSON path, exactly as for the plain verb (same strings, and
/// response_status() / response_header() readable for an HTTP error); only the
/// result type changes on the way out (as get_raw() does).
HttpClient::JsonCallback http_raw_error_path(HttpClient::BytesCallback callback) {
    return [callback = std::move(callback)](Result<QJsonDocument> result) {
        if (result.is_err())
            callback(Result<QByteArray>::err(result.error()));
        else
            callback(Result<QByteArray>::ok(QByteArray()));
    };
}

/// The log line for a 2xx body that is not JSON (the caller gets the parse error):
/// an empty body at DEBUG, anything else WARN, once a minute per endpoint. Any thread.
void http_log_unparsable(const QString& safe_url, int status, qsizetype bytes, const QString& error) {
    if (bytes == 0) {
        LOG_DEBUG("HTTP", logf("Response body is empty", {{"url", safe_url}, {"status", status}}));
        return;
    }
    LOG_WARN_RL("HTTP", QStringLiteral("nojson|") + safe_url, 60000,
                logf("Response body is not valid JSON",
                     {{"url", safe_url}, {"status", status}, {"bytes", bytes}, {"error", error}}));
}
} // namespace

Result<QJsonDocument> HttpClient::parse_body(const QByteArray& body) {
    http_off_thread::ParsedBody parsed = http_off_thread::parse_body(body);
    if (!parsed.ok)
        return Result<QJsonDocument>::err(parsed.error.toStdString());
    return Result<QJsonDocument>::ok(std::move(parsed.doc));
}

int HttpClient::response_status() {
    return http_current_reply_tls.status;
}

QByteArray HttpClient::response_header(const char* name) {
    return http_current_header(name);
}

QByteArray HttpClient::error_header(const char* name) {
    return http_current_reply_tls.http_error ? http_current_header(name) : QByteArray();
}

qint64 HttpClient::poll_interval_ms(const QByteArray& header_value) {
    bool ok = false;
    const qint64 s = header_value.trimmed().toLongLong(&ok);
    if (!ok || s <= 0)
        return 0;
    return std::min<qint64>(s, kMaxPollIntervalMs / 1000) * 1000;
}

QString HttpClient::error_text_from_body(const QJsonObject& body) {
    // "message" first: Fincept puts the readable sentence there and a machine
    // code in "error" (429 plan limits: "concurrent_limit_exceeded" vs "Too many
    // simultaneous requests: …"). Most third-party APIs follow the same split.
    QString msg;
    for (const auto* key : {"message", "error", "detail"}) {
        msg = http_text_of(body.value(QLatin1String(key)));
        if (!msg.isEmpty())
            break;
    }

    // Pydantic/FastAPI field-level validation errors (typically 422).
    if (msg.isEmpty() && body.value(QLatin1String("detail")).isArray()) {
        QStringList parts;
        for (const auto& item : body.value(QLatin1String("detail")).toArray()) {
            const QJsonObject entry = item.toObject();
            const QString m = entry.value(QLatin1String("msg")).toString().trimmed();
            if (m.isEmpty())
                continue;
            // loc is ["body", "field_name"] — take the last meaningful segment.
            QString field;
            const QJsonArray loc = entry.value(QLatin1String("loc")).toArray();
            for (qsizetype i = loc.size() - 1; i >= 0; --i) {
                QString seg = loc.at(i).toString();
                if (seg.isEmpty() || seg == QLatin1String("body"))
                    continue;
                seg[0] = seg[0].toUpper();
                seg.replace('_', ' ');
                field = seg;
                break;
            }
            parts << (field.isEmpty() ? m : field + QStringLiteral(": ") + m);
        }
        msg = parts.join(QStringLiteral("; "));
    }

    if (msg.size() > kHttpMaxServerText)
        msg = msg.left(kHttpMaxServerText - 1).trimmed() + QChar(0x2026);
    return msg;
}

int HttpClient::status_from_error(const std::string& error) {
    const QString e = QString::fromStdString(error);
    if (!e.startsWith(QLatin1String("HTTP_")))
        return 0;
    const qsizetype colon = e.indexOf(QLatin1Char(':'), 5);
    const QString digits = (colon < 0) ? e.mid(5) : e.mid(5, colon - 5);
    bool ok = false;
    const int status = digits.toInt(&ok);
    return ok ? status : 0;
}

QString HttpClient::message_from_error(const std::string& error) {
    const QString e = QString::fromStdString(error);
    if (!e.startsWith(QLatin1String("HTTP_")))
        return e; // transport / JSON-parse failures carry their own text
    const qsizetype colon = e.indexOf(QLatin1Char(':'), 5);
    if (colon < 0)
        return {};
    return e.mid(colon + 1).trimmed();
}

HttpClient& HttpClient::instance() {
    static HttpClient s;
    return s;
}

HttpClient::HttpClient() {
    nam_ = new QNetworkAccessManager(this);
    // Qt disables the transfer timeout by default: a half-open TCP connection
    // never emits finished(), so the callback never runs, nothing is logged and
    // there is no cancel path — the only recovery is restarting the app. Every
    // other network site in this codebase sets one (BrokerHttp, NewsService, …).
    nam_->setTransferTimeout(std::chrono::seconds{20});

    parse_pool_ = new QThreadPool(this);
    parse_pool_->setObjectName(QStringLiteral("HttpClientParse"));
    parse_pool_->setMaxThreadCount(2);

    ip_block_timer_.setSingleShot(true);
    ip_block_timer_.setInterval(kIpBlockWindowMs);
    connect(&ip_block_timer_, &QTimer::timeout, this, [this]() {
        if (!ip_blocked_)
            return;
        ip_blocked_ = false;
        LOG_INFO("HTTP", "No IP_BLOCKED answer from the Fincept API for 30 min — clearing the block state");
        emit ip_block_changed(false, {});
    });
}

bool HttpClient::is_fincept_url(const QUrl& url) const {
    const QUrl base(base_url_);
    return !base.host().isEmpty() && url.host().compare(base.host(), Qt::CaseInsensitive) == 0;
}

QNetworkRequest HttpClient::build_request(const QString& url, const Headers& extra_headers) const {
    const bool is_relative = !http_is_absolute_url(url);
    const QString full_url = is_relative ? (base_url_ + url) : url;
    QUrl qurl(full_url);
    QNetworkRequest req{qurl};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "FinceptTerminal/4.0");

    // Only attach auth on same-host requests — third-party absolute URLs (Slack/Discord/webhooks)
    // share this singleton and must NOT receive X-API-Key / X-Session-Token.
    const bool same_host = is_relative || is_fincept_url(qurl);

    // A caller-supplied credential replaces the default one; an empty value
    // means "omit it" (AuthApi validates the API key without the session token,
    // and checks a saved session token before adopting it).
    if (same_host) {
        telemetry::ClientInfo::apply_headers(req); // descriptive User-Agent + install / run ids
        if (!api_key_.isEmpty() && !http_find_header(extra_headers, "X-API-Key"))
            req.setRawHeader("X-API-Key", api_key_.toUtf8());
        if (!session_token_.isEmpty() && !http_find_header(extra_headers, "X-Session-Token"))
            req.setRawHeader("X-Session-Token", session_token_.toUtf8());
    }

    // Applied last so a caller can override any default above for this one
    // request without touching the shared singleton's auth state.
    for (auto it = extra_headers.constBegin(); it != extra_headers.constEnd(); ++it) {
        if (it.value().isEmpty() && http_is_auth_header(it.key()))
            continue;
        req.setRawHeader(it.key(), it.value());
    }
    return req;
}

void HttpClient::set_ip_blocked(const QString& message, int retry_after_s) {
    // Each IP_BLOCKED answer restarts the window, so the state stays on for as
    // long as the server keeps refusing us and turns off once it has not done so
    // for the block length (Retry-After when sent, else 30 min).
    ip_block_timer_.start(retry_after_s > 0 ? std::min(retry_after_s, 24 * 3600) * 1000 : kIpBlockWindowMs);
    if (ip_blocked_)
        return;
    ip_blocked_ = true;
    LOG_WARN("HTTP", "The Fincept API has temporarily blocked this IP address (403 IP_BLOCKED). "
                     "This is not an account problem; requests resume when the block lifts.");
    emit ip_block_changed(true, message);
}

bool HttpClient::report_forbidden_response(const QByteArray& response_body) {
    const QJsonDocument doc = QJsonDocument::fromJson(response_body);
    if (!doc.isObject() || !http_body_is_ip_blocked(doc.object()))
        return false;
    const QString text = http_ip_block_text(error_text_from_body(doc.object()), 0);
    if (QThread::currentThread() == thread())
        set_ip_blocked(text, 0);
    else // the window timer and the signal belong to this object's thread
        QMetaObject::invokeMethod(this, [this, text]() { set_ip_blocked(text, 0); }, Qt::QueuedConnection);
    return true;
}

void HttpClient::inspect_auth_failure(QNetworkReply* reply) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 401 && status != 403)
        return;
    if (!is_fincept_url(reply->url()))
        return; // a third party's 401/403 says nothing about the Fincept session

    // Look at the body without consuming it — the caller's handler, connected
    // after this one, still reads all of it. Error bodies here are a few hundred
    // bytes; anything large is not one of the shapes we look for.
    QJsonObject body;
    const qint64 avail = reply->bytesAvailable();
    if (avail > 0 && avail <= 16 * 1024) {
        const QJsonDocument doc = QJsonDocument::fromJson(reply->peek(avail));
        if (doc.isObject())
            body = doc.object();
    }

    if (status == 403) {
        if (http_body_is_ip_blocked(body)) {
            const int retry_after_s = http_retry_after_seconds(reply);
            set_ip_blocked(http_ip_block_text(error_text_from_body(body), retry_after_s), retry_after_s);
        }
        return;
    }

    // 401. Only the credentials this request actually carried matter: a reply to
    // a request sent with an older key/token (before a re-login, a key rotation or
    // a previous rejection) must not act on the current ones.
    const QNetworkRequest& sent = reply->request();
    const QByteArray sent_token = sent.rawHeader("X-Session-Token");
    const QByteArray sent_key = sent.rawHeader("X-API-Key");

    if (http_is_session_expired(reply, body)) {
        if (sent_token.isEmpty() || session_token_.isEmpty() || sent_token != session_token_.toUtf8())
            return; // already dropped (concurrent 401s), replaced, or a caller-chosen token
        // Stop sending the dead token right now, before any caller reacts and
        // re-requests — every further 401 counts toward the server's IP ban.
        const QString dead = session_token_;
        session_token_.clear();
        // AuthManager writes the event line ("Session token rejected by the server").
        LOG_DEBUG("HTTP", "Session token rejected (401 session_invalidated: signed in elsewhere or password reset) "
                          "— continuing with the API key only");
        emit session_token_rejected(dead);
        return;
    }

    if (!sent_key.isEmpty() && !api_key_.isEmpty() && sent_key == api_key_.toUtf8())
        emit api_key_rejected();
}

void HttpClient::inspect_credit_headers(QNetworkReply* reply) {
    // main.py attaches X-Credits-Balance to every authenticated response, but only
    // a billed one (X-Credits-Cost) carries the fresh post-charge figure — the
    // others repeat a cached snapshot that can be minutes old. A 402 refusal now
    // carries both too (cost 0, balance = the one that was refused), so the
    // header chip shows the real balance at the moment of refusal. An older
    // server sends no X-Credits-Cost on 402, which is skipped as before.
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool billed_or_refused = (status >= 200 && status < 300) || status == 402;
    if (!billed_or_refused || !reply->hasRawHeader("X-Credits-Cost") || !reply->hasRawHeader("X-Credits-Balance"))
        return;
    if (!is_fincept_url(reply->url()) || api_key_.isEmpty() ||
        reply->request().rawHeader("X-API-Key") != api_key_.toUtf8())
        return; // another account's balance (sent before a sign-out / key change)
    bool ok = false;
    const double balance = reply->rawHeader("X-Credits-Balance").trimmed().toDouble(&ok);
    if (ok)
        emit credit_balance_reported(balance);
}

void HttpClient::handle_reply(QNetworkReply* reply, JsonCallback callback, const QObject* context, Resend resend,
                              BodyMode mode, BytesCallback raw_callback) {
    // Connect to caller-provided context so caller destruction auto-disconnects before the lambda fires.
    const QObject* receiver = context ? context : static_cast<const QObject*>(this);
    // Safety net: guarantee the reply is always deleted. If `context` is
    // destroyed before the reply finishes, Qt auto-disconnects the callback
    // lambda below and its reply->deleteLater() would never run → the reply
    // leaks until app exit. deleteLater() is idempotent, so a double-call when
    // the lambda also runs is harmless.
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    // Session/IP-block bookkeeping. Connected BEFORE the caller's handler (slots
    // run in connection order) and scoped to this singleton, so it runs even if
    // the caller is gone and has always dropped a dead token by the time any
    // caller reacts to its 401.
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        inspect_credit_headers(reply);
        inspect_auth_failure(reply);
    });
    connect(reply, &QNetworkReply::finished, receiver,
            [this, reply, cb = std::move(callback), raw_cb = std::move(raw_callback), receiver, context,
             resend = std::move(resend), base_url = base_url_, mode]() {
        reply->deleteLater();

        QByteArray data = reply->readAll();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // The *_large() and *_raw() forms: a successful (possibly huge) body never
        // meets QJsonDocument::fromJson on this thread. Errors and 304 take the ordinary
        // path below, unchanged.
        if (mode != BodyMode::Parse && reply->error() == QNetworkReply::NoError && status != 304) {
            QList<QNetworkReply::RawHeaderPair> headers = reply->rawHeaderPairs();
            if (mode == BodyMode::Raw && raw_cb) {
                const HttpClientReplyScope reply_scope(status > 0 ? &headers : nullptr, status, false);
                raw_cb(Result<QByteArray>::ok(std::move(data)));
                return;
            }
            // Parsed on this client's worker; the callback runs back on this (the
            // context's) thread, only if the context still exists (http_off_thread::run).
            const QString safe_url = http_log_url(reply->url().toString(), base_url);
            http_off_thread::run(
                parse_pool_, const_cast<QObject*>(receiver),
                [data = std::move(data), cb, headers = std::move(headers), safe_url,
                 status]() mutable -> http_off_thread::Delivery {
                    http_off_thread::ParsedBody parsed = http_off_thread::parse_body(data);
                    data = QByteArray(); // the raw copy is not needed past this point
                    if (!parsed.ok)
                        http_log_unparsable(safe_url, status, parsed.bytes, parsed.error);
                    return [cb, headers = std::move(headers), parsed = std::move(parsed), status]() {
                        const HttpClientReplyScope reply_scope(status > 0 ? &headers : nullptr, status, false);
                        if (!parsed.ok)
                            cb(Result<QJsonDocument>::err(parsed.error.toStdString()));
                        else
                            cb(Result<QJsonDocument>::ok(parsed.doc));
                    };
                });
            return;
        }

        // The reply stays readable through response_status() / response_header()
        // (and error_header() for an HTTP error) while the callback runs. A
        // transport failure has no response, so nothing is published for it.
        const QList<QNetworkReply::RawHeaderPair> headers = reply->rawHeaderPairs();
        const HttpClientReplyScope reply_scope(status > 0 ? &headers : nullptr, status,
                                               status > 0 && reply->error() != QNetworkReply::NoError);

        // Conditional GET answered "not modified": success with no body (the
        // caller keeps its copy). Only for a request that asked (If-None-Match),
        // so nothing that never sends one sees a different result.
        if (status == 304 && reply->request().hasRawHeader("If-None-Match")) {
            cb(Result<QJsonDocument>::ok(QJsonDocument()));
            return;
        }

        QJsonParseError parse_err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parse_err);

        if (reply->error() != QNetworkReply::NoError) {
            // Qt's errorString() for an HTTP failure embeds the full request URL
            // ("Error transferring <url> - server replied: ..."), query and secret
            // path included — scrub it with the same redaction as the URL itself.
            const QString safe_url = http_log_url(reply->url().toString(), base_url);
            QString safe_error = reply->errorString();
            for (const QString& raw : {reply->url().toString(), reply->url().toString(QUrl::FullyEncoded),
                                       reply->request().url().toString(), reply->request().url().toString(QUrl::FullyEncoded)}) {
                if (!raw.isEmpty())
                    safe_error.replace(raw, safe_url);
            }
            // One line per failure at the transport layer (the caller decides what it
            // means for the user). 304/404 are ordinary answers for optional resources
            // and go to DEBUG; 429 says how long to wait; the same status + URL repeating
            // (a polled endpoint that keeps failing) is written once a minute with a count.
            if (status == 304 || status == 404) {
                LOG_DEBUG("HTTP", logf("HTTP request failed", {{"method", http_method_name(reply)},
                                                               {"status", status},
                                                               {"url", safe_url},
                                                               {"error", safe_error}}));
            } else if (status == 429) {
                LOG_WARN_RL("HTTP", QStringLiteral("429|") + safe_url, 60000,
                            logf("Rate limited", {{"method", http_method_name(reply)},
                                                  {"url", safe_url},
                                                  {"retry_after_s", http_retry_after_seconds(reply)}}));
            } else {
                LOG_WARN_RL("HTTP", QString::number(status) + QLatin1Char('|') + safe_url, 60000,
                            logf("HTTP request failed", {{"method", http_method_name(reply)},
                                                         {"status", status},
                                                         {"url", safe_url},
                                                         {"error", safe_error}}));
            }
            // Usage report: failures the Fincept API never sees (no answer) or caused (5xx).
            if ((status == 0 || status >= 500) && is_fincept_url(reply->url()))
                telemetry::UsageReporter::note_http_error(reply->url(), status, reply->error(), safe_error);

            // No HTTP status at all: a transport failure (DNS, refused, TLS, timeout).
            // Surface Qt's (URL-scrubbed) reason instead of a bare "HTTP_0" that tells
            // the user nothing. status_from_error() still yields 0 for this form.
            if (status == 0) {
                cb(Result<QJsonDocument>::err(QStringLiteral("Network error: %1").arg(safe_error).toStdString()));
                return;
            }

            const bool parsed = !data.isEmpty() && parse_err.error == QJsonParseError::NoError;
            const QJsonObject body = parsed && doc.isObject() ? doc.object() : QJsonObject{};

            // Rejected only because the session token it carried is dead (the
            // server checks the token before the handler runs, so nothing was
            // done): inspect_auth_failure() has already dropped it, so send the
            // same request once more with the API key alone and hand THAT result
            // to the caller. Only when the key is still the one this request
            // carried (not after a logout or key rotation), never for a replay
            // (resend is null), and only on our own thread (QNAM is not
            // thread-safe).
            if (status == 401 && resend && QThread::currentThread() == thread()) {
                const QByteArray sent_token = reply->request().rawHeader("X-Session-Token");
                const QByteArray sent_key = reply->request().rawHeader("X-API-Key");
                if (!sent_token.isEmpty() && sent_token != session_token_.toUtf8() && !api_key_.isEmpty() &&
                    sent_key == api_key_.toUtf8() && http_is_session_expired(reply, body)) {
                    LOG_INFO("HTTP", "Re-sending " + safe_url + " without the expired session token");
                    if (QNetworkReply* again = resend()) {
                        handle_reply(again, cb, context, nullptr, mode, raw_cb);
                        return;
                    }
                }
            }

            // Every other HTTP error is an error, including one whose body
            // happens to parse. This used to return ok(doc) so AuthApi could
            // read the server's message off the document — but ~50 other call
            // sites share this client and were told a 400/404/429/500/502/503
            // had succeeded, then parsed the error body as data (DBnomics
            // reported "0 series found" for a 429; MaritimeService cached an
            // empty vessel for 60s). The message is preserved in the error
            // string instead — see http_server_message() / message_from_error().
            // 401/403 carry the server's wording as well; nothing matches on a
            // bare "HTTP_401" any more (session expiry is detected above and in
            // inspect_auth_failure() from the response itself). The response
            // headers stay readable through error_header() while the callback
            // runs (Retry-After, X-Data-Error, …).
            cb(Result<QJsonDocument>::err(http_server_message(body, status, http_retry_after_seconds(reply))));
            return;
        }

        if (parse_err.error != QJsonParseError::NoError) {
            // A 2xx whose body is not JSON: the caller gets the error; say which endpoint.
            http_log_unparsable(http_log_url(reply->url().toString(), base_url), status, data.size(),
                                parse_err.errorString());
            cb(Result<QJsonDocument>::err(parse_err.errorString().toStdString()));
            return;
        }
        cb(Result<QJsonDocument>::ok(doc));
    });
}

// Each verb builds a Resend that rebuilds the request from scratch (so a replay
// picks up the credentials current at that moment) and sends it. A request
// whose session token the caller chose explicitly is never replayed.

void HttpClient::get(const QString& url, JsonCallback callback, const QObject* context, const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "GET " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers]() { return nam_->get(build_request(url, extra_headers)); };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send));
}

void HttpClient::post(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context,
                      const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "POST " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers, payload = QJsonDocument(body).toJson()]() {
        return nam_->post(build_request(url, extra_headers), payload);
    };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send));
}

void HttpClient::post_with_timeout(const QString& url, const QJsonObject& body, int timeout_ms,
                                   JsonCallback callback, const QObject* context, const Headers& extra_headers) {
    if (timeout_ms <= 0) {
        post(url, body, std::move(callback), context, extra_headers);
        return;
    }
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "POST " + http_log_url(url, base_url_));
    // A request-level transfer timeout takes precedence over the manager's.
    Resend send = [this, url, extra_headers, timeout_ms, payload = QJsonDocument(body).toJson()]() {
        QNetworkRequest req = build_request(url, extra_headers);
        req.setTransferTimeout(std::chrono::milliseconds{timeout_ms});
        return nam_->post(req, payload);
    };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send));
}

void HttpClient::post_large(const QString& url, const QJsonObject& body, int timeout_ms, JsonCallback callback,
                            const QObject* context, const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "POST(large) " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers, timeout_ms, payload = QJsonDocument(body).toJson()]() {
        QNetworkRequest req = build_request(url, extra_headers);
        if (timeout_ms > 0)
            req.setTransferTimeout(std::chrono::milliseconds{timeout_ms});
        return nam_->post(req, payload);
    };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send),
                 BodyMode::ParseOnWorker);
}

void HttpClient::get_large(const QString& url, JsonCallback callback, const QObject* context,
                           const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "GET(large) " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers]() { return nam_->get(build_request(url, extra_headers)); };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send),
                 BodyMode::ParseOnWorker);
}

void HttpClient::get_raw(const QString& url, BytesCallback callback, const QObject* context,
                         const Headers& extra_headers) {
    // Errors (and a 304 answer to If-None-Match) take get()'s JSON path; only the
    // result type changes on the way out.
    JsonCallback on_json = [callback](Result<QJsonDocument> result) {
        if (result.is_err())
            callback(Result<QByteArray>::err(result.error()));
        else
            callback(Result<QByteArray>::ok(QByteArray()));
    };
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(on_json));
        return;
    }
    LOG_DEBUG("HTTP", "GET(raw) " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers]() { return nam_->get(build_request(url, extra_headers)); };
    auto* reply = send();
    handle_reply(reply, std::move(on_json), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send), BodyMode::Raw,
                 std::move(callback));
}

void HttpClient::post_form(const QString& url, const QMap<QString, QString>& fields, JsonCallback callback,
                           const QObject* context, const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "POST(form) " + http_log_url(url, base_url_));

    // Percent-encode by hand: QUrlQuery leaves '+' alone in a value, and a server
    // decodes a bare '+' as a space — which would turn "+14155238886" into
    // " 14155238886".
    QByteArray encoded;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        if (!encoded.isEmpty())
            encoded += '&';
        encoded += QUrl::toPercentEncoding(it.key()) + '=' + QUrl::toPercentEncoding(it.value());
    }

    const bool caller_content_type = extra_headers.contains("Content-Type");
    Resend send = [this, url, extra_headers, encoded, caller_content_type]() {
        QNetworkRequest req = build_request(url, extra_headers);
        if (!caller_content_type)
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        return nam_->post(req, encoded);
    };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send));
}

void HttpClient::put(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context,
                     const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "PUT " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers, payload = QJsonDocument(body).toJson()]() {
        return nam_->put(build_request(url, extra_headers), payload);
    };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send));
}

void HttpClient::patch(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context,
                       const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "PATCH " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers, payload = QJsonDocument(body).toJson(QJsonDocument::Compact)]() {
        return nam_->sendCustomRequest(build_request(url, extra_headers), "PATCH", payload);
    };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send));
}

void HttpClient::del(const QString& url, JsonCallback callback, const QObject* context, const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "DELETE " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers]() { return nam_->deleteResource(build_request(url, extra_headers)); };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send));
}

void HttpClient::del(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context,
                     const Headers& extra_headers) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", "DELETE " + http_log_url(url, base_url_));
    Resend send = [this, url, extra_headers, payload = QJsonDocument(body).toJson(QJsonDocument::Compact)]() {
        return nam_->sendCustomRequest(build_request(url, extra_headers), "DELETE", payload);
    };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send));
}

// ── Opt-in forms of POST / PUT / PATCH / DELETE: large (parsed on a worker) and raw ──
// The plain verbs above are untouched; these share one sender that encodes and sends
// the request exactly as the plain verb does.

void HttpClient::send_opt_in(const char* verb, const char* label, const QString& url, const QJsonObject* body,
                             const Headers& extra_headers, JsonCallback callback, const QObject* context,
                             BodyMode mode, BytesCallback raw_callback) {
    if (!http_is_sendable_url(url)) {
        http_reject_bad_url(context ? context : this, std::move(callback));
        return;
    }
    LOG_DEBUG("HTTP", QString::fromLatin1(label) + QLatin1Char(' ') + http_log_url(url, base_url_));
    const QByteArray method(verb);
    const bool has_body = body != nullptr;
    // The payload as the plain verb encodes it: post() / put() indented, patch() and
    // del() with a body compact.
    QByteArray payload;
    if (has_body) {
        payload = (method == "POST" || method == "PUT") ? QJsonDocument(*body).toJson()
                                                        : QJsonDocument(*body).toJson(QJsonDocument::Compact);
    }
    Resend send = [this, url, extra_headers, method, payload, has_body]() -> QNetworkReply* {
        const QNetworkRequest req = build_request(url, extra_headers);
        if (method == "POST")
            return nam_->post(req, payload);
        if (method == "PUT")
            return nam_->put(req, payload);
        if (method == "DELETE" && !has_body)
            return nam_->deleteResource(req);
        return nam_->sendCustomRequest(req, method, payload); // PATCH; DELETE with a body
    };
    auto* reply = send();
    handle_reply(reply, std::move(callback), context,
                 http_find_header(extra_headers, "X-Session-Token") ? Resend{} : std::move(send), mode,
                 std::move(raw_callback));
}

void HttpClient::put_large(const QString& url, const QJsonObject& body, JsonCallback callback,
                           const QObject* context, const Headers& extra_headers) {
    send_opt_in("PUT", "PUT(large)", url, &body, extra_headers, std::move(callback), context,
                BodyMode::ParseOnWorker);
}

void HttpClient::patch_large(const QString& url, const QJsonObject& body, JsonCallback callback,
                             const QObject* context, const Headers& extra_headers) {
    send_opt_in("PATCH", "PATCH(large)", url, &body, extra_headers, std::move(callback), context,
                BodyMode::ParseOnWorker);
}

void HttpClient::del_large(const QString& url, JsonCallback callback, const QObject* context,
                           const Headers& extra_headers) {
    send_opt_in("DELETE", "DELETE(large)", url, nullptr, extra_headers, std::move(callback), context,
                BodyMode::ParseOnWorker);
}

void HttpClient::del_large(const QString& url, const QJsonObject& body, JsonCallback callback,
                           const QObject* context, const Headers& extra_headers) {
    send_opt_in("DELETE", "DELETE(large)", url, &body, extra_headers, std::move(callback), context,
                BodyMode::ParseOnWorker);
}

// Raw forms: the error adapter is built before `callback` is moved into the raw slot
// (argument evaluation order is unspecified).

void HttpClient::post_raw(const QString& url, const QJsonObject& body, BytesCallback callback,
                          const QObject* context, const Headers& extra_headers) {
    JsonCallback on_json = http_raw_error_path(callback);
    send_opt_in("POST", "POST(raw)", url, &body, extra_headers, std::move(on_json), context, BodyMode::Raw,
                std::move(callback));
}

void HttpClient::put_raw(const QString& url, const QJsonObject& body, BytesCallback callback,
                         const QObject* context, const Headers& extra_headers) {
    JsonCallback on_json = http_raw_error_path(callback);
    send_opt_in("PUT", "PUT(raw)", url, &body, extra_headers, std::move(on_json), context, BodyMode::Raw,
                std::move(callback));
}

void HttpClient::patch_raw(const QString& url, const QJsonObject& body, BytesCallback callback,
                           const QObject* context, const Headers& extra_headers) {
    JsonCallback on_json = http_raw_error_path(callback);
    send_opt_in("PATCH", "PATCH(raw)", url, &body, extra_headers, std::move(on_json), context, BodyMode::Raw,
                std::move(callback));
}

void HttpClient::del_raw(const QString& url, BytesCallback callback, const QObject* context,
                         const Headers& extra_headers) {
    JsonCallback on_json = http_raw_error_path(callback);
    send_opt_in("DELETE", "DELETE(raw)", url, nullptr, extra_headers, std::move(on_json), context, BodyMode::Raw,
                std::move(callback));
}

void HttpClient::del_raw(const QString& url, const QJsonObject& body, BytesCallback callback,
                         const QObject* context, const Headers& extra_headers) {
    JsonCallback on_json = http_raw_error_path(callback);
    send_opt_in("DELETE", "DELETE(raw)", url, &body, extra_headers, std::move(on_json), context, BodyMode::Raw,
                std::move(callback));
}

void HttpClient::set_auth_header(const QString& api_key) {
    api_key_ = api_key;
}

void HttpClient::set_session_token(const QString& token) {
    // A fresh token (login) is only ever set by AuthManager; a token the server
    // already rejected is never set back (AuthManager forgets it).
    session_token_ = token;
}

void HttpClient::clear_session_token() {
    session_token_.clear();
}

void HttpClient::set_base_url(const QString& base) {
    base_url_ = base;
}

} // namespace fincept
