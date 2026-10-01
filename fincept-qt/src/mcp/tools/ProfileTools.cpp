// ProfileTools.cpp — Profile tab MCP tools (14 tools)
// Covers: profile, session, security, usage, billing, notifications.

#include "mcp/tools/ProfileTools.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "auth/UserApi.h"
#include "core/logging/Logger.h"
#include "mcp/tools/ThreadHelper.h"
#include "services/notifications/NotificationService.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <memory>

namespace fincept::mcp::tools {

using namespace fincept::auth;

// Upper bound on one UserApi round trip. Generous on purpose: the mutating calls
// (regenerate_api_key, MFA) must not report "timed out" while the server is still
// about to apply them, or the model retries a change that already happened.
static constexpr int kProfileUserApiWaitMs = 60000;

// ── Sync helper for UserApi callbacks ────────────────────────────────────────

// The old version spun a QEventLoop on the calling (worker) thread and called
// UserApi — a main-thread QObject whose HttpClient owns the QNetworkAccessManager —
// directly from it: the cross-thread hazard ThreadHelper.h documents, and a nested
// event loop when invoked from the UI thread. Marshal the request to the service's
// thread instead. Result state lives on the heap so a reply that arrives after the
// bounded wait has given up writes into live memory, not a dead stack frame.
struct ProfileApiWaitState {
    bool ok = false;
    QString err = QStringLiteral("API request timed out");
    QJsonObject data;
};

// The server's profile / subscription payloads are passed through to the model, and
// a profile document can carry the account's API key or session token. Strip those
// exact credential keys at any depth — profile_get_api_key is the single, gated
// route to key material.
static void profile_scrub_secrets(QJsonObject& obj) {
    static const QStringList kSecretKeys = {"api_key", "session_token", "access_token", "refresh_token", "password"};
    for (const auto& k : kSecretKeys)
        obj.remove(k);
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().isObject()) {
            QJsonObject child = it.value().toObject();
            profile_scrub_secrets(child);
            it.value() = child;
        }
    }
}

