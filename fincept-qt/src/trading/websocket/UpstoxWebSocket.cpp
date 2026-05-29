// UpstoxWebSocket — Upstox V3 Protobuf3 market-data streaming adapter.
//
// See the header for the protocol summary. OpenAlgo reference:
//   broker/upstox/streaming/upstox_websocket.py
//   broker/upstox/streaming/upstox_adapter.py
//   broker/upstox/streaming/MarketDataFeedV3.proto
//
// Protobuf is NOT linked into the Fincept build, so the FeedResponse message is
// decoded with a minimal hand-rolled wire-format reader (anonymous namespace
// below). All field numbers come straight from MarketDataFeedV3.proto.

#include "trading/websocket/UpstoxWebSocket.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/InstrumentService.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QMetaObject>
#include <QPointer>
#include <QUuid>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fincept::trading {

static constexpr const char* TAG_UPSTOX_WS = "UpstoxWS";

// ─────────────────────────────────────────────────────────────────────────────
// Minimal Protobuf wire-format reader
//
// Protobuf encodes each field as a varint tag (field_number << 3 | wire_type)
// followed by the value. Wire types:
//   0 = varint            (int32/int64/bool/enum)
//   1 = 64-bit            (double / fixed64)
//   2 = length-delimited  (string / bytes / embedded message / packed)
//   5 = 32-bit            (float / fixed32)
//
// We only decode the fields needed to populate BrokerQuote + MarketDepth from
// MarketDataFeedV3.FeedResponse. Submessages are decoded recursively by handing
// each handler the raw byte slice of the embedded message.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Decoded LTPC (last traded price + close).
struct PbLtpc {
    double ltp = 0.0;   // field 1
    int64_t ltt = 0;    // field 2 (epoch ms)
    int64_t ltq = 0;    // field 3
    double cp = 0.0;    // field 4 (previous close)
    bool present = false;
};

// One level of the bid/ask ladder (MarketDataFeedV3.Quote).
struct PbQuote {
    int64_t bidQ = 0;   // field 1
    double bidP = 0.0;  // field 2
    int64_t askQ = 0;   // field 3
    double askP = 0.0;  // field 4
};

// One OHLC bucket (MarketDataFeedV3.OHLC).
struct PbOhlc {
    QString interval;   // field 1
    double open = 0.0;  // field 2
    double high = 0.0;  // field 3
    double low = 0.0;   // field 4
    double close = 0.0; // field 5
    int64_t vol = 0;    // field 6
    int64_t ts = 0;     // field 7
};

// Aggregated decode of a single Feed for one instrument key.
struct PbFeed {
    PbLtpc ltpc;
    double atp = 0.0;     // MarketFullFeed.atp     (field 5)
    int64_t vtt = 0;      // MarketFullFeed.vtt     (field 6, volume traded today)
    double oi = 0.0;      // MarketFullFeed.oi      (field 7)
    double tbq = 0.0;     // MarketFullFeed.tbq     (field 9)
    double tsq = 0.0;     // MarketFullFeed.tsq     (field 10)
    std::vector<PbOhlc> ohlc;     // MarketOHLC.ohlc        (repeated)
    std::vector<PbQuote> bidAsk;  // MarketLevel.bidAskQuote (repeated, up to 5)
    bool hasFullFeed = false;
};

class ProtoReader {
  public:
    ProtoReader(const uint8_t* data, size_t len) : p_(data), end_(data + len) {}

    bool at_end() const { return p_ >= end_; }

    // Read the next field tag. Returns false at end / on corruption.
    bool next_tag(uint32_t& field_number, uint32_t& wire_type) {
        if (p_ >= end_)
            return false;
        uint64_t tag = 0;
        if (!read_varint(tag))
            return false;
        field_number = static_cast<uint32_t>(tag >> 3);
        wire_type = static_cast<uint32_t>(tag & 0x7);
        return true;
    }

    bool read_varint(uint64_t& out) {
        out = 0;
        int shift = 0;
        while (p_ < end_ && shift < 64) {
            uint8_t b = *p_++;
            out |= static_cast<uint64_t>(b & 0x7F) << shift;
            if (!(b & 0x80))
                return true;
            shift += 7;
        }
        return false;
    }

