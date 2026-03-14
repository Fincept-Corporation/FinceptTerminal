#pragma once
// Backtesting Tab — async Python execution layer
// Same pattern as GovData but for backtesting provider scripts

#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::backtesting {

class BacktestData {
public:
    // Run a backtesting provider script asynchronously
    // provider_dir: folder name under Analytics/backtesting/ (e.g. "vectorbt")
    // command: python action (e.g. "run_backtest")
    // args_json: JSON string of arguments
    void run(const std::string& provider_dir, const std::string& command,
             const std::string& args_json);

    // State queries
    bool is_loading() const { return loading_.load(); }
    bool has_result() const;
    const std::string& error() const;

    // Access the JSON result (caller must hold mutex)
    std::mutex& mutex() { return mutex_; }
    const nlohmann::json& result() const { return result_; }

    // Clear previous result
    void clear();

private:
    std::mutex mutex_;
    std::atomic<bool> loading_{false};
    nlohmann::json result_;
    std::string error_;
    bool has_result_ = false;

    void parse_result(const std::string& output);
};

} // namespace fincept::backtesting
