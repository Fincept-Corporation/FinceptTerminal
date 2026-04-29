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

static constexpr int kFeedTransferTimeoutMs = 8000;   // 8s per RSS feed request
static constexpr int kWsReconnectDelayMs    = 10000;  // 10s before WebSocket reconnect
static constexpr int kSummaryMaxChars       = 300;    // max chars for article summary

// Use a real browser User-Agent — Bloomberg, WSJ, FT and other major
// publishers reject "FinceptTerminal/4.0" as scraper traffic. Browser UA
// gets us 200s on the same endpoints.
static constexpr const char* kBrowserUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

// ── Singleton ───────────────────────────────────────────────────────────────

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
    });
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

// ── WebSocket live feed ──────────────────────────────────────────────────────

#ifdef HAS_QT_WEBSOCKETS
void NewsService::connect_live_feed(const QString& ws_url) {
    if (live_ws_)
        return; // already connected

    live_ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(live_ws_, &QWebSocket::connected, this, [this]() {
        live_connected_ = true;
        LOG_INFO("NewsService", "WebSocket live feed connected");
    });

    connect(live_ws_, &QWebSocket::disconnected, this, [this]() {
        live_connected_ = false;
        LOG_WARN("NewsService", "WebSocket live feed disconnected");
        // Auto-reconnect after delay
        QTimer::singleShot(kWsReconnectDelayMs, this, [this]() {
            if (live_ws_ && !live_connected_)
                live_ws_->open(live_ws_->requestUrl());
        });
    });

    connect(live_ws_, &QWebSocket::textMessageReceived, this, [this](const QString& msg) {
        // Parse incoming JSON article
        auto doc = QJsonDocument::fromJson(msg.toUtf8());
        if (!doc.isObject())
            return;

        auto obj = doc.object();
        NewsArticle article;
        article.id = obj["id"].toString();
        article.headline = obj["headline"].toString(obj["title"].toString());
        article.summary = obj["summary"].toString(obj["description"].toString());
        article.source = obj["source"].toString();
        article.link = obj["link"].toString(obj["url"].toString());
        article.category = obj["category"].toString("MARKETS");
        article.sort_ts = obj["timestamp"].toInteger(QDateTime::currentSecsSinceEpoch());
        article.time = QDateTime::fromSecsSinceEpoch(article.sort_ts).toString("MMM dd, HH:mm");
        article.tier = obj["tier"].toInt(2);

        if (article.headline.isEmpty())
            return;

        enrich_article(article);

        // Prepend to cached articles
        QVector<NewsArticle> updated;
        {
            const QVariant cv = fincept::CacheManager::instance().get("news:articles");
            if (!cv.isNull()) {
                const QJsonArray existing = QJsonDocument::fromJson(cv.toString().toUtf8()).array();
                updated.reserve(existing.size() + 1);
                for (const auto& v : existing) {
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
                    updated.append(a);
                }
            }
        }
        updated.prepend(article);
        QJsonArray narr;
        for (const auto& a : updated) {
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
            narr.append(o);
        }
        fincept::CacheManager::instance().put(
            "news:articles", QVariant(QString::fromUtf8(QJsonDocument(narr).toJson(QJsonDocument::Compact))),
            kArticleCacheTtlSec, "news");
        emit articles_partial(updated, 1, 1);
        LOG_INFO("NewsService", "Live article: " + article.headline.left(50));
    });

    QString url = ws_url.isEmpty() ? "wss://api.fincept.in/ws/news" : ws_url;
    live_ws_->open(QUrl(url));
}

void NewsService::disconnect_live_feed() {
    if (!live_ws_)
        return;
    live_ws_->close();
    live_ws_->deleteLater();
    live_ws_ = nullptr;
    live_connected_ = false;
}

bool NewsService::is_live_connected() const {
    return live_connected_;
}
#else
// No WebSocket support — stubs
void NewsService::connect_live_feed(const QString&) {}
void NewsService::disconnect_live_feed() {}
bool NewsService::is_live_connected() const {
    return false;
}
#endif

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

// ── RSS XML parser ──────────────────────────────────────────────────────────

