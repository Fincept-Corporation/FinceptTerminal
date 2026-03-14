#pragma once
// Auth UI — Geometric background art, branded header, polished form helpers
// Art-deco inspired financial terminal aesthetic

#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cmath>

namespace fincept::auth::ui_detail {

// ============================================================================
// Geometric Background — draws behind the auth panel
// ============================================================================
inline void DrawAuthBackground() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float t = (float)ImGui::GetTime();

    // Base gradient — dark to slightly lighter
    ImU32 top    = IM_COL32(10, 10, 14, 255);
    ImU32 bottom = IM_COL32(18, 18, 24, 255);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), display, top, top, bottom, bottom);

    // --- Diagonal grid lines (subtle, slow drift) ---
    {
        ImU32 line_col = IM_COL32(255, 115, 13, 12); // ACCENT at ~5% opacity
        float spacing = 80.0f;
        float offset = fmodf(t * 8.0f, spacing); // slow drift

        // Lines going top-left to bottom-right
        for (float i = -display.y; i < display.x + display.y; i += spacing) {
            dl->AddLine(
                ImVec2(i + offset, 0),
                ImVec2(i + offset - display.y, display.y),
                line_col, 1.0f);
        }
        // Lines going top-right to bottom-left
        for (float i = -display.y; i < display.x + display.y; i += spacing) {
            dl->AddLine(
                ImVec2(i - offset, 0),
                ImVec2(i - offset + display.y, display.y),
                line_col, 1.0f);
        }
    }

    // --- Corner accent brackets (art-deco style) ---
    {
        ImU32 bracket_col = IM_COL32(255, 115, 13, 40);
        float sz = 60.0f;
        float thick = 2.0f;
        float margin = 30.0f;

        // Top-left
        dl->AddLine(ImVec2(margin, margin), ImVec2(margin + sz, margin), bracket_col, thick);
        dl->AddLine(ImVec2(margin, margin), ImVec2(margin, margin + sz), bracket_col, thick);

        // Top-right
        dl->AddLine(ImVec2(display.x - margin, margin), ImVec2(display.x - margin - sz, margin), bracket_col, thick);
        dl->AddLine(ImVec2(display.x - margin, margin), ImVec2(display.x - margin, margin + sz), bracket_col, thick);

        // Bottom-left
        dl->AddLine(ImVec2(margin, display.y - margin), ImVec2(margin + sz, display.y - margin), bracket_col, thick);
        dl->AddLine(ImVec2(margin, display.y - margin), ImVec2(margin, display.y - margin - sz), bracket_col, thick);

        // Bottom-right
        dl->AddLine(ImVec2(display.x - margin, display.y - margin), ImVec2(display.x - margin - sz, display.y - margin), bracket_col, thick);
        dl->AddLine(ImVec2(display.x - margin, display.y - margin), ImVec2(display.x - margin, display.y - margin - sz), bracket_col, thick);
    }

    // --- Floating hexagons (parallax depth) ---
    {
        auto draw_hexagon = [&](float cx, float cy, float r, ImU32 col) {
            ImVec2 pts[6];
            for (int i = 0; i < 6; i++) {
                float angle = (float)i * (3.14159265f / 3.0f) - 3.14159265f / 6.0f;
                pts[i] = ImVec2(cx + cosf(angle) * r, cy + sinf(angle) * r);
            }
            dl->AddPolyline(pts, 6, col, ImDrawFlags_Closed, 1.2f);
        };

        // Scattered hexagons at various depths
        struct HexDef { float base_x, base_y, radius, speed, phase; };
        HexDef hexes[] = {
            {0.08f, 0.15f, 35, 0.3f, 0.0f},
            {0.92f, 0.25f, 28, 0.4f, 1.5f},
            {0.15f, 0.75f, 22, 0.5f, 3.0f},
            {0.85f, 0.80f, 40, 0.25f, 2.0f},
            {0.50f, 0.90f, 18, 0.6f, 4.0f},
            {0.05f, 0.50f, 30, 0.35f, 5.0f},
            {0.95f, 0.55f, 25, 0.45f, 1.0f},
        };

        for (auto& h : hexes) {
            float drift_y = sinf(t * h.speed + h.phase) * 12.0f;
            float drift_x = cosf(t * h.speed * 0.7f + h.phase) * 6.0f;
            float cx = display.x * h.base_x + drift_x;
            float cy = display.y * h.base_y + drift_y;
            // Pulse opacity
            float alpha = 18.0f + sinf(t * 0.8f + h.phase) * 10.0f;
            ImU32 col = IM_COL32(255, 115, 13, (int)alpha);
            draw_hexagon(cx, cy, h.radius, col);
        }
    }

    // --- Horizontal scan line (moves top to bottom slowly) ---
    {
        float scan_y = fmodf(t * 25.0f, display.y + 40.0f) - 20.0f;
        ImU32 scan_col = IM_COL32(255, 115, 13, 8);
        dl->AddRectFilled(ImVec2(0, scan_y), ImVec2(display.x, scan_y + 1.5f), scan_col);
        // Subtle glow around scan line
        ImU32 glow_col = IM_COL32(255, 115, 13, 3);
        dl->AddRectFilled(ImVec2(0, scan_y - 8), ImVec2(display.x, scan_y + 10), glow_col);
    }

    // --- Center diamond accent (behind the panel) ---
    {
        float cx = display.x * 0.5f;
        float cy = display.y * 0.5f;
        float sz = 200.0f + sinf(t * 0.3f) * 20.0f;
        ImU32 diamond_col = IM_COL32(255, 115, 13, 8);

        ImVec2 pts[4] = {
            {cx, cy - sz},      // top
            {cx + sz, cy},      // right
            {cx, cy + sz},      // bottom
            {cx - sz, cy},      // left
        };
        dl->AddPolyline(pts, 4, diamond_col, ImDrawFlags_Closed, 1.0f);

        // Inner diamond
        float sz2 = sz * 0.6f;
        ImVec2 pts2[4] = {
            {cx, cy - sz2}, {cx + sz2, cy}, {cx, cy + sz2}, {cx - sz2, cy},
        };
        dl->AddPolyline(pts2, 4, IM_COL32(255, 115, 13, 5), ImDrawFlags_Closed, 1.0f);
    }
}

