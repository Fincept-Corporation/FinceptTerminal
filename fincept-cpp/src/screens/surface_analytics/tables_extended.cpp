#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

// Generic table renderer for any grid + row/col labels
static void render_generic_table(
    const char* title,
    const std::vector<std::vector<float>>& z,
    const std::vector<std::string>& row_labels,
    const std::vector<std::string>& col_labels,
    const char* row_header,
    const char* fmt = "%.2f",
    bool color_diverging = false)
{
    if (z.empty() || col_labels.empty()) return;

    ImGui::TextColored(ACCENT, "%s", title);
    ImGui::Spacing();

    const int n_cols = (int)col_labels.size();
    if (ImGui::BeginTable("##ext_tbl", n_cols + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {

        ImGui::TableSetupColumn(row_header, ImGuiTableColumnFlags_WidthFixed, 80);
        for (int j = 0; j < n_cols; j++)
            ImGui::TableSetupColumn(col_labels[j].c_str(), ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        const int n_rows = std::min((int)z.size(), (int)row_labels.size());
        for (int i = 0; i < n_rows; i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "%s", row_labels[i].c_str());
            const int cols_in_row = std::min((int)z[i].size(), n_cols);
            for (int j = 0; j < cols_in_row; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                float v = z[i][j];
                ImVec4 col = TEXT_PRIMARY;
                if (color_diverging) {
                    col = v > 0.3f ? MARKET_GREEN : (v < -0.3f ? MARKET_RED : TEXT_PRIMARY);
                }
                char buf[32];
                std::snprintf(buf, 32, fmt, v);
                ImGui::TextColored(col, "%s", buf);
            }
        }
        ImGui::EndTable();
    }
}

// Build string labels from int vectors
static std::vector<std::string> int_labels(const std::vector<int>& vals, const char* suffix) {
    std::vector<std::string> out;
    char b[32];
    for (int v : vals) { std::snprintf(b, 32, "%d%s", v, suffix); out.push_back(b); }
    return out;
}

static std::vector<std::string> float_labels(const std::vector<float>& vals, const char* fmt) {
    std::vector<std::string> out;
    char b[32];
    for (float v : vals) { std::snprintf(b, 32, fmt, v); out.push_back(b); }
    return out;
}

void SurfaceScreen::render_table_for(ChartType type) {
    switch (type) {
    // --- Equity Derivatives ---
    case ChartType::DeltaSurface: case ChartType::GammaSurface:
    case ChartType::VegaSurface: case ChartType::ThetaSurface: {
        const GreeksSurfaceData* gd = nullptr;
        if (type == ChartType::DeltaSurface) gd = &delta_data_;
        else if (type == ChartType::GammaSurface) gd = &gamma_data_;
        else if (type == ChartType::VegaSurface) gd = &vega_data_;
        else gd = &theta_data_;
        auto sl = float_labels(gd->strikes, "%.0f");
        auto dl = int_labels(gd->expirations, "D");
        char title[64]; std::snprintf(title, 64, "%s SURFACE — %s", gd->greek_name.c_str(), gd->underlying.c_str());
        render_generic_table(title, gd->z, dl, sl, "DTE", "%.4f", type == ChartType::ThetaSurface);
        break;
    }
    case ChartType::SkewSurface: {
        auto dl = float_labels(skew_data_.deltas, "%.0fD");
        auto el = int_labels(skew_data_.expirations, "D");
        render_generic_table("SKEW SURFACE — 25D RISK REVERSAL", skew_data_.z, el, dl, "DTE", "%.2f", true);
        break;
    }
    case ChartType::LocalVolSurface: {
        auto sl = float_labels(local_vol_data_.strikes, "%.0f");
        auto el = int_labels(local_vol_data_.expirations, "D");
        render_generic_table("LOCAL VOLATILITY (DUPIRE)", local_vol_data_.z, el, sl, "DTE", "%.1f");
        break;
    }

    // --- Fixed Income ---
    case ChartType::SwaptionVol: {
        auto tl = int_labels(swaption_data_.swap_tenors, "M");
        auto el = int_labels(swaption_data_.option_expiries, "M");
        render_generic_table("SWAPTION VOLATILITY (bps)", swaption_data_.z, el, tl, "Opt Exp", "%.0f");
        break;
    }
    case ChartType::CapFloorVol: {
        auto sl = float_labels(capfloor_data_.strikes, "%.1f%%");
        auto ml = int_labels(capfloor_data_.maturities, "M");
        render_generic_table("CAP/FLOOR VOLATILITY (bps)", capfloor_data_.z, ml, sl, "Maturity", "%.0f");
        break;
    }
    case ChartType::BondSpread: {
        auto ml = int_labels(bond_spread_data_.maturities, "M");
        render_generic_table("BOND SPREAD BY RATING (bps)", bond_spread_data_.z, bond_spread_data_.ratings, ml, "Rating", "%.0f");
        break;
    }
    case ChartType::OISBasis: {
        auto tl = int_labels(ois_data_.tenors, "M");
        auto dl = int_labels(ois_data_.time_points, "");
        render_generic_table("OIS-SOFR BASIS (bps)", ois_data_.z, dl, tl, "Day", "%.1f", true);
        break;
    }
    case ChartType::RealYield: {
        auto ml = int_labels(real_yield_data_.maturities, "M");
        auto dl = int_labels(real_yield_data_.time_points, "");
        render_generic_table("TIPS REAL YIELD (%)", real_yield_data_.z, dl, ml, "Day", "%.2f", true);
        break;
    }
    case ChartType::ForwardRate: {
        auto fl = int_labels(fwd_rate_data_.forward_periods, "M");
        auto sl = int_labels(fwd_rate_data_.start_tenors, "M");
        render_generic_table("FORWARD RATE MATRIX (%)", fwd_rate_data_.z, sl, fl, "Start", "%.2f");
        break;
    }

    // --- FX ---
    case ChartType::FXVol: {
        auto dl = float_labels(fx_vol_data_.deltas, "%.0fD");
        auto tl = int_labels(fx_vol_data_.tenors, "D");
        char title[64]; std::snprintf(title, 64, "FX VOL SURFACE — %s", fx_vol_data_.pair.c_str());
        render_generic_table(title, fx_vol_data_.z, tl, dl, "Tenor", "%.1f");
        break;
    }
    case ChartType::FXForwardPoints: {
        auto tl = int_labels(fx_fwd_data_.tenors, "D");
        render_generic_table("FX FORWARD POINTS (pips)", fx_fwd_data_.z, fx_fwd_data_.pairs, tl, "Pair", "%.1f", true);
        break;
    }
    case ChartType::CrossCurrencyBasis: {
        auto tl = int_labels(xccy_data_.tenors, "M");
        render_generic_table("CROSS-CURRENCY BASIS (bps)", xccy_data_.z, xccy_data_.pairs, tl, "Pair", "%.0f", true);
        break;
    }

    // --- Credit ---
    case ChartType::CDSSpread: {
        auto tl = int_labels(cds_data_.tenors, "M");
        render_generic_table("CDS SPREAD CURVES (bps)", cds_data_.z, cds_data_.entities, tl, "Entity", "%.0f");
        break;
    }
    case ChartType::CreditTransition: {
        render_generic_table("CREDIT TRANSITION MATRIX (%)", credit_trans_data_.z,
                             credit_trans_data_.ratings, credit_trans_data_.to_ratings, "From", "%.1f");
        break;
    }
    case ChartType::RecoveryRate: {
        render_generic_table("RECOVERY RATE BY SECTOR (%)", recovery_data_.z,
                             recovery_data_.sectors, recovery_data_.seniorities, "Sector", "%.1f");
        break;
    }

    // --- Commodities ---
    case ChartType::CommodityForward: {
        auto ml = int_labels(cmdty_fwd_data_.contract_months, "M");
        render_generic_table("COMMODITY FORWARD CURVES", cmdty_fwd_data_.z, cmdty_fwd_data_.commodities, ml, "Commodity", "%.2f");
        break;
    }
    case ChartType::CommodityVol: {
        auto sl = float_labels(cmdty_vol_data_.strikes, "%.0f%%");
        auto el = int_labels(cmdty_vol_data_.expirations, "D");
        char title[64]; std::snprintf(title, 64, "COMMODITY VOL — %s", cmdty_vol_data_.commodity.c_str());
        render_generic_table(title, cmdty_vol_data_.z, el, sl, "DTE", "%.1f");
        break;
    }
    case ChartType::CrackSpread: {
        auto ml = int_labels(crack_data_.contract_months, "M");
        render_generic_table("CRACK/CRUSH SPREADS ($)", crack_data_.z, crack_data_.spread_types, ml, "Type", "%.1f");
        break;
    }
    case ChartType::ContangoBackwardation: {
        auto ml = int_labels(contango_data_.contract_months, "M");
        render_generic_table("CONTANGO/BACKWARDATION (% from spot)", contango_data_.z,
                             contango_data_.commodities, ml, "Commodity", "%.1f", true);
        break;
    }

    // --- Risk ---
    case ChartType::VaR: {
        auto hl = int_labels(var_data_.horizons, "D");
        auto cl = float_labels(var_data_.confidence_levels, "%.1f%%");
        render_generic_table("VALUE-AT-RISK (%)", var_data_.z, cl, hl, "Conf", "%.2f");
        break;
    }
    case ChartType::StressTestPnL: {
        render_generic_table("STRESS TEST P&L (%)", stress_data_.z,
                             stress_data_.scenarios, stress_data_.portfolios, "Scenario", "%.1f", true);
        break;
    }
    case ChartType::FactorExposure: {
        render_generic_table("FACTOR EXPOSURE", factor_data_.z,
                             factor_data_.assets, factor_data_.factors, "Asset", "%.3f", true);
        break;
    }
    case ChartType::LiquidityHeatmap: {
        auto sl = float_labels(liquidity_data_.strikes, "$%.0f");
        auto el = int_labels(liquidity_data_.expirations, "D");
        render_generic_table("LIQUIDITY — BID-ASK SPREAD", liquidity_data_.z, el, sl, "DTE", "%.2f");
        break;
    }
    case ChartType::Drawdown: {
        auto wl = int_labels(drawdown_data_.windows, "D");
        render_generic_table("MAX DRAWDOWN (%)", drawdown_data_.z, drawdown_data_.assets, wl, "Asset", "%.1f");
        break;
    }
    case ChartType::BetaSurface: {
        auto hl = int_labels(beta_data_.horizons, "D");
        render_generic_table("ROLLING BETA", beta_data_.z, beta_data_.assets, hl, "Asset", "%.3f", true);
        break;
    }
    case ChartType::ImpliedDividend: {
        auto sl = float_labels(impl_div_data_.strikes, "$%.0f");
        auto el = int_labels(impl_div_data_.expirations, "D");
        render_generic_table("IMPLIED DIVIDEND YIELD (%)", impl_div_data_.z, el, sl, "DTE", "%.2f");
        break;
    }

    // --- Macro ---
    case ChartType::InflationExpectations: {
        auto hl = int_labels(inflation_data_.horizons, "Y");
        auto dl = int_labels(inflation_data_.time_points, "");
        render_generic_table("BREAKEVEN INFLATION (%)", inflation_data_.z, dl, hl, "Day", "%.2f");
        break;
    }
    case ChartType::MonetaryPolicyPath: {
        auto ml = int_labels(monetary_data_.meetings_ahead, "");
        render_generic_table("POLICY RATE PATH (%)", monetary_data_.z,
                             monetary_data_.central_banks, ml, "Bank", "%.2f");
        break;
    }

    default: break;
    }
}

} // namespace fincept::surface
