// SKIP_UNITY_BUILD_INCLUSION — modal dialog for CSV import
#include "surface_screen.h"
#include "csv_importer.h"
#include <imgui.h>
#include <cstring>

namespace fincept::surface {

using namespace theme::colors;

// ── dispatch_csv_load ─────────────────────────────────────────────────────────
// Parses the CSV at `path` and routes to the correct per-surface loader.
bool SurfaceScreen::dispatch_csv_load(const std::string& path, std::string& err)
{
    auto rows = parse_csv_file(path, err);
    if (rows.empty()) return false;

    switch (active_chart_) {
    case ChartType::Volatility:
        return load_vol_surface(rows, vol_data_, err);
    case ChartType::DeltaSurface:
        return load_greeks_surface(rows, delta_data_, err, "delta");
    case ChartType::GammaSurface:
        return load_greeks_surface(rows, gamma_data_, err, "gamma");
    case ChartType::VegaSurface:
        return load_greeks_surface(rows, vega_data_, err, "vega");
    case ChartType::ThetaSurface:
        return load_greeks_surface(rows, theta_data_, err, "theta");
    case ChartType::SkewSurface:
        return load_skew_surface(rows, skew_data_, err);
    case ChartType::LocalVolSurface:
        return load_local_vol(rows, local_vol_data_, err);
    case ChartType::YieldCurve:
        return load_yield_curve(rows, yield_data_, err);
    case ChartType::SwaptionVol:
        return load_swaption_vol(rows, swaption_data_, err);
    case ChartType::CapFloorVol:
        return load_capfloor_vol(rows, capfloor_data_, err);
    case ChartType::BondSpread:
        return load_bond_spread(rows, bond_spread_data_, err);
    case ChartType::OISBasis:
        return load_ois_basis(rows, ois_data_, err);
    case ChartType::RealYield:
        return load_real_yield(rows, real_yield_data_, err);
    case ChartType::ForwardRate:
        return load_forward_rate(rows, fwd_rate_data_, err);
    case ChartType::FXVol:
        return load_fx_vol(rows, fx_vol_data_, err);
    case ChartType::FXForwardPoints:
        return load_fx_forward(rows, fx_fwd_data_, err);
    case ChartType::CrossCurrencyBasis:
        return load_xccy_basis(rows, xccy_data_, err);
    case ChartType::CDSSpread:
        return load_cds_spread(rows, cds_data_, err);
    case ChartType::CreditTransition:
        return load_credit_trans(rows, credit_trans_data_, err);
    case ChartType::RecoveryRate:
        return load_recovery_rate(rows, recovery_data_, err);
    case ChartType::CommodityForward:
        return load_cmdty_forward(rows, cmdty_fwd_data_, err);
    case ChartType::CommodityVol:
        return load_cmdty_vol(rows, cmdty_vol_data_, err);
    case ChartType::CrackSpread:
        return load_crack_spread(rows, crack_data_, err);
    case ChartType::ContangoBackwardation:
        return load_contango(rows, contango_data_, err);
    case ChartType::Correlation:
        return load_correlation(rows, corr_data_, err);
    case ChartType::PCA:
        return load_pca(rows, pca_data_, err);
    case ChartType::VaR:
        return load_var(rows, var_data_, err);
    case ChartType::StressTestPnL:
        return load_stress_test(rows, stress_data_, err);
    case ChartType::FactorExposure:
        return load_factor_exposure(rows, factor_data_, err);
    case ChartType::LiquidityHeatmap:
        return load_liquidity(rows, liquidity_data_, err);
    case ChartType::Drawdown:
        return load_drawdown(rows, drawdown_data_, err);
    case ChartType::BetaSurface:
        return load_beta(rows, beta_data_, err);
    case ChartType::ImpliedDividend:
        return load_implied_div(rows, impl_div_data_, err);
    case ChartType::InflationExpectations:
        return load_inflation(rows, inflation_data_, err);
    case ChartType::MonetaryPolicyPath:
        return load_monetary(rows, monetary_data_, err);
    default:
        err = "No CSV loader for this surface.";
        return false;
    }
}

// ── render_csv_import_dialog ──────────────────────────────────────────────────
void SurfaceScreen::render_csv_import_dialog()
{
    if (!show_import_dialog_) return;

    ImGui::OpenPopup("Import CSV##surface_import");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);

