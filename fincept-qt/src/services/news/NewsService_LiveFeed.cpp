// src/services/news/NewsService_LiveFeed.cpp
//
// WebSocket live-feed lifecycle: connect_live_feed / disconnect_live_feed /
// is_live_connected (both the HAS_QT_WEBSOCKETS branch and the stub fallback).
//
// Part of the partial-class split of NewsService.cpp.

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "network/http/HttpClient.h"
#include "services/news/NewsService.h"
#include "storage/cache/CacheManager.h"

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

// Capability switch for the live feed — the one place to turn it on. The
// default URL is wss://<api host>/ws/news, and api.fincept.in has no WebSocket
// route at all: every attempt was refused, the badge never lit, and the client
// reconnected for as long as the News screen was open. Set to true once the
// backend serves that route and pushes JSON text frames
// {headline|title, summary|description, source, link|url, category, timestamp, tier}.
static constexpr bool kNewsLiveFeedAvailable = false;

bool NewsService::live_feed_available() {
#ifdef HAS_QT_WEBSOCKETS
    constexpr bool kNewsHasWebSockets = true;
#else
    constexpr bool kNewsHasWebSockets = false;
#endif
    return kNewsLiveFeedAvailable && kNewsHasWebSockets;
}

#ifdef HAS_QT_WEBSOCKETS
// First WebSocket reconnect delay; doubles per consecutive failure up to the cap
// so an unreachable endpoint is not hammered every 10 s for the whole session.
static constexpr int kWsReconnectDelayMs = 10000;
static constexpr int kWsReconnectMaxDelayMs = 300000;
// Consecutive failed reconnects before giving up until the next explicit
// connect (screen re-entry or a LIVE click): ~20 min of backoff.
static constexpr int kWsMaxReconnectAttempts = 8;

// Cap on the in-memory list that live pushes extend (newest first).
static constexpr int kNewsLiveMaxRetained = 3000;

// A refused handshake (HTTP 4xx: no such route, forbidden) is permanent;
// retrying it only loops. Qt 6.8 reports it as ConnectionRefusedError with the
// HTTP status in the error text ("...processHandshake: Unhandled http status
// code: 404 (Not Found)."); a TCP-level refusal ("Connection refused") is the
// same enum without a status code and stays retryable.
static bool news_ws_handshake_refused(QAbstractSocket::SocketError error, const QString& error_text) {
    static const QRegularExpression kNewsWsHttp4xx(QStringLiteral("\\b4\\d\\d\\b"));
    return error == QAbstractSocket::ConnectionRefusedError && kNewsWsHttp4xx.match(error_text).hasMatch();
}

