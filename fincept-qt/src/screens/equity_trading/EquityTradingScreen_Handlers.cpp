// src/screens/equity_trading/EquityTradingScreen_Handlers.cpp
//
// User-action handlers: account switch / symbol selection / mode toggle,
// token-expiry recovery, accounts dialog, order submit / cancel / modify,
// order-book click, refresh_candles.
//
// Part of the partial-class split of EquityTradingScreen.cpp.

#include "screens/equity_trading/EquityTradingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "screens/equity_trading/AccountManagementDialog.h"
#include "screens/equity_trading/BroadcastOrderDialog.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChartPanel.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "screens/equity_trading/OrderConfirmDialog.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStringListModel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <memory>

#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::equity;

static const QString TAG = "EquityTrading";


void EquityTradingScreen::on_account_changed(const QString& account_id) {
    if (account_id == focused_account_id_)
        return;

    focused_account_id_ = account_id;
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return;

    account_btn_->setText(account.display_name.toUpper());

    // Configure UI from broker profile
    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (broker) {
        const auto prof = broker->profile();

        // Watchlist symbols come from the active named list (global), not the
        // broker default — see load_watchlists() below. Only the default
        // selected symbol/exchange follow the broker.
        selected_symbol_ = prof.default_symbol;
        selected_exchange_ = prof.default_exchange;

        order_entry_->configure_for_broker(prof);
        order_entry_->set_broker_id(account.broker_id);
        watchlist_->set_broker_id(account.broker_id);
    }

    // Update mode button to reflect this account's trading mode
    const bool is_live = (account.trading_mode == "live");
    mode_btn_->setChecked(is_live);
    mode_btn_->setText(is_live ? tr("LIVE") : tr("PAPER"));
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);

    exchange_label_->setText(selected_exchange_);
    symbol_input_->setText(selected_symbol_);
    // Load named watchlists (seeds a default from the broker on first run) and
    // populate watchlist_symbols_ from the active list before the stream wiring
    // below subscribes to it.
    load_watchlists();
    ticker_bar_->set_symbol(selected_symbol_);
    ticker_bar_->set_currency(currency_symbol(exchange_currency(selected_exchange_)));
    order_entry_->set_symbol(selected_symbol_);
    order_entry_->set_exchange(selected_exchange_);

    // Resubscribe hub topics for new focused account
    if (isVisible())
        hub_subscribe_streaming();

    // Ensure stream is running and configure it. Skip data fetches when the
    // account has no credentials yet — every fetch_* spawns a QtConcurrent
    // thread that bails on an empty api_key check inside the lambda. Firing
    // ~11 of those at once trips _pthread_create on macOS and crashes the
    // process (see crash report 2026-05-08-123609.ips). The credentials_saved
    // handler will re-run the data-fetch path once the user connects.
    const bool has_creds = !AccountManager::instance().load_credentials(account_id).api_key.isEmpty();
    auto& dsm = DataStreamManager::instance();
    if (has_creds) {
        // Load the broker instrument master (numeric securityId map) so quotes,
        // charts and depth resolve. Loads from the SQLite cache when present,
        // else downloads. on_instruments_ready() reloads market data when done.
        ensure_instruments_loaded(account_id);
        dsm.start_stream(account_id);
        auto* stream = dsm.stream_for(account_id);
        if (stream) {
            stream->set_selected_symbol(selected_symbol_, selected_exchange_);
            stream->subscribe_symbols(watchlist_symbols_);
            stream->fetch_candles(selected_symbol_, chart_->current_timeframe());
            stream->fetch_orderbook(selected_symbol_);
            stream->fetch_time_sales(selected_symbol_);
            stream->fetch_calendar();
            stream->fetch_clock();
        }
    }

    // Load paper portfolio orders into matcher
    if (!account.paper_portfolio_id.isEmpty() && account.trading_mode == "paper") {
        auto pending = pt_get_orders(account.paper_portfolio_id, "pending");
        for (const auto& o : pending) {
            if (o.order_type != "market")
                OrderMatcher::instance().add_order(o);
        }
        // Refresh paper data immediately (positions/orders/trades/stats/funds).
        refresh_paper_panels();
    }

    update_account_menu();
    update_connection_status();
    LOG_INFO(TAG, QString("Switched to account: %1 (%2)").arg(account.display_name, account.broker_id));
}

