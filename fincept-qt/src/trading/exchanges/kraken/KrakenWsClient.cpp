#include "trading/exchanges/kraken/KrakenWsClient.h"

#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"
#include "python/PythonRunner.h"
#include "trading/exchanges/kraken/KrakenSymbolMapper.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

#include <numeric>

namespace fincept::trading::kraken {

namespace {
constexpr const char* kKrakenWsUrl = "wss://ws.kraken.com/v2";
constexpr const char* kTag = "KrakenWS";

QString send_compact(const QJsonObject& obj) {
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QVector<QPair<double, double>> parse_levels(const QJsonArray& arr) {
    QVector<QPair<double, double>> out;
    out.reserve(arr.size());
    for (const auto& v : arr) {
        const auto o = v.toObject();
        out.append({o.value("price").toDouble(), o.value("qty").toDouble()});
    }
    return out;
}
} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

KrakenWsClient::KrakenWsClient(QObject* parent) : QObject(parent) {
    ws_ = new fincept::WebSocketClient(this);
    connect(ws_, &fincept::WebSocketClient::connected, this, &KrakenWsClient::on_ws_connected);
    connect(ws_, &fincept::WebSocketClient::disconnected, this, &KrakenWsClient::on_ws_disconnected);
    connect(ws_, &fincept::WebSocketClient::message_received, this, &KrakenWsClient::on_ws_message);
    connect(ws_, &fincept::WebSocketClient::error_occurred, this, &KrakenWsClient::on_ws_error);

    heartbeat_timer_ = new QTimer(this);
    heartbeat_timer_->setSingleShot(true);
    heartbeat_timer_->setInterval(HEARTBEAT_TIMEOUT_MS);
    connect(heartbeat_timer_, &QTimer::timeout, this, &KrakenWsClient::on_heartbeat_timeout);

    stats_timer_ = new QTimer(this);
    stats_timer_->setInterval(STATS_LOG_INTERVAL_MS);
    connect(stats_timer_, &QTimer::timeout, this, &KrakenWsClient::emit_stats_log);
}

void KrakenWsClient::emit_stats_log() {
    if (ticker_count_since_log_.isEmpty()) {
        LOG_WARN(kTag, "stats: NO ticker updates received in last 10s — feed may be stalled");
        return;
    }

    // Per-symbol breakdown — sort by Fincept symbol for stable output. This
    // catches the case where a /USD-resolved symbol receives ticks under a
    // Kraken name we don't have in the reverse map (so resolve_to_fincept
    // returns the Kraken name verbatim, never matching all_symbols_).
    QStringList active, silent, unknown_keys;
    int total = 0;
    QSet<QString> watchlist_set(all_symbols_.begin(), all_symbols_.end());

    for (auto it = ticker_count_since_log_.constBegin();
         it != ticker_count_since_log_.constEnd(); ++it) {
        total += it.value();
        if (!watchlist_set.contains(it.key()))
            unknown_keys.append(QString("%1=%2").arg(it.key()).arg(it.value()));
    }

    for (const auto& fincept_sym : all_symbols_) {
        const int n = ticker_count_since_log_.value(fincept_sym, 0);
        if (n > 0)
            active.append(QString("%1=%2").arg(fincept_sym).arg(n));
        else
            silent.append(fincept_sym);
    }

    LOG_INFO(kTag, QString("stats (10s) total=%1 active=%2/%3")
                       .arg(total).arg(active.size()).arg(all_symbols_.size()));
    LOG_INFO(kTag, "  active: " + active.join(", "));
    if (!silent.isEmpty())
        LOG_WARN(kTag, "  silent: " + silent.join(", "));
    if (!unknown_keys.isEmpty())
        LOG_WARN(kTag, "  unmapped (kraken name leaked through): " + unknown_keys.join(", "));
    ticker_count_since_log_.clear();
}

KrakenWsClient::~KrakenWsClient() {
    stop();
}

// ── Public API ───────────────────────────────────────────────────────────────

void KrakenWsClient::start(const QString& primary_symbol, const QStringList& all_symbols) {
    primary_symbol_ = primary_symbol;
    all_symbols_ = all_symbols;
    if (!all_symbols_.contains(primary_symbol_))
        all_symbols_.prepend(primary_symbol_);

    kraken_to_fincept_.clear();
    fincept_to_kraken_.clear();
    usd_fallback_attempted_.clear();
    book_.reset();

    // Resolve every requested symbol via ccxt before connecting. This avoids
    // the "subscribe → nack → retry" round-trips for the ~40% of watchlist
    // symbols that aren't /USDT-listed on Kraken (POL/USD instead of MATIC,
    // WIF/USD instead of WIF/USDT, etc.). The resolve takes ~1.5s on cold
    // boot but means every symbol the screen shows is a guaranteed valid
    // Kraken pair before we send a single subscribe frame.
    resolve_symbols_then_connect();
}

void KrakenWsClient::resolve_symbols_then_connect() {
    const QString scripts_dir = python::PythonRunner::instance().scripts_dir();
    const QString python_path = python::PythonRunner::instance().python_path();
    // PythonRunner::run takes a path relative to scripts_dir_ — passing an
    // absolute path causes it to prepend scripts_dir_ again, doubling the path.
    const QString script = QStringLiteral("exchange/resolve_kraken_symbols.py");

    if (python_path.isEmpty() || scripts_dir.isEmpty()) {
        LOG_WARN(kTag, "Python unavailable — falling back to passthrough symbol mapping");
        for (const auto& s : all_symbols_)
            register_symbol(s, to_kraken(s));
        ws_->connect_to(kKrakenWsUrl);
        return;
    }

    QStringList args;
    args.reserve(all_symbols_.size());
    for (const auto& s : all_symbols_)
        args << s;

    QPointer<KrakenWsClient> self = this;
    LOG_INFO(kTag, QString("Resolving %1 symbols via ccxt…").arg(all_symbols_.size()));
    python::PythonRunner::instance().run(
        script, args,
        [self](python::PythonResult r) {
            // PythonRunner's callback fires on the main thread (where we
            // currently still live — set_io_thread was called but we haven't
            // moved yet). Apply the resolved map here, THEN moveToThread so
            // the ws_/QWebSocket affinity is correct from the very first
            // connect_to call.
            if (!self)
                return;
            if (!r.success) {
                LOG_WARN(kTag, QString("Symbol resolve failed: %1 — falling back to passthrough")
                                          .arg(r.error.left(200)));
                for (const auto& s : self->all_symbols_)
                    self->register_symbol(s, to_kraken(s));
            } else {
                const auto doc = QJsonDocument::fromJson(r.output.toUtf8());
                const auto resolved = doc.object().value("resolved").toObject();
                int ok = 0, dropped = 0;
                QStringList retained_symbols;
                retained_symbols.reserve(self->all_symbols_.size());
                for (const auto& fincept_sym : self->all_symbols_) {
                    const auto v = resolved.value(fincept_sym);
                    if (v.isNull() || v.toString().isEmpty()) {
                        ++dropped;
                        LOG_WARN(kTag, "No Kraken market for " + fincept_sym + " — skipping");
                        continue;
                    }
                    const QString kraken_sym = v.toString();
                    self->register_symbol(fincept_sym, kraken_sym);
                    retained_symbols.append(fincept_sym);
                    ++ok;
                }
                self->all_symbols_ = retained_symbols;
                if (!retained_symbols.contains(self->primary_symbol_) && !retained_symbols.isEmpty()) {
                    LOG_WARN(kTag,
                             "Primary symbol " + self->primary_symbol_ + " unsupported on Kraken");
                }
                LOG_INFO(kTag, QString("Resolved %1 symbols (%2 dropped)").arg(ok).arg(dropped));
            }

            // Cross over to the dedicated I/O thread now. Children (ws_,
            // heartbeat_timer_, stats_timer_) move along automatically.
            if (self->io_thread_) {
                LOG_INFO(kTag, "Moving to I/O thread for WebSocket connect");
                self->moveToThread(self->io_thread_);
                // ws_->connect_to must run on the new thread. invokeMethod
                // posts onto the worker's event loop.
                QMetaObject::invokeMethod(self.data(), [self]() {
                    if (!self) return;
                    LOG_INFO(kTag, QString("Connecting to %1").arg(kKrakenWsUrl));
                    self->ws_->connect_to(kKrakenWsUrl);
                }, Qt::QueuedConnection);
            } else {
                // No I/O thread provided — stay on current thread (legacy path).
                LOG_INFO(kTag, QString("Connecting to %1 (no dedicated I/O thread)").arg(kKrakenWsUrl));
                self->ws_->connect_to(kKrakenWsUrl);
            }
        });
}

void KrakenWsClient::stop() {
    if (heartbeat_timer_)
        heartbeat_timer_->stop();
    if (stats_timer_)
        stats_timer_->stop();
    ticker_count_since_log_.clear();
    if (ws_)
        ws_->disconnect();
    if (connected_.exchange(false))
        emit connection_changed(false);
    book_.reset();
}

void KrakenWsClient::set_primary_symbol(const QString& symbol) {
    if (symbol == primary_symbol_)
        return;
    const QString old_primary = primary_symbol_;
    primary_symbol_ = symbol;
    if (!all_symbols_.contains(symbol))
        all_symbols_.prepend(symbol);

    if (!connected_.load())
        return;  // subscriptions will be sent on next connect

    // Unsubscribe ohlc/book/trade for the old primary; subscribe for the new.
    const QString old_kraken = fincept_to_kraken_.value(old_primary, to_kraken(old_primary));
    const QString new_kraken = [&]() {
        QString k = fincept_to_kraken_.value(symbol);
        if (k.isEmpty()) {
            k = to_kraken(symbol);
            register_symbol(symbol, k);
        }
        return k;
    }();

    if (!old_kraken.isEmpty()) {
        send_unsubscribe(QStringLiteral("book"),  {old_kraken});
        send_unsubscribe(QStringLiteral("trade"), {old_kraken});
        send_unsubscribe(QStringLiteral("ohlc"),  {old_kraken}, 1);
        book_.reset_symbol(old_kraken);
    }
    if (!new_kraken.isEmpty()) {
        send_subscribe(QStringLiteral("book"),  {new_kraken});
        send_subscribe(QStringLiteral("trade"), {new_kraken});
        send_subscribe(QStringLiteral("ohlc"),  {new_kraken}, 1);
    }
}

// ── Symbol bookkeeping ───────────────────────────────────────────────────────

void KrakenWsClient::register_symbol(const QString& fincept_symbol, const QString& kraken_symbol) {
    fincept_to_kraken_.insert(fincept_symbol, kraken_symbol);
    kraken_to_fincept_.insert(kraken_symbol, fincept_symbol);
}

QString KrakenWsClient::resolve_to_fincept(const QString& kraken_symbol) const {
    return kraken_to_fincept_.value(kraken_symbol, kraken_symbol);
}

// ── Subscribe / unsubscribe helpers ──────────────────────────────────────────

void KrakenWsClient::send_subscribe(const QString& channel, const QStringList& kraken_symbols,
                                    int interval_minutes, const QString& event_trigger) {
    if (kraken_symbols.isEmpty())
        return;

    QJsonObject params;
    params["channel"] = channel;
    QJsonArray syms;
    for (const auto& s : kraken_symbols)
        syms.append(s);
    params["symbol"] = syms;
    if (interval_minutes > 0)
        params["interval"] = interval_minutes;
    if (!event_trigger.isEmpty())
        params["event_trigger"] = event_trigger;

    QJsonObject msg;
    msg["method"] = "subscribe";
    msg["params"] = params;
    LOG_INFO(kTag, QString("→ subscribe %1 (%2 symbols%3)")
                       .arg(channel)
                       .arg(kraken_symbols.size())
                       .arg(event_trigger.isEmpty() ? QString() : QString(", trigger=%1").arg(event_trigger)));
    ws_->send(send_compact(msg));
}

void KrakenWsClient::send_unsubscribe(const QString& channel, const QStringList& kraken_symbols,
                                      int interval_minutes) {
    if (kraken_symbols.isEmpty())
        return;
    QJsonObject params;
    params["channel"] = channel;
    QJsonArray syms;
    for (const auto& s : kraken_symbols)
        syms.append(s);
    params["symbol"] = syms;
    if (interval_minutes > 0)
        params["interval"] = interval_minutes;

    QJsonObject msg;
    msg["method"] = "unsubscribe";
    msg["params"] = params;
    ws_->send(send_compact(msg));
}

void KrakenWsClient::send_initial_subscriptions() {
    QStringList all_kraken;
    all_kraken.reserve(all_symbols_.size());
    for (const auto& s : all_symbols_) {
        const QString k = fincept_to_kraken_.value(s);
        if (!k.isEmpty())
            all_kraken.append(k);
    }
    if (all_kraken.isEmpty())
        return;

    // Ticker — every symbol in the watchlist. event_trigger="bbo" makes the
    // ticker fire on every best bid/offer change (vs default "trades" which
    // only fires on a print). Without "bbo", illiquid /USD pairs go minutes
    // between updates and the watchlist looks frozen — confirmed via wscat
    // probe (12 symbols / 20s: 2 updates with default vs 541 with bbo).
    send_subscribe(QStringLiteral("ticker"), all_kraken, 0, QStringLiteral("bbo"));

    // Book / trade / ohlc — only the primary symbol. Watchlist symbols don't
    // need their full orderbook or trade tape.
    const QString primary_kraken = fincept_to_kraken_.value(primary_symbol_);
    if (!primary_kraken.isEmpty()) {
        send_subscribe(QStringLiteral("book"),  {primary_kraken});
        send_subscribe(QStringLiteral("trade"), {primary_kraken});
        send_subscribe(QStringLiteral("ohlc"),  {primary_kraken}, 1);
    }

    LOG_INFO(kTag, QString("Initial subscriptions sent (%1 symbols)").arg(all_kraken.size()));
}

// ── WebSocketClient callbacks ────────────────────────────────────────────────

void KrakenWsClient::on_ws_connected() {
    LOG_INFO(kTag, "WebSocket connected");
    heartbeat_timer_->start();
    if (stats_timer_)
        stats_timer_->start();
    send_initial_subscriptions();
    if (!connected_.exchange(true))
        emit connection_changed(true);
}

void KrakenWsClient::on_ws_disconnected() {
    LOG_WARN(kTag, "WebSocket disconnected");
    heartbeat_timer_->stop();
    if (stats_timer_)
        stats_timer_->stop();
    ticker_count_since_log_.clear();
    if (connected_.exchange(false))
        emit connection_changed(false);
    book_.reset();
}

void KrakenWsClient::on_ws_error(const QString& error) {
    LOG_ERROR(kTag, "WebSocket error: " + error);
}

void KrakenWsClient::on_heartbeat_timeout() {
    LOG_WARN(kTag, "Heartbeat timeout — forcing reconnect");
    // Closing triggers WebSocketClient's exponential reconnect.
    if (ws_)
        ws_->disconnect();
}

void KrakenWsClient::on_ws_message(const QString& message) {
    // Reset heartbeat watchdog on any inbound frame.
    if (heartbeat_timer_)
        heartbeat_timer_->start();

    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    handle_message(doc.object());
}

// ── Message dispatch ─────────────────────────────────────────────────────────

void KrakenWsClient::handle_message(const QJsonObject& msg) {
    // Ack frames carry a `method` field instead of `channel`.
    if (msg.contains(QLatin1String("method"))) {
        const QString method = msg.value("method").toString();
        if (method == QLatin1String("subscribe") || method == QLatin1String("unsubscribe")) {
            handle_subscribe_ack(msg);
            return;
        }
        if (method == QLatin1String("pong")) {
            return;  // heartbeat watchdog already reset
        }
        return;
    }

    const QString channel = msg.value("channel").toString();
    if (channel == QLatin1String("heartbeat"))
        return;
    if (channel == QLatin1String("status")) {
        handle_status(msg);
        return;
    }

    const QString type = msg.value("type").toString();
    const auto data = msg.value("data").toArray();
    const bool is_snapshot = (type == QLatin1String("snapshot"));

    if (channel == QLatin1String("ticker"))   handle_ticker(data);
    else if (channel == QLatin1String("book"))   handle_book(data, is_snapshot);
    else if (channel == QLatin1String("trade"))  handle_trade(data);
    else if (channel == QLatin1String("ohlc"))   handle_ohlc(data);
}

void KrakenWsClient::handle_status(const QJsonObject& msg) {
    const auto data = msg.value("data").toArray();
    if (data.isEmpty())
        return;
    const QString system = data.first().toObject().value("system").toString();
    LOG_INFO(kTag, "Kraken system status: " + system);
}

void KrakenWsClient::handle_subscribe_ack(const QJsonObject& msg) {
    const bool success = msg.value("success").toBool(false);
    const QString method = msg.value("method").toString();
    const auto result = msg.value("result").toObject();
    const QString channel = result.value("channel").toString();
    const QString error = msg.value("error").toString();

    // On success the symbol lives in `result.symbol`; on failure Kraken puts
    // it at top level (verified via wscat probe against ws.kraken.com/v2).
    QString kraken_symbol = result.value("symbol").toString();
    if (kraken_symbol.isEmpty())
        kraken_symbol = msg.value("symbol").toString();

    if (success) {
        LOG_INFO(kTag, QString("%1 ack: %2 / %3").arg(method, channel, kraken_symbol));
        return;
    }

    LOG_WARN(kTag, QString("%1 nack: %2 / %3 — %4").arg(method, channel, kraken_symbol, error));

    // ccxt-resolve already filtered unsupported symbols out of all_symbols_,
    // so a nack here means the resolver picked a market Kraken doesn't accept
    // on this specific channel (e.g. a perp listed for ohlc but not book).
    // The /USD retry is the safety net; if it also fails we surface the error.
    if (!maybe_retry_with_usd(channel, kraken_symbol, error))
        emit subscribe_failed(channel, resolve_to_fincept(kraken_symbol), error);
}

bool KrakenWsClient::maybe_retry_with_usd(const QString& channel, const QString& failed_kraken_symbol,
                                          const QString& error) {
    if (failed_kraken_symbol.isEmpty())
        return false;
    if (usd_fallback_attempted_.contains(failed_kraken_symbol))
        return false;
    usd_fallback_attempted_.insert(failed_kraken_symbol);

    // Determine the original Fincept symbol.
    const QString fincept_symbol = resolve_to_fincept(failed_kraken_symbol);
    const QString usd_form = usd_fallback(fincept_symbol);
    if (usd_form.isEmpty())
        return false;

    // Re-register: from now on, ticks for `usd_form` map back to `fincept_symbol`.
    fincept_to_kraken_.insert(fincept_symbol, usd_form);
    kraken_to_fincept_.remove(failed_kraken_symbol);
    kraken_to_fincept_.insert(usd_form, fincept_symbol);

    LOG_INFO(kTag, QString("Retrying %1 / %2 → %3").arg(channel, failed_kraken_symbol, usd_form));

    if (channel == QLatin1String("ohlc"))
        send_subscribe(channel, {usd_form}, 1);
    else
        send_subscribe(channel, {usd_form});
    Q_UNUSED(error);
    return true;
}

// ── Channel handlers ─────────────────────────────────────────────────────────

void KrakenWsClient::handle_ticker(const QJsonArray& data) {
    for (const auto& v : data) {
        const auto o = v.toObject();
        const QString k_sym = o.value("symbol").toString();
        if (k_sym.isEmpty())
            continue;
        TickerData t;
        t.symbol = resolve_to_fincept(k_sym);
        t.bid = o.value("bid").toDouble();
        t.ask = o.value("ask").toDouble();
        t.last = o.value("last").toDouble();
        t.high = o.value("high").toDouble();
        t.low = o.value("low").toDouble();
        t.base_volume = o.value("volume").toDouble();
        t.change = o.value("change").toDouble();
        t.percentage = o.value("change_pct").toDouble();
        t.timestamp = parse_iso_to_ms(o.value("timestamp").toString());

        // We subscribe with event_trigger="bbo" — Kraken pushes ticks on every
        // best bid/offer change. But Kraken's `last` field is the most-recent
        // TRADE price, which doesn't move between trades. If we publish that
        // unchanged value 30+ times/10s, every consumer (watchlist row, order
        // entry, position mark-to-market) sees the same number and the UI
        // looks frozen even though data is flowing. Use the mid-price instead
        // so every BBO change shifts the displayed price by at least the tick
        // size. Fall back to last when bid/ask aren't both populated.
        if (t.bid > 0.0 && t.ask > 0.0)
            t.last = (t.bid + t.ask) / 2.0;
        if (t.last <= 0.0)
            continue;

        ticker_count_since_log_[t.symbol] += 1;
        emit ticker_received(t);
    }
}

void KrakenWsClient::handle_book(const QJsonArray& data, bool is_snapshot) {
    for (const auto& v : data) {
        const auto o = v.toObject();
        const QString k_sym = o.value("symbol").toString();
        if (k_sym.isEmpty())
            continue;
        const auto bids = parse_levels(o.value("bids").toArray());
        const auto asks = parse_levels(o.value("asks").toArray());
        const quint32 checksum = static_cast<quint32>(o.value("checksum").toDouble());
        const QString ts = o.value("timestamp").toString();

        OrderBookData ob;
        if (is_snapshot)
            ob = book_.apply_snapshot(k_sym, bids, asks, checksum, ts);
        else
            ob = book_.apply_update(k_sym, bids, asks, checksum, ts);

        ob.symbol = resolve_to_fincept(k_sym);

        // CRC32 verification is intentionally a no-op here. Kraken's checksum
        // requires exact per-pair price/lot precision rendering — the obvious
        // 8-dp-fixed approximation produces false positives that trigger a
        // resubscribe storm, which Kraken responds to with `Exceeded msg rate`
        // and rejects ALL subsequent subscribes (including /USD tickers in
        // the watchlist). Since the displayed book is for visual depth only
        // (orders route through REST), an occasional drift is acceptable —
        // the next Kraken-pushed snapshot resyncs us automatically.
        Q_UNUSED(checksum);

        emit orderbook_received(ob);
    }
}

void KrakenWsClient::handle_trade(const QJsonArray& data) {
    for (const auto& v : data) {
        const auto o = v.toObject();
        const QString k_sym = o.value("symbol").toString();
        if (k_sym.isEmpty())
            continue;
        TradeData td;
        td.id = QString::number(static_cast<qint64>(o.value("trade_id").toDouble()));
        td.symbol = resolve_to_fincept(k_sym);
        td.side = o.value("side").toString();
        td.price = o.value("price").toDouble();
        td.amount = o.value("qty").toDouble();
        td.cost = td.price * td.amount;
        td.timestamp = parse_iso_to_ms(o.value("timestamp").toString());
        if (td.price <= 0.0 || td.amount <= 0.0)
            continue;
        emit trade_received(td);
    }
}

void KrakenWsClient::handle_ohlc(const QJsonArray& data) {
    for (const auto& v : data) {
        const auto o = v.toObject();
        const QString k_sym = o.value("symbol").toString();
        if (k_sym.isEmpty())
            continue;
        Candle c;
        c.timestamp = parse_iso_to_ms(o.value("interval_begin").toString());
        c.open = o.value("open").toDouble();
        c.high = o.value("high").toDouble();
        c.low = o.value("low").toDouble();
        c.close = o.value("close").toDouble();
        c.volume = o.value("volume").toDouble();
        const int interval = static_cast<int>(o.value("interval").toDouble(1));
        if (c.timestamp == 0 || c.open <= 0.0)
            continue;
        emit candle_received(resolve_to_fincept(k_sym), timeframe_string(interval), c);
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────────

int64_t KrakenWsClient::parse_iso_to_ms(const QString& iso) {
    if (iso.isEmpty())
        return 0;
    const auto dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (dt.isValid())
        return dt.toMSecsSinceEpoch();
    // Fall back to plain ISO without ms.
    const auto dt2 = QDateTime::fromString(iso, Qt::ISODate);
    return dt2.isValid() ? dt2.toMSecsSinceEpoch() : 0;
}

} // namespace fincept::trading::kraken
