// FyersWebSocket — HSM binary tick client
//
// Protocol: wss://socket.fyers.in/hsm/v1-5/prod
//   1. Extract hsm_key from JWT access_token payload
//   2. Connect (no special headers)
//   3. Send binary auth message with hsm_key
//   4. Send binary mode message (full=70)
//   5. Resolve symbols → fyTokens → HSM topic strings via POST /data/symbol-token
//   6. Send binary subscribe with topic strings
//   7. Receive binary data feed: snapshot (type 83) + incremental updates (type 85)
//   8. Send binary ACK every ack_interval messages
//   9. Send binary ping [0,1,11] every 10s

#include "trading/websocket/FyersWebSocket.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent>

#include <cmath>
#include <cstring>

namespace fincept::trading {

static constexpr const char* TAG = "FyersWS";

// ── Segment code → name mapping ────────────────────────────────────────────

static QString segment_name(const QString& code) {
    static const QHash<QString, QString> m = {
        {"1010", "nse_cm"}, {"1011", "nse_fo"}, {"1012", "cde_fo"},
        {"1020", "nse_com"}, {"1120", "mcx_fo"}, {"1210", "bse_cm"},
        {"1211", "bse_fo"}, {"1212", "bcs_fo"},
    };
    return m.value(code, "nse_cm");
}

// ── Big-Endian / Little-Endian helpers ─────────────────────────────────────

quint64 FyersWebSocket::read_u64_be(const uchar* p) {
    return (quint64(p[0]) << 56) | (quint64(p[1]) << 48) | (quint64(p[2]) << 40) | (quint64(p[3]) << 32) |
           (quint64(p[4]) << 24) | (quint64(p[5]) << 16) | (quint64(p[6]) << 8)  | quint64(p[7]);
}
quint32 FyersWebSocket::read_u32_be(const uchar* p) {
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}
qint32 FyersWebSocket::read_i32_be(const uchar* p) {
    return static_cast<qint32>(read_u32_be(p));
}
quint16 FyersWebSocket::read_u16_be(const uchar* p) {
    return (quint16(p[0]) << 8) | quint16(p[1]);
}
quint16 FyersWebSocket::read_u16_le(const uchar* p) {
    return quint16(p[0]) | (quint16(p[1]) << 8);
}
void FyersWebSocket::write_u16_be(uchar* p, quint16 v) {
    p[0] = (v >> 8) & 0xFF; p[1] = v & 0xFF;
}
void FyersWebSocket::write_u32_be(uchar* p, quint32 v) {
    p[0] = (v >> 24) & 0xFF; p[1] = (v >> 16) & 0xFF; p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF;
}
void FyersWebSocket::write_u64_be(uchar* p, quint64 v) {
    for (int i = 7; i >= 0; --i) { p[7 - i] = (v >> (i * 8)) & 0xFF; }
}

double FyersWebSocket::field_to_price(qint32 raw, const TopicState& ts) const {
    if (raw == kSentinel) return 0.0;
    return raw / (std::pow(10.0, ts.precision) * ts.multiplier);
}

// ── JWT hsm_key extraction ─────────────────────────────────────────────────

QString FyersWebSocket::extract_hsm_key(const QString& jwt) {
    auto parts = jwt.split('.');
    if (parts.size() < 2) return {};
    QByteArray b64 = parts[1].toUtf8();
    while (b64.size() % 4 != 0) b64.append('=');
    QByteArray payload = QByteArray::fromBase64(b64, QByteArray::Base64UrlEncoding);
    auto doc = QJsonDocument::fromJson(payload);
    return doc.object().value("hsm_key").toString();
}

// ── Binary message builders ────────────────────────────────────────────────

QByteArray FyersWebSocket::build_auth_message() const {
    // Fields: 1=hsm_key, 2=mode("P"), 3=value(1), 4=source("FinceptTerminal")
    const QByteArray key = hsm_key_.toUtf8();
    const QByteArray mode = "P";
    const QByteArray source = "FinceptTerminal";
    const int body_len = 1 + 1    // req_type + field_count
        + 1 + 2 + key.size()      // field 1
        + 1 + 2 + mode.size()     // field 2
        + 1 + 2 + 1               // field 3
        + 1 + 2 + source.size();  // field 4
    QByteArray msg(2 + body_len, 0);
    auto* d = reinterpret_cast<uchar*>(msg.data());
    write_u16_be(d, static_cast<quint16>(body_len));
    d[2] = 1; // req_type = auth
    d[3] = 4; // field_count
    int pos = 4;
    // Field 1: hsm_key
    d[pos++] = 1;
    write_u16_be(d + pos, static_cast<quint16>(key.size())); pos += 2;
    memcpy(d + pos, key.constData(), key.size()); pos += key.size();
    // Field 2: mode "P"
    d[pos++] = 2;
    write_u16_be(d + pos, static_cast<quint16>(mode.size())); pos += 2;
    memcpy(d + pos, mode.constData(), mode.size()); pos += mode.size();
    // Field 3: value 1
    d[pos++] = 3;
    write_u16_be(d + pos, 1); pos += 2;
    d[pos++] = 1;
    // Field 4: source
    d[pos++] = 4;
    write_u16_be(d + pos, static_cast<quint16>(source.size())); pos += 2;
    memcpy(d + pos, source.constData(), source.size());
    return msg;
}

QByteArray FyersWebSocket::build_mode_message(bool lite) const {
    // req_type=12, field1=channel_bits(u64), field2=mode_byte(70=full,76=lite)
    QByteArray msg(2 + 1 + 1 + 1 + 2 + 8 + 1 + 2 + 1, 0);
    auto* d = reinterpret_cast<uchar*>(msg.data());
    write_u16_be(d, 0); // placeholder
    d[2] = 12; // req_type
    d[3] = 2;  // field_count
    d[4] = 1;  // field 1 id
    write_u16_be(d + 5, 8); // field 1 size
    quint64 channels = (1ULL << 11); // channel 11 active
    write_u64_be(d + 7, channels);
    d[15] = 2; // field 2 id
    write_u16_be(d + 16, 1);
    d[18] = lite ? 76 : 70;
    return msg;
}

QByteArray FyersWebSocket::build_subscribe_message(const QStringList& topics, bool unsub) const {
    // Build scrips_data: [u16 count][for each: u8 len, bytes string]
    QByteArray scrips;
    {
        QByteArray tmp(2, 0);
        auto* t = reinterpret_cast<uchar*>(tmp.data());
        write_u16_be(t, static_cast<quint16>(topics.size()));
        scrips.append(tmp);
    }
    for (const auto& topic : topics) {
        QByteArray tb = topic.toUtf8();
        scrips.append(static_cast<char>(tb.size() & 0xFF));
        scrips.append(tb);
    }
    const int body_len = 1 + 1               // req_type + field_count
        + 1 + 2 + scrips.size()              // field 1
        + 1 + 2 + 1;                         // field 2
    QByteArray msg(2 + body_len, 0);
    auto* d = reinterpret_cast<uchar*>(msg.data());
    write_u16_be(d, static_cast<quint16>(body_len));
    d[2] = unsub ? 5 : 4; // 4=subscribe, 5=unsubscribe
    d[3] = 2;
    int pos = 4;
    d[pos++] = 1;
    write_u16_be(d + pos, static_cast<quint16>(scrips.size())); pos += 2;
    memcpy(d + pos, scrips.constData(), scrips.size()); pos += scrips.size();
    d[pos++] = 2;
    write_u16_be(d + pos, 1); pos += 2;
    d[pos] = 11; // channel 11
    return msg;
}

QByteArray FyersWebSocket::build_ack_message(quint32 msg_number) const {
    QByteArray msg(2 + 1 + 1 + 1 + 2 + 4, 0);
    auto* d = reinterpret_cast<uchar*>(msg.data());
    write_u16_be(d, 9); // body size
    d[2] = 3; // req_type = ack
    d[3] = 1; // field_count
    d[4] = 1; // field id
    write_u16_be(d + 5, 4); // field size
    write_u32_be(d + 7, msg_number);
    return msg;
}

// ── Constructor ────────────────────────────────────────────────────────────

FyersWebSocket::FyersWebSocket(const QString& client_id, const QString& access_token, QObject* parent)
    : QObject(parent), client_id_(client_id), access_token_(access_token) {
    hsm_key_ = extract_hsm_key(access_token);
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &FyersWebSocket::on_ws_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &FyersWebSocket::on_ws_disconnected);
    connect(ws_, &WebSocketClient::binary_message_received, this, &FyersWebSocket::on_binary_message);
    connect(ws_, &WebSocketClient::message_received, this, &FyersWebSocket::on_text_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &FyersWebSocket::error_occurred);

