// src/screens/equity_trading/EquityTradingScreen_Handlers.cpp
//
// User-action handlers: account switch / symbol selection / mode toggle,
// token-expiry recovery, accounts dialog, order submit / cancel / modify,
// order-book click, refresh_candles.
//
// Part of the partial-class split of EquityTradingScreen.cpp.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "screens/equity_trading/AccountManagementDialog.h"
#include "screens/equity_trading/BroadcastOrderDialog.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChartPanel.h"
#include "screens/equity_trading/EquityCommandBar.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityTradingScreen.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "screens/equity_trading/OrderConfirmDialog.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/OrderSourceCapture.h"
#include "trading/OrderThreadPool.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "core/instrument/VenueSymbols.h"
#include "ui/formatting/NumberFormat.h"
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
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStringListModel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <memory>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::equity;

static const QString TAG = "EquityTrading";

namespace {
// An order price with its currency unit for a confirmation / status line: the
// order exchange's currency (venue table), else `fallback` (the broker account's);
// no unit when neither is known. Never converted.
QString eqh_order_px(double price, const QString& symbol, const QString& exchange, const QString& fallback = {}) {
    namespace uf = fincept::ui::formatting;
    const uf::InstrumentFmt inst = uf::instrument_for_symbol(symbol, exchange_currency(exchange, fallback));
    return inst.currency.isEmpty() ? uf::price(price, inst) : uf::price_with_symbol(price, inst);
}
} // namespace

void EquityTradingScreen::on_account_changed(const QString& account_id) {
    if (account_id == focused_account_id_)
        return;
    if (is_builtin_paper_account(account_id)) {
        focus_builtin_account(); // no broker: its own, much smaller set-up
        return;
    }
    const bool from_builtin = focused_is_builtin();
    focused_account_id_ = account_id;
    if (!public_tracked_portfolio_.isEmpty()) {
        // The previous paper account's delayed-price tracking belongs to it alone.
        PaperMarkService::instance().untrack_public_portfolio(public_tracked_portfolio_);
        public_tracked_portfolio_.clear();
    }
    if (from_builtin) {
        // Back on a broker: its symbols, its exchange list, live price rules.
        builtin_selected_symbol_ = selected_symbol_;
        instruments_.clear();
        mode_btn_->setEnabled(true);
        mode_btn_->setToolTip(QString());
        ticker_bar_->set_bid_ask_visible(true);
        ticker_bar_->set_price_format(QString(), -1);
        if (first_run_note_)
            first_run_note_->hide();
    }
    // The ticket's / watchlist's search marks what this broker cannot trade.
    {
        const QString bid = AccountManager::instance().get_account(account_id).broker_id;
        QStringList exchanges;
        QString broker_name = bid;
        if (auto* b = BrokerRegistry::instance().get(bid)) {
            exchanges = b->profile().exchanges;
            broker_name = b->profile().display_name;
        }
        auto filter = [bid, exchanges, broker_name](const fincept::core::instrument::Instrument& inst) {
            QString ex;
            const QString bsym = fincept::core::instrument::to_broker_symbol(inst, bid, &ex);
            if (bsym.isEmpty() || (!exchanges.isEmpty() && !exchanges.contains(ex, Qt::CaseInsensitive)))
                return SymbolBox::RowCheck(SymbolBox::RowState::Marked,
                                           QCoreApplication::translate("EquityTradingScreen", "not tradable on %1")
                                               .arg(broker_name));
            return SymbolBox::RowCheck();
        };
        symbol_box_->set_row_filter(filter);
        watchlist_->add_box()->set_row_filter(filter);
        // F&O contracts and broker-only codes are still added through the broker's
        // own instrument master (raw text → on_watchlist_symbol_added).
        watchlist_->add_box()->set_raw_text_allowed(true);
    }
    // A "no live quotes" verdict belongs to the previous account's broker; this one
    // gets its own from its first depth request (note_broker_quote_refusal).
    broker_quote_seen_ = false;
    if (!broker_quote_refusal_.isEmpty()) {
        broker_quote_refusal_.clear();
        apply_broker_quote_notice();
    }
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return;

    // Wire the positions/orders header buttons (square-off winners/losers,
    // SQUARE OFF ALL, cancel-all-orders) to this account. Without this the panel's
    // account_id_ stays empty and every one of those buttons silently no-ops.
    bottom_panel_->set_account_id(account_id);

    account_btn_->set_label(account.display_name.toUpper());

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
        order_entry_->set_account_id(account_id); // margin line reads THIS account's credentials
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
    sync_native_paper(); // blotter source: broker rows for LIVE + native paper, paper-engine rows otherwise

    // Load named watchlists (seeds a default from the broker on first run) and
    // populate watchlist_symbols_ from the active list before the stream wiring
    // below subscribes to it.
    load_watchlists();
    ticker_bar_->set_symbol(selected_symbol_);
    // Price currency: the exchange's (venue table), else the broker's account currency.
    ticker_bar_->set_currency(exchange_currency(selected_exchange_, broker ? broker->profile().currency : QString()));
    apply_selected_instrument_ui(); // symbol box, instrument line, ticket symbol / exchange
    update_right_column();

    // Resubscribe hub topics for new focused account
    if (isVisible())
        hub_subscribe_streaming();

    // Decide the market-data source for this account before any fetch below: a
    // broker that can't serve market data hands the screen to the public feed
    // (which this call starts / retargets); the fetches below then run only for a
    // broker source. They are this path's own, hence no fetch on the switch.
    sync_market_feed(/*fetch_on_switch=*/false);

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
            stream->subscribe_symbols(QStringLiteral("equity:watchlist"), stream_symbols());
            if (!public_mode_) {
                request_chart_history();
                stream->fetch_orderbook(selected_symbol_);
                // US-only market data — skip for Indian/other brokers (no tape/calendar).
                if (focused_is_us_market_) {
                    stream->fetch_time_sales(selected_symbol_);
                    bottom_panel_->set_auctions({}); // never show another account's / symbol's prints
                    stream->fetch_auctions(selected_symbol_);
                    stream->fetch_condition_codes(); // decodes the TIME & SALES "Conditions" column
                    stream->fetch_calendar();
                    stream->fetch_clock();
                }
            }
        }
    }

    // Switching accounts cleared the shared blotter (EquityBottomPanel::
    // set_account_id). For a live account whose stream was already running, force
    // an immediate portfolio refetch so it refills with THIS account's positions/
    // holdings/orders right away instead of staying blank until the 5-min poll.
    if (has_creds && (is_live || focused_native_paper_) && stream_existed)
        dsm.refresh_portfolio(account_id);

    // Each account's order book opens on today's session.
    orders_view_day_ = QDate::currentDate();

    // Load paper portfolio orders into matcher (local simulator only — a native-paper
    // account's orders live at the broker)
    if (!account.paper_portfolio_id.isEmpty() && account.trading_mode == "paper" && !focused_native_paper_) {
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
    update_feed_notices(); // ticket / depth / blotter notes follow the new account + mode
    // Which account the ticket sends to (never its display name: free text, often a person's name).
    LOG_INFO(TAG, logf("Focused trading account changed", {{"account", log_mask(account.account_id)},
                                                            {"broker", account.broker_id},
                                                            {"mode", account.trading_mode}}));
}

void EquityTradingScreen::on_symbol_selected(const QString& raw_symbol) {
    QString symbol = raw_symbol;
    if (canonical_mode() && !symbol.isEmpty() && !instruments_.contains(symbol)) {
        // Paper account: screen symbols are canonical. A broker-style or otherwise
        // non-canonical spelling (a restored state, a position row) is resolved first.
        const auto inst = eq_instrument_for_stored(symbol, exchange_for_symbol(symbol));
        if (inst.symbol.isEmpty()) {
            order_entry_->show_status_message(tr("%1 has no public market data").arg(symbol), false);
            return;
        }
        instruments_.insert(inst.symbol, inst);
        symbol = inst.symbol;
    }
    if (symbol.isEmpty() || symbol == selected_symbol_)
        return;
    switch_symbol(symbol);
}

