#pragma once
// AI Quant Lab API — HTTP + Python bridge for all 18 modules
// Qlib, RD-Agent, Deep Agent, GluonTS, Statsmodels, VisionQuant, GS Quant

#include <string>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>

namespace fincept::ai_quant_lab {

using json = nlohmann::json;

struct ApiResult {
    bool success = false;
    json data;
    std::string error;
};

class QuantLabApi {
public:
    static QuantLabApi& instance();

    // ── Qlib Service (Model Library, Backtesting, Live Signals, Online, Meta, Rolling, Advanced) ──
    std::future<ApiResult> qlib_list_models();
    std::future<ApiResult> qlib_train_model(const json& config);
    std::future<ApiResult> qlib_run_backtest(const json& config);
    std::future<ApiResult> qlib_get_live_predictions(const json& config);
    std::future<ApiResult> qlib_online_create(const json& config);
    std::future<ApiResult> qlib_online_train(const json& config);
    std::future<ApiResult> qlib_online_performance(const std::string& model_id);
    std::future<ApiResult> qlib_online_handle_drift(const json& config);
    std::future<ApiResult> qlib_online_setup_rolling(const json& config);
    std::future<ApiResult> qlib_meta_list_models();
    std::future<ApiResult> qlib_meta_run_selection(const json& config);
    std::future<ApiResult> qlib_rolling_list_schedules();
    std::future<ApiResult> qlib_rolling_create_schedule(const json& config);
    std::future<ApiResult> qlib_rolling_retrain(const std::string& schedule_id);
    std::future<ApiResult> qlib_advanced_list_models();
    std::future<ApiResult> qlib_advanced_train(const json& config);

    // ── RD-Agent Service (Factor Discovery) ──
    std::future<ApiResult> rdagent_start_mining(const json& config);
    std::future<ApiResult> rdagent_get_status(const std::string& task_id);
    std::future<ApiResult> rdagent_get_factors(const std::string& task_id);

    // ── Deep Agent Service ──
    std::future<ApiResult> deep_agent_create_plan(const json& config);
    std::future<ApiResult> deep_agent_execute_step(const json& config);
    std::future<ApiResult> deep_agent_synthesize(const json& config);

    // ── GluonTS Service ──
    std::future<ApiResult> gluonts_forecast(const json& config);
    std::future<ApiResult> gluonts_evaluate(const json& config);

    // ── Statsmodels Service ──
    std::future<ApiResult> statsmodels_analyze(const json& config);

    // ── GS Quant Service ──
    std::future<ApiResult> gs_quant_analyze(const json& config);

    // ── VisionQuant (Pattern Intelligence) ──
    std::future<ApiResult> visionquant_search(const json& config);
    std::future<ApiResult> visionquant_score(const json& config);
    std::future<ApiResult> visionquant_backtest(const json& config);

    // ── Functime Service ──
    std::future<ApiResult> functime_forecast(const json& config);

    // ── Fortitudo Service ──
    std::future<ApiResult> fortitudo_analyze(const json& config);

    // ── CFA Quant Service ──
    std::future<ApiResult> cfa_quant_analyze(const json& config);

    // ── RL Trading ──
    std::future<ApiResult> rl_create_env(const json& config);
    std::future<ApiResult> rl_train_agent(const json& config);
    std::future<ApiResult> rl_evaluate(const json& config);

private:
    QuantLabApi() = default;

    // Execute via Python script (same as backtesting pattern)
    std::future<ApiResult> run_python(const std::string& script,
                                       const std::string& command,
                                       const json& args);

    // Execute via HTTP API
    std::future<ApiResult> http_post(const std::string& path, const json& body);
    std::future<ApiResult> http_get(const std::string& path,
                                     const std::map<std::string, std::string>& params = {});
};

} // namespace fincept::ai_quant_lab
