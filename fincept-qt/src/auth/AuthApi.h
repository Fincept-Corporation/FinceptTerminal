#pragma once
#include "auth/AuthTypes.h"

#include <QByteArray>
#include <QMap>
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
    /// POST /user/resend-otp {"email"} — a new signup verification code for an
    /// account that is not verified yet. Always the same neutral success message
    /// (also when nothing was sent: unknown / verified address, resend cooldown).
    void resend_otp(const QString& email, Callback cb, const QObject* context = nullptr);
    void forgot_password(const ForgotPasswordRequest& req, Callback cb, const QObject* context = nullptr);
    void reset_password(const ResetPasswordRequest& req, Callback cb, const QObject* context = nullptr);
    void verify_mfa(const QString& email, const QString& otp, Callback cb, const QObject* context = nullptr);

    // Desktop Google login: exchange a one-time handoff code (from the browser
    // loopback redirect) for a real session. Unauthenticated.
    void redeem_desktop_handoff(const QString& code, Callback cb, const QObject* context = nullptr);

    // Authenticated (uses api_key stored on HttpClient)
    void logout(Callback cb, const QObject* context = nullptr);
    void session_pulse(Callback cb, const QObject* context = nullptr);
    /// GET /user/profile. With `if_none_match` (the ETag of an earlier answer for
    /// the same key) an unchanged profile comes back as success with
    /// status_code 304 and empty data — keep the cached one. Every answer carries
    /// the response's etag and poll_interval_ms (ApiResponse).
    void get_user_profile(Callback cb, const QObject* context = nullptr, const QString& if_none_match = {});

    /// GET /user/profile with the API key ONLY (no X-Session-Token, whatever
    /// HttpClient holds): a 401 here means the key itself is refused, never a
    /// stale session. Used by session recovery.
    void validate_api_key(Callback cb, const QObject* context = nullptr);

    /// GET /user/session-pulse with `session_token` as X-Session-Token (instead of
    /// whatever HttpClient holds). 200 = the token is still this account's;
    /// 401 = it was replaced (signed in elsewhere / password reset). Used to
    /// check a saved token at startup before putting it on HttpClient.
    void check_session_token(const QString& session_token, Callback cb, const QObject* context = nullptr);

    // Subscription / payment
    void get_subscription_plans(Callback cb, const QObject* context = nullptr);
    void generate_checkout_token(const QString& plan_id, Callback cb, const QObject* context = nullptr);

  private:
    AuthApi() = default;

    void request(const QString& method, const QString& endpoint, const QJsonObject& body, Callback cb,
                 const QObject* context = nullptr, const QMap<QByteArray, QByteArray>& extra_headers = {});
};

} // namespace fincept::auth
