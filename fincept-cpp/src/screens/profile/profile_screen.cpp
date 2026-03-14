#include "profile_screen.h"
#include "auth/auth_manager.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>

namespace fincept::profile {

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
    float label_w = avail * 0.35f;

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
// MAIN RENDER — Yoga-based responsive layout
// ============================================================================
void ProfileScreen::render() {
    init();

    using namespace theme::colors;
    auto& session = auth::AuthManager::instance().session();

    // ScreenFrame replaces all manual viewport math
    ui::ScreenFrame frame("##profile", ImVec2(12, 8), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    // Yoga vertical stack: header(~24) + error(24 if visible) + tabs(~30) + content(flex)
    float header_h = ImGui::GetTextLineHeightWithSpacing() + 4;
    float error_h = error_.empty() ? 0 : 24;
    float tabs_h = ImGui::GetFrameHeightWithSpacing() + 4;
    auto vstack = ui::vstack_layout(frame.width(), frame.height(),
        error_h > 0 ? std::vector<float>{header_h, error_h, tabs_h, -1}
                     : std::vector<float>{header_h, tabs_h, -1});

    // Header
    std::string subtitle = session.is_registered()
        ? (session.user_info.username.empty() ? session.user_info.email : session.user_info.username)
        : "Guest Session";

    char cb[32]; std::snprintf(cb, sizeof(cb), "%.2f", session.user_info.credit_balance);
    std::string at = session.user_info.account_type;
    for (auto& c : at) c = (char)std::toupper(c);
    char badge_credits[64]; std::snprintf(badge_credits, sizeof(badge_credits), "CREDITS: %s", cb);
    char badge_plan[64]; std::snprintf(badge_plan, sizeof(badge_plan), "PLAN: %s", at.c_str());
    float badge_w = ImGui::CalcTextSize(badge_credits).x + 16 + ImGui::CalcTextSize(badge_plan).x + 16;

    float win_w = ImGui::GetContentRegionAvail().x;
    float title_w = ImGui::CalcTextSize("PROFILE & ACCOUNT").x;
    float max_subtitle_w = win_w - title_w - badge_w - 40;

    ImGui::TextColored(ACCENT, "PROFILE & ACCOUNT");
    ImGui::SameLine(0, 8);

    if (max_subtitle_w > 60) {
        ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + max_subtitle_w, ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight()),
            true);
        ImGui::TextColored(TEXT_DIM, "%s", subtitle.c_str());
        ImGui::PopClipRect();
    }

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

    // Content (scrollable) — Yoga-computed height
    float content_h = vstack.heights.back();
    ImGui::BeginChild("##profile_content", ImVec2(0, content_h));

    switch (active_section_) {
        case Section::Overview: render_overview(); break;
        case Section::Usage:    render_usage(); break;
        case Section::Security: render_security(); break;
        case Section::Billing:  render_billing(); break;
        case Section::Support:  render_support(); break;
    }

    ImGui::EndChild();

    frame.end();
}

} // namespace fincept::profile
