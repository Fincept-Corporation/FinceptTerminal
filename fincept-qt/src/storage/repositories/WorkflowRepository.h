#pragma once
#include "screens/node_editor/NodeEditorTypes.h"
#include "storage/repositories/BaseRepository.h"

namespace fincept {

/// Lightweight DB row for workflow list display.
struct WorkflowRow {
    QString id;
    QString name;
    QString description;
    QString status;
    QString created_at;
    QString updated_at;
};

class WorkflowRepository : public BaseRepository<WorkflowRow> {
  public:
    static WorkflowRepository& instance();

    /// Save a complete workflow (upsert workflow + delete/re-insert nodes & edges).
    Result<void> save(const workflow::WorkflowDef& wf);

    /// Load a complete workflow by id.
    Result<workflow::WorkflowDef> load(const QString& id);

    /// List all workflow summaries (no nodes/edges).
    Result<QVector<WorkflowRow>> list_all();

    /// Delete a workflow and cascade nodes/edges.
    Result<void> remove(const QString& id);

  private:
    WorkflowRepository() = default;
    static WorkflowRow map_row(QSqlQuery& q);
};

} // namespace fincept