    ping_timer_ = new QTimer(this);
    ping_timer_->setInterval(kPingIntervalMs);
    connect(ping_timer_, &QTimer::timeout, this, &FyersWebSocket::send_ping);
}

// ── Public API ─────────────────────────────────────────────────────────────

void FyersWebSocket::open() {
    if (hsm_key_.isEmpty()) {
        LOG_ERROR(TAG, "Cannot connect — failed to extract hsm_key from JWT");
        emit error_occurred("hsm_key extraction failed");
        return;
    }
    LOG_INFO(TAG, QString("Connecting to HSM (hsm_key=%1...)").arg(hsm_key_.left(8)));
    ws_->connect_to(QString(kWsUrl));
}

void FyersWebSocket::close() {
    ping_timer_->stop();
    ws_->disconnect();
    authenticated_ = false;
}

bool FyersWebSocket::is_connected() const {
    return ws_->is_connected() && authenticated_;
}

void FyersWebSocket::subscribe(const QStringList& symbols) {
    for (const auto& s : symbols)
        if (!subscribed_symbols_.contains(s))
            subscribed_symbols_.append(s);
    if (authenticated_)
        resolve_and_subscribe(symbols);
}

void FyersWebSocket::unsubscribe(const QStringList& symbols) {
    QStringList topics_to_unsub;
    for (const auto& s : symbols) {
        subscribed_symbols_.removeAll(s);
        // Find matching topics
        for (auto it = topics_.begin(); it != topics_.end(); ++it) {
            if (it->symbol == s || it->topic_name.contains(s))
                topics_to_unsub.append(it->topic_name);
        }
    }
    if (!topics_to_unsub.isEmpty() && authenticated_) {
        ws_->send_binary(build_subscribe_message(topics_to_unsub, true));
        for (const auto& t : topics_to_unsub)
            subscribed_topics_.removeAll(t);
    }
}

