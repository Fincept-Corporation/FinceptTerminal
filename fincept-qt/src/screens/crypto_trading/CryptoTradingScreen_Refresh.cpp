// src/screens/crypto_trading/CryptoTradingScreen_Refresh.cpp
//
// Feed-mode toggling (apply_feed_mode, flush_ws_updates) and the per-panel
// refresh slots (ticker, orderbook, portfolio, watchlist, market_info,
// candles, live_data).
//
// Part of the partial-class split of CryptoTradingScreen.cpp.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "screens/crypto_trading/CryptoBottomPanel.h"
#include "screens/crypto_trading/CryptoChart.h"
#include "screens/crypto_trading/CryptoCredentials.h"
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "screens/crypto_trading/CryptoOrderEntry.h"
#include "screens/crypto_trading/CryptoTickerBar.h"
#include "screens/crypto_trading/CryptoTradingScreen.h"
#include "screens/crypto_trading/CryptoWatchlist.h"
#include "storage/repositories/PaperTradingRepository.h"
#include "trading/ExchangeService.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"

#include <QCompleter>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <QSplitter>
#include <QStringListModel>
#include <QStyle>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::crypto;

static const QString TAG = "CryptoTrading";

// Resting paper orders (pending / partial) for the blotter, filtered in SQL.
// The blotter's set_orders() shows only these, so reading every historical
// order every 1.5 s and dropping most of them in C++ was pure waste.
static QVector<PtOrder> crypto_open_paper_orders(const QString& portfolio_id) {
    auto r = PaperTradingRepository::instance().get_open_orders(portfolio_id);
    return r.is_ok() ? r.value() : QVector<PtOrder>{};
}

// Before the engine may book cash for `symbol` (a resting order filling, an
// SL / TP closing a position): the pair's FX factor into the paper account's
// currency, from a real rate. Kept current from FxRateService while a rate is
// known; without one the factor set earlier this run (from a real rate) is
// kept; with neither, false — the caller does not match that symbol on this tick
// (the order keeps resting), so nothing is ever booked at 1:1. Same currency, or
// a symbol whose quote asset is unknown: true. Worker thread.
static bool crypto_paper_fx_ready(const QString& portfolio_id, const QString& symbol, const QString& account_ccy) {
    const QString px_ccy = crypto::pair_quote_ccy(symbol);
    const QString acct = account_ccy.trimmed().toUpper();
    if (px_ccy.isEmpty() || acct.isEmpty() || px_ccy == acct)
        return true;
    if (const auto f = PaperMarkService::fx_factor(px_ccy, acct)) {
        pt_set_symbol_fx(portfolio_id, symbol, *f);
        return true;
    }
    return pt_has_symbol_fx(portfolio_id, symbol);
}

