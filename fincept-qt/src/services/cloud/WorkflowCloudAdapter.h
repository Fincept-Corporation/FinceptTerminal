#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for node-editor workflows — mirrors local workflows (graph of
/// nodes + edges) to `/v1/workflows`. Atomic full-replace: each push sends the
/// whole graph (`POST /import` to create, `PUT` to update); pull rebuilds the
/// local WorkflowDef. Local id (UUID) ↔ `wkf_`. Singleton.
class WorkflowCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static WorkflowCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("workflow"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    WorkflowCloudAdapter() = default;
    Q_DISABLE_COPY(WorkflowCloudAdapter)

    void push_upsert(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
    void apply_remote_workflow(const QJsonObject& o);
    void reconcile_deletes(const QSet<QString>& remote_ids);
};

} // namespace fincept::services::cloud
