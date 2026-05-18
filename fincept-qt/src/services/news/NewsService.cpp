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
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QAtomicInt>
#include <QDateTime>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <QXmlStreamReader>

#ifdef HAS_QT_WEBSOCKETS
#    include <QtWebSockets/QWebSocket>
#endif

#include <algorithm>
#include <memory>

namespace fincept::services {


// HTTP timing + browser user agent — used by the RSS fetch paths.
static constexpr int kFeedTransferTimeoutMs = 4000;   // 4s per RSS feed request

// Use a real browser User-Agent — major financial publishers reject
// "FinceptTerminal/4.0" as scraper traffic. Browser UA gets us 200s on
// the same endpoints.
static constexpr const char* kBrowserUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

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

// ── Fetch all RSS feeds in parallel ─────────────────────────────────────────

void NewsService::fetch_all_news(bool force, ArticlesCallback cb) {
    if (!force) {
        const QVariant cached = fincept::CacheManager::instance().get("news:articles");
        if (!cached.isNull()) {
            const QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
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
                a.impact = impact_from_string(o["impact"].toString());
                a.lang = o["lang"].toString();
                for (const auto& t : o["tickers"].toArray())
                    a.tickers << t.toString();
                articles.append(a);
            }
            cb(true, articles);
            publish_articles_to_hub(articles);
            return;
        }
    }

    auto feeds = default_feeds();
    feed_count_ = feeds.size();

    // Shared state for collecting results from parallel requests
    struct FetchState {
        QMutex mutex;
        QVector<NewsArticle> all_articles;
        QAtomicInt remaining{0};
        ArticlesCallback callback;
        NewsService* service = nullptr;
    };

    auto state = std::make_shared<FetchState>();
    state->remaining.storeRelaxed(feeds.size());
    state->callback = std::move(cb);
    state->service = this;

    for (const auto& feed : feeds) {
        QNetworkRequest req(QUrl(feed.url));
        req.setHeader(QNetworkRequest::UserAgentHeader, kBrowserUserAgent);
        req.setRawHeader("Accept", "application/rss+xml, application/xml, text/xml, */*");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(kFeedTransferTimeoutMs);

        auto* reply = nam_->get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, feed, state]() {
            reply->deleteLater();

            QVector<NewsArticle> articles;
            const int http_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                const QByteArray trimmed = data.trimmed();
                // Cheap shape check: real RSS/Atom starts with <?xml or <rss or
                // <feed. HTML error pages (Akamai access-denied, Cloudflare
                // captcha) start with <html / <!doctype and would pass through
                // the parser silently producing 0 articles — flag them clearly.
                const bool looks_like_html =
                    trimmed.left(20).toLower().contains("<html") ||
                    trimmed.left(20).toLower().contains("<!doctype html");
                if (looks_like_html) {
                    LOG_WARN("NewsService",
                             QString("Feed %1 (%2) returned HTML (likely access-denied), %3 bytes")
                                 .arg(feed.id, feed.source).arg(data.size()));
                } else if (trimmed.startsWith('<')) {
                    articles = parse_rss_xml(data, feed);
                }
                if (articles.isEmpty() && !looks_like_html) {
                    LOG_WARN("NewsService", QString("Feed %1 (%2) returned %3 bytes but no parsed articles")
                                                .arg(feed.id, feed.source).arg(data.size()));
                }
            } else {
                LOG_WARN("NewsService", QString("Feed %1 (%2) failed: HTTP %3, err=%4")
                                            .arg(feed.id, feed.source).arg(http_code).arg(reply->errorString()));
            }

            {
                QMutexLocker lock(&state->mutex);
                state->all_articles.append(articles);
            }

            if (state->remaining.fetchAndSubRelaxed(1) == 1) {
                // Last feed done — sort by time descending
                auto& all = state->all_articles;
                std::sort(all.begin(), all.end(),
                          [](const NewsArticle& a, const NewsArticle& b) { return a.sort_ts > b.sort_ts; });

                QSet<QString> sources;
                for (const auto& a : all)
                    sources.insert(a.source);
                state->service->active_sources_ = sources.values();

                // Serialize to CacheManager
                QJsonArray arr;
                for (const auto& a : all) {
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
                    o["impact"] = impact_string(a.impact);
                    o["lang"] = a.lang;
                    QJsonArray tickers;
                    for (const auto& t : a.tickers)
                        tickers.append(t);
                    o["tickers"] = tickers;
                    arr.append(o);
                }
                fincept::CacheManager::instance().put(
                    "news:articles", QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                    kArticleCacheTtlSec, "news");

                LOG_INFO("NewsService",
                         QString("Fetched %1 articles from %2 sources").arg(all.size()).arg(sources.size()));

                state->callback(true, all);
                emit state->service->articles_updated(all);
                state->service->publish_articles_to_hub(all);
            }
        });
    }
}

// ── Progressive fetch — emits partial batches as each feed arrives ───────────
// First few fast feeds (Reuters, BBC) typically arrive in <300ms giving the
// screen something to render immediately while slower feeds trickle in.

