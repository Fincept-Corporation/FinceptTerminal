#include "trading/websocket/NorenWebSocket.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace fincept::trading {

// ────────────────────────────────────────────────────────────────────────────
// Field helpers — Noren sends numeric fields as JSON strings ("1234.50"). These
// coerce string-or-number-or-missing into the target type, treating absent /
// blank / "NA" as 0 so partial frames don't clobber cached values via merge_tick.
// ────────────────────────────────────────────────────────────────────────────

namespace {

double field_double(const QJsonObject& o, const char* key) {
    if (!o.contains(QLatin1String(key)))
        return 0.0;
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble())
        return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        double d = v.toString().toDouble(&ok);
        return ok ? d : 0.0;
    }
    return 0.0;
}

qint64 field_i64(const QJsonObject& o, const char* key) {
    if (!o.contains(QLatin1String(key)))
        return 0;
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble())
        return static_cast<qint64>(v.toDouble());
    if (v.isString()) {
        bool ok = false;
        // Volumes / OI can arrive as "1234" or "1234.0" — parse as double then truncate.
        double d = v.toString().toDouble(&ok);
        return ok ? static_cast<qint64>(d) : 0;
    }
    return 0;
}

int field_int(const QJsonObject& o, const char* key) {
    return static_cast<int>(field_i64(o, key));
}

QString field_str(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString() : QString();
}

} // namespace

// ────────────────────────────────────────────────────────────────────────────
// Construction
// ────────────────────────────────────────────────────────────────────────────

NorenWebSocket::NorenWebSocket(const QString& ws_url, const QString& user_id, const QString& account_id,
                               const QString& susertoken, QObject* parent)
    : BrokerWebSocketBase(parent),
      ws_url_(ws_url),
      user_id_(user_id),
      account_id_(account_id),
      susertoken_raw_(susertoken) {
    ws_ = new WebSocketClient(this);
    connect(ws_, &WebSocketClient::connected, this, &NorenWebSocket::on_ws_connected);
    connect(ws_, &WebSocketClient::disconnected, this, &NorenWebSocket::on_ws_disconnected);
    connect(ws_, &WebSocketClient::message_received, this, &NorenWebSocket::on_ws_message);
    connect(ws_, &WebSocketClient::error_occurred, this, &NorenWebSocket::error_occurred);

    heartbeat_timer_.setInterval(kHeartbeatMs);
    connect(&heartbeat_timer_, &QTimer::timeout, this, &NorenWebSocket::send_heartbeat);
}

// ────────────────────────────────────────────────────────────────────────────
// Public API
// ────────────────────────────────────────────────────────────────────────────

void NorenWebSocket::open() {
    LOG_INFO("NorenWS", QString("Connecting to %1").arg(ws_url_));
    auth_ok_ = false;
    ws_->connect_to(ws_url_);
}

void NorenWebSocket::close() {
    heartbeat_timer_.stop();
    stop_health_check();
    auth_ok_ = false;
    ws_->disconnect();
}

bool NorenWebSocket::is_connected() const {
    // Usable only once the broker has acked our connect frame.
    return ws_ && ws_->is_connected() && auth_ok_;
}

void NorenWebSocket::subscribe(const QStringList& scrips) {
    QStringList added;
    for (const QString& s : scrips) {
        const QString scrip = s.trimmed();
        if (scrip.isEmpty())
            continue;
        if (!subscribed_scrips_.contains(scrip)) {
            subscribed_scrips_.insert(scrip);
            added.append(scrip);
        }
    }
    if (added.isEmpty())
        return;

    if (is_connected()) {
        // Send only the newly-added scrips; existing ones are already streaming.
        const int count = static_cast<int>(added.size());
        for (int start = 0; start < count; start += kSubscribeBatch) {
            const int end = std::min(start + kSubscribeBatch, count);
            const QString key = QStringList(added.mid(start, end - start)).join('#');

            QJsonObject touchline;
            touchline["t"] = "t";
            touchline["k"] = key;
            ws_->send(QString::fromUtf8(QJsonDocument(touchline).toJson(QJsonDocument::Compact)));

            QJsonObject depth;
            depth["t"] = "d";
            depth["k"] = key;
            ws_->send(QString::fromUtf8(QJsonDocument(depth).toJson(QJsonDocument::Compact)));
        }
        LOG_INFO("NorenWS", QString("Subscribed %1 scrips").arg(added.size()));
    }
}

