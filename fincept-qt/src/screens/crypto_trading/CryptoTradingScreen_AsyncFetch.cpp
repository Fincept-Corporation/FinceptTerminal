// src/screens/crypto_trading/CryptoTradingScreen_AsyncFetch.cpp
//
// QtConcurrent-driven REST fetches: candles, live positions/orders/balance,
// my_trades, trading_fees, mark_price, set_leverage, set_margin_mode.
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
#include "trading/ExchangeService.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QCompleter>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>
#include <QSplitter>
#include <QStringListModel>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::crypto;

void CryptoTradingScreen::async_fetch_candles(const QString& symbol, const QString& timeframe, int attempt) {
    if (candles_fetching_.exchange(true))
        return;
    QPointer<CryptoTradingScreen> self = this;
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbol, timeframe, attempt, ex]() {
        auto candles = ExchangeSessionManager::instance().session(ex)->fetch_ohlcv(symbol, timeframe, OHLCV_FETCH_COUNT);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, candles, symbol, timeframe, attempt, ex]() {
                if (!self)
                    return;
                // Release the single-flight guard on the UI thread (it used to be
                // cleared from this worker through the QPointer, which races the
                // widget's destruction).
                self->candles_fetching_.store(false);
                // The user may have moved on while the REST call was in flight.
                if (self->selected_symbol_ != symbol || self->chart_->current_timeframe() != timeframe ||
                    self->exchange_id_ != ex) {
                    // The fetch for what the chart is on NOW was swallowed by the
                    // single-flight guard while this one ran, and this reply is
                    // being discarded — so nothing would ever load its history.
                    // Re-issue for the current selection.
                    self->async_fetch_candles(self->selected_symbol_, self->chart_->current_timeframe());
                    return;
                }

                if (!candles.isEmpty()) {
                    self->chart_->set_candles(candles);
                    self->chart_symbol_ = symbol + QLatin1Char('|') + timeframe;
                    return;
                }

                // Empty result — the daemon errored, rate-limited, or the
                // market has no history. Pushing it into the chart wipes the
                // history and leaves the user watching WS bars trickle in one
                // per minute, each drawn as a giant block (#338). Keep what we
                // have, and only clear when the stale content belongs to a
                // different symbol/timeframe.
                const QString key = symbol + QLatin1Char('|') + timeframe;
                if (self->chart_symbol_ != key) {
                    self->chart_->clear();
                    self->chart_symbol_.clear();
                }
                LOG_DEBUG("CryptoTrading", QString("fetch_ohlcv returned no candles for %1 %2 (attempt %3/%4)")
                                               .arg(symbol, timeframe)
                                               .arg(attempt + 1)
                                               .arg(CANDLE_FETCH_MAX_ATTEMPTS));

                if (attempt + 1 < CANDLE_FETCH_MAX_ATTEMPTS) {
                    // The first fetch races the daemon's exchange handshake on
                    // a cold start; back off and try again rather than leaving
                    // the chart permanently empty.
                    const int delay_ms = CANDLE_FETCH_RETRY_MS * (attempt + 1);
                    QTimer::singleShot(delay_ms, self, [self, symbol, timeframe, attempt]() {
                        if (!self || self->selected_symbol_ != symbol)
                            return;
                        if (self->chart_->current_timeframe() != timeframe)
                            return;
                        self->async_fetch_candles(symbol, timeframe, attempt + 1);
                    });
                } else {
                    LOG_WARN("CryptoTrading", fincept::logf("Chart history unavailable — chart is live-only",
                                                            {{"exchange", ex},
                                                             {"symbol", symbol},
                                                             {"timeframe", timeframe},
                                                             {"attempts", CANDLE_FETCH_MAX_ATTEMPTS}}));
                    // Nothing drawn: say so (RETRY) instead of a blank plot. Streamed
                    // candles, if any arrive, replace this page as they come in.
                    if (self->chart_symbol_.isEmpty())
                        self->chart_->show_feed_error(
                            tr("%1 returned no %2 price history for %3. Live candles are drawn here as they stream "
                               "in; RETRY asks for the history again.")
                                .arg(ex.toUpper(), timeframe, symbol));
                }
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_positions() {
    QPointer<CryptoTradingScreen> self = this;
    // Snapshot the symbol on the UI thread. Reading self->selected_symbol_
    // from the worker is a torn read of an implicitly-shared QString being
    // written by the UI thread, and the `if (!self)` above it is a TOCTOU —
    // the widget can die between the check and the dereference.
    const QString symbol = market_.valid() ? market_.symbol : selected_symbol_;
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbol, ex]() {
        if (!self) {
            // Widget destroyed before dispatch — no counter to decrement.
            return;
        }
        // A throw here would skip the invokeMethod below, leaving
        // `live_inflight_` permanently above zero — `refresh_live_data()`
        // then returns early on every tick and LIVE data never updates again.
        QJsonObject result;
        try {
            result = ExchangeSessionManager::instance().session(ex)->fetch_positions_live(symbol);
        } catch (...) {
            LOG_WARN("CryptoTrading", "fetch_positions_live threw");
            // Without an "error" key the panel kept whatever rows it had (paper ones
            // included) with no sign the live fetch failed.
            result = QJsonObject{{QStringLiteral("error"), QStringLiteral("positions request failed")}};
        }
        QMetaObject::invokeMethod(
            self,
            [self, result, ex]() {
                if (!self)
                    return;
                if (self->trading_mode_ != crypto::TradingMode::Live || self->exchange_id_ != ex) {
                    self->live_inflight_.fetch_sub(1); // PAPER or another venue now — drop the rows
                    return;
                }
                if (result.contains("positions"))
                    self->bottom_panel_->set_live_positions(result.value("positions").toArray());
                else if (result.contains("error"))
                    self->bottom_panel_->set_live_positions_unavailable(result.value("error").toString());
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_orders() {
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = market_.valid() ? market_.symbol : selected_symbol_; // snapshot on the UI thread
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbol, ex]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeSessionManager::instance().session(ex)->fetch_open_orders_live(symbol);
        } catch (...) {
            LOG_WARN("CryptoTrading", "fetch_open_orders_live threw");
            result = QJsonObject{{QStringLiteral("error"), QStringLiteral("open-orders request failed")}};
        }
        QMetaObject::invokeMethod(
            self,
            [self, result, ex]() {
                if (!self)
                    return;
                if (self->trading_mode_ != crypto::TradingMode::Live || self->exchange_id_ != ex) {
                    self->live_inflight_.fetch_sub(1); // PAPER or another venue now — drop the rows
                    return;
                }
                if (result.contains("orders"))
                    self->bottom_panel_->set_live_orders(result.value("orders").toArray());
                else if (result.contains("error"))
                    self->bottom_panel_->set_live_orders_unavailable(result.value("error").toString());
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_balance() {
    QPointer<CryptoTradingScreen> self = this;
    // The quote currency of the pair actually being traded, so a USD- or
    // USDC-margined account isn't reported as $0.00. Hard-coding "USDT" made
    // the whole LIVE balance read empty on every non-USDT venue — which looks
    // identical to a genuinely empty account right next to a live BUY button.
    // The currency the resolved market spends: its quote, or for a contract the
    // settlement currency (a USDT-settled perp margins in USDT).
    // No quote in the pair text: none is guessed — the stable-quote fallback below
    // picks the balance the account holds, labelled with its own currency.
    QString quote = crypto::pair_quote_ccy(selected_symbol_); // "BTC/USDC:USDC" -> "USDC"
    if (market_.valid())
        quote = market_.is_contract() && !market_.settle.isEmpty() ? market_.settle : market_.quote;
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, quote, ex]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeSessionManager::instance().session(ex)->fetch_balance();
        } catch (...) {
            // Must still post back: `live_inflight_` is only decremented on the
            // UI thread, and if it never reaches zero `refresh_live_data()`
            // skips every subsequent tick and live data freezes permanently.
            LOG_WARN("CryptoTrading", fincept::logf("fetch_balance threw", {{"exchange", ex}}));
            result = QJsonObject{{QStringLiteral("error"), QStringLiteral("balance fetch threw")}};
        }
        QMetaObject::invokeMethod(
            self,
            [self, result, quote, ex]() {
                if (!self)
                    return;
                if (self->trading_mode_ != crypto::TradingMode::Live || self->exchange_id_ != ex) {
                    // Back in PAPER: a late live balance must not overwrite the
                    // ticket's paper balance.
                    self->live_inflight_.fetch_sub(1);
                    return;
                }
                // A daemon/bridge failure (e.g. bad API key) returns an "error"
                // key or simply omits the balance keys — both would otherwise
                // decode to 0.0 and render a misleading $0.00 that looks exactly
                // like a genuinely empty account. Surface an explicit unavailable
                // state instead, and leave the order-entry balance untouched.
                // The daemon's fetch_balance returns {"balances": {CCY: {free, used,
                // total}}} (non-zero currencies only); this reader only knew the
                // legacy {"total": {CCY: x}, "free": {...}, "used": {...}} shape, so
                // a perfectly good response always landed in the UNAVAILABLE branch
                // and the live balance never reached the ticket. Accept both.
                const bool daemon_shape = result.contains("balances");
                if (result.contains("error") || (!daemon_shape && !result.contains("total"))) {
                    self->bottom_panel_->set_balance_unavailable(
                        result.value("error").toString(QStringLiteral("no data")));
                } else {
                    QJsonObject totals;
                    QJsonObject frees;
                    QJsonObject useds;
                    if (daemon_shape) {
                        const QJsonObject per_ccy = result.value("balances").toObject();
                        for (auto it = per_ccy.constBegin(); it != per_ccy.constEnd(); ++it) {
                            const QJsonObject b = it.value().toObject();
                            totals.insert(it.key(), b.value("total"));
                            frees.insert(it.key(), b.value("free"));
                            useds.insert(it.key(), b.value("used"));
                        }
                    } else {
                        totals = result.value("total").toObject();
                        frees = result.value("free").toObject();
                        useds = result.value("used").toObject();
                    }
                    // Fall back through the common stable quotes so an account
                    // funded in USDC on a USDT-quoted pair still shows — labelled
                    // with the currency it is actually held in (USDC is not USDT:
                    // the ticket converts it with a real rate before comparing).
                    QString ccy = quote;
                    if (ccy.isEmpty() || !totals.contains(ccy)) {
                        for (const QString& alt : {QStringLiteral("USDT"), QStringLiteral("USDC"),
                                                   QStringLiteral("USD"), QStringLiteral("BUSD")}) {
                            if (totals.contains(alt)) {
                                ccy = alt;
                                break;
                            }
                        }
                    }
                    const double total = totals.value(ccy).toDouble();
                    const double free = frees.value(ccy).toDouble();
                    const double used = useds.value(ccy).toDouble();
                    self->bottom_panel_->set_live_balance(free, total, used, ccy);
                    self->order_entry_->set_balance(free, ccy);
                }
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

// ============================================================================
// Slot Handlers
// ============================================================================

void CryptoTradingScreen::async_fetch_my_trades() {
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = market_.valid() ? market_.symbol : selected_symbol_; // snapshot on the UI thread
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbol, ex]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeSessionManager::instance().session(ex)->fetch_my_trades(symbol);
        } catch (...) {
            LOG_WARN("CryptoTrading", "fetch_my_trades threw");
        }
        QMetaObject::invokeMethod(
            self,
            [self, result, ex]() {
                if (!self)
                    return;
                if (self->trading_mode_ == crypto::TradingMode::Live && self->exchange_id_ == ex)
                    self->bottom_panel_->update_my_trades(result);
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_trading_fees() {
    QPointer<CryptoTradingScreen> self = this;
    // The resolved market only when it is the one for the pair and venue on screen
    // (it lags a switch until async_fetch_market_limits() answers). Snapshot on the UI thread.
    const QString pair = selected_symbol_;
    const bool resolved = market_.valid() && market_.requested == pair && market_.exchange == exchange_id_;
    const QString symbol = resolved ? market_.symbol : pair;
    const QString ex = exchange_id_;
    // Without an API key the daemon answers with the venue's PUBLISHED schedule
    // (source "market"); with one, the account's own tier. Read-only either way.
    (void)QtConcurrent::run([self, symbol, pair, ex]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeSessionManager::instance().session(ex)->fetch_trading_fees(symbol);
        } catch (...) {
            LOG_WARN("CryptoTrading", fincept::logf("fetch_trading_fees threw", {{"exchange", ex}, {"symbol", symbol}}));
            result = QJsonObject{{QStringLiteral("error"), QStringLiteral("fee request failed")}};
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result, ex, pair]() {
                // Another venue or pair now: its own request fills the tab.
                if (!self || self->exchange_id_ != ex || self->selected_symbol_ != pair)
                    return;
                self->bottom_panel_->update_fees(result);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_mark_price() {
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_; // snapshot on the UI thread — see above
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbol, ex]() {
        if (!self)
            return;
        auto mp = ExchangeSessionManager::instance().session(ex)->fetch_mark_price(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, mp, symbol, ex]() {
                if (!self || self->exchange_id_ != ex || self->selected_symbol_ != symbol)
                    return;
                self->ticker_bar_->update_mark_price(mp.mark_price, mp.index_price);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::on_apply_leverage(int leverage, const QString& mode) {
    // Leverage / margin mode are ACCOUNT settings on the exchange. In PAPER mode the
    // controls only drive the cost preview. In LIVE they change nothing until APPLY,
    // and APPLY asks first, naming the scope — on Bybit the margin mode is
    // account-wide, every other venue applies it to the one market.
    if (trading_mode_ != crypto::TradingMode::Live)
        return;
    const crypto::ResolvedMarket mk = market_;
    if (!mk.valid() || mk.match != QLatin1String("exact") || !mk.is_contract()) {
        order_entry_->show_order_refusal(
            tr("⚠ Leverage and margin mode apply to an exact contract market; %1 is not one on %2.")
                .arg(selected_symbol_, exchange_id_.toUpper()));
        return;
    }
    if (account_action_busy_) {
        order_entry_->show_order_refusal(tr("⚠ An account change is still running — wait for its result."));
        return;
    }
    const bool lev_change = mk.leverage && leverage > 0;
    const bool mode_change = mk.margin_mode && (mode == QLatin1String("cross") || mode == QLatin1String("isolated"));
    if (!lev_change && !mode_change) {
        order_entry_->show_order_refusal(
            tr("⚠ %1 does not let this app change leverage or margin mode.").arg(exchange_id_.toUpper()));
        return;
    }
    const LiveContext ctx = live_context();
    QString text = tr("Change account settings on %1 (%2)?\n\nMarket        %3\n")
                       .arg(ctx.exchange.toUpper(), ctx.account_label, mk.describe());
    if (lev_change)
        text += tr("Leverage      %1x — for %2 on this account, including a position already open there\n")
                    .arg(leverage)
                    .arg(mk.symbol);
    if (mode_change)
        text += mk.margin_scope == QLatin1String("account")
                    ? tr("Margin mode   %1 — on %2 this applies to the WHOLE ACCOUNT: every market and every "
                         "open position\n")
                          .arg(mode, ctx.exchange.toUpper())
                    : tr("Margin mode   %1 — for %2\n").arg(mode, mk.symbol);
    text += tr("\nThe exchange's values are read back afterwards.");
    QMessageBox box(QMessageBox::Warning, tr("Change Leverage / Margin Mode"), text, QMessageBox::Yes | QMessageBox::No,
                    this);
    box.setDefaultButton(QMessageBox::No);
    if (auto* yes = box.button(QMessageBox::Yes))
        yes->setText(tr("Apply to account"));
    if (box.exec() != QMessageBox::Yes)
        return;
    QString why;
    if (!live_context_current(ctx, &why)) {
        QMessageBox::warning(this, tr("Not Applied"), tr("Nothing was sent: %1").arg(why));
        return;
    }
    account_action_busy_ = true;
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self, ctx, mk, leverage, mode, lev_change, mode_change]() {
        QStringList lines;
        bool unknown = false;
        auto* sess = ExchangeSessionManager::instance().session(ctx.exchange);
        // `kind` / `value` name the change in the audit log (one line per change).
        auto run = [&](const QString& method, const QJsonObject& args, const QString& what, bool is_leverage,
                       const QString& value) {
            const QJsonObject r = sess->request_as(method, args, ctx.creds, 20000);
            const QString outcome = crypto::account_change_outcome(r);
            if (is_leverage)
                LOG_AUDIT("CryptoTrading", "Leverage changed",
                          {{"venue", ctx.exchange},
                           {"account", fincept::log_mask(ctx.account_id)},
                           {"symbol", mk.symbol},
                           {"leverage", value},
                           {"outcome", outcome},
                           {"reason", r.value("error").toString()}});
            else
                LOG_AUDIT("CryptoTrading", "Margin mode changed",
                          {{"venue", ctx.exchange},
                           {"account", fincept::log_mask(ctx.account_id)},
                           {"symbol", mk.symbol},
                           {"mode", value},
                           {"outcome", outcome},
                           {"reason", r.value("error").toString()}});
            if (outcome == QLatin1String("accepted"))
                lines << tr("%1: done").arg(what);
            else if (outcome == QLatin1String("rejected"))
                lines << tr("%1: refused — %2").arg(what, r.value("error").toString());
            else {
                unknown = true;
                lines << tr("%1: not confirmed — it may have been applied (%2)").arg(what, r.value("error").toString());
            }
        };
        if (mode_change)
            run(QStringLiteral("set_margin_mode"),
                QJsonObject{{"symbol", mk.symbol}, {"mode", mode}, {"leverage", leverage}},
                tr("Margin mode %1").arg(mode), false, mode);
        if (lev_change)
            run(QStringLiteral("set_leverage"), QJsonObject{{"symbol", mk.symbol}, {"leverage", leverage}},
                tr("Leverage %1x").arg(leverage), true, QString::number(leverage));
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, lines, unknown]() {
                if (!self)
                    return;
                self->account_action_busy_ = false;
                const QString msg = lines.join(QLatin1Char('\n'));
                // Each change was written to the audit log as it was answered (Leverage changed /
                // Margin mode changed, with its outcome).
                if (unknown)
                    QMessageBox::warning(self, tr("Leverage / Margin Mode"),
                                         msg + tr("\n\nThe values shown are read back from the exchange now."));
                else
                    self->order_entry_->reset_after_fill(msg, QStringLiteral("info"));
                self->async_fetch_leverage_info();
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_leverage_info() {
    if (trading_mode_ != crypto::TradingMode::Live || !market_.valid() ||
        market_.match != QLatin1String("exact") || !market_.is_contract())
        return;
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = market_.symbol;
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, symbol, ex]() {
        const QJsonObject r =
            ExchangeSessionManager::instance().session(ex)->request("fetch_leverage_info", {{"symbol", symbol}});
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, symbol, ex, r]() {
                if (!self || self->exchange_id_ != ex || self->market_.symbol != symbol)
                    return;
                if (r.contains("error")) {
                    self->order_entry_->set_account_leverage(0, QString());
                    return;
                }
                self->order_entry_->set_account_leverage(static_cast<int>(r.value("leverage").toDouble()),
                                                         r.value("margin_mode").toString());
            },
            Qt::QueuedConnection);
    });
}

namespace {
// Prints fetched to seed the time & sales tape on a symbol / exchange switch.
constexpr int kTapeSeedTrades = 50;
} // namespace

void CryptoTradingScreen::async_seed_trade_tape() {
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_; // snapshot on the UI thread — see above
    const QString exchange = exchange_id_;
    (void)QtConcurrent::run([self, symbol, exchange]() {
        if (!self)
            return;
        QVector<TradeData> trades;
        try {
            trades = ExchangeSessionManager::instance().session(exchange)->fetch_trades(symbol, kTapeSeedTrades);
        } catch (...) {
            LOG_WARN("CryptoTrading", "fetch_trades threw");
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, symbol, exchange, trades]() {
                if (!self || trades.isEmpty())
                    return;
                // The user moved on (or the call reached another venue's session).
                if (self->selected_symbol_ != symbol || self->exchange_id_ != exchange)
                    return;
                QVector<crypto::TradeEntry> history;
                history.reserve(trades.size());
                for (const auto& td : trades) {
                    if (td.price <= 0.0 || td.timestamp <= 0)
                        continue;
                    crypto::TradeEntry e;
                    e.id = td.id;
                    e.side = td.side.toLower();
                    e.price = td.price;
                    e.amount = td.amount;
                    e.timestamp = td.timestamp;
                    history.append(e);
                }
                self->bottom_panel_->seed_trade_history(history);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_market_limits() {
    // Which market the selected pair is on this exchange — from the daemon's one
    // resolver, the same the live stream and the order path use — with its
    // contract facts, minimums and what the ticket may send there. Until the
    // reply, LIVE orders are refused ("still checking").
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_;
    const QString exchange = exchange_id_;
    market_ = crypto::ResolvedMarket{};
    market_.exchange = exchange;
    market_.requested = symbol;
    order_entry_->set_market_limits(0.0, 0.0); // unknown until the reply
    order_entry_->set_market(market_);
    bottom_panel_->set_scope_market(QString());
    apply_price_market(); // the pair's own quote asset until the market resolves
    const QStringList watch = watchlist_symbols_;
    (void)QtConcurrent::run([self, symbol, exchange, watch]() {
        if (!self)
            return;
        QJsonObject reply;
        QJsonObject notes;
        try {
            auto* s = ExchangeSessionManager::instance().session(exchange);
            // A cold market list (Gate: ~23 s) is loaded by this call.
            reply = s->request("fetch_markets", {{"symbol", symbol}}, 45000);
            // How every watchlist pair resolves here (local to the daemon, cheap).
            notes = s->request("fetch_markets", {{"symbols", QJsonArray::fromStringList(watch)}}, 15000);
        } catch (...) {
            LOG_WARN("CryptoTrading", fincept::logf("fetch_markets threw", {{"exchange", exchange}, {"symbol", symbol}}));
            reply = QJsonObject{{"error", QStringLiteral("market lookup failed")}};
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, symbol, exchange, reply, notes]() {
                if (!self || self->exchange_id_ != exchange)
                    return;
                if (notes.contains("resolutions")) {
                    QHash<QString, QString> shown_from;
                    QHash<QString, QString> missing;
                    for (const auto& v : notes.value("resolutions").toArray()) {
                        const QJsonObject r = v.toObject();
                        const QString match = r.value("match").toString();
                        if (match == QLatin1String("substituted"))
                            shown_from.insert(r.value("requested").toString(), r.value("symbol").toString());
                        else if (match == QLatin1String("missing"))
                            missing.insert(r.value("requested").toString(), r.value("reason").toString());
                    }
                    self->watchlist_->set_market_notes(shown_from, missing);
                }
                if (self->selected_symbol_ != symbol)
                    return;
                crypto::ResolvedMarket r = crypto::parse_resolved_market(reply, exchange, symbol);
                if (!r.loaded) {
                    // Say why instead of "checking…" forever; LIVE stays refused.
                    r.loaded = true;
                    r.reason = tr("could not check the market (%1)").arg(reply.value("error").toString());
                }
                self->market_ = r;
                self->order_entry_->set_market(r);
                self->apply_price_market(); // prices come from this market: its quote asset is their unit
                if (r.valid())
                    self->order_entry_->set_market_limits(r.min_base, r.min_cost, r.symbol,
                                                          r.is_contract() && !r.settle.isEmpty() ? r.settle : r.quote);
                self->bottom_panel_->set_scope_market(r.valid() && r.match == QLatin1String("exact") ? r.symbol
                                                                                                    : QString());
                self->update_futures_visibility();
                if (self->trading_mode_ == crypto::TradingMode::Live)
                    self->async_fetch_leverage_info();
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::reconcile_after_unconfirmed() {
    // live_inflight_ makes refresh_live_data() skip while an earlier round is
    // still out, so the delayed rounds are the ones that reliably land; the
    // daemon may complete the request a few seconds after the caller gave up.
    refresh_live_data();
    QPointer<CryptoTradingScreen> self = this;
    for (const int delay_ms : {3000, 10000}) {
        QTimer::singleShot(delay_ms, this, [self]() {
            if (self)
                self->refresh_live_data();
        });
    }
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────
} // namespace fincept::screens
