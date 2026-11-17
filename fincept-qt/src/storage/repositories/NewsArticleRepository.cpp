#include "storage/repositories/NewsArticleRepository.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>
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
        case Priority::FLASH:
            return "FLASH";
        case Priority::URGENT:
            return "URGENT";
        case Priority::BREAKING:
            return "BREAKING";
        default:
            return "ROUTINE";
    }
}

static Priority priority_from(const QString& s) {
    if (s == "FLASH")
        return Priority::FLASH;
    if (s == "URGENT")
        return Priority::URGENT;
    if (s == "BREAKING")
        return Priority::BREAKING;
    return Priority::ROUTINE;
}

static QString sentiment_str(Sentiment s) {
    switch (s) {
        case Sentiment::BULLISH:
            return "BULLISH";
        case Sentiment::BEARISH:
            return "BEARISH";
        default:
            return "NEUTRAL";
    }
}

static Sentiment sentiment_from(const QString& s) {
    if (s == "BULLISH")
        return Sentiment::BULLISH;
    if (s == "BEARISH")
        return Sentiment::BEARISH;
    return Sentiment::NEUTRAL;
}

static QString impact_str(Impact i) {
    switch (i) {
        case Impact::HIGH:
            return "HIGH";
        case Impact::MEDIUM:
            return "MEDIUM";
        default:
            return "LOW";
    }
}

static Impact impact_from(const QString& s) {
    if (s == "HIGH")
        return Impact::HIGH;
    if (s == "MEDIUM")
        return Impact::MEDIUM;
    return Impact::LOW;
}

static QString threat_level_str(ThreatLevel l) {
    switch (l) {
        case ThreatLevel::CRITICAL:
            return "CRITICAL";
        case ThreatLevel::HIGH:
            return "HIGH";
        case ThreatLevel::MEDIUM:
            return "MEDIUM";
        case ThreatLevel::LOW:
            return "LOW";
        default:
            return "INFO";
    }
}

static ThreatLevel threat_level_from(const QString& s) {
    if (s == "CRITICAL")
        return ThreatLevel::CRITICAL;
    if (s == "HIGH")
        return ThreatLevel::HIGH;
    if (s == "MEDIUM")
        return ThreatLevel::MEDIUM;
    if (s == "LOW")
        return ThreatLevel::LOW;
    return ThreatLevel::INFO;
}

// ── Row mapper ───────────────────────────────────────────────────────────────

NewsArticle NewsArticleRepository::map_row(QSqlQuery& q) {
    NewsArticle a;
    a.id = q.value(0).toString();
    a.headline = q.value(1).toString();
    a.summary = q.value(2).toString();
    a.source = q.value(3).toString();
    a.region = q.value(4).toString();
    a.category = q.value(5).toString();
    a.link = q.value(6).toString();
    a.sort_ts = q.value(7).toLongLong();
    a.priority = priority_from(q.value(8).toString());
    a.sentiment = sentiment_from(q.value(9).toString());
    a.impact = impact_from(q.value(10).toString());
    a.tier = q.value(12).toInt();
    a.lang = q.value(13).toString();
    a.threat.level = threat_level_from(q.value(14).toString());
    a.threat.category = q.value(15).toString();
    a.threat.confidence = q.value(16).toDouble();
    a.source_flag = static_cast<SourceFlag>(q.value(17).toInt());

    // tickers: stored as JSON array string
    const QString tickers_json = q.value(11).toString();
    if (!tickers_json.isEmpty()) {
        const QJsonArray arr = QJsonDocument::fromJson(tickers_json.toUtf8()).array();
        for (const auto& v : arr)
            a.tickers << v.toString();
    }
    // Rows read without the v058 columns: tone unknown (sentiment_scored stays
    // false), so a stored BULLISH label is never averaged in as +0.5.
    return a;
}

