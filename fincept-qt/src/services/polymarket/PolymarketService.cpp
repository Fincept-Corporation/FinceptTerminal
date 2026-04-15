#include "services/polymarket/PolymarketService.h"

#include "core/logging/Logger.h"
#include "storage/cache/CacheManager.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::services::polymarket {

// ── API base URLs ────────────────────────────────────────────────────────────

static const QString GAMMA_BASE = QStringLiteral("https://gamma-api.polymarket.com");
static const QString CLOB_BASE = QStringLiteral("https://clob.polymarket.com");
static const QString DATA_BASE = QStringLiteral("https://data-api.polymarket.com");

// ── JSON helpers ─────────────────────────────────────────────────────────────

static QJsonArray parse_json_arr(const QJsonValue& val) {
    if (val.isArray())
        return val.toArray();
    if (val.isString()) {
        auto doc = QJsonDocument::fromJson(val.toString().toUtf8());
        if (doc.isArray())
            return doc.array();
    }
    return {};
}

// ── Existing type parsing ───────────────────────────────────────────────────

Market Market::from_json(const QJsonObject& obj) {
    Market m;
    m.id = obj["id"].toInt();
    m.question = obj["question"].toString();
    m.slug = obj["slug"].toString();
    m.description = obj["description"].toString();
    m.condition_id = obj["conditionId"].toString();
    m.image = obj["image"].toString();
    m.volume = obj["volume"].toDouble();
    m.liquidity = obj["liquidity"].toDouble();
    m.active = obj["active"].toBool();
    m.closed = obj["closed"].toBool();
    m.end_date = obj["endDate"].toString();
    m.category = obj["category"].toString();
    m.event_id = obj["eventId"].toInt();

    auto outcomes_arr = parse_json_arr(obj["outcomes"]);
    auto prices_arr = parse_json_arr(obj["outcomePrices"]);
    for (int i = 0; i < outcomes_arr.size(); ++i) {
        Outcome o;
        o.name = outcomes_arr[i].toString();
        o.price = (i < prices_arr.size()) ? prices_arr[i].toDouble() : 0.0;
        m.outcomes.append(o);
    }

    auto tokens_arr = parse_json_arr(obj["clobTokenIds"]);
    for (const auto& t : tokens_arr) {
        m.clob_token_ids.append(t.toString());
    }

    auto tags_arr = obj["tags"].toArray();
    for (const auto& t : tags_arr) {
        auto tag_obj = t.toObject();
        QString label = tag_obj["label"].toString();
        if (!label.isEmpty())
            m.tags.append(label);
    }
    if (m.category.isEmpty() && !m.tags.isEmpty()) {
        m.category = m.tags.first();
    }

    return m;
}

Event Event::from_json(const QJsonObject& obj) {
    Event e;
    e.id = obj["id"].toInt();
    e.title = obj["title"].toString();
    e.slug = obj["slug"].toString();
    e.description = obj["description"].toString();
    e.image = obj["image"].toString();
    e.volume = obj["volume"].toDouble();
    e.liquidity = obj["liquidity"].toDouble();
    e.active = obj["active"].toBool();
    e.closed = obj["closed"].toBool();
    e.end_date = obj["endDate"].toString();

    auto tags_arr = obj["tags"].toArray();
    for (const auto& t : tags_arr) {
        auto tag_obj = t.toObject();
        e.tags.append(tag_obj["label"].toString());
    }
    if (!e.tags.isEmpty())
        e.category = e.tags.first();

    auto markets_arr = obj["markets"].toArray();
    for (const auto& m : markets_arr) {
        e.markets.append(Market::from_json(m.toObject()));
    }

    return e;
}

OrderBook OrderBook::from_json(const QJsonObject& obj) {
    OrderBook book;
    book.market = obj["market"].toString();
    book.asset_id = obj["asset_id"].toString();
    book.tick_size = obj["tick_size"].toDouble();
    book.min_order_size = obj["min_order_size"].toDouble();
    book.neg_risk = obj["neg_risk"].toBool();

    for (const auto& b : obj["bids"].toArray()) {
        auto bo = b.toObject();
        book.bids.append({bo["price"].toDouble(), bo["size"].toDouble()});
    }
    for (const auto& a : obj["asks"].toArray()) {
        auto ao = a.toObject();
        book.asks.append({ao["price"].toDouble(), ao["size"].toDouble()});
    }
    return book;
}

