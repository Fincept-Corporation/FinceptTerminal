#include "mna_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include <imgui.h>
#include <ctime>
#include <cstdio>

namespace fincept::mna {

void MNAScreen::init() {
    if (initialized_) return;
    initialized_ = true;
}

void MNAScreen::render() {
    init();

    ui::ScreenFrame frame("##mna_screen", ImVec2(0, 0), theme::colors::BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    // Vertical stack: nav_bar(44) | body(flex) | status_bar(28)
    auto vstack = ui::vstack_layout(w, h, {44, -1, 28});

    // === NAV BAR ===
    render_nav_bar(vstack.heights[0]);

    // === MAIN BODY (3-panel) ===
    float body_h = vstack.heights[1];
    float left_w = left_panel_open_ ? 200.0f : 36.0f;
    float right_w = right_panel_open_ ? 220.0f : 36.0f;
    float center_w = w - left_w - right_w - 2.0f;  // 2px for borders
    if (center_w < 200) center_w = 200;

    // Left sidebar
    ImGui::BeginChild("##mna_left", ImVec2(left_w, body_h), ImGuiChildFlags_Borders);
    render_left_sidebar(left_w, body_h);
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // Center content
    ImGui::BeginChild("##mna_center", ImVec2(center_w, body_h), ImGuiChildFlags_Borders);
    render_center_content(center_w, body_h);
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // Right info panel
    ImGui::BeginChild("##mna_right", ImVec2(right_w, body_h), ImGuiChildFlags_Borders);
    render_right_info(right_w, body_h);
    ImGui::EndChild();

    // === STATUS BAR ===
    render_status_bar(vstack.heights[2]);

    frame.end();
}

// =============================================================================
// Nav Bar — branding + module quick selector + status badge
// =============================================================================
void MNAScreen::render_nav_bar(float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mna_nav", ImVec2(0, height), ImGuiChildFlags_Borders);

    // Branding badge
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.4f));
    ImGui::Button("M&A ANALYTICS");
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    // Module quick selector buttons
    auto* mods = modules();
    for (int i = 0; i < MODULE_COUNT; i++) {
        bool active = (i == active_module_);
        if (ColoredButton(mods[i].short_label, mods[i].color, active)) {
            active_module_ = i;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", mods[i].label);
        }
        if (i < MODULE_COUNT - 1) ImGui::SameLine(0, 2);
    }

    // Right side: status badge
    float badge_w = ImGui::CalcTextSize("READY").x + 30;
    ImGui::SameLine(ImGui::GetWindowWidth() - badge_w - 120);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.4f));
    ImGui::SmallButton("READY");
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "CORPORATE FINANCE TOOLKIT");

    ImGui::EndChild();
    ImGui::PopStyleColor();  // ChildBg
}

// =============================================================================
// Left Sidebar — module navigation
// =============================================================================
void MNAScreen::render_left_sidebar(float /*width*/, float /*height*/) {
    using namespace theme::colors;
    auto* mods = modules();

    // Header
    if (left_panel_open_) {
        ImGui::TextColored(ACCENT, "MODULES");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
    }
    if (ImGui::SmallButton(left_panel_open_ ? "<<" : ">>")) {
        left_panel_open_ = !left_panel_open_;
    }
    ImGui::Separator();

    if (!left_panel_open_) {
        // Collapsed: just icons/short labels vertically
        for (int i = 0; i < MODULE_COUNT; i++) {
            bool active = (i == active_module_);
            ImVec4 col = active ? mods[i].color : TEXT_DIM;
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImVec4(mods[i].color.x, mods[i].color.y, mods[i].color.z, 0.15f));
            }
            if (ImGui::Selectable(mods[i].short_label, active, 0, ImVec2(0, 20))) {
                active_module_ = i;
            }
            if (active) ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", mods[i].label);
        }
        return;
    }

    // Expanded: categorized module list
    const char* categories[] = {"CORE", "SPECIALIZED", "ANALYTICS"};
    for (const char* cat : categories) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "%s", cat);
        ImGui::Spacing();

        for (int i = 0; i < MODULE_COUNT; i++) {
            if (std::strcmp(mods[i].category, cat) != 0) continue;

            bool active = (i == active_module_);
            ImVec4 col = active ? mods[i].color : TEXT_SECONDARY;

            // Left accent bar for active
            if (active) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    p, ImVec2(p.x + 3, p.y + 22),
                    ImGui::ColorConvertFloat4ToU32(mods[i].color));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6);
            }

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImVec4(mods[i].color.x, mods[i].color.y, mods[i].color.z, 0.15f));
            }
            if (ImGui::Selectable(mods[i].label, active, 0, ImVec2(0, 22))) {
                active_module_ = i;
            }
            if (active) ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }
    }
}