NewsArticle NewsArticleRepository::map_row_ext(QSqlQuery& q) {
    NewsArticle a = map_row(q);
    const QVariant score = q.value(18);
    if (!score.isNull()) {
        a.sentiment_score = score.toDouble();
        a.sentiment_scored = true;
    }
    a.event_type = q.value(19).toString();
    const QString conf_json = q.value(20).toString();
    if (!conf_json.isEmpty()) {
        const QJsonArray arr = QJsonDocument::fromJson(conf_json.toUtf8()).array();
        for (const auto& v : arr)
            a.ticker_confidence.append(v.toDouble());
        if (a.ticker_confidence.size() != a.tickers.size())
            a.ticker_confidence.clear(); // inconsistent: treat as unknown
    }
    const QString unv_json = q.value(21).toString();
    if (!unv_json.isEmpty()) {
        for (const auto& v : QJsonDocument::fromJson(unv_json.toUtf8()).array())
            a.unverified_tickers << v.toString();
    }
    return a;
}

bool NewsArticleRepository::has_analytics_columns() const {
    const int known = analytics_columns_.load(std::memory_order_relaxed);
    if (known >= 0)
        return known == 1;
    bool present = false;
    auto info = db().execute("PRAGMA table_info(news_articles)", {});
    if (info.is_ok()) {
        auto& q = info.value();
        while (q.next()) {
            if (q.value(1).toString() == QLatin1String("ticker_conf")) {
                present = true;
                break;
            }
        }
    }
    if (present) // a later migration run can still add them — only cache "yes"
        analytics_columns_.store(1, std::memory_order_relaxed);
    return present;
}

QString NewsArticleRepository::select_columns(const QString& alias) const {
    static const char* const base_cols[] = {"id",          "headline",     "summary",     "source",    "region",
                                            "category",    "link",         "sort_ts",     "priority",  "sentiment",
                                            "impact",      "tickers",      "tier",        "lang",      "threat_level",
                                            "threat_cat",  "threat_conf",  "source_flag"};
    static const char* const ext_cols[] = {"sentiment_score", "event_type", "ticker_conf", "unverified_tickers"};
    const QString prefix = alias.isEmpty() ? QString() : alias + QLatin1Char('.');
    QStringList cols;
    for (const char* c : base_cols)
        cols << prefix + QLatin1String(c);
    if (has_analytics_columns())
        for (const char* c : ext_cols)
            cols << prefix + QLatin1String(c);
    return cols.join(QStringLiteral(", "));
}

// ── upsert_batch ─────────────────────────────────────────────────────────────

