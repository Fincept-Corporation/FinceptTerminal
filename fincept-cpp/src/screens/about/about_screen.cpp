#include "about_screen.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/config.h"
#include <imgui.h>
#include <cstdio>
#include <ctime>

namespace fincept::about {

using namespace theme::colors;

// =============================================================================
// Helper: draw a labeled row in a two-column info block
// =============================================================================
static void info_row(const char* label, const char* value, ImVec4 val_color) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextColored(val_color, "%s", value);
}

// Table flags — clean borders, no row background alternation
static constexpr ImGuiTableFlags TBL =
    ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuterH |
    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterV;

// =============================================================================
// Main render — three-panel layout
// =============================================================================
void AboutScreen::render() {
    ui::ScreenFrame frame("##about_screen", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    render_header_bar();

    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;

    // Three-panel split: left 32% | center 36% | right 32%
    float left_w  = avail_w * 0.32f;
    float right_w = avail_w * 0.32f;
    float center_w = avail_w - left_w - right_w;
    if (center_w < 200) center_w = 200;

    // LEFT PANEL
    ImGui::BeginChild("##about_left", ImVec2(left_w, avail_h), ImGuiChildFlags_Borders);
    render_system_info(left_w, avail_h);
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // CENTER PANEL
    ImGui::BeginChild("##about_center", ImVec2(center_w, avail_h), ImGuiChildFlags_Borders);
    render_license_panel(center_w, avail_h);
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // RIGHT PANEL
    ImGui::BeginChild("##about_right", ImVec2(right_w, avail_h), ImGuiChildFlags_Borders);
    render_resources_panel(right_w, avail_h);
    ImGui::EndChild();

    frame.end();
}

// =============================================================================
// Header bar
// =============================================================================
void AboutScreen::render_header_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##about_header", ImVec2(0, ImGui::GetFrameHeight() + 8), ImGuiChildFlags_Borders);

    ImGui::TextColored(ACCENT, "FINCEPT TERMINAL");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_PRIMARY, "ABOUT");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Timestamp
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char datetime[32];
    std::strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", t);
    ImGui::TextColored(TEXT_DIM, "%s", datetime);

    // Right side — version badge
    char ver_str[32];
    std::snprintf(ver_str, sizeof(ver_str), "v%d.%d.%d",
        config::VERSION_MAJOR, config::VERSION_MINOR, config::VERSION_PATCH);
    float ver_w = ImGui::CalcTextSize(ver_str).x + 16;
    ImGui::SameLine(ImGui::GetWindowWidth() - ver_w);
    ImGui::TextColored(SUCCESS, "%s", ver_str);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Left panel — System Information
