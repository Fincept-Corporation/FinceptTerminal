#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

void SurfaceScreen::render_metrics_risk() {
    float metric_label_w = ImGui::GetContentRegionAvail().x * 0.45f;
    auto row = [metric_label_w](const char* lbl, const char* val, ImVec4 col) {
        ImGui::TextColored(TEXT_DIM, "%s", lbl);
        ImGui::SameLine(metric_label_w);
        ImGui::TextColored(col, "%s", val);
    };
    char b[64];

    switch (active_chart_) {
    case ChartType::VaR: {
        if (var_data_.z.empty()) break;
        row("TYPE", "Value-at-Risk", TEXT_PRIMARY);
        // 99% 1-day VaR
        std::snprintf(b, 64, "%.2f%%", var_data_.z[3][0]); row("99% 1D VAR", b, MARKET_RED);
        // 95% 10-day VaR
        std::snprintf(b, 64, "%.2f%%", var_data_.z[1][3]); row("95% 10D VAR", b, WARNING);
        std::snprintf(b, 64, "%d", (int)var_data_.confidence_levels.size()); row("CONF LEVELS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::StressTestPnL: {
        if (stress_data_.z.empty()) break;
        row("TYPE", "Stress Test P&L", TEXT_PRIMARY);
        // Worst scenario across portfolios
        float worst = 0;
        for (const auto& r : stress_data_.z) for (float v : r) worst = std::min(worst, v);
        std::snprintf(b, 64, "%.1f%%", worst); row("WORST LOSS", b, MARKET_RED);
        std::snprintf(b, 64, "%d", (int)stress_data_.scenarios.size()); row("SCENARIOS", b, TEXT_SECONDARY);
        std::snprintf(b, 64, "%d", (int)stress_data_.portfolios.size()); row("PORTFOLIOS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::FactorExposure: {
        if (factor_data_.z.empty()) break;
        row("TYPE", "Fama-French + Quality", TEXT_PRIMARY);
        std::snprintf(b, 64, "%d", (int)factor_data_.factors.size()); row("FACTORS", b, TEXT_SECONDARY);
        std::snprintf(b, 64, "%d", (int)factor_data_.assets.size()); row("ASSETS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::LiquidityHeatmap: {
        if (liquidity_data_.z.empty()) break;
        row("TYPE", "Bid-Ask Spread Map", TEXT_PRIMARY);
        row("SYMBOL", liquidity_data_.underlying.c_str(), ACCENT);
        // ATM avg spread
        int ms = (int)liquidity_data_.strikes.size() / 2;
        float avg_atm = 0;
        for (const auto& r : liquidity_data_.z) avg_atm += r[ms];
        avg_atm /= (float)liquidity_data_.z.size();
        std::snprintf(b, 64, "%.2f", avg_atm); row("ATM AVG SPR", b, MARKET_GREEN);
        break;
    }
    case ChartType::Drawdown: {
        if (drawdown_data_.z.empty()) break;
        row("TYPE", "Rolling Max Drawdown", TEXT_PRIMARY);
        float worst = 0;
        for (const auto& r : drawdown_data_.z) for (float v : r) worst = std::max(worst, v);
        std::snprintf(b, 64, "%.1f%%", worst); row("WORST DD", b, MARKET_RED);
        std::snprintf(b, 64, "%d", (int)drawdown_data_.assets.size()); row("ASSETS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::BetaSurface: {
        if (beta_data_.z.empty()) break;
        row("TYPE", "Rolling Beta to SPX", TEXT_PRIMARY);
        std::snprintf(b, 64, "%d", (int)beta_data_.assets.size()); row("ASSETS", b, TEXT_SECONDARY);
        std::snprintf(b, 64, "%d", (int)beta_data_.horizons.size()); row("HORIZONS", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::ImpliedDividend: {
        if (impl_div_data_.z.empty()) break;
        row("TYPE", "Implied Div Yield", TEXT_PRIMARY);
        row("SYMBOL", impl_div_data_.underlying.c_str(), ACCENT);
        int me = (int)impl_div_data_.z.size()/2, ms2 = (int)impl_div_data_.strikes.size()/2;
        std::snprintf(b, 64, "%.2f%%", impl_div_data_.z[me][ms2]); row("ATM DIV", b, MARKET_GREEN);
        break;
    }
    default: break;
    }
}

void SurfaceScreen::render_chart_risk() {
    char lb[32];
    switch (active_chart_) {
    case ChartType::VaR: {
        if (var_data_.z.empty()) break;
        float mn = 0, mx = 0;
        for (const auto& r : var_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> horizon_labels, conf_labels;
        for (int h : var_data_.horizons) { std::snprintf(lb, 32, "%dD", h); horizon_labels.push_back(lb); }
        for (float c : var_data_.confidence_levels) { std::snprintf(lb, 32, "%.1f%%", c); conf_labels.push_back(lb); }
        render_3d_surface(var_data_.z, "HORIZON", "VAR %", "CONFIDENCE", mn, mx, false, &horizon_labels, &conf_labels);
        break;
    }
    case ChartType::StressTestPnL: {
        if (stress_data_.z.empty()) break;
        float mn = 0, mx = 0;
        for (const auto& r : stress_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        render_3d_surface(stress_data_.z, "PORTFOLIO", "P&L %", "SCENARIO", mn, mx, true,
                          &stress_data_.portfolios, &stress_data_.scenarios);
        break;
    }
    case ChartType::FactorExposure: {
        if (factor_data_.z.empty()) break;
        render_3d_surface(factor_data_.z, "FACTOR", "EXPOSURE", "ASSET", -1.5f, 1.5f, true,
                          &factor_data_.factors, &factor_data_.assets);
        break;
    }
    case ChartType::LiquidityHeatmap: {
        if (liquidity_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : liquidity_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> strike_labels, dte_labels;
        for (float s : liquidity_data_.strikes) { std::snprintf(lb, 32, "$%.0f", s); strike_labels.push_back(lb); }
        for (int e : liquidity_data_.expirations) { std::snprintf(lb, 32, "%dD", e); dte_labels.push_back(lb); }
        render_3d_surface(liquidity_data_.z, "STRIKE", "SPREAD", "DTE", mn, mx, false, &strike_labels, &dte_labels);
        break;
    }
    case ChartType::Drawdown: {
        if (drawdown_data_.z.empty()) break;
        float mn = 0, mx = 0;
        for (const auto& r : drawdown_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> window_labels;
        for (int w : drawdown_data_.windows) { std::snprintf(lb, 32, "%dD", w); window_labels.push_back(lb); }
        render_3d_surface(drawdown_data_.z, "WINDOW", "MAX DD %", "ASSET", mn, mx, false, &window_labels, &drawdown_data_.assets);
        break;
    }
    case ChartType::BetaSurface: {
        if (beta_data_.z.empty()) break;
        float mn = 999, mx = -999;
        for (const auto& r : beta_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> horizon_labels;
        for (int h : beta_data_.horizons) { std::snprintf(lb, 32, "%dD", h); horizon_labels.push_back(lb); }
        render_3d_surface(beta_data_.z, "HORIZON", "BETA", "ASSET", mn, mx, true, &horizon_labels, &beta_data_.assets);
        break;
    }
    case ChartType::ImpliedDividend: {
        if (impl_div_data_.z.empty()) break;
        float mn = 0, mx = 0;
        for (const auto& r : impl_div_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> strike_labels, dte_labels;
        for (float s : impl_div_data_.strikes) { std::snprintf(lb, 32, "$%.0f", s); strike_labels.push_back(lb); }
        for (int e : impl_div_data_.expirations) { std::snprintf(lb, 32, "%dD", e); dte_labels.push_back(lb); }
        render_3d_surface(impl_div_data_.z, "STRIKE", "DIV YIELD %", "DTE", mn, mx, false, &strike_labels, &dte_labels);
        break;
    }
    default: break;
    }
}

} // namespace fincept::surface
