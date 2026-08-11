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

    /// Every method takes an optional `context` that is forwarded to HttpClient,
    /// which auto-disconnects the reply handler when that object is destroyed.
    /// Pass `this` from any screen or widget — omitting it scopes the callback
    /// to the AuthApi singleton, i.e. to the whole app lifetime, so a callback
    /// fires into a destroyed screen. nullptr is correct only for callers that
    /// live as long as the app (AuthManager, SessionGuard).

    // Auth (unauthenticated — no API key needed)
    void login(const LoginRequest& req, Callback cb, const QObject* context = nullptr);
    void register_user(const RegisterRequest& req, Callback cb, const QObject* context = nullptr);
    void verify_otp(const VerifyOtpRequest& req, Callback cb, const QObject* context = nullptr);
    void forgot_password(const ForgotPasswordRequest& req, Callback cb, const QObject* context = nullptr);
    void reset_password(const ResetPasswordRequest& req, Callback cb, const QObject* context = nullptr);
    void verify_mfa(const QString& email, const QString& otp, Callback cb, const QObject* context = nullptr);

    // Desktop Google login: exchange a one-time handoff code (from the browser
    // loopback redirect) for a real session. Unauthenticated.
    void redeem_desktop_handoff(const QString& code, Callback cb, const QObject* context = nullptr);

    // Authenticated (uses api_key stored on HttpClient)
    void logout(Callback cb, const QObject* context = nullptr);
    void session_pulse(Callback cb, const QObject* context = nullptr);
    void get_user_profile(Callback cb, const QObject* context = nullptr);

    // Subscription / payment
    void get_subscription_plans(Callback cb, const QObject* context = nullptr);
    void generate_checkout_token(const QString& plan_id, Callback cb, const QObject* context = nullptr);

  private:
    AuthApi() = default;

    void request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb,
                 const QObject* context = nullptr);
};

} // namespace fincept::auth
