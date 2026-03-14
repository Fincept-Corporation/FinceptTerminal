#pragma once
// Algo Service — deployment lifecycle, backtesting, scanning, process management
// Bridges between the UI screen and the backend (DB, Python scripts, unified trading)

#include "algo_types.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace fincept::algo {

class AlgoService {
public:
    static AlgoService& instance();

    // --- Deployment lifecycle ---
    // Deploy a strategy: spawn algo_live_runner.py, track PID, prefetch candles
    std::string deploy_strategy(const std::string& strategy_id,
                                 const std::string& symbol,
                                 const std::string& mode,
                                 const std::string& provider,
                                 const std::string& timeframe,
                                 double quantity,
                                 const std::string& params_json = "{}");

    // Stop a running deployment (kill process, update DB)
    bool stop_deployment(const std::string& deployment_id);

    // Stop all running deployments (emergency stop)
    int stop_all_deployments();

    // Delete a deployment and its cascaded data
    void delete_deployment(const std::string& deployment_id);

    // --- Backtesting ---
    // Run condition-based backtest via backtest_engine.py
    BacktestResult run_backtest(const std::string& strategy_id,
                                 const std::string& symbol,
                                 const std::string& start_date,
                                 const std::string& end_date,
                                 double capital,
                                 const std::string& provider = "fyers");

    // Run Python strategy backtest via python_backtest_engine.py
    BacktestResult run_python_backtest(const std::string& code,
                                        const std::string& symbol,
                                        const std::string& start_date,
                                        const std::string& end_date,
                                        double capital);

    // --- Scanner ---
    // Run condition-based scan across symbols via scanner_engine.py
    std::vector<ScanResult> run_scan(const std::string& conditions_json,
                                      const std::vector<std::string>& symbols,
                                      const std::string& timeframe,
                                      const std::string& provider = "fyers");

    // --- Condition evaluation ---
    // One-shot condition evaluation via condition_evaluator.py
    std::string evaluate_conditions(const std::string& conditions_json,
                                     const std::string& symbol,
                                     const std::string& timeframe);

    // --- Python Strategy Library ---
    // Parse _registry.py and return all built-in strategies
    std::vector<PythonStrategy> list_python_strategies() const;

    // Get categories from the registry
    std::vector<std::string> get_strategy_categories() const;

    // Read strategy source code from file
    std::string get_strategy_code(const std::string& strategy_id) const;

    // Validate Python syntax via `python -c "import ast; ast.parse(...)"`
    bool validate_python_syntax(const std::string& code, std::string& error_out) const;

    // Extract parameters from Python code (UPPER_CASE = number patterns)
    std::vector<StrategyParameter> extract_strategy_parameters(const std::string& code) const;

    // --- Process management ---
    bool is_pid_alive(int pid) const;
    bool verify_pid_is_algo_runner(int pid) const;
    bool safe_kill_process(int pid) const;

    // --- Deployment count ---
    int running_deployment_count() const;

    AlgoService(const AlgoService&) = delete;
    AlgoService& operator=(const AlgoService&) = delete;

private:
    AlgoService() = default;

    // Max concurrent deployments
    static constexpr int MAX_CONCURRENT = 10;

    // Get the path to algo_trading scripts directory
    std::string get_scripts_dir() const;
    // Get the path to strategies library directory
    std::string get_strategies_dir() const;
    // Get the main DB path for passing to Python scripts
    std::string get_db_path() const;

    mutable std::mutex mutex_;
};

} // namespace fincept::algo
