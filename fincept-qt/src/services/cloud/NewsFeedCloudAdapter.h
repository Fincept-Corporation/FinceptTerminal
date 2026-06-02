#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for user RSS feeds — mirrors local `rss_feeds` (user-added feeds
/// + overrides) to `/v1/news-cloud/feeds`. Local UUID ↔ `rss_`; `enabled` via the
/// toggle endpoint; `url` is the immutable conflict key (sent on create, not on
/// update). Singleton.
class NewsFeedCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static NewsFeedCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("news_feed"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    NewsFeedCloudAdapter() = default;
    Q_DISABLE_COPY(NewsFeedCloudAdapter)

    void push_upsert(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
    void push_toggle(const OutboxRow& row, PushDone done);
    void apply_remote_feed(const QJsonObject& o);
    void reconcile_deletes(const QSet<QString>& remote_ids);
};

} // namespace fincept::services::cloud
