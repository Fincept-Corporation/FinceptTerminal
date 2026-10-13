#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVector>

namespace fincept::workflow {

/// Executes a WorkflowDef as a DAG — cycle detection, topological ordering,
/// parallel per-level execution with signal-driven progress.
///
/// Trading safety: every run has an execution mode, read from
/// static_data["execution_mode"]. Only the exact value "live" arms LIVE; anything
/// else (missing, empty, a typo) is PAPER. In PAPER every order node is forced to
/// paper, nodes that can only trade for real (smart order, cancel all, close all,
/// live-trading MCP tools) are refused, and a sub-workflow started from this run
/// inherits the run's mode — its own saved mode is ignored. In LIVE an order node
/// still needs its own Mode set to "live" to reach the broker (double opt-in).
class WorkflowExecutor : public QObject {
    Q_OBJECT
  public:
    explicit WorkflowExecutor(QObject* parent = nullptr);
    ~WorkflowExecutor() override;

    /// static_data key holding the workflow-level trading mode ("paper" | "live").
    static constexpr const char* kExecutionModeKey = "execution_mode";
    /// Upper bound on node launches per top-level run, sub-workflows included.
    static constexpr int kMaxNodeLaunches = 2000;
    /// Upper bound on control.execute_workflow nesting.
    static constexpr int kMaxSubworkflowDepth = 8;

    /// True only when the workflow is explicitly armed for LIVE trading.
    static bool is_live_mode(const WorkflowDef& wf);

    /// Order nodes that carry a paper|live `mode` parameter.
    static bool is_moded_order_type(const QString& type);
    /// Read-only trading nodes that carry a paper|live `mode` parameter.
    static bool is_moded_read_type(const QString& type);
    /// Order nodes with no paper implementation — they always act on the broker.
    static bool is_live_only_order_type(const QString& type);
    /// True when the node's own Mode parameter asks for live (case/space-insensitive).
    static bool node_requests_live(const QJsonObject& params);
    /// The MCP tool an mcp.tool_call node will call: its `tool` parameter, else the
    /// `tool` field of a Tool Picker payload on its first input.
    static QString effective_mcp_tool(const QJsonObject& params, const QVector<QJsonValue>& inputs);
    /// True for an internal, destructive tool of the "live-trading" category (orders at a broker).
    static bool mcp_tool_moves_money(const QString& tool);
    /// Node ids forming one cycle of `adjacency` (first id repeated at the end is omitted);
    /// empty when the graph is acyclic.
    static QStringList find_cycle(const QHash<QString, QVector<QString>>& adjacency);

    /// Long-running trading nodes (scale-in tranches) receive the run's token in
    /// params["_run_token"] and poll this — from any thread — before each order: it
    /// turns false as soon as the run is stopped, aborted or finished.
    static bool is_run_active(const QString& run_token);

    /// Start executing a workflow. Non-blocking — progress via signals.
    void execute(const WorkflowDef& workflow);

    /// Execute starting from a specific node (partial execution).
    /// Only executes the subgraph downstream of start_node_id.
    void execute_from(const WorkflowDef& workflow, const QString& start_node_id);

    /// Stop a running execution (and every sub-workflow it started). In-flight
    /// nodes are allowed to finish; calling stop() a second time while some are
    /// still running abandons them and finishes immediately (escape hatch for a
    /// node that never reports back).
    void stop();

    bool is_running() const { return running_; }
    /// The mode this run executes in (valid once execute() has been called).
    bool live_allowed() const { return live_allowed_; }

  signals:
    void execution_started(const QString& workflow_id);
    void node_started(const QString& node_id);
    void node_completed(const QString& node_id, const NodeExecutionResult& result);
    void execution_finished(const WorkflowExecutionResult& result);

  private:
    /// Shared set-up of execute()/execute_from(): reset state, resolve the run's
    /// mode / parent, build the graph and reject cycles. Returns false when the
    /// run already finished (cycle, nesting too deep).
    bool prepare_run(const WorkflowDef& workflow);

    /// Seed the in-degree map for `order` and start launching.
    void start_run(const QVector<QString>& order, const QString& audit_note);

