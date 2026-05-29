// FivePaisaWebSocket — 5Paisa JSON market-data streaming adapter.
//
// See the header for the protocol summary. The OpenAlgo reference is
//   broker/fivepaisa/streaming/fivepaisa_websocket.py
//   broker/fivepaisa/streaming/fivepaisa_adapter.py

#include "trading/websocket/FivePaisaWebSocket.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"
#include "trading/instruments/InstrumentService.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

namespace fincept::trading {

static constexpr const char* TAG_FP_WS = "FivePaisaWS";

// 5Paisa redirect-server → feed host. Mirrors WEBSOCKET_URLS in the py source.
static QString host_for_server(const QString& server) {
    if (server == "A")
        return "wss://aopenfeed.5paisa.com/feeds/api/chat";
    if (server == "B")
        return "wss://bopenfeed.5paisa.com/feeds/api/chat";
    if (server == "C")
        return "wss://openfeed.5paisa.com/feeds/api/chat";
    return "wss://openfeed.5paisa.com/Feeds/api/chat"; // default
}

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────

QString FivePaisaWebSocket::decode_redirect_server(const QString& access_token) {
    // JWT = header.payload.signature — decode the base64url payload and read
    // the "RedirectServer" claim. Fall back to "default" on any failure.
    QStringList parts = access_token.split('.');
    if (parts.size() != 3)
        return QStringLiteral("default");

    QByteArray payload = parts[1].toUtf8();
    // base64url → base64 + padding
    payload.replace('-', '+').replace('_', '/');
    int pad = payload.size() % 4;
    if (pad)
        payload.append(4 - pad, '=');

    QByteArray decoded = QByteArray::fromBase64(payload);
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(decoded, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QStringLiteral("default");

    QString server = doc.object().value("RedirectServer").toString();
    return server.isEmpty() ? QStringLiteral("default") : server;
}

QString FivePaisaWebSocket::feed_url_for_token(const QString& access_token) {
    return host_for_server(decode_redirect_server(access_token));
}

QString FivePaisaWebSocket::fp_exchange(const QString& exchange) {
    if (exchange == "BSE" || exchange == "BFO" || exchange == "BCD")
        return QStringLiteral("B");
    if (exchange == "MCX")
        return QStringLiteral("M");
    return QStringLiteral("N"); // NSE, NFO, CDS
}

QString FivePaisaWebSocket::fp_exchange_type(const QString& exchange) {
    if (exchange == "NFO" || exchange == "BFO" || exchange == "MCX")
        return QStringLiteral("D");
    if (exchange == "CDS" || exchange == "BCD")
        return QStringLiteral("U");
    return QStringLiteral("C"); // NSE, BSE
}

int64_t FivePaisaWebSocket::parse_fp_time(const QString& s) {
    if (s.isEmpty())
        return 0;
    // Format: "/Date(1690000000000)/" or "/Date(1690000000000+0530)/"
    static const QRegularExpression re(QStringLiteral("/Date\\((\\d+)"));
    auto m = re.match(s);
    if (m.hasMatch())
        return m.captured(1).toLongLong();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

FivePaisaWebSocket::FivePaisaWebSocket(const QString& access_token, const QString& client_code, QObject* parent)
    : BrokerWebSocketBase(parent), access_token_(access_token), client_code_(client_code) {
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &FivePaisaWebSocket::on_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &FivePaisaWebSocket::on_disconnected);
    connect(ws_, &WebSocketClient::message_received, this, &FivePaisaWebSocket::on_text_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &FivePaisaWebSocket::error_occurred);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void FivePaisaWebSocket::open() {
    QString url =
        QString("%1?Value1=%2|%3").arg(feed_url_for_token(access_token_), access_token_, client_code_);
    LOG_INFO(TAG_FP_WS, "Connecting to 5Paisa feed");
    ws_->connect_to(url);
    start_health_check();
}

void FivePaisaWebSocket::close() {
    stop_health_check();
    ws_->disconnect();
}

bool FivePaisaWebSocket::is_connected() const {
    return ws_->is_connected();
}

void FivePaisaWebSocket::subscribe(const QVector<qint64>& tokens) {
    QVector<qint64> new_tokens;
    for (qint64 t : tokens) {
        if (!subscribed_tokens_.contains(t)) {
            subscribed_tokens_.insert(t);
            new_tokens.append(t);
        }
    }
    // Coalesce rapid subscribe calls via the base-class debounce. The actual
    // wire send happens in flush_subscribe_queue().
    if (!new_tokens.isEmpty())
        enqueue_subscribe(new_tokens);
}

void FivePaisaWebSocket::unsubscribe() {
    if (subscribed_tokens_.isEmpty())
        return;
    QVector<qint64> all(subscribed_tokens_.begin(), subscribed_tokens_.end());
    if (is_connected())
        send_subscribe(all, /*subscribe=*/false);
    subscribed_tokens_.clear();
    clear_tick_cache();
}

void FivePaisaWebSocket::unsubscribe(const QVector<qint64>& tokens) {
    QVector<qint64> to_remove;
    for (qint64 t : tokens) {
        if (subscribed_tokens_.remove(t))
            to_remove.append(t);
    }
    if (!to_remove.isEmpty() && is_connected())
        send_subscribe(to_remove, /*subscribe=*/false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Base-class hooks
// ─────────────────────────────────────────────────────────────────────────────

void FivePaisaWebSocket::flush_subscribe_queue() {
    QVector<qint64> tokens = take_subscribe_queue();
    if (tokens.isEmpty() || !is_connected())
        return;
    send_subscribe(tokens, /*subscribe=*/true);
}

void FivePaisaWebSocket::on_data_stall() {
    LOG_WARN(TAG_FP_WS, "Data stall — forcing reconnect");
    ws_->disconnect();
    open();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void FivePaisaWebSocket::on_connected() {
    LOG_INFO(TAG_FP_WS, "Connected — resubscribing all scrips");
    resubscribe_all();
    emit connected();
}

void FivePaisaWebSocket::on_disconnected() {
    LOG_WARN(TAG_FP_WS, "Disconnected");
    emit disconnected();
}

void FivePaisaWebSocket::on_text_message(const QString& msg) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        LOG_DEBUG(TAG_FP_WS, QString("Non-JSON message: %1").arg(msg.left(120)));
        return;
    }

    // 5Paisa pushes either a single tick object or an array of tick objects.
    if (doc.isArray()) {
        for (const QJsonValue& v : doc.array()) {
            if (v.isObject())
                handle_tick(v.toObject());
        }
    } else if (doc.isObject()) {
        handle_tick(doc.object());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Wire senders
// ─────────────────────────────────────────────────────────────────────────────

void FivePaisaWebSocket::send_subscribe(const QVector<qint64>& tokens, bool subscribe) {
    if (tokens.isEmpty())
        return;

    auto& svc = InstrumentService::instance();

    // Build MarketFeedData: one {Exch, ExchType, ScripCode} entry per token.
    QJsonArray feed_data;
    for (qint64 token : tokens) {
        QString exch = QStringLiteral("N");
        QString exch_type = QStringLiteral("C");
        auto inst = svc.find_by_token(static_cast<quint32>(token), "fivepaisa");
        if (inst.has_value()) {
            const QString& ex = inst->exchange.isEmpty() ? inst->brexchange : inst->exchange;
            exch = fp_exchange(ex);
            exch_type = fp_exchange_type(ex);
        }
        QJsonObject scrip;
        scrip["Exch"] = exch;
        scrip["ExchType"] = exch_type;
        scrip["ScripCode"] = static_cast<double>(token);
        feed_data.append(scrip);
    }

    QJsonObject req;
    req["Method"] = QString(kMethodMarketFeed);
    req["Operation"] = subscribe ? QStringLiteral("Subscribe") : QStringLiteral("Unsubscribe");
    req["ClientCode"] = client_code_;
    req["MarketFeedData"] = feed_data;

    ws_->send(QJsonDocument(req).toJson(QJsonDocument::Compact));
    LOG_INFO(TAG_FP_WS, QString("%1 %2 scrips")
                            .arg(subscribe ? "Subscribed" : "Unsubscribed")
                            .arg(tokens.size()));
}

void FivePaisaWebSocket::resubscribe_all() {
    if (subscribed_tokens_.isEmpty())
        return;
    QVector<qint64> all(subscribed_tokens_.begin(), subscribed_tokens_.end());
    send_subscribe(all, /*subscribe=*/true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick handling
// ─────────────────────────────────────────────────────────────────────────────

void FivePaisaWebSocket::handle_tick(const QJsonObject& obj) {
    if (!obj.contains("Token"))
        return; // not a market-data tick (could be an ack/control message)

    note_tick();

    const qint64 token = static_cast<qint64>(obj.value("Token").toDouble());
    const QString key = QString::number(token);

    // Build a partial BrokerQuote from this tick. Fields that are 0 will be
    // back-filled from the cached last-known-good values in merge_tick().
    BrokerQuote partial;
    partial.ltp = obj.value("LastRate").toDouble();
    partial.open = obj.value("OpenRate").toDouble();
    partial.high = obj.value("High").toDouble();
    partial.low = obj.value("Low").toDouble();
    partial.close = obj.value("PClose").toDouble();
    partial.volume = obj.value("TotalQty").toDouble();
    partial.bid = obj.value("BidRate").toDouble();
    partial.ask = obj.value("OffRate").toDouble();
    partial.bid_size = obj.value("BidQty").toDouble();
    partial.ask_size = obj.value("OffQty").toDouble();
    partial.timestamp = parse_fp_time(obj.value("TickDt").toString());

    // Enrich with the normalised symbol (fall back to numeric token).
    auto& svc = InstrumentService::instance();
    auto inst = svc.find_by_token(static_cast<quint32>(token), "fivepaisa");
    partial.symbol = inst.has_value() ? inst->symbol : key;

    // OHLC carryforward — merge against cached non-zero values.
    BrokerQuote merged = merge_tick(key, partial);
    if (merged.timestamp == 0)
        merged.timestamp = QDateTime::currentMSecsSinceEpoch();

    emit tick_received(merged);

    // 5-level depth (present in MarketDepthService payloads under "Details").
    if (obj.contains("Details") && obj.value("Details").isArray()) {
        MarketDepth depth;
        depth.symbol = merged.symbol;
        depth.exchange = inst.has_value() ? inst->exchange : QString();
        depth.ltp = merged.ltp;
        depth.volume = merged.volume;

        for (const QJsonValue& dv : obj.value("Details").toArray()) {
            if (!dv.isObject())
                continue;
            QJsonObject d = dv.toObject();
            DepthLevel lvl;
            lvl.price = d.value("Price").toDouble();
            lvl.quantity = d.value("Quantity").toInt();
            lvl.orders = d.value("NumberOfOrders").toInt();
            // BbBuySellFlag: 66 ('B') = buy, 83 ('S') = sell.
            const int flag = d.value("BbBuySellFlag").toInt();
            if (flag == 66 && depth.bids.size() < 5)
                depth.bids.append(lvl);
            else if (flag == 83 && depth.asks.size() < 5)
                depth.asks.append(lvl);
        }
        emit depth_received(depth);
    }
}

} // namespace fincept::trading