void NewsService::fetch_all_news_progressive(bool force, ArticlesCallback final_cb) {
    if (!force) {
        const QVariant cached = fincept::CacheManager::instance().get("news:articles");
        if (!cached.isNull()) {
            const QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
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
                a.impact = impact_from_string(o["impact"].toString());
                a.lang = o["lang"].toString();
                for (const auto& t : o["tickers"].toArray())
                    a.tickers << t.toString();
                articles.append(a);
            }
            final_cb(true, articles);
            emit articles_partial(articles, feed_count_, feed_count_);
            publish_articles_to_hub(articles);
            return;
        }
    }

    auto feeds = default_feeds();
    feed_count_ = feeds.size();
    const int total = feeds.size();

    struct FetchState {
        QMutex mutex;
        QVector<NewsArticle> all_articles;
        QAtomicInt remaining{0};
        QAtomicInt done{0};
        ArticlesCallback callback;
        NewsService* service = nullptr;
    };

    auto state = std::make_shared<FetchState>();
    state->remaining.storeRelaxed(total);
    state->callback = std::move(final_cb);
    state->service = this;

    for (const auto& feed : feeds) {
        QNetworkRequest req(QUrl(feed.url));
        req.setHeader(QNetworkRequest::UserAgentHeader, kBrowserUserAgent);
        req.setRawHeader("Accept", "application/rss+xml, application/xml, text/xml, */*");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(kFeedTransferTimeoutMs);

        auto* reply = nam_->get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, feed, state, total, this]() {
            reply->deleteLater();

            QVector<NewsArticle> batch;
            const int http_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                const QByteArray trimmed = data.trimmed();
                const bool looks_like_html =
                    trimmed.left(20).toLower().contains("<html") ||
                    trimmed.left(20).toLower().contains("<!doctype html");
                if (looks_like_html) {
                    LOG_WARN("NewsService",
                             QString("Feed %1 (%2) returned HTML (likely access-denied), %3 bytes")
                                 .arg(feed.id, feed.source).arg(data.size()));
                } else if (trimmed.startsWith('<')) {
                    batch = parse_rss_xml(data, feed);
                }
                if (batch.isEmpty() && !looks_like_html) {
                    LOG_WARN("NewsService", QString("Feed %1 (%2) returned %3 bytes but no parsed articles")
                                                .arg(feed.id, feed.source).arg(data.size()));
                }
            } else {
                LOG_WARN("NewsService", QString("Feed %1 (%2) failed: HTTP %3, err=%4")
                                            .arg(feed.id, feed.source).arg(http_code).arg(reply->errorString()));
            }

            QVector<NewsArticle> snapshot;
            int feeds_done = 0;
            {
                QMutexLocker lock(&state->mutex);
                state->all_articles.append(batch);
                feeds_done = state->done.fetchAndAddRelaxed(1) + 1;
                // Partial snapshot sorted by time for progressive display
                snapshot = state->all_articles;
            }
            std::sort(snapshot.begin(), snapshot.end(),
                      [](const NewsArticle& a, const NewsArticle& b) { return a.sort_ts > b.sort_ts; });
            emit articles_partial(snapshot, feeds_done, total);
            // Progressive publish — each chunk fans out the accumulated
            // list. Hub's per-topic coalescing (news:general at 250ms)
            // throttles the UI repaint storm on cold-cache fills.
            publish_articles_to_hub(snapshot);

            if (state->remaining.fetchAndSubRelaxed(1) == 1) {
                // All feeds done — finalize cache
                auto& all = state->all_articles;
                std::sort(all.begin(), all.end(),
                          [](const NewsArticle& a, const NewsArticle& b) { return a.sort_ts > b.sort_ts; });

                QSet<QString> sources;
                for (const auto& a : all)
                    sources.insert(a.source);
                state->service->active_sources_ = sources.values();

                QJsonArray parr;
                for (const auto& a : all) {
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
                    o["impact"] = impact_string(a.impact);
                    o["lang"] = a.lang;
                    QJsonArray tickers;
                    for (const auto& t : a.tickers)
                        tickers.append(t);
                    o["tickers"] = tickers;
                    parr.append(o);
                }
                fincept::CacheManager::instance().put(
                    "news:articles", QVariant(QString::fromUtf8(QJsonDocument(parr).toJson(QJsonDocument::Compact))),
                    kArticleCacheTtlSec, "news");

                LOG_INFO(
                    "NewsService",
                    QString("Progressive fetch complete: %1 articles, %2 sources").arg(all.size()).arg(sources.size()));

                state->callback(true, all);
                emit state->service->articles_updated(all);
                state->service->publish_articles_to_hub(all);
            }
        });
    }
}

// ── AI Analysis via Fincept API ─────────────────────────────────────────────