PriceHistory PriceHistory::from_json(const QJsonObject& obj) {
    PriceHistory ph;
    auto history_arr = obj["history"].toArray();
    ph.points.reserve(history_arr.size());
    for (const auto& pt : history_arr) {
        auto po = pt.toObject();
        PricePoint pp;
        pp.timestamp = static_cast<int64_t>(po["t"].toVariant().toLongLong());
        pp.price = po["p"].toDouble();
        ph.points.append(pp);
    }
    return ph;
}

Trade Trade::from_json(const QJsonObject& obj) {
    Trade t;
    t.side = obj["side"].toString();
    t.price = obj["price"].toDouble();
    t.size = obj["size"].toDouble();
    t.timestamp = static_cast<int64_t>(obj["timestamp"].toVariant().toLongLong());
    t.condition_id = obj["conditionId"].toString();
    return t;
}

// ── New type parsing ────────────────────────────────────────────────────────

TopHolder TopHolder::from_json(const QJsonObject& obj) {
    TopHolder h;
    h.address = obj["proxyWallet"].toString();
    if (h.address.isEmpty())
        h.address = obj["address"].toString();
    h.display_name = obj["name"].toString();
    if (h.display_name.isEmpty() && !h.address.isEmpty())
        h.display_name = h.address.left(6) + "..." + h.address.right(4);
    h.position_size = obj["size"].toDouble();
    h.entry_price = obj["avgPrice"].toDouble();
    h.rank = obj["rank"].toInt();
    return h;
}

LeaderboardEntry LeaderboardEntry::from_json(const QJsonObject& obj) {
    LeaderboardEntry e;
    e.address = obj["address"].toString();
    if (e.address.isEmpty())
        e.address = obj["proxyWallet"].toString();
    e.display_name = obj["name"].toString();
    if (e.display_name.isEmpty())
        e.display_name = obj["pseudonym"].toString();
    if (e.display_name.isEmpty() && !e.address.isEmpty())
        e.display_name = e.address.left(6) + "..." + e.address.right(4);
    e.profile_image = obj["profileImage"].toString();
    e.pnl = obj["pnl"].toDouble();
    if (e.pnl == 0)
        e.pnl = obj["cashPnl"].toDouble();
    e.volume = obj["volume"].toDouble();
    e.num_trades = obj["numTrades"].toInt();
    e.rank = obj["rank"].toInt();
    return e;
}

Activity Activity::from_json(const QJsonObject& obj) {
    Activity a;
    a.type = obj["type"].toString();
    a.address = obj["proxyWallet"].toString();
    if (a.address.isEmpty())
        a.address = obj["address"].toString();
    a.amount = obj["size"].toDouble();
    a.usdc_size = obj["usdcSize"].toDouble();
    a.price = obj["price"].toDouble();
    a.timestamp = static_cast<int64_t>(obj["timestamp"].toVariant().toLongLong());
    a.condition_id = obj["conditionId"].toString();
    a.title = obj["title"].toString();
    a.outcome = obj["outcome"].toString();
    return a;
}

Comment Comment::from_json(const QJsonObject& obj) {
    Comment c;
    c.id = obj["id"].toInt();
    c.body = obj["body"].toString();
    if (c.body.isEmpty())
        c.body = obj["content"].toString();
    c.author = obj["name"].toString();
    if (c.author.isEmpty())
        c.author = obj["author"].toString();
    c.author_address = obj["address"].toString();
    c.created_at = static_cast<int64_t>(obj["createdAt"].toVariant().toLongLong());
    c.parent_id = obj["parentId"].toInt();
    c.likes = obj["likes"].toInt();
    return c;
}

Series Series::from_json(const QJsonObject& obj) {
    Series s;
    s.id = obj["id"].toInt();
    s.title = obj["title"].toString();
    s.slug = obj["slug"].toString();
    for (const auto& e : obj["events"].toArray()) {
        s.events.append(Event::from_json(e.toObject()));
    }
    return s;
}

Team Team::from_json(const QJsonObject& obj) {
    Team t;
    t.name = obj["name"].toString();
    t.league = obj["league"].toString();
    t.abbreviation = obj["abbreviation"].toString();
    t.image = obj["image"].toString();
    return t;
}

