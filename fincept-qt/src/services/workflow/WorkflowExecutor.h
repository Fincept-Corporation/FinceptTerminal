#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVector>

namespace fincept::workflow {

/// Executes a WorkflowDef as a DAG — topological sort, cycle detection,
/// parallel per-level execution with signal-driven progress.
class WorkflowExecutor : public QObject {
    Q_OBJECT
  public:
    explicit WorkflowExecutor(QObject* parent = nullptr);

    /// Start executing a workflow. Non-blocking — progress via signals.
    void execute(const WorkflowDef& workflow);

    /// Execute starting from a specific node (partial execution).
    /// Only executes the subgraph downstream of start_node_id.
    void execute_from(const WorkflowDef& workflow, const QString& start_node_id);

    /// Stop a running execution. In-flight nodes are allowed to finish; calling
    /// stop() a second time while some are still running abandons them and
    /// finishes immediately (escape hatch for a node that never reports back).
    void stop();

    bool is_running() const { return running_; }

  signals:
    void execution_started(const QString& workflow_id);
    void node_started(const QString& node_id);
    void node_completed(const QString& node_id, const NodeExecutionResult& result);
    void execution_finished(const WorkflowExecutionResult& result);

  private:
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

    /// DFS cycle detection. Returns true if cycle found.
    bool has_cycle() const;

    /// Compute topological ordering.
    QVector<QString> topological_sort() const;

    /// Launch all nodes whose in-degree has reached zero.
    void launch_ready_nodes();

    /// Launch a single node for execution.
    void launch_single_node(const QString& node_id);

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

    WorkflowDef workflow_;
    bool running_ = false;
    bool stop_requested_ = false;

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
