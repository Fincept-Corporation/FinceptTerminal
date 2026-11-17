#pragma once
#include "auth/AuthTypes.h"

#include <QByteArray>
#include <QMap>
#include <QObject>

#include <functional>

namespace fincept::auth {

/// User profile, notifications, subscriptions, support.
/// All methods use the api_key/session_token already set on HttpClient.
class UserApi : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(ApiResponse)>;

    static UserApi& instance();

    /// Every method takes an optional `context` that is forwarded to HttpClient,
    /// which auto-disconnects the reply handler when that object is destroyed.
    /// Pass `this` from any screen or widget — omitting it scopes the callback
    /// to the UserApi singleton, i.e. to the whole app lifetime, so a callback
    /// fires into a destroyed screen. nullptr is correct only for callers that
    /// live as long as the app (AuthManager, SessionGuard).

    // Profile
    void get_user_profile(Callback cb, const QObject* context = nullptr);
    void update_user_profile(const QJsonObject& data, Callback cb, const QObject* context = nullptr);
    /// Low-level call. The old key stops working the moment the server answers,
    /// so use AuthManager::regenerate_api_key(), which adopts the new one.
    void regenerate_api_key(Callback cb, const QObject* context = nullptr);
    void get_user_usage(int days, Callback cb, const QObject* context = nullptr);
    /// Credit fields of GET /user/profile, reshaped to {"success":true,"data":{credit_balance,
    /// credit_balance_display, account_type, effective_plan, credits_expire_at}}.
    void get_user_credits(Callback cb, const QObject* context = nullptr);
    void delete_user_account(const QString& confirm_email, const QString& password, Callback cb,
                             const QObject* context = nullptr);

    // Login history
    void get_login_history(int limit, int offset, Callback cb, const QObject* context = nullptr);

    // MFA
    void enable_mfa(Callback cb, const QObject* context = nullptr);
    /// POST /user/mfa/disable {"password"} — the API refuses it without the password.
    void disable_mfa(const QString& password, Callback cb, const QObject* context = nullptr);

    // Subscriptions — GET /cashfree/subscription (plan, credits, expiry, support type).
    // With `if_none_match` (an earlier answer's ETag for the same key) an unchanged
    // record comes back as success with status_code 304 and empty data, once the
    // server supports it; until then the header is ignored and a full answer comes.
    void get_user_subscription(Callback cb, const QObject* context = nullptr, const QString& if_none_match = {});

    // Payment — GET /cashfree/payments (plan purchases, paginated)
    void get_payment_history(int page, int limit, Callback cb, const QObject* context = nullptr);

    // Support
    void get_tickets(Callback cb, const QObject* context = nullptr);
    void get_ticket_details(int ticket_id, Callback cb, const QObject* context = nullptr);
    void create_ticket(const QString& subject, const QString& description, const QString& category,
                       const QString& priority, Callback cb, const QObject* context = nullptr);
    void add_ticket_message(int ticket_id, const QString& message, Callback cb, const QObject* context = nullptr);
    void update_ticket_status(int ticket_id, const QString& status, Callback cb, const QObject* context = nullptr);
    void get_support_categories(Callback cb, const QObject* context = nullptr);

  private:
    UserApi() = default;

    void request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb,
                 const QObject* context = nullptr, const QMap<QByteArray, QByteArray>& extra_headers = {});
};

} // namespace fincept::auth
