// KotakWebSocket — Kotak Neo HSI binary market-data adapter.
//
// See the header for the protocol summary. OpenAlgo references:
//   broker/kotak/streaming/HSWebSocketLib.py   (binary framing — authoritative)
//   broker/kotak/streaming/kotak_websocket.py  (JSON→binary glue + tick parsing)
//   broker/kotak/streaming/kotak_adapter.py    (batch/merge/mode handling)

#include "trading/websocket/KotakWebSocket.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QStringList>

#include <cmath>

namespace fincept::trading {

static constexpr const char* TAG_KOTAK_WS = "KotakWS";

// ── HSI protocol constants (from HSWebSocketLib.py) ─────────────────────────
namespace {
constexpr quint8 CONNECTION_TYPE = 1;
constexpr quint8 DATA_TYPE = 6;
constexpr quint8 SUBSCRIBE_TYPE = 4;
constexpr quint8 UNSUBSCRIBE_TYPE = 5;

constexpr quint8 RESP_SNAP = 83; // 'S'
constexpr quint8 RESP_UPDATE = 85; // 'U'

// HSI's "trash"/invalid long value — fields carrying it are ignored.
constexpr qint64 TRASH_VAL = -2147483648LL;

// SCRIP feed field indices (SCRIP_MAPPING in the py source).
constexpr int SC_VOLUME = 4;
constexpr int SC_LTP = 5;
constexpr int SC_BP = 9;   // best bid price
constexpr int SC_SP = 10;  // best ask price
constexpr int SC_BQ = 11;  // best bid qty
constexpr int SC_BS = 12;  // best ask qty
constexpr int SC_LOW = 14; // "lo"
constexpr int SC_HIGH = 15; // "h"
constexpr int SC_OPEN = 20; // "op"
constexpr int SC_CLOSE = 21; // "c" (prev close)
constexpr int SC_OI = 22;
constexpr int SC_MULT = 23;
constexpr int SC_PREC = 24;

// INDEX feed field indices (INDEX_MAPPING).
constexpr int IX_LTP = 2;    // "iv"
constexpr int IX_CLOSE = 3;  // "ic"
constexpr int IX_HIGH = 5;
constexpr int IX_LOW = 6;
constexpr int IX_OPEN = 7;
constexpr int IX_MULT = 8;
constexpr int IX_PREC = 9;

// DEPTH feed field indices (DEPTH_MAPPING). bp/bp1..bp4 bids, sp/sp1..sp4 asks.
constexpr int DP_BP0 = 2;  // bp, bp1, bp2, bp3, bp4 → 2..6
constexpr int DP_SP0 = 7;  // sp, sp1, sp2, sp3, sp4 → 7..11
constexpr int DP_BQ0 = 12; // bq, bq1..bq4 → 12..16
constexpr int DP_BS0 = 17; // bs(ask qty l0), bs1..bs4 → 17..21
constexpr int DP_BNO0 = 22; // bid orders bno1..bno5 → 22..26
constexpr int DP_SNO0 = 27; // ask orders sno1..sno5 → 27..31
constexpr int DP_MULT = 32;
constexpr int DP_PREC = 33;

// Shared string field indices (STRING_INDEX).
[[maybe_unused]] constexpr int STR_NAME = 51;
constexpr int STR_SYMBOL = 52; // token ("tk")
constexpr int STR_EXCHG = 53;  // exchange ("e")
constexpr int STR_TSYMBOL = 54; // trading symbol ("ts")

// Big-endian readers over a const uchar*.
quint16 be16(const uchar* p) { return quint16((quint16(p[0]) << 8) | p[1]); }
quint32 be32(const uchar* p) {
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | p[3];
}

// Append helpers for binary frame construction (big-endian).
void put8(QByteArray& b, quint8 v) { b.append(char(v)); }
void put16(QByteArray& b, quint16 v) {
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
}
void put32(QByteArray& b, quint32 v) {
    b.append(char((v >> 24) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

KotakWebSocket::KotakWebSocket(const QString& auth_token, const QString& sid,
                               const QString& hs_server_id, const QString& access_token,
                               QObject* parent)
    : BrokerWebSocketBase(parent), auth_token_(auth_token), sid_(sid),
      hs_server_id_(hs_server_id), access_token_(access_token) {
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &KotakWebSocket::on_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &KotakWebSocket::on_disconnected);
    connect(ws_, &WebSocketClient::binary_message_received, this, &KotakWebSocket::on_binary_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &KotakWebSocket::error_occurred);
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void KotakWebSocket::open() {
    auth_ok_ = false;
    LOG_INFO(TAG_KOTAK_WS, "Connecting to Kotak HSI feed");
    ws_->connect_to(QString::fromLatin1(kWsUrl));
    start_health_check();
}

void KotakWebSocket::close() {
    stop_health_check();
    auth_ok_ = false;
    ws_->disconnect();
}

bool KotakWebSocket::is_connected() const {
    return ws_->is_connected() && auth_ok_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Subscription management
// ─────────────────────────────────────────────────────────────────────────────

void KotakWebSocket::subscribe(const QVector<qint64>& tokens) {
    QVector<qint64> new_tokens;
    for (qint64 t : tokens) {
        if (!subscribed_tokens_.contains(t)) {
            subscribed_tokens_.insert(t);
            new_tokens.append(t);
        }
    }
    // Debounce a burst of subscribe() calls (e.g. an option-chain load) into one
    // batch; the wire send happens in flush_subscribe_queue().
    if (!new_tokens.isEmpty())
        enqueue_subscribe(new_tokens);
}

void KotakWebSocket::unsubscribe() {
    if (subscribed_tokens_.isEmpty())
        return;
    QVector<qint64> all(subscribed_tokens_.begin(), subscribed_tokens_.end());
    if (is_connected())
        send_subscribe(all, /*subscribe=*/false);
    subscribed_tokens_.clear();
    clear_tick_cache();
    topics_.clear();
}

void KotakWebSocket::unsubscribe(const QVector<qint64>& tokens) {
    QVector<qint64> to_remove;
    for (qint64 t : tokens) {
        if (subscribed_tokens_.remove(t))
            to_remove.append(t);
    }
    if (!to_remove.isEmpty() && is_connected())
        send_subscribe(to_remove, /*subscribe=*/false);
}

void KotakWebSocket::flush_subscribe_queue() {
    QVector<qint64> tokens = take_subscribe_queue();
    if (tokens.isEmpty() || !is_connected())
        return;
    send_subscribe(tokens, /*subscribe=*/true);
}

void KotakWebSocket::on_data_stall() {
    LOG_WARN(TAG_KOTAK_WS, "Data stall — forcing reconnect");
    auth_ok_ = false;
    ws_->disconnect();
    open();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void KotakWebSocket::on_connected() {
    LOG_INFO(TAG_KOTAK_WS, "Transport open — sending HSI connection request");
    // Wait for the connection ack before flushing subscriptions; the cn frame
    // carries the JWT (auth_token) as "Authorization" and sid as "Sid".
    ws_->send_binary(build_connection_request(auth_token_, sid_));
}

void KotakWebSocket::on_disconnected() {
    LOG_WARN(TAG_KOTAK_WS, "Disconnected");
    auth_ok_ = false;
    emit disconnected();
}

// ─────────────────────────────────────────────────────────────────────────────
// Wire builders (binary, big-endian) — port of HSWebSocketLib.py
// ─────────────────────────────────────────────────────────────────────────────

QByteArray KotakWebSocket::build_connection_request(const QString& jwt, const QString& sid) {
    // prepareConnectionRequest2(jwt, redis_key):
    //   [len:2][type=1][sub=3][seg=1][short jwtLen][jwt][2][short redisLen][redis]
    //   [3][short srcLen]["JS_API"]
    // markStartOfMsg reserves the leading 2 length bytes; markEndOfMsg fills them
    // with (bytes_written_after_length_field).
    const QByteArray jwt_b = jwt.toUtf8();
    const QByteArray sid_b = sid.toUtf8();
    static const QByteArray src = QByteArrayLiteral("JS_API");

    QByteArray body;
    put8(body, CONNECTION_TYPE);
    put8(body, 3);
    put8(body, 1);
    put16(body, quint16(jwt_b.size()));
    body.append(jwt_b);
    put8(body, 2);
    put16(body, quint16(sid_b.size()));
    body.append(sid_b);
    put8(body, 3);
    put16(body, quint16(src.size()));
    body.append(src);

    QByteArray frame;
    put16(frame, quint16(body.size())); // length prefix = body length
    frame.append(body);
    return frame;
}

QByteArray KotakWebSocket::build_subs_request(const QString& scrips_amp, int subscribe_type,
                                              const QString& prefix, int channel_num) {
    // getScripByteArray: prefix every "<exch>|<token>" scrip with "<prefix>|",
    // then [count:2] then per scrip [len:1][chars].
    QStringList scrips = scrips_amp.split('&', Qt::SkipEmptyParts);
    QByteArray data_arr;
    put16(data_arr, quint16(scrips.size()));
    for (const QString& s : scrips) {
        const QByteArray full = (prefix + "|" + s).toUtf8();
        put8(data_arr, quint8(full.size() & 0xFF));
        data_arr.append(full);
    }

    // prepareSubsUnSubsRequest:
    //   [len:2][type][2][1][short dataLen][dataArr][2][short 1][channel]
    QByteArray body;
    put8(body, quint8(subscribe_type));
    put8(body, 2);
    put8(body, 1);
    put16(body, quint16(data_arr.size()));
    body.append(data_arr);
    put8(body, 2);
    put16(body, 1);
    put8(body, quint8(channel_num));

    QByteArray frame;
    put16(frame, quint16(body.size()));
    frame.append(body);
    return frame;
}

QByteArray KotakWebSocket::build_ack_request(qint32 msg_num) {
    // get_acknowledgement_req: [len:2][ACK_TYPE=3][1][1][short 4][int msg_num]
    QByteArray body;
    put8(body, 3); // ACK_TYPE
    put8(body, 1);
    put8(body, 1);
    put16(body, 4);
    put32(body, quint32(msg_num));

    QByteArray frame;
    put16(frame, quint16(body.size()));
    frame.append(body);
    return frame;
}

void KotakWebSocket::send_subscribe(const QVector<qint64>& tokens, bool subscribe) {
    if (tokens.isEmpty())
        return;

    const int type = subscribe ? SUBSCRIBE_TYPE : UNSUBSCRIBE_TYPE;

    // Build "<exch>|<token>" scrips, batched at kMaxScripsPerFrame. For each
    // batch we send a scrip ("sf", quotes) and a depth ("dp", order book) frame.
    QStringList scrip_list;
    scrip_list.reserve(tokens.size());
    for (qint64 t : tokens) {
        const QString s = scrip_for_token(t);
        if (!s.isEmpty())
            scrip_list.append(s);
    }
    if (scrip_list.isEmpty())
        return;

    for (int i = 0; i < scrip_list.size(); i += kMaxScripsPerFrame) {
        const QStringList chunk = scrip_list.mid(i, kMaxScripsPerFrame);
        const QString joined = chunk.join('&');
        ws_->send_binary(build_subs_request(joined, type, QStringLiteral("sf"), 1));
        ws_->send_binary(build_subs_request(joined, type, QStringLiteral("dp"), 1));
    }
    LOG_INFO(TAG_KOTAK_WS, QString("%1 %2 scrips")
                               .arg(subscribe ? "Subscribed" : "Unsubscribed")
                               .arg(scrip_list.size()));
}

void KotakWebSocket::resubscribe_all() {
    if (subscribed_tokens_.isEmpty())
        return;
    QVector<qint64> all(subscribed_tokens_.begin(), subscribed_tokens_.end());
    send_subscribe(all, /*subscribe=*/true);
}

QString KotakWebSocket::scrip_for_token(qint64 token) const {
    QString exchange = default_exchange_;
    auto inst = InstrumentService::instance().find_by_token(static_cast<quint32>(token), "kotak");
    if (inst.has_value()) {
        const QString& ex = inst->brexchange.isEmpty() ? inst->exchange : inst->brexchange;
        if (!ex.isEmpty())
            exchange = ex;
    }
    return QString("%1|%2").arg(exchange).arg(token);
}

// ─────────────────────────────────────────────────────────────────────────────
// Inbound decode — port of HSWrapper.parseData
// ─────────────────────────────────────────────────────────────────────────────

void KotakWebSocket::on_binary_message(const QByteArray& data) {
    parse_data(data);
}

void KotakWebSocket::parse_data(const QByteArray& data) {
    const int total = data.size();
    if (total < 3)
        return;
    const uchar* e = reinterpret_cast<const uchar*>(data.constData());

    int pos = 0;
    pos += 2; // packetsCount (unused)
    const quint8 type = e[pos];
    pos += 1;

    // Defensive bounds-checked reads: any out-of-range index aborts the frame.
    auto avail = [&](int need) { return pos + need <= total; };

    if (type == CONNECTION_TYPE) {
        // Connection ack: field count, then field(s) carrying the "K"/"N" status.
        if (!avail(1))
            return;
        const int fcount = e[pos];
        pos += 1;
        QString status;
        if (fcount >= 1) {
            if (!avail(1))
                return;
            pos += 1; // field id
            if (!avail(2))
                return;
            const int vlen = be16(e + pos);
            pos += 2;
            if (!avail(vlen))
                return;
            status = QString::fromUtf8(reinterpret_cast<const char*>(e + pos), vlen);
            pos += vlen;
            // Second field (ack count) present when fcount >= 2.
            if (fcount >= 2 && avail(3)) {
                pos += 1; // field id
                const int alen = be16(e + pos);
                pos += 2;
                if (avail(alen)) {
                    qint64 ack = 0;
                    for (int i = 0; i < alen; ++i)
                        ack = (ack << 8) | e[pos + i];
                    ack_num_ = int(ack);
                    pos += alen;
                }
            }
        }
        if (status == QStringLiteral("K")) {
            LOG_INFO(TAG_KOTAK_WS, "HSI connection acknowledged — flushing subscriptions");
            auth_ok_ = true;
            ack_counter_ = 0;
            emit connected();
            resubscribe_all();
        } else {
            LOG_ERROR(TAG_KOTAK_WS, QString("HSI connection failed (status=%1)").arg(status));
            emit error_occurred(QStringLiteral("Kotak HSI authentication failed"));
        }
        return;
    }

    if (type != DATA_TYPE)
        return; // subscribe/snapshot acks — ignore (subscription is best-effort)

    note_tick();

    // Optional ack handshake: when ack_num_>0 the server prefixes a 4-byte
    // message number and expects an ACK frame once we've seen ack_num_ messages.
    if (ack_num_ > 0) {
        if (!avail(4))
            return;
        ack_counter_ += 1;
        const qint32 msg_num = qint32(be32(e + pos));
        pos += 4;
        if (ack_counter_ >= ack_num_) {
            ws_->send_binary(build_ack_request(msg_num));
            ack_counter_ = 0;
        }
    }

    if (!avail(2))
        return;
    const int rec_count = be16(e + pos);
    pos += 2;

    for (int r = 0; r < rec_count; ++r) {
        if (!avail(2))
            return;
        const int sub_len = be16(e + pos);
        pos += 2;
        const int sub_start = pos;
        if (sub_len <= 0 || sub_start + sub_len > total)
            return;

        const quint8 resp = e[pos];
        pos += 1;

        TopicRecord* rec = nullptr;
        qint64 topic_id = 0;

        if (resp == RESP_SNAP) {
            if (!avail(4)) { pos = sub_start + sub_len; continue; }
            topic_id = be32(e + pos);
            pos += 4;
            if (!avail(1)) { pos = sub_start + sub_len; continue; }
            const int name_len = e[pos];
            pos += 1;
            if (!avail(name_len)) { pos = sub_start + sub_len; continue; }
            const QString topic_name =
                QString::fromUtf8(reinterpret_cast<const char*>(e + pos), name_len);
            pos += name_len;

            // topic_name is "<feed>|<exch>|<token>" — feed is the prefix.
            const QString feed = topic_name.section('|', 0, 0);

            // Evict oldest topic if at cap (mirrors the py _max_topics guard).
            if (topics_.size() >= kMaxTopics && !topics_.contains(topic_id))
                topics_.erase(topics_.begin());

            TopicRecord& tr = topics_[topic_id];
            tr.feed_type = feed;
            rec = &tr;

            // Long field block: fcount1 × (4-byte value), index = field id.
            if (!avail(1)) { pos = sub_start + sub_len; continue; }
            int fcount = e[pos];
            pos += 1;
            for (int k = 0; k < fcount; ++k) {
                if (!avail(4)) { pos = sub_start + sub_len; break; }
                const qint64 v = qint64(qint32(be32(e + pos)));
                pos += 4;
                if (v != TRASH_VAL)
                    tr.fields[k] = v;
            }
            // Pull multiplier/precision out so FLOAT32 fields decode correctly.
            const int mult_idx = (feed == "if") ? IX_MULT : (feed == "dp") ? DP_MULT : SC_MULT;
            const int prec_idx = (feed == "if") ? IX_PREC : (feed == "dp") ? DP_PREC : SC_PREC;
            if (tr.fields.contains(mult_idx) && tr.fields[mult_idx] > 0)
                tr.multiplier = double(tr.fields[mult_idx]);
            if (tr.fields.contains(prec_idx) && tr.fields[prec_idx] >= 0) {
                tr.precision = int(tr.fields[prec_idx]);
                tr.precision_value = std::pow(10.0, tr.precision);
            }

            // String field block: fcount2 × (1-byte fid, 1-byte len, chars).
            if (avail(1)) {
                int scount = e[pos];
                pos += 1;
                for (int k = 0; k < scount; ++k) {
                    if (!avail(2)) break;
                    const int fid = e[pos];
                    pos += 1;
                    const int dlen = e[pos];
                    pos += 1;
                    if (!avail(dlen)) break;
                    const QString sval =
                        QString::fromUtf8(reinterpret_cast<const char*>(e + pos), dlen);
                    pos += dlen;
                    if (fid == STR_SYMBOL)
                        tr.token = sval;
                    else if (fid == STR_EXCHG)
                        tr.exchange = sval;
                    else if (fid == STR_TSYMBOL)
                        tr.tsymbol = sval;
                }
            }
        } else if (resp == RESP_UPDATE) {
            if (!avail(4)) { pos = sub_start + sub_len; continue; }
            topic_id = be32(e + pos);
            pos += 4;
            auto it = topics_.find(topic_id);
            if (it == topics_.end()) {
                // Unknown topic — skip its long block to stay frame-aligned.
                pos = sub_start + sub_len;
                continue;
            }
            rec = &it.value();
            if (!avail(1)) { pos = sub_start + sub_len; continue; }
            int fcount = e[pos];
            pos += 1;
            for (int k = 0; k < fcount; ++k) {
                if (!avail(4)) break;
                const qint64 v = qint64(qint32(be32(e + pos)));
                pos += 4;
                if (v != TRASH_VAL)
                    rec->fields[k] = v;
            }
        } else {
            // Unknown response type — skip the whole sub-message.
            pos = sub_start + sub_len;
            continue;
        }

        // Keep pos aligned to the declared sub-message length regardless of any
        // trailing bytes we didn't consume.
        pos = sub_start + sub_len;

        if (!rec)
            continue;

        // ── Emit ──────────────────────────────────────────────────────────
        const QString key = QString::number(topic_id);
        const double divisor = rec->multiplier * rec->precision_value;
        auto fval = [&](int idx) -> double {
            auto it = rec->fields.constFind(idx);
            if (it == rec->fields.constEnd())
                return 0.0;
            return divisor != 0.0 ? double(it.value()) / divisor : double(it.value());
        };
        auto lval = [&](int idx) -> qint64 {
            auto it = rec->fields.constFind(idx);
            return it == rec->fields.constEnd() ? 0 : it.value();
        };

        const QString sym = !rec->tsymbol.isEmpty() ? rec->tsymbol
                            : !rec->token.isEmpty()  ? rec->token
                                                     : key;

        if (rec->feed_type == "dp") {
            MarketDepth depth;
            depth.symbol = sym;
            depth.exchange = rec->exchange;
            for (int lvl = 0; lvl < 5; ++lvl) {
                DepthLevel bid;
                bid.price = fval(DP_BP0 + lvl);
                bid.quantity = int(lval(DP_BQ0 + lvl));
                bid.orders = int(lval(DP_BNO0 + lvl));
                depth.bids.append(bid);

                DepthLevel ask;
                ask.price = fval(DP_SP0 + lvl);
                ask.quantity = int(lval(DP_BS0 + lvl));
                ask.orders = int(lval(DP_SNO0 + lvl));
                depth.asks.append(ask);
            }
            emit depth_received(depth);
            continue;
        }

        BrokerQuote partial;
        partial.symbol = sym;
        if (rec->feed_type == "if") {
            partial.ltp = fval(IX_LTP);
            partial.open = fval(IX_OPEN);
            partial.high = fval(IX_HIGH);
            partial.low = fval(IX_LOW);
            partial.close = fval(IX_CLOSE);
        } else { // "sf" scrip / quote
            partial.ltp = fval(SC_LTP);
            partial.open = fval(SC_OPEN);
            partial.high = fval(SC_HIGH);
            partial.low = fval(SC_LOW);
            partial.close = fval(SC_CLOSE);
            partial.volume = double(lval(SC_VOLUME));
            partial.bid = fval(SC_BP);
            partial.ask = fval(SC_SP);
            partial.bid_size = double(lval(SC_BQ));
            partial.ask_size = double(lval(SC_BS));
            partial.oi = lval(SC_OI);
        }
        partial.timestamp = QDateTime::currentMSecsSinceEpoch();

        // HSI sends incremental updates with unchanged fields absent (= 0 here),
        // so route through merge_tick to preserve last non-zero OHLC.
        BrokerQuote merged = merge_tick(key, partial);
        emit tick_received(merged);
    }
}

} // namespace fincept::trading
