#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for the watchlist domain — mirrors local watchlists (+ their
/// stocks) to `/v1/watchlists`. QObject so it can be the lifetime context for
/// CloudClient callbacks. Singleton; registered with CloudSyncEngine in main.cpp.
class WatchlistCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static WatchlistCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("watchlist"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    WatchlistCloudAdapter() = default;
    Q_DISABLE_COPY(WatchlistCloudAdapter)

    // push sub-handlers, one per outbox op
    void push_create(const OutboxRow& row, PushDone done);
    void push_update(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
    void push_stock_add(const OutboxRow& row, PushDone done);
    void push_stock_remove(const OutboxRow& row, PushDone done);

    // pull helpers (run under SyncOutbox::ApplyGuard)
    void apply_remote_watchlist(const QJsonObject& o);
    void reconcile_deletes(const QSet<QString>& remote_ids);
};

} // namespace fincept::services::cloud