void FyersWebSocket::set_subscriptions(const QStringList& symbols) {
    if (authenticated_ && !subscribed_topics_.isEmpty())
        ws_->send_binary(build_subscribe_message(subscribed_topics_, true));
    subscribed_symbols_ = symbols;
    subscribed_topics_.clear();
    topics_.clear();
    if (authenticated_)
        resolve_and_subscribe(symbols);
}

void FyersWebSocket::clear_subscriptions() {
    if (authenticated_ && !subscribed_topics_.isEmpty())
        ws_->send_binary(build_subscribe_message(subscribed_topics_, true));
    subscribed_symbols_.clear();
    subscribed_topics_.clear();
    topics_.clear();
}

// ── Connection slots ───────────────────────────────────────────────────────

void FyersWebSocket::on_ws_connected() {
    LOG_INFO(TAG, "WS transport connected — sending auth");
    authenticated_ = false;
    msg_count_ = 0;
    ws_->send_binary(build_auth_message());
}

void FyersWebSocket::on_ws_disconnected() {
    ping_timer_->stop();
    authenticated_ = false;
    LOG_WARN(TAG, "Disconnected");
    emit disconnected();
}

void FyersWebSocket::on_text_message(const QString& msg) {
    LOG_INFO(TAG, QString("Text: %1").arg(msg.left(200)));
}

