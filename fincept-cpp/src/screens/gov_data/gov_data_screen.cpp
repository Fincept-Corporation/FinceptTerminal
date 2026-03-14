#include "gov_data_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include <imgui.h>
#include <ctime>

namespace fincept::gov_data {

void GovDataScreen::render() {
    ui::ScreenFrame frame("##gov_data_screen", ImVec2(0, 0), theme::colors::BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    // Layout: sidebar(220) | content(flex) | status_bar at bottom(28)
    auto vstack = ui::vstack_layout(w, h, {-1, 28});
    float body_h = vstack.heights[0];

    // --- Sidebar ---
    float sidebar_w = 220.0f;
    float content_w = w - sidebar_w - 1.0f;
    if (content_w < 300) content_w = 300;

    ImGui::BeginChild("##gov_sidebar", ImVec2(sidebar_w, body_h), ImGuiChildFlags_Borders);
    render_sidebar();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // --- Content ---
    ImGui::BeginChild("##gov_content", ImVec2(content_w, body_h), ImGuiChildFlags_Borders);
    render_content();
    ImGui::EndChild();

    // --- Status Bar ---
    render_status_bar();

    frame.end();
}

// =============================================================================
// Sidebar — provider selector list
// =============================================================================
void GovDataScreen::render_sidebar() {
    using namespace theme::colors;

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##gov_sidebar_header", ImVec2(0, 36), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(8, 8));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(provider_colors::CKAN.x, provider_colors::CKAN.y, provider_colors::CKAN.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(provider_colors::CKAN.x, provider_colors::CKAN.y, provider_colors::CKAN.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_Text, provider_colors::CKAN);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(provider_colors::CKAN.x, provider_colors::CKAN.y, provider_colors::CKAN.z, 0.4f));
    ImGui::Button("GOVERNMENT DATA");
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Provider list
    const auto& provs = providers();
    for (size_t i = 0; i < provs.size(); i++) {
        const auto& p = provs[i];
        bool active = (p.id == active_provider_);

        if (active) {
            // Active: left accent bar + tinted bg
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + 3, pos.y + 48),
                ImGui::ColorConvertFloat4ToU32(p.color));

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(p.color.x, p.color.y, p.color.z, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(p.color.x, p.color.y, p.color.z, 0.20f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);
        }

        ImGui::PushID(static_cast<int>(i));
        ImGui::SetCursorPosX(8);

        if (ImGui::Selectable("##prov", active, 0, ImVec2(0, 48))) {
            active_provider_ = p.id;
        }

        // Overlay: flag + name + description
        ImVec2 item_min = ImGui::GetItemRectMin();
        auto* dl = ImGui::GetWindowDrawList();

        // Flag
        char flag_label[8];
        std::snprintf(flag_label, sizeof(flag_label), "%s", p.flag);
        dl->AddText(ImVec2(item_min.x + 12, item_min.y + 6),
                     ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY), flag_label);

        // Name
        dl->AddText(ImVec2(item_min.x + 40, item_min.y + 6),
                     ImGui::ColorConvertFloat4ToU32(active ? p.color : TEXT_PRIMARY), p.name);

        // Description
        std::string desc = truncate(p.description, 28);
        dl->AddText(ImVec2(item_min.x + 40, item_min.y + 24),
                     ImGui::ColorConvertFloat4ToU32(TEXT_DIM), desc.c_str());

        ImGui::PopID();
        ImGui::PopStyleColor(2);
    }
}

// =============================================================================
// Content — dispatch to active provider
// =============================================================================
void GovDataScreen::render_content() {
    switch (active_provider_) {
        case ProviderId::USTreasury:   us_treasury_.render();    break;
        case ProviderId::USCongress:   us_congress_.render();    break;
        case ProviderId::CanadaGov:    canada_gov_.render();     break;
        case ProviderId::OpenAfrica:   openafrica_.render();     break;
        case ProviderId::SpainData:    spain_data_.render();     break;
        case ProviderId::FinlandPxWeb: pxweb_.render();          break;
        case ProviderId::SwissGov:     swiss_gov_.render();      break;
        case ProviderId::FranceGov:    french_gov_.render();     break;
        case ProviderId::UniversalCkan:universal_ckan_.render(); break;
        case ProviderId::HongKongGov:  data_gov_hk_.render();   break;
        default: break;
    }
}

// =============================================================================
// Status Bar
// =============================================================================
void GovDataScreen::render_status_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##gov_status", ImVec2(0, 28), ImGuiChildFlags_Borders);

    // Active provider info
    const auto& provs = providers();
    for (const auto& p : provs) {
        if (p.id == active_provider_) {
            ImGui::TextColored(p.color, "%s", p.name);
            ImGui::SameLine(0, 16);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, 16);
            ImGui::TextColored(TEXT_DIM, "PORTAL:");
            ImGui::SameLine(0, 4);
            ImGui::TextColored(TEXT_SECONDARY, "%s", p.full_name);
            ImGui::SameLine(0, 16);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, 16);
            ImGui::TextColored(TEXT_DIM, "COUNTRY:");
            ImGui::SameLine(0, 4);
            ImGui::TextColored(TEXT_SECONDARY, "%s", p.country);
            break;
        }
    }

    // Right: clock
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tb[32];
    std::strftime(tb, sizeof(tb), "%H:%M:%S", t);
    float time_w = ImGui::CalcTextSize(tb).x + 12;
    ImGui::SameLine(ImGui::GetWindowWidth() - time_w - 140);
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

} // namespace fincept::gov_data
