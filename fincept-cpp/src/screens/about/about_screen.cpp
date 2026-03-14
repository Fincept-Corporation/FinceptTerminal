#include "about_screen.h"
#include "ui/theme.h"
#include "core/config.h"
#include <imgui.h>
#include <cstdio>

namespace fincept::about {

// =============================================================================
// Main render
// =============================================================================
void AboutScreen::render() {
    using namespace theme::colors;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##about_screen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    float avail_w = ImGui::GetContentRegionAvail().x;
    float content_w = (avail_w > 960.0f) ? 960.0f : avail_w;
    float pad_x = (avail_w - content_w) * 0.5f;
    if (pad_x < 20.0f) pad_x = 20.0f;

    // Scrollable centered column
    ImGui::SetCursorPosX(pad_x);
    ImGui::BeginChild("##about_content", ImVec2(content_w, 0), ImGuiChildFlags_None);

    ImGui::Spacing(); ImGui::Spacing();

    render_version_info();
    ImGui::Spacing(); ImGui::Spacing();

    render_license_panels();
    ImGui::Spacing(); ImGui::Spacing();

    render_trademarks();
    ImGui::Spacing(); ImGui::Spacing();

    render_resources();
    ImGui::Spacing(); ImGui::Spacing();

    render_contact();
    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// =============================================================================
// Version Info
// =============================================================================
void AboutScreen::render_version_info() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##version_info", ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::Spacing();

    // Title
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16);
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("FINCEPT TERMINAL");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16);
    ImGui::TextColored(TEXT_SECONDARY, "Cross-Platform Financial Intelligence Terminal");

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Info grid
    float label_x = ImGui::GetCursorPosX() + 16;

    auto info_row = [&](const char* label, const char* value, ImVec4 val_color) {
        ImGui::SetCursorPosX(label_x);
        ImGui::TextColored(TEXT_DIM, "%s", label);
        ImGui::SameLine(200);
        ImGui::TextColored(val_color, "%s", value);
    };

    char ver_str[32];
    std::snprintf(ver_str, sizeof(ver_str), "%d.%d.%d",
        config::VERSION_MAJOR, config::VERSION_MINOR, config::VERSION_PATCH);

    info_row("Version:",      ver_str,                           SUCCESS);
    info_row("Platform:",     "Desktop (C++ / ImGui / OpenGL)",  TEXT_PRIMARY);
    info_row("License:",      "AGPL-3.0-or-later",              INFO);
    info_row("Copyright:",    "2024-2026 Fincept Corporation",   TEXT_SECONDARY);
    info_row("Website:",      "https://fincept.in",              ACCENT);

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// License Panels
// =============================================================================
void AboutScreen::render_license_panels() {
    using namespace theme::colors;

    theme::SectionHeader("LICENSE INFORMATION");

    float avail = ImGui::GetContentRegionAvail().x;
    float col_w = (avail - 12) * 0.5f;

    // Two-column layout
    ImGui::BeginGroup();

    // Open Source License
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##oss_license", ImVec2(col_w, 220),
        ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(16, 12));
    ImGui::TextColored(MARKET_GREEN, "[Open Source]");
    ImGui::SetCursorPosX(16);
    ImGui::TextColored(TEXT_PRIMARY, "GNU AGPL v3.0");

    ImGui::Spacing();
    ImGui::SetCursorPosX(16);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + col_w - 32);
    ImGui::TextColored(TEXT_SECONDARY, "Free for personal and non-commercial use.");
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "* Source code must remain open");
    ImGui::TextColored(TEXT_DIM, "* Modifications must be shared");
    ImGui::TextColored(TEXT_DIM, "* Network use requires source sharing");
    ImGui::TextColored(TEXT_DIM, "* Same license for derivative works");
    ImGui::Spacing();
    ImGui::TextColored(INFO, "gnu.org/licenses/agpl-3.0");
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 12);

    // Commercial License
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##com_license", ImVec2(col_w, 220),
        ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(16, 12));
    ImGui::TextColored(ACCENT, "[Commercial]");
    ImGui::SetCursorPosX(16);
    ImGui::TextColored(TEXT_PRIMARY, "Commercial License");

    ImGui::Spacing();
    ImGui::SetCursorPosX(16);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + col_w - 32);
    ImGui::TextColored(TEXT_SECONDARY, "Required for commercial or proprietary use.");
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "* No source sharing requirement");
    ImGui::TextColored(TEXT_DIM, "* Priority support included");
    ImGui::TextColored(TEXT_DIM, "* Custom development available");
    ImGui::TextColored(TEXT_DIM, "* Dedicated integration support");
    ImGui::Spacing();
    ImGui::TextColored(INFO, "Contact: support@fincept.in");
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::EndGroup();
}

