#include "surface_screen.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace fincept::surface {

// ============================================================================
// Color helpers
// ============================================================================
static ImVec4 lerp4(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t);
}

ImVec4 SurfaceScreen::surface_color(float t, bool diverging) const {
    t = std::max(0.0f, std::min(1.0f, t));
    if (diverging) {
        static const ImVec4 stops[] = {
            {0.72f, 0.11f, 0.11f, 1.0f},
            {0.90f, 0.30f, 0.15f, 1.0f},
            {1.00f, 0.62f, 0.00f, 1.0f},
            {1.00f, 0.92f, 0.23f, 1.0f},
            {0.49f, 0.70f, 0.26f, 1.0f},
            {0.26f, 0.63f, 0.28f, 1.0f},
            {0.18f, 0.49f, 0.20f, 1.0f},
        };
        float idx = t * 6.0f;
        int lo = std::max(0, std::min(5, (int)idx));
        return lerp4(stops[lo], stops[lo+1], idx - (float)lo);
    }
    static const ImVec4 stops[] = {
        {0.10f, 0.14f, 0.49f, 1.0f},
        {0.08f, 0.40f, 0.75f, 1.0f},
        {0.02f, 0.65f, 0.82f, 1.0f},
        {0.15f, 0.78f, 0.55f, 1.0f},
        {1.00f, 0.76f, 0.08f, 1.0f},
        {1.00f, 0.44f, 0.16f, 1.0f},
        {0.83f, 0.18f, 0.18f, 1.0f},
    };
    float idx = t * 6.0f;
    int lo = std::max(0, std::min(5, (int)idx));
    return lerp4(stops[lo], stops[lo+1], idx - (float)lo);
}

// ============================================================================
// 3D projection: rotate then orthographic project
// ============================================================================
ImVec2 SurfaceScreen::project(Vec3 p, ImVec2 center, float scale) const {
    float cy = std::cos(cam_yaw_), sy = std::sin(cam_yaw_);
    float cp = std::cos(cam_pitch_), sp = std::sin(cam_pitch_);

    float x1 = p.x * cy - p.z * sy;
    float z1 = p.x * sy + p.z * cy;
    float y1 = p.y * cp - z1 * sp;
    float z2 = p.y * sp + z1 * cp;

    float persp = 1.0f / (1.0f + z2 * 0.15f);
    float s = scale * cam_zoom_ * persp;
    return ImVec2(center.x + x1 * s, center.y - y1 * s);
}

