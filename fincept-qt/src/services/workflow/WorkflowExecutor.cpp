#include "services/workflow/WorkflowExecutor.h"

#include "core/logging/Logger.h"
#include "services/workflow/AuditLogger.h"
#include "services/workflow/ExecutionHooks.h"
#include "services/workflow/NodeRegistry.h"

#include <QDateTime>
#include <QElapsedTimer>
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
    current_index_ = 0;
    results_.clear();
    node_map_.clear();

    // Index nodes by id
    for (const auto& nd : workflow_.nodes)
        node_map_.insert(nd.id, nd);

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

    execution_order_ = topological_sort();

    if (execution_order_.isEmpty()) {
        LOG_WARN("Executor", "No executable nodes in workflow");
        running_ = false;
        WorkflowExecutionResult wr;
        wr.workflow_id = workflow_.id;
        wr.success = true;
        emit execution_finished(wr);
        return;
    }

    start_time_ms_ = QDateTime::currentMSecsSinceEpoch();
    ExecutionHooks::instance().emit_workflow_start(workflow_.id);
    AuditLogger::instance().log(AuditAction::WorkflowStarted, workflow_.id, {}, {},
                                QString("Started: %1 (%2 nodes)").arg(workflow_.name).arg(execution_order_.size()));
    emit execution_started(workflow_.id);

    LOG_INFO("Executor", QString("Starting execution: %1 (%2 nodes)").arg(workflow_.name).arg(execution_order_.size()));

    execute_next();
}

void WorkflowExecutor::execute_from(const WorkflowDef& workflow, const QString& start_node_id) {
    if (running_) {
        LOG_WARN("Executor", "Execution already in progress");
        return;
    }

    workflow_ = workflow;
    running_ = true;
    stop_requested_ = false;
    current_index_ = 0;
    results_.clear();
    node_map_.clear();

    for (const auto& nd : workflow_.nodes)
        node_map_.insert(nd.id, nd);

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
    execution_order_.clear();
    for (const auto& id : full_order) {
        if (downstream.contains(id))
            execution_order_.append(id);
    }

    if (execution_order_.isEmpty()) {
        running_ = false;
        WorkflowExecutionResult wr;
        wr.workflow_id = workflow_.id;
        wr.success = true;
        emit execution_finished(wr);
        return;
    }

    start_time_ms_ = QDateTime::currentMSecsSinceEpoch();
    ExecutionHooks::instance().emit_workflow_start(workflow_.id);
    emit execution_started(workflow_.id);

    LOG_INFO("Executor",
             QString("Partial execution from %1: %2 nodes").arg(start_node_id).arg(execution_order_.size()));

    execute_next();
}

void WorkflowExecutor::stop() {
    if (!running_)
        return;
    stop_requested_ = true;
    LOG_INFO("Executor", "Stop requested");
    // Force immediate completion — don't wait for the in-flight node to finish.
    // Set running_ false and emit execution_finished right now so the UI unlocks.
    running_ = false;
    WorkflowExecutionResult wr;
    wr.workflow_id = workflow_.id;
    wr.total_duration_ms = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - start_time_ms_);
    wr.success = false;
    wr.error = "Execution stopped by user";
    for (auto it = results_.constBegin(); it != results_.constEnd(); ++it)
        wr.node_results.append(it.value());
    ExecutionHooks::instance().emit_workflow_end(workflow_.id, false, wr.total_duration_ms);
    AuditLogger::instance().log(AuditAction::WorkflowFailed, workflow_.id, {}, {}, "Stopped by user");
    emit execution_finished(wr);
}

// ── Graph construction ─────────────────────────────────────────────────

void WorkflowExecutor::build_graph() {
    adjacency_.clear();
    reverse_adj_.clear();

    // Initialize all nodes
    for (const auto& nd : workflow_.nodes) {
        adjacency_[nd.id];
        reverse_adj_[nd.id];
    }

    for (const auto& ed : workflow_.edges) {
        adjacency_[ed.source_node].append(ed.target_node);
        reverse_adj_[ed.target_node].append(ed.source_node);
    }
}

// ── Cycle detection (DFS with coloring) ────────────────────────────────

bool WorkflowExecutor::has_cycle() const {
    enum Color { White, Gray, Black };
    QMap<QString, Color> color;
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
    QMap<QString, int> in_degree;
    for (auto it = adjacency_.constBegin(); it != adjacency_.constEnd(); ++it)
        in_degree[it.key()] = 0;

    for (auto it = adjacency_.constBegin(); it != adjacency_.constEnd(); ++it) {
        for (const auto& v : it.value())
            in_degree[v]++;
    }

    QVector<QString> queue;
    for (auto it = in_degree.constBegin(); it != in_degree.constEnd(); ++it) {
        if (it.value() == 0)
            queue.append(it.key());
    }

    QVector<QString> order;
    int front = 0;
    while (front < queue.size()) {
        QString u = queue[front++];

        // Skip disabled nodes
        auto nd_it = node_map_.constFind(u);
        if (nd_it != node_map_.constEnd() && nd_it->disabled)
            continue;

        order.append(u);

        for (const auto& v : adjacency_.value(u)) {
            in_degree[v]--;
            if (in_degree[v] == 0)
                queue.append(v);
        }
    }

    return order;
}

