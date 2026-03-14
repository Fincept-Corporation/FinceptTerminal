#pragma once
// Fincept Terminal dark theme
// Dense, data-focused, monospace, orange accent on near-black background

#include <imgui.h>
#include <string>

namespace fincept::theme {

// Fincept color palette — higher contrast for readability
namespace colors {
    // Backgrounds (lifted ~30-40% brighter while staying dark)
    constexpr ImVec4 BG_DARKEST     = {0.067f, 0.067f, 0.071f, 1.0f};  // #111112
    constexpr ImVec4 BG_DARK        = {0.094f, 0.094f, 0.102f, 1.0f};  // #18181A
    constexpr ImVec4 BG_PANEL       = {0.118f, 0.118f, 0.129f, 1.0f};  // #1E1E21
    constexpr ImVec4 BG_WIDGET      = {0.149f, 0.149f, 0.161f, 1.0f};  // #262629
    constexpr ImVec4 BG_INPUT       = {0.173f, 0.173f, 0.188f, 1.0f};  // #2C2C30
    constexpr ImVec4 BG_HOVER       = {0.212f, 0.212f, 0.227f, 1.0f};  // #36363A

    // Borders (brighter for visible separation)
    constexpr ImVec4 BORDER_DIM     = {0.220f, 0.220f, 0.235f, 1.0f};  // #38383C
    constexpr ImVec4 BORDER         = {0.275f, 0.275f, 0.294f, 1.0f};  // #46464B
    constexpr ImVec4 BORDER_BRIGHT  = {0.353f, 0.353f, 0.376f, 1.0f};  // #5A5A60

    // Text (brighter, higher contrast against backgrounds)
    constexpr ImVec4 TEXT_PRIMARY   = {0.953f, 0.953f, 0.961f, 1.0f};  // #F3F3F5
    constexpr ImVec4 TEXT_SECONDARY = {0.722f, 0.722f, 0.749f, 1.0f};  // #B8B8BF
    constexpr ImVec4 TEXT_DIM       = {0.533f, 0.533f, 0.569f, 1.0f};  // #888891
    constexpr ImVec4 TEXT_DISABLED  = {0.400f, 0.400f, 0.431f, 1.0f};  // #66666E

    // Fincept orange accents
    constexpr ImVec4 ACCENT         = {1.0f, 0.45f, 0.05f, 1.0f};     // #FF730D
    constexpr ImVec4 ACCENT_HOVER   = {1.0f, 0.52f, 0.12f, 1.0f};     // #FF851F
    constexpr ImVec4 ACCENT_DIM     = {0.85f, 0.37f, 0.03f, 0.8f};    // #D95E08
    constexpr ImVec4 ACCENT_BG      = {1.0f, 0.45f, 0.05f, 0.15f};    // #FF730D at 15%

    // Semantic (saturated for visibility)
    constexpr ImVec4 SUCCESS        = {0.25f, 0.84f, 0.42f, 1.0f};    // #40D66B
    constexpr ImVec4 ERROR_RED      = {0.96f, 0.30f, 0.30f, 1.0f};    // #F54D4D
    constexpr ImVec4 WARNING        = {1.0f, 0.78f, 0.08f, 1.0f};     // #FFC714
    constexpr ImVec4 INFO           = {0.30f, 0.60f, 0.96f, 1.0f};    // #4D99F5

    // Market colors (brighter greens/reds)
    constexpr ImVec4 MARKET_GREEN   = {0.05f, 0.85f, 0.42f, 1.0f};    // #0DD96B
    constexpr ImVec4 MARKET_RED     = {0.96f, 0.25f, 0.25f, 1.0f};    // #F54040
    constexpr ImVec4 MARKET_YELLOW  = {1.0f, 0.87f, 0.08f, 1.0f};     // #FFDE14