void EquityTradingScreen::on_symbol_selected(const QString& symbol) {
    if (symbol.isEmpty() || symbol == selected_symbol_)
        return;
    switch_symbol(symbol);
}

void EquityTradingScreen::switch_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    current_price_ = 0.0; // unknown until the new symbol's first quote arrives
    symbol_input_->setText(symbol);
    ticker_bar_->set_symbol(symbol);
    order_entry_->set_symbol(symbol);
    watchlist_->set_active_symbol(symbol);
    update_chart_position(); // show/clear the new symbol's position on the chart

    // Resubscribe quote topics so the new symbol gets a hub subscription
    if (isVisible() && hub_active_)
        hub_subscribe_streaming();

    // Publish to the linked group so other panels (Watchlist, EquityResearch,
    // News, Derivatives, SurfaceAnalytics) follow. `this` as source suppresses
    // the echo back into this screen.
    if (link_group_ != SymbolGroup::None) {
        SymbolContext::instance().set_group_symbol(
            link_group_, SymbolRef::equity(symbol, selected_exchange_), this);
    }

    auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
    if (stream) {
        stream->set_selected_symbol(symbol, selected_exchange_);
        stream->fetch_candles(symbol, chart_->current_timeframe());
        stream->fetch_orderbook(symbol);

        auto account = AccountManager::instance().get_account(focused_account_id_);
        if (account.trading_mode == "live") {
            stream->fetch_time_sales(symbol);
        }
    }
}

QStringList EquityTradingScreen::search_broker_ids() const {
    // Focused broker first (its matches sort to the top of unified search), then
    // every other connected broker so a symbol only one carries is still found.
    QStringList ids;
    const QString bid = broker_id_for_focused();
    if (!bid.isEmpty())
        ids << bid;
    for (const auto& a : AccountManager::instance().active_accounts())
        if (!a.broker_id.isEmpty() && !ids.contains(a.broker_id))
            ids << a.broker_id;
    return ids;
}

void EquityTradingScreen::on_symbol_search_changed(const QString& text) {
    const QString query = text.trimmed().toUpper();
    if (query.length() < 2)
        return; // wait for a meaningful prefix before hitting the catalog

    const QStringList ids = search_broker_ids();
    if (ids.isEmpty())
        return;

    // search_all queries the SQLite instrument catalog directly (no in-memory
    // gate), so suggestions surface as soon as a connected broker's master is on
    // disk — even before the in-memory cache rebuild lands. One row per
    // (broker, instrument); symbol stays the leading token so prefix completion
    // on the typed text keeps working, with exchange + broker as context tags.
    const auto results = InstrumentService::instance().search_all(query, "", ids, 24);
    QStringList suggestions;
    suggestions.reserve(results.size());
    for (const auto& inst : results) {
        auto* b = BrokerRegistry::instance().get(inst.broker_id);
        const QString broker_name = b ? b->profile().display_name : inst.broker_id;
        suggestions.append(QString("%1  ·  %2  ·  %3").arg(inst.symbol, inst.exchange, broker_name));
    }
    symbol_completer_model_->setStringList(suggestions);
    // Re-evaluate the popup against the just-updated model; otherwise the
    // completer filters this keystroke against the previous (stale/empty) model
    // and shows nothing.
    if (!suggestions.isEmpty())
        symbol_completer_->complete();
}

void EquityTradingScreen::apply_symbol_input(const QString& raw) {
    // A chosen suggestion is "SYMBOL  ·  EXCH  ·  Broker" — keep only the leading
    // symbol token. Also tolerate the legacy "EXCHANGE:SYMBOL" form and bare input.
    QString sym = raw.trimmed().section(QStringLiteral("  ·  "), 0, 0).trimmed().toUpper();
    if (sym.contains(':'))
        sym = sym.section(':', 1);
    if (sym.isEmpty())
        return;
    on_symbol_selected(sym);
}

