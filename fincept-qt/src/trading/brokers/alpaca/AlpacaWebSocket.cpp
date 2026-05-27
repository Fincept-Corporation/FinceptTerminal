#include "trading/brokers/alpaca/AlpacaWebSocket.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QtConcurrent>

namespace fincept::trading {

namespace {
QString compact_json(const QJsonObject& obj) {
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
} // namespace

// ── Construction ────────────────────────────────────────────────────────────

AlpacaWebSocket::AlpacaWebSocket(const QString& api_key, const QString& api_secret,
                                 QObject* parent)
    : QObject(parent), api_key_(api_key), api_secret_(api_secret) {
    ws_ = new fincept::WebSocketClient(this);
    connect(ws_, &fincept::WebSocketClient::connected, this, &AlpacaWebSocket::on_ws_connected);
    connect(ws_, &fincept::WebSocketClient::disconnected, this, &AlpacaWebSocket::on_ws_disconnected);
    connect(ws_, &fincept::WebSocketClient::message_received, this, &AlpacaWebSocket::on_ws_message);
    connect(ws_, &fincept::WebSocketClient::error_occurred, this, &AlpacaWebSocket::on_ws_error);

    heartbeat_timer_ = new QTimer(this);
    heartbeat_timer_->setSingleShot(true);
    heartbeat_timer_->setInterval(HEARTBEAT_TIMEOUT_MS);
    connect(heartbeat_timer_, &QTimer::timeout, this, &AlpacaWebSocket::on_heartbeat_timeout);

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(CLOCK_CHECK_INTERVAL_MS);
    connect(clock_timer_, &QTimer::timeout, this, &AlpacaWebSocket::on_clock_check);
}

AlpacaWebSocket::~AlpacaWebSocket() {
    close();
}

// ── Public API ──────────────────────────────────────────────────────────────

void AlpacaWebSocket::open() {
    LOG_INFO(kTag, "Checking market clock before connecting...");

    QPointer<AlpacaWebSocket> self = this;
    const QString api_key = api_key_;
    const QString api_secret = api_secret_;

    (void)QtConcurrent::run([self, api_key, api_secret]() {
        auto* broker = BrokerRegistry::instance().get("alpaca");
        if (!broker) return;

        BrokerCredentials creds;
        creds.broker_id = "alpaca";
        creds.api_key = api_key;
        creds.api_secret = api_secret;

        auto result = broker->get_clock(creds);
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;

            bool should_connect = true;
            if (result.success && result.data) {
                const auto& clock = *result.data;
                if (!clock.is_open) {
                    LOG_INFO(kTag, QString("Market is closed. Next open: %1").arg(clock.next_open));
                    should_connect = false;
                    emit self->market_closed();
                }
            }

            if (should_connect) {
                LOG_INFO(kTag, QString("Connecting to %1").arg(kWsUrl));
                self->ws_->connect_to(QString::fromLatin1(kWsUrl));
            }

            self->clock_timer_->start();
        }, Qt::QueuedConnection);
    });
}

void AlpacaWebSocket::close() {
    if (heartbeat_timer_)
        heartbeat_timer_->stop();
    if (clock_timer_)
        clock_timer_->stop();
    authenticated_ = false;
    if (ws_)
        ws_->disconnect();
    if (connected_.exchange(false))
        emit disconnected();
}

void AlpacaWebSocket::subscribe(const QStringList& symbols) {
    for (const auto& s : symbols) {
        if (!subscribed_symbols_.contains(s))
            subscribed_symbols_.append(s);
    }
    if (authenticated_)
        send_subscribe(symbols);
}

void AlpacaWebSocket::unsubscribe(const QStringList& symbols) {
    for (const auto& s : symbols)
        subscribed_symbols_.removeAll(s);
    if (authenticated_)
        send_unsubscribe(symbols);
}

void AlpacaWebSocket::set_subscriptions(const QStringList& symbols) {
    if (authenticated_ && !subscribed_symbols_.isEmpty())
        send_unsubscribe(subscribed_symbols_);
    subscribed_symbols_ = symbols;
    if (authenticated_)
        send_subscribe(symbols);
}

