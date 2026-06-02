#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for the portfolio domain — mirrors the full graph (portfolio +
/// assets + transactions + snapshots) to `/v1/portfolios`.
///
/// Because two repo mutators (update/delete transaction) don't carry the
/// portfolio id, and the sub-resources are nested, sync is driven by a single
/// portfolio-level "sync" outbox marker per changed portfolio (deduped via
/// SyncOutbox::record_unique). push("sync") does a full reconcile:
///   - portfolio fields  : POST (new) / PUT (existing); broker_account_id is
///                         device-local and never sent.
///   - assets (by symbol): PUT existing / POST new / DELETE gone. (POST *adds*
///                         quantity server-side, so existing assets must PUT.)
///   - transactions      : sync_map("portfolio_txn") tracks local-uuid<->ptx_;
///                         POST unmapped locals, DELETE orphaned-cloud.
///   - snapshots (by date): POST dates missing in cloud (server upserts by date).
/// The /sell and /dividend atomic endpoints are never used (the terminal already
/// applied that math locally). Singleton; registered in main.cpp.
class PortfolioCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static PortfolioCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("portfolio"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    PortfolioCloudAdapter() = default;
    Q_DISABLE_COPY(PortfolioCloudAdapter)

    void push_delete(const OutboxRow& row, PushDone done);
    void push_sync(const OutboxRow& row, PushDone done);
    // Reconcile a portfolio's children (local → cloud) once the portfolio's
    // remote id (pfl) is known.
    void reconcile_up(const QString& local_pid, const QString& pfl, PushDone done);

    // Pull: apply one cloud portfolio (detail+txns+snaps) into the local cache
    // under SyncOutbox::ApplyGuard.
    void apply_remote_portfolio(const QJsonObject& detail, const QJsonArray& txns, const QJsonArray& snaps);
    void reconcile_portfolio_deletes(const QSet<QString>& remote_ids);
};

} // namespace fincept::services::cloud
