#pragma once
#include "auth/AuthTypes.h"

#include <QObject>

#include <functional>

namespace fincept::auth {

/// Stateless API calls for authentication endpoints.
/// All token state lives in HttpClient — AuthApi has no state of its own.
class AuthApi : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(ApiResponse)>;

    static AuthApi& instance();

    // Health
    void check_health(std::function<void(bool)> cb);

    // Auth (unauthenticated — no API key needed)
    void login(const LoginRequest& req, Callback cb);
    void register_user(const RegisterRequest& req, Callback cb);
    void verify_otp(const VerifyOtpRequest& req, Callback cb);
    void forgot_password(const ForgotPasswordRequest& req, Callback cb);
    void reset_password(const ResetPasswordRequest& req, Callback cb);
    void verify_mfa(const QString& email, const QString& otp, Callback cb);

    // Authenticated (uses api_key stored on HttpClient)
    void logout(Callback cb);
    void validate_session_server(Callback cb);
    void session_pulse(Callback cb);
    void get_user_profile(Callback cb);
    void update_user_profile(const QJsonObject& data, Callback cb);
    void validate_api_key(Callback cb);
    void regenerate_api_key(Callback cb);

    // Unauthenticated
    void get_auth_status(Callback cb);

    // Subscription / payment
    void get_subscription_plans(Callback cb);
    void create_payment_order(const QString& plan_id, Callback cb);
    void get_transaction_status(const QString& order_id, Callback cb);

  private:
    AuthApi() = default;

    void request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb);
};

} // namespace fincept::auth
