#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

void SurfaceScreen::render_metrics_fixed_income() {
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
    case ChartType::SwaptionVol: {
        if (swaption_data_.z.empty()) break;
        row("TYPE", "Swaption Normal Vol", TEXT_PRIMARY);
        int me = (int)swaption_data_.z.size()/2, ms = (int)swaption_data_.swap_tenors.size()/2;
        std::snprintf(b, 64, "%.0f bps", swaption_data_.z[me][ms]); row("ATM VOL", b, MARKET_GREEN);
        std::snprintf(b, 64, "%d", (int)(swaption_data_.option_expiries.size() * swaption_data_.swap_tenors.size()));
        row("POINTS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::CapFloorVol: {
        if (capfloor_data_.z.empty()) break;
        row("TYPE", "Cap/Floor Normal Vol", TEXT_PRIMARY);
        int me = (int)capfloor_data_.z.size()/2, ms = (int)capfloor_data_.strikes.size()/2;
        std::snprintf(b, 64, "%.0f bps", capfloor_data_.z[me][ms]); row("ATM VOL", b, MARKET_GREEN);
        break;
    }
    case ChartType::BondSpread: {
        if (bond_spread_data_.z.empty()) break;
        row("TYPE", "Credit Spread by Rating", TEXT_PRIMARY);
        // IG average (AAA-BBB = indices 0-3)
        float ig_avg = 0;
        for (int i = 0; i < 4; i++) {
            int mid = (int)bond_spread_data_.maturities.size() / 2;
            ig_avg += bond_spread_data_.z[i][mid];
        }
        ig_avg /= 4.0f;
        std::snprintf(b, 64, "%.0f bps", ig_avg); row("IG AVG", b, MARKET_GREEN);
        // HY average (BB-CCC = indices 4-6)
        float hy_avg = 0;
        for (int i = 4; i < 7; i++) {
            int mid = (int)bond_spread_data_.maturities.size() / 2;
            hy_avg += bond_spread_data_.z[i][mid];
        }
        hy_avg /= 3.0f;
        std::snprintf(b, 64, "%.0f bps", hy_avg); row("HY AVG", b, MARKET_RED);
        break;
    }
    case ChartType::OISBasis: {
        if (ois_data_.z.empty()) break;
        row("TYPE", "OIS-SOFR Basis", TEXT_PRIMARY);
        const auto& last = ois_data_.z.back();
        if (!last.empty()) {
            std::snprintf(b, 64, "%.1f bps", last[0]); row("FRONT", b, TEXT_PRIMARY);
            std::snprintf(b, 64, "%.1f bps", last.back()); row("BACK", b, TEXT_PRIMARY);
        }
        break;
    }
    case ChartType::RealYield: {
        if (real_yield_data_.z.empty()) break;
        row("TYPE", "TIPS Real Yield", TEXT_PRIMARY);
        const auto& last = real_yield_data_.z.back();
        if (last.size() > 4) {
            std::snprintf(b, 64, "%.2f%%", last[2]); row("5Y REAL", b, last[2] > 0 ? MARKET_GREEN : MARKET_RED);
            std::snprintf(b, 64, "%.2f%%", last[4]); row("10Y REAL", b, TEXT_PRIMARY);
        }
        break;
    }
    case ChartType::ForwardRate: {
        if (fwd_rate_data_.z.empty()) break;
        row("TYPE", "Nelson-Siegel Forward", TEXT_PRIMARY);
        // 1Y forward 1Y
        if (fwd_rate_data_.z.size() > 3 && !fwd_rate_data_.z[3].empty()) {
            std::snprintf(b, 64, "%.2f%%", fwd_rate_data_.z[3][0]); row("1Y1Y FWD", b, TEXT_PRIMARY);
        }
        break;
    }
    default: break;
    }
}

void SurfaceScreen::render_chart_fixed_income() {
    char lb[32];
    switch (active_chart_) {
    case ChartType::SwaptionVol: {
        if (swaption_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : swaption_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> tenor_labels, exp_labels;
        for (int t : swaption_data_.swap_tenors) { std::snprintf(lb, 32, "%dM", t); tenor_labels.push_back(lb); }
        for (int e : swaption_data_.option_expiries) { std::snprintf(lb, 32, "%dM", e); exp_labels.push_back(lb); }
        render_3d_surface(swaption_data_.z, "SWAP TENOR", "VOL (bps)", "OPT EXPIRY", mn, mx, false, &tenor_labels, &exp_labels);
        break;
    }
    case ChartType::CapFloorVol: {
        if (capfloor_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : capfloor_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> strike_labels, mat_labels;
        for (float s : capfloor_data_.strikes) { std::snprintf(lb, 32, "%.1f%%", s); strike_labels.push_back(lb); }
        for (int m : capfloor_data_.maturities) { std::snprintf(lb, 32, "%dM", m); mat_labels.push_back(lb); }
        render_3d_surface(capfloor_data_.z, "STRIKE", "VOL (bps)", "MATURITY", mn, mx, false, &strike_labels, &mat_labels);
        break;
    }
    case ChartType::BondSpread: {
        if (bond_spread_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : bond_spread_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> mat_labels;
        for (int m : bond_spread_data_.maturities) { std::snprintf(lb, 32, "%dM", m); mat_labels.push_back(lb); }
        render_3d_surface(bond_spread_data_.z, "MATURITY", "SPREAD (bps)", "RATING", mn, mx, false, &mat_labels, &bond_spread_data_.ratings);
        break;
    }
    case ChartType::OISBasis: {
        if (ois_data_.z.empty()) break;
        float mn = 999, mx = -999;
        for (const auto& r : ois_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> tenor_labels, time_labels;
        for (int t : ois_data_.tenors) { std::snprintf(lb, 32, "%dM", t); tenor_labels.push_back(lb); }
        for (int d : ois_data_.time_points) { std::snprintf(lb, 32, "D-%d", 60 - d); time_labels.push_back(lb); }
        render_3d_surface(ois_data_.z, "TENOR", "BASIS (bps)", "TIME", mn, mx, true, &tenor_labels, &time_labels);
        break;
    }
    case ChartType::RealYield: {
        if (real_yield_data_.z.empty()) break;
        float mn = 999, mx = -999;
        for (const auto& r : real_yield_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> mat_labels, time_labels;
        for (int m : real_yield_data_.maturities) { std::snprintf(lb, 32, "%dM", m); mat_labels.push_back(lb); }
        for (int d : real_yield_data_.time_points) { std::snprintf(lb, 32, "D-%d", 60 - d); time_labels.push_back(lb); }
        render_3d_surface(real_yield_data_.z, "MATURITY", "REAL YIELD %", "TIME", mn, mx, true, &mat_labels, &time_labels);
        break;
    }
    case ChartType::ForwardRate: {
        if (fwd_rate_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : fwd_rate_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> fwd_labels, start_labels;
        for (int f : fwd_rate_data_.forward_periods) { std::snprintf(lb, 32, "%dM", f); fwd_labels.push_back(lb); }
        for (int s : fwd_rate_data_.start_tenors) { std::snprintf(lb, 32, "%dM", s); start_labels.push_back(lb); }
        render_3d_surface(fwd_rate_data_.z, "FWD PERIOD", "RATE %", "START", mn, mx, false, &fwd_labels, &start_labels);
        break;
    }
    default: break;
    }
}

} // namespace fincept::surface
