// src/screens/crypto_trading/CryptoTradingScreen_Refresh.cpp
//
// Feed-mode toggling (apply_feed_mode, flush_ws_updates) and the per-panel
// refresh slots (ticker, orderbook, portfolio, watchlist, market_info,
// candles, live_data).
//
// Part of the partial-class split of CryptoTradingScreen.cpp.

#include "screens/crypto_trading/CryptoTradingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "screens/crypto_trading/CryptoBottomPanel.h"
#include "screens/crypto_trading/CryptoChart.h"
#include "screens/crypto_trading/CryptoCredentials.h"
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "screens/crypto_trading/CryptoOrderEntry.h"
#include "screens/crypto_trading/CryptoTickerBar.h"
#include "screens/crypto_trading/CryptoWatchlist.h"
#include "trading/ExchangeService.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/exchanges/kraken/KrakenWsClient.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QCompleter>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPointer>
#include <QSplitter>
#include <QStringListModel>
#include <QStyle>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::crypto;

static const QString TAG = "CryptoTrading";


void CryptoTradingScreen::apply_feed_mode(bool ws_connected) {
    // WS-only mode: no REST polling fallbacks. We keep this method as a
    // logging/state-tracking hook so the WS status pill in the command bar
    // can react to drops. Polling timers are not (re)started here.
    const int desired = ws_connected ? 1 : 0;
    if (desired == last_ws_state_)
        return;
    last_ws_state_ = desired;
    LOG_INFO(TAG, ws_connected ? "WS connected" : "WS disconnected (waiting for reconnect)");
}

void CryptoTradingScreen::flush_ws_updates() {
    apply_feed_mode(ExchangeService::instance().is_ws_connected());

    // Flush primary symbol ticker → header bar + order entry
    if (has_pending_primary_) {
        const auto& t = pending_primary_ticker_;
        ticker_bar_->update_data(t.last, t.percentage, t.high, t.low, t.base_volume,
                                 ExchangeService::instance().is_ws_connected());
        if (t.bid > 0 && t.ask > 0)
            ticker_bar_->update_bid_ask(t.bid, t.ask, std::abs(t.ask - t.bid));
        order_entry_->set_current_price(t.last);
        has_pending_primary_ = false;
    }

    // Flush latest orderbook snapshot (one repaint per flush tick, not per WS msg)
    if (has_pending_orderbook_) {
        const auto& ob = pending_orderbook_;
        orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
        bottom_panel_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
        has_pending_orderbook_ = false;
    }

    // Flush buffered candles in order (chart handles intrabar updates via timestamp match)
    if (!pending_candles_.isEmpty()) {
        for (const auto& c : pending_candles_)
            chart_->append_candle(c);
        pending_candles_.clear();
    }

    // Flush buffered trade entries — one loop instead of one invokeMethod per trade
    if (!pending_trades_.isEmpty()) {
        for (const auto& entry : pending_trades_)
            bottom_panel_->add_trade_entry(entry);
        pending_trades_.clear();
    }

    // Flush all accumulated tickers → watchlist + paper trading engine
    if (!pending_tickers_.isEmpty()) {
        QVector<TickerData> batch;
        batch.reserve(pending_tickers_.size());
        for (auto it = pending_tickers_.constBegin(); it != pending_tickers_.constEnd(); ++it)
            batch.append(it.value());
        pending_tickers_.clear();
        watchlist_->update_prices(batch);

        // Live mark-to-market on the positions table — works in paper AND live
        // mode because it patches columns 4 (Current) + 5 (P&L) in place from
        // the WS tick, no SQLite writes, no daemon round-trip.
        QHash<QString, double> last_prices;
        last_prices.reserve(batch.size());
        for (const auto& t : batch) {
            if (t.last > 0.0)
                last_prices.insert(t.symbol, t.last);
        }
        if (!last_prices.isEmpty())
            bottom_panel_->update_position_prices(last_prices);

        // Feed WS prices into paper trading engine on a worker thread — every tick
        // would otherwise do 3N+ SQLite ops on the UI thread (price update, order
        // match, SL/TP check) plus 1-5 queries for UI refresh at 10Hz.
        if (trading_mode_ == TradingMode::Paper && !portfolio_id_.isEmpty()) {
            // Drop this tick if the previous batch is still running — keeps us
            // from queueing work faster than the DB can drain it.
            bool expected = false;
            if (paper_bookkeeping_in_flight_.compare_exchange_strong(expected, true)) {
                QPointer<CryptoTradingScreen> self = this;
                const QString pid = portfolio_id_;
                (void)QtConcurrent::run([self, pid, batch]() {
                    struct Result {
                        QVector<PtPosition> positions;
                        bool fill_occurred = false;
                        PtPortfolio portfolio;
                        QVector<PtOrder> orders;
                        QVector<PtTrade> trades;
                        PtStats stats;
                    };
                    Result r;
                    try {
                        const int orders_before = pt_get_orders(pid, "open").size();
                        for (const auto& ticker : batch) {
                            if (ticker.last <= 0)
                                continue;
                            pt_update_position_price(pid, ticker.symbol, ticker.last);
                            PriceData pd;
                            pd.last = ticker.last;
                            pd.bid = ticker.bid;
                            pd.ask = ticker.ask;
                            pd.timestamp = ticker.timestamp;
                            OrderMatcher::instance().check_orders(ticker.symbol, pd, pid);
                            OrderMatcher::instance().check_sl_tp_triggers(pid, ticker.symbol, ticker.last);
                        }
                        r.positions = pt_get_positions(pid);
                        const int orders_after = pt_get_orders(pid, "open").size();
                        r.fill_occurred = orders_after < orders_before;
                        if (r.fill_occurred) {
                            r.portfolio = pt_get_portfolio(pid);
                            r.orders = pt_get_orders(pid);
                            r.trades = pt_get_trades(pid, 50);
                            r.stats = pt_get_stats(pid);
                        }
                    } catch (...) {
                        if (self)
                            LOG_WARN("CryptoTradingScreen", "Paper bookkeeping worker threw");
                    }

                    if (!self)
                        return;  // widget destroyed — nothing to reset
                    QMetaObject::invokeMethod(self, [self, r]() {
                        if (!self)
                            return;
                        self->paper_bookkeeping_in_flight_.store(false);
                        self->bottom_panel_->set_positions(r.positions);
                        if (r.fill_occurred) {
                            self->portfolio_ = r.portfolio;
                            self->order_entry_->set_balance(r.portfolio.balance);
                            self->bottom_panel_->set_orders(r.orders);
                            self->bottom_panel_->set_trades(r.trades);
                            self->bottom_panel_->set_stats(r.stats);
                        }
                    }, Qt::QueuedConnection);
                });
            }
        }
        // Polling timers are managed centrally by apply_feed_mode() at the top
        // of this function — no per-flush stop/start here.
    }
}

