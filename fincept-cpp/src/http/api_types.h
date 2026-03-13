#pragma once
// API request/response types mirroring the TypeScript interfaces exactly

#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <vector>

namespace fincept::api {

using json = nlohmann::json;

// =============================================================================
// Generic API Response
// =============================================================================
struct ApiResponse {
    bool success = false;
    json data;
    std::string message;
    std::string error;
    int status_code = 0;
};

// =============================================================================
// Auth Request Types
// =============================================================================
struct LoginRequest {
    std::string email;
    std::string password;
    bool force_login = false;
};

inline json to_json_obj(const LoginRequest& r) {
    json j;
    j["email"] = r.email;
    j["password"] = r.password;
    if (r.force_login) j["force_login"] = true;
    return j;
}

struct RegisterRequest {
    std::string username;
    std::string email;
    std::string password;
    std::string phone;
    std::string country;
    std::string country_code;
    std::string preferred_currency = "USD";
};

inline json to_json_obj(const RegisterRequest& r) {
    json j;
    j["username"] = r.username;
    j["email"] = r.email;
    j["password"] = r.password;
    if (!r.phone.empty()) j["phone"] = r.phone;
    if (!r.country.empty()) j["country"] = r.country;
    j["country_code"] = r.country_code;
    j["preferred_currency"] = r.preferred_currency;
    return j;
}

struct VerifyOtpRequest {
    std::string email;
    std::string otp;
};

inline json to_json_obj(const VerifyOtpRequest& r) {
    return {{"email", r.email}, {"otp", r.otp}};
}

struct ForgotPasswordRequest {
    std::string email;
};

inline json to_json_obj(const ForgotPasswordRequest& r) {
    return {{"email", r.email}};
}

struct ResetPasswordRequest {
    std::string email;
    std::string otp;
    std::string new_password;
};

inline json to_json_obj(const ResetPasswordRequest& r) {
    return {{"email", r.email}, {"otp", r.otp}, {"new_password", r.new_password}};
}

struct DeviceRegisterRequest {
    std::string device_id;
    std::string device_name;
    std::string platform = "desktop";
    json hardware_info;
};

inline json to_json_obj(const DeviceRegisterRequest& r) {
    return {
        {"device_id", r.device_id},
        {"device_name", r.device_name},
        {"platform", r.platform},
        {"hardware_info", r.hardware_info}
    };
}

// =============================================================================
// Auth Response Types
// =============================================================================
struct LoginResponse {
    std::string api_key;
    std::string session_token;
    std::string message;
    std::string username;
    std::string email;
    double credit_balance = 0.0;
    bool mfa_required = false;
    bool active_session = false;
};

inline void from_json_obj(const json& j, LoginResponse& r) {
    if (j.contains("api_key")) r.api_key = j["api_key"].get<std::string>();
    if (j.contains("session_token")) r.session_token = j["session_token"].get<std::string>();
    if (j.contains("message")) r.message = j["message"].get<std::string>();
    if (j.contains("username")) r.username = j["username"].get<std::string>();
    if (j.contains("email")) r.email = j["email"].get<std::string>();
    if (j.contains("credit_balance")) r.credit_balance = j["credit_balance"].get<double>();
    if (j.contains("mfa_required")) r.mfa_required = j["mfa_required"].get<bool>();
    if (j.contains("active_session")) r.active_session = j["active_session"].get<bool>();
}

struct UserProfile {
    int id = 0;
    std::string username;
    std::string email;
    std::string account_type;
    double credit_balance = 0.0;
    bool is_verified = false;
    bool is_admin = false;
    bool mfa_enabled = false;
    std::string phone;
    std::string country_code;
    std::string country;
    std::string created_at;
    std::string last_login_at;
};

inline void from_json_obj(const json& j, UserProfile& p) {
    if (j.contains("id")) p.id = j["id"].get<int>();
    if (j.contains("username")) p.username = j["username"].get<std::string>();
    if (j.contains("email")) p.email = j["email"].get<std::string>();
    if (j.contains("account_type")) p.account_type = j["account_type"].get<std::string>();
    if (j.contains("credit_balance")) p.credit_balance = j["credit_balance"].get<double>();
    if (j.contains("is_verified")) p.is_verified = j["is_verified"].get<bool>();
    if (j.contains("is_admin")) p.is_admin = j["is_admin"].get<bool>();
    if (j.contains("mfa_enabled")) p.mfa_enabled = j["mfa_enabled"].get<bool>();
    if (j.contains("phone") && !j["phone"].is_null()) p.phone = j["phone"].get<std::string>();
    if (j.contains("country_code") && !j["country_code"].is_null()) p.country_code = j["country_code"].get<std::string>();
    if (j.contains("country") && !j["country"].is_null()) p.country = j["country"].get<std::string>();
    if (j.contains("created_at")) p.created_at = j["created_at"].get<std::string>();
    if (j.contains("last_login_at")) p.last_login_at = j["last_login_at"].get<std::string>();
}

struct UserSubscription {
    int user_id = 0;
    std::string account_type;
    double credit_balance = 0.0;
    std::string credits_expire_at;
    std::string support_type;
    std::string last_credit_purchase_at;
    std::string created_at;
};

inline void from_json_obj(const json& j, UserSubscription& s) {
    if (j.contains("user_id")) s.user_id = j["user_id"].get<int>();
    if (j.contains("account_type")) s.account_type = j["account_type"].get<std::string>();
    if (j.contains("credit_balance")) s.credit_balance = j["credit_balance"].get<double>();
    if (j.contains("credits_expire_at") && !j["credits_expire_at"].is_null()) s.credits_expire_at = j["credits_expire_at"].get<std::string>();
    if (j.contains("support_type")) s.support_type = j["support_type"].get<std::string>();
    if (j.contains("last_credit_purchase_at") && !j["last_credit_purchase_at"].is_null()) s.last_credit_purchase_at = j["last_credit_purchase_at"].get<std::string>();
    if (j.contains("created_at")) s.created_at = j["created_at"].get<std::string>();
}

struct SubscriptionPlan {
    std::string plan_id;
    std::string name;
    std::string description;
    double price_usd = 0.0;
    std::string currency;
    int credits = 0;
    std::string support_type;
    int validity_days = 0;
    std::vector<std::string> features;
    bool is_free = false;
    int display_order = 0;
};

inline void from_json_obj(const json& j, SubscriptionPlan& p) {
    if (j.contains("plan_id")) p.plan_id = j["plan_id"].get<std::string>();
    if (j.contains("name")) p.name = j["name"].get<std::string>();
    if (j.contains("description")) p.description = j["description"].get<std::string>();
    if (j.contains("price_usd")) p.price_usd = j["price_usd"].get<double>();
    if (j.contains("currency")) p.currency = j["currency"].get<std::string>();
    if (j.contains("credits")) p.credits = j["credits"].get<int>();
    if (j.contains("support_type")) p.support_type = j["support_type"].get<std::string>();
    if (j.contains("validity_days")) p.validity_days = j["validity_days"].get<int>();
    if (j.contains("features") && j["features"].is_array()) {
        for (auto& f : j["features"]) p.features.push_back(f.get<std::string>());
    }
    if (j.contains("is_free")) p.is_free = j["is_free"].get<bool>();
    if (j.contains("display_order")) p.display_order = j["display_order"].get<int>();
}

// =============================================================================
// Session Data (mirrors TypeScript SessionData exactly)
// =============================================================================
struct SessionData {
    bool authenticated = false;
    std::string user_type; // "guest" | "registered"
    std::string api_key;
    std::string session_token;
    std::string device_id;
    UserProfile user_info;
    std::string expires_at;
    double credit_balance = 0.0;
    int requests_today = 0;
    UserSubscription subscription;
    bool has_subscription = false;