// =============================================================================
void AboutScreen::render_system_info(float width, float height) {
    (void)width; (void)height;

    ImGui::TextColored(ACCENT, "SYSTEM INFORMATION");
    ImGui::Separator();

    // Product info
    theme::SectionHeader("PRODUCT");
    if (ImGui::BeginTable("##prod_info", 2, TBL)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Value");

        char ver_str[32];
        std::snprintf(ver_str, sizeof(ver_str), "%d.%d.%d",
            config::VERSION_MAJOR, config::VERSION_MINOR, config::VERSION_PATCH);

        info_row("Product",    "Fincept Terminal",         ACCENT);
        info_row("Version",    ver_str,                    SUCCESS);
        info_row("Build",      "Desktop Native",           TEXT_PRIMARY);
        info_row("Engine",     "C++ / ImGui / OpenGL",     TEXT_PRIMARY);
        info_row("License",    "AGPL-3.0-or-later",        INFO);
        info_row("Copyright",  "2024-2026 Fincept Corp.",  TEXT_SECONDARY);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Platform info
    theme::SectionHeader("PLATFORM");
    if (ImGui::BeginTable("##plat_info", 2, TBL)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Value");

#if defined(_WIN32)
        info_row("OS",         "Windows",             TEXT_PRIMARY);
#elif defined(__APPLE__)
        info_row("OS",         "macOS",               TEXT_PRIMARY);
#else
        info_row("OS",         "Linux",               TEXT_PRIMARY);
#endif
        info_row("Arch",       "x86_64",              TEXT_PRIMARY);
        info_row("Renderer",   "OpenGL 3.3+",         TEXT_PRIMARY);
        info_row("UI",         "Dear ImGui",          TEXT_SECONDARY);
        info_row("Data",       "SQLite + REST",       TEXT_SECONDARY);
        info_row("Python",     "Embedded Runtime",    TEXT_SECONDARY);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Capabilities
    theme::SectionHeader("CAPABILITIES");
    if (ImGui::BeginTable("##caps", 2, TBL)) {
        ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Status");

        const char* modules[] = {
            "Markets", "Trading", "Analytics", "AI / LLM",
            "QuantLib", "Backtesting", "Node Editor", "Geopolitics"
        };
        for (auto* mod : modules) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_PRIMARY, "%s", mod);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(MARKET_GREEN, "ACTIVE");
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Statistics
    theme::SectionHeader("STATISTICS");
    if (ImGui::BeginTable("##stats", 2, TBL)) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Value");

        info_row("Tabs",           "70+",   ACCENT);
        info_row("Data Sources",   "100+",  ACCENT);
        info_row("Python Scripts", "60+",   TEXT_PRIMARY);
        info_row("Rust Commands",  "100+",  TEXT_PRIMARY);
        info_row("Languages",      "8",     TEXT_PRIMARY);
        info_row("QuantLib Tabs",  "18",    TEXT_PRIMARY);

        ImGui::EndTable();
    }
}

// =============================================================================
// Center panel — License & Legal
// =============================================================================
void AboutScreen::render_license_panel(float width, float height) {
    (void)width; (void)height;

    ImGui::TextColored(ACCENT, "LICENSE & LEGAL");
    ImGui::Separator();

    // Open Source License
    theme::SectionHeader("OPEN SOURCE LICENSE");
    if (ImGui::BeginTable("##oss", 2, TBL)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Value");

        info_row("License",   "GNU AGPL v3.0",                MARKET_GREEN);
        info_row("Type",      "Copyleft / Open",               TEXT_PRIMARY);
        info_row("Usage",     "Personal & Non-Commercial",     TEXT_PRIMARY);
        info_row("Reference", "gnu.org/licenses/agpl-3.0",     INFO);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##oss_terms", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 8);
    ImGui::TextColored(TEXT_DIM, "* Source code must remain open");
    ImGui::TextColored(TEXT_DIM, "* Modifications must be shared");
    ImGui::TextColored(TEXT_DIM, "* Network use requires source sharing");
    ImGui::TextColored(TEXT_DIM, "* Same license for derivative works");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Commercial License
    theme::SectionHeader("COMMERCIAL LICENSE");
    if (ImGui::BeginTable("##com", 2, TBL)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Value");

        info_row("License",   "Commercial",                ACCENT);
        info_row("Type",      "Proprietary",               TEXT_PRIMARY);
        info_row("Usage",     "Commercial & Enterprise",   TEXT_PRIMARY);
        info_row("Contact",   "support@fincept.in",        INFO);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##com_terms", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 8);
    ImGui::TextColored(TEXT_DIM, "* No source sharing requirement");
    ImGui::TextColored(TEXT_DIM, "* Priority support included");
    ImGui::TextColored(TEXT_DIM, "* Custom development available");
    ImGui::TextColored(TEXT_DIM, "* Dedicated integration support");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Trademarks
    theme::SectionHeader("TRADEMARKS");

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##tm", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 8);
    ImGui::TextColored(TEXT_SECONDARY,
        "\"Fincept\", \"Fincept Terminal\", and associated logos are "
        "trademarks of Fincept Corporation. Use of these trademarks "
        "without prior written permission is strictly prohibited.");
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM,
        "All other product names, logos, and brands are property of "
        "their respective owners.");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // CLA
    theme::SectionHeader("CONTRIBUTOR AGREEMENT");
    if (ImGui::BeginTable("##cla", 2, TBL)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Value");

        info_row("CLA",       "Required for contributions",   TEXT_PRIMARY);
        info_row("Location",  "See CLA file in repository",   TEXT_SECONDARY);

        ImGui::EndTable();
    }
}

// =============================================================================
// Right panel — Resources & Contact
// =============================================================================
void AboutScreen::render_resources_panel(float width, float height) {
    (void)width; (void)height;

    ImGui::TextColored(ACCENT, "RESOURCES & CONTACT");
    ImGui::Separator();

    // Quick Links
    theme::SectionHeader("QUICK LINKS");
    if (ImGui::BeginTable("##links", 2, TBL)) {
        ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Location");

        info_row("Website",      "fincept.in",                     ACCENT);
        info_row("GitHub",       "github.com/Fincept-Corporation", INFO);
        info_row("MS Store",     "apps.microsoft.com",             INFO);
        info_row("Releases",     "GitHub Releases page",           TEXT_SECONDARY);
        info_row("License",      "LICENSE in repo root",           TEXT_SECONDARY);
        info_row("Docs",         "docs/ directory",                TEXT_SECONDARY);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Contact
    theme::SectionHeader("CONTACT");
    if (ImGui::BeginTable("##contact", 2, TBL)) {
        ImGui::TableSetupColumn("Department", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Email");

        info_row("General",     "support@fincept.in",   INFO);
        info_row("Commercial",  "support@fincept.in",   ACCENT);
        info_row("Security",    "support@fincept.in",   ERROR_RED);
        info_row("Legal",       "support@fincept.in",   WARNING);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Community
    theme::SectionHeader("COMMUNITY");
    if (ImGui::BeginTable("##community", 2, TBL)) {
        ImGui::TableSetupColumn("Platform", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Link");

        info_row("Discord",     "discord.gg/ae87a8ygbN",  INFO);
        info_row("Forum",       "In-app Forum tab",       TEXT_SECONDARY);
        info_row("Discussions", "GitHub Discussions",      TEXT_SECONDARY);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Technology Stack
    theme::SectionHeader("TECHNOLOGY STACK");
    if (ImGui::BeginTable("##techstack", 2, TBL)) {
        ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Technology");

        info_row("Frontend",   "ImGui + OpenGL",       TEXT_PRIMARY);
        info_row("Backend",    "C++ / Python",         TEXT_PRIMARY);
        info_row("Database",   "SQLite",               TEXT_PRIMARY);
        info_row("Networking", "REST + WebSocket",     TEXT_PRIMARY);
        info_row("Charts",     "Custom ImGui Canvas",  TEXT_PRIMARY);
        info_row("AI / LLM",  "Multi-provider",       TEXT_PRIMARY);
        info_row("Quant",     "QuantLib + Custom",     TEXT_PRIMARY);
        info_row("Crypto",    "Kraken / HyperLiquid",  TEXT_PRIMARY);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Build
    theme::SectionHeader("BUILD");
    if (ImGui::BeginTable("##build_info", 2, TBL)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Value");

        info_row("Compiler",   "MSVC / GCC / Clang",   TEXT_PRIMARY);
        info_row("Build Sys",  "CMake + Ninja",        TEXT_PRIMARY);
        info_row("Packages",   "vcpkg",                TEXT_PRIMARY);
#ifdef NDEBUG
        info_row("Config",     "Release",              MARKET_GREEN);
#else
        info_row("Config",     "Debug",                WARNING);
#endif

        ImGui::EndTable();
    }
}

} // namespace fincept::about
