#include "services/polymarket/PolymarketWebSocket.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "network/websocket/WebSocketClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace fincept::services::polymarket {

static constexpr int kPingIntervalMs = 50000;
static constexpr int kPolyWsIdleCloseMs = 5000; // socket stays up this long after the last unsubscribe

namespace {

// The CLOB WebSocket sends every price / size as a JSON *string* ("0.52", "219.2").
// QJsonValue::toDouble() returns its default (0) for a string, so the old direct
// toDouble() calls turned every WS book level into 0 @ 0 and dropped every price
// update (price > 0 never held). Accept either form.
double pm_ws_num(const QJsonValue& v) {
    return v.isString() ? v.toString().toDouble() : v.toDouble();
}

// Polymarket sends bids ascending and asks descending (worst level first); every
// consumer expects best-first (bids high->low, asks low->high).
void pm_ws_sort_levels(OrderBook& book) {
    std::stable_sort(book.bids.begin(), book.bids.end(),
                     [](const OrderLevel& a, const OrderLevel& b) { return a.price > b.price; });
    std::stable_sort(book.asks.begin(), book.asks.end(),
                     [](const OrderLevel& a, const OrderLevel& b) { return a.price < b.price; });
}

} // namespace

PolymarketWebSocket& PolymarketWebSocket::instance() {
    static PolymarketWebSocket s;
    return s;
}

PolymarketWebSocket::PolymarketWebSocket() : QObject(nullptr) {
    ws_ = new WebSocketClient(this);

    connect(ws_, &WebSocketClient::connected, this, &PolymarketWebSocket::on_ws_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &PolymarketWebSocket::on_ws_disconnected);
    connect(ws_, &WebSocketClient::message_received, this, &PolymarketWebSocket::on_ws_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &PolymarketWebSocket::on_ws_error);

    ping_timer_ = new QTimer(this);
    ping_timer_->setInterval(kPingIntervalMs);
    connect(ping_timer_, &QTimer::timeout, this, &PolymarketWebSocket::send_ping);

    // Closes the socket only after it has stayed subscription-free for a while, so a
    // "unsubscribe old market, subscribe new market" switch (and a show/hide of the
    // Polymarket screen) doesn't tear the connection down and race a re-open.
    idle_timer_ = new QTimer(this);
    idle_timer_->setSingleShot(true);
    idle_timer_->setInterval(kPolyWsIdleCloseMs);
    connect(idle_timer_, &QTimer::timeout, this, [this]() {
        if (subscribed_tokens_.isEmpty())
            disconnect();
    });
}

bool PolymarketWebSocket::is_connected() const {
    return connected_;
}

void PolymarketWebSocket::ensure_connected() {
    if (connected_ || connecting_ || ws_->is_connected())
        return;
    LOG_INFO("Polymarket WS", "Connecting to " + QString(WS_URL));
    connecting_ = true;
    ws_->connect_to(QString(WS_URL));
}

// Subscriptions are reference-counted per token: the Polymarket screen and the dashboard's
// PolymarketPriceWidget both call subscribe() for tokens that can overlap, and a plain set
// let one consumer's unsubscribe() silently kill the other's stream (and, when the set
// emptied, close the socket under it).
void PolymarketWebSocket::subscribe(const QStringList& token_ids) {
    idle_timer_->stop();
    QStringList fresh; // tokens nobody was subscribed to yet
    for (const auto& id : token_ids) {
        if (id.isEmpty())
            continue;
        if (++token_refs_[id] == 1)
            fresh << id;
        subscribed_tokens_.insert(id);
    }
    const bool was_connected = connected_;
    ensure_connected();
    // A fresh connection sends the whole live set from on_ws_connected(). On an open one the
    // new tokens must go out as a dynamic "subscribe" operation — a second {"type":"market"}
    // frame is answered with "INVALID OPERATION" by the server, so tokens added after the first
    // subscribe never streamed.
    if (was_connected && !fresh.isEmpty())
        send_dynamic(QStringLiteral("subscribe"), fresh);
}

void PolymarketWebSocket::unsubscribe(const QStringList& token_ids) {
    QStringList gone; // tokens whose last subscriber just left
    for (const auto& id : token_ids) {
        auto it = token_refs_.find(id);
        if (it == token_refs_.end())
            continue;
        if (--(*it) > 0)
            continue;
        token_refs_.erase(it);
        subscribed_tokens_.remove(id);
        books_.remove(id);
        gone << id;
    }
    if (subscribed_tokens_.isEmpty()) {
        idle_timer_->start(); // close lazily — see the constructor
        return;
    }
    if (connected_ && !gone.isEmpty())
        send_dynamic(QStringLiteral("unsubscribe"), gone);
}