    /// Build adjacency list from edges. Dangling edges (endpoint is not a node of
    /// this workflow) are dropped, and disabled nodes are bridged out so their
    /// inputs flow straight through to their consumers.
    void build_graph();

    /// Finish before anything ran (cycle / nothing executable) — still emits
    /// execution_started so the UI opens its results panel and shows `error`.
    void finish_early(bool success, const QString& error);

    /// Display name for a node in error messages ("name" or its type id).
    QString node_label(const QString& node_id) const;

    /// True when `node_id` feeds a control.error_handler node (so its failure is "caught").
    bool feeds_error_handler(const QString& node_id) const;

    /// Compute topological ordering.
    QVector<QString> topological_sort() const;

    /// Launch all nodes whose in-degree has reached zero.
    void launch_ready_nodes();

    /// Launch a single node for execution.
    void launch_single_node(const QString& node_id);

    /// Report a node as failed without running it (refused by the execution mode,
    /// step budget exhausted, unknown type).
    void fail_without_running(const QString& node_id, const QString& error);

    /// Apply the run's execution mode to a node's parameters. Rewrites `mode` to
    /// exactly "paper" or "live"; returns a non-empty refusal when the node may not
    /// run in this mode.
    QString enforce_trading_mode(const NodeDef& node, QJsonObject& params, const QVector<QJsonValue>& inputs) const;

    /// Complete a node without running it (skipped branch, pass-through) and
    /// release the nodes waiting on it.
    void settle_without_running(const QString& node_id, const QJsonObject& output);

    /// Called when a node finishes execution.
    void on_node_done(const QString& node_id, bool success, const QJsonValue& output, const QString& error,
                      int duration_ms = 0);

    /// Collect input data for a node from its upstream nodes' outputs.
    QVector<QJsonValue> collect_inputs(const QString& node_id) const;

    /// The node's parameters with {{ ... }} templates in its plain-text ("string")
    /// parameters resolved against the upstream data. Parameters without a template
    /// marker (all of them, in nearly every workflow) are passed through untouched.
    QJsonObject resolved_parameters(const NodeDef& node, const NodeTypeDef& type, const QVector<QJsonValue>& inputs) const;

    /// Emit execution_finished with aggregated results.
    void finish_execution();

    /// The executor that owns the step budget (the top-level run).
    WorkflowExecutor* budget_owner();

    /// Withdraw this run's token so in-flight trading nodes stop placing orders.
    void release_run_token();

    WorkflowDef workflow_;
    bool running_ = false;
    bool stop_requested_ = false;

    // Execution mode / nesting. A sub-workflow started from one of our nodes
    // (control.execute_workflow) records the top-level run as root_ and inherits its mode.
    bool live_allowed_ = false;
    int depth_ = 0;
    int launches_ = 0; // node launches counted on the top-level executor
    QPointer<WorkflowExecutor> root_;
    QVector<QPointer<WorkflowExecutor>> children_;
    QString hard_abort_reason_; // set when the step budget ran out — ignores continue-on-fail
    QString run_token_;         // see is_run_active()

    // Graph: node_id -> list of downstream node_ids
    QHash<QString, QVector<QString>> adjacency_;
    // Edge lookup: target_node_id -> list of edges pointing to it
    QHash<QString, QVector<EdgeDef>> incoming_edges_;

    // Parallel execution state
    QHash<QString, int> in_degree_; // remaining unfinished dependencies (-1 = launched)
    int pending_count_ = 0;         // nodes currently in-flight
    int completed_count_ = 0;       // total completed
    int total_count_ = 0;           // total nodes to execute

    // Failure reporting: first node failure, and the reason the run was aborted
    // (set only when a failing node — not the user — triggered the stop).
    QString first_failure_;
    QString abort_reason_;
    QSet<QString> in_flight_; // ids of nodes launched but not yet reported back
    QSet<QString> skipped_roots_; // disabled nodes with no inputs — complete as "skipped", never run

    // Node data
    QHash<QString, NodeExecutionResult> results_;
    QHash<QString, NodeDef> node_map_;
    QHash<QString, const NodeTypeDef*> type_cache_; // pre-resolved type pointers

    // Timing
    qint64 start_time_ms_ = 0;
};

} // namespace fincept::workflow
