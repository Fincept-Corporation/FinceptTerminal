// AngelOneWebSocket — SmartStream v2 binary tick client
//
// Protocol summary:
//   1. Connect to wss://smartapisocket.angelone.in/smart-stream
//   2. On open: send JSON auth message with feed_token + client_code
//   3. Subscribe: send JSON { "action":1, "params":{ "mode":<int>, "tokenList":[...] } }
//   4. Receive binary ticks (Little-Endian):
//        byte 0    : subscription_mode (1=LTP, 2=Quote, 3=SnapQuote)
//        byte 1    : exchange_type
//        bytes 2-26: token (null-padded ASCII string, 25 bytes)
//        bytes 27-34: sequence_number (int64 LE)
//        bytes 35-42: exchange_timestamp (int64 LE, ms since epoch)
//        bytes 43-50: last_traded_price (int64 LE, paise)
//        ... (Quote/SnapQuote extend further — see parse_tick)
//   5. Server sends WebSocket pings every ~30s; Qt handles pong automatically.

#include "trading/websocket/AngelOneWebSocket.h"

#include "core/logging/Logger.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

namespace fincept::trading {

static constexpr const char* TAG_AO_WS = "AngelOneWS";

// ─────────────────────────────────────────────────────────────────────────────
// Little-Endian read helpers
// (Angel One uses LE — opposite of Zerodha which is BE)
// ─────────────────────────────────────────────────────────────────────────────

qint64 AngelOneWebSocket::read_i64_le(const uchar* p) {
    quint64 v = quint64(p[0]) | (quint64(p[1]) << 8) | (quint64(p[2]) << 16) | (quint64(p[3]) << 24) |
                (quint64(p[4]) << 32) | (quint64(p[5]) << 40) | (quint64(p[6]) << 48) | (quint64(p[7]) << 56);
    return static_cast<qint64>(v);
}

double AngelOneWebSocket::read_d64_le(const uchar* p) {
    quint64 raw = quint64(p[0]) | (quint64(p[1]) << 8) | (quint64(p[2]) << 16) | (quint64(p[3]) << 24) |
                  (quint64(p[4]) << 32) | (quint64(p[5]) << 40) | (quint64(p[6]) << 48) | (quint64(p[7]) << 56);
    double d;
    memcpy(&d, &raw, 8);
    return d;
}

quint16 AngelOneWebSocket::read_u16_le(const uchar* p) {
    return quint16(p[0]) | (quint16(p[1]) << 8);
}

double AngelOneWebSocket::paise_to_rupees(qint64 paise) {
    return paise / 100.0;
}

QString AngelOneWebSocket::parse_token_string(const uchar* p, int max_len) {
    QString token;
    for (int i = 0; i < max_len; ++i) {
        if (p[i] == '\0')
            break;
        token += QChar(p[i]);
    }
    return token;
}

quint8 AngelOneWebSocket::exchange_type_to_int(AoExchangeType t) {
    return static_cast<quint8>(t);
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AngelOneWebSocket::AngelOneWebSocket(const QString& api_key, const QString& client_code, const QString& feed_token,
                                     QObject* parent)
    : QObject(parent), api_key_(api_key), client_code_(client_code), feed_token_(feed_token) {
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &AngelOneWebSocket::on_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &AngelOneWebSocket::on_disconnected);
    connect(ws_, &WebSocketClient::binary_message_received, this, &AngelOneWebSocket::on_binary_message);
    connect(ws_, &WebSocketClient::message_received, this, &AngelOneWebSocket::on_text_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &AngelOneWebSocket::error_occurred);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void AngelOneWebSocket::open() {
    LOG_INFO(TAG_AO_WS, "Connecting to SmartStream");
    ws_->connect_to(QString(kWsUrl));
}

void AngelOneWebSocket::close() {
    ws_->disconnect();
}

bool AngelOneWebSocket::is_connected() const {
    return ws_->is_connected();
}

void AngelOneWebSocket::subscribe(const QVector<Subscription>& subs, AoSubMode mode) {
    mode_ = mode;
    QVector<Subscription> new_subs;
    for (const auto& s : subs) {
        QString key = QString::number(exchange_type_to_int(s.exchange_type)) + ":" + s.token;
        if (!subscribed_keys_.contains(key)) {
            subscribed_keys_.insert(key);
            subscriptions_.append(s);
            new_subs.append(s);
        }
    }
    if (!new_subs.isEmpty() && is_connected())
        send_subscribe(new_subs, mode);
}

void AngelOneWebSocket::unsubscribe(const QVector<Subscription>& subs, AoSubMode mode) {
    QVector<Subscription> to_remove;
    for (const auto& s : subs) {
        QString key = QString::number(exchange_type_to_int(s.exchange_type)) + ":" + s.token;
        if (subscribed_keys_.remove(key)) {
            subscriptions_.removeIf(
                [&](const Subscription& x) { return x.token == s.token && x.exchange_type == s.exchange_type; });
            to_remove.append(s);
        }
    }
    if (!to_remove.isEmpty() && is_connected())
        send_unsubscribe(to_remove, mode);
}

void AngelOneWebSocket::set_subscriptions(const QVector<Subscription>& subs, AoSubMode mode) {
    mode_ = mode;
    subscribed_keys_.clear();
    subscriptions_.clear();
    for (const auto& s : subs) {
        QString key = QString::number(exchange_type_to_int(s.exchange_type)) + ":" + s.token;
        subscribed_keys_.insert(key);
        subscriptions_.append(s);
    }
    if (is_connected())
        resubscribe_all();
}

void AngelOneWebSocket::clear_subscriptions() {
    if (is_connected() && !subscriptions_.isEmpty())
        send_unsubscribe(subscriptions_, mode_);
    subscribed_keys_.clear();
    subscriptions_.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void AngelOneWebSocket::on_connected() {
    LOG_INFO(TAG_AO_WS, "Connected — sending auth");
    send_auth();
    // Subscriptions are sent after auth ACK arrives in on_text_message
}

void AngelOneWebSocket::on_disconnected() {
    LOG_WARN(TAG_AO_WS, "Disconnected");
    emit disconnected();
}

void AngelOneWebSocket::on_text_message(const QString& msg) {
    // Angel One sends a JSON text message to acknowledge the auth/connect.
    // We resubscribe when we see any non-error response after connect.
    // Format: { "type": "connected" } or error variants.
    LOG_DEBUG(TAG_AO_WS, QString("Text msg: %1").arg(msg));

    auto doc = QJsonDocument::fromJson(msg.toUtf8());
    if (doc.isObject()) {
        QString type = doc.object().value("type").toString();
        if (type == "error") {
            QString err = doc.object().value("message").toString();
            LOG_ERROR(TAG_AO_WS, QString("Auth error: %1").arg(err));
            emit error_occurred(err);
            return;
        }
    }

    // Auth acknowledged — (re)subscribe all tokens
    if (!subscriptions_.isEmpty()) {
        LOG_INFO(TAG_AO_WS, "Auth OK — resubscribing all tokens");
        resubscribe_all();
    }
    emit connected();
}

void AngelOneWebSocket::on_binary_message(const QByteArray& data) {
    // Minimum valid LTP packet: 51 bytes
    if (data.size() < 51)
        return;

    AoTick tick = parse_tick(data);
    if (tick.token.isEmpty())
        return;

    // Enrich with symbol/exchange from InstrumentService
    auto& svc = InstrumentService::instance();
    bool ok = false;
    quint32 token_int = tick.token.toUInt(&ok);
    if (ok) {
        auto inst = svc.find_by_token(token_int, "angelone");
        if (inst.has_value()) {
            tick.symbol = inst->symbol;
            tick.exchange = inst->exchange;
        }
    }
    if (tick.symbol.isEmpty())
        tick.symbol = tick.token; // fallback: use raw token

    emit tick_received(tick);
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON message senders
// ─────────────────────────────────────────────────────────────────────────────

void AngelOneWebSocket::send_auth() {
    // Connect/auth message — must be sent immediately after WebSocket opens
    QJsonObject params;
    params["userID"] = client_code_;
    params["feedToken"] = feed_token_;

    QJsonObject msg;
    msg["correlationID"] = "fincept_auth";
    msg["action"] = 2; // 2 = connect/auth
    msg["params"] = params;

    ws_->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void AngelOneWebSocket::send_subscribe(const QVector<Subscription>& subs, AoSubMode mode) {
    if (subs.isEmpty())
        return;

    // Group tokens by exchange type
    QMap<quint8, QJsonArray> by_exchange;
    for (const auto& s : subs)
        by_exchange[exchange_type_to_int(s.exchange_type)].append(s.token);

    QJsonArray token_list;
    for (auto it = by_exchange.constBegin(); it != by_exchange.constEnd(); ++it) {
        QJsonObject entry;
        entry["exchangeType"] = int(it.key());
        entry["tokens"] = it.value();
        token_list.append(entry);
    }

    QJsonObject params;
    params["mode"] = static_cast<int>(mode);
    params["tokenList"] = token_list;

    QJsonObject msg;
    msg["correlationID"] = "fincept_sub";
    msg["action"] = 1; // 1 = subscribe
    msg["params"] = params;

    ws_->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    LOG_INFO(TAG_AO_WS, QString("Subscribed %1 tokens (mode %2)").arg(subs.size()).arg(static_cast<int>(mode)));
}

void AngelOneWebSocket::send_unsubscribe(const QVector<Subscription>& subs, AoSubMode mode) {
    if (subs.isEmpty())
        return;

    QMap<quint8, QJsonArray> by_exchange;
    for (const auto& s : subs)
        by_exchange[exchange_type_to_int(s.exchange_type)].append(s.token);

    QJsonArray token_list;
    for (auto it = by_exchange.constBegin(); it != by_exchange.constEnd(); ++it) {
        QJsonObject entry;
        entry["exchangeType"] = int(it.key());
        entry["tokens"] = it.value();
        token_list.append(entry);
    }

    QJsonObject params;
    params["mode"] = static_cast<int>(mode);
    params["tokenList"] = token_list;

    QJsonObject msg;
    msg["correlationID"] = "fincept_unsub";
    msg["action"] = 0; // 0 = unsubscribe
    msg["params"] = params;

    ws_->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void AngelOneWebSocket::resubscribe_all() {
    if (subscriptions_.isEmpty())
        return;
    send_subscribe(subscriptions_, mode_);
}

// ─────────────────────────────────────────────────────────────────────────────
// Binary tick parser
//
// Angel One SmartStream binary layout (Little-Endian):
//
//  Offset  Size  Type    Field
//  0       1     u8      subscription_mode
//  1       1     u8      exchange_type
//  2       25    char[]  token (null-padded ASCII)
//  27      8     i64     sequence_number
//  35      8     i64     exchange_timestamp (ms since epoch)
//  43      8     i64     last_traded_price (paise)
//
// Quote + SnapQuote extra (mode >= 2, offset 51+):
//  51      8     i64     last_traded_quantity
//  59      8     i64     average_traded_price (paise)
//  67      8     i64     volume_trade_for_the_day
//  75      8     f64     total_buy_quantity
//  83      8     f64     total_sell_quantity
//  91      8     i64     open_price_of_the_day (paise)
//  99      8     i64     high_price_of_the_day (paise)
//  107     8     i64     low_price_of_the_day (paise)
//  115     8     i64     closed_price (paise)
//
// SnapQuote extra (mode == 3, offset 123+):
//  123     8     i64     last_traded_timestamp
//  131     8     i64     open_interest
//  139     8     i64     oi_change_percentage (× 100)
//  147     200           best-5 buy/sell (10 × 20-byte entries)
//                          each entry: flag(u16,2) qty(i64,8) price(i64,8) orders(u16,2)
//                          flag==0 → buy, flag!=0 → sell
//  347     8     i64     upper_circuit_limit (paise)
//  355     8     i64     lower_circuit_limit (paise)
//  363     8     i64     52_week_high_price (paise)
//  371     8     i64     52_week_low_price (paise)
// ─────────────────────────────────────────────────────────────────────────────

AoTick AngelOneWebSocket::parse_tick(const QByteArray& data) const {
    const uchar* buf = reinterpret_cast<const uchar*>(data.constData());
    const int sz = data.size();

    AoTick tick;

    // ── Header (all modes) ─────────────────────────────────────
    tick.mode = static_cast<AoSubMode>(buf[0]);
    tick.exchange_type = static_cast<AoExchangeType>(buf[1]);
    tick.token = parse_token_string(buf + 2, 25);

    if (sz < 51)
        return tick; // need at least to LTP

    tick.sequence_number = read_i64_le(buf + 27);
    tick.exchange_timestamp = QDateTime::fromMSecsSinceEpoch(read_i64_le(buf + 35), QTimeZone::UTC);
    tick.ltp = paise_to_rupees(read_i64_le(buf + 43));

    if (tick.mode == AoSubMode::LTP)
        return tick;

    // ── Quote fields (mode 2 & 3) ──────────────────────────────
    if (sz < 123)
        return tick;

    tick.ltq = read_i64_le(buf + 51);
    tick.atp = paise_to_rupees(read_i64_le(buf + 59));
    tick.volume = read_i64_le(buf + 67);
    tick.buy_qty = read_d64_le(buf + 75);
    tick.sell_qty = read_d64_le(buf + 83);
    tick.open = paise_to_rupees(read_i64_le(buf + 91));
    tick.high = paise_to_rupees(read_i64_le(buf + 99));
    tick.low = paise_to_rupees(read_i64_le(buf + 107));
    tick.close = paise_to_rupees(read_i64_le(buf + 115));

    if (tick.mode == AoSubMode::Quote)
        return tick;

    // ── SnapQuote extra (mode 3) ───────────────────────────────
    if (sz < 379)
        return tick;

    tick.oi = read_i64_le(buf + 131);
    tick.oi_change_pct = read_i64_le(buf + 139);

    // Best-5 depth: 10 × 20-byte entries at offset 147
    // Each entry: flag(u16,2) qty(i64,8) price(i64,8) orders(u16,2)
    // flag==0 → buy, flag!=0 → sell
    int buy_idx = 0, sell_idx = 0;
    for (int i = 0; i < 10 && buy_idx < 5 && sell_idx < 5; ++i) {
        const uchar* entry = buf + 147 + i * 20;
        quint16 flag = read_u16_le(entry);
        qint64 qty = read_i64_le(entry + 2);
        qint64 price = read_i64_le(entry + 10);
        quint16 orders = read_u16_le(entry + 18);

        if (flag == 0 && buy_idx < 5) {
            tick.bids[buy_idx].quantity = qty;
            tick.bids[buy_idx].price = price;
            tick.bids[buy_idx].orders = orders;
            ++buy_idx;
        } else if (flag != 0 && sell_idx < 5) {
            tick.asks[sell_idx].quantity = qty;
            tick.asks[sell_idx].price = price;
            tick.asks[sell_idx].orders = orders;
            ++sell_idx;
        }
    }

    tick.upper_circuit = paise_to_rupees(read_i64_le(buf + 347));
    tick.lower_circuit = paise_to_rupees(read_i64_le(buf + 355));
    tick.week52_high = paise_to_rupees(read_i64_le(buf + 363));
    tick.week52_low = paise_to_rupees(read_i64_le(buf + 371));

    return tick;
}

} // namespace fincept::trading
