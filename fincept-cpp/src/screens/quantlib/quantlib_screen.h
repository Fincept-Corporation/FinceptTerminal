#pragma once
// QuantLib Screen V2 — Bloomberg Terminal clone aesthetic
// Full amber/green terminal colors, dense data grids, monospace everything
// Layout: [Header 40px] [Sidebar 230px | Content] [StatusBar 24px]
// Split: quantlib_screen.cpp (init/render/dispatch)
//        quantlib_ui.cpp (header, sidebar, status bar)
//        quantlib_helpers.cpp (inputs, cards, result rendering, JSON→table)
//        panels_core.cpp, panels_models.cpp, panels_analytics.cpp,
//        panels_instruments.cpp, panels_quant.cpp

#include "quantlib_types.h"
#include "quantlib_api.h"
#include <set>
#include <map>
#include <vector>
#include <imgui.h>

namespace fincept::quantlib {

// Bloomberg terminal color palette
namespace bb {
    constexpr ImVec4 BG_BLACK       = {0.00f, 0.00f, 0.00f, 1.0f};   // #000000
    constexpr ImVec4 BG_DARK        = {0.05f, 0.05f, 0.07f, 1.0f};   // #0D0D12
    constexpr ImVec4 BG_PANEL       = {0.08f, 0.08f, 0.10f, 1.0f};   // #14141A
    constexpr ImVec4 BG_CARD        = {0.10f, 0.10f, 0.13f, 1.0f};   // #1A1A21
    constexpr ImVec4 BG_INPUT       = {0.06f, 0.06f, 0.08f, 1.0f};   // #0F0F14
    constexpr ImVec4 BG_HOVER       = {0.14f, 0.14f, 0.18f, 1.0f};   // #24242E
    constexpr ImVec4 BG_HEADER      = {0.02f, 0.02f, 0.04f, 1.0f};   // #05050A

    constexpr ImVec4 BORDER         = {0.18f, 0.18f, 0.22f, 1.0f};   // #2E2E38
    constexpr ImVec4 BORDER_DIM     = {0.12f, 0.12f, 0.15f, 1.0f};   // #1F1F26
    constexpr ImVec4 BORDER_BRIGHT  = {0.25f, 0.25f, 0.30f, 1.0f};   // #40404D

    // Bloomberg amber/orange text
    constexpr ImVec4 AMBER          = {1.00f, 0.65f, 0.00f, 1.0f};   // #FFA600
    constexpr ImVec4 AMBER_DIM      = {0.80f, 0.52f, 0.00f, 1.0f};   // #CC8500
    constexpr ImVec4 AMBER_BRIGHT   = {1.00f, 0.78f, 0.20f, 1.0f};   // #FFC733

    // Bloomberg green
    constexpr ImVec4 GREEN          = {0.00f, 0.85f, 0.35f, 1.0f};   // #00D959
    constexpr ImVec4 GREEN_DIM      = {0.00f, 0.60f, 0.25f, 1.0f};   // #009940
    constexpr ImVec4 GREEN_BRIGHT   = {0.20f, 1.00f, 0.50f, 1.0f};   // #33FF80

    // Data colors
    constexpr ImVec4 WHITE          = {0.90f, 0.90f, 0.92f, 1.0f};   // #E6E6EB
    constexpr ImVec4 GRAY           = {0.55f, 0.55f, 0.60f, 1.0f};   // #8C8C99
    constexpr ImVec4 GRAY_DIM       = {0.35f, 0.35f, 0.40f, 1.0f};   // #595966
    constexpr ImVec4 RED            = {1.00f, 0.25f, 0.25f, 1.0f};   // #FF4040
    constexpr ImVec4 BLUE           = {0.30f, 0.55f, 1.00f, 1.0f};   // #4D8CFF
    constexpr ImVec4 CYAN           = {0.00f, 0.80f, 0.80f, 1.0f};   // #00CCCC
    constexpr ImVec4 YELLOW         = {1.00f, 0.90f, 0.10f, 1.0f};   // #FFE61A
    constexpr ImVec4 MAGENTA        = {0.85f, 0.30f, 0.85f, 1.0f};   // #D94DD9

    // Semantic
    constexpr ImVec4 POSITIVE       = {0.00f, 0.85f, 0.35f, 1.0f};   // green
    constexpr ImVec4 NEGATIVE       = {1.00f, 0.25f, 0.25f, 1.0f};   // red
    constexpr ImVec4 NEUTRAL        = {0.55f, 0.55f, 0.60f, 1.0f};   // gray
    constexpr ImVec4 RUNNING        = {1.00f, 0.78f, 0.20f, 1.0f};   // amber bright
    constexpr ImVec4 ERROR_COL      = {1.00f, 0.20f, 0.20f, 1.0f};   // bright red

