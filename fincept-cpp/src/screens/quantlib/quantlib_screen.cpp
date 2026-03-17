// QuantLib Screen — init, render loop, section dispatch
#include "quantlib_screen.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>

namespace fincept::quantlib {

void QuantLibScreen::init() {
    if (initialized_) return;
    modules_ = get_module_defs();
    initialized_ = true;
}

void QuantLibScreen::render() {
    init();
    poll_futures();

    // ScreenFrame: positions a borderless fullscreen window between tab bar and footer
    ui::ScreenFrame frame("##quantlib_screen", ImVec2(0, 0), bb::BG_BLACK);
    if (!frame.begin()) { frame.end(); return; }

    const float w = frame.width();
    const float h = frame.height();

    // Use Yoga vstack to divide: header(36) | body(flex) | status(22)
    constexpr float HEADER_H = 36.0f;
    constexpr float STATUS_H = 22.0f;
    auto vstack = ui::vstack_layout(w, h, {HEADER_H, -1, STATUS_H});

    // ── Header ──
    render_header(w);

    // ── Body: sidebar + content ──
    const float body_h = vstack.heights[1];

    // Responsive sidebar width: narrower on compact screens
    const float sidebar_w = (frame.breakpoint() == ui::Breakpoint::Compact) ? 160.0f : 230.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    // Sidebar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_DARK);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::BORDER_DIM);
    ImGui::BeginChild("##ql_sidebar", ImVec2(sidebar_w, body_h), true);
    render_sidebar(body_h);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    // Content — fills remaining width
    const float content_w = w - sidebar_w;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_BLACK);
    ImGui::BeginChild("##ql_content", ImVec2(content_w, body_h), false);
    render_content(content_w, body_h);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();

    // ── Status bar ──
    render_status_bar(w);

    frame.end();
}

void QuantLibScreen::render_content(float /*width*/, float /*height*/) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));

    // Breadcrumb
    std::string mod_label, section_label;
    for (auto& m : modules_) {
        for (auto& sec : m.sections) {
            if (sec.id == active_section_) {
                mod_label = m.label;
                section_label = sec.label;
                break;
            }
        }
        if (!mod_label.empty()) break;
    }
    ImGui::TextColored(bb::AMBER, "%s", mod_label.c_str());
    ImGui::SameLine();
    ImGui::TextColored(bb::GRAY_DIM, " > ");
    ImGui::SameLine();
    ImGui::TextColored(bb::GREEN, "%s", section_label.c_str());
    ImGui::Spacing();

    // Amber separator — uses actual available width
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float avail_w = ImGui::GetContentRegionAvail().x;
    dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + avail_w, p.y),
        IM_COL32(255, 166, 0, 100), 1.0f);
    ImGui::Dummy(ImVec2(0, 4));

    // Panels fill all remaining space
    ImGui::BeginChild("##ql_panels", ImVec2(-1, -1), false);

    const auto& s = active_section_;
    if      (s == "types")          render_types();
    else if (s == "conventions")    render_conventions();
    else if (s == "autodiff")       render_autodiff();
    else if (s == "distributions")  render_distributions();
    else if (s == "math")           render_math();
    else if (s == "operations")     render_operations();
    else if (s == "legs")           render_legs();
    else if (s == "periods")        render_periods();
    else if (s == "basel")          render_basel();
    else if (s == "saccr")          render_saccr();
    else if (s == "ifrs9")          render_ifrs9();
    else if (s == "liquidity")      render_liquidity();
    else if (s == "stress")         render_stress();
    else if (s == "surface")        render_surface();
    else if (s == "sabr")           render_sabr();
    else if (s == "localvol")       render_localvol();
    else if (s == "shortrate")      render_shortrate();
    else if (s == "hullwhite")      render_hullwhite();
    else if (s == "heston")         render_heston();
    else if (s == "jumpdiff")       render_jumpdiff();
    else if (s == "volmodels")      render_volmodels();
    else if (s == "processes")      render_processes();
    else if (s == "exact")          render_exact();
    else if (s == "simulation")     render_simulation();
    else if (s == "sampling")       render_sampling();
    else if (s == "theory")         render_theory();
    else if (s == "ml-credit")      render_ml_credit();
    else if (s == "ml-regression")  render_ml_regression();
    else if (s == "ml-clustering")  render_ml_clustering();
    else if (s == "ml-preprocessing") render_ml_preprocessing();
    else if (s == "ml-features")    render_ml_features();
    else if (s == "ml-validation")  render_ml_validation();
    else if (s == "ml-timeseries")  render_ml_timeseries();
    else if (s == "ml-gpnn")        render_ml_gpnn();
    else if (s == "ml-factor")      render_ml_factor();
    else if (s == "stat-continuous")  render_stat_continuous();
    else if (s == "stat-discrete")    render_stat_discrete();
    else if (s == "stat-timeseries")  render_stat_timeseries();
    else if (s == "port-optimize")  render_port_optimize();
    else if (s == "port-risk")      render_port_risk();
    else if (s == "sched-calendar") render_sched_calendar();
    else if (s == "sched-daycount") render_sched_daycount();
    else if (s == "phys-entropy")   render_phys_entropy();
    else if (s == "phys-thermo")    render_phys_thermo();
    else if (s == "curves-build")       render_curves_build();
    else if (s == "curves-transform")   render_curves_transform();
    else if (s == "curves-fitting")     render_curves_fitting();
    else if (s == "curves-specialized") render_curves_specialized();
    else if (s == "pricing-bs")         render_pricing_bs();
    else if (s == "pricing-black76")    render_pricing_black76();
    else if (s == "pricing-bachelier")  render_pricing_bachelier();
    else if (s == "pricing-numerical")  render_pricing_numerical();
    else if (s == "risk-var")           render_risk_var();
    else if (s == "risk-evt-xva")       render_risk_evt_xva();
    else if (s == "risk-sensitivities") render_risk_sensitivities();
    else if (s == "econ-equilibrium")   render_econ_equilibrium();
    else if (s == "econ-gametheory")    render_econ_gametheory();
    else if (s == "econ-auctions")      render_econ_auctions();
    else if (s == "econ-utility")       render_econ_utility();
    else if (s == "solver-bond")        render_solver_bond();
    else if (s == "solver-rates")       render_solver_rates();
    else if (s == "solver-cashflow")    render_solver_cashflow();
    else if (s == "num-difffft")        render_num_difffft();
    else if (s == "num-interplinalg")   render_num_interplinalg();
    else if (s == "num-solvers")        render_num_solvers();
    else if (s == "instr-bonds")        render_instr_bonds();
    else if (s == "instr-swaps")        render_instr_swaps();
    else if (s == "instr-markets")      render_instr_markets();
    else if (s == "instr-creditfut")    render_instr_creditfut();
    else if (s == "analysis-fundamentals")  render_analysis_fundamentals();
    else if (s == "analysis-profit-liq")    render_analysis_profit_liq();
    else if (s == "analysis-eff-growth")    render_analysis_eff_growth();
    else if (s == "analysis-lev-cf")        render_analysis_lev_cf();
    else if (s == "analysis-val-qual")      render_analysis_val_qual();
    else if (s == "analysis-industry")      render_analysis_industry();
    else if (s == "analysis-dcf")           render_analysis_dcf();
    else if (s == "analysis-models")        render_analysis_models();
    else {
        ImGui::TextColored(bb::GRAY, "Select a section from the sidebar.");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

} // namespace fincept::quantlib