// ============================================================================
// Branded Header — "FINCEPT TERMINAL" with glow + subtitle
// ============================================================================
inline void BrandHeader(const char* subtitle = nullptr) {
    float content_w = ImGui::GetContentRegionAvail().x;
    float t = (float)ImGui::GetTime();

    // Draw glowing accent line above title
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float line_w = content_w * 0.4f;
    float line_x = cursor.x + (content_w - line_w) / 2.0f;
    float line_y = cursor.y;

    // Gradient line: transparent -> orange -> transparent
    ImU32 col_edge = IM_COL32(255, 115, 13, 0);
    ImU32 col_mid  = IM_COL32(255, 115, 13, 180);
    float mid_x = line_x + line_w / 2.0f;
    dl->AddRectFilledMultiColor(
        ImVec2(line_x, line_y), ImVec2(mid_x, line_y + 2),
        col_edge, col_mid, col_mid, col_edge);
    dl->AddRectFilledMultiColor(
        ImVec2(mid_x, line_y), ImVec2(line_x + line_w, line_y + 2),
        col_mid, col_edge, col_edge, col_mid);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 14);

    // Title with subtle brightness pulse
    float pulse = 0.92f + sinf(t * 1.5f) * 0.08f;
    ImVec4 title_col = {1.0f * pulse, 0.45f * pulse, 0.05f * pulse, 1.0f};

    const char* title = "FINCEPT TERMINAL";
    float title_w = ImGui::CalcTextSize(title).x;
    ImGui::SetCursorPosX((content_w - title_w) / 2.0f);
    ImGui::TextColored(title_col, "%s", title);

    ImGui::Spacing();

    // Subtitle
    if (subtitle) {
        float sub_w = ImGui::CalcTextSize(subtitle).x;
        ImGui::SetCursorPosX((content_w - sub_w) / 2.0f);
        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "%s", subtitle);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Bottom accent line (thinner)
    cursor = ImGui::GetCursorScreenPos();
    line_y = cursor.y;
    dl->AddRectFilledMultiColor(
        ImVec2(line_x, line_y), ImVec2(mid_x, line_y + 1),
        col_edge, col_mid, col_mid, col_edge);
    dl->AddRectFilledMultiColor(
        ImVec2(mid_x, line_y), ImVec2(line_x + line_w, line_y + 1),
        col_mid, col_edge, col_edge, col_mid);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
}

