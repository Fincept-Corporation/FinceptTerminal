#pragma once
// NewsAnalyticsRepository — the News screen's analytics tables (migration v058):
// hourly volume history, the symbol alias table, saved searches and feed health.
//
// Every method checks once whether the v058 tables exist and degrades to an
// empty result (never an error log storm) on a database that has not been
// migrated yet. Call from worker threads (Database hands out per-thread
// connections); none of these belong on the GUI thread.

#include "storage/repositories/BaseRepository.h"

#include <QString>
#include <QVector>

#include <atomic>

namespace fincept {

struct NewsVolumeRow {
    qint64 hour_ts = 0; ///< start of the hour, unix seconds
    QString dim;        ///< "all" | "cat" | "region" | "lang"
    QString key;        ///< category / region / language; empty for "all"
    int count = 0;
    int scored = 0;
    double score_sum = 0.0;
};

struct NewsSymbolAliasRow {
    QString alias;  ///< company name as it appears in text
    QString symbol; ///< Yahoo-style symbol
    QString name;   ///< display name
    QString source; ///< "search" | "watchlist" | "portfolio" | "manual"
};

struct NewsSavedSearchRow {
    QString id;
    QString name;
    QString state_json; ///< filter state (category, time, search, language, region, ...)
    QString alert_json; ///< alert rule; "{}" = no alert
    qint64 created_at = 0;
    qint64 updated_at = 0;
};

struct NewsFeedHealthRow {
    QString feed_id;
    qint64 last_ok_at = 0;
    qint64 last_fail_at = 0;
    int consecutive_failures = 0;
    QString last_error;
    int last_http = 0;
    int last_count = 0;
};

class NewsAnalyticsRepository : public BaseRepository<NewsVolumeRow> {
  public:
    static NewsAnalyticsRepository& instance();

    /// True once the v058 tables exist (checked once per process, then cached).
    bool available() const;

    // ── Volume history ────────────────────────────────────────────────────
    /// Upsert hourly counts; an existing hour keeps the larger count (a later
    /// refresh may have seen more of that hour's articles, never fewer).
    Result<void> record_volume(const QVector<NewsVolumeRow>& rows);
    /// Rows of `dim` with hour_ts >= since_hour, oldest first.
    Result<QVector<NewsVolumeRow>> load_volume(qint64 since_hour, const QString& dim) const;
    /// Drop history older than `cutoff` (unix seconds).
    Result<void> prune_volume(qint64 cutoff);

    // ── Symbol aliases ────────────────────────────────────────────────────
    Result<void> upsert_aliases(const QVector<NewsSymbolAliasRow>& rows);
    Result<QVector<NewsSymbolAliasRow>> load_aliases(int limit = 20000) const;

    // ── Saved searches ────────────────────────────────────────────────────
    Result<QVector<NewsSavedSearchRow>> list_saved_searches() const;
    Result<void> save_saved_search(const NewsSavedSearchRow& row);
    Result<void> remove_saved_search(const QString& id);

    // ── Feed health ───────────────────────────────────────────────────────
    Result<QVector<NewsFeedHealthRow>> load_feed_health() const;
    Result<void> save_feed_health(const QVector<NewsFeedHealthRow>& rows);

  private:
    NewsAnalyticsRepository() = default;
    mutable std::atomic<int> available_{-1}; // -1 unknown, 0 absent, 1 present
};

} // namespace fincept
