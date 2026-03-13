#pragma once
// Auth API service — mirrors AuthApiService from TypeScript exactly
// All endpoints hit api.fincept.in

#include "http/api_types.h"
#include "http/http_client.h"
#include <string>
#include <vector>

namespace fincept::auth {

class AuthApi {
public:
    static AuthApi& instance();

    // Session token management
    void set_session_token(const std::string& token);
    void clear_session_token();
    std::string get_session_token() const;

    // Auth headers for other services to use
    http::Headers get_auth_headers(const std::string& api_key) const;

    // Health check
    bool check_health();

    // Login
    api::ApiResponse login(const api::LoginRequest& req);

    // Register
    api::ApiResponse register_user(const api::RegisterRequest& req);

    // Verify OTP
    api::ApiResponse verify_otp(const api::VerifyOtpRequest& req);

    // Verify MFA
    api::ApiResponse verify_mfa(const std::string& email, const std::string& otp);

    // Forgot password
    api::ApiResponse forgot_password(const api::ForgotPasswordRequest& req);

    // Reset password
    api::ApiResponse reset_password(const api::ResetPasswordRequest& req);

    // Guest session
    api::ApiResponse create_guest_session(const api::DeviceRegisterRequest& req);

    // Get user profile
    api::ApiResponse get_user_profile(const std::string& api_key);

    // Get auth status
    api::ApiResponse get_auth_status(const std::string& api_key, const std::string& device_id);

    // Validate session (server-side)
    api::ApiResponse validate_session_server(const std::string& api_key);

    // Logout
    api::ApiResponse logout(const std::string& api_key);

    // Get subscription plans
    api::ApiResponse get_subscription_plans();

    // Get user subscription
    api::ApiResponse get_user_subscription(const std::string& api_key);

private:
    AuthApi() = default;

    static constexpr const char* BASE_URL = "https://api.fincept.in";

    std::string session_token_;

    std::string endpoint(const std::string& path) const;
    api::ApiResponse make_request(const std::string& method, const std::string& path,
                                   const api::json& body = {},
                                   const http::Headers& extra_headers = {});
};

} // namespace fincept::auth