OpenInterest OpenInterest::from_json(const QJsonObject& obj) {
    OpenInterest oi;
    oi.condition_id = obj["conditionId"].toString();
    if (oi.condition_id.isEmpty())
        oi.condition_id = obj["market"].toString();
    oi.open_interest = obj["openInterest"].toDouble();
    if (oi.open_interest == 0)
        oi.open_interest = obj["value"].toDouble();
    return oi;
}

LiveVolume LiveVolume::from_json(const QJsonObject& obj) {
    LiveVolume lv;
    lv.event_id = obj["eventId"].toString();
    if (lv.event_id.isEmpty())
        lv.event_id = obj["event_id"].toString();
    lv.volume_24h = obj["volume"].toDouble();
    return lv;
}

WsMarketUpdate WsMarketUpdate::from_json(const QJsonObject& obj) {
    WsMarketUpdate u;
    u.asset_id = obj["asset_id"].toString();
    u.price = obj["price"].toDouble();
    u.timestamp = static_cast<int64_t>(obj["timestamp"].toVariant().toLongLong());
    for (const auto& b : obj["bids"].toArray()) {
        auto bo = b.toObject();
        u.bids.append({bo["price"].toDouble(), bo["size"].toDouble()});
    }
    for (const auto& a : obj["asks"].toArray()) {
        auto ao = a.toObject();
        u.asks.append({ao["price"].toDouble(), ao["size"].toDouble()});
    }
    return u;
}

// ── Singleton ────────────────────────────────────────────────────────────────

PolymarketService& PolymarketService::instance() {
    static PolymarketService s;
    return s;
}

PolymarketService::PolymarketService() : QObject(nullptr) {
    gamma_nam_ = new QNetworkAccessManager(this);
    clob_nam_ = new QNetworkAccessManager(this);
    data_nam_ = new QNetworkAccessManager(this);
}

// ── Generic HTTP helpers ─────────────────────────────────────────────────────

void PolymarketService::get_json(QNetworkAccessManager* nam, const QString& url, JsonCallback on_success,
                                 const QString& error_ctx) {
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("Accept", "application/json");

    QPointer<PolymarketService> self = this;
    auto* reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [self, reply, on_success, error_ctx]() {
        reply->deleteLater();
        if (!self)
            return;

        if (reply->error() != QNetworkReply::NoError) {
            QString msg = reply->errorString();
            LOG_ERROR("Polymarket", error_ctx + " request failed: " + msg);
            emit self->request_error(error_ctx, msg);
            return;
        }

        auto data = reply->readAll();
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(data, &err);
        if (doc.isNull()) {
            QString msg = "JSON parse error: " + err.errorString();
            LOG_ERROR("Polymarket", error_ctx + " " + msg);
            emit self->request_error(error_ctx, msg);
            return;
        }

        on_success(doc);
    });
}

void PolymarketService::get_gamma(const QString& path, JsonCallback on_success, const QString& error_ctx) {
    get_json(gamma_nam_, GAMMA_BASE + path, std::move(on_success), error_ctx);
}

void PolymarketService::get_clob(const QString& path, JsonCallback on_success, const QString& error_ctx) {
    get_json(clob_nam_, CLOB_BASE + path, std::move(on_success), error_ctx);
}

void PolymarketService::get_data(const QString& path, JsonCallback on_success, const QString& error_ctx) {
    get_json(data_nam_, DATA_BASE + path, std::move(on_success), error_ctx);
}

// ── Gamma API ────────────────────────────────────────────────────────────────

void PolymarketService::fetch_markets(const QString& sort_by, int limit, int offset, bool closed) {
    const QString cache_key =
        QString("polymarket:markets:%1:%2:%3:%4").arg(sort_by).arg(limit).arg(offset).arg(closed ? 1 : 0);
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        QVector<Market> result;
        result.reserve(arr.size());
        for (const auto& v : arr)
            result.append(Market::from_json(v.toObject()));
        emit markets_ready(result);
        return;
    }

    QString path = QString("/markets?closed=%1&limit=%2&offset=%3&order=%4&ascending=false")
                       .arg(closed ? "true" : "false")
                       .arg(limit)
                       .arg(offset)
                       .arg(sort_by);

    get_gamma(
        path,
        [this, cache_key](const QJsonDocument& doc) {
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                kMarketsTtlSec, "polymarket");
            QVector<Market> result;
            result.reserve(arr.size());
            for (const auto& v : arr)
                result.append(Market::from_json(v.toObject()));
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " markets");
            emit markets_ready(result);
        },
        "FetchMarkets");
}

