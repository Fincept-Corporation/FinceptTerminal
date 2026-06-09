// IIFLWebSocket — IIFL XTS Socket.IO market-data adapter.
//
// See the header for the protocol summary. OpenAlgo references:
//   broker/iifl/streaming/iifl_websocket.py
//   broker/iifl/streaming/iifl_adapter.py
//   broker/iifl/streaming/iifl_mapping.py  (exchange segment codes)

#include "trading/websocket/IIFLWebSocket.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"
#include "trading/instruments/InstrumentService.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace fincept::trading {

static constexpr const char* TAG_IIFL_WS = "IIFLWS";

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

IIFLWebSocket::IIFLWebSocket(const QString& market_token, const QString& user_id, QObject* parent)
    : BrokerWebSocketBase(parent), market_token_(market_token), user_id_(user_id) {
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &IIFLWebSocket::on_ws_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &IIFLWebSocket::on_ws_disconnected);
    connect(ws_, &WebSocketClient::message_received, this, &IIFLWebSocket::on_ws_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &IIFLWebSocket::error_occurred);

    // Dedicated NAM so each subscription request can carry its own
    // Authorization header without mutating the shared HttpClient singleton.
    nam_ = new QNetworkAccessManager(this);
}

IIFLWebSocket::~IIFLWebSocket() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void IIFLWebSocket::open() {
    sio_connected_ = false;
    // Engine.IO v4 over WebSocket. token + userID authenticate the feed; XTS
    // publishes JSON in FULL broadcast mode.
    const QString url =
        QString("wss://ttblaze.iifl.com/apimarketdata/socket.io/"
                "?EIO=4&transport=websocket&token=%1&userID=%2&publishFormat=JSON&broadcastMode=Full")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(market_token_)),
                 QString::fromUtf8(QUrl::toPercentEncoding(user_id_)));
    LOG_INFO(TAG_IIFL_WS, "Connecting to IIFL XTS Socket.IO");
    ws_->connect_to(url);
    start_health_check();
}

void IIFLWebSocket::close() {
    stop_health_check();
    sio_connected_ = false;
    // Polite Socket.IO disconnect ("41"), then drop the transport.
    if (ws_->is_connected())
        ws_->send(QStringLiteral("41"));
    ws_->disconnect();
}