    // Tab colors (more visible active state)
    constexpr ImVec4 TAB_BG         = {0.082f, 0.082f, 0.090f, 1.0f}; // #151517
    constexpr ImVec4 TAB_ACTIVE     = {0.165f, 0.165f, 0.180f, 1.0f}; // #2A2A2E
    constexpr ImVec4 TAB_HOVERED    = {0.133f, 0.133f, 0.145f, 1.0f}; // #222225

    // Scrollbar
    constexpr ImVec4 SCROLLBAR_BG   = {0.067f, 0.067f, 0.071f, 0.5f};
    constexpr ImVec4 SCROLLBAR_GRAB = {0.275f, 0.275f, 0.294f, 0.8f};
}

inline void ApplyFinceptTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // No rounding — sharp and angular
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 1.0f;  // Minimal rounding on inputs only
    style.PopupRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 0.0f;
    style.TabRounding       = 0.0f;

    // Dense, compact spacing
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.CellPadding       = ImVec2(4.0f, 2.0f);
    style.ItemSpacing       = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 10.0f;
    style.GrabMinSize       = 8.0f;

    // Borders
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 1.0f;

    // Separators
    style.SeparatorTextBorderSize = 1.0f;

    // Docking
    style.DockingSeparatorSize = 2.0f;

    // Colors
    ImVec4* c = style.Colors;

    // Window
    c[ImGuiCol_WindowBg]             = colors::BG_DARK;
    c[ImGuiCol_ChildBg]              = colors::BG_PANEL;
    c[ImGuiCol_PopupBg]              = colors::BG_PANEL;

    // Borders
    c[ImGuiCol_Border]               = colors::BORDER_DIM;
    c[ImGuiCol_BorderShadow]         = {0.0f, 0.0f, 0.0f, 0.0f};

    // Text
    c[ImGuiCol_Text]                 = colors::TEXT_PRIMARY;
    c[ImGuiCol_TextDisabled]         = colors::TEXT_DISABLED;
    c[ImGuiCol_TextSelectedBg]       = colors::ACCENT_DIM;

    // Frame (input fields, checkboxes)
    c[ImGuiCol_FrameBg]              = colors::BG_INPUT;
    c[ImGuiCol_FrameBgHovered]       = colors::BG_HOVER;
    c[ImGuiCol_FrameBgActive]        = colors::BG_HOVER;

    // Title bar
    c[ImGuiCol_TitleBg]              = colors::BG_DARKEST;
    c[ImGuiCol_TitleBgActive]        = colors::BG_DARK;
    c[ImGuiCol_TitleBgCollapsed]     = colors::BG_DARKEST;

    // Menu
    c[ImGuiCol_MenuBarBg]            = colors::BG_DARKEST;

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = colors::SCROLLBAR_BG;
    c[ImGuiCol_ScrollbarGrab]        = colors::SCROLLBAR_GRAB;
    c[ImGuiCol_ScrollbarGrabHovered] = colors::BORDER_BRIGHT;
    c[ImGuiCol_ScrollbarGrabActive]  = colors::TEXT_DIM;

    // Buttons
    c[ImGuiCol_Button]               = colors::BG_WIDGET;
    c[ImGuiCol_ButtonHovered]        = colors::BG_HOVER;
    c[ImGuiCol_ButtonActive]         = colors::ACCENT_DIM;

    // Headers (collapsing headers, selectables)
    c[ImGuiCol_Header]               = colors::BG_WIDGET;
    c[ImGuiCol_HeaderHovered]        = colors::BG_HOVER;
    c[ImGuiCol_HeaderActive]         = colors::ACCENT_DIM;

    // Separator
    c[ImGuiCol_Separator]            = colors::BORDER_DIM;
    c[ImGuiCol_SeparatorHovered]     = colors::ACCENT;
    c[ImGuiCol_SeparatorActive]      = colors::ACCENT;

