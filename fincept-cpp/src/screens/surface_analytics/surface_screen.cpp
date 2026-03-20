#include "surface_screen.h"
#include "demo_data.h"
#include "csv_importer.h"
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

// Category accent colors for visual distinction
static const ImVec4 CAT_COLORS[] = {
    {0.20f, 0.60f, 1.00f, 1.0f},  // EQUITY DERIV  — blue
    {0.40f, 0.85f, 0.60f, 1.0f},  // FIXED INCOME  — green
    {1.00f, 0.75f, 0.20f, 1.0f},  // FX            — gold
    {0.90f, 0.35f, 0.35f, 1.0f},  // CREDIT        — red
    {0.75f, 0.55f, 1.00f, 1.0f},  // COMMODITIES   — purple
    {1.00f, 0.55f, 0.15f, 1.0f},  // RISK          — orange
    {0.35f, 0.85f, 0.95f, 1.0f},  // MACRO         — cyan
};

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

    // Two-row header: category tabs (28px) + surface chips (28px)
    render_control_bar();

    float content_w = frame.width();
    float header_h  = 28.0f + 4.0f + 28.0f + 4.0f; // two rows + gaps
    float content_h = frame.height() - header_h;

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

    // CSV import modal (rendered over everything else)
    render_csv_import_dialog();

    frame.end();
}