// ── Execution loop ─────────────────────────────────────────────────────

void WorkflowExecutor::execute_next() {
    if (stop_requested_ || current_index_ >= execution_order_.size()) {
        // Execution complete
        running_ = false;

        WorkflowExecutionResult wr;
        wr.workflow_id = workflow_.id;
        wr.total_duration_ms = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - start_time_ms_);

        // Check if any node failed
        wr.success = true;
        for (auto it = results_.constBegin(); it != results_.constEnd(); ++it) {
            wr.node_results.append(it.value());
            if (!it->success)
                wr.success = false;
        }

        if (stop_requested_) {
            wr.success = false;
            wr.error = "Execution stopped by user";
        }

        LOG_INFO("Executor",
                 QString("Execution %1 in %2ms").arg(wr.success ? "completed" : "failed").arg(wr.total_duration_ms));

        ExecutionHooks::instance().emit_workflow_end(workflow_.id, wr.success, wr.total_duration_ms);
        AuditLogger::instance().log(
            wr.success ? AuditAction::WorkflowCompleted : AuditAction::WorkflowFailed, workflow_.id, {}, {},
            QString("%1 in %2ms").arg(wr.success ? "Completed" : "Failed").arg(wr.total_duration_ms));

        emit execution_finished(wr);
        return;
    }

    QString node_id = execution_order_[current_index_];
    auto nd_it = node_map_.constFind(node_id);
    if (nd_it == node_map_.constEnd()) {
        current_index_++;
        execute_next();
        return;
    }

    const NodeDef& nd = nd_it.value();
    const auto* type_def = NodeRegistry::instance().find(nd.type);

    if (!type_def || !type_def->execute) {
        // No executor — pass through with warning
        LOG_WARN("Executor", QString("No executor for node type: %1").arg(nd.type));
        NodeExecutionResult nr;
        nr.node_id = node_id;
        nr.success = true;
        nr.output = QJsonObject{{"pass_through", true}};
        results_.insert(node_id, nr);
        emit node_completed(node_id, nr);
        current_index_++;
        // Use queued invocation to avoid deep recursion
        QMetaObject::invokeMethod(this, &WorkflowExecutor::execute_next, Qt::QueuedConnection);
        return;
    }

    ExecutionHooks::instance().emit_node_start(workflow_.id, node_id, nd.type);
    emit node_started(node_id);

    QVector<QJsonValue> inputs = collect_inputs(node_id);
    qint64 node_start = QDateTime::currentMSecsSinceEpoch();

    // Execute asynchronously with QPointer guard (P8)
    QPointer<WorkflowExecutor> self = this;
    type_def->execute(nd.parameters, inputs,
                      [self, node_id, node_start](bool success, QJsonValue output, QString error) {
                          if (!self)
                              return;
                          QMetaObject::invokeMethod(
                              self,
                              [self, node_id, success, output, error, node_start]() {
                                  if (!self)
                                      return;
                                  NodeExecutionResult nr;
                                  nr.node_id = node_id;
                                  nr.success = success;
                                  nr.output = output;
                                  nr.error = error;
                                  nr.duration_ms = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - node_start);
                                  self->on_node_done(node_id, success, output, error);
                              },
                              Qt::QueuedConnection);
                      });
}

void WorkflowExecutor::on_node_done(const QString& node_id, bool success, QJsonValue output, QString error) {
    NodeExecutionResult nr;
    nr.node_id = node_id;
    nr.success = success;
    nr.output = output;
    nr.error = error;

    results_.insert(node_id, nr);

    if (success) {
        ExecutionHooks::instance().emit_node_end(workflow_.id, node_id, true, nr.duration_ms);
        AuditLogger::instance().log(AuditAction::NodeExecuted, workflow_.id, node_id);
    } else {
        ExecutionHooks::instance().emit_node_error(workflow_.id, node_id, error);
        AuditLogger::instance().log(AuditAction::NodeFailed, workflow_.id, node_id, {}, error);
    }

    emit node_completed(node_id, nr);

    if (!success) {
        auto nd_it = node_map_.constFind(node_id);
        bool continue_on_fail = nd_it != node_map_.constEnd() && nd_it->continue_on_fail;

        if (!continue_on_fail) {
            LOG_ERROR("Executor", QString("Node %1 failed: %2 — aborting").arg(node_id, error));
            stop_requested_ = true;
        } else {
            LOG_WARN("Executor", QString("Node %1 failed but continue_on_fail is set: %2").arg(node_id, error));
        }
    }

    current_index_++;
    execute_next();
}

QVector<QJsonValue> WorkflowExecutor::collect_inputs(const QString& node_id) const {
    QVector<QJsonValue> inputs;
    for (const auto& upstream_id : reverse_adj_.value(node_id)) {
        auto it = results_.constFind(upstream_id);
        if (it != results_.constEnd())
            inputs.append(it->output);
    }
    return inputs;
}

} // namespace fincept::workflow
