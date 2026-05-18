// src/screens/crypto_trading/CryptoTradingScreen_Handlers.cpp
//
// User-action handlers: exchange/symbol switching, mode toggle, API config,
// order submit/cancel, order-book click, search.
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


void CryptoTradingScreen::on_exchange_changed(const QString& exchange) {
    if (exchange == exchange_id_)
        return;
    LOG_INFO(TAG, QString("Exchange changed: %1 → %2").arg(exchange_id_, exchange));
    exchange_id_ = exchange;
    exchange_btn_->setText(exchange.toUpper());
    if (ws_transport_) {
        const bool native = (exchange == "kraken");
        ws_transport_->setText(native ? "NATIVE" : "DAEMON");
        ws_transport_->setToolTip(native
                                      ? "Native C++ WebSocket — direct connection, no Python subprocess"
                                      : "ws_stream.py via ccxt.pro — Python subprocess");
    }

    auto& es = ExchangeService::instance();

    // Phase 3: sessions stay warm for the app lifetime. Do NOT stop the old
    // exchange's WS — `ExchangeSessionManager` keeps it running so switching
    // back is instant. Dropping our hub subscriptions is enough to stop the
    // old exchange's ticks landing in our pending_* buffers; the session
    // keeps updating its cache and publishing to the hub in the background.
    hub_unsubscribe_topics();

    // Clear accumulated buffers — stale data from the old exchange is useless.
    pending_tickers_.clear();
    pending_orderbook_ = {};
    has_pending_orderbook_ = false;
    pending_primary_ticker_ = {};
    has_pending_primary_ = false;
    pending_candles_.clear();
    pending_trades_.clear();
    last_ws_state_ = -1;  // re-evaluate feed mode after new stream connects

    // Reset fetch guards — a prior in-flight fetch from the old exchange must
    // not suppress the first fetch against the new exchange.
    candles_fetching_.store(false);
    live_inflight_.store(0);

    es.set_exchange(exchange_id_);

    // Re-initialize — re-registers the four WS callbacks on the NEW session.
    // If that session's WS is already warm (we visited it earlier), init_exchange
    // skips the start_ws_stream() call, so there's no second handshake.
    initialized_ = false;
    init_exchange();

    load_portfolio();

    // Chart history (left of "now") still needs one REST fetch — Kraken WS
    // only streams the current bar. Everything else (ticker, orderbook,
    // watchlist) repopulates from the new session's WS subscriptions within
    // ~1s of the handshake. Funding/OI is perp-only and stays on its 30s
    // market_info_timer_ — those endpoints aren't on the public WS feed.
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    refresh_market_info();

    ScreenStateManager::instance().notify_changed(this);
}

void CryptoTradingScreen::on_symbol_selected(const QString& symbol) {
    if (symbol.isEmpty() || symbol == selected_symbol_)
        return;
    switch_symbol(symbol);
    ScreenStateManager::instance().notify_changed(this);

    // Phase 7: publish to the linked group so other panels in the same
    // group switch to this pair. Source = `this` so on_group_symbol_changed
    // can suppress its own re-publish below.
    if (link_group_ != SymbolGroup::None) {
        SymbolRef ref;
        ref.symbol = symbol;
        ref.asset_class = QStringLiteral("crypto");
        ref.exchange = exchange_id_;
        SymbolContext::instance().set_group_symbol(link_group_, ref, this);
    }
}

void CryptoTradingScreen::on_group_symbol_changed(const SymbolRef& ref) {
    // Phase 7: subscribe-side. Only react to crypto symbols — an inbound
    // equity ticker has nothing meaningful to do here. Empty asset_class
    // is treated as "unknown" and ignored to avoid surprising the user
    // with a stale cross-asset propagation.
    if (!ref.is_valid())
        return;
    if (ref.asset_class != QStringLiteral("crypto"))
        return;
    if (ref.symbol == selected_symbol_)
        return; // already showing this pair
    // Reuse the existing publish-suppressing path: switch_symbol mutates
    // selected_symbol_ but doesn't re-emit because we don't go through
    // on_symbol_selected.
    switch_symbol(ref.symbol);
}

SymbolRef CryptoTradingScreen::current_symbol() const {
    if (selected_symbol_.isEmpty())
        return {};
    SymbolRef r;
    r.symbol = selected_symbol_;
    r.asset_class = QStringLiteral("crypto");
    r.exchange = exchange_id_;
    return r;
}

