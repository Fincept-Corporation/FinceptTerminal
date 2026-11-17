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

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChartPanel.h"
#include "screens/equity_trading/EquityCommandBar.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityTradingScreen.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "trading/MarketDataRequests.h"
#include "screens/equity_trading/EquityQuoteSnapshot.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "trading/websocket/FyersTickTypes.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScreenCommandBar.h"

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

    // Local paper accounts must NOT show live broker positions/holdings/orders/balance —
    // that data comes from the paper engine (pt_* / OrderMatcher). Live mode, and PAPER
    // mode on a broker's own paper venue (focused_native_paper_), subscribe to the broker
    // portfolio topics instead. Quotes flow in every mode.
    //
    // Cache the trading mode + paper portfolio id once here. This runs on every
    // focus / symbol / mode change, so the cache stays fresh, and it keeps the
    // per-tick quote handler off AccountManager::get_account() — which locks a
    // mutex and copies the whole BrokerAccount struct on every single tick.
    const auto focused_account = AccountManager::instance().get_account(aid);
    // "is_paper" below means the LOCAL simulator.
    const bool is_paper = focused_account.trading_mode == QLatin1String("paper") && !focused_native_paper_;
    focused_is_paper_ = is_paper;
    focused_paper_portfolio_id_ = is_paper ? focused_account.paper_portfolio_id : QString();
    // Drop any prices buffered for a previous account/portfolio. An in-flight flush
    // timer simply finds the map empty (or the new portfolio's prices) and no-ops.
    pending_paper_prices_.clear();

    if (!is_paper) {
        // ── Positions ──
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("positions")), [this](const QVariant& v) {
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
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("holdings")), [this](const QVariant& v) {
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
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("orders")), [this](const QVariant& v) {
            if (!v.canConvert<QVector<BrokerOrderInfo>>())
                return;
            bottom_panel_->set_orders(v.value<QVector<BrokerOrderInfo>>());
        });

        // ── Balance / Funds ──
        // The broker account's ISO currency for the funds cards and the P&L
        // summary (empty when the broker is unknown: amounts without a unit, never
        // a guessed "$"). A funds payload that names its own currency wins.
        const QString acct_ccy = [&]() {
            if (auto* b = BrokerRegistry::instance().get(bid))
                return b->profile().currency;
            return QString();
        }();
        bottom_panel_->set_currency(acct_ccy); // so the positions P&L summary formats correctly
        hub.subscribe(this, broker_topic(bid, aid, QStringLiteral("balance")), [this, acct_ccy](const QVariant& v) {
            if (!v.canConvert<BrokerFunds>())
                return;
            const auto funds = v.value<BrokerFunds>();
            EquityFundsView fv;
            fv.is_paper = false;
            const QString reported = funds.raw_data.value(QStringLiteral("currency")).toString().trimmed();
            fv.currency = reported.isEmpty() ? acct_ccy : reported;
            fv.available = funds.available_balance;
            fv.used_margin = funds.used_margin;
            fv.total_equity = funds.total_balance;
            fv.collateral = funds.collateral;
            const double denom = funds.used_margin + funds.available_balance;
            fv.margin_util_pct = denom > 0.0 ? (funds.used_margin / denom) * 100.0 : 0.0;
            bottom_panel_->set_funds_view(fv);
            order_entry_->set_balance(funds.available_balance, fv.currency);
            last_funds_view_ = fv;
            have_funds_view_ = true;
        });
    }

    // ── Quotes (per-symbol subscriptions for ticker + watchlist + paper trading) ──
    hub_subscribe_quotes();

    hub_active_ = true;
    LOG_DEBUG(TAG, QString("Hub subscribed: %1 / %2").arg(bid, aid));
}

void EquityTradingScreen::hub_subscribe_quotes() {
    // On the public fallback the quotes come from EquityPublicFeed — never mix the
    // two sources (a reconnecting broker would interleave its ticks with Yahoo's).
    if (public_mode_)
        return;
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

    LOG_DEBUG(TAG, QString("subscribing %1 quote topics for acct=%2: [%3]")
                           .arg(symbols.size())
                           .arg(aid, QStringList(symbols.begin(), symbols.end()).join(',')));
    for (const auto& sym : symbols) {
        const QString topic = broker_topic(bid, aid, QStringLiteral("quote"), sym);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<BrokerQuote>())
                return;
            apply_market_quote(sym, v.value<BrokerQuote>(), /*from_broker=*/true);
        });
    }
}