void NewsService::connect_live_feed(const QString& ws_url) {
    if (!live_feed_available())
        return; // no live-feed server exists — RSS polling carries the news
    if (live_ws_)
        return; // already connected

    // Every handler below is pinned to this epoch: a socket that has since been
    // disconnect_live_feed()'d (or replaced) must not flip the connection state
    // or re-open itself.
    const quint64 epoch = ++live_epoch_;
    live_reconnect_attempts_ = 0;
    live_ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(live_ws_, &QWebSocket::connected, this, [this, epoch]() {
        if (epoch != live_epoch_)
            return;
        live_connected_ = true;
        live_reconnect_attempts_ = 0;
        LOG_INFO("NewsService", "WebSocket live feed connected");
        emit live_state_changed(true);
    });

    // Used by both `disconnected` and `errorOccurred` (a failed connect may only
    // raise the latter). The token makes a double signal arm a single attempt.
    auto schedule_reconnect = [this, epoch]() {
        if (epoch != live_epoch_ || !live_ws_)
            return;
        if (live_reconnect_attempts_ >= kWsMaxReconnectAttempts) {
            LOG_WARN("NewsService", QString("WebSocket live feed: giving up after %1 failed reconnects")
                                        .arg(live_reconnect_attempts_));
            disconnect_live_feed();
            return;
        }
        const int delay_ms =
            std::min(kWsReconnectMaxDelayMs, kWsReconnectDelayMs << std::min(live_reconnect_attempts_, 5));
        const quint64 token = ++live_reconnect_token_;
        QTimer::singleShot(delay_ms, this, [this, epoch, token]() {
            if (epoch != live_epoch_ || token != live_reconnect_token_)
                return;
            if (live_ws_ && !live_connected_) {
                ++live_reconnect_attempts_; // counted per attempt actually made (error + disconnected both schedule)
                live_ws_->open(live_ws_->requestUrl());
            }
        });
    };

    connect(live_ws_, &QWebSocket::disconnected, this, [this, epoch, schedule_reconnect]() {
        if (epoch != live_epoch_)
            return;
        const bool was_connected = live_connected_;
        live_connected_ = false;
        if (was_connected) {
            LOG_WARN("NewsService", "WebSocket live feed disconnected");
            emit live_state_changed(false);
        }
        schedule_reconnect();
    });

    connect(live_ws_, &QWebSocket::errorOccurred, this,
            [this, epoch, schedule_reconnect](QAbstractSocket::SocketError error) {
                if (epoch != live_epoch_)
                    return;
                const QString text = live_ws_ ? live_ws_->errorString() : QString();
                LOG_WARN("NewsService", "WebSocket live feed error: " + text);
                if (news_ws_handshake_refused(error, text)) {
                    LOG_WARN("NewsService", "WebSocket live feed: server refused the handshake — not retrying");
                    disconnect_live_feed(); // bumps the epoch: the trailing `disconnected` is ignored
                    return;
                }
                schedule_reconnect();
            });

    connect(live_ws_, &QWebSocket::textMessageReceived, this, [this, epoch](const QString& msg) {
        if (epoch != live_epoch_)
            return;
        // Parse incoming JSON article
        auto doc = QJsonDocument::fromJson(msg.toUtf8());
        if (!doc.isObject())
            return;

        auto obj = doc.object();
        NewsArticle article;
        article.headline = obj["headline"].toString(obj["title"].toString());
        if (article.headline.isEmpty())
            return;
        article.summary = obj["summary"].toString(obj["description"].toString());
        article.source = obj["source"].toString();
        article.link = obj["link"].toString(obj["url"].toString());
        if (!is_web_url(article.link)) // untrusted — web URLs only
            article.link.clear();
        article.category = obj["category"].toString("MARKETS");
        const int64_t now_ts = QDateTime::currentSecsSinceEpoch();
        int64_t ts = obj["timestamp"].toInteger(now_ts);
        if (ts > 100000000000LL) // epoch milliseconds
            ts /= 1000;
        article.sort_ts = (ts <= 0 || ts > now_ts + 300) ? now_ts : ts;
        article.time = QDateTime::fromSecsSinceEpoch(article.sort_ts).toString("MMM dd, HH:mm");
        article.tier = obj["tier"].toInt(2);
        // Same id scheme as the RSS path, so a story pushed live and later seen
        // in a feed is one article, not two. (A server-supplied id is ignored.)
        article.id = stable_article_id(article.link, article.source, article.headline);

        enrich_article(article);

        // Extend the current list. It used to be re-read from the cache and
        // written back with only `articles_partial` emitted — which nothing
        // listens to outside a refresh — so live articles never reached the
        // screen and were wiped by the next RSS refresh.
        if (latest_articles_.isEmpty()) {
            const QVariant cv = fincept::CacheManager::instance().get("news:articles");
            if (!cv.isNull())
                latest_articles_ = deserialize_articles(cv.toString());
        }
        for (int i = 0; i < latest_articles_.size(); ++i) {
            if (latest_articles_[i].id == article.id) {
                latest_articles_.removeAt(i);
                break;
            }
        }
        latest_articles_.prepend(article);
        if (latest_articles_.size() > kNewsLiveMaxRetained)
            latest_articles_.resize(kNewsLiveMaxRetained);

        fincept::CacheManager::instance().put("news:articles", QVariant(serialize_articles(latest_articles_)),
                                              kArticleCacheTtlSec, "news");
        persist_articles_async({article});
        emit articles_updated(latest_articles_);
        publish_articles_to_hub(latest_articles_);
        LOG_DEBUG("NewsService", logf("Live article received", {{"id", article.id}, {"source", article.source}}));
    });

    QString ws_base = fincept::AppConfig::instance().api_base_url();
    ws_base.replace(QStringLiteral("https://"), QStringLiteral("wss://"));
    ws_base.replace(QStringLiteral("http://"), QStringLiteral("ws://"));
    QString url = ws_url.isEmpty() ? (ws_base + QStringLiteral("/ws/news")) : ws_url;
    live_ws_->open(QUrl(url));
}

void NewsService::disconnect_live_feed() {
    if (!live_ws_)
        return;
    // Invalidate this socket's handlers and any armed reconnect timer first.
    ++live_epoch_;
    ++live_reconnect_token_;
    auto* ws = live_ws_;
    live_ws_ = nullptr;
    live_connected_ = false;
    ws->close();
    ws->deleteLater();
    emit live_state_changed(false);
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

} // namespace fincept::services