void NorenWebSocket::subscribe(const QVector<qint64>& tokens) {
    QStringList scrips;
    scrips.reserve(tokens.size());
    for (qint64 t : tokens)
        scrips.append(scrip_for_token(t));
    subscribe(scrips);
}

void NorenWebSocket::unsubscribe() {
    if (subscribed_scrips_.isEmpty())
        return;
    unsubscribe(QStringList(subscribed_scrips_.begin(), subscribed_scrips_.end()));
}

void NorenWebSocket::unsubscribe(const QStringList& scrips) {
    QStringList removed;
    for (const QString& s : scrips) {
        const QString scrip = s.trimmed();
        if (subscribed_scrips_.remove(scrip))
            removed.append(scrip);
    }
    if (removed.isEmpty())
        return;

    if (is_connected()) {
        const int count = static_cast<int>(removed.size());
        for (int start = 0; start < count; start += kSubscribeBatch) {
            const int end = std::min(start + kSubscribeBatch, count);
            const QString key = QStringList(removed.mid(start, end - start)).join('#');

            QJsonObject touchline;
            touchline["t"] = "u"; // unsubscribe touchline
            touchline["k"] = key;
            ws_->send(QString::fromUtf8(QJsonDocument(touchline).toJson(QJsonDocument::Compact)));

            QJsonObject depth;
            depth["t"] = "ud"; // unsubscribe depth
            depth["k"] = key;
            ws_->send(QString::fromUtf8(QJsonDocument(depth).toJson(QJsonDocument::Compact)));
        }
        LOG_INFO("NorenWS", QString("Unsubscribed %1 scrips").arg(removed.size()));
    }
}

void NorenWebSocket::set_exchange_for_token(qint64 token, const QString& exchange) {
    token_exchange_.insert(token, exchange);
}

// ────────────────────────────────────────────────────────────────────────────
// Slots
// ────────────────────────────────────────────────────────────────────────────

void NorenWebSocket::on_ws_connected() {
    LOG_INFO("NorenWS", "Socket open — sending connect frame");
    auth_ok_ = false;
    send_connect_frame();
    // Heartbeat + health check are armed on auth ack so we don't ping a socket
    // the broker hasn't accepted yet.
}

void NorenWebSocket::on_ws_disconnected() {
    LOG_WARN("NorenWS", "Disconnected");
    auth_ok_ = false;
    heartbeat_timer_.stop();
    stop_health_check();
    emit disconnected();
}

