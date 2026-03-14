// QuantLib Screen — 18-module quantitative analysis suite
// 590+ endpoints across 77 panels, all via HTTP API

#include "quantlib_screen.h"
#include "auth/auth_manager.h"
#include "core/logger.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace fincept::quantlib {

// ============================================================================
// Init
// ============================================================================
void QuantLibScreen::init() {
    if (initialized_) return;
    modules_ = get_module_defs();
    initialized_ = true;
    LOG_INFO("QuantLib", "Initialized with %zu modules", modules_.size());
}

// ============================================================================
// Main render
// ============================================================================
void QuantLibScreen::render() {
    init();
    using namespace theme::colors;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_bar_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    float x = vp->WorkPos.x;
    float y = vp->WorkPos.y + tab_bar_h;
    float w = vp->WorkSize.x;
    float h = vp->WorkSize.y - tab_bar_h - footer_h;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##quantlib", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking);

    float sidebar_w = 210.0f;
    ImGui::BeginChild("##ql_sidebar", ImVec2(sidebar_w, h), true);
    render_sidebar(h);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##ql_content", ImVec2(w - sidebar_w - 2, h));
    render_content(w - sidebar_w - 18, h);
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    poll_futures();
}

// ============================================================================
// Sidebar
// ============================================================================
void QuantLibScreen::render_sidebar(float /*height*/) {
    using namespace theme::colors;

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ql_sidebar_header", ImVec2(-1, 44), false);
    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::TextColored(ACCENT, "QUANTLIB");
    ImGui::SetCursorPos(ImVec2(12, 26));
    ImGui::TextColored(TEXT_DIM, "590 ENDPOINTS | 18 MODULES");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Accent line
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(p,
        ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 2),
        IM_COL32(255, 136, 0, 255));
    ImGui::Dummy(ImVec2(0, 2));

    // Find which module the active section belongs to
    std::string active_module_id;
    for (auto& mod : modules_) {
        for (auto& sec : mod.sections) {
            if (sec.id == active_section_) {
                active_module_id = mod.id;
                break;
            }
        }
        if (!active_module_id.empty()) break;
    }

    // Module list
    for (auto& mod : modules_) {
        bool is_expanded = expanded_modules_.count(mod.id) > 0;
        bool is_active_module = (mod.id == active_module_id);

        ImVec4 header_bg = is_active_module
            ? ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.03f)
            : ImVec4(0, 0, 0, 0);
        ImGui::PushStyleColor(ImGuiCol_Header, header_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, header_bg);

        char label[128];
        std::snprintf(label, sizeof(label), "%s %s",
            is_expanded ? "v" : ">", mod.label.c_str());

        bool clicked = ImGui::Selectable(label, false,
            ImGuiSelectableFlags_None, ImVec2(0, 22));
        ImGui::PopStyleColor(3);

        // Endpoint count on right
        {
            char count_str[16];
            std::snprintf(count_str, sizeof(count_str), "%d", mod.endpoint_count);
            float count_w = ImGui::CalcTextSize(count_str).x;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - count_w + 4);
            ImGui::TextColored(TEXT_DIM, "%s", count_str);
        }

        if (clicked) {
            if (is_expanded) expanded_modules_.erase(mod.id);
            else expanded_modules_.insert(mod.id);
        }

        ImGui::Separator();

        // Section items
        if (is_expanded) {
            for (auto& sec : mod.sections) {
                bool is_active = (sec.id == active_section_);

                ImGui::PushStyleColor(ImGuiCol_Header, is_active
                    ? ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.08f)
                    : ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text, is_active ? ACCENT : TEXT_SECONDARY);

                if (is_active) {
                    ImVec2 sp = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        sp, ImVec2(sp.x + 2, sp.y + 20),
                        IM_COL32(255, 136, 0, 255));
                }

                char sec_label[128];
                std::snprintf(sec_label, sizeof(sec_label), "    %s", sec.label.c_str());
                if (ImGui::Selectable(sec_label, is_active,
                    ImGuiSelectableFlags_None, ImVec2(0, 20))) {
                    active_section_ = sec.id;
                }

                ImGui::PopStyleColor(3);
            }
        }
    }
}