void PolymarketService::fetch_markets_by_tag(const QString& tag, const QString& sort_by, int limit, int offset) {
    QString path = QString("/markets?tag=%1&closed=false&limit=%2&offset=%3&order=%4&ascending=false")
                       .arg(QUrl::toPercentEncoding(tag))
                       .arg(limit)
                       .arg(offset)
                       .arg(sort_by);

    get_gamma(
        path,
        [this](const QJsonDocument& doc) {
            QVector<Market> result;
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            result.reserve(arr.size());
            for (const auto& v : arr)
                result.append(Market::from_json(v.toObject()));
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " markets by tag");
            emit markets_ready(result);
        },
        "FetchMarketsByTag");
}

void PolymarketService::fetch_market_by_id(int id) {
    get_gamma(
        "/markets/" + QString::number(id),
        [this](const QJsonDocument& doc) {
            if (doc.isObject()) {
                emit market_detail_ready(Market::from_json(doc.object()));
            } else if (doc.isArray() && !doc.array().isEmpty()) {
                emit market_detail_ready(Market::from_json(doc.array()[0].toObject()));
            }
        },
        "FetchMarketById");
}

void PolymarketService::search_markets(const QString& query, int limit) {
    QString path = "/markets?_q=" + QUrl::toPercentEncoding(query) + "&limit=" + QString::number(limit);

    get_gamma(
        path,
        [this](const QJsonDocument& doc) {
            QVector<Market> result;
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            result.reserve(arr.size());
            for (const auto& v : arr)
                result.append(Market::from_json(v.toObject()));
            LOG_INFO("Polymarket", "Search returned " + QString::number(result.size()) + " markets");
            emit markets_ready(result);
        },
        "SearchMarkets");
}

void PolymarketService::unified_search(const QString& query) {
    // Use the Gamma search endpoint that returns markets and events
    QString path = "/search?query=" + QUrl::toPercentEncoding(query) + "&limit=20";

    get_gamma(
        path,
        [this](const QJsonDocument& doc) {
            QVector<Market> markets;
            QVector<Event> events;

            if (doc.isObject()) {
                auto obj = doc.object();
                for (const auto& v : obj["markets"].toArray())
                    markets.append(Market::from_json(v.toObject()));
                for (const auto& v : obj["events"].toArray())
                    events.append(Event::from_json(v.toObject()));
            } else if (doc.isArray()) {
                // Fallback: treat as market array
                for (const auto& v : doc.array())
                    markets.append(Market::from_json(v.toObject()));
            }
            emit search_results_ready(markets, events);
        },
        "UnifiedSearch");
}

void PolymarketService::fetch_events(const QString& sort_by, int limit, int offset, bool closed) {
    const QString cache_key =
        QString("polymarket:events:%1:%2:%3:%4").arg(sort_by).arg(limit).arg(offset).arg(closed ? 1 : 0);
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        QVector<Event> result;
        result.reserve(arr.size());
        for (const auto& v : arr)
            result.append(Event::from_json(v.toObject()));
        emit events_ready(result);
        return;
    }

    QString path = QString("/events?closed=%1&limit=%2&offset=%3&order=%4&ascending=false")
                       .arg(closed ? "true" : "false")
                       .arg(limit)
                       .arg(offset)
                       .arg(sort_by);

    get_gamma(
        path,
        [this, cache_key](const QJsonDocument& doc) {
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                kEventsTtlSec, "polymarket");
            QVector<Event> result;
            result.reserve(arr.size());
            for (const auto& v : arr)
                result.append(Event::from_json(v.toObject()));
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " events");
            emit events_ready(result);
        },
        "FetchEvents");
}

void PolymarketService::fetch_event_by_id(int id) {
    get_gamma(
        "/events/" + QString::number(id),
        [this](const QJsonDocument& doc) {
            if (doc.isObject()) {
                emit event_detail_ready(Event::from_json(doc.object()));
            } else if (doc.isArray() && !doc.array().isEmpty()) {
                emit event_detail_ready(Event::from_json(doc.array()[0].toObject()));
            }
        },
        "FetchEventById");
}

