#include "services/workflow/WorkflowExecutor.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/workflow/AuditLogger.h"
#include "services/workflow/ExpressionEngine.h"
#include "services/workflow/NodeRegistry.h"

#include <QDateTime>
#include <QJsonArray>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QUuid>

#include <atomic>
#include <memory>
#include <stdexcept>

namespace fincept::workflow {

namespace wf_executor_detail {

// The executor whose node is being launched right now. A WorkflowExecutor started
// *during* that call is a sub-workflow (control.execute_workflow runs its child
// synchronously inside the node's execute function): it inherits the parent's
// execution mode, step budget and cancellation. Executors live on the GUI thread
// only, so a plain pointer guarded by RAII is sufficient.
WorkflowExecutor* g_launching_parent = nullptr;

// Tokens of runs that are still going (is_run_active). Read from worker threads.
QMutex g_active_runs_mutex;
QSet<QString> g_active_runs;

struct LaunchScope {
    explicit LaunchScope(WorkflowExecutor* ex) : previous(g_launching_parent) { g_launching_parent = ex; }
    ~LaunchScope() { g_launching_parent = previous; }
    LaunchScope(const LaunchScope&) = delete;
    LaunchScope& operator=(const LaunchScope&) = delete;
    WorkflowExecutor* previous;
};

} // namespace wf_executor_detail

WorkflowExecutor::WorkflowExecutor(QObject* parent) : QObject(parent) {}

WorkflowExecutor::~WorkflowExecutor() {
    release_run_token();
}

// ── Execution-mode helpers ─────────────────────────────────────────────

bool WorkflowExecutor::is_live_mode(const WorkflowDef& wf) {
    // Exact match only: an empty, misspelled or missing value must never arm LIVE.
    return wf.static_data.value(QLatin1String(kExecutionModeKey)).toString() == QLatin1String("live");
}

bool WorkflowExecutor::is_moded_order_type(const QString& type) {
    static const QSet<QString> k = {
        QStringLiteral("trading.place_order"),   QStringLiteral("trading.cancel_order"),
        QStringLiteral("trading.modify_order"),  QStringLiteral("trading.close_position"),
        QStringLiteral("trading.bracket_order"), QStringLiteral("trading.trailing_stop"),
        QStringLiteral("trading.scale_in"),
    };
    return k.contains(type);
}

bool WorkflowExecutor::is_moded_read_type(const QString& type) {
    static const QSet<QString> k = {
        QStringLiteral("trading.get_orders"),
        QStringLiteral("trading.get_positions"),
        QStringLiteral("trading.get_holdings"),
        QStringLiteral("trading.get_balance"),
    };
    return k.contains(type);
}

bool WorkflowExecutor::is_live_only_order_type(const QString& type) {
    static const QSet<QString> k = {
        QStringLiteral("trading.smart_order"),
        QStringLiteral("trading.cancel_all"),
        QStringLiteral("trading.close_all"),
    };
    return k.contains(type);
}

bool WorkflowExecutor::node_requests_live(const QJsonObject& params) {
    return params.value(QStringLiteral("mode"))
               .toString()
               .trimmed()
               .compare(QLatin1String("live"), Qt::CaseInsensitive) == 0;
}

QString WorkflowExecutor::effective_mcp_tool(const QJsonObject& params, const QVector<QJsonValue>& inputs) {
    QString tool = params.value(QStringLiteral("tool")).toString().trimmed();
    if (tool.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
        tool = inputs[0].toObject().value(QStringLiteral("tool")).toString().trimmed();
    return tool;
}

bool WorkflowExecutor::mcp_tool_moves_money(const QString& tool) {
    if (tool.isEmpty() || tool.contains(QLatin1String("__")))
        return false; // external tools carry no category metadata
    for (const auto& t : mcp::McpService::instance().get_all_tools()) {
        if (t.is_internal && t.name == tool)
            return t.is_destructive && t.category == QLatin1String("live-trading");
    }
    return false;
}

QStringList WorkflowExecutor::find_cycle(const QHash<QString, QVector<QString>>& adjacency) {
    // Iterative three-colour DFS (a recursive one overflows the stack on long chains),
    // keeping the current path so a back edge can be turned into the list of nodes on
    // the loop.
    enum Colour { White, Grey, Black };
    QHash<QString, Colour> colour;
    colour.reserve(adjacency.size());

    struct Frame {
        QString node;
        int next_child = 0;
    };

    for (auto root = adjacency.constBegin(); root != adjacency.constEnd(); ++root) {
        if (colour.value(root.key(), White) != White)
            continue;
        QVector<Frame> stack;
        stack.append({root.key(), 0});
        colour[root.key()] = Grey;
        while (!stack.isEmpty()) {
            Frame& top = stack.last();
            const QVector<QString> children = adjacency.value(top.node);
            if (top.next_child >= children.size()) {
                colour[top.node] = Black;
                stack.removeLast();
                continue;
            }
            const QString child = children[top.next_child++];
            const Colour c = colour.value(child, White);
            if (c == Grey) {
                // Back edge: the cycle is the stack from `child` to the top.
                QStringList cycle;
                bool in_cycle = false;
                for (const Frame& f : stack) {
                    if (f.node == child)
                        in_cycle = true;
                    if (in_cycle)
                        cycle << f.node;
                }
                return cycle;
            }
            if (c == White) {
                colour[child] = Grey;
                stack.append({child, 0});
            }
        }
    }
    return {};
}

bool WorkflowExecutor::is_run_active(const QString& run_token) {
    if (run_token.isEmpty())
        return false;
    QMutexLocker lock(&wf_executor_detail::g_active_runs_mutex);
    return wf_executor_detail::g_active_runs.contains(run_token);
}

void WorkflowExecutor::release_run_token() {
    if (run_token_.isEmpty())
        return;
    QMutexLocker lock(&wf_executor_detail::g_active_runs_mutex);
    wf_executor_detail::g_active_runs.remove(run_token_);
}

// ── Run set-up ─────────────────────────────────────────────────────────

bool WorkflowExecutor::prepare_run(const WorkflowDef& workflow) {
    workflow_ = workflow;
    running_ = true;
    stop_requested_ = false;
    pending_count_ = 0;
    completed_count_ = 0;
    launches_ = 0;
    first_failure_.clear();
    abort_reason_.clear();
    hard_abort_reason_.clear();
    in_flight_.clear();
    results_.clear();
    node_map_.clear();
    type_cache_.clear();
    in_degree_.clear();
    children_.clear();

    release_run_token();
    run_token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QMutexLocker lock(&wf_executor_detail::g_active_runs_mutex);
        wf_executor_detail::g_active_runs.insert(run_token_);
    }

    // Started from inside a parent's node launch → sub-workflow: inherit the
    // parent's mode (never this workflow's own saved mode), budget and nesting.
    WorkflowExecutor* launcher = wf_executor_detail::g_launching_parent;
    if (launcher && launcher != this && launcher->running_) {
        root_ = launcher->root_ ? launcher->root_.data() : launcher;
        depth_ = launcher->depth_ + 1;
        live_allowed_ = launcher->live_allowed_;
        launcher->children_.append(QPointer<WorkflowExecutor>(this));
    } else {
        root_.clear();
        depth_ = 0;
        live_allowed_ = is_live_mode(workflow);
    }

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

    if (depth_ > kMaxSubworkflowDepth) {
        const QString err = QString("Sub-workflows are nested more than %1 levels deep — the run was refused")
                                .arg(kMaxSubworkflowDepth);
        LOG_ERROR("Executor", err);
        finish_early(false, err);
        return false;
    }

    build_graph();

    const QStringList cycle = find_cycle(adjacency_);
    if (!cycle.isEmpty()) {
        QStringList names;
        for (const auto& id : cycle)
            names << QString("'%1'").arg(node_label(id));
        names << QString("'%1'").arg(node_label(cycle.first()));
        const QString err =
            QString("Workflow contains a cycle: %1. Remove one of these connections — workflows must be acyclic.")
                .arg(names.join(QStringLiteral(" → ")));
        LOG_ERROR("Executor", err);
        finish_early(false, err);
        return false;
    }
    return true;
}

void WorkflowExecutor::start_run(const QVector<QString>& order, const QString& audit_note) {
    // Build in-degree map from the execution set
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
    AuditLogger::instance().log(AuditAction::WorkflowStarted, workflow_.id, {}, {},
                                QString("%1 [%2 mode]").arg(audit_note, live_allowed_ ? "LIVE" : "PAPER"), {},
                                !live_allowed_);
    emit execution_started(workflow_.id);

    LOG_INFO("Executor", QString("Starting execution: %1 (%2 nodes, %3 mode, depth %4)")
                             .arg(workflow_.name)
                             .arg(total_count_)
                             .arg(live_allowed_ ? "LIVE" : "PAPER")
                             .arg(depth_));

    launch_ready_nodes();
}

void WorkflowExecutor::execute(const WorkflowDef& workflow) {
    if (running_) {
        LOG_WARN("Executor", "Execution already in progress");
        return;
    }
    if (!prepare_run(workflow))
        return;

    QVector<QString> order = topological_sort();
    if (order.isEmpty()) {
        LOG_WARN("Executor", "No executable nodes in workflow");
        finish_early(true, {});
        return;
    }
    start_run(order, QString("Started: %1 (%2 nodes)").arg(workflow_.name).arg(order.size()));
}

void WorkflowExecutor::execute_from(const WorkflowDef& workflow, const QString& start_node_id) {
    if (running_) {
        LOG_WARN("Executor", "Execution already in progress");
        return;
    }
    if (!prepare_run(workflow))
        return;

    // Get full topo order, then find start node and take only downstream
    const QVector<QString> full_order = topological_sort();

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
    start_run(order, QString("Started from node %1: %2 (%3 nodes)")
                         .arg(node_label(start_node_id), workflow_.name)
                         .arg(order.size()));
}

void WorkflowExecutor::stop() {
    if (!running_)
        return;

    // Cancel every sub-workflow this run started first — otherwise a nested
    // workflow keeps placing orders after the user pressed STOP on the parent.
    const auto children = children_;
    for (const auto& child : children) {
        if (child && child->is_running())
            child->stop();
    }

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
    release_run_token(); // a scale-in in progress places no further tranche
    LOG_INFO("Executor", "Stop requested");

    // If no nodes are in-flight, finish immediately.
    // Otherwise, in-flight nodes will complete and on_node_done will
    // see stop_requested_ and trigger finish_execution when pending reaches 0.
    if (pending_count_ == 0)
        finish_execution();
}

void WorkflowExecutor::finish_early(bool success, const QString& error) {
    running_ = false;
    release_run_token();
    start_time_ms_ = QDateTime::currentMSecsSinceEpoch();
    // Open the UI's results panel first — it only shows itself on execution_started,
    // so without this a cycle error would be visible in the log only.
    emit execution_started(workflow_.id);

    WorkflowExecutionResult wr;
    wr.workflow_id = workflow_.id;
    wr.success = success;
    wr.error = error;
    if (!success)
        AuditLogger::instance().log(AuditAction::WorkflowFailed, workflow_.id, {}, {}, error);
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

WorkflowExecutor* WorkflowExecutor::budget_owner() {
    return root_ ? root_.data() : this;
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
    // node's producers.
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
        // edges, or every downstream node keeps a non-zero in-degree and is
        // silently stranded. (build_graph() already bridged disabled nodes out of
        // the edge set; the only disabled nodes that can still have edges are
        // input-less ones in skipped_roots_, which stay in the order and complete
        // as "skipped".)
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

    for (const QString& node_id : ready) {
        if (!running_ || stop_requested_)
            break; // a node failed synchronously and aborted the run
        launch_single_node(node_id);
    }
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

void WorkflowExecutor::fail_without_running(const QString& node_id, const QString& error) {
    emit node_started(node_id);
    pending_count_++;
    in_flight_.insert(node_id);
    on_node_done(node_id, false, {}, error);
}

QString WorkflowExecutor::enforce_trading_mode(const NodeDef& node, QJsonObject& params,
                                               const QVector<QJsonValue>& inputs) const {
    const QString& type = node.type;
    const QString label = node.name.isEmpty() ? type : node.name;

    if (is_moded_order_type(type)) {
        // The bridges treat any mode other than the exact string "paper" as live
        // ("Paper", "", a typo...). Rewrite it to one of the two exact values so the
        // only way to reach a broker is: run armed LIVE *and* node Mode = live.
        const bool live = live_allowed_ && node_requests_live(params);
        if (!live && node_requests_live(params))
            LOG_INFO("Executor", QString("'%1' is set to live but the run is in PAPER mode — simulating").arg(label));
        params[QStringLiteral("mode")] = live ? QStringLiteral("live") : QStringLiteral("paper");
        return {};
    }
    if (is_moded_read_type(type)) {
        // Reads move no money; keep the node's own choice but in canonical form.
        params[QStringLiteral("mode")] = node_requests_live(params) ? QStringLiteral("live") : QStringLiteral("paper");
        return {};
    }
    if (is_live_only_order_type(type)) {
        if (!live_allowed_)
            return QString("Blocked: '%1' always trades at the broker (it has no paper mode) and this workflow is "
                           "running in PAPER mode. Arm LIVE in SAVE & RUN to use it.")
                .arg(label);
        return {};
    }
    if (type == QLatin1String("mcp.tool_call")) {
        const QString tool = effective_mcp_tool(params, inputs);
        if (mcp_tool_moves_money(tool)) {
            if (!live_allowed_)
                return QString("Blocked: MCP tool '%1' places or cancels real broker orders and this workflow is "
                               "running in PAPER mode. Arm LIVE in SAVE & RUN to use it.")
                    .arg(tool);
            // A tool chosen at run time (Tool Picker / upstream data) was never shown in
            // the LIVE confirmation — it may not trade. Name it in the node's Tool param.
            if (params.value(QStringLiteral("tool")).toString().trimmed().isEmpty())
                return QString("Blocked: the live-trading MCP tool '%1' was chosen at run time, not set on the node. "
                               "Set the node's Tool parameter explicitly to allow it.")
                    .arg(tool);
        }
    }
    return {};
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

    // Step budget, shared with every sub-workflow of this run. A DAG runs each node
    // once, so only runaway nesting (A → B → C → ...) can get here — stop it hard.
    WorkflowExecutor* owner = budget_owner();
    if (++owner->launches_ > kMaxNodeLaunches) {
        hard_abort_reason_ = QString("Step limit reached: more than %1 node executions in one run (sub-workflows "
                                     "included). The run was stopped to prevent a runaway workflow.")
                                 .arg(kMaxNodeLaunches);
        LOG_ERROR("Executor", hard_abort_reason_);
        fail_without_running(node_id, hard_abort_reason_);
        return;
    }

    if (!type_def) {
        // Unregistered node type (stale save, removed node). Fail it so the user
        // sees which node is the problem instead of a silent green pass-through.
        LOG_ERROR("Executor", QString("Unknown node type: %1").arg(nd.type));
        fail_without_running(node_id, QString("Unknown node type '%1'").arg(nd.type));
        return;
    }

    if (!type_def->execute) {
        // Known type without an executor — pass through with warning
        LOG_WARN("Executor", QString("No executor for node type: %1").arg(nd.type));
        settle_without_running(node_id, QJsonObject{{"pass_through", true}});
        return;
    }

    QJsonObject node_params = resolved_parameters(nd, *type_def, inputs);
    const QString refusal = enforce_trading_mode(nd, node_params, inputs);
    if (nd.type.startsWith(QLatin1String("trading.")))
        node_params[QStringLiteral("_run_token")] = run_token_;
    if (!refusal.isEmpty()) {
        LOG_WARN("Executor", refusal);
        fail_without_running(node_id, refusal);
        return;
    }

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
    try {
        // Anything the node starts synchronously (a sub-workflow) sees us as its parent.
        wf_executor_detail::LaunchScope scope(this);
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
                                    node_id, {}, QString("Finished after the run was stopped. %1").arg(error), {},
                                    !live_allowed_);
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

    if (success)
        AuditLogger::instance().log(AuditAction::NodeExecuted, workflow_.id, node_id, {}, {}, {}, !live_allowed_);
    else
        AuditLogger::instance().log(AuditAction::NodeFailed, workflow_.id, node_id, {}, error, {}, !live_allowed_);

    emit node_completed(node_id, nr);
    completed_count_++;

    if (!success) {
        const QString failure = QString("Node '%1' failed: %2")
                                    .arg(node_label(node_id), error.isEmpty() ? QStringLiteral("unknown error") : error);
        if (first_failure_.isEmpty())
            first_failure_ = failure;

        auto nd_it = node_map_.constFind(node_id);
        // A node wired into an Error Handler is "caught": the handler is the
        // user's explicit instruction for what to do on failure. An exhausted step
        // budget is never caught.
        const bool continue_on_fail =
            hard_abort_reason_.isEmpty() &&
            ((nd_it != node_map_.constEnd() && nd_it->continue_on_fail) || feeds_error_handler(node_id));

        if (!continue_on_fail) {
            LOG_ERROR("Executor", QString("%1 — aborting").arg(failure));
            if (!stop_requested_)
                abort_reason_ = failure; // a node (not the user) ended this run
            stop_requested_ = true;
            release_run_token();
            // Nested runs started by this one must stop too.
            const auto children = children_;
            for (const auto& child : children) {
                if (child && child->is_running())
                    child->stop();
            }
            if (pending_count_ == 0)
                finish_execution();
            return;
        }
        LOG_WARN("Executor", QString("%1 (continuing)").arg(failure));
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

    // A forced stop abandons nested runs as well.
    const auto children = children_;
    for (const auto& child : children) {
        if (child && child->is_running())
            child->stop();
    }

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
    release_run_token();

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

    AuditLogger::instance().log(wr.success ? AuditAction::WorkflowCompleted : AuditAction::WorkflowFailed, workflow_.id,
                                {}, {},
                                wr.success ? QString("Completed in %1ms").arg(wr.total_duration_ms)
                                           : QString("Failed in %1ms: %2").arg(wr.total_duration_ms).arg(wr.error),
                                {}, !live_allowed_);

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
