#pragma once
// AI Quant Lab Screen — 18-module quantitative research platform
// Layout: [Left: Module sidebar] [Center: Active panel] [Right: System status]
// Mirrors the Tauri AIQuantLabTab with all sub-panels

#include "ai_quant_lab_types.h"
#include "ai_quant_lab_api.h"
#include <imgui.h>
#include <set>

namespace fincept::ai_quant_lab {

class AIQuantLabScreen {
public:
    void render();

private:
    void init();

    // ── Layout ──
    void render_sidebar(float height);
    void render_content(float width, float height);
    void render_status_panel(float height);

    // ── 18 Panel renderers ──
    void render_factor_discovery();
    void render_model_library();
    void render_backtesting();
    void render_live_signals();
    void render_functime();
    void render_fortitudo();
    void render_statsmodels();
    void render_cfa_quant();
    void render_deep_agent();
    void render_rl_trading();
    void render_online_learning();
    void render_hft();
    void render_meta_learning();
    void render_rolling_retraining();
    void render_advanced_models();
    void render_gluonts();
    void render_gs_quant();
    void render_pattern_intelligence();

    // ── UI Helpers ──
    void metric_card(const char* label, const char* value, ImVec4 color);
    void metric_card(const char* label, double value, const char* fmt, ImVec4 color);
    void progress_bar(float fraction, const char* label = nullptr);
    bool config_input_text(const char* label, char* buf, int buf_size, float width = 200.0f);
    bool config_input_float(const char* label, float* val, float width = 120.0f);
    bool config_input_int(const char* label, int* val, float width = 120.0f);
    bool config_combo(const char* label, int* current, const char* const* items, int count, float width = 150.0f);
    void section_header(const char* title, ImVec4 color);
    void status_badge(const char* text, ImVec4 bg_color, ImVec4 text_color);

    // ── State ──
    bool initialized_ = false;
    ViewMode active_view_ = ViewMode::FactorDiscovery;
    SystemStatus system_status_ = SystemStatus::Idle;
    bool left_panel_visible_ = true;
    bool right_panel_visible_ = true;
    std::vector<ModuleItem> modules_;
    std::set<ModuleCategory> expanded_categories_;

    // ── Factor Discovery state ──
    char fd_task_desc_[512] = "Find alpha factors for US equity market";
    char fd_api_key_[256] = "";
    int fd_market_idx_ = 0;     // 0=US, 1=CN, 2=CRYPTO
    int fd_budget_ = 10;
    std::string fd_task_id_;
    AsyncOp fd_op_;
    std::vector<DiscoveredFactor> fd_factors_;
    bool fd_polling_ = false;
    double fd_last_poll_ = 0;

    // ── Model Library state ──
    int ml_filter_idx_ = 0;    // 0=all, 1=tree, 2=neural, 3=linear, 4=ensemble
    AsyncOp ml_list_op_;
    std::vector<QlibModel> ml_models_;
    int ml_selected_ = -1;
    char ml_instruments_[128] = "csi300";
    char ml_start_date_[16] = "2020-01-01";
    char ml_end_date_[16] = "2024-01-01";
    AsyncOp ml_train_op_;

    // ── Backtesting state ──
    int bt_strategy_idx_ = 0;  // topk_dropout, weight_base, enhanced_indexing
    char bt_instruments_[128] = "csi300";
    char bt_start_[16] = "2020-01-01";
    char bt_end_[16] = "2024-01-01";
    float bt_capital_ = 1000000.0f;
    char bt_benchmark_[32] = "SH000300";
    float bt_commission_ = 0.001f;
    float bt_slippage_ = 0.001f;
    AsyncOp bt_op_;

    // ── Live Signals state ──
    int ls_model_idx_ = 0;     // LightGBM, XGBoost, LSTM, Transformer
    int ls_refresh_sec_ = 30;
    bool ls_streaming_ = false;
    double ls_last_refresh_ = 0;
    int ls_filter_idx_ = 0;   // 0=ALL, 1=BUY, 2=SELL
    AsyncOp ls_op_;
    std::vector<SignalEntry> ls_signals_;

    // ── Deep Agent state ──
    int da_agent_idx_ = 0;
    char da_query_[1024] = "";
    int da_phase_ = 0;        // 0=idle, 1=planning, 2=executing, 3=synthesizing, 4=done
    std::vector<AgentStep> da_steps_;
    int da_current_step_ = 0;
    AsyncOp da_op_;
    std::string da_plan_text_;
    std::string da_synthesis_;

