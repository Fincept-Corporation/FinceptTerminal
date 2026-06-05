#pragma once
#include "services/feeds/FeedTypes.h"
#include "storage/repositories/BaseRepository.h"

#include <QVector>

namespace fincept {

/// SQLite persistence for generalized feed monitor subscriptions (feed_subscriptions
/// table, v043). Separate from RssFeedRepository so arbitrary user feeds never leak
/// into the curated News catalog. Mirrors the RssFeedRepository pattern; JSON columns
/// (field_map, headers) are serialized via QJsonDocument. No cloud sync.
class FeedSubscriptionRepository : public BaseRepository<feeds::FeedSubscription> {
  public:
    static FeedSubscriptionRepository& instance();

    Result<QVector<feeds::FeedSubscription>> list_all() const;
    Result<void> upsert(const feeds::FeedSubscription& row) const;
    Result<void> remove(const QString& id) const;
    Result<void> set_enabled(const QString& id, bool enabled) const;

  private:
    FeedSubscriptionRepository() = default;
    static feeds::FeedSubscription map_row(QSqlQuery& q);
};

} // namespace fincept
