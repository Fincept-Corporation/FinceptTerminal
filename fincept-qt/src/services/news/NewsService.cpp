// src/services/news/NewsService.cpp
//
// Core lifecycle: singleton + cache + fetch_all_news (cached/eager) +
// fetch_all_news_progressive (streaming) + analyze_article + summarize_headlines
// + auto-refresh + DataHub publish surface.
// The non-core concerns live in:
//   - NewsService_LiveFeed.cpp       — WebSocket live feed lifecycle
//   - NewsService_Parsing.cpp        — RSS/Atom parsing + article enrichment
//   - NewsService_Classification.cpp — threat classification + source flags
//   - NewsService_Feeds.cpp          — static catalog of default RSS feeds
#include "services/news/NewsService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "network/http/HttpClient.h"
#include "services/news/NewsFeedParse.h"
#include "services/news/NewsSymbolDirectory.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/NewsAnalyticsRepository.h"
#include "storage/repositories/NewsArticleRepository.h"

#include <QAtomicInt>
#include <QDateTime>
#include <QJsonDocument>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QUuid>
#include <QXmlStreamReader>
#include <QtConcurrent>

#ifdef HAS_QT_WEBSOCKETS
#    include <QtWebSockets/QWebSocket>
#endif

#include <algorithm>
#include <atomic>
#include <memory>