    // ── RL Trading state ──
    int rl_algo_idx_ = 0;
    int rl_phase_ = 0;        // 0=config, 1=env_created, 2=training, 3=evaluating, 4=done
    char rl_tickers_[128] = "AAPL,MSFT,GOOGL";
    char rl_start_[16] = "2020-01-01";
    char rl_end_[16] = "2024-01-01";
    float rl_initial_cash_ = 100000.0f;
    float rl_commission_ = 0.001f;
    int rl_action_space_ = 0;  // 0=continuous, 1=discrete
    int rl_timesteps_ = 100000;
    float rl_learning_rate_ = 0.0003f;
    AsyncOp rl_op_;

    // ── Online Learning state ──
    int ol_model_idx_ = 0;
    bool ol_live_mode_ = false;
    double ol_last_update_ = 0;
    int ol_drift_strategy_ = 0; // 0=retrain, 1=adaptive
    AsyncOp ol_create_op_;
    AsyncOp ol_train_op_;
    AsyncOp ol_perf_op_;

    // ── HFT state ──
    bool hft_live_ = false;
    double hft_last_update_ = 0;
    std::vector<OrderLevel> hft_bids_;
    std::vector<OrderLevel> hft_asks_;
    MicrostructureMetrics hft_metrics_;
    char hft_symbol_[32] = "AAPL";
    float hft_spread_target_ = 0.02f;
    float hft_position_limit_ = 1000.0f;

    // ── Meta Learning state ──
    AsyncOp meta_list_op_;
    AsyncOp meta_run_op_;
    int meta_task_type_ = 0;   // 0=classification, 1=regression

    // ── Rolling Retraining state ──
    AsyncOp rr_list_op_;
    AsyncOp rr_create_op_;
    char rr_model_name_[128] = "LightGBM_v1";
    int rr_window_days_ = 252;
    int rr_frequency_ = 1;     // 0=hourly, 1=daily, 2=weekly

    // ── Advanced Models state ──
    AsyncOp am_list_op_;
    AsyncOp am_train_op_;
    int am_model_idx_ = 0;     // 0=LSTM, 1=GRU, 2=Transformer

    // ── GluonTS state ──
    int gts_model_idx_ = 0;
    char gts_data_path_[512] = "";
    int gts_pred_length_ = 30;
    int gts_freq_idx_ = 1;    // 0=H, 1=D, 2=W, 3=M
    int gts_epochs_ = 50;
    AsyncOp gts_op_;

    // ── GS Quant state ──
    int gsq_mode_idx_ = 0;
    char gsq_symbols_[256] = "AAPL,MSFT,GOOGL";
    AsyncOp gsq_op_;

    // ── Statsmodels state ──
    int sm_analysis_idx_ = 0;
    int sm_data_source_ = 0;  // 0=manual, 1=symbol
    char sm_symbol_[32] = "AAPL";
    char sm_data_[2048] = "";
    int sm_arima_p_ = 1, sm_arima_d_ = 1, sm_arima_q_ = 1;
    AsyncOp sm_op_;

    // ── Functime state ──
    char ft_symbol_[32] = "AAPL";
    int ft_horizon_ = 30;
    int ft_model_idx_ = 0;
    AsyncOp ft_op_;

    // ── Fortitudo state ──
    int fort_mode_idx_ = 0;
    char fort_symbols_[256] = "AAPL,MSFT,GOOGL,AMZN";
    AsyncOp fort_op_;

    // ── CFA Quant state ──
    char cfa_symbol_[32] = "AAPL";
    int cfa_analysis_idx_ = 0;
    AsyncOp cfa_op_;

    // ── Pattern Intelligence state ──
    char pi_symbol_[32] = "AAPL";
    int pi_pattern_idx_ = 0;
    AsyncOp pi_search_op_;
    AsyncOp pi_score_op_;
    AsyncOp pi_backtest_op_;

    // ── Status log ──
    struct LogEntry { double time; std::string msg; ImVec4 color; };
    std::vector<LogEntry> status_log_;
    void log_status(const std::string& msg, ImVec4 color = {0.72f, 0.72f, 0.75f, 1.0f});
};

} // namespace fincept::ai_quant_lab