void PolymarketService::fetch_related_markets(int event_id) {
    get_gamma(
        "/events/" + QString::number(event_id),
        [this](const QJsonDocument& doc) {
            QVector<Market> result;
            if (doc.isObject()) {
                auto markets_arr = doc.object()["markets"].toArray();
                for (const auto& m : markets_arr)
                    result.append(Market::from_json(m.toObject()));
            }
            emit related_markets_ready(result);
        },
        "FetchRelatedMarkets");
}

void PolymarketService::fetch_tags() {
    const QVariant cached = fincept::CacheManager::instance().get("polymarket:tags");
    if (!cached.isNull()) {
        QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        QVector<Tag> result;
        for (const auto& v : arr) {
            auto obj = v.toObject();
            Tag tag;
            tag.label = obj["label"].toString();
            tag.slug = obj["slug"].toString();
            if (!tag.label.isEmpty())
                result.append(tag);
        }
        emit tags_ready(result);
        return;
    }

    get_gamma(
        "/tags?limit=50",
        [this](const QJsonDocument& doc) {
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            fincept::CacheManager::instance().put(
                "polymarket:tags", QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                kTagsTtlSec, "polymarket");
            QVector<Tag> result;
            for (const auto& v : arr) {
                auto obj = v.toObject();
                Tag tag;
                tag.label = obj["label"].toString();
                tag.slug = obj["slug"].toString();
                if (!tag.label.isEmpty())
                    result.append(tag);
            }
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " tags");
            emit tags_ready(result);
        },
        "FetchTags");
}

void PolymarketService::fetch_comments(const QString& market_slug, int limit) {
    QString path = "/comments?asset=" + QUrl::toPercentEncoding(market_slug) + "&limit=" + QString::number(limit);

    get_gamma(
        path,
        [this](const QJsonDocument& doc) {
            QVector<Comment> result;
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            for (const auto& v : arr)
                result.append(Comment::from_json(v.toObject()));
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " comments");
            emit comments_ready(result);
        },
        "FetchComments");
}

void PolymarketService::fetch_teams(const QString& league) {
    QString path = "/teams?limit=50";
    if (!league.isEmpty())
        path += "&league=" + QUrl::toPercentEncoding(league);

    get_gamma(
        path,
        [this](const QJsonDocument& doc) {
            QVector<Team> result;
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            for (const auto& v : arr)
                result.append(Team::from_json(v.toObject()));
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " teams");
            emit teams_ready_list(result);
        },
        "FetchTeams");
}

// ── CLOB API ─────────────────────────────────────────────────────────────────

void PolymarketService::fetch_order_book(const QString& token_id) {
    get_clob(
        "/book?token_id=" + token_id,
        [this](const QJsonDocument& doc) {
            if (doc.isObject()) {
                emit order_book_ready(OrderBook::from_json(doc.object()));
            }
        },
        "OrderBook");
}

void PolymarketService::fetch_price_history(const QString& token_id, const QString& interval, int fidelity) {
    QString path =
        "/prices-history?market=" + token_id + "&interval=" + interval + "&fidelity=" + QString::number(fidelity);

    get_clob(
        path,
        [this](const QJsonDocument& doc) {
            if (doc.isObject()) {
                auto history = PriceHistory::from_json(doc.object());
                LOG_INFO("Polymarket", "Price history: " + QString::number(history.points.size()) + " points");
                emit price_history_ready(history);
            }
        },
        "PriceHistory");
}

void PolymarketService::fetch_price_summary(const QString& token_id) {
    struct Accumulator {
        PriceSummary summary;
        int pending = 3;
    };
    auto acc = std::make_shared<Accumulator>();
    QPointer<PolymarketService> self = this;

    auto maybe_emit = [self, acc]() {
        if (--acc->pending == 0 && self) {
            emit self->price_summary_ready(acc->summary);
        }
    };

    get_clob(
        "/midpoint?token_id=" + token_id,
        [acc, maybe_emit](const QJsonDocument& doc) {
            acc->summary.midpoint = doc.object()["mid"].toDouble();
            maybe_emit();
        },
        "Midpoint");

    get_clob(
        "/spread?token_id=" + token_id,
        [acc, maybe_emit](const QJsonDocument& doc) {
            acc->summary.spread = doc.object()["spread"].toDouble();
            maybe_emit();
        },
        "Spread");

    get_clob(
        "/last-trade-price?token_id=" + token_id,
        [acc, maybe_emit](const QJsonDocument& doc) {
            acc->summary.last_trade_price = doc.object()["price"].toDouble();
            maybe_emit();
        },
        "LastTradePrice");
}

