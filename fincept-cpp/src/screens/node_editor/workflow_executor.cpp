// Workflow Executor — Execution engine implementation

#include "workflow_executor.h"
#include "core/logger.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <queue>

namespace fincept::node_editor {

// ============================================================================
// DirectedGraph
// ============================================================================

void DirectedGraph::build(const std::vector<NodeInstance>& nodes, const std::vector<Link>& links) {
    node_ids_.clear();
    adj_.clear();
    rev_adj_.clear();
    in_degree_.clear();
    pin_to_node_.clear();
    connections_.clear();

    // Build pin-to-node mapping
    for (auto& node : nodes) {
        node_ids_.push_back(node.id);
        in_degree_[node.id] = 0;
        for (auto& p : node.inputs) pin_to_node_[p.id] = node.id;
        for (auto& p : node.outputs) pin_to_node_[p.id] = node.id;
    }

    // Build adjacency from links
    for (auto& link : links) {
        auto src_it = pin_to_node_.find(link.start_pin);
        auto dst_it = pin_to_node_.find(link.end_pin);
        if (src_it == pin_to_node_.end() || dst_it == pin_to_node_.end()) continue;

        int src_node = src_it->second;
        int dst_node = dst_it->second;

        // Avoid self-loops
        if (src_node == dst_node) continue;

        adj_[src_node].push_back(dst_node);
        rev_adj_[dst_node].push_back(src_node);
        in_degree_[dst_node]++;

        connections_.push_back({src_node, link.start_pin, dst_node, link.end_pin});
    }
}

std::vector<int> DirectedGraph::get_parents(int node_id) const {
    auto it = rev_adj_.find(node_id);
    if (it == rev_adj_.end()) return {};
    return it->second;
}

std::vector<int> DirectedGraph::get_children(int node_id) const {
    auto it = adj_.find(node_id);
    if (it == adj_.end()) return {};
    return it->second;
}

std::vector<int> DirectedGraph::topological_sort() const {
    // Kahn's algorithm
    std::map<int, int> deg = in_degree_;
    std::queue<int> q;

    for (auto& [id, d] : deg) {
        if (d == 0) q.push(id);
    }

    std::vector<int> order;
    order.reserve(node_ids_.size());

    while (!q.empty()) {
        int id = q.front();
        q.pop();
        order.push_back(id);

        auto children_it = adj_.find(id);
        if (children_it != adj_.end()) {
            for (int child : children_it->second) {
                deg[child]--;
                if (deg[child] == 0) q.push(child);
            }
        }
    }

    // If order doesn't include all nodes, there's a cycle
    if (order.size() != node_ids_.size()) {
        LOG_ERROR("WorkflowExecutor", "Cycle detected in workflow graph! Sorted %zu of %zu nodes",
                  order.size(), node_ids_.size());
        return {};
    }

    return order;
}

std::vector<DirectedGraph::PinConnection> DirectedGraph::get_incoming_connections(int node_id) const {
    std::vector<PinConnection> result;
    for (auto& conn : connections_) {
        if (conn.dst_node_id == node_id) {
            result.push_back(conn);
        }
    }
    return result;
}

// ============================================================================
// WorkflowExecutor
// ============================================================================

void WorkflowExecutor::update_status(int node_id, const std::string& status,
                                      const std::string& result, const std::string& error) {
    if (status_cb_) {
        status_cb_(node_id, status, result, error);
    }
}

std::map<std::string, nlohmann::json> WorkflowExecutor::gather_inputs(
    int node_id,
    const std::vector<NodeInstance>& nodes,
    const DirectedGraph& graph) const {

    std::map<std::string, nlohmann::json> inputs;
    auto incoming = graph.get_incoming_connections(node_id);

    for (auto& conn : incoming) {
        // Find the destination pin name
        std::string pin_name = "input";
        for (auto& node : nodes) {
            if (node.id != node_id) continue;
            for (auto& p : node.inputs) {
                if (p.id == conn.dst_pin_id) {
                    pin_name = p.name;
                    break;
                }
            }
            break;
        }

        // Get the upstream node's result
        auto result_it = results_.find(conn.src_node_id);
        if (result_it != results_.end() && result_it->second.success) {
            inputs[pin_name] = result_it->second.data;
        }
    }

    return inputs;
}

bool WorkflowExecutor::execute(std::vector<NodeInstance>& nodes, const std::vector<Link>& links) {
    executing_.store(true);
    cancel_requested_.store(false);
    results_.clear();

    // Reset all node statuses
    for (auto& node : nodes) {
        node.status = "idle";
        node.result.clear();
        node.error.clear();
        update_status(node.id, "idle");
    }

    // Build graph and get execution order
    DirectedGraph graph;
    graph.build(nodes, links);

    auto order = graph.topological_sort();
    if (order.empty() && !nodes.empty()) {
        LOG_ERROR("WorkflowExecutor", "Cannot execute: cycle detected");
        executing_.store(false);
        return false;
    }

    LOG_INFO("WorkflowExecutor", "Executing %zu nodes in topological order", order.size());

    bool all_success = true;

    for (int node_id : order) {
        if (cancel_requested_.load()) {
            LOG_INFO("WorkflowExecutor", "Execution cancelled");
            break;
        }

        // Find the node instance
        NodeInstance* node = nullptr;
        for (auto& n : nodes) {
            if (n.id == node_id) { node = &n; break; }
        }
        if (!node) continue;

        // Update status to running
        node->status = "running";
        update_status(node_id, "running");

        // Gather inputs from upstream nodes
        auto inputs = gather_inputs(node_id, nodes, graph);

        // Execute
        auto start_time = std::chrono::high_resolution_clock::now();
        ExecutionResult result;

        if (executor_fn_) {
            result = executor_fn_(*node, inputs);
        } else {
            // Fallback: no executor set
            result.success = true;
            result.display_text = "Executed (no executor configured)";
            result.data = nlohmann::json{{"status", "ok"}, {"node_type", node->type}};
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        result.exec_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        // Store result
        {
            std::lock_guard<std::mutex> lock(results_mutex_);
            results_[node_id] = result;
        }

        // Update node status
        if (result.success) {
            node->status = "completed";
            node->result = result.display_text;
            node->error.clear();
            update_status(node_id, "completed", result.display_text);
        } else {
            node->status = "error";
            node->result.clear();
            node->error = result.error;
            update_status(node_id, "error", "", result.error);
            all_success = false;

            // Check continue-on-fail (for now, always continue)
            LOG_WARN("WorkflowExecutor", "Node '%s' (id=%d) failed: %s",
                     node->label.c_str(), node_id, result.error.c_str());
        }
    }

    LOG_INFO("WorkflowExecutor", "Workflow execution %s (%zu nodes)",
             all_success ? "completed" : "completed with errors", order.size());
    executing_.store(false);
    return all_success;
}

void WorkflowExecutor::execute_async(std::vector<NodeInstance>& nodes, const std::vector<Link>& links) {
    if (executing_.load()) return;

    std::thread([this, &nodes, &links]() {
        execute(nodes, links);
    }).detach();
}

} // namespace fincept::node_editor
