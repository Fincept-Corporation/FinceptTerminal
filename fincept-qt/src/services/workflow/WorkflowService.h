#pragma once

#include "core/result/Result.h"
#include "screens/node_editor/NodeEditorTypes.h"

#include <QObject>
#include <QStringList>
#include <QVector>

namespace fincept::workflow {

/// Singleton service managing workflow persistence, validation and execution.
class WorkflowService : public QObject {
    Q_OBJECT
  public:
    static WorkflowService& instance();

    // ── CRUD ───────────────────────────────────────────────────────
    void save_workflow(const WorkflowDef& wf);
    void load_workflow(const QString& id);
    void list_workflows();
    void delete_workflow(const QString& id);

    // ── Import/Export ──────────────────────────────────────────────
    Result<WorkflowDef> import_from_json(const QString& path);
    Result<void> export_to_json(const WorkflowDef& wf, const QString& path);

    // ── Execution ───────────────────────────────────────────────────
    /// One row per node in `wf` that would act for REAL if the workflow were run in the
    /// given mode — and the same inside any sub-workflow reached through
    /// control.execute_workflow (sub-workflows inherit the parent's mode). Disabled
    /// nodes are ignored. `moves_money` rows reach a broker: in LIVE mode, order nodes
    /// whose own Mode is live plus the always-live smart_order / cancel_all / close_all
    /// and live-trading MCP tools; each names the broker account that would be used.
    /// Other destructive MCP tools (file writes, deletes) are listed in either mode.
    struct ActionRow {
        QString node_id;
        QString text;
        bool moves_money = false;
    };
    static QVector<ActionRow> live_action_rows(const WorkflowDef& wf, bool live_mode);

    /// Order nodes set to live that a PAPER run will simulate instead (display rows).
    static QStringList simulated_live_rows(const WorkflowDef& wf);

    /// Pre-run validation problem on one node (empty node_id = whole workflow).
    struct ValidationIssue {
        QString node_id;
        QString message;
    };
    /// Required parameters, order sanity (quantity, limit price, stops), unknown node
    /// types, invalid Mode values and cycles. Empty = OK to run.
    static QVector<ValidationIssue> validate(const WorkflowDef& wf);

    void execute_workflow(const WorkflowDef& wf);
    void execute_from_node(const WorkflowDef& wf, const QString& start_node_id);
    void stop_execution();
    bool is_executing() const;

  signals:
    void workflow_saved(const QString& id);
    void workflow_loaded(const WorkflowDef& wf);
    void workflow_load_failed(const QString& error);
    void workflows_listed(const QVector<WorkflowDef>& workflows);
    void workflow_deleted(const QString& id);
    void save_failed(const QString& error);

    // Execution signals (forwarded from WorkflowExecutor)
    void execution_started(const QString& workflow_id);
    void node_execution_started(const QString& node_id);
    void node_execution_completed(const QString& node_id, const NodeExecutionResult& result);
    void execution_finished(const WorkflowExecutionResult& result);

  private:
    WorkflowService();
    /// Create the executor for a new run and wire its signals. Returns nullptr
    /// (and logs) while a previous run is still active.
    class WorkflowExecutor* make_executor();
    class WorkflowExecutor* executor_ = nullptr;
};

} // namespace fincept::workflow