void EquityTradingScreen::on_mode_toggled() {
    if (focused_account_id_.isEmpty())
        return;

    const bool is_live = mode_btn_->isChecked();
    mode_btn_->setText(is_live ? tr("LIVE") : tr("PAPER"));
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);

    // Update per-account trading mode
    AccountManager::instance().set_trading_mode(focused_account_id_, is_live ? "live" : "paper");

    // Re-point hub subscriptions at the new mode: paper drops the live broker
    // positions/holdings/orders/balance topics so live data can't leak in.
    if (isVisible())
        hub_subscribe_streaming();

    if (!is_live) {
        refresh_paper_panels(); // positions/orders/trades/stats/funds from the paper engine
    }
    // Live mode data comes automatically from the running AccountDataStream
}

void EquityTradingScreen::handle_token_expired(const QString& account_id) {
    bool expected = false;
    if (!token_expired_shown_.compare_exchange_strong(expected, true))
        return;

    conn_label_->setText(tr("\xe2\x9a\xa0 TOKEN EXPIRED — click ACCOUNTS"));
    conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::NEGATIVE()));
    LOG_WARN(TAG, QString("Access token expired for account %1 — user must re-authenticate").arg(account_id));
    update_connection_status();
}

void EquityTradingScreen::on_accounts_clicked() {
    auto* dlg = new AccountManagementDialog(this);

    connect(dlg, &AccountManagementDialog::account_added, this, [this](const QString& account_id) {
        Q_UNUSED(account_id);
        update_account_menu();
        // Don't auto-focus a freshly-added account: it has no credentials yet,
        // so on_account_changed() would spawn ~11 QtConcurrent::run fetches that
        // all bail at the empty-api_key check. On macOS that thread-storm trips
        // pthread_create inside QThreadPool and crashes the main thread (see
        // crash report 2026-05-08-123609.ips). Defer focus to credentials_saved.
    });

    connect(dlg, &AccountManagementDialog::account_removed, this, [this](const QString& account_id) {
        DataStreamManager::instance().stop_stream(account_id);
        update_account_menu();
        // If removed the focused account, switch to next available
        if (account_id == focused_account_id_) {
            auto accounts = AccountManager::instance().active_accounts();
            if (!accounts.isEmpty())
                on_account_changed(accounts.first().account_id);
            else
                focused_account_id_.clear();
        }
        update_connection_status();
    });

    connect(dlg, &AccountManagementDialog::credentials_saved, this, [this](const QString& account_id) {
        token_expired_shown_.store(false);
        // Broker just connected — load its instrument master so market data
        // (watchlist quotes, charts, depth) can resolve numeric securityIds.
        // Covers every broker: all connect paths emit credentials_saved.
        ensure_instruments_loaded(account_id);
        // First account ever connected: focus it now (account_added intentionally
        // does not auto-focus — see comment there). on_account_changed() does its
        // own start_stream() + symbol/watchlist setup, so we're done after that.
        if (focused_account_id_.isEmpty()) {
            on_account_changed(account_id);
            AccountManager::instance().set_connection_state(account_id, ConnectionState::Connected);
            update_connection_status();
            return;
        }
        // Start/restart the stream with new credentials
        DataStreamManager::instance().start_stream(account_id);
        auto* stream = DataStreamManager::instance().stream_for(account_id);
        if (stream && account_id == focused_account_id_) {
            stream->set_selected_symbol(selected_symbol_, selected_exchange_);
            stream->subscribe_symbols(watchlist_symbols_);
        }
        AccountManager::instance().set_connection_state(account_id, ConnectionState::Connected);
        update_connection_status();
    });

    dlg->exec();
    dlg->deleteLater();
}

// ============================================================================
// Order Handling
// ============================================================================

