#include "services/workflow/WorkflowExecutor.h"

#include "core/logging/Logger.h"
#include "services/workflow/AuditLogger.h"
#include "services/workflow/ExecutionHooks.h"
#include "services/workflow/ExpressionEngine.h"
#include "services/workflow/NodeRegistry.h"

#include <QDateTime>
#include <QJsonArray>
#include <QMetaObject>
#include <QSet>

#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>

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
    first_failure_.clear();
    abort_reason_.clear();
    in_flight_.clear();
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
        finish_early(false, "Workflow contains a cycle");
        return;
    }

    QVector<QString> order = topological_sort();

    if (order.isEmpty()) {
        LOG_WARN("Executor", "No executable nodes in workflow");
        finish_early(true, {});
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
    first_failure_.clear();
    abort_reason_.clear();
    in_flight_.clear();
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
        LOG_ERROR("Executor", "Workflow contains a cycle — aborting");
        finish_early(false, "Workflow contains a cycle");
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
        finish_early(true, {});
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
    AuditLogger::instance().log(AuditAction::WorkflowStarted, workflow_.id, {}, {},
                                QString("Started from node %1: %2 (%3 nodes)")
                                    .arg(start_node_id, workflow_.name)
                                    .arg(total_count_));
    emit execution_started(workflow_.id);

    LOG_INFO("Executor", QString("Partial execution from %1: %2 nodes").arg(start_node_id).arg(total_count_));

    launch_ready_nodes();
}

void WorkflowExecutor::stop() {
    if (!running_)
        return;

    // Second stop while nodes are still in flight: one of them is not reporting
    // back (slow agent, hung request, lost callback). Waiting for it again would
    // leave the editor stuck in "running" forever, so abandon whatever is left.
    if (stop_requested_ && pending_count_ > 0) {
        LOG_WARN("Executor", QString("Stop requested again with %1 node(s) still in flight — abandoning them")
                                 .arg(pending_count_));
        finish_execution();
        return;
    }

    stop_requested_ = true;
    LOG_INFO("Executor", "Stop requested");

    // If no nodes are in-flight, finish immediately.
    // Otherwise, in-flight nodes will complete and on_node_done will
    // see stop_requested_ and trigger finish_execution when pending reaches 0.
    if (pending_count_ == 0)
        finish_execution();
}

void WorkflowExecutor::finish_early(bool success, const QString& error) {
    running_ = false;
    start_time_ms_ = QDateTime::currentMSecsSinceEpoch();
    // Open the UI's results panel first — it only shows itself on execution_started,
    // so without this a cycle error would be visible in the log only.
    emit execution_started(workflow_.id);

    WorkflowExecutionResult wr;
    wr.workflow_id = workflow_.id;
    wr.success = success;
    wr.error = error;
    emit execution_finished(wr);
}

QString WorkflowExecutor::node_label(const QString& node_id) const {
    auto it = node_map_.constFind(node_id);
    if (it == node_map_.constEnd())
        return node_id;
    return it->name.isEmpty() ? it->type : it->name;
}

bool WorkflowExecutor::feeds_error_handler(const QString& node_id) const {
    for (const auto& child : adjacency_.value(node_id)) {
        auto it = node_map_.constFind(child);
        if (it != node_map_.constEnd() && it->type == QLatin1String("control.error_handler"))
            return true;
    }
    return false;
}

// ── Graph construction ─────────────────────────────────────────────────