    // Resize grip
    c[ImGuiCol_ResizeGrip]           = colors::BORDER_DIM;
    c[ImGuiCol_ResizeGripHovered]    = colors::ACCENT;
    c[ImGuiCol_ResizeGripActive]     = colors::ACCENT;

    // Tabs
    c[ImGuiCol_Tab]                  = colors::TAB_BG;
    c[ImGuiCol_TabHovered]           = colors::TAB_HOVERED;
    c[ImGuiCol_TabSelected]          = colors::TAB_ACTIVE;
    c[ImGuiCol_TabSelectedOverline]  = colors::ACCENT;
    c[ImGuiCol_TabDimmed]            = colors::BG_DARKEST;
    c[ImGuiCol_TabDimmedSelected]    = colors::BG_DARK;

    // Docking
    c[ImGuiCol_DockingPreview]       = colors::ACCENT_DIM;
    c[ImGuiCol_DockingEmptyBg]       = colors::BG_DARKEST;

    // Checkmark
    c[ImGuiCol_CheckMark]            = colors::ACCENT;

    // Slider
    c[ImGuiCol_SliderGrab]           = colors::ACCENT_DIM;
    c[ImGuiCol_SliderGrabActive]     = colors::ACCENT;

    // Nav
    c[ImGuiCol_NavHighlight]         = colors::ACCENT;
    c[ImGuiCol_NavWindowingHighlight]= {1.0f, 1.0f, 1.0f, 0.7f};
    c[ImGuiCol_NavWindowingDimBg]    = {0.0f, 0.0f, 0.0f, 0.5f};

    // Modal dim
    c[ImGuiCol_ModalWindowDimBg]     = {0.0f, 0.0f, 0.0f, 0.65f};

    // Table
    c[ImGuiCol_TableHeaderBg]        = colors::BG_WIDGET;
    c[ImGuiCol_TableBorderStrong]    = colors::BORDER;
    c[ImGuiCol_TableBorderLight]     = colors::BORDER_DIM;
    c[ImGuiCol_TableRowBg]           = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_TableRowBgAlt]        = {1.0f, 1.0f, 1.0f, 0.04f};

    // Plot
    c[ImGuiCol_PlotLines]            = colors::ACCENT;
    c[ImGuiCol_PlotLinesHovered]     = colors::ACCENT_HOVER;
    c[ImGuiCol_PlotHistogram]        = colors::ACCENT;
    c[ImGuiCol_PlotHistogramHovered] = colors::ACCENT_HOVER;

    // Drag/drop
    c[ImGuiCol_DragDropTarget]       = colors::ACCENT;
}

// Helper to draw section header with orange accent
inline void SectionHeader(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, colors::ACCENT);
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

// Helper for status indicator (green/red dot + text)
inline void StatusIndicator(const char* label, bool online) {
    ImVec4 color = online ? colors::SUCCESS : colors::ERROR_RED;
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Bullet();
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextColored(colors::TEXT_SECONDARY, "%s", label);
}

// Helper for accent-colored button
inline bool AccentButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, colors::ACCENT_DIM);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::ACCENT_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

// Helper for disabled/secondary button
inline bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, colors::BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::BORDER);
    ImGui::PushStyleColor(ImGuiCol_Text, colors::TEXT_SECONDARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return clicked;
}

// Error message box
inline void ErrorMessage(const char* msg) {
    if (!msg || msg[0] == '\0') return;
    ImGui::PushStyleColor(ImGuiCol_Text, colors::ERROR_RED);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.27f, 0.27f, 0.08f));
    ImGui::BeginChild("##error_msg", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::TextWrapped("%s", msg);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// Loading spinner (simple rotating text indicator)
inline void LoadingSpinner(const char* label = "Loading...") {
    static const char* frames[] = {"|", "/", "-", "\\"};
    int frame = (int)(ImGui::GetTime() / 0.15f) % 4;
    ImGui::TextColored(colors::ACCENT, "%s %s", frames[frame], label);
}

} // namespace fincept::theme