// The distinct quote assets of a paper portfolio's fills, for the TURNOVER tile
// (the engine sums price × quantity in each fill's own quote asset). Re-read
// only when the number of fills changes. Worker thread.
static QStringList crypto_paper_quote_assets(const QString& portfolio_id, qint64 total_trades) {
    static QMutex mutex;
    static QHash<QString, QPair<qint64, QStringList>> cache;
    {
        QMutexLocker lock(&mutex);
        const auto it = cache.constFind(portfolio_id);
        if (it != cache.constEnd() && it->first == total_trades)
            return it->second;
    }
    QStringList quotes;
    for (const auto& t : pt_get_trades(portfolio_id, -1)) { // -1: every fill (SQLite LIMIT -1)
        const QString q = crypto::pair_quote_ccy(t.symbol);
        if (!q.isEmpty() && !quotes.contains(q))
            quotes << q;
    }
    QMutexLocker lock(&mutex);
    cache.insert(portfolio_id, qMakePair(total_trades, quotes));
    return quotes;
}

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
    apply_feed_mode(session()->is_ws_connected());

    // Flush primary symbol ticker → header bar + order entry
    if (has_pending_primary_) {
        const auto& t = pending_primary_ticker_;
        ticker_bar_->update_data(t.last, t.percentage, t.high, t.low, t.base_volume,
                                 session()->is_ws_connected());
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
        // First snapshot since the last reset / error: the book replaces its skeleton.
        if (!ob_feed_reported_ && (!ob.bids.isEmpty() || !ob.asks.isEmpty())) {
            ob_feed_reported_ = true;
            ob_pane_->feed_data(QJsonValue(true));
        }
    }

    // Flush buffered candles: keep only the newest update of each bar (in
    // arrival order) and hand them over in ONE call, so the chart re-fits its
    // axes and recomputes every indicator overlay once per flush instead of
    // once per WS message.
    if (!pending_candles_.isEmpty()) {
        QVector<Candle> bars;
        bars.reserve(pending_candles_.size());
        for (const auto& c : pending_candles_) {
            const auto same_bar = std::find_if(bars.rbegin(), bars.rend(),
                                               [&c](const Candle& b) { return b.timestamp == c.timestamp; });
            if (same_bar != bars.rend())
                *same_bar = c;
            else
                bars.append(c);
        }
        pending_candles_.clear();
        chart_->append_candles(bars);
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
                const QString book_ccy = portfolio_.currency; // the paper account's cash currency
                (void)QtConcurrent::run([self, pid, book_ccy, batch]() {
                    struct Result {
                        QVector<PtPosition> positions;
                        bool fill_occurred = false;
                        PtPortfolio portfolio;
                        QVector<PtOrder> orders;
                        QVector<PtTrade> trades;
                        PtStats stats;
                        QStringList quote_assets;
                    };
                    Result r;
                    try {
                        // Resting paper orders are stored with status "pending"/"partial"
                        // (never "open"), and a resting order that fills drops out of that
                        // set. SL/TP triggers close positions without touching orders, so
                        // also watch the newest trade id — either change means a fill.
                        const auto resting_orders = [&pid]() { return crypto_open_paper_orders(pid).size(); };
                        const auto newest_trade_id = [&pid]() {
                            const auto t = pt_get_trades(pid, 1);
                            return t.isEmpty() ? QString() : t.first().id;
                        };
                        // Symbols the engine may book cash for on this tick (a resting
                        // order or an open position, i.e. an SL / TP).
                        const auto open_orders = crypto_open_paper_orders(pid);
                        QSet<QString> active;
                        for (const auto& o : open_orders)
                            active.insert(o.symbol);
                        for (const auto& p : pt_get_positions(pid))
                            active.insert(p.symbol);
                        const auto orders_before = open_orders.size();
                        const QString trade_before = newest_trade_id();
                        for (const auto& ticker : batch) {
                            if (ticker.last <= 0)
                                continue;
                            pt_update_position_price(pid, ticker.symbol, ticker.last);
                            // A fill books cash at the pair's FX factor: no real rate
                            // for a cross-currency pair yet -> do not match it on this
                            // tick (the order keeps resting; the next tick retries).
                            if (!active.contains(ticker.symbol) ||
                                !crypto_paper_fx_ready(pid, ticker.symbol, book_ccy))
                                continue;
                            PriceData pd;
                            pd.last = ticker.last;
                            pd.bid = ticker.bid;
                            pd.ask = ticker.ask;
                            pd.timestamp = ticker.timestamp;
                            OrderMatcher::instance().check_orders(ticker.symbol, pd, pid);
                            OrderMatcher::instance().check_sl_tp_triggers(pid, ticker.symbol, ticker.last);
                        }
                        r.positions = pt_get_positions(pid);
                        r.fill_occurred = resting_orders() != orders_before || newest_trade_id() != trade_before;
                        if (r.fill_occurred) {
                            r.portfolio = pt_get_portfolio(pid);
                            r.orders = crypto_open_paper_orders(pid);
                            r.trades = pt_get_trades(pid, 50);
                            r.stats = pt_get_stats(pid);
                            r.quote_assets = crypto_paper_quote_assets(pid, r.stats.total_trades);
                        }
                    } catch (...) {
                        if (self)
                            LOG_WARN("CryptoTradingScreen", "Paper bookkeeping worker threw");
                    }

                    if (!self)
                        return; // widget destroyed — nothing to reset
                    QMetaObject::invokeMethod(
                        self,
                        [self, r]() {
                            if (!self)
                                return;
                            self->paper_bookkeeping_in_flight_.store(false);
                            // Switched to LIVE while this ran: paper rows must not land
                            // in the live account views.
                            if (self->trading_mode_ != TradingMode::Paper)
                                return;
                            self->bottom_panel_->set_positions(r.positions);
                            if (r.fill_occurred) {
                                self->portfolio_ = r.portfolio;
                                self->order_entry_->set_balance(r.portfolio.balance, r.portfolio.currency);
                                self->bottom_panel_->set_orders(r.orders);
                                self->bottom_panel_->set_trades(r.trades, r.portfolio.currency);
                                self->bottom_panel_->set_stats(r.stats, r.portfolio.currency, r.quote_assets);
                            }
                        },
                        Qt::QueuedConnection);
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
    // This screen's own exchange session — never ExchangeService's process-wide
    // "current exchange", which another window may have switched.
    auto* es = session();
    const auto cached = es->get_cached_price(selected_symbol_);
    if (cached.last > 0) {
        ticker_bar_->update_data(cached.last, cached.percentage, cached.high, cached.low, cached.base_volume,
                                 es->is_ws_connected());
        if (cached.bid > 0 && cached.ask > 0)
            ticker_bar_->update_bid_ask(cached.bid, cached.ask, std::abs(cached.ask - cached.bid));
        order_entry_->set_current_price(cached.last);
        return;
    }

    // Cache miss + WS not connected — prime the ticker on a worker so the UI
    // doesn't stay empty waiting for the first WS tick after an exchange
    // switch or cold start. Skip if the WS is live: the tick will arrive.
    if (es->is_ws_connected())
        return;
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_;
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbol, ex]() {
        if (!self)
            return;
        auto ticker = ExchangeSessionManager::instance().session(ex)->fetch_ticker(symbol);
        if (!self || ticker.last <= 0)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, symbol, ex, ticker]() {
                if (!self || self->selected_symbol_ != symbol || self->exchange_id_ != ex)
                    return;
                self->ticker_bar_->update_data(ticker.last, ticker.percentage, ticker.high, ticker.low,
                                               ticker.base_volume, self->session()->is_ws_connected());
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
    const QString symbol = selected_symbol_; // snapshot on the UI thread — see refresh_ticker()
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbol, ex]() {
        if (!self)
            return;
        auto ob = ExchangeSessionManager::instance().session(ex)->fetch_orderbook(symbol, OB_MAX_DISPLAY_LEVELS);
        QMetaObject::invokeMethod(
            self,
            [self, symbol, ex, ob]() {
                if (!self || self->selected_symbol_ != symbol || self->exchange_id_ != ex)
                    return;
                self->orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
                self->bottom_panel_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
                if (!self->ob_feed_reported_ && (!ob.bids.isEmpty() || !ob.asks.isEmpty())) {
                    self->ob_feed_reported_ = true;
                    self->ob_pane_->feed_data(QJsonValue(true));
                }
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
            QStringList quote_assets;
            bool ok = false;
        };
        Snapshot s;
        try {
            s.portfolio = pt_get_portfolio(pid);
            s.positions = pt_get_positions(pid);
            s.orders = crypto_open_paper_orders(pid);
            s.trades = pt_get_trades(pid, 50);
            s.stats = pt_get_stats(pid);
            s.quote_assets = crypto_paper_quote_assets(pid, s.stats.total_trades);
            s.ok = true;
        } catch (...) {
            if (self)
                LOG_WARN("CryptoTradingScreen", "Exception refreshing portfolio panel");
        }
        if (!self || !s.ok)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, s]() {
                if (!self)
                    return;
                if (self->trading_mode_ != TradingMode::Paper)
                    return; // switched to LIVE while the read ran — keep paper rows out of live views
                self->portfolio_ = s.portfolio;
                self->order_entry_->set_balance(s.portfolio.balance, s.portfolio.currency);
                self->bottom_panel_->set_positions(s.positions);
                self->bottom_panel_->set_orders(s.orders);
                self->bottom_panel_->set_trades(s.trades, s.portfolio.currency);
                self->bottom_panel_->set_stats(s.stats, s.portfolio.currency, s.quote_assets);
                // Runs every 1.5 s — diagnostics only (P14).
                LOG_DEBUG(TAG, QString("Paper portfolio refreshed: %1 positions, %2 open orders, %3 trades, "
                                       "balance=%4")
                                   .arg(s.positions.size())
                                   .arg(s.orders.size())
                                   .arg(s.trades.size())
                                   .arg(s.portfolio.balance, 0, 'f', 2));
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_watchlist() {
    if (!initialized_)
        return;
    QPointer<CryptoTradingScreen> self = this;
    const QStringList symbols = watchlist_symbols_; // snapshot on the UI thread — see refresh_ticker()
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbols, ex]() {
        if (!self)
            return;
        auto tickers = ExchangeSessionManager::instance().session(ex)->fetch_tickers(symbols);
        QMetaObject::invokeMethod(
            self,
            [self, ex, tickers]() {
                if (!self || self->exchange_id_ != ex)
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
    const QString ex = exchange_id_;
    market_info_key_ = ex + QLatin1Char('|') + symbol; // MARKET tab: asked for this pair already

    (void)QtConcurrent::run([self, symbol, ex]() {
        if (!self)
            return;
        auto fr = ExchangeSessionManager::instance().session(ex)->fetch_funding_rate(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, symbol, ex, fr]() {
                if (!self)
                    return;
                if (self->selected_symbol_ != symbol || self->exchange_id_ != ex)
                    return; // user switched symbols — discard stale result
                if (fr.symbol.isEmpty() || (fr.mark_price <= 0.0 && fr.funding_rate == 0.0 &&
                                            fr.next_funding_timestamp <= 0)) {
                    // Daemon error / venue without funding (spot): no fabricated 0.0000 % /
                    // $0 mark — hide the ribbon's mark/index chips and let the MARKET tiles
                    // say why instead of loading forever.
                    self->ticker_bar_->update_mark_price(0.0, 0.0);
                    self->bottom_panel_->set_funding_unavailable(
                        self->is_perp_market() ? tr("not reported by %1").arg(ex.toUpper())
                                               : tr("spot market — no funding"));
                    return;
                }
                // Mark / index prices are in the quote asset of the market they come from.
                self->market_info_cache_.symbol = self->market_.valid() ? self->market_.symbol : symbol;
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

    (void)QtConcurrent::run([self, symbol, ex]() {
        if (!self)
            return;
        auto oi = ExchangeSessionManager::instance().session(ex)->fetch_open_interest(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, symbol, ex, oi]() {
                if (!self)
                    return;
                if (self->selected_symbol_ != symbol || self->exchange_id_ != ex)
                    return; // user switched symbols — discard stale result
                if (oi.symbol.isEmpty() || oi.open_interest_value <= 0.0) {
                    // Daemon error / unsupported — say so rather than a fake $0 OI.
                    self->bottom_panel_->set_open_interest_unavailable(
                        self->is_perp_market() ? tr("not reported by %1").arg(ex.toUpper())
                                               : tr("spot market — no open interest"));
                    return;
                }
                // The OI value is in the quote asset of the market it comes from.
                self->market_info_cache_.symbol = self->market_.valid() ? self->market_.symbol : symbol;
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