namespace fincept::services {

// HTTP timing — used by the RSS fetch paths. The transfer timeout is "no byte
// for this long" (it restarts on progress); 4 s used to cancel DNS + TLS on an
// ordinary link and every slow publisher (UN, BoJ, Hacker News: "Operation
// canceled"), which then went into back-off — leaving the few fastest feeds
// (Bloomberg) as nearly the only source. The deadline caps a slow drip.
static constexpr int kFeedTransferTimeoutMs = 15000;
static constexpr int kFeedDeadlineMs = 30000;

namespace news_svc_detail {
// Newest "news:articles" cache write handed to a worker, plus the lock that
// makes "still the newest? then write" atomic — so an older list whose worker
// finishes late can never overwrite a newer one.
QMutex g_cache_write_mutex;
std::atomic<quint64> g_cache_write_generation{0};
} // namespace news_svc_detail

// ── Cache (de)serialisation + merge helpers ─────────────────────────────────

QString NewsService::serialize_articles(const QVector<NewsArticle>& articles) {
    QJsonArray arr;
    for (const auto& a : articles) {
        QJsonObject o;
        o["id"] = a.id;
        o["time"] = a.time;
        o["headline"] = a.headline;
        o["summary"] = a.summary;
        o["source"] = a.source;
        o["region"] = a.region;
        o["category"] = a.category;
        o["link"] = a.link;
        o["sort_ts"] = static_cast<qint64>(a.sort_ts);
        o["tier"] = a.tier;
        o["priority"] = priority_string(a.priority);
        o["sentiment"] = sentiment_string(a.sentiment);
        o["sentiment_score"] = a.sentiment_score;
        o["impact"] = impact_string(a.impact);
        o["lang"] = a.lang;
        QJsonArray tickers;
        for (const auto& t : a.tickers)
            tickers.append(t);
        o["tickers"] = tickers;
        o["sentiment_scored"] = a.sentiment_scored;
        if (!a.event_type.isEmpty())
            o["event_type"] = a.event_type;
        if (!a.ticker_confidence.isEmpty()) {
            QJsonArray conf;
            for (double c : a.ticker_confidence)
                conf.append(c);
            o["ticker_conf"] = conf;
        }
        if (!a.unverified_tickers.isEmpty())
            o["unverified"] = QJsonArray::fromStringList(a.unverified_tickers);
        if (a.undated)
            o["undated"] = true;
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<NewsArticle> NewsService::deserialize_articles(const QString& json) {
    const QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
    QVector<NewsArticle> articles;
    articles.reserve(arr.size());
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        NewsArticle a;
        a.id = o["id"].toString();
        a.time = o["time"].toString();
        a.headline = o["headline"].toString();
        a.summary = o["summary"].toString();
        a.source = o["source"].toString();
        a.region = o["region"].toString();
        a.category = o["category"].toString();
        a.link = o["link"].toString();
        a.sort_ts = o["sort_ts"].toVariant().toLongLong();
        a.tier = o["tier"].toInt(4);
        a.priority = priority_from_string(o["priority"].toString());
        a.sentiment = sentiment_from_string(o["sentiment"].toString());
        a.sentiment_score = o["sentiment_score"].toDouble();
        a.impact = impact_from_string(o["impact"].toString());
        a.lang = o["lang"].toString();
        for (const auto& t : o["tickers"].toArray())
            a.tickers << t.toString();
        // Entries cached by an older build carry no flag; those builds scored
        // every article, so a present score counts as measured.
        a.sentiment_scored = o.contains("sentiment_scored") ? o["sentiment_scored"].toBool()
                                                            : o.contains("sentiment_score");
        a.event_type = o["event_type"].toString();
        for (const auto& c : o["ticker_conf"].toArray())
            a.ticker_confidence.append(c.toDouble());
        if (a.ticker_confidence.size() != a.tickers.size())
            a.ticker_confidence.clear();
        for (const auto& u : o["unverified"].toArray())
            a.unverified_tickers << u.toString();
        a.undated = o["undated"].toBool(false);
        // Threat + source flag are pure functions of the fields above, so they
        // are recomputed here rather than bloating the cache. Without this a
        // cache-served list lost every threat badge and credibility flag.
        // Priority / impact / event too, so a cache written by an older rule
        // set does not bring its labels back.
        classify_article(a);
        a.source_flag = source_flag_for(a.source);
        articles.append(std::move(a));
    }
    return articles;
}

void NewsService::merge_unique(QVector<NewsArticle>& all, QSet<QString>& seen_ids,
                               const QVector<NewsArticle>& incoming) {
    for (const auto& a : incoming) {
        if (a.id.isEmpty() || !seen_ids.contains(a.id)) {
            seen_ids.insert(a.id);
            all.append(a);
        }
    }
}

void NewsService::persist_articles_async(const QVector<NewsArticle>& articles) {
    if (articles.isEmpty())
        return;
    // Single-transaction upsert with FTS triggers — keep it off the UI thread.
    // Database hands each worker its own SQLite connection, and ids are stable,
    // so repeat refreshes only insert genuinely new articles. The hourly volume
    // history is written right after, from the stored articles, in the same task
    // (so it sees this batch).
    (void)QtConcurrent::run([articles]() {
        auto r = fincept::NewsArticleRepository::instance().upsert_batch(articles);
        if (r.is_err())
            LOG_WARN("NewsService", "Persisting articles failed: " + QString::fromStdString(r.error()));
        record_volume_history();
    });
}

void NewsService::record_volume_history() {
    // Every complete hour of the last six is "observed" by this fetch: its counts
    // (from all stored articles, which keep accumulating across refreshes) are
    // upserted, keeping the larger count. An observed hour with no articles gets
    // an explicit zero, so quiet hours count in the baseline.
    auto& analytics = fincept::NewsAnalyticsRepository::instance();
    if (!analytics.available())
        return;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 to = now - (now % 3600); // start of the current (partial) hour
    const qint64 from = to - 6 * 3600;
    auto counts = fincept::NewsArticleRepository::instance().hourly_counts(from, to);
    if (counts.is_err())
        return;
    QVector<fincept::NewsVolumeRow> rows = counts.value();
    QSet<qint64> hours_with_rows;
    for (const auto& r : rows)
        if (r.dim == QLatin1String("all"))
            hours_with_rows.insert(r.hour_ts);
    for (qint64 h = from; h < to; h += 3600)
        if (!hours_with_rows.contains(h))
            rows.append({h, QStringLiteral("all"), QString(), 0, 0, 0.0});
    auto w = analytics.record_volume(rows);
    if (w.is_err())
        LOG_WARN("NewsService", "Recording volume history failed: " + QString::fromStdString(w.error()));
    // Keep ~5 weeks of hourly history (the baselines read 7 days, charts 30).
    if ((now / 3600) % 24 == 0)
        (void)analytics.prune_volume(now - 35LL * 86400);
}

void NewsService::store_cache_async(const QVector<NewsArticle>& articles) {
    // JSON-encoding thousands of articles and writing a multi-MB cache row used
    // to run on the GUI thread at the end of every refresh.
    const quint64 generation = ++news_svc_detail::g_cache_write_generation;
    (void)QtConcurrent::run([articles, generation]() {
        const QString json = serialize_articles(articles);
        QMutexLocker lock(&news_svc_detail::g_cache_write_mutex);
        if (generation != news_svc_detail::g_cache_write_generation.load())
            return; // a newer list was handed off meanwhile — it owns the entry
        fincept::CacheManager::instance().put("news:articles", QVariant(json), kArticleCacheTtlSec, "news");
    });
}

QVector<NewsArticle> NewsService::parse_feed_payload(const QByteArray& data, int http_code, bool ok,
                                                     const QString& error, const RSSFeed& feed, QString* problem) {
    QVector<NewsArticle> articles;
    // A dead feed fails on every refresh: logged when it starts failing (WARN, with
    // the reason), at DEBUG while it keeps failing, and once when it works again.
    const QString state_key = QStringLiteral("feed|") + feed.id;
    auto feed_failed = [&](const QString& reason) {
        if (fincept::Logger::instance().transition("NewsService", state_key, 1))
            LOG_WARN("NewsService", logf("Feed failed", {{"feed", feed.id},
                                                         {"source", feed.source},
                                                         {"status", http_code},
                                                         {"bytes", data.size()},
                                                         {"error", reason}}));
        else
            LOG_DEBUG("NewsService", logf("Feed still failing", {{"feed", feed.id}, {"error", reason}}));
    };
    if (ok) {
        // HTML error pages (Akamai access-denied, Cloudflare captcha) would
        // pass through the parser silently producing 0 articles — flag them.
        // A UTF-8 byte-order mark or whitespace before the first tag is fine
        // (the Federal Reserve, RBI and Der Standard feeds start with a BOM,
        // which an old "starts with '<'" check rejected as "no articles").
        if (news_feed::looks_like_html(data)) {
            feed_failed(QStringLiteral("returned HTML, likely access denied"));
            if (problem)
                *problem = QStringLiteral("returned a web page, not a feed");
        } else {
            QString why;
            articles = parse_rss_xml(data, feed, &why);
            if (articles.isEmpty()) {
                feed_failed(why.isEmpty() ? QStringLiteral("no parsed articles")
                                          : QStringLiteral("no parsed articles: ") + why);
                if (problem)
                    *problem = why.isEmpty() ? QStringLiteral("no items") : why;
            } else {
                fincept::LogTransition was;
                auto& logger = fincept::Logger::instance();
                if (logger.transition_possible(0) && logger.transition("NewsService", state_key, 0, &was))
                    LOG_INFO("NewsService", logf("Feed recovered", {{"feed", feed.id},
                                                                    {"failed_refreshes", was.observations_in_previous}}));
            }
        }
    } else {
        feed_failed(error);
    }
    return articles;
}

// Use a real, current browser User-Agent — major financial publishers reject
// "FinceptTerminal/4.0" as scraper traffic, and bot filters flag browser
// versions that are years old. Every built-in feed was verified with this one.
static constexpr const char* kBrowserUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                                 "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";

NewsService& NewsService::instance() {
    static NewsService s;
    return s;
}

NewsService::NewsService() {
    nam_ = new QNetworkAccessManager(this);
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(kArticleCacheTtlSec * 1000);
    connect(refresh_timer_, &QTimer::timeout, this,
            [this]() { fetch_all_news(true, [](bool, QVector<NewsArticle>) {}); });
}

// ── Feed requests: bounded concurrency, per-feed health and back-off ────────

struct NewsService::FeedQueue {
    QVector<RSSFeed> feeds;
    int next = 0;
    int in_flight = 0;
    std::function<void(const RSSFeed&, QVector<NewsArticle>)> on_done;
};

namespace {
/// Back-off after `failures` consecutive failures: none below three, then 15 min
/// doubling per further failure, capped at 6 h. Refreshes run every 10 min by
/// default, so a dead feed costs a request every few hours instead of every
/// refresh — and comes back on its own when it recovers.
qint64 news_feed_backoff_secs(int failures) {
    if (failures < 3)
        return 0;
    const int steps = std::min(failures - 3, 5);
    return std::min<qint64>(15LL * 60 * (1LL << steps), 6LL * 3600);
}
} // namespace

bool NewsService::feed_backing_off(const QString& feed_id, qint64 now) const {
    const auto it = feed_health_.constFind(feed_id);
    return it != feed_health_.constEnd() && it->retry_after > now;
}

QVector<FeedHealth> NewsService::feed_health() const {
    QVector<FeedHealth> out;
    out.reserve(feed_health_.size());
    for (auto it = feed_health_.cbegin(); it != feed_health_.cend(); ++it)
        out.append(it.value());
    return out;
}

int NewsService::feeds_backing_off() const {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    int n = 0;
    for (auto it = feed_health_.cbegin(); it != feed_health_.cend(); ++it)
        if (it->retry_after > now)
            ++n;
    return n;
}

void NewsService::reset_feed_health(const QString& feed_id) {
    auto it = feed_health_.find(feed_id);
    if (it == feed_health_.end())
        return;
    it->consecutive_failures = 0;
    it->retry_after = 0;
    feed_health_dirty_.insert(feed_id);
    persist_feed_health();
    emit feed_health_changed();
}

void NewsService::note_feed_result(const RSSFeed& feed, bool ok, int http_code, const QString& error,
                                   int article_count) {
    FeedHealth& h = feed_health_[feed.id];
    h.feed_id = feed.id;
    h.name = feed.name;
    h.region = feed.region;
    h.lang = feed.lang;
    h.last_http = http_code;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (ok) {
        h.last_ok_at = now;
        h.consecutive_failures = 0;
        h.retry_after = 0;
        h.last_count = article_count;
        h.last_error.clear();
    } else {
        h.last_fail_at = now;
        ++h.consecutive_failures;
        h.last_error = error;
        const qint64 backoff = news_feed_backoff_secs(h.consecutive_failures);
        h.retry_after = backoff > 0 ? now + backoff : 0;
    }
    feed_health_dirty_.insert(feed.id);
}

void NewsService::persist_feed_health() {
    if (feed_health_dirty_.isEmpty())
        return;
    QVector<fincept::NewsFeedHealthRow> rows;
    rows.reserve(feed_health_dirty_.size());
    for (const QString& id : std::as_const(feed_health_dirty_)) {
        const auto it = feed_health_.constFind(id);
        if (it == feed_health_.constEnd())
            continue;
        rows.append({it->feed_id, it->last_ok_at, it->last_fail_at, it->consecutive_failures, it->last_error,
                     it->last_http, it->last_count});
    }
    feed_health_dirty_.clear();
    (void)QtConcurrent::run([rows]() {
        auto r = fincept::NewsAnalyticsRepository::instance().save_feed_health(rows);
        if (r.is_err())
            LOG_WARN("NewsService", "Saving feed health failed: " + QString::fromStdString(r.error()));
    });
}

void NewsService::load_feed_health_once() {
    if (feed_health_loaded_ || feed_health_loading_)
        return;
    feed_health_loading_ = true;
    // The symbol directory reads watchlists / holdings / learned names for the
    // tagger — start it with the first fetch (no-op cost after that).
    NewsSymbolDirectory::instance().request_reload();
    QPointer<NewsService> self = this;
    (void)QtConcurrent::run([self]() {
        auto r = fincept::NewsAnalyticsRepository::instance().load_feed_health();
        QVector<fincept::NewsFeedHealthRow> rows;
        if (r.is_ok())
            rows = r.value();
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, rows]() {
                if (!self)
                    return;
                self->feed_health_loading_ = false;
                self->feed_health_loaded_ = true;
                // Failures recorded before the fetch pipeline was fixed (4 s
                // timeout, BOM feeds read as empty, moved URLs) say nothing about
                // the feeds now: the first run of this build starts every feed
                // with a clean slate instead of honouring hours of back-off.
                QSettings settings;
                const QString reset_key = QStringLiteral("news/feed_health_reset_v2");
                const bool fresh_start = !settings.value(reset_key, false).toBool();
                if (fresh_start)
                    settings.setValue(reset_key, true);
                for (const auto& row : rows) {
                    if (self->feed_health_.contains(row.feed_id))
                        continue; // this session already knows better
                    FeedHealth h;
                    h.feed_id = row.feed_id;
                    h.last_ok_at = row.last_ok_at;
                    h.last_fail_at = row.last_fail_at;
                    h.consecutive_failures = fresh_start ? 0 : row.consecutive_failures;
                    h.last_error = row.last_error;
                    h.last_http = row.last_http;
                    h.last_count = row.last_count;
                    const qint64 backoff = news_feed_backoff_secs(h.consecutive_failures);
                    h.retry_after = backoff > 0 && h.last_fail_at > 0 ? h.last_fail_at + backoff : 0;
                    self->feed_health_.insert(row.feed_id, h);
                }
                emit self->feed_health_changed();
            },
            Qt::QueuedConnection);
    });
}

