#include "auth_manager.h"
#include "auth_api.h"
#include "storage/database.h"
#include "core/device_id.h"
#include "core/validators.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "core/notification.h"
#include <thread>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fincept::auth {

using json = nlohmann::json;

static constexpr const char* SESSION_KEY = "fincept_session";

AuthManager& AuthManager::instance() {
    static AuthManager mgr;
    return mgr;
}

void AuthManager::initialize() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loading_ = true;

        // Load saved session from DB (fast, local-only)
        if (load_session()) {
            if (session_.is_registered()) {
                // Session loaded — mark authenticated optimistically so the UI
                // can proceed immediately.  Server validation runs in background.
                LOG_INFO("Auth", "Loaded saved session, validating in background...");
            }
            // Guest sessions are used as-is (checked for expiry in load_session)
        }

        // If no registered session needs validation, we're done loading
        if (!session_.is_registered() || !session_.authenticated) {
            loading_ = false;
            LOG_INFO("Auth", "Initialized — authenticated: %d, type: %s",
                     session_.authenticated, session_.user_type.c_str());

            if (session_.authenticated) {
                core::EventBus::instance().publish("auth.session_changed", {
                    {"authenticated", true},
                    {"user_type", session_.user_type},
                    {"has_api_key", !session_.api_key.empty()}
                });
            }
            return;
        }
    }

    // Validate registered session with server in background thread
    // to avoid blocking the render loop during startup
    auto saved_session = session_;
    std::thread([this, saved_session]() {
        auto validated = validate_session(saved_session);

        std::lock_guard<std::mutex> lock(mutex_);
        if (validated.authenticated) {
            session_ = validated;
            save_session();
        } else {
            clear_session();
        }
        loading_ = false;

        LOG_INFO("Auth", "Background validation complete — authenticated: %d, type: %s",
                 session_.authenticated, session_.user_type.c_str());

        core::EventBus::instance().publish("auth.session_changed", {
            {"authenticated", session_.authenticated},
            {"user_type", session_.user_type},
            {"has_api_key", !session_.api_key.empty()}
        });
    }).detach();
}

// =============================================================================
// State Queries
// =============================================================================

bool AuthManager::is_authenticated() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_.authenticated;
}

bool AuthManager::is_loading() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loading_;
}

bool AuthManager::is_logging_out() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return logging_out_;
}

const api::SessionData& AuthManager::session() const {
    return session_;
}

bool AuthManager::has_session() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_.authenticated && !session_.api_key.empty();
}

// =============================================================================
// Session Persistence
// =============================================================================

void AuthManager::save_session() {
    db::ops::save_setting(SESSION_KEY, session_.to_json().dump(), "auth");
}

bool AuthManager::load_session() {
    auto stored = db::ops::get_setting(SESSION_KEY);
    if (!stored.has_value()) return false;

    try {
        auto j = json::parse(stored.value());
        session_ = api::SessionData::from_json(j);

        // Check guest session expiry (ISO8601 format: "2026-03-14T12:00:00Z")
        if (session_.is_guest() && !session_.expires_at.empty()) {
            // Parse ISO8601 date — extract year/month/day/hour/min/sec
            struct tm expires_tm = {};
            if (std::sscanf(session_.expires_at.c_str(), "%d-%d-%dT%d:%d:%d",
                &expires_tm.tm_year, &expires_tm.tm_mon, &expires_tm.tm_mday,
                &expires_tm.tm_hour, &expires_tm.tm_min, &expires_tm.tm_sec) >= 3) {
                expires_tm.tm_year -= 1900;
                expires_tm.tm_mon -= 1;
                time_t expires_time = mktime(&expires_tm);
                time_t now = time(nullptr);
                if (now > expires_time) {
                    LOG_WARN("Auth", "Guest session expired, clearing");
                    core::notify::send("Session Expired",
                        "Your session has expired. Please log in again.",
                        core::NotifyLevel::Warning);
                    clear_session();
                    return false;
                }
            }
        }

        return session_.authenticated;
    } catch (const std::exception& e) {
        LOG_ERROR("Auth", "Failed to load session: %s", e.what());
        clear_session();
        return false;
    }
}

