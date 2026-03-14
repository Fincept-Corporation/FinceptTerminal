#pragma once
// QuantLib Screen — 18-module quantitative analysis suite
// Layout: [Left: 210px sidebar with collapsible modules] [Right: Active panel]
// All computation via HTTP API to https://api.fincept.in/quantlib/...

#include "quantlib_types.h"
#include "quantlib_api.h"
#include <set>
#include <map>

namespace fincept::quantlib {

class QuantLibScreen {
public:
    void render();

private:
    void init();

    // Layout
    void render_sidebar(float height);
    void render_content(float width, float height);

    // ── UI Helpers (reusable across all panels) ──
    bool begin_endpoint_card(const std::string& id, const char* title, const char* description = nullptr);
    void end_endpoint_card();

    void input_float(const std::string& ep_id, const char* label, const char* field,
                     const char* default_val, float width = 100.0f);
    void input_text(const std::string& ep_id, const char* label, const char* field,
                    const char* default_val, float width = 140.0f);
    void input_select(const std::string& ep_id, const char* label, const char* field,
                      const std::vector<const char*>& options, float width = 100.0f);
    void input_int(const std::string& ep_id, const char* label, const char* field,
                   const char* default_val, float width = 100.0f);
    void input_array(const std::string& ep_id, const char* label, const char* field,
                     const char* default_val, float width = 200.0f);

    bool run_button(const std::string& ep_id);
    void render_result(const std::string& ep_id);

    void fire_post(const std::string& ep_id, const std::string& api_path, const json& body);
    void fire_get(const std::string& ep_id, const std::string& api_path,
                  const std::map<std::string, std::string>& params = {});

    void poll_futures();

    // ── Panel renderers (one per section) ──
    // Core
    void render_types(); void render_conventions(); void render_autodiff();
    void render_distributions(); void render_math(); void render_operations();
    void render_legs(); void render_periods();
    // Regulatory
    void render_basel(); void render_saccr(); void render_ifrs9();
    void render_liquidity(); void render_stress();
    // Volatility
    void render_surface(); void render_sabr(); void render_localvol();
    // Models
    void render_shortrate(); void render_hullwhite(); void render_heston();
    void render_jumpdiff(); void render_volmodels();
    // Stochastic
    void render_processes(); void render_exact(); void render_simulation();
    void render_sampling(); void render_theory();
    // ML
    void render_ml_credit(); void render_ml_regression(); void render_ml_clustering();
    void render_ml_preprocessing(); void render_ml_features(); void render_ml_validation();
    void render_ml_timeseries(); void render_ml_gpnn(); void render_ml_factor();
    // Statistics
    void render_stat_continuous(); void render_stat_discrete(); void render_stat_timeseries();
    // Portfolio
    void render_port_optimize(); void render_port_risk();
    // Scheduling
    void render_sched_calendar(); void render_sched_daycount();
    // Physics
    void render_phys_entropy(); void render_phys_thermo();
    // Curves
    void render_curves_build(); void render_curves_transform();
    void render_curves_fitting(); void render_curves_specialized();
    // Pricing
    void render_pricing_bs(); void render_pricing_black76();
    void render_pricing_bachelier(); void render_pricing_numerical();
    // Risk
    void render_risk_var(); void render_risk_evt_xva(); void render_risk_sensitivities();
    // Economics
    void render_econ_equilibrium(); void render_econ_gametheory();
    void render_econ_auctions(); void render_econ_utility();
    // Solver
    void render_solver_bond(); void render_solver_rates(); void render_solver_cashflow();
    // Numerical
    void render_num_difffft(); void render_num_interplinalg(); void render_num_solvers();
    // Instruments
    void render_instr_bonds(); void render_instr_swaps();
    void render_instr_markets(); void render_instr_creditfut();
    // Analysis
    void render_analysis_fundamentals(); void render_analysis_profit_liq();
    void render_analysis_eff_growth(); void render_analysis_lev_cf();
    void render_analysis_val_qual(); void render_analysis_industry();
    void render_analysis_dcf(); void render_analysis_models();

    // ── State ──
    bool initialized_ = false;
    std::string active_section_ = "types";
    std::set<std::string> expanded_modules_ = {"core"};
    std::vector<ModuleDef> modules_;
    std::map<std::string, EndpointState> endpoints_;

    EndpointState& ep(const std::string& id) { return endpoints_[id]; }
};

} // namespace fincept::quantlib
