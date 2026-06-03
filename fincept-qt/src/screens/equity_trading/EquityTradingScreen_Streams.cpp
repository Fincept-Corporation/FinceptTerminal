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

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::equity;

static const QString TAG = "EquityTrading";

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
    watchlist_quote_cache_.clear();

    const QString bid = broker_id_for_focused();
    if (bid.isEmpty() || focused_account_id_.isEmpty())
        return;

    const QString aid = focused_account_id_;

    // Paper accounts must NOT show live broker positions/holdings/orders/balance —
    // that data comes from the paper engine (pt_* / OrderMatcher). Only live mode
    // subscribes to the live broker portfolio topics. Quotes flow in both modes.
    const bool is_paper =
        AccountManager::instance().get_account(aid).trading_mode == QLatin1String("paper");

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
                          bottom_panel_->set_holdings(v.value<QVector<BrokerHolding>>());
                      });

        // ── Orders ──
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("orders")),
                      [this](const QVariant& v) {
                          if (!v.canConvert<QVector<BrokerOrderInfo>>())
                              return;
                          bottom_panel_->set_orders(v.value<QVector<BrokerOrderInfo>>());
                      });

        // ── Balance / Funds ──
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("balance")),
                      [this](const QVariant& v) {
                          if (!v.canConvert<BrokerFunds>())
                              return;
                          const auto funds = v.value<BrokerFunds>();
                          bottom_panel_->set_funds(funds);
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

    for (const auto& sym : symbols) {
        const QString topic = broker_topic(bid, aid, QStringLiteral("quote"), sym);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<BrokerQuote>())
                return;
            const auto quote = v.value<BrokerQuote>();

            // Cache for watchlist rebuild
            watchlist_quote_cache_[sym] = quote;

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

            // Rebuild watchlist from cache
            QVector<BrokerQuote> wl_quotes;
            wl_quotes.reserve(watchlist_quote_cache_.size());
            for (auto it = watchlist_quote_cache_.cbegin(); it != watchlist_quote_cache_.cend(); ++it)
                wl_quotes.append(it.value());
            watchlist_->update_quotes(wl_quotes);

            // Feed paper trading engine
            auto account = AccountManager::instance().get_account(focused_account_id_);
            if (account.trading_mode == "paper" && !account.paper_portfolio_id.isEmpty() && quote.ltp > 0) {
                pt_update_position_price(account.paper_portfolio_id, sym, quote.ltp);
                PriceData pd;
                pd.last = quote.ltp;
                pd.bid = quote.bid;
                pd.ask = quote.ask;
                pd.timestamp = quote.timestamp;
                OrderMatcher::instance().check_orders(sym, pd, account.paper_portfolio_id);
                OrderMatcher::instance().check_sl_tp_triggers(account.paper_portfolio_id, sym, quote.ltp);
            }
        });
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