void AuthManager::clear_session() {
    session_ = api::SessionData{};
    db::ops::save_setting(SESSION_KEY, "", "auth");
    db::ops::save_setting("fincept_api_key", "", "auth");
    db::ops::save_setting("fincept_device_id", "", "auth");
}

// =============================================================================
// Internal Helpers
// =============================================================================

api::UserProfile AuthManager::fetch_profile(const std::string& api_key) {
    api::UserProfile profile;
    auto result = AuthApi::instance().get_user_profile(api_key);
    if (result.success && !result.data.is_null()) {
        // Unwrap double-wrapped response
        json profile_data = result.data.contains("data") ? result.data["data"] : result.data;
        api::from_json_obj(profile_data, profile);
    }
    return profile;
}

api::UserSubscription AuthManager::fetch_subscription(const std::string& api_key) {
    api::UserSubscription sub;
    auto result = AuthApi::instance().get_user_subscription(api_key);
    if (result.success && !result.data.is_null()) {
        json sub_data = result.data.contains("data") ? result.data["data"] : result.data;
        api::from_json_obj(sub_data, sub);
    }
    return sub;
}

api::SessionData AuthManager::validate_session(const api::SessionData& saved) {
    auto& api = AuthApi::instance();

    // Restore session token
    if (!saved.session_token.empty()) {
        api.set_session_token(saved.session_token);
    }

    // Validate session token — non-fatal if it fails (session may have expired
    // due to login from another device, but API key may still be valid)
    bool session_token_valid = false;
    if (!saved.session_token.empty()) {
        auto val_result = api.validate_session_server(saved.api_key);
        if (val_result.success) {
            json val_data = val_result.data.contains("data") ? val_result.data["data"] : val_result.data;
            session_token_valid = !val_data.contains("valid") || val_data["valid"].get<bool>();
        }
        if (!session_token_valid) {
            api.clear_session_token();
        }
    }

    // Check auth status — this works with just API key
    auto result = api.get_auth_status(saved.api_key, saved.device_id);
    if (!result.success || result.data.is_null()) {
        return api::SessionData{};
    }

    json auth_data = result.data.contains("data") ? result.data["data"] : result.data;

    if (auth_data.contains("authenticated") && auth_data["authenticated"].get<bool>()) {
        api::SessionData validated;
        validated.authenticated = true;
        validated.user_type = auth_data.value("user_type", "");
        validated.api_key = saved.api_key;
        validated.session_token = session_token_valid ? saved.session_token : "";
        validated.device_id = saved.device_id;

        // Populate user info from auth status response (always available)
        if (auth_data.contains("user") && auth_data["user"].is_object()) {
            auto& u = auth_data["user"];
            validated.user_info.id = u.value("id", 0);
            validated.user_info.username = u.value("username", "");
            validated.user_info.email = u.value("email", "");
            validated.user_info.account_type = u.value("account_type", "");
            validated.user_info.credit_balance = u.value("credit_balance", 0.0);
            validated.user_info.created_at = u.value("created_at", "");
        }

        // Try to fetch full profile and subscription (may fail if session expired)
        if (validated.is_registered()) {
            auto profile = fetch_profile(saved.api_key);
            if (!profile.username.empty()) {
                validated.user_info = profile;
            }
            auto sub = fetch_subscription(saved.api_key);
            if (!sub.account_type.empty()) {
                validated.subscription = sub;
                validated.has_subscription = true;
            } else {
                // Use account_type from auth status as fallback
                validated.subscription.account_type = validated.user_info.account_type;
                validated.subscription.credit_balance = validated.user_info.credit_balance;
                validated.has_subscription = !validated.user_info.account_type.empty();
            }
        }

        return validated;
    }

    return api::SessionData{};
}

