#include "services/prediction/kalshi/KalshiWsClient.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "network/websocket/WebSocketClient.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::services::prediction::kalshi_ns {

namespace pr = fincept::services::prediction;

static constexpr int kPingIntervalMs = 20000;

static double kalshi_fp_to_double(const QJsonValue& v) {
    if (v.isString()) return v.toString().toDouble();
    return v.toDouble();
}

// ── Construction / lifecycle ────────────────────────────────────────────────

KalshiWsClient::KalshiWsClient(QObject* parent) : QObject(parent) {
    ws_ = new fincept::WebSocketClient(this);
    connect(ws_, &fincept::WebSocketClient::connected, this, &KalshiWsClient::on_connected);
    connect(ws_, &fincept::WebSocketClient::disconnected, this, &KalshiWsClient::on_disconnected);
    connect(ws_, &fincept::WebSocketClient::message_received, this, &KalshiWsClient::on_message);
    connect(ws_, &fincept::WebSocketClient::error_occurred, this, &KalshiWsClient::on_error);

    ping_timer_ = new QTimer(this);
    ping_timer_->setInterval(kPingIntervalMs);
    connect(ping_timer_, &QTimer::timeout, this, &KalshiWsClient::send_ping);
}

KalshiWsClient::~KalshiWsClient() = default;

// ── Credentials ─────────────────────────────────────────────────────────────

void KalshiWsClient::set_credentials(const KalshiCredentials& creds) {
    const bool had = creds_.is_valid();
    creds_ = creds;
    if (had && !creds_.is_valid()) {
        disconnect();
    }
    if (creds_.is_valid() && !subscribed_tickers_.isEmpty()) {
        ensure_connected();
    }
}

bool KalshiWsClient::has_credentials() const { return creds_.is_valid(); }

// ── Subscribe ───────────────────────────────────────────────────────────────

void KalshiWsClient::subscribe(const QStringList& market_tickers) {
    for (const auto& t : market_tickers) subscribed_tickers_.insert(t);
    if (!creds_.is_valid()) {
        LOG_INFO("KalshiWS",
                 "Subscribe deferred — no credentials configured (Phase 7 will enable streaming)");
        return;
    }
    ensure_connected();
    if (connected_) send_subscribe(market_tickers);
}

void KalshiWsClient::unsubscribe(const QStringList& market_tickers) {
    for (const auto& t : market_tickers) subscribed_tickers_.remove(t);
    if (subscribed_tickers_.isEmpty()) disconnect();
}

void KalshiWsClient::unsubscribe_all() {
    subscribed_tickers_.clear();
    disconnect();
}

void KalshiWsClient::disconnect() {
    ping_timer_->stop();
    ws_->disconnect();
}

void KalshiWsClient::ensure_connected() {
    // Phase 4 keeps this a stub that logs what *would* happen. Phase 7
    // wires in the RSA-PSS signing via the Python bridge and flips the
    // ws_->connect_to() call live. The connect needs KALSHI-ACCESS-KEY /
    // -SIGNATURE / -TIMESTAMP headers which fincept::WebSocketClient
    // doesn't expose — adding that is also Phase 7 work.
    if (ws_->is_connected()) return;
    if (!creds_.is_valid()) return;
    LOG_DEBUG("KalshiWS",
              QStringLiteral("Ready to connect to ") + (creds_.use_demo ? kDemoWs : kProdWs) +
                  QStringLiteral(" (connect deferred to Phase 7)"));
}

