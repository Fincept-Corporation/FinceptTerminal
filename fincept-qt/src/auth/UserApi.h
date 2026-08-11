#pragma once
#include "auth/AuthTypes.h"

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
    void regenerate_api_key(Callback cb, const QObject* context = nullptr);
    void get_user_usage(int days, Callback cb, const QObject* context = nullptr);
    void get_user_credits(Callback cb, const QObject* context = nullptr);
    void delete_user_account(const QString& confirm_email, const QString& password, Callback cb,
                             const QObject* context = nullptr);

    // Login history
    void get_login_history(int limit, int offset, Callback cb, const QObject* context = nullptr);

    // MFA
    void enable_mfa(Callback cb, const QObject* context = nullptr);
    void disable_mfa(Callback cb, const QObject* context = nullptr);

    // Subscriptions
    void get_user_subscription(Callback cb, const QObject* context = nullptr);

    // Payment
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
                 const QObject* context = nullptr);
};

} // namespace fincept::auth