// =============================================================================
// Auth Operations
// =============================================================================

AuthResult AuthManager::login(const std::string& email, const std::string& password, bool force_login) {
    std::lock_guard<std::mutex> lock(mutex_);
    loading_ = true;

    auto& api = AuthApi::instance();
    api::LoginRequest req{email, password, force_login};
    auto result = api.login(req);

    // Unwrap double-wrapped response
    json api_data = result.data.contains("data") ? result.data["data"] : result.data;

    // Check active session on another device
    if (api_data.contains("active_session") && api_data["active_session"].get<bool>()) {
        loading_ = false;
        return {false, api_data.value("message", "Already logged in on another device."), false, true};
    }

    // Check MFA required
    if (result.success && api_data.contains("mfa_required") && api_data["mfa_required"].get<bool>()) {
        loading_ = false;
        return {false, "MFA verification required", true, false};
    }

    if (result.success && api_data.contains("api_key")) {
        std::string api_key = api_data["api_key"].get<std::string>();
        std::string session_token = api_data.value("session_token", "");

        api.set_session_token(session_token);

        session_.authenticated = true;
        session_.user_type = "registered";
        session_.api_key = api_key;
        session_.session_token = session_token;
        session_.device_id = utils::generate_device_id();

        // Pre-populate from login response data
        if (api_data.contains("username")) session_.user_info.username = api_data["username"].get<std::string>();
        if (api_data.contains("email")) session_.user_info.email = api_data["email"].get<std::string>();
        if (api_data.contains("credit_balance")) session_.user_info.credit_balance = api_data["credit_balance"].get<double>();

        // Try to fetch full profile and subscription
        auto profile = fetch_profile(api_key);
        if (!profile.username.empty()) {
            session_.user_info = profile;
        }

        auto subscription = fetch_subscription(api_key);
        if (!subscription.account_type.empty()) {
            session_.subscription = subscription;
            session_.has_subscription = true;
        }

        // If profile/subscription fetch failed, get data from auth status
        if (session_.user_info.account_type.empty()) {
            auto auth_status = api.get_auth_status(api_key, session_.device_id);
            if (auth_status.success && !auth_status.data.is_null()) {
                json as_data = auth_status.data.contains("data") ? auth_status.data["data"] : auth_status.data;
                if (as_data.contains("user") && as_data["user"].is_object()) {
                    auto& u = as_data["user"];
                    if (session_.user_info.account_type.empty())
                        session_.user_info.account_type = u.value("account_type", "");
                    if (session_.user_info.credit_balance == 0.0)
                        session_.user_info.credit_balance = u.value("credit_balance", 0.0);
                    if (session_.user_info.id == 0)
                        session_.user_info.id = u.value("id", 0);
                }
            }
        }

        // Use account_type as subscription fallback
        if (session_.subscription.account_type.empty() && !session_.user_info.account_type.empty()) {
            session_.subscription.account_type = session_.user_info.account_type;
            session_.subscription.credit_balance = session_.user_info.credit_balance;
            session_.has_subscription = true;
        }

        save_session();

        // Legacy keys
        db::ops::save_setting("fincept_api_key", api_key, "auth");
        db::ops::save_setting("fincept_device_id", session_.device_id, "auth");

        loading_ = false;
        LOG_INFO("Auth", "Login success — account_type: %s, api_key present: %d",
                 session_.user_info.account_type.c_str(), !session_.api_key.empty());

        // Notify all screens that auth state changed
        core::EventBus::instance().publish("auth.session_changed", {
            {"authenticated", true},
            {"user_type", session_.user_type},
            {"has_api_key", !session_.api_key.empty()}
        });

        return {true, ""};
    }

    loading_ = false;
    return {false, result.error.empty() ? "Login failed" : result.error};
}

