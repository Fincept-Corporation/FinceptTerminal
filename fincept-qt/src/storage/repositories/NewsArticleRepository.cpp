#include "storage/repositories/NewsArticleRepository.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <cstdint>

namespace fincept {

using fincept::services::Impact;
using fincept::services::NewsArticle;
using fincept::services::Priority;
using fincept::services::Sentiment;
using fincept::services::SourceFlag;
using fincept::services::ThreatLevel;

// ── Singleton ────────────────────────────────────────────────────────────────

NewsArticleRepository& NewsArticleRepository::instance() {
    static NewsArticleRepository s;
    return s;
}

// ── Helpers: enum ↔ string ───────────────────────────────────────────────────

static QString priority_str(Priority p) {
    switch (p) {
    case Priority::FLASH:    return "FLASH";
    case Priority::URGENT:   return "URGENT";
    case Priority::BREAKING: return "BREAKING";
    default:                 return "ROUTINE";
    }
}

static Priority priority_from(const QString& s) {
    if (s == "FLASH")    return Priority::FLASH;
    if (s == "URGENT")   return Priority::URGENT;
    if (s == "BREAKING") return Priority::BREAKING;
    return Priority::ROUTINE;
}

static QString sentiment_str(Sentiment s) {
    switch (s) {
    case Sentiment::BULLISH: return "BULLISH";
    case Sentiment::BEARISH: return "BEARISH";
    default:                 return "NEUTRAL";
    }
}

static Sentiment sentiment_from(const QString& s) {
    if (s == "BULLISH") return Sentiment::BULLISH;
    if (s == "BEARISH") return Sentiment::BEARISH;
    return Sentiment::NEUTRAL;
}

static QString impact_str(Impact i) {
    switch (i) {
    case Impact::HIGH:   return "HIGH";
    case Impact::MEDIUM: return "MEDIUM";
    default:             return "LOW";
    }
}

static Impact impact_from(const QString& s) {
    if (s == "HIGH")   return Impact::HIGH;
    if (s == "MEDIUM") return Impact::MEDIUM;
    return Impact::LOW;
}

static QString threat_level_str(ThreatLevel l) {
    switch (l) {
    case ThreatLevel::CRITICAL: return "CRITICAL";
    case ThreatLevel::HIGH:     return "HIGH";
    case ThreatLevel::MEDIUM:   return "MEDIUM";
    case ThreatLevel::LOW:      return "LOW";
    default:                    return "INFO";
    }
}

static ThreatLevel threat_level_from(const QString& s) {
    if (s == "CRITICAL") return ThreatLevel::CRITICAL;
    if (s == "HIGH")     return ThreatLevel::HIGH;
    if (s == "MEDIUM")   return ThreatLevel::MEDIUM;
    if (s == "LOW")      return ThreatLevel::LOW;
    return ThreatLevel::INFO;
}

// ── Row mapper ───────────────────────────────────────────────────────────────

NewsArticle NewsArticleRepository::map_row(QSqlQuery& q) {
    NewsArticle a;
    a.id         = q.value(0).toString();
    a.headline   = q.value(1).toString();
    a.summary    = q.value(2).toString();
    a.source     = q.value(3).toString();
    a.region     = q.value(4).toString();
    a.category   = q.value(5).toString();
    a.link       = q.value(6).toString();
    a.sort_ts    = q.value(7).toLongLong();
    a.priority   = priority_from(q.value(8).toString());
    a.sentiment  = sentiment_from(q.value(9).toString());
    a.impact     = impact_from(q.value(10).toString());
    a.tier       = q.value(12).toInt();
    a.lang       = q.value(13).toString();
    a.threat.level      = threat_level_from(q.value(14).toString());
    a.threat.category   = q.value(15).toString();
    a.threat.confidence = q.value(16).toDouble();
    a.source_flag       = static_cast<SourceFlag>(q.value(17).toInt());

    // tickers: stored as JSON array string
    const QString tickers_json = q.value(11).toString();
    if (!tickers_json.isEmpty()) {
        const QJsonArray arr = QJsonDocument::fromJson(tickers_json.toUtf8()).array();
        for (const auto& v : arr)
            a.tickers << v.toString();
    }
    return a;
}

// ── upsert_batch ─────────────────────────────────────────────────────────────

Result<void> NewsArticleRepository::upsert_batch(const QVector<NewsArticle>& articles) {
    if (articles.isEmpty())
        return Result<void>::ok();

    auto& database = db();
    auto bt = database.begin_transaction();
    if (bt.is_err())
        return bt;

    const QString sql =
        "INSERT OR IGNORE INTO news_articles "
        "(id, headline, summary, source, region, category, link, sort_ts, "
        " priority, sentiment, impact, tickers, tier, lang, "
        " threat_level, threat_cat, threat_conf, source_flag) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

    int inserted = 0;
    for (const auto& a : articles) {
        QJsonArray tickers_arr;
        for (const auto& t : a.tickers)
            tickers_arr.append(t);
        const QString tickers_json = QString::fromUtf8(
            QJsonDocument(tickers_arr).toJson(QJsonDocument::Compact));

        auto r = exec_write(sql, {
            a.id,
            a.headline,
            a.summary,
            a.source,
            a.region,
            a.category,
            a.link,
            static_cast<qint64>(a.sort_ts),
            priority_str(a.priority),
            sentiment_str(a.sentiment),
            impact_str(a.impact),
            tickers_json,
            a.tier,
            a.lang,
            threat_level_str(a.threat.level),
            a.threat.category,
            a.threat.confidence,
            static_cast<int>(a.source_flag)
        });
        if (r.is_ok())
            ++inserted;
    }

    auto ct = database.commit();
    if (ct.is_err()) {
        database.rollback();
        return ct;
    }

    LOG_DEBUG("NewsArticleRepo",
              QString("upsert_batch: %1 new / %2 total").arg(inserted).arg(articles.size()));
    return Result<void>::ok();
}

// ── load_recent ──────────────────────────────────────────────────────────────

Result<QVector<NewsArticle>> NewsArticleRepository::load_recent(int64_t since_ts,
                                                                  const QString& category,
                                                                  int limit) const {
    if (category.isEmpty()) {
        return query_list(
            "SELECT id, headline, summary, source, region, category, link, sort_ts, "
            "       priority, sentiment, impact, tickers, tier, lang, "
            "       threat_level, threat_cat, threat_conf, source_flag "
            "FROM news_articles WHERE sort_ts >= ? ORDER BY sort_ts DESC LIMIT ?",
            {static_cast<qint64>(since_ts), limit},
            map_row);
    }
    return query_list(
        "SELECT id, headline, summary, source, region, category, link, sort_ts, "
        "       priority, sentiment, impact, tickers, tier, lang, "
        "       threat_level, threat_cat, threat_conf, source_flag "
        "FROM news_articles WHERE sort_ts >= ? AND category = ? ORDER BY sort_ts DESC LIMIT ?",
        {static_cast<qint64>(since_ts), category, limit},
        map_row);
}

// ── count ────────────────────────────────────────────────────────────────────

int NewsArticleRepository::count() const {
    auto r = db().execute("SELECT COUNT(*) FROM news_articles", {});
    if (r.is_err())
        return 0;
    auto& q = r.value();
    return q.next() ? q.value(0).toInt() : 0;
}

// ── prune_older_than ─────────────────────────────────────────────────────────

Result<void> NewsArticleRepository::prune_older_than(int64_t cutoff_ts) {
    return exec_write("DELETE FROM news_articles WHERE sort_ts < ?",
                      {static_cast<qint64>(cutoff_ts)});
}

// ── ensure_seen_column ───────────────────────────────────────────────────────

void NewsArticleRepository::ensure_seen_column() {
    // ALTER TABLE IF NOT EXISTS is not supported in older SQLite — use a
    // pragma to check if the column already exists before adding it.
    auto r = db().execute("PRAGMA table_info(news_articles)", {});
    if (r.is_err())
        return;
    auto& q = r.value();
    bool has_seen_at = false;
    while (q.next()) {
        if (q.value(1).toString() == "seen_at") {
            has_seen_at = true;
            break;
        }
    }
    if (!has_seen_at)
        exec_raw("ALTER TABLE news_articles ADD COLUMN seen_at INTEGER");
}

// ── mark_seen ────────────────────────────────────────────────────────────────

Result<void> NewsArticleRepository::mark_seen(const QString& id) {
    return exec_write(
        "UPDATE news_articles SET seen_at = ? WHERE id = ? AND seen_at IS NULL",
        {static_cast<qint64>(QDateTime::currentSecsSinceEpoch()), id});
}

// ── ensure_saved_column ───────────────────────────────────────────────────────

void NewsArticleRepository::ensure_saved_column() {
    auto r = db().execute("PRAGMA table_info(news_articles)", {});
    if (r.is_err())
        return;
    auto& q = r.value();
    bool has_saved = false;
    while (q.next()) {
        if (q.value(1).toString() == "saved") {
            has_saved = true;
            break;
        }
    }
    if (!has_saved)
        exec_raw("ALTER TABLE news_articles ADD COLUMN saved INTEGER DEFAULT 0");
}

// ── toggle_saved ──────────────────────────────────────────────────────────────

Result<bool> NewsArticleRepository::toggle_saved(const QString& id) {
    // Read current state
    auto r = db().execute("SELECT saved FROM news_articles WHERE id = ?", {id});
    if (r.is_err())
        return Result<bool>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<bool>::err("Article not found");
    bool currently_saved = q.value(0).toInt() != 0;
    bool new_state = !currently_saved;
    auto wr = exec_write("UPDATE news_articles SET saved = ? WHERE id = ?",
                         {new_state ? 1 : 0, id});
    if (wr.is_err())
        return Result<bool>::err(wr.error());
    return Result<bool>::ok(new_state);
}

// ── load_saved ────────────────────────────────────────────────────────────────

Result<QVector<fincept::services::NewsArticle>> NewsArticleRepository::load_saved() const {
    return query_list(
        "SELECT id, headline, summary, source, region, category, link, sort_ts, "
        "       priority, sentiment, impact, tickers, tier, lang, "
        "       threat_level, threat_cat, threat_conf, source_flag "
        "FROM news_articles WHERE saved = 1 ORDER BY sort_ts DESC",
        {},
        map_row);
}

// ── search_fts ────────────────────────────────────────────────────────────────

Result<QVector<fincept::services::NewsArticle>> NewsArticleRepository::search_fts(
    const QString& query, int64_t since_ts, int limit) const {

    // Escape FTS special characters to prevent query parse errors
    QString safe = query;
    safe.replace('"', ' ');

    // Try FTS5 path first
    const QString fts_sql =
        since_ts > 0
        ? "SELECT a.id, a.headline, a.summary, a.source, a.region, a.category, a.link, a.sort_ts,"
          "       a.priority, a.sentiment, a.impact, a.tickers, a.tier, a.lang,"
          "       a.threat_level, a.threat_cat, a.threat_conf, a.source_flag"
          " FROM news_fts f"
          " JOIN news_articles a ON a.id = f.id"
          " WHERE news_fts MATCH ? AND a.sort_ts >= ?"
          " ORDER BY rank LIMIT ?"
        : "SELECT a.id, a.headline, a.summary, a.source, a.region, a.category, a.link, a.sort_ts,"
          "       a.priority, a.sentiment, a.impact, a.tickers, a.tier, a.lang,"
          "       a.threat_level, a.threat_cat, a.threat_conf, a.source_flag"
          " FROM news_fts f"
          " JOIN news_articles a ON a.id = f.id"
          " WHERE news_fts MATCH ?"
          " ORDER BY rank LIMIT ?";

    QVariantList params;
    params << safe;
    if (since_ts > 0)
        params << static_cast<qint64>(since_ts);
    params << limit;

    auto r = db().execute(fts_sql, params);
    if (r.is_ok()) {
        QVector<fincept::services::NewsArticle> result;
        auto& q = r.value();
        while (q.next())
            result.append(map_row(q));
        return Result<QVector<fincept::services::NewsArticle>>::ok(std::move(result));
    }

    // Fallback: LIKE scan (FTS table may not exist on old DB)
    LOG_WARN("NewsArticleRepo", "FTS5 unavailable, falling back to LIKE scan");
    const QString like = "%" + query + "%";
    const QString like_sql =
        since_ts > 0
        ? "SELECT id, headline, summary, source, region, category, link, sort_ts,"
          "       priority, sentiment, impact, tickers, tier, lang,"
          "       threat_level, threat_cat, threat_conf, source_flag"
          " FROM news_articles"
          " WHERE (headline LIKE ? OR summary LIKE ?) AND sort_ts >= ?"
          " ORDER BY sort_ts DESC LIMIT ?"
        : "SELECT id, headline, summary, source, region, category, link, sort_ts,"
          "       priority, sentiment, impact, tickers, tier, lang,"
          "       threat_level, threat_cat, threat_conf, source_flag"
          " FROM news_articles"
          " WHERE headline LIKE ? OR summary LIKE ?"
          " ORDER BY sort_ts DESC LIMIT ?";

    QVariantList like_params;
    like_params << like << like;
    if (since_ts > 0)
        like_params << static_cast<qint64>(since_ts);
    like_params << limit;

    return query_list(like_sql, like_params, map_row);
}

// ── load_seen_ids ─────────────────────────────────────────────────────────────

Result<QSet<QString>> NewsArticleRepository::load_seen_ids(int64_t since_ts) const {
    auto r = db().execute(
        since_ts > 0
            ? "SELECT id FROM news_articles WHERE seen_at IS NOT NULL AND sort_ts >= ?"
            : "SELECT id FROM news_articles WHERE seen_at IS NOT NULL",
        since_ts > 0 ? QVariantList{static_cast<qint64>(since_ts)} : QVariantList{});
    if (r.is_err())
        return Result<QSet<QString>>::err(r.error());
    QSet<QString> ids;
    auto& q = r.value();
    while (q.next())
        ids.insert(q.value(0).toString());
    return Result<QSet<QString>>::ok(std::move(ids));
}

} // namespace fincept