// ============================================================================
void SurfaceScreen::render_control_bar() {
    const auto categories = get_surface_categories();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Row 1: Category tab bar ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##sa_catbar", ImVec2(0, 28), ImGuiChildFlags_None);

    ImVec2 row1_origin = ImGui::GetCursorScreenPos();
    ImGui::SetCursorPos(ImVec2(8, 4));

    for (int i = 0; i < (int)categories.size(); i++) {
        const ImVec4& cat_col = CAT_COLORS[i];
        bool active = (i == active_category_);

        // Measure label width
        float lw = ImGui::CalcTextSize(categories[i].name).x;
        float bw = lw + 18.0f;
        float bh = 20.0f;

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 btn_min = cursor;
        ImVec2 btn_max = ImVec2(cursor.x + bw, cursor.y + bh);

        // Background fill
        if (active) {
            dl->AddRectFilled(btn_min, btn_max,
                ImGui::GetColorU32(ImVec4(cat_col.x*0.18f, cat_col.y*0.18f, cat_col.z*0.18f, 1.0f)));
            // Active bottom accent line
            dl->AddLine(ImVec2(btn_min.x, btn_max.y), btn_max,
                ImGui::GetColorU32(cat_col), 2.5f);
        } else {
            bool hov = ImGui::IsMouseHoveringRect(btn_min, btn_max);
            if (hov)
                dl->AddRectFilled(btn_min, btn_max, ImGui::GetColorU32(BG_HOVER));
        }

        // Invisible button for interaction
        ImGui::InvisibleButton(categories[i].name, ImVec2(bw, bh));
        if (ImGui::IsItemClicked()) {
            active_category_ = i;
            active_chart_ = categories[i].types[0];
        }

        // Draw text centered in button
        ImVec4 text_col = active ? cat_col : TEXT_DIM;
        ImVec2 text_pos = ImVec2(
            btn_min.x + (bw - ImGui::CalcTextSize(categories[i].name).x) * 0.5f,
            btn_min.y + (bh - ImGui::GetTextLineHeight()) * 0.5f
        );
        dl->AddText(text_pos, ImGui::GetColorU32(text_col), categories[i].name);

        ImGui::SameLine(0, 2);
    }

    // Right-aligned: IMPORT + view toggle + refresh
    float avail_x = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(0, avail_x - 224);

    // IMPORT button
    {
        float iw = ImGui::CalcTextSize("IMPORT").x + 14.0f;
        ImVec2 ic = ImGui::GetCursorScreenPos();
        ImVec2 imn = ic, imx = ImVec2(ic.x + iw, ic.y + 20.0f);
        bool ihov = ImGui::IsMouseHoveringRect(imn, imx);
        dl->AddRectFilled(imn, imx,
            ImGui::GetColorU32(ihov ? ImVec4(0.18f, 0.42f, 0.80f, 1.0f)
                                    : ImVec4(0.12f, 0.30f, 0.60f, 1.0f)), 2.0f);
        dl->AddRect(imn, imx, ImGui::GetColorU32(ImVec4(0.25f, 0.55f, 1.0f, 0.6f)), 2.0f);
        float itw = ImGui::CalcTextSize("IMPORT").x;
        dl->AddText(ImVec2(imn.x + (iw - itw) * 0.5f,
                           imn.y + (20.0f - ImGui::GetTextLineHeight()) * 0.5f),
                    ImGui::GetColorU32(ImVec4(0.55f, 0.80f, 1.0f, 1.0f)), "IMPORT");
        ImGui::InvisibleButton("##import_csv", ImVec2(iw, 20.0f));
        if (ImGui::IsItemClicked()) {
            show_import_dialog_ = true;
            import_error_.clear();
            import_success_ = false;
        }
    }
    ImGui::SameLine(0, 8);

    // 3D / TABLE toggle as segmented control
    auto seg_btn = [&](const char* label, bool selected, ImVec4 accent) {
        float tw = ImGui::CalcTextSize(label).x + 14.0f;
        ImVec2 c = ImGui::GetCursorScreenPos();
        ImVec2 mn = c, mx = ImVec2(c.x + tw, c.y + 20.0f);
        bool hov = ImGui::IsMouseHoveringRect(mn, mx);
        ImU32 bg = selected
            ? ImGui::GetColorU32(ImVec4(accent.x*0.25f, accent.y*0.25f, accent.z*0.25f, 1.0f))
            : (hov ? ImGui::GetColorU32(BG_HOVER) : ImGui::GetColorU32(BG_WIDGET));
        dl->AddRectFilled(mn, mx, bg);
        if (selected)
            dl->AddRect(mn, mx, ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.5f)));
        ImVec2 tp = ImVec2(mn.x + (tw - ImGui::CalcTextSize(label).x)*0.5f,
                           mn.y + (20.0f - ImGui::GetTextLineHeight())*0.5f);
        dl->AddText(tp, ImGui::GetColorU32(selected ? accent : TEXT_DIM), label);
        ImGui::InvisibleButton(label, ImVec2(tw, 20.0f));
        return ImGui::IsItemClicked();
    };

    const ImVec4& cat_accent = CAT_COLORS[active_category_];
    if (seg_btn("3D", !show_table_, cat_accent))   show_table_ = false;
    ImGui::SameLine(0, 2);
    if (seg_btn("TABLE", show_table_, cat_accent))  show_table_ = true;
    ImGui::SameLine(0, 8);

    // Refresh button
    {
        float rw = ImGui::CalcTextSize("REFRESH").x + 14.0f;
        ImVec2 rc = ImGui::GetCursorScreenPos();
        ImVec2 rmn = rc, rmx = ImVec2(rc.x+rw, rc.y+20.0f);
        bool rhov = ImGui::IsMouseHoveringRect(rmn, rmx);
        dl->AddRectFilled(rmn, rmx, ImGui::GetColorU32(rhov ? ACCENT_DIM : BG_WIDGET));
        dl->AddText(ImVec2(rmn.x+7, rmn.y+(20.0f-ImGui::GetTextLineHeight())*0.5f),
                    ImGui::GetColorU32(ACCENT), "REFRESH");
        ImGui::InvisibleButton("##refresh", ImVec2(rw, 20.0f));
        if (ImGui::IsItemClicked()) load_demo_data();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Divider
    ImGui::Spacing();

    // ── Row 2: Surface chip buttons ─────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##sa_surfbar", ImVec2(0, 28), ImGuiChildFlags_None);
    ImGui::SetCursorPos(ImVec2(8, 4));

    const auto& cat = categories[active_category_];
    const ImVec4& cc = CAT_COLORS[active_category_];

    for (int i = 0; i < (int)cat.types.size(); i++) {
        bool act = (active_chart_ == cat.types[i]);
        const char* lbl = chart_type_name(cat.types[i]);
        float lw2 = ImGui::CalcTextSize(lbl).x + 16.0f;

        ImVec2 cur = ImGui::GetCursorScreenPos();
        ImVec2 mn2 = cur, mx2 = ImVec2(cur.x + lw2, cur.y + 20.0f);
        bool hov2 = ImGui::IsMouseHoveringRect(mn2, mx2);

        ImU32 bg2 = act
            ? ImGui::GetColorU32(ImVec4(cc.x*0.22f, cc.y*0.22f, cc.z*0.22f, 1.0f))
            : (hov2 ? ImGui::GetColorU32(BG_HOVER) : ImGui::GetColorU32(BG_WIDGET));
        dl->AddRectFilled(mn2, mx2, bg2, 3.0f);
        if (act)
            dl->AddRect(mn2, mx2, ImGui::GetColorU32(ImVec4(cc.x, cc.y, cc.z, 0.7f)), 3.0f, 0, 1.2f);

        ImVec2 tp2 = ImVec2(mn2.x + (lw2 - ImGui::CalcTextSize(lbl).x)*0.5f,
                            mn2.y + (20.0f - ImGui::GetTextLineHeight())*0.5f);
        ImU32 tc2 = act ? ImGui::GetColorU32(cc) : ImGui::GetColorU32(hov2 ? TEXT_SECONDARY : TEXT_DIM);
        dl->AddText(tp2, tc2, lbl);

        ImGui::InvisibleButton(lbl, ImVec2(lw2, 20.0f));
        if (ImGui::IsItemClicked()) active_chart_ = cat.types[i];

        ImGui::SameLine(0, 4);
    }

    // Symbol selector for equity-linked surfaces (right-aligned)
    bool needs_symbol = (active_category_ == 0); // EQUITY DERIV
    if (needs_symbol) {
        float ra = ImGui::GetContentRegionAvail().x;
        // rough width: combo is ~90px
        ImGui::SameLine(0, ra - 92.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        ImGui::PushItemWidth(88);
        int prev = selected_symbol_;
        if (ImGui::BeginCombo("##sym", VOL_SYMBOLS[selected_symbol_], ImGuiComboFlags_NoArrowButton)) {
            for (int s = 0; s < N_SYMBOLS; s++) {
                if (ImGui::Selectable(VOL_SYMBOLS[s], s == selected_symbol_))
                    selected_symbol_ = s;
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleColor(2);
        if (selected_symbol_ != prev) load_demo_data();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Helper: draw a filled badge with text
static void draw_badge(const char* text, ImVec4 bg_col, ImVec4 text_col) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cur = ImGui::GetCursorScreenPos();
    float tw = ImGui::CalcTextSize(text).x;
    float bw = tw + 10.0f, bh = ImGui::GetTextLineHeight() + 4.0f;
    dl->AddRectFilled(cur, ImVec2(cur.x + bw, cur.y + bh),
                      ImGui::GetColorU32(bg_col), 3.0f);
    dl->AddText(ImVec2(cur.x + 5.0f, cur.y + 2.0f),
                ImGui::GetColorU32(text_col), text);
    ImGui::Dummy(ImVec2(bw, bh));
}

// Helper: colored metric row with value badge
static void metric_row(const char* label, const char* value, ImVec4 val_col, float label_w) {
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::SameLine(label_w);
    ImGui::TextColored(val_col, "%s", value);
}

// Helper: thick separator with color tint
static void colored_separator(ImVec4 col) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    dl->AddLine(p, ImVec2(p.x + w, p.y),
                ImGui::GetColorU32(ImVec4(col.x, col.y, col.z, 0.45f)), 1.5f);
    ImGui::Dummy(ImVec2(w, 2.0f));
}

void SurfaceScreen::render_metrics_panel() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    const auto categories = get_surface_categories();
    const ImVec4& cat_col = CAT_COLORS[active_category_];

    // Panel header — category color accent bar + surface name
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        // Left accent stripe
        dl->AddRectFilled(p0, ImVec2(p0.x + 3.0f, p0.y + 48.0f),
                          ImGui::GetColorU32(cat_col));
        // Header bg
        dl->AddRectFilled(ImVec2(p0.x + 3.0f, p0.y),
                          ImVec2(p0.x + w, p0.y + 48.0f),
                          ImGui::GetColorU32(ImVec4(cat_col.x*0.08f, cat_col.y*0.08f, cat_col.z*0.08f, 1.0f)));
        ImGui::Dummy(ImVec2(0, 0));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
        ImGui::TextColored(cat_col, "%s", categories[active_category_].name);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::TextColored(TEXT_PRIMARY, "%s", chart_type_name(active_chart_));
        ImGui::Dummy(ImVec2(0, 4.0f));
    }

    colored_separator(cat_col);
    ImGui::Spacing();

    float panel_w = ImGui::GetContentRegionAvail().x;
    float label_w = panel_w * 0.50f;
    char b[64];

    // Metric dispatch
    switch (active_chart_) {
    case ChartType::Volatility: {
        if (vol_data_.z.empty()) break;
        int me = (int)vol_data_.z.size()/2, ms = (int)vol_data_.strikes.size()/2;
        float atm = vol_data_.z[me][ms];
        int op = (int)(vol_data_.strikes.size()*0.2f), oc = (int)(vol_data_.strikes.size()*0.8f);
        float skew = vol_data_.z[me][op] - vol_data_.z[me][oc];

        std::snprintf(b,64,"%.1f%%",atm);  metric_row("ATM VOL",b,TEXT_PRIMARY,label_w);
        std::snprintf(b,64,"%.1f%%",skew); metric_row("SKEW",b, skew>0?MARKET_GREEN:MARKET_RED, label_w);
        std::snprintf(b,64,"$%.2f",vol_data_.spot_price); metric_row("SPOT",b,TEXT_PRIMARY,label_w);
        std::snprintf(b,64,"%d",(int)(vol_data_.strikes.size()*vol_data_.expirations.size()));
        metric_row("DATA PTS",b,TEXT_SECONDARY,label_w);

        ImGui::Spacing(); colored_separator(cat_col); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "UNDERLYING");
        ImGui::SameLine(label_w); ImGui::TextColored(cat_col, "%s", vol_data_.underlying.c_str());
        std::snprintf(b,64,"%.0f - %.0f", vol_data_.strikes.front(), vol_data_.strikes.back());
        metric_row("STRIKES",b,TEXT_SECONDARY,label_w);
        std::snprintf(b,64,"%dD - %dD", vol_data_.expirations.front(), vol_data_.expirations.back());
        metric_row("EXPIRIES",b,TEXT_SECONDARY,label_w);
        break;
    }
    case ChartType::DeltaSurface: case ChartType::GammaSurface:
    case ChartType::VegaSurface: case ChartType::ThetaSurface:
    case ChartType::SkewSurface: case ChartType::LocalVolSurface:
        render_metrics_derivatives();
        break;

    case ChartType::YieldCurve: {
        if (yield_data_.z.empty()) break;
        const auto& last = yield_data_.z.back();
        float y2=last.size()>4?last[4]:0.0f, y10=last.size()>8?last[8]:0.0f, sp=y10-y2;
        std::snprintf(b,64,"%.2f%%",y2);  metric_row("2Y YIELD",b,TEXT_PRIMARY,label_w);
        std::snprintf(b,64,"%.2f%%",y10); metric_row("10Y YIELD",b,TEXT_PRIMARY,label_w);
        std::snprintf(b,64,"%.2f%%",sp);  metric_row("2-10 SPREAD",b, sp<0?MARKET_RED:MARKET_GREEN, label_w);

        ImGui::Spacing(); colored_separator(cat_col); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "SHAPE");
        ImGui::SameLine(label_w);
        if (sp < 0)      draw_badge("INVERTED", ImVec4(0.5f,0.05f,0.05f,1), MARKET_RED);
        else if (sp<0.5f) draw_badge("FLAT",    ImVec4(0.4f,0.35f,0.0f,1), WARNING);
        else              draw_badge("NORMAL",  ImVec4(0.05f,0.3f,0.05f,1), MARKET_GREEN);
        std::snprintf(b,64,"%d",(int)yield_data_.maturities.size());
        metric_row("TENORS",b,TEXT_SECONDARY,label_w);
        break;
    }
    case ChartType::SwaptionVol: case ChartType::CapFloorVol:
    case ChartType::BondSpread: case ChartType::OISBasis:
    case ChartType::RealYield: case ChartType::ForwardRate:
        render_metrics_fixed_income();
        break;

    case ChartType::FXVol: case ChartType::FXForwardPoints:
    case ChartType::CrossCurrencyBasis:
        render_metrics_fx();
        break;

    case ChartType::CDSSpread: case ChartType::CreditTransition:
    case ChartType::RecoveryRate:
        render_metrics_credit();
        break;

    case ChartType::CommodityForward: case ChartType::CommodityVol:
    case ChartType::CrackSpread: case ChartType::ContangoBackwardation:
        render_metrics_commodities();
        break;

    case ChartType::Correlation: {
        if (corr_data_.z.empty()) break;
        const auto& last = corr_data_.z.back();
        int n = (int)corr_data_.assets.size();
        float sum=0.0f, cmx=-2.0f, cmn=2.0f; int cnt=0;
        for (int i=0;i<n;i++) for (int j=0;j<n;j++) if (i!=j) {
            float c=last[i*n+j]; sum+=c; cnt++;
            cmx = c > cmx ? c : cmx; cmn = c < cmn ? c : cmn;
        }
        float avg = cnt>0 ? sum/(float)cnt : 0.0f;
        std::snprintf(b,64,"%.3f",avg);  metric_row("AVG CORR",b, avg>0.5f?MARKET_RED:MARKET_GREEN, label_w);
        std::snprintf(b,64,"%.3f",cmx);  metric_row("MAX CORR",b, cmx>0.8f?MARKET_RED:TEXT_PRIMARY, label_w);
        std::snprintf(b,64,"%.3f",cmn);  metric_row("MIN CORR",b, cmn<0.0f?INFO:TEXT_PRIMARY, label_w);
        std::snprintf(b,64,"%d",n);      metric_row("ASSETS",b,TEXT_SECONDARY,label_w);
        std::snprintf(b,64,"%dD",corr_data_.window); metric_row("WINDOW",b,TEXT_SECONDARY,label_w);
        break;
    }
    case ChartType::PCA: {
        if (pca_data_.variance_explained.empty()) break;
        int np = (int)pca_data_.variance_explained.size();
        for (int i=0; i<3 && i<np; i++) {
            char lbl[16]; std::snprintf(lbl,16,"PC%d VAR",i+1);
            std::snprintf(b,64,"%.1f%%",pca_data_.variance_explained[i]*100.0f);
            metric_row(lbl, b, i==0?MARKET_GREEN:TEXT_PRIMARY, label_w);
        }
        float tot=0.0f;
        for (int i=0; i<3 && i<np; i++) tot+=pca_data_.variance_explained[i];
        std::snprintf(b,64,"%.1f%%",tot*100.0f); metric_row("TOP 3 CUM",b, tot>0.85f?MARKET_GREEN:WARNING, label_w);
        std::snprintf(b,64,"%d",(int)pca_data_.assets.size()); metric_row("ASSETS",b,TEXT_SECONDARY,label_w);
        break;
    }
    case ChartType::VaR: case ChartType::StressTestPnL:
    case ChartType::FactorExposure: case ChartType::LiquidityHeatmap:
    case ChartType::Drawdown: case ChartType::BetaSurface:
    case ChartType::ImpliedDividend:
        render_metrics_risk();
        break;

    case ChartType::InflationExpectations: case ChartType::MonetaryPolicyPath:
        render_metrics_macro();
        break;
    }

    // Controls hint (3D mode only) — at bottom of panel
    if (!show_table_) {
        ImGui::Spacing(); colored_separator(ImVec4(0.3f,0.3f,0.35f,1)); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "INTERACT");
        ImGui::TextColored(TEXT_DISABLED, "  Drag  Rotate");
        ImGui::TextColored(TEXT_DISABLED, "  Scroll  Zoom");
    }

    ImGui::PopStyleColor();
}