AuthResult AuthManager::signup(const std::string& username, const std::string& email, const std::string& password,
                                const std::string& phone, const std::string& country,
                                const std::string& country_code) {
    std::lock_guard<std::mutex> lock(mutex_);
    loading_ = true;

    api::RegisterRequest req;
    req.username = username;
    req.email = email;
    req.password = password;
    req.phone = phone;
    req.country = country;
    req.country_code = country_code;

    auto result = AuthApi::instance().register_user(req);

    loading_ = false;

    if (result.success) {
        return {true, ""};
    }
    return {false, result.error.empty() ? (result.message.empty() ? "Registration failed" : result.message) : result.error};
}

AuthResult AuthManager::verify_otp(const std::string& email, const std::string& otp) {
    std::lock_guard<std::mutex> lock(mutex_);
    loading_ = true;

    auto& api = AuthApi::instance();
    auto result = api.verify_otp({email, otp});
    json api_data = result.data.contains("data") ? result.data["data"] : result.data;

    if (result.success && api_data.contains("api_key")) {
        std::string api_key = api_data["api_key"].get<std::string>();
        std::string session_token = api_data.value("session_token", "");

        api.set_session_token(session_token);
        auto profile = fetch_profile(api_key);

        session_.authenticated = true;
        session_.user_type = "registered";
        session_.api_key = api_key;
        session_.session_token = session_token;
        session_.device_id = utils::generate_device_id();
        session_.user_info = profile;

        save_session();
        loading_ = false;
        return {true, ""};
    }

    loading_ = false;
    return {false, result.error.empty() ? "Verification failed" : result.error};
}

AuthResult AuthManager::verify_mfa(const std::string& email, const std::string& otp) {
    std::lock_guard<std::mutex> lock(mutex_);
    loading_ = true;

    auto& api = AuthApi::instance();
    auto result = api.verify_mfa(email, otp);
    json api_data = result.data.contains("data") ? result.data["data"] : result.data;

    if (result.success && api_data.contains("api_key")) {
        std::string api_key = api_data["api_key"].get<std::string>();
        std::string session_token = api_data.value("session_token", "");

        api.set_session_token(session_token);
        auto profile = fetch_profile(api_key);
        auto subscription = fetch_subscription(api_key);

        session_.authenticated = true;
        session_.user_type = "registered";
        session_.api_key = api_key;
        session_.session_token = session_token;
        session_.device_id = utils::generate_device_id();
        session_.user_info = profile;
        session_.subscription = subscription;
        session_.has_subscription = !subscription.account_type.empty();

        save_session();
        loading_ = false;
        return {true, ""};
    }

    loading_ = false;
    return {false, result.error.empty() ? "MFA verification failed" : result.error};
}

AuthResult AuthManager::setup_guest_access() {
    std::lock_guard<std::mutex> lock(mutex_);
    loading_ = true;

    std::string device_id = utils::generate_device_id();

    api::DeviceRegisterRequest req;
    req.device_id = device_id;
    req.device_name = "Fincept Terminal - Desktop";
    req.platform = "desktop";
    req.hardware_info = {{"cpu_cores", 4}, {"memory_gb", 8}};

#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    req.hardware_info["cpu_cores"] = (int)si.dwNumberOfProcessors;

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    req.hardware_info["screen_resolution"] = std::to_string(w) + "x" + std::to_string(h);
#elif defined(__APPLE__) || defined(__linux__)
    req.hardware_info["cpu_cores"] = (int)std::thread::hardware_concurrency();
#endif

    auto result = AuthApi::instance().create_guest_session(req);

    if (result.success && !result.data.is_null()) {
        json api_data = result.data.contains("data") ? result.data["data"] : result.data;

        session_.authenticated = true;
        session_.user_type = "guest";
        session_.api_key = api_data.value("api_key", "");
        session_.device_id = device_id;
        session_.expires_at = api_data.value("expires_at", "");
        session_.credit_balance = api_data.value("credit_balance", 0.0);

        save_session();
        loading_ = false;

        core::EventBus::instance().publish("auth.session_changed", {
            {"authenticated", true},
            {"user_type", "guest"},
            {"has_api_key", !session_.api_key.empty()}
        });

        return {true, ""};
    }

    loading_ = false;
    return {false, result.error.empty() ? "Failed to setup guest access" : result.error};
}