void NewsService::run_feed_requests(const QVector<RSSFeed>& feeds,
                                    std::function<void(const RSSFeed&, QVector<NewsArticle>)> on_done) {
    load_feed_health_once();
    auto queue = std::make_shared<FeedQueue>();
    queue->feeds = feeds;
    // Feeds that answered last time first, failing ones last: the list fills
    // from the reliable publishers while the slow / broken ones are still out.
    // Within each group the regions take turns, so the first wave of requests
    // is not one block of the catalogue (it used to be the US wires — Bloomberg,
    // CNBC, MarketWatch — that alone filled the screen for the first seconds).
    const auto healthy = [this](const RSSFeed& f) {
        const auto it = feed_health_.constFind(f.id);
        return it == feed_health_.constEnd() || it->consecutive_failures == 0;
    };
    const auto by_region_turns = [](const QVector<RSSFeed>& in) {
        QStringList order;
        QHash<QString, QVector<RSSFeed>> groups;
        for (const auto& f : in) {
            if (!groups.contains(f.region))
                order.append(f.region);
            groups[f.region].append(f);
        }
        QVector<RSSFeed> out;
        out.reserve(in.size());
        for (int round = 0; out.size() < in.size(); ++round)
            for (const QString& region : std::as_const(order)) {
                const auto& g = groups[region];
                if (round < g.size())
                    out.append(g[round]);
            }
        return out;
    };
    QVector<RSSFeed> good, failing;
    for (const auto& f : feeds)
        (healthy(f) ? good : failing).append(f);
    queue->feeds = by_region_turns(good) + by_region_turns(failing);
    queue->on_done = std::move(on_done);
    pump_feed_queue(queue);
}

