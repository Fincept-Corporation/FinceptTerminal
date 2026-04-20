#include "trading/ExchangeSession.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/secure/SecureStorage.h"
#include "trading/BrokerRegistry.h"
#include "trading/ExchangeDaemonPool.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QPointer>
#include <QTimer>

namespace fincept::trading {

namespace {
const QString kSessionTag = "ExchangeSession";

// SecureStorage keys for per-exchange crypto credentials. Keeping three
// separate keys (rather than one JSON blob) matches the existing `auth`
// module convention and lets operators rotate fields independently.
QString creds_key(const QString& exchange, const QString& field) {
    return QStringLiteral("crypto:") + exchange + QLatin1Char(':') + field;
}

QString session_resolve_script_path(const QString& relative) {
    const QString dir = fincept::python::PythonRunner::instance().scripts_dir();
    if (dir.isEmpty())
        return {};
    return dir + "/" + relative;
}

bool session_is_broker_stream(const QString& id) {
    return fincept::trading::BrokerRegistry::instance().has(id);
}

QStringList session_to_broker_symbol_args(const QStringList& symbols) {
    QStringList out;
    out.reserve(symbols.size());
    for (const auto& s : symbols) {
        QString sym = s.trimmed();
        if (sym.isEmpty())
            continue;
        QString exchange = "NSE";
        if (sym.contains(':')) {
            exchange = sym.section(':', 0, 0).trimmed().toUpper();
            sym = sym.section(':', 1).trimmed();
        }
        out << (sym + ":" + exchange + ":0");
    }
    return out;
}
} // namespace

ExchangeSession::ExchangeSession(const QString& exchange_id, SessionPublisher publisher, QObject* parent)
    : QObject(parent), exchange_id_(exchange_id), publisher_(std::move(publisher)) {
    // Phase 5: rehydrate any previously-saved credentials for this exchange
    // so users don't have to re-enter API keys on every launch.
    load_stored_credentials();
}

ExchangeSession::~ExchangeSession() {
    stop_ws();
}

// ── Credentials ────────────────────────────────────────────────────────────

void ExchangeSession::set_credentials(const ExchangeCredentials& creds) {
    {
        QMutexLocker lock(&mutex_);
        credentials_ = creds;
    }
    // Tell the shared daemon pool to resend on next call (it fingerprints
    // per-exchange, so no wasted writes if the key didn't actually change).
    ExchangeDaemonPool::instance().forget_credentials(exchange_id_);

    // Persist to OS-native encrypted storage so the next launch recovers
    // these without prompting. Empty fields map to a remove() so clearing
    // credentials actually wipes them from disk. A failure to persist is
    // non-fatal — the in-memory credentials still drive this session.
    auto& ss = fincept::SecureStorage::instance();
    const QString k_api = creds_key(exchange_id_, "api_key");
    const QString k_sec = creds_key(exchange_id_, "secret");
    const QString k_pw = creds_key(exchange_id_, "password");

    auto put_or_remove = [&](const QString& key, const QString& value) {
        if (value.isEmpty()) {
            (void) ss.remove(key);
        } else {
            auto r = ss.store(key, value);
            if (r.is_err())
                LOG_WARN(kSessionTag, QString("SecureStorage: failed to persist %1").arg(key));
        }
    };
    put_or_remove(k_api, creds.api_key);
    put_or_remove(k_sec, creds.secret);
    put_or_remove(k_pw, creds.password);

    LOG_INFO(kSessionTag, QString("Credentials updated for %1 (persisted=%2)")
                              .arg(exchange_id_, creds.api_key.isEmpty() ? "no" : "yes"));
}

void ExchangeSession::load_stored_credentials() {
    auto& ss = fincept::SecureStorage::instance();
    ExchangeCredentials next;
    {
        QMutexLocker lock(&mutex_);
        next = credentials_;
    }
    const auto r_api = ss.retrieve(creds_key(exchange_id_, "api_key"));
    if (r_api.is_ok() && !r_api.value().isEmpty())
        next.api_key = r_api.value();
    const auto r_sec = ss.retrieve(creds_key(exchange_id_, "secret"));
    if (r_sec.is_ok() && !r_sec.value().isEmpty())
        next.secret = r_sec.value();
    const auto r_pw = ss.retrieve(creds_key(exchange_id_, "password"));
    if (r_pw.is_ok() && !r_pw.value().isEmpty())
        next.password = r_pw.value();

    if (next.api_key.isEmpty() && next.secret.isEmpty() && next.password.isEmpty())
        return; // nothing stored — leave session state as-is

    {
        QMutexLocker lock(&mutex_);
        credentials_ = next;
    }
    // Invalidate any cached "already sent" state on the daemon so the next
    // RPC picks up the restored keys.
    ExchangeDaemonPool::instance().forget_credentials(exchange_id_);
    LOG_INFO(kSessionTag, "Restored stored credentials for " + exchange_id_);
}

ExchangeCredentials ExchangeSession::get_credentials() const {
    QMutexLocker lock(&mutex_);
    return credentials_;
}

// ── Daemon RPC helper ──────────────────────────────────────────────────────

QJsonObject ExchangeSession::daemon_call(const QString& method, const QJsonObject& args, int timeout_ms) {
    ExchangeCredentials creds;
    {
        QMutexLocker lock(&mutex_);
        creds = credentials_;
    }
    return ExchangeDaemonPool::instance().call(exchange_id_, method, args, creds, timeout_ms);
}

// ── Watch management ───────────────────────────────────────────────────────

void ExchangeSession::watch_symbol(const QString& symbol, const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    watched_[symbol].insert(portfolio_id);
}

void ExchangeSession::unwatch_symbol(const QString& symbol, const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    auto it = watched_.find(symbol);
    if (it != watched_.end()) {
        it->remove(portfolio_id);
        if (it->isEmpty())
            watched_.erase(it);
    }
}

QHash<QString, QSet<QString>> ExchangeSession::snapshot_watched() const {
    QMutexLocker lock(&mutex_);
    return watched_;
}

// ── Callbacks ──────────────────────────────────────────────────────────────

// ── WS lifecycle ───────────────────────────────────────────────────────────

bool ExchangeSession::start_ws(const QString& primary_symbol, const QStringList& all_symbols) {
    stop_ws();

    ws_primary_symbol_ = primary_symbol;
    ws_all_symbols_ = all_symbols;

    const QString python_path = python::PythonRunner::instance().python_path();
    QString script_path;
    QStringList args;

    if (session_is_broker_stream(exchange_id_)) {
        script_path = session_resolve_script_path("exchange/broker_ws_bridge.py");
        if (python_path.isEmpty() || script_path.isEmpty()) {
            LOG_ERROR(kSessionTag, "Python or broker_ws_bridge.py not found");
            return false;
        }
        auto* broker = BrokerRegistry::instance().get(exchange_id_);
        if (!broker) {
            LOG_ERROR(kSessionTag, "Broker stream requested but broker is not registered: " + exchange_id_);
            return false;
        }
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty() || creds.access_token.isEmpty()) {
            LOG_ERROR(kSessionTag, "Broker stream credentials missing for " + exchange_id_);
            return false;
        }
        QString feed_token;
        QString user_id = creds.user_id;
        if (!creds.additional_data.isEmpty()) {
            const auto ad = QJsonDocument::fromJson(creds.additional_data.toUtf8()).object();
            if (user_id.isEmpty())
                user_id = ad.value("client_code").toString();
            feed_token = ad.value("feed_token").toString();
        }
        if (user_id.isEmpty())
            user_id = creds.api_key;

        QStringList broker_symbols = session_to_broker_symbol_args(all_symbols);
        if (broker_symbols.isEmpty())
            broker_symbols = session_to_broker_symbol_args({primary_symbol});
        if (broker_symbols.isEmpty()) {
            LOG_ERROR(kSessionTag, "No symbols provided for broker websocket stream");
            return false;
        }
        args << "-u" << "-B" << script_path << exchange_id_ << creds.api_key << creds.access_token << user_id;
        if (!feed_token.isEmpty())
            args << "--feed-token" << feed_token;
        args << "--symbols";
        args.append(broker_symbols);
    } else {
        script_path = session_resolve_script_path("exchange/ws_stream.py");
        if (python_path.isEmpty() || script_path.isEmpty()) {
            LOG_ERROR(kSessionTag, "Python or ws_stream.py not found");
            return false;
        }
        args << "-u" << "-B" << script_path << exchange_id_;
        for (const auto& sym : all_symbols)
            args << sym;
    }

