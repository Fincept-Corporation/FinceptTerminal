#include "auth/AuthApi.h"
#include "network/http/HttpClient.h"
#include "core/logging/Logger.h"
#include <QJsonDocument>

namespace fincept::auth {

AuthApi& AuthApi::instance() {
    static AuthApi s;
    return s;
}

void AuthApi::request(const QString& method, const QString& endpoint,
                       const QJsonObject& body, Callback cb) {
    auto& http = fincept::HttpClient::instance();

    auto handle = [cb, endpoint](fincept::Result<QJsonDocument> result) {
        if (result.is_err()) {
            QString err = QString::fromStdString(result.error());
            // Extract HTTP status from "HTTP_NNN" sentinel set by HttpClient
            int status = 0;
            if (err.startsWith("HTTP_")) {
                status = err.mid(5).toInt();
            }
            // Build a meaningful error message from the status
            QString msg;
            switch (status) {
                case 401: msg = "Session expired. Please log in again."; break;
                case 403: msg = "Access denied."; break;
                case 404: msg = "Resource not found."; break;
                case 422: msg = "Invalid request data."; break;
                case 500: msg = "Server error. Please try again."; break;
                default:  msg = status > 0
                              ? QString("Request failed (HTTP %1)").arg(status)
                              : "Network error. Check your connection.";
            }
            cb({false, {}, msg, status});
            return;
        }

        auto doc = result.value();
        auto obj = doc.object();

        // Detect API-level failures: {success: false, message: "..."}
        if (obj.contains("success") && !obj["success"].toBool()) {
            QString msg = obj.value("message").toString(
                          obj.value("detail").toString("Request failed"));
            cb({false, obj, msg, 200});
            return;
        }

        cb({true, obj, {}, 200});
    };

    if (method == "GET")         http.get(endpoint, handle);
    else if (method == "POST")   http.post(endpoint, body, handle);
    else if (method == "PUT")    http.put(endpoint, body, handle);
    else if (method == "DELETE") http.del(endpoint, handle);
}

// ── Health ───────────────────────────────────────────────────────────────────

void AuthApi::check_health(std::function<void(bool)> cb) {
    request("GET", "/health", {}, [cb](ApiResponse r) { cb(r.success); });
}

// ── Unauthenticated auth endpoints ───────────────────────────────────────────

void AuthApi::login(const LoginRequest& req, Callback cb) {
    request("POST", "/user/login", req.to_json(), cb);
}

void AuthApi::register_user(const RegisterRequest& req, Callback cb) {
    request("POST", "/user/register", req.to_json(), cb);
}

void AuthApi::verify_otp(const VerifyOtpRequest& req, Callback cb) {
    request("POST", "/user/verify-otp", req.to_json(), cb);
}

void AuthApi::forgot_password(const ForgotPasswordRequest& req, Callback cb) {
    request("POST", "/user/forgot-password", req.to_json(), cb);
}

void AuthApi::reset_password(const ResetPasswordRequest& req, Callback cb) {
    request("POST", "/user/reset-password", req.to_json(), cb);
}

void AuthApi::verify_mfa(const QString& email, const QString& otp, Callback cb) {
    QJsonObject body;
    body["email"] = email;
    body["otp"] = otp;
    request("POST", "/user/verify-mfa", body, cb);
}

void AuthApi::get_auth_status(Callback cb) {
    request("GET", "/auth/status", {}, cb);
}

// ── Authenticated endpoints (HttpClient carries X-API-Key + X-Session-Token) ─

void AuthApi::logout(Callback cb) {
    request("POST", "/user/logout", {}, cb);
}

void AuthApi::validate_session_server(Callback cb) {
    request("POST", "/user/validate-session", {}, cb);
}

void AuthApi::session_pulse(Callback cb) {
    request("GET", "/user/session-pulse", {}, cb);
}

void AuthApi::get_user_profile(Callback cb) {
    request("GET", "/user/profile", {}, cb);
}

void AuthApi::update_user_profile(const QJsonObject& data, Callback cb) {
    request("PUT", "/user/profile", data, cb);
}

void AuthApi::validate_api_key(Callback cb) {
    request("GET", "/auth/validate", {}, cb);
}

void AuthApi::regenerate_api_key(Callback cb) {
    request("POST", "/user/regenerate-api-key", {}, cb);
}

// ── Subscription / payment ────────────────────────────────────────────────────

void AuthApi::get_subscription_plans(Callback cb) {
    request("GET", "/cashfree/plans", {}, cb);
}

void AuthApi::create_payment_order(const QString& plan_id, Callback cb) {
    QJsonObject body;
    body["plan_id"] = plan_id;
    body["currency"] = QStringLiteral("USD");
    request("POST", "/cashfree/create-order", body, cb);
}

} // namespace fincept::auth
