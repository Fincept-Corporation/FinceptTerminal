#include "profile_screen.h"
#include "auth/auth_manager.h"
#include "http/http_client.h"
#include "theme/bloomberg_theme.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fincept::profile {

// Open URL in default browser
static void open_url(const char* url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    std::string cmd = std::string("open ") + url;
    system(cmd.c_str());
#else
    std::string cmd = std::string("xdg-open ") + url;
    system(cmd.c_str());
#endif
}

static const char* API_BASE = "https://api.fincept.in";

// Helper: build auth headers
static http::Headers auth_headers(const std::string& api_key, const std::string& session_token = "") {
    http::Headers h;
    h["Content-Type"] = "application/json";
    h["X-API-Key"] = api_key;
    if (!session_token.empty()) h["X-Session-Token"] = session_token;
    return h;
}

// ============================================================================
// Init
// ============================================================================
void ProfileScreen::init() {
    if (initialized_) return;
    auto& session = auth::AuthManager::instance().session();
    if (session.is_registered()) {
        std::strncpy(edit_username_, session.user_info.username.c_str(), sizeof(edit_username_) - 1);
        std::strncpy(edit_phone_, session.user_info.phone.c_str(), sizeof(edit_phone_) - 1);
        std::strncpy(edit_country_, session.user_info.country.c_str(), sizeof(edit_country_) - 1);
        std::strncpy(edit_country_code_, session.user_info.country_code.c_str(), sizeof(edit_country_code_) - 1);
    }
    initialized_ = true;
}

// ============================================================================
// Helpers
// ============================================================================
void ProfileScreen::render_data_row(const char* label, const char* value, ImVec4 value_color) {
    using namespace theme::colors;
    if (value_color.w == 0) value_color = TEXT_PRIMARY;

    float avail = ImGui::GetContentRegionAvail().x;
    float label_w = avail * 0.35f; // 35% for label

    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::SameLine(label_w);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (avail - label_w));
    ImGui::TextColored(value_color, "%s", value);
    ImGui::PopTextWrapPos();
}

void ProfileScreen::render_stat_box(const char* label, const char* value, ImVec4 color) {
    using namespace theme::colors;
    if (color.w == 0) color = TEXT_PRIMARY;

    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild(ImGui::GetID(label), ImVec2(0, 60), ImGuiChildFlags_Borders);
    ImGui::SetCursorPosY(8);
    float w = ImGui::GetContentRegionAvail().x;
    float tw = ImGui::CalcTextSize(value).x;
    ImGui::SetCursorPosX((w - tw) * 0.5f);
    ImGui::TextColored(color, "%s", value);
    float lw = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorPosX((w - lw) * 0.5f);
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}

void ProfileScreen::render_panel_header(const char* title) {
    ImGui::TextColored(theme::colors::ACCENT, "%s", title);
    ImGui::Separator();
    ImGui::Spacing();
}