    ws_process_ = new QProcess(this);
    connect(ws_process_, &QProcess::readyReadStandardOutput, this, &ExchangeSession::drain_ws_buffer);
    connect(ws_process_, &QProcess::readyReadStandardError, this, [this]() {
        if (!ws_process_)
            return;
        const QString err = QString::fromUtf8(ws_process_->readAllStandardError()).trimmed();
        if (!err.isEmpty())
            LOG_WARN(kSessionTag, QString("[%1] WS stderr: %2").arg(exchange_id_, err));
    });
    connect(ws_process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        LOG_ERROR(kSessionTag, QString("[%1] WS process errorOccurred: %2")
                                    .arg(exchange_id_)
                                    .arg(static_cast<int>(err)));
        ws_connected_ = false;
    });

    ws_process_->start(python_path, args);
    LOG_INFO(kSessionTag, QString("WS stream start requested for %1/%2").arg(exchange_id_, primary_symbol));
    return true;
}

bool ExchangeSession::is_ws_active() const {
    return ws_process_ != nullptr && ws_process_->state() != QProcess::NotRunning;
}

void ExchangeSession::stop_ws() {
    if (!ws_process_)
        return;
    auto* proc = ws_process_;
    ws_process_ = nullptr;
    ws_connected_ = false;
    proc->disconnect(this);
    proc->terminate();
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QObject::deleteLater);
    QTimer::singleShot(3000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning)
            proc->kill();
    });
}