void EquityTradingScreen::switch_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    current_price_ = 0.0; // unknown until the new symbol's first quote arrives
    if (canonical_mode()) {
        builtin_selected_symbol_ = symbol;
        selected_exchange_ = instrument_of(symbol).venue;
    }
    const auto inst = instrument_of(symbol);
    ticker_bar_->set_symbol(canonical_mode() && !inst.display.isEmpty() ? inst.display : symbol);
    watchlist_->set_active_symbol(symbol);
    apply_selected_instrument_ui(); // symbol box, instrument line, ticket, session line
    apply_quote_detail(symbol);     // a quote already in hand: snapshot + ticket price source now
    update_chart_position();        // show/clear the new symbol's position on the chart

    // Resubscribe quote topics so the new symbol gets a hub subscription
    if (isVisible() && hub_active_)
        hub_subscribe_streaming();

    // Publish to the linked group so other panels (Watchlist, EquityResearch,
    // News, Derivatives, SurfaceAnalytics) follow. `this` as source suppresses
    // the echo back into this screen.
    // current_symbol() attaches an exchange only when it is known for this symbol
    // (not our default market to a ticker adopted from the group).
    if (link_group_ != SymbolGroup::None) {
        SymbolContext::instance().set_group_symbol(link_group_, current_symbol(), this);
    }

    auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
    if (stream)
        stream->set_selected_symbol(symbol, selected_exchange_); // keeps the stream coherent for a reconnect

    if (public_mode_) {
        // Public fallback: retarget its quote set + chart at the new symbol, and
        // show what is already known for it (its row's topic may be subscribed
        // already, so nothing new would arrive before the next refresh).
        ticker_bar_->setToolTip(QString());
        push_public_targets();
        if (public_feed_)
            public_feed_->replay_quote(symbol);
        return;
    }

    // The previous symbol's ladder must not stay on screen (or be clickable into
    // the new ticket's limit price) while this symbol's depth loads; a depth
    // failure note from the previous symbol goes with it.
    orderbook_->clear();
    orderbook_->set_empty_message(QString());
    orderbook_symbol_.clear();

    request_chart_history();
    if (stream) {
        stream->fetch_orderbook(symbol);

        auto account = AccountManager::instance().get_account(focused_account_id_);
        // The previous symbol's auction prints must not stay under this one.
        bottom_panel_->set_auctions({});
        if (account.trading_mode == "live" && focused_is_us_market_) {
            stream->fetch_time_sales(symbol);
            stream->fetch_auctions(symbol);
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

void EquityTradingScreen::on_mode_toggled() {
    if (focused_account_id_.isEmpty())
        return;
    if (focused_is_builtin()) {
        mode_btn_->setChecked(false); // never LIVE: there is no broker behind it
        return;
    }

    const bool is_live = mode_btn_->isChecked();
    mode_btn_->setText(is_live ? tr("LIVE") : tr("PAPER"));
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);

    // Update per-account trading mode
    AccountManager::instance().set_trading_mode(focused_account_id_, is_live ? "live" : "paper");
    sync_native_paper(); // reads the mode just stored; also switches the blotter's data source

    // Re-point hub subscriptions at the new mode: local paper drops the live broker
    // positions/holdings/orders/balance topics so live data can't leak in.
    if (isVisible())
        hub_subscribe_streaming();

    if (!is_live && !focused_native_paper_) {
        refresh_paper_panels(); // positions/orders/trades/stats/funds from the paper engine
    } else {
        // Switching to live (or to a native broker paper venue): set_mode() just
        // cleared the shared blotter and the stream is already running (so it won't
        // re-fetch on its own). Force an immediate refresh so broker
        // positions/holdings/orders appear now instead of leaving the blotter blank
        // until the 5-min poll.
        DataStreamManager::instance().refresh_portfolio(focused_account_id_);
    }
    update_feed_notices(); // what the ticket can do without a broker depends on the mode
}

void EquityTradingScreen::sync_native_paper() {
    const auto account = AccountManager::instance().get_account(focused_account_id_);
    const bool is_live = (account.trading_mode == "live");
    focused_native_paper_ = !is_live && UnifiedTrading::instance().uses_native_paper(focused_account_id_);
    // The blotter renders paper-engine rows only for the LOCAL simulator; LIVE and native
    // paper both feed it broker rows. (No-op when unchanged, so this is cheap to repeat.)
    bottom_panel_->set_mode(!is_live && !focused_native_paper_);
}

void EquityTradingScreen::handle_token_expired(const QString& account_id) {
    bool expected = false;
    if (!token_expired_shown_.compare_exchange_strong(expected, true))
        return;

    LOG_WARN(TAG, QString("Access token expired for account %1 — user must re-authenticate").arg(account_id));
    // Sets the label (TOKEN EXPIRED, or the public-feed variant) — a text set here
    // first was overwritten by this call straight away.
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
        // No account left (or the next one can't serve market data): hand the
        // screen to the public feed instead of leaving it on a dead stream.
        sync_market_feed();
    });

    connect(dlg, &AccountManagementDialog::credentials_saved, this, [this](const QString& account_id) {
        // Handled here; the queued AccountManager::credentials_changed follow-up for this
        // same connect (see connect_data_stream_signals) must not run it a second time.
        creds_saved_handled_.insert(account_id);
        on_credentials_saved(account_id);
    });

    dlg->exec();
    dlg->deleteLater();
}

void EquityTradingScreen::on_credentials_saved(const QString& account_id) {
    token_expired_shown_.store(false);
    // Broker just connected — load its instrument master so market data
    // (watchlist quotes, charts, depth) can resolve numeric securityIds.
    // Covers every broker: all connect paths emit credentials_saved.
    ensure_instruments_loaded(account_id);
    // First account ever connected: focus it now (account_added intentionally
    // does not auto-focus — see comment there). on_account_changed() does its
    // own start_stream() + symbol/watchlist setup, so we're done after that.
    // (The built-in paper account counts as "nothing focused" here: connecting a
    // broker is the user asking for it.)
    if (focused_account_id_.isEmpty() || focused_is_builtin()) {
        on_account_changed(account_id);
        AccountManager::instance().set_connection_state(account_id, ConnectionState::Connected);
        update_connection_status();
        return;
    }
    // New credentials can flip the focused account between the local simulator and a
    // broker paper venue (e.g. live keys replaced by paper keys): re-derive, re-point
    // the hub topics and reload the matching data when that happened.
    if (account_id == focused_account_id_) {
        const bool was_native = focused_native_paper_;
        sync_native_paper();
        if (was_native != focused_native_paper_) {
            if (isVisible())
                hub_subscribe_streaming();
            if (focused_native_paper_)
                DataStreamManager::instance().refresh_portfolio(account_id);
            else
                refresh_paper_panels();
        }
    }
    // Start/restart the stream with new credentials
    DataStreamManager::instance().start_stream(account_id);
    auto* stream = DataStreamManager::instance().stream_for(account_id);
    if (stream && account_id == focused_account_id_) {
        stream->set_selected_symbol(selected_symbol_, selected_exchange_);
        stream->subscribe_symbols(QStringLiteral("equity:watchlist"), stream_symbols());
    }
    AccountManager::instance().set_connection_state(account_id, ConnectionState::Connected);
    update_connection_status();
}

// ============================================================================
// Order Handling
// ============================================================================

