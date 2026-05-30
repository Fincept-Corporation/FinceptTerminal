// IciciDirectWebSocket — ICICI Direct (Breeze) Socket.IO market-data adapter.
//
// Breeze SDK references (python-socketio): the live feed connects to
//   wss://livestream.icicidirect.com/socket.io/?EIO=4&transport=websocket
// with auth={"user":user_id,"token":session_key} sent in the Socket.IO CONNECT
// packet, subscribes via emit('join', "<exch>.1!<token>"), and receives ticks on
// the 'stock' event as a positional list (parse_data field order replicated below).

#include "trading/websocket/IciciDirectWebSocket.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"
#include "trading/instruments/InstrumentService.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace fincept::trading {

static constexpr const char* TAG_ICICI_WS = "IciciWS";

namespace {

// Breeze stock-token exchange prefix. The same number appears in inbound ticks'
// data[0] ("<exch>.<type>!<token>").
int breeze_exch_code(const QString& exchange) {
    const QString ex = exchange.toUpper();
    if (ex == "BSE")
        return 1;
    if (ex == "BFO")
        return 8;
    if (ex == "MCX")
        return 13;
    if (ex == "NDX")
        return 6;
    // NSE cash + NFO derivatives share segment 4.
    return 4;
}

// Breeze tick list values arrive as JSON numbers OR numeric strings.
double jval(const QJsonValue& v) {
    if (v.isDouble())
        return v.toDouble();
    return v.toString().toDouble();
}

} // namespace

IciciDirectWebSocket::IciciDirectWebSocket(const QString& session_token_b64, const QString& user_id, QObject* parent)
    : BrokerWebSocketBase(parent), user_(user_id) {
    // The stored access_token is base64("user_id:session_key"); recover the raw
    // session_key (and user id, if not supplied) for the Socket.IO auth payload.
    const QString decoded = QString::fromUtf8(QByteArray::fromBase64(session_token_b64.toUtf8()));
    if (decoded.contains(':')) {
        if (user_.isEmpty())
            user_ = decoded.section(':', 0, 0);
        session_key_ = decoded.section(':', 1);
    } else {
        session_key_ = session_token_b64; // fallback: already a raw key
    }

    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &IciciDirectWebSocket::on_ws_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &IciciDirectWebSocket::on_ws_disconnected);
    connect(ws_, &WebSocketClient::message_received, this, &IciciDirectWebSocket::on_ws_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &IciciDirectWebSocket::error_occurred);
}

IciciDirectWebSocket::~IciciDirectWebSocket() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void IciciDirectWebSocket::open() {
    sio_connected_ = false;
    // Engine.IO v4 over WebSocket; auth is sent later in the SIO CONNECT packet.
    const QString url =
        QStringLiteral("wss://livestream.icicidirect.com/socket.io/?EIO=4&transport=websocket");
    LOG_INFO(TAG_ICICI_WS, "Connecting to ICICI Breeze Socket.IO");
    ws_->connect_to(url);
    start_health_check();
}

void IciciDirectWebSocket::close() {
    stop_health_check();
    sio_connected_ = false;
    if (ws_->is_connected()) {
        if (!subscribed_tokens_.isEmpty())
            send_leave(QVector<qint64>(subscribed_tokens_.begin(), subscribed_tokens_.end()));
        ws_->send(QStringLiteral("41")); // Socket.IO disconnect
    }
    ws_->disconnect();
}

bool IciciDirectWebSocket::is_connected() const {
    return ws_->is_connected() && sio_connected_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Subscription
// ─────────────────────────────────────────────────────────────────────────────

void IciciDirectWebSocket::subscribe(const QVector<qint64>& tokens) {
    QVector<qint64> new_tokens;
    for (qint64 t : tokens) {
        if (!subscribed_tokens_.contains(t)) {
            subscribed_tokens_.insert(t);
            new_tokens.append(t);
        }
    }
    if (!new_tokens.isEmpty() && sio_connected_)
        send_join(new_tokens);
}

void IciciDirectWebSocket::unsubscribe() {
    if (subscribed_tokens_.isEmpty())
        return;
    if (sio_connected_)
        send_leave(QVector<qint64>(subscribed_tokens_.begin(), subscribed_tokens_.end()));
    subscribed_tokens_.clear();
    clear_tick_cache();
}

void IciciDirectWebSocket::unsubscribe(const QVector<qint64>& tokens) {
    QVector<qint64> to_remove;
    for (qint64 t : tokens) {
        if (subscribed_tokens_.remove(t))
            to_remove.append(t);
    }
    if (!to_remove.isEmpty() && sio_connected_)
        send_leave(to_remove);
}

void IciciDirectWebSocket::on_data_stall() {
    LOG_WARN(TAG_ICICI_WS, "Data stall — forcing reconnect");
    sio_connected_ = false;
    ws_->disconnect();
    open();
}

QString IciciDirectWebSocket::stock_token_for(qint64 token) const {
    QString exchange = QStringLiteral("NSE");
    auto inst = InstrumentService::instance().find_by_token(static_cast<quint32>(token), "icicidirect");
    if (inst.has_value()) {
        const QString& ex = inst->brexchange.isEmpty() ? inst->exchange : inst->brexchange;
        if (!ex.isEmpty())
            exchange = ex;
    }
    // ".1" selects the quotes feed (".2" would be market depth).
    return QStringLiteral("%1.1!%2").arg(breeze_exch_code(exchange)).arg(token);
}

void IciciDirectWebSocket::send_join(const QVector<qint64>& tokens) {
    if (tokens.isEmpty())
        return;
    QJsonArray toks;
    for (qint64 t : tokens)
        toks.append(stock_token_for(t));
    const QJsonArray event{QStringLiteral("join"), toks};
    ws_->send(QStringLiteral("42") + QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)));
    LOG_INFO(TAG_ICICI_WS, QString("Subscribed %1 tokens").arg(tokens.size()));
}