// ============================================================================
// Refresh Functions
// ============================================================================

void CryptoTradingScreen::refresh_ticker() {
    if (!initialized_)
        return;
    auto& es = ExchangeService::instance();
    const auto cached = es.get_cached_price(selected_symbol_);
    if (cached.last > 0) {
        ticker_bar_->update_data(cached.last, cached.percentage, cached.high, cached.low, cached.base_volume,
                                 es.is_ws_connected());
        if (cached.bid > 0 && cached.ask > 0)
            ticker_bar_->update_bid_ask(cached.bid, cached.ask, std::abs(cached.ask - cached.bid));
        order_entry_->set_current_price(cached.last);
        return;
    }

    // Cache miss + WS not connected — prime the ticker on a worker so the UI
    // doesn't stay empty waiting for the first WS tick after an exchange
    // switch or cold start. Skip if the WS is live: the tick will arrive.
    if (es.is_ws_connected())
        return;
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_;
    (void)QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        auto ticker = ExchangeService::instance().fetch_ticker(symbol);
        if (!self || ticker.last <= 0)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, symbol, ticker]() {
                if (!self || self->selected_symbol_ != symbol)
                    return;
                auto& es = ExchangeService::instance();
                self->ticker_bar_->update_data(ticker.last, ticker.percentage, ticker.high, ticker.low,
                                               ticker.base_volume, es.is_ws_connected());
                if (ticker.bid > 0 && ticker.ask > 0)
                    self->ticker_bar_->update_bid_ask(ticker.bid, ticker.ask, std::abs(ticker.ask - ticker.bid));
                self->order_entry_->set_current_price(ticker.last);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_orderbook() {
    if (!initialized_)
        return;
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self)
            return;
        auto ob = ExchangeService::instance().fetch_orderbook(self->selected_symbol_, OB_MAX_DISPLAY_LEVELS);
        QMetaObject::invokeMethod(
            self,
            [self, ob]() {
                if (!self)
                    return;
                self->orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
                self->bottom_panel_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_portfolio() {
    if (portfolio_id_.isEmpty())
        return;
    if (trading_mode_ == TradingMode::Live)
        return;

    // Perform 5 SQLite reads on a worker thread — with 1.5s timer + order
    // placement/cancel flushes, this was one of the loudest UI-thread stalls.
    QPointer<CryptoTradingScreen> self = this;
    const QString pid = portfolio_id_;
    (void)QtConcurrent::run([self, pid]() {
        struct Snapshot {
            PtPortfolio portfolio;
            QVector<PtPosition> positions;
            QVector<PtOrder> orders;
            QVector<PtTrade> trades;
            PtStats stats;
            bool ok = false;
        };
        Snapshot s;
        try {
            s.portfolio = pt_get_portfolio(pid);
            s.positions = pt_get_positions(pid);
            s.orders = pt_get_orders(pid);
            s.trades = pt_get_trades(pid, 50);
            s.stats = pt_get_stats(pid);
            s.ok = true;
        } catch (...) {
            if (self)
                LOG_WARN("CryptoTradingScreen", "Exception refreshing portfolio panel");
        }
        if (!self || !s.ok)
            return;
        QMetaObject::invokeMethod(self, [self, s]() {
            if (!self)
                return;
            self->portfolio_ = s.portfolio;
            self->order_entry_->set_balance(s.portfolio.balance);
            self->bottom_panel_->set_positions(s.positions);
            self->bottom_panel_->set_orders(s.orders);
            self->bottom_panel_->set_trades(s.trades);
            self->bottom_panel_->set_stats(s.stats);
            LOG_INFO(TAG, QString("Paper portfolio refreshed: %1 positions, %2 orders, %3 trades, balance=%4")
                              .arg(s.positions.size())
                              .arg(s.orders.size())
                              .arg(s.trades.size())
                              .arg(s.portfolio.balance, 0, 'f', 2));
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_watchlist() {
    if (!initialized_)
        return;
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self)
            return;
        auto tickers = ExchangeService::instance().fetch_tickers(self->watchlist_symbols_);
        QMetaObject::invokeMethod(
            self,
            [self, tickers]() {
                if (!self)
                    return;
                self->watchlist_->update_prices(tickers);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_market_info() {
    if (!initialized_)
        return;
    // Fire the two daemon calls on separate worker threads so their UI-thread
    // return paths run independently. The daemon serializes via its own mutex,
    // but the funding-rate branch updates the ticker mark price as soon as it
    // lands instead of waiting for open-interest too. Each worker writes its
    // portion into market_info_cache_ on the UI thread, then pushes the union
    // to the bottom panel — cache access is UI-thread-only, no lock needed.
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_;

    (void)QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        auto fr = ExchangeService::instance().fetch_funding_rate(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, symbol, fr]() {
                if (!self)
                    return;
                if (self->selected_symbol_ != symbol)
                    return; // user switched symbols — discard stale result
                self->market_info_cache_.funding_rate = fr.funding_rate;
                self->market_info_cache_.mark_price = fr.mark_price;
                self->market_info_cache_.index_price = fr.index_price;
                self->market_info_cache_.next_funding_time = fr.next_funding_timestamp;
                self->market_info_cache_.has_data = true;
                self->ticker_bar_->update_mark_price(fr.mark_price, fr.index_price);
                self->bottom_panel_->set_market_info(self->market_info_cache_);
            },
            Qt::QueuedConnection);
    });

    (void)QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        auto oi = ExchangeService::instance().fetch_open_interest(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, symbol, oi]() {
                if (!self)
                    return;
                if (self->selected_symbol_ != symbol)
                    return; // user switched symbols — discard stale result
                self->market_info_cache_.open_interest = oi.open_interest;
                self->market_info_cache_.open_interest_value = oi.open_interest_value;
                self->market_info_cache_.has_data = true;
                self->bottom_panel_->set_market_info(self->market_info_cache_);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_candles() {
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void CryptoTradingScreen::refresh_live_data() {
    if (trading_mode_ != TradingMode::Live)
        return;
    // Skip this tick entirely if any of the 4 live fetches from the previous
    // tick are still in-flight — prevents burst-stacking against the exchange
    // API and against the daemon's request pipeline.
    if (live_inflight_.load() > 0)
        return;

    live_inflight_.store(4);
    async_fetch_live_positions();
    async_fetch_live_orders();
    async_fetch_live_balance();
    async_fetch_my_trades();
}

} // namespace fincept::screens
