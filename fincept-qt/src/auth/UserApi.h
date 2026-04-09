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

    // Profile
    void get_user_profile(Callback cb);
    void update_user_profile(const QJsonObject& data, Callback cb);
    void regenerate_api_key(Callback cb);
    void get_user_usage(int days, Callback cb);
    void get_user_credits(Callback cb);
    void delete_user_account(Callback cb);

    // Session
    void logout(Callback cb);
    void validate_session(Callback cb);

    // Login history
    void get_login_history(int limit, int offset, Callback cb);

    // MFA
    void enable_mfa(Callback cb);
    void disable_mfa(Callback cb);

    // Subscriptions
    void get_user_subscriptions(Callback cb);
    void subscribe_to_database(const QString& db_name, Callback cb);
    void get_user_transactions(Callback cb);

    // Payment
    void get_user_subscription(Callback cb);
    void get_payment_history(int page, int limit, Callback cb);

    // Support
    void get_tickets(Callback cb);
    void get_ticket_details(int ticket_id, Callback cb);
    void create_ticket(const QString& subject, const QString& description, const QString& category,
                       const QString& priority, Callback cb);
    void add_ticket_message(int ticket_id, const QString& message, Callback cb);
    void update_ticket_status(int ticket_id, const QString& status, Callback cb);
    void get_support_categories(Callback cb);
    void submit_feedback(int rating, const QString& feedback_text, const QString& category, Callback cb);
    void submit_contact_form(const QString& name, const QString& email, const QString& subject, const QString& message,
                             Callback cb);

  private:
    UserApi() = default;

    void request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb);
};

} // namespace fincept::auth
