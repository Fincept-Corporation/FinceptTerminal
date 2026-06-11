// src/screens/equity_trading/EquityTradingScreen_Streams.cpp
//
// DataHub subscription wiring (D4 migration) + on-demand legacy signal slots.
//
// Streaming data (quotes, positions, holdings, orders, funds) is consumed
// via DataHub::subscribe() instead of direct DataStreamManager signals.
// On-demand / one-shot data (candles, orderbook, time/sales, calendar, clock)
// remains wired as legacy signals — no hub topic exists for those.
//
// Part of the partial-class split of EquityTradingScreen.cpp.

#include "screens/equity_trading/EquityTradingScreen.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChartPanel.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/websocket/FyersTickTypes.h"

#include <QTimer>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::equity;

static const QString TAG = "EquityTrading";

// Paper position prices are persisted to SQLite at most once per this window per
// burst of ticks (coalesced), instead of one UPDATE per tick. The in-memory table
// and chart card still update on every tick; only the DB write is throttled.
static constexpr int kPaperPriceFlushMs = 1000;

// ============================================================================
// DataHub subscription helpers (D4)
// ============================================================================

QString EquityTradingScreen::broker_id_for_focused() const {
    if (focused_account_id_.isEmpty())
        return {};
    return AccountManager::instance().get_account(focused_account_id_).broker_id;
}

void EquityTradingScreen::hub_subscribe_streaming() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    hub_active_ = false;

    const QString bid = broker_id_for_focused();
    if (bid.isEmpty() || focused_account_id_.isEmpty())
        return;

    const QString aid = focused_account_id_;

    // Paper accounts must NOT show live broker positions/holdings/orders/balance —
    // that data comes from the paper engine (pt_* / OrderMatcher). Only live mode
    // subscribes to the live broker portfolio topics. Quotes flow in both modes.
    //
    // Cache the trading mode + paper portfolio id once here. This runs on every
    // focus / symbol / mode change, so the cache stays fresh, and it keeps the
    // per-tick quote handler off AccountManager::get_account() — which locks a
    // mutex and copies the whole BrokerAccount struct on every single tick.
    const auto focused_account = AccountManager::instance().get_account(aid);
    const bool is_paper = focused_account.trading_mode == QLatin1String("paper");
    focused_is_paper_ = is_paper;
    focused_paper_portfolio_id_ = is_paper ? focused_account.paper_portfolio_id : QString();
    // Drop any prices buffered for a previous account/portfolio. An in-flight flush
    // timer simply finds the map empty (or the new portfolio's prices) and no-ops.
    pending_paper_prices_.clear();

    if (!is_paper) {
        // ── Positions ──
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("positions")),
                      [this](const QVariant& v) {
                          if (!v.canConvert<QVector<BrokerPosition>>())
                              return;
                          const auto positions = v.value<QVector<BrokerPosition>>();
                          bottom_panel_->set_positions(positions);
                          live_positions_ = positions; // cache for the chart overlay
                          QStringList ps;
                          for (const auto& p : positions)
                              ps << p.symbol;
                          update_position_symbols(ps); // give held symbols live WS prices
                          update_chart_position();     // refresh on-chart position card
                      });

        // ── Holdings ──
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("holdings")),
                      [this](const QVariant& v) {
                          if (!v.canConvert<QVector<BrokerHolding>>())
                              return;
                          const auto holdings = v.value<QVector<BrokerHolding>>();
                          bottom_panel_->set_holdings(holdings);
                          // Give held symbols live WS prices so their LTP updates in
                          // real time instead of only on the 5-minute REST poll.
                          QStringList hs;
                          for (const auto& h : holdings)
                              hs << h.symbol;
                          update_holding_symbols(hs);
                      });

        // ── Orders ──
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("orders")),
                      [this](const QVariant& v) {
                          if (!v.canConvert<QVector<BrokerOrderInfo>>())
                              return;
                          bottom_panel_->set_orders(v.value<QVector<BrokerOrderInfo>>());
                      });

        // ── Balance / Funds ──
        // Resolve the broker's currency symbol once (₹/$/…) for the funds cards.
        const QString ccy_sym = [&]() {
            if (auto* b = BrokerRegistry::instance().get(bid))
                return currency_symbol(b->profile().currency);
            return QStringLiteral("$");
        }();
        bottom_panel_->set_currency(ccy_sym); // so the positions P&L summary formats correctly
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("balance")),
                      [this, ccy_sym](const QVariant& v) {
                          if (!v.canConvert<BrokerFunds>())
                              return;
                          const auto funds = v.value<BrokerFunds>();
                          EquityFundsView fv;
                          fv.is_paper = false;
                          fv.currency = ccy_sym;
                          fv.available = funds.available_balance;
                          fv.used_margin = funds.used_margin;
                          fv.total_equity = funds.total_balance;
                          fv.collateral = funds.collateral;
                          const double denom = funds.used_margin + funds.available_balance;
                          fv.margin_util_pct = denom > 0.0 ? (funds.used_margin / denom) * 100.0 : 0.0;
                          bottom_panel_->set_funds_view(fv);
                          order_entry_->set_balance(funds.available_balance);
                      });
    }

    // ── Quotes (per-symbol subscriptions for ticker + watchlist + paper trading) ──
    hub_subscribe_quotes();

    hub_active_ = true;
    LOG_INFO(TAG, QString("Hub subscribed: %1 / %2").arg(bid, aid));
}