// ============================================================================
// 3D Surface renderer — Bloomberg-style professional visualization
// ============================================================================
void SurfaceScreen::render_3d_surface(
    const std::vector<std::vector<float>>& grid,
    const char* x_label, const char* y_label, const char* z_label,
    float min_val, float max_val, bool diverging,
    const std::vector<std::string>* col_labels,
    const std::vector<std::string>* row_labels)
{
    if (grid.empty() || grid[0].empty()) return;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    // Reserve 20px bottom for status strip and 32px top for overlay
    avail.y -= 20.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    origin.y += 32.0f; // push below the top overlay

    ImVec2 center(origin.x + avail.x * 0.46f, origin.y + avail.y * 0.52f);
    float sc = std::min(avail.x, avail.y) * 0.34f;

    // Subtle gradient background for the 3D canvas
    {
        ImDrawList* bg_dl = ImGui::GetWindowDrawList();
        ImVec2 bg_min = ImGui::GetWindowPos();
        bg_min.y += 32.0f;
        ImVec2 bg_max = ImVec2(bg_min.x + ImGui::GetWindowWidth(), bg_min.y + avail.y);
        bg_dl->AddRectFilledMultiColor(bg_min, bg_max,
            ImGui::GetColorU32(ImVec4(0.07f, 0.08f, 0.10f, 1.0f)),
            ImGui::GetColorU32(ImVec4(0.07f, 0.08f, 0.10f, 1.0f)),
            ImGui::GetColorU32(ImVec4(0.05f, 0.06f, 0.07f, 1.0f)),
            ImGui::GetColorU32(ImVec4(0.05f, 0.06f, 0.07f, 1.0f)));
    }

    ImGui::InvisibleButton("##3d_interact", avail);
    bool hovered = ImGui::IsItemHovered();

    if (hovered && ImGui::IsMouseClicked(0)) {
        dragging_ = true;
        drag_start_x_ = ImGui::GetMousePos().x;
        drag_start_y_ = ImGui::GetMousePos().y;
        drag_start_yaw_ = cam_yaw_;
        drag_start_pitch_ = cam_pitch_;
    }
    if (dragging_) {
        if (ImGui::IsMouseDown(0)) {
            float dx = ImGui::GetMousePos().x - drag_start_x_;
            float dy = ImGui::GetMousePos().y - drag_start_y_;
            cam_yaw_ = drag_start_yaw_ + dx * 0.005f;
            cam_pitch_ = std::max(0.05f, std::min(1.5f, drag_start_pitch_ + dy * 0.005f));
        } else {
            dragging_ = false;
        }
    }

    if (hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) cam_zoom_ = std::max(0.3f, std::min(3.5f, cam_zoom_ + wheel * 0.1f));
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    int rows = (int)grid.size();
    int cols = (int)grid[0].size();
    float range = (max_val > min_val) ? (max_val - min_val) : 1.0f;

    auto v3 = [](float vx, float vy, float vz) -> Vec3 { return {vx, vy, vz}; };

    // ---- Wireframe box ----
    ImU32 axis_col = ImGui::GetColorU32(ImVec4(0.45f, 0.55f, 0.65f, 0.9f));
    ImU32 axis_dim = ImGui::GetColorU32(ImVec4(0.30f, 0.38f, 0.45f, 0.5f));

    dl->AddLine(project(v3(-1,-1,-1), center, sc), project(v3( 1,-1,-1), center, sc), axis_col, 1.5f);
    dl->AddLine(project(v3(-1,-1,-1), center, sc), project(v3(-1,-1, 1), center, sc), axis_col, 1.5f);
    dl->AddLine(project(v3( 1,-1,-1), center, sc), project(v3( 1,-1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1,-1, 1), center, sc), project(v3( 1,-1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1,-1,-1), center, sc), project(v3(-1, 1,-1), center, sc), axis_col, 1.5f);
    dl->AddLine(project(v3( 1,-1,-1), center, sc), project(v3( 1, 1,-1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1,-1, 1), center, sc), project(v3(-1, 1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3( 1,-1, 1), center, sc), project(v3( 1, 1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1, 1,-1), center, sc), project(v3( 1, 1,-1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1, 1,-1), center, sc), project(v3(-1, 1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3( 1, 1,-1), center, sc), project(v3( 1, 1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1, 1, 1), center, sc), project(v3( 1, 1, 1), center, sc), axis_dim, 1.0f);

    // ---- Grid lines on base plane ----
    ImU32 grid_col = ImGui::GetColorU32(ImVec4(0.25f, 0.32f, 0.38f, 0.35f));
    constexpr int n_grid = 8;
    for (int gi = 0; gi <= n_grid; gi++) {
        float gt = -1.0f + 2.0f * (float)gi / (float)n_grid;
        dl->AddLine(project(v3(gt,-1,-1), center, sc), project(v3(gt,-1,1), center, sc), grid_col);
        dl->AddLine(project(v3(-1,-1,gt), center, sc), project(v3(1,-1,gt), center, sc), grid_col);
    }

    // ---- Grid lines on back walls ----
    ImU32 wall_col = ImGui::GetColorU32(ImVec4(0.22f, 0.28f, 0.34f, 0.25f));
    for (int gi = 1; gi < n_grid; gi++) {
        float gt = -1.0f + 2.0f * (float)gi / (float)n_grid;
        dl->AddLine(project(v3(-1,gt,-1), center, sc), project(v3(1,gt,-1), center, sc), wall_col);
        dl->AddLine(project(v3(gt,-1,-1), center, sc), project(v3(gt,1,-1), center, sc), wall_col);
        dl->AddLine(project(v3(-1,gt,-1), center, sc), project(v3(-1,gt,1), center, sc), wall_col);
        dl->AddLine(project(v3(-1,-1,gt), center, sc), project(v3(-1,1,gt), center, sc), wall_col);
    }

    // ---- Axis labels — styled with background pill ----
    ImU32 label_col = ImGui::GetColorU32(ImVec4(0.40f, 0.85f, 0.95f, 1.0f));
    ImU32 label_bg  = ImGui::GetColorU32(ImVec4(0.10f, 0.22f, 0.28f, 0.82f));
    auto draw_axis_label = [&](Vec3 pos, const char* text) {
        ImVec2 tp = project(pos, center, sc);
        float tw = ImGui::CalcTextSize(text).x;
        float th = ImGui::GetTextLineHeight();
        dl->AddRectFilled(ImVec2(tp.x-3, tp.y-2), ImVec2(tp.x+tw+3, tp.y+th+2), label_bg, 2.0f);
        dl->AddText(tp, label_col, text);
    };
    draw_axis_label(v3( 0.0f, -1.45f, -1.0f), x_label);
    draw_axis_label(v3(-1.50f,  0.0f, -1.0f), y_label);
    draw_axis_label(v3(-1.0f, -1.45f,  0.0f), z_label);

    // ---- Tick labels ----
    ImU32 tick_col = ImGui::GetColorU32(ImVec4(0.50f, 0.58f, 0.65f, 0.8f));
    char tick_buf[32];
    constexpr int n_ticks = 5;
    for (int ti = 0; ti <= n_ticks; ti++) {
        float frac = (float)ti / (float)n_ticks;
        float norm = -1.0f + 2.0f * frac;

        if (col_labels && ti < (int)col_labels->size()) {
            int idx = (int)(frac * (col_labels->size() - 1));
            dl->AddText(project(v3(norm, -1.0f, -1.15f), center, sc), tick_col, (*col_labels)[idx].c_str());
        }
        float yval = min_val + frac * range;
        std::snprintf(tick_buf, 32, "%.1f", yval);
        dl->AddText(project(v3(-1.2f, norm, -1.0f), center, sc), tick_col, tick_buf);

        if (row_labels && ti < (int)row_labels->size()) {
            int idx = (int)(frac * (row_labels->size() - 1));
            dl->AddText(project(v3(-1.15f, -1.0f, norm), center, sc), tick_col, (*row_labels)[idx].c_str());
        }
    }

    // ---- Build surface quads ----
    int step_r = std::max(1, rows / 50);
    int step_c = std::max(1, cols / 50);

    struct SurfQuad {
        ImVec2 pts[4];
        ImVec4 color;
        float depth;
        float value;
        int row, col;
    };
    std::vector<SurfQuad> quads;
    quads.reserve((rows / step_r) * (cols / step_c));

    float cam_cy = std::cos(cam_yaw_), cam_sy = std::sin(cam_yaw_);
    float cam_cp = std::cos(cam_pitch_), cam_sp = std::sin(cam_pitch_);
    auto depth_of = [&](Vec3 pt) -> float {
        float zr = pt.x * cam_sy + pt.z * cam_cy;
        return pt.y * cam_sp + zr * cam_cp;
    };

    Vec3 light_dir = {0.3f, 0.8f, -0.5f};
    float light_len = std::sqrt(light_dir.x*light_dir.x + light_dir.y*light_dir.y + light_dir.z*light_dir.z);
    light_dir.x /= light_len; light_dir.y /= light_len; light_dir.z /= light_len;

    for (int i = 0; i < rows - step_r; i += step_r) {
        for (int j = 0; j < cols - step_c; j += step_c) {
            int i2 = std::min(i + step_r, rows - 1);
            int j2 = std::min(j + step_c, cols - 1);

            float x0 = -1.0f + 2.0f * (float)j / (float)(cols - 1);
            float x1 = -1.0f + 2.0f * (float)j2 / (float)(cols - 1);
            float zz0 = -1.0f + 2.0f * (float)i / (float)(rows - 1);
            float zz1 = -1.0f + 2.0f * (float)i2 / (float)(rows - 1);

            float v00 = grid[i][j], v01 = grid[i][j2], v10 = grid[i2][j], v11 = grid[i2][j2];
            float y00 = -1.0f + 2.0f * (v00 - min_val) / range;
            float y01 = -1.0f + 2.0f * (v01 - min_val) / range;
            float y10 = -1.0f + 2.0f * (v10 - min_val) / range;
            float y11 = -1.0f + 2.0f * (v11 - min_val) / range;

            float avg_val = (v00 + v01 + v10 + v11) * 0.25f;
            float ct = (avg_val - min_val) / range;
            ImVec4 col = surface_color(ct, diverging);

            float dx1 = x1 - x0, dy1 = y01 - y00, dz1 = 0.0f;
            float dx2 = 0.0f, dy2 = y10 - y00, dz2 = zz1 - zz0;
            Vec3 normal = {
                dy1 * dz2 - dz1 * dy2,
                dz1 * dx2 - dx1 * dz2,
                dx1 * dy2 - dy1 * dx2
            };
            float nlen = std::sqrt(normal.x*normal.x + normal.y*normal.y + normal.z*normal.z);
            if (nlen > 0.0001f) { normal.x /= nlen; normal.y /= nlen; normal.z /= nlen; }

            float ndot = normal.x * light_dir.x + normal.y * light_dir.y + normal.z * light_dir.z;
            float shade = 0.55f + 0.45f * std::max(0.0f, ndot);

            col.x *= shade; col.y *= shade; col.z *= shade;
            col.w = 0.92f;

            Vec3 p00 = {x0, y00, zz0}, p01 = {x1, y01, zz0};
            Vec3 p10 = {x0, y10, zz1}, p11 = {x1, y11, zz1};

            float d = (depth_of(p00) + depth_of(p01) + depth_of(p10) + depth_of(p11)) * 0.25f;

            SurfQuad q;
            q.pts[0] = project(p00, center, sc);
            q.pts[1] = project(p01, center, sc);
            q.pts[2] = project(p11, center, sc);
            q.pts[3] = project(p10, center, sc);
            q.color = col;
            q.depth = d;
            q.value = avg_val;
            q.row = (i + i2) / 2;
            q.col = (j + j2) / 2;
            quads.push_back(q);
        }
    }

    std::sort(quads.begin(), quads.end(), [](const SurfQuad& a, const SurfQuad& b) {
        return a.depth < b.depth;
    });

    auto point_in_tri = [](ImVec2 p, ImVec2 a, ImVec2 b, ImVec2 c) -> bool {
        float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
        float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
        float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
        bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(has_neg && has_pos);
    };
    auto point_in_quad = [&](ImVec2 p, const ImVec2 qp[4]) -> bool {
        return point_in_tri(p, qp[0], qp[1], qp[2]) || point_in_tri(p, qp[0], qp[2], qp[3]);
    };

    int hover_idx = -1;
    ImVec2 mouse = ImGui::GetMousePos();
    for (int qi = 0; qi < (int)quads.size(); qi++) {
        auto& q = quads[qi];
        ImU32 fill = ImGui::GetColorU32(q.color);
        ImVec4 edge_col(q.color.x * 0.5f, q.color.y * 0.5f, q.color.z * 0.5f, 0.6f);
        ImU32 edge = ImGui::GetColorU32(edge_col);

        dl->AddQuadFilled(q.pts[0], q.pts[1], q.pts[2], q.pts[3], fill);
        dl->AddQuad(q.pts[0], q.pts[1], q.pts[2], q.pts[3], edge, 0.8f);

        if (hovered && !dragging_ && point_in_quad(mouse, q.pts))
            hover_idx = qi;
    }

    // ---- Hover highlight + tooltip ----
    if (hover_idx >= 0) {
        auto& hq = quads[hover_idx];
        dl->AddQuad(hq.pts[0], hq.pts[1], hq.pts[2], hq.pts[3],
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.95f)), 2.5f);
        ImVec4 glow = hq.color; glow.x = std::min(1.0f, glow.x + 0.3f);
        glow.y = std::min(1.0f, glow.y + 0.3f); glow.z = std::min(1.0f, glow.z + 0.3f); glow.w = 1.0f;
        dl->AddQuadFilled(hq.pts[0], hq.pts[1], hq.pts[2], hq.pts[3], ImGui::GetColorU32(glow));

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.10f, 0.14f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.70f, 0.90f, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::BeginTooltip();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 0.95f, 1.0f));
        ImGui::Text("%s", y_label);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1,1,1,1), "%.4f", hq.value);

        ImGui::Separator();

        if (col_labels && hq.col >= 0 && hq.col < (int)col_labels->size())
            ImGui::TextColored(ImVec4(0.6f,0.7f,0.8f,1), "%s: %s", x_label, (*col_labels)[hq.col].c_str());
        else
            ImGui::TextColored(ImVec4(0.6f,0.7f,0.8f,1), "%s: %d", x_label, hq.col);

        if (row_labels && hq.row >= 0 && hq.row < (int)row_labels->size())
            ImGui::TextColored(ImVec4(0.6f,0.7f,0.8f,1), "%s: %s", z_label, (*row_labels)[hq.row].c_str());
        else
            ImGui::TextColored(ImVec4(0.6f,0.7f,0.8f,1), "%s: %d", z_label, hq.row);

        ImGui::EndTooltip();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    // ---- Color bar — wider, with gradient border and clean ticks ----
    float bar_w   = 18.0f;
    float bar_x   = origin.x + avail.x - bar_w - 52.0f;
    float bar_top = origin.y + 36.0f;
    float bar_h   = avail.y - 72.0f;

    // Draw gradient bar pixel by pixel
    for (int ci = 0; ci < (int)bar_h; ci++) {
        float ct = 1.0f - (float)ci / bar_h;
        ImVec4 cc = surface_color(ct, diverging);
        dl->AddRectFilled(
            ImVec2(bar_x, bar_top + (float)ci),
            ImVec2(bar_x + bar_w, bar_top + (float)ci + 1.5f),
            ImGui::GetColorU32(cc));
    }
    // Border
    dl->AddRect(ImVec2(bar_x - 1, bar_top - 1),
                ImVec2(bar_x + bar_w + 1, bar_top + bar_h + 1),
                ImGui::GetColorU32(ImVec4(0.35f, 0.45f, 0.55f, 0.7f)), 0, 0, 1.0f);

    // Tick marks + labels
    ImU32 tick_col2 = ImGui::GetColorU32(ImVec4(0.55f, 0.65f, 0.75f, 0.9f));
    constexpr int bar_ticks = 6;
    for (int bi = 0; bi <= bar_ticks; bi++) {
        float frac = (float)bi / (float)bar_ticks;
        float val  = max_val - frac * range;
        float py   = bar_top + frac * bar_h;
        char bl[16]; std::snprintf(bl, 16, "%.2f", val);
        dl->AddLine(ImVec2(bar_x + bar_w, py),
                    ImVec2(bar_x + bar_w + 5, py),
                    tick_col2, 1.2f);
        dl->AddText(ImVec2(bar_x + bar_w + 7, py - 6), tick_col2, bl);
    }

    // Y-axis label above bar
    ImU32 bar_label_col = ImGui::GetColorU32(ImVec4(0.40f, 0.85f, 0.95f, 1.0f));
    float yl_tw = ImGui::CalcTextSize(y_label).x;
    dl->AddText(ImVec2(bar_x + (bar_w - yl_tw) * 0.5f, bar_top - 18.0f), bar_label_col, y_label);

    // Min/Max caps
    char cap_max[16], cap_min[16];
    std::snprintf(cap_max, 16, "MAX"); std::snprintf(cap_min, 16, "MIN");
    dl->AddText(ImVec2(bar_x + bar_w + 7, bar_top - 6),
                ImGui::GetColorU32(ImVec4(0.40f, 0.85f, 0.95f, 0.7f)), cap_max);
    dl->AddText(ImVec2(bar_x + bar_w + 7, bar_top + bar_h - 6),
                ImGui::GetColorU32(ImVec4(0.40f, 0.85f, 0.95f, 0.7f)), cap_min);
}

} // namespace fincept::surface