void WorkflowExecutor::build_graph() {
    const int n = workflow_.nodes.size();
    adjacency_.clear();
    adjacency_.reserve(n);
    incoming_edges_.clear();
    incoming_edges_.reserve(n);
    skipped_roots_.clear();

    // Initialize all nodes
    for (const auto& nd : workflow_.nodes) {
        adjacency_[nd.id];
        incoming_edges_[nd.id];
    }

    // Drop dangling edges. An edge whose endpoint is not a node of this workflow
    // (stale import, node type removed, partial sub-workflow) used to make the
    // phantom node part of the topological order; it was then "launched" as an
    // unknown node, never released its consumers, and the run hung forever.
    QVector<EdgeDef> edges;
    edges.reserve(workflow_.edges.size());
    for (const auto& ed : workflow_.edges) {
        if (!node_map_.contains(ed.source_node) || !node_map_.contains(ed.target_node)) {
            LOG_WARN("Executor", QString("Ignoring edge %1: endpoint is not a node of this workflow").arg(ed.id));
            continue;
        }
        edges.append(ed);
    }

    // Bridge out disabled nodes. A disabled node does not run, but it must not
    // sever the graph either: its consumers should receive what the disabled
    // node would have received (pass-through), and must wait for the disabled
    // node's producers. Previously the consumers lost the edge entirely, started
    // immediately with no input and ran ahead of the nodes they depend on.
    for (const auto& nd : workflow_.nodes) {
        if (!nd.disabled)
            continue;
        QVector<EdgeDef> in_edges;
        QVector<EdgeDef> out_edges;
        QVector<EdgeDef> rest;
        rest.reserve(edges.size());
        for (const auto& ed : edges) {
            if (ed.source_node == nd.id && ed.target_node == nd.id)
                continue; // self-loop on a disabled node — drop
            if (ed.target_node == nd.id)
                in_edges.append(ed);
            else if (ed.source_node == nd.id)
                out_edges.append(ed);
            else
                rest.append(ed);
        }
        if (in_edges.isEmpty() && !out_edges.isEmpty()) {
            // A disabled node with nothing feeding it has no data to pass through.
            // Its consumers must not start from nothing (e.g. a disabled trigger in
            // front of an order node), so keep it in the graph as a node that
            // completes as "skipped" — that cascades to everything it feeds.
            skipped_roots_.insert(nd.id);
            continue;
        }
        for (const auto& in : in_edges) {
            for (const auto& out : out_edges) {
                EdgeDef bridged;
                bridged.id = QString("bridge:%1>%2").arg(in.id, out.id);
                bridged.source_node = in.source_node;
                bridged.source_port = in.source_port;
                bridged.target_node = out.target_node;
                bridged.target_port = out.target_port;
                rest.append(bridged);
            }
        }
        edges = std::move(rest);
    }

    for (const auto& ed : edges) {
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

        // A disabled node does not execute, but we MUST still relax its outgoing
        // edges. The old code `continue`d before the edge-relaxation loop, so every
        // downstream node kept a permanently non-zero in-degree and was silently
        // stranded — never added to `order`, excluded from total_count_, yet
        // execution still reported success. (build_graph() already bridged
        // disabled nodes out of the edge set; the only disabled nodes that can
        // still have edges are input-less ones in skipped_roots_, which stay in
        // the order and complete as "skipped".)
        auto nd_it = node_map_.constFind(u);
        const bool disabled =
            (nd_it != node_map_.constEnd() && nd_it->disabled && !skipped_roots_.contains(u));
        if (!disabled)
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
    if (!running_)
        return; // a queued launch can arrive after the run was finished (forced stop)

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

void WorkflowExecutor::settle_without_running(const QString& node_id, const QJsonObject& output) {
    NodeExecutionResult nr;
    nr.node_id = node_id;
    nr.success = true;
    nr.output = output;
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
}

void WorkflowExecutor::launch_single_node(const QString& node_id) {
    auto nd_it = node_map_.constFind(node_id);
    if (nd_it == node_map_.constEnd()) {
        // Not reachable now that build_graph() drops dangling edges, but never
        // leave the run hanging on a phantom node: count it and release its consumers.
        settle_without_running(node_id, QJsonObject{{"_skipped", true}});
        return;
    }

    const NodeDef& nd = nd_it.value();
    const auto* type_def = type_cache_.value(node_id, nullptr);

    // A disabled node with nothing feeding it never runs, and neither may what it feeds.
    if (skipped_roots_.contains(node_id)) {
        settle_without_running(node_id, QJsonObject{{"_skipped", true}});
        return;
    }

    QVector<QJsonValue> inputs = collect_inputs(node_id);

    // A sub-workflow's entry nodes receive the parent node's payload (the
    // control.execute_workflow bridge stores it in static_data["parent_input"]).
    if (incoming_edges_.value(node_id).isEmpty() && workflow_.static_data.contains("parent_input"))
        inputs.append(workflow_.static_data.value("parent_input"));

    // Skip nodes whose upstream branching filtered out all inputs — or whose
    // upstream was itself skipped / failed-and-continued. Skipping must cascade:
    // a node behind a gate that did not pass (risk check, trading hours, loss
    // limit...) must not run just because the gate node it hangs off was skipped.
    if (inputs.isEmpty() && !incoming_edges_.value(node_id).isEmpty()) {
        bool has_upstream_results = false;
        for (const auto& edge : incoming_edges_.value(node_id)) {
            if (results_.contains(edge.source_node)) {
                has_upstream_results = true;
                break;
            }
        }
        if (has_upstream_results) {
            settle_without_running(node_id, QJsonObject{{"_skipped", true}});
            return;
        }
    }

    if (!type_def) {
        // Unregistered node type (stale save, removed node). The old behaviour was a
        // silent green "pass_through" success, which made a node that never ran look
        // like it had — fail it so the user sees which node is the problem.
        LOG_ERROR("Executor", QString("Unknown node type: %1").arg(nd.type));
        ExecutionHooks::instance().emit_node_start(workflow_.id, node_id, nd.type);
        emit node_started(node_id);
        pending_count_++;
        in_flight_.insert(node_id);
        on_node_done(node_id, false, {}, QString("Unknown node type '%1'").arg(nd.type));
        return;
    }

    if (!type_def->execute) {
        // Known type without an executor — pass through with warning
        LOG_WARN("Executor", QString("No executor for node type: %1").arg(nd.type));
        settle_without_running(node_id, QJsonObject{{"pass_through", true}});
        return;
    }

    ExecutionHooks::instance().emit_node_start(workflow_.id, node_id, nd.type);
    emit node_started(node_id);

    pending_count_++;
    in_flight_.insert(node_id);
    qint64 node_start = QDateTime::currentMSecsSinceEpoch();

    // A node must report exactly once. A second callback (retry path, signal fired
    // twice) would decrement pending_count_ again and corrupt the run, and the
    // callback may arrive from a worker thread — hence the atomic.
    auto reported = std::make_shared<std::atomic<bool>>(false);

    // Execute asynchronously with QPointer guard (P8)
    QPointer<WorkflowExecutor> self = this;
    const QJsonObject node_params = resolved_parameters(nd, *type_def, inputs);
    try {
        type_def->execute(node_params, inputs,
                          [self, node_id, node_start, reported](bool success, QJsonValue output, QString error) {
                              if (reported->exchange(true))
                                  return;
                              if (!self)
                                  return;
                              QMetaObject::invokeMethod(
                                  self,
                                  [self, node_id, success, output = std::move(output), error = std::move(error),
                                   node_start]() {
                                      if (!self)
                                          return;
                                      int duration = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - node_start);
                                      self->on_node_done(node_id, success, output, error, duration);
                                  },
                                  Qt::QueuedConnection);
                          });
    } catch (const std::exception& ex) {
        LOG_ERROR("Executor", QString("Node %1 threw exception: %2").arg(node_id, ex.what()));
        if (!reported->exchange(true))
            on_node_done(node_id, false, {}, QString("Exception: %1").arg(ex.what()));
    } catch (...) {
        LOG_ERROR("Executor", QString("Node %1 threw unknown exception").arg(node_id));
        if (!reported->exchange(true))
            on_node_done(node_id, false, {}, "Unknown exception in node executor");
    }
}

void WorkflowExecutor::on_node_done(const QString& node_id, bool success, const QJsonValue& output,
                                    const QString& error, int duration_ms) {
    if (!running_) {
        // The run was already finished (forced stop) and this node was abandoned.
        // Keep the audit trail honest — an order node may well have gone through.
        LOG_WARN("Executor", QString("Node %1 reported %2 after the run ended")
                                 .arg(node_id, success ? QStringLiteral("success") : "failure: " + error));
        AuditLogger::instance().log(success ? AuditAction::NodeExecuted : AuditAction::NodeFailed, workflow_.id,
                                    node_id, {}, QString("Finished after the run was stopped. %1").arg(error));
        return;
    }
    if (!in_flight_.remove(node_id))
        return; // already settled (defensive — callers guarantee a single report)
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
        const QString failure = QString("Node '%1' failed: %2")
                                    .arg(node_label(node_id), error.isEmpty() ? QStringLiteral("unknown error") : error);
        if (first_failure_.isEmpty())
            first_failure_ = failure;

        auto nd_it = node_map_.constFind(node_id);
        // A node wired into an Error Handler is "caught": the handler is the
        // user's explicit instruction for what to do on failure.
        const bool continue_on_fail =
            (nd_it != node_map_.constEnd() && nd_it->continue_on_fail) || feeds_error_handler(node_id);

        if (!continue_on_fail) {
            LOG_ERROR("Executor", QString("%1 — aborting").arg(failure));
            if (!stop_requested_)
                abort_reason_ = failure; // a node (not the user) ended this run
            stop_requested_ = true;
            if (pending_count_ == 0)
                finish_execution();
            return;
        } else {
            LOG_WARN("Executor", QString("%1 (continuing)").arg(failure));
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
    if (!running_)
        return; // already finished (e.g. forced stop followed by a queued launch)

    // Anything still in flight at this point was abandoned by a forced stop. Give
    // each a terminal result so the editor does not leave it spinning as "running".
    for (const QString& id : in_flight_) {
        NodeExecutionResult nr;
        nr.node_id = id;
        nr.success = false;
        nr.error = QStringLiteral("Abandoned — the run was stopped before this node reported back");
        results_.insert(id, nr);
        emit node_completed(id, nr);
    }
    in_flight_.clear();
    pending_count_ = 0;

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
        // "Execution stopped" is reserved for a user stop (the results panel keys
        // off it); a node that aborted the run reports which node and why.
        wr.error = abort_reason_.isEmpty() ? QStringLiteral("Execution stopped") : abort_reason_;
    } else if (!wr.success) {
        wr.error = first_failure_;
    }

    LOG_INFO("Executor",
             QString("Execution %1 in %2ms").arg(wr.success ? "completed" : "failed").arg(wr.total_duration_ms));

    ExecutionHooks::instance().emit_workflow_end(workflow_.id, wr.success, wr.total_duration_ms);
    AuditLogger::instance().log(
        wr.success ? AuditAction::WorkflowCompleted : AuditAction::WorkflowFailed, workflow_.id, {}, {},
        wr.success ? QString("Completed in %1ms").arg(wr.total_duration_ms)
                   : QString("Failed in %1ms: %2").arg(wr.total_duration_ms).arg(wr.error));

    emit execution_finished(wr);
}

QJsonObject WorkflowExecutor::resolved_parameters(const NodeDef& node, const NodeTypeDef& type,
                                                  const QVector<QJsonValue>& inputs) const {
    // Fast path: nothing to resolve.
    bool has_template = false;
    for (auto it = node.parameters.constBegin(); it != node.parameters.constEnd(); ++it) {
        if (it.value().isString() && it.value().toString().contains(QLatin1String("{{"))) {
            has_template = true;
            break;
        }
    }
    if (!has_template)
        return node.parameters;

    // Order / risk nodes keep exactly the literal values the user typed: what is sent to a
    // broker must never depend on upstream data through a text template.
    if (node.type.startsWith(QLatin1String("trading.")) || node.type.startsWith(QLatin1String("safety.")))
        return node.parameters;

    // Expression context = the merged upstream objects (later inputs win on a key clash); a
    // non-object upstream value is exposed as `value`.
    QJsonObject context;
    for (const auto& in : inputs) {
        if (in.isObject()) {
            const QJsonObject o = in.toObject();
            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                context.insert(it.key(), it.value());
        }
    }
    if (!inputs.isEmpty() && !inputs[0].isObject() && !inputs[0].isNull() && !inputs[0].isUndefined())
        context.insert(QStringLiteral("value"), inputs[0]);

    // Only plain-text parameters are templated. "expression" (If/Else, Assert, Map), "code"
    // (Code, Template, SQL) and "json" parameters interpret their own text.
    QJsonObject out = node.parameters;
    for (const auto& pd : type.parameters) {
        if (pd.type != QLatin1String("string"))
            continue;
        const QJsonValue raw = out.value(pd.key);
        if (!raw.isString() || !raw.toString().contains(QLatin1String("{{")))
            continue;
        const QJsonValue resolved = ExpressionEngine::evaluate(raw, context);
        // A text parameter must stay text: ={{$input.price}} would otherwise hand the node a
        // number, which the executors' toString() reads as an empty string.
        out[pd.key] = resolved.isString() ? resolved : QJsonValue(ExpressionEngine::value_to_string(resolved));
    }
    return out;
}

QVector<QJsonValue> WorkflowExecutor::collect_inputs(const QString& node_id) const {
    QVector<QJsonValue> inputs;

    auto self_it = node_map_.constFind(node_id);
    const bool is_error_handler =
        self_it != node_map_.constEnd() && self_it->type == QLatin1String("control.error_handler");

    for (const auto& edge : incoming_edges_.value(node_id)) {
        auto it = results_.constFind(edge.source_node);
        if (it == results_.constEnd())
            continue;

        // An upstream node that failed (and was allowed to continue) produced no
        // data. Only an Error Handler consumes it — as an error record — every other
        // node treats it as "no input" so it cannot run on a null payload.
        if (!it->success) {
            if (is_error_handler) {
                QJsonObject err;
                err["_error"] = true;
                err["error"] = it->error;
                err["node_id"] = edge.source_node;
                err["node_name"] = node_label(edge.source_node);
                inputs.append(err);
            }
            continue;
        }

        const QJsonValue& output = it->output;

        // Port-based routing: if upstream output has _branch or _route annotation,
        // only flow data through the matching source_port.
        if (output.isObject()) {
            QJsonObject obj = output.toObject();

            // The upstream node never ran (its branch was not taken, a gate did not
            // pass, or it is disabled): it has no data to hand on.
            if (obj.value("_skipped").toBool(false))
                continue;

            // If/Else branching: _branch = "true" or "false"
            if (obj.contains("_branch")) {
                QString branch = obj.value("_branch").toString();
                bool is_true_port = (edge.source_port == "output_true" || edge.source_port == "output_pass" ||
                                     edge.source_port == "output_open" || edge.source_port == "output_success");
                bool is_false_port = (edge.source_port == "output_false" || edge.source_port == "output_fail" ||
                                      edge.source_port == "output_closed" || edge.source_port == "output_error");
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

            // Loop routing: a "_loop" node (control.loop) splits its two ports —
            // "output_item" receives the items array, "output_done" the summary.
            if (obj.contains("_loop")) {
                if (edge.source_port == "output_item") {
                    inputs.append(obj.value("items"));
                    continue;
                }
                if (edge.source_port == "output_done") {
                    inputs.append(obj.value("done"));
                    continue;
                }
            }

            // Multi-output data routing: a node with several data ports (e.g.
            // Compare Datasets: added / removed / changed) packs one value per port
            // under "_ports"; each edge receives only its own port's value.
            if (obj.value("_ports").isObject()) {
                const QJsonObject ports = obj.value("_ports").toObject();
                if (ports.contains(edge.source_port)) {
                    inputs.append(ports.value(edge.source_port));
                    continue;
                }
            }
        }

        inputs.append(output);
    }

    return inputs;
}

} // namespace fincept::workflow
