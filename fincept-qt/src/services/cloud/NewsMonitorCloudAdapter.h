#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for news keyword monitors — mirrors local `news_monitors` to
/// `/v1/news-cloud/monitors`. Local UUID ↔ `nwm_`; keywords as a JSON array;
/// `enabled` via the toggle endpoint (parity-preserving), reconciled absolutely
/// on pull. (RSS feeds and saved articles are not synced in v1.) Singleton.
class NewsMonitorCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static NewsMonitorCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("news_monitor"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    NewsMonitorCloudAdapter() = default;
    Q_DISABLE_COPY(NewsMonitorCloudAdapter)

    void push_upsert(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
    void push_toggle(const OutboxRow& row, PushDone done);
    void apply_remote_monitor(const QJsonObject& o);
    void reconcile_deletes(const QSet<QString>& remote_ids);
};

} // namespace fincept::services::cloud