void KalshiWsClient::send_subscribe(const QStringList& tickers) {
    QJsonObject params;
    QJsonArray channels;
    // Kalshi v2 (Apr 2026):
    //   - `ticker` is the current L1 channel (yes_bid_dollars, *_size_fp).
    //   - `orderbook_delta` + `orderbook_snapshot` for L2 book state.
    //   - `trade` streams the public trade tape.
    //   - `market_lifecycle_v2` notifies when markets open/close/settle
    //     (the v1 `market_lifecycle` channel has been superseded).
    channels.append("orderbook_delta");
    channels.append("ticker");
    channels.append("trade");
    channels.append("market_lifecycle_v2");
    params.insert("channels", channels);
    QJsonArray tarr;
    for (const auto& t : tickers) tarr.append(t);
    params.insert("market_tickers", tarr);

    QJsonObject msg;
    msg.insert("id", next_msg_id_++);
    msg.insert("cmd", "subscribe");
    msg.insert("params", params);

    ws_->send(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
    LOG_INFO("KalshiWS", "Subscribed to " + QString::number(tickers.size()) + " tickers");
}

void KalshiWsClient::send_ping() {
    if (!connected_) return;
    QJsonObject ping;
    ping.insert("id", next_msg_id_++);
    ping.insert("cmd", "ping");
    ws_->send(QString::fromUtf8(QJsonDocument(ping).toJson(QJsonDocument::Compact)));
}

// ── Socket handlers ─────────────────────────────────────────────────────────

void KalshiWsClient::on_connected() {
    connected_ = true;
    ping_timer_->start();
    LOG_INFO("KalshiWS", "Connected");
    emit connection_status_changed(true);
    if (!subscribed_tickers_.isEmpty()) {
        send_subscribe(QStringList(subscribed_tickers_.begin(), subscribed_tickers_.end()));
    }
}

void KalshiWsClient::on_disconnected() {
    connected_ = false;
    ping_timer_->stop();
    LOG_WARN("KalshiWS", "Disconnected");
    emit connection_status_changed(false);
}

void KalshiWsClient::on_error(const QString& err) {
    LOG_ERROR("KalshiWS", "Error: " + err);
}

void KalshiWsClient::on_message(const QString& msg) {
    if (msg.isEmpty()) return;
    QJsonParseError perr;
    auto doc = QJsonDocument::fromJson(msg.toUtf8(), &perr);
    if (doc.isNull()) return;
    const auto obj = doc.object();
    const QString type = obj.value("type").toString();
    const auto payload = obj.value("msg").toObject();
    const QString ticker = payload.value("market_ticker").toString();

    if (type == QStringLiteral("ticker")) {
        if (ticker.isEmpty()) return;
        const double yes_price = kalshi_fp_to_double(payload.value("yes_bid_dollars"));
        if (yes_price > 0) publish_price(ticker + QStringLiteral(":yes"), yes_price);
        const double no_price = kalshi_fp_to_double(payload.value("no_bid_dollars"));
        if (no_price > 0) publish_price(ticker + QStringLiteral(":no"), no_price);
        return;
    }

    if (type == QStringLiteral("trade")) {
        if (ticker.isEmpty()) return;
        pr::PredictionTrade t;
        t.asset_id = ticker + QStringLiteral(":yes");
        const QString ts_side = payload.value("taker_side").toString().toLower();
        t.side = (ts_side == QStringLiteral("no")) ? QStringLiteral("SELL")
                                                   : QStringLiteral("BUY");
        t.price = kalshi_fp_to_double(payload.value("yes_price_dollars"));
        t.size = kalshi_fp_to_double(payload.value("count_fp"));
        const QString iso = payload.value("created_time").toString();
        t.ts_ms = iso.isEmpty()
            ? qint64(payload.value("ts").toVariant().toLongLong()) * 1000
            : QDateTime::fromString(iso, Qt::ISODate).toMSecsSinceEpoch();
        emit trade_received(t);
        fincept::datahub::DataHub::instance().publish(
            QStringLiteral("prediction:kalshi:trade:") + ticker,
            QVariant::fromValue(t));
        return;
    }

    if (type == QStringLiteral("market_lifecycle_v2") ||
        type == QStringLiteral("market_lifecycle")) {
        if (ticker.isEmpty()) return;
        const QString status = payload.value("status").toString();
        emit market_lifecycle_changed(ticker, status);
        return;
    }

    if (type != QStringLiteral("orderbook_snapshot") &&
        type != QStringLiteral("orderbook_delta")) {
        return;
    }
    if (ticker.isEmpty()) return;

    // Snapshot / delta path — callers should rebuild their local book from
    // the snapshot then apply deltas. Phase 4 ships only the transport;
    // full reconciliation (seq gap detection, REST resnapshot) lands with
    // Phase 7 trading work when live streaming is genuinely exercised.
    pr::PredictionOrderBook book;
    const QString side = payload.value("side").toString();
    book.asset_id = ticker + QStringLiteral(":") + (side.isEmpty() ? "yes" : side);
    book.last_update_ms = payload.value("ts").toVariant().toLongLong() * 1000;
    publish_orderbook(book.asset_id, book);
}

// ── Hub publish helpers ─────────────────────────────────────────────────────

void KalshiWsClient::publish_price(const QString& asset_id, double price) {
    emit price_updated(asset_id, price);
    const QString topic = QStringLiteral("prediction:kalshi:price:") + asset_id;
    fincept::datahub::DataHub::instance().publish(topic, QVariant(price));
}

void KalshiWsClient::publish_orderbook(const QString& asset_id, const pr::PredictionOrderBook& book) {
    emit orderbook_updated(asset_id, book);
    const QString topic = QStringLiteral("prediction:kalshi:orderbook:") + asset_id;
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(book));
}

// ── DataHub producer ────────────────────────────────────────────────────────

QStringList KalshiWsClient::topic_patterns() const {
    return {
        QStringLiteral("prediction:kalshi:price:*"),
        QStringLiteral("prediction:kalshi:orderbook:*"),
    };
}

void KalshiWsClient::refresh(const QStringList& /*topics*/) {
    // push_only: scheduler never calls this.
}

void KalshiWsClient::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy push_only;
    push_only.push_only = true;
    push_only.ttl_ms = 0;
    push_only.min_interval_ms = 0;

    hub.set_policy_pattern(QStringLiteral("prediction:kalshi:price:*"), push_only);
    hub.set_policy_pattern(QStringLiteral("prediction:kalshi:orderbook:*"), push_only);

    hub_registered_ = true;
    LOG_INFO("KalshiWS",
             "Registered with DataHub (prediction:kalshi:price:*, prediction:kalshi:orderbook:*)");
}

} // namespace fincept::services::prediction::kalshi_ns