void ExchangeSession::set_ws_primary_symbol(const QString& symbol) {
    QMutexLocker lock(&mutex_);
    ws_primary_symbol_ = symbol;
}

QString ExchangeSession::get_ws_primary_symbol() const {
    QMutexLocker lock(&mutex_);
    return ws_primary_symbol_;
}

// ── WS line handler (ported from ExchangeService::handle_ws_line) ──────────

QString ExchangeSession::remap_symbol(const QString& exchange_symbol) const {
    QMutexLocker lock(&mutex_);
    auto it = ws_symbol_map_.constFind(exchange_symbol);
    return (it == ws_symbol_map_.constEnd()) ? exchange_symbol : it.value();
}

void ExchangeSession::handle_ws_line(const QString& line) {
    if (line.isEmpty() || line[0] != '{')
        return;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(line.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError)
        return;
    const auto msg = doc.object();
    const QString type = msg.value("type").toString();

    if (type == "status") {
        const bool connected = msg.value("connected").toBool(false);
        const bool prev = ws_connected_.exchange(connected);
        if (connected != prev) {
            LOG_INFO(kSessionTag,
                     QString("[%1] WS status: %2").arg(exchange_id_, connected ? "CONNECTED" : "DISCONNECTED"));
        }
        if (!connected) {
            QMutexLocker lock(&mutex_);
            ws_symbol_map_.clear();
        }
        return;
    }

    if (type == "symbol_map") {
        QMutexLocker lock(&mutex_);
        ws_symbol_map_.clear();
        const auto map_obj = msg.value("map").toObject();
        for (auto it = map_obj.constBegin(); it != map_obj.constEnd(); ++it)
            ws_symbol_map_[it.key()] = it.value().toString();
        LOG_INFO(kSessionTag,
                 QString("[%1] Symbol map updated: %2 remaps").arg(exchange_id_).arg(ws_symbol_map_.size()));
        return;
    }

    if (type == "ticker" || type == "tick") {
        TickerData ticker;
        if (type == "tick") {
            ticker.symbol = msg.value("symbol").toString();
            ticker.last = msg.value("ltp").toDouble();
            ticker.bid = msg.value("bid").toDouble();
            ticker.ask = msg.value("ask").toDouble();
            ticker.high = msg.value("high").toDouble();
            ticker.low = msg.value("low").toDouble();
            ticker.open = msg.value("open").toDouble();
            ticker.close = msg.value("close").toDouble();
            ticker.base_volume = msg.value("volume").toDouble();
            ticker.timestamp = msg.value("timestamp").toVariant().toLongLong();
        } else {
            QJsonObject normalised = msg;
            if (!msg.contains("baseVolume") && msg.contains("base_volume"))
                normalised["baseVolume"] = msg.value("base_volume");
            if (!msg.contains("quoteVolume") && msg.contains("quote_volume"))
                normalised["quoteVolume"] = msg.value("quote_volume");
            ticker = parse_ticker(normalised);
        }
        if (ticker.symbol.isEmpty() || ticker.last <= 0.0)
            return;
        // Remap exchange_symbol → requested_symbol so downstream consumers
        // (cache, hub topics) all key by the symbol the caller asked for.
        // e.g. on Hyperliquid: "BTC/USDC:USDC" → "BTC/USDT".
        ticker.symbol = remap_symbol(ticker.symbol);
        {
            QMutexLocker lock(&mutex_);
            price_cache_[ticker.symbol] = ticker;
        }
        if (publisher_.publish_ticker)
            publisher_.publish_ticker(exchange_id_, ticker.symbol, ticker);
        return;
    }

    if (type == "orderbook") {
        OrderBookData ob = parse_orderbook(msg);
        // Apply the same exchange→requested symbol remap as tickers, so
        // per-symbol topics are consistent across all WS message types.
        ob.symbol = remap_symbol(ob.symbol);
        if (publisher_.publish_orderbook)
            publisher_.publish_orderbook(exchange_id_, ob.symbol, ob);
        return;
    }

    if (type == "candle" || type == "ohlcv") {
        QJsonObject candle_obj = (type == "ohlcv") ? msg.value("candle").toObject() : msg;
        Candle c = parse_candle(candle_obj);
        const QString raw_sym = msg.value("symbol").toString();
        if (raw_sym.isEmpty() || c.timestamp == 0)
            return;
        const QString sym = remap_symbol(raw_sym);
        const QString interval = msg.value("interval").toString("1m");
        if (publisher_.publish_candle)
            publisher_.publish_candle(exchange_id_, sym, interval, c);
        return;
    }

    if (type == "trade") {
        TradeData td;
        td.symbol = remap_symbol(msg.value("symbol").toString());
        td.side = msg.value("side").toString();
        td.price = msg.value("price").toDouble();
        td.amount = msg.value("amount").toDouble();
        td.timestamp = msg.value("timestamp").toVariant().toLongLong();
        if (td.symbol.isEmpty())
            return;

        // Fast-path ticker update from trade — on exchanges where watch_ticker
        // is slow (e.g. Kraken spot ~2/15s), trade flow keeps price_cache_
        // current. Publish a synthetic ticker alongside so the hub's
        // ticker:<pair> topic stays fresh.
        TickerData fast_ticker;
        bool emit_ticker = false;
        if (td.price > 0.0) {
            QMutexLocker lock(&mutex_);
            auto& cached = price_cache_[td.symbol];
            cached.symbol = td.symbol;
            cached.last = td.price;
            if (cached.bid <= 0.0)
                cached.bid = td.price;
            if (cached.ask <= 0.0)
                cached.ask = td.price;
            fast_ticker = cached;
            emit_ticker = true;
        }

        if (publisher_.publish_trade)
            publisher_.publish_trade(exchange_id_, td.symbol, td);
        if (emit_ticker && publisher_.publish_ticker)
            publisher_.publish_ticker(exchange_id_, fast_ticker.symbol, fast_ticker);
    }
}

