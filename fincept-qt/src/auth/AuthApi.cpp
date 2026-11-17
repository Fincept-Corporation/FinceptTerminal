#include "auth/AuthApi.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QJsonDocument>

namespace fincept::auth {

AuthApi& AuthApi::instance() {
    static AuthApi s;
    return s;
}

// Parse FastAPI/Pydantic 422 validation error detail array into a human-readable string.
// Input: {"detail": [{"loc": ["body", "email"], "msg": "...", "type": "..."}, ...]}
// Output: "Email: value is not a valid email address"
static QString parse_422_detail(const QJsonDocument& doc) {
    if (doc.isNull() || !doc.isObject())
        return {};

    auto root = doc.object();
    if (!root.contains("detail"))
        return {};

    // detail can be a string (plain message) or an array (Pydantic field errors)
    if (root["detail"].isString())
        return root["detail"].toString();

    if (!root["detail"].isArray())
        return {};

    QStringList msgs;
    for (const auto& item : root["detail"].toArray()) {
        auto err = item.toObject();
        QString msg = err["msg"].toString();
        if (msg.isEmpty())
            continue;

        // loc is ["body", "field_name"] — extract the field name (last element)
        QString field;
        auto loc = err["loc"].toArray();
        if (!loc.isEmpty()) {
            // Skip "body" prefix, take the last meaningful segment
            for (int i = loc.size() - 1; i >= 0; --i) {
                QString seg = loc[i].toString();
                if (!seg.isEmpty() && seg != "body") {
                    // Convert snake_case to Title Case for display
                    seg[0] = seg[0].toUpper();
                    seg.replace('_', ' ');
                    field = seg;
                    break;
                }
            }
        }

        if (!field.isEmpty())
            msgs << field + ": " + msg;
        else
            msgs << msg;
    }

    return msgs.join("\n");
}

void AuthApi::request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb,
                      const QObject* context, const QMap<QByteArray, QByteArray>& extra_headers) {
    auto& http = fincept::HttpClient::instance();

    auto handle = [cb_in = std::move(cb), endpoint](fincept::Result<QJsonDocument> result) {
        // Response metadata, read while HttpClient's reply is current (it is only
        // valid inside this callback): stamped on every answer handed on.
        const QString etag = QString::fromUtf8(fincept::HttpClient::response_header("ETag"));
        const qint64 poll_ms =
            fincept::HttpClient::poll_interval_ms(fincept::HttpClient::response_header("X-Poll-Interval"));
        auto cb = [&cb_in, &etag, poll_ms](ApiResponse r) {
            r.etag = etag;
            r.poll_interval_ms = poll_ms;
            cb_in(std::move(r));
        };

        // ── HTTP error / transport error path ────────────────────────────────
        // Any non-2xx is err(). HttpClient carries the server's own wording in
        // the error string ("HTTP_<status>: <message>") — prefer it, since that
        // is where the specific failures live, 401/403/429 included: "Incorrect
        // password. You have 2 attempt(s) remaining…", "No account found with
        // this email address…", "Your account is not verified yet…" (403), "Too
        // many failed attempts. Please try again in 1 minute(s)…" (429), and an
        // IP block (403 IP_BLOCKED, with how long it lasts). The switch below is
        // only the fallback for a response that carried no text.
        if (result.is_err()) {
            const int status = fincept::HttpClient::status_from_error(result.error());
            QString msg = status > 0 ? fincept::HttpClient::message_from_error(result.error()) : QString{};
            if (!msg.isEmpty()) {
                cb({false, {}, msg, status});
                return;
            }

            switch (status) {
                case 400:
                    msg = tr("Bad request. Please check your input.");
                    break;
                case 401:
                    msg = tr("Incorrect email or password.");
                    break;
                case 403:
                    // Not "your account may be suspended": the API never answers
                    // 403 for an account state on these routes (unverified is
                    // reported with its own message; a suspended key is a 401).
                    msg = tr("The server refused the request. Please try again later.");
                    break;
                case 404:
                    msg = tr("Account not found. Please check your email.");
                    break;
                case 409:
                    msg = tr("An account with this email or username already exists.");
                    break;
                case 422:
                    msg = tr("Please check your input and try again.");
                    break;
                case 429:
                    msg = tr("Too many attempts. Please wait a moment and try again.");
                    break;
                case 500:
                    msg = tr("Server error. Please try again later.");
                    break;
                case 503:
                    msg = tr("Service unavailable. Please try again later.");
                    break;
                default:
                    msg = status > 0 ? tr("Request failed (HTTP %1). Please try again.").arg(status)
                                     : tr("Network error. Check your connection.");
            }
            cb({false, {}, msg, status});
            return;
        }

        // ── 304: a conditional GET whose resource is unchanged ───────────────
        // Success with no body — the caller keeps its copy. Never an empty profile.
        if (fincept::HttpClient::response_status() == 304) {
            cb({true, {}, {}, 304});
            return;
        }

        // ── 2xx body path ────────────────────────────────────────────────────
        // The backend reports most business-logic failures with HTTP 200 and a
        // {"success": false, ...} envelope, so the checks below still matter.
        auto doc = result.value();
        auto obj = doc.object();

        // 1. Pydantic 422 field-level validation errors: {"detail": [{loc, msg, type}, ...]}
        if (obj.contains("detail") && obj["detail"].isArray()) {
            QString detail_msg = parse_422_detail(doc);
            if (!detail_msg.isEmpty()) {
                cb({false, obj, detail_msg, 422});
                return;
            }
        }

        // 2. A 2xx {"success": false, "message": ...} envelope. The current API
        //    reports login/registration/verification failures with a real 4xx
        //    status (handled above), not this way — but keep honouring it, and
        //    read "message" whether it is a string or a {"message": ...} object.
        if (obj.contains("success") && !obj["success"].toBool()) {
            QString msg = fincept::HttpClient::error_text_from_body(obj);
            if (msg.isEmpty())
                msg = tr("Request failed. Please try again.");
            cb({false, obj, msg, 200});
            return;
        }

        // 3. Plain {"detail": "string"} error (FastAPI default for non-422 errors)
        if (obj.contains("detail") && obj["detail"].isString()) {
            QString msg = obj["detail"].toString();
            if (!msg.isEmpty()) {
                cb({false, obj, msg, 0});
                return;
            }
        }

        cb({true, obj, {}, 200});
    };

    // Forward `context` so HttpClient auto-disconnects the handler if the caller
    // is destroyed mid-flight. Dropping it scoped every callback to the AuthApi
    // singleton instead of the caller — including PricingScreen's checkout-token
    // call, which spans a payment-provider round trip (the longest-latency call
    // in the app, and exactly when a user is likely to close the window).
    if (method == "GET")
        http.get(endpoint, handle, context, extra_headers);
    else if (method == "POST")
        http.post(endpoint, body, handle, context, extra_headers);
    else if (method == "PUT")
        http.put(endpoint, body, handle, context, extra_headers);
    else if (method == "DELETE")
        http.del(endpoint, handle, context, extra_headers);
}

