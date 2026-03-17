#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

void SurfaceScreen::render_metrics_commodities() {
    float metric_label_w = ImGui::GetContentRegionAvail().x * 0.45f;
    auto row = [metric_label_w](const char* lbl, const char* val, ImVec4 col) {
        ImGui::TextColored(TEXT_DIM, "%s", lbl);
        ImGui::SameLine(metric_label_w);
        ImGui::TextColored(col, "%s", val);
    };
    char b[64];

    switch (active_chart_) {
    case ChartType::CommodityForward: {
        if (cmdty_fwd_data_.z.empty()) break;
        row("TYPE", "Forward Curve", TEXT_PRIMARY);
        std::snprintf(b, 64, "%d", (int)cmdty_fwd_data_.commodities.size()); row("CMDTIES", b, TEXT_SECONDARY);
        std::snprintf(b, 64, "%d", (int)cmdty_fwd_data_.contract_months.size()); row("CONTRACTS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::CommodityVol: {
        if (cmdty_vol_data_.z.empty()) break;
        row("COMMODITY", cmdty_vol_data_.commodity.c_str(), ACCENT);
        int me = (int)cmdty_vol_data_.z.size()/2, ms = (int)cmdty_vol_data_.strikes.size()/2;
        std::snprintf(b, 64, "%.1f%%", cmdty_vol_data_.z[me][ms]); row("ATM VOL", b, MARKET_GREEN);
        break;
    }
    case ChartType::CrackSpread: {
        if (crack_data_.z.empty()) break;
        row("TYPE", "Crack/Crush Spreads", TEXT_PRIMARY);
        std::snprintf(b, 64, "%d", (int)crack_data_.spread_types.size()); row("SPREAD TYPES", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::ContangoBackwardation: {
        if (contango_data_.z.empty()) break;
        row("TYPE", "Contango/Backwardation", TEXT_PRIMARY);
        // Count how many are in contango vs backwardation
        int contango_count = 0, backw_count = 0;
        for (const auto& r : contango_data_.z) {
            float avg = 0;
            for (float v : r) avg += v;
            avg /= (float)r.size();
            if (avg > 0) contango_count++; else backw_count++;
        }
        std::snprintf(b, 64, "%d", contango_count); row("CONTANGO", b, MARKET_GREEN);
        std::snprintf(b, 64, "%d", backw_count); row("BACKWRD", b, MARKET_RED);
        break;
    }
    default: break;
    }
}

void SurfaceScreen::render_chart_commodities() {
    char lb[32];
    switch (active_chart_) {
    case ChartType::CommodityForward: {
        if (cmdty_fwd_data_.z.empty()) break;
        float mn = 999999, mx = 0;
        for (const auto& r : cmdty_fwd_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> month_labels;
        for (int m : cmdty_fwd_data_.contract_months) { std::snprintf(lb, 32, "%dM", m); month_labels.push_back(lb); }
        render_3d_surface(cmdty_fwd_data_.z, "CONTRACT", "PRICE", "COMMODITY", mn, mx, false, &month_labels, &cmdty_fwd_data_.commodities);
        break;
    }
    case ChartType::CommodityVol: {
        if (cmdty_vol_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : cmdty_vol_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> strike_labels, dte_labels;
        for (float s : cmdty_vol_data_.strikes) { std::snprintf(lb, 32, "%.0f%%", s); strike_labels.push_back(lb); }
        for (int e : cmdty_vol_data_.expirations) { std::snprintf(lb, 32, "%dD", e); dte_labels.push_back(lb); }
        render_3d_surface(cmdty_vol_data_.z, "STRIKE", "VOL %", "DTE", mn, mx, false, &strike_labels, &dte_labels);
        break;
    }
    case ChartType::CrackSpread: {
        if (crack_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : crack_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> month_labels;
        for (int m : crack_data_.contract_months) { std::snprintf(lb, 32, "%dM", m); month_labels.push_back(lb); }
        render_3d_surface(crack_data_.z, "CONTRACT", "SPREAD $", "TYPE", mn, mx, false, &month_labels, &crack_data_.spread_types);
        break;
    }
    case ChartType::ContangoBackwardation: {
        if (contango_data_.z.empty()) break;
        float mn = 999, mx = -999;
        for (const auto& r : contango_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> month_labels;
        for (int m : contango_data_.contract_months) { std::snprintf(lb, 32, "%dM", m); month_labels.push_back(lb); }
        render_3d_surface(contango_data_.z, "CONTRACT", "% FROM SPOT", "COMMODITY", mn, mx, true, &month_labels, &contango_data_.commodities);
        break;
    }
    default: break;
    }
}

} // namespace fincept::surface