void NewsService::analyze_article(const QString& url, AnalysisCallback cb) {
    QJsonObject body;
    body["url"] = url;

    // context = `this` ensures the callback drops if NewsService ever stops
    // being a singleton — today it always outlives the request.
    HttpClient::instance().post("/news/analyze", body, [this, cb](Result<QJsonDocument> result) {
        if (result.is_err()) {
            LOG_ERROR("NewsService", "Analysis failed: " + QString::fromStdString(result.error()));
            cb(false, {});
            return;
        }

        auto obj = result.value().object();
        if (!obj["success"].toBool(false)) {
            LOG_ERROR("NewsService", "API returned failure: " + obj["message"].toString());
            cb(false, {});
            return;
        }

        auto data = obj["data"].toObject();
        auto a = data["analysis"].toObject();
        auto sent = a["sentiment"].toObject();
        auto mi = a["market_impact"].toObject();
        auto rs = a["risk_signals"].toObject();

        NewsAnalysis analysis;
        analysis.sentiment = {sent["score"].toDouble(), sent["intensity"].toDouble(), sent["confidence"].toDouble()};
        analysis.market_impact = {mi["urgency"].toString(), mi["prediction"].toString()};
        analysis.summary = a["summary"].toString();
        analysis.credits_used = data["credits_used"].toInt();
        analysis.credits_remaining = data["credits_remaining"].toInt();

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

        cb(true, analysis);
        emit this->analysis_ready(analysis);
    }, this);
}

// ── AI Headline Summarization ────────────────────────────────────────────────

void NewsService::summarize_headlines(const QVector<NewsArticle>& articles, int count, SummaryCallback cb) {
    // Build headline signature for cache check
    QStringList headlines;
    for (int i = 0; i < std::min(count, static_cast<int>(articles.size())); ++i)
        headlines.append(articles[i].headline);
    std::sort(headlines.begin(), headlines.end());
    QString sig = headlines.join("|").left(500);

    {
        const QString sum_key = "news:summary:" + sig.left(200);
        const QVariant cached = fincept::CacheManager::instance().get(sum_key);
        if (!cached.isNull()) {
            cb(true, cached.toString());
            return;
        }
    }

    // Build request body
    QJsonArray headline_array;
    for (const auto& h : headlines)
        headline_array.append(h);

    QJsonObject body;
    body["headlines"] = headline_array;
    body["count"] = count;

    HttpClient::instance().post("/news/summarize", body, [cb, sig](Result<QJsonDocument> result) {
        if (result.is_err()) {
            LOG_WARN("NewsService", "Summarization failed: " + QString::fromStdString(result.error()));
            cb(false, {});
            return;
        }

        auto obj = result.value().object();
        QString summary = obj["summary"].toString();
        if (summary.isEmpty() && obj.contains("data"))
            summary = obj["data"].toObject()["summary"].toString();

        if (!summary.isEmpty()) {
            fincept::CacheManager::instance().put("news:summary:" + sig.left(200), QVariant(summary),
                                                  kSummaryCacheTtlSec, "news");
        }

        cb(!summary.isEmpty(), summary);
    });
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
    return {QStringLiteral("news:general"),
            QStringLiteral("news:symbol:*"),
            QStringLiteral("news:category:*"),
            QStringLiteral("news:cluster:*")};
}

void NewsService::refresh(const QStringList& topics) {
    // Cluster topics are push-only — producer never pulls them.
    bool needs_general = false;
    for (const auto& t : topics) {
        if (t == QLatin1String("news:general") ||
            t.startsWith(QLatin1String("news:symbol:")) ||
            t.startsWith(QLatin1String("news:category:"))) {
            needs_general = true;
            break;
        }
    }
    if (!needs_general) return;

    // Cache-first: prime the hub with last-known-good articles from disk so
    // subscribers (NewsWidget etc.) render in ~50 ms on cold start instead
    // of waiting for RSS feeds to resolve. The network refresh below then
    // overwrites with fresh data when it lands.
    const QVariant cached = fincept::CacheManager::instance().get("news:articles");
    if (!cached.isNull()) {
        const QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        if (!arr.isEmpty()) {
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
                a.impact = impact_from_string(o["impact"].toString());
                a.lang = o["lang"].toString();
                for (const auto& t : o["tickers"].toArray())
                    a.tickers << t.toString();
                articles.append(a);
            }
            publish_articles_to_hub(articles);
        }
    }

    // All non-cluster topics derive from the general feed; one fetch
    // fans out via publish_articles_to_hub.
    fetch_all_news_progressive(/*force=*/true, [](bool, QVector<NewsArticle>) {});
}

int NewsService::max_requests_per_sec() const {
    return 2;  // RSS aggregator pacing — generous but avoids request storms
}

void NewsService::ensure_registered_with_hub() {
    if (hub_registered_) return;
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
    LOG_INFO("NewsService",
             "Registered with DataHub (news:general, news:symbol:*, "
             "news:category:*, news:cluster:*)");
}

void NewsService::publish_articles_to_hub(const QVector<NewsArticle>& accumulated) {
    if (!hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();

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
        hub.publish(QStringLiteral("news:symbol:") + it.key(),
                    QVariant::fromValue(it.value()));
    }
    for (auto it = by_category.constBegin(); it != by_category.constEnd(); ++it) {
        hub.publish(QStringLiteral("news:category:") + it.key(),
                    QVariant::fromValue(it.value()));
    }
}

} // namespace fincept::services