// ============================================================================
// Content dispatch
// ============================================================================
void QuantLibScreen::render_content(float /*width*/, float /*height*/) {
    using namespace theme::colors;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));

    // Section header
    std::string section_label, module_label;
    for (auto& mod : modules_) {
        for (auto& sec : mod.sections) {
            if (sec.id == active_section_) {
                section_label = sec.label;
                module_label = mod.label;
                break;
            }
        }
        if (!section_label.empty()) break;
    }

    ImGui::TextColored(ACCENT, "%s", module_label.c_str());
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, ">");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_PRIMARY, "%s", section_label.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    // Dispatch to section renderer
    const auto& s = active_section_;
    // Core
    if      (s == "types")          render_types();
    else if (s == "conventions")    render_conventions();
    else if (s == "autodiff")       render_autodiff();
    else if (s == "distributions")  render_distributions();
    else if (s == "math")           render_math();
    else if (s == "operations")     render_operations();
    else if (s == "legs")           render_legs();
    else if (s == "periods")        render_periods();
    // Regulatory
    else if (s == "basel")          render_basel();
    else if (s == "saccr")          render_saccr();
    else if (s == "ifrs9")          render_ifrs9();
    else if (s == "liquidity")      render_liquidity();
    else if (s == "stress")         render_stress();
    // Volatility
    else if (s == "surface")        render_surface();
    else if (s == "sabr")           render_sabr();
    else if (s == "localvol")       render_localvol();
    // Models
    else if (s == "shortrate")      render_shortrate();
    else if (s == "hullwhite")      render_hullwhite();
    else if (s == "heston")         render_heston();
    else if (s == "jumpdiff")       render_jumpdiff();
    else if (s == "volmodels")      render_volmodels();
    // Stochastic
    else if (s == "processes")      render_processes();
    else if (s == "exact")          render_exact();
    else if (s == "simulation")     render_simulation();
    else if (s == "sampling")       render_sampling();
    else if (s == "theory")         render_theory();
    // ML
    else if (s == "ml-credit")      render_ml_credit();
    else if (s == "ml-regression")  render_ml_regression();
    else if (s == "ml-clustering")  render_ml_clustering();
    else if (s == "ml-preprocessing") render_ml_preprocessing();
    else if (s == "ml-features")    render_ml_features();
    else if (s == "ml-validation")  render_ml_validation();
    else if (s == "ml-timeseries")  render_ml_timeseries();
    else if (s == "ml-gpnn")        render_ml_gpnn();
    else if (s == "ml-factor")      render_ml_factor();
    // Statistics
    else if (s == "stat-continuous")  render_stat_continuous();
    else if (s == "stat-discrete")    render_stat_discrete();
    else if (s == "stat-timeseries")  render_stat_timeseries();
    // Portfolio
    else if (s == "port-optimize")  render_port_optimize();
    else if (s == "port-risk")      render_port_risk();
    // Scheduling
    else if (s == "sched-calendar") render_sched_calendar();
    else if (s == "sched-daycount") render_sched_daycount();
    // Physics
    else if (s == "phys-entropy")   render_phys_entropy();
    else if (s == "phys-thermo")    render_phys_thermo();
    // Curves
    else if (s == "curves-build")       render_curves_build();
    else if (s == "curves-transform")   render_curves_transform();
    else if (s == "curves-fitting")     render_curves_fitting();
    else if (s == "curves-specialized") render_curves_specialized();
    // Pricing
    else if (s == "pricing-bs")         render_pricing_bs();
    else if (s == "pricing-black76")    render_pricing_black76();
    else if (s == "pricing-bachelier")  render_pricing_bachelier();
    else if (s == "pricing-numerical")  render_pricing_numerical();
    // Risk
    else if (s == "risk-var")           render_risk_var();
    else if (s == "risk-evt-xva")       render_risk_evt_xva();
    else if (s == "risk-sensitivities") render_risk_sensitivities();
    // Economics
    else if (s == "econ-equilibrium")   render_econ_equilibrium();
    else if (s == "econ-gametheory")    render_econ_gametheory();
    else if (s == "econ-auctions")      render_econ_auctions();
    else if (s == "econ-utility")       render_econ_utility();
    // Solver
    else if (s == "solver-bond")        render_solver_bond();
    else if (s == "solver-rates")       render_solver_rates();
    else if (s == "solver-cashflow")    render_solver_cashflow();
    // Numerical
    else if (s == "num-difffft")        render_num_difffft();
    else if (s == "num-interplinalg")   render_num_interplinalg();
    else if (s == "num-solvers")        render_num_solvers();
    // Instruments
    else if (s == "instr-bonds")        render_instr_bonds();
    else if (s == "instr-swaps")        render_instr_swaps();
    else if (s == "instr-markets")      render_instr_markets();
    else if (s == "instr-creditfut")    render_instr_creditfut();
    // Analysis
    else if (s == "analysis-fundamentals")  render_analysis_fundamentals();
    else if (s == "analysis-profit-liq")    render_analysis_profit_liq();
    else if (s == "analysis-eff-growth")    render_analysis_eff_growth();
    else if (s == "analysis-lev-cf")        render_analysis_lev_cf();
    else if (s == "analysis-val-qual")      render_analysis_val_qual();
    else if (s == "analysis-industry")      render_analysis_industry();
    else if (s == "analysis-dcf")           render_analysis_dcf();
    else if (s == "analysis-models")        render_analysis_models();
    else {
        ImGui::TextColored(TEXT_DIM, "Select a section from the sidebar.");
    }

    ImGui::PopStyleVar();
}

// ============================================================================
// UI Helpers
// ============================================================================

bool QuantLibScreen::begin_endpoint_card(const std::string& id, const char* title,
                                           const char* description) {
    using namespace theme::colors;
    auto& state = ep(id);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);

    char child_id[128];
    std::snprintf(child_id, sizeof(child_id), "##epc_%s", id.c_str());

    float card_h = state.expanded ? 0 : 30;
    if (!state.expanded) {
        ImGui::BeginChild(child_id, ImVec2(-1, card_h), true);
    } else {
        ImGui::BeginChild(child_id, ImVec2(-1, 0), true,
            ImGuiWindowFlags_AlwaysAutoResize);
    }

    // Header row
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);

    char btn_label[256];
    std::snprintf(btn_label, sizeof(btn_label), "%s %s##hdr_%s",
        state.expanded ? "v" : ">", title, id.c_str());
    if (ImGui::Button(btn_label, ImVec2(-1, 22))) {
        state.expanded = !state.expanded;
    }
    ImGui::PopStyleColor(3);

    if (description && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", description);
    }

    if (state.loading) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        ImGui::TextColored(WARNING, "Running...");
    }

    ImGui::PopStyleColor(2); // ChildBg, Border

    if (!state.expanded) {
        ImGui::EndChild();
        return false;
    }

    ImGui::Separator();
    return true;
}

void QuantLibScreen::end_endpoint_card() {
    ImGui::EndChild();
}

void QuantLibScreen::input_float(const std::string& ep_id, const char* label,
                                   const char* field, const char* default_val, float width) {
    using namespace theme::colors;
    auto& state = ep(ep_id);
    auto& val = state.buf(field, default_val);

    ImGui::BeginGroup();
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::SetNextItemWidth(width);

    char input_id[128];
    std::snprintf(input_id, sizeof(input_id), "##%s_%s", ep_id.c_str(), field);

    char buf[64];
    std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText(input_id, buf, sizeof(buf))) {
        val = buf;
    }
    ImGui::EndGroup();
    ImGui::SameLine();
}

void QuantLibScreen::input_text(const std::string& ep_id, const char* label,
                                  const char* field, const char* default_val, float width) {
    input_float(ep_id, label, field, default_val, width);
}

void QuantLibScreen::input_int(const std::string& ep_id, const char* label,
                                 const char* field, const char* default_val, float width) {
    input_float(ep_id, label, field, default_val, width);
}

