#include "auth/UserApi.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QJsonDocument>

namespace fincept::auth {

UserApi& UserApi::instance() {
    static UserApi s;
    return s;
}

void UserApi::request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb,
                      const QObject* context) {
    auto& http = fincept::HttpClient::instance();

    auto handle = [cb](fincept::Result<QJsonDocument> result) {
        if (result.is_err()) {
            const int status = fincept::HttpClient::status_from_error(result.error());
            // Prefer the server's own wording — HttpClient carries it in the
            // error string ("HTTP_<status>: <message>") rather than handing the
            // error body back as ok(). 401/403 stay message-free by design and
            // fall through to our own wording below.
            QString msg = status > 0 ? fincept::HttpClient::message_from_error(result.error()) : QString{};
            if (!msg.isEmpty()) {
                cb({false, {}, msg, status});
                return;
            }
            switch (status) {
                case 401:
                    msg = "Session expired. Please log in again.";
                    break;
                case 403:
                    msg = "Access denied.";
                    break;
                case 404:
                    msg = "Resource not found.";
                    break;
                case 422:
                    msg = "Invalid request data.";
                    break;
                case 500:
                    msg = "Server error. Please try again.";
                    break;
                default:
                    msg = status > 0 ? QString("Request failed (HTTP %1)").arg(status)
                                     : "Network error. Check your connection.";
            }
            cb({false, {}, msg, status});
            return;
        }

        auto obj = result.value().object();
        if (obj.contains("success") && !obj["success"].toBool()) {
            QString msg = obj.value("message").toString(obj.value("detail").toString("Request failed"));
            cb({false, obj, msg, 200});
            return;
        }

        cb({true, obj, {}, 200});
    };

    // Forward `context` so HttpClient auto-disconnects the handler if the caller
    // is destroyed mid-flight. Dropping it scoped every callback to the UserApi
    // singleton instead, i.e. to the whole app lifetime — which is the opposite
    // of what HttpClient's `context` parameter exists for.
    if (method == "GET")
        http.get(endpoint, handle, context);
    else if (method == "POST")
        http.post(endpoint, body, handle, context);
    else if (method == "PUT")
        http.put(endpoint, body, handle, context);
    else if (method == "DELETE") {
        if (body.isEmpty())
            http.del(endpoint, handle, context);
        else
            http.del(endpoint, body, handle, context);
    }
}

// ── Profile ──────────────────────────────────────────────────────────────────

void UserApi::get_user_profile(Callback cb, const QObject* context) {
    request("GET", "/user/profile", {}, cb, context);
}
void UserApi::update_user_profile(const QJsonObject& data, Callback cb, const QObject* context) {
    request("PUT", "/user/profile", data, cb, context);
}
void UserApi::regenerate_api_key(Callback cb, const QObject* context) {
    request("POST", "/user/regenerate-api-key", {}, cb, context);
}
void UserApi::get_user_credits(Callback cb, const QObject* context) {
    request("GET", "/user/credits", {}, cb, context);
}
void UserApi::delete_user_account(const QString& confirm_email, const QString& password, Callback cb,
                                  const QObject* context) {
    QJsonObject body;
    body.insert("confirm", true);
    body.insert("email", confirm_email);
    body.insert("password", password);
    request("DELETE", "/user/account", body, cb, context);
}

void UserApi::get_user_usage(int days, Callback cb, const QObject* context) {
    request("GET", QString("/user/usage?days=%1").arg(days), {}, cb, context);
}

// ── Login history ────────────────────────────────────────────────────────────

void UserApi::get_login_history(int limit, int offset, Callback cb, const QObject* context) {
    request("GET", QString("/user/login-history?limit=%1&offset=%2").arg(limit).arg(offset), {}, cb, context);
}

// ── MFA ──────────────────────────────────────────────────────────────────────

void UserApi::enable_mfa(Callback cb, const QObject* context) {
    request("POST", "/user/mfa/enable", {}, cb, context);
}
void UserApi::disable_mfa(Callback cb, const QObject* context) {
    request("POST", "/user/mfa/disable", {}, cb, context);
}

// ── Subscriptions ────────────────────────────────────────────────────────────

void UserApi::get_user_subscription(Callback cb, const QObject* context) {
    request("GET", "/user/subscriptions", {}, cb, context);
}

// ── Payment ──────────────────────────────────────────────────────────────────

void UserApi::get_payment_history(int page, int limit, Callback cb, const QObject* context) {
    request("GET", QString("/user/transactions?page=%1&limit=%2").arg(page).arg(limit), {}, cb, context);
}

// ── Support ──────────────────────────────────────────────────────────────────

void UserApi::get_tickets(Callback cb, const QObject* context) {
    request("GET", "/support/tickets", {}, cb, context);
}

void UserApi::get_ticket_details(int ticket_id, Callback cb, const QObject* context) {
    request("GET", QString("/support/tickets/%1").arg(ticket_id), {}, cb, context);
}

void UserApi::create_ticket(const QString& subject, const QString& description, const QString& category,
                            const QString& priority, Callback cb, const QObject* context) {
    QJsonObject body;
    body["subject"] = subject;
    body["description"] = description;
    body["category"] = category;
    body["priority"] = priority;
    request("POST", "/support/tickets", body, cb, context);
}

void UserApi::add_ticket_message(int ticket_id, const QString& message, Callback cb, const QObject* context) {
    QJsonObject body;
    body["message"] = message;
    request("POST", QString("/support/tickets/%1/messages").arg(ticket_id), body, cb, context);
}

void UserApi::update_ticket_status(int ticket_id, const QString& status, Callback cb, const QObject* context) {
    QJsonObject body;
    body["status"] = status;
    request("PUT", QString("/support/tickets/%1").arg(ticket_id), body, cb, context);
}

void UserApi::get_support_categories(Callback cb, const QObject* context) {
    request("GET", "/support/categories", {}, cb, context);
}

} // namespace fincept::auth
