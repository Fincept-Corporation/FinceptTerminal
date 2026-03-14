#include "profile_screen.h"
#include "auth/auth_manager.h"
#include "http/http_client.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fincept::profile {

// ============================================================================
// OVERVIEW
// ============================================================================
void ProfileScreen::render_overview() {
    using namespace theme::colors;
    auto& session = auth::AuthManager::instance().session();

    // Two-column layout via Yoga
    ImVec2 avail = ImGui::GetContentRegionAvail();
    auto panels = ui::two_panel_layout(avail.x, avail.y, true, 50, 280, 500);

    // Left: Account Info
    ImGui::BeginChild("##ov_left", ImVec2(panels.main_w, 0));
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
    ImGui::BeginChild("##ov_right", ImVec2(panels.side_w, 0));

    render_panel_header("CREDITS & BALANCE");

    char cred_buf[32];
    std::snprintf(cred_buf, sizeof(cred_buf), "%.2f", session.user_info.credit_balance);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##cred_box", ImVec2(0, 80), ImGuiChildFlags_Borders);
    ImGui::SetCursorPosY(12);
    float tw = ImGui::CalcTextSize(cred_buf).x;
    float bw = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((bw - tw) * 0.5f);

    ImGui::PushFont(nullptr);
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
static void open_url(const char* url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    std::string cmd = std::string("open ") + url;
    (void)system(cmd.c_str());
#else
    std::string cmd = std::string("xdg-open ") + url;
    (void)system(cmd.c_str());
#endif
}

void ProfileScreen::render_support() {
    using namespace theme::colors;

    render_panel_header("CONTACT & SUPPORT");
    ImGui::TextColored(TEXT_SECONDARY, "For support, reach us at:");
    ImGui::Spacing();

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
    render_data_row("VERSION", "4.0.0", ACCENT);
    render_data_row("PLATFORM", "Fincept Desktop");
    render_data_row("LICENSE", "AGPL-3.0-or-later");
    render_data_row("DEVELOPER", "Fincept Corporation");
}

} // namespace fincept::profile