    bool read_double(double& out) {
        if (p_ + 8 > end_)
            return false;
        uint64_t bits = 0;
        std::memcpy(&bits, p_, 8); // little-endian on all supported platforms
        std::memcpy(&out, &bits, 8);
        p_ += 8;
        return true;
    }

    bool read_float(float& out) {
        if (p_ + 4 > end_)
            return false;
        uint32_t bits = 0;
        std::memcpy(&bits, p_, 4);
        std::memcpy(&out, &bits, 4);
        p_ += 4;
        return true;
    }

    // Read a length-delimited slice (string / bytes / submessage) without copy.
    bool read_bytes(const uint8_t*& begin, size_t& length) {
        uint64_t len = 0;
        if (!read_varint(len))
            return false;
        if (p_ + len > end_)
            return false;
        begin = p_;
        length = static_cast<size_t>(len);
        p_ += len;
        return true;
    }

    // Skip a field whose value we do not care about, given its wire type.
    bool skip(uint32_t wire_type) {
        switch (wire_type) {
            case 0: { // varint
                uint64_t v;
                return read_varint(v);
            }
            case 1: // 64-bit
                if (p_ + 8 > end_)
                    return false;
                p_ += 8;
                return true;
            case 2: { // length-delimited
                const uint8_t* b;
                size_t l;
                return read_bytes(b, l);
            }
            case 5: // 32-bit
                if (p_ + 4 > end_)
                    return false;
                p_ += 4;
                return true;
            default:
                return false; // groups (3/4) are not used by this schema
        }
    }

  private:
    const uint8_t* p_;
    const uint8_t* end_;
};

// LTPC { ltp=1 double, ltt=2 int64, ltq=3 int64, cp=4 double }
PbLtpc decode_ltpc(const uint8_t* data, size_t len) {
    PbLtpc out;
    out.present = true;
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        if (fn == 1 && wt == 1) {
            r.read_double(out.ltp);
        } else if (fn == 2 && wt == 0) {
            uint64_t v;
            r.read_varint(v);
            out.ltt = static_cast<int64_t>(v);
        } else if (fn == 3 && wt == 0) {
            uint64_t v;
            r.read_varint(v);
            out.ltq = static_cast<int64_t>(v);
        } else if (fn == 4 && wt == 1) {
            r.read_double(out.cp);
        } else if (!r.skip(wt)) {
            break;
        }
    }
    return out;
}

// Quote { bidQ=1 int64, bidP=2 double, askQ=3 int64, askP=4 double }
PbQuote decode_quote(const uint8_t* data, size_t len) {
    PbQuote out;
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        if (fn == 1 && wt == 0) {
            uint64_t v;
            r.read_varint(v);
            out.bidQ = static_cast<int64_t>(v);
        } else if (fn == 2 && wt == 1) {
            r.read_double(out.bidP);
        } else if (fn == 3 && wt == 0) {
            uint64_t v;
            r.read_varint(v);
            out.askQ = static_cast<int64_t>(v);
        } else if (fn == 4 && wt == 1) {
            r.read_double(out.askP);
        } else if (!r.skip(wt)) {
            break;
        }
    }
    return out;
}

// OHLC { interval=1 string, open=2, high=3, low=4, close=5 double, vol=6 int64, ts=7 int64 }
PbOhlc decode_ohlc(const uint8_t* data, size_t len) {
    PbOhlc out;
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        if (fn == 1 && wt == 2) {
            const uint8_t* b;
            size_t l;
            if (r.read_bytes(b, l))
                out.interval = QString::fromUtf8(reinterpret_cast<const char*>(b), int(l));
        } else if (fn == 2 && wt == 1) {
            r.read_double(out.open);
        } else if (fn == 3 && wt == 1) {
            r.read_double(out.high);
        } else if (fn == 4 && wt == 1) {
            r.read_double(out.low);
        } else if (fn == 5 && wt == 1) {
            r.read_double(out.close);
        } else if (fn == 6 && wt == 0) {
            uint64_t v;
            r.read_varint(v);
            out.vol = static_cast<int64_t>(v);
        } else if (fn == 7 && wt == 0) {
            uint64_t v;
            r.read_varint(v);
            out.ts = static_cast<int64_t>(v);
        } else if (!r.skip(wt)) {
            break;
        }
    }
    return out;
}

