#include "services/polymarket/PolymarketWebSocket.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::services::polymarket {

static constexpr int kPingIntervalMs = 50000;

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
}

bool PolymarketWebSocket::is_connected() const {
    return connected_;
}

void PolymarketWebSocket::ensure_connected() {
    if (!ws_->is_connected()) {
        LOG_INFO("Polymarket WS", "Connecting to " + QString(WS_URL));
        ws_->connect_to(QString(WS_URL));
    }
}

void PolymarketWebSocket::subscribe(const QStringList& token_ids) {
    for (const auto& id : token_ids)
        subscribed_tokens_.insert(id);
    ensure_connected();
    if (connected_)
        send_subscribe(token_ids);
}

void PolymarketWebSocket::unsubscribe(const QStringList& token_ids) {
    for (const auto& id : token_ids)
        subscribed_tokens_.remove(id);
    if (subscribed_tokens_.isEmpty())
        disconnect();
}

void PolymarketWebSocket::unsubscribe_all() {
    subscribed_tokens_.clear();
    disconnect();
}

void PolymarketWebSocket::disconnect() {
    ping_timer_->stop();
    ws_->disconnect();
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
    ping_timer_->start();
    LOG_INFO("Polymarket WS", "Connected");
    emit connection_status_changed(true);
    if (!subscribed_tokens_.isEmpty()) {
        send_subscribe(QStringList(subscribed_tokens_.begin(), subscribed_tokens_.end()));
    }
}

void PolymarketWebSocket::on_ws_disconnected() {
    connected_ = false;
    ping_timer_->stop();
    LOG_WARN("Polymarket WS", "Disconnected");
    emit connection_status_changed(false);
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
        QString asset_id = obj["asset_id"].toString();
        if (asset_id.isEmpty())
            asset_id = obj["market"].toString();
        if (asset_id.isEmpty())
            continue;

        if (obj.contains("price")) {
            double price = obj["price"].toDouble();
            if (price > 0) {
                emit price_updated(asset_id, price);
                publish_price_to_hub(asset_id, price);
            }
        }

        if (obj.contains("bids") || obj.contains("asks")) {
            OrderBook book;
            book.asset_id = asset_id;
            for (const auto& b : obj["bids"].toArray()) {
                auto bo = b.toObject();
                book.bids.append({bo["price"].toDouble(), bo["size"].toDouble()});
            }
            for (const auto& a : obj["asks"].toArray()) {
                auto ao = a.toObject();
                book.asks.append({ao["price"].toDouble(), ao["size"].toDouble()});
            }
            if (!book.bids.isEmpty() || !book.asks.isEmpty()) {
                emit orderbook_updated(asset_id, book);
                publish_orderbook_to_hub(asset_id, book);
            }
        }
    }
}

void PolymarketWebSocket::on_ws_error(const QString& error) {
    LOG_ERROR("Polymarket WS", "Error: " + error);
}

QStringList PolymarketWebSocket::topic_patterns() const {
    return {"prediction:polymarket:price:*", "prediction:polymarket:orderbook:*"};
}

void PolymarketWebSocket::refresh(const QStringList& /*topics*/) {
    // push_only: scheduler never calls this.
}

int PolymarketWebSocket::max_requests_per_sec() const {
    return 10;  // CLOB REST cap — informational; refresh() is a no-op anyway
}

void PolymarketWebSocket::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy push_only;
    push_only.push_only = true;
    push_only.ttl_ms = 0;
    push_only.min_interval_ms = 0;

    hub.set_policy_pattern("prediction:polymarket:price:*", push_only);
    hub.set_policy_pattern("prediction:polymarket:orderbook:*", push_only);

    hub_registered_ = true;
    LOG_INFO("Polymarket WS", "Registered with DataHub (prediction:polymarket:price:*, prediction:polymarket:orderbook:*)");
}

void PolymarketWebSocket::publish_price_to_hub(const QString& asset_id, double price) {
    if (asset_id.isEmpty()) return;
    const QString topic = QStringLiteral("prediction:polymarket:price:") + asset_id;
    fincept::datahub::DataHub::instance().publish(topic, QVariant(price));
}

void PolymarketWebSocket::publish_orderbook_to_hub(const QString& asset_id, const OrderBook& book) {
    if (asset_id.isEmpty()) return;
    const QString topic = QStringLiteral("prediction:polymarket:orderbook:") + asset_id;
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(book));
}

} // namespace fincept::services::polymarket