void EquityTradingScreen::apply_market_quote(const QString& sym, BrokerQuote quote, bool from_broker) {
    // One source at a time: a broker tick that lands after the switch to the
    // public feed (or a public quote queued before the switch back) is dropped,
    // so neither source paints over the other.
    if (from_broker == public_mode_)
        return;

    // Some broker adapters leave BrokerQuote.symbol empty (the topic already
    // carries it). The watchlist matches rows on quote.symbol, so an unset one
    // silently stopped that row from ever updating.
    if (quote.symbol.isEmpty())
        quote.symbol = sym;

    // A broker price proves the broker has a quote path: drop any "no live quotes"
    // notice (one flag test per tick once seen).
    if (from_broker && !broker_quote_seen_ && quote.ltp > 0) {
        broker_quote_seen_ = true;
        if (!broker_quote_refusal_.isEmpty()) {
            broker_quote_refusal_.clear();
            apply_broker_quote_notice();
        }
    }

    // Delayed public quotes drive the ticket, the position marks on screen and the
    // chart's P&L only for paper on delayed prices (the built-in account, or a local
    // paper account whose broker feed is down) — there they ARE the simulator's
    // prices. A LIVE account never takes a public quote as its ticket price.
    const bool drives_trading = from_broker || simulated_on_delayed_prices();

    // Open-positions table (bottom sheet): patch LTP / P&L from the SAME quote
    // that feeds the ticker bar, so the header and the position row never show
    // different prices. Live & paper both handled inside.
    if (drives_trading)
        bottom_panel_->update_quote(sym, quote);

    // Ticker bar (+ order entry / chart for the broker) for the selected symbol
    if (sym == selected_symbol_) {
        ticker_bar_->set_notice(QString());
        ticker_bar_->update_quote(quote.ltp, quote.change, quote.change_pct, quote.high, quote.low, quote.volume,
                                  quote.bid, quote.ask);
        if (!ticker_bar_->toolTip().isEmpty())
            ticker_bar_->setToolTip(QString()); // a failure note from an earlier public quote
        if (drives_trading) {
            current_price_ = quote.ltp;
            order_entry_->set_current_price(quote.ltp);
            chart_->update_pnl(quote.ltp); // P&L on the chart's position card
        }
        if (from_broker)
            chart_->on_quote(quote); // public ticks reach the chart through the feed's chart_tick
    }

    // Repaint only this symbol's watchlist row (a no-op if it isn't in the active
    // list). Rebuilding the entire watchlist from a cache on every tick was
    // O(rows×entries) of GUI-thread work per tick — the dominant cost on an
    // unthrottled quote feed.
    watchlist_->update_quote(quote);

    if (!from_broker)
        return;

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
        // A fill books cash at the symbol's FX factor: a cross-currency symbol with no
        // real rate yet is marked, not matched, on this tick (never filled at 1:1).
        if (PaperMarkService::fill_fx_ready(focused_paper_portfolio_id_, sym)) {
            OrderMatcher::instance().check_orders(sym, pd, focused_paper_portfolio_id_);
            OrderMatcher::instance().check_sl_tp_triggers(focused_paper_portfolio_id_, sym, quote.ltp);
        }
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
            LOG_DEBUG(TAG, QString("route_option_quote tick='%1' acct=%2 focused=%3 pos=[%4]")
                                   .arg(symbol, account_id, focused_account_id_, position_symbols_.join(',')));
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
            if (PaperMarkService::fill_fx_ready(focused_paper_portfolio_id_, pos_sym)) { // never filled at 1:1
                OrderMatcher::instance().check_orders(pos_sym, pd, focused_paper_portfolio_id_);
                OrderMatcher::instance().check_sl_tp_triggers(focused_paper_portfolio_id_, pos_sym, quote.ltp);
            }
        }
        return;
    }
}

void EquityTradingScreen::hub_unsubscribe_all() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
    LOG_DEBUG(TAG, "Hub unsubscribed");
}

