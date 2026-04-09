#include "services/workflow/WorkflowExecutor.h"

#include "core/logging/Logger.h"
#include "services/workflow/AuditLogger.h"
#include "services/workflow/ExecutionHooks.h"
#include "services/workflow/NodeRegistry.h"

#include <QDateTime>
#include <QJsonArray>
#include <QMetaObject>
#include <QSet>

namespace fincept::workflow {

WorkflowExecutor::WorkflowExecutor(QObject* parent) : QObject(parent) {}

void WorkflowExecutor::execute(const WorkflowDef& workflow) {
    if (running_) {
        LOG_WARN("Executor", "Execution already in progress");
        return;
    }

    workflow_ = workflow;
    running_ = true;
    stop_requested_ = false;
    pending_count_ = 0;
    completed_count_ = 0;
    results_.clear();
    node_map_.clear();
    type_cache_.clear();
    in_degree_.clear();

    const int n = workflow_.nodes.size();
    node_map_.reserve(n);
    results_.reserve(n);
    type_cache_.reserve(n);
    in_degree_.reserve(n);

    // Index nodes by id and pre-resolve type pointers
    for (const auto& nd : workflow_.nodes) {
        node_map_.insert(nd.id, nd);
        type_cache_.insert(nd.id, NodeRegistry::instance().find(nd.type));
    }

    build_graph();

    // Cycle detection
    if (has_cycle()) {
        LOG_ERROR("Executor", "Workflow contains a cycle — aborting");
        running_ = false;
        WorkflowExecutionResult wr;
        wr.workflow_id = workflow_.id;
        wr.success = false;
        wr.error = "Workflow contains a cycle";
        emit execution_finished(wr);
        return;
    }

    QVector<QString> order = topological_sort();

    if (order.isEmpty()) {
        LOG_WARN("Executor", "No executable nodes in workflow");
        running_ = false;
        WorkflowExecutionResult wr;
        wr.workflow_id = workflow_.id;
        wr.success = true;
        emit execution_finished(wr);
        return;
    }

    // Build in-degree map from topological set
    QSet<QString> exec_set;
    exec_set.reserve(order.size());
    for (const auto& id : order)
        exec_set.insert(id);

    for (const auto& id : order) {
        int deg = 0;
        for (const auto& edge : incoming_edges_.value(id)) {
            if (exec_set.contains(edge.source_node))
                ++deg;
        }
        in_degree_.insert(id, deg);
    }

    total_count_ = order.size();

    start_time_ms_ = QDateTime::currentMSecsSinceEpoch();
    ExecutionHooks::instance().emit_workflow_start(workflow_.id);
    AuditLogger::instance().log(AuditAction::WorkflowStarted, workflow_.id, {}, {},
                                QString("Started: %1 (%2 nodes)").arg(workflow_.name).arg(total_count_));
    emit execution_started(workflow_.id);

    LOG_INFO("Executor", QString("Starting execution: %1 (%2 nodes)").arg(workflow_.name).arg(total_count_));

    launch_ready_nodes();
}

void WorkflowExecutor::execute_from(const WorkflowDef& workflow, const QString& start_node_id) {
    if (running_) {
        LOG_WARN("Executor", "Execution already in progress");
        return;
    }

    workflow_ = workflow;
    running_ = true;
    stop_requested_ = false;
    pending_count_ = 0;
    completed_count_ = 0;
    results_.clear();
    node_map_.clear();
    type_cache_.clear();
    in_degree_.clear();

    const int n = workflow_.nodes.size();
    node_map_.reserve(n);
    type_cache_.reserve(n);

    for (const auto& nd : workflow_.nodes) {
        node_map_.insert(nd.id, nd);
        type_cache_.insert(nd.id, NodeRegistry::instance().find(nd.type));
    }

    build_graph();

    if (has_cycle()) {
        running_ = false;
        WorkflowExecutionResult wr;
        wr.workflow_id = workflow_.id;
        wr.success = false;
        wr.error = "Workflow contains a cycle";
        emit execution_finished(wr);
        return;
    }

    // Get full topo order, then find start node and take only downstream
    QVector<QString> full_order = topological_sort();

    // Find descendants of start_node (BFS)
    QSet<QString> downstream;
    downstream.insert(start_node_id);
    QVector<QString> queue = {start_node_id};
    int front = 0;
    while (front < queue.size()) {
        QString current = queue[front++];
        for (const auto& child : adjacency_.value(current)) {
            if (!downstream.contains(child)) {
                downstream.insert(child);
                queue.append(child);
            }
        }
    }

    // Filter topo order to only downstream nodes
    QVector<QString> order;
    order.reserve(downstream.size());
    for (const auto& id : full_order) {
        if (downstream.contains(id))
            order.append(id);
    }

    if (order.isEmpty()) {
        running_ = false;
        WorkflowExecutionResult wr;
        wr.workflow_id = workflow_.id;
        wr.success = true;
        emit execution_finished(wr);
        return;
    }

    // Build in-degree map for the downstream subset
    QSet<QString> exec_set;
    exec_set.reserve(order.size());
    for (const auto& id : order)
        exec_set.insert(id);

    in_degree_.reserve(order.size());
    results_.reserve(order.size());
    for (const auto& id : order) {
        int deg = 0;
        for (const auto& edge : incoming_edges_.value(id)) {
            if (exec_set.contains(edge.source_node))
                ++deg;
        }
        in_degree_.insert(id, deg);
    }

    total_count_ = order.size();

    start_time_ms_ = QDateTime::currentMSecsSinceEpoch();
    ExecutionHooks::instance().emit_workflow_start(workflow_.id);
    emit execution_started(workflow_.id);

    LOG_INFO("Executor", QString("Partial execution from %1: %2 nodes").arg(start_node_id).arg(total_count_));

    launch_ready_nodes();
}

void WorkflowExecutor::stop() {
    if (!running_)
        return;
    stop_requested_ = true;
    LOG_INFO("Executor", "Stop requested");

    // If no nodes are in-flight, finish immediately.
    // Otherwise, in-flight nodes will complete and on_node_done will
    // see stop_requested_ and trigger finish_execution when pending reaches 0.
    if (pending_count_ == 0)
        finish_execution();
}

// ── Graph construction ─────────────────────────────────────────────────

void WorkflowExecutor::build_graph() {
    const int n = workflow_.nodes.size();
    adjacency_.clear();
    adjacency_.reserve(n);
    incoming_edges_.clear();
    incoming_edges_.reserve(n);

    // Initialize all nodes
    for (const auto& nd : workflow_.nodes) {
        adjacency_[nd.id];
        incoming_edges_[nd.id];
    }

    for (const auto& ed : workflow_.edges) {
        adjacency_[ed.source_node].append(ed.target_node);
        incoming_edges_[ed.target_node].append(ed);
    }
}

// ── Cycle detection (DFS with coloring) ────────────────────────────────

bool WorkflowExecutor::has_cycle() const {
    enum Color { White, Gray, Black };
    QHash<QString, Color> color;
    color.reserve(adjacency_.size());
    for (auto it = adjacency_.constBegin(); it != adjacency_.constEnd(); ++it)
        color[it.key()] = White;

    std::function<bool(const QString&)> dfs = [&](const QString& u) -> bool {
        color[u] = Gray;
        for (const auto& v : adjacency_.value(u)) {
            if (color.value(v) == Gray)
                return true; // back edge = cycle
            if (color.value(v) == White && dfs(v))
                return true;
        }
        color[u] = Black;
        return false;
    };

    for (auto it = color.constBegin(); it != color.constEnd(); ++it) {
        if (it.value() == White && dfs(it.key()))
            return true;
    }
    return false;
}

// ── Topological sort (Kahn's algorithm) ────────────────────────────────

QVector<QString> WorkflowExecutor::topological_sort() const {
    QHash<QString, int> in_deg;
    in_deg.reserve(adjacency_.size());
    for (auto it = adjacency_.constBegin(); it != adjacency_.constEnd(); ++it)
        in_deg[it.key()] = 0;

    for (auto it = adjacency_.constBegin(); it != adjacency_.constEnd(); ++it) {
        for (const auto& v : it.value())
            in_deg[v]++;
    }

    QVector<QString> queue;
    queue.reserve(adjacency_.size());
    for (auto it = in_deg.constBegin(); it != in_deg.constEnd(); ++it) {
        if (it.value() == 0)
            queue.append(it.key());
    }

    QVector<QString> order;
    order.reserve(adjacency_.size());
    int front = 0;
    while (front < queue.size()) {
        QString u = queue[front++];

        // Skip disabled nodes
        auto nd_it = node_map_.constFind(u);
        if (nd_it != node_map_.constEnd() && nd_it->disabled)
            continue;

        order.append(u);

        for (const auto& v : adjacency_.value(u)) {
            in_deg[v]--;
            if (in_deg[v] == 0)
                queue.append(v);
        }
    }

    return order;
}

// ── Parallel execution ─────────────────────────────────────────────────

void WorkflowExecutor::launch_ready_nodes() {
    if (stop_requested_) {
        if (pending_count_ == 0)
            finish_execution();
        return;
    }

    // Collect all nodes with in_degree_ == 0 (ready to run)
    QVector<QString> ready;
    for (auto it = in_degree_.begin(); it != in_degree_.end(); ++it) {
        if (it.value() == 0) {
            ready.append(it.key());
            it.value() = -1; // mark as launched
        }
    }

    for (const QString& node_id : ready)
        launch_single_node(node_id);
}

void WorkflowExecutor::launch_single_node(const QString& node_id) {
    auto nd_it = node_map_.constFind(node_id);
    if (nd_it == node_map_.constEnd()) {
        // Unknown node — skip
        completed_count_++;
        if (completed_count_ == total_count_)
            finish_execution();
        return;
    }

    const NodeDef& nd = nd_it.value();
    const auto* type_def = type_cache_.value(node_id, nullptr);

    if (!type_def || !type_def->execute) {
        // No executor — pass through with warning
        LOG_WARN("Executor", QString("No executor for node type: %1").arg(nd.type));
        NodeExecutionResult nr;
        nr.node_id = node_id;
        nr.success = true;
        nr.output = QJsonObject{{"pass_through", true}};
        results_.insert(node_id, nr);
        emit node_completed(node_id, nr);
        completed_count_++;

        // Propagate readiness to downstream nodes
        for (const auto& downstream : adjacency_.value(node_id)) {
            auto deg_it = in_degree_.find(downstream);
            if (deg_it != in_degree_.end() && deg_it.value() > 0)
                deg_it.value()--;
        }

        if (completed_count_ == total_count_)
            finish_execution();
        else
            QMetaObject::invokeMethod(this, &WorkflowExecutor::launch_ready_nodes, Qt::QueuedConnection);
        return;
    }

    ExecutionHooks::instance().emit_node_start(workflow_.id, node_id, nd.type);
    emit node_started(node_id);

    QVector<QJsonValue> inputs = collect_inputs(node_id);

    // Skip nodes whose upstream branching filtered out all inputs.
    if (inputs.isEmpty() && !incoming_edges_.value(node_id).isEmpty()) {
        bool has_upstream_results = false;
        for (const auto& edge : incoming_edges_.value(node_id)) {
            if (results_.contains(edge.source_node)) {
                has_upstream_results = true;
                break;
            }
        }
        if (has_upstream_results) {
            // Upstream nodes ran but outputs were filtered by port routing — skip.
            NodeExecutionResult nr;
            nr.node_id = node_id;
            nr.success = true;
            nr.output = QJsonObject{{"_skipped", true}};
            results_.insert(node_id, nr);
            emit node_completed(node_id, nr);
            completed_count_++;

            for (const auto& downstream : adjacency_.value(node_id)) {
                auto deg_it = in_degree_.find(downstream);
                if (deg_it != in_degree_.end() && deg_it.value() > 0)
                    deg_it.value()--;
            }

            if (completed_count_ == total_count_)
                finish_execution();
            else
                QMetaObject::invokeMethod(this, &WorkflowExecutor::launch_ready_nodes, Qt::QueuedConnection);
            return;
        }
    }

    pending_count_++;
    qint64 node_start = QDateTime::currentMSecsSinceEpoch();

    // Execute asynchronously with QPointer guard (P8)
    QPointer<WorkflowExecutor> self = this;
    type_def->execute(
        nd.parameters, inputs, [self, node_id, node_start](bool success, QJsonValue output, QString error) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, node_id, success, output = std::move(output), error = std::move(error), node_start]() {
                    if (!self)
                        return;
                    int duration = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - node_start);
                    self->on_node_done(node_id, success, output, error, duration);
                },
                Qt::QueuedConnection);
        });
}