// ============================================================================
void SurfaceScreen::render_chart_area() {
    const auto categories = get_surface_categories();
    const ImVec4& cat_col = CAT_COLORS[active_category_];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // Top-left overlay: surface name + category badge
    {
        const char* surf_name = chart_type_name(active_chart_);
        const char* cat_name  = categories[active_category_].name;
        float cat_tw = ImGui::CalcTextSize(cat_name).x + 10.0f;
        float surf_tw = ImGui::CalcTextSize(surf_name).x;

        // Category pill
        ImVec2 pill_min = ImVec2(origin.x + 8, origin.y + 8);
        ImVec2 pill_max = ImVec2(pill_min.x + cat_tw, pill_min.y + 18.0f);
        dl->AddRectFilled(pill_min, pill_max,
            ImGui::GetColorU32(ImVec4(cat_col.x*0.25f, cat_col.y*0.25f, cat_col.z*0.25f, 0.95f)), 3.0f);
        dl->AddRect(pill_min, pill_max,
            ImGui::GetColorU32(ImVec4(cat_col.x, cat_col.y, cat_col.z, 0.4f)), 3.0f);
        dl->AddText(ImVec2(pill_min.x + 5.0f, pill_min.y + 2.0f),
            ImGui::GetColorU32(cat_col), cat_name);

        // Surface name
        dl->AddText(ImVec2(pill_max.x + 8.0f, pill_min.y + 2.0f),
            ImGui::GetColorU32(TEXT_PRIMARY), surf_name);

        // View mode badge (top right)
        const char* view_lbl = show_table_ ? "TABLE VIEW" : "3D SURFACE";
        float vw = ImGui::CalcTextSize(view_lbl).x + 12.0f;
        ImVec2 vp_min = ImVec2(origin.x + avail.x - vw - 48.0f, origin.y + 8.0f);
        ImVec2 vp_max = ImVec2(vp_min.x + vw, vp_min.y + 18.0f);
        dl->AddRectFilled(vp_min, vp_max, ImGui::GetColorU32(BG_WIDGET), 3.0f);
        dl->AddText(ImVec2(vp_min.x + 6.0f, vp_min.y + 2.0f),
            ImGui::GetColorU32(TEXT_DIM), view_lbl);
    }

    // Bottom status strip
    {
        float strip_h = 20.0f;
        ImVec2 sm = ImVec2(origin.x, origin.y + avail.y - strip_h);
        ImVec2 sx = ImVec2(origin.x + avail.x, origin.y + avail.y);
        dl->AddRectFilled(sm, sx, ImGui::GetColorU32(BG_DARKEST));
        dl->AddLine(sm, ImVec2(sx.x, sm.y),
            ImGui::GetColorU32(ImVec4(cat_col.x, cat_col.y, cat_col.z, 0.3f)));

        // Status text
        char status[128];
        std::snprintf(status, 128, "FINCEPT SURFACE ANALYTICS  |  %s  |  %s  |  DEMO DATA",
            categories[active_category_].name, chart_type_name(active_chart_));
        dl->AddText(ImVec2(sm.x + 8.0f, sm.y + 3.0f),
            ImGui::GetColorU32(TEXT_DISABLED), status);
    }

    if (show_table_) {
        ImGui::SetCursorPos(ImVec2(8, 30));
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