void EquityTradingScreen::hub_subscribe_quotes() {
    auto& hub = datahub::DataHub::instance();
    const QString bid = broker_id_for_focused();
    if (bid.isEmpty() || focused_account_id_.isEmpty())
        return;

    const QString aid = focused_account_id_;

    // Build deduplicated symbol set: selected symbol + watchlist
    QSet<QString> symbols;
    symbols.insert(selected_symbol_);
    for (const auto& s : watchlist_symbols_)
        symbols.insert(s);
    for (const auto& s : position_symbols_) // held symbols get live quotes too
        symbols.insert(s);
    for (const auto& s : holding_symbols_) // holdings stream live too (not just 5-min REST)
        symbols.insert(s);

    LOG_INFO("posdbg", QString("subscribing %1 quote topics for acct=%2: [%3]")
                           .arg(symbols.size()).arg(aid, QStringList(symbols.begin(), symbols.end()).join(',')));
    for (const auto& sym : symbols) {
        const QString topic = broker_topic(bid, aid, QStringLiteral("quote"), sym);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<BrokerQuote>())
                return;
            const auto quote = v.value<BrokerQuote>();

            // Open-positions table (bottom sheet): patch LTP / P&L from the SAME
            // quote that feeds the ticker bar, so the header and the position row
            // never show different prices. Live & paper both handled inside.
            bottom_panel_->update_quote(sym, quote);

            // Ticker bar + order entry for selected symbol
            if (sym == selected_symbol_) {
                current_price_ = quote.ltp;
                ticker_bar_->update_quote(quote.ltp, quote.change, quote.change_pct,
                                          quote.high, quote.low, quote.volume,
                                          quote.bid, quote.ask);
                order_entry_->set_current_price(quote.ltp);
                chart_->on_quote(quote);       // roll the tick into the live forming candle
                chart_->update_pnl(quote.ltp); // live P&L on the chart's position card
            }

            // Repaint only this symbol's watchlist row (a no-op if it isn't in the
            // active list). Rebuilding the entire watchlist from a cache on every
            // tick was O(rows×entries) of GUI-thread work per tick — the dominant
            // cost on an unthrottled quote feed.
            watchlist_->update_quote(quote);

            // Feed paper trading engine. Mode + portfolio id are cached at subscribe
            // time (hub_subscribe_streaming) so this hot path never copies a
            // BrokerAccount under AccountManager's mutex on every tick.
            if (focused_is_paper_ && !focused_paper_portfolio_id_.isEmpty() && quote.ltp > 0) {
                // Buffer the latest price and coalesce the SQLite persist onto a
                // timer — one UPDATE per symbol per window instead of per tick.
                // flush_paper_prices() also runs synchronously before
                // refresh_paper_panels reads, so order/funds snapshots stay exact.
                pending_paper_prices_[sym] = quote.ltp;
                if (!paper_flush_armed_) {
                    paper_flush_armed_ = true;
                    QTimer::singleShot(kPaperPriceFlushMs, this, [this]() { flush_paper_prices(); });
                }
                // SL/TP + limit matching must see EVERY tick — run on the live
                // price against in-memory order/trigger tables (no DB per tick).
                PriceData pd;
                pd.last = quote.ltp;
                pd.bid = quote.bid;
                pd.ask = quote.ask;
                pd.timestamp = quote.timestamp;
                OrderMatcher::instance().check_orders(sym, pd, focused_paper_portfolio_id_);
                OrderMatcher::instance().check_sl_tp_triggers(focused_paper_portfolio_id_, sym, quote.ltp);
            }
        });
    }
}

