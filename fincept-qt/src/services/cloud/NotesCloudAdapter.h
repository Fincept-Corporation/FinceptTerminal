#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for the notes domain — mirrors local financial notes to
/// `/v1/notes`. Local id is an int (mapped to `nte_` via sync_map). Favorite /
/// archive are toggle endpoints (parity-preserving); pull reconciles absolute
/// state. Singleton; registered with CloudSyncEngine in main.cpp.
class NotesCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static NotesCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("note"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    NotesCloudAdapter() = default;
    Q_DISABLE_COPY(NotesCloudAdapter)

    void push_create(const OutboxRow& row, PushDone done);
    void push_update(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
    void push_toggle(const OutboxRow& row, const QString& sub, PushDone done); // "favorite" | "archive"

    void apply_remote_note(const QJsonObject& o);                // under ApplyGuard
    void reconcile_deletes(const QSet<QString>& remote_ids);     // under ApplyGuard
};

} // namespace fincept::services::cloud
