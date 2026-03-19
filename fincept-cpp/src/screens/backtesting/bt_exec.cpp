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

void BacktestingScreen::render_status_bar() {
    using namespace theme::colors;
    auto& info = get_provider_info(selected_provider_);
    auto color = provider_color(selected_provider_);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##bt_statusbar", ImVec2(0, 24));

    ImGui::TextColored(color, "%s", info.display_name);
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "CMD:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_PRIMARY, "%s", command_label(active_command_));

    // Current strategy
    auto strats = get_strategies(selected_provider_, selected_category_);
    if (selected_strategy_idx_ < (int)strats.size()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        ImGui::TextColored(TEXT_DIM, "STRATEGY:");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(INFO, "%s", strats[selected_strategy_idx_].name);
    }

    // Status
    float right_x = ImGui::GetWindowWidth() - 80;
    ImGui::SameLine(right_x);
    bool running = data_.is_loading();
    ImGui::TextColored(running ? ACCENT : SUCCESS, "%s", running ? "RUNNING" : "READY");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Pickup result from async data
// =============================================================================
void BacktestingScreen::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;
    std::lock_guard<std::mutex> lock(data_.mutex());
    if (data_.has_result()) {
        result_ = data_.result();
        has_error_ = false;
        error_msg_.clear();
        status_ = "Data loaded";
        status_time_ = ImGui::GetTime();
        LOG_INFO("Backtesting", "Result received for command: %s", command_label(active_command_));
    } else if (!data_.error().empty()) {
        has_error_ = true;
        error_msg_ = data_.error();
        result_ = json();
        status_ = "Error: " + data_.error();
        status_time_ = ImGui::GetTime();
        LOG_ERROR("Backtesting", "Command failed: %s — %s", command_label(active_command_), error_msg_.c_str());
    }
    data_.clear();
}

// =============================================================================
// Execute command — build JSON args and run Python
// =============================================================================
void BacktestingScreen::execute_command() {
    auto& info = get_provider_info(selected_provider_);
    std::string py_command = command_to_python(selected_provider_, active_command_);
    std::string args_json = build_args_json();

    LOG_INFO("Backtesting", "Executing: provider=%s command=%s py_cmd=%s",
             info.display_name, command_label(active_command_), py_command.c_str());
    LOG_DEBUG("Backtesting", "Args JSON: %s", args_json.c_str());

    result_ = json();
    has_error_ = false;
    error_msg_.clear();
    status_ = "Running " + std::string(command_label(active_command_)) + "...";
    status_time_ = ImGui::GetTime();

    data_.run(info.script_dir, py_command, args_json);
}

