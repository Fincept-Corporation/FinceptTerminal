#pragma once
#include "core/result/Result.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <functional>
#include <string>

class QThreadPool;

namespace fincept {

/// Async HTTP client wrapping QNetworkAccessManager. Responses delivered on the Qt event loop.
///
/// Fincept API auth failures are inspected here, once, for every caller:
///  - 401 "session_invalidated" (response header `X-Session-Expired: true`) on a
///    request that carried the current session token: the token is dropped at
///    once (later requests go out with the API key only), session_token_rejected()
///    is emitted once for that token, and the rejected request is re-sent a single
///    time without the token, so the caller never sees the 401.
///  - any other 401 on a request that carried the current API key:
///    api_key_rejected() (AuthManager re-validates the key, rate-limited).
///  - 403 with `error_code == "IP_BLOCKED"`: ip_block_changed(true, …). Never an
///    account problem; see is_ip_blocked().
class HttpClient : public QObject {
    Q_OBJECT
  public:
    static HttpClient& instance();

    using JsonCallback = std::function<void(Result<QJsonDocument>)>;

    /// Extra raw headers for one request, applied last so they override the
    /// defaults set by build_request(). This is the supported way to send a
    /// per-caller credential: this class is a process-wide singleton, so
    /// set_auth_header()/set_session_token() leak into every other caller's
    /// requests and must not be used to scope one call.
    /// An `X-API-Key` / `X-Session-Token` entry replaces the default credential;
    /// an EMPTY value for either one means "send this request without it".
    using Headers = QMap<QByteArray, QByteArray>;

    /// `context` scopes callback lifetime — pass `this` from anything that can outlive its reply.
    /// nullptr is safe only for objects whose lifetime equals the app's (singletons).
    void get(const QString& url, JsonCallback callback, const QObject* context = nullptr,
             const Headers& extra_headers = {});
    void post(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context = nullptr,
              const Headers& extra_headers = {});
    /// post() with its own transfer timeout instead of the client-wide 20 s, for
    /// the few endpoints that legitimately work longer before the first byte
    /// (e.g. /news/analyze: article fetch + model call). `timeout_ms` <= 0 keeps
    /// the client-wide default. Every other behaviour is identical to post().
    void post_with_timeout(const QString& url, const QJsonObject& body, int timeout_ms, JsonCallback callback,
                           const QObject* context = nullptr, const Headers& extra_headers = {});
    /// post_with_timeout() for answers that can be tens of MB (e.g. a world-wide
    /// vessel area search): a successful body is parsed on a worker thread — see
    /// get_large(), which this is the POST form of.
    void post_large(const QString& url, const QJsonObject& body, int timeout_ms, JsonCallback callback,
                    const QObject* context = nullptr, const Headers& extra_headers = {});

    /// get() for answers that can be large (catalogue lists, long series, symbol
    /// masters; anything that may exceed ~256 KB). Opt-in per request; get() itself
    /// is unchanged. A 2xx body is parsed on one of this client's worker threads,
    /// never on the GUI thread; the callback then runs on `context`'s thread
    /// (nullptr: this client's), and never once `context` has been destroyed.
    /// Everything else is exactly get(): the same error strings and log lines, the
    /// 401 re-send, the auth / IP-block / credit signals, 304 for If-None-Match,
    /// and response_status() / response_header() inside the callback.
    void get_large(const QString& url, JsonCallback callback, const QObject* context = nullptr,
                   const Headers& extra_headers = {});

    /// A 2xx body handed over as it arrived, unparsed — for a caller that decodes
    /// on its own worker thread anyway (parse it there with parse_body(), so it is
    /// parsed once and never on the GUI thread). Errors are the strings get() gives;
    /// a 304 answer to If-None-Match is ok(empty) with response_status() == 304.
    /// Same threading, lifetime and logging as get(); an unparsable body is the
    /// caller's to report.
    using BytesCallback = std::function<void(Result<QByteArray>)>;
    void get_raw(const QString& url, BytesCallback callback, const QObject* context = nullptr,
                 const Headers& extra_headers = {});

    /// A body as get() would have turned it into JSON (its error text on failure).
    /// Pure; any thread — for get_raw() callers on their worker.
    static Result<QJsonDocument> parse_body(const QByteArray& body);
    void put(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context = nullptr,
             const Headers& extra_headers = {});
    /// HTTP PATCH with a JSON body (sent as a custom verb; same handling as put()).
    void patch(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context = nullptr,
               const Headers& extra_headers = {});
    void del(const QString& url, JsonCallback callback, const QObject* context = nullptr,
             const Headers& extra_headers = {});
    void del(const QString& url, const QJsonObject& body, JsonCallback callback, const QObject* context = nullptr,
             const Headers& extra_headers = {});