// ── Unauthenticated auth endpoints ───────────────────────────────────────────

void AuthApi::login(const LoginRequest& req, Callback cb, const QObject* context) {
    request("POST", "/user/login", req.to_json(), cb, context);
}

void AuthApi::register_user(const RegisterRequest& req, Callback cb, const QObject* context) {
    request("POST", "/user/register", req.to_json(), cb, context);
}

void AuthApi::verify_otp(const VerifyOtpRequest& req, Callback cb, const QObject* context) {
    request("POST", "/user/verify-otp", req.to_json(), cb, context);
}

void AuthApi::resend_otp(const QString& email, Callback cb, const QObject* context) {
    QJsonObject body;
    body["email"] = email;
    request("POST", "/user/resend-otp", body, cb, context);
}

void AuthApi::forgot_password(const ForgotPasswordRequest& req, Callback cb, const QObject* context) {
    request("POST", "/user/forgot-password", req.to_json(), cb, context);
}

void AuthApi::reset_password(const ResetPasswordRequest& req, Callback cb, const QObject* context) {
    request("POST", "/user/reset-password", req.to_json(), cb, context);
}

void AuthApi::verify_mfa(const QString& email, const QString& otp, Callback cb, const QObject* context) {
    QJsonObject body;
    body["email"] = email;
    body["otp"] = otp;
    request("POST", "/user/verify-mfa", body, cb, context);
}

void AuthApi::redeem_desktop_handoff(const QString& code, Callback cb, const QObject* context) {
    QJsonObject body;
    body["token"] = code;
    request("POST", "/user/auth/desktop-handoff/redeem", body, cb, context);
}

// ── Authenticated endpoints (HttpClient carries X-API-Key + X-Session-Token) ─

void AuthApi::logout(Callback cb, const QObject* context) {
    request("POST", "/user/logout", {}, cb, context);
}

void AuthApi::session_pulse(Callback cb, const QObject* context) {
    request("GET", "/user/session-pulse", {}, cb, context);
}

void AuthApi::get_user_profile(Callback cb, const QObject* context, const QString& if_none_match) {
    QMap<QByteArray, QByteArray> headers;
    if (!if_none_match.isEmpty())
        headers.insert(QByteArrayLiteral("If-None-Match"), if_none_match.toUtf8());
    request("GET", "/user/profile", {}, cb, context, headers);
}

void AuthApi::validate_api_key(Callback cb, const QObject* context) {
    // Empty value = HttpClient omits the header (see HttpClient::Headers).
    request("GET", "/user/profile", {}, cb, context, {{QByteArrayLiteral("X-Session-Token"), QByteArray()}});
}

void AuthApi::check_session_token(const QString& session_token, Callback cb, const QObject* context) {
    request("GET", "/user/session-pulse", {}, cb, context,
            {{QByteArrayLiteral("X-Session-Token"), session_token.toUtf8()}});
}

// ── Subscription / payment ────────────────────────────────────────────────────

void AuthApi::get_subscription_plans(Callback cb, const QObject* context) {
    request("GET", "/cashfree/plans", {}, cb, context);
}

void AuthApi::generate_checkout_token(const QString& plan_id, Callback cb, const QObject* context) {
    QJsonObject body;
    body["plan_id"] = plan_id;
    request("POST", "/user/generate-checkout-token", body, cb, context);
}

} // namespace fincept::auth