    ImGuiWindowFlags modal_flags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::BeginPopupModal("Import CSV##surface_import", &show_import_dialog_, modal_flags)) {
        return;
    }

    const CsvSchema& schema = get_csv_schema(active_chart_);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Header ────────────────────────────────────────────────────────────────
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 32.0f),
                          ImGui::GetColorU32(ImVec4(0.12f, 0.16f, 0.22f, 1.0f)));
        dl->AddLine(ImVec2(p.x, p.y + 32.0f), ImVec2(p.x + w, p.y + 32.0f),
                    ImGui::GetColorU32(ACCENT), 1.5f);
        ImGui::Dummy(ImVec2(w, 8.0f));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::TextColored(ACCENT, "IMPORT CSV");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "—  %s", schema.surface_name);
        ImGui::Dummy(ImVec2(w, 4.0f));
    }

    ImGui::Spacing();

    // ── Schema reference ──────────────────────────────────────────────────────
    ImGui::TextColored(TEXT_SECONDARY, "Required columns (header row must match exactly):");
    ImGui::Spacing();

    float col_w = ImGui::GetContentRegionAvail().x;
    ImVec2 table_origin = ImGui::GetCursorScreenPos();
    float row_h = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
    float total_table_h = (float)(schema.columns.size() + 1) * row_h + 4.0f;

    // Table background
    dl->AddRectFilled(table_origin,
                      ImVec2(table_origin.x + col_w, table_origin.y + total_table_h),
                      ImGui::GetColorU32(ImVec4(0.08f, 0.10f, 0.14f, 1.0f)), 4.0f);

    ImGui::Dummy(ImVec2(col_w, total_table_h));
    ImGui::SetCursorScreenPos(ImVec2(table_origin.x + 8.0f, table_origin.y + 4.0f));

    // Header row
    float name_col_x  = table_origin.x + 8.0f;
    float desc_col_x  = table_origin.x + col_w * 0.34f;
    float cur_y = table_origin.y + 4.0f;

    dl->AddText(ImVec2(name_col_x, cur_y), ImGui::GetColorU32(TEXT_DIM), "COLUMN");
    dl->AddText(ImVec2(desc_col_x, cur_y), ImGui::GetColorU32(TEXT_DIM), "DESCRIPTION");
    cur_y += row_h;
    dl->AddLine(ImVec2(table_origin.x + 4.0f, cur_y - 2.0f),
                ImVec2(table_origin.x + col_w - 4.0f, cur_y - 2.0f),
                ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.30f, 1.0f)));

    // Column rows
    for (const auto& col : schema.columns) {
        dl->AddText(ImVec2(name_col_x, cur_y), ImGui::GetColorU32(MARKET_GREEN), col.name);
        dl->AddText(ImVec2(desc_col_x, cur_y), ImGui::GetColorU32(TEXT_SECONDARY), col.description);
        cur_y += row_h;
    }

    ImGui::Spacing();

    // Example row
    ImGui::TextColored(TEXT_DIM, "Example row:  ");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_SECONDARY, "%s", schema.example_row);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── File path input ───────────────────────────────────────────────────────
    ImGui::TextColored(TEXT_SECONDARY, "CSV file path:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImVec4(0.10f, 0.12f, 0.16f, 1.0f)));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(TEXT_PRIMARY));
    ImGui::InputText("##csv_path", import_path_buf_, sizeof(import_path_buf_));
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── Error / success messages ──────────────────────────────────────────────
    if (!import_error_.empty()) {
        ImGui::TextColored(MARKET_RED, "Error: %s", import_error_.c_str());
        ImGui::Spacing();
    }
    if (import_success_) {
        ImGui::TextColored(MARKET_GREEN, "Data loaded successfully.");
        ImGui::Spacing();
    }

    // ── Buttons ───────────────────────────────────────────────────────────────
    float btn_w = 110.0f;
    float avail  = ImGui::GetContentRegionAvail().x;

    // LOAD button
    {
        ImVec2 c = ImGui::GetCursorScreenPos();
        ImVec2 mn = c, mx = ImVec2(c.x + btn_w, c.y + 26.0f);
        bool hov = ImGui::IsMouseHoveringRect(mn, mx);
        dl->AddRectFilled(mn, mx,
            ImGui::GetColorU32(hov ? ImVec4(0.20f, 0.50f, 0.95f, 1.0f)
                                   : ImVec4(0.14f, 0.38f, 0.78f, 1.0f)), 3.0f);
        float tw = ImGui::CalcTextSize("LOAD").x;
        dl->AddText(ImVec2(mn.x + (btn_w - tw) * 0.5f,
                           mn.y + (26.0f - ImGui::GetTextLineHeight()) * 0.5f),
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "LOAD");
        ImGui::InvisibleButton("##csv_load", ImVec2(btn_w, 26.0f));
        if (ImGui::IsItemClicked()) {
            import_error_.clear();
            import_success_ = false;
            std::string path(import_path_buf_);
            if (path.empty()) {
                import_error_ = "Please enter a file path.";
            } else {
                std::string err;
                if (dispatch_csv_load(path, err)) {
                    import_success_ = true;
                    show_import_dialog_ = false;
                    ImGui::CloseCurrentPopup();
                } else {
                    import_error_ = err;
                }
            }
        }
    }

    ImGui::SameLine(0, 8);

    // CANCEL button
    {
        ImVec2 c = ImGui::GetCursorScreenPos();
        ImVec2 mn = c, mx = ImVec2(c.x + btn_w, c.y + 26.0f);
        bool hov = ImGui::IsMouseHoveringRect(mn, mx);
        dl->AddRectFilled(mn, mx,
            ImGui::GetColorU32(hov ? ImVec4(0.28f, 0.28f, 0.32f, 1.0f)
                                   : ImVec4(0.18f, 0.18f, 0.22f, 1.0f)), 3.0f);
        float tw = ImGui::CalcTextSize("CANCEL").x;
        dl->AddText(ImVec2(mn.x + (btn_w - tw) * 0.5f,
                           mn.y + (26.0f - ImGui::GetTextLineHeight()) * 0.5f),
                    ImGui::GetColorU32(TEXT_SECONDARY), "CANCEL");
        ImGui::InvisibleButton("##csv_cancel", ImVec2(btn_w, 26.0f));
        if (ImGui::IsItemClicked()) {
            show_import_dialog_ = false;
            import_error_.clear();
            import_success_ = false;
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::Spacing();
    ImGui::EndPopup();
}

} // namespace fincept::surface
