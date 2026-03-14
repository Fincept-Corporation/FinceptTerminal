#include "surface_screen.h"
#include "demo_data.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

static const char* VOL_SYMBOLS[] = {"SPY", "QQQ", "IWM", "AAPL", "TSLA", "NVDA", "AMZN"};
static const float VOL_SPOTS[]   = {450, 380, 200, 175, 250, 120, 180};
static const int N_SYMBOLS = 7;
static const char* MAT_LABELS[] = {"1M","3M","6M","1Y","2Y","3Y","5Y","7Y","10Y","20Y","30Y"};

// ============================================================================
// Color helpers
// ============================================================================
static ImVec4 lerp4(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t);
}

ImVec4 SurfaceScreen::surface_color(float t, bool diverging) const {
    t = std::max(0.0f, std::min(1.0f, t));
    if (diverging) {
        // Red(-1) → Yellow(0) → Green(+1), mapped from 0..1
        static const ImVec4 stops[] = {
            {0.72f, 0.11f, 0.11f, 1.0f},  // deep red
            {0.90f, 0.30f, 0.15f, 1.0f},  // red-orange
            {1.00f, 0.62f, 0.00f, 1.0f},  // orange
            {1.00f, 0.92f, 0.23f, 1.0f},  // yellow (midpoint)
            {0.49f, 0.70f, 0.26f, 1.0f},  // light green
            {0.26f, 0.63f, 0.28f, 1.0f},  // green
            {0.18f, 0.49f, 0.20f, 1.0f},  // deep green
        };
        float idx = t * 6.0f;
        int lo = std::max(0, std::min(5, (int)idx));
        return lerp4(stops[lo], stops[lo+1], idx - (float)lo);
    }
    // Blue → Cyan → Green → Yellow → Orange → Red
    static const ImVec4 stops[] = {
        {0.10f, 0.14f, 0.49f, 1.0f},  // deep blue
        {0.08f, 0.40f, 0.75f, 1.0f},  // blue
        {0.02f, 0.65f, 0.82f, 1.0f},  // cyan
        {0.15f, 0.78f, 0.55f, 1.0f},  // teal-green
        {1.00f, 0.76f, 0.08f, 1.0f},  // yellow
        {1.00f, 0.44f, 0.16f, 1.0f},  // orange
        {0.83f, 0.18f, 0.18f, 1.0f},  // red
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

    // Rotate around Y (yaw) then X (pitch)
    float x1 = p.x * cy - p.z * sy;
    float z1 = p.x * sy + p.z * cy;
    float y1 = p.y * cp - z1 * sp;
    float z2 = p.y * sp + z1 * cp;

    // Simple perspective factor
    float persp = 1.0f / (1.0f + z2 * 0.15f);
    float s = scale * cam_zoom_ * persp;
    return ImVec2(center.x + x1 * s, center.y - y1 * s);
}

// ============================================================================
void SurfaceScreen::load_demo_data() {
    srand((unsigned)time(nullptr));
    vol_data_ = generate_vol_surface(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    corr_data_ = generate_correlation(corr_assets_);
    yield_data_ = generate_yield_curve();
    pca_data_ = generate_pca(corr_assets_);
    data_loaded_ = true;
}

// ============================================================================
void SurfaceScreen::render() {
    if (!data_loaded_) load_demo_data();

    ui::ScreenFrame frame("##surface_analytics", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    // Yoga vertical stack: control_bar(36) + content(flex)
    auto vstack = ui::vstack_layout(frame.width(), frame.height(), {36, -1});

    render_control_bar();

    float content_w = frame.width();
    float content_h = vstack.heights[1];

    // Responsive: hide metrics sidebar on compact screens
    if (frame.is_compact()) {
        ImGui::BeginChild("##sa_chart", ImVec2(content_w, content_h), ImGuiChildFlags_Borders);
        render_chart_area();
        ImGui::EndChild();
    } else {
        // Yoga two-panel layout for metrics sidebar (left) + chart area (right)
        // side_visible=true, side gets 15-20% with min 150 max 200
        auto panels = ui::two_panel_layout(content_w, content_h, true,
            frame.is_medium() ? 20 : 15, 150, 200);

        // Metrics is the "side" panel but rendered first (left)
        ImGui::BeginChild("##sa_metrics", ImVec2(panels.side_w, panels.side_h), ImGuiChildFlags_Borders);
        render_metrics_panel();
        ImGui::EndChild();

        ImGui::SameLine();

        // Chart is the "main" panel (right, flex-grow)
        ImGui::BeginChild("##sa_chart", ImVec2(panels.main_w, panels.main_h), ImGuiChildFlags_Borders);
        render_chart_area();
        ImGui::EndChild();
    }

    frame.end();
}

// ============================================================================
void SurfaceScreen::render_control_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##sa_ctrl", ImVec2(0, 36), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(8, 6));

    struct { ChartType t; const char* l; } types[] = {
        {ChartType::Volatility, "VOL SURFACE"}, {ChartType::Correlation, "CORRELATION"},
        {ChartType::YieldCurve, "YIELD CURVE"}, {ChartType::PCA, "PCA"},
    };
    for (int i = 0; i < 4; i++) {
        bool act = (active_chart_ == types[i].t);
        ImGui::PushStyleColor(ImGuiCol_Button, act ? ACCENT_DIM : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, act ? ImVec4(1,1,1,1) : TEXT_SECONDARY);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, act ? ACCENT : BG_HOVER);
        if (ImGui::SmallButton(types[i].l)) active_chart_ = types[i].t;
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 4);
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    // View toggle: Figure / Table
    {
        bool is_fig = !show_table_;
        ImGui::PushStyleColor(ImGuiCol_Button, is_fig ? ACCENT_DIM : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, is_fig ? ImVec4(1,1,1,1) : TEXT_DIM);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
        if (ImGui::SmallButton("FIGURE")) show_table_ = false;
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 2);
        ImGui::PushStyleColor(ImGuiCol_Button, show_table_ ? ACCENT_DIM : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, show_table_ ? ImVec4(1,1,1,1) : TEXT_DIM);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
        if (ImGui::SmallButton("TABLE")) show_table_ = true;
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    // Symbol selector for vol surface
    if (active_chart_ == ChartType::Volatility) {
        ImGui::TextColored(TEXT_DIM, "Symbol:");
        ImGui::SameLine(0, 4);
        ImGui::PushItemWidth(80);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        if (ImGui::BeginCombo("##sym", VOL_SYMBOLS[selected_symbol_], ImGuiComboFlags_NoArrowButton)) {
            for (int i = 0; i < N_SYMBOLS; i++) {
                if (ImGui::Selectable(VOL_SYMBOLS[i], i == selected_symbol_)) {
                    selected_symbol_ = i;
                    vol_data_ = generate_vol_surface(VOL_SYMBOLS[i], VOL_SPOTS[i]);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 12);
    }

    // Refresh
    ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    if (ImGui::SmallButton("REFRESH")) load_demo_data();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "DEMO DATA");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
void SurfaceScreen::render_metrics_panel() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::SetCursorPos(ImVec2(8, 8));

    const char* titles[] = {"VOLATILITY", "CORRELATION", "YIELD CURVE", "PCA ANALYSIS"};
    ImGui::TextColored(ACCENT, "%s", titles[(int)active_chart_]);
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    float metric_label_w = ImGui::GetContentRegionAvail().x * 0.45f;
    auto row = [metric_label_w](const char* lbl, const char* val, ImVec4 col) {
        ImGui::TextColored(TEXT_DIM, "%s", lbl);
        ImGui::SameLine(metric_label_w);
        ImGui::TextColored(col, "%s", val);
    };
    char b[64];

    switch (active_chart_) {
    case ChartType::Volatility: {
        if (vol_data_.z.empty()) break;
        int me = (int)vol_data_.z.size()/2, ms = (int)vol_data_.strikes.size()/2;
        float atm = vol_data_.z[me][ms];
        int op = (int)(vol_data_.strikes.size()*0.2f), oc = (int)(vol_data_.strikes.size()*0.8f);
        float skew = vol_data_.z[me][op] - vol_data_.z[me][oc];
        std::snprintf(b, 64, "%.1f%%", atm); row("ATM VOL", b, TEXT_PRIMARY);
        std::snprintf(b, 64, "%.1f%%", skew); row("SKEW", b, skew > 0 ? MARKET_GREEN : MARKET_RED);
        std::snprintf(b, 64, "$%.2f", vol_data_.spot_price); row("SPOT", b, TEXT_PRIMARY);
        std::snprintf(b, 64, "%d", (int)(vol_data_.strikes.size()*vol_data_.expirations.size()));
        row("POINTS", b, TEXT_SECONDARY);
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "UNDERLYING");
        ImGui::TextColored(TEXT_PRIMARY, "  %s", vol_data_.underlying.c_str());
        std::snprintf(b, 64, "  %.0f - %.0f", vol_data_.strikes.front(), vol_data_.strikes.back());
        ImGui::TextColored(TEXT_DIM, "STRIKES"); ImGui::TextColored(TEXT_PRIMARY, "%s", b);
        std::snprintf(b, 64, "  %dD - %dD", vol_data_.expirations.front(), vol_data_.expirations.back());
        ImGui::TextColored(TEXT_DIM, "EXPIRIES"); ImGui::TextColored(TEXT_PRIMARY, "%s", b);
        break;
    }
    case ChartType::Correlation: {
        if (corr_data_.z.empty()) break;
        auto& last = corr_data_.z.back();
        int n = (int)corr_data_.assets.size();
        float sum=0, mx=-2, mn=2; int cnt=0;
        for (int i=0;i<n;i++) for (int j=0;j<n;j++) if (i!=j) {
            float c=last[i*n+j]; sum+=c; cnt++; mx=std::max(mx,c); mn=std::min(mn,c);
        }
        float avg = cnt>0 ? sum/cnt : 0;
        std::snprintf(b,64,"%.3f",avg); row("AVG CORR",b, avg>0.5f ? MARKET_RED : MARKET_GREEN);
        std::snprintf(b,64,"%.3f",mx); row("MAX CORR",b, mx>0.8f ? MARKET_RED : TEXT_PRIMARY);
        std::snprintf(b,64,"%.3f",mn); row("MIN CORR",b, mn<0 ? INFO : TEXT_PRIMARY);
        std::snprintf(b,64,"%d",n); row("ASSETS",b, TEXT_SECONDARY);
        std::snprintf(b,64,"%dD",corr_data_.window); row("WINDOW",b, TEXT_SECONDARY);
        break;
    }
    case ChartType::YieldCurve: {
        if (yield_data_.z.empty()) break;
        auto& last = yield_data_.z.back();
        float y2=last.size()>4?last[4]:0, y10=last.size()>8?last[8]:0, sp=y10-y2;
        std::snprintf(b,64,"%.2f%%",y2); row("2Y YIELD",b,TEXT_PRIMARY);
        std::snprintf(b,64,"%.2f%%",y10); row("10Y YIELD",b,TEXT_PRIMARY);
        std::snprintf(b,64,"%.2f%%",sp); row("2-10 SPREAD",b, sp<0?MARKET_RED:MARKET_GREEN);
        row("CURVE", sp<0?"INVERTED":"NORMAL", sp<0?MARKET_RED:MARKET_GREEN);
        std::snprintf(b,64,"%d",(int)yield_data_.maturities.size()); row("TENORS",b,TEXT_SECONDARY);
        break;
    }
    case ChartType::PCA: {
        if (pca_data_.variance_explained.empty()) break;
        for (int i=0;i<std::min(3,(int)pca_data_.variance_explained.size());i++) {
            char lbl[16]; std::snprintf(lbl,16,"PC%d VAR",i+1);
            std::snprintf(b,64,"%.1f%%",pca_data_.variance_explained[i]*100);
            row(lbl, b, i==0?MARKET_GREEN:TEXT_PRIMARY);
        }
        float tot=0;
        for (int i=0;i<std::min(3,(int)pca_data_.variance_explained.size());i++) tot+=pca_data_.variance_explained[i];
        std::snprintf(b,64,"%.1f%%",tot*100); row("TOP 3",b, tot>0.85f?MARKET_GREEN:WARNING);
        std::snprintf(b,64,"%d",(int)pca_data_.assets.size()); row("ASSETS",b,TEXT_SECONDARY);
        break;
    }
    }

    // Drag hint for 3D mode
    if (!show_table_) {
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "CONTROLS");
        ImGui::TextColored(TEXT_SECONDARY, "  Drag: Rotate");
        ImGui::TextColored(TEXT_SECONDARY, "  Scroll: Zoom");
    }

    ImGui::PopStyleColor();
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
    ImVec2 origin = ImGui::GetCursorScreenPos();
    // Offset center slightly left to make room for color bar
    ImVec2 center(origin.x + avail.x * 0.46f, origin.y + avail.y * 0.52f);
    float sc = std::min(avail.x, avail.y) * 0.34f;

    // Invisible button for mouse interaction
    ImGui::InvisibleButton("##3d_interact", avail);
    bool hovered = ImGui::IsItemHovered();

    // Mouse drag to rotate
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

    // Scroll to zoom
    if (hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) cam_zoom_ = std::max(0.3f, std::min(3.5f, cam_zoom_ + wheel * 0.1f));
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    int rows = (int)grid.size();
    int cols = (int)grid[0].size();
    float range = (max_val > min_val) ? (max_val - min_val) : 1.0f;

    // Helper to make Vec3
    auto v3 = [](float vx, float vy, float vz) -> Vec3 { return {vx, vy, vz}; };

    // ---- Draw wireframe box (all 12 edges) ----
    ImU32 axis_col = ImGui::GetColorU32(ImVec4(0.45f, 0.55f, 0.65f, 0.9f));
    ImU32 axis_dim = ImGui::GetColorU32(ImVec4(0.30f, 0.38f, 0.45f, 0.5f));

    // Bottom face (y=-1)
    dl->AddLine(project(v3(-1,-1,-1), center, sc), project(v3( 1,-1,-1), center, sc), axis_col, 1.5f);
    dl->AddLine(project(v3(-1,-1,-1), center, sc), project(v3(-1,-1, 1), center, sc), axis_col, 1.5f);
    dl->AddLine(project(v3( 1,-1,-1), center, sc), project(v3( 1,-1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1,-1, 1), center, sc), project(v3( 1,-1, 1), center, sc), axis_dim, 1.0f);
    // Vertical edges
    dl->AddLine(project(v3(-1,-1,-1), center, sc), project(v3(-1, 1,-1), center, sc), axis_col, 1.5f);
    dl->AddLine(project(v3( 1,-1,-1), center, sc), project(v3( 1, 1,-1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1,-1, 1), center, sc), project(v3(-1, 1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3( 1,-1, 1), center, sc), project(v3( 1, 1, 1), center, sc), axis_dim, 1.0f);
    // Top face (y=1)
    dl->AddLine(project(v3(-1, 1,-1), center, sc), project(v3( 1, 1,-1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1, 1,-1), center, sc), project(v3(-1, 1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3( 1, 1,-1), center, sc), project(v3( 1, 1, 1), center, sc), axis_dim, 1.0f);
    dl->AddLine(project(v3(-1, 1, 1), center, sc), project(v3( 1, 1, 1), center, sc), axis_dim, 1.0f);

    // ---- Grid lines on base plane (y=-1) ----
    ImU32 grid_col = ImGui::GetColorU32(ImVec4(0.25f, 0.32f, 0.38f, 0.35f));
    int n_grid = 8;
    for (int gi = 0; gi <= n_grid; gi++) {
        float gt = -1.0f + 2.0f * (float)gi / (float)n_grid;
        dl->AddLine(project(v3(gt,-1,-1), center, sc), project(v3(gt,-1,1), center, sc), grid_col);
        dl->AddLine(project(v3(-1,-1,gt), center, sc), project(v3(1,-1,gt), center, sc), grid_col);
    }

    // ---- Grid lines on back walls for depth ----
    ImU32 wall_col = ImGui::GetColorU32(ImVec4(0.22f, 0.28f, 0.34f, 0.25f));
    for (int gi = 1; gi < n_grid; gi++) {
        float gt = -1.0f + 2.0f * (float)gi / (float)n_grid;
        // Back wall (z=-1): horizontal lines
        dl->AddLine(project(v3(-1,gt,-1), center, sc), project(v3(1,gt,-1), center, sc), wall_col);
        // Back wall (z=-1): vertical lines
        dl->AddLine(project(v3(gt,-1,-1), center, sc), project(v3(gt,1,-1), center, sc), wall_col);
        // Left wall (x=-1): horizontal lines
        dl->AddLine(project(v3(-1,gt,-1), center, sc), project(v3(-1,gt,1), center, sc), wall_col);
        // Left wall (x=-1): vertical lines
        dl->AddLine(project(v3(-1,-1,gt), center, sc), project(v3(-1,1,gt), center, sc), wall_col);
    }

    // ---- Axis labels with better positioning ----
    ImU32 label_col = ImGui::GetColorU32(ImVec4(0.40f, 0.85f, 0.95f, 1.0f));
    ImVec2 lx = project(v3(0.0f, -1.35f, -1.0f), center, sc);
    ImVec2 ly = project(v3(-1.35f, 0.0f, -1.0f), center, sc);
    ImVec2 lz = project(v3(-1.0f, -1.35f, 0.0f), center, sc);
    dl->AddText(lx, label_col, x_label);
    dl->AddText(ly, label_col, y_label);
    dl->AddText(lz, label_col, z_label);

    // ---- Tick labels on axes ----
    ImU32 tick_col = ImGui::GetColorU32(ImVec4(0.50f, 0.58f, 0.65f, 0.8f));
    char tick_buf[32];
    int n_ticks = 5;
    for (int ti = 0; ti <= n_ticks; ti++) {
        float frac = (float)ti / (float)n_ticks;
        float norm = -1.0f + 2.0f * frac;

        // X-axis ticks (along bottom front edge)
        if (col_labels && ti < (int)col_labels->size()) {
            int idx = (int)(frac * (col_labels->size() - 1));
            ImVec2 tp = project(v3(norm, -1.0f, -1.15f), center, sc);
            dl->AddText(tp, tick_col, (*col_labels)[idx].c_str());
        }
        // Y-axis ticks (along left edge)
        float yval = min_val + frac * range;
        std::snprintf(tick_buf, 32, "%.1f", yval);
        ImVec2 yp = project(v3(-1.2f, norm, -1.0f), center, sc);
        dl->AddText(yp, tick_col, tick_buf);

        // Z-axis ticks (along bottom left edge)
        if (row_labels && ti < (int)row_labels->size()) {
            int idx = (int)(frac * (row_labels->size() - 1));
            ImVec2 zp = project(v3(-1.15f, -1.0f, norm), center, sc);
            dl->AddText(zp, tick_col, (*row_labels)[idx].c_str());
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

    // Precompute camera trig for depth sorting
    float cam_cy = std::cos(cam_yaw_), cam_sy = std::sin(cam_yaw_);
    float cam_cp = std::cos(cam_pitch_), cam_sp = std::sin(cam_pitch_);
    auto depth_of = [&](Vec3 pt) -> float {
        float zr = pt.x * cam_sy + pt.z * cam_cy;
        return pt.y * cam_sp + zr * cam_cp;
    };

    // Simple directional light for shading
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

            // Compute face normal for lighting
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
            float shade = 0.55f + 0.45f * std::max(0.0f, ndot); // ambient 0.55 + diffuse 0.45

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

    // Sort back-to-front (painter's algorithm)
    std::sort(quads.begin(), quads.end(), [](const SurfQuad& a, const SurfQuad& b) {
        return a.depth < b.depth;
    });

    // Point-in-quad test helper
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

    // Draw quads with thin wireframe overlay
    int hover_idx = -1;
    ImVec2 mouse = ImGui::GetMousePos();
    for (int qi = 0; qi < (int)quads.size(); qi++) {
        auto& q = quads[qi];
        ImU32 fill = ImGui::GetColorU32(q.color);
        // Subtle darker wireframe edges
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
        // Glow outline on hovered quad
        dl->AddQuad(hq.pts[0], hq.pts[1], hq.pts[2], hq.pts[3],
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.95f)), 2.5f);
        // Inner glow
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

    // ---- Color bar (gradient) on the right with tick labels ----
    float bar_x = origin.x + avail.x - 40;
    float bar_top = origin.y + 30;
    float bar_h = avail.y - 60;
    float bar_w = 16.0f;

    // Gradient
    for (int ci = 0; ci < (int)bar_h; ci++) {
        float ct = 1.0f - (float)ci / bar_h;
        ImVec4 cc = surface_color(ct, diverging);
        dl->AddRectFilled(ImVec2(bar_x, bar_top + ci), ImVec2(bar_x + bar_w, bar_top + ci + 1),
                          ImGui::GetColorU32(cc));
    }
    dl->AddRect(ImVec2(bar_x, bar_top), ImVec2(bar_x + bar_w, bar_top + bar_h),
                ImGui::GetColorU32(ImVec4(0.4f, 0.5f, 0.6f, 0.6f)), 0, 0, 1.0f);

    // Tick labels along color bar
    int bar_ticks = 6;
    for (int bi = 0; bi <= bar_ticks; bi++) {
        float frac = (float)bi / (float)bar_ticks;
        float val = max_val - frac * range;
        float py = bar_top + frac * bar_h;
        char bl[16]; std::snprintf(bl, 16, "%.1f", val);
        // Tick mark
        dl->AddLine(ImVec2(bar_x + bar_w, py), ImVec2(bar_x + bar_w + 4, py),
                    ImGui::GetColorU32(ImVec4(0.5f, 0.6f, 0.7f, 0.7f)));
        dl->AddText(ImVec2(bar_x + bar_w + 6, py - 6), ImGui::GetColorU32(ImVec4(0.55f, 0.65f, 0.75f, 0.9f)), bl);
    }
    // Color bar title
    dl->AddText(ImVec2(bar_x - 4, bar_top - 16), ImGui::GetColorU32(ImVec4(0.40f, 0.85f, 0.95f, 1.0f)), y_label);
}

// ============================================================================
// Table renderers
// ============================================================================
void SurfaceScreen::render_vol_table() {
    if (vol_data_.z.empty()) return;
    int n_exp = (int)vol_data_.expirations.size();
    int n_str = (int)vol_data_.strikes.size();

    ImGui::TextColored(ACCENT, "IV SURFACE — %s ($%.2f)", vol_data_.underlying.c_str(), vol_data_.spot_price);
    ImGui::Spacing();

    if (ImGui::BeginTable("##vol_tbl", n_str + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        // Header
        ImGui::TableSetupColumn("DTE", ImGuiTableColumnFlags_WidthFixed, 50);
        for (int j = 0; j < n_str; j++) {
            char h[16]; std::snprintf(h, 16, "%.0f", vol_data_.strikes[j]);
            ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthFixed, 50);
        }
        ImGui::TableHeadersRow();

        for (int i = 0; i < n_exp; i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "%dD", vol_data_.expirations[i]);
            for (int j = 0; j < n_str; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                float v = vol_data_.z[i][j];
                ImGui::TextColored(TEXT_PRIMARY, "%.1f", v);
            }
        }
        ImGui::EndTable();
    }
}

void SurfaceScreen::render_corr_table() {
    if (corr_data_.z.empty()) return;
    int n = (int)corr_data_.assets.size();
    auto& last = corr_data_.z.back();

    ImGui::TextColored(ACCENT, "CORRELATION MATRIX — %dD ROLLING", corr_data_.window);
    ImGui::Spacing();

    if (ImGui::BeginTable("##corr_tbl", n + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 50);
        for (int j = 0; j < n; j++)
            ImGui::TableSetupColumn(corr_data_.assets[j].c_str(), ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableHeadersRow();

        for (int i = 0; i < n; i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "%s", corr_data_.assets[i].c_str());
            for (int j = 0; j < n; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                float c = last[i * n + j];
                ImVec4 col = c > 0.5f ? MARKET_GREEN : (c < -0.2f ? MARKET_RED : TEXT_PRIMARY);
                if (i == j) col = TEXT_DIM;
                ImGui::TextColored(col, "%.2f", c);
            }
        }
        ImGui::EndTable();
    }
}

void SurfaceScreen::render_yield_table() {
    if (yield_data_.z.empty()) return;
    int n_mat = (int)yield_data_.maturities.size();
    int n_show = std::min(30, (int)yield_data_.z.size());
    int start = (int)yield_data_.z.size() - n_show;

    ImGui::TextColored(ACCENT, "US TREASURY YIELD CURVE");
    ImGui::Spacing();

    if (ImGui::BeginTable("##yield_tbl", n_mat + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Day", ImGuiTableColumnFlags_WidthFixed, 45);
        for (int j = 0; j < n_mat && j < 11; j++)
            ImGui::TableSetupColumn(MAT_LABELS[j], ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableHeadersRow();

        for (int i = start; i < (int)yield_data_.z.size(); i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "D-%d", (int)yield_data_.z.size() - i);
            for (int j = 0; j < n_mat; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                ImGui::TextColored(TEXT_PRIMARY, "%.2f", yield_data_.z[i][j]);
            }
        }
        ImGui::EndTable();
    }
}

void SurfaceScreen::render_pca_table() {
    if (pca_data_.z.empty()) return;
    int n_f = (int)pca_data_.factors.size();

    ImGui::TextColored(ACCENT, "PCA FACTOR LOADINGS");
    ImGui::Spacing();

    // Variance bar
    for (int i = 0; i < n_f; i++) {
        char b[32]; std::snprintf(b, 32, "PC%d: %.1f%%", i+1, pca_data_.variance_explained[i]*100);
        ImGui::SameLine(0, i == 0 ? 0 : 8);
        ImGui::TextColored(i == 0 ? ACCENT : TEXT_SECONDARY, "%s", b);
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("##pca_tbl", n_f + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthFixed, 55);
        for (int j = 0; j < n_f; j++)
            ImGui::TableSetupColumn(pca_data_.factors[j].c_str(), ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)pca_data_.assets.size(); i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "%s", pca_data_.assets[i].c_str());
            for (int j = 0; j < n_f; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                float v = pca_data_.z[i][j];
                ImVec4 col = v > 0.5f ? MARKET_GREEN : (v < -0.3f ? MARKET_RED : TEXT_PRIMARY);
                ImGui::TextColored(col, "%.3f", v);
            }
        }
        ImGui::EndTable();
    }
}

// ============================================================================
void SurfaceScreen::render_chart_area() {
    if (show_table_) {
        ImGui::SetCursorPos(ImVec2(8, 8));
        switch (active_chart_) {
            case ChartType::Volatility:  render_vol_table(); break;
            case ChartType::Correlation: render_corr_table(); break;
            case ChartType::YieldCurve:  render_yield_table(); break;
            case ChartType::PCA:         render_pca_table(); break;
        }
        return;
    }

    // 3D surface plot (default)
    switch (active_chart_) {
    case ChartType::Volatility: {
        if (vol_data_.z.empty()) break;
        float mn=999, mx=0;
        for (auto& r : vol_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        // Build axis labels: strikes (cols) and DTEs (rows)
        std::vector<std::string> strike_labels, dte_labels;
        char lb[32];
        for (float s : vol_data_.strikes) { std::snprintf(lb, 32, "$%.0f", s); strike_labels.push_back(lb); }
        for (int e : vol_data_.expirations) { std::snprintf(lb, 32, "%dD", e); dte_labels.push_back(lb); }
        render_3d_surface(vol_data_.z, "STRIKE", "IV %", "DTE", mn, mx, false, &strike_labels, &dte_labels);
        break;
    }
    case ChartType::Correlation: {
        if (corr_data_.z.empty()) break;
        int n = (int)corr_data_.assets.size();
        std::vector<std::vector<float>> mat;
        auto& flat = corr_data_.z.back();
        for (int i=0;i<n;i++) { std::vector<float> r; for(int j=0;j<n;j++) r.push_back(flat[i*n+j]); mat.push_back(r); }
        render_3d_surface(mat, "ASSET", "CORR", "ASSET", -1, 1, true, &corr_data_.assets, &corr_data_.assets);
        break;
    }
    case ChartType::YieldCurve: {
        if (yield_data_.z.empty()) break;
        int ns = std::min(30, (int)yield_data_.z.size());
        int st = (int)yield_data_.z.size() - ns;
        std::vector<std::vector<float>> sub(yield_data_.z.begin()+st, yield_data_.z.end());
        float mn=999, mx=0;
        for (auto& r : sub) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        // Build maturity labels (cols) and time labels (rows)
        std::vector<std::string> mat_labels;
        char mb[32];
        for (int m : yield_data_.maturities) { std::snprintf(mb, 32, "%dM", m); mat_labels.push_back(mb); }
        std::vector<std::string> time_labels;
        char lb[32];
        for (int ti = 0; ti < ns; ti++) { std::snprintf(lb, 32, "D-%d", ns - ti); time_labels.push_back(lb); }
        render_3d_surface(sub, "MATURITY", "YIELD %", "TIME", mn, mx, false, &mat_labels, &time_labels);
        break;
    }
    case ChartType::PCA: {
        if (pca_data_.z.empty()) break;
        render_3d_surface(pca_data_.z, "FACTOR", "LOADING", "ASSET", -1, 1, true,
                          &pca_data_.factors, &pca_data_.assets);
        break;
    }
    }
}

} // namespace fincept::surface