// ── WebSocketClient callbacks ───────────────────────────────────────────────

void AlpacaWebSocket::on_ws_connected() {
    LOG_INFO(kTag, "WebSocket connected, sending auth...");
    heartbeat_timer_->start();
    send_auth();
}

void AlpacaWebSocket::on_ws_disconnected() {
    LOG_WARN(kTag, "WebSocket disconnected");
    heartbeat_timer_->stop();
    authenticated_ = false;
    if (connected_.exchange(false))
        emit disconnected();
}

void AlpacaWebSocket::on_ws_error(const QString& error) {
    LOG_ERROR(kTag, "WebSocket error: " + error);
    emit error_occurred(error);
}

void AlpacaWebSocket::on_heartbeat_timeout() {
    LOG_WARN(kTag, "No messages received in 15s — forcing reconnect");
    if (ws_)
        ws_->disconnect();
}

void AlpacaWebSocket::on_clock_check() {
    QPointer<AlpacaWebSocket> self = this;
    const QString api_key = api_key_;
    const QString api_secret = api_secret_;

    (void)QtConcurrent::run([self, api_key, api_secret]() {
        auto* broker = BrokerRegistry::instance().get("alpaca");
        if (!broker) return;

        BrokerCredentials creds;
        creds.broker_id = "alpaca";
        creds.api_key = api_key;
        creds.api_secret = api_secret;

        auto result = broker->get_clock(creds);
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;
            if (!result.success || !result.data) return;

            const auto& clock = *result.data;
            if (clock.is_open && !self->connected_.load()) {
                LOG_INFO(kTag, "Market opened — connecting WebSocket");
                self->ws_->connect_to(QString::fromLatin1(kWsUrl));
            } else if (!clock.is_open && self->connected_.load()) {
                LOG_INFO(kTag, "Market closed — disconnecting WebSocket");
                self->close();
                self->clock_timer_->start();
                emit self->market_closed();
            }
        }, Qt::QueuedConnection);
    });
}

// ── Protocol messages ───────────────────────────────────────────────────────

void AlpacaWebSocket::send_auth() {
    QJsonObject msg;
    msg["action"] = QStringLiteral("auth");
    msg["key"] = api_key_;
    msg["secret"] = api_secret_;
    ws_->send(compact_json(msg));
}

void AlpacaWebSocket::send_subscribe(const QStringList& symbols) {
    if (symbols.isEmpty()) return;

    QJsonArray syms;
    for (const auto& s : symbols)
        syms.append(s);

    QJsonObject msg;
    msg["action"] = QStringLiteral("subscribe");
    msg["trades"] = syms;
    msg["quotes"] = syms;
    msg["bars"] = syms;

    LOG_INFO(kTag, QString("Subscribing to %1 symbols").arg(symbols.size()));
    ws_->send(compact_json(msg));
}

void AlpacaWebSocket::send_unsubscribe(const QStringList& symbols) {
    if (symbols.isEmpty()) return;

    QJsonArray syms;
    for (const auto& s : symbols)
        syms.append(s);

    QJsonObject msg;
    msg["action"] = QStringLiteral("unsubscribe");
    msg["trades"] = syms;
    msg["quotes"] = syms;
    msg["bars"] = syms;

    ws_->send(compact_json(msg));
}

// ── Message parsing ─────────────────────────────────────────────────────────

void AlpacaWebSocket::on_ws_message(const QString& message) {
    if (heartbeat_timer_)
        heartbeat_timer_->start();

    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError)
        return;

    if (!doc.isArray())
        return;

    const auto arr = doc.array();
    for (const auto& val : arr) {
        if (!val.isObject()) continue;
        handle_message(val.toObject());
    }
}

void AlpacaWebSocket::handle_message(const QJsonObject& msg) {
    const QString type = msg.value("T").toString();

    if (type == QLatin1String("success"))       handle_success(msg);
    else if (type == QLatin1String("error"))     handle_error(msg);
    else if (type == QLatin1String("subscription")) handle_subscription(msg);
    else if (type == QLatin1String("t"))         handle_trade(msg);
    else if (type == QLatin1String("q"))         handle_quote(msg);
    else if (type == QLatin1String("b"))         handle_bar(msg);
}