void WorkflowExecutor::on_node_done(const QString& node_id, bool success, const QJsonValue& output,
                                    const QString& error, int duration_ms) {
    pending_count_--;

    NodeExecutionResult nr;
    nr.node_id = node_id;
    nr.success = success;
    nr.output = output;
    nr.error = error;
    nr.duration_ms = duration_ms;

    results_.insert(node_id, nr);

    if (success) {
        ExecutionHooks::instance().emit_node_end(workflow_.id, node_id, true, nr.duration_ms);
        AuditLogger::instance().log(AuditAction::NodeExecuted, workflow_.id, node_id);
    } else {
        ExecutionHooks::instance().emit_node_error(workflow_.id, node_id, error);
        AuditLogger::instance().log(AuditAction::NodeFailed, workflow_.id, node_id, {}, error);
    }

    emit node_completed(node_id, nr);
    completed_count_++;

    if (!success) {
        auto nd_it = node_map_.constFind(node_id);
        bool continue_on_fail = nd_it != node_map_.constEnd() && nd_it->continue_on_fail;

        if (!continue_on_fail) {
            LOG_ERROR("Executor", QString("Node %1 failed: %2 — aborting").arg(node_id, error));
            stop_requested_ = true;
            if (pending_count_ == 0)
                finish_execution();
            return;
        } else {
            LOG_WARN("Executor", QString("Node %1 failed but continue_on_fail is set: %2").arg(node_id, error));
        }
    }

    // Propagate readiness to downstream nodes
    for (const auto& downstream : adjacency_.value(node_id)) {
        auto deg_it = in_degree_.find(downstream);
        if (deg_it != in_degree_.end() && deg_it.value() > 0)
            deg_it.value()--;
    }

    if (completed_count_ == total_count_) {
        finish_execution();
    } else if (!stop_requested_) {
        launch_ready_nodes();
    } else if (pending_count_ == 0) {
        finish_execution();
    }
}

