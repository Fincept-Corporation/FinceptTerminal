#include "trading/websocket/ZerodhaWebSocket.h"

#include "core/logging/Logger.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace fincept::trading {

// ────────────────────────────────────────────────────────────────────────────
// Big-endian read helpers
// ────────────────────────────────────────────────────────────────────────────

quint32 ZerodhaWebSocket::read_u32(const uchar* p) {
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

qint32 ZerodhaWebSocket::read_i32(const uchar* p) {
    return qint32(read_u32(p));
}

quint16 ZerodhaWebSocket::read_u16(const uchar* p) {
    return quint16((quint16(p[0]) << 8) | quint16(p[1]));
}

double ZerodhaWebSocket::paise_to_rupees(qint32 paise) {
    return paise / 100.0;
}

// ────────────────────────────────────────────────────────────────────────────
// Constructor / destructor
// ────────────────────────────────────────────────────────────────────────────

ZerodhaWebSocket::ZerodhaWebSocket(const QString& api_key, const QString& access_token,
                                   QObject* parent)
    : QObject(parent), api_key_(api_key), access_token_(access_token) {
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected,              this, &ZerodhaWebSocket::on_connected);
    connect(ws_, &WebSocketClient::disconnected,           this, &ZerodhaWebSocket::on_disconnected);
    connect(ws_, &WebSocketClient::binary_message_received,this, &ZerodhaWebSocket::on_binary_message);
    connect(ws_, &WebSocketClient::error_occurred,         this, &ZerodhaWebSocket::error_occurred);
}

// ────────────────────────────────────────────────────────────────────────────
// Public API
// ────────────────────────────────────────────────────────────────────────────

void ZerodhaWebSocket::open() {
    QString url = QString("%1?api_key=%2&access_token=%3")
                      .arg(kWsUrl, api_key_, access_token_);
    LOG_INFO("ZerodhaWS", "Connecting to KiteTicker");
    ws_->connect_to(url);
}

void ZerodhaWebSocket::close() {
    ws_->disconnect();
}

bool ZerodhaWebSocket::is_connected() const {
    return ws_->is_connected();
}

void ZerodhaWebSocket::subscribe(const QVector<quint32>& tokens) {
    QVector<quint32> new_tokens;
    for (quint32 t : tokens) {
        if (!subscribed_tokens_.contains(t)) {
            subscribed_tokens_.insert(t);
            new_tokens.append(t);
        }
    }
    if (!new_tokens.isEmpty() && is_connected()) {
        send_subscribe(new_tokens);
        send_mode(new_tokens);
    }
}

void ZerodhaWebSocket::unsubscribe(const QVector<quint32>& tokens) {
    QVector<quint32> to_remove;
    for (quint32 t : tokens) {
        if (subscribed_tokens_.remove(t))
            to_remove.append(t);
    }
    if (!to_remove.isEmpty() && is_connected())
        send_unsubscribe(to_remove);
}

void ZerodhaWebSocket::set_subscriptions(const QVector<quint32>& tokens) {
    subscribed_tokens_.clear();
    for (quint32 t : tokens)
        subscribed_tokens_.insert(t);
    if (is_connected())
        resubscribe_all();
}

void ZerodhaWebSocket::clear_subscriptions() {
    if (is_connected() && !subscribed_tokens_.isEmpty()) {
        QVector<quint32> all(subscribed_tokens_.begin(), subscribed_tokens_.end());
        send_unsubscribe(all);
    }
    subscribed_tokens_.clear();
}

// ────────────────────────────────────────────────────────────────────────────
// Slots
// ────────────────────────────────────────────────────────────────────────────

void ZerodhaWebSocket::on_connected() {
    LOG_INFO("ZerodhaWS", "Connected — resubscribing all tokens");
    resubscribe_all();
    emit connected();
}

void ZerodhaWebSocket::on_disconnected() {
    LOG_WARN("ZerodhaWS", "Disconnected");
    emit disconnected();
}

