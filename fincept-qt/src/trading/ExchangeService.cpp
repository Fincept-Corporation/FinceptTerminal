// ExchangeService — facade over ExchangeSessionManager (Phase 2).
//
// See header for design notes. This file intentionally contains no WS /
// daemon / cache state of its own — every non-trivial call is a one-line
// forward to the active session.

#include "trading/ExchangeService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "trading/ExchangeDaemonPool.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"

#include <QMutexLocker>
#include <QPointer>
#include <QtConcurrent/QtConcurrent>

namespace fincept::trading {

namespace {
const QString kServiceTag = "ExchangeService";
} // namespace

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

    // Re-emit the pool's ready() signal as our own daemon_ready() so existing
    // consumers (e.g. CryptoTradingScreen) need no changes.
    connect(&ExchangeDaemonPool::instance(), &ExchangeDaemonPool::ready, this,
            [this]() { emit daemon_ready(); });
}

ExchangeService::~ExchangeService() {
    // Session lifecycles are owned by ExchangeSessionManager. Nothing to do.
    if (feed_timer_)
        feed_timer_->stop();
}

// ============================================================================
// Session resolution
// ============================================================================

ExchangeSession* ExchangeService::active_session() const {
    QString id;
    {
        QMutexLocker lock(&mutex_);
        id = exchange_id_;
    }
    return ExchangeSessionManager::instance().session(id);
}

// ============================================================================
// Configuration
// ============================================================================

void ExchangeService::ensure_registered_with_hub() {
    ExchangeSessionManager::instance().ensure_registered_with_hub();
}

void ExchangeService::set_exchange(const QString& exchange_id) {
    {
        QMutexLocker lock(&mutex_);
        exchange_id_ = exchange_id;
    }
    // Touch the session so it exists by the time subsequent calls arrive.
    ExchangeSessionManager::instance().session(exchange_id);
    LOG_INFO(kServiceTag, "Exchange set to: " + exchange_id);
}

QString ExchangeService::get_exchange() const {
    QMutexLocker lock(&mutex_);
    return exchange_id_;
}

void ExchangeService::set_credentials(const ExchangeCredentials& creds) {
    active_session()->set_credentials(creds);
}

bool ExchangeService::wait_for_daemon_ready(int timeout_ms) {
    return ExchangeDaemonPool::instance().wait_for_ready(timeout_ms);
}

// ============================================================================
// Watch management
// ============================================================================

void ExchangeService::watch_symbol(const QString& symbol, const QString& portfolio_id) {
    active_session()->watch_symbol(symbol, portfolio_id);
}

void ExchangeService::unwatch_symbol(const QString& symbol, const QString& portfolio_id) {
    active_session()->unwatch_symbol(symbol, portfolio_id);
}

// ============================================================================
// Price Feed (polling)
// ============================================================================

void ExchangeService::start_price_feed(int interval_seconds) {
    feed_interval_ = interval_seconds;
    feed_running_ = true;
    feed_timer_->start(interval_seconds * 1000);
    LOG_INFO(kServiceTag, QString("Price feed started (interval=%1s)").arg(interval_seconds));
}

void ExchangeService::stop_price_feed() {
    feed_running_ = false;
    if (feed_timer_)
        feed_timer_->stop();
}

bool ExchangeService::is_feed_running() const {
    return feed_running_.load();
}

