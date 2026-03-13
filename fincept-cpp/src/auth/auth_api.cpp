#include "auth_api.h"
#include <iostream>
#include <algorithm>

namespace fincept::auth {

using json = nlohmann::json;

AuthApi& AuthApi::instance() {
    static AuthApi api;
    return api;
}

void AuthApi::set_session_token(const std::string& token) {
    session_token_ = token;
}

void AuthApi::clear_session_token() {
    session_token_.clear();
}

std::string AuthApi::get_session_token() const {
    return session_token_;
}

http::Headers AuthApi::get_auth_headers(const std::string& api_key) const {
    http::Headers headers;
    headers["X-API-Key"] = api_key;
    if (!session_token_.empty()) {
        headers["X-Session-Token"] = session_token_;
    }
    return headers;
}

std::string AuthApi::endpoint(const std::string& path) const {
    return std::string(BASE_URL) + path;
}

// Centralized request handler matching TypeScript's makeApiRequest
api::ApiResponse AuthApi::make_request(const std::string& method, const std::string& path,
                                        const json& body, const http::Headers& extra_headers) {
    api::ApiResponse result;
    auto& client = http::HttpClient::instance();

    http::Headers headers = extra_headers;
    // Auto-inject session token
    if (!session_token_.empty() && headers.find("X-Session-Token") == headers.end()) {
        headers["X-Session-Token"] = session_token_;
    }

    std::string url = endpoint(path);
    http::HttpResponse resp;

    if (method == "GET") {
        resp = client.get(url, headers);
    } else if (method == "POST") {
        resp = client.post_json(url, body, headers);
    } else if (method == "PUT") {
        resp = client.put_json(url, body, headers);
    } else if (method == "DELETE") {
        resp = client.del(url, body.dump(), headers);
    }

    result.status_code = resp.status_code;

    if (!resp.error.empty() && resp.status_code == 0) {
        // Network error
        result.success = false;
        result.error = "Network error: " + resp.error;
        return result;
    }

    // Parse response
    json response_data;
    try {
        response_data = json::parse(resp.body);
    } catch (...) {
        // Non-JSON response — generate user-friendly error
        result.success = false;
        if (resp.status_code == 401 || resp.status_code == 403) {
            result.error = "Invalid email or password. Please check your credentials and try again.";
        } else if (resp.status_code == 404) {
            result.error = "Account not found. Please check your email or sign up.";
        } else if (resp.status_code >= 500) {
            result.error = "Server error. Please try again later.";
        } else {
            result.error = "Unable to connect to server. Please try again later.";
        }
        return result;
    }

    result.success = resp.success;
    result.data = response_data;

    // Generate user-friendly error messages (matching TypeScript logic)
    if (!resp.success) {
        std::string backend_msg;
        if (response_data.contains("message")) backend_msg = response_data["message"].get<std::string>();
        if (backend_msg.empty() && response_data.contains("error")) backend_msg = response_data["error"].get<std::string>();

        if (resp.status_code == 401 || resp.status_code == 403) {
            std::string lower_msg = backend_msg;
            std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);
            if (lower_msg.find("password") != std::string::npos) {
                result.error = "Incorrect password. Please try again.";
            } else {
                result.error = "Invalid email or password. Please check your credentials and try again.";
            }
        } else if (resp.status_code == 404) {
            result.error = "Account not found. Please check your email or sign up for a new account.";
        } else if (resp.status_code == 400) {
            result.error = backend_msg.empty() ? "Invalid request. Please check your information." : backend_msg;
        } else if (resp.status_code >= 500) {
            result.error = "Server error. Please try again later.";
        } else {
            result.error = backend_msg.empty() ? ("HTTP " + std::to_string(resp.status_code)) : backend_msg;
        }
    }

    return result;
}

bool AuthApi::check_health() {
    auto result = make_request("GET", "/health");
    return result.success;
}

api::ApiResponse AuthApi::login(const api::LoginRequest& req) {
    return make_request("POST", "/user/login", api::to_json_obj(req));
}

api::ApiResponse AuthApi::register_user(const api::RegisterRequest& req) {
    return make_request("POST", "/user/register", api::to_json_obj(req));
}

api::ApiResponse AuthApi::verify_otp(const api::VerifyOtpRequest& req) {
    return make_request("POST", "/user/verify-otp", api::to_json_obj(req));
}

api::ApiResponse AuthApi::verify_mfa(const std::string& email, const std::string& otp) {
    return make_request("POST", "/user/verify-mfa", {{"email", email}, {"otp", otp}});
}

api::ApiResponse AuthApi::forgot_password(const api::ForgotPasswordRequest& req) {
    return make_request("POST", "/user/forgot-password", api::to_json_obj(req));
}

api::ApiResponse AuthApi::reset_password(const api::ResetPasswordRequest& req) {
    return make_request("POST", "/user/reset-password", api::to_json_obj(req));
}

api::ApiResponse AuthApi::create_guest_session(const api::DeviceRegisterRequest& req) {
    return make_request("POST", "/guest/session", api::to_json_obj(req));
}

api::ApiResponse AuthApi::get_user_profile(const std::string& api_key) {
    return make_request("GET", "/user/profile", {}, {{"X-API-Key", api_key}});
}

api::ApiResponse AuthApi::get_auth_status(const std::string& api_key, const std::string& device_id) {
    http::Headers headers;
    if (!api_key.empty()) headers["X-API-Key"] = api_key;
    if (!device_id.empty()) headers["X-Device-ID"] = device_id;
    return make_request("GET", "/auth/status", {}, headers);
}

api::ApiResponse AuthApi::validate_session_server(const std::string& api_key) {
    return make_request("POST", "/user/validate-session", {}, {{"X-API-Key", api_key}});
}

api::ApiResponse AuthApi::logout(const std::string& api_key) {
    return make_request("POST", "/user/logout", {}, {{"X-API-Key", api_key}});
}

api::ApiResponse AuthApi::get_subscription_plans() {
    return make_request("GET", "/cashfree/plans");
}

api::ApiResponse AuthApi::get_user_subscription(const std::string& api_key) {
    return make_request("GET", "/cashfree/subscription", {}, {{"X-API-Key", api_key}});
}

} // namespace fincept::auth
