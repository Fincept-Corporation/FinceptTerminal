#include "trading/AccountDataStream.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/HistoricalDataService.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/instruments/InstrumentService.h"
#include "trading/brokers/alpaca/AlpacaWebSocket.h"
#include "trading/websocket/AliceBlueWebSocket.h"
#include "trading/websocket/AngelOneWebSocket.h"
#include "trading/websocket/BrokerWebSocketBase.h"
#include "trading/websocket/DhanWebSocket.h"
#include "trading/websocket/FivePaisaWebSocket.h"
#include "trading/websocket/FyersWebSocket.h"
#include "trading/websocket/IIFLWebSocket.h"
#include "trading/websocket/IciciDirectWebSocket.h"
#include "trading/websocket/KotakWebSocket.h"
#include "trading/websocket/MotilalPoller.h"
#include "trading/websocket/ShoonyaWebSocket.h"
#include "trading/websocket/UpstoxWebSocket.h"
#include "trading/websocket/ZerodhaWebSocket.h"

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
constexpr int ADS_ACTIVE_FEED_POLL_MS = 3000; // fast poll for algo/active-feed symbols (non-WS brokers)
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

    active_feed_timer_ = new QTimer(this);
    active_feed_timer_->setInterval(ADS_ACTIVE_FEED_POLL_MS);
    connect(active_feed_timer_, &QTimer::timeout, this, &AccountDataStream::on_active_feed_timer);
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
    if (!active_feed_symbol_union().isEmpty())
        active_feed_timer_->start();

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
    active_feed_timer_->stop();
    ws_teardown();

    LOG_INFO(ADS_TAG, QString("Stopped stream for account %1").arg(account_id_));
}

void AccountDataStream::pause() {
    quote_timer_->stop();
    portfolio_timer_->stop();
    watchlist_timer_->stop();
    active_feed_timer_->stop();
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
    if (!active_feed_symbol_union().isEmpty())
        active_feed_timer_->start();
}

void AccountDataStream::refresh_portfolio_now() {
    if (!running_)
        return;
    async_fetch_positions();
    async_fetch_holdings();
    async_fetch_orders();
    async_fetch_funds();
}

// ── Symbol management ───────────────────────────────────────────────────────

void AccountDataStream::subscribe_symbols(const QString& consumer_id, const QStringList& symbols) {
    LOG_INFO("subdbg", QString("subscribe_symbols consumer='%1' (%2 syms): [%3] ws_connected=%4")
                           .arg(consumer_id).arg(symbols.size())
                           .arg(symbols.join(','), ws_connected() ? "Y" : "N"));
    if (symbols.isEmpty())
        consumer_symbols_.remove(consumer_id);
    else
        consumer_symbols_[consumer_id] = symbols;
    // Resubscribe whenever the socket is connected — NOT gated on ws_active(),
    // which also requires ticks. On an account switch-back the socket is
    // connected but may have 0 ticks (it was subscribed to 0 symbols while
    // unfocused), so an ws_active() gate would never re-push the watchlist.
    if (ws_connected())
        ws_resubscribe();
}

void AccountDataStream::unsubscribe_consumer(const QString& consumer_id) {
    const bool had = consumer_symbols_.remove(consumer_id) > 0;
    active_feed_symbols_.remove(consumer_id);
    if (active_feed_timer_ && active_feed_symbol_union().isEmpty())
        active_feed_timer_->stop();
    if (had && ws_connected())
        ws_resubscribe();
}

void AccountDataStream::set_active_feed(const QString& consumer_id, const QStringList& symbols) {
    if (symbols.isEmpty())
        active_feed_symbols_.remove(consumer_id);
    else
        active_feed_symbols_[consumer_id] = symbols;
    if (!active_feed_timer_)
        return;
    const bool any = !active_feed_symbol_union().isEmpty();
    if (any && running_) {
        if (!active_feed_timer_->isActive())
            active_feed_timer_->start();
        if (!ws_connected())
            async_fetch_active_feed_quotes(); // non-WS only; WS brokers wait for the pushed tick
    } else if (!any && active_feed_timer_->isActive()) {
        active_feed_timer_->stop();
    }
}

