#pragma once
// Derivatives Screen — Bloomberg-style derivatives pricing & valuation.
// Layout: instrument tabs | [input panel | results + payoff diagram]

#include "derivatives_types.h"
#include <string>
#include <vector>
#include <future>

namespace fincept::derivatives {

class DerivativesScreen {
public:
    void render();

private:
    void init();

    // Panel renderers
    void render_instrument_tabs(float w);
    void render_input_panel(float x, float y, float w, float h);
    void render_results_panel(float x, float y, float w, float h);

    // Instrument-specific input forms
    void render_bond_inputs(float w);
    void render_equity_option_inputs(float w);
    void render_fx_option_inputs(float w);
    void render_swap_inputs(float w);
    void render_cds_inputs(float w);
    void render_vollib_inputs(float w);

    // Results renderers
    void render_bond_results(float w, float h);
    void render_greeks_results(float w, float h);
    void render_swap_results(float w, float h);
    void render_cds_results(float w, float h);
    void render_payoff_diagram(float w, float h);

    // Calculation actions (calls Python via python_runner)
    void calculate_bond_price();
    void calculate_bond_ytm();
    void calculate_option_price();
    void calculate_implied_vol();
    void calculate_fx_option();
    void calculate_swap();
    void calculate_cds();
    void calculate_vollib();

    // Payoff computation (pure C++, no Python needed)
    void compute_payoff_diagram();

    // State
    bool initialized_ = false;
    InstrumentType active_instrument_ = InstrumentType::bonds;

    // Parameters
    BondParams bond_params_;
    EquityOptionParams option_params_;
    FXOptionParams fx_params_;
    SwapParams swap_params_;
    CDSParams cds_params_;
    VollibParams vollib_params_;

    // Results
    BondResult bond_result_;
    GreeksResult greeks_result_;
    GreeksResult fx_greeks_result_;
    SwapResult swap_result_;
    CDSResult cds_result_;
    GreeksResult vollib_result_;

    // Payoff diagram data
    std::vector<PayoffPoint> payoff_data_;

    // Async state
    bool loading_ = false;
    std::string error_msg_;
    std::string status_msg_;
};

} // namespace fincept::derivatives
