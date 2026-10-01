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
    (void)QtConcurrent::run([self, symbol, timeframe, attempt]() {
        auto candles = ExchangeService::instance().fetch_ohlcv(symbol, timeframe, OHLCV_FETCH_COUNT);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, candles, symbol, timeframe, attempt]() {
                if (!self)
                    return;
                // Release the single-flight guard on the UI thread (it used to be
                // cleared from this worker through the QPointer, which races the
                // widget's destruction).
                self->candles_fetching_.store(false);
                // The user may have moved on while the REST call was in flight.
                if (self->selected_symbol_ != symbol || self->chart_->current_timeframe() != timeframe) {
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
                LOG_WARN("CryptoTrading", QString("fetch_ohlcv returned no candles for %1 %2 (attempt %3/%4)")
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
                    LOG_ERROR("CryptoTrading",
                              QString("no OHLCV history for %1 %2 after %3 attempts — chart is live-only")
                                  .arg(symbol, timeframe)
                                  .arg(CANDLE_FETCH_MAX_ATTEMPTS));
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
    const QString symbol = selected_symbol_;
    (void)QtConcurrent::run([self, symbol]() {
        if (!self) {
            // Widget destroyed before dispatch — no counter to decrement.
            return;
        }
        // A throw here would skip the invokeMethod below, leaving
        // `live_inflight_` permanently above zero — `refresh_live_data()`
        // then returns early on every tick and LIVE data never updates again.
        QJsonObject result;
        try {
            result = ExchangeService::instance().fetch_positions_live(symbol);
        } catch (...) {
            LOG_WARN("CryptoTrading", "fetch_positions_live threw");
        }
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
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
    const QString symbol = selected_symbol_; // snapshot on the UI thread — see above
    (void)QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeService::instance().fetch_open_orders_live(symbol);
        } catch (...) {
            LOG_WARN("CryptoTrading", "fetch_open_orders_live threw");
        }
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
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
    const int slash = selected_symbol_.indexOf(QLatin1Char('/'));
    QString quote = slash > 0 ? selected_symbol_.mid(slash + 1) : QStringLiteral("USDT");
    const int colon = quote.indexOf(QLatin1Char(':')); // settled perps: "BTC/USDC:USDC"
    if (colon > 0)
        quote = quote.left(colon);
    (void)QtConcurrent::run([self, quote]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeService::instance().fetch_balance();
        } catch (...) {
            // Must still post back: `live_inflight_` is only decremented on the
            // UI thread, and if it never reaches zero `refresh_live_data()`
            // skips every subsequent tick and live data freezes permanently.
            result = QJsonObject{{QStringLiteral("error"), QStringLiteral("balance fetch threw")}};
        }
        QMetaObject::invokeMethod(
            self,
            [self, result, quote]() {
                if (!self)
                    return;
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
                    // funded in USDC on a USDT-quoted pair still shows.
                    QString ccy = quote;
                    if (!totals.contains(ccy)) {
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
                    self->bottom_panel_->set_live_balance(free, total, used);
                    self->order_entry_->set_balance(free);
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
    const QString symbol = selected_symbol_; // snapshot on the UI thread — see above
    (void)QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeService::instance().fetch_my_trades(symbol);
        } catch (...) {
            LOG_WARN("CryptoTrading", "fetch_my_trades threw");
        }
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                self->bottom_panel_->update_my_trades(result);
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_trading_fees() {
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_; // snapshot on the UI thread — see above
    (void)QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        auto result = ExchangeService::instance().fetch_trading_fees(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                self->bottom_panel_->update_fees(result);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_mark_price() {
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_; // snapshot on the UI thread — see above
    (void)QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        auto mp = ExchangeService::instance().fetch_mark_price(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, mp]() {
                if (!self)
                    return;
                self->ticker_bar_->update_mark_price(mp.mark_price, mp.index_price);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_set_leverage(int leverage) {
    // Leverage / margin mode are ACCOUNT settings on the exchange. In PAPER mode the
    // ticket's controls only drive the cost preview (the paper engine runs 1x), and
    // forwarding them anyway silently re-levers the user's REAL account whenever API
    // keys are configured. Only touch the exchange in LIVE mode.
    if (trading_mode_ != crypto::TradingMode::Live)
        return;
    const QString symbol = selected_symbol_;
    const int seq = ++leverage_seq_;
    QPointer<CryptoTradingScreen> self = this;
    // Debounce: the spin box emits on every step (wheel / held arrow), and each
    // used to be its own exchange API call. Only the value the user settles on
    // should be sent.
    QTimer::singleShot(600, this, [self, symbol, leverage, seq]() {
        if (!self || seq != self->leverage_seq_ || self->trading_mode_ != crypto::TradingMode::Live)
            return;
        (void)QtConcurrent::run([self, symbol, leverage]() {
            QString err;
            try {
                const QJsonObject r = ExchangeService::instance().set_leverage(symbol, leverage);
                if (r.contains("error") || !r.value("success").toBool(true))
                    err = r.value("error").toString(QStringLiteral("The exchange rejected the leverage change."));
            } catch (...) {
                err = QStringLiteral("leverage request failed");
            }
            if (err.isEmpty() || !self)
                return;
            LOG_WARN("CryptoTrading", QString("set_leverage(%1, %2x) failed: %3").arg(symbol).arg(leverage).arg(err));
            QMetaObject::invokeMethod(
                self,
                [self, symbol, leverage, err]() {
                    if (!self)
                        return;
                    QMessageBox::warning(self, tr("Leverage"),
                                         tr("Could not set %1 leverage to %2x:\n%3").arg(symbol).arg(leverage).arg(err));
                },
                Qt::QueuedConnection);
        });
    });
}

void CryptoTradingScreen::async_set_margin_mode(const QString& mode) {
    if (trading_mode_ != crypto::TradingMode::Live)
        return; // see async_set_leverage — never change a real account from PAPER mode
    const QString symbol = selected_symbol_;
    const QString m = mode;
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self, symbol, m]() {
        QString err;
        try {
            const QJsonObject r = ExchangeService::instance().set_margin_mode(symbol, m);
            if (r.contains("error") || !r.value("success").toBool(true))
                err = r.value("error").toString(QStringLiteral("The exchange rejected the margin-mode change."));
        } catch (...) {
            err = QStringLiteral("margin-mode request failed");
        }
        if (err.isEmpty() || !self)
            return;
        LOG_WARN("CryptoTrading", QString("set_margin_mode(%1, %2) failed: %3").arg(symbol, m, err));
        QMetaObject::invokeMethod(
            self,
            [self, symbol, m, err]() {
                if (!self)
                    return;
                QMessageBox::warning(self, tr("Margin mode"),
                                     tr("Could not set %1 margin mode to %2:\n%3").arg(symbol, m, err));
            },
            Qt::QueuedConnection);
    });
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────
} // namespace fincept::screens
