#pragma once
// Algo Trading Types — mirrors Tauri types.ts + Rust structs
// Used by algo_service, algo_trading_screen, and db operations

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace fincept::algo {

using json = nlohmann::json;

// ============================================================================
// Condition system — JSON-serialized condition trees
// ============================================================================

struct ConditionItem {
    std::string indicator;      // e.g. "rsi", "sma", "close"
    std::string operator_type;  // e.g. "crosses_above", ">", "between"
    std::string value;          // threshold or second indicator
    std::string value2;         // for "between" operator
    std::string field;          // "value" or specific field like "close", "volume"
    std::string timeframe;      // override per-condition timeframe
    int period = 14;            // indicator period
    int period2 = 0;            // second period (for MACD signal, etc.)
};

inline void to_json(json& j, const ConditionItem& c) {
    j = json{{"indicator", c.indicator}, {"operator", c.operator_type},
             {"value", c.value}, {"value2", c.value2}, {"field", c.field},
             {"timeframe", c.timeframe}, {"period", c.period}, {"period2", c.period2}};
}

inline void from_json(const json& j, ConditionItem& c) {
    c.indicator = j.value("indicator", "");
    c.operator_type = j.value("operator", ">");
    c.value = j.value("value", "");
    c.value2 = j.value("value2", "");
    c.field = j.value("field", "value");
    c.timeframe = j.value("timeframe", "");
    c.period = j.value("period", 14);
    c.period2 = j.value("period2", 0);
}

// ============================================================================
// Strategy
// ============================================================================

struct AlgoStrategy {
    std::string id;
    std::string name;
    std::string description;
    std::string symbols;            // JSON array string e.g. '["RELIANCE","TCS"]'
    std::string entry_conditions;   // JSON array of ConditionItem
    std::string exit_conditions;    // JSON array of ConditionItem
    std::string entry_logic = "AND";
    std::string exit_logic = "AND";
    std::string timeframe = "5m";
    double stop_loss = 0;
    double take_profit = 0;
    double trailing_stop = 0;
    double position_size = 1;
    int max_positions = 1;
    std::string provider = "fyers";
    bool is_active = true;
    std::string created_at;
    std::string updated_at;
};

// ============================================================================
// Deployment
// ============================================================================

enum class DeploymentStatus {
    Starting,
    Running,
    Stopped,
    Error,
    Completed
};

inline const char* deployment_status_str(DeploymentStatus s) {
    switch (s) {
        case DeploymentStatus::Starting:  return "starting";
        case DeploymentStatus::Running:   return "running";
        case DeploymentStatus::Stopped:   return "stopped";
        case DeploymentStatus::Error:     return "error";
        case DeploymentStatus::Completed: return "completed";
    }
    return "unknown";
}

inline DeploymentStatus parse_deployment_status(const std::string& s) {
    if (s == "running")   return DeploymentStatus::Running;
    if (s == "stopped")   return DeploymentStatus::Stopped;
    if (s == "error")     return DeploymentStatus::Error;
    if (s == "completed") return DeploymentStatus::Completed;
    return DeploymentStatus::Starting;
}

struct AlgoDeployment {
    std::string id;
    std::string strategy_id;
    std::string strategy_name;
    std::string mode = "paper";       // "paper" or "live"
    DeploymentStatus status = DeploymentStatus::Starting;
    int pid = 0;
    std::string portfolio_id;
    std::string started_at;
    std::string stopped_at;
    std::string error_message;
    std::string config;               // JSON string
};

// ============================================================================
// Metrics
// ============================================================================

struct AlgoMetrics {
    std::string deployment_id;
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double total_pnl = 0;
    double max_drawdown = 0;
    double win_rate = 0;
    double avg_win = 0;
    double avg_loss = 0;
    double sharpe_ratio = 0;
    std::string updated_at;
};

// ============================================================================
// Trades
// ============================================================================

struct AlgoTrade {
    std::string id;
    std::string deployment_id;
    std::string symbol;
    std::string side;         // "buy" or "sell"
    double quantity = 0;
    double price = 0;
    double pnl = 0;
    double fees = 0;
    std::string order_type;   // "market", "limit"
    std::string status;       // "filled", "partial", "cancelled"
    std::string executed_at;
};

// ============================================================================
// Order Signals (bridge: Python runner -> order execution)
// ============================================================================

struct AlgoOrderSignal {
    std::string id;
    std::string deployment_id;
    std::string symbol;
    std::string side;
    double quantity = 0;
    std::string order_type = "market";
    double price = 0;
    double stop_price = 0;
    std::string status = "pending";  // "pending", "processing", "executed", "failed"
    std::string created_at;
    std::string executed_at;
    std::string error;
};

// ============================================================================
// Candle Data
// ============================================================================

struct CandleData {
    std::string symbol;
    std::string timeframe;
    int64_t timestamp = 0;
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double volume = 0;
    std::string provider;
};

// ============================================================================
// Scanner
// ============================================================================

struct ScanResult {
    std::string symbol;
    bool entry_match = false;
    bool exit_match = false;
    double last_price = 0;
    double volume = 0;
    std::string details;       // JSON string with indicator values
    std::string scanned_at;
};

// ============================================================================
// Python Strategy Library
// ============================================================================

struct PythonStrategy {
    std::string id;            // e.g. "FCT-00001"
    std::string name;
    std::string description;
    std::string category;
    std::string filename;
    std::vector<std::string> tags;
};

struct CustomPythonStrategy {
    std::string id;
    std::string name;
    std::string description;
    std::string code;
    std::string category = "custom";
    std::string parameters;    // JSON string
    std::string created_at;
    std::string updated_at;
};

struct StrategyParameter {
    std::string name;
    std::string type;          // "int", "float", "str", "bool"
    std::string default_value;
    std::string description;
};

// ============================================================================
// Backtest Results (returned from Python backtest scripts)
// ============================================================================

struct BacktestTrade {
    std::string symbol;
    std::string side;
    double entry_price = 0;
    double exit_price = 0;
    double quantity = 0;
    double pnl = 0;
    double pnl_pct = 0;
    std::string entry_time;
    std::string exit_time;
};

struct BacktestMetrics {
    double total_return = 0;
    double total_return_pct = 0;
    double max_drawdown = 0;
    double sharpe_ratio = 0;
    double win_rate = 0;
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double avg_win = 0;
    double avg_loss = 0;
    double profit_factor = 0;
    double avg_trade_duration = 0;
};

struct EquityPoint {
    std::string date;
    double equity = 0;
    double drawdown = 0;
};

struct BacktestResult {
    bool success = false;
    std::string error;
    BacktestMetrics metrics;
    std::vector<BacktestTrade> trades;
    std::vector<EquityPoint> equity_curve;
};

// ============================================================================
// Price Cache Entry
// ============================================================================

struct PriceCacheEntry {
    std::string symbol;
    double price = 0;
    double volume = 0;
    double change_pct = 0;
    std::string updated_at;
};

// ============================================================================
// Trading mode for deployment
// ============================================================================

enum class AlgoTradingMode {
    Paper,
    Live
};

inline const char* trading_mode_str(AlgoTradingMode m) {
    return m == AlgoTradingMode::Live ? "live" : "paper";
}

} // namespace fincept::algo