    bool is_registered() const { return user_type == "registered"; }
    bool is_guest() const { return user_type == "guest"; }

    bool has_paid_plan() const {
        std::string at = user_info.account_type;
        // lowercase
        for (auto& c : at) c = (char)std::tolower(c);
        return at == "basic" || at == "standard" || at == "pro" || at == "enterprise";
    }

    json to_json() const {
        json j;
        j["authenticated"] = authenticated;
        j["user_type"] = user_type;
        j["api_key"] = api_key;
        j["session_token"] = session_token;
        j["device_id"] = device_id;
        j["expires_at"] = expires_at;
        j["credit_balance"] = credit_balance;
        j["requests_today"] = requests_today;
        j["has_subscription"] = has_subscription;

        json ui;
        ui["username"] = user_info.username;
        ui["email"] = user_info.email;
        ui["account_type"] = user_info.account_type;
        ui["credit_balance"] = user_info.credit_balance;
        ui["is_verified"] = user_info.is_verified;
        ui["mfa_enabled"] = user_info.mfa_enabled;
        ui["phone"] = user_info.phone;
        ui["country"] = user_info.country;
        ui["country_code"] = user_info.country_code;
        j["user_info"] = ui;

        json sub;
        sub["user_id"] = subscription.user_id;
        sub["account_type"] = subscription.account_type;
        sub["credit_balance"] = subscription.credit_balance;
        sub["credits_expire_at"] = subscription.credits_expire_at;
        sub["support_type"] = subscription.support_type;
        sub["last_credit_purchase_at"] = subscription.last_credit_purchase_at;
        sub["created_at"] = subscription.created_at;
        j["subscription"] = sub;

        return j;
    }

    static SessionData from_json(const json& j) {
        SessionData s;
        if (j.contains("authenticated")) s.authenticated = j["authenticated"].get<bool>();
        if (j.contains("user_type")) s.user_type = j["user_type"].get<std::string>();
        if (j.contains("api_key")) s.api_key = j["api_key"].get<std::string>();
        if (j.contains("session_token")) s.session_token = j["session_token"].get<std::string>();
        if (j.contains("device_id")) s.device_id = j["device_id"].get<std::string>();
        if (j.contains("expires_at")) s.expires_at = j["expires_at"].get<std::string>();
        if (j.contains("credit_balance")) s.credit_balance = j["credit_balance"].get<double>();
        if (j.contains("requests_today")) s.requests_today = j["requests_today"].get<int>();
        if (j.contains("has_subscription")) s.has_subscription = j["has_subscription"].get<bool>();
        if (j.contains("user_info")) {
            from_json_obj(j["user_info"], s.user_info);
        }
        if (j.contains("subscription") && j["subscription"].is_object()) {
            from_json_obj(j["subscription"], s.subscription);
            s.has_subscription = !s.subscription.account_type.empty();
        }
        return s;
    }
};

} // namespace fincept::api