// MarketLevel { bidAskQuote=1 repeated Quote }
void decode_market_level(const uint8_t* data, size_t len, PbFeed& feed) {
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        if (fn == 1 && wt == 2) {
            const uint8_t* b;
            size_t l;
            if (r.read_bytes(b, l))
                feed.bidAsk.push_back(decode_quote(b, l));
        } else if (!r.skip(wt)) {
            break;
        }
    }
}

// MarketOHLC { ohlc=1 repeated OHLC }
void decode_market_ohlc(const uint8_t* data, size_t len, PbFeed& feed) {
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        if (fn == 1 && wt == 2) {
            const uint8_t* b;
            size_t l;
            if (r.read_bytes(b, l))
                feed.ohlc.push_back(decode_ohlc(b, l));
        } else if (!r.skip(wt)) {
            break;
        }
    }
}

// MarketFullFeed {
//   ltpc=1, marketLevel=2, optionGreeks=3, marketOHLC=4,
//   atp=5 double, vtt=6 int64, oi=7 double, iv=8, tbq=9 double, tsq=10 double }
// IndexFullFeed { ltpc=1, marketOHLC=2 } — handled by the same reader because
// the overlapping field numbers (1=ltpc) decode identically and the
// non-overlapping ones (index marketOHLC=2 vs market marketLevel=2) are
// distinguished by their submessage content; for indices we only need ltpc+ohlc.
void decode_full_feed_inner(const uint8_t* data, size_t len, PbFeed& feed, bool is_index) {
    feed.hasFullFeed = true;
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        const uint8_t* b;
        size_t l;
        if (fn == 1 && wt == 2) { // ltpc (both market + index)
            if (r.read_bytes(b, l))
                feed.ltpc = decode_ltpc(b, l);
        } else if (!is_index && fn == 2 && wt == 2) { // MarketFullFeed.marketLevel
            if (r.read_bytes(b, l))
                decode_market_level(b, l, feed);
        } else if (is_index && fn == 2 && wt == 2) { // IndexFullFeed.marketOHLC
            if (r.read_bytes(b, l))
                decode_market_ohlc(b, l, feed);
        } else if (!is_index && fn == 4 && wt == 2) { // MarketFullFeed.marketOHLC
            if (r.read_bytes(b, l))
                decode_market_ohlc(b, l, feed);
        } else if (!is_index && fn == 5 && wt == 1) {
            r.read_double(feed.atp);
        } else if (!is_index && fn == 6 && wt == 0) {
            uint64_t v;
            r.read_varint(v);
            feed.vtt = static_cast<int64_t>(v);
        } else if (!is_index && fn == 7 && wt == 1) {
            r.read_double(feed.oi);
        } else if (!is_index && fn == 9 && wt == 1) {
            r.read_double(feed.tbq);
        } else if (!is_index && fn == 10 && wt == 1) {
            r.read_double(feed.tsq);
        } else if (!r.skip(wt)) {
            break;
        }
    }
}

// FullFeed { oneof { marketFF=1, indexFF=2 } }
void decode_full_feed(const uint8_t* data, size_t len, PbFeed& feed) {
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        const uint8_t* b;
        size_t l;
        if (fn == 1 && wt == 2) { // marketFF
            if (r.read_bytes(b, l))
                decode_full_feed_inner(b, l, feed, /*is_index=*/false);
        } else if (fn == 2 && wt == 2) { // indexFF
            if (r.read_bytes(b, l))
                decode_full_feed_inner(b, l, feed, /*is_index=*/true);
        } else if (!r.skip(wt)) {
            break;
        }
    }
}

// FirstLevelWithGreeks { ltpc=1, firstDepth=2 Quote, optionGreeks=3, vtt=4, oi=5, iv=6 }
void decode_first_level(const uint8_t* data, size_t len, PbFeed& feed) {
    feed.hasFullFeed = true;
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        const uint8_t* b;
        size_t l;
        if (fn == 1 && wt == 2) { // ltpc
            if (r.read_bytes(b, l))
                feed.ltpc = decode_ltpc(b, l);
        } else if (fn == 2 && wt == 2) { // firstDepth (single Quote → one ladder level)
            if (r.read_bytes(b, l))
                feed.bidAsk.push_back(decode_quote(b, l));
        } else if (fn == 4 && wt == 0) { // vtt
            uint64_t v;
            r.read_varint(v);
            feed.vtt = static_cast<int64_t>(v);
        } else if (fn == 5 && wt == 1) { // oi
            r.read_double(feed.oi);
        } else if (!r.skip(wt)) {
            break;
        }
    }
}