void EquityTradingScreen::on_order_submitted(const UnifiedOrder& order) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }

    auto account = AccountManager::instance().get_account(focused_account_id_);

    if (account.trading_mode == "paper") {
        const QString portfolio_id = account.paper_portfolio_id;
        if (portfolio_id.isEmpty()) {
            order_entry_->show_order_status(tr("No paper portfolio for this account"), false);
            return;
        }

        LOG_INFO(TAG, QString("Paper order: %1 %2 x%3 account=%4")
                          .arg(order.side == OrderSide::Buy ? "BUY" : "SELL")
                          .arg(order.symbol)
                          .arg(order.quantity)
                          .arg(account.display_name));

        const QString side = order.side == OrderSide::Buy ? "buy" : "sell";
        const QString type = order_type_str(order.order_type);
        std::optional<double> price_opt;
        if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
            price_opt = order.price;
        } else if (order.order_type == OrderType::Market) {
            const double ref = current_price_ > 0 ? current_price_ : (order.price > 0 ? order.price : 0.0);
            if (ref > 0)
                price_opt = ref;
        }
        std::optional<double> stop_opt;
        if (order.stop_price > 0)
            stop_opt = order.stop_price;

        if (order.order_type == OrderType::Market && !price_opt) {
            order_entry_->show_order_status(tr("Price not available yet — wait for quotes to load"), false);
            return;
        }

        try {
            auto pt_order = pt_place_order(portfolio_id, order.symbol, side, type, order.quantity, price_opt, stop_opt);

            if (order.order_type == OrderType::Market) {
                double fill_price = current_price_ > 0 ? current_price_ : (order.price > 0 ? order.price : 0.0);
                if (fill_price <= 0) {
                    order_entry_->show_order_status(tr("No price available for fill"), false);
                    return;
                }
                pt_fill_order(pt_order.id, fill_price);
                order_entry_->show_order_status(
                    tr("Paper order filled: %1 @ %2").arg(order.symbol).arg(fill_price, 0, 'f', 2), true);
            } else {
                OrderMatcher::instance().add_order(pt_order);
                order_entry_->show_order_status(tr("Paper order queued: %1").arg(order.symbol), true);
            }
        } catch (const std::exception& e) {
            order_entry_->show_order_status(tr("Order failed: %1").arg(e.what()), false);
            return;
        }

        // Refresh paper portfolio
        try {
            auto portfolio = pt_get_portfolio(portfolio_id);
            auto positions = pt_get_positions(portfolio_id);
            auto orders_list = pt_get_orders(portfolio_id);
            auto trades = pt_get_trades(portfolio_id, 50);
            auto stats = pt_get_stats(portfolio_id);
            order_entry_->set_balance(portfolio.balance);
            bottom_panel_->set_paper_positions(positions);
            bottom_panel_->set_paper_orders(orders_list);
            bottom_panel_->set_paper_trades(trades);
            bottom_panel_->set_paper_stats(stats);
        } catch (...) {}
    } else {
        // Route to live broker via account-aware UnifiedTrading
        // Safety: capture account_id by value at click time (immutable per order lifecycle)
        const QString acct_id = focused_account_id_;

        // Semi-Auto pre-gate: confirm inline before sending. Manual orders are
        // synchronous (the trader is present), so approval is an inline modal —
        // no pending-queue round-trip. On confirm we place immediately, exactly
        // like Auto mode. (Headless paths — AI/workflow/broadcast — still queue
        // via ActionCenter and surface in the status-bar popover.)
        if (ActionCenter::instance().should_queue(acct_id, "placeorder")) {
            const auto acct_meta = AccountManager::instance().get_account(acct_id);
            const QString acct_label =
                acct_meta.display_name.isEmpty() ? acct_id : acct_meta.display_name;
            if (!OrderConfirmDialog::confirm(this, order, acct_label, current_price_)) {
                order_entry_->show_order_status(tr("Order cancelled"), false);
                return;
            }
        }

        QPointer<EquityTradingScreen> self = this;
        auto order_copy = order;
        (void)QtConcurrent::run([self, acct_id, order_copy]() {
            if (!self)
                return;
            auto result = UnifiedTrading::instance().place_order(acct_id, order_copy);
            QMetaObject::invokeMethod(
                self,
                [self, result]() {
                    if (!self)
                        return;
                    if (result.success) {
                        LOG_INFO(TAG, QString("Order placed: %1").arg(result.order_id));
                        self->order_entry_->show_order_status(self->tr("Order placed: %1").arg(result.order_id), true);
                    } else {
                        LOG_ERROR(TAG, QString("Order failed: %1").arg(result.message));
                        self->order_entry_->show_order_status(result.message, false);
                    }
                },
                Qt::QueuedConnection);
        });
    }
}