void AlpacaWebSocket::handle_success(const QJsonObject& msg) {
    const QString m = msg.value("msg").toString();
    if (m == QLatin1String("connected")) {
        LOG_INFO(kTag, "Server acknowledged connection");
    } else if (m == QLatin1String("authenticated")) {
        LOG_INFO(kTag, "Authentication successful");
        authenticated_ = true;
        if (!connected_.exchange(true))
            emit connected();
        if (!subscribed_symbols_.isEmpty())
            send_subscribe(subscribed_symbols_);
    }
}

void AlpacaWebSocket::handle_error(const QJsonObject& msg) {
    const int code = msg.value("code").toInt();
    const QString err_msg = msg.value("msg").toString();
    LOG_ERROR(kTag, QString("Error %1: %2").arg(code).arg(err_msg));

    if (code == 402) {
        LOG_ERROR(kTag, "Authentication failed — invalid credentials");
        emit error_occurred("Authentication failed: " + err_msg);
    } else if (code == 406) {
        LOG_WARN(kTag, "Connection limit exceeded — another session may be active");
        emit error_occurred("Connection limit exceeded");
    } else {
        emit error_occurred(QString("Error %1: %2").arg(code).arg(err_msg));
    }
}

void AlpacaWebSocket::handle_subscription(const QJsonObject& msg) {
    const auto trades = msg.value("trades").toArray();
    const auto quotes = msg.value("quotes").toArray();
    const auto bars = msg.value("bars").toArray();
    LOG_INFO(kTag, QString("Subscribed — trades: %1, quotes: %2, bars: %3")
                       .arg(trades.size()).arg(quotes.size()).arg(bars.size()));
}

void AlpacaWebSocket::handle_trade(const QJsonObject& msg) {
    BrokerTrade trade;
    trade.symbol = msg.value("S").toString();
    trade.price = msg.value("p").toDouble();
    trade.size = msg.value("s").toDouble();
    trade.exchange = msg.value("x").toString();
    trade.timestamp = msg.value("t").toString();
    trade.tape = msg.value("z").toString();

    const auto conditions = msg.value("c").toArray();
    for (const auto& c : conditions)
        trade.conditions.append(c.toString());

    if (trade.price <= 0.0) return;
    emit trade_received(trade);
}

void AlpacaWebSocket::handle_quote(const QJsonObject& msg) {
    const QString symbol = msg.value("S").toString();
    const double bid = msg.value("bp").toDouble();
    const double ask = msg.value("ap").toDouble();
    const double bid_size = msg.value("bs").toDouble();
    const double ask_size = msg.value("as").toDouble();
    const int64_t ts = parse_rfc3339_to_ms(msg.value("t").toString());

    BrokerQuote q;
    q.symbol = symbol;
    q.bid = bid;
    q.ask = ask;
    q.bid_size = bid_size;
    q.ask_size = ask_size;
    q.timestamp = ts;

    if (bid > 0.0 && ask > 0.0)
        q.ltp = (bid + ask) / 2.0;

    emit tick_received(q);
}

void AlpacaWebSocket::handle_bar(const QJsonObject& msg) {
    const QString symbol = msg.value("S").toString();

    BrokerCandle candle;
    candle.open = msg.value("o").toDouble();
    candle.high = msg.value("h").toDouble();
    candle.low = msg.value("l").toDouble();
    candle.close = msg.value("c").toDouble();
    candle.volume = msg.value("v").toDouble();
    candle.timestamp = parse_rfc3339_to_ms(msg.value("t").toString());

    if (candle.open <= 0.0) return;
    emit bar_received(symbol, candle);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

int64_t AlpacaWebSocket::parse_rfc3339_to_ms(const QString& ts) {
    if (ts.isEmpty()) return 0;
    const auto dt = QDateTime::fromString(ts, Qt::ISODateWithMs);
    if (dt.isValid()) return dt.toMSecsSinceEpoch();
    const auto dt2 = QDateTime::fromString(ts, Qt::ISODate);
    return dt2.isValid() ? dt2.toMSecsSinceEpoch() : 0;
}

} // namespace fincept::trading