void ExchangeSession::drain_ws_buffer() {
    if (!ws_process_)
        return;
    int processed = 0;
    constexpr int MAX_LINES_PER_CYCLE = 50;
    constexpr qint64 MAX_MS_PER_CYCLE = 4;
    QElapsedTimer elapsed;
    elapsed.start();
    while (ws_process_->canReadLine() && processed < MAX_LINES_PER_CYCLE) {
        const QString line = QString::fromUtf8(ws_process_->readLine()).trimmed();
        if (!line.isEmpty())
            handle_ws_line(line);
        ++processed;
        if (elapsed.elapsed() >= MAX_MS_PER_CYCLE)
            break;
    }
    if (ws_process_ && ws_process_->canReadLine()) {
        QPointer<ExchangeSession> self = this;
        QTimer::singleShot(1, this, [self]() {
            if (self) self->drain_ws_buffer();
        });
    }
}

// ── Parsers ────────────────────────────────────────────────────────────────

TickerData ExchangeSession::parse_ticker(const QJsonObject& j) {
    TickerData t;
    t.symbol = j.value("symbol").toString();
    t.last = j.value("last").toDouble();
    t.bid = j.value("bid").toDouble();
    t.ask = j.value("ask").toDouble();
    t.high = j.value("high").toDouble();
    t.low = j.value("low").toDouble();
    t.open = j.value("open").toDouble();
    t.close = j.value("close").toDouble();
    t.change = j.value("change").toDouble();
    t.percentage = j.value("percentage").toDouble();
    t.base_volume = j.value("baseVolume").toDouble();
    t.quote_volume = j.value("quoteVolume").toDouble();
    t.timestamp = j.value("timestamp").toVariant().toLongLong();
    return t;
}