// Feed { oneof { ltpc=1, fullFeed=2, firstLevelWithGreeks=3 } requestMode=4 }
PbFeed decode_feed(const uint8_t* data, size_t len) {
    PbFeed feed;
    ProtoReader r(data, len);
    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        const uint8_t* b;
        size_t l;
        if (fn == 1 && wt == 2) { // ltpc (mode=ltpc)
            if (r.read_bytes(b, l))
                feed.ltpc = decode_ltpc(b, l);
        } else if (fn == 2 && wt == 2) { // fullFeed (mode=full)
            if (r.read_bytes(b, l))
                decode_full_feed(b, l, feed);
        } else if (fn == 3 && wt == 2) { // firstLevelWithGreeks (mode=option_greeks)
            if (r.read_bytes(b, l))
                decode_first_level(b, l, feed);
        } else if (!r.skip(wt)) {
            break;
        }
    }
    return feed;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

UpstoxWebSocket::UpstoxWebSocket(const QString& access_token, QObject* parent)
    : BrokerWebSocketBase(parent), access_token_(access_token) {
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &UpstoxWebSocket::on_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &UpstoxWebSocket::on_disconnected);
    connect(ws_, &WebSocketClient::binary_message_received, this, &UpstoxWebSocket::on_binary_message);
    connect(ws_, &WebSocketClient::message_received, this, &UpstoxWebSocket::on_text_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &UpstoxWebSocket::error_occurred);
}

UpstoxWebSocket::~UpstoxWebSocket() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void UpstoxWebSocket::open() {
    if (connecting_) {
        LOG_DEBUG(TAG_UPSTOX_WS, "open() ignored — authorize already in flight");
        return;
    }
    connecting_ = true;
    LOG_INFO(TAG_UPSTOX_WS, "Authorizing market-data feed");

    // The authorize REST call (BrokerHttp) is synchronous, so run it on a worker
    // thread and connect on the UI thread (P1 / P8: never block UI, QPointer guard).
    QPointer<UpstoxWebSocket> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self)
            return;
        QString url = self->fetch_ws_url();
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, url]() {
                if (!self)
                    return;
                self->connecting_ = false;
                if (url.isEmpty()) {
                    LOG_ERROR(TAG_UPSTOX_WS, "Failed to obtain WS URL from authorize endpoint");
                    emit self->error_occurred(QStringLiteral("Upstox feed authorize failed"));
                    return;
                }
                self->ws_url_ = url;
                LOG_INFO(TAG_UPSTOX_WS, "Connecting to authorized feed URI");
                self->ws_->connect_to(url);
                self->start_health_check();
            },
            Qt::QueuedConnection);
    });
}

void UpstoxWebSocket::close() {
    stop_health_check();
    ws_->disconnect();
}

bool UpstoxWebSocket::is_connected() const {
    return ws_->is_connected();
}

void UpstoxWebSocket::subscribe(const QStringList& instrument_keys, const QString& mode) {
    QStringList fresh;
    for (const QString& key : instrument_keys) {
        if (key.isEmpty())
            continue;
        if (!subscriptions_.contains(key)) {
            subscriptions_.insert(key, mode);
            fresh.append(key);
        }
    }
    if (!fresh.isEmpty() && is_connected())
        send_subscription(fresh, QStringLiteral("sub"), mode);
}

