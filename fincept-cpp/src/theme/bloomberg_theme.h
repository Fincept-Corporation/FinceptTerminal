#pragma once
// Bloomberg Terminal-inspired dark theme for Dear ImGui
// Dense, data-focused, monospace, orange accent on near-black background

#include <imgui.h>
#include <string>

namespace fincept::theme {

// Bloomberg color palette
namespace colors {
    // Backgrounds
    constexpr ImVec4 BG_DARKEST     = {0.039f, 0.039f, 0.039f, 1.0f};  // #0A0A0A
    constexpr ImVec4 BG_DARK        = {0.059f, 0.059f, 0.059f, 1.0f};  // #0F0F0F
    constexpr ImVec4 BG_PANEL       = {0.078f, 0.078f, 0.078f, 1.0f};  // #141414
    constexpr ImVec4 BG_WIDGET      = {0.098f, 0.098f, 0.098f, 1.0f};  // #191919
    constexpr ImVec4 BG_INPUT       = {0.118f, 0.118f, 0.118f, 1.0f};  // #1E1E1E
    constexpr ImVec4 BG_HOVER       = {0.157f, 0.157f, 0.157f, 1.0f};  // #282828

    // Borders
    constexpr ImVec4 BORDER_DIM     = {0.157f, 0.157f, 0.157f, 1.0f};  // #282828
    constexpr ImVec4 BORDER         = {0.196f, 0.196f, 0.196f, 1.0f};  // #323232
    constexpr ImVec4 BORDER_BRIGHT  = {0.275f, 0.275f, 0.275f, 1.0f};  // #464646

    // Text
    constexpr ImVec4 TEXT_PRIMARY   = {0.894f, 0.894f, 0.906f, 1.0f};  // #E4E4E7
    constexpr ImVec4 TEXT_SECONDARY = {0.631f, 0.631f, 0.655f, 1.0f};  // #A1A1A7
    constexpr ImVec4 TEXT_DIM       = {0.443f, 0.443f, 0.478f, 1.0f};  // #71717A
    constexpr ImVec4 TEXT_DISABLED  = {0.318f, 0.318f, 0.349f, 1.0f};  // #515159

    // Bloomberg orange accents
    constexpr ImVec4 ACCENT         = {1.0f, 0.4f, 0.0f, 1.0f};       // #FF6600
    constexpr ImVec4 ACCENT_HOVER   = {1.0f, 0.467f, 0.067f, 1.0f};   // #FF7711
    constexpr ImVec4 ACCENT_DIM     = {0.8f, 0.32f, 0.0f, 0.7f};      // #CC5200
    constexpr ImVec4 ACCENT_BG      = {1.0f, 0.4f, 0.0f, 0.12f};      // #FF6600 at 12%

    // Semantic
    constexpr ImVec4 SUCCESS        = {0.2f, 0.78f, 0.35f, 1.0f};     // #33C759
    constexpr ImVec4 ERROR_RED      = {0.94f, 0.27f, 0.27f, 1.0f};    // #F04545
    constexpr ImVec4 WARNING        = {1.0f, 0.73f, 0.0f, 1.0f};      // #FFBB00
    constexpr ImVec4 INFO           = {0.24f, 0.56f, 0.94f, 1.0f};    // #3D8FF0

    // Market colors
    constexpr ImVec4 MARKET_GREEN   = {0.0f, 0.8f, 0.36f, 1.0f};     // #00CC5C
    constexpr ImVec4 MARKET_RED     = {0.94f, 0.2f, 0.2f, 1.0f};     // #F03333
    constexpr ImVec4 MARKET_YELLOW  = {1.0f, 0.84f, 0.0f, 1.0f};     // #FFD700

    // Tab colors
    constexpr ImVec4 TAB_BG         = {0.059f, 0.059f, 0.059f, 1.0f}; // #0F0F0F
    constexpr ImVec4 TAB_ACTIVE     = {0.118f, 0.118f, 0.118f, 1.0f}; // #1E1E1E
    constexpr ImVec4 TAB_HOVERED    = {0.098f, 0.098f, 0.098f, 1.0f}; // #191919

    // Scrollbar
    constexpr ImVec4 SCROLLBAR_BG   = {0.039f, 0.039f, 0.039f, 0.5f};
    constexpr ImVec4 SCROLLBAR_GRAB = {0.196f, 0.196f, 0.196f, 0.8f};
}

inline void ApplyBloombergTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // No rounding — Bloomberg is sharp and angular
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
    c[ImGuiCol_TableRowBgAlt]        = {1.0f, 1.0f, 1.0f, 0.02f};

    // Plot
    c[ImGuiCol_PlotLines]            = colors::ACCENT;
    c[ImGuiCol_PlotLinesHovered]     = colors::ACCENT_HOVER;
    c[ImGuiCol_PlotHistogram]        = colors::ACCENT;
    c[ImGuiCol_PlotHistogramHovered] = colors::ACCENT_HOVER;

    // Drag/drop
    c[ImGuiCol_DragDropTarget]       = colors::ACCENT;
}

// Helper to draw Bloomberg-style section header with orange left border
inline void SectionHeader(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, colors::ACCENT);
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

// Helper for Bloomberg-style status indicator (green/red dot + text)
inline void StatusIndicator(const char* label, bool online) {
    ImVec4 color = online ? colors::SUCCESS : colors::ERROR_RED;
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Bullet();
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextColored(colors::TEXT_SECONDARY, "%s", label);
}

// Helper for accent-colored button (Bloomberg orange)
inline bool AccentButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, colors::ACCENT_DIM);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::ACCENT_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return clicked;
}

// Helper for disabled/secondary button
inline bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, colors::BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::BORDER);
    ImGui::PushStyleColor(ImGuiCol_Text, colors::TEXT_SECONDARY);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return clicked;
}

// Bloomberg-style error message box
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
