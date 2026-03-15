#include "backtesting_data.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "core/logger.h"
#include <thread>
#include <nlohmann/json.hpp>

namespace fincept::backtesting {

using json = nlohmann::json;

void BacktestData::run(const std::string& provider_dir,
                        const std::string& command,
                        const std::string& args_json) {
    if (loading_.load()) {
        LOG_WARN("BacktestData", "Already running, ignoring new request");
        return;
    }
    LOG_INFO("BacktestData", "Starting: provider=%s command=%s", provider_dir.c_str(), command.c_str());
    loading_.store(true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        has_result_ = false;
        error_.clear();
        result_ = json();
    }

    // Script path: Analytics/backtesting/{provider}/{provider}_provider.py
    std::string script = "Analytics/backtesting/" + provider_dir + "/" + provider_dir + "_provider.py";

    std::thread([this, script, command, args_json]() {
        auto& cache = CacheService::instance();
        std::string cache_key = CacheService::make_key("reference", "backtest",
            script + "_" + command + "_" + args_json.substr(0, std::min((size_t)64, args_json.size())));

        auto cached = cache.get(cache_key);
        if (cached && !cached->empty()) {
            LOG_INFO("BacktestData", "Cache hit for: %s", cache_key.c_str());
            std::lock_guard<std::mutex> lock(mutex_);
            parse_result(*cached);
            loading_.store(false);
            return;
        }

        std::vector<std::string> args = {command, args_json};
        auto py_result = python::execute(script, args);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (py_result.success) {
                LOG_INFO("BacktestData", "Script completed successfully, parsing result");
                parse_result(py_result.output);
                if (has_result_ && !py_result.output.empty()) {
                    cache.set(cache_key, py_result.output, "reference", CacheTTL::FIFTEEN_MIN);
                }
            } else {
                error_ = py_result.error.empty()
                    ? "Script failed (exit " + std::to_string(py_result.exit_code) + ")"
                    : py_result.error;
                has_result_ = false;
                LOG_ERROR("BacktestData", "Script failed: %s — %s",
                          script.c_str(), error_.c_str());
            }
        }
        loading_.store(false);
    }).detach();
}

bool BacktestData::has_result() const {
    return has_result_;
}

const std::string& BacktestData::error() const {
    return error_;
}

void BacktestData::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = json();
    error_.clear();
    has_result_ = false;
}

void BacktestData::parse_result(const std::string& output) {
    try {
        std::string json_str = python::extract_json(output);
        if (json_str.empty()) json_str = output;

        auto parsed = json::parse(json_str);

        // Handle {success: true, data: {...}} wrapper
        if (parsed.contains("success") && parsed["success"].is_boolean()) {
            if (parsed["success"].get<bool>()) {
                result_ = parsed;  // Keep full response (has data + success)
                has_result_ = true;
            } else {
                error_ = parsed.value("error", "Unknown error from Python");
                has_result_ = false;
            }
        }
        // Handle {data: ..., error: null} wrapper
        else if (parsed.contains("data") && parsed.contains("error")) {
            if (parsed["error"].is_null()) {
                result_ = parsed;
                has_result_ = true;
            } else {
                error_ = parsed["error"].get<std::string>();
                has_result_ = false;
            }
        }
        else {
            // Raw JSON result — wrap it
            result_ = parsed;
            has_result_ = true;
        }
    } catch (const json::exception& e) {
        error_ = std::string("JSON parse error: ") + e.what();
        has_result_ = false;
        LOG_ERROR("BacktestData", "JSON parse failed: %s", e.what());
    }
}

} // namespace fincept::backtesting