void EquityTradingScreen::on_cancel_order(const QString& order_id) {
    auto account = AccountManager::instance().get_account(focused_account_id_);

    if (account.trading_mode == "paper") {
        try {
            pt_cancel_order(order_id);
            OrderMatcher::instance().remove_order(order_id);
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Paper cancel failed: %1 order_id=%2").arg(e.what(), order_id));
        }
        // Refresh paper data
        if (!account.paper_portfolio_id.isEmpty()) {
            try {
                auto portfolio = pt_get_portfolio(account.paper_portfolio_id);
                auto positions = pt_get_positions(account.paper_portfolio_id);
                auto orders = pt_get_orders(account.paper_portfolio_id);
                auto trades = pt_get_trades(account.paper_portfolio_id, 50);
                auto stats = pt_get_stats(account.paper_portfolio_id);
                order_entry_->set_balance(portfolio.balance);
                bottom_panel_->set_paper_positions(positions);
                bottom_panel_->set_paper_orders(orders);
                bottom_panel_->set_paper_trades(trades);
                bottom_panel_->set_paper_stats(stats);
            } catch (...) {}
        }
    } else {
        const QString acct_id = focused_account_id_;
        QPointer<EquityTradingScreen> self = this;
        (void)QtConcurrent::run([self, acct_id, order_id]() {
            if (!self)
                return;
            UnifiedTrading::instance().cancel_order(acct_id, order_id);
        });
    }
}

void EquityTradingScreen::on_ob_price_clicked(double price) {
    order_entry_->set_current_price(price);
}