void IciciDirectWebSocket::send_leave(const QVector<qint64>& tokens) {
    if (tokens.isEmpty())
        return;
    QJsonArray toks;
    for (qint64 t : tokens)
        toks.append(stock_token_for(t));
    const QJsonArray event{QStringLiteral("leave"), toks};
    ws_->send(QStringLiteral("42") + QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)));
}

// ─────────────────────────────────────────────────────────────────────────────
// Engine.IO / Socket.IO framing
// ─────────────────────────────────────────────────────────────────────────────

void IciciDirectWebSocket::on_ws_connected() {
    LOG_INFO(TAG_ICICI_WS, "WebSocket transport open — awaiting Engine.IO handshake");
}

void IciciDirectWebSocket::on_ws_disconnected() {
    LOG_WARN(TAG_ICICI_WS, "Disconnected");
    sio_connected_ = false;
    emit disconnected();
}

void IciciDirectWebSocket::on_ws_message(const QString& message) {
    if (!message.isEmpty())
        handle_engineio(message);
}

void IciciDirectWebSocket::handle_engineio(const QString& packet) {
    const QChar type = packet.at(0);
    const QString rest = packet.mid(1);

    switch (type.toLatin1()) {
        case '0': { // Engine.IO open → Socket.IO CONNECT with auth payload.
            QJsonObject auth{{"user", user_}, {"token", session_key_}};
            ws_->send(QStringLiteral("40") +
                      QString::fromUtf8(QJsonDocument(auth).toJson(QJsonDocument::Compact)));
            break;
        }
        case '2': // ping → pong
            ws_->send(QStringLiteral("3"));
            break;
        case '3': // pong — ignore
            break;
        case '4': { // Socket.IO message; second char is the SIO packet type.
            if (rest.isEmpty())
                break;
            const QChar sio_type = rest.at(0);
            const QString sio_rest = rest.mid(1);
            switch (sio_type.toLatin1()) {
                case '0': // CONNECT ack — namespace joined.
                    LOG_INFO(TAG_ICICI_WS, "Socket.IO connected");
                    sio_connected_ = true;
                    emit connected();
                    if (!subscribed_tokens_.isEmpty())
                        send_join(QVector<qint64>(subscribed_tokens_.begin(), subscribed_tokens_.end()));
                    break;
                case '1': // DISCONNECT
                    LOG_WARN(TAG_ICICI_WS, "Socket.IO disconnected");
                    sio_connected_ = false;
                    break;
                case '2': // EVENT — ["event", data]
                    handle_socketio_event(sio_rest);
                    break;
                case '4': // CONNECT_ERROR
                    LOG_ERROR(TAG_ICICI_WS, QString("Socket.IO connect error: %1").arg(sio_rest.left(200)));
                    emit error_occurred(QStringLiteral("ICICI Socket.IO connect error"));
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

void IciciDirectWebSocket::handle_socketio_event(const QString& payload) {
    // Optional leading ack-id digits before the JSON array — skip them.
    int start = 0;
    while (start < payload.size() && payload.at(start).isDigit())
        ++start;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload.mid(start).toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return;

    const QJsonArray arr = doc.array();
    if (arr.isEmpty())
        return;

    const QString event = arr.at(0).toString();
    if (event == QStringLiteral("stock")) // tick feed; "order" notifications ignored for now
        handle_tick(arr.size() > 1 ? arr.at(1).toArray() : QJsonArray());
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick parsing — Breeze 'stock' positional list (parse_data field order)
// ─────────────────────────────────────────────────────────────────────────────

void IciciDirectWebSocket::handle_tick(const QJsonArray& data) {
    if (data.isEmpty())
        return;
    note_tick();

    // data[0] = "<exch>.<type>!<token>"
    const QString tok_str = data.at(0).toString();
    const int bang = tok_str.indexOf('!');
    const qint64 token = bang >= 0 ? tok_str.mid(bang + 1).toLongLong() : 0;

    QString symbol = tok_str;
    auto inst = InstrumentService::instance().find_by_token(static_cast<quint32>(token), "icicidirect");
    if (inst.has_value())
        symbol = inst->symbol;

    const int n = data.size();
    BrokerQuote q;
    q.symbol = symbol;
    q.open = jval(data.at(1));
    q.ltp = jval(data.at(2));
    q.high = jval(data.at(3));
    q.low = jval(data.at(4));
    q.bid = jval(data.at(6));
    q.bid_size = jval(data.at(7));
    q.ask = jval(data.at(8));
    q.ask_size = jval(data.at(9));

    // NSE/BSE cash = 21 fields (ttq@12, close@20); F&O = 23 fields (OI@12,
    // CHNGOI@13, ttq@14, close@22).
    if (n >= 23) {
        q.oi = static_cast<qint64>(jval(data.at(12)));
        q.volume = jval(data.at(14));
        q.close = jval(data.at(22));
    } else if (n >= 21) {
        q.volume = jval(data.at(12));
        q.close = jval(data.at(20));
    }

    if (q.close > 0) {
        q.change = q.ltp - q.close;
        q.change_pct = (q.ltp - q.close) / q.close * 100.0;
    }
    q.timestamp = QDateTime::currentMSecsSinceEpoch();

    // Partial frames omit unchanged fields → preserve via merge_tick.
    emit tick_received(merge_tick(QString::number(token), q));
}

} // namespace fincept::trading
