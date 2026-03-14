#pragma once
// Backtesting Tab — main screen with three-panel layout
// Multi-provider strategy backtesting terminal

#include "backtesting_types.h"
#include "backtesting_data.h"
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace fincept::backtesting {

class BacktestingScreen {
public:
    void render();

private:
    BacktestData data_;

    // Provider state
    ProviderSlug selected_provider_ = ProviderSlug::VectorBT;

    // Command state
    CommandType active_command_ = CommandType::Backtest;
    std::string selected_category_ = "trend";
    int selected_strategy_idx_ = 0;

    // Backtest config
    char symbols_[128] = "SPY";
    char start_date_[16] = "2025-03-15";
    char end_date_[16] = "2026-03-14";
    float initial_capital_ = 100000.0f;
    float commission_ = 0.1f;    // percentage
    float slippage_ = 0.05f;     // percentage

    // Strategy params (dynamic, keyed by param name)
    std::map<std::string, float> strategy_params_;
    char custom_code_[4096] = "# Custom strategy code\n";

    // Advanced config
    bool show_advanced_ = false;
    float leverage_ = 1.0f;
    float margin_ = 1.0f;
    float stop_loss_ = 0.0f;
    float take_profit_ = 0.0f;
    float trailing_stop_ = 0.0f;
    int position_sizing_idx_ = 1; // percent
    float position_size_value_ = 1.0f;
    bool allow_short_ = false;
    bool enable_benchmark_ = false;
    char benchmark_symbol_[32] = "SPY";
    bool random_benchmark_ = false;

    // Optimize config
    int optimize_objective_idx_ = 0; // sharpe
    int optimize_method_idx_ = 0;    // grid
    int max_iterations_ = 500;
    std::map<std::string, std::array<float,3>> param_ranges_; // min,max,step

    // Walk forward config
    int wf_splits_ = 5;
    float wf_train_ratio_ = 0.7f;

    // Indicator config
    int indicator_idx_ = 0;
    std::map<std::string, float> indicator_params_;

    // Indicator signals config
    int signal_mode_idx_ = 0;
    std::map<std::string, float> ind_signal_params_;

    // Labels config
    int labels_type_idx_ = 0;
    std::map<std::string, float> labels_params_;

    // Splitters config
    int splitter_type_idx_ = 0;
    std::map<std::string, float> splitter_params_;

    // Returns config
    int returns_type_idx_ = 0;
    int returns_rolling_window_ = 21;

    // Signal generator config
    int signal_gen_idx_ = 0;
    std::map<std::string, float> signal_gen_params_;

    // Execution state
    nlohmann::json result_;
    bool has_error_ = false;
    std::string error_msg_;
    ResultView result_view_ = ResultView::Summary;

    // Status
    std::string status_;
    double status_time_ = 0;

    // Section expansion state
    std::map<std::string, bool> expanded_ = {
        {"market", true}, {"strategy", true}, {"execution", false}, {"advanced", false},
        {"optimization", true}, {"walk_forward", true},
    };

    // Rendering methods
    void render_top_nav();
    void render_command_panel();
    void render_config_panel();
    void render_results_panel();
    void render_status_bar();

    // Config sub-panels
    void render_backtest_config();
    void render_advanced_config();
    void render_optimize_config();
    void render_walk_forward_config();
    void render_indicator_config();
    void render_indicator_signals_config();
    void render_labels_config();
    void render_splitters_config();
    void render_returns_config();
    void render_signals_config();

    // Result sub-panels
    void render_result_summary();
    void render_result_metrics();
    void render_result_trades();
    void render_result_raw();
    void render_result_charts();
    void render_result_drawdowns();
    void render_result_indicator();
    void render_result_signals();
    void render_result_signal_generator();
    void render_result_labels();
    void render_result_splitters();
    void render_result_returns();

    // Actions
    void pickup_result();
    void execute_command();
    std::string build_args_json();

    // Helpers
    void init_strategy_params();
};

} // namespace fincept::backtesting
