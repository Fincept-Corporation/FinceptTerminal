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
#include "screens/equity_trading/EquityChart.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
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

        // Update defaults if switching broker types
        watchlist_symbols_ = QStringList(prof.default_watchlist.begin(), prof.default_watchlist.end());
        selected_symbol_ = prof.default_symbol;
        selected_exchange_ = prof.default_exchange;

        order_entry_->configure_for_broker(prof);
        order_entry_->set_broker_id(account.broker_id);
        watchlist_->set_broker_id(account.broker_id);
    }

    // Update mode button to reflect this account's trading mode
    const bool is_live = (account.trading_mode == "live");
    mode_btn_->setChecked(is_live);
    mode_btn_->setText(is_live ? "LIVE" : "PAPER");
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);

    exchange_label_->setText(selected_exchange_);
    symbol_input_->setText(selected_symbol_);
    watchlist_->set_symbols(watchlist_symbols_);
    ticker_bar_->set_symbol(selected_symbol_);
    ticker_bar_->set_currency(currency_symbol(exchange_currency(selected_exchange_)));
    order_entry_->set_symbol(selected_symbol_);
    order_entry_->set_exchange(selected_exchange_);

    // Ensure stream is running and configure it. Skip data fetches when the
    // account has no credentials yet — every fetch_* spawns a QtConcurrent
    // thread that bails on an empty api_key check inside the lambda. Firing
    // ~11 of those at once trips _pthread_create on macOS and crashes the
    // process (see crash report 2026-05-08-123609.ips). The credentials_saved
    // handler will re-run the data-fetch path once the user connects.
    const bool has_creds = !AccountManager::instance().load_credentials(account_id).api_key.isEmpty();
    auto& dsm = DataStreamManager::instance();
    if (has_creds) {
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
        // Refresh paper data immediately
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
        } catch (...) {
            LOG_WARN(TAG, "Exception loading paper portfolio on account switch");
        }
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
    symbol_input_->setText(symbol);
    ticker_bar_->set_symbol(symbol);
    order_entry_->set_symbol(symbol);
    watchlist_->set_active_symbol(symbol);

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

void EquityTradingScreen::on_mode_toggled() {
    if (focused_account_id_.isEmpty())
        return;

    const bool is_live = mode_btn_->isChecked();
    mode_btn_->setText(is_live ? "LIVE" : "PAPER");
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);

    // Update per-account trading mode
    AccountManager::instance().set_trading_mode(focused_account_id_, is_live ? "live" : "paper");

    if (!is_live) {
        // Refresh paper data for this account
        auto account = AccountManager::instance().get_account(focused_account_id_);
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
            } catch (...) {
                LOG_WARN(TAG, "Exception refreshing paper portfolio on mode switch");
            }
        }
    }
    // Live mode data comes automatically from the running AccountDataStream
}

void EquityTradingScreen::handle_token_expired(const QString& account_id) {
    bool expected = false;
    if (!token_expired_shown_.compare_exchange_strong(expected, true))
        return;

    conn_label_->setText(QString::fromUtf8("\xe2\x9a\xa0 TOKEN EXPIRED — click ACCOUNTS"));
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
        order_entry_->show_order_status("No account selected — add one via ACCOUNTS", false);
        return;
    }

    auto account = AccountManager::instance().get_account(focused_account_id_);

    if (account.trading_mode == "paper") {
        const QString portfolio_id = account.paper_portfolio_id;
        if (portfolio_id.isEmpty()) {
            order_entry_->show_order_status("No paper portfolio for this account", false);
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
            order_entry_->show_order_status("Price not available yet — wait for quotes to load", false);
            return;
        }

        try {
            auto pt_order = pt_place_order(portfolio_id, order.symbol, side, type, order.quantity, price_opt, stop_opt);

            if (order.order_type == OrderType::Market) {
                double fill_price = current_price_ > 0 ? current_price_ : (order.price > 0 ? order.price : 0.0);
                if (fill_price <= 0) {
                    order_entry_->show_order_status("No price available for fill", false);
                    return;
                }
                pt_fill_order(pt_order.id, fill_price);
                order_entry_->show_order_status(
                    QString("Paper order filled: %1 @ %2").arg(order.symbol).arg(fill_price, 0, 'f', 2), true);
            } else {
                OrderMatcher::instance().add_order(pt_order);
                order_entry_->show_order_status(QString("Paper order queued: %1").arg(order.symbol), true);
            }
        } catch (const std::exception& e) {
            order_entry_->show_order_status(QString("Order failed: %1").arg(e.what()), false);
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
                        self->order_entry_->show_order_status(QString("Order placed: %1").arg(result.order_id), true);
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

// ============================================================================
// Import holdings -> Portfolio
// ============================================================================

// Broker-exchange → yfinance suffix. Mirrors PortfolioDialogs.cpp so imported
// symbols match the format the portfolio screen's price engine (yfinance) expects.
} // namespace fincept::screens