void UpstoxWebSocket::subscribe(const QVector<qint64>& tokens) {
    // Resolve each numeric token to an Upstox instrument key ("SEGMENT|TOKEN").
    auto& svc = InstrumentService::instance();
    QStringList keys;
    for (qint64 token : tokens) {
        auto inst = svc.find_by_token(static_cast<quint32>(token), "upstox");
        if (inst.has_value()) {
            // Build SEGMENT|TOKEN. brexchange is the Upstox segment (NSE_EQ, …);
            // fall back to NSE_EQ when missing.
            QString seg = inst->brexchange.isEmpty() ? QStringLiteral("NSE_EQ") : inst->brexchange;
            keys.append(seg + "|" + QString::number(token));
        } else {
            LOG_WARN(TAG_UPSTOX_WS,
                     QString("No instrument for token %1 — skipping subscribe").arg(token));
        }
    }
    if (!keys.isEmpty())
        subscribe(keys, QStringLiteral("full"));
}

void UpstoxWebSocket::unsubscribe() {
    if (subscriptions_.isEmpty())
        return;
    if (is_connected())
        send_subscription(subscriptions_.keys(), QStringLiteral("unsub"), QString());
    subscriptions_.clear();
    clear_tick_cache();
}

void UpstoxWebSocket::unsubscribe(const QStringList& instrument_keys) {
    QStringList removed;
    for (const QString& key : instrument_keys) {
        if (subscriptions_.remove(key))
            removed.append(key);
    }
    if (!removed.isEmpty() && is_connected())
        send_subscription(removed, QStringLiteral("unsub"), QString());
}

// ─────────────────────────────────────────────────────────────────────────────
// Base-class hooks
// ─────────────────────────────────────────────────────────────────────────────

