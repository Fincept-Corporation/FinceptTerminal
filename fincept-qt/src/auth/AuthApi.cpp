#include "auth/AuthApi.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QJsonDocument>

#include <memory>

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

void AuthApi::request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb) {
    auto& http = fincept::HttpClient::instance();
    auto& config = fincept::AppConfig::instance();

    auto make_url = [&endpoint](const QString& base) {
        QString normalized = base.trimmed();
        if (normalized.endsWith('/'))
            normalized.chop(1);
        return normalized + endpoint;
    };

    auto hosts = config.auth_base_urls();
    const QString primary = config.api_base_url().trimmed();
    if (!primary.isEmpty() && !hosts.contains(primary))
        hosts.prepend(primary);
    hosts.removeAll(QString{});
    if (hosts.isEmpty())
        hosts = {"https://api.fincept.in"};

    auto handle = [cb, endpoint](fincept::Result<QJsonDocument> result) {
        // ── No body / network error path ─────────────────────────────────────
        // HttpClient returns err() only when there is NO parseable JSON body.
        // In that case we fall back to status-based messages.
        if (result.is_err()) {
            QString err = QString::fromStdString(result.error());
            int status = 0;
            if (err.startsWith("HTTP_"))
                status = err.mid(5).toInt();

            QString msg;
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

        // ── JSON body path (covers both success and all HTTP error responses) ─
        auto doc = result.value();
        auto obj = doc.object();

        // Infer HTTP status from body when available (HttpClient doesn't pass status with ok())
        // We detect error bodies by absence of positive signals below.

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

    auto should_retry = [](const fincept::Result<QJsonDocument>& result) {
        if (!result.is_err())
            return false;
        QString err = QString::fromStdString(result.error());
        int status = 0;
        if (err.startsWith("HTTP_"))
            status = err.mid(5).toInt();
        return status == 0 || status == 502 || status == 503 || status == 504;
    };

    using RawResultCallback = std::function<void(fincept::Result<QJsonDocument>)>;
    auto request_on_host = [&http, &method, &body](const QString& url, RawResultCallback done) {
        if (method == "GET")
            http.get(url, [done](fincept::Result<QJsonDocument> r) { done(std::move(r)); });
        else if (method == "POST")
            http.post(url, body, [done](fincept::Result<QJsonDocument> r) { done(std::move(r)); });
        else if (method == "PUT")
            http.put(url, body, [done](fincept::Result<QJsonDocument> r) { done(std::move(r)); });
        else if (method == "DELETE")
            http.del(url, [done](fincept::Result<QJsonDocument> r) { done(std::move(r)); });
    };

    auto idx = std::make_shared<int>(0);
    auto attempt = std::make_shared<std::function<void()>>();
    *attempt = [hosts, idx, handle, should_retry, request_on_host, make_url, endpoint, attempt]() {
        const QString url = make_url(hosts.at(*idx));
        request_on_host(url, [hosts, idx, handle, should_retry, endpoint, attempt](fincept::Result<QJsonDocument> r) {
            if (should_retry(r) && (*idx + 1) < hosts.size()) {
                const QString failed = hosts.at(*idx);
                ++(*idx);
                const QString next = hosts.at(*idx);
                LOG_WARN("Auth", QString("Retrying %1 on fallback backend: %2 -> %3").arg(endpoint, failed, next));
                (*attempt)();
                return;
            }
            handle(std::move(r));
        });
    };
    (*attempt)();
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

void AuthApi::generate_checkout_token(const QString& plan_id, Callback cb) {
    QJsonObject body;
    body["plan_id"] = plan_id;
    request("POST", "/user/generate-checkout-token", body, cb);
}

} // namespace fincept::auth