void CryptoTradingScreen::switch_symbol(const QString& symbol) {
    LOG_INFO(TAG, QString("Symbol changed: %1 → %2").arg(selected_symbol_, symbol));
    auto& es = ExchangeService::instance();
    es.unwatch_symbol(selected_symbol_, portfolio_id_);
    selected_symbol_ = symbol;
    symbol_input_->setText(symbol);
    ticker_bar_->set_symbol(symbol);
    order_entry_->set_symbol(symbol);
    watchlist_->set_active_symbol(symbol);

    // Drop per-symbol buffers tied to the old symbol to prevent cross-symbol leakage.
    has_pending_primary_ = false;
    pending_primary_ticker_ = {};
    has_pending_orderbook_ = false;
    pending_orderbook_ = {};
    pending_candles_.clear();
    pending_trades_.clear();
    market_info_cache_ = {};

    es.watch_symbol(selected_symbol_, portfolio_id_);
    es.set_ws_primary_symbol(symbol);

    // Three of the five subscriptions (ticker/orderbook/trades + ohlc
    // pattern) are primary-symbol-specific and need to be re-bound to the
    // new symbol. The watchlist ticker pattern doesn't change. Simpler to
    // resubscribe wholesale than diff.
    hub_subscribe_topics();

    // WS-only mode: ticker + orderbook reflow naturally as the new symbol's
    // WS subscriptions kick in. Only history needs a REST fetch.
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void CryptoTradingScreen::on_mode_toggled() {
    const bool is_live = mode_btn_->isChecked();
    trading_mode_ = is_live ? TradingMode::Live : TradingMode::Paper;
    mode_btn_->setText(is_live ? "LIVE" : "PAPER");
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);

    if (is_live) {
        live_data_timer_->start(5000);
        refresh_live_data();
    } else {
        live_data_timer_->stop();
        refresh_portfolio();
    }
}

void CryptoTradingScreen::on_api_clicked() {
    auto* dlg = new CryptoCredentials(exchange_id_, this);
    connect(dlg, &CryptoCredentials::credentials_saved, this,
            [this](const QString& key, const QString& secret, const QString& pw) {
                ExchangeCredentials creds;
                creds.api_key = key;
                creds.secret = secret;
                creds.password = pw;
                ExchangeService::instance().set_credentials(creds);
                LOG_INFO(TAG, "Credentials saved for " + exchange_id_);
            });
    dlg->exec();
    dlg->deleteLater();
}

void CryptoTradingScreen::on_order_submitted(const QString& side, const QString& order_type, double qty, double price,
                                             double stop_price, double sl, double tp) {
    LOG_INFO(TAG, QString("Order submit: mode=%1 %2 %3 qty=%4 px=%5 stop=%6 sl=%7 tp=%8 sym=%9")
                      .arg(trading_mode_ == TradingMode::Paper ? "PAPER" : "LIVE", side, order_type)
                      .arg(qty).arg(price).arg(stop_price).arg(sl).arg(tp).arg(selected_symbol_));
    try {
        if (trading_mode_ == TradingMode::Paper) {
            auto ticker = ExchangeService::instance().get_cached_price(selected_symbol_);
            std::optional<double> price_opt;
            if (order_type == "market")
                price_opt = ticker.last > 0 ? ticker.last : 1000.0;
            else if (price > 0)
                price_opt = price;

            std::optional<double> stop_opt;
            if (stop_price > 0)
                stop_opt = stop_price;

            auto order = pt_place_order(portfolio_id_, selected_symbol_, side, order_type, qty, price_opt, stop_opt);
            if (order_type == "market") {
                double fill = ticker.last > 0 ? ticker.last : price_opt.value_or(1000.0);
                pt_fill_order(order.id, fill);
            } else {
                OrderMatcher::instance().add_order(order);
                if (sl > 0 || tp > 0)
                    OrderMatcher::instance().set_sl_tp(portfolio_id_, selected_symbol_, order.id, sl, tp);
            }
            refresh_portfolio();
        } else {
            QPointer<CryptoTradingScreen> self = this;
            (void)QtConcurrent::run([self, side, order_type, qty, price]() {
                if (!self)
                    return;
                ExchangeService::instance().place_exchange_order(self->selected_symbol_, side, order_type, qty, price);
                QMetaObject::invokeMethod(
                    self,
                    [self]() {
                        if (!self)
                            return;
                        self->refresh_live_data();
                    },
                    Qt::QueuedConnection);
            });
        }
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, QString("Order failed: %1").arg(e.what()));
    }
}

void CryptoTradingScreen::on_cancel_order(const QString& order_id) {
    LOG_INFO(TAG, QString("Cancel order: %1 (%2)").arg(order_id, trading_mode_ == TradingMode::Paper ? "paper" : "live"));
    if (trading_mode_ == TradingMode::Paper) {
        try {
            pt_cancel_order(order_id);
            OrderMatcher::instance().remove_order(order_id);
            refresh_portfolio();
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Cancel failed: %1").arg(e.what()));
        }
    } else {
        QPointer<CryptoTradingScreen> self = this;
        (void)QtConcurrent::run([self, order_id]() {
            if (!self)
                return;
            ExchangeService::instance().cancel_exchange_order(order_id, self->selected_symbol_);
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    if (!self)
                        return;
                    self->refresh_live_data();
                },
                Qt::QueuedConnection);
        });
    }
}

void CryptoTradingScreen::on_ob_price_clicked(double price) {
    order_entry_->set_current_price(price);
}

void CryptoTradingScreen::on_search_requested(const QString& filter) {
    QPointer<CryptoTradingScreen> self = this;
    QString filter_copy = filter;
    (void)QtConcurrent::run([self, filter_copy]() {
        if (!self)
            return;
        auto markets = ExchangeService::instance().fetch_markets("spot", filter_copy);
        QMetaObject::invokeMethod(
            self,
            [self, markets]() {
                if (!self)
                    return;
                self->watchlist_->set_search_results(markets);
            },
            Qt::QueuedConnection);
    });
}

// ============================================================================
// Refresh Functions
// ============================================================================

// ============================================================================
// WS Update Coalescing — flush accumulated data to UI at 10fps
// ============================================================================

} // namespace fincept::screens