void NewsService::pump_feed_queue(const std::shared_ptr<FeedQueue>& queue) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    while (queue->in_flight < kMaxConcurrentFeedRequests && queue->next < queue->feeds.size()) {
        const RSSFeed feed = queue->feeds.at(queue->next++);
        if (feed_backing_off(feed.id, now)) {
            // Not requested this round; still reported so progress completes.
            QMetaObject::invokeMethod(
                this, [queue, feed]() { queue->on_done(feed, {}); }, Qt::QueuedConnection);
            continue;
        }
        ++queue->in_flight;
        QNetworkRequest req(QUrl(feed.url));
        req.setHeader(QNetworkRequest::UserAgentHeader, kBrowserUserAgent);
        req.setRawHeader("Accept",
                         "application/rss+xml, application/atom+xml, application/xml;q=0.9, text/xml;q=0.9, */*;q=0.8");
        req.setRawHeader("Accept-Language", "en-US,en;q=0.9");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(kFeedTransferTimeoutMs);

        auto* reply = nam_->get(req);
        // Hard deadline on top of the stall timeout: a server that trickles a
        // byte every few seconds must not hold a request slot for minutes.
        QTimer::singleShot(kFeedDeadlineMs, reply, [reply]() {
            if (reply->isRunning())
                reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, feed, queue]() {
            reply->deleteLater();
            --queue->in_flight;
            // Only the reply bookkeeping stays on the GUI thread: parsing a feed
            // (XML + enrichment + tagging of every item) runs on a worker. The
            // parsed batch comes back here and is handed on in arrival order.
            const bool net_ok = reply->error() == QNetworkReply::NoError;
            const int http_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray data = net_ok ? reply->readAll() : QByteArray();
            const QString error = net_ok ? QString()
                                  : reply->error() == QNetworkReply::OperationCanceledError ||
                                            reply->error() == QNetworkReply::TimeoutError
                                      ? QStringLiteral("timed out")
                                      : reply->errorString();
            pump_feed_queue(queue); // the slot is free — start the next feed now
            QPointer<NewsService> self = this;
            (void)QtConcurrent::run([self, data, http_code, net_ok, error, feed, queue]() {
                QString problem;
                QVector<NewsArticle> parsed = parse_feed_payload(data, http_code, net_ok, error, feed, &problem);
                if (!self)
                    return;
                QMetaObject::invokeMethod(
                    self,
                    [self, feed, queue, http_code, net_ok, error, problem, batch = std::move(parsed)]() mutable {
                        if (!self)
                            return;
                        QString why;
                        if (!net_ok)
                            why = http_code > 0 ? QStringLiteral("HTTP %1").arg(http_code) : error;
                        else if (batch.isEmpty())
                            why = problem.isEmpty() ? QStringLiteral("no items") : problem;
                        self->note_feed_result(feed, why.isEmpty(), http_code, why, static_cast<int>(batch.size()));
                        queue->on_done(feed, std::move(batch));
                    },
                    Qt::QueuedConnection);
            });
        });
    }
}

// ── Fetch all RSS feeds ──────────────────────────────────────────────────────
//
// One body for both entry points. Nothing that can block runs on the GUI
// thread: the cache read + JSON decode (+ threat re-classification of every
// cached article) and the feed-list query run on a worker; feed parsing runs
// on a worker per feed (pump_feed_queue). The GUI thread only merges each
// parsed batch into the list, which is kept in canonical order (newest first,
// ties by id — news_feed::newer_first) by an O(n) merge, and — for the
// progressive fetch — hands a snapshot to the screen at most every
// kPartialEmitIntervalMs, so 120 feeds answering do not mean 120 full-list
// copies, sorts, signal deliveries and hub fan-outs in a row.

struct NewsService::FetchState {
    QVector<NewsArticle> all; ///< canonical order
    QSet<QString> seen_ids;   ///< cross-feed dedup (same URL served by two feeds)
    int done = 0;
    int total = 0;
    bool progressive = false;
    bool flush_scheduled = false;
    qint64 last_emit_ms = 0;
    ArticlesCallback callback;
};

void NewsService::fetch_all_news(bool force, ArticlesCallback cb) {
    run_fetch(force, /*progressive=*/false, std::move(cb));
}