// =============================================================================
// Center Content — dispatch to active panel
// =============================================================================
void MNAScreen::render_center_content(float /*width*/, float /*height*/) {
    using namespace theme::colors;
    auto* mods = modules();

    // Module header bar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mna_module_header", ImVec2(0, 32), ImGuiChildFlags_Borders);
    ImGui::TextColored(mods[active_module_].color, "%s", mods[active_module_].label);
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "%s", mods[active_module_].category);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Content area
    ImGui::BeginChild("##mna_content_area", ImVec2(0, 0));
    switch (active_module_) {
        case 0: valuation_panel_.render(data_);   break;
        case 1: merger_panel_.render(data_);       break;
        case 2: deal_db_panel_.render(data_);      break;
        case 3: startup_panel_.render(data_);      break;
        case 4: fairness_panel_.render(data_);     break;
        case 5: industry_panel_.render(data_);     break;
        case 6: advanced_panel_.render(data_);     break;
        case 7: comparison_panel_.render(data_);   break;
        default: break;
    }
    ImGui::EndChild();
}

// =============================================================================
// Right Info Panel — module info, capabilities, stats, tips
// =============================================================================
void MNAScreen::render_right_info(float /*width*/, float /*height*/) {
    using namespace theme::colors;
    auto* mods = modules();

    // Header
    if (right_panel_open_) {
        ImGui::TextColored(ma_colors::MERGER, "MODULE INFO");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
    }
    if (ImGui::SmallButton(right_panel_open_ ? ">>" : "<<")) {
        right_panel_open_ = !right_panel_open_;
    }
    ImGui::Separator();

    if (!right_panel_open_) return;

    // Active module info card
    auto& mod = mods[active_module_];
    ImGui::Spacing();

    // Module card with left accent border
    ImVec2 card_pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        card_pos, ImVec2(card_pos.x + 3, card_pos.y + 50),
        ImGui::ColorConvertFloat4ToU32(mod.color));

    ImGui::Indent(8);
    ImGui::TextColored(mod.color, "%s", mod.label);
    ImGui::TextColored(TEXT_DIM, "%s module", mod.category);
    ImGui::Unindent(8);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Capabilities list
    ImGui::TextColored(ACCENT, "CAPABILITIES");
    ImGui::Spacing();
    auto caps = module_capabilities(active_module_);
    for (int i = 0; caps[i] != nullptr; i++) {
        ImGui::TextColored(mod.color, ">");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_PRIMARY, "%s", caps[i]);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Quick stats
    ImGui::TextColored(ma_colors::MERGER, "QUICK STATS");
    ImGui::Spacing();
    DataRow("Total Modules", "8", TEXT_PRIMARY);
    DataRow("Valuation Methods", "15+", TEXT_PRIMARY);
    DataRow("Python Scripts", "30+", TEXT_PRIMARY);
    DataRow("Analysis Types", "60+", TEXT_PRIMARY);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Tips
    ImGui::TextColored(TEXT_DIM, "TIPS");
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM,
        "Click sidebar modules to switch views. "
        "Each module provides professional-grade "
        "analytics with export capabilities.");
}

// =============================================================================
// Status Bar
// =============================================================================
void MNAScreen::render_status_bar(float height) {
    using namespace theme::colors;
    auto* mods = modules();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##mna_status", ImVec2(0, height), ImGuiChildFlags_Borders);

    // Active module
    ImGui::TextColored(mods[active_module_].color, "%s", mods[active_module_].label);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "MODULE:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_SECONDARY, "%s", mods[active_module_].category);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "ENGINE:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(SUCCESS, "PYTHON + C++");

    // Right: clock
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tb[32];
    std::strftime(tb, sizeof(tb), "%H:%M:%S", t);
    float time_w = ImGui::CalcTextSize(tb).x + 12;
    ImGui::SameLine(ImGui::GetWindowWidth() - time_w - 120);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "FINCEPT TERMINAL v4.0");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_SECONDARY, "%s", tb);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::mna
