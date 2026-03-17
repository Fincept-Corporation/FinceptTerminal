#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

void SurfaceScreen::render_metrics_fx() {
    float metric_label_w = ImGui::GetContentRegionAvail().x * 0.45f;
    auto row = [metric_label_w](const char* lbl, const char* val, ImVec4 col) {
        ImGui::TextColored(TEXT_DIM, "%s", lbl);
        ImGui::SameLine(metric_label_w);
        ImGui::TextColored(col, "%s", val);
    };
    char b[64];

    switch (active_chart_) {
    case ChartType::FXVol: {
        if (fx_vol_data_.z.empty()) break;
        row("PAIR", fx_vol_data_.pair.c_str(), ACCENT);
        int atm_idx = (int)fx_vol_data_.deltas.size() / 2;
        std::snprintf(b, 64, "%.1f%%", fx_vol_data_.z[0][atm_idx]); row("1W ATM VOL", b, TEXT_PRIMARY);
        std::snprintf(b, 64, "%.1f%%", fx_vol_data_.z.back()[atm_idx]); row("1Y ATM VOL", b, TEXT_PRIMARY);
        std::snprintf(b, 64, "%d", (int)fx_vol_data_.tenors.size()); row("TENORS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::FXForwardPoints: {
        if (fx_fwd_data_.z.empty()) break;
        row("TYPE", "Forward Points (pips)", TEXT_PRIMARY);
        std::snprintf(b, 64, "%d pairs", (int)fx_fwd_data_.pairs.size()); row("COVERAGE", b, TEXT_SECONDARY);
        std::snprintf(b, 64, "%d tenors", (int)fx_fwd_data_.tenors.size()); row("TENORS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::CrossCurrencyBasis: {
        if (xccy_data_.z.empty()) break;
        row("TYPE", "Cross-Ccy Basis Swap", TEXT_PRIMARY);
        // Show widest basis
        float widest = 0;
        std::string widest_pair;
        for (int i = 0; i < (int)xccy_data_.pairs.size(); i++) {
            int mid = (int)xccy_data_.tenors.size() / 2;
            if (std::abs(xccy_data_.z[i][mid]) > std::abs(widest)) {
                widest = xccy_data_.z[i][mid];
                widest_pair = xccy_data_.pairs[i];
            }
        }
        std::snprintf(b, 64, "%.0f bps", widest); row("WIDEST", b, MARKET_RED);
        row("PAIR", widest_pair.c_str(), TEXT_SECONDARY);
        break;
    }
    default: break;
    }
}

void SurfaceScreen::render_chart_fx() {
    char lb[32];
    switch (active_chart_) {
    case ChartType::FXVol: {
        if (fx_vol_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : fx_vol_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> delta_labels, tenor_labels;
        for (float d : fx_vol_data_.deltas) { std::snprintf(lb, 32, "%dD", (int)d); delta_labels.push_back(lb); }
        for (int t : fx_vol_data_.tenors) { std::snprintf(lb, 32, "%dD", t); tenor_labels.push_back(lb); }
        render_3d_surface(fx_vol_data_.z, "DELTA", "VOL %", "TENOR", mn, mx, false, &delta_labels, &tenor_labels);
        break;
    }
    case ChartType::FXForwardPoints: {
        if (fx_fwd_data_.z.empty()) break;
        float mn = 999, mx = -999;
        for (const auto& r : fx_fwd_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> tenor_labels;
        for (int t : fx_fwd_data_.tenors) { std::snprintf(lb, 32, "%dD", t); tenor_labels.push_back(lb); }
        render_3d_surface(fx_fwd_data_.z, "TENOR", "PIPS", "PAIR", mn, mx, true, &tenor_labels, &fx_fwd_data_.pairs);
        break;
    }
    case ChartType::CrossCurrencyBasis: {
        if (xccy_data_.z.empty()) break;
        float mn = 999, mx = -999;
        for (const auto& r : xccy_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> tenor_labels;
        for (int t : xccy_data_.tenors) { std::snprintf(lb, 32, "%dM", t); tenor_labels.push_back(lb); }
        render_3d_surface(xccy_data_.z, "TENOR", "BASIS (bps)", "PAIR", mn, mx, true, &tenor_labels, &xccy_data_.pairs);
        break;
    }
    default: break;
    }
}

} // namespace fincept::surface
