#include "trading/AccountDataStream.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"

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
constexpr int ADS_QUOTE_POLL_MS = 5000;
constexpr int ADS_PORTFOLIO_POLL_MS = 3000;
constexpr int ADS_WATCHLIST_POLL_MS = 10000;
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

void AccountDataStream::on_quote_timer() {
    if (ws_active())
        return; // WS provides real-time quotes
    async_fetch_quote();
}

void AccountDataStream::on_portfolio_timer() {
    if (portfolio_fetching_.exchange(true))
        return;
    async_fetch_positions();
    async_fetch_holdings();
    async_fetch_orders();
    async_fetch_funds();
    portfolio_fetching_ = false;
}

void AccountDataStream::on_watchlist_timer() {
    if (ws_active())
        return;
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

    QtConcurrent::run([self, acct_id, bid, symbol]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) {
            if (self) self->quote_fetching_ = false;
            return;
        }
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) {
            if (self) self->quote_fetching_ = false;
            return;
        }
        auto result = broker->get_quotes(creds, {symbol});
        if (self) self->quote_fetching_ = false;
        if (!result.success || !result.data || result.data->isEmpty()) {
            if (result.error.startsWith("[TOKEN_EXPIRED]") && self)
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) emit self->token_expired(self->account_id_);
                }, Qt::QueuedConnection);
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

    QtConcurrent::run([self, acct_id, bid]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) return;
        auto result = broker->get_positions(creds);
        if (!result.success || !result.data) {
            if (result.error.startsWith("[TOKEN_EXPIRED]") && self)
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) emit self->token_expired(self->account_id_);
                }, Qt::QueuedConnection);
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

    QtConcurrent::run([self, acct_id, bid]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) return;
        auto result = broker->get_holdings(creds);
        if (!result.success || !result.data) {
            if (result.error.startsWith("[TOKEN_EXPIRED]") && self)
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) emit self->token_expired(self->account_id_);
                }, Qt::QueuedConnection);
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

    QtConcurrent::run([self, acct_id, bid]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) return;
        auto result = broker->get_orders(creds);
        if (!result.success || !result.data) {
            if (result.error.startsWith("[TOKEN_EXPIRED]") && self)
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) emit self->token_expired(self->account_id_);
                }, Qt::QueuedConnection);
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

    QtConcurrent::run([self, acct_id, bid]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) return;
        auto result = broker->get_funds(creds);
        if (!result.success || !result.data) {
            if (result.error.startsWith("[TOKEN_EXPIRED]") && self)
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) emit self->token_expired(self->account_id_);
                }, Qt::QueuedConnection);
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

    QtConcurrent::run([self, acct_id, bid, symbols]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) return;
        auto result = broker->get_quotes(creds, symbols.toVector());
        if (!result.success || !result.data) return;
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

    QtConcurrent::run([self, acct_id, bid, symbol, timeframe]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) {
            if (self) self->candles_fetching_ = false;
            return;
        }
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) {
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
            if (result.error.startsWith("[TOKEN_EXPIRED]") && self)
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) emit self->token_expired(self->account_id_);
                }, Qt::QueuedConnection);
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

    QtConcurrent::run([self, acct_id, bid, symbol]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) return;

        // Try L2 depth from historical quotes
        const QString today = QDate::currentDate().toString("yyyy-MM-dd");
        auto result = broker->get_historical_quotes_single(creds, symbol, today + "T00:00:00Z", "", 1000);

        if (!result.success || !result.data || result.data->isEmpty()) {
            // Fallback to snapshot bid/ask
            auto snap = broker->get_quotes(creds, {symbol});
            if (!snap.success || !snap.data || snap.data->isEmpty()) return;
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
                emit self->orderbook_fetched(acct_id, bids, asks, spread, spread_pct);
            }, Qt::QueuedConnection);
            return;
        }

        // Aggregate L2
        QMap<double, double> bid_map, ask_map;
        for (const auto& q : *result.data) {
            if (q.bid > 0) bid_map[q.bid] += q.bid_size > 0 ? q.bid_size : 1.0;
            if (q.ask > 0) ask_map[q.ask] += q.ask_size > 0 ? q.ask_size : 1.0;
        }
        QVector<QPair<double, double>> bids, asks;
        auto bid_keys = bid_map.keys();
        std::sort(bid_keys.begin(), bid_keys.end(), std::greater<double>());
        for (const double p : bid_keys.mid(0, 10))
            bids.append({p, bid_map[p]});
        auto ask_keys = ask_map.keys();
        std::sort(ask_keys.begin(), ask_keys.end());
        for (const double p : ask_keys.mid(0, 10))
            asks.append({p, ask_map[p]});
        if (bids.isEmpty() || asks.isEmpty()) return;

        const double best_bid = bids.first().first;
        const double best_ask = asks.first().first;
        const double spread = best_ask - best_bid;
        const double spread_pct = best_bid > 0 ? (spread / best_bid) * 100.0 : 0.0;
        QMetaObject::invokeMethod(self, [self, acct_id, bids, asks, spread, spread_pct]() {
            if (!self) return;
            emit self->orderbook_fetched(acct_id, bids, asks, spread, spread_pct);
        }, Qt::QueuedConnection);
    });
}

void AccountDataStream::fetch_time_sales(const QString& symbol) {
    const QString acct_id = account_id_;
    const QString bid = broker_id_;
    QPointer<AccountDataStream> self = this;

    QtConcurrent::run([self, acct_id, bid, symbol]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) return;
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

    QtConcurrent::run([self, acct_id, bid, symbol]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        if (creds.api_key.isEmpty()) return;
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

    QtConcurrent::run([self, acct_id, bid]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
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

    QtConcurrent::run([self, acct_id, bid]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        if (!broker) return;
        auto creds = AccountManager::instance().load_credentials(acct_id);
        auto result = broker->get_clock(creds);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, acct_id, data = *result.data]() {
            if (!self) return;
            emit self->clock_fetched(acct_id, data);
        }, Qt::QueuedConnection);
    });
}

// ── WebSocket ───────────────────────────────────────────────────────────────

void AccountDataStream::ws_init() {
    // Currently only AngelOne has WebSocket support wired up
    // Zerodha WS exists but isn't integrated into equity trading yet
    if (broker_id_ != "angelone")
        return;

    auto creds = AccountManager::instance().load_credentials(account_id_);
    if (creds.api_key.isEmpty() || creds.additional_data.isEmpty())
        return;

    // Parse feed_token and client_code from additional_data JSON
    auto doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
    if (!doc.isObject())
        return;
    auto obj = doc.object();
    const QString feed_token = obj.value("feed_token").toString();
    const QString client_code = obj.value("client_code").toString();
    if (feed_token.isEmpty() || client_code.isEmpty())
        return;

    // Dynamically create AngelOneWebSocket (avoids hard header dependency)
    // The AngelOneWebSocket class is in trading/websocket/AngelOneWebSocket.h
    // For now, we include it directly — this can be abstracted later
    // NOTE: This is deferred to Phase 5 when we wire the screen to DataStreamManager.
    // At this point, ws_ stays null and polling is used.
    // The ws_init implementation will be completed when EquityTradingScreen is refactored.
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
    // Will be implemented when WS is wired in Phase 5
    return false;
}

void AccountDataStream::ws_resubscribe() {
    // Will be implemented when WS is wired in Phase 5
}

} // namespace fincept::trading
