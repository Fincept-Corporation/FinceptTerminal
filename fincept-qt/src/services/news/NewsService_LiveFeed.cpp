// src/services/news/NewsService_LiveFeed.cpp
//
// WebSocket live-feed lifecycle: connect_live_feed / disconnect_live_feed /
// is_live_connected (both the HAS_QT_WEBSOCKETS branch and the stub fallback).
//
// Part of the partial-class split of NewsService.cpp.

#include "services/news/NewsService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/cache/CacheManager.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"

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

// 10s before WebSocket reconnect.
static constexpr int kWsReconnectDelayMs = 10000;

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

} // namespace fincept::services