void FyersWebSocket::on_binary_message(const QByteArray& data) {
    if (data.size() < 3) return;
    const auto* buf = reinterpret_cast<const uchar*>(data.constData());
    const int sz = data.size();

    const quint16 msg_len = read_u16_be(buf);
    const quint8 resp_type = buf[2];
    Q_UNUSED(msg_len);

    switch (resp_type) {
    case 1: { // Auth response
        // Parse: field_count at [3], then fields. First field value = "K" means success.
        if (sz < 8) break;
        const quint8 fc = buf[3];
        if (fc < 1) break;
        // field 1: id, len, data
        int pos = 4;
        if (pos >= sz) break;
        pos++; // field_id
        if (pos + 2 > sz) break;
        const quint16 flen = read_u16_be(buf + pos); pos += 2;
        if (pos + flen > sz) break;
        QString val = QString::fromUtf8(reinterpret_cast<const char*>(buf + pos), flen);
        pos += flen;
        if (val == "K") {
            // Try to read ack_interval from field 2
            if (fc >= 2 && pos + 3 + 4 <= sz) {
                pos++; // field_id
                const quint16 f2len = read_u16_be(buf + pos); pos += 2;
                if (f2len == 4 && pos + 4 <= sz)
                    ack_interval_ = read_u32_be(buf + pos);
            }
            authenticated_ = true;
            LOG_INFO(TAG, QString("Auth OK (ack_interval=%1) — sending full mode").arg(ack_interval_));
            ws_->send_binary(build_mode_message(false));
            ping_timer_->start();
            if (!subscribed_symbols_.isEmpty())
                resolve_and_subscribe(subscribed_symbols_);
            emit connected();
        } else {
            LOG_ERROR(TAG, QString("Auth failed: %1").arg(val));
            emit error_occurred("HSM auth failed: " + val);
        }
        break;
    }
    case 4: // Subscribe ACK
        LOG_INFO(TAG, "Subscribe acknowledged");
        break;
    case 5: // Unsubscribe ACK
        LOG_DEBUG(TAG, "Unsubscribe acknowledged");
        break;
    case 6: // Data feed
        parse_data_feed(data, 3);
        break;
    case 12: // Mode ACK
        LOG_INFO(TAG, "Mode set acknowledged");
        break;
    default:
        LOG_DEBUG(TAG, QString("Unknown resp_type=%1 (size=%2)").arg(resp_type).arg(sz));
        break;
    }
}

// ── Data feed parser ───────────────────────────────────────────────────────

void FyersWebSocket::parse_data_feed(const QByteArray& data, int offset) {
    const auto* buf = reinterpret_cast<const uchar*>(data.constData());
    const int sz = data.size();
    if (offset + 6 > sz) return;

    const quint32 msg_number = read_u32_be(buf + offset); offset += 4;
    const quint16 scrip_count = read_u16_be(buf + offset); offset += 2;

    // Send ACK if needed
    ++msg_count_;
    if (ack_interval_ > 0 && msg_count_ % ack_interval_ == 0)
        ws_->send_binary(build_ack_message(msg_number));

    for (quint16 i = 0; i < scrip_count && offset < sz; ++i) {
        const quint8 data_type = buf[offset];
        switch (data_type) {
        case 83: parse_snapshot(buf, sz, offset); break;
        case 85: parse_update(buf, sz, offset); break;
        case 76: parse_lite_update(buf, sz, offset); break;
        default:
            LOG_WARN(TAG, QString("Unknown data_type=%1 at offset %2").arg(data_type).arg(offset));
            return; // Can't safely skip unknown types
        }
    }
}

