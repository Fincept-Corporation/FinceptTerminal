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
        self->candles_fetching_ = false;
        QMetaObject::invokeMethod(
            self,
            [self, candles, symbol, timeframe, attempt]() {
                if (!self)
                    return;
                // The user may have moved on while the REST call was in flight.
                if (self->selected_symbol_ != symbol || self->chart_->current_timeframe() != timeframe)
                    return;

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
    (void)QtConcurrent::run([self]() {
        if (!self) {
            // Widget destroyed before dispatch — no counter to decrement.
            return;
        }
        // A throw here would skip the invokeMethod below, leaving
        // `live_inflight_` permanently above zero — `refresh_live_data()`
        // then returns early on every tick and LIVE data never updates again.
        QJsonObject result;
        try {
            result = ExchangeService::instance().fetch_positions_live(self->selected_symbol_);
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
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_orders() {
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeService::instance().fetch_open_orders_live(self->selected_symbol_);
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
                if (result.contains("error") || !result.contains("total")) {
                    self->bottom_panel_->set_balance_unavailable(
                        result.value("error").toString(QStringLiteral("no data")));
                } else {
                    const QJsonObject totals = result.value("total").toObject();
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
                    const double free = result.value("free").toObject().value(ccy).toDouble();
                    const double used = result.value("used").toObject().value(ccy).toDouble();
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
    (void)QtConcurrent::run([self]() {
        if (!self)
            return;
        QJsonObject result;
        try {
            result = ExchangeService::instance().fetch_my_trades(self->selected_symbol_);
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
    (void)QtConcurrent::run([self]() {
        if (!self)
            return;
        auto result = ExchangeService::instance().fetch_trading_fees(self->selected_symbol_);
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
    (void)QtConcurrent::run([self]() {
        if (!self)
            return;
        auto mp = ExchangeService::instance().fetch_mark_price(self->selected_symbol_);
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
    const QString symbol = selected_symbol_;
    (void)QtConcurrent::run([symbol, leverage]() { ExchangeService::instance().set_leverage(symbol, leverage); });
}

void CryptoTradingScreen::async_set_margin_mode(const QString& mode) {
    const QString symbol = selected_symbol_;
    const QString m = mode;
    (void)QtConcurrent::run([symbol, m]() { ExchangeService::instance().set_margin_mode(symbol, m); });
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────
} // namespace fincept::screens