QStringList AccountDataStream::subscribed_symbols() const {
    QStringList out;
    for (auto it = consumer_symbols_.constBegin(); it != consumer_symbols_.constEnd(); ++it)
        for (const QString& s : it.value())
            if (!out.contains(s))
                out.append(s);
    return out;
}

QStringList AccountDataStream::active_feed_symbol_union() const {
    QStringList out;
    for (auto it = active_feed_symbols_.constBegin(); it != active_feed_symbols_.constEnd(); ++it)
        for (const QString& s : it.value())
            if (!out.contains(s))
                out.append(s);
    return out;
}

void AccountDataStream::set_selected_symbol(const QString& symbol, const QString& exchange) {
    selected_symbol_ = symbol;
    selected_exchange_ = exchange;
    if (ws_connected())
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
            LOG_WARN(ADS_TAG, QString("async_fetch_quote failed for %1/%2 [%3]: %4")
                                  .arg(bid, acct_id, symbol, result.error));
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
            LOG_WARN(ADS_TAG, QString("async_fetch_positions failed for %1/%2: %3")
                                  .arg(bid, acct_id, result.error));
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        // Success — log the count so an empty live blotter can be told apart from a
        // token/error (which logs WARN above): 0 rows here = genuinely no positions.
        LOG_INFO(ADS_TAG, QString("get_positions %1/%2 → %3 row(s)").arg(bid, acct_id).arg(result.data->size()));
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
            LOG_WARN(ADS_TAG, QString("async_fetch_holdings failed for %1/%2: %3")
                                  .arg(bid, acct_id, result.error));
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        LOG_INFO(ADS_TAG, QString("get_holdings %1/%2 → %3 row(s)").arg(bid, acct_id).arg(result.data->size()));
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
            LOG_WARN(ADS_TAG, QString("async_fetch_orders failed for %1/%2: %3")
                                  .arg(bid, acct_id, result.error));
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        LOG_INFO(ADS_TAG, QString("get_orders %1/%2 → %3 row(s)").arg(bid, acct_id).arg(result.data->size()));
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
            LOG_WARN(ADS_TAG, QString("async_fetch_funds failed for %1/%2: %3")
                                  .arg(bid, acct_id, result.error));
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
    const QStringList symbols = subscribed_symbols();
    if (symbols.isEmpty())
        return;
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty()) return;

    (void)QtConcurrent::run([self, acct_id, bid, symbols, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto result = broker->get_quotes(creds, symbols.toVector());
        if (!result.success || !result.data) {
            LOG_WARN(ADS_TAG, QString("async_fetch_watchlist_quotes failed for %1/%2 (%3 syms): %4")
                                  .arg(bid, acct_id).arg(symbols.size()).arg(result.error));
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

void AccountDataStream::on_active_feed_timer() {
    if (!running_ || ws_connected())
        return; // WS is connected and pushes ticks — poll ONLY for non-WS brokers / WS outage
    if (is_token_expired()) {
        emit token_expired(account_id_);
        return;
    }
    async_fetch_active_feed_quotes();
}

void AccountDataStream::async_fetch_active_feed_quotes() {
    const QStringList symbols = active_feed_symbol_union();
    if (symbols.isEmpty())
        return;
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    auto creds = AccountManager::instance().load_credentials(acct_id);
    if (creds.api_key.isEmpty())
        return;

    (void)QtConcurrent::run([self, acct_id, bid, symbols, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker)
            return;
        auto result = broker->get_quotes(creds, symbols.toVector());
        if (!result.success || !result.data) {
            if (self)
                self->check_token_expiry(result.error);
            return;
        }
        // Emit PER-SYMBOL quote_updated (not watchlist_updated) so DataStreamManager
        // publishes each to broker:<id>:<acct>:quote:<symbol> (the topic algo feeds read).
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            for (const auto& q : data) {
                self->quote_cache_[q.symbol] = q;
                emit self->quote_updated(acct_id, q.symbol, q);
            }
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::fetch_candles(const QString& symbol, const QString& timeframe) {
    if (candles_fetching_.exchange(true))
        return;
    const QString acct_id = account_id_;
    QPointer<AccountDataStream> self = this;

    // Map the chart timeframe to a look-back window (preserves the prior per-tf
    // windows), then delegate to the shared HistoricalDataService (broker fetch +
    // symbol resolution + 60s cache). Yahoo fallback is algo-only, not used here.
    auto lookback_for = [](const QString& tf) -> int {
        if (tf == "1d" || tf == "D" || tf == "1w" || tf == "W") return 3 * 365;
        if (tf == "1h" || tf == "60") return 60;
        if (tf == "30m") return 45;
        if (tf == "15m") return 30;
        if (tf == "5m") return 15;
        return 7;
    };

    HistoricalDataService::instance().fetch(
        symbol, timeframe, lookback_for(timeframe), broker_id_, acct_id,
        [self, acct_id](bool ok, const QVector<BrokerCandle>& candles, const QString& err) {
            if (!self)
                return;
            self->candles_fetching_ = false;
            if (ok) {
                emit self->candles_fetched(acct_id, candles);
            } else {
                self->check_token_expiry(err);
                LOG_WARN(ADS_TAG, QString("fetch_candles failed for %1/%2: %3")
                                      .arg(self->broker_id_, acct_id, err));
            }
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
        // FULL-mode feeds (e.g. Dhan) stream depth for EVERY subscribed watchlist
        // symbol, but the depth table is single-symbol. Drop books that aren't for
        // the selected symbol, else every stock's book rotates through the table.
        // Guarded on a populated d.symbol so brokers that don't tag depth are unaffected.
        auto bare = [](QString s) {
            const int colon = s.lastIndexOf(QLatin1Char(':'));
            if (colon >= 0) s = s.mid(colon + 1);
            if (s.endsWith(QLatin1String("-EQ"))) s.chop(3);
            return s;
        };
        if (!selected_symbol_.isEmpty() && !d.symbol.isEmpty() &&
            bare(d.symbol).compare(bare(selected_symbol_), Qt::CaseInsensitive) != 0)
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

// Map a normalised exchange string → Angel One SmartStream segment code.
static AoExchangeType ao_exchange_type(const QString& exch) {
    const QString e = exch.toUpper();
    if (e == "NFO" || e == "NSE_FO") return AoExchangeType::NSE_FO;
    if (e == "BSE") return AoExchangeType::BSE_CM;
    if (e == "BFO" || e == "BSE_FO") return AoExchangeType::BSE_FO;
    if (e == "MCX") return AoExchangeType::MCX_FO;
    if (e == "CDS") return AoExchangeType::CDE_FO;
    return AoExchangeType::NSE_CM; // NSE cash / default
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
            q.oi = qint64(tick.oi);  // F&O open interest; 0 for cash/equity ticks
            quote_cache_[tick.symbol] = q;
            emit quote_updated(account_id_, tick.symbol, q);
        });

        connect(fws, &FyersWebSocket::depth_received, this,
                [this](const QString& symbol, const QVector<QPair<double, double>>& bids,
                       const QVector<QPair<double, double>>& asks) {
            // The WS streams depth for every subscribed watchlist symbol, but the
            // depth table is single-symbol. Drop ticks that aren't for the selected
            // symbol, else every watchlist symbol's book overwrites the table.
            auto bare = [](QString s) {
                const int colon = s.lastIndexOf(QLatin1Char(':'));
                if (colon >= 0) s = s.mid(colon + 1);
                if (s.endsWith(QLatin1String("-EQ"))) s.chop(3);
                return s;
            };
            if (bare(symbol).compare(bare(selected_symbol_), Qt::CaseInsensitive) != 0)
                return;
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
                // Strip any exchange prefix to inspect the bare contract: a held
                // F&O option arrives as "NFO:NIFTY...CE" (paper) or "NIFTY...CE"
                // (live) — both must resolve on Fyers' "NSE:" segment, NOT the
                // equity "...-EQ" form, or the option never gets a live feed.
                QString core = sym;
                const int colon = core.indexOf(QLatin1Char(':'));
                if (colon >= 0)
                    core = core.mid(colon + 1);
                if (is_fyers_fno_symbol(core))
                    return QStringLiteral("NSE:") + core;
                if (colon >= 0)
                    return sym;  // already-qualified equity (e.g. "BSE:TCS")
                return QStringLiteral("NSE:") + core + QStringLiteral("-EQ");
            };
            QStringList fyers_symbols;
            if (!selected_symbol_.isEmpty())
                fyers_symbols.append(to_fyers(selected_symbol_));
            for (const QString& s : subscribed_symbols())
                fyers_symbols.append(to_fyers(s));
            fyers_symbols.removeDuplicates();
            {
                QStringList fno;
                for (const auto& s : fyers_symbols)
                    if (is_fyers_fno_symbol(s.section(QLatin1Char(':'), -1)))
                        fno << s;
                LOG_INFO("subdbg", QString("[connect] consumers=%1 → %2 fyers syms (F&O: [%3]); raw subscribed=[%4]")
                                       .arg(subscribed_symbols().size())
                                       .arg(fyers_symbols.size())
                                       .arg(fno.join(','), subscribed_symbols().join(',')));
            }
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
            for (const QString& s : subscribed_symbols()) {
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

    if (broker_id_ == "zerodha") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        if (creds.api_key.isEmpty() || creds.access_token.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("Zerodha WS: missing api_key/access_token for %1").arg(account_id_));
            return;
        }
        auto* zws = new ZerodhaWebSocket(creds.api_key, creds.access_token, this);
        ws_ = zws;
        connect(zws, &ZerodhaWebSocket::tick_received, this, [this](const ZerodhaTick& tick) {
            // KiteTicker ticks are keyed by numeric instrument_token — reverse-map
            // to the normalised symbol via the loaded Zerodha master. Drop ticks we
            // can't resolve or with a non-positive LTP so a bad map never surfaces a
            // wrong price under a wrong symbol (quotes fall back to REST instead).
            auto inst = InstrumentService::instance().find_by_token(tick.instrument_token, broker_id_);
            if (!inst.has_value() || inst->symbol.isEmpty() || tick.ltp <= 0.0)
                return;
            ++ws_tick_count_;
            BrokerQuote q;
            q.symbol = inst->symbol;
            q.ltp = tick.ltp;
            q.open = tick.open;
            q.high = tick.high;
            q.low = tick.low;
            q.close = tick.close;
            q.volume = tick.volume;
            q.change = tick.close > 0 ? tick.ltp - tick.close : 0.0;
            q.change_pct = tick.close > 0 ? (tick.ltp - tick.close) / tick.close * 100.0 : 0.0;
            q.oi = tick.oi;
            q.timestamp = tick.exchange_timestamp.isValid() ? tick.exchange_timestamp.toMSecsSinceEpoch() : 0;
            // Market depth arrives only on the 184-byte "full" packet (the selected
            // symbol is subscribed in full mode; the rest stay "quote" for latency).
            const bool has_depth = tick.bids[0].price > 0.0 || tick.asks[0].price > 0.0;
            if (has_depth) {
                q.bid = tick.bids[0].price; // best bid/ask feed the ticker + order matcher
                q.ask = tick.asks[0].price;
            }
            quote_cache_[q.symbol] = q;
            emit quote_updated(account_id_, q.symbol, q);

            // Publish the 5-level order book for the selected symbol (single-symbol
            // depth table). Only the full-mode symbol carries depth, so this fires
            // for it alone; the selected-symbol guard mirrors the Fyers path.
            if (has_depth && q.symbol.compare(selected_symbol_, Qt::CaseInsensitive) == 0) {
                QVector<QPair<double, double>> bids, asks;
                QVector<int> bid_orders, ask_orders;
                for (const auto& b : tick.bids)
                    if (b.price > 0.0) { bids.append({b.price, double(b.quantity)}); bid_orders.append(b.orders); }
                for (const auto& a : tick.asks)
                    if (a.price > 0.0) { asks.append({a.price, double(a.quantity)}); ask_orders.append(a.orders); }
                const double best_bid = bids.isEmpty() ? 0.0 : bids.first().first;
                const double best_ask = asks.isEmpty() ? 0.0 : asks.first().first;
                double spread = 0.0, spread_pct = 0.0;
                if (best_bid > 0.0 && best_ask > 0.0) {
                    spread = best_ask - best_bid;
                    spread_pct = (spread / best_bid) * 100.0;
                }
                LOG_INFO(ADS_TAG, QString("Zerodha WS depth: %1 bids, %2 asks for %3")
                                      .arg(bids.size()).arg(asks.size()).arg(q.symbol));
                emit orderbook_fetched(account_id_, bids, asks, spread, spread_pct, bid_orders, ask_orders);
            }
        });
        connect(zws, &ZerodhaWebSocket::connected, this, [this]() {
            LOG_INFO(ADS_TAG, QString("Zerodha WS connected for %1").arg(account_id_));
            ws_permission_denied_ = false; // streaming is permitted after all
            emit connection_state_changed(account_id_, ConnectionState::Connected);
            ws_resubscribe(); // resolves current symbols → tokens and subscribes
        });
        connect(zws, &ZerodhaWebSocket::disconnected, this, [this]() {
            emit connection_state_changed(account_id_, ConnectionState::Disconnected);
        });
        connect(zws, &ZerodhaWebSocket::error_occurred, this, [this](const QString& e) {
            LOG_ERROR(ADS_TAG, QString("Zerodha WS error: %1").arg(e));
            // A 403/Forbidden on the KiteTicker handshake is a permission verdict,
            // not a transient network error: Kite refuses the streaming socket when
            // the API key has no active Kite Connect (market-data) subscription —
            // even though the *same* access_token authenticates the REST account
            // APIs (profile/holdings 200, /quote and ws.kite.trade 403). The retry
            // loop is futile and otherwise leaves the user staring at blank quote
            // tables, so surface the cause once via the account's Error state.
            if (!ws_permission_denied_ &&
                (e.contains(QStringLiteral("403")) ||
                 e.contains(QStringLiteral("Forbidden"), Qt::CaseInsensitive))) {
                ws_permission_denied_ = true;
                QPointer<AccountDataStream> self = this;
                QMetaObject::invokeMethod(this, [self]() {
                    if (!self)
                        return;
                    const QString msg = QStringLiteral(
                        "Live market data unavailable — Kite refused the streaming "
                        "connection (403). This Kite API key has no active Kite Connect "
                        "subscription. Account data still works; live quotes and "
                        "streaming require an active Kite Connect plan for this key.");
                    AccountManager::instance().set_connection_state(
                        self->account_id_, ConnectionState::Error, msg);
                    LOG_WARN(ADS_TAG,
                             QString("Zerodha streaming denied (403) for %1 — no Kite "
                                     "Connect market-data subscription for this API key")
                                 .arg(self->account_id_));
                    emit self->connection_state_changed(self->account_id_, ConnectionState::Error);
                }, Qt::QueuedConnection);
                return;
            }
            check_token_expiry(e);
        });
        zws->open();
        return;
    }

    // ── Phase 2 broker WebSocket adapters (shared BrokerWebSocketBase) ─────────
    // Helper: collect the current selected + watchlist symbols (deduped).
    auto current_symbols = [this]() {
        QStringList syms;
        if (!selected_symbol_.isEmpty())
            syms.append(selected_symbol_);
        for (const QString& s : subscribed_symbols()) {
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

    if (broker_id_ == "icicidirect") {
        auto creds = AccountManager::instance().load_credentials(account_id_);
        // access_token = base64("user_id:session_key") from the Breeze session flow.
        if (creds.access_token.isEmpty()) {
            LOG_WARN(ADS_TAG, QString("ICICI WS: missing session token for %1").arg(account_id_));
            return;
        }
        auto* iws = new IciciDirectWebSocket(creds.access_token, creds.user_id, this);
        ws_ = iws;
        wire_base_ws(iws);
        connect(iws, &IciciDirectWebSocket::connected, this, [this, current_symbols, resolve_tokens]() {
            LOG_INFO(ADS_TAG, QString("ICICI WS connected for %1").arg(account_id_));
            if (auto* w = qobject_cast<IciciDirectWebSocket*>(ws_))
                w->subscribe(resolve_tokens(current_symbols()));
        });
        iws->open();
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
    if (feed_token.isEmpty() || client_code.isEmpty()) {
        LOG_WARN(ADS_TAG, QString("AngelOne WS: missing feed_token/client_code for %1").arg(account_id_));
        return;
    }

    auto* aows = new AngelOneWebSocket(creds.api_key, client_code, feed_token, this);
    ws_ = aows;
    connect(aows, &AngelOneWebSocket::tick_received, this, [this](const AoTick& tick) {
        // SmartStream ticks carry a token string (and the adapter may set
        // tick.symbol). Resolve to a normalised symbol, preferring the adapter's
        // value, else the master lookup. Drop unresolved / non-positive ticks.
        QString sym = tick.symbol;
        if (sym.isEmpty()) {
            bool ok = false;
            const quint32 tok = tick.token.toUInt(&ok);
            if (ok) {
                auto inst = InstrumentService::instance().find_by_token(tok, broker_id_);
                if (inst.has_value())
                    sym = inst->symbol;
            }
        }
        if (sym.isEmpty() || tick.ltp <= 0.0)
            return;
        ++ws_tick_count_;
        BrokerQuote q;
        q.symbol = sym;
        q.ltp = tick.ltp;
        q.open = tick.open;
        q.high = tick.high;
        q.low = tick.low;
        q.close = tick.close;
        q.volume = static_cast<double>(tick.volume);
        q.change = tick.close > 0 ? tick.ltp - tick.close : 0.0;
        q.change_pct = tick.close > 0 ? (tick.ltp - tick.close) / tick.close * 100.0 : 0.0;
        q.oi = tick.oi;
        q.timestamp = tick.exchange_timestamp.isValid() ? tick.exchange_timestamp.toMSecsSinceEpoch() : 0;
        quote_cache_[q.symbol] = q;
        emit quote_updated(account_id_, q.symbol, q);
    });
    connect(aows, &AngelOneWebSocket::connected, this, [this]() {
        LOG_INFO(ADS_TAG, QString("AngelOne WS connected for %1").arg(account_id_));
        emit connection_state_changed(account_id_, ConnectionState::Connected);
        ws_resubscribe(); // resolves current symbols → {token, exchange_type} and subscribes
    });
    connect(aows, &AngelOneWebSocket::disconnected, this, [this]() {
        emit connection_state_changed(account_id_, ConnectionState::Disconnected);
    });
    connect(aows, &AngelOneWebSocket::error_occurred, this, [this](const QString& e) {
        LOG_ERROR(ADS_TAG, QString("AngelOne WS error: %1").arg(e));
        check_token_expiry(e);
    });
    aows->open();
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
    if (auto* zws = qobject_cast<ZerodhaWebSocket*>(ws_))
        return zws->is_connected() && ws_tick_count_ > 0;
    if (auto* aows = qobject_cast<AngelOneWebSocket*>(ws_))
        return aows->is_connected() && ws_tick_count_ > 0;
    if (auto* b = qobject_cast<BrokerWebSocketBase*>(ws_))
        return b->is_connected() && ws_tick_count_ > 0;
    return false;
}

bool AccountDataStream::ws_connected() const {
    // Like ws_active() but without the tick-count requirement — used to decide
    // whether a (re)subscribe can be sent right now.
    if (!ws_)
        return false;
    if (auto* fws = qobject_cast<FyersWebSocket*>(ws_))
        return fws->is_connected();
    if (auto* aws = qobject_cast<AlpacaWebSocket*>(ws_))
        return aws->is_connected();
    if (auto* zws = qobject_cast<ZerodhaWebSocket*>(ws_))
        return zws->is_connected();
    if (auto* aows = qobject_cast<AngelOneWebSocket*>(ws_))
        return aows->is_connected();
    if (auto* b = qobject_cast<BrokerWebSocketBase*>(ws_))
        return b->is_connected();
    return false;
}

void AccountDataStream::ws_resubscribe() {
    if (auto* fws = qobject_cast<FyersWebSocket*>(ws_)) {
        auto to_fyers = [](const QString& sym) {
            // Mirror the connect-handler mapping: strip any exchange prefix and
            // route F&O options onto Fyers' "NSE:" segment (handles the paper
            // "NFO:...CE" prefix too), equities to "NSE:<sym>-EQ".
            QString core = sym;
            const int colon = core.indexOf(QLatin1Char(':'));
            if (colon >= 0)
                core = core.mid(colon + 1);
            if (is_fyers_fno_symbol(core))
                return QStringLiteral("NSE:") + core;
            if (colon >= 0)
                return sym;
            return QStringLiteral("NSE:") + core + QStringLiteral("-EQ");
        };
        QStringList fyers_symbols;
        if (!selected_symbol_.isEmpty())
            fyers_symbols.append(to_fyers(selected_symbol_));
        for (const QString& s : subscribed_symbols())
            fyers_symbols.append(to_fyers(s));
        fyers_symbols.removeDuplicates();
        {
            QStringList fno;
            for (const auto& s : fyers_symbols)
                if (is_fyers_fno_symbol(s.section(QLatin1Char(':'), -1)))
                    fno << s;
            LOG_INFO("subdbg", QString("[ws_resubscribe] consumers=%1 → %2 fyers syms (F&O: [%3])")
                                   .arg(subscribed_symbols().size())
                                   .arg(fyers_symbols.size())
                                   .arg(fno.join(',')));
        }
        fws->set_subscriptions(fyers_symbols);
    }

    if (auto* aws = qobject_cast<AlpacaWebSocket*>(ws_)) {
        QStringList symbols;
        if (!selected_symbol_.isEmpty())
            symbols.append(selected_symbol_);
        for (const QString& s : subscribed_symbols()) {
            if (!symbols.contains(s))
                symbols.append(s);
        }
        aws->set_subscriptions(symbols);
    }

    // Zerodha KiteTicker — resolve current symbols → numeric instrument tokens.
    if (auto* zws = qobject_cast<ZerodhaWebSocket*>(ws_)) {
        QStringList symbols;
        if (!selected_symbol_.isEmpty())
            symbols.append(selected_symbol_);
        for (const QString& s : subscribed_symbols())
            if (!symbols.contains(s))
                symbols.append(s);
        symbols.removeDuplicates();
        QVector<quint32> tokens;
        auto& svc = InstrumentService::instance();
        for (const auto& s : symbols) {
            const QString exch = (s == selected_symbol_ && !selected_exchange_.isEmpty())
                                     ? selected_exchange_
                                     : QStringLiteral("NSE");
            auto tok = svc.instrument_token(s, exch, broker_id_);
            if (tok.has_value() && *tok > 0)
                tokens.append(static_cast<quint32>(*tok));
        }
        // Subscribe the selected symbol in "full" mode (5-level depth for the order
        // book); every other symbol stays "quote" (lighter, no depth) for latency.
        quint32 sel_token = 0;
        if (!selected_symbol_.isEmpty()) {
            const QString exch = selected_exchange_.isEmpty() ? QStringLiteral("NSE") : selected_exchange_;
            auto t = svc.instrument_token(selected_symbol_, exch, broker_id_);
            if (t.has_value() && *t > 0)
                sel_token = static_cast<quint32>(*t);
        }
        zws->set_full_mode_token(sel_token);
        zws->set_subscriptions(tokens);
        return;
    }

    // Angel One SmartStream — resolve symbols → {token string, exchange segment}.
    if (auto* aows = qobject_cast<AngelOneWebSocket*>(ws_)) {
        QStringList symbols;
        if (!selected_symbol_.isEmpty())
            symbols.append(selected_symbol_);
        for (const QString& s : subscribed_symbols())
            if (!symbols.contains(s))
                symbols.append(s);
        symbols.removeDuplicates();
        QVector<AngelOneWebSocket::Subscription> subs;
        auto& svc = InstrumentService::instance();
        for (const auto& s : symbols) {
            const QString exch = (s == selected_symbol_ && !selected_exchange_.isEmpty())
                                     ? selected_exchange_
                                     : QStringLiteral("NSE");
            auto tok = svc.instrument_token(s, exch, broker_id_);
            if (tok.has_value() && *tok > 0)
                subs.append({QString::number(*tok), ao_exchange_type(exch)});
        }
        aows->set_subscriptions(subs);
        return;
    }

    // Generic Phase 2 adapters (shared BrokerWebSocketBase). Rebuild the symbol
    // set wholesale: drop existing subscriptions, then subscribe the current set.
    if (auto* b = qobject_cast<BrokerWebSocketBase*>(ws_)) {
        QStringList symbols;
        if (!selected_symbol_.isEmpty())
            symbols.append(selected_symbol_);
        for (const QString& s : subscribed_symbols()) {
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
