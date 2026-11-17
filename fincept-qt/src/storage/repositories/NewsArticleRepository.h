#pragma once
#include "services/news/NewsService.h"
#include "storage/repositories/BaseRepository.h"
#include "storage/repositories/NewsAnalyticsRepository.h"

#include <atomic>
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
    Result<QVector<fincept::services::NewsArticle>> load_recent(int64_t since_ts, const QString& category = {},
                                                                int limit = 2000) const;

    /// Total number of stored articles.
    int count() const;

    /// Delete articles older than cutoff_ts (unix seconds).
    Result<void> prune_older_than(int64_t cutoff_ts);

    /// Mark an article as read (sets seen_at = now). No-op if already seen.
    Result<void> mark_seen(const QString& id);

    /// Return article IDs seen since since_ts (unix seconds), newest first.
    /// Pass 0 for "no lower bound". Used to pre-populate the feed model on
    /// startup. Bounded like load_recent(): news_articles is the fastest-growing
    /// table, and an unbounded scan made first entry to the News screen cost
    /// proportional to all accumulated history.
    Result<QSet<QString>> load_seen_ids(int64_t since_ts = 0, int limit = 5000) const;

    /// Ensure the seen_at column exists (idempotent ALTER TABLE).
    /// Called once at startup before any seen operations.
    void ensure_seen_column();

    /// Toggle bookmark state for an article. Returns the new saved state.
    Result<bool> toggle_saved(const QString& id);

    /// Whether one article is bookmarked — a primary-key lookup (the detail
    /// panel used to load every saved row on each article open).
    Result<bool> is_saved(const QString& id) const;

    /// Number of bookmarked articles.
    int saved_count() const;

    /// Hourly article counts per category / region / language (plus the "all"
    /// total) for articles with sort_ts in [from, to), with the tone sums of the
    /// scored ones — the volume history news_volume_hourly is filled from.
    Result<QVector<NewsVolumeRow>> hourly_counts(int64_t from, int64_t to) const;

    /// True once migration v058 added the tone / tag columns (cached).
    bool has_analytics_columns() const;

    /// Return bookmarked articles, newest first. Bounded like load_recent().
    Result<QVector<fincept::services::NewsArticle>> load_saved(int limit = 2000) const;

    /// Ensure the saved column exists (idempotent ALTER TABLE).
    void ensure_saved_column();

    /// Persist the AI analysis JSON for an article URL (INSERT OR REPLACE).
    /// Overwrites any prior result so a re-run from the UI updates the cache.
    Result<void> save_analysis(const QString& url, const QString& analysis_json) const;

    /// Load the cached analysis JSON for a URL. Returns an empty string (ok)
    /// when none is stored.
    Result<QString> load_analysis(const QString& url) const;

    /// Full-text search over headline + summary using the FTS5 index.
    /// Returns matching articles ordered by FTS rank (best match first).
    /// Falls back to LIKE scan if FTS table is unavailable.
    Result<QVector<fincept::services::NewsArticle>> search_fts(const QString& query, int64_t since_ts = 0,
                                                               int limit = 500) const;

  private:
    NewsArticleRepository() = default;
    static fincept::services::NewsArticle map_row(QSqlQuery& q);
    /// map_row + the v058 columns (sentiment_score, event_type, ticker_conf,
    /// unverified_tickers) at indexes 18..21.
    static fincept::services::NewsArticle map_row_ext(QSqlQuery& q);
    /// The SELECT column list (with or without the v058 columns), prefixed
    /// with `alias` + "." when given.
    QString select_columns(const QString& alias = {}) const;
    mutable std::atomic<int> analytics_columns_{-1}; // -1 unknown, 0 absent, 1 present
};

} // namespace fincept