    // Button
    constexpr ImVec4 BTN_BG         = {0.00f, 0.45f, 0.18f, 1.0f};   // dark green
    constexpr ImVec4 BTN_HOVER      = {0.00f, 0.60f, 0.25f, 1.0f};   // green
    constexpr ImVec4 BTN_ACTIVE     = {0.00f, 0.75f, 0.30f, 1.0f};   // bright green

    constexpr ImVec4 BTN_SEC_BG     = {0.12f, 0.12f, 0.15f, 1.0f};
    constexpr ImVec4 BTN_SEC_HOVER  = {0.18f, 0.18f, 0.22f, 1.0f};
}

class QuantLibScreen {
public:
    void render();

private:
    void init();

    // ── Layout Zones (quantlib_ui.cpp) ──
    void render_header(float width);
    void render_sidebar(float height);
    void render_content(float width, float height);
    void render_status_bar(float width);

    // ── UI Helpers (quantlib_helpers.cpp) ──
    bool begin_endpoint_card(const std::string& id, const char* title, const char* description = nullptr);
    void end_endpoint_card();

    void input_float(const std::string& ep_id, const char* label, const char* field,
                     const char* default_val, float width = 120.0f);
    void input_text(const std::string& ep_id, const char* label, const char* field,
                    const char* default_val, float width = 150.0f);
    void input_select(const std::string& ep_id, const char* label, const char* field,
                      const std::vector<const char*>& options, float width = 120.0f);
    void input_int(const std::string& ep_id, const char* label, const char* field,
                   const char* default_val, float width = 100.0f);
    void input_array(const std::string& ep_id, const char* label, const char* field,
                     const char* default_val, float width = 220.0f);

    bool run_button(const std::string& ep_id);
    void render_result(const std::string& ep_id);

    // Smart result display
    void render_result_table(const nlohmann::json& data);
    void render_result_kv(const nlohmann::json& data, int depth = 0);
    void render_result_array_table(const nlohmann::json& arr);

    void fire_post(const std::string& ep_id, const std::string& api_path, const json& body);
    void fire_get(const std::string& ep_id, const std::string& api_path,
                  const std::map<std::string, std::string>& params = {});
    void poll_futures();

    // ── Panel renderers ──
    // panels_core.cpp
    void render_types(); void render_conventions(); void render_autodiff();
    void render_distributions(); void render_math(); void render_operations();
    void render_legs(); void render_periods();
    void render_basel(); void render_saccr(); void render_ifrs9();
    void render_liquidity(); void render_stress();
    // panels_models.cpp
    void render_surface(); void render_sabr(); void render_localvol();
    void render_shortrate(); void render_hullwhite(); void render_heston();
    void render_jumpdiff(); void render_volmodels();
    void render_processes(); void render_exact(); void render_simulation();
    void render_sampling(); void render_theory();
    // panels_analytics.cpp
    void render_ml_credit(); void render_ml_regression(); void render_ml_clustering();
    void render_ml_preprocessing(); void render_ml_features(); void render_ml_validation();
    void render_ml_timeseries(); void render_ml_gpnn(); void render_ml_factor();
    void render_stat_continuous(); void render_stat_discrete(); void render_stat_timeseries();
    void render_port_optimize(); void render_port_risk();
    // panels_instruments.cpp
    void render_curves_build(); void render_curves_transform();
    void render_curves_fitting(); void render_curves_specialized();
    void render_pricing_bs(); void render_pricing_black76();
    void render_pricing_bachelier(); void render_pricing_numerical();
    void render_risk_var(); void render_risk_evt_xva(); void render_risk_sensitivities();
    void render_instr_bonds(); void render_instr_swaps();
    void render_instr_markets(); void render_instr_creditfut();
    // panels_quant.cpp
    void render_econ_equilibrium(); void render_econ_gametheory();
    void render_econ_auctions(); void render_econ_utility();
    void render_solver_bond(); void render_solver_rates(); void render_solver_cashflow();
    void render_num_difffft(); void render_num_interplinalg(); void render_num_solvers();
    void render_sched_calendar(); void render_sched_daycount();
    void render_phys_entropy(); void render_phys_thermo();
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
    char search_buf_[128] = {};
    int total_calls_ = 0;
    double total_latency_ = 0;
    bool show_raw_json_ = false;

    EndpointState& ep(const std::string& id) { return endpoints_[id]; }
};

} // namespace fincept::quantlib