OrderBookData ExchangeSession::parse_orderbook(const QJsonObject& j) {
    OrderBookData ob;
    ob.symbol = j.value("symbol").toString();
    const auto bids = j.value("bids").toArray();
    for (const auto& b : bids) {
        const auto arr = b.toArray();
        if (arr.size() >= 2)
            ob.bids.append({arr[0].toDouble(), arr[1].toDouble()});
    }
    const auto asks = j.value("asks").toArray();
    for (const auto& a : asks) {
        const auto arr = a.toArray();
        if (arr.size() >= 2)
            ob.asks.append({arr[0].toDouble(), arr[1].toDouble()});
    }
    if (!ob.bids.isEmpty())
        ob.best_bid = ob.bids.first().first;
    if (!ob.asks.isEmpty())
        ob.best_ask = ob.asks.first().first;
    if (ob.best_bid > 0 && ob.best_ask > 0) {
        ob.spread = ob.best_ask - ob.best_bid;
        ob.spread_pct = (ob.spread / ob.best_ask) * 100.0;
    }
    return ob;
}

Candle ExchangeSession::parse_candle(const QJsonObject& j) {
    Candle c;
    c.timestamp = j.value("timestamp").toVariant().toLongLong();
    c.open = j.value("open").toDouble();
    c.high = j.value("high").toDouble();
    c.low = j.value("low").toDouble();
    c.close = j.value("close").toDouble();
    c.volume = j.value("volume").toDouble();
    return c;
}

MarketInfo ExchangeSession::parse_market(const QJsonObject& j) {
    MarketInfo m;
    m.symbol = j.value("symbol").toString();
    m.base = j.value("base").toString();
    m.quote = j.value("quote").toString();
    m.type = j.value("type").toString();
    m.active = j.value("active").toBool(true);
    m.maker_fee = j.value("maker").toDouble();
    m.taker_fee = j.value("taker").toDouble();
    m.min_amount = j.value("minAmount").toDouble();
    m.min_cost = j.value("minCost").toDouble();
    return m;
}

// ── One-shot REST fetches — all via the shared daemon pool ─────────────────

TickerData ExchangeSession::fetch_ticker(const QString& symbol) {
    const auto j = daemon_call("fetch_ticker", {{"symbol", symbol}});
    if (j.contains("error"))
        return {};
    return parse_ticker(j);
}

QVector<TickerData> ExchangeSession::fetch_tickers(const QStringList& symbols) {
    QJsonArray sym_arr;
    for (const auto& s : symbols)
        sym_arr.append(s);
    const auto j = daemon_call("fetch_tickers", {{"symbols", sym_arr}});
    QVector<TickerData> result;
    if (j.contains("error") || !j.contains("tickers"))
        return result;
    const auto arr = j.value("tickers").toArray();
    for (const auto& item : arr)
        result.append(parse_ticker(item.toObject()));
    return result;
}

OrderBookData ExchangeSession::fetch_orderbook(const QString& symbol, int limit) {
    const auto j = daemon_call("fetch_orderbook", {{"symbol", symbol}, {"limit", limit}});
    if (j.contains("error"))
        return {};
    return parse_orderbook(j);
}

QVector<Candle> ExchangeSession::fetch_ohlcv(const QString& symbol, const QString& timeframe, int limit) {
    const auto j = daemon_call("fetch_ohlcv", {{"symbol", symbol}, {"timeframe", timeframe}, {"limit", limit}});
    QVector<Candle> result;
    if (j.contains("error") || !j.contains("candles"))
        return result;
    const auto arr = j.value("candles").toArray();
    for (const auto& item : arr)
        result.append(parse_candle(item.toObject()));
    return result;
}

QVector<MarketInfo> ExchangeSession::fetch_markets(const QString& type, const QString& query) {
    QJsonObject args;
    if (!type.isEmpty())
        args["type"] = type;
    if (!query.isEmpty()) {
        args["query"] = query;
        args["limit"] = 100;
    }
    const auto j = daemon_call("fetch_markets", args);
    QVector<MarketInfo> result;
    if (j.contains("error") || !j.contains("markets"))
        return result;
    const auto arr = j.value("markets").toArray();
    for (const auto& item : arr)
        result.append(parse_market(item.toObject()));
    return result;
}

QVector<TradeData> ExchangeSession::fetch_trades(const QString& symbol, int limit) {
    const auto j = daemon_call("fetch_trades", {{"symbol", symbol}, {"limit", limit}});
    QVector<TradeData> result;
    if (j.contains("error") || !j.contains("trades"))
        return result;
    const auto arr = j.value("trades").toArray();
    for (const auto& item : arr) {
        const auto o = item.toObject();
        TradeData td;
        td.id = o.value("id").toString();
        td.symbol = o.value("symbol").toString();
        td.side = o.value("side").toString();
        td.price = o.value("price").toDouble();
        td.amount = o.value("amount").toDouble();
        td.cost = o.value("cost").toDouble();
        td.timestamp = o.value("timestamp").toVariant().toLongLong();
        result.append(td);
    }
    return result;
}