void PolymarketWebSocket::unsubscribe_all() {
    idle_timer_->stop();
    token_refs_.clear();
    subscribed_tokens_.clear();
    books_.clear();
    disconnect();
}

void PolymarketWebSocket::disconnect() {
    ping_timer_->stop();
    // WebSocketClient::disconnect() alone lets its auto-reconnect re-open the socket a second
    // later — an idle connection nobody asked for. Stop that first (connect_to re-arms it).
    ws_->stop_reconnect();
    connecting_ = false;
    closing_ = connected_; // on_ws_disconnected() re-opens if someone subscribed during the close
    ws_->disconnect();
}

void PolymarketWebSocket::send_dynamic(const QString& operation, const QStringList& token_ids) {
    QJsonObject msg;
    QJsonArray assets;
    for (const auto& id : token_ids)
        assets.append(id);
    msg["assets_ids"] = assets;
    msg["operation"] = operation;
    ws_->send(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
    LOG_INFO("Polymarket WS", operation + " " + QString::number(token_ids.size()) + " tokens");
}

void PolymarketWebSocket::send_subscribe(const QStringList& token_ids) {
    QJsonObject msg;
    msg["type"] = "market";
    QJsonArray assets;
    for (const auto& id : token_ids)
        assets.append(id);
    msg["assets_ids"] = assets;
    msg["initial_dump"] = true;
    ws_->send(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
    LOG_INFO("Polymarket WS", "Subscribed to " + QString::number(token_ids.size()) + " tokens");
}

void PolymarketWebSocket::send_ping() {
    if (connected_)
        ws_->send("PING");
}

void PolymarketWebSocket::on_ws_connected() {
    connected_ = true;
    connecting_ = false;
    ping_timer_->start();
    LOG_INFO("Polymarket WS", "Connected");
    emit connection_status_changed(true);
    if (!subscribed_tokens_.isEmpty()) {
        send_subscribe(QStringList(subscribed_tokens_.begin(), subscribed_tokens_.end()));
    }
}

void PolymarketWebSocket::on_ws_disconnected() {
    connected_ = false;
    connecting_ = false;
    ping_timer_->stop();
    books_.clear(); // deltas must not patch a pre-disconnect snapshot; the resubscribe dump re-seeds them
    LOG_WARN("Polymarket WS", "Disconnected");
    emit connection_status_changed(false);
    // We closed it on purpose (idle / unsubscribe_all) and auto-reconnect is stopped. If a
    // subscribe() landed in the window between the close request and this signal, it was
    // sent into a dying socket — open a fresh one so those tokens actually stream.
    if (closing_) {
        closing_ = false;
        if (!subscribed_tokens_.isEmpty())
            ensure_connected();
    }
}

void PolymarketWebSocket::on_ws_message(const QString& msg) {
    if (msg == "PONG" || msg.isEmpty())
        return;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
    if (doc.isNull())
        return;

    QJsonArray updates;
    if (doc.isArray())
        updates = doc.array();
    else if (doc.isObject())
        updates.append(doc.object());

    for (const auto& val : updates) {
        auto obj = val.toObject();

        // Live book deltas carry their asset ids inside "price_changes" (the
        // top-level "market" is the condition id) — patch the local books.
        if (obj["event_type"].toString() == QLatin1String("price_change")) {
            apply_price_changes(obj["price_changes"].toArray());
            continue;
        }

        QString asset_id = obj["asset_id"].toString();
        if (asset_id.isEmpty())
            asset_id = obj["market"].toString();
        if (asset_id.isEmpty())
            continue;
        // The server keeps streaming a token for a moment after its last unsubscribe(); don't
        // seed books or publish prices for tokens nobody is subscribed to any more.
        if (!subscribed_tokens_.contains(asset_id))
            continue;

        if (obj.contains("price")) {
            double price = pm_ws_num(obj["price"]);
            if (price > 0) {
                emit price_updated(asset_id, price);
                publish_price_to_hub(asset_id, price);
            }
        }

        if (obj.contains("bids") || obj.contains("asks")) {
            OrderBook book;
            book.asset_id = asset_id;
            book.market = obj["market"].toString();
            // The WS book carries tick_size (as a string) but no min_order_size. The struct
            // defaults (0.01 / 0.01) used to flow through as if they were the market's real
            // limits and overwrote the REST book's values in the trade ticket's validation
            // (rejecting valid sub-cent-tick prices). 0 means "unknown — don't override".
            book.tick_size = pm_ws_num(obj["tick_size"]);
            book.min_order_size = 0.0;
            for (const auto& b : obj["bids"].toArray()) {
                auto bo = b.toObject();
                book.bids.append({pm_ws_num(bo["price"]), pm_ws_num(bo["size"])});
            }
            for (const auto& a : obj["asks"].toArray()) {
                auto ao = a.toObject();
                book.asks.append({pm_ws_num(ao["price"]), pm_ws_num(ao["size"])});
            }
            pm_ws_sort_levels(book);
            if (!book.bids.isEmpty() || !book.asks.isEmpty()) {
                books_.insert(asset_id, book);
                emit orderbook_updated(asset_id, book);
                publish_orderbook_to_hub(asset_id, book);
            }
        }
    }
}

void PolymarketWebSocket::apply_price_changes(const QJsonArray& changes) {
    QSet<QString> touched;
    for (const auto& v : changes) {
        const auto c = v.toObject();
        const QString asset_id = c["asset_id"].toString();
        auto it = books_.find(asset_id);
        if (it == books_.end())
            continue; // no snapshot yet — the subscribe-time "book" dump will seed it

        const double price = pm_ws_num(c["price"]);
        const double size = pm_ws_num(c["size"]); // new TOTAL size at this level; 0 = level removed
        if (price <= 0.0)
            continue;
        const bool is_buy = c["side"].toString().compare(QLatin1String("BUY"), Qt::CaseInsensitive) == 0;
        auto& levels = is_buy ? it->bids : it->asks;
        auto lit = std::find_if(levels.begin(), levels.end(),
                                [price](const OrderLevel& l) { return std::abs(l.price - price) < 1e-9; });
        if (size <= 0.0) {
            if (lit != levels.end())
                levels.erase(lit);
        } else if (lit != levels.end()) {
            lit->size = size;
        } else {
            levels.append(OrderLevel{price, size});
        }
        touched.insert(asset_id);
    }
    for (const QString& asset_id : touched) {
        auto it = books_.find(asset_id);
        if (it == books_.end())
            continue;
        pm_ws_sort_levels(it.value());
        emit orderbook_updated(asset_id, it.value());
        publish_orderbook_to_hub(asset_id, it.value());
    }
}

void PolymarketWebSocket::on_ws_error(const QString& error) {
    connecting_ = false; // a failed attempt must not block the next ensure_connected()
    LOG_ERROR("Polymarket WS", "Error: " + error);
}

QStringList PolymarketWebSocket::topic_patterns() const {
    return {"prediction:polymarket:price:*", "prediction:polymarket:orderbook:*"};
}

void PolymarketWebSocket::refresh(const QStringList& /*topics*/) {
    // push_only: scheduler never calls this.
}

int PolymarketWebSocket::max_requests_per_sec() const {
    return 10; // CLOB REST cap — informational; refresh() is a no-op anyway
}

void PolymarketWebSocket::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy push_only;
    push_only.push_only = true;
    push_only.ttl_ms = 0;
    push_only.min_interval_ms = 0;

    hub.set_policy_pattern("prediction:polymarket:price:*", push_only);
    hub.set_policy_pattern("prediction:polymarket:orderbook:*", push_only);

    hub_registered_ = true;
    LOG_INFO("Polymarket WS",
             "Registered with DataHub (prediction:polymarket:price:*, prediction:polymarket:orderbook:*)");
}

void PolymarketWebSocket::publish_price_to_hub(const QString& asset_id, double price) {
    if (asset_id.isEmpty())
        return;
    const QString topic = QStringLiteral("prediction:polymarket:price:") + asset_id;
    fincept::datahub::DataHub::instance().publish(topic, QVariant(price));
}

void PolymarketWebSocket::publish_orderbook_to_hub(const QString& asset_id, const OrderBook& book) {
    if (asset_id.isEmpty())
        return;
    const QString topic = QStringLiteral("prediction:polymarket:orderbook:") + asset_id;
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(book));
}

} // namespace fincept::services::polymarket