    /// put() / patch() / del() for answers that can be large — the PUT, PATCH and
    /// DELETE forms of get_large(). Opt-in per request; the plain verbs are unchanged.
    /// A 2xx body is parsed on one of this client's worker threads, never on the GUI
    /// thread; the callback then runs on `context`'s thread (nullptr: this client's),
    /// and never once `context` has been destroyed. Everything else is the plain
    /// verb's: the request as it sends it, the same error strings and log lines, the
    /// 401 re-send, the auth / IP-block / credit signals, and response_status() /
    /// response_header() inside the callback.
    void put_large(const QString& url, const QJsonObject& body, JsonCallback callback,
                   const QObject* context = nullptr, const Headers& extra_headers = {});
    void patch_large(const QString& url, const QJsonObject& body, JsonCallback callback,
                     const QObject* context = nullptr, const Headers& extra_headers = {});
    void del_large(const QString& url, JsonCallback callback, const QObject* context = nullptr,
                   const Headers& extra_headers = {});
    void del_large(const QString& url, const QJsonObject& body, JsonCallback callback,
                   const QObject* context = nullptr, const Headers& extra_headers = {});

    /// get_raw() for the write verbs: a 2xx body handed over exactly as it arrived,
    /// unparsed — for third-party endpoints whose success answer is not JSON by
    /// design. Webhooks answer "ok" (Slack, Mattermost), "1" or a text (Teams), 202 /
    /// 204 with no body (Teams Workflows, Discord); the JsonCallback forms report such
    /// a 2xx as an error carrying the JSON parse text (status_from_error() == 0,
    /// response_status() still the 2xx) and drop the text.
    ///  - A 2xx: ok(body), ok(empty) for no body. Inside
    ///    the callback response_status() is the status and response_header() reads
    ///    the answer's headers (Content-Type, …). The body is never logged and never
    ///    reported as "not valid JSON": what it means is the caller's to decide.
    ///  - 4xx / 5xx: the plain verb's err("HTTP_<status>[: <server sentence>]"), with
    ///    response_status(), response_header() and error_header() readable inside the
    ///    callback; the raw error body is not handed over (option (b) of the
    ///    notification providers' request, which they accepted).
    ///  - A transport failure (no response): err("Network error: …"), URL-scrubbed,
    ///    response_status() == 0. A URL that is not absolute http(s) or an API path:
    ///    err("Invalid URL: …"), delivered asynchronously, as for every verb.
    /// Otherwise the plain verb's request (headers, per-request credentials, auth
    /// handling, the client-wide 20 s timeout), the 401 session re-send (the caller
    /// gets the re-sent answer), the auth / IP-block / credit signals and the
    /// URL-scrubbed log lines. Delivered on `context`'s thread (nullptr: this
    /// client's), never once `context` has been destroyed. No worker parse.
    void post_raw(const QString& url, const QJsonObject& body, BytesCallback callback,
                  const QObject* context = nullptr, const Headers& extra_headers = {});
    void put_raw(const QString& url, const QJsonObject& body, BytesCallback callback,
                 const QObject* context = nullptr, const Headers& extra_headers = {});
    void patch_raw(const QString& url, const QJsonObject& body, BytesCallback callback,
                   const QObject* context = nullptr, const Headers& extra_headers = {});
    void del_raw(const QString& url, BytesCallback callback, const QObject* context = nullptr,
                 const Headers& extra_headers = {});
    void del_raw(const QString& url, const QJsonObject& body, BytesCallback callback,
                 const QObject* context = nullptr, const Headers& extra_headers = {});

    /// POST `fields` as an application/x-www-form-urlencoded body, for APIs that
    /// do not accept JSON (Twilio's Messages API). The response is still parsed
    /// as JSON like every other call.
    void post_form(const QString& url, const QMap<QString, QString>& fields, JsonCallback callback,
                   const QObject* context = nullptr, const Headers& extra_headers = {});

    /// Error strings produced by this client are `"HTTP_<status>"` or
    /// `"HTTP_<status>: <server message>"`. These two recover the halves —
    /// use them instead of re-implementing the parse at each call site.
    /// status_from_error() returns 0 for transport/parse failures (no status).
    static int status_from_error(const std::string& error);
    /// Server-supplied text, or empty when the response carried none.
    /// For a non-HTTP failure (transport/JSON parse) the raw text is returned.
    static QString message_from_error(const std::string& error);

    /// The reply whose JsonCallback is running right now. Valid only inside that
    /// callback, before it returns — copy what you need; outside one (and for a
    /// transport failure, which has no response) these return 0 / empty.
    ///  - response_status(): its HTTP status (200, 304, 402, …).
    ///  - response_header(name): one of its headers (ETag, X-Poll-Interval, …),
    ///    success or HTTP error alike; empty when the response did not carry it.
    ///  - error_header(name): the same, but only when the callback received an
    ///    "HTTP_<status>" error — the error string carries the status and the
    ///    server's sentence, some answers say more in headers (Retry-After,
    ///    X-Data-Error, X-Symbol-Suggestions).
    /// Header names are case-insensitive.
    static int response_status();
    static QByteArray response_header(const char* name);
    static QByteArray error_header(const char* name);

    /// Conditional GET: a caller that sends `If-None-Match` (extra_headers) and
    /// gets 304 Not Modified receives SUCCESS with an empty document, and
    /// response_status() == 304 inside the callback — "unchanged, keep yours".
    /// A 304 to a request without If-None-Match is handled as before.

