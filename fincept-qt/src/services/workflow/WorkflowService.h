#pragma once

#include "core/result/Result.h"
#include "screens/node_editor/NodeEditorTypes.h"

#include <QObject>
#include <QTimer>
#include <QVector>

namespace fincept::workflow {

/// Singleton service managing workflow persistence and (future) execution.
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
    class WorkflowExecutor* executor_ = nullptr;
};

} // namespace fincept::workflow
