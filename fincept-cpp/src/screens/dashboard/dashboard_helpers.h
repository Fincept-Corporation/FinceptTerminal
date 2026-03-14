#pragma once
// Shared color palette and formatting helpers for dashboard components
// Used by dashboard_screen.cpp, dashboard_widgets.cpp, dashboard_panels.cpp

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <string>

namespace fincept::dashboard {

// ============================================================================
// Fincept color palette
// ============================================================================
inline const ImVec4 FC_ORANGE  = ImVec4(1.0f, 0.533f, 0.0f, 1.0f);
inline const ImVec4 FC_GREEN   = ImVec4(0.0f, 0.84f, 0.435f, 1.0f);
inline const ImVec4 FC_RED     = ImVec4(1.0f, 0.231f, 0.231f, 1.0f);
inline const ImVec4 FC_CYAN    = ImVec4(0.0f, 0.898f, 1.0f, 1.0f);
inline const ImVec4 FC_YELLOW  = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);
inline const ImVec4 FC_GRAY    = ImVec4(0.47f, 0.47f, 0.47f, 1.0f);
inline const ImVec4 FC_MUTED   = ImVec4(0.29f, 0.29f, 0.29f, 1.0f);
inline const ImVec4 FC_WHITE   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline const ImVec4 FC_PANEL   = ImVec4(0.059f, 0.059f, 0.059f, 1.0f);
inline const ImVec4 FC_HEADER  = ImVec4(0.102f, 0.102f, 0.102f, 1.0f);
inline const ImVec4 FC_BORDER  = ImVec4(0.165f, 0.165f, 0.165f, 1.0f);
inline const ImVec4 FC_DARK    = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
inline const ImVec4 FC_BLUE    = ImVec4(0.0f, 0.533f, 1.0f, 1.0f);
inline const ImVec4 FC_PURPLE  = ImVec4(0.616f, 0.306f, 0.867f, 1.0f);

// ============================================================================
// Formatting helpers
// ============================================================================
inline ImVec4 chg_col(double v) { return v > 0 ? FC_GREEN : (v < 0 ? FC_RED : FC_GRAY); }

inline std::string fmt_price(double v, int decimals = 2) {
    char b[32];
    if (v == 0) return "--";
    if (std::abs(v) >= 1e6) std::snprintf(b, sizeof(b), "%.2fM", v / 1e6);
    else if (std::abs(v) >= 1e4) std::snprintf(b, sizeof(b), "%.*fK", decimals > 0 ? 1 : 0, v / 1e3);
    else std::snprintf(b, sizeof(b), "%.*f", decimals, v);
    return b;
}

inline std::string fmt_volume(double v) {
    char b[32];
    if (v >= 1e9) std::snprintf(b, sizeof(b), "%.1fB", v / 1e9);
    else if (v >= 1e6) std::snprintf(b, sizeof(b), "%.1fM", v / 1e6);
    else if (v >= 1e3) std::snprintf(b, sizeof(b), "%.1fK", v / 1e3);
    else std::snprintf(b, sizeof(b), "%.0f", v);
    return b;
}

inline bool mini_button(const char* label, ImVec4 text_col) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.12f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.08f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, text_col);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    bool clicked = ImGui::SmallButton(label);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return clicked;
}

} // namespace fincept::dashboard
