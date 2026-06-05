#pragma once
#include "services/feeds/FeedTypes.h"
#include "storage/repositories/BaseRepository.h"

#include <QVector>

namespace fincept {

/// Persisted feed item history (feed_items table, v044). Enables offline cache
/// (show last-known items when a feed is down) and historical queries. Items are
/// keyed by (feed_id, item_id); inserts ignore duplicates to preserve first-seen.
class FeedItemRepository : public BaseRepository<feeds::FeedItem> {
  public:
    static FeedItemRepository& instance();

    /// Insert new items for a feed (existing item_ids are left untouched).
    Result<void> upsert(const QString& feed_id, const QVector<feeds::FeedItem>& items, qint64 fetched_at) const;
    /// Newest-first cached items for a feed.
    Result<QVector<feeds::FeedItem>> recent(const QString& feed_id, int limit) const;
    /// Newest-first items in [from_ts, to_ts] (epoch secs) for historical queries.
    Result<QVector<feeds::FeedItem>> history(const QString& feed_id, qint64 from_ts, qint64 to_ts, int limit) const;
    /// Keep only the `keep` newest items for a feed; drop the rest.
    Result<void> prune(const QString& feed_id, int keep) const;
    Result<void> clear(const QString& feed_id) const;

  private:
    FeedItemRepository() = default;
    static feeds::FeedItem map_row(QSqlQuery& q);
};

} // namespace fincept
