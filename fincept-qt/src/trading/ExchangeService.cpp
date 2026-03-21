// ExchangeService — Bridge between Python ccxt and C++ paper trading engine
// Uses QProcess for subprocess management

#include "trading/ExchangeService.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "python/PythonRunner.h"
#include "core/logging/Logger.h"
#include "core/events/EventBus.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutexLocker>
#include <QCoreApplication>

// Adapter: PythonRunner uses scripts_dir(), not resolve_script()
namespace {
QString resolve_script_path(const QString& relative) {
    QString dir = fincept::python::PythonRunner::instance().scripts_dir();
    if (dir.isEmpty()) return {};
    return dir + "/" + relative;
}
} // anonymous namespace

namespace fincept::trading {

static const QString TAG = "ExchangeService";

// ============================================================================
// Singleton
// ============================================================================

ExchangeService& ExchangeService::instance() {
    static ExchangeService s;
    return s;
}

ExchangeService::ExchangeService() {
    feed_timer_ = new QTimer(this);
    connect(feed_timer_, &QTimer::timeout, this, &ExchangeService::poll_prices);
}

ExchangeService::~ExchangeService() {
    stop_ws_stream();
    stop_price_feed();
}

// ============================================================================
// Configuration
// ============================================================================

void ExchangeService::set_exchange(const QString& exchange_id) {
    QMutexLocker lock(&mutex_);
    exchange_id_ = exchange_id;
    LOG_INFO(TAG, "Exchange set to: " + exchange_id);
}

QString ExchangeService::get_exchange() const {
    QMutexLocker lock(&mutex_);
    return exchange_id_;
}

void ExchangeService::set_credentials(const ExchangeCredentials& creds) {
    QMutexLocker lock(&mutex_);
    credentials_ = creds;
}

// ============================================================================
// Watch management
// ============================================================================

void ExchangeService::watch_symbol(const QString& symbol, const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    watched_[symbol].insert(portfolio_id);
}

void ExchangeService::unwatch_symbol(const QString& symbol, const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    auto it = watched_.find(symbol);
    if (it != watched_.end()) {
        it->remove(portfolio_id);
        if (it->isEmpty()) watched_.erase(it);
    }
}

// ============================================================================
// Price Feed (polling via QTimer)
// ============================================================================

void ExchangeService::start_price_feed(int interval_seconds) {
    feed_interval_ = interval_seconds;
    feed_running_ = true;
    feed_timer_->start(interval_seconds * 1000);
    LOG_INFO(TAG, QString("Price feed started (interval=%1s)").arg(interval_seconds));
}

void ExchangeService::stop_price_feed() {
    feed_running_ = false;
    if (feed_timer_) feed_timer_->stop();
}

bool ExchangeService::is_feed_running() const {
    return feed_running_.load();
}

void ExchangeService::poll_prices() {
    QMutexLocker lock(&mutex_);
    if (watched_.isEmpty()) return;

    QStringList symbols;
    for (auto it = watched_.constBegin(); it != watched_.constEnd(); ++it) {
        symbols.append(it.key());
    }
    lock.unlock();

    // Fetch tickers for watched symbols
    auto tickers = fetch_tickers(symbols);
    for (const auto& ticker : tickers) {
        QMutexLocker lock2(&mutex_);
        price_cache_[ticker.symbol] = ticker;

        // Feed OrderMatcher for each portfolio watching this symbol
        auto it = watched_.find(ticker.symbol);
        if (it != watched_.end()) {
            PriceData pd;
            pd.last = ticker.last;
            pd.bid = ticker.bid;
            pd.ask = ticker.ask;
            pd.high = ticker.high;
            pd.low = ticker.low;
            pd.volume = ticker.base_volume;
            pd.change = ticker.change;
            pd.change_percent = ticker.percentage;
            pd.timestamp = ticker.timestamp;

            for (const auto& portfolio_id : *it) {
                OrderMatcher::instance().check_orders(ticker.symbol, pd, portfolio_id);
                OrderMatcher::instance().check_sl_tp_triggers(portfolio_id, ticker.symbol, ticker.last);
                pt_update_position_price(portfolio_id, ticker.symbol, ticker.last);
            }
        }

        // Notify price callbacks
        for (auto cb_it = price_callbacks_.constBegin(); cb_it != price_callbacks_.constEnd(); ++cb_it) {
            try { cb_it.value()(ticker.symbol, ticker); } catch (...) {}
        }
    }
}

// ============================================================================
// Callbacks
// ============================================================================

int ExchangeService::on_price_update(PriceUpdateCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    price_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_price_callback(int id) {
    QMutexLocker lock(&mutex_);
    price_callbacks_.remove(id);
}

int ExchangeService::on_orderbook_update(OrderBookCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    orderbook_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_orderbook_callback(int id) {
    QMutexLocker lock(&mutex_);
    orderbook_callbacks_.remove(id);
}

int ExchangeService::on_candle_update(CandleCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    candle_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_candle_callback(int id) {
    QMutexLocker lock(&mutex_);
    candle_callbacks_.remove(id);
}

int ExchangeService::on_trade_update(TradeCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    trade_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_trade_callback(int id) {
    QMutexLocker lock(&mutex_);
    trade_callbacks_.remove(id);
}

// ============================================================================
// WebSocket Stream (QProcess-based)
// ============================================================================

void ExchangeService::start_ws_stream(const QString& primary_symbol,
                                       const QStringList& all_symbols) {
    stop_ws_stream();

    ws_primary_symbol_ = primary_symbol;
    ws_all_symbols_ = all_symbols;

    // Find Python and script paths via PythonRunner
    QString python_path = python::PythonRunner::instance().python_path();
    QString script_path = resolve_script_path("exchange/ws_stream.py");

    if (python_path.isEmpty() || script_path.isEmpty()) {
        LOG_ERROR(TAG, "Python or ws_stream.py not found");
        return;
    }

    QStringList args;
    args << "-u" << "-B" << script_path << exchange_id_;
    args << "--symbols";
    for (const auto& sym : all_symbols) args << sym;

    ws_process_ = new QProcess(this);
    connect(ws_process_, &QProcess::readyReadStandardOutput, this, [this]() {
        while (ws_process_->canReadLine()) {
            QString line = QString::fromUtf8(ws_process_->readLine()).trimmed();
            if (!line.isEmpty()) handle_ws_line(line);
        }
    });
    connect(ws_process_, &QProcess::readyReadStandardError, this, [this]() {
        QString err = QString::fromUtf8(ws_process_->readAllStandardError()).trimmed();
        if (!err.isEmpty()) LOG_WARN(TAG, "WS stderr: " + err);
    });

    ws_process_->start(python_path, args);
    LOG_INFO(TAG, "WS stream started for " + primary_symbol);
}

void ExchangeService::stop_ws_stream() {
    if (ws_process_) {
        ws_process_->terminate();
        ws_process_->waitForFinished(3000);
        if (ws_process_->state() != QProcess::NotRunning) {
            ws_process_->kill();
        }
        ws_process_->deleteLater();
        ws_process_ = nullptr;
    }
    ws_connected_ = false;
}

bool ExchangeService::is_ws_connected() const {
    return ws_connected_.load();
}

void ExchangeService::set_ws_primary_symbol(const QString& symbol) {
    QMutexLocker lock(&mutex_);
    ws_primary_symbol_ = symbol;
}

QString ExchangeService::get_ws_primary_symbol() const {
    QMutexLocker lock(&mutex_);
    return ws_primary_symbol_;
}

void ExchangeService::handle_ws_line(const QString& line) {
    if (line.isEmpty() || line[0] != '{') return;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(line.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) return;

    auto msg = doc.object();
    QString type = msg.value("type").toString();

    if (type == "status") {
        ws_connected_ = msg.value("connected").toBool(false);
        return;
    }

    if (type == "ticker") {
        TickerData ticker = parse_ticker(msg);
        if (ticker.symbol.isEmpty() || ticker.last <= 0.0) return;

        QMutexLocker lock(&mutex_);
        price_cache_[ticker.symbol] = ticker;

        for (auto cb_it = price_callbacks_.constBegin(); cb_it != price_callbacks_.constEnd(); ++cb_it) {
            try { cb_it.value()(ticker.symbol, ticker); } catch (...) {}
        }
        return;
    }

    if (type == "orderbook") {
        OrderBookData ob = parse_orderbook(msg);
        for (auto cb_it = orderbook_callbacks_.constBegin(); cb_it != orderbook_callbacks_.constEnd(); ++cb_it) {
            try { cb_it.value()(ob.symbol, ob); } catch (...) {}
        }
        return;
    }

    if (type == "candle") {
        Candle c = parse_candle(msg);
        QString sym = msg.value("symbol").toString();
        for (auto cb_it = candle_callbacks_.constBegin(); cb_it != candle_callbacks_.constEnd(); ++cb_it) {
            try { cb_it.value()(sym, c); } catch (...) {}
        }
        return;
    }

    if (type == "trade") {
        TradeData td;
        td.symbol    = msg.value("symbol").toString();
        td.side      = msg.value("side").toString();
        td.price     = msg.value("price").toDouble();
        td.amount    = msg.value("amount").toDouble();
        td.timestamp = msg.value("timestamp").toVariant().toLongLong();
        for (auto cb_it = trade_callbacks_.constBegin(); cb_it != trade_callbacks_.constEnd(); ++cb_it) {
            try { cb_it.value()(td.symbol, td); } catch (...) {}
        }
    }
}

// ============================================================================
// Python Script Calls
// ============================================================================

QJsonObject ExchangeService::call_script(const QString& script, const QStringList& args) {
    QString python_path = python::PythonRunner::instance().python_path();
    QString script_path = resolve_script_path(script);

    if (python_path.isEmpty() || script_path.isEmpty()) {
        return {{"error", "Python or script not found: " + script}};
    }

    QStringList full_args;
    full_args << "-u" << "-B" << script_path << exchange_id_;
    full_args.append(args);

    QProcess proc;
    proc.start(python_path, full_args);
    proc.waitForFinished(30000);

    QString output = proc.readAllStandardOutput().trimmed();
    if (output.isEmpty()) {
        QString err = proc.readAllStandardError().trimmed();
        return {{"error", err.isEmpty() ? "No output from script" : err}};
    }

    QJsonParseError parseErr;
    auto doc = QJsonDocument::fromJson(output.toUtf8(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        return {{"error", "JSON parse error: " + parseErr.errorString()}};
    }

    return doc.object();
}

QJsonObject ExchangeService::call_script_with_credentials(const QString& script,
                                                            const QStringList& args) {
    QStringList full_args = args;
    QMutexLocker lock(&mutex_);
    if (!credentials_.api_key.isEmpty()) {
        full_args << "--api-key" << credentials_.api_key;
        full_args << "--secret" << credentials_.secret;
        if (!credentials_.password.isEmpty()) {
            full_args << "--password" << credentials_.password;
        }
    }
    lock.unlock();
    return call_script(script, full_args);
}

// ============================================================================
// One-shot Fetches
// ============================================================================

TickerData ExchangeService::parse_ticker(const QJsonObject& j) {
    TickerData t;
    t.symbol       = j.value("symbol").toString();
    t.last         = j.value("last").toDouble();
    t.bid          = j.value("bid").toDouble();
    t.ask          = j.value("ask").toDouble();
    t.high         = j.value("high").toDouble();
    t.low          = j.value("low").toDouble();
    t.open         = j.value("open").toDouble();
    t.close        = j.value("close").toDouble();
    t.change       = j.value("change").toDouble();
    t.percentage   = j.value("percentage").toDouble();
    t.base_volume  = j.value("baseVolume").toDouble();
    t.quote_volume = j.value("quoteVolume").toDouble();
    t.timestamp    = j.value("timestamp").toVariant().toLongLong();
    return t;
}

OrderBookData ExchangeService::parse_orderbook(const QJsonObject& j) {
    OrderBookData ob;
    ob.symbol = j.value("symbol").toString();

    auto bids = j.value("bids").toArray();
    for (const auto& b : bids) {
        auto arr = b.toArray();
        if (arr.size() >= 2) {
            ob.bids.append({arr[0].toDouble(), arr[1].toDouble()});
        }
    }

    auto asks = j.value("asks").toArray();
    for (const auto& a : asks) {
        auto arr = a.toArray();
        if (arr.size() >= 2) {
            ob.asks.append({arr[0].toDouble(), arr[1].toDouble()});
        }
    }

    if (!ob.bids.isEmpty()) ob.best_bid = ob.bids.first().first;
    if (!ob.asks.isEmpty()) ob.best_ask = ob.asks.first().first;
    if (ob.best_bid > 0 && ob.best_ask > 0) {
        ob.spread = ob.best_ask - ob.best_bid;
        ob.spread_pct = (ob.spread / ob.best_ask) * 100.0;
    }
    return ob;
}

Candle ExchangeService::parse_candle(const QJsonObject& j) {
    Candle c;
    c.timestamp = j.value("timestamp").toVariant().toLongLong();
    c.open      = j.value("open").toDouble();
    c.high      = j.value("high").toDouble();
    c.low       = j.value("low").toDouble();
    c.close     = j.value("close").toDouble();
    c.volume    = j.value("volume").toDouble();
    return c;
}

MarketInfo ExchangeService::parse_market(const QJsonObject& j) {
    MarketInfo m;
    m.symbol     = j.value("symbol").toString();
    m.base       = j.value("base").toString();
    m.quote      = j.value("quote").toString();
    m.type       = j.value("type").toString();
    m.active     = j.value("active").toBool(true);
    m.maker_fee  = j.value("maker").toDouble();
    m.taker_fee  = j.value("taker").toDouble();
    m.min_amount = j.value("minAmount").toDouble();
    m.min_cost   = j.value("minCost").toDouble();
    return m;
}

TickerData ExchangeService::fetch_ticker(const QString& symbol) {
    auto j = call_script("exchange/fetch_ticker.py", {"--symbol", symbol});
    if (j.contains("error")) return {};
    return parse_ticker(j);
}

QVector<TickerData> ExchangeService::fetch_tickers(const QStringList& symbols) {
    QStringList args = {"--symbols"};
    args.append(symbols);
    auto j = call_script("exchange/fetch_tickers.py", args);
    QVector<TickerData> result;
    if (j.contains("tickers")) {
        auto arr = j.value("tickers").toArray();
        for (const auto& item : arr) {
            result.append(parse_ticker(item.toObject()));
        }
    }
    return result;
}

OrderBookData ExchangeService::fetch_orderbook(const QString& symbol, int limit) {
    auto j = call_script("exchange/fetch_orderbook.py",
                          {"--symbol", symbol, "--limit", QString::number(limit)});
    if (j.contains("error")) return {};
    return parse_orderbook(j);
}

QVector<Candle> ExchangeService::fetch_ohlcv(const QString& symbol,
                                               const QString& timeframe, int limit) {
    auto j = call_script("exchange/fetch_ohlcv.py",
                          {"--symbol", symbol, "--timeframe", timeframe,
                           "--limit", QString::number(limit)});
    QVector<Candle> result;
    if (j.contains("candles")) {
        auto arr = j.value("candles").toArray();
        for (const auto& item : arr) {
            result.append(parse_candle(item.toObject()));
        }
    }
    return result;
}

QVector<MarketInfo> ExchangeService::fetch_markets(const QString& type) {
    QStringList args;
    if (!type.isEmpty()) args << "--type" << type;
    auto j = call_script("exchange/fetch_markets.py", args);
    QVector<MarketInfo> result;
    if (j.contains("markets")) {
        auto arr = j.value("markets").toArray();
        for (const auto& item : arr) {
            result.append(parse_market(item.toObject()));
        }
    }
    return result;
}

QStringList ExchangeService::list_exchange_ids() {
    auto j = call_script("exchange/list_exchanges.py", {});
    QStringList result;
    if (j.contains("exchanges")) {
        auto arr = j.value("exchanges").toArray();
        for (const auto& item : arr) {
            result.append(item.toString());
        }
    }
    return result;
}

QVector<TradeData> ExchangeService::fetch_trades(const QString& symbol, int limit) {
    auto j = call_script("exchange/fetch_trades.py",
                          {"--symbol", symbol, "--limit", QString::number(limit)});
    QVector<TradeData> result;
    if (j.contains("trades")) {
        auto arr = j.value("trades").toArray();
        for (const auto& item : arr) {
            auto o = item.toObject();
            TradeData td;
            td.id        = o.value("id").toString();
            td.symbol    = o.value("symbol").toString();
            td.side      = o.value("side").toString();
            td.price     = o.value("price").toDouble();
            td.amount    = o.value("amount").toDouble();
            td.cost      = o.value("cost").toDouble();
            td.timestamp = o.value("timestamp").toVariant().toLongLong();
            result.append(td);
        }
    }
    return result;
}

FundingRateData ExchangeService::fetch_funding_rate(const QString& symbol) {
    auto j = call_script("exchange/fetch_funding_rate.py", {"--symbol", symbol});
    FundingRateData fr;
    if (!j.contains("error")) {
        fr.symbol                 = j.value("symbol").toString();
        fr.funding_rate           = j.value("fundingRate").toDouble();
        fr.mark_price             = j.value("markPrice").toDouble();
        fr.index_price            = j.value("indexPrice").toDouble();
        fr.funding_timestamp      = j.value("fundingTimestamp").toVariant().toLongLong();
        fr.next_funding_timestamp = j.value("nextFundingTimestamp").toVariant().toLongLong();
    }
    return fr;
}

OpenInterestData ExchangeService::fetch_open_interest(const QString& symbol) {
    auto j = call_script("exchange/fetch_open_interest.py", {"--symbol", symbol});
    OpenInterestData oi;
    if (!j.contains("error")) {
        oi.symbol              = j.value("symbol").toString();
        oi.open_interest       = j.value("openInterest").toDouble();
        oi.open_interest_value = j.value("openInterestValue").toDouble();
        oi.timestamp           = j.value("timestamp").toVariant().toLongLong();
    }
    return oi;
}

QJsonObject ExchangeService::fetch_balance() {
    return call_script_with_credentials("exchange/fetch_balance.py", {});
}

QJsonObject ExchangeService::place_exchange_order(const QString& symbol, const QString& side,
                                                    const QString& type, double amount, double price) {
    QStringList args = {"--symbol", symbol, "--side", side, "--type", type,
                        "--amount", QString::number(amount, 'f', 8)};
    if (price > 0) args << "--price" << QString::number(price, 'f', 8);
    return call_script_with_credentials("exchange/place_order.py", args);
}

QJsonObject ExchangeService::cancel_exchange_order(const QString& order_id, const QString& symbol) {
    return call_script_with_credentials("exchange/cancel_order.py",
                                         {"--order-id", order_id, "--symbol", symbol});
}

QJsonObject ExchangeService::fetch_positions_live(const QString& symbol) {
    QStringList args;
    if (!symbol.isEmpty()) args << "--symbol" << symbol;
    return call_script_with_credentials("exchange/fetch_positions.py", args);
}

QJsonObject ExchangeService::fetch_open_orders_live(const QString& symbol) {
    QStringList args;
    if (!symbol.isEmpty()) args << "--symbol" << symbol;
    return call_script_with_credentials("exchange/fetch_open_orders.py", args);
}

const QHash<QString, TickerData>& ExchangeService::get_price_cache() const {
    return price_cache_;
}

TickerData ExchangeService::get_cached_price(const QString& symbol) const {
    QMutexLocker lock(&mutex_);
    return price_cache_.value(symbol);
}

} // namespace fincept::trading