AuthResult AuthManager::forgot_password(const std::string& email) {
    auto result = AuthApi::instance().forgot_password({email});
    return {result.success, result.error.empty() ? result.message : result.error};
}

AuthResult AuthManager::reset_password(const std::string& email, const std::string& otp, const std::string& new_password) {
    auto result = AuthApi::instance().reset_password({email, otp, new_password});
    return {result.success, result.error.empty() ? result.message : result.error};
}

void AuthManager::logout() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (logging_out_) return;
        logging_out_ = true;
    }

    // Server-side logout (best effort)
    if (!session_.api_key.empty()) {
        try {
            AuthApi::instance().logout(session_.api_key);
        } catch (...) {}
    }

    AuthApi::instance().clear_session_token();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        clear_session();
        logging_out_ = false;
    }

    LOG_INFO("Auth", "Logout completed");

    core::EventBus::instance().publish("auth.session_changed", {
        {"authenticated", false},
        {"user_type", ""},
        {"has_api_key", false}
    });
}

void AuthManager::refresh_user_data() {
    if (!session_.authenticated || !session_.is_registered()) return;

    auto profile = fetch_profile(session_.api_key);
    auto subscription = fetch_subscription(session_.api_key);

    std::lock_guard<std::mutex> lock(mutex_);
    if (!profile.username.empty()) {
        session_.user_info = profile;
    }
    session_.subscription = subscription;
    session_.has_subscription = !subscription.account_type.empty();
    save_session();
}

// =============================================================================
// Async Wrappers
// =============================================================================

void AuthManager::run_async(std::function<AuthResult()> fn) {
    result_ready_ = false;
    pending_future_ = std::async(std::launch::async, std::move(fn));
}

bool AuthManager::has_pending_result() const {
    if (result_ready_) return true;
    if (pending_future_.valid()) {
        return pending_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
    }
    return false;
}

AuthResult AuthManager::consume_result() {
    if (pending_future_.valid() && pending_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        last_result_ = pending_future_.get();
        result_ready_ = true;
    }
    result_ready_ = false;
    return last_result_;
}

void AuthManager::login_async(const std::string& email, const std::string& password, bool force_login) {
    run_async([this, email, password, force_login]() { return login(email, password, force_login); });
}

void AuthManager::signup_async(const std::string& username, const std::string& email, const std::string& password,
                                const std::string& phone, const std::string& country,
                                const std::string& country_code) {
    run_async([this, username, email, password, phone, country, country_code]() {
        return signup(username, email, password, phone, country, country_code);
    });
}

void AuthManager::verify_otp_async(const std::string& email, const std::string& otp) {
    run_async([this, email, otp]() { return verify_otp(email, otp); });
}

void AuthManager::verify_mfa_async(const std::string& email, const std::string& otp) {
    run_async([this, email, otp]() { return verify_mfa(email, otp); });
}

void AuthManager::setup_guest_async() {
    run_async([this]() { return setup_guest_access(); });
}

void AuthManager::forgot_password_async(const std::string& email) {
    run_async([this, email]() { return forgot_password(email); });
}

void AuthManager::reset_password_async(const std::string& email, const std::string& otp, const std::string& new_password) {
    run_async([this, email, otp, new_password]() { return reset_password(email, otp, new_password); });
}

void AuthManager::logout_async() {
    run_async([this]() { logout(); return AuthResult{true, ""}; });
}

} // namespace fincept::auth
