#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for notebooks — mirrors the local `notebooks` table (v038) to
/// `/v1/notebooks`. UUID ↔ `nbk_`; `cells` (JSON array) and `metadata` (JSON
/// object) are strings locally, structured on the wire. No local editor UI yet,
/// so in practice this pulls cloud notebooks down; push activates once a notebook
/// editor writes via NotebookRepository. Singleton.
class NotebookCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static NotebookCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("notebook"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    NotebookCloudAdapter() = default;
    Q_DISABLE_COPY(NotebookCloudAdapter)

    void push_upsert(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
    void apply_remote_notebook(const QJsonObject& o);
    void reconcile_deletes(const QSet<QString>& remote_ids);
};

} // namespace fincept::services::cloud
