#include "backtesting_screen.h"
#include "backtesting_constants.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace fincept::backtesting {

using json = nlohmann::json;

// =============================================================================
// Config panel (center)
// =============================================================================
void BacktestingScreen::render_config_panel() {
    using namespace theme::colors;

    // Header with command name + run button
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##bt_cfg_hdr", ImVec2(0, 36));

    ImGui::TextColored(TEXT_PRIMARY, "%s", command_label(active_command_));
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DISABLED, "CONFIGURATION");

    ImGui::SameLine(ImGui::GetWindowWidth() - 160);
    if (RunButton(("RUN " + std::string(command_label(active_command_))).c_str(),
                  data_.is_loading(), provider_color(selected_provider_))) {
        execute_command();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Content area
    ImGui::BeginChild("##bt_cfg_content", ImVec2(0, 0));

    switch (active_command_) {
        case CommandType::Backtest:
            render_backtest_config();
            if (show_advanced_) render_advanced_config();
            break;
        case CommandType::Optimize:
            render_optimize_config();
            break;
        case CommandType::WalkForward:
            render_walk_forward_config();
            break;
        case CommandType::Indicator:
            render_indicator_config();
            break;
        case CommandType::IndicatorSignals:
            render_indicator_signals_config();
            break;
        case CommandType::Labels:
            render_labels_config();
            break;
        case CommandType::Splits:
            render_splitters_config();
            break;
        case CommandType::Returns:
            render_returns_config();
            break;
        case CommandType::Signals:
            render_signals_config();
            break;
        default: break;
    }

    ImGui::EndChild();
}

