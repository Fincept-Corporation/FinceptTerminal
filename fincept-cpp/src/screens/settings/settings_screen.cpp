#include "settings_screen.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cstdio>

namespace fincept::settings {

void SettingsScreen::init() {
    if (initialized_) return;
    initialized_ = true;
}

// ============================================================================
// Sidebar — section navigation (left panel)
// ============================================================================
void SettingsScreen::render_sidebar(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##settings_sidebar", ImVec2(width, height),
                      ImGuiChildFlags_Borders);

    // Header
    ImGui::TextColored(ACCENT, "SETTINGS");
    ImGui::Separator();
    ImGui::Spacing();

    struct SidebarItem {
        Section section;
        const char* label;
        const char* icon;  // text-based icon prefix
    };

    static const SidebarItem items[] = {
        {Section::Credentials,   "Credentials",   "[K]"},
        {Section::LLM,           "LLM Config",    "[A]"},
        {Section::Appearance,    "Appearance",     "[T]"},
        {Section::Storage,       "Storage & Cache","[D]"},
        {Section::Notifications, "Notifications",  "[N]"},
        {Section::General,       "General",        "[G]"},
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

    for (const auto& item : items) {
        bool is_active = (active_section_ == item.section);

        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        }

        char label[128];
        std::snprintf(label, sizeof(label), "%s %s", item.icon, item.label);

        if (ImGui::Button(label, ImVec2(width - 16, 0))) {
            active_section_ = item.section;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Content — active section rendering (right panel)
// ============================================================================
void SettingsScreen::render_content(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##settings_content", ImVec2(width, height),
                      ImGuiChildFlags_Borders);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));

    switch (active_section_) {
        case Section::Credentials:   credentials_.render(); break;
        case Section::LLM:           llm_config_.render(); break;
        case Section::Appearance:    appearance_.render(); break;
        case Section::Storage:       storage_.render(); break;
        case Section::Notifications: notifications_.render(); break;
        case Section::General:       general_.render(); break;
    }

    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Status bar — bottom info strip
// ============================================================================
void SettingsScreen::render_status_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##settings_status", ImVec2(0, ImGui::GetFrameHeight()),
                      ImGuiChildFlags_Borders);

    ImGui::TextColored(TEXT_DIM, "Settings");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);

    // Show section name
    const char* section_names[] = {
        "Credentials", "LLM Configuration", "Appearance",
        "Storage & Cache", "Notifications", "General"
    };
    int idx = static_cast<int>(active_section_);
    if (idx >= 0 && idx < 6) {
        ImGui::TextColored(ACCENT, "%s", section_names[idx]);
    }

    // Status message on right
    if (status_.type != StatusMessage::None) {
        double elapsed = ImGui::GetTime() - status_.expire_time;
        if (elapsed < 0) {
            ImVec4 color = (status_.type == StatusMessage::Error) ? ERROR_RED
                         : (status_.type == StatusMessage::Success) ? SUCCESS
                         : INFO;
            float msg_w = ImGui::CalcTextSize(status_.text.c_str()).x + 16;
            ImGui::SameLine(ImGui::GetWindowWidth() - msg_w);
            ImGui::TextColored(color, "%s", status_.text.c_str());
        } else {
            status_.type = StatusMessage::None;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Main render — Yoga-based responsive layout
// ============================================================================
void SettingsScreen::render() {
    init();

    using namespace theme::colors;

    ui::ScreenFrame frame("##settings", ImVec2(4, 4), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    // Yoga two-panel layout: sidebar (200px) | content (flex)
    float sidebar_w = 200.0f;
    float status_h = ImGui::GetFrameHeight() + 4;
    float content_h = frame.height() - status_h;

    // Sidebar
    render_sidebar(sidebar_w, content_h);
    ImGui::SameLine(0, 4);

    // Content area
    float content_w = frame.width() - sidebar_w - 4;
    render_content(content_w, content_h);

    // Status bar at bottom
    render_status_bar();

    frame.end();
}

} // namespace fincept::settings
