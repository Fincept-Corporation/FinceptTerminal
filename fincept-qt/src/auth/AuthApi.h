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

    // Auth (unauthenticated — no API key needed)
    void login(const LoginRequest& req, Callback cb);
    void register_user(const RegisterRequest& req, Callback cb);
    void verify_otp(const VerifyOtpRequest& req, Callback cb);
    void forgot_password(const ForgotPasswordRequest& req, Callback cb);
    void reset_password(const ResetPasswordRequest& req, Callback cb);
    void verify_mfa(const QString& email, const QString& otp, Callback cb);

    // Desktop Google login: exchange a one-time handoff code (from the browser
    // loopback redirect) for a real session. Unauthenticated.
    void redeem_desktop_handoff(const QString& code, Callback cb);

    // Authenticated (uses api_key stored on HttpClient)
    void logout(Callback cb);
    void session_pulse(Callback cb);
    void get_user_profile(Callback cb);

    // Subscription / payment
    void get_subscription_plans(Callback cb);
    void generate_checkout_token(const QString& plan_id, Callback cb);

  private:
    AuthApi() = default;

    void request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb);
};

} // namespace fincept::auth
