#pragma once
// Workflow Executor — Real execution engine with topological ordering,
// input propagation, status callbacks, and background thread execution

#include "node_types.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

namespace fincept::node_editor {

// ============================================================================
// Execution result from a single node
// ============================================================================
struct ExecutionResult {
    bool success = false;
    nlohmann::json data;       // Output data to pass downstream
    std::string error;         // Error message if failed
    double exec_time_ms = 0;   // Execution duration
    std::string display_text;  // Human-readable result summary
};

// ============================================================================
// Directed graph for workflow topology
// ============================================================================
class DirectedGraph {
public:
    void build(const std::vector<NodeInstance>& nodes, const std::vector<Link>& links);

    std::vector<int> get_parents(int node_id) const;
    std::vector<int> get_children(int node_id) const;

    // Returns topologically sorted node IDs, or empty if cycle detected
    std::vector<int> topological_sort() const;

    // Find which output pin of src connects to which input pin of dst
    struct PinConnection {
        int src_node_id;
        int src_pin_id;
        int dst_node_id;
        int dst_pin_id;
    };
    std::vector<PinConnection> get_incoming_connections(int node_id) const;

private:
    std::vector<int> node_ids_;
    std::map<int, std::vector<int>> adj_;       // parent -> children
    std::map<int, std::vector<int>> rev_adj_;   // child -> parents
    std::map<int, int> in_degree_;
    std::vector<PinConnection> connections_;

    // Pin-to-node mapping
    std::map<int, int> pin_to_node_;
};

// ============================================================================
// Node executor function signature
// ============================================================================
using NodeExecutorFn = std::function<ExecutionResult(
    const NodeInstance& node,
    const std::map<std::string, nlohmann::json>& inputs  // pin_name -> data from upstream
)>;

// ============================================================================
// Workflow Executor
// ============================================================================
class WorkflowExecutor {
public:
    // Status callback: called when a node's status changes
    using StatusCallback = std::function<void(int node_id, const std::string& status,
                                              const std::string& result, const std::string& error)>;

    void set_status_callback(StatusCallback cb) { status_cb_ = std::move(cb); }
    void set_executor(NodeExecutorFn fn) { executor_fn_ = std::move(fn); }

    // Execute workflow synchronously (call from background thread)
    bool execute(std::vector<NodeInstance>& nodes, const std::vector<Link>& links);

    // Execute workflow on a background thread
    void execute_async(std::vector<NodeInstance>& nodes, const std::vector<Link>& links);

    // Check if currently executing
    bool is_executing() const { return executing_.load(); }

    // Cancel execution
    void cancel() { cancel_requested_.store(true); }

    // Get results map (node_id -> result)
    const std::map<int, ExecutionResult>& results() const { return results_; }

private:
    // Gather inputs for a node from upstream results
    std::map<std::string, nlohmann::json> gather_inputs(
        int node_id,
        const std::vector<NodeInstance>& nodes,
        const DirectedGraph& graph) const;

    void update_status(int node_id, const std::string& status,
                       const std::string& result = "", const std::string& error = "");

    NodeExecutorFn executor_fn_;
    StatusCallback status_cb_;
    std::map<int, ExecutionResult> results_;
    std::atomic<bool> executing_{false};
    std::atomic<bool> cancel_requested_{false};
    std::mutex results_mutex_;
};

} // namespace fincept::node_editor