void WorkflowExecutor::finish_execution() {
    running_ = false;

    WorkflowExecutionResult wr;
    wr.workflow_id = workflow_.id;
    wr.total_duration_ms = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - start_time_ms_);

    wr.success = true;
    wr.node_results.reserve(results_.size());
    for (auto it = results_.constBegin(); it != results_.constEnd(); ++it) {
        wr.node_results.append(it.value());
        if (!it->success)
            wr.success = false;
    }

    if (stop_requested_) {
        wr.success = false;
        if (wr.error.isEmpty())
            wr.error = "Execution stopped";
    }

    LOG_INFO("Executor",
             QString("Execution %1 in %2ms").arg(wr.success ? "completed" : "failed").arg(wr.total_duration_ms));

    ExecutionHooks::instance().emit_workflow_end(workflow_.id, wr.success, wr.total_duration_ms);
    AuditLogger::instance().log(
        wr.success ? AuditAction::WorkflowCompleted : AuditAction::WorkflowFailed, workflow_.id, {}, {},
        QString("%1 in %2ms").arg(wr.success ? "Completed" : "Failed").arg(wr.total_duration_ms));

    emit execution_finished(wr);
}

QVector<QJsonValue> WorkflowExecutor::collect_inputs(const QString& node_id) const {
    QVector<QJsonValue> inputs;

    for (const auto& edge : incoming_edges_.value(node_id)) {
        auto it = results_.constFind(edge.source_node);
        if (it == results_.constEnd())
            continue;

        const QJsonValue& output = it->output;

        // Port-based routing: if upstream output has _branch or _route annotation,
        // only flow data through the matching source_port.
        if (output.isObject()) {
            QJsonObject obj = output.toObject();

            // If/Else branching: _branch = "true" or "false"
            if (obj.contains("_branch")) {
                QString branch = obj.value("_branch").toString();
                bool is_true_port = (edge.source_port == "output_true" || edge.source_port == "output_pass" ||
                                     edge.source_port == "output_open");
                bool is_false_port = (edge.source_port == "output_false" || edge.source_port == "output_fail" ||
                                      edge.source_port == "output_closed");
                if (is_true_port && branch != "true")
                    continue;
                if (is_false_port && branch != "false")
                    continue;
            }

            // Switch routing: _route = 0, 1, 2
            if (obj.contains("_route")) {
                int route = obj.value("_route").toInt(-1);
                if (edge.source_port == "output_0" && route != 0)
                    continue;
                if (edge.source_port == "output_1" && route != 1)
                    continue;
                if (edge.source_port == "output_2" && route != 2)
                    continue;
            }
        }

        inputs.append(output);
    }

    return inputs;
}

} // namespace fincept::workflow