// Progressive fetch — emits partial snapshots as feeds arrive, so the screen
// fills from the first fast feeds instead of waiting for the slowest one.
void NewsService::fetch_all_news_progressive(bool force, ArticlesCallback final_cb) {
    run_fetch(force, /*progressive=*/true, std::move(final_cb));
}

void NewsService::run_fetch(bool force, bool progressive, ArticlesCallback cb) {
    if (force) {
        start_network_fetch(progressive, std::move(cb));
        return;
    }
    QPointer<NewsService> self = this;
    (void)QtConcurrent::run([self, progressive, cb = std::move(cb)]() mutable {
        QVector<NewsArticle> articles;
        if (const auto cached = fincept::CacheManager::instance().try_get(QStringLiteral("news:articles")))
            articles = deserialize_articles(*cached);
        news_feed::sort_newest_first(articles);
        const QVector<RSSFeed> feeds = articles.isEmpty() || !self ? QVector<RSSFeed>() : self->list_effective_feeds();
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, progressive, cb = std::move(cb), articles = std::move(articles), feeds]() mutable {
                if (!self)
                    return;
                if (articles.isEmpty()) { // nothing cached (or expired): go to the network
                    self->start_network_fetch(progressive, std::move(cb));
                    return;
                }
                // A cache-served list must still report how many feeds/sources it
                // came from (the intel strip reads them).
                self->effective_feeds_snapshot_ = feeds;
                self->feed_count_ = static_cast<int>(feeds.size());
                QSet<QString> sources;
                for (const auto& a : articles)
                    sources.insert(a.source);
                self->active_sources_ = sources.values();
                self->latest_articles_ = articles;
                cb(true, articles);
                if (progressive)
                    emit self->articles_partial(articles, self->feed_count_, self->feed_count_);
                self->publish_articles_to_hub(articles);
            },
            Qt::QueuedConnection);
    });
}

void NewsService::start_network_fetch(bool progressive, ArticlesCallback cb) {
    // The feed list comes from the database (rss_feeds overlay): read it on a
    // worker, then start the requests from the GUI thread (the NAM lives here).
    QPointer<NewsService> self = this;
    (void)QtConcurrent::run([self, progressive, cb = std::move(cb)]() mutable {
        if (!self)
            return;
        QVector<RSSFeed> feeds = self->list_effective_feeds();
        QMetaObject::invokeMethod(
            self,
            [self, progressive, cb = std::move(cb), feeds = std::move(feeds)]() mutable {
                if (!self)
                    return;
                self->effective_feeds_snapshot_ = feeds;
                self->feed_count_ = static_cast<int>(feeds.size());
                if (feeds.isEmpty()) {
                    LOG_INFO("NewsService", "No news feeds are enabled — nothing to fetch");
                    cb(true, {});
                    if (progressive)
                        emit self->articles_partial({}, 0, 0);
                    else
                        emit self->articles_updated({});
                    self->publish_articles_to_hub({});
                    return;
                }
                auto state = std::make_shared<FetchState>();
                state->total = static_cast<int>(feeds.size());
                state->progressive = progressive;
                state->callback = std::move(cb);
                QPointer<NewsService> guard = self;
                self->run_feed_requests(feeds, [guard, state](const RSSFeed&, QVector<NewsArticle> batch) {
                    if (!guard)
                        return;
                    QVector<NewsArticle> fresh;
                    fresh.reserve(batch.size());
                    for (auto& a : batch) {
                        if (!a.id.isEmpty() && state->seen_ids.contains(a.id))
                            continue;
                        state->seen_ids.insert(a.id);
                        fresh.append(std::move(a));
                    }
                    if (!fresh.isEmpty()) {
                        news_feed::sort_newest_first(fresh);
                        state->all = news_feed::merge_sorted_unique(state->all, fresh);
                    }
                    ++state->done;
                    if (state->done >= state->total) {
                        guard->finish_fetch(state);
                        return;
                    }
                    if (!state->progressive)
                        return;
                    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
                    const qint64 since = now_ms - state->last_emit_ms;
                    if (state->last_emit_ms == 0 || since >= kPartialEmitIntervalMs) {
                        guard->emit_partial(state);
                    } else if (!state->flush_scheduled) {
                        state->flush_scheduled = true;
                        QTimer::singleShot(static_cast<int>(kPartialEmitIntervalMs - since), guard.data(),
                                           [guard, state]() {
                                               state->flush_scheduled = false;
                                               if (guard && state->done < state->total)
                                                   guard->emit_partial(state);
                                           });
                    }
                });
            },
            Qt::QueuedConnection);
    });
}

void NewsService::emit_partial(const std::shared_ptr<FetchState>& state) {
    state->last_emit_ms = QDateTime::currentMSecsSinceEpoch();
    emit articles_partial(state->all, state->done, state->total);
    // Progressive publish — each chunk fans out the accumulated list. Hub's
    // per-topic coalescing (news:general at 250ms) throttles the repaints.
    publish_articles_to_hub(state->all);
}

void NewsService::finish_fetch(const std::shared_ptr<FetchState>& state) {
    const QVector<NewsArticle>& all = state->all; // already in canonical order
    if (state->progressive)
        emit articles_partial(all, state->done, state->total);

    QSet<QString> sources;
    for (const auto& a : all)
        sources.insert(a.source);
    active_sources_ = sources.values();

    // Only rewrite the on-disk cache when we actually fetched articles: with
    // every feed failing (full network loss) `all` is empty, and overwriting
    // the cache would wipe the last-known-good news.
    if (!all.isEmpty()) {
        store_cache_async(all);
        latest_articles_ = all;
        persist_articles_async(all);
    } else {
        LOG_WARN("NewsService", "All feeds returned no articles — keeping last-known-good cache");
    }
    persist_feed_health();
    emit feed_health_changed();

    LOG_DEBUG("NewsService", QString("Fetch complete: %1 articles from %2 sources (%3 feeds)")
                                .arg(all.size())
                                .arg(sources.size())
                                .arg(state->total));

    state->callback(true, all);
    emit articles_updated(all);
    publish_articles_to_hub(all);
}