void NorenWebSocket::on_ws_message(const QString& message) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN("NorenWS", QString("Non-JSON / parse error: %1").arg(err.errorString()));
        return;
    }
    const QJsonObject obj = doc.object();
    const QString type = obj.value("t").toString();

    if (type == "ck" || type == "cf" || type == "ak") {
        // Connect acknowledgment. Status lives in "s" (AliceBlue/Shoonya) or
        // "k" (some Noren docs). Treat "OK" (any case) as success.
        const QString status = obj.contains("s") ? field_str(obj, "s") : field_str(obj, "k");
        if (status.compare("OK", Qt::CaseInsensitive) == 0) {
            LOG_INFO("NorenWS", "Authenticated");
            auth_ok_ = true;
            heartbeat_timer_.start();
            start_health_check(kHeartbeatMs, /*timeout_ms=*/90000);
            resubscribe_all();
            emit connected();
        } else {
            LOG_ERROR("NorenWS", QString("Auth failed: %1")
                                     .arg(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))));
            emit error_occurred(QStringLiteral("Authentication failed"));
        }
        return;
    }

    if (type == "tk") {
        handle_touchline(obj, /*full=*/true);
    } else if (type == "tf") {
        handle_touchline(obj, /*full=*/false);
    } else if (type == "dk") {
        handle_depth(obj, /*full=*/true);
    } else if (type == "df") {
        handle_depth(obj, /*full=*/false);
    } else if (type == "h") {
        // Heartbeat echo — ignore.
    } else {
        LOG_DEBUG("NorenWS", QString("Unhandled message type '%1'").arg(type));
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Send helpers
// ────────────────────────────────────────────────────────────────────────────

void NorenWebSocket::send_connect_frame() {
    QJsonObject msg;
    msg["t"] = "c";
    msg["uid"] = connect_uid();
    msg["actid"] = connect_actid();
    msg["susertoken"] = derive_susertoken(susertoken_raw_);
    msg["source"] = "API";
    ws_->send(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
}

void NorenWebSocket::send_heartbeat() {
    if (!ws_ || !ws_->is_connected())
        return;
    QJsonObject msg;
    msg["t"] = "h";
    ws_->send(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
    LOG_DEBUG("NorenWS", "Heartbeat sent");
}

void NorenWebSocket::resubscribe_all() {
    if (subscribed_scrips_.isEmpty())
        return;
    const QStringList all(subscribed_scrips_.begin(), subscribed_scrips_.end());
    LOG_INFO("NorenWS", QString("Resubscribing %1 scrips after auth").arg(all.size()));
    const int count = static_cast<int>(all.size());
    for (int start = 0; start < count; start += kSubscribeBatch) {
        const int end = std::min(start + kSubscribeBatch, count);
        const QString key = QStringList(all.mid(start, end - start)).join('#');

        QJsonObject touchline;
        touchline["t"] = "t";
        touchline["k"] = key;
        ws_->send(QString::fromUtf8(QJsonDocument(touchline).toJson(QJsonDocument::Compact)));

        QJsonObject depth;
        depth["t"] = "d";
        depth["k"] = key;
        ws_->send(QString::fromUtf8(QJsonDocument(depth).toJson(QJsonDocument::Compact)));
    }
}

void NorenWebSocket::flush_subscribe_queue() {
    // Token debounce queue from the base class — drain and route through the
    // string-based path so token and scrip subscribes share one wire format.
    const QVector<qint64> tokens = take_subscribe_queue();
    if (tokens.isEmpty())
        return;
    QStringList scrips;
    scrips.reserve(tokens.size());
    for (qint64 t : tokens)
        scrips.append(scrip_for_token(t));
    subscribe(scrips);
}

void NorenWebSocket::on_data_stall() {
    LOG_WARN("NorenWS", "Data stall detected — forcing reconnect");
    // Drop and re-open; WebSocketClient handles backoff, and resubscribe_all
    // fires again on the next auth ack.
    auth_ok_ = false;
    heartbeat_timer_.stop();
    ws_->disconnect();
    ws_->connect_to(ws_url_);
}

// ────────────────────────────────────────────────────────────────────────────
// Tick / depth parsing
// ────────────────────────────────────────────────────────────────────────────

void NorenWebSocket::handle_touchline(const QJsonObject& obj, bool full) {
    // `full` distinguishes tk (full snapshot) from tf (partial); merge_tick
    // handles the partial-overlay uniformly, so both are processed the same way.
    Q_UNUSED(full);
    const QString exchange = field_str(obj, "e");
    const QString token = field_str(obj, "tk");
    if (token.isEmpty())
        return;
    const QString key = exchange + "|" + token; // matches the scrip form

    BrokerQuote q;
    q.symbol = field_str(obj, "ts"); // present on full (tk) frames; blank on partial
    q.ltp = field_double(obj, "lp");
    q.open = field_double(obj, "o");
    q.high = field_double(obj, "h");
    q.low = field_double(obj, "l");
    q.close = field_double(obj, "c");
    q.volume = static_cast<double>(field_i64(obj, "v"));
    q.change_pct = field_double(obj, "pc");
    q.oi = field_i64(obj, "toi");
    // Feed time (ft) is an epoch-seconds string when present.
    const qint64 ft = field_i64(obj, "ft");
    q.timestamp = ft > 0 ? ft * 1000 : QDateTime::currentMSecsSinceEpoch();

    // Partial (tf) frames carry only changed fields; merge against last full tick.
    const BrokerQuote merged = merge_tick(key, q);
    note_tick();
    emit tick_received(merged);
}

void NorenWebSocket::handle_depth(const QJsonObject& obj, bool full) {
    // `full` distinguishes dk (full snapshot) from df (partial); merge_tick
    // handles the partial-overlay uniformly, so both are processed the same way.
    Q_UNUSED(full);
    const QString exchange = field_str(obj, "e");
    const QString token = field_str(obj, "tk");
    if (token.isEmpty())
        return;
    const QString key = exchange + "|" + token;

    // A depth frame also carries the touchline scalars (lp/o/h/l/c/v/pc/toi),
    // so feed them through merge_tick too — keeps the quote fresh for consumers
    // that subscribe to depth-only.
    BrokerQuote q;
    q.symbol = field_str(obj, "ts");
    q.ltp = field_double(obj, "lp");
    q.open = field_double(obj, "o");
    q.high = field_double(obj, "h");
    q.low = field_double(obj, "l");
    q.close = field_double(obj, "c");
    q.volume = static_cast<double>(field_i64(obj, "v"));
    q.change_pct = field_double(obj, "pc");
    q.oi = field_i64(obj, "toi");
    if (obj.contains("bp1"))
        q.bid = field_double(obj, "bp1");
    if (obj.contains("sp1"))
        q.ask = field_double(obj, "sp1");
    if (obj.contains("bq1"))
        q.bid_size = static_cast<double>(field_i64(obj, "bq1"));
    if (obj.contains("sq1"))
        q.ask_size = static_cast<double>(field_i64(obj, "sq1"));
    const qint64 ft = field_i64(obj, "ft");
    q.timestamp = ft > 0 ? ft * 1000 : QDateTime::currentMSecsSinceEpoch();

    const BrokerQuote merged = merge_tick(key, q);
    note_tick();
    emit tick_received(merged);

    // Build the 5-level order book. Partial (df) frames send only changed
    // levels; absent levels coerce to 0 and are skipped below, so a partial
    // book is still emitted with whatever changed — consumers that need a full
    // book should hold the last full (dk) snapshot.
    MarketDepth depth;
    depth.symbol = merged.symbol;
    depth.exchange = exchange;
    depth.ltp = merged.ltp;
    depth.volume = merged.volume;
    depth.oi = static_cast<double>(merged.oi);

    for (int i = 1; i <= 5; ++i) {
        const QByteArray bp = QByteArray("bp") + QByteArray::number(i);
        const QByteArray bq = QByteArray("bq") + QByteArray::number(i);
        const QByteArray bo = QByteArray("bo") + QByteArray::number(i);
        const QByteArray sp = QByteArray("sp") + QByteArray::number(i);
        const QByteArray sq = QByteArray("sq") + QByteArray::number(i);
        const QByteArray so = QByteArray("so") + QByteArray::number(i);

        const double bid_price = field_double(obj, bp.constData());
        if (bid_price > 0.0) {
            DepthLevel lvl;
            lvl.price = bid_price;
            lvl.quantity = field_int(obj, bq.constData());
            lvl.orders = field_int(obj, bo.constData());
            depth.bids.append(lvl);
        }
        const double ask_price = field_double(obj, sp.constData());
        if (ask_price > 0.0) {
            DepthLevel lvl;
            lvl.price = ask_price;
            lvl.quantity = field_int(obj, sq.constData());
            lvl.orders = field_int(obj, so.constData());
            depth.asks.append(lvl);
        }
    }

    if (!depth.bids.isEmpty() || !depth.asks.isEmpty())
        emit depth_received(depth);
}

QString NorenWebSocket::scrip_for_token(qint64 token) const {
    const QString exch = token_exchange_.value(token, default_exchange_);
    return exch + "|" + QString::number(token);
}

} // namespace fincept::trading
