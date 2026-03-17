#include "derivatives_screen.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include "python/python_runner.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <thread>
#include <nlohmann/json.hpp>

namespace fincept::derivatives {

using namespace theme::colors;
using json = nlohmann::json;
static constexpr const char* TAG = "Derivatives";

// Layout constants (ES.45)
static constexpr float TAB_HEIGHT        = 36.0f;
static constexpr float INPUT_PANEL_WIDTH = 340.0f;
static constexpr int   PAYOFF_POINTS     = 100;
static constexpr double PI               = 3.14159265358979;

// ============================================================================
// Initialization
// ============================================================================

void DerivativesScreen::init() {
    LOG_INFO(TAG, "Initializing derivatives screen");
    compute_payoff_diagram();
    initialized_ = true;
}

// ============================================================================
// Main render
// ============================================================================

void DerivativesScreen::render() {
    if (!initialized_) init();

    ui::ScreenFrame frame("##derivatives_screen", ImVec2(0, 0), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    const float w = frame.width();
    const float h = frame.height();

    render_instrument_tabs(w);

    const float body_y = ImGui::GetCursorPosY();
    const float body_h = h - body_y;

    if (body_h > 50.0f) {
        const float left_w = INPUT_PANEL_WIDTH;
        const float right_w = w - left_w;
        render_input_panel(0.0f, body_y, left_w, body_h);
        render_results_panel(left_w, body_y, right_w, body_h);
    }

    frame.end();
}

// ============================================================================
// Instrument Tab Bar
// ============================================================================

void DerivativesScreen::render_instrument_tabs(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##deriv_tabs", ImVec2(w, TAB_HEIGHT), false);

    ImGui::SetCursorPos(ImVec2(8.0f, 4.0f));

    static constexpr InstrumentType tabs[] = {
        InstrumentType::bonds, InstrumentType::equity_options,
        InstrumentType::fx_options, InstrumentType::swaps,
        InstrumentType::credit, InstrumentType::vollib
    };

    for (const auto t : tabs) {
        const bool active = (t == active_instrument_);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
        if (ImGui::SmallButton(instrument_label(t))) {
            active_instrument_ = t;
            error_msg_.clear();
            status_msg_.clear();
            compute_payoff_diagram();
        }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Input Panel (left side)
// ============================================================================

void DerivativesScreen::render_input_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##deriv_input", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "%s PARAMETERS", instrument_label(active_instrument_));
    ImGui::Separator();

    switch (active_instrument_) {
        case InstrumentType::bonds:          render_bond_inputs(w); break;
        case InstrumentType::equity_options: render_equity_option_inputs(w); break;
        case InstrumentType::fx_options:     render_fx_option_inputs(w); break;
        case InstrumentType::swaps:          render_swap_inputs(w); break;
        case InstrumentType::credit:         render_cds_inputs(w); break;
        case InstrumentType::vollib:         render_vollib_inputs(w); break;
    }

    // Error / status
    if (!error_msg_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ERROR_RED, "%s", error_msg_.c_str());
    }
    if (!status_msg_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(SUCCESS, "%s", status_msg_.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Bond Inputs
// ============================================================================

void DerivativesScreen::render_bond_inputs(float w) {
    const float iw = w - 24.0f;

    ImGui::TextColored(TEXT_DIM, "Issue Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##bond_issue", bond_params_.issue_date, sizeof(bond_params_.issue_date));

    ImGui::TextColored(TEXT_DIM, "Settlement Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##bond_settle", bond_params_.settlement_date, sizeof(bond_params_.settlement_date));

    ImGui::TextColored(TEXT_DIM, "Maturity Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##bond_maturity", bond_params_.maturity_date, sizeof(bond_params_.maturity_date));

    ImGui::TextColored(TEXT_DIM, "Coupon Rate (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##bond_coupon", &bond_params_.coupon_rate, 0.1, 0.5, "%.2f");

    ImGui::TextColored(TEXT_DIM, "YTM (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##bond_ytm", &bond_params_.ytm, 0.1, 0.5, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Frequency");
    ImGui::SetNextItemWidth(iw);
    static constexpr const char* freq_labels[] = {"Annual", "Semi-Annual", "Quarterly"};
    static constexpr int freq_vals[] = {1, 2, 4};
    int freq_idx = (bond_params_.frequency == 4) ? 2 : (bond_params_.frequency == 2) ? 1 : 0;
    if (ImGui::Combo("##bond_freq", &freq_idx, freq_labels, 3)) {
        bond_params_.frequency = freq_vals[freq_idx];
    }

    ImGui::TextColored(TEXT_DIM, "Clean Price (for YTM calc)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##bond_cp", &bond_params_.clean_price, 0.5, 1.0, "%.4f");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (loading_) ImGui::BeginDisabled();
    if (ImGui::Button("Calculate Price", ImVec2(iw * 0.48f, 30.0f))) {
        calculate_bond_price();
    }
    ImGui::SameLine();
    if (ImGui::Button("Calculate YTM", ImVec2(iw * 0.48f, 30.0f))) {
        calculate_bond_ytm();
    }
    if (loading_) ImGui::EndDisabled();

    if (loading_) {
        ImGui::TextColored(TEXT_DIM, "Calculating...");
    }
}

// ============================================================================
// Equity Option Inputs
// ============================================================================

void DerivativesScreen::render_equity_option_inputs(float w) {
    const float iw = w - 24.0f;

    ImGui::TextColored(TEXT_DIM, "Valuation Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##opt_val", option_params_.valuation_date, sizeof(option_params_.valuation_date));

    ImGui::TextColored(TEXT_DIM, "Expiry Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##opt_exp", option_params_.expiry_date, sizeof(option_params_.expiry_date));

    ImGui::TextColored(TEXT_DIM, "Option Type");
    ImGui::SetNextItemWidth(iw);
    static constexpr const char* ot_labels[] = {"Call", "Put"};
    ImGui::Combo("##opt_type", &option_params_.option_type, ot_labels, 2);

    ImGui::TextColored(TEXT_DIM, "Spot Price");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##opt_spot", &option_params_.spot, 1.0, 5.0, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Strike Price");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##opt_strike", &option_params_.strike, 1.0, 5.0, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Volatility (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##opt_vol", &option_params_.volatility, 1.0, 5.0, "%.1f");

    ImGui::TextColored(TEXT_DIM, "Risk-Free Rate (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##opt_rf", &option_params_.risk_free_rate, 0.1, 0.5, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Dividend Yield (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##opt_div", &option_params_.dividend_yield, 0.1, 0.5, "%.2f");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (loading_) ImGui::BeginDisabled();
    if (ImGui::Button("Price Option", ImVec2(iw * 0.48f, 30.0f))) {
        calculate_option_price();
        compute_payoff_diagram();
    }
    ImGui::SameLine();
    if (ImGui::Button("Implied Vol", ImVec2(iw * 0.48f, 30.0f))) {
        calculate_implied_vol();
    }
    if (loading_) ImGui::EndDisabled();
}

// ============================================================================
// FX Option Inputs
// ============================================================================

void DerivativesScreen::render_fx_option_inputs(float w) {
    const float iw = w - 24.0f;

    ImGui::TextColored(TEXT_DIM, "Valuation Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##fx_val", fx_params_.valuation_date, sizeof(fx_params_.valuation_date));

    ImGui::TextColored(TEXT_DIM, "Expiry Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##fx_exp", fx_params_.expiry_date, sizeof(fx_params_.expiry_date));

    ImGui::TextColored(TEXT_DIM, "Option Type");
    ImGui::SetNextItemWidth(iw);
    static constexpr const char* ot_labels[] = {"Call", "Put"};
    ImGui::Combo("##fx_type", &fx_params_.option_type, ot_labels, 2);

    ImGui::TextColored(TEXT_DIM, "Spot Rate");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##fx_spot", &fx_params_.spot, 0.01, 0.05, "%.4f");

    ImGui::TextColored(TEXT_DIM, "Strike Rate");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##fx_strike", &fx_params_.strike, 0.01, 0.05, "%.4f");

    ImGui::TextColored(TEXT_DIM, "Volatility (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##fx_vol", &fx_params_.volatility, 0.5, 1.0, "%.1f");

    ImGui::TextColored(TEXT_DIM, "Domestic Rate (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##fx_dom", &fx_params_.domestic_rate, 0.1, 0.5, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Foreign Rate (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##fx_for", &fx_params_.foreign_rate, 0.1, 0.5, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Notional");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##fx_not", &fx_params_.notional, 10000.0, 100000.0, "%.0f");

    ImGui::Spacing();
    if (loading_) ImGui::BeginDisabled();
    if (ImGui::Button("Price FX Option", ImVec2(iw, 30.0f))) {
        calculate_fx_option();
    }
    if (loading_) ImGui::EndDisabled();
}

// ============================================================================
// Swap Inputs
// ============================================================================

void DerivativesScreen::render_swap_inputs(float w) {
    const float iw = w - 24.0f;

    ImGui::TextColored(TEXT_DIM, "Effective Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##sw_eff", swap_params_.effective_date, sizeof(swap_params_.effective_date));

    ImGui::TextColored(TEXT_DIM, "Maturity Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##sw_mat", swap_params_.maturity_date, sizeof(swap_params_.maturity_date));

    ImGui::TextColored(TEXT_DIM, "Fixed Rate (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##sw_fix", &swap_params_.fixed_rate, 0.1, 0.5, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Frequency");
    ImGui::SetNextItemWidth(iw);
    static constexpr const char* freq_labels[] = {"Annual", "Semi-Annual", "Quarterly"};
    static constexpr int freq_vals[] = {1, 2, 4};
    int freq_idx = (swap_params_.frequency == 4) ? 2 : (swap_params_.frequency == 2) ? 1 : 0;
    if (ImGui::Combo("##sw_freq", &freq_idx, freq_labels, 3)) {
        swap_params_.frequency = freq_vals[freq_idx];
    }

    ImGui::TextColored(TEXT_DIM, "Notional");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##sw_not", &swap_params_.notional, 10000.0, 100000.0, "%.0f");

    ImGui::TextColored(TEXT_DIM, "Discount Rate (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##sw_disc", &swap_params_.discount_rate, 0.1, 0.5, "%.2f");

    ImGui::Spacing();
    if (loading_) ImGui::BeginDisabled();
    if (ImGui::Button("Calculate Swap", ImVec2(iw, 30.0f))) {
        calculate_swap();
    }
    if (loading_) ImGui::EndDisabled();
}

// ============================================================================
// CDS Inputs
// ============================================================================

void DerivativesScreen::render_cds_inputs(float w) {
    const float iw = w - 24.0f;

    ImGui::TextColored(TEXT_DIM, "Valuation Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##cds_val", cds_params_.valuation_date, sizeof(cds_params_.valuation_date));

    ImGui::TextColored(TEXT_DIM, "Maturity Date");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputText("##cds_mat", cds_params_.maturity_date, sizeof(cds_params_.maturity_date));

    ImGui::TextColored(TEXT_DIM, "Recovery Rate (%%)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##cds_rec", &cds_params_.recovery_rate, 1.0, 5.0, "%.1f");

    ImGui::TextColored(TEXT_DIM, "Notional");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##cds_not", &cds_params_.notional, 100000.0, 1000000.0, "%.0f");

    ImGui::TextColored(TEXT_DIM, "Spread (bps)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##cds_spread", &cds_params_.spread_bps, 5.0, 25.0, "%.0f");

    ImGui::Spacing();
    if (loading_) ImGui::BeginDisabled();
    if (ImGui::Button("Calculate CDS", ImVec2(iw, 30.0f))) {
        calculate_cds();
    }
    if (loading_) ImGui::EndDisabled();
}

// ============================================================================
// Vollib Inputs
// ============================================================================

void DerivativesScreen::render_vollib_inputs(float w) {
    const float iw = w - 24.0f;

    ImGui::TextColored(TEXT_DIM, "Model");
    ImGui::SetNextItemWidth(iw);
    static constexpr const char* model_labels[] = {"Black", "Black-Scholes", "Black-Scholes-Merton"};
    int model_idx = static_cast<int>(vollib_params_.model);
    if (ImGui::Combo("##vl_model", &model_idx, model_labels, 3)) {
        vollib_params_.model = static_cast<VollibModel>(model_idx);
    }

    ImGui::TextColored(TEXT_DIM, "Option Type");
    ImGui::SetNextItemWidth(iw);
    static constexpr const char* flag_labels[] = {"Call", "Put"};
    ImGui::Combo("##vl_flag", &vollib_params_.flag, flag_labels, 2);

    ImGui::TextColored(TEXT_DIM, "Spot (S)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##vl_s", &vollib_params_.S, 1.0, 5.0, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Strike (K)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##vl_k", &vollib_params_.K, 1.0, 5.0, "%.2f");

    ImGui::TextColored(TEXT_DIM, "Time to Expiry (years)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##vl_t", &vollib_params_.t, 0.01, 0.1, "%.4f");

    ImGui::TextColored(TEXT_DIM, "Risk-Free Rate");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##vl_r", &vollib_params_.r, 0.005, 0.01, "%.4f");

    ImGui::TextColored(TEXT_DIM, "Volatility (sigma)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##vl_sig", &vollib_params_.sigma, 0.01, 0.05, "%.4f");

    ImGui::TextColored(TEXT_DIM, "Dividend Yield (q)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##vl_q", &vollib_params_.q, 0.005, 0.01, "%.4f");

    ImGui::TextColored(TEXT_DIM, "Option Price (for IV)");
    ImGui::SetNextItemWidth(iw);
    ImGui::InputDouble("##vl_price", &vollib_params_.option_price, 0.5, 1.0, "%.2f");

    ImGui::Spacing();
    if (loading_) ImGui::BeginDisabled();
    if (ImGui::Button("Price", ImVec2(iw * 0.48f, 30.0f))) {
        calculate_vollib();
    }
    ImGui::SameLine();
    if (ImGui::Button("Implied Vol", ImVec2(iw * 0.48f, 30.0f))) {
        calculate_vollib();
    }
    if (loading_) ImGui::EndDisabled();
}

// ============================================================================
// Results Panel (right side)
// ============================================================================

void DerivativesScreen::render_results_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##deriv_results", ImVec2(w, h), true);

    const float results_h = h * 0.45f;
    const float payoff_h = h * 0.50f;

    // Results section
    ImGui::TextColored(ACCENT, "RESULTS");
    ImGui::Separator();

    switch (active_instrument_) {
        case InstrumentType::bonds:
            render_bond_results(w, results_h);
            break;
        case InstrumentType::equity_options:
        case InstrumentType::fx_options:
        case InstrumentType::vollib:
            render_greeks_results(w, results_h);
            break;
        case InstrumentType::swaps:
            render_swap_results(w, results_h);
            break;
        case InstrumentType::credit:
            render_cds_results(w, results_h);
            break;
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Payoff diagram (for options)
    if (active_instrument_ == InstrumentType::equity_options ||
        active_instrument_ == InstrumentType::fx_options ||
        active_instrument_ == InstrumentType::vollib) {
        ImGui::TextColored(ACCENT, "PAYOFF DIAGRAM");
        render_payoff_diagram(w - 16.0f, payoff_h);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Bond Results
// ============================================================================

void DerivativesScreen::render_bond_results(float w, float h) {
    if (!bond_result_.valid) {
        ImGui::TextColored(TEXT_DIM, "Click Calculate to see results");
        return;
    }

    if (ImGui::BeginTable("##bond_res", 2,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders,
            ImVec2(w - 16.0f, 0))) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_None, 0.5f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None, 0.5f);
        ImGui::TableHeadersRow();

        auto row = [](const char* label, const char* fmt, double val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%s", label);
            ImGui::TableNextColumn();
            ImGui::Text(fmt, val);
        };

        row("Clean Price",      "%.4f", bond_result_.clean_price);
        row("Dirty Price",      "%.4f", bond_result_.dirty_price);
        row("Accrued Interest",  "%.4f", bond_result_.accrued_interest);
        row("Duration",         "%.4f", bond_result_.duration);
        row("Convexity",        "%.4f", bond_result_.convexity);
        row("YTM",              "%.4f%%", bond_result_.ytm * 100.0);

        ImGui::EndTable();
    }
}

// ============================================================================
// Greeks Results (equity options, FX options, vollib)
// ============================================================================

void DerivativesScreen::render_greeks_results(float w, float h) {
    const GreeksResult* res = nullptr;
    if (active_instrument_ == InstrumentType::equity_options) res = &greeks_result_;
    else if (active_instrument_ == InstrumentType::fx_options) res = &fx_greeks_result_;
    else res = &vollib_result_;

    if (!res->valid) {
        ImGui::TextColored(TEXT_DIM, "Click Calculate to see results");
        return;
    }

    // Price display
    ImGui::TextColored(TEXT_PRIMARY, "Option Price: ");
    ImGui::SameLine();
    ImGui::TextColored(ACCENT, "%.4f", res->price);

    if (res->implied_vol > 0.0) {
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::TextColored(TEXT_PRIMARY, "IV: ");
        ImGui::SameLine();
        ImGui::TextColored(WARNING, "%.2f%%", res->implied_vol * 100.0);
    }

    ImGui::Spacing();

    // Greeks table
    if (ImGui::BeginTable("##greeks_tbl", 2,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders,
            ImVec2(w - 16.0f, 0))) {
        ImGui::TableSetupColumn("Greek", ImGuiTableColumnFlags_None, 0.4f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None, 0.6f);
        ImGui::TableHeadersRow();

        struct GreekRow { const char* name; double value; const char* desc; };
        const GreekRow rows[] = {
            {"Delta",  res->delta, "Price sensitivity to underlying"},
            {"Gamma",  res->gamma, "Delta sensitivity to underlying"},
            {"Vega",   res->vega,  "Price sensitivity to volatility"},
            {"Theta",  res->theta, "Time decay per day"},
            {"Rho",    res->rho,   "Price sensitivity to interest rate"},
        };

        for (const auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ACCENT, "%s", r.name);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", r.desc);
            }
            ImGui::TableNextColumn();
            const auto& col = (r.value >= 0) ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(col, "%+.6f", r.value);
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Swap Results
// ============================================================================

void DerivativesScreen::render_swap_results(float w, float h) {
    if (!swap_result_.valid) {
        ImGui::TextColored(TEXT_DIM, "Click Calculate to see results");
        return;
    }

    if (ImGui::BeginTable("##swap_res", 2,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders,
            ImVec2(w - 16.0f, 0))) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_None, 0.5f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None, 0.5f);
        ImGui::TableHeadersRow();

        auto row = [](const char* label, const char* fmt, double val, const ImVec4& col) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%s", label);
            ImGui::TableNextColumn();
            ImGui::TextColored(col, fmt, val);
        };

        row("Fixed Leg PV",  "%.2f", swap_result_.fixed_leg_pv, TEXT_PRIMARY);
        row("Float Leg PV",  "%.2f", swap_result_.float_leg_pv, TEXT_PRIMARY);
        const auto& sv_col = (swap_result_.swap_value >= 0) ? MARKET_GREEN : MARKET_RED;
        row("Swap Value",    "%+.2f", swap_result_.swap_value, sv_col);
        row("DV01",          "%.4f", swap_result_.dv01, WARNING);

        ImGui::EndTable();
    }
}

// ============================================================================
// CDS Results
// ============================================================================

void DerivativesScreen::render_cds_results(float w, float h) {
    if (!cds_result_.valid) {
        ImGui::TextColored(TEXT_DIM, "Click Calculate to see results");
        return;
    }

    if (ImGui::BeginTable("##cds_res", 2,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders,
            ImVec2(w - 16.0f, 0))) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_None, 0.5f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None, 0.5f);
        ImGui::TableHeadersRow();

        auto row = [](const char* label, const char* fmt, double val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%s", label);
            ImGui::TableNextColumn();
            ImGui::Text(fmt, val);
        };

        row("Upfront PV",    "%.2f", cds_result_.upfront_pv);
        row("Running PV",    "%.2f", cds_result_.running_pv);
        row("Clean Price",   "%.4f", cds_result_.clean_price);
        row("Risky PV01",    "%.6f", cds_result_.risky_pv01);

        ImGui::EndTable();
    }
}

// ============================================================================
// Payoff Diagram (pure C++ computation + DrawList rendering)
// ============================================================================

void DerivativesScreen::compute_payoff_diagram() {
    payoff_data_.clear();

    double spot = 0.0;
    double strike = 0.0;
    bool is_call = true;
    double premium = 0.0;

    if (active_instrument_ == InstrumentType::equity_options) {
        spot = option_params_.spot;
        strike = option_params_.strike;
        is_call = (option_params_.option_type == 0);
        premium = greeks_result_.valid ? greeks_result_.price : 5.0;
    } else if (active_instrument_ == InstrumentType::fx_options) {
        spot = fx_params_.spot;
        strike = fx_params_.strike;
        is_call = (fx_params_.option_type == 0);
        premium = fx_greeks_result_.valid ? fx_greeks_result_.price : 0.02;
    } else if (active_instrument_ == InstrumentType::vollib) {
        spot = vollib_params_.S;
        strike = vollib_params_.K;
        is_call = (vollib_params_.flag == 0);
        premium = vollib_result_.valid ? vollib_result_.price : 5.0;
    } else {
        return;
    }

    if (strike <= 0.0) return;

    const double lo = strike * 0.7;
    const double hi = strike * 1.3;
    const double step = (hi - lo) / PAYOFF_POINTS;

    payoff_data_.reserve(PAYOFF_POINTS + 1);
    for (int i = 0; i <= PAYOFF_POINTS; ++i) {
        PayoffPoint pt;
        pt.underlying = lo + step * static_cast<double>(i);
        if (is_call) {
            pt.payoff = std::max(0.0, pt.underlying - strike);
        } else {
            pt.payoff = std::max(0.0, strike - pt.underlying);
        }
        pt.pnl = pt.payoff - premium;
        payoff_data_.push_back(pt);
    }
}

void DerivativesScreen::render_payoff_diagram(float w, float h) {
    if (payoff_data_.empty() || w < 40.0f || h < 40.0f) {
        ImGui::TextColored(TEXT_DIM, "No payoff data");
        return;
    }

    const ImVec2 p = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();

    // Background
    draw->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(17, 17, 18, 255));

    // Find bounds
    double min_pnl = 0.0;
    double max_pnl = 0.0;
    double min_und = payoff_data_.front().underlying;
    double max_und = payoff_data_.back().underlying;
    for (const auto& pt : payoff_data_) {
        min_pnl = std::min(min_pnl, pt.pnl);
        max_pnl = std::max(max_pnl, pt.pnl);
    }
    if (max_pnl <= min_pnl) max_pnl = min_pnl + 1.0;

    const float pad = 40.0f;
    const float chart_w = w - pad - 10.0f;
    const float chart_h = h - 20.0f;
    const float origin_x = p.x + pad;
    const float origin_y = p.y + 10.0f;

    auto to_screen = [&](double und, double pnl) -> ImVec2 {
        const float fx = origin_x + chart_w * static_cast<float>((und - min_und) / (max_und - min_und));
        const float fy = origin_y + chart_h * static_cast<float>((max_pnl - pnl) / (max_pnl - min_pnl));
        return ImVec2(fx, fy);
    };

    // Zero line
    const ImVec2 z0 = to_screen(min_und, 0.0);
    const ImVec2 z1 = to_screen(max_und, 0.0);
    draw->AddLine(z0, z1, IM_COL32(80, 80, 85, 150), 1.0f);

    // Grid lines (3 horizontal)
    for (int i = 0; i <= 3; ++i) {
        const double pnl_at = min_pnl + (max_pnl - min_pnl) * static_cast<double>(3 - i) / 3.0;
        const ImVec2 gl = to_screen(min_und, pnl_at);
        const ImVec2 gr = to_screen(max_und, pnl_at);
        draw->AddLine(gl, gr, IM_COL32(40, 40, 45, 100));
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", pnl_at);
        draw->AddText(ImVec2(p.x + 2.0f, gl.y - 6.0f), IM_COL32(136, 136, 145, 200), buf);
    }

    // Payoff line (blue)
    for (size_t i = 1; i < payoff_data_.size(); ++i) {
        const ImVec2 a = to_screen(payoff_data_[i - 1].underlying, payoff_data_[i - 1].payoff);
        const ImVec2 b = to_screen(payoff_data_[i].underlying, payoff_data_[i].payoff);
        draw->AddLine(a, b, IM_COL32(77, 153, 245, 200), 1.5f);
    }

    // P&L line (green above zero, red below)
    for (size_t i = 1; i < payoff_data_.size(); ++i) {
        const ImVec2 a = to_screen(payoff_data_[i - 1].underlying, payoff_data_[i - 1].pnl);
        const ImVec2 b = to_screen(payoff_data_[i].underlying, payoff_data_[i].pnl);
        const bool profit = (payoff_data_[i].pnl >= 0.0);
        const ImU32 col = profit ? IM_COL32(13, 217, 107, 220) : IM_COL32(245, 64, 64, 220);
        draw->AddLine(a, b, col, 2.0f);
    }

    // Strike marker
    const ImVec2 sk_top = to_screen(option_params_.strike, max_pnl);
    const ImVec2 sk_bot = to_screen(option_params_.strike, min_pnl);
    draw->AddLine(sk_top, sk_bot, IM_COL32(255, 135, 0, 150), 1.0f);
    draw->AddText(ImVec2(sk_top.x + 4.0f, sk_top.y), IM_COL32(255, 135, 0, 200), "K");

    // Legend
    const float lx = p.x + w - 140.0f;
    const float ly = p.y + 14.0f;
    draw->AddLine(ImVec2(lx, ly + 4.0f), ImVec2(lx + 20.0f, ly + 4.0f),
                  IM_COL32(77, 153, 245, 200), 2.0f);
    draw->AddText(ImVec2(lx + 24.0f, ly - 2.0f), IM_COL32(180, 180, 190, 200), "Payoff");
    draw->AddLine(ImVec2(lx, ly + 18.0f), ImVec2(lx + 20.0f, ly + 18.0f),
                  IM_COL32(13, 217, 107, 220), 2.0f);
    draw->AddText(ImVec2(lx + 24.0f, ly + 12.0f), IM_COL32(180, 180, 190, 200), "P&L");

    ImGui::Dummy(ImVec2(w, h));
}

// ============================================================================
// Calculation actions — call Python FinancePy scripts
// ============================================================================

void DerivativesScreen::calculate_bond_price() {
    loading_ = true;
    error_msg_.clear();

    auto bp = bond_params_;  // copy for thread safety
    std::thread([this, bp]() {
        std::vector<std::string> args = {
            "bond_price",
            bp.issue_date, bp.settlement_date, bp.maturity_date,
            std::to_string(bp.coupon_rate), std::to_string(bp.ytm),
            std::to_string(bp.frequency)
        };

        auto result = python::execute("Analytics/derivatives/financepy_pricing.py", args);
        if (result.success) {
            try {
                json j = json::parse(result.output);
                bond_result_.clean_price = j.value("clean_price", 0.0);
                bond_result_.dirty_price = j.value("dirty_price", 0.0);
                bond_result_.accrued_interest = j.value("accrued_interest", 0.0);
                bond_result_.duration = j.value("duration", 0.0);
                bond_result_.convexity = j.value("convexity", 0.0);
                bond_result_.ytm = j.value("ytm", 0.0);
                bond_result_.valid = true;
                status_msg_ = "Bond pricing complete";
            } catch (const std::exception& e) {
                error_msg_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_msg_ = result.error.empty() ? "Calculation failed" : result.error;
        }
        loading_ = false;
    }).detach();
}

void DerivativesScreen::calculate_bond_ytm() {
    loading_ = true;
    error_msg_.clear();

    auto bp = bond_params_;
    std::thread([this, bp]() {
        std::vector<std::string> args = {
            "bond_ytm",
            bp.issue_date, bp.settlement_date, bp.maturity_date,
            std::to_string(bp.coupon_rate), std::to_string(bp.clean_price),
            std::to_string(bp.frequency)
        };

        auto result = python::execute("Analytics/derivatives/financepy_pricing.py", args);
        if (result.success) {
            try {
                json j = json::parse(result.output);
                bond_result_.ytm = j.value("ytm", 0.0);
                bond_result_.valid = true;
                status_msg_ = "YTM calculation complete";
            } catch (const std::exception& e) {
                error_msg_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_msg_ = result.error.empty() ? "Calculation failed" : result.error;
        }
        loading_ = false;
    }).detach();
}

void DerivativesScreen::calculate_option_price() {
    loading_ = true;
    error_msg_.clear();

    auto op = option_params_;
    std::thread([this, op]() {
        std::vector<std::string> args = {
            "equity_option_price",
            op.valuation_date, op.expiry_date,
            std::to_string(op.strike), std::to_string(op.spot),
            std::to_string(op.volatility), std::to_string(op.risk_free_rate),
            std::to_string(op.dividend_yield),
            (op.option_type == 0) ? "call" : "put"
        };

        auto result = python::execute("Analytics/derivatives/financepy_pricing.py", args);
        if (result.success) {
            try {
                json j = json::parse(result.output);
                greeks_result_.price = j.value("price", 0.0);
                if (j.contains("greeks")) {
                    const auto& g = j["greeks"];
                    greeks_result_.delta = g.value("delta", 0.0);
                    greeks_result_.gamma = g.value("gamma", 0.0);
                    greeks_result_.vega = g.value("vega", 0.0);
                    greeks_result_.theta = g.value("theta", 0.0);
                    greeks_result_.rho = g.value("rho", 0.0);
                }
                greeks_result_.valid = true;
                status_msg_ = "Option pricing complete";
            } catch (const std::exception& e) {
                error_msg_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_msg_ = result.error.empty() ? "Calculation failed" : result.error;
        }
        loading_ = false;
    }).detach();
}

void DerivativesScreen::calculate_implied_vol() {
    loading_ = true;
    error_msg_.clear();

    auto op = option_params_;
    std::thread([this, op]() {
        std::vector<std::string> args = {
            "equity_option_implied_vol",
            op.valuation_date, op.expiry_date,
            std::to_string(op.strike), std::to_string(op.spot),
            "15.0",  // option_price placeholder
            std::to_string(op.risk_free_rate), std::to_string(op.dividend_yield),
            (op.option_type == 0) ? "call" : "put"
        };

        auto result = python::execute("Analytics/derivatives/financepy_pricing.py", args);
        if (result.success) {
            try {
                json j = json::parse(result.output);
                greeks_result_.implied_vol = j.value("implied_vol", 0.0);
                greeks_result_.valid = true;
                status_msg_ = "Implied vol calculation complete";
            } catch (const std::exception& e) {
                error_msg_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_msg_ = result.error.empty() ? "Calculation failed" : result.error;
        }
        loading_ = false;
    }).detach();
}

void DerivativesScreen::calculate_fx_option() {
    loading_ = true;
    error_msg_.clear();

    auto fp = fx_params_;
    std::thread([this, fp]() {
        std::vector<std::string> args = {
            "fx_option_price",
            fp.valuation_date, fp.expiry_date,
            std::to_string(fp.strike), std::to_string(fp.spot),
            std::to_string(fp.volatility),
            std::to_string(fp.domestic_rate), std::to_string(fp.foreign_rate),
            (fp.option_type == 0) ? "call" : "put",
            std::to_string(fp.notional)
        };

        auto result = python::execute("Analytics/derivatives/financepy_pricing.py", args);
        if (result.success) {
            try {
                json j = json::parse(result.output);
                fx_greeks_result_.price = j.value("price", 0.0);
                if (j.contains("greeks")) {
                    const auto& g = j["greeks"];
                    fx_greeks_result_.delta = g.value("delta", 0.0);
                    fx_greeks_result_.gamma = g.value("gamma", 0.0);
                    fx_greeks_result_.vega = g.value("vega", 0.0);
                    fx_greeks_result_.theta = g.value("theta", 0.0);
                    fx_greeks_result_.rho = g.value("rho", 0.0);
                }
                fx_greeks_result_.valid = true;
                status_msg_ = "FX option pricing complete";
            } catch (const std::exception& e) {
                error_msg_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_msg_ = result.error.empty() ? "Calculation failed" : result.error;
        }
        loading_ = false;
    }).detach();
}

void DerivativesScreen::calculate_swap() {
    loading_ = true;
    error_msg_.clear();

    auto sp = swap_params_;
    std::thread([this, sp]() {
        std::vector<std::string> args = {
            "swap_price",
            sp.effective_date, sp.maturity_date,
            std::to_string(sp.fixed_rate), std::to_string(sp.frequency),
            std::to_string(sp.notional), std::to_string(sp.discount_rate)
        };

        auto result = python::execute("Analytics/derivatives/financepy_pricing.py", args);
        if (result.success) {
            try {
                json j = json::parse(result.output);
                swap_result_.fixed_leg_pv = j.value("fixed_leg_pv", 0.0);
                swap_result_.float_leg_pv = j.value("float_leg_pv", 0.0);
                swap_result_.swap_value = j.value("swap_value", 0.0);
                swap_result_.dv01 = j.value("dv01", 0.0);
                swap_result_.valid = true;
                status_msg_ = "Swap pricing complete";
            } catch (const std::exception& e) {
                error_msg_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_msg_ = result.error.empty() ? "Calculation failed" : result.error;
        }
        loading_ = false;
    }).detach();
}

void DerivativesScreen::calculate_cds() {
    loading_ = true;
    error_msg_.clear();

    auto cp = cds_params_;
    std::thread([this, cp]() {
        std::vector<std::string> args = {
            "cds_price",
            cp.valuation_date, cp.maturity_date,
            std::to_string(cp.recovery_rate), std::to_string(cp.notional),
            std::to_string(cp.spread_bps)
        };

        auto result = python::execute("Analytics/derivatives/financepy_pricing.py", args);
        if (result.success) {
            try {
                json j = json::parse(result.output);
                cds_result_.upfront_pv = j.value("upfront_pv", 0.0);
                cds_result_.running_pv = j.value("running_pv", 0.0);
                cds_result_.clean_price = j.value("clean_price", 0.0);
                cds_result_.risky_pv01 = j.value("risky_pv01", 0.0);
                cds_result_.valid = true;
                status_msg_ = "CDS pricing complete";
            } catch (const std::exception& e) {
                error_msg_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_msg_ = result.error.empty() ? "Calculation failed" : result.error;
        }
        loading_ = false;
    }).detach();
}

void DerivativesScreen::calculate_vollib() {
    loading_ = true;
    error_msg_.clear();

    auto vp = vollib_params_;
    std::thread([this, vp]() {
        std::vector<std::string> args = {
            "vollib_price",
            vollib_model_label(vp.model),
            (vp.flag == 0) ? "c" : "p",
            std::to_string(vp.S), std::to_string(vp.K),
            std::to_string(vp.t), std::to_string(vp.r),
            std::to_string(vp.sigma), std::to_string(vp.q)
        };

        auto result = python::execute("Analytics/derivatives/vollib_pricing.py", args);
        if (result.success) {
            try {
                json j = json::parse(result.output);
                vollib_result_.price = j.value("price", 0.0);
                if (j.contains("greeks")) {
                    const auto& g = j["greeks"];
                    vollib_result_.delta = g.value("delta", 0.0);
                    vollib_result_.gamma = g.value("gamma", 0.0);
                    vollib_result_.vega = g.value("vega", 0.0);
                    vollib_result_.theta = g.value("theta", 0.0);
                    vollib_result_.rho = g.value("rho", 0.0);
                }
                vollib_result_.valid = true;
                status_msg_ = "Vollib pricing complete";
            } catch (const std::exception& e) {
                error_msg_ = std::string("Parse error: ") + e.what();
            }
        } else {
            error_msg_ = result.error.empty() ? "Calculation failed" : result.error;
        }
        loading_ = false;
    }).detach();
}

} // namespace fincept::derivatives
