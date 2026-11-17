#include "storage/repositories/NewsAnalyticsRepository.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>

namespace fincept {

NewsAnalyticsRepository& NewsAnalyticsRepository::instance() {
    static NewsAnalyticsRepository s;
    return s;
}

bool NewsAnalyticsRepository::available() const {
    const int known = available_.load(std::memory_order_relaxed);
    if (known >= 0)
        return known == 1;
    auto r = db().execute("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name IN "
                          "('news_volume_hourly', 'news_symbol_aliases', 'news_saved_searches', 'news_feed_health')",
                          {});
    bool present = false;
    if (r.is_ok()) {
        auto& q = r.value();
        present = q.next() && q.value(0).toInt() == 4;
    }
    // Only a positive answer is final: the migration may still run later in a
    // session that started before it was registered.
    if (present)
        available_.store(1, std::memory_order_relaxed);
    return present;
}

// ── Volume history ───────────────────────────────────────────────────────────

Result<void> NewsAnalyticsRepository::record_volume(const QVector<NewsVolumeRow>& rows) {
    if (rows.isEmpty() || !available())
        return Result<void>::ok();
    auto& database = db();
    auto bt = database.begin_transaction();
    if (bt.is_err())
        return bt;
    const QString sql = "INSERT INTO news_volume_hourly (hour_ts, dim, key, count, scored, score_sum) "
                        "VALUES (?, ?, ?, ?, ?, ?) "
                        "ON CONFLICT(hour_ts, dim, key) DO UPDATE SET "
                        "  count = excluded.count, scored = excluded.scored, score_sum = excluded.score_sum "
                        "WHERE excluded.count > news_volume_hourly.count";
    for (const auto& row : rows) {
        // The "all" dimension has no key. A null QString binds as SQL NULL, which
        // the NOT NULL key column (part of the primary key) rejects — and one bad
        // row rolled the whole hour batch back. Bind an empty, non-null string.
        const QString key = row.key.isNull() ? QString("") : row.key;
        auto r = database.execute(sql, {row.hour_ts, row.dim, key, row.count, row.scored, row.score_sum});
        if (r.is_err()) {
            database.rollback();
            return Result<void>::err(r.error());
        }
    }
    auto ct = database.commit();
    if (ct.is_err()) {
        database.rollback();
        return ct;
    }
    return Result<void>::ok();
}

Result<QVector<NewsVolumeRow>> NewsAnalyticsRepository::load_volume(qint64 since_hour, const QString& dim) const {
    if (!available())
        return Result<QVector<NewsVolumeRow>>::ok({});
    return query_list("SELECT hour_ts, dim, key, count, scored, score_sum FROM news_volume_hourly "
                      "WHERE hour_ts >= ? AND dim = ? ORDER BY hour_ts ASC",
                      {since_hour, dim}, [](QSqlQuery& q) {
                          NewsVolumeRow r;
                          r.hour_ts = q.value(0).toLongLong();
                          r.dim = q.value(1).toString();
                          r.key = q.value(2).toString();
                          r.count = q.value(3).toInt();
                          r.scored = q.value(4).toInt();
                          r.score_sum = q.value(5).toDouble();
                          return r;
                      });
}

Result<void> NewsAnalyticsRepository::prune_volume(qint64 cutoff) {
    if (!available())
        return Result<void>::ok();
    return exec_write("DELETE FROM news_volume_hourly WHERE hour_ts < ?", {cutoff});
}

// ── Symbol aliases ───────────────────────────────────────────────────────────

Result<void> NewsAnalyticsRepository::upsert_aliases(const QVector<NewsSymbolAliasRow>& rows) {
    if (rows.isEmpty() || !available())
        return Result<void>::ok();
    auto& database = db();
    auto bt = database.begin_transaction();
    if (bt.is_err())
        return bt;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QString sql = "INSERT INTO news_symbol_aliases (alias, symbol, name, source, updated_at) "
                        "VALUES (?, ?, ?, ?, ?) "
                        "ON CONFLICT(alias, symbol) DO UPDATE SET name = excluded.name, source = excluded.source, "
                        "updated_at = excluded.updated_at";
    for (const auto& row : rows) {
        if (row.alias.trimmed().isEmpty() || row.symbol.trimmed().isEmpty())
            continue;
        auto r = database.execute(sql, {row.alias.trimmed(), row.symbol.trimmed().toUpper(), row.name, row.source, now});
        if (r.is_err()) {
            database.rollback();
            return Result<void>::err(r.error());
        }
    }
    auto ct = database.commit();
    if (ct.is_err()) {
        database.rollback();
        return ct;
    }
    return Result<void>::ok();
}

Result<QVector<NewsSymbolAliasRow>> NewsAnalyticsRepository::load_aliases(int limit) const {
    if (!available())
        return Result<QVector<NewsSymbolAliasRow>>::ok({});
    return query_list_as<NewsSymbolAliasRow>(
        "SELECT alias, symbol, name, source FROM news_symbol_aliases ORDER BY updated_at DESC LIMIT ?", {limit},
        [](QSqlQuery& q) {
            return NewsSymbolAliasRow{q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                                      q.value(3).toString()};
        });
}

// ── Saved searches ───────────────────────────────────────────────────────────

Result<QVector<NewsSavedSearchRow>> NewsAnalyticsRepository::list_saved_searches() const {
    if (!available())
        return Result<QVector<NewsSavedSearchRow>>::ok({});
    return query_list_as<NewsSavedSearchRow>(
        "SELECT id, name, state_json, alert_json, created_at, updated_at FROM news_saved_searches "
        "ORDER BY name COLLATE NOCASE",
        {}, [](QSqlQuery& q) {
            NewsSavedSearchRow r;
            r.id = q.value(0).toString();
            r.name = q.value(1).toString();
            r.state_json = q.value(2).toString();
            r.alert_json = q.value(3).toString();
            r.created_at = q.value(4).toLongLong();
            r.updated_at = q.value(5).toLongLong();
            return r;
        });
}

Result<void> NewsAnalyticsRepository::save_saved_search(const NewsSavedSearchRow& row) {
    if (!available())
        return Result<void>::err("news_saved_searches table missing (migration v058 not applied)");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return exec_write("INSERT INTO news_saved_searches (id, name, state_json, alert_json, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(id) DO UPDATE SET name = excluded.name, state_json = excluded.state_json, "
                      "alert_json = excluded.alert_json, updated_at = excluded.updated_at",
                      {row.id, row.name, row.state_json.isEmpty() ? QStringLiteral("{}") : row.state_json,
                       row.alert_json.isEmpty() ? QStringLiteral("{}") : row.alert_json,
                       row.created_at > 0 ? row.created_at : now, now});
}

Result<void> NewsAnalyticsRepository::remove_saved_search(const QString& id) {
    if (!available())
        return Result<void>::ok();
    return exec_write("DELETE FROM news_saved_searches WHERE id = ?", {id});
}

// ── Feed health ──────────────────────────────────────────────────────────────

Result<QVector<NewsFeedHealthRow>> NewsAnalyticsRepository::load_feed_health() const {
    if (!available())
        return Result<QVector<NewsFeedHealthRow>>::ok({});
    return query_list_as<NewsFeedHealthRow>(
        "SELECT feed_id, last_ok_at, last_fail_at, consecutive_failures, last_error, last_http, last_count "
        "FROM news_feed_health",
        {}, [](QSqlQuery& q) {
            NewsFeedHealthRow r;
            r.feed_id = q.value(0).toString();
            r.last_ok_at = q.value(1).toLongLong();
            r.last_fail_at = q.value(2).toLongLong();
            r.consecutive_failures = q.value(3).toInt();
            r.last_error = q.value(4).toString();
            r.last_http = q.value(5).toInt();
            r.last_count = q.value(6).toInt();
            return r;
        });
}

Result<void> NewsAnalyticsRepository::save_feed_health(const QVector<NewsFeedHealthRow>& rows) {
    if (rows.isEmpty() || !available())
        return Result<void>::ok();
    auto& database = db();
    auto bt = database.begin_transaction();
    if (bt.is_err())
        return bt;
    const QString sql = "INSERT OR REPLACE INTO news_feed_health "
                        "(feed_id, last_ok_at, last_fail_at, consecutive_failures, last_error, last_http, last_count) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?)";
    for (const auto& r : rows) {
        auto e = database.execute(sql, {r.feed_id, r.last_ok_at, r.last_fail_at, r.consecutive_failures,
                                        r.last_error.left(300), r.last_http, r.last_count});
        if (e.is_err()) {
            database.rollback();
            return Result<void>::err(e.error());
        }
    }
    auto ct = database.commit();
    if (ct.is_err()) {
        database.rollback();
        return ct;
    }
    return Result<void>::ok();
}

} // namespace fincept