// =============================================================================
// Build args JSON (mirrors Tauri handleRunCommand)
// =============================================================================
std::string BacktestingScreen::build_args_json() {
    json args;

    // Parse symbols
    json symbol_list = json::array();
    std::istringstream ss(symbols_);
    std::string sym;
    while (std::getline(ss, sym, ',')) {
        // Trim whitespace
        size_t start = sym.find_first_not_of(" \t");
        size_t end = sym.find_last_not_of(" \t");
        if (start != std::string::npos)
            symbol_list.push_back(sym.substr(start, end - start + 1));
    }

    auto strats = get_strategies(selected_provider_, selected_category_);
    std::string strat_id = (selected_strategy_idx_ < (int)strats.size())
        ? strats[selected_strategy_idx_].id : "sma_crossover";
    std::string strat_name = (selected_strategy_idx_ < (int)strats.size())
        ? strats[selected_strategy_idx_].name : "SMA Crossover";

    switch (active_command_) {
        case CommandType::Backtest: {
            json strategy;
            strategy["type"] = (strat_id == "code") ? "code" : strat_id;
            strategy["name"] = strat_name;
            if (strat_id == "code") {
                strategy["parameters"] = {{"code", custom_code_}};
            } else {
                json params;
                for (auto& [k, v] : strategy_params_) params[k] = v;
                strategy["parameters"] = params;
            }

            json assets = json::array();
            float weight = symbol_list.empty() ? 1.0f : 1.0f / (float)symbol_list.size();
            for (auto& s : symbol_list) {
                assets.push_back({{"symbol", s}, {"assetClass", "stocks"}, {"weight", weight}});
            }

            args["strategy"] = strategy;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["initialCapital"] = initial_capital_;
            args["assets"] = assets;
            args["commission"] = commission_ / 100.0f;
            args["slippage"] = slippage_ / 100.0f;
            args["leverage"] = leverage_;
            args["margin"] = margin_;
            args["stopLoss"] = stop_loss_ > 0 ? json(stop_loss_) : json(nullptr);
            args["takeProfit"] = take_profit_ > 0 ? json(take_profit_) : json(nullptr);
            args["trailingStop"] = trailing_stop_ > 0 ? json(trailing_stop_) : json(nullptr);
            args["positionSizing"] = position_sizing_options()[position_sizing_idx_].id;
            args["positionSizeValue"] = position_size_value_;
            args["allowShort"] = allow_short_;
            args["benchmarkSymbol"] = enable_benchmark_ ? json(benchmark_symbol_) : json(nullptr);
            args["randomBenchmark"] = random_benchmark_;
            break;
        }
        case CommandType::Optimize: {
            json strategy;
            strategy["type"] = strat_id;
            strategy["name"] = strat_name;

            json assets = json::array();
            float weight = symbol_list.empty() ? 1.0f : 1.0f / (float)symbol_list.size();
            for (auto& s : symbol_list) {
                assets.push_back({{"symbol", s}, {"assetClass", "stocks"}, {"weight", weight}});
            }

            json ranges;
            for (auto& [k, v] : param_ranges_) {
                ranges[k] = {{"min", v[0]}, {"max", v[1]}, {"step", v[2]}};
            }

            args["strategy"] = strategy;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["initialCapital"] = initial_capital_;
            args["assets"] = assets;
            args["parameters"] = ranges;
            args["objective"] = OPTIMIZE_OBJECTIVES[optimize_objective_idx_];
            args["method"] = OPTIMIZE_METHODS[optimize_method_idx_];
            args["maxIterations"] = max_iterations_;
            break;
        }
        case CommandType::WalkForward: {
            json strategy;
            strategy["type"] = strat_id;
            strategy["name"] = strat_name;
            json params;
            for (auto& [k, v] : strategy_params_) params[k] = v;
            strategy["parameters"] = params;

            json assets = json::array();
            float weight = symbol_list.empty() ? 1.0f : 1.0f / (float)symbol_list.size();
            for (auto& s : symbol_list) {
                assets.push_back({{"symbol", s}, {"assetClass", "stocks"}, {"weight", weight}});
            }

            args["strategy"] = strategy;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["initialCapital"] = initial_capital_;
            args["assets"] = assets;
            args["nSplits"] = wf_splits_;
            args["trainRatio"] = wf_train_ratio_;
            break;
        }
        case CommandType::Indicator: {
            auto& inds = indicators();
            args["symbols"] = symbol_list;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["indicator"] = inds[indicator_idx_].id;
            break;
        }
        case CommandType::IndicatorSignals: {
            auto& modes = signal_modes();
            auto& inds = indicators();
            args["symbols"] = symbol_list;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["mode"] = modes[signal_mode_idx_].id;
            args["indicator"] = inds[indicator_idx_].id;
            break;
        }
        case CommandType::Labels: {
            auto& lts = label_types();
            args["symbols"] = symbol_list;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["labelType"] = lts[labels_type_idx_].id;
            json lp;
            for (auto& [k, v] : labels_params_) lp[k] = v;
            args["params"] = lp;
            break;
        }
        case CommandType::Splits: {
            auto& sts = splitter_types();
            args["symbols"] = symbol_list;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["splitterType"] = sts[splitter_type_idx_].id;
            json sp;
            for (auto& [k, v] : splitter_params_) sp[k] = v;
            args["params"] = sp;
            break;
        }
        case CommandType::Returns: {
            auto& rts = returns_analysis_types();
            args["symbols"] = symbol_list;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["analysisType"] = rts[returns_type_idx_].id;
            args["rollingWindow"] = returns_rolling_window_;
            if (rts[returns_type_idx_].id == std::string("benchmark_comparison"))
                args["benchmarkSymbol"] = benchmark_symbol_;
            break;
        }
        case CommandType::Signals: {
            auto& sgs = signal_generator_types();
            args["symbols"] = symbol_list;
            args["startDate"] = start_date_;
            args["endDate"] = end_date_;
            args["generatorType"] = sgs[signal_gen_idx_].id;
            json gp;
            for (auto& [k, v] : signal_gen_params_) gp[k] = v;
            args["params"] = gp;
            break;
        }
        default: break;
    }

    return args.dump();
}

// =============================================================================
// Initialize strategy params from selected strategy defaults
// =============================================================================
void BacktestingScreen::init_strategy_params() {
    strategy_params_.clear();
    auto strats = get_strategies(selected_provider_, selected_category_);
    if (selected_strategy_idx_ < (int)strats.size()) {
        for (auto& p : strats[selected_strategy_idx_].params) {
            strategy_params_[p.name] = p.default_val;
        }
    }
}


} // namespace fincept::backtesting
