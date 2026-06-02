#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for dashboard layouts — mirrors local layout profiles to
/// `/v1/dashboard/layouts`. Keyed by profile name (the URL key on both sides);
/// atomic full-replace of the grid + widgets per profile. The outbox local_id IS
/// the profile name. Singleton.
class DashboardCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static DashboardCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("dashboard"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    DashboardCloudAdapter() = default;
    Q_DISABLE_COPY(DashboardCloudAdapter)

    void push_upsert(const OutboxRow& row, PushDone done);
    void apply_remote_layout(const QString& profile, const QJsonObject& o);
    void reconcile_deletes(const QSet<QString>& remote_profiles);
};

} // namespace fincept::services::cloud
