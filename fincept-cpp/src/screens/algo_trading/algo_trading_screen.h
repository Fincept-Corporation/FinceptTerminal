#pragma once
// Algo Trading Screen — condition-based strategy builder, deployment, monitoring
// Layout: TopNav (5 sub-views) + Content Area + Status Bar
//
// Sub-views: Builder | Strategies | Library | Scanner | Dashboard

#include "algo_types.h"
#include "algo_constants.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace fincept::algo {

class AlgoTradingScreen {
public:
    void render();

private:
    bool initialized_ = false;
    void init();

    // --- Sub-view navigation ---
    enum class SubView { Builder, Strategies, Library, Scanner, Dashboard };
    SubView active_view_ = SubView::Builder;

    // --- Strategy Builder state ---
    char strategy_name_[128] = "";
    char strategy_desc_[256] = "";
    char symbols_buf_[256] = "";
    int timeframe_idx_ = 3;          // default "5m"
    int provider_idx_ = 0;           // default "fyers"
    float stop_loss_ = 0.0f;
    float take_profit_ = 0.0f;
    float trailing_stop_ = 0.0f;
    float position_size_ = 1.0f;
    int max_positions_ = 1;
    int entry_logic_idx_ = 0;        // 0=AND, 1=OR
    int exit_logic_idx_ = 0;
    std::vector<ConditionItem> entry_conditions_;
    std::vector<ConditionItem> exit_conditions_;
    std::string editing_strategy_id_; // non-empty = editing existing

    // --- Strategy List state ---
    std::vector<AlgoStrategy> strategies_;
    bool strategies_loaded_ = false;

    // --- Deployment state ---
    std::vector<AlgoDeployment> deployments_;
    bool deployments_loaded_ = false;

    // --- Python Library state ---
    std::vector<PythonStrategy> python_strategies_;
    std::vector<CustomPythonStrategy> custom_strategies_;
    std::vector<std::string> library_categories_;
    bool library_loaded_ = false;
    int library_category_idx_ = 0;  // 0 = All
    char library_search_[128] = "";
    std::string selected_strategy_id_;
    std::string code_preview_;
    bool loading_code_ = false;
    bool editing_code_ = false;
    char custom_code_[8192] = "";
    char custom_name_[128] = "";
    char custom_desc_[256] = "";
    std::string syntax_error_;

    // --- Scanner state ---
    char scan_symbols_[512] = "";
    bool scanning_ = false;
    std::vector<ScanResult> scan_results_;
    int scan_tf_idx_ = 3;        // default "5m"
    int scan_prov_idx_ = 0;      // default "fyers"

    // --- Dashboard state ---
    std::vector<AlgoMetrics> dashboard_metrics_;
    std::vector<AlgoTrade> recent_trades_;

    // --- Backtest state ---
    bool show_backtest_modal_ = false;
    char bt_start_date_[16] = "2025-03-15";
    char bt_end_date_[16] = "2026-03-14";
    float bt_capital_ = 100000.0f;
    BacktestResult backtest_result_;
    bool backtest_running_ = false;

    // --- Status ---
    std::string status_msg_;
    int running_count_ = 0;

    std::mutex data_mutex_;

    // --- Rendering methods ---
    void render_top_nav(float w);
    void render_status_bar(float w);

    // Sub-view renderers
    void render_builder(float w, float h);
    void render_strategies(float w, float h);
    void render_library(float w, float h);
    void render_scanner(float w, float h);
    void render_dashboard(float w, float h);

    // Builder helpers
    void render_condition_editor(const char* label, std::vector<ConditionItem>& conditions,
                                  int& logic_idx);
    void render_condition_row(ConditionItem& cond, int idx, bool& remove);
    void render_backtest_results();
    void render_dashboard_summary();

    // Actions
    void save_strategy();
    void load_strategies();
    void delete_strategy(const std::string& id);
    void edit_strategy(const AlgoStrategy& s);
    void deploy_strategy(const std::string& strategy_id, const std::string& mode);
    void stop_deployment(const std::string& deployment_id);
    void stop_all_deployments();
    void delete_deployment_action(const std::string& deployment_id);
    void load_deployments();
    void load_library();
    void run_scan();
    void run_backtest(const std::string& strategy_id);
    void load_dashboard_data();
    void refresh_running_count();
};

} // namespace fincept::algo
