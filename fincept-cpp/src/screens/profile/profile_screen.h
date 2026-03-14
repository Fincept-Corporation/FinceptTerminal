#pragma once
// Profile & Account Management — mirrors ProfileTab.tsx
// Sections: Overview, Usage, Security, Billing, Support
// Real API calls to api.fincept.in

#include <imgui.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <nlohmann/json.hpp>

namespace fincept::profile {

using json = nlohmann::json;

enum class Section { Overview, Usage, Security, Billing, Support };

// API data types matching profileTypes.ts
struct UsageSummary {
    int total_requests = 0;
    double total_credits_used = 0;
    double avg_credits_per_request = 0;
    double avg_response_time_ms = 0;
    std::string first_request_at;
    std::string last_request_at;
};

struct DailyUsage {
    std::string date;
    int request_count = 0;
    double credits_used = 0;
};

struct EndpointBreakdown {
    std::string endpoint;
    int request_count = 0;
    double credits_used = 0;
    double avg_response_time_ms = 0;
};

struct RecentRequest {
    std::string timestamp;
    std::string endpoint;
    std::string method;
    int status_code = 0;
    double credits_used = 0;
    double response_time_ms = 0;
};

struct UsageAccount {
    double credit_balance = 0;
    std::string account_type;
    std::string credits_expire_at;
    int rate_limit_per_hour = 0;
    std::string support_type;
};

struct UsageData {
    UsageAccount account;
    int period_days = 30;
    UsageSummary summary;
    std::vector<DailyUsage> daily_usage;
    std::vector<EndpointBreakdown> endpoint_breakdown;
    std::vector<RecentRequest> recent_requests;
    bool valid = false;
};

struct PaymentHistoryItem {
    std::string payment_uuid;
    std::string payment_gateway;
    double amount_usd = 0;
    std::string currency;
    std::string status;
    std::string plan_name;
    double credits_purchased = 0;
    std::string payment_method;
    std::string created_at;
    std::string completed_at;
};

struct SubscriptionData {
    int user_id = 0;
    std::string account_type;
    double credit_balance = 0;
    std::string credits_expire_at;
    std::string support_type;
    std::string last_credit_purchase_at;
    std::string created_at;
    bool valid = false;
};

class ProfileScreen {
public:
    void render();

private:
    bool initialized_ = false;
    Section active_section_ = Section::Overview;

    // Data (protected by mutex for async fetches)
    std::mutex data_mutex_;
    UsageData usage_data_;
    SubscriptionData subscription_data_;
    std::vector<PaymentHistoryItem> payment_history_;
    std::string error_;
    std::atomic<bool> loading_{false};

    // Security
    bool show_api_key_ = false;
    bool show_logout_confirm_ = false;
    bool show_regen_confirm_ = false;

    // Edit profile
    bool editing_profile_ = false;
    char edit_username_[128] = "";
    char edit_phone_[64] = "";
    char edit_country_[64] = "";
    char edit_country_code_[16] = "";
    std::string edit_status_;
    bool edit_loading_ = false;

    // MFA
    bool mfa_loading_ = false;
    std::string mfa_status_;

    // API key regen
    bool regen_loading_ = false;
    std::string regen_status_;

    // Fetch tracking
    double last_fetch_time_ = 0;
    Section last_fetched_section_ = Section::Overview;

    // Init
    void init();

    // Section renderers
    void render_section_tabs();
    void render_overview();
    void render_usage();
    void render_security();
    void render_billing();
    void render_support();

    // API calls (run in background thread)
    void fetch_usage_data();
    void fetch_billing_data();
    void fetch_profile_update();
    void toggle_mfa(bool enable);
    void regenerate_api_key();

    // Helpers
    void render_data_row(const char* label, const char* value, ImVec4 value_color = ImVec4(0, 0, 0, 0));
    void render_stat_box(const char* label, const char* value, ImVec4 color = ImVec4(0, 0, 0, 0));
    void render_panel_header(const char* title);
};

} // namespace fincept::profile
