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
#include "trading/instruments/InstrumentNormalize.h"
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
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
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

    // Wire the positions/orders header buttons (square-off winners/losers,
    // SQUARE OFF ALL, cancel-all-orders) to this account. Without this the panel's
    // account_id_ stays empty and every one of those buttons silently no-ops.
    bottom_panel_->set_account_id(account_id);

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

        // Time & Sales / Auctions / Calendar are US-broker-only market data —
        // show those tabs and run their fetches only for US brokers (Alpaca etc.).
        focused_is_us_market_ = (prof.region == QLatin1String("US"));
        bottom_panel_->set_us_market_tabs_visible(focused_is_us_market_);
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
    // If the stream is ALREADY running, start_stream() below is a no-op and does
    // NOT re-fetch the portfolio. A fresh start() fetches on its own, so we only
    // force a refresh in the already-running case — never stack a second fetch
    // batch on a fresh start (firing too many async fetches at once has crashed
    // macOS; see the credentials note below).
    const bool stream_existed = dsm.has_stream(account_id);
    if (has_creds) {
        // Load the broker instrument master (numeric securityId map) so quotes,
        // charts and depth resolve. Loads from the SQLite cache when present,
        // else downloads. on_instruments_ready() reloads market data when done.
        ensure_instruments_loaded(account_id);
        dsm.start_stream(account_id);
        auto* stream = dsm.stream_for(account_id);
        if (stream) {
            stream->set_selected_symbol(selected_symbol_, selected_exchange_);
            stream->subscribe_symbols(QStringLiteral("equity:watchlist"), effective_symbols());
            stream->fetch_candles(selected_symbol_, chart_->current_timeframe());
            stream->fetch_orderbook(selected_symbol_);
            // US-only market data — skip for Indian/other brokers (no tape/calendar).
            if (focused_is_us_market_) {
                stream->fetch_time_sales(selected_symbol_);
                stream->fetch_calendar();
                stream->fetch_clock();
            }
        }
    }

    // Switching accounts cleared the shared blotter (EquityBottomPanel::
    // set_account_id). For a live account whose stream was already running, force
    // an immediate portfolio refetch so it refills with THIS account's positions/
    // holdings/orders right away instead of staying blank until the 5-min poll.
    if (has_creds && is_live && stream_existed)
        dsm.refresh_portfolio(account_id);

    // Each account's order book opens on today's session.
    orders_view_day_ = QDate::currentDate();

    // Load paper portfolio orders into matcher
    if (!account.paper_portfolio_id.isEmpty() && account.trading_mode == "paper") {
        auto pending = pt_get_orders(account.paper_portfolio_id, "pending");
        for (const auto& o : pending) {
            if (o.order_type != "market")
                OrderMatcher::instance().add_order(o);
        }
        // Refresh paper data immediately (positions/holdings/orders/funds/stats).
        refresh_paper_panels();
    }

    update_account_menu();
    update_connection_status();
    LOG_INFO(TAG, QString("Switched to account: %1 (%2)").arg(account.display_name, account.broker_id));
}

