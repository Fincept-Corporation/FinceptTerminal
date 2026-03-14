#pragma once
// Node Executor — Dispatches node execution to real backends
// HTTP for data sources, Python for indicators/agents, mock for trading/notifications

#include "workflow_executor.h"
#include "node_types.h"
#include <string>
#include <map>
#include <nlohmann/json.hpp>

namespace fincept::node_editor {

class NodeExecutor {
public:
    // Execute a single node with its inputs, returns result
    static ExecutionResult execute(const NodeInstance& node,
                                   const std::map<std::string, nlohmann::json>& inputs);

    // Get the executor function suitable for WorkflowExecutor::set_executor()
    static NodeExecutorFn get_executor_fn();

private:
    // Real executors (call external systems)
    static ExecutionResult exec_data_source(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_technical_indicator(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_agent(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_backtest(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_yfinance(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_http_request(const NodeInstance& node, const nlohmann::json& input);

    // In-process executors (data manipulation)
    static ExecutionResult exec_transform(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_control_flow(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_results_display(const NodeInstance& node, const nlohmann::json& input);

    // Stub executors (log only)
    static ExecutionResult exec_trading_stub(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_notification_stub(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_trigger_stub(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_safety_check(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_file_stub(const NodeInstance& node, const nlohmann::json& input);
    static ExecutionResult exec_generic(const NodeInstance& node, const nlohmann::json& input);

    // Helpers
    static std::string get_param_str(const NodeInstance& node, const std::string& key, const std::string& fallback = "");
    static double get_param_num(const NodeInstance& node, const std::string& key, double fallback = 0.0);
    static int get_param_int(const NodeInstance& node, const std::string& key, int fallback = 0);
    static bool get_param_bool(const NodeInstance& node, const std::string& key, bool fallback = false);
};

} // namespace fincept::node_editor