// =============================================================================
// Backtest config
// =============================================================================
void BacktestingScreen::render_backtest_config() {
    using namespace theme::colors;

    // Market Data section
    bool market_open = expanded_["market"];
    if (SectionHeader("MARKET DATA", &market_open)) {
        expanded_["market"] = true;
        if (ImGui::BeginTable("##bt_market", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
            ImGui::TableNextColumn(); InputFloat("Initial Capital", &initial_capital_, 1000);
            ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
            ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
            ImGui::EndTable();
        }
    } else { expanded_["market"] = false; }

    ImGui::Spacing();

    // Strategy selection section
    auto strats = get_strategies(selected_provider_, selected_category_);
    bool strat_open = expanded_["strategy"];
    auto cats = get_categories(selected_provider_);
    const char* cat_label = selected_category_.c_str();
    for (auto& c : cats) {
        if (selected_category_ == c.key) { cat_label = c.label; break; }
    }

    char section_label[128];
    snprintf(section_label, sizeof(section_label), "%s STRATEGIES", cat_label);

    if (SectionHeader(section_label, &strat_open)) {
        expanded_["strategy"] = true;

        // Strategy grid (2 columns)
        if (ImGui::BeginTable("##bt_strats", 2, ImGuiTableFlags_None)) {
            for (int i = 0; i < (int)strats.size(); i++) {
                ImGui::TableNextColumn();
                bool selected = (i == selected_strategy_idx_);
                ImVec4 btn_color = selected ? ACCENT : ImVec4(0,0,0,0);
                ImVec4 txt_color = selected ? BG_DARKEST : TEXT_SECONDARY;

                ImGui::PushStyleColor(ImGuiCol_Button, btn_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? ACCENT_HOVER : BG_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text, txt_color);

                if (ImGui::Button(strats[i].name, ImVec2(-1, 40))) {
                    selected_strategy_idx_ = i;
                    init_strategy_params();
                }
                ImGui::PopStyleColor(3);

                // Description tooltip
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", strats[i].description);
                }
            }
            ImGui::EndTable();
        }

        // Strategy parameters
        if (selected_strategy_idx_ < (int)strats.size()) {
            auto& strat = strats[selected_strategy_idx_];

            if (std::string(strat.id) == "code") {
                // Custom code editor
                ImGui::Spacing();
                ImGui::TextColored(TEXT_DIM, "CUSTOM PYTHON CODE");
                ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
                ImGui::InputTextMultiline("##bt_custom_code", custom_code_, sizeof(custom_code_),
                    ImVec2(-1, 200));
                ImGui::PopStyleColor();
            }
            else if (!strat.params.empty()) {
                ImGui::Spacing();
                int cols = std::min(3, (int)strat.params.size());
                if (ImGui::BeginTable("##bt_params", cols, ImGuiTableFlags_None)) {
                    for (auto& param : strat.params) {
                        ImGui::TableNextColumn();
                        float val = strategy_params_[param.name];
                        if (InputFloat(param.label, &val, param.step)) {
                            if (val >= param.min_val && val <= param.max_val)
                                strategy_params_[param.name] = val;
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
    } else { expanded_["strategy"] = false; }

    ImGui::Spacing();

    // Execution settings
    bool exec_open = expanded_["execution"];
    if (SectionHeader("EXECUTION SETTINGS", &exec_open)) {
        expanded_["execution"] = true;
        if (ImGui::BeginTable("##bt_exec", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn(); InputFloat("Commission (%)", &commission_, 0.01f);
            ImGui::TableNextColumn(); InputFloat("Slippage (%)", &slippage_, 0.01f);
            ImGui::EndTable();
        }
    } else { expanded_["execution"] = false; }
}

// =============================================================================
// Advanced config
// =============================================================================
void BacktestingScreen::render_advanced_config() {
    using namespace theme::colors;

    ImGui::Spacing();
    bool adv_open = expanded_["advanced"];
    if (SectionHeader("ADVANCED SETTINGS", &adv_open)) {
        expanded_["advanced"] = true;

        // Risk management
        ImGui::TextColored(INFO, "RISK MANAGEMENT");
        if (ImGui::BeginTable("##bt_risk", 3, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn(); InputFloat("Stop Loss (%)", &stop_loss_, 0.1f);
            ImGui::TableNextColumn(); InputFloat("Take Profit (%)", &take_profit_, 0.1f);
            ImGui::TableNextColumn(); InputFloat("Trailing Stop (%)", &trailing_stop_, 0.1f);
            ImGui::EndTable();
        }

        ImGui::Spacing();

        // Position sizing
        ImGui::TextColored(INFO, "POSITION SIZING");
        auto& ps_opts = position_sizing_options();
        if (ImGui::BeginTable("##bt_pos", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "METHOD");
            ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##bt_ps_method", ps_opts[position_sizing_idx_].label)) {
                for (int i = 0; i < (int)ps_opts.size(); i++) {
                    if (ImGui::Selectable(ps_opts[i].label, i == position_sizing_idx_))
                        position_sizing_idx_ = i;
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleColor();
            ImGui::TableNextColumn(); InputFloat("Size Value", &position_size_value_);
            ImGui::EndTable();
        }

        ImGui::Spacing();

        // Leverage & margin
        ImGui::TextColored(INFO, "LEVERAGE & MARGIN");
        if (ImGui::BeginTable("##bt_lev", 3, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn(); InputFloat("Leverage", &leverage_, 0.1f);
            ImGui::TableNextColumn(); InputFloat("Margin", &margin_, 0.1f);
            ImGui::TableNextColumn();
            ImGui::Spacing();
            ImGui::Checkbox("Allow Short", &allow_short_);
            ImGui::EndTable();
        }

        ImGui::Spacing();

        // Benchmark
        ImGui::TextColored(INFO, "BENCHMARK COMPARISON");
        ImGui::Checkbox("Enable Benchmark", &enable_benchmark_);
        if (enable_benchmark_) {
            InputField("Benchmark Symbol", benchmark_symbol_, sizeof(benchmark_symbol_));
            ImGui::Checkbox("Random Benchmark", &random_benchmark_);
        }
    } else { expanded_["advanced"] = false; }
}

// =============================================================================
// Optimize config
// =============================================================================
void BacktestingScreen::render_optimize_config() {
    using namespace theme::colors;

    // Market data (same as backtest)
    bool market_open = expanded_["market"];
    if (SectionHeader("MARKET DATA", &market_open)) {
        expanded_["market"] = true;
        if (ImGui::BeginTable("##bt_opt_market", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
            ImGui::TableNextColumn(); InputFloat("Initial Capital", &initial_capital_, 1000);
            ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
            ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
            ImGui::EndTable();
        }
    } else { expanded_["market"] = false; }

    ImGui::Spacing();

    // Optimization settings
    bool opt_open = expanded_["optimization"];
    if (SectionHeader("OPTIMIZATION SETTINGS", &opt_open)) {
        expanded_["optimization"] = true;

        if (ImGui::BeginTable("##bt_opt_cfg", 3, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "OBJECTIVE");
            ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##bt_opt_obj", OPTIMIZE_OBJECTIVE_LABELS[optimize_objective_idx_])) {
                for (int i = 0; i < 4; i++) {
                    if (ImGui::Selectable(OPTIMIZE_OBJECTIVE_LABELS[i], i == optimize_objective_idx_))
                        optimize_objective_idx_ = i;
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "METHOD");
            ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##bt_opt_method", OPTIMIZE_METHOD_LABELS[optimize_method_idx_])) {
                for (int i = 0; i < 2; i++) {
                    if (ImGui::Selectable(OPTIMIZE_METHOD_LABELS[i], i == optimize_method_idx_))
                        optimize_method_idx_ = i;
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            InputInt("Max Iterations", &max_iterations_);
            ImGui::EndTable();
        }

        // Parameter ranges
        auto strats = get_strategies(selected_provider_, selected_category_);
        if (selected_strategy_idx_ < (int)strats.size()) {
            auto& strat = strats[selected_strategy_idx_];
            if (!strat.params.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(INFO, "PARAMETER RANGES");
                if (ImGui::BeginTable("##bt_param_ranges", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Parameter");
                    ImGui::TableSetupColumn("Min");
                    ImGui::TableSetupColumn("Max");
                    ImGui::TableSetupColumn("Step");
                    ImGui::TableHeadersRow();

                    for (auto& param : strat.params) {
                        auto& range = param_ranges_[param.name];
                        if (range[0] == 0 && range[1] == 0) {
                            range[0] = param.min_val;
                            range[1] = param.max_val;
                            range[2] = param.step > 0 ? param.step : 1.0f;
                        }
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%s", param.label);
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-1);
                        ImGui::InputFloat(("##rng_min_" + std::string(param.name)).c_str(), &range[0], 0, 0, "%.2f");
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-1);
                        ImGui::InputFloat(("##rng_max_" + std::string(param.name)).c_str(), &range[1], 0, 0, "%.2f");
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-1);
                        ImGui::InputFloat(("##rng_step_" + std::string(param.name)).c_str(), &range[2], 0, 0, "%.2f");
                        ImGui::PopItemWidth();
                    }
                    ImGui::EndTable();
                }
            }
        }
    } else { expanded_["optimization"] = false; }
}

// =============================================================================
// Walk forward config
// =============================================================================
void BacktestingScreen::render_walk_forward_config() {
    using namespace theme::colors;

    // Market data
    bool market_open = expanded_["market"];
    if (SectionHeader("MARKET DATA", &market_open)) {
        expanded_["market"] = true;
        if (ImGui::BeginTable("##bt_wf_market", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
            ImGui::TableNextColumn(); InputFloat("Initial Capital", &initial_capital_, 1000);
            ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
            ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
            ImGui::EndTable();
        }
    } else { expanded_["market"] = false; }

    ImGui::Spacing();

    bool wf_open = expanded_["walk_forward"];
    if (SectionHeader("WALK FORWARD SETTINGS", &wf_open)) {
        expanded_["walk_forward"] = true;
        if (ImGui::BeginTable("##bt_wf_cfg", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn(); InputInt("Number of Splits", &wf_splits_);
            ImGui::TableNextColumn(); InputFloat("Train Ratio", &wf_train_ratio_, 0.05f);
            ImGui::EndTable();
        }
    } else { expanded_["walk_forward"] = false; }
}

// =============================================================================
// Indicator config
// =============================================================================
void BacktestingScreen::render_indicator_config() {
    using namespace theme::colors;

    auto& inds = indicators();
    ImGui::TextColored(TEXT_DIM, "INDICATOR");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(200);
    if (ImGui::BeginCombo("##bt_ind", inds[indicator_idx_].label)) {
        for (int i = 0; i < (int)inds.size(); i++) {
            if (ImGui::Selectable(inds[i].label, i == indicator_idx_))
                indicator_idx_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Market data fields
    if (ImGui::BeginTable("##bt_ind_market", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
        ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
        ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
        ImGui::EndTable();
    }
}

// =============================================================================
// Indicator signals config
// =============================================================================
void BacktestingScreen::render_indicator_signals_config() {
    using namespace theme::colors;

    auto& modes = signal_modes();
    ImGui::TextColored(TEXT_DIM, "SIGNAL MODE");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(200);
    if (ImGui::BeginCombo("##bt_sig_mode", modes[signal_mode_idx_].label)) {
        for (int i = 0; i < (int)modes.size(); i++) {
            if (ImGui::Selectable(modes[i].label, i == signal_mode_idx_))
                signal_mode_idx_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", modes[signal_mode_idx_].description);

    ImGui::Spacing();

    if (ImGui::BeginTable("##bt_indsig_market", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
        ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
        ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
        ImGui::EndTable();
    }
}

// =============================================================================
// Labels config (ML)
// =============================================================================
void BacktestingScreen::render_labels_config() {
    using namespace theme::colors;

    auto& lts = label_types();
    ImGui::TextColored(TEXT_DIM, "LABEL TYPE");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(200);
    if (ImGui::BeginCombo("##bt_lbl_type", lts[labels_type_idx_].label)) {
        for (int i = 0; i < (int)lts.size(); i++) {
            if (ImGui::Selectable(lts[i].label, i == labels_type_idx_))
                labels_type_idx_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", lts[labels_type_idx_].description);

    ImGui::Spacing();

    // Label params
    auto& lt = lts[labels_type_idx_];
    for (auto& p : lt.params) {
        float val = labels_params_[p];
        if (val == 0.0f) {
            // Defaults
            if (std::string(p) == "horizon") val = 5;
            else if (std::string(p) == "threshold") val = 0.02f;
            else if (std::string(p) == "window") val = 20;
            else if (std::string(p) == "alpha") val = 2.0f;
            labels_params_[p] = val;
        }
        if (InputFloat(p, &val)) labels_params_[p] = val;
    }

    ImGui::Spacing();

    if (ImGui::BeginTable("##bt_lbl_market", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
        ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
        ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
        ImGui::EndTable();
    }
}

// =============================================================================
// Splitters config (CV)
// =============================================================================
void BacktestingScreen::render_splitters_config() {
    using namespace theme::colors;

    auto& sts = splitter_types();
    ImGui::TextColored(TEXT_DIM, "SPLITTER TYPE");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(200);
    if (ImGui::BeginCombo("##bt_spl_type", sts[splitter_type_idx_].label)) {
        for (int i = 0; i < (int)sts.size(); i++) {
            if (ImGui::Selectable(sts[i].label, i == splitter_type_idx_))
                splitter_type_idx_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", sts[splitter_type_idx_].description);

    ImGui::Spacing();

    auto& st = sts[splitter_type_idx_];
    for (auto& p : st.params) {
        float val = splitter_params_[p];
        if (val == 0.0f) {
            if (std::string(p) == "window_len" || std::string(p) == "min_len") val = 252;
            else if (std::string(p) == "test_len") val = 63;
            else if (std::string(p) == "step") val = 21;
            else if (std::string(p) == "n_splits") val = 5;
            else if (std::string(p) == "purge_len" || std::string(p) == "embargo_len") val = 5;
            splitter_params_[p] = val;
        }
        if (InputFloat(p, &val)) splitter_params_[p] = val;
    }

    ImGui::Spacing();

    if (ImGui::BeginTable("##bt_spl_market", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
        ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
        ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
        ImGui::EndTable();
    }
}

// =============================================================================
// Returns config
// =============================================================================
void BacktestingScreen::render_returns_config() {
    using namespace theme::colors;

    auto& rts = returns_analysis_types();
    ImGui::TextColored(TEXT_DIM, "ANALYSIS TYPE");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(200);
    if (ImGui::BeginCombo("##bt_ret_type", rts[returns_type_idx_].label)) {
        for (int i = 0; i < (int)rts.size(); i++) {
            if (ImGui::Selectable(rts[i].label, i == returns_type_idx_))
                returns_type_idx_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    InputInt("Rolling Window", &returns_rolling_window_);

    if (rts[returns_type_idx_].id == std::string("benchmark_comparison")) {
        InputField("Benchmark Symbol", benchmark_symbol_, sizeof(benchmark_symbol_));
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("##bt_ret_market", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
        ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
        ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
        ImGui::EndTable();
    }
}

// =============================================================================
// Signals config (random signal generators)
// =============================================================================
void BacktestingScreen::render_signals_config() {
    using namespace theme::colors;

    auto& sgs = signal_generator_types();
    ImGui::TextColored(TEXT_DIM, "GENERATOR TYPE");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushItemWidth(200);
    if (ImGui::BeginCombo("##bt_siggen_type", sgs[signal_gen_idx_].label)) {
        for (int i = 0; i < (int)sgs.size(); i++) {
            if (ImGui::Selectable(sgs[i].label, i == signal_gen_idx_))
                signal_gen_idx_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", sgs[signal_gen_idx_].description);

    ImGui::Spacing();

    auto& sg = sgs[signal_gen_idx_];
    for (auto& p : sg.params) {
        float val = signal_gen_params_[p];
        if (val == 0.0f) {
            if (std::string(p) == "n") val = 10;
            else if (std::string(p) == "seed") val = 42;
            else if (std::string(p) == "entry_prob" || std::string(p) == "exit_prob") val = 0.1f;
            else if (std::string(p) == "cooldown") val = 5;
            else if (std::string(p) == "min_hold") val = 5;
            else if (std::string(p) == "max_hold") val = 20;
            signal_gen_params_[p] = val;
        }
        if (InputFloat(p, &val)) signal_gen_params_[p] = val;
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("##bt_sig_market", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn(); InputField("Symbols", symbols_, sizeof(symbols_));
        ImGui::TableNextColumn(); InputField("Start Date", start_date_, sizeof(start_date_));
        ImGui::TableNextColumn(); InputField("End Date", end_date_, sizeof(end_date_));
        ImGui::EndTable();
    }
}

// =============================================================================
// Results panel (right)
// =============================================================================

} // namespace fincept::backtesting