void EquityTradingScreen::route_option_quote(const QString& account_id, const QString& symbol,
                                             const BrokerQuote& quote) {
    // The live tick arrives in Fyers' HSM/brsymbol spelling ("NIFTY09JUN26C23200")
    // which carries a parseable strike; equity ticks (e.g. "RELIANCE") don't parse
    // as options and are ignored here.
    const auto tick = trading::fyers_parse_option(symbol);
    if (tick.valid) {
        // [posdbg] throttled — confirm the router fires for option ticks, on which
        // account, and what it tries to match. Remove once F&O pricing is verified.
        static qint64 s_last = 0;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - s_last > 3000) {
            s_last = now;
            LOG_INFO("posdbg", QString("route_option_quote tick='%1' acct=%2 focused=%3 pos=[%4]")
                                   .arg(symbol, account_id, focused_account_id_,
                                        position_symbols_.join(',')));
        }
    }
    if (account_id != focused_account_id_ || quote.ltp <= 0.0)
        return;
    if (!tick.valid || tick.strike <= 0.0)
        return;
    const QString strike_str = QString::number(qRound64(tick.strike));

    // Reconcile to a held position by (underlying, side, strike). The position
    // symbol uses the REST "…<strike>CE" spelling, which never string-matches the
    // tick's "…C<strike>" spelling — so match on the format-independent identity:
    // same underlying root, same call/put, and the position's strike (trailing
    // digits before CE/PE) equals the tick's strike.
    for (const QString& pos_sym : position_symbols_) {
        const auto leg = trading::fyers_parse_option(pos_sym);
        if (!leg.valid || leg.is_call != tick.is_call || leg.underlying != tick.underlying)
            continue;
        QString core = pos_sym.section(QLatin1Char(':'), -1);
        if (core.endsWith(QLatin1String("CE")) || core.endsWith(QLatin1String("PE")))
            core.chop(2);
        if (!core.endsWith(strike_str))
            continue;

        // Mark the position/holding row to market. Passing the position's OWN
        // symbol lets update_quote's existing string match find the row.
        bottom_panel_->update_quote(pos_sym, quote);

        // Paper engine: buffer the option's price so funds/stats mark to market
        // too, and run limit + SL/TP matching — the same coalesced-flush path the
        // equity quote handler uses, keyed by the position's own symbol.
        if (focused_is_paper_ && !focused_paper_portfolio_id_.isEmpty()) {
            pending_paper_prices_[pos_sym] = quote.ltp;
            if (!paper_flush_armed_) {
                paper_flush_armed_ = true;
                QTimer::singleShot(kPaperPriceFlushMs, this, [this]() { flush_paper_prices(); });
            }
            PriceData pd;
            pd.last = quote.ltp;
            pd.bid = quote.bid;
            pd.ask = quote.ask;
            pd.timestamp = quote.timestamp;
            OrderMatcher::instance().check_orders(pos_sym, pd, focused_paper_portfolio_id_);
            OrderMatcher::instance().check_sl_tp_triggers(focused_paper_portfolio_id_, pos_sym, quote.ltp);
        }
        return;
    }
}