void NewsService::load_recent_from_db(qint64 since_ts, int limit, ArticlesCallback cb) {
    QPointer<NewsService> self = this;
    (void)QtConcurrent::run([self, since_ts, limit, cb = std::move(cb)]() mutable {
        QVector<NewsArticle> rows;
        auto r = fincept::NewsArticleRepository::instance().load_recent(since_ts, {}, limit);
        const bool ok = r.is_ok();
        if (ok)
            rows = r.value();
        else
            LOG_WARN("NewsService", "Loading stored articles failed: " + QString::fromStdString(r.error()));
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        // Rows stored by older builds may carry a future time (feeds that
        // mislabel their zone used to be clamped to the fetch time, and items
        // dated ahead were kept as is): never let them sort above real news.
        rows.erase(std::remove_if(rows.begin(), rows.end(),
                                  [now](const NewsArticle& a) {
                                      return a.sort_ts > now + news_feed::kMaxFutureSkewSec || a.headline.isEmpty();
                                  }),
                   rows.end());
        for (auto& a : rows) {
            if (a.time.isEmpty())
                a.time = QLocale().toString(QDateTime::fromSecsSinceEpoch(a.sort_ts).toLocalTime(), QLocale::ShortFormat);
            // Stored labels follow the current rules (a row classified by an
            // older build kept e.g. URGENT for a road-safety story).
            classify_article(a);
        }
        news_feed::sort_newest_first(rows);
        remember_first_seen(rows);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, ok, cb = std::move(cb), rows = std::move(rows)]() mutable {
                if (self)
                    cb(ok, std::move(rows));
            },
            Qt::QueuedConnection);
    });
}

// ── AI Analysis via Fincept API ─────────────────────────────────────────────

// Parse the `data` object of a /news/analyze response into a NewsAnalysis.
// Shared by the live request path and the persisted-cache load path so both
// stay in lockstep.
static NewsAnalysis parse_analysis_data(const QJsonObject& data) {
    auto a = data["analysis"].toObject();
    auto sent = a["sentiment"].toObject();
    auto mi = a["market_impact"].toObject();
    auto rs = a["risk_signals"].toObject();

    NewsAnalysis analysis;
    analysis.sentiment = {sent["score"].toDouble(), sent["intensity"].toDouble(), sent["confidence"].toDouble()};
    analysis.market_impact = {mi["urgency"].toString(), mi["prediction"].toString()};
    analysis.summary = a["summary"].toString();
    // Credits are fractional (a call can cost 0.35); -1 = not reported.
    analysis.credits_used = data["credits_used"].toDouble(-1);
    analysis.credits_remaining = data["credits_remaining"].toDouble(-1);

    for (const auto& v : a["keywords"].toArray())
        analysis.keywords << v.toString();
    for (const auto& v : a["topics"].toArray())
        analysis.topics << v.toString();
    for (const auto& v : a["key_points"].toArray())
        analysis.key_points << v.toString();

    auto reg = rs["regulatory"].toObject();
    auto geo = rs["geopolitical"].toObject();
    auto ops = rs["operational"].toObject();
    auto mkt = rs["market"].toObject();
    analysis.regulatory = {reg["level"].toString(), reg["details"].toString()};
    analysis.geopolitical = {geo["level"].toString(), geo["details"].toString()};
    analysis.operational = {ops["level"].toString(), ops["details"].toString()};
    analysis.market = {mkt["level"].toString(), mkt["details"].toString()};

    // Entities — organizations carry ticker/sector/sentiment; locations
    // carry a country code; people are name-only.
    auto ent = a["entities"].toObject();
    for (const auto& v : ent["organizations"].toArray()) {
        auto o = v.toObject();
        analysis.organizations.push_back(
            {o["name"].toString(), o["ticker"].toString(), o["sector"].toString(), o["sentiment"].toDouble()});
    }
    for (const auto& v : ent["people"].toArray()) {
        auto p = v.toObject();
        analysis.people.push_back({p["name"].toString(), p["role"].toString(), {}, 0}); // detail = role
    }
    for (const auto& v : ent["locations"].toArray()) {
        auto l = v.toObject();
        analysis.locations.push_back({l["name"].toString(), l["country_code"].toString(), {}, 0});
    }

    // Content fetch metadata — surfaces publisher-block notices.
    auto content = data["content"].toObject();
    analysis.content.headline = content["headline"].toString();
    analysis.content.word_count = content["word_count"].toInt();
    analysis.content.fetch_note = content["fetch_note"].toString();

    // Newer servers: what the analysis rests on ("article_page" | "supplied_text"),
    // and published_at may be null (toString() of null is "").
    analysis.basis = data["analysis_basis"].toString(content["basis"].toString());
    analysis.published_at = data["published_at"].toString();

    return analysis;
}

std::optional<NewsAnalysis> NewsService::cached_analysis(const QString& url) {
    auto r = fincept::NewsArticleRepository::instance().load_analysis(url);
    if (r.is_err() || r.value().isEmpty())
        return std::nullopt;
    const QJsonDocument doc = QJsonDocument::fromJson(r.value().toUtf8());
    if (!doc.isObject())
        return std::nullopt;
    NewsAnalysis cached = parse_analysis_data(doc.object());
    // The stored response carries the charge and balance of the day it was
    // analysed; re-showing it charges nothing and that balance is stale.
    cached.credits_used = -1;
    cached.credits_remaining = -1;
    return cached;
}