void QuantLibScreen::input_array(const std::string& ep_id, const char* label,
                                   const char* field, const char* default_val, float width) {
    input_float(ep_id, label, field, default_val, width);
}

void QuantLibScreen::input_select(const std::string& ep_id, const char* label,
                                    const char* field, const std::vector<const char*>& options,
                                    float width) {
    using namespace theme::colors;
    auto& state = ep(ep_id);
    auto& val = state.buf(field, options.empty() ? "" : options[0]);

    ImGui::BeginGroup();
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::SetNextItemWidth(width);

    char combo_id[128];
    std::snprintf(combo_id, sizeof(combo_id), "##%s_%s", ep_id.c_str(), field);

    if (ImGui::BeginCombo(combo_id, val.c_str())) {
        for (auto* opt : options) {
            bool selected = (val == opt);
            if (ImGui::Selectable(opt, selected)) {
                val = opt;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndGroup();
    ImGui::SameLine();
}

bool QuantLibScreen::run_button(const std::string& ep_id) {
    using namespace theme::colors;
    auto& state = ep(ep_id);

    char btn_id[128];
    std::snprintf(btn_id, sizeof(btn_id), "Run##run_%s", ep_id.c_str());

    ImGui::NewLine();
    if (state.loading) {
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(TEXT_DIM.x, TEXT_DIM.y, TEXT_DIM.z, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Button("Running...##dis", ImVec2(90, 0));
        ImGui::PopStyleColor(2);
        return false;
    }

    bool clicked = theme::AccentButton(btn_id, ImVec2(90, 0));
    return clicked;
}

void QuantLibScreen::render_result(const std::string& ep_id) {
    using namespace theme::colors;
    auto& state = ep(ep_id);

    if (!state.has_result && state.error.empty()) return;

    ImGui::Spacing();

    if (!state.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.08f));
        ImGui::BeginChild(("##err_" + ep_id).c_str(), ImVec2(-1, 40), true);
        ImGui::TextColored(ERROR_RED, "%s", state.error.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    if (state.has_result) {
        ImGui::TextColored(TEXT_DIM, "%.0fms", state.elapsed_ms);
        ImGui::SameLine();

        if (ImGui::SmallButton(("Copy##cp_" + ep_id).c_str())) {
            std::string text = state.result_data.dump(2);
            ImGui::SetClipboardText(text.c_str());
        }

        std::string json_text = state.result_data.dump(2);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);

        float text_h = std::min(200.0f,
            ImGui::CalcTextSize(json_text.c_str()).y + 16);
        ImGui::BeginChild(("##res_" + ep_id).c_str(), ImVec2(-1, text_h), true);
        ImGui::PushStyleColor(ImGuiCol_Text, INFO);
        ImGui::TextWrapped("%s", json_text.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

void QuantLibScreen::fire_post(const std::string& ep_id, const std::string& api_path,
                                 const json& body) {
    auto& state = ep(ep_id);
    if (state.loading) return;

    state.loading = true;
    state.has_result = false;
    state.error.clear();
    state.future = QuantLibApi::instance().post(api_path, body);
}

void QuantLibScreen::fire_get(const std::string& ep_id, const std::string& api_path,
                                const std::map<std::string, std::string>& params) {
    auto& state = ep(ep_id);
    if (state.loading) return;

    state.loading = true;
    state.has_result = false;
    state.error.clear();
    state.future = QuantLibApi::instance().get(api_path, params);
}

void QuantLibScreen::poll_futures() {
    for (auto& [id, state] : endpoints_) {
        if (state.loading && state.future.valid() &&
            state.future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            auto result = state.future.get();
            state.loading = false;
            state.elapsed_ms = result.elapsed_ms;
            if (result.success) {
                state.result_data = result.data;
                state.has_result = true;
                state.error.clear();
            } else {
                state.error = result.error;
                state.has_result = false;
            }
        }
    }
}

// ============================================================================
// Phase 2: Core Panels
// ============================================================================

void QuantLibScreen::render_types() {
    // Tenor Add to Date
    if (begin_endpoint_card("types-tenor", "Tenor Add to Date", "Add a tenor period to a date")) {
        input_text("types-tenor", "Tenor", "tenor", "6M");
        input_text("types-tenor", "Start Date", "start_date", "2024-01-15");
        if (run_button("types-tenor")) {
            auto& s = ep("types-tenor");
            fire_post("types-tenor", "core/types/tenor/add-to-date", {
                {"tenor", s.get("tenor", "6M")},
                {"start_date", s.get("start_date", "2024-01-15")}
            });
        }
        render_result("types-tenor");
        end_endpoint_card();
    }

    // Rate Convert
    if (begin_endpoint_card("types-rate", "Rate Convert", "Convert between rate conventions")) {
        input_float("types-rate", "Value", "value", "0.05");
        input_select("types-rate", "Unit", "unit", {"percent", "bps", "decimal"});
        if (run_button("types-rate")) {
            auto& s = ep("types-rate");
            fire_post("types-rate", "core/types/rate/convert", {
                {"value", s.getd("value", 0.05)},
                {"unit", s.get("unit", "percent")}
            });
        }
        render_result("types-rate");
        end_endpoint_card();
    }

    // Money Create
    if (begin_endpoint_card("types-money-create", "Money Create", "Create money amount")) {
        input_float("types-money-create", "Amount", "amount", "1000000");
        input_text("types-money-create", "Currency", "currency", "USD");
        if (run_button("types-money-create")) {
            auto& s = ep("types-money-create");
            fire_post("types-money-create", "core/types/money/create", {
                {"amount", s.getd("amount")}, {"currency", s.get("currency", "USD")}
            });
        }
        render_result("types-money-create");
        end_endpoint_card();
    }

    // Money Convert
    if (begin_endpoint_card("types-money-convert", "Money Convert", "Convert between currencies")) {
        input_float("types-money-convert", "Amount", "amount", "1000000");
        input_text("types-money-convert", "From", "from_currency", "USD");
        input_text("types-money-convert", "To", "to_currency", "EUR");
        input_float("types-money-convert", "Rate", "rate", "0.92");
        if (run_button("types-money-convert")) {
            auto& s = ep("types-money-convert");
            fire_post("types-money-convert", "core/types/money/convert", {
                {"amount", s.getd("amount")}, {"from_currency", s.get("from_currency")},
                {"to_currency", s.get("to_currency")}, {"rate", s.getd("rate")}
            });
        }
        render_result("types-money-convert");
        end_endpoint_card();
    }

    // Spread from BPS
    if (begin_endpoint_card("types-spread", "Spread from BPS", "Convert basis points to spread")) {
        input_float("types-spread", "BPS", "bps", "250");
        if (run_button("types-spread")) {
            auto& s = ep("types-spread");
            fire_get("types-spread", "core/types/spread/from-bps",
                {{"bps", s.get("bps", "250")}});
        }
        render_result("types-spread");
        end_endpoint_card();
    }

    // Notional Schedule
    if (begin_endpoint_card("types-notional", "Notional Schedule", "Generate amortizing notional schedule")) {
        input_select("types-notional", "Type", "schedule_type", {"bullet", "linear", "annuity"});
        input_float("types-notional", "Notional", "notional", "1000000");
        input_int("types-notional", "Periods", "periods", "10");
        input_float("types-notional", "Rate", "rate", "0.05");
        if (run_button("types-notional")) {
            auto& s = ep("types-notional");
            fire_post("types-notional", "core/types/notional-schedule", {
                {"schedule_type", s.get("schedule_type", "bullet")},
                {"notional", s.getd("notional")}, {"periods", s.geti("periods", 10)},
                {"rate", s.getd("rate", 0.05)}
            });
        }
        render_result("types-notional");
        end_endpoint_card();
    }

    // List Currencies
    if (begin_endpoint_card("types-currencies", "List Currencies", "All supported currencies")) {
        if (run_button("types-currencies")) {
            fire_get("types-currencies", "core/types/currencies");
        }
        render_result("types-currencies");
        end_endpoint_card();
    }

    // List Frequencies
    if (begin_endpoint_card("types-frequencies", "List Frequencies", "All supported payment frequencies")) {
        if (run_button("types-frequencies")) {
            fire_get("types-frequencies", "core/types/frequencies");
        }
        render_result("types-frequencies");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Conventions (6 endpoints)
// ============================================================================
void QuantLibScreen::render_conventions() {
    if (begin_endpoint_card("conv-format", "Format Date", "Format a date string")) {
        input_text("conv-format", "Date", "date_str", "2024-01-15");
        input_text("conv-format", "Format", "format", "%Y-%m-%d");
        if (run_button("conv-format")) {
            auto& s = ep("conv-format");
            fire_post("conv-format", "core/conventions/format-date", {
                {"date_str", s.get("date_str")}, {"format", s.get("format")}
            });
        }
        render_result("conv-format");
        end_endpoint_card();
    }

    if (begin_endpoint_card("conv-parse", "Parse Date", "Parse a date string")) {
        input_text("conv-parse", "Date String", "date_string", "January 15, 2024");
        if (run_button("conv-parse")) {
            auto& s = ep("conv-parse");
            fire_post("conv-parse", "core/conventions/parse-date", {
                {"date_string", s.get("date_string")}
            });
        }
        render_result("conv-parse");
        end_endpoint_card();
    }

    if (begin_endpoint_card("conv-d2y", "Days to Years", "Convert day count to year fraction")) {
        input_float("conv-d2y", "Value", "value", "180");
        input_select("conv-d2y", "Convention", "convention",
            {"ACT/360", "ACT/365", "ACT/ACT", "30/360"});
        if (run_button("conv-d2y")) {
            auto& s = ep("conv-d2y");
            fire_post("conv-d2y", "core/conventions/days-to-years", {
                {"value", s.getd("value")}, {"convention", s.get("convention")}
            });
        }
        render_result("conv-d2y");
        end_endpoint_card();
    }

    if (begin_endpoint_card("conv-y2d", "Years to Days", "Convert year fraction to day count")) {
        input_float("conv-y2d", "Value", "value", "0.5");
        input_select("conv-y2d", "Convention", "convention",
            {"ACT/360", "ACT/365", "ACT/ACT", "30/360"});
        if (run_button("conv-y2d")) {
            auto& s = ep("conv-y2d");
            fire_post("conv-y2d", "core/conventions/years-to-days", {
                {"value", s.getd("value")}, {"convention", s.get("convention")}
            });
        }
        render_result("conv-y2d");
        end_endpoint_card();
    }

    if (begin_endpoint_card("conv-norm-rate", "Normalize Rate", "Normalize rate value")) {
        input_float("conv-norm-rate", "Value", "value", "5.0");
        input_select("conv-norm-rate", "Unit", "unit", {"percent", "bps", "decimal"});
        if (run_button("conv-norm-rate")) {
            auto& s = ep("conv-norm-rate");
            fire_post("conv-norm-rate", "core/conventions/normalize-rate", {
                {"value", s.getd("value")}, {"unit", s.get("unit")}
            });
        }
        render_result("conv-norm-rate");
        end_endpoint_card();
    }

    if (begin_endpoint_card("conv-norm-vol", "Normalize Volatility", "Normalize volatility value")) {
        input_float("conv-norm-vol", "Value", "value", "20");
        input_select("conv-norm-vol", "Type", "vol_type", {"normal", "lognormal"});
        input_select("conv-norm-vol", "Is Percent", "is_percent", {"true", "false"});
        if (run_button("conv-norm-vol")) {
            auto& s = ep("conv-norm-vol");
            fire_post("conv-norm-vol", "core/conventions/normalize-volatility", {
                {"value", s.getd("value")}, {"vol_type", s.get("vol_type")},
                {"is_percent", s.get("is_percent") == "true"}
            });
        }
        render_result("conv-norm-vol");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — AutoDiff (3 endpoints)
// ============================================================================
void QuantLibScreen::render_autodiff() {
    if (begin_endpoint_card("ad-dual", "Dual Eval", "Evaluate function with dual numbers")) {
        input_select("ad-dual", "Function", "func_name",
            {"exp", "log", "sin", "cos", "sqrt", "power"});
        input_float("ad-dual", "x", "x", "1.0");
        input_int("ad-dual", "Power N", "power_n", "2");
        if (run_button("ad-dual")) {
            auto& s = ep("ad-dual");
            fire_post("ad-dual", "core/autodiff/dual-eval", {
                {"func_name", s.get("func_name")},
                {"x", s.getd("x")}, {"power_n", s.geti("power_n", 2)}
            });
        }
        render_result("ad-dual");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ad-gradient", "Gradient", "Compute gradient vector")) {
        input_select("ad-gradient", "Function", "func_name",
            {"exp", "log", "sin", "cos", "sqrt", "power"});
        input_array("ad-gradient", "x[]", "x", "1.0, 2.0, 3.0");
        if (run_button("ad-gradient")) {
            auto& s = ep("ad-gradient");
            // Parse comma-separated array
            json x_arr = json::array();
            std::string x_str = s.get("x", "1.0, 2.0, 3.0");
            std::stringstream ss(x_str);
            std::string token;
            while (std::getline(ss, token, ',')) {
                try { x_arr.push_back(std::stod(token)); } catch (...) {}
            }
            fire_post("ad-gradient", "core/autodiff/gradient", {
                {"func_name", s.get("func_name")}, {"x", x_arr}
            });
        }
        render_result("ad-gradient");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ad-taylor", "Taylor Expand", "Taylor series expansion")) {
        input_select("ad-taylor", "Function", "func_name",
            {"exp", "log", "sin", "cos", "sqrt", "power"});
        input_float("ad-taylor", "x0", "x0", "0.0");
        input_int("ad-taylor", "Order", "order", "5");
        if (run_button("ad-taylor")) {
            auto& s = ep("ad-taylor");
            fire_post("ad-taylor", "core/autodiff/taylor-expand", {
                {"func_name", s.get("func_name")},
                {"x0", s.getd("x0")}, {"order", s.geti("order", 5)}
            });
        }
        render_result("ad-taylor");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Distributions (13 endpoints)
// ============================================================================
void QuantLibScreen::render_distributions() {
    auto dist_card = [&](const char* id_suffix, const char* title, const char* path,
                         const std::vector<std::pair<const char*, const char*>>& fields) {
        std::string eid = std::string("dist-") + id_suffix;
        if (begin_endpoint_card(eid, title)) {
            for (auto& [label, def_val] : fields) {
                input_float(eid, label, label, def_val);
            }
            if (run_button(eid)) {
                auto& s = ep(eid);
                json body;
                for (auto& [label, def_val] : fields) {
                    body[label] = s.getd(label, std::stod(def_val));
                }
                fire_post(eid, path, body);
            }
            render_result(eid);
            end_endpoint_card();
        }
    };

    // Normal
    dist_card("norm-cdf", "Normal CDF", "core/distributions/normal/cdf", {{"x", "0.0"}, {"mean", "0.0"}, {"std", "1.0"}});
    dist_card("norm-pdf", "Normal PDF", "core/distributions/normal/pdf", {{"x", "0.0"}, {"mean", "0.0"}, {"std", "1.0"}});
    dist_card("norm-ppf", "Normal PPF", "core/distributions/normal/ppf", {{"p", "0.975"}, {"mean", "0.0"}, {"std", "1.0"}});
    // Student-t
    dist_card("t-cdf", "Student-t CDF", "core/distributions/t/cdf", {{"x", "1.96"}, {"df", "10"}});
    dist_card("t-pdf", "Student-t PDF", "core/distributions/t/pdf", {{"x", "0.0"}, {"df", "10"}});
    dist_card("t-ppf", "Student-t PPF", "core/distributions/t/ppf", {{"p", "0.975"}, {"df", "10"}});
    // Chi-squared
    dist_card("chi2-cdf", "Chi-Squared CDF", "core/distributions/chi2/cdf", {{"x", "3.84"}, {"df", "1"}});
    dist_card("chi2-pdf", "Chi-Squared PDF", "core/distributions/chi2/pdf", {{"x", "1.0"}, {"df", "5"}});
    // Gamma
    dist_card("gamma-cdf", "Gamma CDF", "core/distributions/gamma/cdf", {{"x", "2.0"}, {"shape", "2.0"}, {"scale", "1.0"}});
    dist_card("gamma-pdf", "Gamma PDF", "core/distributions/gamma/pdf", {{"x", "1.0"}, {"shape", "2.0"}, {"scale", "1.0"}});
    // Exponential
    dist_card("exp-cdf", "Exponential CDF", "core/distributions/exponential/cdf", {{"x", "1.0"}, {"rate", "1.0"}});
    dist_card("exp-pdf", "Exponential PDF", "core/distributions/exponential/pdf", {{"x", "1.0"}, {"rate", "1.0"}});
    dist_card("exp-ppf", "Exponential PPF", "core/distributions/exponential/ppf", {{"p", "0.95"}, {"rate", "1.0"}});
}

// ============================================================================
// Core — Math (2 endpoints)
// ============================================================================
void QuantLibScreen::render_math() {
    if (begin_endpoint_card("math-eval", "Math Eval", "Evaluate mathematical function")) {
        input_select("math-eval", "Function", "func_name",
            {"exp", "log", "sin", "cos", "sqrt", "abs", "erf", "erfc", "lgamma"});
        input_float("math-eval", "x", "x", "1.0");
        if (run_button("math-eval")) {
            auto& s = ep("math-eval");
            fire_post("math-eval", "core/math/eval", {
                {"func_name", s.get("func_name")}, {"x", s.getd("x")}
            });
        }
        render_result("math-eval");
        end_endpoint_card();
    }

    if (begin_endpoint_card("math-two", "Two-Arg Math", "Two-argument math function")) {
        input_select("math-two", "Function", "func_name",
            {"pow", "fmod", "max", "min", "atan2", "copysign"});
        input_float("math-two", "x", "x", "2.0");
        input_float("math-two", "y", "y", "3.0");
        if (run_button("math-two")) {
            auto& s = ep("math-two");
            fire_post("math-two", "core/math/two-arg", {
                {"func_name", s.get("func_name")},
                {"x", s.getd("x")}, {"y", s.getd("y")}
            });
        }
        render_result("math-two");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Operations (12 endpoints)
// ============================================================================
void QuantLibScreen::render_operations() {
    if (begin_endpoint_card("ops-bs", "Black-Scholes", "Quick BS price")) {
        input_select("ops-bs", "Type", "option_type", {"call", "put"});
        input_float("ops-bs", "Spot", "spot", "100");
        input_float("ops-bs", "Strike", "strike", "100");
        input_float("ops-bs", "Rate", "rate", "0.05");
        input_float("ops-bs", "Time", "time", "1.0");
        input_float("ops-bs", "Vol", "volatility", "0.2");
        if (run_button("ops-bs")) {
            auto& s = ep("ops-bs");
            fire_post("ops-bs", "core/ops/black-scholes", {
                {"option_type", s.get("option_type")},
                {"spot", s.getd("spot")}, {"strike", s.getd("strike")},
                {"rate", s.getd("rate")}, {"time", s.getd("time")},
                {"volatility", s.getd("volatility")}
            });
        }
        render_result("ops-bs");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ops-b76", "Black76", "Black76 option price")) {
        input_select("ops-b76", "Type", "option_type", {"call", "put"});
        input_float("ops-b76", "Forward", "forward", "100");
        input_float("ops-b76", "Strike", "strike", "100");
        input_float("ops-b76", "Vol", "volatility", "0.2");
        input_float("ops-b76", "Time", "time", "1.0");
        input_float("ops-b76", "DF", "discount_factor", "0.95");
        if (run_button("ops-b76")) {
            auto& s = ep("ops-b76");
            fire_post("ops-b76", "core/ops/black76", {
                {"option_type", s.get("option_type")},
                {"forward", s.getd("forward")}, {"strike", s.getd("strike")},
                {"volatility", s.getd("volatility")}, {"time", s.getd("time")},
                {"discount_factor", s.getd("discount_factor")}
            });
        }
        render_result("ops-b76");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ops-var", "Value at Risk", "VaR calculation")) {
        input_select("ops-var", "Method", "method", {"parametric", "historical", "monte_carlo"});
        input_float("ops-var", "Portfolio Value", "portfolio_value", "1000000");
        input_float("ops-var", "Portfolio Std", "portfolio_std", "0.02");
        input_float("ops-var", "Confidence", "confidence", "0.95");
        input_int("ops-var", "Holding Period", "holding_period", "1");
        if (run_button("ops-var")) {
            auto& s = ep("ops-var");
            fire_post("ops-var", "core/ops/var", {
                {"method", s.get("method")},
                {"portfolio_value", s.getd("portfolio_value")},
                {"portfolio_std", s.getd("portfolio_std")},
                {"confidence", s.getd("confidence")},
                {"holding_period", s.geti("holding_period", 1)}
            });
        }
        render_result("ops-var");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ops-interp", "Interpolate", "1D interpolation")) {
        input_select("ops-interp", "Method", "method", {"linear", "cubic", "log_linear"});
        input_float("ops-interp", "x", "x", "2.5");
        input_array("ops-interp", "x_data", "x_data", "1,2,3,4,5");
        input_array("ops-interp", "y_data", "y_data", "1,4,9,16,25");
        if (run_button("ops-interp")) {
            auto& s = ep("ops-interp");
            auto parse_arr = [](const std::string& str) {
                json arr = json::array();
                std::stringstream ss(str);
                std::string tok;
                while (std::getline(ss, tok, ',')) {
                    try { arr.push_back(std::stod(tok)); } catch (...) {}
                }
                return arr;
            };
            fire_post("ops-interp", "core/ops/interpolate", {
                {"method", s.get("method")}, {"x", s.getd("x")},
                {"x_data", parse_arr(s.get("x_data"))},
                {"y_data", parse_arr(s.get("y_data"))}
            });
        }
        render_result("ops-interp");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ops-fwd", "Forward Rate", "Implied forward rate from discount factors")) {
        input_float("ops-fwd", "DF1", "df1", "0.98");
        input_float("ops-fwd", "DF2", "df2", "0.95");
        input_float("ops-fwd", "t1", "t1", "0.5");
        input_float("ops-fwd", "t2", "t2", "1.0");
        if (run_button("ops-fwd")) {
            auto& s = ep("ops-fwd");
            fire_post("ops-fwd", "core/ops/forward-rate", {
                {"df1", s.getd("df1")}, {"df2", s.getd("df2")},
                {"t1", s.getd("t1")}, {"t2", s.getd("t2")}
            });
        }
        render_result("ops-fwd");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ops-zero", "Zero Rate Convert", "Convert zero rate / discount factor")) {
        input_select("ops-zero", "Direction", "direction", {"rate_to_df", "df_to_rate"});
        input_float("ops-zero", "Value", "value", "0.05");
        input_float("ops-zero", "t", "t", "1.0");
        if (run_button("ops-zero")) {
            auto& s = ep("ops-zero");
            fire_post("ops-zero", "core/ops/zero-rate-convert", {
                {"direction", s.get("direction")},
                {"value", s.getd("value")}, {"t", s.getd("t")}
            });
        }
        render_result("ops-zero");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ops-gbm", "GBM Paths", "Geometric Brownian Motion simulation")) {
        input_float("ops-gbm", "Spot", "spot", "100");
        input_float("ops-gbm", "Drift", "drift", "0.05");
        input_float("ops-gbm", "Vol", "volatility", "0.2");
        input_float("ops-gbm", "Time", "time", "1.0");
        input_int("ops-gbm", "Paths", "n_paths", "100");
        input_int("ops-gbm", "Steps", "n_steps", "252");
        if (run_button("ops-gbm")) {
            auto& s = ep("ops-gbm");
            fire_post("ops-gbm", "core/ops/gbm-paths", {
                {"spot", s.getd("spot")}, {"drift", s.getd("drift")},
                {"volatility", s.getd("volatility")}, {"time", s.getd("time")},
                {"n_paths", s.geti("n_paths", 100)}, {"n_steps", s.geti("n_steps", 252)},
                {"seed", 42}, {"antithetic", true}
            });
        }
        render_result("ops-gbm");
        end_endpoint_card();
    }

    if (begin_endpoint_card("ops-stats", "Statistics", "Descriptive statistics on values")) {
        input_array("ops-stats", "Values", "values", "1.2, 2.3, 3.1, 4.5, 2.8");
        input_int("ops-stats", "DDOF", "ddof", "1");
        if (run_button("ops-stats")) {
            auto& s = ep("ops-stats");
            json vals = json::array();
            std::stringstream ss(s.get("values"));
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                try { vals.push_back(std::stod(tok)); } catch (...) {}
            }
            fire_post("ops-stats", "core/ops/statistics", {
                {"values", vals}, {"ddof", s.geti("ddof", 1)}
            });
        }
        render_result("ops-stats");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Legs (3 endpoints)
// ============================================================================
void QuantLibScreen::render_legs() {
    if (begin_endpoint_card("legs-fixed", "Fixed Leg", "Generate fixed leg cashflows")) {
        input_float("legs-fixed", "Notional", "notional", "1000000");
        input_float("legs-fixed", "Rate", "rate", "0.05");
        input_text("legs-fixed", "Start", "start_date", "2024-01-15");
        input_text("legs-fixed", "End", "end_date", "2029-01-15");
        input_select("legs-fixed", "Freq", "frequency", {"annual", "semi_annual", "quarterly"});
        input_select("legs-fixed", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        input_text("legs-fixed", "Currency", "currency", "USD");
        if (run_button("legs-fixed")) {
            auto& s = ep("legs-fixed");
            fire_post("legs-fixed", "core/legs/fixed", {
                {"notional", s.getd("notional")}, {"rate", s.getd("rate")},
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"frequency", s.get("frequency")}, {"day_count", s.get("day_count")},
                {"currency", s.get("currency")}
            });
        }
        render_result("legs-fixed");
        end_endpoint_card();
    }

    if (begin_endpoint_card("legs-float", "Float Leg", "Generate floating leg cashflows")) {
        input_float("legs-float", "Notional", "notional", "1000000");
        input_text("legs-float", "Start", "start_date", "2024-01-15");
        input_text("legs-float", "End", "end_date", "2029-01-15");
        input_float("legs-float", "Spread", "spread", "0.005");
        input_select("legs-float", "Freq", "frequency", {"quarterly", "semi_annual", "annual"});
        input_select("legs-float", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        input_text("legs-float", "Index", "index_name", "SOFR");
        if (run_button("legs-float")) {
            auto& s = ep("legs-float");
            fire_post("legs-float", "core/legs/float", {
                {"notional", s.getd("notional")},
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"spread", s.getd("spread")}, {"frequency", s.get("frequency")},
                {"day_count", s.get("day_count")}, {"index_name", s.get("index_name")}
            });
        }
        render_result("legs-float");
        end_endpoint_card();
    }

    if (begin_endpoint_card("legs-zero", "Zero-Coupon Leg", "Generate zero-coupon leg")) {
        input_float("legs-zero", "Notional", "notional", "1000000");
        input_float("legs-zero", "Rate", "rate", "0.05");
        input_text("legs-zero", "Start", "start_date", "2024-01-15");
        input_text("legs-zero", "End", "end_date", "2029-01-15");
        input_select("legs-zero", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        if (run_button("legs-zero")) {
            auto& s = ep("legs-zero");
            fire_post("legs-zero", "core/legs/zero-coupon", {
                {"notional", s.getd("notional")}, {"rate", s.getd("rate")},
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"day_count", s.get("day_count")}
            });
        }
        render_result("legs-zero");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Periods (3 endpoints)
// ============================================================================
void QuantLibScreen::render_periods() {
    if (begin_endpoint_card("per-fixed", "Fixed Coupon Period", "Calculate fixed coupon period")) {
        input_float("per-fixed", "Notional", "notional", "1000000");
        input_float("per-fixed", "Rate", "rate", "0.05");
        input_text("per-fixed", "Start", "start_date", "2024-01-15");
        input_text("per-fixed", "End", "end_date", "2024-07-15");
        input_select("per-fixed", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        if (run_button("per-fixed")) {
            auto& s = ep("per-fixed");
            fire_post("per-fixed", "core/periods/fixed-coupon", {
                {"notional", s.getd("notional")}, {"rate", s.getd("rate")},
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"day_count", s.get("day_count")}
            });
        }
        render_result("per-fixed");
        end_endpoint_card();
    }

    if (begin_endpoint_card("per-float", "Float Coupon Period", "Calculate floating coupon period")) {
        input_float("per-float", "Notional", "notional", "1000000");
        input_text("per-float", "Start", "start_date", "2024-01-15");
        input_text("per-float", "End", "end_date", "2024-07-15");
        input_float("per-float", "Spread", "spread", "0.005");
        input_select("per-float", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        input_float("per-float", "Fixing Rate", "fixing_rate", "0.04");
        if (run_button("per-float")) {
            auto& s = ep("per-float");
            fire_post("per-float", "core/periods/float-coupon", {
                {"notional", s.getd("notional")},
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"spread", s.getd("spread")}, {"day_count", s.get("day_count")},
                {"fixing_rate", s.getd("fixing_rate")}
            });
        }
        render_result("per-float");
        end_endpoint_card();
    }

    if (begin_endpoint_card("per-dcf", "Day Count Fraction", "Calculate day count fraction")) {
        input_text("per-dcf", "Start", "start_date", "2024-01-15");
        input_text("per-dcf", "End", "end_date", "2024-07-15");
        input_select("per-dcf", "Convention", "convention",
            {"ACT/360", "ACT/365", "ACT/ACT", "30/360", "30E/360"});
        if (run_button("per-dcf")) {
            auto& s = ep("per-dcf");
            fire_post("per-dcf", "core/periods/day-count-fraction", {
                {"start_date", s.get("start_date")},
                {"end_date", s.get("end_date")},
                {"convention", s.get("convention")}
            });
        }
        render_result("per-dcf");
        end_endpoint_card();
    }
}

// ============================================================================
// Stub panels — remaining 69 sections (to be implemented in phases 3-7)
// Each renders a "coming soon" placeholder
// ============================================================================

#define STUB_PANEL(name, label) \
void QuantLibScreen::name() { \
    using namespace theme::colors; \
    ImGui::TextColored(TEXT_DIM, label " — endpoints coming soon"); \
    ImGui::Spacing(); \
    ImGui::TextColored(TEXT_DISABLED, "This panel will contain full endpoint cards"); \
    ImGui::TextColored(TEXT_DISABLED, "with input fields, Run buttons, and JSON results."); \
}

// Regulatory
STUB_PANEL(render_basel,    "Basel III")
STUB_PANEL(render_saccr,   "SA-CCR")
STUB_PANEL(render_ifrs9,   "IFRS 9")
STUB_PANEL(render_liquidity,"Liquidity")
STUB_PANEL(render_stress,  "Stress Test")

// Volatility
STUB_PANEL(render_surface, "Volatility Surface")
STUB_PANEL(render_sabr,    "SABR")
STUB_PANEL(render_localvol,"Local Vol")

// Models
STUB_PANEL(render_shortrate,"Short Rate")
STUB_PANEL(render_hullwhite,"Hull-White")
STUB_PANEL(render_heston,  "Heston")
STUB_PANEL(render_jumpdiff,"Jump Diffusion")
STUB_PANEL(render_volmodels,"Dupire / SVI")

// Stochastic
STUB_PANEL(render_processes,"Stochastic Processes")
STUB_PANEL(render_exact,   "Exact Solutions")
STUB_PANEL(render_simulation,"Simulation")
STUB_PANEL(render_sampling,"Sampling Methods")
STUB_PANEL(render_theory,  "Probability Theory")

// ML
STUB_PANEL(render_ml_credit,       "ML Credit")
STUB_PANEL(render_ml_regression,   "ML Regression")
STUB_PANEL(render_ml_clustering,   "ML Clustering")
STUB_PANEL(render_ml_preprocessing,"ML Preprocessing")
STUB_PANEL(render_ml_features,     "ML Features")
STUB_PANEL(render_ml_validation,   "ML Validation")
STUB_PANEL(render_ml_timeseries,   "ML Time Series")
STUB_PANEL(render_ml_gpnn,         "GP / Neural Net")
STUB_PANEL(render_ml_factor,       "Factor / Cov")

// Statistics
STUB_PANEL(render_stat_continuous, "Continuous Distributions")
STUB_PANEL(render_stat_discrete,   "Discrete Distributions")
STUB_PANEL(render_stat_timeseries, "Time Series Stats")

// Portfolio
STUB_PANEL(render_port_optimize,   "Portfolio Optimization")
STUB_PANEL(render_port_risk,       "Portfolio Risk")

// Scheduling
STUB_PANEL(render_sched_calendar,  "Calendars")
STUB_PANEL(render_sched_daycount,  "Day Count")

// Physics
STUB_PANEL(render_phys_entropy,    "Entropy")
STUB_PANEL(render_phys_thermo,     "Thermodynamics")

// Curves
STUB_PANEL(render_curves_build,       "Curve Build & Query")
STUB_PANEL(render_curves_transform,   "Curve Transforms")
STUB_PANEL(render_curves_fitting,     "NS / NSS Fitting")
STUB_PANEL(render_curves_specialized, "Specialized Curves")

// Pricing
STUB_PANEL(render_pricing_bs,         "Black-Scholes Pricing")
STUB_PANEL(render_pricing_black76,    "Black76 Pricing")
STUB_PANEL(render_pricing_bachelier,  "Bachelier Pricing")
STUB_PANEL(render_pricing_numerical,  "Numerical Pricing")

// Risk
STUB_PANEL(render_risk_var,           "VaR / Stress")
STUB_PANEL(render_risk_evt_xva,       "EVT / XVA")
STUB_PANEL(render_risk_sensitivities, "Sensitivities")

// Economics
STUB_PANEL(render_econ_equilibrium,   "Equilibrium")
STUB_PANEL(render_econ_gametheory,    "Game Theory")
STUB_PANEL(render_econ_auctions,      "Auctions")
STUB_PANEL(render_econ_utility,       "Utility Theory")

// Solver
STUB_PANEL(render_solver_bond,        "Bond Analytics")
STUB_PANEL(render_solver_rates,       "Rates / IV")
STUB_PANEL(render_solver_cashflow,    "Cashflows")

// Numerical
STUB_PANEL(render_num_difffft,        "Diff / FFT / Integration")
STUB_PANEL(render_num_interplinalg,   "Interp / LinAlg")
STUB_PANEL(render_num_solvers,        "ODE / Roots / Opt")

// Instruments
STUB_PANEL(render_instr_bonds,        "Bonds")
STUB_PANEL(render_instr_swaps,        "Swaps / FRA")
STUB_PANEL(render_instr_markets,      "Market Instruments")
STUB_PANEL(render_instr_creditfut,    "Credit / Futures")

// Analysis
STUB_PANEL(render_analysis_fundamentals, "Fundamentals")
STUB_PANEL(render_analysis_profit_liq,   "Profit / Liquidity")
STUB_PANEL(render_analysis_eff_growth,   "Efficiency / Growth")
STUB_PANEL(render_analysis_lev_cf,       "Leverage / CF")
STUB_PANEL(render_analysis_val_qual,     "Val Ratios / Quality")
STUB_PANEL(render_analysis_industry,     "Industry Analysis")
STUB_PANEL(render_analysis_dcf,          "DCF Valuation")
STUB_PANEL(render_analysis_models,       "Valuation Models")

#undef STUB_PANEL

} // namespace fincept::quantlib
