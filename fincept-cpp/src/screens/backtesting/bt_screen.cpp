#include "backtesting_screen.h"
#include "backtesting_constants.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace fincept::backtesting {

using json = nlohmann::json;

// =============================================================================
// Main render
// =============================================================================
void BacktestingScreen::render() {
    using namespace theme::colors;
    ui::ScreenFrame frame("##backtesting", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    pickup_result();

    // Top nav bar
    render_top_nav();

    // Three-panel layout — fills available content area
    float avail_h = ImGui::GetContentRegionAvail().y - 24; // reserve status bar
    float avail_w = ImGui::GetContentRegionAvail().x;

    float left_w = 260.0f;
    float right_w = 300.0f;
    float center_w = avail_w - left_w - right_w;
    if (center_w < 200) center_w = 200;

    ImGui::BeginChild("##bt_body", ImVec2(0, avail_h));

    // LEFT PANEL — Commands + Categories
    ImGui::BeginChild("##bt_left", ImVec2(left_w, 0), ImGuiChildFlags_Borders);
    render_command_panel();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // CENTER PANEL — Config (flex)
    ImGui::BeginChild("##bt_center", ImVec2(center_w, 0), ImGuiChildFlags_Borders);
    render_config_panel();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // RIGHT PANEL — Results
    ImGui::BeginChild("##bt_right", ImVec2(right_w, 0), ImGuiChildFlags_Borders);
    render_results_panel();
    ImGui::EndChild();

    ImGui::EndChild(); // bt_body

    // Status bar
    render_status_bar();

    frame.end();
}

// =============================================================================
// Top navigation bar — provider tabs + advanced toggle + status
// =============================================================================
void BacktestingScreen::render_top_nav() {
    using namespace theme::colors;
    auto color = provider_color(selected_provider_);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##bt_topnav", ImVec2(0, 36), ImGuiChildFlags_Borders);

    // Title
    ImGui::TextColored(ACCENT, "BACKTESTING");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    // Provider tabs
    for (auto& p : provider_list()) {
        if (ProviderTab(p.display_name, provider_color(p.slug), selected_provider_ == p.slug)) {
            selected_provider_ = p.slug;
            LOG_INFO("Backtesting", "Provider switched to: %s", p.display_name);
            // Reset to first available command
            auto cmds = provider_commands(selected_provider_);
            if (!cmds.empty()) active_command_ = cmds[0];
            // Reset category
            auto cats = get_categories(selected_provider_);
            if (!cats.empty()) selected_category_ = cats[0].key;
            selected_strategy_idx_ = 0;
            init_strategy_params();
            result_ = json();
            has_error_ = false;
        }
        ImGui::SameLine(0, 2);
    }

    // Right side — Advanced toggle + Status
    float right_x = ImGui::GetWindowWidth() - 200;
    ImGui::SameLine(right_x);

    // Advanced toggle
    if (show_advanced_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    }
    if (ImGui::SmallButton("ADVANCED")) show_advanced_ = !show_advanced_;
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 12);

    // Status indicator
    bool running = data_.is_loading();
    ImVec4 status_color = running ? ACCENT : SUCCESS;
    ImGui::TextColored(status_color, "%s", running ? "RUNNING" : "READY");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Command panel (left)
// =============================================================================
void BacktestingScreen::render_command_panel() {
    using namespace theme::colors;

    auto cmds = provider_commands(selected_provider_);
    auto color = provider_color(selected_provider_);

    // Commands header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##bt_cmd_hdr", ImVec2(0, 24));
    ImGui::TextColored(TEXT_DIM, "COMMANDS");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
    ImGui::TextColored(color, "%d", (int)cmds.size());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Command list
    for (auto& cmd_type : cmds) {
        const CommandDef* def = nullptr;
        for (auto& c : all_commands()) {
            if (c.type == cmd_type) { def = &c; break; }
        }
        if (!def) continue;

        if (CommandItem(def->label, def->description, def->color, active_command_ == cmd_type)) {
            active_command_ = cmd_type;
        }
    }

    // Strategy categories (only for backtest/optimize/walk_forward)
    bool show_categories = (active_command_ == CommandType::Backtest ||
                           active_command_ == CommandType::Optimize ||
                           active_command_ == CommandType::WalkForward);
    if (!show_categories) return;

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##bt_cat_hdr", ImVec2(0, 24));
    ImGui::TextColored(TEXT_DIM, "STRATEGIES");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    auto cats = get_categories(selected_provider_);
    for (auto& cat : cats) {
        auto strats = get_strategies(selected_provider_, cat.key);
        bool active = (selected_category_ == cat.key);

        if (CommandItem(cat.label, "", cat.color, active)) {
            selected_category_ = cat.key;
            selected_strategy_idx_ = 0;
            init_strategy_params();
        }

        // Show count
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 220, pos.y - 30));
        ImGui::TextColored(active ? cat.color : TEXT_DISABLED, "%d", (int)strats.size());
        ImGui::SetCursorScreenPos(pos);
    }
}


} // namespace fincept::backtesting
