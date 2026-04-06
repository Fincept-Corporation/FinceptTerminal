#pragma once
#include "services/news/NewsService.h"
#include "storage/repositories/BaseRepository.h"

#include <cstdint>

namespace fincept {

/// Persists NewsArticle records to the news_articles SQLite table.
/// INSERT OR IGNORE keeps the first-seen enriched copy; duplicates are skipped.
class NewsArticleRepository : public BaseRepository<fincept::services::NewsArticle> {
  public:
    static NewsArticleRepository& instance();

    /// Insert a batch of articles; ignores duplicates (same id).
    /// Call this off the UI thread — wraps in a single transaction for speed.
    Result<void> upsert_batch(const QVector<fincept::services::NewsArticle>& articles);

    /// Load articles newer than since_ts (unix seconds).
    /// Optional category filter ("" = all). Ordered newest-first.
    Result<QVector<fincept::services::NewsArticle>> load_recent(int64_t since_ts,
                                                                const QString& category = {},
                                                                int limit = 2000) const;

    /// Total number of stored articles.
    int count() const;

    /// Delete articles older than cutoff_ts (unix seconds).
    Result<void> prune_older_than(int64_t cutoff_ts);

    /// Mark an article as read (sets seen_at = now). No-op if already seen.
    Result<void> mark_seen(const QString& id);

    /// Return all article IDs seen since since_ts (unix seconds).
    /// Pass 0 to load all. Used to pre-populate the feed model on startup.
    Result<QSet<QString>> load_seen_ids(int64_t since_ts = 0) const;

    /// Ensure the seen_at column exists (idempotent ALTER TABLE).
    /// Called once at startup before any seen operations.
    void ensure_seen_column();

    /// Toggle bookmark state for an article. Returns the new saved state.
    Result<bool> toggle_saved(const QString& id);

    /// Return all bookmarked articles, newest first.
    Result<QVector<fincept::services::NewsArticle>> load_saved() const;

    /// Ensure the saved column exists (idempotent ALTER TABLE).
    void ensure_saved_column();

    /// Full-text search over headline + summary using the FTS5 index.
    /// Returns matching articles ordered by FTS rank (best match first).
    /// Falls back to LIKE scan if FTS table is unavailable.
    Result<QVector<fincept::services::NewsArticle>> search_fts(const QString& query,
                                                               int64_t since_ts = 0,
                                                               int limit = 500) const;

  private:
    NewsArticleRepository() = default;
    static fincept::services::NewsArticle map_row(QSqlQuery& q);
};

} // namespace fincept