bool IIFLWebSocket::is_connected() const {
    return ws_->is_connected() && sio_connected_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Subscription
// ─────────────────────────────────────────────────────────────────────────────

void IIFLWebSocket::subscribe(const QVector<qint64>& tokens) {
    QVector<qint64> new_tokens;
    for (qint64 t : tokens) {
        if (!subscribed_tokens_.contains(t)) {
            subscribed_tokens_.insert(t);
            new_tokens.append(t);
        }
    }
    if (new_tokens.isEmpty())
        return;
    // XTS subscription is an HTTP call; only valid once the SIO channel is up so
    // the resulting stream has somewhere to land.
    if (sio_connected_)
        http_subscription(new_tokens, /*subscribe=*/true);
}

void IIFLWebSocket::unsubscribe() {
    if (subscribed_tokens_.isEmpty())
        return;
    QVector<qint64> all(subscribed_tokens_.begin(), subscribed_tokens_.end());
    if (sio_connected_)
        http_subscription(all, /*subscribe=*/false);
    subscribed_tokens_.clear();
    clear_tick_cache();
}

void IIFLWebSocket::unsubscribe(const QVector<qint64>& tokens) {
    QVector<qint64> to_remove;
    for (qint64 t : tokens) {
        if (subscribed_tokens_.remove(t))
            to_remove.append(t);
    }
    if (!to_remove.isEmpty() && sio_connected_)
        http_subscription(to_remove, /*subscribe=*/false);
}

void IIFLWebSocket::on_data_stall() {
    LOG_WARN(TAG_IIFL_WS, "Data stall — forcing reconnect");
    sio_connected_ = false;
    ws_->disconnect();
    open();
}

void IIFLWebSocket::http_subscription(const QVector<qint64>& tokens, bool subscribe) {
    if (tokens.isEmpty())
        return;

    QJsonArray instruments;
    for (qint64 t : tokens) {
        QJsonObject inst;
        inst["exchangeSegment"] = segment_for_token(t);
        // XTS expects the instrument id as a string.
        inst["exchangeInstrumentID"] = QString::number(t);
        instruments.append(inst);
    }

    // Subscribe both Quote (1501) and Depth (1502) so consumers get quotes + book.
    for (int code : {kCodeQuote, kCodeDepth}) {
        QJsonObject body;
        body["instruments"] = instruments;
        body["xtsMessageCode"] = code;

        QNetworkRequest req{QUrl(QString::fromUtf8(kSubscriptionUrl))};
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", market_token_.toUtf8());

        const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QNetworkReply* reply = subscribe ? nam_->post(req, payload)
                                         : nam_->sendCustomRequest(req, "PUT", payload);

        // `this` as context: Qt auto-disconnects on destruction (safe — see P8).
        connect(reply, &QNetworkReply::finished, this, [reply, subscribe, code]() {
            const int status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError || status != 200) {
                LOG_WARN(TAG_IIFL_WS,
                         QString("%1 (code %2) failed: HTTP %3 %4")
                             .arg(subscribe ? "Subscribe" : "Unsubscribe")
                             .arg(code)
                             .arg(status)
                             .arg(reply->errorString()));
            } else {
                LOG_INFO(TAG_IIFL_WS, QString("%1 ok (xtsCode %2)")
                                          .arg(subscribe ? "Subscribed" : "Unsubscribed")
                                          .arg(code));
            }
            reply->deleteLater();
        });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Engine.IO / Socket.IO framing
// ─────────────────────────────────────────────────────────────────────────────

void IIFLWebSocket::on_ws_connected() {
    // Transport is up; the Engine.IO handshake ("0{...}") drives the rest. We
    // emit BrokerWebSocketBase::connected() only after the SIO namespace
    // connect ("40") is acknowledged.
    LOG_INFO(TAG_IIFL_WS, "WebSocket transport open — awaiting Engine.IO handshake");
}

void IIFLWebSocket::on_ws_disconnected() {
    LOG_WARN(TAG_IIFL_WS, "Disconnected");
    sio_connected_ = false;
    emit disconnected();
}

void IIFLWebSocket::on_ws_message(const QString& message) {
    if (message.isEmpty())
        return;
    handle_engineio(message);
}

void IIFLWebSocket::handle_engineio(const QString& packet) {
    // Engine.IO packet type is the first character.
    const QChar type = packet.at(0);
    const QString rest = packet.mid(1);

    switch (type.toLatin1()) {
        case '0': // open — server announced sid/ping config; join the namespace.
            LOG_DEBUG(TAG_IIFL_WS, "Engine.IO open — sending Socket.IO connect (40)");
            ws_->send(QStringLiteral("40"));
            break;
        case '2': // ping → reply pong.
            ws_->send(QStringLiteral("3"));
            break;
        case '3': // pong (we don't initiate pings) — ignore.
            break;
        case '4': { // Socket.IO message; second char is the SIO packet type.
            if (rest.isEmpty())
                break;
            const QChar sio_type = rest.at(0);
            const QString sio_rest = rest.mid(1);
            switch (sio_type.toLatin1()) {
                case '0': // CONNECT ack — namespace joined.
                    LOG_INFO(TAG_IIFL_WS, "Socket.IO namespace connected");
                    sio_connected_ = true;
                    emit connected();
                    // (Re)subscribe everything we already wanted.
                    if (!subscribed_tokens_.isEmpty()) {
                        QVector<qint64> all(subscribed_tokens_.begin(), subscribed_tokens_.end());
                        http_subscription(all, /*subscribe=*/true);
                    }
                    break;
                case '1': // DISCONNECT
                    LOG_WARN(TAG_IIFL_WS, "Socket.IO namespace disconnected");
                    sio_connected_ = false;
                    break;
                case '2': // EVENT — payload is a JSON array ["event", data].
                    handle_socketio_event(sio_rest);
                    break;
                case '4': // CONNECT_ERROR
                    LOG_ERROR(TAG_IIFL_WS, QString("Socket.IO connect error: %1").arg(sio_rest));
                    emit error_occurred(QStringLiteral("IIFL Socket.IO connect error"));
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

void IIFLWebSocket::handle_socketio_event(const QString& payload) {
    // payload is the JSON array immediately following "42", optionally prefixed
    // by an ack id (digits) which we don't use.
    int start = 0;
    while (start < payload.size() && payload.at(start).isDigit())
        ++start;
    const QString json_str = payload.mid(start);

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        LOG_DEBUG(TAG_IIFL_WS, QString("Unparseable SIO event: %1").arg(json_str.left(120)));
        return;
    }

    const QJsonArray arr = doc.array();
    if (arr.isEmpty())
        return;

    const QString event = arr.at(0).toString();
    const QJsonValue data = arr.size() > 1 ? arr.at(1) : QJsonValue();

    // XTS market-data events are named "<code>-json-full" / "<code>-json-partial".
    if (event.contains(QStringLiteral("-json-")))
        handle_market_data(event, data);
}

// ─────────────────────────────────────────────────────────────────────────────
// Market-data parsing
// ─────────────────────────────────────────────────────────────────────────────

void IIFLWebSocket::handle_market_data(const QString& event, const QJsonValue& data) {
    // The data value may be a JSON object or a JSON string holding an object.
    QJsonObject obj;
    if (data.isObject()) {
        obj = data.toObject();
    } else if (data.isString()) {
        QJsonParseError err;
        QJsonDocument d = QJsonDocument::fromJson(data.toString().toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !d.isObject())
            return;
        obj = d.object();
    } else {
        return;
    }

    note_tick();

    const int segment = obj.value("ExchangeSegment").toInt();
    const qint64 instrument_id = qint64(obj.value("ExchangeInstrumentID").toDouble());
    const int message_code =
        obj.contains("MessageCode")
            ? obj.value("MessageCode").toInt()
            : event.section('-', 0, 0).toInt(); // fall back to the event prefix

    const QString key = QString::number(instrument_id);

    // Enrich with the normalised symbol; fall back to the numeric id.
    QString symbol = key;
    QString exchange = segment_to_exchange(segment);
    auto inst = InstrumentService::instance().find_by_token(static_cast<quint32>(instrument_id), "iifl");
    if (inst.has_value()) {
        symbol = inst->symbol;
        if (!inst->exchange.isEmpty())
            exchange = inst->exchange;
    }

    // For Depth (1502) the quote fields live under "Touchline"; otherwise root.
    QJsonObject tl =
        (message_code == kCodeDepth && obj.value("Touchline").isObject()) ? obj.value("Touchline").toObject() : obj;

    BrokerQuote partial;
    partial.symbol = symbol;
    partial.ltp = tl.value("LastTradedPrice").toDouble();
    partial.open = tl.value("Open").toDouble();
    partial.high = tl.value("High").toDouble();
    partial.low = tl.value("Low").toDouble();
    partial.close = tl.value("Close").toDouble();
    partial.volume = tl.value("TotalTradedQuantity").toDouble();
    partial.oi = qint64(obj.value("OpenInterest").toDouble());
    partial.timestamp = QDateTime::currentMSecsSinceEpoch();

    // XTS partial frames omit unchanged fields → preserve via merge_tick.
    BrokerQuote merged = merge_tick(key, partial);
    emit tick_received(merged);

    // Depth: "Bids"/"Asks" arrays with Price/Size/TotalOrders entries.
    if (message_code == kCodeDepth && obj.value("Bids").isArray() && obj.value("Asks").isArray()) {
        MarketDepth depth;
        depth.symbol = symbol;
        depth.exchange = exchange;
        depth.ltp = merged.ltp;
        depth.volume = merged.volume;
        depth.oi = double(merged.oi);

        const QJsonArray bids = obj.value("Bids").toArray();
        const QJsonArray asks = obj.value("Asks").toArray();
        for (int i = 0; i < bids.size() && depth.bids.size() < 5; ++i) {
            const QJsonObject b = bids.at(i).toObject();
            DepthLevel lvl;
            lvl.price = b.value("Price").toDouble();
            lvl.quantity = b.value("Size").toInt();
            lvl.orders = b.value("TotalOrders").toInt();
            depth.bids.append(lvl);
        }
        for (int i = 0; i < asks.size() && depth.asks.size() < 5; ++i) {
            const QJsonObject a = asks.at(i).toObject();
            DepthLevel lvl;
            lvl.price = a.value("Price").toDouble();
            lvl.quantity = a.value("Size").toInt();
            lvl.orders = a.value("TotalOrders").toInt();
            depth.asks.append(lvl);
        }
        emit depth_received(depth);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Exchange segment mapping (IiflExchangeMapper)
// ─────────────────────────────────────────────────────────────────────────────

int IIFLWebSocket::xts_segment(const QString& exchange) {
    if (exchange == "NFO" || exchange == "NSEFO")
        return 2;
    if (exchange == "BSE" || exchange == "BSECM" || exchange == "BSE_INDEX")
        return 11;
    if (exchange == "BFO" || exchange == "BSEFO")
        return 12;
    if (exchange == "MCX" || exchange == "MCXFO")
        return 51;
    if (exchange == "CDS" || exchange == "NSECD")
        return 3;
    return 1; // NSE / NSECM / NSE_INDEX
}

QString IIFLWebSocket::segment_to_exchange(int segment) {
    switch (segment) {
        case 2:
            return QStringLiteral("NFO");
        case 3:
            return QStringLiteral("CDS");
        case 11:
            return QStringLiteral("BSE");
        case 12:
            return QStringLiteral("BFO");
        case 51:
            return QStringLiteral("MCX");
        default:
            return QStringLiteral("NSE");
    }
}

int IIFLWebSocket::segment_for_token(qint64 token) const {
    QString exchange = default_exchange_;
    auto inst = InstrumentService::instance().find_by_token(static_cast<quint32>(token), "iifl");
    if (inst.has_value()) {
        const QString& ex = inst->brexchange.isEmpty() ? inst->exchange : inst->brexchange;
        if (!ex.isEmpty())
            exchange = ex;
    }
    return xts_segment(exchange);
}

} // namespace fincept::trading
