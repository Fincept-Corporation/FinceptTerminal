#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

void SurfaceScreen::render_metrics_derivatives() {
    float metric_label_w = ImGui::GetContentRegionAvail().x * 0.45f;
    auto row = [metric_label_w](const char* lbl, const char* val, ImVec4 col) {
        ImGui::TextColored(TEXT_DIM, "%s", lbl);
        ImGui::SameLine(metric_label_w);
        ImGui::TextColored(col, "%s", val);
    };
    char b[64];

    switch (active_chart_) {
    case ChartType::DeltaSurface: case ChartType::GammaSurface:
    case ChartType::VegaSurface: case ChartType::ThetaSurface: {
        const GreeksSurfaceData* gd = nullptr;
        if (active_chart_ == ChartType::DeltaSurface) gd = &delta_data_;
        else if (active_chart_ == ChartType::GammaSurface) gd = &gamma_data_;
        else if (active_chart_ == ChartType::VegaSurface) gd = &vega_data_;
        else gd = &theta_data_;
        if (gd->z.empty()) break;

        row("GREEK", gd->greek_name.c_str(), ACCENT);
        std::snprintf(b, 64, "$%.2f", gd->spot_price); row("SPOT", b, TEXT_PRIMARY);
        std::snprintf(b, 64, "%s", gd->underlying.c_str()); row("SYMBOL", b, TEXT_PRIMARY);

        int me = (int)gd->z.size()/2, ms = (int)gd->strikes.size()/2;
        std::snprintf(b, 64, "%.4f", gd->z[me][ms]); row("ATM VALUE", b, MARKET_GREEN);

        std::snprintf(b, 64, "%d", (int)(gd->strikes.size() * gd->expirations.size()));
        row("POINTS", b, TEXT_SECONDARY);
        std::snprintf(b, 64, "%.0f - %.0f", gd->strikes.front(), gd->strikes.back());
        row("STRIKES", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::SkewSurface: {
        if (skew_data_.z.empty()) break;
        row("TYPE", "25D Risk Reversal", TEXT_PRIMARY);
        row("SYMBOL", skew_data_.underlying.c_str(), TEXT_PRIMARY);
        // Front-month skew
        std::snprintf(b, 64, "%.2f%%", skew_data_.z[0][2]); // 25D put at front tenor
        row("1W 25D SKEW", b, skew_data_.z[0][2] > 0 ? MARKET_RED : MARKET_GREEN);
        // Back-month skew
        int last = (int)skew_data_.z.size() - 1;
        std::snprintf(b, 64, "%.2f%%", skew_data_.z[last][2]);
        row("1Y 25D SKEW", b, TEXT_PRIMARY);
        std::snprintf(b, 64, "%d", (int)skew_data_.expirations.size());
        row("TENORS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::LocalVolSurface: {
        if (local_vol_data_.z.empty()) break;
        std::snprintf(b, 64, "$%.2f", local_vol_data_.spot_price); row("SPOT", b, TEXT_PRIMARY);
        row("SYMBOL", local_vol_data_.underlying.c_str(), TEXT_PRIMARY);
        row("MODEL", "Dupire Local Vol", TEXT_SECONDARY);
        int me = (int)local_vol_data_.z.size()/2, ms = (int)local_vol_data_.strikes.size()/2;
        std::snprintf(b, 64, "%.1f%%", local_vol_data_.z[me][ms]); row("ATM LV", b, MARKET_GREEN);
        break;
    }
    default: break;
    }
}

void SurfaceScreen::render_chart_derivatives() {
    char lb[32];
    switch (active_chart_) {
    case ChartType::DeltaSurface: case ChartType::GammaSurface:
    case ChartType::VegaSurface: case ChartType::ThetaSurface: {
        const GreeksSurfaceData* gd = nullptr;
        if (active_chart_ == ChartType::DeltaSurface) gd = &delta_data_;
        else if (active_chart_ == ChartType::GammaSurface) gd = &gamma_data_;
        else if (active_chart_ == ChartType::VegaSurface) gd = &vega_data_;
        else gd = &theta_data_;
        if (gd->z.empty()) break;

        float mn = 999, mx = -999;
        for (const auto& r : gd->z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }

        std::vector<std::string> strike_labels, dte_labels;
        for (float s : gd->strikes) { std::snprintf(lb, 32, "$%.0f", s); strike_labels.push_back(lb); }
        for (int e : gd->expirations) { std::snprintf(lb, 32, "%dD", e); dte_labels.push_back(lb); }

        bool diverging = (active_chart_ == ChartType::ThetaSurface);
        render_3d_surface(gd->z, "STRIKE", gd->greek_name.c_str(), "DTE",
                          mn, mx, diverging, &strike_labels, &dte_labels);
        break;
    }
    case ChartType::SkewSurface: {
        if (skew_data_.z.empty()) break;
        float mn = 999, mx = -999;
        for (const auto& r : skew_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }

        std::vector<std::string> delta_labels, dte_labels;
        for (float d : skew_data_.deltas) { std::snprintf(lb, 32, "%dD", (int)d); delta_labels.push_back(lb); }
        for (int e : skew_data_.expirations) { std::snprintf(lb, 32, "%dD", e); dte_labels.push_back(lb); }

        render_3d_surface(skew_data_.z, "DELTA", "SKEW %", "DTE", mn, mx, true, &delta_labels, &dte_labels);
        break;
    }
    case ChartType::LocalVolSurface: {
        if (local_vol_data_.z.empty()) break;
        float mn = 999, mx = -999;
        for (const auto& r : local_vol_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }

        std::vector<std::string> strike_labels, dte_labels;
        for (float s : local_vol_data_.strikes) { std::snprintf(lb, 32, "$%.0f", s); strike_labels.push_back(lb); }
        for (int e : local_vol_data_.expirations) { std::snprintf(lb, 32, "%dD", e); dte_labels.push_back(lb); }

        render_3d_surface(local_vol_data_.z, "STRIKE", "LOCAL VOL %", "DTE", mn, mx, false, &strike_labels, &dte_labels);
        break;
    }
    default: break;
    }
}

} // namespace fincept::surface
