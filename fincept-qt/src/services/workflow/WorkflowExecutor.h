#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QMap>
#include <QObject>
#include <QPointer>
#include <QVector>

namespace fincept::workflow {

/// Executes a WorkflowDef as a DAG — topological sort, cycle detection,
/// async per-node execution with signal-driven progress.
class WorkflowExecutor : public QObject {
    Q_OBJECT
  public:
    explicit WorkflowExecutor(QObject* parent = nullptr);

    /// Start executing a workflow. Non-blocking — progress via signals.
    void execute(const WorkflowDef& workflow);

    /// Execute starting from a specific node (partial execution).
    /// Only executes the subgraph downstream of start_node_id.
    void execute_from(const WorkflowDef& workflow, const QString& start_node_id);

    /// Stop a running execution.
    void stop();

    bool is_running() const { return running_; }

  signals:
    void execution_started(const QString& workflow_id);
    void node_started(const QString& node_id);
    void node_completed(const QString& node_id, const NodeExecutionResult& result);
    void execution_finished(const WorkflowExecutionResult& result);

  private:
    /// Build adjacency list from edges.
    void build_graph();

    /// DFS cycle detection. Returns true if cycle found.
    bool has_cycle() const;

    /// Compute topological ordering.
    QVector<QString> topological_sort() const;

    /// Execute the next ready node in the queue.
    void execute_next();

    /// Called when a node finishes execution.
    void on_node_done(const QString& node_id, bool success, QJsonValue output, QString error);

    /// Collect input data for a node from its upstream nodes' outputs.
    QVector<QJsonValue> collect_inputs(const QString& node_id) const;

    WorkflowDef workflow_;
    bool running_ = false;
    bool stop_requested_ = false;

    // Graph: node_id -> list of downstream node_ids
    QMap<QString, QVector<QString>> adjacency_;
    // Reverse: node_id -> list of upstream node_ids
    QMap<QString, QVector<QString>> reverse_adj_;

    // Execution state
    QVector<QString> execution_order_;
    int current_index_ = 0;
    QMap<QString, NodeExecutionResult> results_;
    QMap<QString, NodeDef> node_map_;

    // Timing
    qint64 start_time_ms_ = 0;
};

} // namespace fincept::workflow