namespace {
QString nar_json_strings(const QStringList& list) {
    QJsonArray arr;
    for (const auto& s : list)
        arr.append(s);
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
QString nar_json_numbers(const QVector<double>& list) {
    QJsonArray arr;
    for (double d : list)
        arr.append(std::round(d * 100.0) / 100.0);
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
} // namespace

Result<void> NewsArticleRepository::upsert_batch(const QVector<NewsArticle>& articles) {
    if (articles.isEmpty())
        return Result<void>::ok();

    const bool ext = has_analytics_columns();
    auto& database = db();
    auto bt = database.begin_transaction();
    if (bt.is_err())
        return bt;

    // The first-seen copy of an article is kept (seen / saved state lives on that
    // row). Rows stored before validated tagging (ticker_conf '') are re-derived
    // once from the fresh copy: tone, tags, language, impact and threat — the
    // old guesser's tickers and the all-"en" language stamp are replaced. The
    // WHERE keeps every other conflict a no-op, so the FTS update trigger does
    // not fire on each refresh.
    const QString sql =
        ext ? QStringLiteral(
                  "INSERT INTO news_articles "
                  "(id, headline, summary, source, region, category, link, sort_ts, "
                  " priority, sentiment, impact, tickers, tier, lang, "
                  " threat_level, threat_cat, threat_conf, source_flag, "
                  " sentiment_score, event_type, ticker_conf, unverified_tickers) "
                  "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
                  "ON CONFLICT(id) DO UPDATE SET "
                  " sentiment = excluded.sentiment, sentiment_score = excluded.sentiment_score, "
                  " impact = excluded.impact, tickers = excluded.tickers, lang = excluded.lang, "
                  " threat_level = excluded.threat_level, threat_cat = excluded.threat_cat, "
                  " threat_conf = excluded.threat_conf, event_type = excluded.event_type, "
                  " ticker_conf = excluded.ticker_conf, unverified_tickers = excluded.unverified_tickers "
                  "WHERE COALESCE(news_articles.ticker_conf, '') = ''")
            : QStringLiteral("INSERT OR IGNORE INTO news_articles "
                             "(id, headline, summary, source, region, category, link, sort_ts, "
                             " priority, sentiment, impact, tickers, tier, lang, "
                             " threat_level, threat_cat, threat_conf, source_flag) "
                             "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");

    int written = 0;
    for (const auto& a : articles) {
        QVariantList params{a.id,
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
                            nar_json_strings(a.tickers),
                            a.tier,
                            a.lang,
                            threat_level_str(a.threat.level),
                            a.threat.category,
                            a.threat.confidence,
                            static_cast<int>(a.source_flag)};
        if (ext) {
            params << (a.sentiment_scored ? QVariant(a.sentiment_score) : QVariant(QMetaType(QMetaType::Double)))
                   << a.event_type
                   // Always a JSON array (at least "[]"): non-empty marks the row as tagged.
                   << nar_json_numbers(a.ticker_confidence.size() == a.tickers.size() ? a.ticker_confidence
                                                                                      : QVector<double>(a.tickers.size(), 0.0))
                   << nar_json_strings(a.unverified_tickers);
        }
        auto r = database.execute(sql, params);
        if (r.is_ok())
            ++written;
    }

    auto ct = database.commit();
    if (ct.is_err()) {
        database.rollback();
        return ct;
    }

    LOG_DEBUG("NewsArticleRepo", QString("upsert_batch: %1 / %2 written").arg(written).arg(articles.size()));
    return Result<void>::ok();
}

// ── load_recent ──────────────────────────────────────────────────────────────

Result<QVector<NewsArticle>> NewsArticleRepository::load_recent(int64_t since_ts, const QString& category,
                                                                int limit) const {
    const bool ext = has_analytics_columns();
    const QString cols = select_columns();
    if (category.isEmpty()) {
        return query_list(QStringLiteral("SELECT %1 FROM news_articles WHERE sort_ts >= ? ORDER BY sort_ts DESC LIMIT ?")
                              .arg(cols),
                          {static_cast<qint64>(since_ts), limit}, ext ? map_row_ext : map_row);
    }
    return query_list(
        QStringLiteral("SELECT %1 FROM news_articles WHERE sort_ts >= ? AND category = ? ORDER BY sort_ts DESC LIMIT ?")
            .arg(cols),
        {static_cast<qint64>(since_ts), category, limit}, ext ? map_row_ext : map_row);
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
    // Bookmarked ("saved") articles are the user's own collection — retention must
    // not delete them. The `saved` column is added at runtime by ensure_saved_column()
    // (there is no migration), so prune can run before it exists: ask the schema
    // rather than assuming, and fall back to the plain delete (no column = nothing
    // can have been bookmarked yet).
    bool has_saved_column = false;
    { // scoped: the PRAGMA statement must be finished before the DELETE runs
        auto info = db().execute("PRAGMA table_info(news_articles)", {});
        if (info.is_ok()) {
            auto& q = info.value();
            while (q.next()) {
                if (q.value(1).toString() == QLatin1String("saved")) {
                    has_saved_column = true;
                    break;
                }
            }
        }
    }
    if (has_saved_column) {
        return exec_write("DELETE FROM news_articles WHERE sort_ts < ? AND COALESCE(saved, 0) = 0",
                          {static_cast<qint64>(cutoff_ts)});
    }
    return exec_write("DELETE FROM news_articles WHERE sort_ts < ?", {static_cast<qint64>(cutoff_ts)});
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
    return exec_write("UPDATE news_articles SET seen_at = ? WHERE id = ? AND seen_at IS NULL",
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
    auto wr = exec_write("UPDATE news_articles SET saved = ? WHERE id = ?", {new_state ? 1 : 0, id});
    if (wr.is_err())
        return Result<bool>::err(wr.error());
    return Result<bool>::ok(new_state);
}

// ── load_saved ────────────────────────────────────────────────────────────────

Result<QVector<fincept::services::NewsArticle>> NewsArticleRepository::load_saved(int limit) const {
    return query_list(
        QStringLiteral("SELECT %1 FROM news_articles WHERE saved = 1 ORDER BY sort_ts DESC LIMIT ?").arg(select_columns()),
        {limit}, has_analytics_columns() ? map_row_ext : map_row);
}

Result<bool> NewsArticleRepository::is_saved(const QString& id) const {
    auto r = db().execute("SELECT saved FROM news_articles WHERE id = ?", {id});
    if (r.is_err())
        return Result<bool>::err(r.error());
    auto& q = r.value();
    return Result<bool>::ok(q.next() && q.value(0).toInt() != 0);
}

int NewsArticleRepository::saved_count() const {
    auto r = db().execute("SELECT COUNT(*) FROM news_articles WHERE saved = 1", {});
    if (r.is_err())
        return 0;
    auto& q = r.value();
    return q.next() ? q.value(0).toInt() : 0;
}

Result<QVector<NewsVolumeRow>> NewsArticleRepository::hourly_counts(int64_t from, int64_t to) const {
    const bool ext = has_analytics_columns();
    const QString sql =
        ext ? QStringLiteral("SELECT (sort_ts / 3600) * 3600 AS h, COALESCE(category, ''), COALESCE(region, ''), "
                             "COALESCE(lang, ''), COUNT(*), SUM(CASE WHEN sentiment_score IS NULL THEN 0 ELSE 1 END), "
                             "SUM(COALESCE(sentiment_score, 0)) FROM news_articles WHERE sort_ts >= ? AND sort_ts < ? "
                             "GROUP BY h, 2, 3, 4")
            : QStringLiteral("SELECT (sort_ts / 3600) * 3600 AS h, COALESCE(category, ''), COALESCE(region, ''), "
                             "COALESCE(lang, ''), COUNT(*), 0, 0 FROM news_articles WHERE sort_ts >= ? AND sort_ts < ? "
                             "GROUP BY h, 2, 3, 4");
    auto r = db().execute(sql, {static_cast<qint64>(from), static_cast<qint64>(to)});
    if (r.is_err())
        return Result<QVector<NewsVolumeRow>>::err(r.error());
    // Fold the (hour, category, region, lang) groups into one row per dimension.
    QHash<QString, NewsVolumeRow> rows;
    auto add = [&rows](qint64 h, const QString& dim, const QString& key, int n, int scored, double sum) {
        const QString k = QString::number(h) + QLatin1Char('|') + dim + QLatin1Char('|') + key;
        auto& row = rows[k];
        row.hour_ts = h;
        row.dim = dim;
        row.key = key;
        row.count += n;
        row.scored += scored;
        row.score_sum += sum;
    };
    auto& q = r.value();
    while (q.next()) {
        const qint64 h = q.value(0).toLongLong();
        const int n = q.value(4).toInt();
        const int scored = q.value(5).toInt();
        const double sum = q.value(6).toDouble();
        add(h, QStringLiteral("all"), QString(), n, scored, sum);
        add(h, QStringLiteral("cat"), q.value(1).toString(), n, scored, sum);
        add(h, QStringLiteral("region"), q.value(2).toString(), n, scored, sum);
        add(h, QStringLiteral("lang"), q.value(3).toString(), n, scored, sum);
    }
    QVector<NewsVolumeRow> out;
    out.reserve(rows.size());
    for (auto it = rows.cbegin(); it != rows.cend(); ++it)
        out.append(it.value());
    return Result<QVector<NewsVolumeRow>>::ok(std::move(out));
}

// ── save_analysis / load_analysis ─────────────────────────────────────────────

Result<void> NewsArticleRepository::save_analysis(const QString& url, const QString& analysis_json) const {
    return exec_write("INSERT OR REPLACE INTO news_analysis (url, analysis_json, created_at) "
                      "VALUES (?, ?, strftime('%s','now'))",
                      {url, analysis_json});
}

Result<QString> NewsArticleRepository::load_analysis(const QString& url) const {
    auto r = db().execute("SELECT analysis_json FROM news_analysis WHERE url = ?", {url});
    if (r.is_err())
        return Result<QString>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<QString>::ok({}); // none stored — not an error
    return Result<QString>::ok(q.value(0).toString());
}

// ── search_fts ────────────────────────────────────────────────────────────────

Result<QVector<fincept::services::NewsArticle>> NewsArticleRepository::search_fts(const QString& query,
                                                                                  int64_t since_ts, int limit) const {

    // Escape FTS special characters to prevent query parse errors
    QString safe = query;
    safe.replace('"', ' ');

    const bool ext = has_analytics_columns();
    const auto mapper = ext ? map_row_ext : map_row;
    // Try FTS5 path first
    const QString fts_sql =
        since_ts > 0 ? QStringLiteral("SELECT %1 FROM news_fts f JOIN news_articles a ON a.id = f.id"
                                      " WHERE news_fts MATCH ? AND a.sort_ts >= ? ORDER BY rank LIMIT ?")
                           .arg(select_columns(QStringLiteral("a")))
                     : QStringLiteral("SELECT %1 FROM news_fts f JOIN news_articles a ON a.id = f.id"
                                      " WHERE news_fts MATCH ? ORDER BY rank LIMIT ?")
                           .arg(select_columns(QStringLiteral("a")));

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
            result.append(mapper(q));
        return Result<QVector<fincept::services::NewsArticle>>::ok(std::move(result));
    }

    // Fallback: LIKE scan (FTS table may not exist on old DB)
    LOG_WARN("NewsArticleRepo", "FTS5 unavailable, falling back to LIKE scan");
    const QString like = "%" + query + "%";
    const QString like_sql =
        since_ts > 0 ? QStringLiteral("SELECT %1 FROM news_articles WHERE (headline LIKE ? OR summary LIKE ?) "
                                      "AND sort_ts >= ? ORDER BY sort_ts DESC LIMIT ?")
                           .arg(select_columns())
                     : QStringLiteral("SELECT %1 FROM news_articles WHERE headline LIKE ? OR summary LIKE ? "
                                      "ORDER BY sort_ts DESC LIMIT ?")
                           .arg(select_columns());

    QVariantList like_params;
    like_params << like << like;
    if (since_ts > 0)
        like_params << static_cast<qint64>(since_ts);
    like_params << limit;

    return query_list(like_sql, like_params, mapper);
}

// ── load_seen_ids ─────────────────────────────────────────────────────────────

Result<QSet<QString>> NewsArticleRepository::load_seen_ids(int64_t since_ts, int limit) const {
    // ORDER BY before LIMIT so the cap keeps the *newest* seen IDs — those are
    // the ones the feed model can still be showing.
    auto r = db().execute(since_ts > 0 ? "SELECT id FROM news_articles WHERE seen_at IS NOT NULL AND sort_ts >= ? "
                                         "ORDER BY sort_ts DESC LIMIT ?"
                                       : "SELECT id FROM news_articles WHERE seen_at IS NOT NULL "
                                         "ORDER BY sort_ts DESC LIMIT ?",
                          since_ts > 0 ? QVariantList{static_cast<qint64>(since_ts), limit} : QVariantList{limit});
    if (r.is_err())
        return Result<QSet<QString>>::err(r.error());
    QSet<QString> ids;
    auto& q = r.value();
    while (q.next())
        ids.insert(q.value(0).toString());
    return Result<QSet<QString>>::ok(std::move(ids));
}

} // namespace fincept