void EquityTradingScreen::on_symbol_selected(const QString& symbol) {
    LOG_INFO("posdbg", QString("on_symbol_selected sym='%1' (current='%2' exch='%3')")
                           .arg(symbol, selected_symbol_, selected_exchange_));
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
        if (account.trading_mode == "live" && focused_is_us_market_) {
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
    symbol_suggestion_map_.clear();
    for (const auto& inst : results) {
        // Show a clean human label (e.g. "NIFTY 7 Jul 26 18250 CE  ·  NFO") instead
        // of the raw canonical symbol, and remember the real symbol for selection.
        const QString friendly =
            norm::display_name(inst.name, inst.instrument_type, inst.expiry, inst.strike, inst.symbol);
        const QString label = QStringLiteral("%1  ·  %2").arg(friendly, inst.exchange);
        if (symbol_suggestion_map_.contains(label))
            continue; // collapse identical labels surfaced by multiple brokers
        symbol_suggestion_map_.insert(label, inst.symbol);
        suggestions.append(label);
    }
    symbol_completer_model_->setStringList(suggestions);
    // Re-evaluate the popup against the just-updated model; otherwise the
    // completer filters this keystroke against the previous (stale/empty) model
    // and shows nothing.
    if (!suggestions.isEmpty())
        symbol_completer_->complete();
}

void EquityTradingScreen::apply_symbol_input(const QString& raw) {
    const QString trimmed = raw.trimmed();
    // A picked suggestion maps its friendly label back to the real symbol.
    QString sym = symbol_suggestion_map_.value(trimmed);
    if (sym.isEmpty()) {
        // Typed text or legacy "SYMBOL  ·  EXCH" / "EXCHANGE:SYMBOL" forms.
        sym = trimmed.section(QStringLiteral("  ·  "), 0, 0).trimmed().toUpper();
        if (sym.contains(':'))
            sym = sym.section(':', 1);
    }
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
    } else {
        // Switching to live: set_mode() just cleared the shared blotter and the
        // stream is already running (so it won't re-fetch on its own). Force an
        // immediate refresh so live positions/holdings/orders appear now instead
        // of leaving the blotter blank until the 5-min poll.
        DataStreamManager::instance().refresh_portfolio(focused_account_id_);
    }
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
        // If removed the focused account, switch to the next configured broker.
        if (account_id == focused_account_id_) {
            focused_account_id_.clear();
            ensure_account_loaded(); // credential-aware next pick
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
            stream->subscribe_symbols(QStringLiteral("equity:watchlist"), effective_symbols());
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

        // Forward the exchange + broker product (MIS/CNC/NRML) so the engine
        // applies per-product leverage, tags the position, and routes CNC to the
        // Holdings tab. Without this the product was silently dropped (everything
        // looked like blank-product CNC) and MIS could never be told apart.
        const QString product = product_to_broker_str(order.product_type);
        try {
            auto pt_order = pt_place_order(portfolio_id, order.symbol, side, type, order.quantity, price_opt,
                                           stop_opt, /*reduce_only=*/false, order.exchange, product);

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

        // Refresh paper portfolio (positions/holdings/orders/funds/stats).
        refresh_paper_panels();
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

// Inline multi-broker submit (BROKERS selector in the order panel). One explicit
// confirmation listing every target account + its mode, then the same per-account
// Semi-Auto gating and background broadcast the ALL dialog uses.
void EquityTradingScreen::on_multi_broker_submit(const trading::UnifiedOrder& order,
                                                 const QStringList& account_ids) {
    if (account_ids.isEmpty())
        return;

    QStringList lines;
    for (const QString& id : account_ids) {
        const auto account = AccountManager::instance().get_account(id);
        if (account.account_id.isEmpty())
            continue;
        const QString mode_tag = account.trading_mode == "live" ? tr("LIVE") : tr("PAPER");
        lines << QString("• %1  [%2]").arg(account.display_name, mode_tag);
    }
    if (lines.isEmpty()) {
        order_entry_->show_order_status(tr("Selected accounts no longer exist"), false);
        return;
    }

    const QString side = order.side == trading::OrderSide::Buy ? tr("BUY") : tr("SELL");
    const QString px = order.order_type == trading::OrderType::Market
                           ? tr("MARKET")
                           : QString::number(order.price, 'f', 2);
    const auto ret = QMessageBox::question(
        this, tr("Multi-Broker Order"),
        tr("%1 %2 × %3 @ %4 on %5 account(s):\n\n%6")
            .arg(side, QString::number(order.quantity), order.symbol, px)
            .arg(lines.size())
            .arg(lines.join("\n")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    // Per-account Semi-Auto gate: queue those accounts for approval, broadcast
    // the rest immediately (mirrors BroadcastOrderDialog::on_place_order).
    QStringList immediate;
    int queued = 0;
    for (const QString& acct : account_ids) {
        if (ActionCenter::instance().should_queue(acct, "placeorder")) {
            const QString pid = ActionCenter::instance().queue_order(
                acct, "placeorder", ActionCenter::serialize_unified_order(order));
            if (!pid.isEmpty())
                ++queued;
        } else {
            immediate.append(acct);
        }
    }
    if (immediate.isEmpty()) {
        order_entry_->show_order_status(tr("%1 order(s) queued for approval").arg(queued), true);
        return;
    }
    order_entry_->show_order_status(tr("Placing on %1 account(s)…").arg(immediate.size()), true);

    QPointer<EquityTradingScreen> self = this;
    auto order_copy = order;
    (void)QtConcurrent::run([self, immediate, order_copy, queued]() {
        const auto results = trading::UnifiedTrading::instance().broadcast_order(immediate, order_copy);
        QMetaObject::invokeMethod(
            self,
            [self, results, queued]() {
                if (!self)
                    return;
                int ok_n = 0;
                QStringList errors;
                for (const auto& r : results) {
                    if (r.response.success)
                        ++ok_n;
                    else
                        errors << QString("%1: %2").arg(r.display_name, r.response.message);
                }
                QString msg = tr("✓ %1 placed").arg(ok_n);
                if (queued > 0)
                    msg += tr(", %1 queued").arg(queued);
                if (!errors.isEmpty())
                    msg += tr(" — ✗ %1").arg(errors.join("; "));
                self->order_entry_->show_order_status(msg, errors.isEmpty());
                // Repaint each target's blotter right away (paper panels too).
                for (const auto& r : results)
                    trading::DataStreamManager::instance().refresh_portfolio(r.account_id);
                self->refresh_paper_panels();
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_chart_buy_requested(double price) {
    open_chart_order_ticket(true, price);
}

void EquityTradingScreen::on_chart_sell_requested(double price) {
    open_chart_order_ticket(false, price);
}

void EquityTradingScreen::on_chart_add_to_watchlist() {
    if (!selected_symbol_.isEmpty())
        on_watchlist_symbol_added(selected_symbol_);
}

// Kite-style chart trading: a compact confirm ticket (qty + limit price,
// prefilled at the clicked level), then route through the normal placement path
// so paper/live handling, balances and status all behave exactly like the order
// panel. For live accounts on_order_submitted() still applies its own pre-send
// gate; for paper this ticket is the single confirmation.
void EquityTradingScreen::open_chart_order_ticket(bool is_buy, double price) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    if (price <= 0)
        price = current_price_; // cursor price unavailable — fall back to last traded

    QDialog dlg(this);
    dlg.setWindowTitle(is_buy ? tr("Buy %1").arg(selected_symbol_) : tr("Sell %1").arg(selected_symbol_));
    auto* form = new QFormLayout(&dlg);

    auto* hdr = new QLabel(
        QStringLiteral("%1  %2 · %3").arg(is_buy ? tr("BUY") : tr("SELL"), selected_symbol_, selected_exchange_), &dlg);
    hdr->setStyleSheet(QString("font-weight:700;font-size:13px;color:%1;")
                           .arg(is_buy ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    form->addRow(hdr);

    auto* type_combo = new QComboBox(&dlg);
    type_combo->addItems({tr("Market"), tr("Limit")});
    type_combo->setCurrentIndex(0); // default to Market
    form->addRow(tr("Type"), type_combo);

    auto* qty_spin = new QSpinBox(&dlg);
    qty_spin->setRange(1, 10000000);
    qty_spin->setValue(1);
    form->addRow(tr("Qty"), qty_spin);

    auto* px_spin = new QDoubleSpinBox(&dlg);
    px_spin->setRange(0.01, 1.0e9);
    px_spin->setDecimals(2);
    px_spin->setValue(price > 0 ? price : 0.01);
    form->addRow(tr("Limit price"), px_spin);

    // A Market order needs no price (it fills at the LTP), so the limit-price
    // row (label + field) is hidden entirely unless Limit is selected.
    auto sync_type = [form, px_spin](int idx) { form->setRowVisible(px_spin, idx == 1); }; // 1 = Limit
    connect(type_combo, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, sync_type);
    sync_type(type_combo->currentIndex());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton* ok = buttons->addButton(is_buy ? tr("Buy") : tr("Sell"), QDialogButtonBox::AcceptRole);
    ok->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    UnifiedOrder order;
    order.symbol = selected_symbol_;
    order.exchange = selected_exchange_;
    order.side = is_buy ? OrderSide::Buy : OrderSide::Sell;
    const bool is_market = (type_combo->currentIndex() == 0);
    order.order_type = is_market ? OrderType::Market : OrderType::Limit;
    order.quantity = qty_spin->value();
    order.price = is_market ? 0.0 : px_spin->value();
    order.product_type = ProductType::Intraday;
    order.validity = QStringLiteral("DAY");
    on_order_submitted(order);
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
        refresh_paper_panels();
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

void EquityTradingScreen::flush_paper_prices() {
    paper_flush_armed_ = false;
    if (pending_paper_prices_.isEmpty() || focused_paper_portfolio_id_.isEmpty()) {
        pending_paper_prices_.clear();
        return;
    }
    // Persist each symbol's latest buffered LTP. Coalesced off the per-tick hot
    // path; recomputes the position's unrealized P&L in the same SQLite UPDATE.
    for (auto it = pending_paper_prices_.cbegin(); it != pending_paper_prices_.cend(); ++it)
        pt_update_position_price(focused_paper_portfolio_id_, it.key(), it.value());
    pending_paper_prices_.clear();
}

void EquityTradingScreen::refresh_paper_panels() {
    auto account = AccountManager::instance().get_account(focused_account_id_);
    // Keep the per-tick quote handler's cached context fresh — this also fires on
    // paper order events, where the portfolio id may have just been assigned.
    focused_is_paper_ = account.trading_mode == "paper";
    focused_paper_portfolio_id_ = focused_is_paper_ ? account.paper_portfolio_id : QString();
    if (account.trading_mode != "paper" || account.paper_portfolio_id.isEmpty())
        return; // live data flows from AccountDataStream via the hub
    // Persist any buffered tick prices first so the positions/funds snapshot below
    // reflects the latest LTP at this refresh/order moment, not a coalesce window ago.
    flush_paper_prices();
    try {
        const auto portfolio = pt_get_portfolio(account.paper_portfolio_id);
        const auto positions = pt_get_positions(account.paper_portfolio_id);
        const auto stats = pt_get_stats(account.paper_portfolio_id);
        const QString sym = currency_symbol(portfolio.currency.isEmpty() ? QStringLiteral("INR") : portfolio.currency);

        // Split exposure: CNC/delivery -> Holdings tab, MIS/NRML -> Positions tab.
        QVector<trading::PtPosition> intraday;
        QVector<trading::BrokerHolding> holdings;
        double used_margin = 0.0, mis_unrealized = 0.0;
        double holdings_value = 0.0, holdings_unreal = 0.0;
        QStringList pos_syms;
        for (const auto& p : positions) {
            pos_syms << p.symbol;
            if (product_is_delivery(p.product)) {
                trading::BrokerHolding h;
                h.symbol = p.symbol;
                h.quantity = p.quantity;
                h.avg_price = p.entry_price;
                h.ltp = p.current_price > 0.0 ? p.current_price : p.entry_price;
                h.invested_value = p.entry_price * p.quantity;
                h.current_value = h.ltp * p.quantity;
                // Sign P&L by side: a short position gains when price falls. Using the
                // long formula unconditionally inverted short-side P&L and corrupted
                // holdings_value/total_equity.
                h.pnl = (p.side.compare("short", Qt::CaseInsensitive) == 0)
                            ? (h.invested_value - h.current_value)
                            : (h.current_value - h.invested_value);
                h.pnl_pct = h.invested_value > 0.0 ? (h.pnl / h.invested_value) * 100.0 : 0.0;
                holdings.push_back(h);
                holdings_value += h.current_value;
                holdings_unreal += h.pnl;
            } else {
                intraday.push_back(p);
                used_margin += p.held_margin;
                mis_unrealized += p.unrealized_pnl;
            }
        }
        const double unrealized = mis_unrealized + holdings_unreal;

        order_entry_->set_balance(portfolio.balance);
        bottom_panel_->set_currency(sym); // before positions so the P&L summary formats in ₹
        bottom_panel_->set_paper_positions(intraday);
        bottom_panel_->set_holdings(holdings);

        // Order book: the day shown by the date selector (default today, so each
        // session starts empty). Keep the picker in sync with the viewed day.
        bottom_panel_->set_orders_date(orders_view_day_);
        bottom_panel_->set_paper_orders(pt_get_orders_for_day(account.paper_portfolio_id, orders_view_day_));

        // ── Funds card view ──
        EquityFundsView fv;
        fv.is_paper = true;
        fv.currency = sym;
        fv.available = portfolio.balance;
        fv.used_margin = used_margin;
        fv.holdings_value = holdings_value;
        fv.unrealized_pnl = unrealized;
        fv.total_equity = portfolio.balance + used_margin + holdings_value + mis_unrealized;
        fv.opening_balance = portfolio.initial_balance;
        fv.realized_pnl = stats.today_pnl; // booked today
        fv.collateral = 0.0;
        const double margin_denom = used_margin + portfolio.balance;
        fv.margin_util_pct = margin_denom > 0.0 ? (used_margin / margin_denom) * 100.0 : 0.0;
        bottom_panel_->set_funds_view(fv);

        // ── Stats card view ──
        EquityStatsView sv;
        sv.currency = sym;
        sv.realized_pnl = stats.total_pnl; // all-time realized (gross of fees)
        sv.unrealized_pnl = unrealized;
        // Net P&L is the bottom line: realized − fees + unrealized. This makes
        // net_pnl == (total_equity − opening_balance); previously it omitted fees,
        // so Net P&L / Return % read higher than the actual account gain.
        sv.net_pnl = stats.total_pnl - stats.total_fees + unrealized;
        sv.today_pnl = stats.today_pnl;
        sv.return_pct = portfolio.initial_balance > 0.0 ? (sv.net_pnl / portfolio.initial_balance) * 100.0 : 0.0;
        sv.win_rate = stats.win_rate;
        sv.total_trades = stats.total_trades;
        sv.winning_trades = stats.winning_trades;
        sv.losing_trades = stats.losing_trades;
        sv.profit_factor = stats.profit_factor;
        sv.avg_win = stats.avg_win;
        sv.avg_loss = stats.avg_loss;
        sv.largest_win = stats.largest_win;
        sv.largest_loss = stats.largest_loss;
        sv.total_fees = stats.total_fees;
        sv.turnover = stats.turnover;
        bottom_panel_->set_stats_view(sv);

        // [TEMP DEBUG] Capture the paper accounting so we can see exactly where an
        // import/replicate inflates the total. balance should DROP by purchase cost
        // and holdings_value should rise by ~the same → total_equity ≈ opening.
        LOG_INFO("statsdbg",
                 QString("[paper-stats] acct=%1 opening=%2 balance=%3 used_margin=%4 holdings_value=%5 "
                         "mis_unreal=%6 total_equity=%7 #pos=%8 #holdings=%9 realized=%10 unreal=%11")
                     .arg(account.paper_portfolio_id)
                     .arg(portfolio.initial_balance, 0, 'f', 2)
                     .arg(portfolio.balance, 0, 'f', 2)
                     .arg(used_margin, 0, 'f', 2)
                     .arg(holdings_value, 0, 'f', 2)
                     .arg(mis_unrealized, 0, 'f', 2)
                     .arg(fv.total_equity, 0, 'f', 2)
                     .arg(intraday.size())
                     .arg(holdings.size())
                     .arg(sv.realized_pnl, 0, 'f', 2)
                     .arg(unrealized, 0, 'f', 2));

        // Held symbols get live WebSocket prices even if not in the active list.
        update_position_symbols(pos_syms);
        update_chart_position(); // refresh the on-chart position card
    } catch (...) {
        LOG_WARN(TAG, "Exception refreshing paper panels");
    }
}

void EquityTradingScreen::on_convert_position(const QString& position_id, const QString& symbol,
                                              const QString& new_product) {
    if (focused_account_id_.isEmpty() || position_id.isEmpty())
        return;
    auto account = AccountManager::instance().get_account(focused_account_id_);
    if (account.trading_mode != "paper" || account.paper_portfolio_id.isEmpty()) {
        order_entry_->show_order_status(tr("Product conversion is available for paper accounts"), false);
        return;
    }
    const auto reply = QMessageBox::question(
        this, tr("Convert to %1").arg(new_product),
        tr("Convert %1 to %2 (delivery)?\n\nThis locks the full position value as cash and carries it overnight "
           "instead of auto-squaring at 15:30.")
            .arg(symbol, new_product),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;
    try {
        pt_convert_position_product(position_id, new_product);
        order_entry_->show_order_status(tr("%1 converted to %2").arg(symbol, new_product), true);
    } catch (const std::exception& e) {
        order_entry_->show_order_status(tr("Convert failed: %1").arg(e.what()), false);
    }
    refresh_paper_panels();
}

void EquityTradingScreen::on_orders_day_changed(const QDate& day) {
    orders_view_day_ = day;
    auto account = AccountManager::instance().get_account(focused_account_id_);
    if (account.trading_mode == "paper" && !account.paper_portfolio_id.isEmpty())
        bottom_panel_->set_paper_orders(pt_get_orders_for_day(account.paper_portfolio_id, day));
}

void EquityTradingScreen::on_square_off_group(const QString& account_id, int sign) {
    if (account_id.isEmpty() || sign == 0)
        return;
    auto account = AccountManager::instance().get_account(account_id);

    // Collect the matching positions (the panel already confirmed via dialog).
    struct Target {
        QString symbol;
        QString exchange;
        QString product;
    };
    QVector<Target> targets;
    if (account.trading_mode == "paper" && !account.paper_portfolio_id.isEmpty()) {
        for (const auto& p : pt_get_positions(account.paper_portfolio_id)) {
            if (product_is_delivery(p.product))
                continue; // CNC lives in Holdings — not squared by the positions buttons
            if ((sign > 0 && p.unrealized_pnl > 0.0) || (sign < 0 && p.unrealized_pnl < 0.0))
                targets.push_back({p.symbol, QString(), p.product});
        }
    } else {
        for (const auto& p : live_positions_) {
            if ((sign > 0 && p.pnl > 0.0) || (sign < 0 && p.pnl < 0.0))
                targets.push_back({p.symbol, p.exchange, p.product_type});
        }
    }
    if (targets.isEmpty()) {
        order_entry_->show_order_status(
            tr("No %1 positions to square off").arg(sign > 0 ? tr("winning") : tr("losing")), false);
        return;
    }

    // Close each in a single background worker (sequential) to avoid a thread storm.
    const QString acct_id = account_id;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run([self, acct_id, targets]() {
        if (!self)
            return;
        for (const auto& t : targets)
            UnifiedTrading::instance().close_position(acct_id, t.symbol, t.exchange, t.product);
        QMetaObject::invokeMethod(
            self,
            [self]() {
                if (self)
                    self->refresh_paper_panels();
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_trade_symbol_requested(const QString& symbol, const QString& product, bool is_buy,
                                                    double qty) {
    // Exchange isn't carried on the row; default to the screen's current exchange.
    open_order_ticket_for(symbol, QString(), product, is_buy, qty);
}

void EquityTradingScreen::open_order_ticket_for(const QString& symbol, const QString& exchange,
                                                const QString& product, bool is_buy, double qty) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    const QString exch = exchange.isEmpty() ? selected_exchange_ : exchange;
    const double ref_price = (symbol == selected_symbol_ && current_price_ > 0.0) ? current_price_ : 0.0;

    QDialog dlg(this);
    dlg.setWindowTitle(is_buy ? tr("Buy %1").arg(symbol) : tr("Sell %1").arg(symbol));
    auto* form = new QFormLayout(&dlg);

    auto* hdr = new QLabel(
        QStringLiteral("%1  %2 · %3 · %4").arg(is_buy ? tr("BUY") : tr("SELL"), symbol, exch, product.toUpper()), &dlg);
    hdr->setStyleSheet(QString("font-weight:700;font-size:13px;color:%1;")
                           .arg(is_buy ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    form->addRow(hdr);

    auto* type_combo = new QComboBox(&dlg);
    type_combo->addItems({tr("Market"), tr("Limit")});
    type_combo->setCurrentIndex(0);
    form->addRow(tr("Type"), type_combo);

    auto* qty_spin = new QSpinBox(&dlg);
    qty_spin->setRange(1, 10000000);
    // Pre-fill with the held quantity on a reduce/exit (qty > 0); otherwise default
    // to 1. Lets "Sell" exit the whole position in one click, like the FNO ticket.
    qty_spin->setValue(qty > 0.0 ? static_cast<int>(qty) : 1);
    form->addRow(tr("Qty"), qty_spin);

    auto* px_spin = new QDoubleSpinBox(&dlg);
    px_spin->setRange(0.01, 1.0e9);
    px_spin->setDecimals(2);
    px_spin->setValue(ref_price > 0.0 ? ref_price : 0.01);
    form->addRow(tr("Limit price"), px_spin);

    auto sync_type = [form, px_spin](int idx) { form->setRowVisible(px_spin, idx == 1); }; // 1 = Limit
    connect(type_combo, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, sync_type);
    sync_type(type_combo->currentIndex());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton* ok = buttons->addButton(is_buy ? tr("Buy") : tr("Sell"), QDialogButtonBox::AcceptRole);
    ok->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    UnifiedOrder order;
    order.symbol = symbol;
    order.exchange = exch;
    order.side = is_buy ? OrderSide::Buy : OrderSide::Sell;
    const bool is_market = (type_combo->currentIndex() == 0);
    order.order_type = is_market ? OrderType::Market : OrderType::Limit;
    order.quantity = qty_spin->value();
    order.price = is_market ? 0.0 : px_spin->value();
    order.product_type = product_from_broker_str(product);
    order.validity = QStringLiteral("DAY");
    on_order_submitted(order);
}

QString EquityTradingScreen::pick_account_for_exchanges(const QStringList& match) const {
    if (match.isEmpty())
        return {};
    QString connected, paper;
    for (const auto& a : AccountManager::instance().list_accounts()) {
        if (!a.is_active)
            continue;
        const bool live_ok = a.state == ConnectionState::Connected;
        if (!(a.trading_mode == "paper" || live_ok))
            continue;
        auto* b = BrokerRegistry::instance().get(a.broker_id);
        if (!b)
            continue;
        const QStringList exchanges = b->profile().exchanges;
        bool hit = false;
        for (const auto& me : match) {
            if (exchanges.contains(me, Qt::CaseInsensitive)) {
                hit = true;
                break;
            }
        }
        if (!hit)
            continue;
        if (a.account_id == focused_account_id_)
            return a.account_id; // already focused & matches — least disruptive
        if (live_ok && connected.isEmpty())
            connected = a.account_id;
        if (paper.isEmpty())
            paper = a.account_id;
    }
    return !connected.isEmpty() ? connected : paper;
}

void EquityTradingScreen::open_external_order_ticket(const QString& symbol, const QString& exchange,
                                                     const QStringList& match_exchanges, bool is_buy,
                                                     double ref_price) {
    if (symbol.isEmpty())
        return;

    // Route to a usable account whose broker can actually trade this symbol's market
    // (broker.exchanges ∩ match_exchanges) — so a US ticker goes to Alpaca/IBKR/Saxo
    // and an NSE ticker to the Indian broker, even with several brokers connected.
    // When the caller didn't constrain the market, fall back to the credential-aware
    // focused pick (also covers a never-shown screen with no focus yet).
    QString target = pick_account_for_exchanges(match_exchanges);
    if (target.isEmpty() && match_exchanges.isEmpty()) {
        ensure_account_loaded();
        target = focused_account_id_;
    }
    if (target.isEmpty()) {
        QMessageBox::information(
            this, tr("Trade %1").arg(symbol),
            tr("No connected broker can trade %1. Add or connect a broker for this market in the ACCOUNTS panel.")
                .arg(symbol));
        return;
    }

    // Switch to the matching account through the normal path (which clears and
    // refetches the shared blotter correctly) so the order — and the trading tab,
    // next time it's opened — reflect the broker we're trading with.
    if (target != focused_account_id_)
        on_account_changed(target);
    if (focused_account_id_.isEmpty())
        return;

    // open_order_ticket_for() / on_order_submitted() read the ref + paper-fill price
    // from selected_symbol_ / current_price_. Adopt the caller's symbol/price (and
    // the resolved exchange) for THIS order, then restore — the per-order overrides
    // must not outlive the modal. exec() is synchronous, so placement completes
    // before we restore. An empty exchange keeps the broker's default (set by
    // on_account_changed) — correct for US brokers that route by symbol. ref_price<=0
    // leaves price at 0 so a paper MARKET order is guarded ("price not available")
    // rather than filling at a stale price.
    const QString prev_symbol = selected_symbol_;
    const QString prev_exchange = selected_exchange_;
    const double prev_price = current_price_;

    selected_symbol_ = symbol;
    if (!exchange.isEmpty())
        selected_exchange_ = exchange;
    current_price_ = ref_price > 0.0 ? ref_price : 0.0;

    // Delivery (CNC) default for a research-driven order; the quick ticket doesn't
    // expose product choice (the full trading tab does).
    open_order_ticket_for(symbol, selected_exchange_, QStringLiteral("CNC"), is_buy, 0.0);

    selected_symbol_ = prev_symbol;
    selected_exchange_ = prev_exchange;
    current_price_ = prev_price;
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
    auto account = AccountManager::instance().get_account(focused_account_id_);

    // SQUARE OFF ALL acts on the POSITIONS tab only (intraday MIS/NRML). CNC /
    // delivery exposure lives in Holdings and is squared off separately — this
    // button never touches it. (The shared UnifiedTrading::close_all_positions
    // closes everything, so we collect intraday-only targets and close each.)
    struct Target {
        QString symbol;
        QString exchange;
        QString product;
    };
    QVector<Target> targets;
    if (account.trading_mode == "paper" && !account.paper_portfolio_id.isEmpty()) {
        for (const auto& p : pt_get_positions(account.paper_portfolio_id)) {
            if (p.quantity == 0.0 || product_is_delivery(p.product))
                continue; // CNC/delivery -> Holdings, not squared here
            targets.push_back({p.symbol, QString(), p.product});
        }
    } else {
        // live_positions_ is the Positions tab feed (broker get_positions);
        // Holdings come from a separate holdings feed and are not included.
        for (const auto& p : live_positions_)
            targets.push_back({p.symbol, p.exchange, p.product_type});
    }
    if (targets.isEmpty()) {
        order_entry_->show_order_status(tr("No open positions to square off"), false);
        return;
    }

    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run([self, acct_id, targets]() {
        if (!self)
            return;
        int ok = 0, fail = 0;
        for (const auto& t : targets) {
            auto r = UnifiedTrading::instance().close_position(acct_id, t.symbol, t.exchange, t.product);
            r.success ? ++ok : ++fail;
        }
        QMetaObject::invokeMethod(
            self,
            [self, ok, fail]() {
                if (!self)
                    return;
                self->order_entry_->show_order_status(
                    self->tr("Closed %1 position(s)%2")
                        .arg(ok)
                        .arg(fail > 0 ? self->tr(", %1 failed").arg(fail) : QString()),
                    fail == 0);
                self->refresh_paper_panels();
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_square_off_all_holdings(const QVector<trading::BrokerHolding>& holdings) {
    LOG_INFO("sqoff", QString("[screen] on_square_off_all_holdings: focused_account='%1' holdings=%2")
                          .arg(focused_account_id_)
                          .arg(holdings.size()));
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    // The Holdings tab already confirmed via dialog. Square off each holding the
    // SAME way the Positions tab does — via close_position, which finds the
    // existing position by symbol and closes it with the correct opposite side.
    // A raw SELL can open a short in paper (or fail to net) instead of reducing
    // the holding. Holdings are delivery, so product = CNC. Touches ONLY holdings.
    struct Target {
        QString symbol;
        QString exchange;
    };
    QVector<Target> targets;
    for (const auto& h : holdings)
        if (h.quantity > 0)
            targets.push_back({h.symbol, h.exchange});
    if (targets.isEmpty()) {
        order_entry_->show_order_status(tr("No holdings to square off"), false);
        return;
    }

    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run([self, acct_id, targets]() {
        if (!self)
            return;
        int ok = 0, fail = 0;
        for (const auto& t : targets) {
            auto r = UnifiedTrading::instance().close_position(acct_id, t.symbol, t.exchange,
                                                               QStringLiteral("CNC"));
            LOG_INFO("sqoff", QString("[screen] close_position sym='%1' exch='%2' -> success=%3 err='%4'")
                                  .arg(t.symbol, t.exchange,
                                       r.success ? QStringLiteral("true") : QStringLiteral("false"), r.error));
            r.success ? ++ok : ++fail;
        }
        QMetaObject::invokeMethod(
            self,
            [self, ok, fail]() {
                if (!self)
                    return;
                self->order_entry_->show_order_status(
                    self->tr("Squared off %1 holding(s)%2")
                        .arg(ok)
                        .arg(fail > 0 ? self->tr(", %1 failed").arg(fail) : QString()),
                    fail == 0);
                self->refresh_paper_panels(); // paper refreshes here; live flows via the hub
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_square_off_holding(const QString& symbol, const QString& exchange) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    const auto reply =
        QMessageBox::question(this, tr("Square Off Holding"), tr("Square off %1 at market?").arg(symbol),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run([self, acct_id, symbol, exchange]() {
        if (!self)
            return;
        auto r = UnifiedTrading::instance().close_position(acct_id, symbol, exchange, QStringLiteral("CNC"));
        LOG_INFO("sqoff", QString("[screen] per-row close_position sym='%1' exch='%2' -> success=%3 err='%4'")
                              .arg(symbol, exchange, r.success ? QStringLiteral("true") : QStringLiteral("false"),
                                   r.error));
        QMetaObject::invokeMethod(
            self,
            [self, r, symbol]() {
                if (!self)
                    return;
                self->order_entry_->show_order_status(
                    r.success ? self->tr("Squared off %1").arg(symbol) : r.error, r.success);
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