void ExchangeService::poll_prices() {
    if (poll_in_progress_.exchange(true))
        return;

    ExchangeSession* sess = active_session();
    const auto watched = sess->snapshot_watched();
    if (watched.isEmpty()) {
        poll_in_progress_ = false;
        return;
    }

    QStringList symbols;
    symbols.reserve(watched.size());
    for (auto it = watched.constBegin(); it != watched.constEnd(); ++it)
        symbols.append(it.key());

    QPointer<ExchangeService> self = this;
    QPointer<ExchangeSession> session_ptr = sess;
    (void)QtConcurrent::run([self, session_ptr, symbols, watched]() {
        if (!self || !session_ptr)
            return;
        auto tickers = session_ptr->fetch_tickers(symbols);
        if (!self)
            return;

        // Feed paper-trading engine with fresh prices (worker thread — these
        // are SQLite writes; keeping them off the UI thread is P1).
        for (const auto& ticker : tickers) {
            auto it = watched.find(ticker.symbol);
            if (it == watched.end())
                continue;
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

        // Poll results land in the session's WS price_cache_ on subsequent
        // pushes from WS; we intentionally do NOT stuff them into the
        // session's cache here (the cache is authoritative for WS, polling
        // is a fallback for when WS isn't up).
        if (self)
            self->poll_in_progress_ = false;
    });
}

// ============================================================================
// WebSocket streaming — delegated
// ============================================================================

bool ExchangeService::start_ws_stream(const QString& primary_symbol, const QStringList& all_symbols) {
    if (!fincept::AppConfig::instance().crypto_markets_enabled()) {
        LOG_WARN(kServiceTag,
                 "Crypto WS stream start blocked by feature flag "
                 "(features/crypto_markets_enabled=false)");
        return false;
    }
    return active_session()->start_ws(primary_symbol, all_symbols);
}

void ExchangeService::stop_ws_stream() {
    active_session()->stop_ws();
}

bool ExchangeService::is_ws_connected() const {
    return active_session()->is_ws_connected();
}

bool ExchangeService::is_ws_active() const {
    return active_session()->is_ws_active();
}

void ExchangeService::set_ws_primary_symbol(const QString& symbol) {
    active_session()->set_ws_primary_symbol(symbol);
}

QString ExchangeService::get_ws_primary_symbol() const {
    return active_session()->get_ws_primary_symbol();
}

// ============================================================================
// One-shot fetches — delegated
// ============================================================================

TickerData ExchangeService::fetch_ticker(const QString& symbol) {
    return active_session()->fetch_ticker(symbol);
}
QVector<TickerData> ExchangeService::fetch_tickers(const QStringList& symbols) {
    return active_session()->fetch_tickers(symbols);
}
OrderBookData ExchangeService::fetch_orderbook(const QString& symbol, int limit) {
    return active_session()->fetch_orderbook(symbol, limit);
}
QVector<Candle> ExchangeService::fetch_ohlcv(const QString& symbol, const QString& timeframe, int limit) {
    return active_session()->fetch_ohlcv(symbol, timeframe, limit);
}
QVector<MarketInfo> ExchangeService::fetch_markets(const QString& type, const QString& query) {
    return active_session()->fetch_markets(type, query);
}

QStringList ExchangeService::list_exchange_ids() {
    // Exchange-agnostic daemon call — list_exchange_ids returns the ccxt
    // library's exchange directory, independent of any session.
    if (!ExchangeDaemonPool::instance().wait_for_ready(8000))
        return {};
    // Reuse the active session's exchange id as a dummy; the daemon
    // doesn't actually use it for list_exchange_ids.
    const QString exchange = get_exchange();
    auto j = ExchangeDaemonPool::instance().call(exchange, "list_exchange_ids", {}, {}, 15000);
    QStringList result;
    if (j.contains("exchanges")) {
        auto arr = j.value("exchanges").toArray();
        for (const auto& item : arr)
            result.append(item.toString());
    }
    return result;
}

QVector<TradeData> ExchangeService::fetch_trades(const QString& symbol, int limit) {
    return active_session()->fetch_trades(symbol, limit);
}
FundingRateData ExchangeService::fetch_funding_rate(const QString& symbol) {
    return active_session()->fetch_funding_rate(symbol);
}
OpenInterestData ExchangeService::fetch_open_interest(const QString& symbol) {
    return active_session()->fetch_open_interest(symbol);
}

QJsonObject ExchangeService::fetch_balance() {
    return active_session()->fetch_balance();
}
QJsonObject ExchangeService::place_exchange_order(const QString& symbol, const QString& side, const QString& type,
                                                  double amount, double price) {
    return active_session()->place_exchange_order(symbol, side, type, amount, price);
}
QJsonObject ExchangeService::cancel_exchange_order(const QString& order_id, const QString& symbol) {
    return active_session()->cancel_exchange_order(order_id, symbol);
}
QJsonObject ExchangeService::fetch_positions_live(const QString& symbol) {
    return active_session()->fetch_positions_live(symbol);
}
QJsonObject ExchangeService::fetch_open_orders_live(const QString& symbol) {
    return active_session()->fetch_open_orders_live(symbol);
}
QJsonObject ExchangeService::fetch_my_trades(const QString& symbol, int limit) {
    return active_session()->fetch_my_trades(symbol, limit);
}
QJsonObject ExchangeService::fetch_trading_fees(const QString& symbol) {
    return active_session()->fetch_trading_fees(symbol);
}
QJsonObject ExchangeService::set_leverage(const QString& symbol, int leverage) {
    return active_session()->set_leverage(symbol, leverage);
}
QJsonObject ExchangeService::set_margin_mode(const QString& symbol, const QString& mode) {
    return active_session()->set_margin_mode(symbol, mode);
}
MarkPriceData ExchangeService::fetch_mark_price(const QString& symbol) {
    return active_session()->fetch_mark_price(symbol);
}

// ============================================================================
// Cache
// ============================================================================

TickerData ExchangeService::get_cached_price(const QString& symbol) const {
    return active_session()->get_cached_price(symbol);
}

} // namespace fincept::trading
