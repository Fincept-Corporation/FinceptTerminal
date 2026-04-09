// ProfileTools.cpp — Profile tab MCP tools (14 tools)
// Covers: profile, session, security, usage, billing, notifications.

#include "mcp/tools/ProfileTools.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "auth/UserApi.h"
#include "core/logging/Logger.h"
#include "services/notifications/NotificationService.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

namespace fincept::mcp::tools {

using namespace fincept::auth;

static constexpr int kTimeoutMs = 15000;

// ── Sync helper for UserApi callbacks ────────────────────────────────────────

static ToolResult run_user_api(std::function<void(UserApi::Callback)> trigger) {
    bool ok = false;
    QString err;
    QJsonObject data;
    bool fired = false;

    QEventLoop loop;
    QTimer::singleShot(kTimeoutMs, &loop, &QEventLoop::quit);

    trigger([&](ApiResponse resp) {
        fired = true;
        ok = resp.success;
        data = resp.data;
        err = resp.error;
        loop.quit();
    });

    if (!fired)
        loop.exec();

    if (!ok)
        return ToolResult::fail(err.isEmpty() ? "API request failed" : err);

    // Return array or object depending on what's in data
    if (data.isEmpty())
        return ToolResult::ok("OK");

    // If data has a single array-valued key, unwrap it
    if (data.size() == 1) {
        auto it = data.begin();
        if (it.value().isArray())
            return ToolResult::ok_data(it.value().toArray());
    }

    return ToolResult::ok_data(data);
}

// ── Tool registration ─────────────────────────────────────────────────────────

std::vector<ToolDef> get_profile_tools() {
    std::vector<ToolDef> tools;

    // ── profile_get ──────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get";
        t.description = "Get the current user's full profile: username, email, account type, "
                        "credit balance, verification status, MFA status, phone, country, "
                        "created_at, last_login_at.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_user_api([](auto cb) { UserApi::instance().get_user_profile(cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_update ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_update";
        t.description = "Update the current user's profile fields. "
                        "Only include fields you want to change (username, phone, country).";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{
            {"username", QJsonObject{{"type", "string"}, {"description", "New username"}}},
            {"phone", QJsonObject{{"type", "string"}, {"description", "Phone number"}}},
            {"country", QJsonObject{{"type", "string"}, {"description", "Country name"}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QJsonObject body;
            if (args.contains("username") && !args["username"].toString().isEmpty())
                body["username"] = args["username"].toString();
            if (args.contains("phone"))
                body["phone"] = args["phone"].toString();
            if (args.contains("country"))
                body["country"] = args["country"].toString();
            if (body.isEmpty())
                return ToolResult::fail("No fields provided to update");

            return run_user_api([body](auto cb) { UserApi::instance().update_user_profile(body, cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_session ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_session";
        t.description = "Get current session information: authentication state, username, "
                        "email, account type, credit balance, subscription details, MFA status.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            const auto& sess = AuthManager::instance().session();
            if (!sess.authenticated)
                return ToolResult::fail("Not authenticated");
            return ToolResult::ok_data(sess.to_json());
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_api_key ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_api_key";
        t.description = "Get the current user's API key from the active session.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            const auto& sess = AuthManager::instance().session();
            if (!sess.authenticated)
                return ToolResult::fail("Not authenticated");
            if (sess.api_key.isEmpty())
                return ToolResult::fail("No API key in session");
            return ToolResult::ok_data(QJsonObject{{"api_key", sess.api_key}});
        };
        tools.push_back(std::move(t));
    }

    // ── profile_regenerate_api_key ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_regenerate_api_key";
        t.description = "Regenerate the API key. WARNING: the current API key will be "
                        "immediately invalidated. Returns the new API key.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_user_api([](auto cb) { UserApi::instance().regenerate_api_key(cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_login_history ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_login_history";
        t.description = "Get login history entries (timestamp, IP address, status). "
                        "Useful for security auditing.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Max entries to return (default: 20)"}}},
            {"offset", QJsonObject{{"type", "integer"}, {"description", "Offset for pagination (default: 0)"}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int limit = args["limit"].toInt(20);
            int offset = args["offset"].toInt(0);
            return run_user_api([limit, offset](auto cb) { UserApi::instance().get_login_history(limit, offset, cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_enable_mfa ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_enable_mfa";
        t.description = "Enable two-factor authentication (MFA/2FA) for the account.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_user_api([](auto cb) { UserApi::instance().enable_mfa(cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_disable_mfa ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_disable_mfa";
        t.description = "Disable two-factor authentication (MFA/2FA) for the account.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_user_api([](auto cb) { UserApi::instance().disable_mfa(cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_usage ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_usage";
        t.description = "Get API usage statistics for the specified number of days. "
                        "Returns summary (total requests, credits used, avg response time), "
                        "daily breakdown, and top endpoints by usage.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{
            {"days", QJsonObject{{"type", "integer"}, {"description", "Number of days to look back (default: 30)"}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int days = args["days"].toInt(30);
            return run_user_api([days](auto cb) { UserApi::instance().get_user_usage(days, cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_credits ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_credits";
        t.description = "Get the current credit balance for the account.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_user_api([](auto cb) { UserApi::instance().get_user_credits(cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_subscription ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_subscription";
        t.description = "Get the current subscription details: plan/account type, "
                        "credit balance, credits expiry, support type.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_user_api([](auto cb) { UserApi::instance().get_user_subscription(cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_payment_history ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_payment_history";
        t.description = "Get payment transaction history: date, plan name, amount (USD), "
                        "credits purchased, status.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{
            {"page", QJsonObject{{"type", "integer"}, {"description", "Page number (default: 1)"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Items per page (default: 20)"}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int page = args["page"].toInt(1);
            int limit = args["limit"].toInt(20);
            return run_user_api([page, limit](auto cb) { UserApi::instance().get_payment_history(page, limit, cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_notifications ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_notifications";
        t.description = "Get in-app notification history. Can filter to unread only.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Max notifications (default: 20)"}}},
            {"offset", QJsonObject{{"type", "integer"}, {"description", "Pagination offset (default: 0)"}}},
            {"unread_only",
             QJsonObject{{"type", "boolean"}, {"description", "Only return unread notifications (default: false)"}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int limit = args["limit"].toInt(20);
            int offset = args["offset"].toInt(0);
            bool unread = args["unread_only"].toBool(false);

            using namespace fincept::notifications;
            const auto& history = NotificationService::instance().history();

            QJsonArray arr;
            int count = 0;
            int skipped = 0;
            for (const auto& rec : history) {
                if (unread && rec.read)
                    continue;
                if (skipped < offset) {
                    ++skipped;
                    continue;
                }
                if (count >= limit)
                    break;

                QJsonObject obj;
                obj["id"] = rec.id;
                obj["title"] = rec.request.title;
                obj["message"] = rec.request.message;
                obj["read"] = rec.read;
                obj["time"] = rec.received_at.toString(Qt::ISODate);
                arr.append(obj);
                ++count;
            }

            QJsonObject result;
            result["notifications"] = arr;
            result["total"] = arr.size();
            return ToolResult::ok("OK", QJsonValue(result));
        };
        tools.push_back(std::move(t));
    }

    // ── profile_mark_notification_read ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_mark_notification_read";
        t.description = "Mark a specific in-app notification as read by its ID.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "integer"}, {"description", "Notification ID to mark as read"}}},
        };
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args["id"].toInt(-1);
            if (id < 0)
                return ToolResult::fail("Missing or invalid 'id'");
            fincept::notifications::NotificationService::instance().mark_read(id);
            QJsonObject result;
            result["success"] = true;
            result["id"] = id;
            return ToolResult::ok("Marked as read", QJsonValue(result));
        };
        tools.push_back(std::move(t));
    }

    // ── profile_mark_all_notifications_read ──────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_mark_all_notifications_read";
        t.description = "Mark all in-app notifications as read at once.";
        t.category = "profile";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            fincept::notifications::NotificationService::instance().mark_all_read();
            return ToolResult::ok("All notifications marked as read");
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
