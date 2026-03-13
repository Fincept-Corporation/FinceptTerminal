#pragma once
// Auth session manager — mirrors AuthContext from TypeScript
// Manages session lifecycle, persistence, and API auth state

#include "http/api_types.h"
#include <string>
#include <functional>
#include <future>
#include <mutex>

namespace fincept::auth {

// Auth operation result
struct AuthResult {
    bool success = false;
    std::string error;
    bool mfa_required = false;
    bool active_session = false;
};

class AuthManager {
public:
    static AuthManager& instance();

    // Initialize — loads saved session and validates
    void initialize();

    // Auth operations (blocking — call from background thread)
    AuthResult login(const std::string& email, const std::string& password, bool force_login = false);
    AuthResult signup(const std::string& username, const std::string& email, const std::string& password,
                      const std::string& phone = "", const std::string& country = "",
                      const std::string& country_code = "");
    AuthResult verify_otp(const std::string& email, const std::string& otp);
    AuthResult verify_mfa(const std::string& email, const std::string& otp);
    AuthResult setup_guest_access();
    AuthResult forgot_password(const std::string& email);
    AuthResult reset_password(const std::string& email, const std::string& otp, const std::string& new_password);
    void logout();
    void refresh_user_data();

    // State queries (thread-safe)
    bool is_authenticated() const;
    bool is_loading() const;
    bool is_logging_out() const;
    const api::SessionData& session() const;
    bool has_session() const;

    // Async wrappers — run auth operations off main thread
    void login_async(const std::string& email, const std::string& password, bool force_login = false);
    void signup_async(const std::string& username, const std::string& email, const std::string& password,
                      const std::string& phone = "", const std::string& country = "",
                      const std::string& country_code = "");
    void verify_otp_async(const std::string& email, const std::string& otp);
    void verify_mfa_async(const std::string& email, const std::string& otp);
    void setup_guest_async();
    void forgot_password_async(const std::string& email);
    void reset_password_async(const std::string& email, const std::string& otp, const std::string& new_password);
    void logout_async();

    // Poll for async result (call from main/render thread)
    bool has_pending_result() const;
    AuthResult consume_result();

    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

private:
    AuthManager() = default;

    api::SessionData session_;
    mutable std::mutex mutex_;
    bool loading_ = true;
    bool logging_out_ = false;

    // Async result
    std::future<AuthResult> pending_future_;
    AuthResult last_result_;
    bool result_ready_ = false;

    // Session persistence
    void save_session();
    bool load_session();
    void clear_session();

    // Internal helpers
    api::UserProfile fetch_profile(const std::string& api_key);
    api::UserSubscription fetch_subscription(const std::string& api_key);
    api::SessionData validate_session(const api::SessionData& saved);

    void run_async(std::function<AuthResult()> fn);
};

} // namespace fincept::auth