// ── Data API ─────────────────────────────────────────────────────────────────

void PolymarketService::fetch_trades(const QString& condition_id, int limit) {
    QString path = "/trades?market=" + condition_id + "&limit=" + QString::number(limit);

    get_data(
        path,
        [this](const QJsonDocument& doc) {
            QVector<Trade> result;
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            result.reserve(arr.size());
            for (const auto& v : arr)
                result.append(Trade::from_json(v.toObject()));
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " trades");
            emit trades_ready(result);
        },
        "FetchTrades");
}

void PolymarketService::fetch_top_holders(const QString& condition_id, int limit) {
    QString path = "/top-holders?market=" + condition_id + "&limit=" + QString::number(limit);

    get_data(
        path,
        [this](const QJsonDocument& doc) {
            QVector<TopHolder> result;
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            // Some endpoints wrap in data object
            if (arr.isEmpty() && doc.isObject() && doc.object().contains("data"))
                arr = doc.object()["data"].toArray();
            int rank = 1;
            for (const auto& v : arr) {
                auto h = TopHolder::from_json(v.toObject());
                if (h.rank == 0)
                    h.rank = rank++;
                result.append(h);
            }
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " top holders");
            emit top_holders_ready(result);
        },
        "FetchTopHolders");
}

void PolymarketService::fetch_leaderboard(int limit) {
    const QString cache_key = QString("polymarket:leaderboard:%1").arg(limit);
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        QJsonArray arr = QJsonDocument::fromJson(cached.toString().toUtf8()).array();
        QVector<LeaderboardEntry> result;
        int rank = 1;
        for (const auto& v : arr) {
            auto e = LeaderboardEntry::from_json(v.toObject());
            if (e.rank == 0)
                e.rank = rank++;
            result.append(e);
        }
        emit leaderboard_ready(result);
        return;
    }

    QString path = "/leaderboard?limit=" + QString::number(limit);

    get_data(
        path,
        [this, cache_key](const QJsonDocument& doc) {
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            if (arr.isEmpty() && doc.isObject() && doc.object().contains("data"))
                arr = doc.object()["data"].toArray();
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                kLeaderboardTtlSec, "polymarket");
            QVector<LeaderboardEntry> result;
            int rank = 1;
            for (const auto& v : arr) {
                auto e = LeaderboardEntry::from_json(v.toObject());
                if (e.rank == 0)
                    e.rank = rank++;
                result.append(e);
            }
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " leaderboard entries");
            emit leaderboard_ready(result);
        },
        "FetchLeaderboard");
}

void PolymarketService::fetch_activity(const QString& condition_id, int limit) {
    QString path = "/activity?market=" + condition_id + "&limit=" + QString::number(limit);

    get_data(
        path,
        [this](const QJsonDocument& doc) {
            QVector<Activity> result;
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            if (arr.isEmpty() && doc.isObject() && doc.object().contains("data"))
                arr = doc.object()["data"].toArray();
            for (const auto& v : arr)
                result.append(Activity::from_json(v.toObject()));
            LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " activities");
            emit activity_ready(result);
        },
        "FetchActivity");
}

void PolymarketService::fetch_live_volume(const QString& event_id) {
    QString path = "/live-volume?event_id=" + event_id;

    get_data(
        path,
        [this](const QJsonDocument& doc) {
            if (doc.isObject()) {
                emit live_volume_ready(LiveVolume::from_json(doc.object()));
            }
        },
        "FetchLiveVolume");
}

void PolymarketService::fetch_open_interest(const QStringList& condition_ids) {
    QString ids = condition_ids.join(",");
    QString path = "/open-interest?condition_ids=" + ids;

    get_data(
        path,
        [this](const QJsonDocument& doc) {
            QVector<OpenInterest> result;
            QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
            if (arr.isEmpty() && doc.isObject() && doc.object().contains("data"))
                arr = doc.object()["data"].toArray();
            for (const auto& v : arr)
                result.append(OpenInterest::from_json(v.toObject()));
            emit open_interest_ready(result);
        },
        "FetchOpenInterest");
}

} // namespace fincept::services::polymarket

