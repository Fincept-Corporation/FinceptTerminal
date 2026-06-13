// DhanWebSocket — Dhan binary market-data streaming adapter.
//
// See the header for the protocol summary. Byte offsets below are transcribed
// directly from the authoritative struct.unpack format strings in
//   broker/dhan/streaming/dhan_websocket.py
// All multi-byte fields are little-endian.

#include "trading/websocket/DhanWebSocket.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

#include <cstring>

namespace fincept::trading {

static constexpr const char* TAG_DHAN_WS = "DhanWS";

static constexpr const char* kFiveDepthUrl = "wss://api-feed.dhan.co";

// ─────────────────────────────────────────────────────────────────────────────
// Little-endian read helpers (qFromLittleEndian handles unaligned access).
// ─────────────────────────────────────────────────────────────────────────────

quint16 DhanWebSocket::read_u16(const uchar* p) {
    return qFromLittleEndian<quint16>(p);
}

quint32 DhanWebSocket::read_u32(const uchar* p) {
    return qFromLittleEndian<quint32>(p);
}

float DhanWebSocket::read_f32(const uchar* p) {
    // IEEE-754 float is transmitted little-endian; read the raw u32 then bit-cast.
    quint32 bits = qFromLittleEndian<quint32>(p);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

QString DhanWebSocket::inst_key(const Instrument& i) {
    return i.exchange_segment + QLatin1Char('|') + i.security_id;
}

// ─────────────────────────────────────────────────────────────────────────────
// Segment ↔ exchange mapping (dhan_mapping.py SEGMENT_TO_EXCHANGE / EXCHANGE_MAP)
// ─────────────────────────────────────────────────────────────────────────────

QString DhanWebSocket::segment_code_to_exchange(int code) {
    switch (code) {
        case 0: return QStringLiteral("NSE_INDEX"); // IDX_I (both NSE/BSE index → default NSE)
        case 1: return QStringLiteral("NSE");       // NSE_EQ
        case 2: return QStringLiteral("NFO");       // NSE_FNO
        case 3: return QStringLiteral("CDS");       // NSE_CURRENCY
        case 4: return QStringLiteral("BSE");       // BSE_EQ
        case 5: return QStringLiteral("MCX");       // MCX_COMM
        case 7: return QStringLiteral("BCD");       // BSE_CURRENCY
        case 8: return QStringLiteral("BFO");       // BSE_FNO
        default: return QString();
    }
}

QString DhanWebSocket::exchange_to_segment_string(const QString& exchange) {
    if (exchange == "NSE") return QStringLiteral("NSE_EQ");
    if (exchange == "BSE") return QStringLiteral("BSE_EQ");
    if (exchange == "NFO") return QStringLiteral("NSE_FNO");
    if (exchange == "BFO") return QStringLiteral("BSE_FNO");
    if (exchange == "MCX") return QStringLiteral("MCX_COMM");
    if (exchange == "CDS") return QStringLiteral("NSE_CURRENCY");
    if (exchange == "BCD") return QStringLiteral("BSE_CURRENCY");
    if (exchange == "NSE_INDEX" || exchange == "BSE_INDEX") return QStringLiteral("IDX_I");
    return QStringLiteral("NSE_EQ"); // safe default
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

DhanWebSocket::DhanWebSocket(const QString& access_token, const QString& client_id, QObject* parent)
    : BrokerWebSocketBase(parent), access_token_(access_token), client_id_(client_id) {
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &DhanWebSocket::on_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &DhanWebSocket::on_disconnected);
    connect(ws_, &WebSocketClient::binary_message_received, this, &DhanWebSocket::on_binary_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &DhanWebSocket::error_occurred);
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void DhanWebSocket::open() {
    // 5-depth endpoint. authType=2 + token + clientId in the query string.
    QString url = QString("%1?version=2&token=%2&clientId=%3&authType=2")
                      .arg(QString(kFiveDepthUrl), access_token_, client_id_);
    // Log the connection params with the token redacted — never log the JWT.
    const QString tok_tail = access_token_.size() > 6 ? access_token_.right(6) : QStringLiteral("??????");
    LOG_INFO(TAG_DHAN_WS, QString("Connecting to Dhan 5-depth feed (clientId=%1, token=…%2, len=%3)")
                              .arg(client_id_, tok_tail)
                              .arg(access_token_.size()));
    ws_->connect_to(url);
    start_health_check();
}

void DhanWebSocket::close() {
    stop_health_check();
    // Dhan honours a JSON disconnect request before the socket teardown.
    if (ws_->is_connected()) {
        QJsonObject msg;
        msg["RequestCode"] = kReqDisconnect;
        ws_->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    }
    ws_->disconnect();
}

bool DhanWebSocket::is_connected() const {
    return ws_->is_connected();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public subscribe API
// ─────────────────────────────────────────────────────────────────────────────

void DhanWebSocket::subscribe(const QVector<Instrument>& instruments) {
    QVector<Instrument> fresh;
    for (const Instrument& inst : instruments) {
        if (inst.security_id.isEmpty())
            continue;
        const QString key = inst_key(inst);
        if (!subscribed_.contains(key)) {
            subscribed_.insert(key, inst);
            fresh.append(inst);
        }
    }
    if (fresh.isEmpty())
        return;

    // Coalesce rapid subscribe calls via the base-class debounce; the wire send
    // happens in flush_subscribe_queue(). We can't push Instrument structs
    // through the qint64 queue, so we just arm the debounce with the security
    // ids and rebuild the pending set from subscribed_ on flush.
    QVector<qint64> token_hint;
    token_hint.reserve(fresh.size());
    for (const Instrument& inst : fresh)
        token_hint.append(inst.security_id.toLongLong());
    enqueue_subscribe(token_hint);
}

void DhanWebSocket::subscribe(const QVector<qint64>& tokens) {
    auto& svc = InstrumentService::instance();
    QVector<Instrument> instruments;
    instruments.reserve(tokens.size());
    for (qint64 token : tokens) {
        Instrument inst;
        inst.security_id = QString::number(token);
        inst.exchange_segment = default_segment_;
        auto found = svc.find_by_token(static_cast<quint32>(token), "dhan");
        if (found.has_value()) {
            const QString& ex = found->exchange.isEmpty() ? found->brexchange : found->exchange;
            if (!ex.isEmpty())
                inst.exchange_segment = exchange_to_segment_string(ex);
        }
        instruments.append(inst);
    }
    subscribe(instruments);
}

void DhanWebSocket::unsubscribe() {
    // Dhan has no real unsubscribe — sending the subscribe request again with a
    // different RequestCode does not retire a feed. We mirror the OpenAlgo
    // adapter: drop local tracking and clear the merge cache. The upstream feed
    // stops when the socket closes.
    subscribed_.clear();
    clear_tick_cache();
}

void DhanWebSocket::unsubscribe(const QVector<Instrument>& instruments) {
    for (const Instrument& inst : instruments)
        subscribed_.remove(inst_key(inst));
}

// ─────────────────────────────────────────────────────────────────────────────
// Base-class hooks
// ─────────────────────────────────────────────────────────────────────────────

void DhanWebSocket::flush_subscribe_queue() {
    // The qint64 queue is only used to drive the debounce timer; the source of
    // truth is subscribed_. Drain the queue and (re)send the full set so a
    // burst of subscribe() calls collapses into batched FULL requests.
    take_subscribe_queue();
    if (subscribed_.isEmpty() || !is_connected())
        return;
    send_subscribe(subscribed_.values().toVector(), /*subscribe=*/true);
}

void DhanWebSocket::on_data_stall() {
    LOG_WARN(TAG_DHAN_WS, "Data stall — forcing reconnect");
    ws_->disconnect();
    open();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void DhanWebSocket::on_connected() {
    LOG_INFO(TAG_DHAN_WS, "Connected — resubscribing all instruments");
    resubscribe_all();
    emit connected();
}

void DhanWebSocket::on_disconnected() {
    LOG_WARN(TAG_DHAN_WS, "Disconnected");
    emit disconnected();
}

void DhanWebSocket::emit_tick(BrokerQuote merged) {
    if (merged.timestamp == 0)
        merged.timestamp = QDateTime::currentMSecsSinceEpoch();
    // Dhan's quote/full packets carry the *previous* close in the close field but
    // never send day change pre-computed — derive it here so the watchlist shows a
    // real CHG/CHG% instead of +0.00 for every symbol. (mirrors get_quotes REST.)
    if (merged.close > 0.0) {
        merged.change = merged.ltp - merged.close;
        merged.change_pct = (merged.ltp - merged.close) / merged.close * 100.0;
    }
    emit tick_received(merged);
}

void DhanWebSocket::on_binary_message(const QByteArray& data) {
    note_tick(); // any inbound frame (even a heartbeat) keeps the watchdog happy

    const uchar* buf = reinterpret_cast<const uchar*>(data.constData());
    const int total = data.size();
    int offset = 0;

    // A single frame may pack multiple response packets back-to-back.
    while (offset + 8 <= total) {
        const uchar* hdr = buf + offset;
        const int response_code = int(hdr[0]);
        const int message_length = int(read_u16(hdr + 1)); // total incl. 8-byte header
        const int exchange_segment = int(hdr[3]);
        const quint32 security_id = read_u32(hdr + 4);

        if (message_length < 8 || offset + message_length > total) {
            // Truncated / malformed packet — stop to avoid reading past the buffer.
            LOG_DEBUG(TAG_DHAN_WS, QString("Incomplete packet (code=%1 len=%2 have=%3)")
                                       .arg(response_code).arg(message_length).arg(total - offset));
            break;
        }

        const uchar* payload = hdr + 8;
        const int payload_len = message_length - 8;
        const QString fallback = QString::number(security_id);

        switch (response_code) {
            case 2: { // TICKER (8B payload: ltp f32, ltt u32)
                BrokerQuote q = parse_ticker(payload, payload_len, exchange_segment, security_id);
                emit_tick(merge_tick(fallback, q));
                break;
            }
            case 4: { // QUOTE (42B payload)
                BrokerQuote q = parse_quote(payload, payload_len, exchange_segment, security_id);
                emit_tick(merge_tick(fallback, q));
                break;
            }
            case 5: { // OI (4B payload: oi u32)
                if (payload_len >= 4) {
                    BrokerQuote q;
                    q.symbol = symbol_for_security(security_id, fallback);
                    q.oi = qint64(read_u32(payload));
                    emit_tick(merge_tick(fallback, q));
                }
                break;
            }
            case 6: { // PREV_CLOSE (8B payload: prev_close f32, prev_oi u32)
                if (payload_len >= 4) {
                    BrokerQuote q;
                    q.symbol = symbol_for_security(security_id, fallback);
                    q.close = read_f32(payload); // previous close carried into close field
                    emit_tick(merge_tick(fallback, q));
                }
                break;
            }
            case 8: { // FULL (154B payload: quote + 5-level depth)
                MarketDepth depth;
                bool has_depth = false;
                BrokerQuote q =
                    parse_full(payload, payload_len, exchange_segment, security_id, depth, has_depth);
                emit_tick(merge_tick(fallback, q));
                if (has_depth)
                    emit depth_received(depth);
                break;
            }
            case 50: // DISCONNECT
                handle_disconnect(payload, payload_len);
                break;
            case 0: // heartbeat / ack — silently ignore
                break;
            default:
                LOG_DEBUG(TAG_DHAN_WS, QString("Unknown response code: %1").arg(response_code));
                break;
        }

        offset += message_length;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Wire senders
// ─────────────────────────────────────────────────────────────────────────────

void DhanWebSocket::send_subscribe(const QVector<Instrument>& instruments, bool subscribe) {
    if (instruments.isEmpty() || !is_connected())
        return;

    // Dhan has no real unsubscribe; a "false" request is a no-op upstream. We
    // still gate on it so callers can express intent without spamming the wire.
    if (!subscribe)
        return;

    // Batch into <=100 instruments per FULL subscribe message.
    for (int start = 0; start < instruments.size(); start += kBatchSize) {
        const int end = std::min(start + kBatchSize, int(instruments.size()));
        QJsonArray list;
        for (int j = start; j < end; ++j) {
            QJsonObject entry;
            entry["ExchangeSegment"] = instruments[j].exchange_segment;
            entry["SecurityId"] = instruments[j].security_id;
            list.append(entry);
        }
        QJsonObject msg;
        msg["RequestCode"] = kReqMode; // FULL (21)
        msg["InstrumentCount"] = end - start;
        msg["InstrumentList"] = list;
        ws_->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
        LOG_INFO(TAG_DHAN_WS, QString("Subscribed %1 instruments (FULL)").arg(end - start));
    }
}

void DhanWebSocket::resubscribe_all() {
    if (subscribed_.isEmpty())
        return;
    send_subscribe(subscribed_.values().toVector(), /*subscribe=*/true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Symbol enrichment
// ─────────────────────────────────────────────────────────────────────────────

QString DhanWebSocket::symbol_for_security(quint32 security_id, const QString& fallback) const {
    auto inst = InstrumentService::instance().find_by_token(security_id, "dhan");
    return inst.has_value() && !inst->symbol.isEmpty() ? inst->symbol : fallback;
}

// ─────────────────────────────────────────────────────────────────────────────
// Binary packet parsers — offsets per dhan_websocket.py
// ─────────────────────────────────────────────────────────────────────────────

BrokerQuote DhanWebSocket::parse_ticker(const uchar* payload, int len, int /*segment*/,
                                        quint32 security_id) const {
    // _parse_ticker_packet: ltp f32 @0, ltt u32 @4 (epoch seconds).
    BrokerQuote q;
    q.symbol = symbol_for_security(security_id, QString::number(security_id));
    if (len < 8)
        return q;
    q.ltp = read_f32(payload + 0);
    const quint32 ltt = read_u32(payload + 4);
    if (ltt != 0)
        q.timestamp = qint64(ltt) * 1000; // seconds → ms
    return q;
}

BrokerQuote DhanWebSocket::parse_quote(const uchar* payload, int len, int /*segment*/,
                                       quint32 security_id) const {
    // _parse_quote_packet (42 bytes):
    //   ltp f32 @0, ltq u16 @4, ltt u32 @6, atp f32 @10, volume u32 @14,
    //   total_sell_qty u32 @18, total_buy_qty u32 @22,
    //   open f32 @26, close f32 @30, high f32 @34, low f32 @38.
    BrokerQuote q;
    q.symbol = symbol_for_security(security_id, QString::number(security_id));
    if (len < 42)
        return q;
    q.ltp = read_f32(payload + 0);
    // ltq u16 @4 — no dedicated BrokerQuote field; skip.
    const quint32 ltt = read_u32(payload + 6);
    if (ltt != 0)
        q.timestamp = qint64(ltt) * 1000;
    // atp f32 @10 — average traded price; no BrokerQuote field; skip.
    q.volume = read_u32(payload + 14);
    q.ask_size = read_u32(payload + 18); // total_sell_qty
    q.bid_size = read_u32(payload + 22); // total_buy_qty
    q.open = read_f32(payload + 26);
    q.close = read_f32(payload + 30);
    q.high = read_f32(payload + 34);
    q.low = read_f32(payload + 38);
    return q;
}

BrokerQuote DhanWebSocket::parse_full(const uchar* payload, int len, int segment, quint32 security_id,
                                      MarketDepth& depth_out, bool& has_depth) const {
    // _parse_full_packet (154 bytes):
    //   ltp f32 @0, ltq u16 @4, ltt u32 @6, atp f32 @10, volume u32 @14,
    //   total_sell_qty u32 @18, total_buy_qty u32 @22, oi u32 @26,
    //   oi_high u32 @30, oi_low u32 @34, open f32 @38, close f32 @42,
    //   high f32 @46, low f32 @50, then 5×20-byte depth @54.
    //   Each depth level: bid_qty u32, ask_qty u32, bid_orders u16,
    //   ask_orders u16, bid_price f32, ask_price f32.
    has_depth = false;
    BrokerQuote q;
    q.symbol = symbol_for_security(security_id, QString::number(security_id));
    if (len < 154)
        return q;

    q.ltp = read_f32(payload + 0);
    // ltq u16 @4 — skip.
    const quint32 ltt = read_u32(payload + 6);
    if (ltt != 0)
        q.timestamp = qint64(ltt) * 1000;
    // atp f32 @10 — skip.
    q.volume = read_u32(payload + 14);
    q.ask_size = read_u32(payload + 18); // total_sell_qty
    q.bid_size = read_u32(payload + 22); // total_buy_qty
    q.oi = qint64(read_u32(payload + 26));
    // oi_high u32 @30, oi_low u32 @34 — no BrokerQuote field; skip.
    q.open = read_f32(payload + 38);
    q.close = read_f32(payload + 42);
    q.high = read_f32(payload + 46);
    q.low = read_f32(payload + 50);

    // 5-level depth at offset 54, 20 bytes per level.
    depth_out.symbol = q.symbol;
    depth_out.exchange = segment_code_to_exchange(segment);
    depth_out.ltp = q.ltp;
    depth_out.volume = q.volume;
    depth_out.oi = double(q.oi);
    depth_out.bids.clear();
    depth_out.asks.clear();
    for (int i = 0; i < 5; ++i) {
        const uchar* lvl = payload + 54 + i * 20;
        const quint32 bid_qty = read_u32(lvl + 0);
        const quint32 ask_qty = read_u32(lvl + 4);
        const quint16 bid_orders = read_u16(lvl + 8);
        const quint16 ask_orders = read_u16(lvl + 10);
        const float bid_price = read_f32(lvl + 12);
        const float ask_price = read_f32(lvl + 16);

        DepthLevel bid;
        bid.price = bid_price;
        bid.quantity = int(bid_qty);
        bid.orders = int(bid_orders);
        depth_out.bids.append(bid);

        DepthLevel ask;
        ask.price = ask_price;
        ask.quantity = int(ask_qty);
        ask.orders = int(ask_orders);
        depth_out.asks.append(ask);
    }
    has_depth = true;

    // Surface best bid/ask on the quote too (level 0 of the book).
    if (!depth_out.bids.isEmpty()) {
        q.bid = depth_out.bids.first().price;
    }
    if (!depth_out.asks.isEmpty()) {
        q.ask = depth_out.asks.first().price;
    }
    return q;
}

void DhanWebSocket::handle_disconnect(const uchar* payload, int len) {
    if (len < 2) {
        LOG_WARN(TAG_DHAN_WS, "Received disconnect packet (no code)");
        return;
    }
    const quint16 code = read_u16(payload);

    // Disconnect codes per DhanHQ v2 live-market-feed docs.
    QString reason;
    bool fatal = false;         // retrying won't help → stop the reconnect timer
    bool token_problem = false; // genuine token/auth failure → surface as TOKEN_EXPIRED
    switch (code) {
        case 805:
            reason = QStringLiteral("Connection limit exceeded (max 5 connections per Client ID)");
            break;
        case 806:
            // Entitlement gap, NOT a token problem: the account is valid for
            // trading but the market-data (Data API) feed isn't subscribed.
            // Reconnecting won't help, but the account must stay Connected and
            // must NOT be reported as an expired token.
            reason = QStringLiteral("Market data feed not subscribed (Dhan Data API) — quotes unavailable; "
                                    "trading is unaffected. Enable the Data API subscription on dhan.co.");
            fatal = true;
            break;
        case 807:
            reason = QStringLiteral("Access token expired");
            fatal = true;
            token_problem = true;
            break;
        case 808:
            reason = QStringLiteral("Authentication failed — invalid Client ID or token");
            fatal = true;
            token_problem = true;
            break;
        case 809:
            reason = QStringLiteral("Access token invalid");
            fatal = true;
            token_problem = true;
            break;
        default:
            reason = QStringLiteral("Unknown reason");
            break;
    }
    LOG_WARN(TAG_DHAN_WS, QString("Disconnect packet code=%1 (%2)").arg(code).arg(reason));

    if (fatal)
        ws_->stop_reconnect(); // no point storming reconnects on any fatal code

    if (token_problem) {
        // The [TOKEN_EXPIRED] sentinel routes through AccountDataStream's
        // check_token_expiry() to mark the account TokenExpired and prompt re-auth.
        emit error_occurred(QString("[TOKEN_EXPIRED] Dhan feed disconnect %1: %2").arg(code).arg(reason));
    } else if (fatal) {
        // Surfaced as a plain (non-expiry) error so the account stays Connected.
        emit error_occurred(QString("Dhan feed disconnect %1: %2").arg(code).arg(reason));
    }
}

} // namespace fincept::trading