void EquityTradingScreen::refresh_candles() {
    auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
    if (stream)
        stream->fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void EquityTradingScreen::async_modify_order(const QString& order_id, double qty, double price) {
    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run([self, acct_id, order_id, qty, price]() {
        if (!self)
            return;
        QJsonObject mods;
        if (qty > 0)
            mods["qty"] = qty;
        if (price > 0)
            mods["price"] = price;
        auto result = UnifiedTrading::instance().modify_order(acct_id, order_id, mods);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                if (result.success) {
                    LOG_INFO(TAG, QString("Order modified: %1").arg(result.order_id));
                } else {
                    LOG_WARN(TAG, QString("Order modify failed: %1").arg(result.message));
                }
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::refresh_paper_panels() {
    auto account = AccountManager::instance().get_account(focused_account_id_);
    if (account.trading_mode != "paper" || account.paper_portfolio_id.isEmpty())
        return; // live data flows from AccountDataStream via the hub
    try {
        auto portfolio = pt_get_portfolio(account.paper_portfolio_id);
        auto positions = pt_get_positions(account.paper_portfolio_id);
        auto orders = pt_get_orders(account.paper_portfolio_id);
        auto trades = pt_get_trades(account.paper_portfolio_id, 50);
        auto stats = pt_get_stats(account.paper_portfolio_id);
        order_entry_->set_balance(portfolio.balance);
        bottom_panel_->set_paper_positions(positions);
        bottom_panel_->set_paper_orders(orders);
        bottom_panel_->set_paper_trades(trades);
        bottom_panel_->set_paper_stats(stats);
        // Paper Funds tab: live set_funds is gated off in paper, so surface a
        // broker-equivalent funds view computed from the paper portfolio.
        // equity = free cash + open P&L; used margin = notional / leverage.
        double unrealized = 0.0, used_margin = 0.0;
        for (const auto& p : positions) {
            unrealized += p.unrealized_pnl;
            const double lev = p.leverage > 0.0 ? p.leverage : 1.0;
            used_margin += (qAbs(p.quantity) * p.current_price) / lev;
        }
        trading::BrokerFunds funds;
        funds.available_balance = portfolio.balance;          // free cash
        funds.used_margin = used_margin;                      // locked in open positions
        funds.total_balance = portfolio.balance + unrealized; // equity
        bottom_panel_->set_funds(funds);
        // Held symbols get live WebSocket prices even if not in the active list.
        QStringList pos_syms;
        for (const auto& p : positions)
            pos_syms << p.symbol;
        update_position_symbols(pos_syms);
        update_chart_position(); // refresh the on-chart position card
    } catch (...) {
        LOG_WARN(TAG, "Exception refreshing paper panels");
    }
}

void EquityTradingScreen::on_chart_exit_position(const QString& symbol, const QString& exchange,
                                                 const QString& product_type, const QString& side,
                                                 double qty) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    const QString side_disp =
        side.compare("short", Qt::CaseInsensitive) == 0 ? tr("SHORT") : tr("LONG");
    const auto reply = QMessageBox::question(
        this, tr("Exit Position"),
        tr("Exit %1 %2 %3 at market?").arg(side_disp).arg(qty, 0, 'f', 0).arg(symbol),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run([self, acct_id, symbol, exchange, product_type]() {
        if (!self)
            return;
        auto result =
            UnifiedTrading::instance().close_position(acct_id, symbol, exchange, product_type);
        QMetaObject::invokeMethod(
            self,
            [self, result, symbol]() {
                if (!self)
                    return;
                self->order_entry_->show_order_status(
                    result.success ? self->tr("Exit order placed: %1").arg(symbol) : result.error,
                    result.success);
                self->refresh_paper_panels(); // paper refreshes here; live flows via the hub
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_cancel_all_orders() {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run([self, acct_id]() {
        if (!self)
            return;
        auto result = UnifiedTrading::instance().cancel_all_orders(acct_id);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                if (result.success && result.data) {
                    const auto& r = *result.data;
                    self->order_entry_->show_order_status(
                        self->tr("Cancelled %1 order(s)%2")
                            .arg(r.canceled_order_ids.size())
                            .arg(r.failed.isEmpty() ? QString() : self->tr(", %1 failed").arg(r.failed.size())),
                        r.failed.isEmpty());
                } else {
                    self->order_entry_->show_order_status(result.error, false);
                }
                self->refresh_paper_panels();
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_close_all_positions() {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run([self, acct_id]() {
        if (!self)
            return;
        auto result = UnifiedTrading::instance().close_all_positions(acct_id);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                if (result.success && result.data) {
                    const auto& r = *result.data;
                    self->order_entry_->show_order_status(
                        self->tr("Closed %1 position(s)%2")
                            .arg(r.closed_symbols.size())
                            .arg(r.failed.isEmpty() ? QString() : self->tr(", %1 failed").arg(r.failed.size())),
                        r.failed.isEmpty());
                } else {
                    self->order_entry_->show_order_status(result.error, false);
                }
                self->refresh_paper_panels();
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_strategy_submitted(const trading::BasketOrderRequest& basket) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    if (basket.orders.isEmpty()) {
        order_entry_->show_order_status(tr("Strategy has no legs to place"), false);
        return;
    }
    const QString acct_id = focused_account_id_;

    // Semi-Auto gate (headless entry): a strategy/basket is placed in one shot
    // with no per-leg prompt, so queue it for approval in the status-bar popover
    // rather than confirming inline. Auto mode falls straight through.
    if (ActionCenter::instance().should_queue(acct_id, "basketorder")) {
        const QString pending_id = ActionCenter::instance().queue_order(
            acct_id, "basketorder", ActionCenter::serialize_basket_order(basket));
        order_entry_->show_order_status(
            pending_id.isEmpty()
                ? tr("Failed to queue strategy")
                : tr("Strategy queued for approval (%1 legs)").arg(basket.orders.size()),
            !pending_id.isEmpty());
        return;
    }

    QPointer<EquityTradingScreen> self = this;
    // place_basket_orders runs async on its own worker and delivers the result
    // back on the caller's thread; marshal to the UI thread defensively.
    UnifiedTrading::instance().place_basket_orders(
        acct_id, basket, [self](const trading::BasketOrderResult& result) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, result]() {
                    if (!self)
                        return;
                    self->order_entry_->show_order_status(
                        self->tr("Strategy: %1/%2 legs placed%3")
                            .arg(result.successful)
                            .arg(result.total)
                            .arg(result.failed > 0 ? self->tr(", %1 failed").arg(result.failed) : QString()),
                        result.failed == 0);
                    self->refresh_paper_panels();
                },
                Qt::QueuedConnection);
        });
}

// ============================================================================
// Import holdings -> Portfolio
// ============================================================================

// Broker-exchange → yfinance suffix. Mirrors PortfolioDialogs.cpp so imported
// symbols match the format the portfolio screen's price engine (yfinance) expects.
} // namespace fincept::screens
