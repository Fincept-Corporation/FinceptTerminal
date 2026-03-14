#include "profile_screen.h"
#include "auth/auth_manager.h"
#include "http/http_client.h"
#include <thread>

namespace fincept::profile {

static const char* API_BASE = "https://api.fincept.in";

static http::Headers auth_headers(const std::string& api_key, const std::string& session_token = "") {
    http::Headers h;
    h["Content-Type"] = "application/json";
    h["X-API-Key"] = api_key;
    if (!session_token.empty()) h["X-Session-Token"] = session_token;
    return h;
}

// ============================================================================
// Fetch Usage Data
// ============================================================================
void ProfileScreen::fetch_usage_data() {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    std::thread([this]() {
        auto& session = auth::AuthManager::instance().session();
        auto& http = http::HttpClient::instance();
        auto hdrs = auth_headers(session.api_key, session.session_token);

        auto resp = http.get(std::string(API_BASE) + "/user/usage?days=30", hdrs);

        std::lock_guard<std::mutex> lock(data_mutex_);
        if (resp.success) {
            try {
                auto j = resp.json_body();
                auto data = j.contains("data") ? j["data"] : j;

                // Account
                if (data.contains("account")) {
                    auto& a = data["account"];
                    if (a.contains("credit_balance")) usage_data_.account.credit_balance = a["credit_balance"].get<double>();
                    if (a.contains("account_type")) usage_data_.account.account_type = a["account_type"].get<std::string>();
                    if (a.contains("rate_limit_per_hour")) usage_data_.account.rate_limit_per_hour = a["rate_limit_per_hour"].get<int>();
                }
                if (data.contains("period_days")) usage_data_.period_days = data["period_days"].get<int>();

                // Summary
                if (data.contains("summary")) {
                    auto& s = data["summary"];
                    if (s.contains("total_requests")) usage_data_.summary.total_requests = s["total_requests"].get<int>();
                    if (s.contains("total_credits_used")) usage_data_.summary.total_credits_used = s["total_credits_used"].get<double>();
                    if (s.contains("avg_credits_per_request")) usage_data_.summary.avg_credits_per_request = s["avg_credits_per_request"].get<double>();
                    if (s.contains("avg_response_time_ms")) usage_data_.summary.avg_response_time_ms = s["avg_response_time_ms"].get<double>();
                }

                // Daily usage
                usage_data_.daily_usage.clear();
                if (data.contains("daily_usage") && data["daily_usage"].is_array()) {
                    for (auto& d : data["daily_usage"]) {
                        DailyUsage du;
                        if (d.contains("date")) du.date = d["date"].get<std::string>();
                        if (d.contains("request_count")) du.request_count = d["request_count"].get<int>();
                        if (d.contains("credits_used")) du.credits_used = d["credits_used"].get<double>();
                        usage_data_.daily_usage.push_back(du);
                    }
                }

                // Endpoint breakdown
                usage_data_.endpoint_breakdown.clear();
                if (data.contains("endpoint_breakdown") && data["endpoint_breakdown"].is_array()) {
                    for (auto& e : data["endpoint_breakdown"]) {
                        EndpointBreakdown eb;
                        if (e.contains("endpoint")) eb.endpoint = e["endpoint"].get<std::string>();
                        if (e.contains("request_count")) eb.request_count = e["request_count"].get<int>();
                        if (e.contains("credits_used")) eb.credits_used = e["credits_used"].get<double>();
                        if (e.contains("avg_response_time_ms")) eb.avg_response_time_ms = e["avg_response_time_ms"].get<double>();
                        usage_data_.endpoint_breakdown.push_back(eb);
                    }
                }

                // Recent requests
                usage_data_.recent_requests.clear();
                if (data.contains("recent_requests") && data["recent_requests"].is_array()) {
                    for (auto& r : data["recent_requests"]) {
                        RecentRequest rq;
                        if (r.contains("timestamp")) rq.timestamp = r["timestamp"].get<std::string>();
                        if (r.contains("endpoint")) rq.endpoint = r["endpoint"].get<std::string>();
                        if (r.contains("method")) rq.method = r["method"].get<std::string>();
                        if (r.contains("status_code")) rq.status_code = r["status_code"].get<int>();
                        if (r.contains("credits_used")) rq.credits_used = r["credits_used"].get<double>();
                        if (r.contains("response_time_ms")) rq.response_time_ms = r["response_time_ms"].get<double>();
                        usage_data_.recent_requests.push_back(rq);
                    }
                }

                usage_data_.valid = true;
            } catch (std::exception& e) {
                error_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_ = resp.error.empty() ? "Failed to fetch usage data" : resp.error;
        }
        loading_ = false;
    }).detach();
}

// ============================================================================
// Fetch Billing Data
// ============================================================================
void ProfileScreen::fetch_billing_data() {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    std::thread([this]() {
        auto& session = auth::AuthManager::instance().session();
        auto& http = http::HttpClient::instance();
        auto hdrs = auth_headers(session.api_key, session.session_token);

        auto sub_resp = http.get(std::string(API_BASE) + "/cashfree/subscription", hdrs);
        auto pay_resp = http.get(std::string(API_BASE) + "/cashfree/payments?page=1&page_size=20", hdrs);

        std::lock_guard<std::mutex> lock(data_mutex_);

        if (sub_resp.success) {
            try {
                auto j = sub_resp.json_body();
                auto d = j.contains("data") ? j["data"] : j;
                if (d.contains("user_id")) subscription_data_.user_id = d["user_id"].get<int>();
                if (d.contains("account_type")) subscription_data_.account_type = d["account_type"].get<std::string>();
                if (d.contains("credit_balance")) subscription_data_.credit_balance = d["credit_balance"].get<double>();
                if (d.contains("credits_expire_at") && !d["credits_expire_at"].is_null())
                    subscription_data_.credits_expire_at = d["credits_expire_at"].get<std::string>();
                if (d.contains("support_type")) subscription_data_.support_type = d["support_type"].get<std::string>();
                if (d.contains("last_credit_purchase_at") && !d["last_credit_purchase_at"].is_null())
                    subscription_data_.last_credit_purchase_at = d["last_credit_purchase_at"].get<std::string>();
                if (d.contains("created_at")) subscription_data_.created_at = d["created_at"].get<std::string>();
                subscription_data_.valid = true;
            } catch (...) {}
        }

        if (pay_resp.success) {
            try {
                auto j = pay_resp.json_body();
                auto data = j.contains("data") ? j["data"] : j;
                auto payments = data.contains("payments") ? data["payments"] : json::array();

                payment_history_.clear();
                for (auto& p : payments) {
                    PaymentHistoryItem item;
                    if (p.contains("payment_uuid")) item.payment_uuid = p["payment_uuid"].get<std::string>();
                    if (p.contains("payment_gateway")) item.payment_gateway = p["payment_gateway"].get<std::string>();
                    if (p.contains("amount_usd")) item.amount_usd = p["amount_usd"].get<double>();
                    if (p.contains("status")) item.status = p["status"].get<std::string>();
                    if (p.contains("plan_name")) item.plan_name = p["plan_name"].get<std::string>();
                    if (p.contains("created_at")) item.created_at = p["created_at"].get<std::string>();
                    payment_history_.push_back(item);
                }
            } catch (...) {}
        }

        loading_ = false;
    }).detach();
}

// ============================================================================
// Profile Update
// ============================================================================
void ProfileScreen::fetch_profile_update() {
    edit_loading_ = true;
    edit_status_.clear();

    std::thread([this]() {
        auto& session = auth::AuthManager::instance().session();
        auto& http = http::HttpClient::instance();
        auto hdrs = auth_headers(session.api_key, session.session_token);

        json body;
        if (edit_username_[0]) body["username"] = edit_username_;
        if (edit_phone_[0]) body["phone"] = edit_phone_;
        if (edit_country_[0]) body["country"] = edit_country_;
        if (edit_country_code_[0]) body["country_code"] = edit_country_code_;

        auto resp = http.put_json(std::string(API_BASE) + "/user/profile", body, hdrs);

        if (resp.success) {
            edit_status_ = "Profile updated successfully!";
            auth::AuthManager::instance().refresh_user_data();
        } else {
            edit_status_ = "Error: " + (resp.error.empty() ? "Failed to update profile" : resp.error);
        }
        edit_loading_ = false;
    }).detach();
}

// ============================================================================
// Toggle MFA
// ============================================================================
void ProfileScreen::toggle_mfa(bool enable) {
    mfa_loading_ = true;
    mfa_status_.clear();

    std::thread([this, enable]() {
        auto& session = auth::AuthManager::instance().session();
        auto& http = http::HttpClient::instance();
        auto hdrs = auth_headers(session.api_key, session.session_token);

        std::string url = std::string(API_BASE) + (enable ? "/user/mfa/enable" : "/user/mfa/disable");
        auto resp = http.post_json(url, json::object(), hdrs);

        if (resp.success) {
            mfa_status_ = enable ? "MFA enabled! OTP codes will be sent via email." : "MFA disabled successfully!";
            auth::AuthManager::instance().refresh_user_data();
        } else {
            mfa_status_ = "Error: " + (resp.error.empty() ? "Failed" : resp.error);
        }
        mfa_loading_ = false;
    }).detach();
}

// ============================================================================
// Regenerate API Key
// ============================================================================
void ProfileScreen::regenerate_api_key() {
    regen_loading_ = true;
    regen_status_.clear();

    std::thread([this]() {
        auto& session = auth::AuthManager::instance().session();
        auto& http = http::HttpClient::instance();
        auto hdrs = auth_headers(session.api_key, session.session_token);

        auto resp = http.post_json(std::string(API_BASE) + "/user/regenerate-api-key", json::object(), hdrs);

        if (resp.success) {
            regen_status_ = "API key regenerated. Please re-login.";
            auth::AuthManager::instance().refresh_user_data();
        } else {
            regen_status_ = "Error: " + (resp.error.empty() ? "Failed" : resp.error);
        }
        regen_loading_ = false;
    }).detach();
}

} // namespace fincept::profile
