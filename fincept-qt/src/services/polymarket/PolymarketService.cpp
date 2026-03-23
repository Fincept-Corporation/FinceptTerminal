#include "services/polymarket/PolymarketService.h"

#include "core/logging/Logger.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrlQuery>

namespace fincept::services::polymarket {

// ── API base URLs ────────────────────────────────────────────────────────────

static const QString GAMMA_BASE = QStringLiteral("https://gamma-api.polymarket.com");
static const QString CLOB_BASE = QStringLiteral("https://clob.polymarket.com");
static const QString DATA_BASE = QStringLiteral("https://data-api.polymarket.com");

// ── JSON helpers ─────────────────────────────────────────────────────────────

/// Parse a value that may be either a native JSON array or a stringified one.
static QJsonArray parse_json_arr(const QJsonValue& val) {
    if (val.isArray()) return val.toArray();
    if (val.isString()) {
        auto doc = QJsonDocument::fromJson(val.toString().toUtf8());
        if (doc.isArray()) return doc.array();
    }
    return {};
}

// ── Type parsing ─────────────────────────────────────────────────────────────

Market Market::from_json(const QJsonObject& obj) {
    Market m;
    m.id = obj["id"].toVariant().toInt();
    m.question = obj["question"].toString();
    m.slug = obj["slug"].toString();
    m.description = obj["description"].toString();
    m.condition_id = obj["conditionId"].toString();
    m.image = obj["image"].toString();
    m.volume = obj["volume"].toVariant().toDouble();
    m.liquidity = obj["liquidity"].toVariant().toDouble();
    m.active = obj["active"].toBool();
    m.closed = obj["closed"].toBool();
    m.end_date = obj["endDate"].toString();
    m.category = obj["category"].toString();

    auto outcomes_arr = parse_json_arr(obj["outcomes"]);
    auto prices_arr = parse_json_arr(obj["outcomePrices"]);
    for (int i = 0; i < outcomes_arr.size(); ++i) {
        Outcome o;
        o.name = outcomes_arr[i].toString();
        o.price = (i < prices_arr.size()) ? prices_arr[i].toVariant().toDouble() : 0.0;
        m.outcomes.append(o);
    }

    auto tokens_arr = parse_json_arr(obj["clobTokenIds"]);
    for (const auto& t : tokens_arr) {
        m.clob_token_ids.append(t.toString());
    }

    return m;
}

Event Event::from_json(const QJsonObject& obj) {
    Event e;
    e.id = obj["id"].toVariant().toInt();
    e.title = obj["title"].toString();
    e.slug = obj["slug"].toString();
    e.description = obj["description"].toString();
    e.image = obj["image"].toString();
    e.volume = obj["volume"].toVariant().toDouble();
    e.liquidity = obj["liquidity"].toVariant().toDouble();
    e.active = obj["active"].toBool();
    e.closed = obj["closed"].toBool();
    e.end_date = obj["endDate"].toString();

    auto tags_arr = obj["tags"].toArray();
    for (const auto& t : tags_arr) {
        auto tag_obj = t.toObject();
        e.tags.append(tag_obj["label"].toString());
    }

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
    book.tick_size = obj["tick_size"].toVariant().toDouble();
    book.min_order_size = obj["min_order_size"].toVariant().toDouble();
    book.neg_risk = obj["neg_risk"].toBool();

    auto bids_arr = obj["bids"].toArray();
    for (const auto& b : bids_arr) {
        auto bo = b.toObject();
        OrderLevel lvl;
        lvl.price = bo["price"].toVariant().toDouble();
        lvl.size = bo["size"].toVariant().toDouble();
        book.bids.append(lvl);
    }

    auto asks_arr = obj["asks"].toArray();
    for (const auto& a : asks_arr) {
        auto ao = a.toObject();
        OrderLevel lvl;
        lvl.price = ao["price"].toVariant().toDouble();
        lvl.size = ao["size"].toVariant().toDouble();
        book.asks.append(lvl);
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
        pp.price = po["p"].toVariant().toDouble();
        ph.points.append(pp);
    }
    return ph;
}

Trade Trade::from_json(const QJsonObject& obj) {
    Trade t;
    t.side = obj["side"].toString();
    t.price = obj["price"].toVariant().toDouble();
    t.size = obj["size"].toVariant().toDouble();
    t.timestamp = static_cast<int64_t>(obj["timestamp"].toVariant().toLongLong());
    t.condition_id = obj["conditionId"].toString();
    return t;
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

void PolymarketService::get_json(QNetworkAccessManager* nam, const QString& url,
                                  JsonCallback on_success, const QString& error_ctx) {
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("Accept", "application/json");

    QPointer<PolymarketService> self = this;
    auto* reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [self, reply, on_success, error_ctx]() {
        reply->deleteLater();
        if (!self) return;

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

void PolymarketService::get_gamma(const QString& path, JsonCallback on_success,
                                   const QString& error_ctx) {
    get_json(gamma_nam_, GAMMA_BASE + path, std::move(on_success), error_ctx);
}

void PolymarketService::get_clob(const QString& path, JsonCallback on_success,
                                  const QString& error_ctx) {
    get_json(clob_nam_, CLOB_BASE + path, std::move(on_success), error_ctx);
}

void PolymarketService::get_data(const QString& path, JsonCallback on_success,
                                  const QString& error_ctx) {
    get_json(data_nam_, DATA_BASE + path, std::move(on_success), error_ctx);
}

// ── Gamma API ────────────────────────────────────────────────────────────────

void PolymarketService::fetch_markets(const QString& sort_by, int limit,
                                       int offset, bool closed) {
    QString path = QString("/markets?closed=%1&limit=%2&offset=%3&order=%4&ascending=false")
                       .arg(closed ? "true" : "false")
                       .arg(limit)
                       .arg(offset)
                       .arg(sort_by);

    get_gamma(path, [this](const QJsonDocument& doc) {
        QVector<Market> result;
        QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
        result.reserve(arr.size());
        for (const auto& v : arr) {
            result.append(Market::from_json(v.toObject()));
        }
        LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " markets");
        emit markets_ready(result);
    }, "FetchMarkets");
}

void PolymarketService::search_markets(const QString& query, int limit) {
    QString path = "/markets?_q=" + QUrl::toPercentEncoding(query) + "&limit=" + QString::number(limit);

    get_gamma(path, [this](const QJsonDocument& doc) {
        QVector<Market> result;
        QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
        result.reserve(arr.size());
        for (const auto& v : arr) {
            result.append(Market::from_json(v.toObject()));
        }
        LOG_INFO("Polymarket", "Search returned " + QString::number(result.size()) + " markets");
        emit markets_ready(result);
    }, "SearchMarkets");
}

void PolymarketService::fetch_events(const QString& sort_by, int limit,
                                      int offset, bool closed) {
    QString path = QString("/events?closed=%1&limit=%2&offset=%3&order=%4&ascending=false")
                       .arg(closed ? "true" : "false")
                       .arg(limit)
                       .arg(offset)
                       .arg(sort_by);

    get_gamma(path, [this](const QJsonDocument& doc) {
        QVector<Event> result;
        QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
        result.reserve(arr.size());
        for (const auto& v : arr) {
            result.append(Event::from_json(v.toObject()));
        }
        LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " events");
        emit events_ready(result);
    }, "FetchEvents");
}

void PolymarketService::fetch_tags() {
    get_gamma("/tags?limit=50", [this](const QJsonDocument& doc) {
        QVector<Tag> result;
        QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
        for (const auto& v : arr) {
            auto obj = v.toObject();
            Tag tag;
            tag.label = obj["label"].toString();
            tag.slug = obj["slug"].toString();
            if (!tag.label.isEmpty()) result.append(tag);
        }
        LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " tags");
        emit tags_ready(result);
    }, "FetchTags");
}

// ── CLOB API ─────────────────────────────────────────────────────────────────

void PolymarketService::fetch_order_book(const QString& token_id) {
    get_clob("/book?token_id=" + token_id, [this](const QJsonDocument& doc) {
        if (doc.isObject()) {
            auto book = OrderBook::from_json(doc.object());
            emit order_book_ready(book);
        }
    }, "OrderBook");
}

void PolymarketService::fetch_price_history(const QString& token_id,
                                             const QString& interval, int fidelity) {
    QString path = "/prices-history?market=" + token_id +
                   "&interval=" + interval +
                   "&fidelity=" + QString::number(fidelity);

    get_clob(path, [this](const QJsonDocument& doc) {
        if (doc.isObject()) {
            auto history = PriceHistory::from_json(doc.object());
            LOG_INFO("Polymarket", "Price history: " + QString::number(history.points.size()) + " points");
            emit price_history_ready(history);
        }
    }, "PriceHistory");
}

void PolymarketService::fetch_price_summary(const QString& token_id) {
    // Fetch midpoint — then chain spread and last-trade-price
    // We assemble PriceSummary from three lightweight calls.
    // Use a shared pointer to accumulate results across async callbacks.
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

    // Midpoint
    get_clob("/midpoint?token_id=" + token_id, [acc, maybe_emit](const QJsonDocument& doc) {
        acc->summary.midpoint = doc.object()["mid"].toVariant().toDouble();
        maybe_emit();
    }, "Midpoint");

    // Spread
    get_clob("/spread?token_id=" + token_id, [acc, maybe_emit](const QJsonDocument& doc) {
        acc->summary.spread = doc.object()["spread"].toVariant().toDouble();
        maybe_emit();
    }, "Spread");

    // Last trade price
    get_clob("/last-trade-price?token_id=" + token_id, [acc, maybe_emit](const QJsonDocument& doc) {
        acc->summary.last_trade_price = doc.object()["price"].toVariant().toDouble();
        maybe_emit();
    }, "LastTradePrice");
}

// ── Data API ─────────────────────────────────────────────────────────────────

void PolymarketService::fetch_trades(const QString& condition_id, int limit) {
    QString path = "/trades?market=" + condition_id + "&limit=" + QString::number(limit);

    get_data(path, [this](const QJsonDocument& doc) {
        QVector<Trade> result;
        QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray();
        result.reserve(arr.size());
        for (const auto& v : arr) {
            result.append(Trade::from_json(v.toObject()));
        }
        LOG_INFO("Polymarket", "Fetched " + QString::number(result.size()) + " trades");
        emit trades_ready(result);
    }, "FetchTrades");
}

} // namespace fincept::services::polymarket
