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
// Regulatory Panels
// ============================================================================

void QuantLibScreen::render_basel() {
    if (begin_endpoint_card("basel-capital", "Capital Ratios", "Calculate CET1, Tier1, Total capital ratios")) {
        input_float("basel-capital", "CET1 Capital", "cet1_capital", "50000");
        input_float("basel-capital", "Tier1 Capital", "tier1_capital", "60000");
        input_float("basel-capital", "Total Capital", "total_capital", "80000");
        input_float("basel-capital", "RWA", "rwa", "500000");
        if (run_button("basel-capital")) {
            auto& s = ep("basel-capital");
            fire_post("basel-capital", "regulatory/basel/capital-ratios", {
                {"cet1_capital", s.getd("cet1_capital")}, {"tier1_capital", s.getd("tier1_capital")},
                {"total_capital", s.getd("total_capital")}, {"risk_weighted_assets", s.getd("rwa")}
            });
        }
        render_result("basel-capital");
        end_endpoint_card();
    }
    if (begin_endpoint_card("basel-oprwa", "Operational RWA", "Basic indicator approach")) {
        input_array("basel-oprwa", "Gross Income (3y)", "gross_income_3y", "100000,110000,120000");
        if (run_button("basel-oprwa")) {
            auto& s = ep("basel-oprwa");
            auto arr = json::array({100000, 110000, 120000});
            fire_post("basel-oprwa", "regulatory/basel/operational-rwa", {{"gross_income_3y", arr}});
        }
        render_result("basel-oprwa");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_saccr() {
    if (begin_endpoint_card("saccr-ead", "SA-CCR EAD", "Standardised approach for counterparty credit risk")) {
        input_float("saccr-ead", "MtM Value", "mtm_value", "1000000");
        input_float("saccr-ead", "Add-on", "addon", "50000");
        input_float("saccr-ead", "Collateral", "collateral", "200000");
        input_float("saccr-ead", "Alpha", "alpha", "1.4");
        if (run_button("saccr-ead")) {
            auto& s = ep("saccr-ead");
            fire_post("saccr-ead", "regulatory/saccr/ead", {
                {"mtm_value", s.getd("mtm_value")}, {"addon", s.getd("addon")},
                {"collateral", s.getd("collateral")}, {"alpha", s.getd("alpha", 1.4)}
            });
        }
        render_result("saccr-ead");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ifrs9() {
    if (begin_endpoint_card("ifrs9-stage", "Stage Assessment", "IFRS 9 staging classification")) {
        input_int("ifrs9-stage", "Days Past Due", "dpd", "30");
        input_float("ifrs9-stage", "Rating Downgrade", "downgrade", "2");
        input_float("ifrs9-stage", "PD Increase Ratio", "pd_ratio", "2.5");
        if (run_button("ifrs9-stage")) {
            auto& s = ep("ifrs9-stage");
            fire_post("ifrs9-stage", "regulatory/ifrs9/stage-assessment", {
                {"days_past_due", s.geti("dpd")}, {"rating_downgrade", s.getd("downgrade")},
                {"pd_increase_ratio", s.getd("pd_ratio")}
            });
        }
        render_result("ifrs9-stage");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ifrs9-ecl12m", "ECL 12-Month", "12-month expected credit loss")) {
        input_float("ifrs9-ecl12m", "PD 12m", "pd", "0.02");
        input_float("ifrs9-ecl12m", "LGD", "lgd", "0.45");
        input_float("ifrs9-ecl12m", "EAD", "ead", "1000000");
        input_float("ifrs9-ecl12m", "Discount Rate", "dr", "0.05");
        if (run_button("ifrs9-ecl12m")) {
            auto& s = ep("ifrs9-ecl12m");
            fire_post("ifrs9-ecl12m", "regulatory/ifrs9/ecl-12m", {
                {"pd_12m", s.getd("pd")}, {"lgd", s.getd("lgd")},
                {"ead", s.getd("ead")}, {"discount_rate", s.getd("dr")}
            });
        }
        render_result("ifrs9-ecl12m");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_liquidity() {
    if (begin_endpoint_card("liq-lcr", "Liquidity Coverage Ratio", "Basel III LCR calculation")) {
        input_float("liq-lcr", "HQLA Level 1", "hqla1", "500000");
        input_float("liq-lcr", "Retail Deposits (stable)", "rd_stable", "200000");
        input_float("liq-lcr", "Unsecured Wholesale", "wholesale", "300000");
        if (run_button("liq-lcr")) {
            auto& s = ep("liq-lcr");
            fire_post("liq-lcr", "regulatory/liquidity/lcr", {
                {"hqla_level1", s.getd("hqla1")},
                {"retail_deposits_stable", s.getd("rd_stable")},
                {"unsecured_wholesale", s.getd("wholesale")}
            });
        }
        render_result("liq-lcr");
        end_endpoint_card();
    }
    if (begin_endpoint_card("liq-nsfr", "Net Stable Funding Ratio", "Basel III NSFR calculation")) {
        input_float("liq-nsfr", "Capital", "capital", "100000");
        input_float("liq-nsfr", "Stable Deposits", "stable", "300000");
        input_float("liq-nsfr", "Loans Retail", "loans_retail", "400000");
        if (run_button("liq-nsfr")) {
            auto& s = ep("liq-nsfr");
            fire_post("liq-nsfr", "regulatory/liquidity/nsfr", {
                {"capital", s.getd("capital")}, {"stable_deposits_retail", s.getd("stable")},
                {"loans_retail", s.getd("loans_retail")}
            });
        }
        render_result("liq-nsfr");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_stress() {
    if (begin_endpoint_card("stress-proj", "Capital Projection", "Multi-period stress test capital projection")) {
        input_float("stress-proj", "Initial Capital", "capital", "100000");
        input_float("stress-proj", "Initial RWA", "rwa", "500000");
        input_array("stress-proj", "Earnings", "earnings", "10000,9000,8000");
        input_array("stress-proj", "Losses", "losses", "5000,15000,20000");
        if (run_button("stress-proj")) {
            auto& s = ep("stress-proj");
            fire_post("stress-proj", "regulatory/stress/capital-projection", {
                {"initial_capital", s.getd("capital")}, {"initial_rwa", s.getd("rwa")},
                {"earnings", json::array({10000,9000,8000})},
                {"losses", json::array({5000,15000,20000})},
                {"rwa_changes", json::array({0,10000,20000})}
            });
        }
        render_result("stress-proj");
        end_endpoint_card();
    }
}

// ============================================================================
// Volatility Panels
// ============================================================================

void QuantLibScreen::render_surface() {
    if (begin_endpoint_card("vol-flat", "Flat Surface", "Constant volatility surface")) {
        input_float("vol-flat", "Volatility", "vol", "0.20");
        input_float("vol-flat", "Query Expiry", "expiry", "1.0");
        input_float("vol-flat", "Query Strike", "strike", "100");
        if (run_button("vol-flat")) {
            auto& s = ep("vol-flat");
            fire_post("vol-flat", "volatility/surface/flat", {
                {"volatility", s.getd("vol")}, {"query_expiry", s.getd("expiry")}, {"query_strike", s.getd("strike")}
            });
        }
        render_result("vol-flat");
        end_endpoint_card();
    }
    if (begin_endpoint_card("vol-term", "Term Structure", "Volatility term structure interpolation")) {
        input_array("vol-term", "Expiries", "expiries", "0.25,0.5,1.0,2.0");
        input_array("vol-term", "Volatilities", "vols", "0.25,0.22,0.20,0.19");
        input_float("vol-term", "Query Expiry", "query", "0.75");
        if (run_button("vol-term")) {
            fire_post("vol-term", "volatility/surface/term-structure", {
                {"expiries", json::array({0.25,0.5,1.0,2.0})},
                {"volatilities", json::array({0.25,0.22,0.20,0.19})},
                {"query_expiry", ep("vol-term").getd("query", 0.75)}
            });
        }
        render_result("vol-term");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_sabr() {
    if (begin_endpoint_card("sabr-iv", "SABR Implied Vol", "SABR model implied volatility")) {
        input_float("sabr-iv", "Forward", "fwd", "100");
        input_float("sabr-iv", "Strike", "strike", "100");
        input_float("sabr-iv", "Expiry", "expiry", "1.0");
        input_float("sabr-iv", "Alpha", "alpha", "0.3");
        input_float("sabr-iv", "Beta", "beta", "0.5");
        input_float("sabr-iv", "Rho", "rho", "-0.3");
        input_float("sabr-iv", "Nu", "nu", "0.4");
        if (run_button("sabr-iv")) {
            auto& s = ep("sabr-iv");
            fire_post("sabr-iv", "volatility/sabr/implied-vol", {
                {"forward", s.getd("fwd")}, {"strike", s.getd("strike")}, {"expiry", s.getd("expiry")},
                {"alpha", s.getd("alpha")}, {"beta", s.getd("beta")}, {"rho", s.getd("rho")}, {"nu", s.getd("nu")}
            });
        }
        render_result("sabr-iv");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sabr-cal", "SABR Calibrate", "Calibrate SABR to market vols")) {
        input_float("sabr-cal", "Forward", "fwd", "100");
        input_float("sabr-cal", "Expiry", "expiry", "1.0");
        input_array("sabr-cal", "Strikes", "strikes", "90,95,100,105,110");
        input_array("sabr-cal", "Market Vols", "vols", "0.25,0.22,0.20,0.21,0.24");
        if (run_button("sabr-cal")) {
            fire_post("sabr-cal", "volatility/sabr/calibrate", {
                {"forward", ep("sabr-cal").getd("fwd")}, {"expiry", ep("sabr-cal").getd("expiry")},
                {"strikes", json::array({90,95,100,105,110})},
                {"market_vols", json::array({0.25,0.22,0.20,0.21,0.24})}
            });
        }
        render_result("sabr-cal");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_localvol() {
    if (begin_endpoint_card("lv-const", "Constant Local Vol", "Constant local volatility")) {
        input_float("lv-const", "Volatility", "vol", "0.20");
        input_float("lv-const", "Spot", "spot", "100");
        input_float("lv-const", "Time", "time", "1.0");
        if (run_button("lv-const")) {
            auto& s = ep("lv-const");
            fire_post("lv-const", "volatility/local-vol/constant", {
                {"volatility", s.getd("vol")}, {"spot", s.getd("spot")}, {"time", s.getd("time")}
            });
        }
        render_result("lv-const");
        end_endpoint_card();
    }
    if (begin_endpoint_card("lv-impl", "Implied to Local", "Convert implied vol to local vol via Dupire")) {
        input_float("lv-impl", "Spot", "spot", "100");
        input_float("lv-impl", "Rate", "rate", "0.05");
        input_float("lv-impl", "Strike", "strike", "100");
        input_float("lv-impl", "Expiry", "expiry", "1.0");
        input_float("lv-impl", "Implied Vol", "iv", "0.20");
        if (run_button("lv-impl")) {
            auto& s = ep("lv-impl");
            fire_post("lv-impl", "volatility/local-vol/implied-to-local", {
                {"spot", s.getd("spot")}, {"rate", s.getd("rate")}, {"strike", s.getd("strike")},
                {"expiry", s.getd("expiry")}, {"implied_vol", s.getd("iv")}
            });
        }
        render_result("lv-impl");
        end_endpoint_card();
    }
}

// ============================================================================
// Models Panels
// ============================================================================

void QuantLibScreen::render_shortrate() {
    if (begin_endpoint_card("sr-bond", "Bond Price", "Short rate model bond pricing")) {
        input_select("sr-bond", "Model", "model", {"vasicek", "cir", "hw"});
        input_float("sr-bond", "Kappa", "kappa", "0.5");
        input_float("sr-bond", "Theta", "theta", "0.05");
        input_float("sr-bond", "Sigma", "sigma", "0.01");
        input_float("sr-bond", "r0", "r0", "0.03");
        if (run_button("sr-bond")) {
            auto& s = ep("sr-bond");
            fire_post("sr-bond", "models/short-rate/bond-price", {
                {"model", s.get("model", "vasicek")}, {"kappa", s.getd("kappa")},
                {"theta", s.getd("theta")}, {"sigma", s.getd("sigma")}, {"r0", s.getd("r0")}
            });
        }
        render_result("sr-bond");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sr-mc", "Monte Carlo", "Short rate Monte Carlo simulation")) {
        input_select("sr-mc", "Model", "model", {"vasicek", "cir", "hw"});
        input_float("sr-mc", "r0", "r0", "0.03"); input_float("sr-mc", "T", "T", "1.0");
        input_int("sr-mc", "Steps", "steps", "100"); input_int("sr-mc", "Paths", "paths", "1000");
        if (run_button("sr-mc")) {
            auto& s = ep("sr-mc");
            fire_post("sr-mc", "models/short-rate/monte-carlo", {
                {"model", s.get("model")}, {"r0", s.getd("r0")}, {"T", s.getd("T")},
                {"n_steps", s.geti("steps")}, {"n_paths", s.geti("paths")}
            });
        }
        render_result("sr-mc");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_hullwhite() {
    if (begin_endpoint_card("hw-cal", "Hull-White Calibrate", "Calibrate Hull-White model to market")) {
        input_float("hw-cal", "Kappa", "kappa", "0.1");
        input_float("hw-cal", "Sigma", "sigma", "0.01");
        input_float("hw-cal", "r0", "r0", "0.03");
        if (run_button("hw-cal")) {
            auto& s = ep("hw-cal");
            fire_post("hw-cal", "models/hull-white/calibrate", {
                {"kappa", s.getd("kappa")}, {"sigma", s.getd("sigma")}, {"r0", s.getd("r0")}
            });
        }
        render_result("hw-cal");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_heston() {
    if (begin_endpoint_card("hes-price", "Heston Price", "Heston stochastic vol option pricing")) {
        input_float("hes-price", "S0", "S0", "100"); input_float("hes-price", "v0", "v0", "0.04");
        input_float("hes-price", "r", "r", "0.05"); input_float("hes-price", "Kappa", "kappa", "2.0");
        input_float("hes-price", "Theta", "theta", "0.04"); input_float("hes-price", "Sigma_v", "sigma_v", "0.3");
        input_float("hes-price", "Rho", "rho", "-0.7"); input_float("hes-price", "Strike", "strike", "100");
        input_float("hes-price", "T", "T", "1.0");
        if (run_button("hes-price")) {
            auto& s = ep("hes-price");
            fire_post("hes-price", "models/heston/price", {
                {"S0", s.getd("S0")}, {"v0", s.getd("v0")}, {"r", s.getd("r")},
                {"kappa", s.getd("kappa")}, {"theta", s.getd("theta")}, {"sigma_v", s.getd("sigma_v")},
                {"rho", s.getd("rho")}, {"strike", s.getd("strike")}, {"T", s.getd("T")}
            });
        }
        render_result("hes-price");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_jumpdiff() {
    if (begin_endpoint_card("jd-merton", "Merton Jump-Diffusion", "Merton model option pricing")) {
        input_float("jd-merton", "S", "S", "100"); input_float("jd-merton", "K", "K", "100");
        input_float("jd-merton", "T", "T", "1.0"); input_float("jd-merton", "r", "r", "0.05");
        input_float("jd-merton", "Sigma", "sigma", "0.2"); input_float("jd-merton", "Lambda", "lambda", "0.1");
        input_float("jd-merton", "Jump Mean", "mu_j", "-0.1"); input_float("jd-merton", "Jump Std", "sigma_j", "0.3");
        if (run_button("jd-merton")) {
            auto& s = ep("jd-merton");
            fire_post("jd-merton", "models/merton/price", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")}, {"r", s.getd("r")},
                {"sigma", s.getd("sigma")}, {"lambda_jump", s.getd("lambda")},
                {"mu_jump", s.getd("mu_j")}, {"sigma_jump", s.getd("sigma_j")}
            });
        }
        render_result("jd-merton");
        end_endpoint_card();
    }
    if (begin_endpoint_card("jd-kou", "Kou Double Exponential", "Kou model option pricing")) {
        input_float("jd-kou", "S", "S", "100"); input_float("jd-kou", "K", "K", "100");
        input_float("jd-kou", "T", "T", "1.0"); input_float("jd-kou", "r", "r", "0.05");
        input_float("jd-kou", "Sigma", "sigma", "0.2"); input_float("jd-kou", "Lambda", "lambda", "0.1");
        input_float("jd-kou", "p (up prob)", "p", "0.4");
        input_float("jd-kou", "Eta1", "eta1", "10"); input_float("jd-kou", "Eta2", "eta2", "5");
        if (run_button("jd-kou")) {
            auto& s = ep("jd-kou");
            fire_post("jd-kou", "models/kou/price", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")}, {"r", s.getd("r")},
                {"sigma", s.getd("sigma")}, {"lambda_jump", s.getd("lambda")},
                {"p", s.getd("p")}, {"eta1", s.getd("eta1")}, {"eta2", s.getd("eta2")}
            });
        }
        render_result("jd-kou");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_volmodels() {
    if (begin_endpoint_card("dupire-price", "Dupire Price", "Local vol model pricing")) {
        input_float("dupire-price", "Spot", "spot", "100"); input_float("dupire-price", "r", "r", "0.05");
        input_float("dupire-price", "Sigma", "sigma", "0.2");
        input_float("dupire-price", "Strike", "strike", "100"); input_float("dupire-price", "T", "T", "1.0");
        if (run_button("dupire-price")) {
            auto& s = ep("dupire-price");
            fire_post("dupire-price", "models/dupire/price", {
                {"spot", s.getd("spot")}, {"r", s.getd("r")}, {"sigma", s.getd("sigma")},
                {"strike", s.getd("strike")}, {"T", s.getd("T")}
            });
        }
        render_result("dupire-price");
        end_endpoint_card();
    }
    if (begin_endpoint_card("svi-cal", "SVI Calibrate", "SVI volatility smile calibration")) {
        input_float("svi-cal", "Forward", "fwd", "100"); input_float("svi-cal", "T", "T", "1.0");
        input_array("svi-cal", "Strikes", "strikes", "90,95,100,105,110");
        input_array("svi-cal", "Market Vols", "vols", "0.25,0.22,0.20,0.21,0.24");
        if (run_button("svi-cal")) {
            fire_post("svi-cal", "models/svi/calibrate", {
                {"forward", ep("svi-cal").getd("fwd")}, {"T", ep("svi-cal").getd("T")},
                {"strikes", json::array({90,95,100,105,110})},
                {"market_vols", json::array({0.25,0.22,0.20,0.21,0.24})}
            });
        }
        render_result("svi-cal");
        end_endpoint_card();
    }
}

// ============================================================================
// Stochastic Panels
// ============================================================================

void QuantLibScreen::render_processes() {
    if (begin_endpoint_card("gbm-sim", "GBM Simulate", "Geometric Brownian Motion simulation")) {
        input_float("gbm-sim", "S0", "S0", "100"); input_float("gbm-sim", "Mu", "mu", "0.1");
        input_float("gbm-sim", "Sigma", "sigma", "0.2"); input_float("gbm-sim", "T", "T", "1.0");
        input_int("gbm-sim", "Steps", "steps", "252"); input_int("gbm-sim", "Paths", "paths", "5");
        if (run_button("gbm-sim")) {
            auto& s = ep("gbm-sim");
            fire_post("gbm-sim", "stochastic/gbm/simulate", {
                {"S0", s.getd("S0")}, {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")},
                {"T", s.getd("T")}, {"n_steps", s.geti("steps")}, {"n_paths", s.geti("paths")}
            });
        }
        render_result("gbm-sim");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ou-sim", "OU Simulate", "Ornstein-Uhlenbeck mean-reverting process")) {
        input_float("ou-sim", "X0", "X0", "0.05"); input_float("ou-sim", "Kappa", "kappa", "5.0");
        input_float("ou-sim", "Theta", "theta", "0.05"); input_float("ou-sim", "Sigma", "sigma", "0.01");
        input_float("ou-sim", "T", "T", "1.0");
        if (run_button("ou-sim")) {
            auto& s = ep("ou-sim");
            fire_post("ou-sim", "stochastic/ou/simulate", {
                {"X0", s.getd("X0")}, {"kappa", s.getd("kappa")}, {"theta", s.getd("theta")},
                {"sigma", s.getd("sigma")}, {"T", s.getd("T")}
            });
        }
        render_result("ou-sim");
        end_endpoint_card();
    }
    if (begin_endpoint_card("cir-sim", "CIR Simulate", "Cox-Ingersoll-Ross interest rate model")) {
        input_float("cir-sim", "r0", "r0", "0.05"); input_float("cir-sim", "Kappa", "kappa", "0.5");
        input_float("cir-sim", "Theta", "theta", "0.05"); input_float("cir-sim", "Sigma", "sigma", "0.1");
        input_float("cir-sim", "T", "T", "5.0");
        if (run_button("cir-sim")) {
            auto& s = ep("cir-sim");
            fire_post("cir-sim", "stochastic/cir/simulate", {
                {"r0", s.getd("r0")}, {"kappa", s.getd("kappa")}, {"theta", s.getd("theta")},
                {"sigma", s.getd("sigma")}, {"T", s.getd("T")}
            });
        }
        render_result("cir-sim");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_exact() {
    if (begin_endpoint_card("exact-gbm", "Exact GBM", "Exact analytical GBM solution")) {
        input_float("exact-gbm", "S0", "S0", "100"); input_float("exact-gbm", "Mu", "mu", "0.1");
        input_float("exact-gbm", "Sigma", "sigma", "0.2"); input_float("exact-gbm", "T", "T", "1.0");
        if (run_button("exact-gbm")) {
            auto& s = ep("exact-gbm");
            fire_post("exact-gbm", "stochastic/exact/gbm", {
                {"S0", s.getd("S0")}, {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")}, {"T", s.getd("T")}
            });
        }
        render_result("exact-gbm");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_simulation() {
    if (begin_endpoint_card("sim-euler", "Euler-Maruyama", "SDE numerical integration")) {
        input_float("sim-euler", "x0", "x0", "1.0"); input_float("sim-euler", "T", "T", "1.0");
        input_float("sim-euler", "Mu", "mu", "0.1"); input_float("sim-euler", "Sigma", "sigma", "0.2");
        input_int("sim-euler", "Steps", "steps", "1000"); input_int("sim-euler", "Paths", "paths", "10");
        if (run_button("sim-euler")) {
            auto& s = ep("sim-euler");
            fire_post("sim-euler", "stochastic/simulation/euler-maruyama", {
                {"x0", s.getd("x0")}, {"T", s.getd("T")}, {"mu", s.getd("mu")},
                {"sigma", s.getd("sigma")}, {"n_steps", s.geti("steps")}, {"n_paths", s.geti("paths")}
            });
        }
        render_result("sim-euler");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sim-milstein", "Milstein", "Higher-order SDE integration")) {
        input_float("sim-milstein", "x0", "x0", "1.0"); input_float("sim-milstein", "T", "T", "1.0");
        input_float("sim-milstein", "Mu", "mu", "0.1"); input_float("sim-milstein", "Sigma", "sigma", "0.2");
        if (run_button("sim-milstein")) {
            auto& s = ep("sim-milstein");
            fire_post("sim-milstein", "stochastic/simulation/milstein", {
                {"x0", s.getd("x0")}, {"T", s.getd("T")}, {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("sim-milstein");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_sampling() {
    if (begin_endpoint_card("samp-sobol", "Sobol Sequence", "Quasi-random low-discrepancy sequence")) {
        input_int("samp-sobol", "Dimensions", "dim", "2"); input_int("samp-sobol", "N Points", "n", "100");
        if (run_button("samp-sobol")) {
            auto& s = ep("samp-sobol");
            fire_post("samp-sobol", "stochastic/sampling/sobol", {{"dim", s.geti("dim")}, {"n", s.geti("n")}});
        }
        render_result("samp-sobol");
        end_endpoint_card();
    }
    if (begin_endpoint_card("samp-anti", "Antithetic Sampling", "Variance reduction via antithetic variates")) {
        input_int("samp-anti", "N", "n", "1000");
        if (run_button("samp-anti")) {
            fire_post("samp-anti", "stochastic/sampling/antithetic", {{"n", ep("samp-anti").geti("n")}});
        }
        render_result("samp-anti");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_theory() {
    if (begin_endpoint_card("th-qv", "Quadratic Variation", "Compute quadratic variation of a path")) {
        input_array("th-qv", "Path", "path", "1.0,1.1,1.05,1.15,1.2");
        input_array("th-qv", "Times", "times", "0,0.25,0.5,0.75,1.0");
        if (run_button("th-qv")) {
            fire_post("th-qv", "stochastic/theory/quadratic-variation", {
                {"path", json::array({1.0,1.1,1.05,1.15,1.2})},
                {"times", json::array({0,0.25,0.5,0.75,1.0})}
            });
        }
        render_result("th-qv");
        end_endpoint_card();
    }
    if (begin_endpoint_card("th-rnd", "Risk-Neutral Drift", "Girsanov risk-neutral drift")) {
        input_float("th-rnd", "Mu", "mu", "0.1"); input_float("th-rnd", "r", "r", "0.05");
        input_float("th-rnd", "Sigma", "sigma", "0.2");
        if (run_button("th-rnd")) {
            auto& s = ep("th-rnd");
            fire_post("th-rnd", "stochastic/theory/girsanov/risk-neutral-drift", {
                {"mu", s.getd("mu")}, {"r", s.getd("r")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("th-rnd");
        end_endpoint_card();
    }
}

// ============================================================================
// ML Panels
// ============================================================================

void QuantLibScreen::render_ml_credit() {
    if (begin_endpoint_card("ml-cr-disc", "Discrimination Metrics", "AUC, KS, Gini for credit scoring")) {
        input_array("ml-cr-disc", "y_true", "y_true", "0,0,1,1,0,1"); input_array("ml-cr-disc", "y_score", "y_score", "0.1,0.3,0.8,0.9,0.2,0.7");
        if (run_button("ml-cr-disc")) {
            fire_post("ml-cr-disc", "ml/credit/discrimination", {
                {"y_true", json::array({0,0,1,1,0,1})}, {"y_score", json::array({0.1,0.3,0.8,0.9,0.2,0.7})}
            });
        }
        render_result("ml-cr-disc");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-cr-perf", "Model Performance", "Credit model performance metrics")) {
        input_array("ml-cr-perf", "y_true", "y_true", "0,0,1,1,0,1"); input_array("ml-cr-perf", "y_score", "y_score", "0.1,0.3,0.8,0.9,0.2,0.7");
        if (run_button("ml-cr-perf")) {
            fire_post("ml-cr-perf", "ml/credit/performance", {
                {"y_true", json::array({0,0,1,1,0,1})}, {"y_score", json::array({0.1,0.3,0.8,0.9,0.2,0.7})}
            });
        }
        render_result("ml-cr-perf");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_regression() {
    if (begin_endpoint_card("ml-reg-fit", "Regression Fit", "Linear/Ridge/Lasso regression")) {
        input_select("ml-reg-fit", "Method", "method", {"ols", "ridge", "lasso", "elastic_net"});
        input_float("ml-reg-fit", "Alpha", "alpha", "1.0");
        if (run_button("ml-reg-fit")) {
            fire_post("ml-reg-fit", "ml/regression/fit", {
                {"X", json::array({json::array({1,2}), json::array({3,4}), json::array({5,6})})},
                {"y", json::array({1.5, 3.5, 5.5})}, {"method", ep("ml-reg-fit").get("method", "ols")}
            });
        }
        render_result("ml-reg-fit");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_clustering() {
    if (begin_endpoint_card("ml-cl-km", "K-Means", "K-Means clustering")) {
        input_int("ml-cl-km", "K Clusters", "k", "3"); input_int("ml-cl-km", "Max Iter", "max_iter", "100");
        if (run_button("ml-cl-km")) {
            fire_post("ml-cl-km", "ml/clustering/kmeans", {
                {"X", json::array({json::array({1,2}), json::array({1.5,1.8}), json::array({5,8}), json::array({8,8}), json::array({1,0.6}), json::array({9,11})})},
                {"n_clusters", ep("ml-cl-km").geti("k", 3)}
            });
        }
        render_result("ml-cl-km");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-cl-pca", "PCA", "Principal Component Analysis")) {
        input_int("ml-cl-pca", "N Components", "n", "2");
        if (run_button("ml-cl-pca")) {
            fire_post("ml-cl-pca", "ml/clustering/pca", {
                {"X", json::array({json::array({1,2,3}), json::array({4,5,6}), json::array({7,8,9})})},
                {"n_components", ep("ml-cl-pca").geti("n", 2)}
            });
        }
        render_result("ml-cl-pca");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_preprocessing() {
    if (begin_endpoint_card("ml-pp-scale", "Scale Data", "StandardScaler / MinMaxScaler / RobustScaler")) {
        input_select("ml-pp-scale", "Method", "method", {"standard", "minmax", "robust"});
        if (run_button("ml-pp-scale")) {
            fire_post("ml-pp-scale", "ml/preprocessing/scale", {
                {"X", json::array({json::array({1,200}), json::array({2,300}), json::array({3,400})})},
                {"method", ep("ml-pp-scale").get("method", "standard")}
            });
        }
        render_result("ml-pp-scale");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-pp-stat", "Stationarity Test", "ADF/KPSS stationarity tests")) {
        input_array("ml-pp-stat", "Data", "data", "1,2,3,4,5,6,7,8,9,10");
        if (run_button("ml-pp-stat")) {
            fire_post("ml-pp-stat", "ml/preprocessing/stationarity", {
                {"data", json::array({1,2,3,4,5,6,7,8,9,10})}
            });
        }
        render_result("ml-pp-stat");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_features() {
    if (begin_endpoint_card("ml-ft-tech", "Technical Features", "Generate technical indicator features")) {
        input_select("ml-ft-tech", "Indicator", "indicator", {"sma", "ema", "rsi", "macd", "bbands"});
        input_int("ml-ft-tech", "Period", "period", "14");
        input_array("ml-ft-tech", "Prices", "prices", "100,101,102,100,99,101,103,105,104,106");
        if (run_button("ml-ft-tech")) {
            fire_post("ml-ft-tech", "ml/features/technical", {
                {"prices", json::array({100,101,102,100,99,101,103,105,104,106})},
                {"indicator", ep("ml-ft-tech").get("indicator", "sma")},
                {"period", ep("ml-ft-tech").geti("period", 14)}
            });
        }
        render_result("ml-ft-tech");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-ft-roll", "Rolling Features", "Rolling window statistics")) {
        input_int("ml-ft-roll", "Window", "window", "5");
        input_select("ml-ft-roll", "Statistic", "stat", {"mean", "std", "skew", "kurt", "sharpe"});
        if (run_button("ml-ft-roll")) {
            fire_post("ml-ft-roll", "ml/features/rolling", {
                {"data", json::array({1,2,3,4,5,6,7,8,9,10})},
                {"window", ep("ml-ft-roll").geti("window", 5)},
                {"statistic", ep("ml-ft-roll").get("stat", "mean")}
            });
        }
        render_result("ml-ft-roll");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_validation() {
    if (begin_endpoint_card("ml-val-stab", "Stability (PSI)", "Population Stability Index")) {
        input_array("ml-val-stab", "Expected", "expected", "0.1,0.2,0.3,0.2,0.1,0.05,0.05");
        input_array("ml-val-stab", "Actual", "actual", "0.12,0.18,0.28,0.22,0.1,0.05,0.05");
        if (run_button("ml-val-stab")) {
            fire_post("ml-val-stab", "ml/validation/stability", {
                {"expected", json::array({0.1,0.2,0.3,0.2,0.1,0.05,0.05})},
                {"actual", json::array({0.12,0.18,0.28,0.22,0.1,0.05,0.05})}
            });
        }
        render_result("ml-val-stab");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_timeseries() {
    if (begin_endpoint_card("ml-ts-hmm", "HMM Regime Detection", "Hidden Markov Model for regime detection")) {
        input_array("ml-ts-hmm", "Returns", "returns", "0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005");
        input_int("ml-ts-hmm", "N States", "states", "2");
        if (run_button("ml-ts-hmm")) {
            fire_post("ml-ts-hmm", "ml/hmm/fit", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005})},
                {"n_states", ep("ml-ts-hmm").geti("states", 2)}
            });
        }
        render_result("ml-ts-hmm");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_gpnn() {
    if (begin_endpoint_card("ml-gp-curve", "GP Yield Curve", "Gaussian Process yield curve fitting")) {
        input_array("ml-gp-curve", "Tenors", "tenors", "0.25,0.5,1,2,5,10,30");
        input_array("ml-gp-curve", "Rates", "rates", "0.04,0.041,0.043,0.045,0.047,0.048,0.049");
        input_array("ml-gp-curve", "Query", "query", "0.5,1.5,3,7,15,20");
        if (run_button("ml-gp-curve")) {
            fire_post("ml-gp-curve", "ml/gp/curve", {
                {"tenors", json::array({0.25,0.5,1,2,5,10,30})},
                {"rates", json::array({0.04,0.041,0.043,0.045,0.047,0.048,0.049})},
                {"query_tenors", json::array({0.5,1.5,3,7,15,20})}
            });
        }
        render_result("ml-gp-curve");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_factor() {
    if (begin_endpoint_card("ml-fac-stat", "Statistical Factor Model", "PCA-based factor decomposition")) {
        input_int("ml-fac-stat", "N Factors", "n", "3");
        if (run_button("ml-fac-stat")) {
            fire_post("ml-fac-stat", "ml/factor/statistical", {
                {"returns", json::array({json::array({0.01,0.02,-0.01}), json::array({0.015,-0.01,0.02}), json::array({-0.005,0.01,0.005})})},
                {"n_factors", ep("ml-fac-stat").geti("n", 3)}
            });
        }
        render_result("ml-fac-stat");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-cov-est", "Covariance Estimation", "Shrinkage and factor covariance estimation")) {
        input_select("ml-cov-est", "Method", "method", {"sample", "ledoit_wolf", "oas", "factor"});
        if (run_button("ml-cov-est")) {
            fire_post("ml-cov-est", "ml/covariance/estimate", {
                {"returns", json::array({json::array({0.01,0.02}), json::array({0.015,-0.01}), json::array({-0.005,0.01})})},
                {"method", ep("ml-cov-est").get("method", "ledoit_wolf")}
            });
        }
        render_result("ml-cov-est");
        end_endpoint_card();
    }
}

// ============================================================================
// Statistics Panels
// ============================================================================

void QuantLibScreen::render_stat_continuous() {
    if (begin_endpoint_card("st-norm", "Normal Distribution", "Properties, PDF, CDF, PPF")) {
        input_float("st-norm", "Mu", "mu", "0"); input_float("st-norm", "Sigma", "sigma", "1");
        input_float("st-norm", "x", "x", "1.96");
        input_select("st-norm", "Function", "func", {"properties", "pdf", "cdf", "ppf"});
        if (run_button("st-norm")) {
            auto& s = ep("st-norm");
            std::string func = s.get("func", "properties");
            fire_post("st-norm", "statistics/distributions/normal/" + func, {
                {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")}, {"x", s.getd("x")}
            });
        }
        render_result("st-norm");
        end_endpoint_card();
    }
    if (begin_endpoint_card("st-lognorm", "Lognormal", "Lognormal distribution")) {
        input_float("st-lognorm", "Mu", "mu", "0"); input_float("st-lognorm", "Sigma", "sigma", "0.5");
        input_float("st-lognorm", "x", "x", "2.0");
        if (run_button("st-lognorm")) {
            auto& s = ep("st-lognorm");
            fire_post("st-lognorm", "statistics/distributions/lognormal/properties", {
                {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("st-lognorm");
        end_endpoint_card();
    }
    if (begin_endpoint_card("st-t", "Student-t", "Student-t distribution")) {
        input_float("st-t", "df", "df", "5"); input_float("st-t", "x", "x", "2.0");
        if (run_button("st-t")) {
            fire_post("st-t", "statistics/distributions/student-t/properties", {{"df", ep("st-t").getd("df")}});
        }
        render_result("st-t");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_stat_discrete() {
    if (begin_endpoint_card("st-binom", "Binomial", "Binomial distribution")) {
        input_int("st-binom", "n", "n", "10"); input_float("st-binom", "p", "p", "0.5");
        input_int("st-binom", "k", "k", "5");
        input_select("st-binom", "Function", "func", {"properties", "pmf", "cdf"});
        if (run_button("st-binom")) {
            auto& s = ep("st-binom");
            fire_post("st-binom", "statistics/distributions/binomial/" + s.get("func", "properties"), {
                {"n", s.geti("n")}, {"p", s.getd("p")}, {"k", s.geti("k")}
            });
        }
        render_result("st-binom");
        end_endpoint_card();
    }
    if (begin_endpoint_card("st-poisson", "Poisson", "Poisson distribution")) {
        input_float("st-poisson", "Lambda", "lam", "5.0"); input_int("st-poisson", "k", "k", "3");
        if (run_button("st-poisson")) {
            auto& s = ep("st-poisson");
            fire_post("st-poisson", "statistics/distributions/poisson/properties", {{"lam", s.getd("lam")}});
        }
        render_result("st-poisson");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_stat_timeseries() {
    if (begin_endpoint_card("st-arima", "ARIMA Fit", "Fit ARIMA(p,d,q) model")) {
        input_int("st-arima", "p", "p", "1"); input_int("st-arima", "d", "d", "1"); input_int("st-arima", "q", "q", "1");
        input_array("st-arima", "Data", "data", "100,102,101,105,108,107,110,112");
        if (run_button("st-arima")) {
            auto& s = ep("st-arima");
            fire_post("st-arima", "statistics/timeseries/arima/fit", {
                {"data", json::array({100,102,101,105,108,107,110,112})},
                {"p", s.geti("p")}, {"d", s.geti("d")}, {"q", s.geti("q")}
            });
        }
        render_result("st-arima");
        end_endpoint_card();
    }
    if (begin_endpoint_card("st-garch", "GARCH Fit", "Fit GARCH(p,q) volatility model")) {
        input_int("st-garch", "p", "p", "1"); input_int("st-garch", "q", "q", "1");
        input_array("st-garch", "Returns", "returns", "0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01");
        if (run_button("st-garch")) {
            fire_post("st-garch", "statistics/timeseries/garch/fit", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01})},
                {"p", ep("st-garch").geti("p")}, {"q", ep("st-garch").geti("q")}
            });
        }
        render_result("st-garch");
        end_endpoint_card();
    }
}

// ============================================================================
// Portfolio Panels
// ============================================================================

void QuantLibScreen::render_port_optimize() {
    if (begin_endpoint_card("pt-minsv", "Min Variance", "Minimum variance portfolio optimization")) {
        input_array("pt-minsv", "Expected Returns", "rets", "0.1,0.12,0.08");
        input_float("pt-minsv", "Risk-Free Rate", "rf", "0.03");
        if (run_button("pt-minsv")) {
            fire_post("pt-minsv", "portfolio/optimize/min-variance", {
                {"expected_returns", json::array({0.1,0.12,0.08})},
                {"covariance_matrix", json::array({json::array({0.04,0.006,0.002}), json::array({0.006,0.09,0.009}), json::array({0.002,0.009,0.01})})},
                {"rf_rate", ep("pt-minsv").getd("rf", 0.03)}
            });
        }
        render_result("pt-minsv");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pt-maxsr", "Max Sharpe", "Maximum Sharpe ratio portfolio")) {
        input_float("pt-maxsr", "Risk-Free Rate", "rf", "0.03");
        if (run_button("pt-maxsr")) {
            fire_post("pt-maxsr", "portfolio/optimize/max-sharpe", {
                {"expected_returns", json::array({0.1,0.12,0.08})},
                {"covariance_matrix", json::array({json::array({0.04,0.006,0.002}), json::array({0.006,0.09,0.009}), json::array({0.002,0.009,0.01})})},
                {"rf_rate", ep("pt-maxsr").getd("rf", 0.03)}
            });
        }
        render_result("pt-maxsr");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_port_risk() {
    if (begin_endpoint_card("pt-var", "Portfolio VaR", "Value at Risk computation")) {
        input_float("pt-var", "Confidence", "conf", "0.95");
        input_float("pt-var", "Portfolio Value", "pv", "1000000");
        input_select("pt-var", "Method", "method", {"parametric", "historical", "monte_carlo"});
        if (run_button("pt-var")) {
            auto& s = ep("pt-var");
            fire_post("pt-var", "portfolio/risk/var", {
                {"weights", json::array({0.4,0.35,0.25})},
                {"covariance_matrix", json::array({json::array({0.04,0.006,0.002}), json::array({0.006,0.09,0.009}), json::array({0.002,0.009,0.01})})},
                {"confidence", s.getd("conf")}, {"portfolio_value", s.getd("pv")}, {"method", s.get("method")}
            });
        }
        render_result("pt-var");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pt-ratios", "Performance Ratios", "Sharpe, Sortino, Calmar, Information ratio")) {
        input_float("pt-ratios", "Risk-Free Rate", "rf", "0.03");
        if (run_button("pt-ratios")) {
            fire_post("pt-ratios", "portfolio/risk/ratios", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01,0.008,-0.005})},
                {"rf_rate", ep("pt-ratios").getd("rf", 0.03)}
            });
        }
        render_result("pt-ratios");
        end_endpoint_card();
    }
}

// ============================================================================
// Scheduling Panels
// ============================================================================

void QuantLibScreen::render_sched_calendar() {
    if (begin_endpoint_card("sc-isbd", "Is Business Day", "Check if date is a business day")) {
        input_text("sc-isbd", "Date", "date", "2024-12-25");
        input_select("sc-isbd", "Calendar", "calendar", {"us", "uk", "de", "jp", "cn", "in"});
        if (run_button("sc-isbd")) {
            auto& s = ep("sc-isbd");
            fire_post("sc-isbd", "scheduling/calendar/is-business-day", {{"date", s.get("date")}, {"calendar", s.get("calendar")}});
        }
        render_result("sc-isbd");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sc-addbd", "Add Business Days", "Add N business days to a date")) {
        input_text("sc-addbd", "Date", "date", "2024-01-15");
        input_int("sc-addbd", "Days", "days", "10");
        input_select("sc-addbd", "Calendar", "calendar", {"us", "uk", "de", "jp"});
        if (run_button("sc-addbd")) {
            auto& s = ep("sc-addbd");
            fire_post("sc-addbd", "scheduling/calendar/add-business-days", {
                {"date", s.get("date")}, {"days", s.geti("days")}, {"calendar", s.get("calendar")}
            });
        }
        render_result("sc-addbd");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_sched_daycount() {
    if (begin_endpoint_card("sc-yf", "Year Fraction", "Compute year fraction between dates")) {
        input_text("sc-yf", "Start Date", "start", "2024-01-15");
        input_text("sc-yf", "End Date", "end", "2024-07-15");
        input_select("sc-yf", "Convention", "conv", {"act/360", "act/365", "30/360", "act/act"});
        if (run_button("sc-yf")) {
            auto& s = ep("sc-yf");
            fire_post("sc-yf", "scheduling/daycount/year-fraction", {
                {"start_date", s.get("start")}, {"end_date", s.get("end")}, {"convention", s.get("conv")}
            });
        }
        render_result("sc-yf");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sc-dc", "Day Count", "Count days between dates")) {
        input_text("sc-dc", "Start Date", "start", "2024-01-15");
        input_text("sc-dc", "End Date", "end", "2024-07-15");
        if (run_button("sc-dc")) {
            auto& s = ep("sc-dc");
            fire_post("sc-dc", "scheduling/daycount/day-count", {{"start_date", s.get("start")}, {"end_date", s.get("end")}});
        }
        render_result("sc-dc");
        end_endpoint_card();
    }
}

// ============================================================================
// Physics Panels
// ============================================================================

void QuantLibScreen::render_phys_entropy() {
    if (begin_endpoint_card("ph-ent", "Shannon Entropy", "Information entropy of a distribution")) {
        input_array("ph-ent", "Probabilities", "probs", "0.25,0.25,0.25,0.25");
        if (run_button("ph-ent")) {
            fire_post("ph-ent", "physics/entropy/shannon", {{"probabilities", json::array({0.25,0.25,0.25,0.25})}});
        }
        render_result("ph-ent");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ph-kl", "KL Divergence", "Kullback-Leibler divergence")) {
        input_array("ph-kl", "P", "p", "0.3,0.4,0.3"); input_array("ph-kl", "Q", "q", "0.33,0.34,0.33");
        if (run_button("ph-kl")) {
            fire_post("ph-kl", "physics/entropy/kl-divergence", {
                {"p", json::array({0.3,0.4,0.3})}, {"q", json::array({0.33,0.34,0.33})}
            });
        }
        render_result("ph-kl");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_phys_thermo() {
    if (begin_endpoint_card("ph-boltz", "Boltzmann Distribution", "Boltzmann energy distribution")) {
        input_array("ph-boltz", "Energy Levels", "energies", "0,1,2,3,4");
        input_float("ph-boltz", "Temperature", "temp", "300");
        if (run_button("ph-boltz")) {
            fire_post("ph-boltz", "physics/thermo/boltzmann", {
                {"energy_levels", json::array({0,1,2,3,4})}, {"temperature", ep("ph-boltz").getd("temp")}
            });
        }
        render_result("ph-boltz");
        end_endpoint_card();
    }
}

// ============================================================================
// Curves Panels
// ============================================================================

void QuantLibScreen::render_curves_build() {
    if (begin_endpoint_card("cv-build", "Build Curve", "Construct yield curve from market data")) {
        input_array("cv-build", "Tenors", "tenors", "0.25,0.5,1,2,5,10,30");
        input_array("cv-build", "Rates", "rates", "0.04,0.041,0.043,0.045,0.047,0.048,0.049");
        input_select("cv-build", "Method", "method", {"linear", "cubic_spline", "monotone_cubic"});
        if (run_button("cv-build")) {
            fire_post("cv-build", "curves/build", {
                {"tenors", json::array({0.25,0.5,1,2,5,10,30})},
                {"rates", json::array({0.04,0.041,0.043,0.045,0.047,0.048,0.049})},
                {"method", ep("cv-build").get("method", "cubic_spline")}
            });
        }
        render_result("cv-build");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_curves_transform() {
    if (begin_endpoint_card("cv-fwd", "Forward Rates", "Extract forward rates from yield curve")) {
        input_array("cv-fwd", "Tenors", "tenors", "0.25,0.5,1,2,5,10");
        input_array("cv-fwd", "Rates", "rates", "0.04,0.041,0.043,0.045,0.047,0.048");
        if (run_button("cv-fwd")) {
            fire_post("cv-fwd", "curves/forward-rates", {
                {"tenors", json::array({0.25,0.5,1,2,5,10})},
                {"rates", json::array({0.04,0.041,0.043,0.045,0.047,0.048})}
            });
        }
        render_result("cv-fwd");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_curves_fitting() {
    if (begin_endpoint_card("cv-ns", "Nelson-Siegel Fit", "Fit NS/NSS parametric model to yields")) {
        input_array("cv-ns", "Tenors", "tenors", "0.25,0.5,1,2,5,10,30");
        input_array("cv-ns", "Rates", "rates", "0.04,0.041,0.043,0.045,0.047,0.048,0.049");
        input_select("cv-ns", "Model", "model", {"nelson_siegel", "svensson"});
        if (run_button("cv-ns")) {
            fire_post("cv-ns", "curves/fitting/nelson-siegel", {
                {"tenors", json::array({0.25,0.5,1,2,5,10,30})},
                {"rates", json::array({0.04,0.041,0.043,0.045,0.047,0.048,0.049})},
                {"model", ep("cv-ns").get("model", "nelson_siegel")}
            });
        }
        render_result("cv-ns");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_curves_specialized() {
    if (begin_endpoint_card("cv-disc", "Discount Factors", "Compute discount factors from yield curve")) {
        input_array("cv-disc", "Tenors", "tenors", "0.25,0.5,1,2,5,10");
        input_array("cv-disc", "Rates", "rates", "0.04,0.041,0.043,0.045,0.047,0.048");
        if (run_button("cv-disc")) {
            fire_post("cv-disc", "curves/discount-factors", {
                {"tenors", json::array({0.25,0.5,1,2,5,10})},
                {"rates", json::array({0.04,0.041,0.043,0.045,0.047,0.048})}
            });
        }
        render_result("cv-disc");
        end_endpoint_card();
    }
}

// ============================================================================
// Pricing Panels
// ============================================================================

void QuantLibScreen::render_pricing_bs() {
    if (begin_endpoint_card("pr-bs", "Black-Scholes", "European option pricing")) {
        input_float("pr-bs", "Spot", "S", "100"); input_float("pr-bs", "Strike", "K", "100");
        input_float("pr-bs", "T", "T", "1.0"); input_float("pr-bs", "r", "r", "0.05");
        input_float("pr-bs", "Sigma", "sigma", "0.2"); input_float("pr-bs", "q", "q", "0");
        input_select("pr-bs", "Type", "type", {"call", "put"});
        if (run_button("pr-bs")) {
            auto& s = ep("pr-bs");
            fire_post("pr-bs", "pricing/black-scholes/price", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")}, {"r", s.getd("r")},
                {"sigma", s.getd("sigma")}, {"q", s.getd("q")}, {"option_type", s.get("type", "call")}
            });
        }
        render_result("pr-bs");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pr-bs-greeks", "BS Greeks", "Option sensitivities")) {
        input_float("pr-bs-greeks", "S", "S", "100"); input_float("pr-bs-greeks", "K", "K", "100");
        input_float("pr-bs-greeks", "T", "T", "1.0"); input_float("pr-bs-greeks", "r", "r", "0.05");
        input_float("pr-bs-greeks", "Sigma", "sigma", "0.2");
        if (run_button("pr-bs-greeks")) {
            auto& s = ep("pr-bs-greeks");
            fire_post("pr-bs-greeks", "pricing/black-scholes/greeks", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("pr-bs-greeks");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_pricing_black76() {
    if (begin_endpoint_card("pr-b76", "Black76 Price", "Futures/forward option pricing")) {
        input_float("pr-b76", "Forward", "F", "100"); input_float("pr-b76", "Strike", "K", "100");
        input_float("pr-b76", "T", "T", "1.0"); input_float("pr-b76", "r", "r", "0.05");
        input_float("pr-b76", "Sigma", "sigma", "0.2");
        input_select("pr-b76", "Type", "type", {"call", "put"});
        if (run_button("pr-b76")) {
            auto& s = ep("pr-b76");
            fire_post("pr-b76", "pricing/black76/price", {
                {"F", s.getd("F")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}, {"option_type", s.get("type")}
            });
        }
        render_result("pr-b76");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_pricing_bachelier() {
    if (begin_endpoint_card("pr-bach", "Bachelier Price", "Normal model option pricing")) {
        input_float("pr-bach", "Forward", "F", "100"); input_float("pr-bach", "Strike", "K", "100");
        input_float("pr-bach", "T", "T", "1.0"); input_float("pr-bach", "r", "r", "0.05");
        input_float("pr-bach", "Sigma (normal)", "sigma", "20");
        input_select("pr-bach", "Type", "type", {"call", "put"});
        if (run_button("pr-bach")) {
            auto& s = ep("pr-bach");
            fire_post("pr-bach", "pricing/bachelier/price", {
                {"F", s.getd("F")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}, {"option_type", s.get("type")}
            });
        }
        render_result("pr-bach");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_pricing_numerical() {
    if (begin_endpoint_card("pr-binom", "Binomial Tree", "CRR binomial tree option pricing")) {
        input_float("pr-binom", "S", "S", "100"); input_float("pr-binom", "K", "K", "100");
        input_float("pr-binom", "T", "T", "1.0"); input_float("pr-binom", "r", "r", "0.05");
        input_float("pr-binom", "Sigma", "sigma", "0.2"); input_int("pr-binom", "Steps", "steps", "100");
        input_select("pr-binom", "Type", "type", {"call", "put"});
        input_select("pr-binom", "Style", "style", {"european", "american"});
        if (run_button("pr-binom")) {
            auto& s = ep("pr-binom");
            fire_post("pr-binom", "pricing/numerical/binomial", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")}, {"r", s.getd("r")},
                {"sigma", s.getd("sigma")}, {"n_steps", s.geti("steps")},
                {"option_type", s.get("type")}, {"style", s.get("style")}
            });
        }
        render_result("pr-binom");
        end_endpoint_card();
    }
}

// ============================================================================
// Risk Panels
// ============================================================================

void QuantLibScreen::render_risk_var() {
    if (begin_endpoint_card("rk-var", "Value at Risk", "Parametric/Historical/MC VaR")) {
        input_array("rk-var", "Returns", "returns", "0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01");
        input_float("rk-var", "Confidence", "conf", "0.95");
        input_select("rk-var", "Method", "method", {"parametric", "historical", "cornish_fisher"});
        if (run_button("rk-var")) {
            fire_post("rk-var", "risk/var", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01})},
                {"confidence", ep("rk-var").getd("conf")}, {"method", ep("rk-var").get("method")}
            });
        }
        render_result("rk-var");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_risk_evt_xva() {
    if (begin_endpoint_card("rk-evt", "EVT Tail Risk", "Extreme Value Theory tail estimation")) {
        input_array("rk-evt", "Losses", "losses", "0.01,0.02,0.03,0.05,0.08,0.12,0.15");
        input_float("rk-evt", "Threshold", "threshold", "0.05");
        if (run_button("rk-evt")) {
            fire_post("rk-evt", "risk/evt/fit", {
                {"losses", json::array({0.01,0.02,0.03,0.05,0.08,0.12,0.15})},
                {"threshold", ep("rk-evt").getd("threshold")}
            });
        }
        render_result("rk-evt");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_risk_sensitivities() {
    if (begin_endpoint_card("rk-sens", "Greeks / Sensitivities", "Compute option sensitivities")) {
        input_float("rk-sens", "S", "S", "100"); input_float("rk-sens", "K", "K", "100");
        input_float("rk-sens", "T", "T", "1.0"); input_float("rk-sens", "r", "r", "0.05");
        input_float("rk-sens", "Sigma", "sigma", "0.2");
        if (run_button("rk-sens")) {
            auto& s = ep("rk-sens");
            fire_post("rk-sens", "risk/sensitivities", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("rk-sens");
        end_endpoint_card();
    }
}

// ============================================================================
// Economics Panels
// ============================================================================

void QuantLibScreen::render_econ_equilibrium() {
    if (begin_endpoint_card("ec-eq", "Market Equilibrium", "Supply-demand equilibrium solver")) {
        input_float("ec-eq", "Supply Slope", "s_slope", "1.0");
        input_float("ec-eq", "Supply Intercept", "s_int", "0");
        input_float("ec-eq", "Demand Slope", "d_slope", "-1.0");
        input_float("ec-eq", "Demand Intercept", "d_int", "100");
        if (run_button("ec-eq")) {
            auto& s = ep("ec-eq");
            fire_post("ec-eq", "economics/equilibrium", {
                {"supply_slope", s.getd("s_slope")}, {"supply_intercept", s.getd("s_int")},
                {"demand_slope", s.getd("d_slope")}, {"demand_intercept", s.getd("d_int")}
            });
        }
        render_result("ec-eq");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_econ_gametheory() {
    if (begin_endpoint_card("ec-nash", "Nash Equilibrium", "Find Nash equilibria in 2-player games")) {
        input_array("ec-nash", "Payoff Matrix (flat)", "payoffs", "3,0,5,1,1,5,0,3");
        if (run_button("ec-nash")) {
            fire_post("ec-nash", "economics/game-theory/nash", {
                {"payoff_matrix", json::array({json::array({3,0}), json::array({5,1}), json::array({1,5}), json::array({0,3})})}
            });
        }
        render_result("ec-nash");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_econ_auctions() {
    if (begin_endpoint_card("ec-auc", "Auction Simulation", "First/Second price auction analysis")) {
        input_select("ec-auc", "Type", "type", {"first_price", "second_price", "english", "dutch"});
        input_array("ec-auc", "Bids", "bids", "10,15,12,8,20");
        if (run_button("ec-auc")) {
            fire_post("ec-auc", "economics/auctions/simulate", {
                {"auction_type", ep("ec-auc").get("type")},
                {"bids", json::array({10,15,12,8,20})}
            });
        }
        render_result("ec-auc");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_econ_utility() {
    if (begin_endpoint_card("ec-util", "Utility Function", "CRRA/CARA utility computation")) {
        input_select("ec-util", "Type", "type", {"crra", "cara", "log"});
        input_float("ec-util", "Wealth", "wealth", "1000000");
        input_float("ec-util", "Risk Aversion", "gamma", "2.0");
        if (run_button("ec-util")) {
            auto& s = ep("ec-util");
            fire_post("ec-util", "economics/utility", {
                {"utility_type", s.get("type")}, {"wealth", s.getd("wealth")}, {"gamma", s.getd("gamma")}
            });
        }
        render_result("ec-util");
        end_endpoint_card();
    }
}

// ============================================================================
// Solver Panels
// ============================================================================

void QuantLibScreen::render_solver_bond() {
    if (begin_endpoint_card("sv-bond-ytm", "Bond YTM", "Calculate yield to maturity")) {
        input_float("sv-bond-ytm", "Face Value", "face", "1000");
        input_float("sv-bond-ytm", "Price", "price", "980");
        input_float("sv-bond-ytm", "Coupon Rate", "coupon", "0.05");
        input_float("sv-bond-ytm", "Maturity (years)", "maturity", "10");
        input_int("sv-bond-ytm", "Frequency", "freq", "2");
        if (run_button("sv-bond-ytm")) {
            auto& s = ep("sv-bond-ytm");
            fire_post("sv-bond-ytm", "solver/bond/ytm", {
                {"face_value", s.getd("face")}, {"price", s.getd("price")},
                {"coupon_rate", s.getd("coupon")}, {"maturity", s.getd("maturity")},
                {"frequency", s.geti("freq")}
            });
        }
        render_result("sv-bond-ytm");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sv-bond-dur", "Bond Duration", "Macaulay and modified duration")) {
        input_float("sv-bond-dur", "Face Value", "face", "1000");
        input_float("sv-bond-dur", "Coupon Rate", "coupon", "0.05");
        input_float("sv-bond-dur", "YTM", "ytm", "0.06");
        input_float("sv-bond-dur", "Maturity", "maturity", "10");
        if (run_button("sv-bond-dur")) {
            auto& s = ep("sv-bond-dur");
            fire_post("sv-bond-dur", "solver/bond/duration", {
                {"face_value", s.getd("face")}, {"coupon_rate", s.getd("coupon")},
                {"ytm", s.getd("ytm")}, {"maturity", s.getd("maturity")}
            });
        }
        render_result("sv-bond-dur");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_solver_rates() {
    if (begin_endpoint_card("sv-iv", "Implied Volatility", "Solve for BS implied vol from market price")) {
        input_float("sv-iv", "Market Price", "price", "10.5"); input_float("sv-iv", "S", "S", "100");
        input_float("sv-iv", "K", "K", "100"); input_float("sv-iv", "T", "T", "1.0");
        input_float("sv-iv", "r", "r", "0.05");
        input_select("sv-iv", "Type", "type", {"call", "put"});
        if (run_button("sv-iv")) {
            auto& s = ep("sv-iv");
            fire_post("sv-iv", "solver/implied-volatility", {
                {"market_price", s.getd("price")}, {"S", s.getd("S")}, {"K", s.getd("K")},
                {"T", s.getd("T")}, {"r", s.getd("r")}, {"option_type", s.get("type")}
            });
        }
        render_result("sv-iv");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_solver_cashflow() {
    if (begin_endpoint_card("sv-irr", "IRR / NPV", "Internal rate of return and net present value")) {
        input_array("sv-irr", "Cash Flows", "cf", "-1000,200,300,400,500");
        input_float("sv-irr", "Discount Rate", "rate", "0.10");
        if (run_button("sv-irr")) {
            fire_post("sv-irr", "solver/cashflow/irr", {
                {"cashflows", json::array({-1000,200,300,400,500})},
                {"discount_rate", ep("sv-irr").getd("rate")}
            });
        }
        render_result("sv-irr");
        end_endpoint_card();
    }
}

// ============================================================================
// Numerical Panels
// ============================================================================

void QuantLibScreen::render_num_difffft() {
    if (begin_endpoint_card("nm-fft", "FFT", "Fast Fourier Transform")) {
        input_array("nm-fft", "Signal", "signal", "1,0,1,0,1,0,1,0");
        if (run_button("nm-fft")) {
            fire_post("nm-fft", "numerical/fft", {{"signal", json::array({1,0,1,0,1,0,1,0})}});
        }
        render_result("nm-fft");
        end_endpoint_card();
    }
    if (begin_endpoint_card("nm-integ", "Numerical Integration", "Trapezoidal/Simpson integration")) {
        input_text("nm-integ", "Function", "func", "x^2"); input_float("nm-integ", "a", "a", "0");
        input_float("nm-integ", "b", "b", "1"); input_int("nm-integ", "N", "n", "100");
        if (run_button("nm-integ")) {
            auto& s = ep("nm-integ");
            fire_post("nm-integ", "numerical/integrate", {
                {"function", s.get("func")}, {"a", s.getd("a")}, {"b", s.getd("b")}, {"n", s.geti("n")}
            });
        }
        render_result("nm-integ");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_num_interplinalg() {
    if (begin_endpoint_card("nm-interp", "Interpolation", "1D interpolation (linear, cubic, etc.)")) {
        input_array("nm-interp", "X", "x", "0,1,2,3,4");
        input_array("nm-interp", "Y", "y", "0,1,4,9,16");
        input_float("nm-interp", "Query", "query", "2.5");
        input_select("nm-interp", "Method", "method", {"linear", "cubic", "akima"});
        if (run_button("nm-interp")) {
            fire_post("nm-interp", "numerical/interpolate", {
                {"x", json::array({0,1,2,3,4})}, {"y", json::array({0,1,4,9,16})},
                {"query", ep("nm-interp").getd("query")}, {"method", ep("nm-interp").get("method")}
            });
        }
        render_result("nm-interp");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_num_solvers() {
    if (begin_endpoint_card("nm-root", "Root Finding", "Bisection/Newton/Brent root finder")) {
        input_text("nm-root", "Function", "func", "x^2 - 2");
        input_float("nm-root", "a", "a", "0"); input_float("nm-root", "b", "b", "2");
        input_select("nm-root", "Method", "method", {"bisection", "newton", "brent"});
        if (run_button("nm-root")) {
            auto& s = ep("nm-root");
            fire_post("nm-root", "numerical/root-find", {
                {"function", s.get("func")}, {"a", s.getd("a")}, {"b", s.getd("b")}, {"method", s.get("method")}
            });
        }
        render_result("nm-root");
        end_endpoint_card();
    }
}

// ============================================================================
// Instruments Panels
// ============================================================================

void QuantLibScreen::render_instr_bonds() {
    if (begin_endpoint_card("in-bond", "Bond Pricing", "Price a fixed-rate bond")) {
        input_float("in-bond", "Face Value", "face", "1000"); input_float("in-bond", "Coupon", "coupon", "0.05");
        input_float("in-bond", "YTM", "ytm", "0.06"); input_float("in-bond", "Maturity", "mat", "10");
        input_int("in-bond", "Frequency", "freq", "2");
        if (run_button("in-bond")) {
            auto& s = ep("in-bond");
            fire_post("in-bond", "instruments/bond/price", {
                {"face_value", s.getd("face")}, {"coupon_rate", s.getd("coupon")},
                {"ytm", s.getd("ytm")}, {"maturity", s.getd("mat")}, {"frequency", s.geti("freq")}
            });
        }
        render_result("in-bond");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_instr_swaps() {
    if (begin_endpoint_card("in-irs", "Interest Rate Swap", "IRS NPV and DV01")) {
        input_float("in-irs", "Notional", "notional", "10000000");
        input_float("in-irs", "Fixed Rate", "fixed_rate", "0.05");
        input_float("in-irs", "Float Spread", "spread", "0.001");
        input_float("in-irs", "Maturity", "maturity", "5");
        if (run_button("in-irs")) {
            auto& s = ep("in-irs");
            fire_post("in-irs", "instruments/swap/irs", {
                {"notional", s.getd("notional")}, {"fixed_rate", s.getd("fixed_rate")},
                {"float_spread", s.getd("spread")}, {"maturity", s.getd("maturity")}
            });
        }
        render_result("in-irs");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_instr_markets() {
    if (begin_endpoint_card("in-fwd", "Forward Contract", "Forward/futures pricing")) {
        input_float("in-fwd", "Spot", "spot", "100"); input_float("in-fwd", "r", "r", "0.05");
        input_float("in-fwd", "q", "q", "0.02"); input_float("in-fwd", "T", "T", "1.0");
        if (run_button("in-fwd")) {
            auto& s = ep("in-fwd");
            fire_post("in-fwd", "instruments/forward/price", {
                {"spot", s.getd("spot")}, {"r", s.getd("r")}, {"q", s.getd("q")}, {"T", s.getd("T")}
            });
        }
        render_result("in-fwd");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_instr_creditfut() {
    if (begin_endpoint_card("in-cds", "CDS Spread", "Credit default swap spread calculation")) {
        input_float("in-cds", "Notional", "notional", "10000000");
        input_float("in-cds", "Recovery", "recovery", "0.4");
        input_float("in-cds", "PD", "pd", "0.02");
        input_float("in-cds", "Maturity", "maturity", "5");
        if (run_button("in-cds")) {
            auto& s = ep("in-cds");
            fire_post("in-cds", "instruments/cds/spread", {
                {"notional", s.getd("notional")}, {"recovery_rate", s.getd("recovery")},
                {"default_probability", s.getd("pd")}, {"maturity", s.getd("maturity")}
            });
        }
        render_result("in-cds");
        end_endpoint_card();
    }
}

// ============================================================================
// Analysis Panels
// ============================================================================

void QuantLibScreen::render_analysis_fundamentals() {
    if (begin_endpoint_card("an-fund", "Financial Ratios", "Compute key financial ratios from statements")) {
        input_text("an-fund", "Symbol", "symbol", "AAPL");
        if (run_button("an-fund")) {
            fire_post("an-fund", "analysis/fundamentals/ratios", {{"symbol", ep("an-fund").get("symbol")}});
        }
        render_result("an-fund");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_profit_liq() {
    if (begin_endpoint_card("an-prof", "Profitability & Liquidity", "ROE, ROA, current ratio, quick ratio")) {
        input_text("an-prof", "Symbol", "symbol", "MSFT");
        if (run_button("an-prof")) {
            fire_post("an-prof", "analysis/profitability-liquidity", {{"symbol", ep("an-prof").get("symbol")}});
        }
        render_result("an-prof");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_eff_growth() {
    if (begin_endpoint_card("an-eff", "Efficiency & Growth", "Asset turnover, revenue growth, earnings growth")) {
        input_text("an-eff", "Symbol", "symbol", "GOOGL");
        if (run_button("an-eff")) {
            fire_post("an-eff", "analysis/efficiency-growth", {{"symbol", ep("an-eff").get("symbol")}});
        }
        render_result("an-eff");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_lev_cf() {
    if (begin_endpoint_card("an-lev", "Leverage & Cash Flow", "Debt ratios, FCF, coverage")) {
        input_text("an-lev", "Symbol", "symbol", "AMZN");
        if (run_button("an-lev")) {
            fire_post("an-lev", "analysis/leverage-cashflow", {{"symbol", ep("an-lev").get("symbol")}});
        }
        render_result("an-lev");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_val_qual() {
    if (begin_endpoint_card("an-val", "Valuation Ratios", "P/E, P/B, EV/EBITDA, PEG")) {
        input_text("an-val", "Symbol", "symbol", "NVDA");
        if (run_button("an-val")) {
            fire_post("an-val", "analysis/valuation-quality", {{"symbol", ep("an-val").get("symbol")}});
        }
        render_result("an-val");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_industry() {
    if (begin_endpoint_card("an-ind", "Industry Comparison", "Compare metrics across sector peers")) {
        input_text("an-ind", "Symbol", "symbol", "TSLA");
        if (run_button("an-ind")) {
            fire_post("an-ind", "analysis/industry", {{"symbol", ep("an-ind").get("symbol")}});
        }
        render_result("an-ind");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_dcf() {
    if (begin_endpoint_card("an-dcf", "DCF Valuation", "Discounted cash flow intrinsic value")) {
        input_text("an-dcf", "Symbol", "symbol", "AAPL");
        input_float("an-dcf", "WACC", "wacc", "0.10");
        input_float("an-dcf", "Terminal Growth", "tg", "0.025");
        input_int("an-dcf", "Forecast Years", "years", "5");
        if (run_button("an-dcf")) {
            auto& s = ep("an-dcf");
            fire_post("an-dcf", "analysis/dcf", {
                {"symbol", s.get("symbol")}, {"wacc", s.getd("wacc")},
                {"terminal_growth", s.getd("tg")}, {"forecast_years", s.geti("years")}
            });
        }
        render_result("an-dcf");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_models() {
    if (begin_endpoint_card("an-ddm", "Dividend Discount Model", "DDM / Gordon growth model valuation")) {
        input_float("an-ddm", "Dividend", "div", "3.0");
        input_float("an-ddm", "Required Return", "r", "0.10");
        input_float("an-ddm", "Growth Rate", "g", "0.05");
        if (run_button("an-ddm")) {
            auto& s = ep("an-ddm");
            fire_post("an-ddm", "analysis/valuation-models/ddm", {
                {"dividend", s.getd("div")}, {"required_return", s.getd("r")}, {"growth_rate", s.getd("g")}
            });
        }
        render_result("an-ddm");
        end_endpoint_card();
    }
}

} // namespace fincept::quantlib