static ToolResult run_user_api(std::function<void(UserApi::Callback)> trigger) {
    auto st = std::make_shared<ProfileApiWaitState>();

    detail::run_async_wait(
        &UserApi::instance(),
        [st, trigger](auto signal_done) {
            trigger([st, signal_done](ApiResponse resp) {
                st->ok = resp.success;
                st->data = resp.data;
                st->err = resp.error;
                signal_done();
            });
        },
        kProfileUserApiWaitMs);

    if (!st->ok)
        return ToolResult::fail(st->err.isEmpty() ? "API request failed" : st->err);

    profile_scrub_secrets(st->data);

    // Return array or object depending on what's in data
    if (st->data.isEmpty())
        return ToolResult::ok("OK");

    // If data has a single array-valued key, unwrap it
    if (st->data.size() == 1) {
        auto it = st->data.begin();
        if (it.value().isArray())
            return ToolResult::ok_data(it.value().toArray());
    }

    return ToolResult::ok_data(st->data);
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
        t.is_destructive = true; // edits the account record on the server
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
            // to_json() carries api_key and session_token verbatim. Returning it here
            // handed the live key to any tool loop WITHOUT the ExplicitConfirm gate
            // profile_get_api_key (below) exists to enforce — the gate was moot while
            // this ungated sibling echoed the same secret. to_persisted_json() is the
            // existing secrets-stripped view.
            return ToolResult::ok_data(sess.to_persisted_json());
        };
        tools.push_back(std::move(t));
    }

    // ── profile_get_api_key ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "profile_get_api_key";
        t.description = "Get the current user's API key from the active session. Returns live key material — "
                        "requires explicit user confirmation.";
        t.category = "profile";
        // Reading a secret is not "destructive" under the current model, so
        // nothing gated this: it returned the session API key verbatim to
        // whatever was driving the tool loop, including a remote agent acting
        // on prompt-injected instructions. ExplicitConfirm is the only level
        // that fails closed without a checker installed.
        t.auth_required = AuthLevel::ExplicitConfirm;
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
                        "immediately invalidated. The new key is not returned here (it is shown on the Profile "
                        "screen); requires explicit user confirmation.";
        t.category = "profile";
        // Invalidates the live key this terminal authenticates with and returns the
        // new secret to whoever drove the tool loop — same exposure profile_get_api_key
        // gates, plus an irreversible state change. Fails closed like that tool.
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
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
            {"limit", QJsonObject{{"type", "integer"},
                                  {"description", "Max entries to return (default: 20, max 100)"},
                                  {"minimum", 1},
                                  {"maximum", 100}}},
            {"offset", QJsonObject{{"type", "integer"},
                                   {"description", "Offset for pagination (default: 0)"},
                                   {"minimum", 0}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int limit = std::clamp(args["limit"].toInt(20), 1, 100);
            int offset = std::max(0, args["offset"].toInt(0));
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
        t.is_destructive = true; // changes account security settings
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
        // Weakens account security; a prompt-injected tool loop must never be able to
        // do this unattended, so fail closed like profile_get_api_key.
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
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
            {"days", QJsonObject{{"type", "integer"},
                                 {"description", "Number of days to look back (default: 30, max 365)"},
                                 {"minimum", 1},
                                 {"maximum", 365}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int days = std::clamp(args["days"].toInt(30), 1, 365);
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
            {"page", QJsonObject{{"type", "integer"}, {"description", "Page number (default: 1)"}, {"minimum", 1}}},
            {"limit", QJsonObject{{"type", "integer"},
                                  {"description", "Items per page (default: 20, max 100)"},
                                  {"minimum", 1},
                                  {"maximum", 100}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int page = std::max(1, args["page"].toInt(1));
            int limit = std::clamp(args["limit"].toInt(20), 1, 100);
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
            {"limit", QJsonObject{{"type", "integer"},
                                  {"description", "Max notifications (default: 20, max 100)"},
                                  {"minimum", 1},
                                  {"maximum", 100}}},
            {"offset", QJsonObject{{"type", "integer"},
                                   {"description", "Pagination offset (default: 0)"},
                                   {"minimum", 0}}},
            {"unread_only",
             QJsonObject{{"type", "boolean"}, {"description", "Only return unread notifications (default: false)"}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const int limit = std::clamp(args["limit"].toInt(20), 1, 100);
            const int offset = std::max(0, args["offset"].toInt(0));
            const bool unread = args["unread_only"].toBool(false);

            using namespace fincept::notifications;
            auto& svc = NotificationService::instance();

            // history() hands back a reference into the service's own vector, which
            // the UI thread appends to as notifications arrive — iterating it from a
            // worker is a data race. Read it on the service's thread.
            QJsonArray arr;
            int matched = 0;
            detail::run_on_target_thread_sync(&svc, [&]() {
                int skipped = 0;
                for (const auto& rec : svc.history()) {
                    if (unread && rec.read)
                        continue;
                    ++matched;
                    if (skipped < offset) {
                        ++skipped;
                        continue;
                    }
                    if (arr.size() >= limit)
                        continue; // keep counting so `total` is the real match count

                    QJsonObject obj;
                    obj["id"] = rec.id;
                    obj["title"] = rec.request.title;
                    obj["message"] = rec.request.message;
                    obj["read"] = rec.read;
                    obj["time"] = rec.received_at.toString(Qt::ISODate);
                    arr.append(obj);
                }
            });

            // `total` used to be arr.size() — the page length, indistinguishable from
            // "that is everything". Report the real match count and say when the page
            // does not reach the end, so a clipped list is never read as complete.
            const int returned = static_cast<int>(arr.size());
            const bool has_more = offset + returned < matched;
            QJsonObject result;
            result["notifications"] = arr;
            result["total"] = matched;
            result["returned"] = returned;
            result["has_more"] = has_more;
            return ToolResult::ok(has_more ? QStringLiteral("Showing %1 of %2 notifications — pass offset=%3 for the next page")
                                                 .arg(returned)
                                                 .arg(matched)
                                                 .arg(offset + returned)
                                           : QStringLiteral("OK"),
                                  QJsonValue(result));
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
            // Mutates the service's history and emits UI signals — do it on its thread.
            auto& svc = fincept::notifications::NotificationService::instance();
            detail::run_on_target_thread_sync(&svc, [&svc, id]() { svc.mark_read(id); });
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
            auto& svc = fincept::notifications::NotificationService::instance();
            detail::run_on_target_thread_sync(&svc, [&svc]() { svc.mark_all_read(); });
            return ToolResult::ok("All notifications marked as read");
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
