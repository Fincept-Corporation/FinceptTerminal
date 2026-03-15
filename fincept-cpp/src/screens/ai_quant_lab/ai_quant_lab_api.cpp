#include "ai_quant_lab_api.h"
#include "python/python_runner.h"
#include "http/http_client.h"
#include "storage/cache_service.h"
#include "core/logger.h"
#include <thread>

namespace fincept::ai_quant_lab {

static constexpr const char* API_BASE = "https://api.fincept.in";

QuantLabApi& QuantLabApi::instance() {
    static QuantLabApi inst;
    return inst;
}

// ============================================================================
// Python execution (mirrors BacktestData pattern)
// ============================================================================
std::future<ApiResult> QuantLabApi::run_python(const std::string& script,
                                                const std::string& command,
                                                const json& args) {
    return std::async(std::launch::async, [script, command, args]() -> ApiResult {
        ApiResult result;
        std::string args_str = args.dump();

        auto py = python::execute(script, {command, args_str});
        if (py.success) {
            std::string json_str = python::extract_json(py.output);
            if (json_str.empty()) json_str = py.output;
            try {
                auto parsed = json::parse(json_str);
                if (parsed.contains("success") && parsed["success"].is_boolean()) {
                    result.success = parsed["success"].get<bool>();
                    result.data = parsed.value("data", parsed);
                    if (!result.success)
                        result.error = parsed.value("error", "Unknown error");
                } else {
                    result.success = true;
                    result.data = parsed;
                }
            } catch (const json::exception& e) {
                result.error = std::string("JSON parse error: ") + e.what();
            }
        } else {
            result.error = py.error.empty()
                ? "Script failed (exit " + std::to_string(py.exit_code) + ")"
                : py.error;
        }
        return result;
    });
}

// ============================================================================
// HTTP calls
// ============================================================================
std::future<ApiResult> QuantLabApi::http_post(const std::string& path, const json& body) {
    return std::async(std::launch::async, [path, body]() -> ApiResult {
        ApiResult result;
        std::string url = std::string(API_BASE) + path;

        auto resp = http::HttpClient::instance().post_json(url, body);
        if (resp.success && resp.status_code >= 200 && resp.status_code < 300) {
            result.success = true;
            result.data = resp.json_body();
        } else {
            result.error = resp.error.empty()
                ? "HTTP " + std::to_string(resp.status_code) : resp.error;
            auto j = resp.json_body();
            if (j.contains("error"))
                result.error = j["error"].get<std::string>();
        }
        return result;
    });
}

std::future<ApiResult> QuantLabApi::http_get(const std::string& path,
                                              const std::map<std::string, std::string>& params) {
    return std::async(std::launch::async, [path, params]() -> ApiResult {
        ApiResult result;
        std::string url = std::string(API_BASE) + path;
        if (!params.empty()) {
            url += "?";
            bool first = true;
            for (auto& [k, v] : params) {
                if (!first) url += "&";
                url += k + "=" + v;
                first = false;
            }
        }

        auto resp = http::HttpClient::instance().get(url);
        if (resp.success && resp.status_code >= 200 && resp.status_code < 300) {
            result.success = true;
            result.data = resp.json_body();
        } else {
            result.error = resp.error.empty()
                ? "HTTP " + std::to_string(resp.status_code) : resp.error;
        }
        return result;
    });
}

// ============================================================================
// Qlib Service — ai_quant_lab/qlib_service.py
// ============================================================================
std::future<ApiResult> QuantLabApi::qlib_list_models() {
    return run_python("ai_quant_lab/qlib_service.py", "list_models", json::object());
}

std::future<ApiResult> QuantLabApi::qlib_train_model(const json& config) {
    return run_python("ai_quant_lab/qlib_service.py", "train_model", config);
}

std::future<ApiResult> QuantLabApi::qlib_run_backtest(const json& config) {
    return run_python("ai_quant_lab/qlib_service.py", "run_backtest", config);
}

std::future<ApiResult> QuantLabApi::qlib_get_live_predictions(const json& config) {
    return run_python("ai_quant_lab/qlib_service.py", "predict", config);
}

// ── Online Learning — ai_quant_lab/qlib_online_learning.py ──
std::future<ApiResult> QuantLabApi::qlib_online_create(const json& config) {
    return run_python("ai_quant_lab/qlib_online_learning.py", "create_model", config);
}

std::future<ApiResult> QuantLabApi::qlib_online_train(const json& config) {
    return run_python("ai_quant_lab/qlib_online_learning.py", "train", config);
}

std::future<ApiResult> QuantLabApi::qlib_online_performance(const std::string& model_id) {
    // Python script expects raw model_id string as sys.argv[2], not JSON
    return std::async(std::launch::async, [model_id]() -> ApiResult {
        ApiResult result;
        auto py = python::execute("ai_quant_lab/qlib_online_learning.py",
                                  {"performance", model_id});
        if (py.success) {
            std::string json_str = python::extract_json(py.output);
            if (json_str.empty()) json_str = py.output;
            try {
                auto parsed = json::parse(json_str);
                if (parsed.contains("success") && parsed["success"].is_boolean()) {
                    result.success = parsed["success"].get<bool>();
                    result.data = parsed.value("data", parsed);
                    if (!result.success)
                        result.error = parsed.value("error", "Unknown error");
                } else {
                    result.success = true;
                    result.data = parsed;
                }
            } catch (const json::exception& e) {
                result.error = std::string("JSON parse error: ") + e.what();
            }
        } else {
            result.error = py.error.empty()
                ? "Script failed (exit " + std::to_string(py.exit_code) + ")"
                : py.error;
        }
        return result;
    });
}

std::future<ApiResult> QuantLabApi::qlib_online_handle_drift(const json& config) {
    return run_python("ai_quant_lab/qlib_online_learning.py", "handle_drift", config);
}

std::future<ApiResult> QuantLabApi::qlib_online_setup_rolling(const json& config) {
    return run_python("ai_quant_lab/qlib_online_learning.py", "setup_rolling", config);
}

// ── Meta Learning — ai_quant_lab/qlib_meta_learning.py ──
std::future<ApiResult> QuantLabApi::qlib_meta_list_models() {
    return run_python("ai_quant_lab/qlib_meta_learning.py", "list_models", json::object());
}

std::future<ApiResult> QuantLabApi::qlib_meta_run_selection(const json& config) {
    return run_python("ai_quant_lab/qlib_meta_learning.py", "run_selection", config);
}

// ── Rolling Retraining — ai_quant_lab/qlib_rolling_retraining.py ──
std::future<ApiResult> QuantLabApi::qlib_rolling_list_schedules() {
    return run_python("ai_quant_lab/qlib_rolling_retraining.py", "list", json::object());
}

std::future<ApiResult> QuantLabApi::qlib_rolling_create_schedule(const json& config) {
    // Python script expects raw model_id string as sys.argv[2]
    return std::async(std::launch::async, [config]() -> ApiResult {
        ApiResult result;
        std::string model_id = config.value("model_name", "model_1");
        auto py = python::execute("ai_quant_lab/qlib_rolling_retraining.py",
                                  {"create", model_id});
        if (py.success) {
            std::string json_str = python::extract_json(py.output);
            if (json_str.empty()) json_str = py.output;
            try {
                auto parsed = json::parse(json_str);
                result.success = parsed.value("success", true);
                result.data = parsed;
                if (!result.success)
                    result.error = parsed.value("error", "Unknown error");
            } catch (const json::exception& e) {
                result.error = std::string("JSON parse error: ") + e.what();
            }
        } else {
            result.error = py.error.empty()
                ? "Script failed (exit " + std::to_string(py.exit_code) + ")"
                : py.error;
        }
        return result;
    });
}

std::future<ApiResult> QuantLabApi::qlib_rolling_retrain(const std::string& schedule_id) {
    // Python script expects raw model_id string as sys.argv[2]
    return std::async(std::launch::async, [schedule_id]() -> ApiResult {
        ApiResult result;
        auto py = python::execute("ai_quant_lab/qlib_rolling_retraining.py",
                                  {"retrain", schedule_id});
        if (py.success) {
            std::string json_str = python::extract_json(py.output);
            if (json_str.empty()) json_str = py.output;
            try {
                auto parsed = json::parse(json_str);
                result.success = parsed.value("success", true);
                result.data = parsed;
                if (!result.success)
                    result.error = parsed.value("error", "Unknown error");
            } catch (const json::exception& e) {
                result.error = std::string("JSON parse error: ") + e.what();
            }
        } else {
            result.error = py.error.empty()
                ? "Script failed (exit " + std::to_string(py.exit_code) + ")"
                : py.error;
        }
        return result;
    });
}

// ── Advanced Models — ai_quant_lab/qlib_advanced_models.py ──
std::future<ApiResult> QuantLabApi::qlib_advanced_list_models() {
    return run_python("ai_quant_lab/qlib_advanced_models.py", "list_models", json::object());
}

std::future<ApiResult> QuantLabApi::qlib_advanced_train(const json& config) {
    return run_python("ai_quant_lab/qlib_advanced_models.py", "train", config);
}

// ============================================================================
// RD-Agent (Factor Discovery) — ai_quant_lab/rd_agent_service.py
// ============================================================================
std::future<ApiResult> QuantLabApi::rdagent_start_mining(const json& config) {
    return run_python("ai_quant_lab/rd_agent_service.py", "start_factor_mining", config);
}

std::future<ApiResult> QuantLabApi::rdagent_get_status(const std::string& task_id) {
    // Python script expects raw task_id string as sys.argv[2], not JSON
    return std::async(std::launch::async, [task_id]() -> ApiResult {
        ApiResult result;
        auto py = python::execute("ai_quant_lab/rd_agent_service.py",
                                  {"get_factor_mining_status", task_id});
        if (py.success) {
            std::string json_str = python::extract_json(py.output);
            if (json_str.empty()) json_str = py.output;
            try {
                auto parsed = json::parse(json_str);
                if (parsed.contains("success") && parsed["success"].is_boolean()) {
                    result.success = parsed["success"].get<bool>();
                    result.data = parsed.value("data", parsed);
                    if (!result.success)
                        result.error = parsed.value("error", "Unknown error");
                } else {
                    result.success = true;
                    result.data = parsed;
                }
            } catch (const json::exception& e) {
                result.error = std::string("JSON parse error: ") + e.what();
            }
        } else {
            result.error = py.error.empty()
                ? "Script failed (exit " + std::to_string(py.exit_code) + ")"
                : py.error;
        }
        return result;
    });
}

std::future<ApiResult> QuantLabApi::rdagent_get_factors(const std::string& task_id) {
    // Python script expects raw task_id string as sys.argv[2], not JSON
    return std::async(std::launch::async, [task_id]() -> ApiResult {
        ApiResult result;
        auto py = python::execute("ai_quant_lab/rd_agent_service.py",
                                  {"get_discovered_factors", task_id});
        if (py.success) {
            std::string json_str = python::extract_json(py.output);
            if (json_str.empty()) json_str = py.output;
            try {
                auto parsed = json::parse(json_str);
                if (parsed.contains("success") && parsed["success"].is_boolean()) {
                    result.success = parsed["success"].get<bool>();
                    result.data = parsed.value("data", parsed);
                    if (!result.success)
                        result.error = parsed.value("error", "Unknown error");
                } else {
                    result.success = true;
                    result.data = parsed;
                }
            } catch (const json::exception& e) {
                result.error = std::string("JSON parse error: ") + e.what();
            }
        } else {
            result.error = py.error.empty()
                ? "Script failed (exit " + std::to_string(py.exit_code) + ")"
                : py.error;
        }
        return result;
    });
}

// ============================================================================
// Deep Agent — agents/deepagents/cli.py
// ============================================================================
std::future<ApiResult> QuantLabApi::deep_agent_create_plan(const json& config) {
    return run_python("agents/deepagents/cli.py", "create_plan", config);
}

std::future<ApiResult> QuantLabApi::deep_agent_execute_step(const json& config) {
    return run_python("agents/deepagents/cli.py", "execute_step", config);
}

std::future<ApiResult> QuantLabApi::deep_agent_synthesize(const json& config) {
    return run_python("agents/deepagents/cli.py", "synthesize_results", config);
}

// ============================================================================
// GluonTS — Analytics/gluonts_wrapper/gluonts_service.py
// ============================================================================
std::future<ApiResult> QuantLabApi::gluonts_forecast(const json& config) {
    // Python dispatch uses "forecast_<model>" as the operation name
    std::string model = config.value("model", "deepar");
    std::string command = "forecast_" + model;
    return run_python("Analytics/gluonts_wrapper/gluonts_service.py", command, config);
}

std::future<ApiResult> QuantLabApi::gluonts_evaluate(const json& config) {
    return run_python("Analytics/gluonts_wrapper/gluonts_service.py", "evaluate", config);
}

// ============================================================================
// Statsmodels — Analytics/statsmodels_cli.py
// ============================================================================
std::future<ApiResult> QuantLabApi::statsmodels_analyze(const json& config) {
    // Map UI analysis labels to Python command names
    std::string analysis = config.value("analysis_type", "descriptive");
    std::string command = "descriptive";
    if (analysis == "ARIMA")           command = "arima";
    else if (analysis == "SARIMAX")    command = "sarimax";
    else if (analysis == "Exp Smoothing") command = "exponential_smoothing";
    else if (analysis == "STL Decompose") command = "stl_decompose";
    else if (analysis == "ADF Test")   command = "adf_test";
    else if (analysis == "KPSS Test")  command = "kpss_test";
    else if (analysis == "ACF")        command = "acf";
    else if (analysis == "PACF")       command = "pacf";
    return run_python("Analytics/statsmodels_cli.py", command, config);
}

// ============================================================================
// GS Quant — Analytics/gs_quant_wrapper/gs_quant_service.py
// ============================================================================
std::future<ApiResult> QuantLabApi::gs_quant_analyze(const json& config) {
    return run_python("Analytics/gs_quant_wrapper/gs_quant_service.py", "analyze", config);
}

// ============================================================================
// VisionQuant (Pattern Intelligence) — vision_quant/*.py
// ============================================================================
std::future<ApiResult> QuantLabApi::visionquant_search(const json& config) {
    return run_python("vision_quant/engine.py", "search", config);
}

std::future<ApiResult> QuantLabApi::visionquant_score(const json& config) {
    return run_python("vision_quant/scorer.py", "score", config);
}

std::future<ApiResult> QuantLabApi::visionquant_backtest(const json& config) {
    return run_python("vision_quant/backtester.py", "backtest", config);
}

// ============================================================================
// Functime — Analytics/functime_wrapper/functime_service.py
// ============================================================================
std::future<ApiResult> QuantLabApi::functime_forecast(const json& config) {
    return run_python("Analytics/functime_wrapper/functime_service.py", "forecast", config);
}

// ============================================================================
// Fortitudo — Analytics/fortitudo_tech_wrapper/fortitudo_service.py
// ============================================================================
std::future<ApiResult> QuantLabApi::fortitudo_analyze(const json& config) {
    // Map UI mode labels to Python handler names
    std::string mode = config.value("mode", "full_analysis");
    std::string command = "full_analysis";
    if (mode == "Entropy Pooling")  command = "entropy_pooling";
    else if (mode == "Optimization") command = "optimize_mean_variance";
    else if (mode == "Option Pricing") command = "option_pricing";
    else if (mode == "Portfolio Risk") command = "portfolio_metrics";
    return run_python("Analytics/fortitudo_tech_wrapper/fortitudo_service.py", command, config);
}

// ============================================================================
// CFA Quant — quant/cfa_quant_service.py (uses quant analytics)
// ============================================================================
std::future<ApiResult> QuantLabApi::cfa_quant_analyze(const json& config) {
    return run_python("Analytics/quant_analytics_cli.py", "analyze", config);
}

// ============================================================================
// RL Trading — ai_quant_lab/qlib_rl.py
// ============================================================================
std::future<ApiResult> QuantLabApi::rl_create_env(const json& config) {
    return run_python("ai_quant_lab/qlib_rl.py", "create_env", config);
}

std::future<ApiResult> QuantLabApi::rl_train_agent(const json& config) {
    return run_python("ai_quant_lab/qlib_rl.py", "train", config);
}

std::future<ApiResult> QuantLabApi::rl_evaluate(const json& config) {
    return run_python("ai_quant_lab/qlib_rl.py", "evaluate", config);
}

} // namespace fincept::ai_quant_lab