void EquityTradingScreen::update_chart_position() {
    if (!chart_)
        return;
    const auto account = AccountManager::instance().get_account(focused_account_id_);
    // Local simulator only: a broker paper venue's positions are the cached broker rows below.
    const bool is_paper = account.trading_mode == QLatin1String("paper") && !focused_native_paper_;

    if (is_paper) {
        if (account.paper_portfolio_id.isEmpty()) {
            chart_->clear_position();
            return;
        }
        for (const auto& p : pt_get_positions(account.paper_portfolio_id)) {
            if (p.symbol == selected_symbol_ && qAbs(p.quantity) > 0.0) {
                // P&L on the card in the instrument's own currency (prices are quoted in it).
                QString ccy = quote_detail_.value(selected_symbol_).currency;
                if (ccy.isEmpty() && canonical_mode())
                    ccy = instrument_of(selected_symbol_).currency;
                chart_->set_position(selected_symbol_, p.side, qAbs(p.quantity), p.entry_price, selected_exchange_,
                                     p.product, ccy);
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
            // The card shows the entry price: its currency is the position's exchange's
            // (venue table), else the broker account's.
            const QString pos_exchange = p.exchange.isEmpty() ? selected_exchange_ : p.exchange;
            chart_->set_position(selected_symbol_, side, qAbs(p.quantity), p.avg_price, pos_exchange, p.product_type,
                                 exchange_currency(pos_exchange, ccy));
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

namespace {
// File-local helpers carry an eqt_ prefix (screens are built outside the unity
// batches, but the convention keeps them collision-proof anyway).

// A failure reason that means "the source has no bars for this", not "the source
// failed" — shown as NO DATA rather than CHART UNAVAILABLE. Matches the broker
// stream's and the public feed's own wording.
bool eqt_reason_is_no_data(const QString& reason) {
    return reason.contains(QLatin1String("no price"), Qt::CaseInsensitive) ||
           reason.contains(QLatin1String("no usable price"), Qt::CaseInsensitive) ||
           reason.contains(QLatin1String("no data"), Qt::CaseInsensitive);
}
} // namespace

bool EquityTradingScreen::is_broker_chart_target(const QString& account_id, const QString& symbol,
                                                 const QString& timeframe) const {
    // On the public fallback the chart belongs to EquityPublicFeed: a late broker
    // answer must not replace or wipe it. The stream echoes the exact strings the
    // request passed, so an exact match is the right test.
    return !public_mode_ && chart_ && account_id == focused_account_id_ && symbol == selected_symbol_ &&
           timeframe == chart_->current_timeframe();
}

bool EquityTradingScreen::is_broker_symbol_target(const QString& account_id, const QString& symbol) const {
    // WebSocket depth names the instrument the broker's way ("NSE:SBIN-EQ"), REST
    // the way it was asked ("SBIN"): compare the bare instrument.
    return !public_mode_ && account_id == focused_account_id_ && market_symbols_match(symbol, selected_symbol_);
}

void EquityTradingScreen::on_stream_candles_for_symbol(const QString& account_id, const QString& symbol,
                                                       const QString& timeframe, const QVector<BrokerCandle>& candles) {
    if (!is_broker_chart_target(account_id, symbol, timeframe)) {
        LOG_DEBUG(TAG, QString("Dropping broker candles for %1 · %2 — the chart now wants %3")
                           .arg(symbol, timeframe, chart_key()));
        return;
    }
    if (candles.isEmpty()) { // the stream reports this as a failure; belt and braces
        on_stream_candles_failed(account_id, symbol, timeframe, QString());
        return;
    }
    chart_pending_key_.clear();   // answered — disarms the last-resort watchdog
    chart_->set_candles(candles); // also hides the loading overlay
    chart_symbol_ = chart_key();
}

void EquityTradingScreen::on_stream_candles_failed(const QString& account_id, const QString& symbol,
                                                   const QString& timeframe, const QString& reason) {
    if (!is_broker_chart_target(account_id, symbol, timeframe))
        return; // a failure for a series the user has already left
    chart_pending_key_.clear();

    // A refresh of the series already on screen failed: last-known-good stays.
    // Wiping it would leave live ticks drawing a couple of giant blocks (#338).
    if (chart_symbol_ == chart_key()) {
        LOG_WARN(TAG, QString("Broker history refresh failed for %1 · %2, keeping the chart: %3")
                          .arg(symbol, timeframe, reason));
        return;
    }

    chart_->clear_candles();
    chart_symbol_.clear();
    const QString why = reason.trimmed().isEmpty()
                            ? tr("No price history was returned for %1 on this timeframe.").arg(symbol)
                            : reason.trimmed();
    const bool no_data = reason.trimmed().isEmpty() || eqt_reason_is_no_data(why);
    // The reason is the broker's own text (or an expired session spelled out).
    chart_->show_status(no_data ? EquityChartPanel::StatusKind::NoData : EquityChartPanel::StatusKind::Error,
                        no_data ? tr("NO DATA · %1 · %2").arg(symbol, timeframe)
                                : tr("CHART UNAVAILABLE · %1 · %2").arg(symbol, timeframe),
                        no_data ? tr("%1: %2 Try another timeframe.").arg(chart_source_label(), why)
                                : tr("%1: %2").arg(chart_source_label(), why));
    LOG_WARN(TAG, QString("Broker history for %1 · %2 failed: %3").arg(symbol, timeframe, why));
}

void EquityTradingScreen::on_stream_orderbook_for_symbol(const QString& account_id, const QString& symbol,
                                                         const QVector<QPair<double, double>>& bids,
                                                         const QVector<QPair<double, double>>& asks, double spread,
                                                         double spread_pct, const QVector<int>& bid_orders,
                                                         const QVector<int>& ask_orders) {
    // The depth panel explains that depth needs a broker while on public data; a
    // stray book from a reconnecting stream must not reappear under it, and a late
    // book for the previous symbol must not be shown (or clicked) under this one.
    if (!is_broker_symbol_target(account_id, symbol))
        return;
    orderbook_->set_data(bids, asks, spread, spread_pct, bid_orders, ask_orders);
    orderbook_symbol_ = selected_symbol_;
}

void EquityTradingScreen::on_stream_orderbook_failed(const QString& account_id, const QString& symbol,
                                                     const QString& reason) {
    if (!is_broker_symbol_target(account_id, symbol))
        return;
    LOG_WARN(TAG, QString("Broker depth for %1 failed: %2").arg(symbol, reason));
    // The depth request ends in the broker's quote call; on a broker without a
    // quote stream its refusal also means the watchlist and ribbon get no prices.
    note_broker_quote_refusal(reason);
    // A live (WebSocket) book for this symbol may already be showing — keep it.
    if (orderbook_symbol_ == selected_symbol_)
        return;
    orderbook_->clear();
    orderbook_->set_empty_message(tr("No market depth for %1.\n\n%2").arg(selected_symbol_, reason));
}

void EquityTradingScreen::note_broker_quote_refusal(const QString& reason) {
    // Never on the public fallback (its own errors are per row), and never once a
    // broker price has arrived: then a quote path exists and this was a depth or
    // per-symbol failure, already shown in the order book.
    if (public_mode_ || broker_quote_seen_)
        return;
    const QString why = reason.trimmed();
    if (why.isEmpty() || why == broker_quote_refusal_)
        return;
    // The capability, not the broker's name: only an adapter that advertises no
    // quote stream depends on the quote call alone. A streaming broker's ticks
    // still arrive even when its snapshot call is refused.
    auto* broker = BrokerRegistry::instance().get(broker_id_for_focused());
    if (!broker)
        return;
    const char* stream_adapter = broker->ws_adapter_name();
    if (stream_adapter && *stream_adapter)
        return;
    broker_quote_refusal_ = why;
    LOG_WARN(TAG, QString("No live quotes from %1 for account '%2': %3")
                      .arg(broker_id_for_focused(), focused_account_id_, why));
    apply_broker_quote_notice();
}

void EquityTradingScreen::apply_broker_quote_notice() {
    if (!watchlist_ || !ticker_bar_)
        return;
    if (public_mode_ || broker_quote_refusal_.isEmpty()) {
        watchlist_->set_feed_notice(QString(), QString());
        ticker_bar_->set_notice(QString());
        return;
    }
    const QString title = tr("NO LIVE QUOTES · %1").arg(chart_source_label());
    // The reason is the broker's own text, shown as delivered.
    watchlist_->set_feed_notice(title, broker_quote_refusal_);
    ticker_bar_->set_notice(title, broker_quote_refusal_);
}

void EquityTradingScreen::on_stream_time_sales_for_symbol(const QString& account_id, const QString& symbol,
                                                          const QVector<BrokerTrade>& trades) {
    if (is_broker_symbol_target(account_id, symbol))
        bottom_panel_->set_time_sales(trades);
}

void EquityTradingScreen::on_stream_time_sales_failed(const QString& account_id, const QString& symbol,
                                                      const QString& reason) {
    if (!is_broker_symbol_target(account_id, symbol))
        return;
    // Never leave the previous symbol's prints under this one. (The tape tab has no
    // per-tab message slot; the reason goes to the log — only where the tab is shown:
    // the other brokers answer "Not supported" to every tape request.)
    bottom_panel_->set_time_sales({});
    if (focused_is_us_market_)
        LOG_WARN(TAG, QString("Broker time & sales for %1 failed: %2").arg(symbol, reason));
}

void EquityTradingScreen::on_stream_latest_trade_for_symbol(const QString& account_id, const QString& symbol,
                                                            const BrokerTrade& trade) {
    if (is_broker_symbol_target(account_id, symbol))
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

// ============================================================================
// Market-data source — focused broker stream, or the public fallback
// ============================================================================

namespace {
// Last-resort guard only. The stream now reports every answer — data
// (candles_for_symbol) or a failure with the broker's reason (candles_failed) —
// normally within one broker HTTP timeout (8 s per attempt, at most two). This
// fires only when neither ever arrives (a broker call that hangs), so the chart
// does not say "loading" forever. Not shorter: a slow but healthy broker can
// legitimately take two full HTTP attempts plus instrument resolution.
constexpr int eqt_kChartResponseTimeoutMs = 25000;
} // namespace

void EquityTradingScreen::wire_public_feed() {
    public_feed_ = new EquityPublicFeed(this);

    // Every sink lands on an entry point the broker path uses too, and checks that
    // the public feed is still the source (public_mode_) and that the payload is
    // for what is selected now.
    EquityPublicFeed::Sinks sinks;
    sinks.quote_detail = [this](const QString& sym, const services::QuoteData& q, qint64 received_ms) {
        if (!public_mode_ || !(q.price > 0.0))
            return;
        const bool first = !quote_detail_.contains(sym);
        const auto prev = quote_detail_.value(sym);
        quote_detail_.insert(sym, q);
        quote_rx_ms_.insert(sym, received_ms);
        // A foreign-currency instrument on a paper account: fetch its FX leg now, so
        // the first order books cash without waiting for the rate.
        if (first && simulated_on_delayed_prices() && !account_currency_.isEmpty())
            (void)trading::PaperMarkService::fx_factor(q.currency, account_currency_);
        // Currency / decimals first known (or changed): relabel rows and the blotter.
        if (first || prev.currency != q.currency || prev.price_digits != q.price_digits)
            schedule_row_info();
        apply_quote_detail(sym);
    };
    sinks.quote = [this](const QString& sym, const BrokerQuote& q) {
        apply_market_quote(sym, q, /*from_broker=*/false);
    };
    sinks.quote_error = [this](const QString& sym, const QString& reason) {
        if (!public_mode_)
            return;
        watchlist_->set_quote_error(sym, reason);
        if (sym == selected_symbol_) {
            ticker_bar_->setToolTip(tr("%1: no public quote. %2").arg(display_of(sym), reason));
            // Without any price for it, say so in the ribbon instead of a row of "--".
            if (!quote_detail_.contains(sym))
                ticker_bar_->set_notice(tr("NO QUOTE"), reason);
            if (snapshot_ && !quote_detail_.contains(sym))
                snapshot_->set_waiting(display_of(sym), reason);
        }
    };
    sinks.chart_tick = [this](const BrokerQuote& q) {
        if (public_mode_ && q.symbol == selected_symbol_)
            chart_->on_quote(q);
    };
    auto is_current = [this](const QString& sym, const QString& tf) {
        return public_mode_ && sym == selected_symbol_ && tf == chart_->current_timeframe();
    };
    sinks.chart_loading = [this, is_current](const QString& sym, const QString& tf) {
        if (!is_current(sym, tf) || chart_symbol_ == chart_key())
            return;
        chart_->show_status(EquityChartPanel::StatusKind::Loading, tr("LOADING %1 · %2").arg(sym, tf),
                            chart_source_label());
    };
    sinks.chart_candles = [this, is_current](const QString& sym, const QString& tf,
                                             const QVector<BrokerCandle>& candles) {
        if (!is_current(sym, tf))
            return;
        chart_->set_candles(candles); // hides the status overlay
        chart_symbol_ = chart_key();
    };
    sinks.chart_error = [this, is_current](const QString& sym, const QString& tf, const QString& reason) {
        if (!is_current(sym, tf))
            return;
        if (chart_symbol_ == chart_key()) {
            // A refresh of the series on screen failed — last-known-good stays.
            LOG_WARN(TAG, QString("Public history refresh failed for %1 (%2), keeping chart: %3").arg(sym, tf, reason));
            return;
        }
        chart_->clear_candles();
        chart_symbol_.clear();
        const bool no_data = eqt_reason_is_no_data(reason);
        // The reason is the hub's / Yahoo's own text, shown as delivered.
        chart_->show_status(no_data ? EquityChartPanel::StatusKind::NoData : EquityChartPanel::StatusKind::Error,
                            no_data ? tr("NO DATA · %1 · %2").arg(sym, tf) : tr("CHART UNAVAILABLE · %1").arg(sym),
                            reason);
    };
    public_feed_->set_sinks(std::move(sinks));
}

void EquityTradingScreen::schedule_market_feed_sync() {
    if (feed_sync_queued_)
        return;
    feed_sync_queued_ = true;
    QMetaObject::invokeMethod(
        this,
        [this]() {
            feed_sync_queued_ = false;
            sync_market_feed();
        },
        Qt::QueuedConnection);
}

void EquityTradingScreen::sync_market_feed(bool fetch_on_switch) {
    using Status = EquityPublicFeed::BrokerStatus;
    feed_state_ = EquityPublicFeed::assess_broker_feed(focused_account_id_);

    const bool dropped_session = feed_state_.status == Status::Disconnected;
    if (!dropped_session) {
        if (feed_grace_timer_)
            feed_grace_timer_->stop();
        feed_grace_expired_ = false;
    }

    bool want_public = !feed_state_.usable();
    // A token session that just dropped (socket closed) usually reconnects within
    // seconds — flipping the whole screen to Yahoo and back on every blip would
    // blank it twice. Keep the broker for a grace period first.
    if (want_public && !public_mode_ && dropped_session && !feed_grace_expired_) {
        if (feed_grace_timer_ && !feed_grace_timer_->isActive())
            feed_grace_timer_->start();
        want_public = false;
    }

    if (want_public != public_mode_) {
        public_mode_ = want_public;
        LOG_INFO(TAG, QString("Market data source -> %1 (account '%2')")
                          .arg(public_mode_ ? QStringLiteral("public feed (Yahoo, delayed): ") + public_feed_reason()
                                            : QStringLiteral("broker stream"),
                               focused_account_id_));
        reset_market_views(); // never show one source's numbers under the other
        // Broker quote topics are subscribed only while the broker is the source
        // (hub_subscribe_quotes); account topics stay either way.
        if (isVisible())
            hub_subscribe_streaming();
        if (!public_mode_) {
            if (public_feed_)
                public_feed_->stop(); // drops its subscriptions and anything still queued
            feed_grace_expired_ = false;
            if (fetch_on_switch)
                refetch_broker_market_data();
        }
    }

    // The public feed runs only while it is the source AND the screen is visible.
    if (public_mode_ && public_feed_ && isVisible()) {
        push_public_targets();
        public_feed_->start(); // idempotent; subscribes the targets just pushed
    }

    // A local-paper broker account whose feed is down marks and fills on delayed
    // public prices (screen-independent, PaperMarkService) until the broker is back.
    sync_public_paper_tracking();
    update_connection_status();
    update_feed_notices();
    update_right_column();
    update_session_lines();
    update_status_bar();
}

void EquityTradingScreen::reset_market_views() {
    watchlist_->clear_quotes();
    ticker_bar_->clear_quote();
    ticker_bar_->setToolTip(QString());
    // A "no live quotes" verdict belongs to the previous source.
    broker_quote_seen_ = false;
    broker_quote_refusal_.clear();
    apply_broker_quote_notice();
    orderbook_->clear();
    orderbook_symbol_.clear();
    current_price_ = 0.0;
    order_entry_->set_current_price(0.0);
    chart_->clear_candles();
    chart_symbol_.clear();
    chart_pending_key_.clear();
    ++chart_request_seq_; // disarms a pending broker no-response watchdog
    chart_->show_status(EquityChartPanel::StatusKind::Loading,
                        tr("LOADING %1 · %2").arg(selected_symbol_, chart_->current_timeframe()), chart_source_label());
}

void EquityTradingScreen::refetch_broker_market_data() {
    auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
    if (stream) {
        stream->set_selected_symbol(selected_symbol_, selected_exchange_);
        stream->subscribe_symbols(QStringLiteral("equity:watchlist"), stream_symbols());
        stream->fetch_orderbook(selected_symbol_);
        if (focused_is_us_market_) {
            stream->fetch_time_sales(selected_symbol_);
            stream->fetch_auctions(selected_symbol_); // AUCTIONS tab (shown once it has rows)
        }
    }
    request_chart_history();
}

void EquityTradingScreen::push_public_targets() {
    if (!public_feed_)
        return;
    // Same set the broker path quotes: the selected symbol + every displayed row
    // (active list ∪ positions ∪ holdings). Each symbol's Yahoo ticker comes from
    // public_target(): the canonical symbol itself on the paper account, a broker
    // symbol canonicalised with its exchange otherwise.
    QVector<EquityPublicFeed::Target> watch;
    QSet<QString> seen;
    auto add = [&](const QString& s) {
        if (s.isEmpty() || seen.contains(s))
            return;
        seen.insert(s);
        watch.append(public_target(s));
    };
    add(selected_symbol_);
    for (const auto& s : effective_symbols())
        add(s);
    public_feed_->set_watch(watch);
    // Without a focused broker nothing else sets the ribbon's currency (it would
    // print RELIANCE in "$"); derive it from the symbol's exchange (no unit when the
    // exchange is not in the venue table). The quote's own currency replaces it as
    // soon as it arrives (apply_quote_detail).
    ticker_bar_->set_currency(exchange_currency(exchange_for_symbol(selected_symbol_)));
    request_chart_history();
}

void EquityTradingScreen::request_chart_history() {
    if (!chart_)
        return;
    const QString tf = chart_->current_timeframe();
    const QString key = chart_key();
    // A different symbol / timeframe / source than the series on screen: clear it
    // and say "loading" — never extend or show the old series under the new label.
    // A refresh of the same series keeps the chart.
    const bool new_series = (chart_symbol_ != key);
    if (new_series) {
        chart_->clear_candles();
        chart_symbol_.clear();
        chart_->show_status(EquityChartPanel::StatusKind::Loading, tr("LOADING %1 · %2").arg(selected_symbol_, tf),
                            chart_source_label());
    }

    if (public_mode_) {
        // The feed reports data / failure through its chart sinks.
        if (public_feed_)
            public_feed_->set_chart(public_target(selected_symbol_), tf);
        return;
    }

    auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
    if (!stream) {
        if (new_series)
            chart_->show_status(EquityChartPanel::StatusKind::Error, tr("CHART UNAVAILABLE · %1").arg(selected_symbol_),
                                tr("No market-data stream is running for %1.").arg(chart_source_label()));
        return;
    }
    // Mark the request pending and arm the guard BEFORE asking: the stream may
    // answer synchronously (a cached series, missing credentials), and that answer
    // must find the request already marked so it can disarm the guard.
    chart_pending_key_ = key;
    const quint64 seq = ++chart_request_seq_;
    if (new_series) {
        QTimer::singleShot(eqt_kChartResponseTimeoutMs, this, [this, seq, key]() {
            if (seq != chart_request_seq_ || public_mode_ || chart_pending_key_ != key || chart_->has_candles())
                return;
            chart_->show_status(EquityChartPanel::StatusKind::Error,
                                tr("NO RESPONSE · %1 · %2").arg(selected_symbol_, chart_->current_timeframe()),
                                tr("%1 has not answered the price-history request for %2 yet. The chart fills in if "
                                   "an answer still arrives; otherwise check the account in ACCOUNTS or try another "
                                   "timeframe.")
                                    .arg(chart_source_label(), selected_symbol_));
        });
    }
    // Every request reaches the stream: it answers only the newest one (data via
    // candles_for_symbol, failure via candles_failed) and drops stale answers.
    stream->fetch_candles(selected_symbol_, tf);
}

QString EquityTradingScreen::chart_key() const {
    return (public_mode_ ? QStringLiteral("pub|") : QStringLiteral("brk|")) + selected_symbol_ + QLatin1Char('|') +
           (chart_ ? chart_->current_timeframe() : QString());
}

QString EquityTradingScreen::chart_source_label() const {
    if (public_mode_)
        return tr("Yahoo Finance (delayed public data)");
    return feed_state_.broker_name.isEmpty() ? tr("The broker") : feed_state_.broker_name;
}

QString EquityTradingScreen::exchange_for_symbol(const QString& symbol) const {
    const QString ex = watchlist_exchanges_.value(symbol);
    return ex.isEmpty() ? selected_exchange_ : ex;
}

QString EquityTradingScreen::public_feed_reason() const {
    using Status = EquityPublicFeed::BrokerStatus;
    const QString name = feed_state_.account_name.isEmpty() ? feed_state_.broker_name : feed_state_.account_name;
    switch (feed_state_.status) {
        case Status::BuiltinPaper:
            return tr("the paper account has no broker by design");
        case Status::NoAccount:
            return tr("no broker account is connected");
        case Status::NoCredentials: {
            // Stored credentials that could not be read are not "none saved".
            const QString read_error = AccountManager::instance().credential_read_error(focused_account_id_);
            if (!read_error.isEmpty())
                return tr("the stored credential of %1 could not be read (%2) — try again").arg(name, read_error);
            return tr("%1 has no saved broker credentials yet").arg(name);
        }
        case Status::TokenExpired:
            return tr("the session token for %1 has expired").arg(name);
        case Status::NoMarketData:
            return feed_state_.detail.isEmpty()
                       ? tr("%1 is not providing market data").arg(name)
                       : tr("%1 is not providing market data (%2)").arg(name, feed_state_.detail);
        case Status::Disconnected:
            return tr("%1 is disconnected").arg(name);
        case Status::Usable:
            break;
    }
    return tr("the broker feed is not available");
}

void EquityTradingScreen::update_feed_notices() {
    using Status = EquityPublicFeed::BrokerStatus;
    const auto account = AccountManager::instance().get_account(focused_account_id_);
    const bool have_account = !focused_account_id_.isEmpty() && !account.account_id.isEmpty();
    // Local paper simulator: orders work without a broker (pt_* engine).
    const bool local_paper = have_account && account.trading_mode == QLatin1String("paper") && !focused_native_paper_;

    // ── Order ticket ──
    if (focused_is_builtin()) {
        // The built-in paper account trades on its own: the ticket says what a fill
        // means on itself (set_builtin_mode); no banner, nothing blocked.
        order_entry_->set_broker_notice(QString(), QString(), false);
    } else if (!have_account) {
        // Nothing can be sent at all — block the ticket instead of offering
        // BUY/SELL buttons that only answer "no account selected".
        order_entry_->set_broker_notice(
            tr("NO ACCOUNT"),
            tr("Pick the paper account in the account menu (no broker needed), or add a broker in ACCOUNTS."),
            /*blocking=*/true);
    } else if (!public_mode_) {
        order_entry_->set_broker_notice(QString(), QString(), false);
    } else if (local_paper) {
        order_entry_->set_broker_notice(
            tr("PAPER · DELAYED PRICES"),
            tr("No live broker feed: %1. Paper orders now fill on delayed public prices (Yahoo Finance, typically "
               "15 min) — simulated, as always. Reconnect the broker in ACCOUNTS for live prices.")
                .arg(public_feed_reason()),
            /*blocking=*/false);
    } else if (feed_state_.status == Status::NoCredentials) {
        // A LIVE (or broker-paper) account without credentials cannot send anything.
        // Credentials that exist but could not be read get their own title.
        const bool unreadable = !AccountManager::instance().credential_read_error(focused_account_id_).isEmpty();
        order_entry_->set_broker_notice(unreadable ? tr("CREDENTIAL UNREADABLE") : tr("BROKER NOT CONNECTED"),
                                        tr("%1, so orders cannot be sent. Connect it in ACCOUNTS for live data, "
                                           "market depth and order placement.")
                                            .arg(public_feed_reason()),
                                        /*blocking=*/true);
    } else {
        // Expired token / refused market data / dropped socket: the order path may
        // still work (or the broker answers with its own error) — inform, never
        // block a trader who may need to exit a position.
        order_entry_->set_broker_notice(tr("NO LIVE MARKET DATA"),
                                        tr("Prices here are delayed Yahoo Finance data: %1. Orders still go to the "
                                           "broker and may be rejected until it is reconnected in ACCOUNTS.")
                                            .arg(public_feed_reason()),
                                        /*blocking=*/false);
    }

    // ── Depth ──
    // A broker that gives no live quotes keeps its reason here too, so a later
    // notice refresh does not turn it back into a bare "No depth data".
    orderbook_->set_empty_message(public_mode_ ? tr("Market depth needs a live broker connection.\n\nPrices on this "
                                                    "screen are delayed public data from Yahoo Finance. Connect a "
                                                    "broker in ACCOUNTS for live depth.")
                                  : broker_quote_refusal_.isEmpty()
                                      ? QString()
                                      : tr("No market depth for %1.\n\n%2").arg(selected_symbol_, broker_quote_refusal_));

    // ── Watchlist + ribbon: a broker that gives no live quotes ──
    apply_broker_quote_notice();

    // ── Blotter / broker-only tabs ──
    QString account_text;
    QString tape_text;
    if (focused_is_builtin()) {
        account_text = tr("Nothing here yet. Place a paper order from the ticket \xE2\x80\x94 fills are simulated on "
                          "delayed public prices.");
    } else if (!have_account) {
        account_text = tr("No account. Pick the paper account in the account menu, or add a broker in ACCOUNTS.");
    } else if (public_mode_ && !local_paper &&
               (feed_state_.status == Status::NoCredentials || feed_state_.status == Status::TokenExpired)) {
        account_text = tr("Nothing to show: %1. Positions, holdings and orders load once the broker is reconnected "
                          "in ACCOUNTS.")
                           .arg(public_feed_reason());
    }
    if (public_mode_)
        tape_text = tr("Time & sales, auctions and the market calendar come from a live broker feed. Connect a "
                       "broker in ACCOUNTS.");
    bottom_panel_->set_empty_notice(account_text, tape_text);

    // ── ACCOUNTS: the obvious next step while on public data (not on the paper
    // account, where public data is the normal, working state) ──
    const bool accent = public_mode_ && !focused_is_builtin();
    if (cmd_bar_ && accent != accounts_btn_accented_) {
        accounts_btn_accented_ = accent;
        // The shared bar paints the role from the live theme: a repaint, no stylesheet.
        auto* acts = cmd_bar_->actions();
        acts->set_action_role(QStringLiteral("accounts"),
                              accent ? ui::ScreenCommandBar::Role::Primary : ui::ScreenCommandBar::Role::Normal);
        acts->set_action_tooltip(QStringLiteral("accounts"),
                                 accent ? tr("Connect a broker for live prices, market depth and order placement (A)")
                                        : tr("Manage broker accounts (A)"));
    }
}

} // namespace fincept::screens