void FyersWebSocket::parse_snapshot(const uchar* buf, int sz, int& pos) {
    if (pos + 4 > sz) return;
    pos++; // skip data_type (83)
    const quint16 topic_id = read_u16_le(buf + pos); pos += 2;
    if (pos >= sz) return;
    const quint8 topic_len = buf[pos++];
    if (pos + topic_len > sz) return;
    const QString topic_name = QString::fromUtf8(reinterpret_cast<const char*>(buf + pos), topic_len);
    pos += topic_len;

    if (pos >= sz) return;
    const quint8 field_count = buf[pos++];
    if (pos + field_count * 4 > sz) return;

    QVector<qint32> fields(field_count);
    for (int f = 0; f < field_count; ++f) {
        fields[f] = read_i32_be(buf + pos);
        pos += 4;
    }

    // Skip 2 bytes, then read multiplier + precision + strings
    if (pos + 2 > sz) return;
    pos += 2;

    quint16 multiplier = 1;
    quint8 precision = 2;
    QString exchange_name, exchange_token, symbol_name;

    if (pos + 3 <= sz) {
        multiplier = read_u16_be(buf + pos); pos += 2;
        precision = buf[pos++];
    }
    // exchange name
    if (pos < sz) {
        quint8 len = buf[pos++];
        if (pos + len <= sz) {
            exchange_name = QString::fromUtf8(reinterpret_cast<const char*>(buf + pos), len);
            pos += len;
        }
    }
    // exchange token
    if (pos < sz) {
        quint8 len = buf[pos++];
        if (pos + len <= sz) {
            exchange_token = QString::fromUtf8(reinterpret_cast<const char*>(buf + pos), len);
            pos += len;
        }
    }
    // symbol name
    if (pos < sz) {
        quint8 len = buf[pos++];
        if (pos + len <= sz) {
            symbol_name = QString::fromUtf8(reinterpret_cast<const char*>(buf + pos), len);
            pos += len;
        }
    }

    if (multiplier == 0) multiplier = 1;

    TopicState& ts = topics_[topic_id];
    ts.topic_name = topic_name;
    ts.symbol = symbol_name;
    ts.exchange = exchange_name;
    ts.multiplier = multiplier;
    ts.precision = precision;
    ts.fields = fields;

    LOG_INFO(TAG, QString("Snapshot: topic=%1 id=%2 sym=%3 exch=%4 prec=%5 mul=%6 fields=%7")
                      .arg(topic_name).arg(topic_id).arg(symbol_name)
                      .arg(exchange_name).arg(precision).arg(multiplier).arg(field_count));
    emit_tick(topic_id);
}

void FyersWebSocket::parse_update(const uchar* buf, int sz, int& pos) {
    if (pos + 4 > sz) return;
    pos++; // skip data_type (85)
    const quint16 topic_id = read_u16_le(buf + pos); pos += 2;
    if (pos >= sz) return;
    const quint8 field_count = buf[pos++];
    if (pos + field_count * 4 > sz) return;

    auto it = topics_.find(topic_id);
    if (it == topics_.end()) {
        pos += field_count * 4;
        return;
    }
    auto& ts = *it;
    bool changed = false;
    for (int f = 0; f < field_count && f < ts.fields.size(); ++f) {
        qint32 val = read_i32_be(buf + pos);
        pos += 4;
        if (val != kSentinel) {
            ts.fields[f] = val;
            changed = true;
        }
    }
    // Skip any extra fields beyond what we stored
    for (int f = ts.fields.size(); f < field_count; ++f)
        pos += 4;

    if (changed)
        emit_tick(topic_id);
}

void FyersWebSocket::parse_lite_update(const uchar* buf, int sz, int& pos) {
    if (pos + 7 > sz) return;
    pos++; // skip data_type (76)
    const quint16 topic_id = read_u16_le(buf + pos); pos += 2;
    const qint32 ltp_raw = read_i32_be(buf + pos); pos += 4;

    auto it = topics_.find(topic_id);
    if (it == topics_.end()) return;
    if (!it->fields.isEmpty())
        it->fields[0] = ltp_raw;
    emit_tick(topic_id);
}

// ── Emit tick from cached topic state ──────────────────────────────────────

