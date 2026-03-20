#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

void SurfaceScreen::render_metrics_macro() {
    float w = ImGui::GetContentRegionAvail().x;
    float metric_label_w = w * 0.52f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto row = [&](const char* lbl, const char* val, ImVec4 col) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(p.x + 4.0f, p.y + ImGui::GetTextLineHeight() * 0.5f + 1.0f),
                            3.0f, ImGui::GetColorU32(ImVec4(col.x, col.y, col.z, 0.7f)));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);
        ImGui::TextColored(TEXT_DIM, "%s", lbl);
        ImGui::SameLine(metric_label_w);
        ImGui::TextColored(col, "%s", val);
    };
    char b[64];

    switch (active_chart_) {
    case ChartType::InflationExpectations: {
        if (inflation_data_.z.empty()) break;
        row("TYPE", "Breakeven Inflation", TEXT_PRIMARY);
        const auto& last = inflation_data_.z.back();
        if (last.size() > 3) {
            std::snprintf(b, 64, "%.2f%%", last[0]); row("1Y BEI", b, last[0] > 3.0f ? MARKET_RED : TEXT_PRIMARY);
            std::snprintf(b, 64, "%.2f%%", last[3]); row("5Y BEI", b, TEXT_PRIMARY);
        }
        if (last.size() > 5) {
            std::snprintf(b, 64, "%.2f%%", last[5]); row("10Y BEI", b, TEXT_PRIMARY);
        }
        std::snprintf(b, 64, "%d", (int)inflation_data_.horizons.size()); row("HORIZONS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::MonetaryPolicyPath: {
        if (monetary_data_.z.empty()) break;
        row("TYPE", "Policy Rate Paths", TEXT_PRIMARY);
        // Show current rates for major banks
        for (int i = 0; i < std::min(4, (int)monetary_data_.central_banks.size()); i++) {
            if (!monetary_data_.z[i].empty()) {
                std::snprintf(b, 64, "%.2f%%", monetary_data_.z[i][0]);
                row(monetary_data_.central_banks[i].c_str(), b, TEXT_PRIMARY);
            }
        }
        std::snprintf(b, 64, "%d", (int)monetary_data_.central_banks.size()); row("BANKS", b, TEXT_SECONDARY);
        break;
    }
    default: break;
    }
}

void SurfaceScreen::render_chart_macro() {
    char lb[32];
    switch (active_chart_) {
    case ChartType::InflationExpectations: {
        if (inflation_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : inflation_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> horizon_labels, time_labels;
        for (int h : inflation_data_.horizons) { std::snprintf(lb, 32, "%dY", h); horizon_labels.push_back(lb); }
        int n = (int)inflation_data_.time_points.size();
        for (int i = 0; i < n; i++) { std::snprintf(lb, 32, "D-%d", n - i); time_labels.push_back(lb); }
        render_3d_surface(inflation_data_.z, "HORIZON", "BEI %", "TIME", mn, mx, false, &horizon_labels, &time_labels);
        break;
    }
    case ChartType::MonetaryPolicyPath: {
        if (monetary_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : monetary_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> meeting_labels;
        for (int m : monetary_data_.meetings_ahead) { std::snprintf(lb, 32, "Mtg %d", m); meeting_labels.push_back(lb); }
        render_3d_surface(monetary_data_.z, "MEETING", "RATE %", "BANK", mn, mx, false,
                          &meeting_labels, &monetary_data_.central_banks);
        break;
    }
    default: break;
    }
}

} // namespace fincept::surface