// =============================================================================
// Trademarks
// =============================================================================
void AboutScreen::render_trademarks() {
    using namespace theme::colors;

    theme::SectionHeader("TRADEMARKS");

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##trademarks", ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(16, 12));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 16);
    ImGui::TextColored(TEXT_SECONDARY,
        "\"Fincept\", \"Fincept Terminal\", and associated logos are trademarks "
        "of Fincept Corporation. Use of these trademarks without prior written "
        "permission from Fincept Corporation is strictly prohibited.");
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM,
        "All other product names, logos, and brands are property of their "
        "respective owners.");
    ImGui::PopTextWrapPos();

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Resources
// =============================================================================
void AboutScreen::render_resources() {
    using namespace theme::colors;

    theme::SectionHeader("RESOURCES");

    float avail = ImGui::GetContentRegionAvail().x;
    float col_w = (avail - 12) * 0.5f;

    struct Resource {
        const char* label;
        const char* url;
        ImVec4 color;
    };

    Resource resources[] = {
        {"GitHub Repository",  "github.com/Fincept-Corporation/FinceptTerminal", ACCENT},
        {"License File",       "See LICENSE in repository root",                 INFO},
        {"Commercial License", "Contact support@fincept.in",                     WARNING},
        {"Trademarks",         "See TRADEMARKS section above",                   TEXT_SECONDARY},
        {"CLA",                "Contributor License Agreement in repo",          TEXT_SECONDARY},
        {"Website",            "https://fincept.in",                             ACCENT},
    };

    // 3x2 grid
    for (int i = 0; i < 6; i += 2) {
        ImGui::BeginGroup();

        for (int j = 0; j < 2 && (i + j) < 6; j++) {
            if (j > 0) ImGui::SameLine(0, 12);
            auto& r = resources[i + j];

            ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
            char id[32];
            std::snprintf(id, sizeof(id), "##res_%d", i + j);
            ImGui::BeginChild(id, ImVec2(col_w, 60), ImGuiChildFlags_Borders);

            ImGui::SetCursorPos(ImVec2(16, 8));
            ImGui::TextColored(TEXT_PRIMARY, "%s", r.label);
            ImGui::SetCursorPosX(16);
            ImGui::TextColored(r.color, "%s", r.url);

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::EndGroup();
    }
}

// =============================================================================
// Contact
// =============================================================================
void AboutScreen::render_contact() {
    using namespace theme::colors;

    theme::SectionHeader("CONTACT");

    float avail = ImGui::GetContentRegionAvail().x;
    float col_w = (avail - 12) * 0.5f;

    struct Contact {
        const char* dept;
        const char* email;
        ImVec4 color;
    };

    Contact contacts[] = {
        {"General Inquiries",   "support@fincept.in", INFO},
        {"Commercial Licensing","support@fincept.in", ACCENT},
        {"Security Issues",     "support@fincept.in", ERROR_RED},
        {"Legal",               "support@fincept.in", WARNING},
    };

    // 2x2 grid
    for (int i = 0; i < 4; i += 2) {
        ImGui::BeginGroup();

        for (int j = 0; j < 2; j++) {
            if (j > 0) ImGui::SameLine(0, 12);
            auto& c = contacts[i + j];

            ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
            char id[32];
            std::snprintf(id, sizeof(id), "##contact_%d", i + j);
            ImGui::BeginChild(id, ImVec2(col_w, 60), ImGuiChildFlags_Borders);

            ImGui::SetCursorPos(ImVec2(16, 8));
            ImGui::TextColored(TEXT_PRIMARY, "%s", c.dept);
            ImGui::SetCursorPosX(16);
            ImGui::TextColored(c.color, "%s", c.email);

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::EndGroup();
    }

    ImGui::Spacing();

    // Discord link
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##discord", ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(16, 8));
    ImGui::TextColored(INFO, "Discord Community");
    ImGui::SetCursorPosX(16);
    ImGui::TextColored(TEXT_SECONDARY, "Join our community: discord.gg/ae87a8ygbN");
    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::about