void FyersWebSocket::emit_tick(quint16 topic_id) {
    auto it = topics_.constFind(topic_id);
    if (it == topics_.constEnd() || it->fields.isEmpty()) return;
    const auto& ts = *it;
    const auto& f = ts.fields;

    if (ts.topic_name.startsWith(QLatin1String("dp|"))) {
        emit_depth(topic_id);
        return;
    }

    FyersTick tick;

    // Normalize: "SBIN-EQ" → "SBIN", "nse_cm" → "NSE"
    tick.symbol = ts.symbol;
    if (tick.symbol.endsWith(QLatin1String("-EQ")))
        tick.symbol.chop(3);
    static const QHash<QString, QString> exch_norm = {
        {"nse_cm", "NSE"}, {"nse_fo", "NSE"}, {"cde_fo", "NSE"},
        {"nse_com", "NSE"}, {"mcx_fo", "MCX"}, {"bse_cm", "BSE"},
        {"bse_fo", "BSE"}, {"bcs_fo", "BSE"},
    };
    tick.exchange = exch_norm.value(ts.exchange, ts.exchange.section('_', 0, 0).toUpper());

    // sf topic fields: 0=ltp, 1=vol, 2=ltt, 3=eft, 4=bid_sz, 5=ask_sz,
    //   6=bid, 7=ask, 8=ltq, 9=tot_buy, 10=tot_sell, 11=atp, 12=OI,
    //   13=low, 14=high, 15=Yhigh, 16=Ylow, 17=lower_ckt, 18=upper_ckt,
    //   19=open, 20=prev_close
    auto val = [&](int idx) -> qint32 { return idx < f.size() ? f[idx] : kSentinel; };
    auto price = [&](int idx) -> double { return field_to_price(val(idx), ts); };

    tick.ltp = price(0);
    tick.volume = val(1) != kSentinel ? static_cast<double>(val(1)) : 0;
    tick.timestamp = val(2) != kSentinel ? static_cast<quint32>(val(2)) : 0;
    tick.bid = price(6);
    tick.ask = price(7);
    tick.ltq = val(8) != kSentinel ? val(8) : 0;
    tick.tot_buy_qty = val(9) != kSentinel ? static_cast<double>(val(9)) : 0;
    tick.tot_sell_qty = val(10) != kSentinel ? static_cast<double>(val(10)) : 0;
    tick.atp = price(11);
    // Field 12 is open interest (contracts) for F&O sf topics — a raw count,
    // not price-scaled. Lite updates only refresh ltp[0] so the last full
    // snapshot's OI persists in f[12]; sentinel/short arrays leave oi at 0.
    tick.oi = val(12) != kSentinel ? static_cast<double>(val(12)) : 0;
    tick.low = price(13);
    tick.high = price(14);
    tick.open = price(19);
    tick.prev_close = price(20);

    if (tick.ltp > 0)
        emit tick_received(tick);
}

// ── Depth emit ─────────────────────────────────────────────────────────────
// dp topic fields: 0-4=bid_price, 5-9=ask_price, 10-14=bid_size, 15-19=ask_size

void FyersWebSocket::emit_depth(quint16 topic_id) {
    auto it = topics_.constFind(topic_id);
    if (it == topics_.constEnd() || it->fields.size() < 20) return;
    const auto& ts = *it;
    const auto& f = ts.fields;

    QString symbol = ts.symbol;
    if (symbol.endsWith(QLatin1String("-EQ")))
        symbol.chop(3);

    QVector<QPair<double, double>> bids, asks;
    for (int i = 0; i < 5; ++i) {
        double bp = field_to_price(f[i], ts);
        double bs = f[10 + i] != kSentinel ? static_cast<double>(f[10 + i]) : 0;
        if (bp > 0) bids.append({bp, bs});
        double ap = field_to_price(f[5 + i], ts);
        double as_ = f[15 + i] != kSentinel ? static_cast<double>(f[15 + i]) : 0;
        if (ap > 0) asks.append({ap, as_});
    }

    if (!bids.isEmpty() || !asks.isEmpty())
        emit depth_received(symbol, bids, asks);
}

// ── Ping ───────────────────────────────────────────────────────────────────

