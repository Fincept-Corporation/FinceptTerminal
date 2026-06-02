#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for report-builder documents — mirrors local `reports` to
/// `/v1/reports`. Local int id ↔ `rpt_`; `content_json` is a string locally but a
/// JSON object on the wire. (Report templates are not synced in v1.) Singleton.
class ReportCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static ReportCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("report"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    ReportCloudAdapter() = default;
    Q_DISABLE_COPY(ReportCloudAdapter)

    void push_create(const OutboxRow& row, PushDone done);
    void push_update(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
    void apply_remote_report(const QJsonObject& o);
    void reconcile_deletes(const QSet<QString>& remote_ids);
};

} // namespace fincept::services::cloud