void EquityTradingScreen::hub_unsubscribe_all() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
    LOG_INFO(TAG, "Hub unsubscribed");
}

void EquityTradingScreen::update_chart_position() {
    if (!chart_)
        return;
    const auto account = AccountManager::instance().get_account(focused_account_id_);
    const bool is_paper = account.trading_mode == QLatin1String("paper");

    if (is_paper) {
        if (account.paper_portfolio_id.isEmpty()) {
            chart_->clear_position();
            return;
        }
        for (const auto& p : pt_get_positions(account.paper_portfolio_id)) {
            if (p.symbol == selected_symbol_ && qAbs(p.quantity) > 0.0) {
                chart_->set_position(selected_symbol_, p.side, qAbs(p.quantity), p.entry_price,
                                     selected_exchange_, QString());
                if (current_price_ > 0.0)
                    chart_->update_pnl(current_price_);
                return;
            }
        }
        chart_->clear_position();
        return;
    }

    // Live: scan the cached broker positions for the displayed symbol. P&L is in
    // the broker's own currency (intrinsic), not the global preference.
    QString ccy;
    if (auto* broker = BrokerRegistry::instance().get(account.broker_id))
        ccy = broker->profile().currency;
    for (const auto& p : live_positions_) {
        if (p.symbol == selected_symbol_ && qAbs(p.quantity) > 0.0) {
            QString side = p.side.toLower();
            if (side != QLatin1String("long") && side != QLatin1String("short"))
                side = p.quantity >= 0 ? QStringLiteral("long") : QStringLiteral("short");
            chart_->set_position(selected_symbol_, side, qAbs(p.quantity), p.avg_price,
                                 p.exchange.isEmpty() ? selected_exchange_ : p.exchange,
                                 p.product_type, ccy);
            if (current_price_ > 0.0)
                chart_->update_pnl(current_price_);
            return;
        }
    }
    chart_->clear_position();
}

// ============================================================================
// On-demand legacy signal slots (no hub topic — D4 exception)
// ============================================================================

void EquityTradingScreen::on_stream_candles_fetched(const QString& account_id,
                                                     const QVector<BrokerCandle>& candles) {
    if (account_id == focused_account_id_)
        chart_->set_candles(candles);
}

void EquityTradingScreen::on_stream_orderbook_fetched(const QString& account_id,
                                                       const QVector<QPair<double, double>>& bids,
                                                       const QVector<QPair<double, double>>& asks,
                                                       double spread, double spread_pct,
                                                       const QVector<int>& bid_orders,
                                                       const QVector<int>& ask_orders) {
    if (account_id == focused_account_id_)
        orderbook_->set_data(bids, asks, spread, spread_pct, bid_orders, ask_orders);
}

void EquityTradingScreen::on_stream_time_sales_fetched(const QString& account_id,
                                                        const QVector<BrokerTrade>& trades) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_time_sales(trades);
}

void EquityTradingScreen::on_stream_latest_trade_fetched(const QString& account_id,
                                                          const BrokerTrade& trade) {
    if (account_id == focused_account_id_)
        bottom_panel_->prepend_trade(trade);
}

void EquityTradingScreen::on_stream_calendar_fetched(const QString& account_id,
                                                      const QVector<MarketCalendarDay>& days) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_calendar(days);
}

void EquityTradingScreen::on_stream_clock_fetched(const QString& account_id, const MarketClock& clock) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_clock(clock);
}

} // namespace fincept::screens
