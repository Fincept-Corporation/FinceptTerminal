#include "sidebar.h"
#include "theme/bloomberg_theme.h"
#include <imgui.h>

namespace fincept::dashboard {

void Sidebar::render() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::colors::BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    if (ImGui::Begin("Navigation")) {
        // Bloomberg-style navigation sections
        ImGui::TextColored(theme::colors::ACCENT, "TERMINAL");
        ImGui::Spacing();

        struct NavItem {
            const char* label;
            const char* shortcut;
            bool separator_after;
        };

        NavItem core_items[] = {
            {"Dashboard",  "F1",  false},
            {"Markets",    "F2",  false},
            {"News",       "F3",  false},
            {"Watchlist",  "F4",  true},
        };

        NavItem trading_items[] = {
            {"Crypto Trading",  nullptr, false},
            {"Equity Trading",  nullptr, false},
            {"Algo Trading",    nullptr, false},
            {"Backtesting",     nullptr, true},
        };

        NavItem research_items[] = {
            {"Equity Research",  nullptr, false},
            {"Screener",         nullptr, false},
            {"Analytics",        nullptr, false},
            {"Portfolio",        nullptr, false},
            {"Derivatives",      nullptr, true},
        };

        NavItem tools_items[] = {
            {"Code Editor",  nullptr, false},
            {"Node Editor",  nullptr, false},
            {"Excel",        nullptr, false},
            {"Settings",     nullptr, false},
        };

        // Render nav section
        auto render_section = [](const char* title, NavItem* items, int count) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "%s", title);
            ImGui::Spacing();

            for (int i = 0; i < count; i++) {
                ImGui::PushStyleColor(ImGuiCol_Header, theme::colors::BG_WIDGET);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme::colors::BG_HOVER);

                bool selected = ImGui::Selectable(items[i].label);
                if (items[i].shortcut) {
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
                    ImGui::TextColored(theme::colors::TEXT_DISABLED, "%s", items[i].shortcut);
                }

                ImGui::PopStyleColor(2);

                if (items[i].separator_after) {
                    ImGui::Spacing();
                    ImGui::Separator();
                }
            }
        };

        render_section("CORE", core_items, 4);
        render_section("TRADING", trading_items, 4);
        render_section("RESEARCH", research_items, 5);
        render_section("TOOLS", tools_items, 4);
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace fincept::dashboard