// ============================================================================
// Accent separator — gradient line instead of plain ImGui::Separator
// ============================================================================
inline void AccentSeparator() {
    ImGui::Spacing();
    float content_w = ImGui::GetContentRegionAvail().x;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 col_edge = IM_COL32(70, 70, 75, 80);
    ImU32 col_mid  = IM_COL32(255, 115, 13, 60);
    float mid_x = cursor.x + content_w / 2.0f;

    dl->AddRectFilledMultiColor(
        ImVec2(cursor.x, cursor.y), ImVec2(mid_x, cursor.y + 1),
        col_edge, col_mid, col_mid, col_edge);
    dl->AddRectFilledMultiColor(
        ImVec2(mid_x, cursor.y), ImVec2(cursor.x + content_w, cursor.y + 1),
        col_mid, col_edge, col_edge, col_mid);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12);
}

// ============================================================================
// Form field label with optional icon character
// ============================================================================
inline void FieldLabel(const char* label) {
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
}

// ============================================================================
// Styled input field — accent left-border on focus
// ============================================================================
inline bool StyledInput(const char* id, char* buf, int buf_size, ImGuiInputTextFlags flags = 0) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme::colors::BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

    ImGui::PushItemWidth(-1);
    bool result = ImGui::InputText(id, buf, buf_size, flags);
    ImGui::PopItemWidth();

    // Draw accent left border on the input
    ImVec2 item_min = ImGui::GetItemRectMin();
    ImVec2 item_max = ImGui::GetItemRectMax();
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();

    if (active || hovered) {
        ImU32 accent = active ? IM_COL32(255, 115, 13, 200) : IM_COL32(255, 115, 13, 80);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(item_min.x, item_min.y),
            ImVec2(item_min.x + 3, item_max.y),
            accent);
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    return result;
}

// ============================================================================
// Link-style button (underline on hover)
// ============================================================================
inline bool LinkButton(const char* label, ImVec4 color = theme::colors::INFO) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, color);

    bool clicked = ImGui::Button(label);

    // Underline on hover
    if (ImGui::IsItemHovered()) {
        ImVec2 p_min = ImGui::GetItemRectMin();
        ImVec2 p_max = ImGui::GetItemRectMax();
        ImU32 ul_col = ImGui::ColorConvertFloat4ToU32(color);
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p_min.x, p_max.y - 1),
            ImVec2(p_max.x, p_max.y - 1),
            ul_col, 1.0f);
    }

    ImGui::PopStyleColor(4);
    return clicked;
}

// ============================================================================
// Panel glow — draws a subtle glow around the auth panel window
// ============================================================================
inline void DrawPanelGlow() {
    ImVec2 wmin = ImGui::GetWindowPos();
    ImVec2 wmax = ImVec2(wmin.x + ImGui::GetWindowWidth(), wmin.y + ImGui::GetWindowHeight());
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Outer glow (very subtle)
    for (int i = 3; i >= 1; i--) {
        float expand = (float)i * 3.0f;
        ImU32 glow = IM_COL32(255, 115, 13, 4 * i);
        dl->AddRect(
            ImVec2(wmin.x - expand, wmin.y - expand),
            ImVec2(wmax.x + expand, wmax.y + expand),
            glow, 4.0f + expand, 0, 1.0f);
    }

    // Top edge accent line
    ImU32 edge = IM_COL32(255, 115, 13, 50);
    dl->AddRectFilled(wmin, ImVec2(wmax.x, wmin.y + 2), edge);
}

// ============================================================================
// Version tag — small version label at bottom of panel
// ============================================================================
inline void VersionTag() {
    float content_w = ImGui::GetContentRegionAvail().x;
    const char* ver = "v4.0.0";
    float ver_w = ImGui::CalcTextSize(ver).x;
    ImGui::SetCursorPosX((content_w - ver_w) / 2.0f);
    ImGui::TextColored(ImVec4(0.35f, 0.35f, 0.38f, 1.0f), "%s", ver);
}

} // namespace fincept::auth::ui_detail
