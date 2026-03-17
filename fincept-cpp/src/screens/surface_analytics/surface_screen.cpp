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
static constexpr int N_SYMBOLS = 7;

// ============================================================================
void SurfaceScreen::load_demo_data() {
    srand((unsigned)time(nullptr));

    // Equity Derivatives
    vol_data_ = generate_vol_surface(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    delta_data_ = generate_delta_surface(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    gamma_data_ = generate_gamma_surface(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    vega_data_ = generate_vega_surface(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    theta_data_ = generate_theta_surface(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    skew_data_ = generate_skew_surface(VOL_SYMBOLS[selected_symbol_]);
    local_vol_data_ = generate_local_vol(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);

    // Fixed Income
    yield_data_ = generate_yield_curve();
    swaption_data_ = generate_swaption_vol();
    capfloor_data_ = generate_capfloor_vol();
    bond_spread_data_ = generate_bond_spread();
    ois_data_ = generate_ois_basis();
    real_yield_data_ = generate_real_yield();
    fwd_rate_data_ = generate_forward_rate();

    // FX
    fx_vol_data_ = generate_fx_vol();
    fx_fwd_data_ = generate_fx_forward_points();
    xccy_data_ = generate_xccy_basis();

    // Credit
    cds_data_ = generate_cds_spread();
    credit_trans_data_ = generate_credit_transition();
    recovery_data_ = generate_recovery_rate();

    // Commodities
    cmdty_fwd_data_ = generate_commodity_forward();
    cmdty_vol_data_ = generate_commodity_vol();
    crack_data_ = generate_crack_spread();
    contango_data_ = generate_contango();

    // Risk & Portfolio
    corr_data_ = generate_correlation(corr_assets_);
    pca_data_ = generate_pca(corr_assets_);
    var_data_ = generate_var();
    stress_data_ = generate_stress_test();
    factor_data_ = generate_factor_exposure(corr_assets_);
    liquidity_data_ = generate_liquidity(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    drawdown_data_ = generate_drawdown(corr_assets_);
    beta_data_ = generate_beta(corr_assets_);
    impl_div_data_ = generate_implied_dividend(VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);

    // Macro
    inflation_data_ = generate_inflation_expectations();
    monetary_data_ = generate_monetary_policy();

    data_loaded_ = true;
}

// ============================================================================
void SurfaceScreen::render() {
    if (!data_loaded_) load_demo_data();

    ui::ScreenFrame frame("##surface_analytics", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    auto vstack = ui::vstack_layout(frame.width(), frame.height(), {36, -1});

    render_control_bar();

    float content_w = frame.width();
    float content_h = vstack.heights[1];

    if (frame.is_compact()) {
        ImGui::BeginChild("##sa_chart", ImVec2(content_w, content_h), ImGuiChildFlags_Borders);
        render_chart_area();
        ImGui::EndChild();
    } else {
        auto panels = ui::two_panel_layout(content_w, content_h, true,
            frame.is_medium() ? 20 : 15, 150, 200);

        ImGui::BeginChild("##sa_metrics", ImVec2(panels.side_w, panels.side_h), ImGuiChildFlags_Borders);
        render_metrics_panel();
        ImGui::EndChild();

        ImGui::SameLine();

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

    const auto categories = get_surface_categories();

    // Category dropdown
    ImGui::PushItemWidth(120);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    if (ImGui::BeginCombo("##cat", categories[active_category_].name, ImGuiComboFlags_NoArrowButton)) {
        for (int i = 0; i < (int)categories.size(); i++) {
            if (ImGui::Selectable(categories[i].name, i == active_category_)) {
                active_category_ = i;
                active_chart_ = categories[i].types[0];
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 8);

    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    // Surface buttons within category
    const auto& cat = categories[active_category_];
    for (int i = 0; i < (int)cat.types.size(); i++) {
        bool act = (active_chart_ == cat.types[i]);
        ImGui::PushStyleColor(ImGuiCol_Button, act ? ACCENT_DIM : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, act ? ImVec4(1,1,1,1) : TEXT_SECONDARY);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, act ? ACCENT : BG_HOVER);
        if (ImGui::SmallButton(chart_type_name(cat.types[i])))
            active_chart_ = cat.types[i];
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 4);
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    // View toggle
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

    ImGui::SameLine(0, 12);
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
    ImGui::TextColored(ACCENT, "%s", chart_type_name(active_chart_));
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
    // Equity Derivatives (excluding Volatility handled above)
    case ChartType::DeltaSurface: case ChartType::GammaSurface:
    case ChartType::VegaSurface: case ChartType::ThetaSurface:
    case ChartType::SkewSurface: case ChartType::LocalVolSurface:
        render_metrics_derivatives();
        break;

    // Fixed Income
    case ChartType::YieldCurve: {
        if (yield_data_.z.empty()) break;
        const auto& last = yield_data_.z.back();
        float y2=last.size()>4?last[4]:0, y10=last.size()>8?last[8]:0, sp=y10-y2;
        std::snprintf(b,64,"%.2f%%",y2); row("2Y YIELD",b,TEXT_PRIMARY);
        std::snprintf(b,64,"%.2f%%",y10); row("10Y YIELD",b,TEXT_PRIMARY);
        std::snprintf(b,64,"%.2f%%",sp); row("2-10 SPREAD",b, sp<0?MARKET_RED:MARKET_GREEN);
        row("CURVE", sp<0?"INVERTED":"NORMAL", sp<0?MARKET_RED:MARKET_GREEN);
        std::snprintf(b,64,"%d",(int)yield_data_.maturities.size()); row("TENORS",b,TEXT_SECONDARY);
        break;
    }
    case ChartType::SwaptionVol: case ChartType::CapFloorVol:
    case ChartType::BondSpread: case ChartType::OISBasis:
    case ChartType::RealYield: case ChartType::ForwardRate:
        render_metrics_fixed_income();
        break;

    // FX
    case ChartType::FXVol: case ChartType::FXForwardPoints:
    case ChartType::CrossCurrencyBasis:
        render_metrics_fx();
        break;

    // Credit
    case ChartType::CDSSpread: case ChartType::CreditTransition:
    case ChartType::RecoveryRate:
        render_metrics_credit();
        break;

    // Commodities
    case ChartType::CommodityForward: case ChartType::CommodityVol:
    case ChartType::CrackSpread: case ChartType::ContangoBackwardation:
        render_metrics_commodities();
        break;

    // Risk
    case ChartType::Correlation: {
        if (corr_data_.z.empty()) break;
        const auto& last = corr_data_.z.back();
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
    case ChartType::VaR: case ChartType::StressTestPnL:
    case ChartType::FactorExposure: case ChartType::LiquidityHeatmap:
    case ChartType::Drawdown: case ChartType::BetaSurface:
    case ChartType::ImpliedDividend:
        render_metrics_risk();
        break;

    // Macro
    case ChartType::InflationExpectations: case ChartType::MonetaryPolicyPath:
        render_metrics_macro();
        break;
    }

    if (!show_table_) {
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "CONTROLS");
        ImGui::TextColored(TEXT_SECONDARY, "  Drag: Rotate");
        ImGui::TextColored(TEXT_SECONDARY, "  Scroll: Zoom");
    }

    ImGui::PopStyleColor();
}

// ============================================================================
void SurfaceScreen::render_chart_area() {
    if (show_table_) {
        ImGui::SetCursorPos(ImVec2(8, 8));
        switch (active_chart_) {
            case ChartType::Volatility:  render_vol_table(); return;
            case ChartType::Correlation: render_corr_table(); return;
            case ChartType::YieldCurve:  render_yield_table(); return;
            case ChartType::PCA:         render_pca_table(); return;
            default: render_table_for(active_chart_); return;
        }
    }

    // 3D surface plot dispatch
    switch (active_chart_) {
    case ChartType::Volatility: {
        if (vol_data_.z.empty()) break;
        float mn=999, mx=0;
        for (const auto& r : vol_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> strike_labels, dte_labels;
        char lb[32];
        for (float s : vol_data_.strikes) { std::snprintf(lb, 32, "$%.0f", s); strike_labels.push_back(lb); }
        for (int e : vol_data_.expirations) { std::snprintf(lb, 32, "%dD", e); dte_labels.push_back(lb); }
        render_3d_surface(vol_data_.z, "STRIKE", "IV %", "DTE", mn, mx, false, &strike_labels, &dte_labels);
        break;
    }
    case ChartType::DeltaSurface: case ChartType::GammaSurface:
    case ChartType::VegaSurface: case ChartType::ThetaSurface:
    case ChartType::SkewSurface: case ChartType::LocalVolSurface:
        render_chart_derivatives();
        break;

    case ChartType::YieldCurve: {
        if (yield_data_.z.empty()) break;
        int ns = std::min(30, (int)yield_data_.z.size());
        int st = (int)yield_data_.z.size() - ns;
        std::vector<std::vector<float>> sub(yield_data_.z.begin()+st, yield_data_.z.end());
        float mn=999, mx=0;
        for (const auto& r : sub) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> mat_labels, time_labels;
        char mb[32], lb[32];
        for (int m : yield_data_.maturities) { std::snprintf(mb, 32, "%dM", m); mat_labels.push_back(mb); }
        for (int ti = 0; ti < ns; ti++) { std::snprintf(lb, 32, "D-%d", ns - ti); time_labels.push_back(lb); }
        render_3d_surface(sub, "MATURITY", "YIELD %", "TIME", mn, mx, false, &mat_labels, &time_labels);
        break;
    }
    case ChartType::SwaptionVol: case ChartType::CapFloorVol:
    case ChartType::BondSpread: case ChartType::OISBasis:
    case ChartType::RealYield: case ChartType::ForwardRate:
        render_chart_fixed_income();
        break;

    case ChartType::FXVol: case ChartType::FXForwardPoints:
    case ChartType::CrossCurrencyBasis:
        render_chart_fx();
        break;

    case ChartType::CDSSpread: case ChartType::CreditTransition:
    case ChartType::RecoveryRate:
        render_chart_credit();
        break;

    case ChartType::CommodityForward: case ChartType::CommodityVol:
    case ChartType::CrackSpread: case ChartType::ContangoBackwardation:
        render_chart_commodities();
        break;

    case ChartType::Correlation: {
        if (corr_data_.z.empty()) break;
        int n = (int)corr_data_.assets.size();
        std::vector<std::vector<float>> mat;
        const auto& flat = corr_data_.z.back();
        for (int i=0;i<n;i++) { std::vector<float> r; for(int j=0;j<n;j++) r.push_back(flat[i*n+j]); mat.push_back(r); }
        render_3d_surface(mat, "ASSET", "CORR", "ASSET", -1, 1, true, &corr_data_.assets, &corr_data_.assets);
        break;
    }
    case ChartType::PCA: {
        if (pca_data_.z.empty()) break;
        render_3d_surface(pca_data_.z, "FACTOR", "LOADING", "ASSET", -1, 1, true,
                          &pca_data_.factors, &pca_data_.assets);
        break;
    }
    case ChartType::VaR: case ChartType::StressTestPnL:
    case ChartType::FactorExposure: case ChartType::LiquidityHeatmap:
    case ChartType::Drawdown: case ChartType::BetaSurface:
    case ChartType::ImpliedDividend:
        render_chart_risk();
        break;

    case ChartType::InflationExpectations: case ChartType::MonetaryPolicyPath:
        render_chart_macro();
        break;
    }
}

} // namespace fincept::surface