void NewsService::analyze_article(const QString& url, AnalysisCallback cb, const QString& headline,
                                  const QString& text) {
    const QJsonObject body = analyze_request_body(url, headline, text);

    // context = `this` ensures the callback drops if NewsService ever stops
    // being a singleton — today it always outlives the request.
    // The server may spend up to 15 s fetching the article and up to 60 s in the
    // model call before it sends a byte, so the client-wide 20 s transfer timeout
    // would abort a request the server is still completing.
    HttpClient::instance().post_with_timeout(
        "/news/analyze", body, kAnalyzeTimeoutMs,
        [url, cb](Result<QJsonDocument> result) {
            if (result.is_err()) {
                LOG_ERROR("NewsService", "Analysis failed: " + QString::fromStdString(result.error()));
                NewsAnalysis failed;
                failed.error = QString::fromStdString(result.error());
                cb(false, failed);
                return;
            }

            auto obj = result.value().object();
            if (!obj["success"].toBool(false)) {
                LOG_ERROR("NewsService", "API returned failure: " + obj["message"].toString());
                NewsAnalysis failed;
                failed.error = obj["message"].toString();
                cb(false, failed);
                return;
            }

            auto data = obj["data"].toObject();
            NewsAnalysis analysis = parse_analysis_data(data);

            // Persist the raw `data` object keyed by URL so reopening the article
            // re-shows this result without re-spending credits. Overwrites any
            // prior cache (re-running ANALYZE refreshes it).
            const QString json = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
            auto wr = fincept::NewsArticleRepository::instance().save_analysis(url, json);
            if (wr.is_err())
                LOG_WARN("NewsService", "Failed to persist analysis: " + QString::fromStdString(wr.error()));

            cb(true, analysis);
        },
        this);
}

// ── AI Headline Summarization ────────────────────────────────────────────────

QVector<NewsArticle> NewsService::brief_articles(const QVector<NewsArticle>& articles, int count) {
    const int n = std::clamp(count, 1, 100);
    QVector<NewsArticle> picked;
    picked.reserve(std::min(n, static_cast<int>(articles.size())));
    for (const auto& a : articles) {
        if (picked.size() >= n)
            break;
        if (!a.headline.trimmed().isEmpty())
            picked.append(a);
    }
    // Same order as the text digest below.
    std::stable_sort(picked.begin(), picked.end(), [](const NewsArticle& x, const NewsArticle& y) {
        if (x.priority != y.priority)
            return static_cast<int>(x.priority) < static_cast<int>(y.priority);
        if (x.impact != y.impact)
            return static_cast<int>(x.impact) < static_cast<int>(y.impact);
        return x.tier < y.tier;
    });
    return picked;
}

void NewsService::summarize_headlines(const QVector<NewsArticle>& articles, int count, SummaryCallback cb) {
    // Deterministic, offline headline digest. There is no backend summarisation
    // endpoint (POST /news/summarize never existed, so this used to fail every
    // time); the caller — the AI tool loop — writes the prose summary itself from
    // this digest. Most important first: higher-tier sources and higher urgency,
    // grouped by category, each line carrying its source and time for citation.
    const int n = std::clamp(count, 1, 100);
    QVector<const NewsArticle*> picked;
    picked.reserve(std::min(n, static_cast<int>(articles.size())));
    for (const auto& a : articles) {
        if (picked.size() >= n)
            break;
        if (!a.headline.trimmed().isEmpty())
            picked.append(&a);
    }
    if (picked.isEmpty()) {
        cb(false, {});
        return;
    }
    // Priority enum is ordered FLASH < URGENT < BREAKING < ROUTINE, Impact HIGH < MEDIUM < LOW,
    // so the underlying values sort most-important first.
    std::stable_sort(picked.begin(), picked.end(), [](const NewsArticle* x, const NewsArticle* y) {
        if (x->priority != y->priority)
            return static_cast<int>(x->priority) < static_cast<int>(y->priority);
        if (x->impact != y->impact)
            return static_cast<int>(x->impact) < static_cast<int>(y->impact);
        return x->tier < y->tier;
    });

    QStringList categories;
    QHash<QString, QStringList> lines_by_cat;
    for (const NewsArticle* a : picked) {
        const QString cat = a->category.trimmed().isEmpty() ? QStringLiteral("General") : a->category.trimmed();
        if (!lines_by_cat.contains(cat))
            categories.append(cat);
        QString line = QStringLiteral("- ") + a->headline.trimmed();
        QStringList meta;
        if (!a->source.isEmpty())
            meta << a->source;
        if (!a->time.isEmpty())
            meta << a->time;
        if (!a->tickers.isEmpty())
            meta << a->tickers.mid(0, 5).join(QLatin1Char(','));
        if (a->priority == Priority::FLASH || a->priority == Priority::URGENT)
            meta << QStringLiteral("URGENT");
        else if (a->impact == Impact::HIGH)
            meta << QStringLiteral("HIGH IMPACT");
        if (!meta.isEmpty())
            line += QStringLiteral(" (") + meta.join(QStringLiteral(" · ")) + QLatin1Char(')');
        lines_by_cat[cat].append(line);
    }

    const QChar nl(u'\n');
    QString digest = QStringLiteral("%1 headlines across %2 categories:").arg(picked.size()).arg(categories.size());
    digest += nl;
    for (const QString& cat : categories) {
        digest += nl + QLatin1Char('[') + cat.toUpper() + QLatin1Char(']') + nl;
        digest += lines_by_cat.value(cat).join(nl) + nl;
    }
    cb(true, digest);
}

// ── WebSocket live feed lives in NewsService_LiveFeed.cpp ───────────────────

// ── Auto-refresh ────────────────────────────────────────────────────────────

void NewsService::set_refresh_interval(int minutes) {
    refresh_timer_->setInterval(minutes * 60 * 1000);
}

void NewsService::start_auto_refresh() {
    refresh_timer_->start();
}
void NewsService::stop_auto_refresh() {
    refresh_timer_->stop();
}

QStringList NewsService::topic_patterns() const {
    return {QStringLiteral("news:general"), QStringLiteral("news:symbol:*"), QStringLiteral("news:category:*"),
            QStringLiteral("news:cluster:*")};
}