    /// `X-Poll-Interval: <seconds>` (the server's "do not poll this faster") as
    /// milliseconds, capped at kMaxPollIntervalMs so a bad header can slow a check
    /// down but never switch it off. 0 when absent or not a positive number.
    /// Callers poll at max(own interval, this).
    static qint64 poll_interval_ms(const QByteArray& header_value);
    static constexpr qint64 kMaxPollIntervalMs = 60LL * 60 * 1000;

    /// Human-readable explanation from an error body, empty when there is none.
    /// Understands the Fincept envelope `{"message": <string | {"message": …}>}`
    /// (preferred over a machine code in "error"), `{"error": <string | {"message": …}>}`
    /// and FastAPI `{"detail": <string | [{loc, msg}]>}`. Used for HTTP errors and
    /// for 2xx `{"success": false}` bodies alike.
    static QString error_text_from_body(const QJsonObject& body);

    void set_auth_header(const QString& api_key);
    void set_session_token(const QString& token);
    void clear_session_token();
    void set_base_url(const QString& base);

    /// True while the Fincept API is refusing this machine's IP (403 IP_BLOCKED)
    /// — advisory: it turns on at the first IP_BLOCKED answer and off once none
    /// has been seen for the block length (Retry-After if sent, else 30 min).
    /// Pollers should pause while it is set. The server keeps the block per
    /// worker process, so some requests may still succeed meanwhile.
    bool is_ip_blocked() const { return ip_blocked_; }

    /// For Fincept API clients with their own QNetworkAccessManager
    /// (ChatModeService, …): pass the body of a 403 the Fincept API returned.
    /// Returns true — and enters the IP-blocked state above, emitting
    /// ip_block_changed() on the transition — when it is an IP block, so it is
    /// never mistaken for an account problem. Any thread.
    bool report_forbidden_response(const QByteArray& response_body);

  signals:
    /// 401 session_invalidated for the session token currently in use (signed in
    /// elsewhere, or password reset). Already dropped here; emitted once per token.
    void session_token_rejected(const QString& rejected_token);
    /// 401 (not a session expiry) for a request that carried the current API key.
    void api_key_rejected();
    /// IP block state changed. `message` is the user-facing text (server wording
    /// plus how long such blocks last) when `blocked`, empty otherwise.
    void ip_block_changed(bool blocked, const QString& message);
    /// A billed Fincept response (2xx with X-Credits-Cost) for a request made
    /// with the current API key carried the balance after the charge
    /// (X-Credits-Balance) — or a 402 refusal carried the balance that caused it
    /// (cost 0). AuthManager keeps the session's balance with it.
    void credit_balance_reported(double balance);

  private:
    HttpClient();
    /// Re-sends the same request built afresh (current credentials). Null when the
    /// request must not be replayed (it carried a caller-chosen session token).
    using Resend = std::function<QNetworkReply*()>;

    QNetworkRequest build_request(const QString& url, const Headers& extra_headers = {}) const;
    /// What happens to a 2xx body: parsed here (get(), post(), …), parsed on a
    /// worker (get_large(), post_large(), put_large(), …) or handed over raw
    /// (get_raw(), post_raw(), …, whose `raw_callback` receives it; errors still go
    /// to `callback`).
    enum class BodyMode { Parse, ParseOnWorker, Raw };
    void handle_reply(QNetworkReply* reply, JsonCallback callback, const QObject* context, Resend resend,
                      BodyMode mode = BodyMode::Parse, BytesCallback raw_callback = {});
    /// The large / raw forms of POST, PUT, PATCH and DELETE: refuse an unsendable URL,
    /// log "<label> <url>", send `verb` exactly as post() / put() / patch() / del() do
    /// (`body` null: no payload — a DELETE without a body) and hand the reply to
    /// handle_reply() in `mode`, replayable unless the caller chose the session token.
    void send_opt_in(const char* verb, const char* label, const QString& url, const QJsonObject* body,
                     const Headers& extra_headers, JsonCallback callback, const QObject* context, BodyMode mode,
                     BytesCallback raw_callback = {});
    /// Runs first on every finished reply (before the caller's callback) on this
    /// object's thread; inspects Fincept 401/403 without consuming the body.
    void inspect_auth_failure(QNetworkReply* reply);
    /// Runs alongside inspect_auth_failure(): forwards the post-charge balance of
    /// a billed Fincept response, or the refusing balance of a 402
    /// (credit_balance_reported).
    void inspect_credit_headers(QNetworkReply* reply);
    bool is_fincept_url(const QUrl& url) const;
    void set_ip_blocked(const QString& message, int retry_after_s);

    QNetworkAccessManager* nam_ = nullptr;
    /// Two threads that parse large bodies (the *_large() forms) — their own,
    /// so a parse never queues behind long jobs on the global pool.
    QThreadPool* parse_pool_ = nullptr;
    QString base_url_;
    QString api_key_;
    QString session_token_;

    bool ip_blocked_ = false;
    QTimer ip_block_timer_;
    /// Server-side blocks last 30 min (24 h after repeated failures).
    static constexpr int kIpBlockWindowMs = 30 * 60 * 1000;
};

} // namespace fincept