void FyersWebSocket::send_ping() {
    if (ws_->is_connected()) {
        const char ping[] = {0x00, 0x01, 0x0B};
        ws_->send_binary(QByteArray(ping, 3));
    }
}

// ── Symbol resolution + subscribe ──────────────────────────────────────────

void FyersWebSocket::resolve_and_subscribe(const QStringList& symbols) {
    QPointer<FyersWebSocket> self = this;
    const QString auth = client_id_ + ":" + access_token_;
    QStringList syms = symbols;

    (void)QtConcurrent::run([self, syms, auth]() {
        // Format symbols to Fyers format if needed
        QStringList fyers_syms;
        for (const auto& s : syms) {
            if (s.contains(':'))
                fyers_syms.append(s);
            else
                fyers_syms.append(QStringLiteral("NSE:") + s + QStringLiteral("-EQ"));
        }

        QJsonArray arr;
        for (const auto& s : fyers_syms) arr.append(s);
        QJsonObject body;
        body["symbols"] = arr;
        {
            // [subdbg] F&O symbols being sent for token resolution (equities omitted
            // to keep the line readable). Lets us confirm a held option is actually
            // requested — and below, whether Fyers resolved or rejected it.
            QStringList fno;
            for (const auto& s : fyers_syms)
                if (!s.endsWith(QLatin1String("-EQ")) && (s.contains(QLatin1String("CE")) ||
                    s.contains(QLatin1String("PE")) || s.contains(QLatin1String("FUT"))))
                    fno << s;
            LOG_INFO("subdbg", QString("resolve_and_subscribe: sending %1 syms, F&O=[%2]")
                                   .arg(fyers_syms.size()).arg(fno.join(',')));
        }

        auto resp = BrokerHttp::instance().post_json(
            QStringLiteral("https://api-t1.fyers.in/data/symbol-token"), body,
            {{"Authorization", auth}, {"Content-Type", "application/json"}});

        QStringList topics;
        if (resp.success && resp.json.value("s").toString() == "ok") {
            const auto valid = resp.json.value("validSymbol").toObject();
            for (auto it = valid.begin(); it != valid.end(); ++it) {
                const QString fytoken = it.value().toString();
                if (fytoken.size() < 10) continue;
                const QString seg_code = fytoken.left(4);
                const QString exch_token = fytoken.mid(10);
                const QString seg = segment_name(seg_code);
                topics.append(QStringLiteral("sf|%1|%2").arg(seg, exch_token));
                topics.append(QStringLiteral("dp|%1|%2").arg(seg, exch_token));
            }
            LOG_INFO("FyersWS", QString("Resolved %1 symbols → %2 topics").arg(fyers_syms.size()).arg(topics.size()));
            // [subdbg] any symbols we SENT that Fyers did NOT return as valid —
            // these silently get no live feed (the suspected option failure mode).
            QStringList unresolved;
            for (const auto& s : fyers_syms)
                if (!valid.contains(s))
                    unresolved << s;
            if (!unresolved.isEmpty())
                LOG_WARN("subdbg", QString("resolve_and_subscribe: %1 UNRESOLVED (no feed): [%2]")
                                       .arg(unresolved.size()).arg(unresolved.join(',')));
            for (const auto& t : topics)
                LOG_DEBUG("FyersWS", QString("  topic: %1").arg(t));
        } else {
            LOG_ERROR("FyersWS", QString("Token resolve failed: %1 (HTTP %2)").arg(resp.error).arg(resp.status_code));
        }

        QMetaObject::invokeMethod(self, [self, topics]() {
            if (!self || topics.isEmpty()) return;
            for (const auto& t : topics)
                if (!self->subscribed_topics_.contains(t))
                    self->subscribed_topics_.append(t);
            self->ws_->send_binary(self->build_subscribe_message(topics));
            LOG_INFO("FyersWS", QString("Sent subscribe for %1 topics").arg(topics.size()));
        }, Qt::QueuedConnection);
    });
}

} // namespace fincept::trading
