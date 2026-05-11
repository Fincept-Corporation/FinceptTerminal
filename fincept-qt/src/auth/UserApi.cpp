#include "auth/UserApi.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QJsonDocument>

namespace fincept::auth {

UserApi& UserApi::instance() {
    static UserApi s;
    return s;
}

void UserApi::request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb) {
    auto& http = fincept::HttpClient::instance();

    auto handle = [cb](fincept::Result<QJsonDocument> result) {
        if (result.is_err()) {
            QString err = QString::fromStdString(result.error());
            int status = 0;
            if (err.startsWith("HTTP_"))
                status = err.mid(5).toInt();
            QString msg;
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

    if (method == "GET")
        http.get(endpoint, handle);
    else if (method == "POST")
        http.post(endpoint, body, handle);
    else if (method == "PUT")
        http.put(endpoint, body, handle);
    else if (method == "DELETE") {
        if (body.isEmpty())
            http.del(endpoint, handle);
        else
            http.del(endpoint, body, handle);
    }
}

// ── Profile ──────────────────────────────────────────────────────────────────

void UserApi::get_user_profile(Callback cb) {
    request("GET", "/user/profile", {}, cb);
}
void UserApi::update_user_profile(const QJsonObject& data, Callback cb) {
    request("PUT", "/user/profile", data, cb);
}
void UserApi::regenerate_api_key(Callback cb) {
    request("POST", "/user/regenerate-api-key", {}, cb);
}
void UserApi::get_user_credits(Callback cb) {
    request("GET", "/user/credits", {}, cb);
}
void UserApi::delete_user_account(const QString& confirm_email, const QString& password, Callback cb) {
    QJsonObject body;
    body.insert("confirm", true);
    body.insert("email", confirm_email);
    body.insert("password", password);
    request("DELETE", "/user/account", body, cb);
}

void UserApi::get_user_usage(int days, Callback cb) {
    request("GET", QString("/user/usage?days=%1").arg(days), {}, cb);
}

// ── Session ──────────────────────────────────────────────────────────────────

void UserApi::logout(Callback cb) {
    request("POST", "/user/logout", {}, cb);
}
void UserApi::validate_session(Callback cb) {
    request("POST", "/user/validate-session", {}, cb);
}

// ── Login history ────────────────────────────────────────────────────────────

void UserApi::get_login_history(int limit, int offset, Callback cb) {
    request("GET", QString("/user/login-history?limit=%1&offset=%2").arg(limit).arg(offset), {}, cb);
}

// ── MFA ──────────────────────────────────────────────────────────────────────

void UserApi::enable_mfa(Callback cb) {
    request("POST", "/user/mfa/enable", {}, cb);
}
void UserApi::disable_mfa(Callback cb) {
    request("POST", "/user/mfa/disable", {}, cb);
}

// ── Subscriptions ────────────────────────────────────────────────────────────

void UserApi::get_user_subscriptions(Callback cb) {
    request("GET", "/user/subscriptions", {}, cb);
}
void UserApi::get_user_transactions(Callback cb) {
    request("GET", "/user/transactions", {}, cb);
}

void UserApi::subscribe_to_database(const QString& db_name, Callback cb) {
    QJsonObject body;
    body["database_name"] = db_name;
    request("POST", "/user/subscribe", body, cb);
}

// ── Payment ──────────────────────────────────────────────────────────────────

void UserApi::get_user_subscription(Callback cb) {
    request("GET", "/user/subscriptions", {}, cb);
}

void UserApi::get_payment_history(int page, int limit, Callback cb) {
    request("GET", QString("/user/transactions?page=%1&limit=%2").arg(page).arg(limit), {}, cb);
}

// ── Support ──────────────────────────────────────────────────────────────────

void UserApi::get_tickets(Callback cb) {
    request("GET", "/support/tickets", {}, cb);
}

void UserApi::get_ticket_details(int ticket_id, Callback cb) {
    request("GET", QString("/support/tickets/%1").arg(ticket_id), {}, cb);
}

void UserApi::create_ticket(const QString& subject, const QString& description, const QString& category,
                            const QString& priority, Callback cb) {
    QJsonObject body;
    body["subject"] = subject;
    body["description"] = description;
    body["category"] = category;
    body["priority"] = priority;
    request("POST", "/support/tickets", body, cb);
}

void UserApi::add_ticket_message(int ticket_id, const QString& message, Callback cb) {
    QJsonObject body;
    body["message"] = message;
    request("POST", QString("/support/tickets/%1/messages").arg(ticket_id), body, cb);
}

void UserApi::update_ticket_status(int ticket_id, const QString& status, Callback cb) {
    QJsonObject body;
    body["status"] = status;
    request("PUT", QString("/support/tickets/%1").arg(ticket_id), body, cb);
}

void UserApi::get_support_categories(Callback cb) {
    request("GET", "/support/categories", {}, cb);
}

void UserApi::submit_feedback(int rating, const QString& feedback_text, const QString& category, Callback cb) {
    QJsonObject body;
    body["rating"] = rating;
    body["feedback_text"] = feedback_text;
    body["category"] = category;
    request("POST", "/support/feedback", body, cb);
}

void UserApi::submit_contact_form(const QString& name, const QString& email, const QString& subject,
                                  const QString& message, Callback cb) {
    QJsonObject body;
    body["name"] = name;
    body["email"] = email;
    body["subject"] = subject;
    body["message"] = message;
    request("POST", "/support/contact", body, cb);
}

} // namespace fincept::auth