FundingRateData ExchangeSession::fetch_funding_rate(const QString& symbol) {
    const auto j = daemon_call("fetch_funding_rate", {{"symbol", symbol}});
    FundingRateData fr;
    if (!j.contains("error")) {
        fr.symbol = j.value("symbol").toString();
        fr.funding_rate = j.value("funding_rate").toDouble();
        fr.mark_price = j.value("mark_price").toDouble();
        fr.index_price = j.value("index_price").toDouble();
        fr.funding_timestamp = j.value("funding_timestamp").toVariant().toLongLong();
        fr.next_funding_timestamp = j.value("next_funding_timestamp").toVariant().toLongLong();
    }
    return fr;
}

OpenInterestData ExchangeSession::fetch_open_interest(const QString& symbol) {
    const auto j = daemon_call("fetch_open_interest", {{"symbol", symbol}});
    OpenInterestData oi;
    if (!j.contains("error")) {
        oi.symbol = j.value("symbol").toString();
        oi.open_interest = j.value("open_interest").toDouble();
        oi.open_interest_value = j.value("open_interest_value").toDouble();
        oi.timestamp = j.value("timestamp").toVariant().toLongLong();
    }
    return oi;
}

MarkPriceData ExchangeSession::fetch_mark_price(const QString& symbol) {
    const auto j = daemon_call("fetch_mark_price", {{"symbol", symbol}});
    MarkPriceData mp;
    mp.symbol = symbol;
    if (!j.contains("error")) {
        mp.mark_price = j.value("mark_price").toDouble();
        mp.index_price = j.value("index_price").toDouble();
        mp.timestamp = j.value("timestamp").toVariant().toLongLong();
    }
    return mp;
}

QJsonObject ExchangeSession::fetch_balance() {
    return daemon_call("fetch_balance", {});
}

QJsonObject ExchangeSession::place_exchange_order(const QString& symbol, const QString& side, const QString& type,
                                                  double amount, double price) {
    QJsonObject args;
    args["symbol"] = symbol;
    args["side"] = side;
    args["type"] = type;
    args["amount"] = amount;
    if (price > 0)
        args["price"] = price;
    return daemon_call("place_order", args);
}

QJsonObject ExchangeSession::cancel_exchange_order(const QString& order_id, const QString& symbol) {
    return daemon_call("cancel_order", {{"order_id", order_id}, {"symbol", symbol}});
}

QJsonObject ExchangeSession::fetch_positions_live(const QString& symbol) {
    QJsonObject args;
    if (!symbol.isEmpty())
        args["symbol"] = symbol;
    return daemon_call("fetch_positions", args);
}

QJsonObject ExchangeSession::fetch_open_orders_live(const QString& symbol) {
    QJsonObject args;
    if (!symbol.isEmpty())
        args["symbol"] = symbol;
    return daemon_call("fetch_open_orders", args);
}

QJsonObject ExchangeSession::fetch_my_trades(const QString& symbol, int limit) {
    return daemon_call("fetch_my_trades", {{"symbol", symbol}, {"limit", limit}});
}

QJsonObject ExchangeSession::fetch_trading_fees(const QString& symbol) {
    QJsonObject args;
    if (!symbol.isEmpty())
        args["symbol"] = symbol;
    return daemon_call("fetch_trading_fees", args);
}

QJsonObject ExchangeSession::set_leverage(const QString& symbol, int leverage) {
    return daemon_call("set_leverage", {{"symbol", symbol}, {"leverage", leverage}});
}

QJsonObject ExchangeSession::set_margin_mode(const QString& symbol, const QString& mode) {
    return daemon_call("set_margin_mode", {{"symbol", symbol}, {"mode", mode}});
}

// ── Cache accessors ────────────────────────────────────────────────────────

TickerData ExchangeSession::get_cached_price(const QString& symbol) const {
    QMutexLocker lock(&mutex_);
    return price_cache_.value(symbol);
}

QHash<QString, TickerData> ExchangeSession::snapshot_price_cache() const {
    QMutexLocker lock(&mutex_);
    return price_cache_;
}

} // namespace fincept::trading