void ZerodhaWebSocket::on_binary_message(const QByteArray& data) {
    const uchar* buf = reinterpret_cast<const uchar*>(data.constData());
    int total = data.size();

    if (total < 2)
        return;

    int packet_count = int(read_u16(buf));
    int offset = 2;

    for (int i = 0; i < packet_count && offset + 2 <= total; ++i) {
        int pkt_len = int(read_u16(buf + offset));
        offset += 2;

        if (offset + pkt_len > total)
            break;

        const uchar* pkt = buf + offset;
        ZerodhaTick tick;

        if (pkt_len == 8) {
            tick = parse_ltp_packet(pkt);
        } else if (pkt_len == 44) {
            tick = parse_quote_packet(pkt);
        } else if (pkt_len == 184) {
            tick = parse_full_packet(pkt);
        } else {
            // Unknown packet size — skip
            offset += pkt_len;
            continue;
        }

        // Enrich tick with symbol/exchange via InstrumentService
        auto& svc = InstrumentService::instance();
        auto inst = svc.find_by_token(tick.instrument_token, "zerodha");
        if (inst.has_value()) {
            tick.symbol   = inst->symbol;
            tick.exchange = inst->exchange;
            tick.tradable = (inst->instrument_type != InstrumentType::INDEX);
        }

        emit tick_received(tick);
        offset += pkt_len;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Send helpers
// ────────────────────────────────────────────────────────────────────────────

void ZerodhaWebSocket::send_subscribe(const QVector<quint32>& tokens) {
    // Send in batches of kBatchSize
    for (int start = 0; start < tokens.size(); start += kBatchSize) {
        QJsonArray arr;
        for (int j = start; j < std::min(start + kBatchSize, int(tokens.size())); ++j)
            arr.append(qint64(tokens[j]));
        QJsonObject msg;
        msg["a"] = "subscribe";
        msg["v"] = arr;
        ws_->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    }
}

void ZerodhaWebSocket::send_unsubscribe(const QVector<quint32>& tokens) {
    for (int start = 0; start < tokens.size(); start += kBatchSize) {
        QJsonArray arr;
        for (int j = start; j < std::min(start + kBatchSize, int(tokens.size())); ++j)
            arr.append(qint64(tokens[j]));
        QJsonObject msg;
        msg["a"] = "unsubscribe";
        msg["v"] = arr;
        ws_->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    }
}

void ZerodhaWebSocket::send_mode(const QVector<quint32>& tokens) {
    for (int start = 0; start < tokens.size(); start += kBatchSize) {
        QJsonArray arr;
        for (int j = start; j < std::min(start + kBatchSize, int(tokens.size())); ++j)
            arr.append(qint64(tokens[j]));
        QJsonObject msg;
        msg["a"] = "mode";
        QJsonArray v;
        v.append("full");
        v.append(arr);
        msg["v"] = v;
        ws_->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    }
}

void ZerodhaWebSocket::resubscribe_all() {
    if (subscribed_tokens_.isEmpty())
        return;
    QVector<quint32> all(subscribed_tokens_.begin(), subscribed_tokens_.end());
    send_subscribe(all);
    send_mode(all);
}

// ────────────────────────────────────────────────────────────────────────────
// Binary packet parsers
// ────────────────────────────────────────────────────────────────────────────

ZerodhaTick ZerodhaWebSocket::parse_ltp_packet(const uchar* p) const {
    ZerodhaTick t;
    t.instrument_token = read_u32(p);          // bytes 0-3
    t.ltp              = paise_to_rupees(read_i32(p + 4)); // bytes 4-7
    return t;
}

ZerodhaTick ZerodhaWebSocket::parse_quote_packet(const uchar* p) const {
    // 44-byte packet layout (all int32 BE, prices in paise):
    // 0-3   instrument_token
    // 4-7   last_price
    // 8-11  last_quantity (ltq)
    // 12-15 average_price (atp)
    // 16-19 volume
    // 20-23 buy_quantity
    // 24-27 sell_quantity
    // 28-31 open
    // 32-35 high
    // 36-39 low
    // 40-43 close
    ZerodhaTick t;
    t.instrument_token = read_u32(p);
    t.ltp              = paise_to_rupees(read_i32(p +  4));
    t.last_quantity    = read_i32(p +  8);
    t.average_price    = paise_to_rupees(read_i32(p + 12));
    t.volume           = read_i32(p + 16);
    t.buy_quantity     = read_i32(p + 20);
    t.sell_quantity    = read_i32(p + 24);
    t.open             = paise_to_rupees(read_i32(p + 28));
    t.high             = paise_to_rupees(read_i32(p + 32));
    t.low              = paise_to_rupees(read_i32(p + 36));
    t.close            = paise_to_rupees(read_i32(p + 40));
    return t;
}

ZerodhaTick ZerodhaWebSocket::parse_full_packet(const uchar* p) const {
    // 184-byte full packet:
    // 0-43  same as quote packet (44 bytes)
    // 44-47 last_trade_time (unix epoch, seconds)
    // 48-51 oi
    // 52-55 oi_day_high
    // 56-59 oi_day_low
    // 60-63 exchange_timestamp (unix epoch, seconds)
    // 64-183 market depth: 10 levels × 12 bytes each
    //   Each level: quantity(4) + price(4) + orders(2) + padding(2)
    //   First 5 = bids, next 5 = asks
    ZerodhaTick t = parse_quote_packet(p); // first 44 bytes identical

    // last_trade_time at offset 44 — not stored, just skip
    t.oi          = read_i32(p + 48);
    t.oi_day_high = read_i32(p + 52);
    t.oi_day_low  = read_i32(p + 56);

    qint32 ex_ts  = read_i32(p + 60);
    t.exchange_timestamp = QDateTime::fromSecsSinceEpoch(ex_ts, Qt::UTC);

    // Depth: 5 bids then 5 asks, each 12 bytes
    for (int i = 0; i < 5; ++i) {
        int base = 64 + i * 12;
        t.bids[i].quantity = read_i32(p + base);
        t.bids[i].price    = paise_to_rupees(read_i32(p + base + 4));
        t.bids[i].orders   = int(read_u16(p + base + 8));
    }
    for (int i = 0; i < 5; ++i) {
        int base = 64 + (5 + i) * 12;
        t.asks[i].quantity = read_i32(p + base);
        t.asks[i].price    = paise_to_rupees(read_i32(p + base + 4));
        t.asks[i].orders   = int(read_u16(p + base + 8));
    }

    return t;
}

} // namespace fincept::trading
