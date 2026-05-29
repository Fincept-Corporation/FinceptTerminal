#include "trading/AccountDataStream.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/instruments/InstrumentService.h"
#include "trading/brokers/alpaca/AlpacaWebSocket.h"
#include "trading/websocket/AliceBlueWebSocket.h"
#include "trading/websocket/BrokerWebSocketBase.h"
#include "trading/websocket/DhanWebSocket.h"
#include "trading/websocket/FivePaisaWebSocket.h"
#include "trading/websocket/FyersWebSocket.h"
#include "trading/websocket/IIFLWebSocket.h"
#include "trading/websocket/KotakWebSocket.h"
#include "trading/websocket/MotilalPoller.h"
#include "trading/websocket/ShoonyaWebSocket.h"
#include "trading/websocket/UpstoxWebSocket.h"

#include <QDate>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent>

#include <algorithm>

namespace fincept::trading {

namespace {
constexpr const char* ADS_TAG = "AccountDataStream";
constexpr int ADS_QUOTE_POLL_MS = 300000;
constexpr int ADS_PORTFOLIO_POLL_MS = 300000;
constexpr int ADS_WATCHLIST_POLL_MS = 300000;
} // namespace

// ── Construction / Destruction ──────────────────────────────────────────────

AccountDataStream::AccountDataStream(const QString& account_id, QObject* parent)
    : QObject(parent), account_id_(account_id) {
    auto account = AccountManager::instance().get_account(account_id);
    broker_id_ = account.broker_id;

    // Create timers (P3: don't start in constructor, intervals only)
    quote_timer_ = new QTimer(this);
    quote_timer_->setInterval(ADS_QUOTE_POLL_MS);
    connect(quote_timer_, &QTimer::timeout, this, &AccountDataStream::on_quote_timer);

    portfolio_timer_ = new QTimer(this);
    portfolio_timer_->setInterval(ADS_PORTFOLIO_POLL_MS);
    connect(portfolio_timer_, &QTimer::timeout, this, &AccountDataStream::on_portfolio_timer);

    watchlist_timer_ = new QTimer(this);
    watchlist_timer_->setInterval(ADS_WATCHLIST_POLL_MS);
    connect(watchlist_timer_, &QTimer::timeout, this, &AccountDataStream::on_watchlist_timer);
}

AccountDataStream::~AccountDataStream() {
    stop();
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void AccountDataStream::start() {
    if (running_)
        return;
    running_ = true;

    // Try to init WebSocket if broker supports it
    ws_init();

    // Start polling (suppressed per-timer if WS active)
    if (!ws_active()) {
        quote_timer_->start();
        watchlist_timer_->start();
    }
    portfolio_timer_->start();

    // Initial data fetch
    async_fetch_quote();
    async_fetch_watchlist_quotes();
    async_fetch_positions();
    async_fetch_holdings();
    async_fetch_orders();
    async_fetch_funds();

    LOG_INFO(ADS_TAG, QString("Started stream for account %1 (%2)").arg(account_id_, broker_id_));
}

void AccountDataStream::stop() {
    if (!running_)
        return;
    running_ = false;

    quote_timer_->stop();
    portfolio_timer_->stop();
    watchlist_timer_->stop();
    ws_teardown();

    LOG_INFO(ADS_TAG, QString("Stopped stream for account %1").arg(account_id_));
}

void AccountDataStream::pause() {
    quote_timer_->stop();
    portfolio_timer_->stop();
    watchlist_timer_->stop();
    // Keep WS alive in background
}

void AccountDataStream::resume() {
    if (!running_)
        return;
    if (!ws_active()) {
        quote_timer_->start();
        watchlist_timer_->start();
    }
    portfolio_timer_->start();
}

// ── Symbol management ───────────────────────────────────────────────────────

void AccountDataStream::subscribe_symbols(const QStringList& symbols) {
    watchlist_symbols_ = symbols;
    if (ws_active())
        ws_resubscribe();
}

void AccountDataStream::set_selected_symbol(const QString& symbol, const QString& exchange) {
    selected_symbol_ = symbol;
    selected_exchange_ = exchange;
    if (ws_active())
        ws_resubscribe();
}

// ── Cached data access ──────────────────────────────────────────────────────

QVector<BrokerPosition> AccountDataStream::cached_positions() const { return positions_; }
QVector<BrokerHolding> AccountDataStream::cached_holdings() const { return holdings_; }
QVector<BrokerOrderInfo> AccountDataStream::cached_orders() const { return orders_; }
BrokerFunds AccountDataStream::cached_funds() const { return funds_; }

BrokerQuote AccountDataStream::cached_quote(const QString& symbol) const {
    auto it = quote_cache_.find(symbol);
    return it != quote_cache_.end() ? it.value() : BrokerQuote{};
}

// ── Timer callbacks ─────────────────────────────────────────────────────────

// ── Token expiry check ─────────────────────────────────────────────────────

bool AccountDataStream::is_token_expired() const {
    auto creds = AccountManager::instance().load_credentials(account_id_);
    if (creds.additional_data.isEmpty())
        return false;
    auto doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
    qint64 expires_at = static_cast<qint64>(doc.object().value("token_expires_at").toDouble(0));
    return expires_at > 0 && expires_at <= QDateTime::currentSecsSinceEpoch();
}

bool AccountDataStream::check_token_expiry(const QString& error) {
    // All brokers prefix expired-session errors with "[TOKEN_EXPIRED]"
    // (originated by ZerodhaBroker; generalized here so every broker benefits).
    if (!error.startsWith("[TOKEN_EXPIRED]"))
        return false;

    // May be called from a QtConcurrent worker — AccountManager mutates state
    // and we emit a signal, so marshal to the thread that owns this object.
    QPointer<AccountDataStream> self = this;
    QMetaObject::invokeMethod(this, [self]() {
        if (!self)
            return;
        AccountManager::instance().set_connection_state(
            self->account_id_, ConnectionState::TokenExpired,
            QStringLiteral("Broker session token expired"));
        LOG_WARN(ADS_TAG, QString("Token expired for account %1 (%2)")
                              .arg(self->account_id_, self->broker_id_));
        emit self->token_expired(self->account_id_);
    }, Qt::QueuedConnection);
    return true;
}

void AccountDataStream::on_quote_timer() {
    if (is_token_expired()) {
        emit token_expired(account_id_);
        return;
    }
    async_fetch_quote();
}

void AccountDataStream::on_portfolio_timer() {
    if (is_token_expired()) {
        emit token_expired(account_id_);
        return;
    }
    if (portfolio_fetching_.exchange(true))
        return;
    async_fetch_positions();
    async_fetch_holdings();
    async_fetch_orders();
    async_fetch_funds();
    portfolio_fetching_ = false;
}

void AccountDataStream::on_watchlist_timer() {
    if (is_token_expired()) {
        emit token_expired(account_id_);
        return;
    }
    async_fetch_watchlist_quotes();
}

// ── Async fetchers ──────────────────────────────────────────────────────────
// All follow P8: capture account_id by value, QPointer guard, QueuedConnection

void AccountDataStream::async_fetch_quote() {
    if (quote_fetching_.exchange(true))
        return;
    if (selected_symbol_.isEmpty()) {
        quote_fetching_ = false;
        return;
    }

    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    const QString symbol = selected_symbol_;
    QPointer<AccountDataStream> self = this;

    // Load credentials on the calling (main) thread. QSqlDatabase is bound
    // to the thread that opened it; calling SecureStorage::retrieve from a
    // QtConcurrent worker corrupts SQLite memory and crashes.
    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) {
        quote_fetching_ = false;
        return;
    }

    (void)QtConcurrent::run([self, acct_id, bid, symbol, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) {
            if (self) self->quote_fetching_ = false;
            return;
        }
        auto result = broker->get_quotes(creds, {symbol});
        if (self) self->quote_fetching_ = false;
        if (!result.success || !result.data || result.data->isEmpty()) {
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        const auto quote = result.data->first();
        QMetaObject::invokeMethod(self, [self, acct_id, symbol, quote]() {
            if (!self) return;
            self->quote_cache_[symbol] = quote;
            emit self->quote_updated(acct_id, symbol, quote);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::async_fetch_positions() {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) return;

    (void)QtConcurrent::run([self, acct_id, bid, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto result = broker->get_positions(creds);
        if (!result.success || !result.data) {
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            self->positions_ = data;
            emit self->positions_updated(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::async_fetch_holdings() {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) return;

    (void)QtConcurrent::run([self, acct_id, bid, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto result = broker->get_holdings(creds);
        if (!result.success || !result.data) {
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            self->holdings_ = data;
            emit self->holdings_updated(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::async_fetch_orders() {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) return;

    (void)QtConcurrent::run([self, acct_id, bid, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto result = broker->get_orders(creds);
        if (!result.success || !result.data) {
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            self->orders_ = data;
            emit self->orders_updated(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::async_fetch_funds() {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) return;

    (void)QtConcurrent::run([self, acct_id, bid, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto result = broker->get_funds(creds);
        if (!result.success || !result.data) {
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            self->funds_ = data;
            emit self->funds_updated(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::async_fetch_watchlist_quotes() {
    if (watchlist_symbols_.isEmpty())
        return;
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    const QStringList symbols = watchlist_symbols_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) return;

    (void)QtConcurrent::run([self, acct_id, bid, symbols, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto result = broker->get_quotes(creds, symbols.toVector());
        if (!result.success || !result.data) {
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            for (const auto& q : data)
                self->quote_cache_[q.symbol] = q;
            emit self->watchlist_updated(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

// ── On-demand fetches ───────────────────────────────────────────────────────

void AccountDataStream::fetch_candles(const QString& symbol, const QString& timeframe) {
    if (candles_fetching_.exchange(true))
        return;
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) {
        candles_fetching_ = false;
        return;
    }

    (void)QtConcurrent::run([self, acct_id, bid, symbol, timeframe, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) {
            if (self) self->candles_fetching_ = false;
            return;
        }
        // Build date range
        const QString to_dt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
        QString from_dt;
        if (timeframe == "1d" || timeframe == "D" || timeframe == "1w" || timeframe == "W")
            from_dt = QDate::currentDate().addYears(-2).toString("yyyy-MM-dd") + " 00:00";
        else if (timeframe == "1h" || timeframe == "60")
            from_dt = QDate::currentDate().addDays(-30).toString("yyyy-MM-dd") + " 00:00";
        else if (timeframe == "30m")
            from_dt = QDate::currentDate().addDays(-10).toString("yyyy-MM-dd") + " 00:00";
        else if (timeframe == "15m")
            from_dt = QDate::currentDate().addDays(-7).toString("yyyy-MM-dd") + " 00:00";
        else if (timeframe == "5m")
            from_dt = QDate::currentDate().addDays(-5).toString("yyyy-MM-dd") + " 00:00";
        else
            from_dt = QDate::currentDate().addDays(-5).toString("yyyy-MM-dd") + " 00:00";

        auto result = broker->get_history(creds, symbol, timeframe, from_dt, to_dt);
        if (self) self->candles_fetching_ = false;
        if (!result.success || !result.data) {
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            emit self->candles_fetched(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::fetch_orderbook(const QString& symbol) {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) {
        LOG_WARN(ADS_TAG, QString("fetch_orderbook: no credentials for %1").arg(acct_id));
        return;
    }

    (void)QtConcurrent::run([self, acct_id, bid, symbol, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;

        const QString today = QDate::currentDate().toString("yyyy-MM-dd");
        auto result = broker->get_historical_quotes_single(creds, symbol, today + "T00:00:00Z", "", 1000);

        if (!result.success || !result.data || result.data->isEmpty()) {
            LOG_WARN(ADS_TAG, QString("fetch_orderbook: L2 depth failed for %1 (%2), trying snapshot")
                                  .arg(symbol, result.error));
            auto snap = broker->get_quotes(creds, {symbol});
            if (!snap.success || !snap.data || snap.data->isEmpty()) {
                LOG_WARN(ADS_TAG, QString("fetch_orderbook: snapshot fallback also failed for %1").arg(symbol));
                return;
            }
            const auto& q = snap.data->first();
            const double b = q.bid > 0 ? q.bid : (q.ltp > 0 ? q.ltp * 0.9995 : 0);
            const double a = q.ask > 0 ? q.ask : (q.ltp > 0 ? q.ltp * 1.0005 : 0);
            if (b <= 0 || a <= 0) return;
            QVector<QPair<double, double>> bids{{b, q.bid_size > 0 ? q.bid_size : 100.0}};
            QVector<QPair<double, double>> asks{{a, q.ask_size > 0 ? q.ask_size : 100.0}};
            const double spread = a - b;
            const double spread_pct = b > 0 ? (spread / b) * 100.0 : 0.0;
            QMetaObject::invokeMethod(self, [self, acct_id, bids, asks, spread, spread_pct]() {
                if (!self) return;
                emit self->orderbook_fetched(acct_id, bids, asks, spread, spread_pct, {}, {});
            }, Qt::QueuedConnection);
            return;
        }

        // Aggregate L2 — track orders count per level
        QMap<double, double> bid_map, ask_map;
        QMap<double, int> bid_ord_map, ask_ord_map;
        for (const auto& q : *result.data) {
            if (q.bid > 0) {
                bid_map[q.bid] += q.bid_size > 0 ? q.bid_size : 1.0;
                if (q.oi > 0) bid_ord_map[q.bid] += static_cast<int>(q.oi);
            }
            if (q.ask > 0) {
                ask_map[q.ask] += q.ask_size > 0 ? q.ask_size : 1.0;
                if (q.oi > 0) ask_ord_map[q.ask] += static_cast<int>(q.oi);
            }
        }
        QVector<QPair<double, double>> bids, asks;
        QVector<int> bid_orders, ask_orders;
        auto bid_keys = bid_map.keys();
        std::sort(bid_keys.begin(), bid_keys.end(), std::greater<double>());
        for (const double p : bid_keys.mid(0, 10)) {
            bids.append({p, bid_map[p]});
            bid_orders.append(bid_ord_map.value(p, 0));
        }
        auto ask_keys = ask_map.keys();
        std::sort(ask_keys.begin(), ask_keys.end());
        for (const double p : ask_keys.mid(0, 10)) {
            asks.append({p, ask_map[p]});
            ask_orders.append(ask_ord_map.value(p, 0));
        }
        if (bids.isEmpty() || asks.isEmpty()) return;

        LOG_INFO(ADS_TAG, QString("fetch_orderbook: %1 bids, %2 asks for %3")
                              .arg(bids.size()).arg(asks.size()).arg(symbol));

        const double best_bid = bids.first().first;
        const double best_ask = asks.first().first;
        const double spread = best_ask - best_bid;
        const double spread_pct = best_bid > 0 ? (spread / best_bid) * 100.0 : 0.0;
        QMetaObject::invokeMethod(self, [self, acct_id, bids, asks, spread, spread_pct,
                                         bid_orders, ask_orders]() {
            if (!self) return;
            emit self->orderbook_fetched(acct_id, bids, asks, spread, spread_pct,
                                         bid_orders, ask_orders);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::fetch_time_sales(const QString& symbol) {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) return;

    (void)QtConcurrent::run([self, acct_id, bid, symbol, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        const QString today = QDate::currentDate().toString("yyyy-MM-dd");
        auto result = broker->get_historical_trades_single(creds, symbol, today + "T00:00:00Z", "", 500);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            emit self->time_sales_fetched(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::fetch_latest_trade(const QString& symbol) {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) return;

    (void)QtConcurrent::run([self, acct_id, bid, symbol, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto result = broker->get_latest_trade(creds, symbol);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            emit self->latest_trade_fetched(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::fetch_calendar() {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);

    (void)QtConcurrent::run([self, acct_id, bid, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        const QString start = QDate::currentDate().addDays(-5).toString("yyyy-MM-dd");
        const QString end = QDate::currentDate().addDays(30).toString("yyyy-MM-dd");
        auto result = broker->get_calendar(creds, start, end);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            emit self->calendar_fetched(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::fetch_clock() {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);

    (void)QtConcurrent::run([self, acct_id, bid, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto result = broker->get_clock(creds);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            emit self->clock_fetched(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

// ── WebSocket ───────────────────────────────────────────────────────────────

void AccountDataStream::wire_base_ws(BrokerWebSocketBase* ws) {
    connect(ws, &BrokerWebSocketBase::tick_received, this, [this](const BrokerQuote& q) {
        ++ws_tick_count_;
        quote_cache_[q.symbol] = q;
        emit quote_updated(account_id_, q.symbol, q);
    });
    connect(ws, &BrokerWebSocketBase::depth_received, this, [this](const MarketDepth& d) {
        if (d.bids.isEmpty() && d.asks.isEmpty())
            return;
        QVector<QPair<double, double>> bids, asks;
        for (const auto& b : d.bids)
            bids.append({b.price, static_cast<double>(b.quantity)});
        for (const auto& a : d.asks)
            asks.append({a.price, static_cast<double>(a.quantity)});
        const double best_bid = bids.isEmpty() ? 0 : bids.first().first;
        const double best_ask = asks.isEmpty() ? 0 : asks.first().first;
        const double spread = (best_bid > 0 && best_ask > 0) ? best_ask - best_bid : 0;
        const double spread_pct = best_bid > 0 ? spread / best_bid * 100.0 : 0;
        emit orderbook_fetched(account_id_, bids, asks, spread, spread_pct, {}, {});
    });
    connect(ws, &BrokerWebSocketBase::connected, this, [this]() {
        emit connection_state_changed(account_id_, ConnectionState::Connected);
    });
    connect(ws, &BrokerWebSocketBase::disconnected, this, [this]() {
        emit connection_state_changed(account_id_, ConnectionState::Disconnected);
    });
    connect(ws, &BrokerWebSocketBase::error_occurred, this, [this](const QString& e) {
        LOG_ERROR(ADS_TAG, QString("WS error: %1").arg(e));
        check_token_expiry(e);
    });
}

void AccountDataStream::ws_init() {
    if (broker_id_ == "fyers") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        if (creds.api_key.isEmpty() || creds.access_token.isEmpty())
            return;

        auto* fws = new FyersWebSocket(creds.api_key, creds.access_token, this);
        ws_ = fws;

        connect(fws, &FyersWebSocket::tick_received, this, [this](const FyersTick& tick) {
            ++ws_tick_count_;
            BrokerQuote q;
            q.symbol = tick.symbol;
            q.ltp = tick.ltp;
            q.open = tick.open;
            q.high = tick.high;
            q.low = tick.low;
            q.close = tick.prev_close;
            q.volume = tick.volume;
            q.bid = tick.bid;
            q.ask = tick.ask;
            q.change = tick.ltp - tick.prev_close;
            q.change_pct = tick.prev_close > 0 ? ((tick.ltp - tick.prev_close) / tick.prev_close) * 100.0 : 0;
            q.timestamp = tick.timestamp;
            quote_cache_[tick.symbol] = q;
            emit quote_updated(account_id_, tick.symbol, q);
        });

        connect(fws, &FyersWebSocket::depth_received, this,
                [this](const QString& symbol, const QVector<QPair<double, double>>& bids,
                       const QVector<QPair<double, double>>& asks) {
            Q_UNUSED(symbol);
            if (bids.isEmpty() && asks.isEmpty()) return;
            const double best_bid = bids.isEmpty() ? 0 : bids.first().first;
            const double best_ask = asks.isEmpty() ? 0 : asks.first().first;
            double spread = 0, spread_pct = 0;
            if (best_bid > 0 && best_ask > 0) {
                spread = best_ask - best_bid;
                spread_pct = (spread / best_bid) * 100.0;
            }
            emit orderbook_fetched(account_id_, bids, asks, spread, spread_pct, {}, {});
        });

        connect(fws, &FyersWebSocket::connected, this, [this]() {
            LOG_INFO(ADS_TAG, QString("Fyers HSM connected for %1").arg(account_id_));
            emit connection_state_changed(account_id_, ConnectionState::Connected);
            auto to_fyers = [](const QString& sym) {
                return sym.contains(':') ? sym : QStringLiteral("NSE:") + sym + QStringLiteral("-EQ");
            };
            QStringList fyers_symbols;
            if (!selected_symbol_.isEmpty())
                fyers_symbols.append(to_fyers(selected_symbol_));
            for (const auto& s : watchlist_symbols_)
                fyers_symbols.append(to_fyers(s));
            fyers_symbols.removeDuplicates();
            if (auto* fws = qobject_cast<FyersWebSocket*>(ws_))
                fws->subscribe(fyers_symbols);
        });

        connect(fws, &FyersWebSocket::disconnected, this, [this]() {
            LOG_WARN(ADS_TAG, QString("Fyers HSM disconnected for %1").arg(account_id_));
            emit connection_state_changed(account_id_, ConnectionState::Disconnected);
        });

        connect(fws, &FyersWebSocket::error_occurred, this, [this](const QString& err) {
            LOG_ERROR(ADS_TAG, QString("Fyers WS error: %1").arg(err));
            check_token_expiry(err);
        });

        fws->open();
        return;
    }

    if (broker_id_ == "alpaca") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        if (creds.api_key.isEmpty() || creds.api_secret.isEmpty())
            return;

        auto* aws = new AlpacaWebSocket(creds.api_key, creds.api_secret, this);
        ws_ = aws;

        connect(aws, &AlpacaWebSocket::tick_received, this, [this](const BrokerQuote& q) {
            ++ws_tick_count_;
            quote_cache_[q.symbol] = q;
            emit quote_updated(account_id_, q.symbol, q);
        });

        connect(aws, &AlpacaWebSocket::trade_received, this, [this](const BrokerTrade& trade) {
            auto it = quote_cache_.find(trade.symbol);
            if (it != quote_cache_.end()) {
                it->ltp = trade.price;
                const auto dt = QDateTime::fromString(trade.timestamp, Qt::ISODateWithMs);
                it->timestamp = dt.isValid() ? dt.toMSecsSinceEpoch() : 0;
                emit quote_updated(account_id_, trade.symbol, it.value());
            }
        });

        connect(aws, &AlpacaWebSocket::bar_received, this,
                [this](const QString& symbol, const BrokerCandle& bar) {
            auto it = quote_cache_.find(symbol);
            if (it != quote_cache_.end()) {
                it->open = bar.open;
                it->high = bar.high;
                it->low = bar.low;
                it->close = bar.close;
                it->volume = bar.volume;
                it->change = bar.close - bar.open;
                it->change_pct = bar.open > 0 ? ((bar.close - bar.open) / bar.open) * 100.0 : 0;
                emit quote_updated(account_id_, symbol, it.value());
            }
        });

        connect(aws, &AlpacaWebSocket::connected, this, [this]() {
            LOG_INFO(ADS_TAG, QString("Alpaca WS connected for %1").arg(account_id_));
            emit connection_state_changed(account_id_, ConnectionState::Connected);
            QStringList symbols;
            if (!selected_symbol_.isEmpty())
                symbols.append(selected_symbol_);
            for (const auto& s : watchlist_symbols_) {
                if (!symbols.contains(s))
                    symbols.append(s);
            }
            if (auto* a = qobject_cast<AlpacaWebSocket*>(ws_))
                a->subscribe(symbols);
        });

        connect(aws, &AlpacaWebSocket::disconnected, this, [this]() {
            LOG_WARN(ADS_TAG, QString("Alpaca WS disconnected for %1").arg(account_id_));
            emit connection_state_changed(account_id_, ConnectionState::Disconnected);
        });

        connect(aws, &AlpacaWebSocket::error_occurred, this, [this](const QString& err) {
            LOG_ERROR(ADS_TAG, QString("Alpaca WS error: %1").arg(err));
            check_token_expiry(err);
        });

        connect(aws, &AlpacaWebSocket::market_closed, this, [this]() {
            LOG_INFO(ADS_TAG, QString("Alpaca market closed for %1 — falling back to polling").arg(account_id_));
        });

        aws->open();
        return;
    }

    // ── Phase 2 broker WebSocket adapters (shared BrokerWebSocketBase) ─────────
    // Helper: collect the current selected + watchlist symbols (deduped).
    auto current_symbols = [this]() {
        QStringList syms;
        if (!selected_symbol_.isEmpty())
            syms.append(selected_symbol_);
        for (const auto& s : watchlist_symbols_) {
            if (!syms.contains(s))
                syms.append(s);
        }
        syms.removeDuplicates();
        return syms;
    };
    // Helper: resolve symbols → numeric instrument tokens via InstrumentService
    // for this broker. Symbols that don't resolve are skipped.
    auto resolve_tokens = [this](const QStringList& syms) {
        QVector<qint64> tokens;
        auto& svc = InstrumentService::instance();
        for (const auto& s : syms) {
            const QString exch = (s == selected_symbol_ && !selected_exchange_.isEmpty())
                                     ? selected_exchange_
                                     : QStringLiteral("NSE");
            auto tok = svc.instrument_token(s, exch, broker_id_);
            if (tok.has_value())
                tokens.append(*tok);
        }
        return tokens;
    };

    if (broker_id_ == "upstox") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        if (creds.access_token.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("Upstox WS: missing access_token for %1").arg(account_id_));
            return;
        }
        auto* uws = new UpstoxWebSocket(creds.access_token, this);
        ws_ = uws;
        wire_base_ws(uws);
        connect(uws, &UpstoxWebSocket::connected, this, [this, current_symbols, resolve_tokens]() {
            LOG_INFO(ADS_TAG, QString("Upstox WS connected for %1").arg(account_id_));
            if (auto* w = qobject_cast<UpstoxWebSocket*>(ws_))
                w->subscribe(resolve_tokens(current_symbols()));
        });
        uws->open();
        return;
    }

    if (broker_id_ == "dhan") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        if (creds.access_token.isEmpty() || creds.api_key.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("Dhan WS: missing access_token/client_id for %1").arg(account_id_));
            return;
        }
        auto* dws = new DhanWebSocket(creds.access_token, creds.api_key, this);
        ws_ = dws;
        wire_base_ws(dws);
        connect(dws, &DhanWebSocket::connected, this, [this, current_symbols, resolve_tokens]() {
            LOG_INFO(ADS_TAG, QString("Dhan WS connected for %1").arg(account_id_));
            if (auto* w = qobject_cast<DhanWebSocket*>(ws_))
                w->subscribe(resolve_tokens(current_symbols()));
        });
        dws->open();
        return;
    }

    if (broker_id_ == "kotak") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        const QStringList parts = creds.access_token.split(":::");
        if (parts.size() < 4) {
            LOG_WARN(ADS_TAG, QString("Kotak WS: malformed token for %1").arg(account_id_));
            return;
        }
        // Packed: "trading_token:::trading_sid:::base_url:::access_token[:::server_id]"
        const QString auth_token = parts.value(0);   // JWT → "Authorization"
        const QString sid = parts.value(1);          // redis session → "Sid"
        const QString access_tok = parts.value(3);
        const QString server_id = parts.value(4);
        if (auth_token.isEmpty() || sid.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("Kotak WS: missing auth_token/sid for %1").arg(account_id_));
            return;
        }
        auto* kws = new KotakWebSocket(auth_token, sid, server_id, access_tok, this);
        ws_ = kws;
        wire_base_ws(kws);
        connect(kws, &KotakWebSocket::connected, this, [this, current_symbols, resolve_tokens]() {
            LOG_INFO(ADS_TAG, QString("Kotak WS connected for %1").arg(account_id_));
            if (auto* w = qobject_cast<KotakWebSocket*>(ws_)) {
                if (!selected_exchange_.isEmpty())
                    w->set_default_exchange(selected_exchange_);
                w->subscribe(resolve_tokens(current_symbols()));
            }
        });
        kws->open();
        return;
    }

    if (broker_id_ == "iifl") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        // access_token packed as "trade_token:::feed_token"; feed token is for market data.
        const QStringList parts = creds.access_token.split(":::");
        const QString market_token = parts.size() >= 2 ? parts.value(1) : creds.access_token;
        if (market_token.isEmpty() || creds.user_id.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("IIFL WS: missing market_token/user_id for %1").arg(account_id_));
            return;
        }
        auto* iws = new IIFLWebSocket(market_token, creds.user_id, this);
        ws_ = iws;
        wire_base_ws(iws);
        connect(iws, &IIFLWebSocket::connected, this, [this, current_symbols, resolve_tokens]() {
            LOG_INFO(ADS_TAG, QString("IIFL WS connected for %1").arg(account_id_));
            if (auto* w = qobject_cast<IIFLWebSocket*>(ws_)) {
                if (!selected_exchange_.isEmpty())
                    w->set_default_exchange(selected_exchange_);
                w->subscribe(resolve_tokens(current_symbols()));
            }
        });
        iws->open();
        return;
    }

    if (broker_id_ == "fivepaisa") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        // client_code = 3rd ":::" part of ApiKey ("app_key:::user_id:::client_id"),
        // falling back to user_id when absent.
        const QStringList key_parts = creds.api_key.split(":::");
        QString client_code = key_parts.size() >= 3 ? key_parts.value(2) : QString();
        if (client_code.isEmpty())
            client_code = creds.user_id;
        if (creds.access_token.isEmpty() || client_code.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("5Paisa WS: missing access_token/client_code for %1").arg(account_id_));
            return;
        }
        auto* fws = new FivePaisaWebSocket(creds.access_token, client_code, this);
        ws_ = fws;
        wire_base_ws(fws);
        connect(fws, &FivePaisaWebSocket::connected, this, [this, current_symbols, resolve_tokens]() {
            LOG_INFO(ADS_TAG, QString("5Paisa WS connected for %1").arg(account_id_));
            if (auto* w = qobject_cast<FivePaisaWebSocket*>(ws_))
                w->subscribe(resolve_tokens(current_symbols()));
        });
        fws->open();
        return;
    }

    if (broker_id_ == "aliceblue") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        // client_id stored in user_id; session JWT in access_token.
        if (creds.user_id.isEmpty() || creds.access_token.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("AliceBlue WS: missing client_id/jwt for %1").arg(account_id_));
            return;
        }
        auto* aws = new AliceBlueWebSocket(creds.user_id, creds.access_token, this);
        ws_ = aws;
        wire_base_ws(aws);
        connect(aws, &AliceBlueWebSocket::connected, this, [this, current_symbols, resolve_tokens]() {
            LOG_INFO(ADS_TAG, QString("AliceBlue WS connected for %1").arg(account_id_));
            if (auto* w = qobject_cast<AliceBlueWebSocket*>(ws_)) {
                if (!selected_exchange_.isEmpty())
                    w->set_default_exchange(selected_exchange_);
                w->subscribe(resolve_tokens(current_symbols()));
            }
        });
        aws->open();
        return;
    }

    if (broker_id_ == "shoonya") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        // user_id = uid/actid; access_token = susertoken (sent verbatim).
        if (creds.user_id.isEmpty() || creds.access_token.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("Shoonya WS: missing user_id/susertoken for %1").arg(account_id_));
            return;
        }
        auto* sws = new ShoonyaWebSocket(creds.user_id, creds.user_id, creds.access_token, this);
        ws_ = sws;
        wire_base_ws(sws);
        connect(sws, &ShoonyaWebSocket::connected, this, [this, current_symbols, resolve_tokens]() {
            LOG_INFO(ADS_TAG, QString("Shoonya WS connected for %1").arg(account_id_));
            if (auto* w = qobject_cast<ShoonyaWebSocket*>(ws_)) {
                if (!selected_exchange_.isEmpty())
                    w->set_default_exchange(selected_exchange_);
                w->subscribe(resolve_tokens(current_symbols()));
            }
        });
        sws->open();
        return;
    }

    if (broker_id_ == "motilal") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        if (creds.api_key.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("Motilal poller: missing credentials for %1").arg(account_id_));
            return;
        }
        // Motilal has no streaming socket — MotilalPoller polls quotes by symbol.
        auto* mp = new MotilalPoller(creds, this);
        ws_ = mp;
        wire_base_ws(mp);
        connect(mp, &MotilalPoller::connected, this, [this, current_symbols]() {
            LOG_INFO(ADS_TAG, QString("Motilal poller started for %1").arg(account_id_));
            if (auto* w = qobject_cast<MotilalPoller*>(ws_))
                w->subscribe(current_symbols());
        });
        mp->open();
        return;
    }

    if (broker_id_ != "angelone")
        return;

    auto creds = AccountManager::instance().load_credentials(account_id_);
    if (creds.api_key.isEmpty() || creds.additional_data.isEmpty())
        return;

    auto doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
    if (!doc.isObject())
        return;
    auto obj = doc.object();
    const QString feed_token = obj.value("feed_token").toString();
    const QString client_code = obj.value("client_code").toString();
    if (feed_token.isEmpty() || client_code.isEmpty())
        return;

    LOG_INFO(ADS_TAG, QString("WebSocket available for account %1 (AngelOne) — deferred to screen wiring").arg(account_id_));
}

void AccountDataStream::ws_teardown() {
    if (!ws_)
        return;
    // The WS object will be an AngelOneWebSocket — call close and deleteLater
    QMetaObject::invokeMethod(ws_, "close");
    ws_->deleteLater();
    ws_ = nullptr;
}

bool AccountDataStream::ws_active() const {
    if (!ws_)
        return false;
    if (auto* fws = qobject_cast<FyersWebSocket*>(ws_))
        return fws->is_connected() && ws_tick_count_ > 0;
    if (auto* aws = qobject_cast<AlpacaWebSocket*>(ws_))
        return aws->is_connected() && ws_tick_count_ > 0;
    if (auto* b = qobject_cast<BrokerWebSocketBase*>(ws_))
        return b->is_connected() && ws_tick_count_ > 0;
    return false;
}

void AccountDataStream::ws_resubscribe() {
    if (auto* fws = qobject_cast<FyersWebSocket*>(ws_)) {
        auto to_fyers = [](const QString& sym) {
            return sym.contains(':') ? sym : QStringLiteral("NSE:") + sym + QStringLiteral("-EQ");
        };
        QStringList fyers_symbols;
        if (!selected_symbol_.isEmpty())
            fyers_symbols.append(to_fyers(selected_symbol_));
        for (const auto& s : watchlist_symbols_)
            fyers_symbols.append(to_fyers(s));
        fyers_symbols.removeDuplicates();
        fws->set_subscriptions(fyers_symbols);
    }

    if (auto* aws = qobject_cast<AlpacaWebSocket*>(ws_)) {
        QStringList symbols;
        if (!selected_symbol_.isEmpty())
            symbols.append(selected_symbol_);
        for (const auto& s : watchlist_symbols_) {
            if (!symbols.contains(s))
                symbols.append(s);
        }
        aws->set_subscriptions(symbols);
    }

    // Generic Phase 2 adapters (shared BrokerWebSocketBase). Rebuild the symbol
    // set wholesale: drop existing subscriptions, then subscribe the current set.
    if (auto* b = qobject_cast<BrokerWebSocketBase*>(ws_)) {
        QStringList symbols;
        if (!selected_symbol_.isEmpty())
            symbols.append(selected_symbol_);
        for (const auto& s : watchlist_symbols_) {
            if (!symbols.contains(s))
                symbols.append(s);
        }
        symbols.removeDuplicates();

        // Motilal polls by symbol — feed it the symbol list directly.
        if (auto* mp = qobject_cast<MotilalPoller*>(ws_)) {
            mp->subscribe(symbols);
            return;
        }

        // Token-based adapters: resolve symbols → instrument tokens.
        QVector<qint64> tokens;
        auto& svc = InstrumentService::instance();
        for (const auto& s : symbols) {
            const QString exch = (s == selected_symbol_ && !selected_exchange_.isEmpty())
                                     ? selected_exchange_
                                     : QStringLiteral("NSE");
            auto tok = svc.instrument_token(s, exch, broker_id_);
            if (tok.has_value())
                tokens.append(*tok);
        }
        b->unsubscribe();
        b->subscribe(tokens);
    }
}

} // namespace fincept::trading