// ============================================================================
// Section Tabs
// ============================================================================
void ProfileScreen::render_section_tabs() {
    using namespace theme::colors;

    struct Tab { Section s; const char* label; };
    Tab tabs[] = {
        {Section::Overview, "OVERVIEW"}, {Section::Usage, "USAGE"},
        {Section::Security, "SECURITY"}, {Section::Billing, "BILLING"},
        {Section::Support, "SUPPORT"}
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

    for (int i = 0; i < 5; i++) {
        bool active = (active_section_ == tabs[i].s);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        if (ImGui::Button(tabs[i].label)) {
            if (active_section_ != tabs[i].s) {
                active_section_ = tabs[i].s;
                // Fetch data for new section
                if (tabs[i].s == Section::Usage && !usage_data_.valid)
                    fetch_usage_data();
                else if (tabs[i].s == Section::Billing && !subscription_data_.valid)
                    fetch_billing_data();
            }
        }
        ImGui::PopStyleColor(3);
        if (i < 4) ImGui::SameLine();
    }

    ImGui::PopStyleVar(2);
    ImGui::Separator();
}

// ============================================================================
// OVERVIEW
// ============================================================================
void ProfileScreen::render_overview() {
    using namespace theme::colors;
    auto& session = auth::AuthManager::instance().session();

    // Two-column layout
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float col_w = (avail.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    // Left: Account Info
    ImGui::BeginChild("##ov_left", ImVec2(col_w, 0));
    render_panel_header("ACCOUNT INFORMATION");

    render_data_row("USERNAME", session.user_info.username.empty() ? "N/A" : session.user_info.username.c_str());
    render_data_row("EMAIL", session.user_info.email.empty() ? "N/A" : session.user_info.email.c_str());
    render_data_row("USER TYPE", session.is_registered() ? "REGISTERED" : "GUEST");

    std::string at = session.user_info.account_type;
    for (auto& c : at) c = (char)std::toupper(c);
    render_data_row("ACCOUNT TYPE", at.c_str(), ACCENT);
    render_data_row("PHONE", session.user_info.phone.empty() ? "---" : session.user_info.phone.c_str(),
        session.user_info.phone.empty() ? TEXT_DISABLED : TEXT_PRIMARY);
    render_data_row("COUNTRY", session.user_info.country.empty() ? "---" : session.user_info.country.c_str(),
        session.user_info.country.empty() ? TEXT_DISABLED : TEXT_PRIMARY);
    render_data_row("COUNTRY CODE", session.user_info.country_code.empty() ? "---" : session.user_info.country_code.c_str());
    render_data_row("EMAIL VERIFIED", session.user_info.is_verified ? "YES" : "NO",
        session.user_info.is_verified ? SUCCESS : ERROR_RED);
    render_data_row("2FA ENABLED", session.user_info.mfa_enabled ? "YES" : "NO",
        session.user_info.mfa_enabled ? SUCCESS : ERROR_RED);
    render_data_row("MEMBER SINCE", session.user_info.created_at.empty() ? "---" : session.user_info.created_at.substr(0, 10).c_str());
    render_data_row("LAST LOGIN", session.user_info.last_login_at.empty() ? "---" : session.user_info.last_login_at.substr(0, 10).c_str());

    ImGui::Spacing();
    if (theme::AccentButton("EDIT PROFILE")) {
        editing_profile_ = true;
        edit_status_.clear();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Right: Credits & Quick Actions
    ImGui::BeginChild("##ov_right", ImVec2(col_w, 0));

    render_panel_header("CREDITS & BALANCE");

    // Big credit display
    char cred_buf[32];
    std::snprintf(cred_buf, sizeof(cred_buf), "%.2f", session.user_info.credit_balance);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##cred_box", ImVec2(0, 80), ImGuiChildFlags_Borders);
    ImGui::SetCursorPosY(12);
    float tw = ImGui::CalcTextSize(cred_buf).x;
    float bw = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((bw - tw) * 0.5f);

    ImGui::PushFont(nullptr); // Use default but we'll scale
    ImGui::TextColored(INFO, "%s", cred_buf);
    ImGui::PopFont();

    const char* cl = "AVAILABLE CREDITS";
    float clw = ImGui::CalcTextSize(cl).x;
    ImGui::SetCursorPosX((bw - clw) * 0.5f);
    ImGui::TextColored(TEXT_DIM, "%s", cl);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    render_data_row("PLAN", at.c_str(), ACCENT);

    if (session.is_guest()) {
        char rbuf[16];
        std::snprintf(rbuf, sizeof(rbuf), "%d", session.requests_today);
        render_data_row("REQUESTS TODAY", rbuf);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    render_panel_header("QUICK ACTIONS");

    if (theme::AccentButton("EDIT PROFILE##2")) {
        editing_profile_ = true;
        edit_status_.clear();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ERROR_RED);
    ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
    if (ImGui::Button("LOGOUT")) {
        show_logout_confirm_ = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::EndChild();

    // Edit Profile Modal
    if (editing_profile_) {
        ImGui::OpenPopup("Edit Profile");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420, 340));
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);
    if (ImGui::BeginPopupModal("Edit Profile", &editing_profile_, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ACCENT, "EDIT PROFILE");
        ImGui::Separator();
        ImGui::Spacing();

        if (!edit_status_.empty()) {
            bool is_err = edit_status_.find("Error") != std::string::npos || edit_status_.find("Failed") != std::string::npos;
            ImGui::TextColored(is_err ? ERROR_RED : SUCCESS, "%s", edit_status_.c_str());
            ImGui::Spacing();
        }

        ImGui::TextColored(TEXT_DIM, "USERNAME");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##edit_user", edit_username_, sizeof(edit_username_));
        ImGui::PopItemWidth();

        ImGui::TextColored(TEXT_DIM, "PHONE (with country dial code)");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##edit_phone", edit_phone_, sizeof(edit_phone_));
        ImGui::PopItemWidth();

        ImGui::TextColored(TEXT_DIM, "COUNTRY");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##edit_country", edit_country_, sizeof(edit_country_));
        ImGui::PopItemWidth();

        ImGui::TextColored(TEXT_DIM, "COUNTRY CODE (e.g. IN, US)");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##edit_cc", edit_country_code_, sizeof(edit_country_code_));
        ImGui::PopItemWidth();

        ImGui::Spacing();

        float bw2 = ImGui::GetContentRegionAvail().x;
        if (theme::SecondaryButton("CANCEL")) {
            editing_profile_ = false;
            edit_status_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (edit_loading_) {
            ImGui::TextColored(ACCENT, "SAVING...");
        } else if (theme::AccentButton("SAVE CHANGES")) {
            fetch_profile_update();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);

    // Logout confirm modal
    if (show_logout_confirm_) {
        ImGui::OpenPopup("Confirm Logout");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(360, 140));
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, ERROR_RED);
    if (ImGui::BeginPopupModal("Confirm Logout", &show_logout_confirm_, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ERROR_RED, "CONFIRM LOGOUT");
        ImGui::Separator();
        ImGui::TextWrapped("Are you sure you want to logout from Fincept Terminal?");
        ImGui::Spacing();

        if (theme::SecondaryButton("CANCEL")) {
            show_logout_confirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ERROR_RED);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (ImGui::Button("LOGOUT")) {
            show_logout_confirm_ = false;
            auth::AuthManager::instance().logout_async();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
}

// ============================================================================
// USAGE
// ============================================================================
void ProfileScreen::render_usage() {
    using namespace theme::colors;
    auto& session = auth::AuthManager::instance().session();

    // Account & Credits stats
    render_panel_header("ACCOUNT & CREDITS");

    if (ImGui::BeginTable("##usage_stats", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        char cb[32]; std::snprintf(cb, sizeof(cb), "%.2f", session.user_info.credit_balance);
        render_stat_box("CREDIT BALANCE", cb, INFO);

        ImGui::TableNextColumn();
        std::string at = session.user_info.account_type;
        for (auto& c : at) c = (char)std::toupper(c);
        render_stat_box("ACCOUNT TYPE", at.c_str(), ACCENT);

        ImGui::TableNextColumn();
        char rl[16]; std::snprintf(rl, sizeof(rl), "%d", usage_data_.valid ? usage_data_.account.rate_limit_per_hour : 0);
        render_stat_box("RATE LIMIT/HR", rl);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    if (loading_) {
        theme::LoadingSpinner("Loading usage data...");
        return;
    }

    if (!usage_data_.valid) {
        ImGui::TextColored(TEXT_DIM, "Click USAGE tab to load data");
        if (theme::AccentButton("LOAD USAGE DATA")) fetch_usage_data();
        return;
    }

    // Summary
    render_panel_header("USAGE SUMMARY");

    if (ImGui::BeginTable("##sum_stats", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        char tr[16]; std::snprintf(tr, sizeof(tr), "%d", usage_data_.summary.total_requests);
        render_stat_box("TOTAL REQUESTS", tr);

        ImGui::TableNextColumn();
        char cu[16]; std::snprintf(cu, sizeof(cu), "%.1f", usage_data_.summary.total_credits_used);
        render_stat_box("CREDITS USED", cu, WARNING);

        ImGui::TableNextColumn();
        char ac[16]; std::snprintf(ac, sizeof(ac), "%.2f", usage_data_.summary.avg_credits_per_request);
        render_stat_box("AVG CREDITS/REQ", ac);

        ImGui::TableNextColumn();
        char ar[16]; std::snprintf(ar, sizeof(ar), "%.0f", usage_data_.summary.avg_response_time_ms);
        render_stat_box("AVG RESPONSE MS", ar);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Daily usage table
    if (!usage_data_.daily_usage.empty()) {
        render_panel_header("DAILY USAGE");

        // Bar chart
        ImVec2 chart_avail = ImGui::GetContentRegionAvail();
        float chart_h = 60;
        ImVec2 cp = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        int max_reqs = 1;
        for (auto& d : usage_data_.daily_usage) max_reqs = std::max(max_reqs, d.request_count);

        int n = (int)usage_data_.daily_usage.size();
        float bar_w = chart_avail.x / (float)n;

        for (int i = 0; i < n; i++) {
            float h = (usage_data_.daily_usage[i].request_count / (float)max_reqs) * chart_h;
            float x = cp.x + i * bar_w;
            float y = cp.y + chart_h - h;
            dl->AddRectFilled(ImVec2(x + 1, y), ImVec2(x + bar_w - 1, cp.y + chart_h),
                usage_data_.daily_usage[i].credits_used > 0 ? IM_COL32(61, 143, 240, 180) : IM_COL32(100, 100, 100, 120));
        }
        ImGui::Dummy(ImVec2(chart_avail.x, chart_h + 4));

        // Table
        if (ImGui::BeginTable("##daily", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 150))) {
            ImGui::TableSetupColumn("DATE"); ImGui::TableSetupColumn("REQUESTS"); ImGui::TableSetupColumn("CREDITS");
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (int i = (int)usage_data_.daily_usage.size() - 1; i >= 0 && i >= (int)usage_data_.daily_usage.size() - 10; i--) {
                auto& d = usage_data_.daily_usage[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextColored(TEXT_PRIMARY, "%s", d.date.c_str());
                ImGui::TableNextColumn(); ImGui::TextColored(INFO, "%d", d.request_count);
                ImGui::TableNextColumn(); ImGui::TextColored(d.credits_used > 0 ? WARNING : TEXT_DISABLED, "%.1f", d.credits_used);
            }
            ImGui::EndTable();
        }
    }

    // Endpoint breakdown
    if (!usage_data_.endpoint_breakdown.empty()) {
        ImGui::Spacing();
        render_panel_header("TOP ENDPOINTS");

        if (ImGui::BeginTable("##ep", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 180))) {
            ImGui::TableSetupColumn("ENDPOINT", 0, 3.0f);
            ImGui::TableSetupColumn("REQUESTS", 0, 1.0f);
            ImGui::TableSetupColumn("CREDITS", 0, 1.0f);
            ImGui::TableSetupColumn("AVG MS", 0, 1.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < usage_data_.endpoint_breakdown.size() && i < 15; i++) {
                auto& ep = usage_data_.endpoint_breakdown[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextColored(TEXT_PRIMARY, "%s", ep.endpoint.c_str());
                ImGui::TableNextColumn(); ImGui::TextColored(INFO, "%d", ep.request_count);
                ImGui::TableNextColumn(); ImGui::TextColored(ep.credits_used > 0 ? WARNING : TEXT_DISABLED, "%.1f", ep.credits_used);
                ImGui::TableNextColumn(); ImGui::TextColored(TEXT_DIM, "%.0f", ep.avg_response_time_ms);
            }
            ImGui::EndTable();
        }
    }

    // Recent requests
    if (!usage_data_.recent_requests.empty()) {
        ImGui::Spacing();
        render_panel_header("RECENT REQUESTS");

        if (ImGui::BeginTable("##rr", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 180))) {
            ImGui::TableSetupColumn("TIME", 0, 1.5f);
            ImGui::TableSetupColumn("ENDPOINT", 0, 3.0f);
            ImGui::TableSetupColumn("METHOD", 0, 1.0f);
            ImGui::TableSetupColumn("STATUS", 0, 1.0f);
            ImGui::TableSetupColumn("MS", 0, 1.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < usage_data_.recent_requests.size() && i < 20; i++) {
                auto& rq = usage_data_.recent_requests[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // Show just time portion
                std::string ts = rq.timestamp.length() > 11 ? rq.timestamp.substr(11, 8) : rq.timestamp;
                ImGui::TextColored(TEXT_DIM, "%s", ts.c_str());
                ImGui::TableNextColumn(); ImGui::TextColored(TEXT_PRIMARY, "%s", rq.endpoint.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(rq.method == "GET" ? INFO : ACCENT, "%s", rq.method.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(rq.status_code < 400 ? SUCCESS : ERROR_RED, "%d", rq.status_code);
                ImGui::TableNextColumn(); ImGui::TextColored(TEXT_DIM, "%.0f", rq.response_time_ms);
            }
            ImGui::EndTable();
        }
    }
}

// ============================================================================
// SECURITY
// ============================================================================
void ProfileScreen::render_security() {
    using namespace theme::colors;
    auto& session = auth::AuthManager::instance().session();

    // API Key Management
    render_panel_header("API KEY MANAGEMENT");

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##apikey_box", ImVec2(0, 70), ImGuiChildFlags_Borders);

    std::string masked = session.api_key;
    if (!show_api_key_ && masked.length() > 20) {
        masked = masked.substr(0, 20) + std::string(masked.length() - 20, '*');
    }
    ImGui::TextColored(TEXT_SECONDARY, "%s", masked.c_str());
    ImGui::TextColored(TEXT_DISABLED, "Keep your API key secure. Never share it publicly.");

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    if (theme::SecondaryButton(show_api_key_ ? "HIDE KEY" : "SHOW KEY")) {
        show_api_key_ = !show_api_key_;
    }
    ImGui::SameLine();
    if (regen_loading_) {
        ImGui::TextColored(ACCENT, "REGENERATING...");
    } else if (theme::AccentButton("REGENERATE KEY")) {
        show_regen_confirm_ = true;
    }

    if (!regen_status_.empty()) {
        ImGui::Spacing();
        bool is_err = regen_status_.find("Error") != std::string::npos || regen_status_.find("Failed") != std::string::npos;
        ImGui::TextColored(is_err ? ERROR_RED : SUCCESS, "%s", regen_status_.c_str());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Security Status
    render_panel_header("SECURITY STATUS");

    if (!mfa_status_.empty()) {
        bool is_err = mfa_status_.find("Error") != std::string::npos || mfa_status_.find("Failed") != std::string::npos;
        ImGui::TextColored(is_err ? ERROR_RED : SUCCESS, "%s", mfa_status_.c_str());
        ImGui::Spacing();
    }

    // Email verification
    if (ImGui::BeginTable("##sec_rows", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(TEXT_PRIMARY, "EMAIL VERIFICATION");
        ImGui::TableNextColumn();
        ImGui::TextColored(session.user_info.is_verified ? SUCCESS : ERROR_RED,
            session.user_info.is_verified ? "ENABLED" : "DISABLED");

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(TEXT_PRIMARY, "MULTI-FACTOR AUTH (EMAIL)");
        ImGui::TableNextColumn();
        ImGui::TextColored(session.user_info.mfa_enabled ? SUCCESS : ERROR_RED,
            session.user_info.mfa_enabled ? "ENABLED" : "DISABLED");
        ImGui::SameLine();
        if (mfa_loading_) {
            ImGui::TextColored(ACCENT, " PROCESSING...");
        } else {
            if (session.user_info.mfa_enabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ERROR_RED);
                ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
                if (ImGui::SmallButton("DISABLE")) toggle_mfa(false);
                ImGui::PopStyleColor(3);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, SUCCESS);
                ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
                if (ImGui::SmallButton("ENABLE")) toggle_mfa(true);
                ImGui::PopStyleColor(3);
            }
        }

        ImGui::EndTable();
    }

    // Regenerate confirm modal
    if (show_regen_confirm_) {
        ImGui::OpenPopup("Regenerate Key");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 150));
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);
    if (ImGui::BeginPopupModal("Regenerate Key", &show_regen_confirm_, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ACCENT, "REGENERATE API KEY");
        ImGui::Separator();
        ImGui::TextWrapped("Your current API key will be invalidated. Are you sure?");
        ImGui::Spacing();

        if (theme::SecondaryButton("CANCEL")) {
            show_regen_confirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (theme::AccentButton("CONFIRM")) {
            show_regen_confirm_ = false;
            regenerate_api_key();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
}

// ============================================================================
// BILLING
// ============================================================================
void ProfileScreen::render_billing() {
    using namespace theme::colors;

    if (loading_) {
        theme::LoadingSpinner("Loading billing data...");
        return;
    }

    // Account & Plan
    render_panel_header("ACCOUNT & PLAN");

    if (subscription_data_.valid) {
        if (ImGui::BeginTable("##bill_stats", 3, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            std::string plan = subscription_data_.account_type;
            for (auto& c : plan) c = (char)std::toupper(c);
            render_stat_box("PLAN", plan.c_str(), ACCENT);

            ImGui::TableNextColumn();
            char cb[16]; std::snprintf(cb, sizeof(cb), "%.2f", subscription_data_.credit_balance);
            render_stat_box("CREDITS", cb, INFO);

            ImGui::TableNextColumn();
            std::string st = subscription_data_.support_type;
            for (auto& c : st) c = (char)std::toupper(c);
            render_stat_box("SUPPORT", st.c_str());

            ImGui::EndTable();
        }

        ImGui::Spacing();
        render_data_row("MEMBER SINCE", subscription_data_.created_at.substr(0, 10).c_str());
        if (!subscription_data_.credits_expire_at.empty())
            render_data_row("CREDITS EXPIRE", subscription_data_.credits_expire_at.substr(0, 10).c_str(), WARNING);
        if (!subscription_data_.last_credit_purchase_at.empty())
            render_data_row("LAST PURCHASE", subscription_data_.last_credit_purchase_at.substr(0, 10).c_str());
    } else {
        ImGui::TextColored(TEXT_DIM, "No account data available");
        if (theme::AccentButton("LOAD BILLING DATA")) fetch_billing_data();
    }

    ImGui::Spacing();

    // Payment History
    render_panel_header("PAYMENT HISTORY");

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (payment_history_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No payment history");
    } else {
        if (ImGui::BeginTable("##payments", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 250))) {
            ImGui::TableSetupColumn("DATE", 0, 1.5f);
            ImGui::TableSetupColumn("PLAN", 0, 1.5f);
            ImGui::TableSetupColumn("GATEWAY", 0, 1.2f);
            ImGui::TableSetupColumn("AMOUNT", 0, 1.0f);
            ImGui::TableSetupColumn("STATUS", 0, 1.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (auto& p : payment_history_) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(TEXT_PRIMARY, "%s", p.created_at.substr(0, 10).c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(TEXT_PRIMARY, "%s", p.plan_name.empty() ? "PURCHASE" : p.plan_name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(TEXT_DIM, "%s", p.payment_gateway.empty() ? "N/A" : p.payment_gateway.c_str());
                ImGui::TableNextColumn();
                char amt[16]; std::snprintf(amt, sizeof(amt), "$%.2f", p.amount_usd);
                ImGui::TextColored(INFO, "%s", amt);
                ImGui::TableNextColumn();
                ImVec4 sc = (p.status == "success" || p.status == "completed") ? SUCCESS :
                           (p.status == "pending" ? WARNING : ERROR_RED);
                std::string su = p.status;
                for (auto& c : su) c = (char)std::toupper(c);
                ImGui::TextColored(sc, "%s", su.c_str());
            }
            ImGui::EndTable();
        }
    }
}

// ============================================================================
// SUPPORT
// ============================================================================
void ProfileScreen::render_support() {
    using namespace theme::colors;

    render_panel_header("CONTACT & SUPPORT");
    ImGui::TextColored(TEXT_SECONDARY, "For support, reach us at:");
    ImGui::Spacing();

    // Clickable links
    ImGui::TextColored(TEXT_DIM, "EMAIL:");
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, INFO);
    if (ImGui::SmallButton("support@fincept.in")) open_url("mailto:support@fincept.in");
    ImGui::PopStyleColor(3);

    ImGui::TextColored(TEXT_DIM, "DISCORD:");
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, INFO);
    if (ImGui::SmallButton("discord.gg/ae87a8ygbN")) open_url("https://discord.gg/ae87a8ygbN");
    ImGui::PopStyleColor(3);

    ImGui::TextColored(TEXT_DIM, "GITHUB:");
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, INFO);
    if (ImGui::SmallButton("github.com/Fincept-Corporation/FinceptTerminal")) open_url("https://github.com/Fincept-Corporation/FinceptTerminal");
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    render_panel_header("ABOUT");
    render_data_row("VERSION", "3.3.1", ACCENT);
    render_data_row("PLATFORM", "C++ / ImGui Desktop");
    render_data_row("LICENSE", "AGPL-3.0-or-later");
    render_data_row("DEVELOPER", "Fincept Corporation");
}

// ============================================================================
// API Calls (background threads)
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

void ProfileScreen::fetch_billing_data() {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    std::thread([this]() {
        auto& session = auth::AuthManager::instance().session();
        auto& http = http::HttpClient::instance();
        auto hdrs = auth_headers(session.api_key, session.session_token);

        // Fetch subscription + payment history in sequence
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

// ============================================================================
// MAIN RENDER
// ============================================================================
void ProfileScreen::render() {
    init();

    using namespace theme::colors;
    auto& session = auth::AuthManager::instance().session();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_bar_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    float work_y = vp->WorkPos.y + tab_bar_h;
    float work_h = vp->WorkSize.y - tab_bar_h - footer_h;
    float work_w = vp->WorkSize.x;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, work_y));
    ImGui::SetNextWindowSize(ImVec2(work_w, work_h));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARKEST);

    ImGui::Begin("##profile", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Header — left: title + subtitle, right: badges (all on one row, safely spaced)
    std::string subtitle = session.is_registered()
        ? (session.user_info.username.empty() ? session.user_info.email : session.user_info.username)
        : "Guest Session";

    // Build right-side text first to measure
    char cb[32]; std::snprintf(cb, sizeof(cb), "%.2f", session.user_info.credit_balance);
    std::string at = session.user_info.account_type;
    for (auto& c : at) c = (char)std::toupper(c);
    char badge_credits[64]; std::snprintf(badge_credits, sizeof(badge_credits), "CREDITS: %s", cb);
    char badge_plan[64]; std::snprintf(badge_plan, sizeof(badge_plan), "PLAN: %s", at.c_str());
    float badge_w = ImGui::CalcTextSize(badge_credits).x + 16 + ImGui::CalcTextSize(badge_plan).x + 16;

    float win_w = ImGui::GetContentRegionAvail().x;
    float title_w = ImGui::CalcTextSize("PROFILE & ACCOUNT").x;
    float max_subtitle_w = win_w - title_w - badge_w - 40; // 40px safety margin

    ImGui::TextColored(ACCENT, "PROFILE & ACCOUNT");
    ImGui::SameLine(0, 8);

    // Truncate subtitle if it would collide
    if (max_subtitle_w > 60) {
        ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + max_subtitle_w, ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight()),
            true);
        ImGui::TextColored(TEXT_DIM, "%s", subtitle.c_str());
        ImGui::PopClipRect();
    }

    // Right-aligned badges
    ImGui::SameLine(win_w - badge_w + ImGui::GetStyle().WindowPadding.x);
    ImGui::TextColored(INFO, "%s", badge_credits);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(ACCENT, "%s", badge_plan);

    ImGui::Separator();

    // Error banner
    if (!error_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.1f));
        ImGui::BeginChild("##err_banner", ImVec2(0, 24), ImGuiChildFlags_Borders);
        ImGui::TextColored(ERROR_RED, "%s", error_.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // Section tabs
    render_section_tabs();
    ImGui::Spacing();

    // Content (scrollable)
    ImGui::BeginChild("##profile_content", ImVec2(0, 0));

    switch (active_section_) {
        case Section::Overview: render_overview(); break;
        case Section::Usage:    render_usage(); break;
        case Section::Security: render_security(); break;
        case Section::Billing:  render_billing(); break;
        case Section::Support:  render_support(); break;
    }

    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace fincept::profile