void NewsService::refresh(const QStringList& topics) {
    // Cluster topics are push-only — producer never pulls them.
    bool needs_general = false;
    for (const auto& t : topics) {
        if (t == QLatin1String("news:general") || t.startsWith(QLatin1String("news:symbol:")) ||
            t.startsWith(QLatin1String("news:category:"))) {
            needs_general = true;
            break;
        }
    }
    if (!needs_general)
        return;

    // Cache-first: prime the hub with last-known-good articles from disk so
    // subscribers (NewsWidget etc.) render in ~50 ms on cold start instead
    // of waiting for RSS feeds to resolve. The network refresh below then
    // overwrites with fresh data when it lands. The cache read and JSON decode
    // (and the threat re-classification of every article) run on a worker —
    // at startup this used to stall the GUI thread. With no cache entry (it
    // expires after 10 minutes) the last day of stored articles is used.
    QPointer<NewsService> self = this;
    (void)QtConcurrent::run([self]() {
        QVector<NewsArticle> articles;
        if (const auto cached = fincept::CacheManager::instance().try_get(QStringLiteral("news:articles")))
            articles = deserialize_articles(*cached);
        if (articles.isEmpty()) {
            const qint64 since = QDateTime::currentSecsSinceEpoch() - 86400;
            if (auto r = fincept::NewsArticleRepository::instance().load_recent(since, {}, 600); r.is_ok())
                articles = r.value();
            const qint64 limit = QDateTime::currentSecsSinceEpoch() + news_feed::kMaxFutureSkewSec;
            articles.erase(std::remove_if(articles.begin(), articles.end(),
                                          [limit](const NewsArticle& a) { return a.sort_ts > limit; }),
                           articles.end());
        }
        news_feed::sort_newest_first(articles);
        if (articles.isEmpty() || !self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, articles = std::move(articles)]() {
                // A fetch that finished meanwhile already published fresher news.
                if (self && self->latest_articles_.isEmpty())
                    self->publish_articles_to_hub(articles);
            },
            Qt::QueuedConnection);
    });

    // All non-cluster topics derive from the general feed; one fetch
    // fans out via publish_articles_to_hub.
    fetch_all_news_progressive(/*force=*/true, [](bool, QVector<NewsArticle>) {});
}

int NewsService::max_requests_per_sec() const {
    return 2; // RSS aggregator pacing — generous but avoids request storms
}

void NewsService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // General feed — cache 5m, min refresh interval 30s, coalesce
    // progressive chunks to 250ms so cold-cache fills don't repaint in
    // a tight loop. `coalesce_within_ms` field arrived in Phase 4.
    fincept::datahub::TopicPolicy general;
    general.ttl_ms = 5 * 60 * 1000;
    general.min_interval_ms = 30 * 1000;
    general.coalesce_within_ms = 250;
    general.push_only = false;
    hub.set_policy_pattern(QStringLiteral("news:general"), general);

    // Per-symbol / per-category slices share the same TTL; they derive
    // from the same fetch so min_interval keeps producer pacing sane.
    fincept::datahub::TopicPolicy derived = general;
    hub.set_policy_pattern(QStringLiteral("news:symbol:*"), derived);
    hub.set_policy_pattern(QStringLiteral("news:category:*"), derived);

    // Server-assigned clusters — push-only, no scheduled refresh.
    fincept::datahub::TopicPolicy cluster_policy;
    cluster_policy.push_only = true;
    cluster_policy.ttl_ms = 0;
    cluster_policy.min_interval_ms = 0;
    hub.set_policy_pattern(QStringLiteral("news:cluster:*"), cluster_policy);

    hub_registered_ = true;
    LOG_DEBUG("NewsService", "Registered with DataHub (news:general, news:symbol:*, "
                            "news:category:*, news:cluster:*)");
}

void NewsService::publish_articles_to_hub(const QVector<NewsArticle>& list) {
    if (!hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();

    // Every consumer (dashboard news tiles, category tiles that show the first
    // N of their slice) gets the canonical order: newest first, ties by id.
    // The fetch paths already deliver it; anything else (a live push) is
    // sorted here so a tile never shows a different order from the News tab.
    QVector<NewsArticle> sorted_copy;
    const bool in_order = std::is_sorted(list.cbegin(), list.cend(), news_feed::newer_first<NewsArticle>);
    if (!in_order) {
        sorted_copy = list;
        news_feed::sort_newest_first(sorted_copy);
    }
    const QVector<NewsArticle>& accumulated = in_order ? list : sorted_copy;

    // Single canonical publish — the whole accumulated list on news:general.
    hub.publish(QStringLiteral("news:general"), QVariant::fromValue(accumulated));

    // Fan out per-symbol and per-category slices, but only for topics
    // that currently have subscribers — the hub is the authority on
    // who's listening. For now publish unconditionally; the hub's
    // push_only policy on symbol/category patterns caches last-known-
    // good even with no live subscribers, so future mounts get the
    // snapshot via peek(). This is cheap: lists are small and the
    // string splits are linear in article count.
    QHash<QString, QVector<NewsArticle>> by_symbol;
    QHash<QString, QVector<NewsArticle>> by_category;
    for (const auto& a : accumulated) {
        for (const auto& sym : a.tickers) {
            if (!sym.isEmpty())
                by_symbol[sym].append(a);
        }
        if (!a.category.isEmpty())
            by_category[a.category].append(a);
    }
    for (auto it = by_symbol.constBegin(); it != by_symbol.constEnd(); ++it) {
        hub.publish(QStringLiteral("news:symbol:") + it.key(), QVariant::fromValue(it.value()));
    }
    for (auto it = by_category.constBegin(); it != by_category.constEnd(); ++it) {
        hub.publish(QStringLiteral("news:category:") + it.key(), QVariant::fromValue(it.value()));
    }
}

} // namespace fincept::services