void EquityTradingScreen::on_order_submitted(const UnifiedOrder& order) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    // Paper on delayed public prices — the built-in account, or a local-paper broker
    // account whose broker feed is down: simulated fills with the delayed-price rules
    // (pt_submit_delayed_order). Never a broker call.
    if (simulated_on_delayed_prices()) {
        place_simulated_order(order);
        return;
    }

    auto account = AccountManager::instance().get_account(focused_account_id_);

    // Reference / fill price of THIS order's symbol. current_price_ is the selected
    // symbol's LTP only — an exit of another symbol from the Positions / Holdings rows
    // used to fill (paper) or be confirmed (live) at the selected symbol's price.
    const double ltp = ref_price_for(order.symbol);

    // PAPER mode runs on the local simulator below, unless the broker has a verified native
    // paper venue (focused_native_paper_) — then it takes the broker route like a LIVE order.
    if (account.trading_mode == "paper" && !focused_native_paper_) {
        const QString portfolio_id = account.paper_portfolio_id;
        if (portfolio_id.isEmpty()) {
            LOG_AUDIT("Orders", "Order rejected",
                      {{"venue", "paper"}, {"account", log_mask(account.account_id)}, {"mode", "paper"},
                       {"symbol", order.symbol}, {"qty", order.quantity}, {"source", "ticket"},
                       {"reason", "no paper portfolio for this account"}});
            order_entry_->show_order_status(tr("No paper portfolio for this account"), false);
            return;
        }

        // The order itself is audited by the paper engine (Order accepted / filled / rejected).
        LOG_DEBUG(TAG, logf("Paper order from the ticket", {{"account", log_mask(account.account_id)},
                                                            {"symbol", order.symbol},
                                                            {"side", order.side == OrderSide::Buy ? "buy" : "sell"},
                                                            {"qty", order.quantity}}));

        const QString side = order.side == OrderSide::Buy ? "buy" : "sell";
        const QString type = order_type_str(order.order_type);
        std::optional<double> price_opt;
        if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
            price_opt = order.price;
        } else if (order.order_type == OrderType::Market) {
            const double ref = ltp > 0 ? ltp : (order.price > 0 ? order.price : 0.0);
            if (ref > 0)
                price_opt = ref;
        }
        std::optional<double> stop_opt;
        if (order.stop_price > 0)
            stop_opt = order.stop_price;

        if (order.order_type == OrderType::Market && !price_opt) {
            LOG_AUDIT("Orders", "Order rejected",
                      {{"venue", "paper"}, {"account", log_mask(account.account_id)}, {"mode", "paper"},
                       {"symbol", order.symbol}, {"side", side}, {"type", type}, {"qty", order.quantity},
                       {"source", "ticket"}, {"reason", "no live price for a market fill"}});
            // On the public fallback no live price will ever arrive for the fill —
            // say so instead of asking the user to wait.
            order_entry_->show_order_status(
                public_mode_ ? tr("No live price: paper MARKET orders fill on the broker feed, which is not "
                                  "connected. Use a LIMIT order, or reconnect the broker in ACCOUNTS.")
                             : tr("Price not available yet — wait for quotes to load"),
                false);
            return;
        }

        // Forward the exchange + broker product (MIS/CNC/NRML) so the engine
        // applies per-product leverage, tags the position, and routes CNC to the
        // Holdings tab. Without this the product was silently dropped (everything
        // looked like blank-product CNC) and MIS could never be told apart.
        const QString product = product_to_broker_str(order.product_type);
        // Cash is booked in the paper book's currency at the engine's per-symbol
        // factor (1.0 until set): an instrument quoted in another currency needs a
        // real rate first — never booked 1:1.
        QString fx_reason;
        if (!PaperMarkService::prepare_symbol_fx(portfolio_id, order.symbol, order.exchange, &fx_reason)) {
            order_entry_->show_order_status(fx_reason, false);
            return;
        }
        try {
            // Provenance for the audit trail: the agent's while an agent pressed BUY / SELL.
            auto pt_order = pt_place_order(portfolio_id, order.symbol, side, type, order.quantity, price_opt, stop_opt,
                                           /*reduce_only=*/false, order.exchange, product,
                                           capture_order_source(QStringLiteral("ticket")));

            if (order.order_type == OrderType::Market) {
                double fill_price = ltp > 0 ? ltp : (order.price > 0 ? order.price : 0.0);
                if (fill_price <= 0) {
                    order_entry_->show_order_status(tr("No price available for fill"), false);
                    return;
                }
                pt_fill_order(pt_order.id, fill_price);
                // Register the protective SL/TP bracket against the filled position.
                // check_sl_tp_triggers runs every tick but ran against an EMPTY registry
                // because set_sl_tp was never called on the equity path — so equity paper
                // stops gave zero protection until now.
                if (order.stop_loss > 0 || order.take_profit > 0)
                    OrderMatcher::instance().set_sl_tp(portfolio_id, order.symbol, pt_order.id, order.stop_loss,
                                                       order.take_profit);
                order_entry_->show_order_status(
                    tr("Paper order filled: %1 @ %2")
                        .arg(order.symbol, eqh_order_px(fill_price, order.symbol, order.exchange, account_currency_)),
                    true);
            } else {
                OrderMatcher::instance().add_order(pt_order);
                if (order.stop_loss > 0 || order.take_profit > 0)
                    OrderMatcher::instance().set_sl_tp(portfolio_id, order.symbol, pt_order.id, order.stop_loss,
                                                       order.take_profit);
                order_entry_->show_order_status(tr("Paper order queued: %1").arg(order.symbol), true);
            }
        } catch (const std::exception& e) {
            // An engine refusal is in the audit trail; this line says where it came from.
            LOG_WARN(TAG, logf("Paper order failed", {{"account", log_mask(account.account_id)},
                                                      {"symbol", order.symbol},
                                                      {"side", side},
                                                      {"qty", order.quantity},
                                                      {"error", e.what()}}));
            order_entry_->show_order_status(tr("Order failed: %1").arg(e.what()), false);
            return;
        }

        // Refresh paper portfolio (positions/holdings/orders/funds/stats).
        refresh_paper_panels();
    } else {
        // Route to the broker via account-aware UnifiedTrading (LIVE, or PAPER on a native
        // broker paper venue — UnifiedTrading re-verifies that before sending).
        // Safety: capture account_id by value at click time (immutable per order lifecycle)
        const QString acct_id = focused_account_id_;

        // SL / TP the broker would silently drop: refuse BEFORE the confirmation lists them
        // as part of the order (UnifiedTrading refuses again at the broker boundary). The
        // ticket already disables them for LIVE; this also covers a broker paper venue.
        if (order.stop_loss > 0.0 || order.take_profit > 0.0) {
            auto* broker = BrokerRegistry::instance().get(account.broker_id);
            if (!broker || !entry_sl_tp_honoured(broker->profile(), order.product_type, order.stop_loss > 0.0,
                                                 order.take_profit > 0.0)) {
                order_entry_->show_order_status(
                    tr("%1 does not accept this stop-loss / take-profit on the entry order — nothing was sent. "
                       "Clear SL/TP and place a separate SL order.")
                        .arg(broker ? broker->profile().display_name : account.broker_id),
                    false);
                return;
            }
        }

        // Pre-send confirmation. Manual orders are synchronous (the trader is
        // present), so approval is an inline modal — no pending-queue round-trip.
        //   • LIVE accounts: ALWAYS confirm, unless the user explicitly opted this
        //     account into one-click trading (Accounts dialog; off by default).
        //   • Semi-Auto accounts (any mode): always confirm — one-click never
        //     overrides an approval requirement the user turned on.
        //   • PAPER (incl. a broker paper venue) in Auto mode: unchanged, no dialog.
        // Headless paths (AI / workflow / broadcast) still queue via ActionCenter.
        const bool semi_auto = ActionCenter::instance().should_queue(acct_id, "placeorder");
        const bool is_live = (account.trading_mode == QLatin1String("live"));
        const bool one_click = is_live && ActionCenter::instance().one_click_enabled(acct_id);
        if (semi_auto || (is_live && !one_click)) {
            const QString acct_label = account.display_name.isEmpty() ? acct_id : account.display_name;
            // The ticket names the price currency: the quote's own when known, else the
            // order exchange's (venue table), else the broker account's.
            const auto detail = quote_detail_.constFind(order.symbol);
            QString px_ccy = detail != quote_detail_.constEnd() ? detail->currency : QString();
            if (px_ccy.isEmpty()) {
                const auto* confirm_broker = BrokerRegistry::instance().get(account.broker_id);
                px_ccy = exchange_currency(order.exchange, confirm_broker ? confirm_broker->profile().currency : QString());
            }
            const int px_digits = detail != quote_detail_.constEnd() ? detail->price_digits : -1;
            if (!OrderConfirmDialog::confirm(this, order, acct_label, ltp, acct_id, px_ccy, px_digits)) {
                order_entry_->show_order_status(tr("Order cancelled"), false);
                return;
            }
        } else if (one_click) {
            // The order itself is audited by UnifiedTrading; this says it went without a confirmation.
            LOG_INFO(TAG, logf("Live order sent without confirmation (one-click)",
                               {{"account", log_mask(acct_id)},
                                {"broker", account.broker_id},
                                {"symbol", order.symbol},
                                {"side", order.side == OrderSide::Buy ? "buy" : "sell"},
                                {"qty", order.quantity},
                                {"one_click", true}}));
        }

        QPointer<EquityTradingScreen> self = this;
        auto order_copy = order;
        // Captured here on the GUI thread, after the confirmation: the order runs on a
        // worker where no agent step is visible. A person who answered the confirmation
        // is outside the agent's step, so their order stays "ticket".
        order_copy.source = capture_order_source(QStringLiteral("ticket"));
        // Dedicated order executor — never queued behind the shared global pool.
        (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id, order_copy]() {
            if (!self)
                return;
            auto result = UnifiedTrading::instance().place_order(acct_id, order_copy);
            QMetaObject::invokeMethod(
                self,
                [self, acct_id, result]() {
                    if (!self)
                        return;
                    if (result.success) {
                        LOG_DEBUG(TAG, QString("Order placed: %1").arg(result.order_id)); // audited by UnifiedTrading
                        self->order_entry_->show_order_status(self->tr("Order placed: %1").arg(result.order_id), true);
                        // A native-paper blotter lives at the broker and otherwise only
                        // refreshes on the 5-min poll — pull it now (async, stream-owned).
                        // The response's own mode tag says whether THIS order went to a
                        // broker paper venue, whatever account is focused by now.
                        if (result.mode == QLatin1String("paper"))
                            DataStreamManager::instance().refresh_portfolio(acct_id);
                    } else {
                        LOG_DEBUG(TAG, QString("Order failed: %1").arg(result.message)); // audited by UnifiedTrading
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
void EquityTradingScreen::on_multi_broker_submit(const trading::UnifiedOrder& order, const QStringList& account_ids) {
    if (account_ids.isEmpty()) {
        // Releases the order ticket's send lock (armed on click) — every exit
        // from this slot must report an outcome or the ticket stays disabled.
        order_entry_->show_order_status(tr("No target accounts selected"), false);
        return;
    }
    // SL / TP cannot be honoured uniformly across accounts (most brokers would drop them on
    // the entry order; a background paper account has no tick-driven bracket) — refuse
    // rather than send N orders the trader believes are protected.
    if (order.stop_loss > 0.0 || order.take_profit > 0.0) {
        order_entry_->show_order_status(tr("Stop-loss / take-profit are not supported on multi-broker orders — "
                                           "nothing was sent. Clear SL/TP or place the order per account."),
                                        false);
        return;
    }

    // Only accounts that still exist may receive the order — the inline BROKERS
    // selector caches ids, and one may have been removed/deactivated since it was
    // ticked. Broadcasting to a stale id is a silently-dropped order at best.
    QStringList live_ids;
    QStringList lines;
    int live_n = 0;
    for (const QString& id : account_ids) {
        const auto account = AccountManager::instance().get_account(id);
        if (account.account_id.isEmpty())
            continue;
        const bool is_live = account.trading_mode == "live";
        if (is_live)
            ++live_n;
        const QString mode_tag = is_live ? tr("LIVE") : tr("PAPER");
        lines << QString("• %1  [%2]").arg(account.display_name, mode_tag);
        live_ids << id;
    }
    if (lines.isEmpty()) {
        order_entry_->show_order_status(tr("Selected accounts no longer exist"), false);
        return;
    }

    const QString side = order.side == trading::OrderSide::Buy ? tr("BUY") : tr("SELL");
    // The instrument's own currency (the order exchange's), named on the price.
    const QString px = order.order_type == trading::OrderType::Market
                           ? tr("MARKET")
                           : eqh_order_px(order.price, order.symbol, order.exchange);
    // Spell out product + exchange + order type: this fires N orders at once and
    // the ticket's product (MIS vs CNC) is not otherwise visible from here.
    // Quantity unrounded (a fractional quantity used to read as a whole number here).
    QString detail = tr("%1 %2 × %3 @ %4\nType %5 · Product %6 · Exchange %7\n\nOn %8 account(s)%9:\n\n%10")
                         .arg(side, format_quantity(order.quantity), order.symbol, px,
                              QString::fromLatin1(order_type_str(order.order_type)),
                              QString::fromLatin1(product_to_broker_str(order.product_type)),
                              order.exchange.isEmpty() ? tr("(broker default)") : order.exchange)
                         .arg(lines.size())
                         .arg(live_n > 0 ? tr(" — %1 LIVE").arg(live_n) : QString())
                         .arg(lines.join("\n"));
    // Every field that rides along must be visible before N orders go out.
    if (order.stop_price > 0.0)
        detail += tr("\n\nTrigger %1").arg(eqh_order_px(order.stop_price, order.symbol, order.exchange));
    const auto ret = QMessageBox::question(this, tr("Multi-Broker Order"), detail, QMessageBox::Yes | QMessageBox::No,
                                           QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        order_entry_->show_order_status(tr("Order cancelled"), false);
        return;
    }

    // Provenance, captured on the GUI thread after the confirmation (see the single ticket).
    trading::UnifiedOrder sourced = order;
    sourced.source = capture_order_source(QStringLiteral("ticket"));

    // Per-account Semi-Auto gate: queue those accounts for approval, broadcast
    // the rest immediately (mirrors BroadcastOrderDialog::on_place_order).
    QStringList immediate;
    int queued = 0;
    for (const QString& acct : live_ids) {
        if (ActionCenter::instance().should_queue(acct, "placeorder")) {
            const QString pid =
                ActionCenter::instance().queue_order(acct, "placeorder", ActionCenter::serialize_unified_order(sourced));
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
    auto order_copy = sourced;
    (void)QtConcurrent::run(&order_thread_pool(), [self, immediate, order_copy, queued]() {
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

    // Same exchange the order ticket would send (its EXCHANGE combo, built from the
    // broker profile) — the chart ticket used to ignore a non-default pick there.
    QString chart_exchange = order_entry_->current_exchange();
    if (chart_exchange.isEmpty())
        chart_exchange = selected_exchange_;

    QDialog dlg(this);
    dlg.setWindowTitle(is_buy ? tr("Buy %1").arg(selected_symbol_) : tr("Sell %1").arg(selected_symbol_));
    auto* form = new QFormLayout(&dlg);

    auto* hdr = new QLabel(
        QStringLiteral("%1  %2 · %3").arg(is_buy ? tr("BUY") : tr("SELL"), selected_symbol_, chart_exchange), &dlg);
    hdr->setStyleSheet(QString("font-weight:700;font-size:" + fincept::ui::fonts::px(-1) + "px;color:%1;")
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
    {
        // FX / crypto / sub-unit prices need more than two decimals.
        const int digits = std::clamp(quote_detail_.value(selected_symbol_).price_digits, 2, 6);
        px_spin->setDecimals(digits);
        px_spin->setRange(std::pow(10.0, -digits), 1.0e9);
    }
    px_spin->setValue(price > 0 ? price : 0.01);
    form->addRow(tr("Limit price"), px_spin);

    // Product type must be explicit: MIS auto-squares at 15:30 while CNC carries
    // overnight and locks the full cash. This ticket used to hardcode Intraday,
    // so a chart right-click silently opened an intraday position even for a
    // delivery trader. Intraday stays the default (unchanged behaviour).
    // The choices are the focused broker's own product list (as the order ticket
    // shows it) — a fixed MIS/CNC/NRML list was meaningless for Alpaca / IBKR / Saxo /
    // MetaTrader accounts.
    QVector<ProductTypeDef> products;
    if (auto* broker = BrokerRegistry::instance().get(broker_id_for_focused()))
        products = broker->profile().product_types;
    if (focused_is_builtin())
        products = {{tr("Intraday — squared off at the close"), ProductType::Intraday},
                    {tr("Delivery — carried overnight"), ProductType::Delivery}};
    if (products.isEmpty())
        products = {{tr("Intraday (MIS)"), ProductType::Intraday},
                    {tr("Delivery (CNC)"), ProductType::Delivery},
                    {tr("Margin (NRML)"), ProductType::Margin}};
    auto* product_combo = new QComboBox(&dlg);
    int default_product = -1;
    for (int i = 0; i < products.size(); ++i) {
        product_combo->addItem(products[i].label);
        if (default_product < 0 && products[i].value == ProductType::Intraday)
            default_product = i; // intraday stays the default where the broker offers it
    }
    product_combo->setCurrentIndex(default_product >= 0 ? default_product : 0);
    form->addRow(tr("Product"), product_combo);

    // A Market order needs no price (it fills at the LTP), so the limit-price
    // row (label + field) is hidden entirely unless Limit is selected.
    auto sync_type = [form, px_spin](int idx) { form->setRowVisible(px_spin, idx == 1); }; // 1 = Limit
    connect(type_combo, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, sync_type);
    sync_type(type_combo->currentIndex());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton* ok = buttons->addButton(is_buy ? tr("Buy") : tr("Sell"), QDialogButtonBox::AcceptRole);
    ok->setProperty("fincept_agent_risk", "order");
    ok->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const int product_idx = product_combo->currentIndex();
    if (product_idx < 0 || product_idx >= products.size())
        return; // no product selected — never guess one

    UnifiedOrder order;
    order.symbol = selected_symbol_;
    order.exchange = chart_exchange;
    order.side = is_buy ? OrderSide::Buy : OrderSide::Sell;
    const bool is_market = (type_combo->currentIndex() == 0);
    order.order_type = is_market ? OrderType::Market : OrderType::Limit;
    order.quantity = qty_spin->value();
    order.price = is_market ? 0.0 : px_spin->value();
    order.product_type = products[product_idx].value;
    order.validity = QStringLiteral("DAY");
    if (simulated_on_delayed_prices())
        place_simulated_order(order, /*confirm=*/false); // this ticket was the confirmation
    else
        on_order_submitted(order);
}

void EquityTradingScreen::on_cancel_order(const QString& order_id) {
    auto account = AccountManager::instance().get_account(focused_account_id_);

    if (account.trading_mode == "paper" && !focused_native_paper_) {
        const QString short_id = order_id.left(8);
        try {
            pt_cancel_order(order_id);
            OrderMatcher::instance().remove_order(order_id);
            // pt_cancel_order() is a silent no-op for an order that is no longer resting
            // (the matcher may have filled it a moment ago) — report what really happened.
            bool cancelled = false;
            if (!account.paper_portfolio_id.isEmpty()) {
                for (const auto& o : pt_get_orders(account.paper_portfolio_id, QStringLiteral("cancelled"))) {
                    if (o.id == order_id) {
                        cancelled = true;
                        break;
                    }
                }
            }
            order_entry_->show_status_message(
                cancelled ? tr("Paper order %1 cancelled").arg(short_id)
                          : tr("Paper order %1 was not cancelled — it is no longer open (filled?)").arg(short_id),
                cancelled);
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, logf("Paper order cancel failed", {{"order_id", order_id}, {"error", e.what()}}));
            order_entry_->show_status_message(tr("Cancel failed: %1").arg(QString::fromUtf8(e.what())), false);
        }
        // Refresh paper data
        refresh_paper_panels();
    } else {
        const QString acct_id = focused_account_id_;
        QPointer<EquityTradingScreen> self = this;
        (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id, order_id]() {
            if (!self)
                return;
            const auto result = UnifiedTrading::instance().cancel_order(acct_id, order_id);
            if (!self)
                return; // QMetaObject::invokeMethod asserts on a null receiver
            QMetaObject::invokeMethod(
                self,
                [self, acct_id, order_id, result]() {
                    if (!self)
                        return;
                    // A rejected cancel (already filled, market closed, expired token) used to
                    // vanish silently — the trader could not tell whether a REAL order was pulled.
                    if (result.success) {
                        LOG_DEBUG(TAG, QString("Order cancelled: %1").arg(order_id)); // audited by UnifiedTrading
                        self->order_entry_->show_status_message(
                            self->tr("Order %1 cancelled").arg(order_id.left(12)), true);
                    } else {
                        LOG_DEBUG(TAG, QString("Order cancel failed: %1 — %2").arg(order_id, result.message)); // audited
                        self->order_entry_->show_status_message(
                            self->tr("Cancel of order %1 failed: %2")
                                .arg(order_id.left(12),
                                     result.message.isEmpty() ? self->tr("no reason given") : result.message),
                            false);
                    }
                    // The blotter lives at the broker (LIVE or native paper): pull it now
                    // instead of waiting for the 5-min poll (async, stream-owned).
                    DataStreamManager::instance().refresh_portfolio(acct_id);
                },
                Qt::QueuedConnection);
        });
    }
}

void EquityTradingScreen::on_ob_price_clicked(double price) {
    // Clicking a depth level is a "trade at THIS price" gesture — it must fill the
    // order ticket's limit price, not overwrite the LTP readout. Overwriting the
    // LTP made the ticket display a price that was never the last traded price,
    // which is exactly the kind of wrong number a trader must not be shown.
    order_entry_->set_limit_price(price);
}

void EquityTradingScreen::refresh_candles() {
    request_chart_history(); // broker stream or public feed, whichever serves the screen
}

void EquityTradingScreen::async_modify_order(const QString& order_id, double qty, double price) {
    // Local paper (incl. the built-in account): modify = cancel + re-place with the
    // new values in the simulator (pt_modify_order). Synchronous, no broker.
    {
        const auto account = AccountManager::instance().get_account(focused_account_id_);
        if (account.trading_mode == QLatin1String("paper") && !focused_native_paper_) {
            try {
                const PtOrder repl = pt_modify_order(order_id, qty, price);
                order_entry_->show_status_message(
                    tr("Paper order modified (re-placed as %1; it loses its time priority)").arg(repl.id.left(8)), true);
            } catch (const std::exception& e) {
                // The refusal is audited by the paper engine (Order modify rejected).
                LOG_WARN(TAG, logf("Paper order modify refused", {{"order_id", order_id}, {"error", e.what()}}));
                order_entry_->show_status_message(tr("Modify refused: %1").arg(QString::fromUtf8(e.what())), false);
            }
            PaperMarkService::instance().resync();
            refresh_paper_panels();
            return;
        }
    }
    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id, order_id, qty, price]() {
        if (!self)
            return;
        QJsonObject mods;
        // "quantity" is the only spelling any broker's modify_order reads — the
        // "qty" key that used to be sent alongside it was read by nothing.
        // UnifiedTrading::modify_order hydrates side/symbol/exchange/product/
        // order_type from the resting order, so this only carries the deltas.
        if (qty > 0)
            mods["quantity"] = qty;
        if (price > 0)
            mods["price"] = price;
        auto result = UnifiedTrading::instance().modify_order(acct_id, order_id, mods);
        if (!self)
            return; // QMetaObject::invokeMethod asserts on a null receiver
        QMetaObject::invokeMethod(
            self,
            [self, acct_id, order_id, result]() {
                if (!self)
                    return;
                // A rejected modify (filled, bad price, market closed, expired token) used to
                // be logged only: the dialog closed and the trader could not tell whether a
                // REAL order was changed. Say what happened and repaint the blotter.
                if (result.success) {
                    LOG_DEBUG(TAG, QString("Order modified: %1").arg(result.order_id)); // audited by UnifiedTrading
                    self->order_entry_->show_status_message(
                        self->tr("Order %1 modified").arg(order_id.left(12)), true);
                } else {
                    LOG_DEBUG(TAG, QString("Order modify failed: %1").arg(result.message)); // audited by UnifiedTrading
                    self->order_entry_->show_status_message(
                        self->tr("Modify of order %1 failed: %2")
                            .arg(order_id.left(12),
                                 result.message.isEmpty() ? self->tr("no reason given") : result.message),
                        false);
                }
                // Paper orders cannot be modified (the refusal above says so); a broker blotter
                // is pulled now instead of on the 5-min poll (async, stream-owned).
                DataStreamManager::instance().refresh_portfolio(acct_id);
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
    const bool local_paper = account.trading_mode == "paper" && !focused_native_paper_;
    focused_is_paper_ = local_paper;
    focused_paper_portfolio_id_ = local_paper ? account.paper_portfolio_id : QString();
    if (account.trading_mode == "paper" && focused_native_paper_) {
        // Native paper: the data is the broker's, delivered over the hub. Callers land here
        // after something changed (a close, a cancel, a basket…), so pull a fresh snapshot
        // now rather than waiting for the 5-min poll — async and stream-owned, never a
        // blocking REST call on the GUI thread.
        if (!focused_account_id_.isEmpty())
            DataStreamManager::instance().refresh_portfolio(focused_account_id_);
        return;
    }
    if (!local_paper || account.paper_portfolio_id.isEmpty())
        return; // live data flows from AccountDataStream via the hub
    // Persist any buffered tick prices first so the positions/funds snapshot below
    // reflects the latest LTP at this refresh/order moment, not a coalesce window ago.
    flush_paper_prices();
    try {
        const auto portfolio = pt_get_portfolio(account.paper_portfolio_id);
        const auto positions = pt_get_positions(account.paper_portfolio_id);
        const auto stats = pt_get_stats(account.paper_portfolio_id);
        // The paper book's ISO currency: every cash figure below is in it.
        const QString ccy = portfolio.currency.trimmed();
        account_currency_ = portfolio.currency;

        // Multi-currency books (built-in account): prices stay in each instrument's
        // currency; money is converted to the account currency with the engine's
        // per-symbol factor (pt_set_symbol_fx, kept current by PaperMarkService).
        // Without a rate yet the factor is NaN: the row's money shows "—" and every
        // total below leaves it out — never 1:1.
        QHash<QString, EquityBottomPanel::SymbolMeta> meta;
        for (const auto& p : positions) {
            EquityBottomPanel::SymbolMeta m;
            const auto d = quote_detail_.constFind(p.symbol);
            m.currency = d != quote_detail_.constEnd() && !d->currency.isEmpty() ? d->currency
                                                                               : instrument_of(p.symbol).currency;
            m.digits = d != quote_detail_.constEnd() ? d->price_digits : -1;
            m.fx = PaperMarkService::display_fx(account.paper_portfolio_id, p.symbol, m.currency, ccy);
            if (!focused_is_builtin() && !simulated_on_delayed_prices()) {
                m.currency.clear(); // single-currency broker paper: the panel's own format
                m.fx = 1.0;
            }
            meta.insert(p.symbol, m);
        }
        bottom_panel_->set_symbol_meta(meta);
        auto fx_of = [&meta](const QString& s) {
            const auto it = meta.constFind(s);
            return it == meta.constEnd() ? 1.0 : it->fx; // NaN = no rate yet
        };

        // Split exposure: CNC/delivery -> Holdings tab, MIS/NRML -> Positions tab.
        QVector<trading::PtPosition> intraday;
        QVector<trading::BrokerHolding> holdings;
        double used_margin = 0.0, mis_unrealized = 0.0;
        double holdings_value = 0.0, holdings_unreal = 0.0;
        int no_fx = 0; // positions with no rate to the account currency yet: left out of the totals
        QStringList pos_syms;
        for (const auto& p : positions) {
            pos_syms << p.symbol;
            const double fx = fx_of(p.symbol);
            if (product_is_delivery(p.product)) {
                trading::BrokerHolding h;
                h.symbol = p.symbol;
                h.quantity = p.quantity;
                h.avg_price = p.entry_price;
                h.ltp = p.current_price > 0.0 ? p.current_price : p.entry_price;
                h.invested_value = p.entry_price * p.quantity * fx;
                h.current_value = h.ltp * p.quantity * fx;
                // Sign P&L by side: a short position gains when price falls. Using the
                // long formula unconditionally inverted short-side P&L and corrupted
                // holdings_value/total_equity.
                h.pnl = (p.side.compare("short", Qt::CaseInsensitive) == 0) ? (h.invested_value - h.current_value)
                                                                            : (h.current_value - h.invested_value);
                h.pnl_pct = !std::isfinite(h.pnl)       ? std::numeric_limits<double>::quiet_NaN()
                            : h.invested_value > 0.0 ? (h.pnl / h.invested_value) * 100.0
                                                     : 0.0;
                holdings.push_back(h);
                if (std::isfinite(fx)) {
                    holdings_value += h.current_value;
                    holdings_unreal += h.pnl;
                } else {
                    ++no_fx;
                }
            } else {
                intraday.push_back(p);
                used_margin += p.held_margin; // cash: already the account currency
                if (std::isfinite(fx))
                    mis_unrealized += p.unrealized_pnl * fx; // instrument currency → account currency
                else
                    ++no_fx;
            }
        }
        const double unrealized = mis_unrealized + holdings_unreal;

        order_entry_->set_balance(portfolio.balance, ccy);
        bottom_panel_->set_currency(ccy); // before positions so the P&L summary formats in the account currency
        bottom_panel_->set_paper_positions(intraday);
        bottom_panel_->set_holdings(holdings);

        // Order book: the day shown by the date selector (default today, so each
        // session starts empty). Keep the picker in sync with the viewed day.
        bottom_panel_->set_orders_date(orders_view_day_);
        bottom_panel_->set_paper_orders(paper_orders_for_day(account.paper_portfolio_id, orders_view_day_));

        // ── Funds card view ──
        EquityFundsView fv;
        fv.is_paper = true;
        fv.currency = ccy;
        fv.fx_missing = no_fx;
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
        sv.currency = ccy;
        sv.fx_missing = no_fx;
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

        // Simulated fills, labelled as such (and with their price source).
        bottom_panel_->set_paper_trades(pt_get_trades(account.paper_portfolio_id, 200),
                                        simulated_on_delayed_prices() ? tr("SIMULATED \xC2\xB7 delayed public price")
                                                                      : tr("SIMULATED \xC2\xB7 broker price"));
        last_funds_view_ = fv;
        have_funds_view_ = true;
        update_status_bar();

        // Paper accounting trace (debug level: this runs on every mark refresh).
        LOG_DEBUG(TAG, QString("[paper-stats] acct=%1 opening=%2 balance=%3 used_margin=%4 holdings_value=%5 "
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
    if (account.trading_mode != "paper" || focused_native_paper_ || account.paper_portfolio_id.isEmpty()) {
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
        LOG_WARN(TAG, logf("Paper position convert failed", {{"position_id", position_id},
                                                             {"symbol", symbol},
                                                             {"product", new_product},
                                                             {"error", e.what()}}));
        order_entry_->show_order_status(tr("Convert failed: %1").arg(e.what()), false);
    }
    refresh_paper_panels();
}

void EquityTradingScreen::on_orders_day_changed(const QDate& day) {
    orders_view_day_ = day;
    auto account = AccountManager::instance().get_account(focused_account_id_);
    // The day filter belongs to the local paper order book; native-paper orders are the
    // broker's own list (hub feed), unaffected by it.
    if (account.trading_mode == "paper" && !focused_native_paper_ && !account.paper_portfolio_id.isEmpty())
        bottom_panel_->set_paper_orders(paper_orders_for_day(account.paper_portfolio_id, day));
}

QVector<trading::PtOrder> EquityTradingScreen::paper_orders_for_day(const QString& portfolio_id, const QDate& day) const {
    if (!focused_is_builtin())
        return pt_get_orders_for_day(portfolio_id, day); // the Indian-session (IST) book, as before
    // The built-in account trades every venue from the user's own clock: the orders
    // placed on that LOCAL day, plus every order still resting or queued (whenever
    // it was placed — a queued order must never vanish from the book).
    QVector<trading::PtOrder> out;
    for (const auto& o : pt_get_orders(portfolio_id)) {
        QDateTime t = QDateTime::fromString(o.created_at, Qt::ISODate);
        if (t.isValid())
            t.setTimeZone(QTimeZone::UTC);
        const bool resting = o.status == QLatin1String("pending") || o.status == QLatin1String("partial");
        if (resting || (t.isValid() && t.toLocalTime().date() == day))
            out.append(o);
    }
    return out;
}

void EquityTradingScreen::on_square_off_group(const QString& account_id, int sign,
                                              const QVector<trading::BrokerPosition>& targets) {
    if (account_id.isEmpty() || sign == 0)
        return;
    // The panel picked these rows from the same tick-updated P&L its ▲/▼ chips show and
    // listed them in its confirmation — close exactly those (the old re-selection here
    // used the last REST snapshot, up to 5 minutes old, and could close a different set).
    if (targets.isEmpty()) {
        order_entry_->show_status_message(
            tr("No %1 positions to square off").arg(sign > 0 ? tr("winning") : tr("losing")), false);
        return;
    }
    const bool is_live = AccountManager::instance().get_account(account_id).trading_mode == QLatin1String("live");

    if (account_id == focused_account_id_ && simulated_on_delayed_prices()) {
        int ok = 0, fail = 0;
        QStringList errors;
        for (const auto& t : targets) {
            QString msg;
            if (simulated_close(t.symbol, t.product_type, 0.0, &msg))
                ++ok;
            else
                ++fail, errors << msg;
        }
        QString msg = tr("Squared off %1 %2 position(s) (simulated)").arg(ok).arg(sign > 0 ? tr("winning") : tr("losing"));
        if (fail > 0)
            msg += tr(", %1 refused — %2").arg(fail).arg(errors.join(QStringLiteral("; ")));
        order_entry_->show_status_message(msg, fail == 0);
        refresh_paper_panels();
        return;
    }

    // Close each in a single background worker (sequential) to avoid a thread storm.
    const QString acct_id = account_id;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id, targets, sign, is_live]() {
        if (!self)
            return;
        int ok = 0, fail = 0;
        QStringList errors;
        for (const auto& t : targets) {
            const auto r = UnifiedTrading::instance().close_position(acct_id, t.symbol, t.exchange, t.product_type);
            if (r.success) {
                ++ok;
            } else {
                ++fail;
                errors << QString("%1: %2").arg(t.symbol, r.error);
            }
        }
        if (!self)
            return; // QMetaObject::invokeMethod asserts on a null receiver
        QMetaObject::invokeMethod(
            self,
            [self, acct_id, ok, fail, errors, sign, is_live]() {
                if (!self)
                    return;
                // The outcome used to be dropped entirely — report it.
                QString msg = self->tr("Squared off %1 %2 position(s)")
                                  .arg(ok)
                                  .arg(sign > 0 ? self->tr("winning") : self->tr("losing"));
                if (fail > 0)
                    msg += self->tr(", %1 failed — %2").arg(fail).arg(errors.join(QStringLiteral("; ")));
                self->order_entry_->show_status_message(msg, fail == 0);
                self->refresh_paper_panels(); // paper (local + broker venue) blotter
                if (is_live)
                    DataStreamManager::instance().refresh_portfolio(acct_id);
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_trade_symbol_requested(const QString& symbol, const QString& exchange,
                                                    const QString& product, bool is_buy, double qty) {
    // The row's own exchange + product (empty exchange: paper rows — the screen's market).
    open_order_ticket_for(symbol, exchange, product, is_buy, qty);
}

void EquityTradingScreen::open_order_ticket_for(const QString& symbol, const QString& exchange, const QString& product,
                                                bool is_buy, double qty, const QString& order_type,
                                                double limit_price) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    const QString exch = exchange.isEmpty() ? selected_exchange_ : exchange;
    const double ref_price = ref_price_for(symbol); // THIS symbol's price, never another one's

    // The row's product as THIS broker means it (Angel One "MARGIN" is MTF, not NRML) —
    // the same mapping the broker close paths use. The generic table sent such a row as
    // NRML, and any unrecognised spelling as MIS (a new intraday position, not an exit).
    // The resolved product is what the header shows and what is sent.
    ProductType product_type = product_from_broker_str(product);
    if (auto* broker = BrokerRegistry::instance().get(broker_id_for_focused()))
        product_type = broker->product_from_wire(product);
    QString product_label = QString::fromLatin1(product_to_broker_str(product_type));
    if (!product.trimmed().isEmpty() && product.trimmed().compare(product_label, Qt::CaseInsensitive) != 0)
        product_label += QStringLiteral(" (%1)").arg(product.trimmed().toUpper());

    QDialog dlg(this);
    dlg.setWindowTitle(is_buy ? tr("Buy %1").arg(symbol) : tr("Sell %1").arg(symbol));
    auto* form = new QFormLayout(&dlg);

    auto* hdr = new QLabel(QStringLiteral("%1  %2 · %3 · %4")
                               .arg(is_buy ? tr("BUY") : tr("SELL"),
                                    canonical_mode() ? display_of(symbol).section(QStringLiteral(" · "), 0, 0) : symbol,
                                    exch, product_label),
                           &dlg);
    hdr->setStyleSheet(QString("font-weight:700;font-size:" + fincept::ui::fonts::px(-1) + "px;color:%1;")
                           .arg(is_buy ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    form->addRow(hdr);

    auto* type_combo = new QComboBox(&dlg);
    type_combo->addItems({tr("Market"), tr("Limit")});
    // Prefilled order type: "limit" -> Limit, "market" / "" -> Market. Anything else is
    // not offered here: the ticket says so (below) instead of guessing.
    const QString want_type = order_type.trimmed().toLower();
    const bool type_unknown = !want_type.isEmpty() && want_type != QLatin1String("market") &&
                              want_type != QLatin1String("limit");
    type_combo->setCurrentIndex(want_type == QLatin1String("limit") ? 1 : 0);
    form->addRow(tr("Type"), type_combo);

    // Lot / tick of THIS instrument, to check a value as if it had been typed. Paper on
    // public data: the venue's lot rule (cash equities); a broker: its instrument master
    // (lot always; tick only as advice — a master's default tick must never block).
    double lot = 1.0;
    bool lot_known = false;
    double tick = 0.0;
    if (canonical_mode()) {
        const auto tm = eq_trading_meta(instrument_of(symbol));
        lot_known = tm.lot_known && tm.lot_size > 1.0;
        lot = lot_known ? tm.lot_size : 1.0;
        if (tm.tick_known && tm.price_tick > 0.0)
            tick = tm.price_tick;
    } else {
        const QString bid = broker_id_for_focused();
        auto& svc = trading::InstrumentService::instance();
        if (!bid.isEmpty() && svc.is_loaded(bid)) {
            if (const auto bi = svc.find(symbol.toUpper(), exch.toUpper(), bid)) {
                lot_known = bi->lot_size > 1;
                lot = lot_known ? double(bi->lot_size) : 1.0;
                if (bi->tick_size > 0.0)
                    tick = bi->tick_size;
            }
        }
    }

    // Pre-fill with the held quantity on a reduce/exit (qty > 0); otherwise default
    // to 1. Lets "Sell" exit the whole position in one click, like the FNO ticket.
    // UNROUNDED: an integer spin box pre-filled a 0.5-share holding as 1 (an oversell).
    // Whole quantities keep 0 decimals; a fractional one shows as many as it carries.
    int qty_decimals = 0;
    if (qty > 0.0 && std::isfinite(qty)) {
        while (qty_decimals < 9) {
            const double scaled = qty * std::pow(10.0, qty_decimals);
            if (std::fabs(scaled - std::round(scaled)) < 1e-6)
                break;
            ++qty_decimals;
        }
    }
    // The range never clamps a prefilled value: an out-of-range one is shown as it is
    // and the check below says why it cannot be sent.
    constexpr double kEqhMaxQty = 1.0e7;
    const double qty_in = qty > 0.0 && std::isfinite(qty) ? qty : 0.0;
    auto* qty_spin = new QDoubleSpinBox(&dlg);
    qty_spin->setDecimals(qty_decimals);
    qty_spin->setRange(0.0, std::max(kEqhMaxQty, qty_in));
    qty_spin->setValue(qty_in > 0.0 ? qty_in : std::max(1.0, lot));
    form->addRow(tr("Qty"), qty_spin);

    auto* px_spin = new QDoubleSpinBox(&dlg);
    {
        int digits = std::clamp(quote_detail_.value(symbol).price_digits, 2, 6);
        // A prefilled limit keeps every decimal it carries (no silent rounding).
        if (limit_price > 0.0 && std::isfinite(limit_price)) {
            while (digits < 9 && std::fabs(limit_price * std::pow(10.0, digits) -
                                           std::round(limit_price * std::pow(10.0, digits))) > 1e-6)
                ++digits;
        }
        px_spin->setDecimals(digits);
        px_spin->setRange(0.0, std::max(1.0e9, limit_price));
    }
    const bool limit_in = limit_price > 0.0 && std::isfinite(limit_price);
    px_spin->setValue(limit_in ? limit_price : (ref_price > 0.0 ? ref_price : 0.01));
    form->addRow(tr("Limit price"), px_spin);
    if (simulated_on_delayed_prices()) {
        auto* sim = new QLabel(tr("Paper: a SIMULATED fill on delayed public prices — nothing reaches a broker."), &dlg);
        sim->setWordWrap(true);
        form->addRow(sim);
    }

    // Inline check, the same for a prefilled and a typed value: hard problems keep the
    // button disabled; a tick mismatch is advice only (the broker decides).
    auto* check = new QLabel(&dlg);
    check->setWordWrap(true);
    check->setStyleSheet(QString("color:%1;").arg(fincept::ui::colors::WARNING()));
    check->hide();
    form->addRow(check);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton* ok = buttons->addButton(is_buy ? tr("Buy") : tr("Sell"), QDialogButtonBox::AcceptRole);
    ok->setProperty("fincept_agent_risk", "order");
    ok->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    auto validate = [=]() {
        QStringList hard;
        QStringList advice;
        const double q = qty_spin->value();
        if (type_unknown)
            hard << tr("Order type \"%1\" is not available on this ticket \xE2\x80\x94 pick Market or Limit.")
                        .arg(order_type.trimmed());
        if (!(q > 0.0))
            hard << tr("Enter a quantity.");
        else if (q > kEqhMaxQty)
            hard << tr("Quantity %1 is above this ticket's limit (%2).").arg(format_quantity(q), format_quantity(kEqhMaxQty));
        else if (lot_known && q < lot)
            hard << tr("The minimum is one lot: %1.").arg(format_quantity(lot));
        else if (lot_known && std::fabs(q / lot - std::round(q / lot)) > 1e-9)
            hard << tr("Quantity must be a multiple of the lot size (%1).").arg(format_quantity(lot));
        if (type_combo->currentIndex() == 1) {
            const double px = px_spin->value();
            if (!(px > 0.0))
                hard << tr("Enter a limit price.");
            else if (tick > 0.0 && std::fabs(px / tick - std::round(px / tick)) > 1e-6)
                advice << tr("The limit is not a multiple of the tick (%1); the broker may reject or round it.")
                              .arg(QString::number(tick, 'g', 10));
        }
        const QStringList all = hard + advice;
        check->setText(all.join(QLatin1Char('\n')));
        check->setVisible(!all.isEmpty());
        ok->setEnabled(hard.isEmpty());
    };
    auto sync_type = [form, px_spin, validate](int idx) {
        form->setRowVisible(px_spin, idx == 1); // 1 = Limit
        validate();
    };
    connect(type_combo, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, sync_type);
    connect(qty_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, [validate](double) { validate(); });
    connect(px_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, [validate](double) { validate(); });
    sync_type(type_combo->currentIndex());
    // The ticket opens with the quantity focused: the trader reviews, then presses BUY / SELL.
    qty_spin->setFocus(Qt::OtherFocusReason);
    qty_spin->selectAll();

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
    order.product_type = product_type; // the product the header showed
    order.validity = QStringLiteral("DAY");
    if (simulated_on_delayed_prices())
        place_simulated_order(order, /*confirm=*/false); // this ticket was the confirmation
    else
        on_order_submitted(order);
}

double EquityTradingScreen::ref_price_for(const QString& symbol) const {
    if (symbol.isEmpty())
        return 0.0;
    // Delayed-price paper: the newest delayed quote of THAT symbol (a reference for the
    // ticket; the fill price itself is decided by pt_submit_delayed_order).
    if (simulated_on_delayed_prices()) {
        const double px = quote_detail_.value(symbol).price;
        return std::isfinite(px) && px > 0.0 ? px : 0.0;
    }
    if (symbol == selected_symbol_)
        return current_price_ > 0.0 ? current_price_ : 0.0;
    // Another symbol (a Positions / Holdings row): the focused broker stream's last
    // quote for it — held symbols are subscribed. Never on the public fallback (a
    // delayed quote must never become a paper fill price or the confirm's reference).
    if (public_mode_ || focused_account_id_.isEmpty())
        return 0.0;
    if (auto* stream = DataStreamManager::instance().stream_for(focused_account_id_)) {
        const double ltp = stream->cached_quote(symbol).ltp;
        if (std::isfinite(ltp) && ltp > 0.0)
            return ltp;
    }
    return 0.0;
}

void EquityTradingScreen::refresh_after_bulk_orders(const QStringList& account_ids) {
    for (const auto& id : account_ids)
        if (!id.isEmpty())
            DataStreamManager::instance().refresh_portfolio(id);
    // The focused account's blotter: local paper re-reads the engine, a broker paper venue
    // pulls the broker; LIVE is pulled above when it was a target, else here.
    refresh_paper_panels();
    if (!focused_account_id_.isEmpty() && !account_ids.contains(focused_account_id_) &&
        AccountManager::instance().get_account(focused_account_id_).trading_mode == QLatin1String("live"))
        DataStreamManager::instance().refresh_portfolio(focused_account_id_);
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
                                                     double ref_price, double quantity, const QString& order_type,
                                                     double limit_price) {
    if (inert_) {
        // The panel's real screen owns the ticket (EquitySingleInstance.h).
        if (primary_)
            primary_->open_external_order_ticket(symbol, exchange, match_exchanges, is_buy, ref_price, quantity,
                                                 order_type, limit_price);
        return;
    }
    // Prefill only (never submitted here); a non-finite or negative value means "default".
    // Out-of-rule values (lot, tick, minimum) are not corrected: the ticket says why.
    const double prefill_qty = std::isfinite(quantity) && quantity > 0.0 ? quantity : 0.0;
    const double prefill_limit = std::isfinite(limit_price) && limit_price > 0.0 ? limit_price : 0.0;
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
    if (target.isEmpty() || is_builtin_paper_account(target)) {
        // No broker can trade it: the built-in paper account can — a simulated order on
        // delayed public prices (the ticket and its confirmation say so).
        const auto inst = eq_instrument_for_stored(symbol, exchange);
        if (inst.symbol.isEmpty()) {
            QMessageBox::information(this, tr("Trade %1").arg(symbol),
                                     tr("No connected broker can trade %1, and it has no public price for a paper "
                                        "trade. Add or connect a broker for this market in ACCOUNTS.")
                                         .arg(symbol));
            return;
        }
        if (!focused_is_builtin())
            on_account_changed(QString::fromLatin1(kBuiltinPaperAccountId));
        instruments_.insert(inst.symbol, inst);
        on_symbol_selected(inst.symbol);
        open_order_ticket_for(inst.symbol, inst.venue, QStringLiteral("CNC"), is_buy, prefill_qty, order_type,
                              prefill_limit);
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
    open_order_ticket_for(symbol, selected_exchange_, QStringLiteral("CNC"), is_buy, prefill_qty, order_type,
                          prefill_limit);

    selected_symbol_ = prev_symbol;
    selected_exchange_ = prev_exchange;
    current_price_ = prev_price;
}

void EquityTradingScreen::on_chart_exit_position(const QString& symbol, const QString& exchange,
                                                 const QString& product_type, const QString& side, double qty) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    const QString side_disp = side.compare("short", Qt::CaseInsensitive) == 0 ? tr("SHORT") : tr("LONG");
    const auto reply = QMessageBox::question(
        this, tr("Exit Position"), tr("Exit %1 %2 %3 at market?").arg(side_disp).arg(qty, 0, 'f', 0).arg(symbol),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    if (simulated_on_delayed_prices()) {
        QString msg;
        const bool ok = simulated_close(symbol, product_type, qty, &msg);
        order_entry_->show_order_status(msg, ok);
        refresh_paper_panels();
        return;
    }

    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id, symbol, exchange, product_type]() {
        if (!self)
            return;
        auto result = UnifiedTrading::instance().close_position(acct_id, symbol, exchange, product_type);
        QMetaObject::invokeMethod(
            self,
            [self, result, symbol]() {
                if (!self)
                    return;
                self->order_entry_->show_order_status(
                    result.success ? self->tr("Exit order placed: %1").arg(symbol) : result.error, result.success);
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
    (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id]() {
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
                // Local paper: the simulator cancelled the rows; the matcher must drop them
                // too (it would otherwise still try to fill them). Re-register what is left.
                if (!self->focused_paper_portfolio_id_.isEmpty()) {
                    const QString pid = self->focused_paper_portfolio_id_;
                    OrderMatcher::instance().clear_portfolio_orders(pid);
                    for (const auto& o : pt_get_orders(pid, QStringLiteral("pending")))
                        OrderMatcher::instance().add_order(o);
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
    if (account.trading_mode == "paper" && !focused_native_paper_ && !account.paper_portfolio_id.isEmpty()) {
        for (const auto& p : pt_get_positions(account.paper_portfolio_id)) {
            if (p.quantity == 0.0 || product_is_delivery(p.product))
                continue; // CNC/delivery -> Holdings, not squared here
            targets.push_back({p.symbol, QString(), p.product});
        }
    } else {
        // live_positions_ is the Positions tab feed (broker get_positions);
        // Holdings come from a separate holdings feed and are not included.
        // Squared-off rows (Zerodha & co. keep them in the net list with qty 0) are not
        // open: the broker close answers "Position not found" for them, which used to
        // end a fully successful SQUARE OFF ALL in red "M failed".
        for (const auto& p : live_positions_)
            if (p.quantity != 0.0)
                targets.push_back({p.symbol, p.exchange, p.product_type});
    }
    if (targets.isEmpty()) {
        order_entry_->show_order_status(tr("No open positions to square off"), false);
        return;
    }

    if (simulated_on_delayed_prices()) {
        // Delayed-price paper: each close fills at the delayed price while its venue is
        // open, else queues for the open (simulated_close). Local, synchronous.
        int ok = 0, fail = 0;
        QStringList notes;
        for (const auto& t : targets) {
            QString msg;
            simulated_close(t.symbol, t.product, 0.0, &msg) ? ++ok : ++fail;
            notes << msg;
        }
        order_entry_->show_order_status(tr("Square off: %1 done / queued%2 \xE2\x80\x94 %3")
                                            .arg(ok)
                                            .arg(fail > 0 ? tr(", %1 refused").arg(fail) : QString())
                                            .arg(notes.join(QStringLiteral("; "))),
                                        fail == 0);
        refresh_paper_panels();
        return;
    }

    const QString acct_id = focused_account_id_;
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id, targets]() {
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
    LOG_INFO(TAG, logf("Holdings square-off requested", {{"account", log_mask(focused_account_id_)},
                                                         {"holdings", static_cast<int>(holdings.size())}}));
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    // The Holdings tab already confirmed via dialog (listing each holding + quantity).
    // Exit each through UnifiedTrading::close_holding(): close_position() only scans the
    // broker's POSITIONS book, which never holds demat holdings, so on a live Indian
    // broker every holding answered "Position not found" and nothing was sold.
    // close_holding re-reads the broker's holdings and sends one delivery MARKET SELL of
    // exactly the quantity shown (refusing if the holding shrank or vanished); local
    // paper keeps the simulator's close path. Touches ONLY holdings.
    struct Target {
        QString symbol;
        QString exchange;
        double quantity = 0.0;
    };
    QString default_exchange;
    if (auto* broker = BrokerRegistry::instance().get(broker_id_for_focused()))
        default_exchange = broker->profile().default_exchange;
    QVector<Target> targets;
    for (const auto& h : holdings)
        if (h.quantity > 0)
            targets.push_back({h.symbol, h.exchange.isEmpty() ? default_exchange : h.exchange, h.quantity});
    if (targets.isEmpty()) {
        order_entry_->show_status_message(tr("No holdings to square off"), false);
        return;
    }

    if (simulated_on_delayed_prices()) {
        int ok = 0, fail = 0;
        QStringList notes;
        for (const auto& t : targets) {
            QString msg;
            simulated_close(t.symbol, QStringLiteral("CNC"), t.quantity, &msg) ? ++ok : ++fail;
            notes << msg;
        }
        order_entry_->show_status_message(tr("Holdings: %1 done / queued%2 \xE2\x80\x94 %3")
                                              .arg(ok)
                                              .arg(fail > 0 ? tr(", %1 refused").arg(fail) : QString())
                                              .arg(notes.join(QStringLiteral("; "))),
                                          fail == 0);
        refresh_paper_panels();
        return;
    }

    const QString acct_id = focused_account_id_;
    const bool is_live = AccountManager::instance().get_account(acct_id).trading_mode == QLatin1String("live");
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id, targets, is_live]() {
        if (!self)
            return;
        int ok = 0, fail = 0;
        QStringList errors;
        for (const auto& t : targets) {
            auto r = UnifiedTrading::instance().close_holding(acct_id, t.symbol, t.exchange, t.quantity);
            // Each exit is audited (Order submitted / accepted / rejected, Holding exit refused).
            LOG_DEBUG(TAG, logf("Holding exit finished", {{"symbol", t.symbol},
                                                          {"exchange", t.exchange},
                                                          {"qty", t.quantity},
                                                          {"ok", r.success},
                                                          {"error", r.error}}));
            if (r.success) {
                ++ok;
            } else {
                ++fail;
                errors << QString("%1: %2").arg(t.symbol, r.error);
            }
        }
        LOG_INFO(TAG, logf("Holdings square-off finished", {{"account", log_mask(acct_id)}, {"ok", ok}, {"failed", fail}}));
        if (!self)
            return; // QMetaObject::invokeMethod asserts on a null receiver
        QMetaObject::invokeMethod(
            self,
            [self, acct_id, ok, fail, errors, is_live]() {
                if (!self)
                    return;
                self->order_entry_->show_status_message(
                    self->tr("Squared off %1 holding(s)%2")
                        .arg(ok)
                        .arg(fail > 0 ? self->tr(", %1 failed — %2").arg(fail).arg(errors.join(QStringLiteral("; ")))
                                      : QString()),
                    fail == 0);
                self->refresh_paper_panels(); // paper (local + broker venue) blotter
                if (is_live)
                    DataStreamManager::instance().refresh_portfolio(acct_id);
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::on_square_off_holding(const QString& symbol, const QString& exchange, double quantity) {
    if (focused_account_id_.isEmpty()) {
        order_entry_->show_order_status(tr("No account selected — add one via ACCOUNTS"), false);
        return;
    }
    if (!(quantity > 0.0) || !std::isfinite(quantity)) {
        order_entry_->show_status_message(tr("%1: no quantity to sell — refresh holdings").arg(symbol), false);
        return;
    }
    // The holding's exchange; the broker default when the adapter reports none (Alpaca).
    QString exch = exchange;
    if (exch.isEmpty()) {
        if (auto* broker = BrokerRegistry::instance().get(broker_id_for_focused()))
            exch = broker->profile().default_exchange;
    }
    // Name exactly what will be sent: side, quantity, symbol, exchange, product, type.
    const auto reply = QMessageBox::question(
        this, tr("Square Off Holding"),
        tr("Place a MARKET SELL of %1 × %2 (%3, delivery / CNC) to exit this holding?")
            .arg(format_quantity(quantity), symbol, exch.isEmpty() ? tr("broker default") : exch),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    if (simulated_on_delayed_prices()) {
        QString msg;
        const bool ok = simulated_close(symbol, QStringLiteral("CNC"), quantity, &msg);
        order_entry_->show_status_message(msg, ok);
        refresh_paper_panels();
        return;
    }

    const QString acct_id = focused_account_id_;
    const bool is_live = AccountManager::instance().get_account(acct_id).trading_mode == QLatin1String("live");
    QPointer<EquityTradingScreen> self = this;
    (void)QtConcurrent::run(&order_thread_pool(), [self, acct_id, symbol, exch, quantity, is_live]() {
        if (!self)
            return;
        // close_holding, not close_position: the latter only scans the positions book and
        // answered "Position not found" for every demat holding on a live Indian broker.
        auto r = UnifiedTrading::instance().close_holding(acct_id, symbol, exch, quantity);
        // The exit is audited (Order submitted / accepted / rejected, Holding exit refused).
        LOG_DEBUG(TAG, logf("Holding exit finished", {{"symbol", symbol},
                                                      {"exchange", exch},
                                                      {"qty", quantity},
                                                      {"ok", r.success},
                                                      {"error", r.error}}));
        if (!self)
            return; // QMetaObject::invokeMethod asserts on a null receiver
        QMetaObject::invokeMethod(
            self,
            [self, r, symbol, acct_id, is_live]() {
                if (!self)
                    return;
                self->order_entry_->show_status_message(
                    r.success ? self->tr("Exit order placed for holding %1").arg(symbol) : r.error, r.success);
                self->refresh_paper_panels();
                if (is_live)
                    DataStreamManager::instance().refresh_portfolio(acct_id);
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
    // rather than confirming inline. Auto mode falls straight through — but a
    // LIVE account always gets a read-back of every leg first (T1).
    if (!ActionCenter::instance().should_queue(acct_id, "basketorder") && !confirm_live_basket(acct_id, basket)) {
        order_entry_->show_order_status(tr("Strategy cancelled — nothing was sent"), false);
        return;
    }
    // Provenance of every leg, captured after the read-back (a person who confirmed it
    // is outside any agent step).
    trading::BasketOrderRequest sourced = basket;
    for (auto& leg : sourced.orders)
        leg.source = capture_order_source(QStringLiteral("basket"));
    if (ActionCenter::instance().should_queue(acct_id, "basketorder")) {
        const QString pending_id =
            ActionCenter::instance().queue_order(acct_id, "basketorder", ActionCenter::serialize_basket_order(sourced));
        order_entry_->show_order_status(pending_id.isEmpty()
                                            ? tr("Failed to queue strategy")
                                            : tr("Strategy queued for approval (%1 legs)").arg(basket.orders.size()),
                                        !pending_id.isEmpty());
        return;
    }

    QPointer<EquityTradingScreen> self = this;
    const QString strategy_name = basket.strategy_name;
    // place_basket_orders runs async on its own worker and delivers the result
    // back on the caller's thread; marshal to the UI thread defensively.
    UnifiedTrading::instance().place_basket_orders(
        acct_id, sourced, [self, acct_id, strategy_name](const trading::BasketOrderResult& result) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, acct_id, strategy_name, result]() {
                    if (!self)
                        return;
                    self->order_entry_->show_order_status(
                        self->tr("Strategy: %1/%2 legs placed%3")
                            .arg(result.successful)
                            .arg(result.total)
                            .arg(result.failed > 0 ? self->tr(", %1 failed").arg(result.failed) : QString()),
                        result.failed == 0);
                    self->refresh_paper_panels();
                    // Partial fill of a multi-leg strategy = unhedged exposure.
                    if (result.failed > 0 && result.successful > 0)
                        self->offer_basket_unwind(acct_id, strategy_name, result);
                },
                Qt::QueuedConnection);
        });
}

bool EquityTradingScreen::confirm_live_basket(const QString& account_id, const trading::BasketOrderRequest& basket) {
    const auto account = AccountManager::instance().get_account(account_id);
    if (account.trading_mode != QLatin1String("live"))
        return true; // paper: unchanged behaviour
    QStringList legs;
    const auto* basket_broker = BrokerRegistry::instance().get(account.broker_id);
    const QString acct_ccy = basket_broker ? basket_broker->profile().currency : QString();
    for (const auto& o : basket.orders) {
        // One multi-value call: six markers, six values, nothing re-scanned (the price used to
        // be dropped — the template had five markers).
        legs << QString("  %1  %2 × %3  %4  %5  @ %6")
                    .arg(QString::fromLatin1(order_side_str(o.side)).toUpper(), format_quantity(o.quantity), o.symbol,
                         o.exchange, QString::fromLatin1(product_to_broker_str(o.product_type)),
                         o.order_type == OrderType::Market ? tr("MARKET")
                                                           : eqh_order_px(o.price, o.symbol, o.exchange, acct_ccy));
    }
    const QString name = basket.strategy_name.isEmpty() ? tr("Strategy") : basket.strategy_name;
    const QString body = tr("LIVE ORDER — real money.\n\n%1 on %2:\n\n%3\n\n"
                            "The legs are sent one after another — this is NOT atomic. If a leg is rejected, the "
                            "legs already sent stay open and you will be offered an unwind.")
                             .arg(name, account.display_name.isEmpty() ? account_id : account.display_name,
                                  legs.join(QLatin1Char('\n')));
    QMessageBox box(this);
    box.setWindowTitle(tr("Confirm Strategy"));
    box.setIcon(QMessageBox::Warning);
    box.setText(body);
    auto* send = box.addButton(tr("Send %1 legs").arg(basket.orders.size()), QMessageBox::AcceptRole);
    send->setProperty("fincept_agent_risk", "order");
    auto* cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(cancel); // Enter never sends a live strategy
    box.exec();
    return box.clickedButton() == send;
}

void EquityTradingScreen::offer_basket_unwind(const QString& account_id, const QString& strategy_name,
                                              const trading::BasketOrderResult& result) {
    QStringList lines;
    for (const auto& r : result.results) {
        lines << (r.success ? tr("  ✓ %1 %2 — placed (order %3)").arg(r.symbol, r.exchange, r.order_id)
                            : tr("  ✗ %1 %2 — FAILED: %3").arg(r.symbol, r.exchange, r.error));
    }
    QMessageBox box(this);
    box.setWindowTitle(tr("Strategy Partially Placed"));
    box.setIcon(QMessageBox::Critical);
    box.setText(tr("%1: only %2 of %3 legs were placed. The placed legs are now UNHEDGED.\n\n%4\n\n"
                   "Unwind cancels any unfilled remainder of the placed legs and closes what filled with "
                   "opposite MARKET orders (same product). Fills are read from the broker's order book; a leg "
                   "whose fill cannot be verified is reported, never guessed.")
                    .arg(strategy_name.isEmpty() ? tr("Strategy") : strategy_name)
                    .arg(result.successful)
                    .arg(result.total)
                    .arg(lines.join(QLatin1Char('\n'))));
    auto* unwind = box.addButton(tr("Unwind placed legs"), QMessageBox::DestructiveRole);
    unwind->setProperty("fincept_agent_risk", "order");
    auto* keep = box.addButton(tr("Keep positions"), QMessageBox::RejectRole);
    box.setDefaultButton(keep);
    box.exec();
    if (box.clickedButton() != unwind)
        return;

    order_entry_->show_order_status(tr("Unwinding %1 placed leg(s)…").arg(result.successful), true);
    QPointer<EquityTradingScreen> self = this;
    const QVector<trading::BasketOrderResult::OrderResult> legs = result.results;
    (void)QtConcurrent::run(&order_thread_pool(), [self, account_id, legs]() {
        const auto out = UnifiedTrading::instance().unwind_basket_legs(account_id, legs);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, out]() {
                if (!self)
                    return;
                int ok = 0;
                QStringList report;
                for (const auto& u : out) {
                    if (u.ok)
                        ++ok;
                    report << (u.ok ? tr("  ✓ %1 — %2").arg(u.symbol, u.action)
                                    : tr("  ✗ %1 — %2%3")
                                          .arg(u.symbol, u.error,
                                               u.action.isEmpty() ? QString() : QStringLiteral(" (") + u.action + ")"));
                }
                const bool all_ok = (ok == out.size());
                self->order_entry_->show_order_status(
                    tr("Unwind: %1/%2 legs done").arg(ok).arg(out.size()), all_ok);
                QMessageBox::information(self, tr("Unwind Report"),
                                         (all_ok ? tr("All placed legs were unwound.\n\n")
                                                 : tr("Some legs could NOT be unwound — check them in the broker's "
                                                      "terminal now.\n\n")) +
                                             report.join(QLatin1Char('\n')));
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
