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
                      const QObject* context) {
    auto& http = fincept::HttpClient::instance();

    auto handle = [cb, endpoint](fincept::Result<QJsonDocument> result) {
        // ── HTTP error / transport error path ────────────────────────────────
        // Any non-2xx is err(). HttpClient carries the server's own wording in
        // the error string ("HTTP_<status>: <message>") — prefer it, since that
        // is where the specific registration/verification failures live. Fall
        // back to a friendly status-specific message when the body had none.
        if (result.is_err()) {
            const int status = fincept::HttpClient::status_from_error(result.error());
            // 401/403 are deliberately message-free (HttpClient keeps them a
            // bare "HTTP_<status>" for session-expiry detection), so they land
            // in the switch below and get our own wording.
            QString msg = status > 0 ? fincept::HttpClient::message_from_error(result.error()) : QString{};
            if (!msg.isEmpty()) {
                cb({false, {}, msg, status});
                return;
            }

            switch (status) {
                case 400:
                    msg = "Bad request. Please check your input.";
                    break;
                case 401:
                    msg = "Incorrect email or password.";
                    break;
                case 403:
                    msg = "Access denied. Your account may be suspended.";
                    break;
                case 404:
                    msg = "Account not found. Please check your email.";
                    break;
                case 409:
                    msg = "An account with this email or username already exists.";
                    break;
                case 422:
                    msg = "Please check your input and try again.";
                    break;
                case 429:
                    msg = "Too many attempts. Please wait a moment and try again.";
                    break;
                case 500:
                    msg = "Server error. Please try again later.";
                    break;
                case 503:
                    msg = "Service unavailable. Please try again later.";
                    break;
                default:
                    msg = status > 0 ? QString("Request failed (HTTP %1). Please try again.").arg(status)
                                     : "Network error. Check your connection.";
            }
            cb({false, {}, msg, status});
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

        // 2. Server business-logic errors: {"success": false, "message": "..."}
        //    This is the primary path for ALL the specific messages:
        //    - "Incorrect password. You have 2 attempt(s) remaining."
        //    - "Too many failed attempts — please try again in 2 minutes."
        //    - "Your account is not verified yet."
        //    - "This username is already taken."
        //    - "An account with this email already exists."
        //    - "Your verification code has expired."
        //    - "Incorrect verification code. You have X attempt(s) remaining."
        //    etc. — the server sends these verbatim and we display them as-is.
        if (obj.contains("success") && !obj["success"].toBool()) {
            // Prefer "message", fall back to "detail" string, then generic
            QString msg = obj.value("message").toString();
            if (msg.isEmpty())
                msg = obj.value("detail").toString();
            if (msg.isEmpty())
                msg = "Request failed. Please try again.";
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
        http.get(endpoint, handle, context);
    else if (method == "POST")
        http.post(endpoint, body, handle, context);
    else if (method == "PUT")
        http.put(endpoint, body, handle, context);
    else if (method == "DELETE")
        http.del(endpoint, handle, context);
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

void AuthApi::get_user_profile(Callback cb, const QObject* context) {
    request("GET", "/user/profile", {}, cb, context);
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