QVector<NewsArticle> NewsService::parse_rss_xml(const QByteArray& xml, const RSSFeed& feed) {
    QVector<NewsArticle> articles;
    QXmlStreamReader reader(xml);

    bool in_item = false;
    NewsArticle current;
    QString current_tag;
    int item_idx = 0;

    while (!reader.atEnd()) {
        auto token = reader.readNext();

        if (token == QXmlStreamReader::StartElement) {
            current_tag = reader.name().toString();

            if (current_tag == "item" || current_tag == "entry") {
                in_item = true;
                item_idx++;
                current = {};
                current.category = feed.category;
                current.source = feed.source;
                current.region = feed.region;
                current.tier = feed.tier;
                current.id = QString("%1-%2-%3").arg(feed.id).arg(QDateTime::currentMSecsSinceEpoch()).arg(item_idx);
            }

            // Atom <link href="..."/> or <link rel="alternate" href="..."/>
            if (in_item && current_tag == "link") {
                auto href = reader.attributes().value("href").toString();
                auto rel = reader.attributes().value("rel").toString();
                if (!href.isEmpty() && (rel.isEmpty() || rel == "alternate")) {
                    if (current.link.isEmpty())
                        current.link = href;
                }
            }
        } else if (token == QXmlStreamReader::Characters && in_item) {
            QString text = reader.text().toString().trimmed();
            if (text.isEmpty())
                continue;

            if (current_tag == "title" && current.headline.isEmpty()) {
                current.headline = text.left(200);
            } else if ((current_tag == "description" || current_tag == "summary" || current_tag == "encoded") &&
                       current.summary.isEmpty()) {
                current.summary = strip_html(text).left(kSummaryMaxChars);
            } else if (current_tag == "link" && current.link.isEmpty()) {
                current.link = text.trimmed();
            } else if ((current_tag == "guid" || current_tag == "id") && current.link.isEmpty()) {
                // guid/id often contains the article URL as fallback
                if (text.startsWith("http"))
                    current.link = text.trimmed();
            } else if (current_tag == "pubDate" || current_tag == "published" || current_tag == "updated" ||
                       current_tag == "date") {
                if (current.sort_ts == 0) {
                    QDateTime dt = QDateTime::fromString(text, Qt::RFC2822Date);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(text, Qt::ISODate);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(text, "ddd, dd MMM yyyy HH:mm:ss");
                    if (dt.isValid()) {
                        current.sort_ts = dt.toSecsSinceEpoch();
                        current.time = dt.toString("MMM dd, HH:mm");
                    } else {
                        current.time = text.left(22);
                    }
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            QString tag = reader.name().toString();
            if ((tag == "item" || tag == "entry") && in_item) {
                in_item = false;
                if (current.headline.isEmpty())
                    continue;

                if (current.time.isEmpty())
                    current.time = QDateTime::currentDateTime().toString("MMM dd, HH:mm");
                if (current.sort_ts == 0)
                    current.sort_ts = QDateTime::currentSecsSinceEpoch();

                enrich_article(current);
                articles.append(std::move(current));
            }
        }
    }

    return articles;
}

// ── Strip HTML tags ─────────────────────────────────────────────────────────

QString NewsService::strip_html(const QString& html) {
    static QRegularExpression re("<[^>]*>");
    QString out = html;
    out.replace(re, "");
    return out.simplified();
}

// ── Enrich article: sentiment, priority, category, tickers ──────────────────

void NewsService::enrich_article(NewsArticle& article) {
    // Build once — reused for all keyword checks, ticker regex, and classify_threat
    const QString combined = article.headline + " " + article.summary;
    const QString text = combined.toLower();

    // Priority
    if (text.contains("breaking") || text.contains("alert"))
        article.priority = Priority::FLASH;
    else if (text.contains("urgent") || text.contains("emergency"))
        article.priority = Priority::URGENT;
    else if (text.contains("announce") || text.contains("report"))
        article.priority = Priority::BREAKING;

    // Weighted sentiment
    struct WordWeight {
        const char* word;
        int weight;
    };

    static const WordWeight positives[] = {
        {"surge", 3},       {"soar", 3},       {"skyrocket", 3}, {"breakthrough", 3}, {"boom", 3},
        {"record high", 3}, {"rally", 2},      {"gain", 2},      {"rise", 2},         {"jump", 2},
        {"climb", 2},       {"spike", 2},      {"rebound", 2},   {"boost", 2},        {"beat", 2},
        {"exceed", 2},      {"upgrade", 2},    {"profit", 2},    {"growth", 2},       {"expand", 2},
        {"recover", 2},     {"victory", 2},    {"ceasefire", 2}, {"treaty", 2},       {"reform", 2},
        {"optimism", 2},    {"milestone", 2},  {"strong", 1},    {"robust", 1},       {"stellar", 1},
        {"buy", 1},         {"positive", 1},   {"success", 1},   {"win", 1},          {"approval", 1},
        {"deal", 1},        {"confidence", 1}, {"dividend", 1},  {"progress", 1},     {"improve", 1},
        {"hope", 1},        {"support", 1},    {"bolster", 1},   {"outperform", 1},   {"bullish", 1},
        {"upside", 1},      {"favorable", 1},  {"momentum", 1},  {"launch", 1},       {"unveil", 1},
    };

    static const WordWeight negatives[] = {
        {"crash", 3},      {"plunge", 3},    {"collapse", 3},   {"devastat", 3},  {"catastroph", 3}, {"invasion", 3},
        {"war crime", 3},  {"nuclear", 3},   {"bankruptcy", 3}, {"meltdown", 3},  {"fall", 2},       {"drop", 2},
        {"decline", 2},    {"tumble", 2},    {"slide", 2},      {"slump", 2},     {"miss", 2},       {"disappoint", 2},
        {"fail", 2},       {"recession", 2}, {"crisis", 2},     {"conflict", 2},  {"attack", 2},     {"kill", 2},
        {"sanction", 2},   {"tariff", 2},    {"escalat", 2},    {"layoff", 2},    {"downgrade", 2},  {"default", 2},
        {"fraud", 2},      {"scandal", 2},   {"coup", 2},       {"protest", 2},   {"disaster", 2},   {"worst", 1},
        {"weak", 1},       {"loss", 1},      {"deficit", 1},    {"fear", 1},      {"risk", 1},       {"threat", 1},
        {"warning", 1},    {"sell", 1},      {"debt", 1},       {"inflation", 1}, {"slowdown", 1},   {"bearish", 1},
        {"negative", 1},   {"volatile", 1},  {"uncertain", 1},  {"reject", 1},    {"ban", 1},        {"suspend", 1},
        {"investigat", 1}, {"probe", 1},     {"hack", 1},       {"leak", 1},      {"shortage", 1},   {"disrupt", 1},
        {"shrink", 1},
    };

    int pos = 0, neg = 0;
    for (const auto& [w, wt] : positives) {
        if (text.contains(w))
            pos += wt;
    }
    for (const auto& [w, wt] : negatives) {
        if (text.contains(w))
            neg += wt;
    }

    int net = pos - neg;
    if (net >= 1)
        article.sentiment = Sentiment::BULLISH;
    else if (net <= -1)
        article.sentiment = Sentiment::BEARISH;
    // else stays NEUTRAL

    // Impact
    int strength = std::abs(net);
    if (article.priority == Priority::FLASH || article.priority == Priority::URGENT || strength >= 6)
        article.impact = Impact::HIGH;
    else if (article.priority == Priority::BREAKING || strength >= 3)
        article.impact = Impact::MEDIUM;

    // Category refinement
    if (text.contains("earnings") || text.contains("quarterly results") || text.contains("eps") ||
        text.contains("guidance"))
        article.category = "EARNINGS";
    else if (text.contains("crypto") || text.contains("bitcoin") || text.contains("ethereum") ||
             text.contains("blockchain"))
        article.category = "CRYPTO";
    else if (text.contains("missile") || text.contains("troops") || text.contains("pentagon") ||
             text.contains("military"))
        article.category = "DEFENSE";
    else if (text.contains("fed ") || text.contains("federal reserve") || text.contains("inflation") ||
             text.contains("gdp") || text.contains("interest rate") || text.contains("central bank"))
        article.category = "ECONOMIC";
    else if (text.contains("s&p 500") || text.contains("nasdaq") || text.contains("dow jones") ||
             text.contains("stock market"))
        article.category = "MARKETS";
    else if (text.contains("energy") || text.contains("crude") || text.contains("opec") ||
             text.contains("natural gas") || text.contains("oil price"))
        article.category = "ENERGY";
    else if (text.contains("tech") || text.contains(" ai ") || text.contains("artificial intelligence") ||
             text.contains("semiconductor") || text.contains("startup"))
        article.category = "TECH";
    else if (text.contains("nato") || text.contains("ukraine") || text.contains("russia") || text.contains("china") ||
             text.contains("gaza") || text.contains("sanctions") || text.contains("geopolit"))
        article.category = "GEOPOLITICS";

    // Extract tickers: uppercase 2-5 letter words
    static QRegularExpression ticker_re("\\b[A-Z]{2,5}\\b");
    static QSet<QString> common_words = {"THE",  "FOR",  "AND",  "BUT",  "NOT",  "FROM", "WITH", "THIS", "THAT", "HAVE",
                                         "WILL", "BEEN", "THEY", "WERE", "SAID", "HAS",  "ITS",  "NEW",  "ARE",  "WAS"};
    auto it = ticker_re.globalMatch(combined); // reuse already-built string
    QSet<QString> found;
    while (it.hasNext() && found.size() < 5) {
        auto m = it.next();
        QString t = m.captured();
        if (!common_words.contains(t))
            found.insert(t);
    }
    article.tickers = found.values();

    // Language detection — check for CJK, Cyrillic, Arabic, Devanagari characters
    auto detect_lang = [](const QString& s) -> QString {
        int cjk = 0, cyrillic = 0, arabic = 0, devanagari = 0;
        for (const auto& ch : s) {
            ushort u = ch.unicode();
            if (u >= 0x4e00 && u <= 0x9fff)
                cjk++;
            else if (u >= 0x3040 && u <= 0x30ff)
                return "ja"; // kana = definitely Japanese
            else if (u >= 0xac00 && u <= 0xd7af)
                return "ko"; // hangul = Korean
            else if (u >= 0x0400 && u <= 0x04ff)
                cyrillic++;
            else if (u >= 0x0600 && u <= 0x06ff)
                arabic++;
            else if (u >= 0x0900 && u <= 0x097f)
                devanagari++;
        }
        int total = s.size();
        if (total == 0)
            return "en";
        if (cjk * 10 > total)
            return "zh";
        if (cyrillic * 10 > total)
            return "ru";
        if (arabic * 10 > total)
            return "ar";
        if (devanagari * 10 > total)
            return "hi";
        return "en";
    };
    article.lang = detect_lang(article.headline);

    // Threat classification — pass pre-built text to avoid a 3rd toLower()
    article.threat = classify_threat(article, text);

    // Source credibility flag
    article.source_flag = source_flag_for(article.source);
}

// ── Threat classification with confidence ───────────────────────────────────

ThreatClassification NewsService::classify_threat(const NewsArticle& article) {
    // Convenience overload — builds text itself (used only outside enrich_article)
    return classify_threat(article, (article.headline + " " + article.summary).toLower());
}

ThreatClassification NewsService::classify_threat(const NewsArticle& article, const QString& text) {
    ThreatClassification tc;
    tc.category = "general";
    tc.confidence = 0.3; // base confidence from keyword matching

    // Critical — immediate, high-impact events
    struct PatternScore {
        const char* pattern;
        const char* category;
        ThreatLevel level;
        double conf;
    };
    static const PatternScore critical_patterns[] = {
        {"nuclear strike", "conflict", ThreatLevel::CRITICAL, 0.95},
        {"nuclear attack", "conflict", ThreatLevel::CRITICAL, 0.95},
        {"war declared", "conflict", ThreatLevel::CRITICAL, 0.95},
        {"market crash", "market", ThreatLevel::CRITICAL, 0.9},
        {"flash crash", "market", ThreatLevel::CRITICAL, 0.9},
        {"circuit breaker", "market", ThreatLevel::CRITICAL, 0.85},
        {"trading halt", "market", ThreatLevel::CRITICAL, 0.85},
        {"bank run", "market", ThreatLevel::CRITICAL, 0.9},
        {"sovereign default", "market", ThreatLevel::CRITICAL, 0.9},
        {"cyberattack", "cyber", ThreatLevel::HIGH, 0.8},
        {"data breach", "cyber", ThreatLevel::HIGH, 0.75},
        {"ransomware", "cyber", ThreatLevel::HIGH, 0.8},
    };

    // High — significant events
    static const PatternScore high_patterns[] = {
        {"invasion", "conflict", ThreatLevel::HIGH, 0.85},
        {"airstrike", "conflict", ThreatLevel::HIGH, 0.85},
        {"missile launch", "conflict", ThreatLevel::HIGH, 0.85},
        {"military deploy", "conflict", ThreatLevel::HIGH, 0.8},
        {"coup attempt", "conflict", ThreatLevel::HIGH, 0.85},
        {"martial law", "conflict", ThreatLevel::HIGH, 0.85},
        {"bankruptcy fil", "market", ThreatLevel::HIGH, 0.8},
        {"rate hike", "market", ThreatLevel::HIGH, 0.7},
        {"rate cut", "market", ThreatLevel::HIGH, 0.7},
        {"earnings miss", "market", ThreatLevel::HIGH, 0.75},
        {"profit warning", "market", ThreatLevel::HIGH, 0.75},
        {"downgrad", "market", ThreatLevel::HIGH, 0.7},
        {"sanction", "regulatory", ThreatLevel::HIGH, 0.7},
        {"embargo", "regulatory", ThreatLevel::HIGH, 0.75},
        {"earthquake", "natural", ThreatLevel::HIGH, 0.8},
        {"tsunami", "natural", ThreatLevel::HIGH, 0.85},
        {"hurricane", "natural", ThreatLevel::HIGH, 0.75},
        {"pandemic", "natural", ThreatLevel::HIGH, 0.8},
    };

    // Medium patterns
    static const PatternScore medium_patterns[] = {
        {"protest", "conflict", ThreatLevel::MEDIUM, 0.6},     {"riot", "conflict", ThreatLevel::MEDIUM, 0.7},
        {"tension", "conflict", ThreatLevel::MEDIUM, 0.5},     {"escalat", "conflict", ThreatLevel::MEDIUM, 0.65},
        {"tariff", "regulatory", ThreatLevel::MEDIUM, 0.65},   {"regulation", "regulatory", ThreatLevel::MEDIUM, 0.5},
        {"antitrust", "regulatory", ThreatLevel::MEDIUM, 0.6}, {"investigat", "regulatory", ThreatLevel::MEDIUM, 0.55},
        {"layoff", "market", ThreatLevel::MEDIUM, 0.6},        {"recession", "market", ThreatLevel::MEDIUM, 0.65},
        {"inflation", "market", ThreatLevel::MEDIUM, 0.55},    {"selloff", "market", ThreatLevel::MEDIUM, 0.6},
        {"sell-off", "market", ThreatLevel::MEDIUM, 0.6},      {"volatil", "market", ThreatLevel::MEDIUM, 0.5},
        {"wildfire", "natural", ThreatLevel::MEDIUM, 0.6},     {"flood", "natural", ThreatLevel::MEDIUM, 0.6},
    };

    // Check patterns in priority order — first critical, then high, then medium
    for (const auto& p : critical_patterns) {
        if (text.contains(p.pattern)) {
            tc.level = p.level;
            tc.category = p.category;
            tc.confidence = p.conf;
            return tc;
        }
    }
    for (const auto& p : high_patterns) {
        if (text.contains(p.pattern)) {
            tc.level = p.level;
            tc.category = p.category;
            tc.confidence = p.conf;
            return tc;
        }
    }
    for (const auto& p : medium_patterns) {
        if (text.contains(p.pattern)) {
            tc.level = p.level;
            tc.category = p.category;
            tc.confidence = p.conf;
            return tc;
        }
    }

    // Low: any negative sentiment article
    if (article.sentiment == Sentiment::BEARISH) {
        tc.level = ThreatLevel::LOW;
        tc.confidence = 0.4;
    }

    return tc;
}

// ── Source credibility ──────────────────────────────────────────────────────

SourceFlag NewsService::source_flag_for(const QString& source) {
    static const QMap<QString, SourceFlag> flags = {
        // State media
        {"XINHUA", SourceFlag::STATE_MEDIA},
        {"CGTN", SourceFlag::STATE_MEDIA},
        {"GLOBAL TIMES", SourceFlag::STATE_MEDIA},
        {"RT", SourceFlag::STATE_MEDIA},
        {"TASS", SourceFlag::STATE_MEDIA},
        {"SPUTNIK", SourceFlag::STATE_MEDIA},
        {"PRESS TV", SourceFlag::STATE_MEDIA},
        {"KCNA", SourceFlag::STATE_MEDIA},
        {"TRT WORLD", SourceFlag::STATE_MEDIA},
        {"AL ARABIYA", SourceFlag::STATE_MEDIA},
        // Caution — sensationalism or low editorial standards
        {"ZEROHEDGE", SourceFlag::CAUTION},
        {"INFOWARS", SourceFlag::CAUTION},
        {"DAILY MAIL", SourceFlag::CAUTION},
        {"NY POST", SourceFlag::CAUTION},
    };
    auto it = flags.find(source.toUpper());
    return it != flags.end() ? it.value() : SourceFlag::NONE;
}

QString NewsService::source_flag_label(SourceFlag flag) {
    switch (flag) {
        case SourceFlag::STATE_MEDIA:
            return "STATE MEDIA";
        case SourceFlag::CAUTION:
            return "CAUTION";
        default:
            return {};
    }
}

// ── Default RSS feeds ──────────────────────────────────────────────────────

QVector<RSSFeed> NewsService::default_feeds() {
    return {
        // Tier 1 — Wire Services & Regulators
        // Reuters discontinued public RSS in 2020 (feeds.reuters.com is dead).
        // We keep tier-1 coverage via AP, BBC, FT, Bloomberg, WSJ instead.
        {"ap-top", "AP Top News", "https://rsshub.app/apnews/topics/ap-top-news", "GEOPOLITICS", "GLOBAL", "AP", 1},
        {"sec-press", "SEC Press Releases", "https://www.sec.gov/news/pressreleases.rss", "REGULATORY", "US", "SEC", 1},
        {"fed-press", "Federal Reserve", "https://www.federalreserve.gov/feeds/press_all.xml", "REGULATORY", "US",
         "FEDERAL RESERVE", 1},
        {"un-news", "UN News", "https://news.un.org/feed/subscribe/en/news/all/rss.xml", "GEOPOLITICS", "GLOBAL", "UN",
         1},
        // (IMF News removed — endpoint serves an Akamai access-denied HTML page,
        //  not RSS. Fincept's macro coverage is already provided by Bloomberg /
        //  WSJ / Economist / IMF press is reachable via UN feeds.)

        // Tier 2 — Major Financial Media
        {"bloomberg-mkts", "Bloomberg Markets", "https://feeds.bloomberg.com/markets/news.rss", "MARKETS", "GLOBAL",
         "BLOOMBERG", 2},
        {"wsj-markets", "WSJ Markets", "https://feeds.a.dj.com/rss/RSSMarketsMain.xml", "MARKETS", "US", "WSJ", 2},
        {"wsj-world", "WSJ World", "https://feeds.a.dj.com/rss/RSSWorldNews.xml", "GEOPOLITICS", "GLOBAL", "WSJ", 2},
        {"marketwatch", "MarketWatch", "https://feeds.marketwatch.com/marketwatch/topstories/", "MARKETS", "US",
         "MARKETWATCH", 2},
        {"cnbc-finance", "CNBC Finance",
         "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114", "MARKETS", "US",
         "CNBC", 2},
        {"seekingalpha", "Seeking Alpha", "https://seekingalpha.com/market_currents.xml", "MARKETS", "US",
         "SEEKING ALPHA", 2},

        // Tier 2 — Global News
        {"bbc-world", "BBC World", "http://feeds.bbci.co.uk/news/world/rss.xml", "GEOPOLITICS", "GLOBAL", "BBC", 2},
        {"bbc-business", "BBC Business", "http://feeds.bbci.co.uk/news/business/rss.xml", "MARKETS", "GLOBAL", "BBC",
         2},
        {"aljazeera", "Al Jazeera", "https://www.aljazeera.com/xml/rss/all.xml", "GEOPOLITICS", "GLOBAL", "AL JAZEERA",
         2},
        {"nyt-world", "NYT World", "https://rss.nytimes.com/services/xml/rss/nyt/World.xml", "GEOPOLITICS", "GLOBAL",
         "NYT", 2},
        {"guardian-world", "Guardian World", "https://www.theguardian.com/world/rss", "GEOPOLITICS", "GLOBAL",
         "GUARDIAN", 2},
        {"france24", "France 24", "https://www.france24.com/en/rss", "GEOPOLITICS", "EU", "FRANCE 24", 2},

        // Tier 2 — Geopolitics & Defense
        {"foreignpolicy", "Foreign Policy", "https://foreignpolicy.com/feed/", "GEOPOLITICS", "GLOBAL",
         "FOREIGN POLICY", 2},
        // (defensenews.com/rss/ returns 404 — endpoint discontinued.)

        // Tier 2 — Energy & Commodities
        {"oilprice", "OilPrice.com", "https://oilprice.com/rss/main", "ENERGY", "GLOBAL", "OILPRICE", 2},
        // (Kitco RSS endpoint removed — kitco.com no longer publishes RSS.)

        // Tier 2 — Tech
        {"techcrunch", "TechCrunch", "https://techcrunch.com/feed/", "TECH", "GLOBAL", "TECHCRUNCH", 2},
        {"wired", "Wired", "https://www.wired.com/feed/rss", "TECH", "US", "WIRED", 2},

        // Tier 2 — Forex
        {"fxstreet", "FXStreet", "https://www.fxstreet.com/rss/news", "MARKETS", "GLOBAL", "FXSTREET", 2},

        // Tier 2 — Asia
        {"scmp", "South China Morning Post", "https://www.scmp.com/rss/91/feed", "GEOPOLITICS", "ASIA", "SCMP", 2},
        {"nikkei-asia", "Nikkei Asia", "https://asia.nikkei.com/rss/feed/nar", "MARKETS", "ASIA", "NIKKEI ASIA", 2},
        {"hindu-biz", "The Hindu Business", "https://www.thehindu.com/business/feeder/default.rss", "MARKETS", "INDIA",
         "THE HINDU", 2},

        // Tier 2 — MENA
        {"middle-east-eye", "Middle East Eye", "https://www.middleeasteye.net/rss", "GEOPOLITICS", "MENA",
         "MIDDLE EAST EYE", 2},

        // ── Additional feeds (29→80+) ──────────────────────────────────────

        // Tier 1 — Wire (additional)
        // (Reuters tech RSS removed — feed discontinued.)

        // Tier 2 — Major Financial (additional)
        {"cnbc-world", "CNBC World",
         "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100727362", "MARKETS", "GLOBAL",
         "CNBC", 2},
        {"cnbc-tech", "CNBC Technology",
         "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=19854910", "TECH", "US", "CNBC",
         2},
        {"investing-news", "Investing.com", "https://www.investing.com/rss/news.rss", "MARKETS", "GLOBAL",
         "INVESTING.COM", 2},
        {"economist", "The Economist", "https://www.economist.com/finance-and-economics/rss.xml", "ECONOMIC", "GLOBAL",
         "ECONOMIST", 2},

        // Tier 2 — Crypto
        {"coindesk", "CoinDesk", "https://www.coindesk.com/arc/outboundfeeds/rss/", "CRYPTO", "GLOBAL", "COINDESK", 2},
        {"cointelegraph", "CoinTelegraph", "https://cointelegraph.com/rss", "CRYPTO", "GLOBAL", "COINTELEGRAPH", 2},
        {"theblock", "The Block", "https://www.theblock.co/rss.xml", "CRYPTO", "GLOBAL", "THE BLOCK", 2},
        {"decrypt", "Decrypt", "https://decrypt.co/feed", "CRYPTO", "GLOBAL", "DECRYPT", 2},

        // Tier 1 — Central Banks & Regulators
        {"ecb-press", "ECB Press", "https://www.ecb.europa.eu/rss/press.html", "REGULATORY", "EU", "ECB", 1},
        {"boe-news", "Bank of England", "https://www.bankofengland.co.uk/rss/news", "REGULATORY", "UK", "BOE", 1},

        // Tier 2 — Commodities (additional)
        {"mining-com", "Mining.com", "https://www.mining.com/feed/", "MARKETS", "GLOBAL", "MINING.COM", 2},

        // Tier 2 — US Markets (additional)
        {"benzinga", "Benzinga", "https://www.benzinga.com/feed", "MARKETS", "US", "BENZINGA", 2},

        // Tier 2 — Europe
        {"dw-world", "Deutsche Welle", "https://rss.dw.com/rdf/rss-en-all", "GEOPOLITICS", "EU", "DW", 2},

        // Tier 2 — Asia & India (additional)
        {"livemint", "LiveMint", "https://www.livemint.com/rss/markets", "MARKETS", "INDIA", "LIVEMINT", 2},
        {"et-markets", "Economic Times", "https://economictimes.indiatimes.com/rssfeedstopstories.cms", "MARKETS",
         "INDIA", "ECONOMIC TIMES", 2},
        {"moneycontrol", "MoneyControl", "https://www.moneycontrol.com/rss/latestnews.xml", "MARKETS", "INDIA",
         "MONEYCONTROL", 2},
        {"channel-news-asia", "CNA", "https://www.channelnewsasia.com/rssfeeds/8395986", "MARKETS", "ASIA", "CNA", 2},

        // Tier 2 — Fintech
        {"finextra", "Finextra", "https://www.finextra.com/rss/headlines.aspx", "TECH", "GLOBAL", "FINEXTRA", 2},

        // Tier 3 — Economic / Macro
        {"zero-hedge", "ZeroHedge", "https://feeds.feedburner.com/zerohedge/feed", "ECONOMIC", "GLOBAL", "ZEROHEDGE",
         3},
        {"calculated-risk", "Calculated Risk", "https://feeds.feedburner.com/CalculatedRisk", "ECONOMIC", "US",
         "CALCULATED RISK", 3},
        {"wolfstreet", "Wolf Street", "https://wolfstreet.com/feed/", "ECONOMIC", "US", "WOLF STREET", 3},

        // Tier 3 — Defense & Security
        // (defenseone.com/rss/ returns 404 — endpoint discontinued.)
        {"bellingcat", "Bellingcat", "https://www.bellingcat.com/feed/", "GEOPOLITICS", "GLOBAL", "BELLINGCAT", 3},

        // Tier 3 — Tech (additional)
        {"arstechnica", "Ars Technica", "https://feeds.arstechnica.com/arstechnica/index", "TECH", "GLOBAL",
         "ARS TECHNICA", 3},
        {"theverge", "The Verge", "https://www.theverge.com/rss/index.xml", "TECH", "GLOBAL", "THE VERGE", 3},
        {"mit-tech", "MIT Tech Review", "https://www.technologyreview.com/feed/", "TECH", "GLOBAL", "MIT TECH REVIEW",
         3},

        // Tier 3 — ESG
        {"carbon-brief", "Carbon Brief", "https://www.carbonbrief.org/feed/", "ENERGY", "GLOBAL", "CARBON BRIEF", 3},

        // Tier 4 — Blogs & Aggregators
        {"hackernews", "Hacker News", "https://hnrss.org/frontpage", "TECH", "GLOBAL", "HACKER NEWS", 4},
        {"abnormal-returns", "Abnormal Returns", "https://abnormalreturns.com/feed/", "MARKETS", "US",
         "ABNORMAL RETURNS", 4},
        {"marginal-rev", "Marginal Revolution", "https://marginalrevolution.com/feed", "ECONOMIC", "GLOBAL",
         "MARGINAL REVOLUTION", 4},
    };
}

// ── Free helpers ────────────────────────────────────────────────────────────

QString priority_string(Priority p) {
    switch (p) {
        case Priority::FLASH:
            return "FLASH";
        case Priority::URGENT:
            return "URGENT";
        case Priority::BREAKING:
            return "BREAKING";
        case Priority::ROUTINE:
            return "ROUTINE";
    }
    return "ROUTINE";
}

QString sentiment_string(Sentiment s) {
    switch (s) {
        case Sentiment::BULLISH:
            return "BULLISH";
        case Sentiment::BEARISH:
            return "BEARISH";
        case Sentiment::NEUTRAL:
            return "NEUTRAL";
    }
    return "NEUTRAL";
}

QString impact_string(Impact i) {
    switch (i) {
        case Impact::HIGH:
            return "HIGH";
        case Impact::MEDIUM:
            return "MEDIUM";
        case Impact::LOW:
            return "LOW";
    }
    return "LOW";
}

QString priority_color(Priority p) {
    switch (p) {
        case Priority::FLASH:
            return "#dc2626";
        case Priority::URGENT:
            return "#d97706";
        case Priority::BREAKING:
            return "#ca8a04";
        case Priority::ROUTINE:
            return "#525252";
    }
    return "#525252";
}

QString sentiment_color(Sentiment s) {
    switch (s) {
        case Sentiment::BULLISH:
            return "#16a34a";
        case Sentiment::BEARISH:
            return "#dc2626";
        case Sentiment::NEUTRAL:
            return "#ca8a04";
    }
    return "#ca8a04";
}

QString relative_time(int64_t unix_ts) {
    if (unix_ts <= 0)
        return {};
    auto now = QDateTime::currentSecsSinceEpoch();
    auto d = now - unix_ts;
    if (d < 0)
        return "now";
    if (d < 60)
        return QString("%1s").arg(d);
    if (d < 3600)
        return QString("%1m").arg(d / 60);
    if (d < 86400)
        return QString("%1h").arg(d / 3600);
    return QString("%1d").arg(d / 86400);
}

QString threat_level_string(ThreatLevel t) {
    switch (t) {
        case ThreatLevel::CRITICAL:
            return "CRITICAL";
        case ThreatLevel::HIGH:
            return "HIGH";
        case ThreatLevel::MEDIUM:
            return "MEDIUM";
        case ThreatLevel::LOW:
            return "LOW";
        case ThreatLevel::INFO:
            return "INFO";
    }
    return "INFO";
}

QString threat_level_color(ThreatLevel t) {
    switch (t) {
        case ThreatLevel::CRITICAL:
            return "#dc2626";
        case ThreatLevel::HIGH:
            return "#f97316";
        case ThreatLevel::MEDIUM:
            return "#eab308";
        case ThreatLevel::LOW:
            return "#22c55e";
        case ThreatLevel::INFO:
            return "#525252";
    }
    return "#525252";
}

Priority priority_from_string(const QString& s) {
    if (s == "FLASH")
        return Priority::FLASH;
    if (s == "URGENT")
        return Priority::URGENT;
    if (s == "BREAKING")
        return Priority::BREAKING;
    return Priority::ROUTINE;
}

Sentiment sentiment_from_string(const QString& s) {
    if (s == "BULLISH")
        return Sentiment::BULLISH;
    if (s == "BEARISH")
        return Sentiment::BEARISH;
    return Sentiment::NEUTRAL;
}

Impact impact_from_string(const QString& s) {
    if (s == "HIGH")
        return Impact::HIGH;
    if (s == "MEDIUM")
        return Impact::MEDIUM;
    return Impact::LOW;
}

// ── DataHub producer wiring ─────────────────────────────────────────────────

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