void UpstoxWebSocket::on_data_stall() {
    LOG_WARN(TAG_UPSTOX_WS, "Data stall — forcing reconnect");
    ws_->disconnect();
    open(); // re-authorize: the WS URL is single-use / time-limited
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void UpstoxWebSocket::on_connected() {
    LOG_INFO(TAG_UPSTOX_WS, "Connected — replaying subscriptions");
    resubscribe_all();
    emit connected();
}

void UpstoxWebSocket::on_disconnected() {
    LOG_WARN(TAG_UPSTOX_WS, "Disconnected");
    emit disconnected();
}

void UpstoxWebSocket::on_binary_message(const QByteArray& data) {
    parse_feed_response(data);
}

void UpstoxWebSocket::on_text_message(const QString& msg) {
    // Upstox normally streams binary protobuf. The only text frames are control
    // / error envelopes: {"status":"failed","error":"...","method":"sub"}.
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    QJsonObject obj = doc.object();
    if (obj.value("status").toString() == QLatin1String("failed")) {
        const QString method = obj.value("method").toString();
        const QString error = obj.value("error").toString();
        LOG_ERROR(TAG_UPSTOX_WS, QString("%1 failed: %2").arg(method, error));
        emit error_occurred(QString("Upstox %1 failed: %2").arg(method, error));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// REST authorize
// ─────────────────────────────────────────────────────────────────────────────

QString UpstoxWebSocket::fetch_ws_url() const {
    QMap<QString, QString> headers;
    headers["Accept"] = "application/json";
    headers["Authorization"] = "Bearer " + access_token_;

    BrokerHttpResponse resp = BrokerHttp::instance().get(kAuthEndpoint, headers);
    if (!resp.success) {
        LOG_ERROR(TAG_UPSTOX_WS,
                  QString("Authorize HTTP %1: %2").arg(resp.status_code).arg(resp.error));
        return QString();
    }
    // { "status":"success", "data": { "authorized_redirect_uri": "wss://..." } }
    const QString uri =
        resp.json.value("data").toObject().value("authorized_redirect_uri").toString();
    if (uri.isEmpty())
        LOG_ERROR(TAG_UPSTOX_WS, "authorize response missing authorized_redirect_uri");
    return uri;
}

// ─────────────────────────────────────────────────────────────────────────────
// Wire senders
// ─────────────────────────────────────────────────────────────────────────────

void UpstoxWebSocket::send_subscription(const QStringList& instrument_keys, const QString& method,
                                        const QString& mode) {
    if (instrument_keys.isEmpty())
        return;

    const int total = static_cast<int>(instrument_keys.size());
    for (int start = 0; start < total; start += kSubscribeBatch) {
        QJsonArray keys;
        const int stop = std::min(start + kSubscribeBatch, total);
        for (int i = start; i < stop; ++i)
            keys.append(instrument_keys[i]);

        QJsonObject data;
        data["instrumentKeys"] = keys;
        if (method == QLatin1String("sub") && !mode.isEmpty())
            data["mode"] = mode;

        QJsonObject envelope;
        // guid: first 20 hex chars of a UUID (matches the python client).
        envelope["guid"] = QUuid::createUuid().toString(QUuid::Id128).left(20);
        envelope["method"] = method;
        envelope["data"] = data;

        // Upstox expects the JSON sent as a BINARY frame.
        ws_->send_binary(QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    }
    LOG_INFO(TAG_UPSTOX_WS, QString("%1 %2 instrument(s)%3")
                                .arg(method == QLatin1String("sub") ? "Subscribed" : "Unsubscribed")
                                .arg(instrument_keys.size())
                                .arg(mode.isEmpty() ? QString() : QString(" (%1)").arg(mode)));
}

void UpstoxWebSocket::resubscribe_all() {
    if (subscriptions_.isEmpty())
        return;
    // Group instrument keys by requested mode so each mode collapses to one
    // (or a few, batched) subscribe frame.
    QMap<QString, QStringList> by_mode;
    for (auto it = subscriptions_.cbegin(); it != subscriptions_.cend(); ++it)
        by_mode[it.value()].append(it.key());
    for (auto it = by_mode.cbegin(); it != by_mode.cend(); ++it)
        send_subscription(it.value(), QStringLiteral("sub"), it.key());
}

// ─────────────────────────────────────────────────────────────────────────────
// Protobuf → tick / depth
// ─────────────────────────────────────────────────────────────────────────────

void UpstoxWebSocket::parse_feed_response(const QByteArray& data) {
    // FeedResponse { type=1 enum, feeds=2 map<string,Feed>, currentTs=3 int64,
    //                marketInfo=4 }. The feeds map is encoded as a repeated
    //   length-delimited entry message { key=1 string, value=2 Feed }.
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(data.constData());
    ProtoReader r(buf, static_cast<size_t>(data.size()));

    int64_t current_ts = 0;
    int feed_count = 0;

    uint32_t fn, wt;
    while (r.next_tag(fn, wt)) {
        const uint8_t* b;
        size_t l;
        if (fn == 2 && wt == 2) { // feeds map entry
            if (!r.read_bytes(b, l))
                break;
            // Decode one map entry: field 1 = key (string), field 2 = Feed.
            ProtoReader entry(b, l);
            QString instrument_key;
            PbFeed feed;
            bool have_feed = false;
            uint32_t efn, ewt;
            while (entry.next_tag(efn, ewt)) {
                const uint8_t* eb;
                size_t el;
                if (efn == 1 && ewt == 2) {
                    if (entry.read_bytes(eb, el))
                        instrument_key = QString::fromUtf8(reinterpret_cast<const char*>(eb), int(el));
                } else if (efn == 2 && ewt == 2) {
                    if (entry.read_bytes(eb, el)) {
                        feed = decode_feed(eb, el);
                        have_feed = true;
                    }
                } else if (!entry.skip(ewt)) {
                    break;
                }
            }
            if (have_feed && !instrument_key.isEmpty()) {
                ++feed_count;
                // current_ts may not have been parsed yet (field order in the
                // wire is type → feeds → currentTs), so we defer the emit until
                // after the full pass. To keep things simple we emit inline and
                // fall back to "now" when currentTs is still 0; the merge cache
                // preserves a sensible timestamp regardless.
                QString symbol, exchange;
                enrich_symbol(instrument_key, symbol, exchange);

                note_tick();

                BrokerQuote partial;
                partial.symbol = symbol;
                partial.ltp = feed.ltpc.ltp;
                partial.close = feed.ltpc.cp;
                partial.oi = static_cast<qint64>(feed.oi);
                partial.volume = static_cast<double>(feed.vtt);
                partial.timestamp = feed.ltpc.ltt != 0 ? feed.ltpc.ltt : current_ts;

                if (feed.hasFullFeed) {
                    // Prefer the 1d OHLC bucket; fall back to the first bucket.
                    const PbOhlc* chosen = nullptr;
                    for (const auto& o : feed.ohlc) {
                        if (o.interval == QLatin1String("1d")) {
                            chosen = &o;
                            break;
                        }
                    }
                    if (!chosen && !feed.ohlc.empty())
                        chosen = &feed.ohlc.front();
                    if (chosen) {
                        partial.open = chosen->open;
                        partial.high = chosen->high;
                        partial.low = chosen->low;
                        if (partial.close == 0)
                            partial.close = chosen->close;
                        if (partial.volume == 0)
                            partial.volume = static_cast<double>(chosen->vol);
                        if (partial.timestamp == 0)
                            partial.timestamp = chosen->ts;
                    }
                    // Top-of-book bid/ask from the first ladder level.
                    if (!feed.bidAsk.empty()) {
                        const PbQuote& top = feed.bidAsk.front();
                        partial.bid = top.bidP;
                        partial.ask = top.askP;
                        partial.bid_size = static_cast<double>(top.bidQ);
                        partial.ask_size = static_cast<double>(top.askQ);
                    }
                }

                BrokerQuote merged = merge_tick(instrument_key, partial);
                if (merged.timestamp == 0)
                    merged.timestamp = QDateTime::currentMSecsSinceEpoch();
                emit tick_received(merged);

                // Emit depth when a ladder is present (full mode).
                if (!feed.bidAsk.empty()) {
                    MarketDepth depth;
                    depth.symbol = symbol;
                    depth.exchange = exchange;
                    depth.ltp = merged.ltp;
                    depth.volume = merged.volume;
                    depth.oi = static_cast<double>(merged.oi);
                    for (const PbQuote& q : feed.bidAsk) {
                        if (q.bidP > 0) {
                            DepthLevel lvl;
                            lvl.price = q.bidP;
                            lvl.quantity = static_cast<int>(q.bidQ);
                            lvl.orders = 0; // Upstox V3 ladder does not carry order count
                            depth.bids.append(lvl);
                        }
                        if (q.askP > 0) {
                            DepthLevel lvl;
                            lvl.price = q.askP;
                            lvl.quantity = static_cast<int>(q.askQ);
                            lvl.orders = 0;
                            depth.asks.append(lvl);
                        }
                    }
                    emit depth_received(depth);
                }
            }
        } else if (fn == 3 && wt == 0) { // currentTs
            uint64_t v;
            if (!r.read_varint(v))
                break;
            current_ts = static_cast<int64_t>(v);
        } else if (!r.skip(wt)) {
            break;
        }
    }

    if (feed_count == 0)
        LOG_DEBUG(TAG_UPSTOX_WS, "FeedResponse with no decodable feeds (control/market-info frame)");
}

void UpstoxWebSocket::enrich_symbol(const QString& instrument_key, QString& symbol,
                                    QString& exchange) const {
    // instrument_key = "SEGMENT|TOKEN" (e.g. "NSE_EQ|256265" or "NSE_EQ|INE002A01018").
    symbol = instrument_key;
    exchange.clear();

    const int bar = instrument_key.indexOf('|');
    if (bar < 0)
        return;
    const QString seg = instrument_key.left(bar);
    const QString token_part = instrument_key.mid(bar + 1);

    // Canonical exchange from the Upstox segment.
    static const QMap<QString, QString> seg_map = {
        {"NSE_EQ", "NSE"},       {"BSE_EQ", "BSE"}, {"NSE_FO", "NFO"},
        {"BSE_FO", "BFO"},       {"NSE_CD", "CDS"}, {"MCX_FO", "MCX"},
        {"NSE_INDEX", "NSE_INDEX"}, {"BSE_INDEX", "BSE_INDEX"},
    };
    exchange = seg_map.value(seg, seg);

    // Try to resolve the display symbol via InstrumentService when the token
    // part is numeric (an instrument_token).
    bool numeric = false;
    const qlonglong token = token_part.toLongLong(&numeric);
    if (numeric) {
        auto inst = InstrumentService::instance().find_by_token(static_cast<quint32>(token), "upstox");
        if (inst.has_value()) {
            symbol = inst->symbol;
            if (!inst->exchange.isEmpty())
                exchange = inst->exchange;
            return;
        }
    }
    // Non-numeric token part is usually the symbol/ISIN already.
    symbol = token_part;
}

} // namespace fincept::trading
