// Equity Trading Screen — multi-account coordinator
#include "screens/equity_trading/EquityTradingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"

#include <QApplication>
#include <QMouseEvent>
#include "screens/equity_trading/AccountManagementDialog.h"
#include "screens/equity_trading/BroadcastOrderDialog.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChart.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "services/portfolio/PortfolioService.h"
#include "services/workspace/WorkspaceManager.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/UnifiedTrading.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

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

// ============================================================================
// Constructor / Destructor
// ============================================================================

EquityTradingScreen::EquityTradingScreen(QWidget* parent) : QWidget(parent) {
    watchlist_symbols_ = DEFAULT_WATCHLIST;
    setup_ui();
    setup_timers();
    connect_data_stream_signals();
    WorkspaceManager::instance().register_participant("equity_trading", this);

    // Accept symbol drops anywhere on the screen — route through the
    // existing selection path so sub-panels resync and the linked group is
    // published to.
    symbol_dnd::installDropFilter(this, [this](const SymbolRef& ref, SymbolGroup) {
        if (ref.is_valid())
            on_symbol_selected(ref.symbol);
    });
}

EquityTradingScreen::~EquityTradingScreen() {
    WorkspaceManager::instance().unregister_participant("equity_trading");
    if (fill_cb_id_ >= 0)
        OrderMatcher::instance().remove_fill_callback(fill_cb_id_);
}

QJsonObject EquityTradingScreen::save_state() const {
    QJsonObject s;
    s["focused_account_id"] = focused_account_id_;
    s["symbol"] = selected_symbol_;
    s["exchange"] = selected_exchange_;
    QJsonArray wl;
    for (const auto& sym : watchlist_symbols_)
        wl.append(sym);
    s["watchlist_symbols"] = QJsonValue(wl);
    return s;
}

void EquityTradingScreen::restore_state(const QJsonObject& state) {
    if (state.contains("focused_account_id"))
        focused_account_id_ = state["focused_account_id"].toString();
    // Legacy migration: old state used broker_id
    if (focused_account_id_.isEmpty() && state.contains("broker_id")) {
        const QString old_broker = state["broker_id"].toString();
        auto accounts = AccountManager::instance().list_accounts(old_broker);
        if (!accounts.isEmpty())
            focused_account_id_ = accounts.first().account_id;
    }
    if (state.contains("symbol"))
        selected_symbol_ = state["symbol"].toString();
    if (state.contains("exchange"))
        selected_exchange_ = state["exchange"].toString();
    if (state.contains("watchlist_symbols")) {
        watchlist_symbols_.clear();
        for (const auto& v : state["watchlist_symbols"].toArray())
            watchlist_symbols_.append(v.toString());
    }
}

// ============================================================================
// Visibility — start/stop data streams (P3)
// ============================================================================

void EquityTradingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    // Restore persisted session state on first show
    if (!initialized_) {
        const auto s = ScreenStateManager::instance().load_direct("equity_trading");
        if (!s.isEmpty()) {
            const QString acct = s.value("focused_account_id").toString();
            const QString sym = s.value("selected_symbol", selected_symbol_).toString();
            if (!acct.isEmpty() && acct != focused_account_id_ && AccountManager::instance().has_account(acct))
                on_account_changed(acct);
            if (sym != selected_symbol_)
                on_symbol_selected(sym);
        }
    }

    // Start all active account data streams
    DataStreamManager::instance().start_all_active();
    DataStreamManager::instance().resume_all();

    // UI-local timers
    if (clock_timer_)
        clock_timer_->start();
    if (market_clock_timer_)
        market_clock_timer_->start();

    if (!initialized_) {
        init_focused_account();
        initialized_ = true;
    }

    LOG_INFO(TAG, "Screen visible — data streams resumed");
}

void EquityTradingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    DataStreamManager::instance().pause_all();
    if (clock_timer_)
        clock_timer_->stop();
    if (market_clock_timer_)
        market_clock_timer_->stop();

    ScreenStateManager::instance().save_direct("equity_trading", {
        {"focused_account_id", focused_account_id_},
        {"selected_symbol", selected_symbol_},
        {"selected_exchange", selected_exchange_},
    });
    LOG_INFO(TAG, "Screen hidden — data streams paused");
}

// ============================================================================
// UI Setup — 4-zone layout
// ============================================================================

void EquityTradingScreen::setup_ui() {
    setObjectName("eqScreen");
    setStyleSheet(ui::styles::equity_trading_styles());

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // ── COMMAND BAR (34px) ────────────────────────────────────────────────────
    auto* cmd_bar = new QWidget(this);
    cmd_bar->setObjectName("eqCommandBar");
    cmd_bar->setFixedHeight(34);
    auto* cmd_layout = new QHBoxLayout(cmd_bar);
    cmd_layout->setContentsMargins(8, 0, 8, 0);
    cmd_layout->setSpacing(6);

    // Account button + menu (replaces broker_btn_)
    account_btn_ = new QPushButton("NO ACCOUNT");
    account_btn_->setObjectName("eqBrokerBtn");
    account_btn_->setFixedHeight(22);
    account_btn_->setCursor(Qt::PointingHandCursor);
    account_menu_ = new QMenu(account_btn_);
    account_btn_->setMenu(account_menu_);
    cmd_layout->addWidget(account_btn_);

    // Separator
    auto* sep = new QLabel("|");
    sep->setObjectName("eqCommandBarSep");
    cmd_layout->addWidget(sep);

    // Exchange label
    exchange_label_ = new QLabel("NSE");
    exchange_label_->setObjectName("eqExchangeLabel");
    cmd_layout->addWidget(exchange_label_);

    // Separator
    auto* sep2 = new QLabel("|");
    sep2->setObjectName("eqCommandBarSep");
    cmd_layout->addWidget(sep2);

    // Symbol input
    symbol_input_ = new QLineEdit("RELIANCE");
    symbol_input_->setObjectName("eqSymbolInput");
    symbol_input_->setFixedWidth(120);
    symbol_input_->setFixedHeight(22);
    auto* completer = new QCompleter(watchlist_symbols_, symbol_input_);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    symbol_input_->setCompleter(completer);
    connect(symbol_input_, &QLineEdit::returnPressed, this,
            [this]() { on_symbol_selected(symbol_input_->text().trimmed().toUpper()); });
    cmd_layout->addWidget(symbol_input_);

    // Separator
    auto* sep3 = new QLabel("|");
    sep3->setObjectName("eqCommandBarSep");
    cmd_layout->addWidget(sep3);

    // Price ribbon
    ticker_bar_ = new EquityTickerBar;
    cmd_layout->addWidget(ticker_bar_, 1);

    // Clock
    clock_label_ = new QLabel("--:--:--");
    clock_label_->setObjectName("eqClock");
    cmd_layout->addWidget(clock_label_);

    // Connection status indicator (aggregate)
    conn_label_ = new QLabel("○ NO ACCOUNTS");
    conn_label_->setObjectName("eqConnLabel");
    conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::TEXT_TERTIARY()));
    cmd_layout->addWidget(conn_label_);

    // Accounts management button (replaces api_btn_)
    accounts_btn_ = new QPushButton("ACCOUNTS");
    accounts_btn_->setObjectName("eqApiBtn");
    accounts_btn_->setFixedHeight(22);
    accounts_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(accounts_btn_);

    // Mode button (per-account mode toggle)
    mode_btn_ = new QPushButton("PAPER");
    mode_btn_->setObjectName("eqModeBtn");
    mode_btn_->setProperty("mode", "paper");
    mode_btn_->setCheckable(true);
    mode_btn_->setFixedHeight(22);
    mode_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(mode_btn_);

    main_layout->addWidget(cmd_bar);

    // ── MAIN 3-PANEL SPLITTER ─────────────────────────────────────────────────
    auto* main_splitter = new QSplitter(Qt::Horizontal);
    main_splitter->setObjectName("eqMainSplitter");
    main_splitter->setHandleWidth(1);

    // LEFT: Watchlist
    watchlist_ = new EquityWatchlist;
    watchlist_->set_symbols(watchlist_symbols_);
    watchlist_->set_active_symbol(selected_symbol_);
    main_splitter->addWidget(watchlist_);

    // CENTER: Chart (top) + Bottom Panel (bottom)
    auto* center_splitter = new QSplitter(Qt::Vertical);
    center_splitter->setObjectName("eqCenterSplitter");
    center_splitter->setHandleWidth(1);

    chart_ = new EquityChart;
    center_splitter->addWidget(chart_);

    bottom_panel_ = new EquityBottomPanel;
    center_splitter->addWidget(bottom_panel_);

    center_splitter->setStretchFactor(0, 5);
    center_splitter->setStretchFactor(1, 2);

    main_splitter->addWidget(center_splitter);

    // RIGHT: Order Book (top) + Order Entry (bottom)
    auto* right_splitter = new QSplitter(Qt::Vertical);
    right_splitter->setObjectName("eqRightSplitter");
    right_splitter->setHandleWidth(1);

    orderbook_ = new EquityOrderBook;
    right_splitter->addWidget(orderbook_);

    order_entry_ = new EquityOrderEntry;
    right_splitter->addWidget(order_entry_);

    right_splitter->setStretchFactor(0, 3);
    right_splitter->setStretchFactor(1, 2);

    main_splitter->addWidget(right_splitter);

    main_splitter->setSizes({220, 600, 290});
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);

    main_layout->addWidget(main_splitter, 1);

    // ── Signal Connections ────────────────────────────────────────────────────
    connect(mode_btn_, &QPushButton::clicked, this, &EquityTradingScreen::on_mode_toggled);
    connect(accounts_btn_, &QPushButton::clicked, this, &EquityTradingScreen::on_accounts_clicked);
    connect(watchlist_, &EquityWatchlist::symbol_selected, this, &EquityTradingScreen::on_symbol_selected);
    connect(watchlist_, &EquityWatchlist::symbol_added, this, [this](const QString& sym) {
        if (!watchlist_symbols_.contains(sym))
            watchlist_symbols_.append(sym);
        // Update all streams with new watchlist
        auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
        if (stream)
            stream->subscribe_symbols(watchlist_symbols_);
    });
    connect(order_entry_, &EquityOrderEntry::order_submitted, this, &EquityTradingScreen::on_order_submitted);
    connect(order_entry_, &EquityOrderEntry::broadcast_requested, this, [this](const trading::UnifiedOrder& order) {
        auto* dlg = new BroadcastOrderDialog(order, this);
        dlg->exec();
        dlg->deleteLater();
    });
    connect(orderbook_, &EquityOrderBook::price_clicked, this, &EquityTradingScreen::on_ob_price_clicked);
    connect(bottom_panel_, &EquityBottomPanel::cancel_order_requested, this, &EquityTradingScreen::on_cancel_order);
    connect(bottom_panel_, &EquityBottomPanel::modify_order_requested, this, &EquityTradingScreen::async_modify_order);
    connect(bottom_panel_, &EquityBottomPanel::import_holdings_requested, this, &EquityTradingScreen::on_import_holdings_requested);
    connect(chart_, &EquityChart::timeframe_changed, this, [this](const QString& tf) {
        auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
        if (stream)
            stream->fetch_candles(selected_symbol_, tf);
    });
}

// ============================================================================
// Timers (only UI-local timers remain)
// ============================================================================

void EquityTradingScreen::setup_timers() {
    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &EquityTradingScreen::update_clock);
    clock_timer_->setInterval(CLOCK_UPDATE_MS);

    // Market clock — poll every 60s via focused account's stream
    market_clock_timer_ = new QTimer(this);
    connect(market_clock_timer_, &QTimer::timeout, this, [this]() {
        auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
        if (stream)
            stream->fetch_clock();
    });
    market_clock_timer_->setInterval(60000);

    // Deferred init — auto-select first account if none focused
    QTimer::singleShot(100, this, [this]() {
        if (focused_account_id_.isEmpty()) {
            auto accounts = AccountManager::instance().active_accounts();
            if (!accounts.isEmpty())
                on_account_changed(accounts.first().account_id);
        }
        update_account_menu();
        update_connection_status();
    });
}

void EquityTradingScreen::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

// ============================================================================
// DataStreamManager signal connections
// ============================================================================

void EquityTradingScreen::connect_data_stream_signals() {
    auto& dsm = DataStreamManager::instance();
    connect(&dsm, &DataStreamManager::quote_updated,
            this, &EquityTradingScreen::on_stream_quote_updated);
    connect(&dsm, &DataStreamManager::watchlist_updated,
            this, &EquityTradingScreen::on_stream_watchlist_updated);
    connect(&dsm, &DataStreamManager::positions_updated,
            this, &EquityTradingScreen::on_stream_positions_updated);
    connect(&dsm, &DataStreamManager::holdings_updated,
            this, &EquityTradingScreen::on_stream_holdings_updated);
    connect(&dsm, &DataStreamManager::orders_updated,
            this, &EquityTradingScreen::on_stream_orders_updated);
    connect(&dsm, &DataStreamManager::funds_updated,
            this, &EquityTradingScreen::on_stream_funds_updated);
    connect(&dsm, &DataStreamManager::candles_fetched,
            this, &EquityTradingScreen::on_stream_candles_fetched);
    connect(&dsm, &DataStreamManager::orderbook_fetched,
            this, &EquityTradingScreen::on_stream_orderbook_fetched);
    connect(&dsm, &DataStreamManager::time_sales_fetched,
            this, &EquityTradingScreen::on_stream_time_sales_fetched);
    connect(&dsm, &DataStreamManager::latest_trade_fetched,
            this, &EquityTradingScreen::on_stream_latest_trade_fetched);
    connect(&dsm, &DataStreamManager::calendar_fetched,
            this, &EquityTradingScreen::on_stream_calendar_fetched);
    connect(&dsm, &DataStreamManager::clock_fetched,
            this, &EquityTradingScreen::on_stream_clock_fetched);
    connect(&dsm, &DataStreamManager::token_expired,
            this, &EquityTradingScreen::handle_token_expired);
}

// ============================================================================
// Init
// ============================================================================

void EquityTradingScreen::init_focused_account() {
    if (focused_account_id_.isEmpty())
        return;

    auto account = AccountManager::instance().get_account(focused_account_id_);
    if (account.account_id.isEmpty())
        return;

    // Configure UI for this account's broker profile
    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (broker) {
        const auto prof = broker->profile();
        order_entry_->configure_for_broker(prof);
        order_entry_->set_broker_id(account.broker_id);
        watchlist_->set_broker_id(account.broker_id);
    }

    // Register fill callback for paper trading
    if (fill_cb_id_ < 0) {
        QPointer<EquityTradingScreen> self = this;
        fill_cb_id_ = OrderMatcher::instance().on_order_fill([self](const OrderFillEvent& ev) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, ev]() {
                    if (!self)
                        return;
                    LOG_INFO(TAG, QString("Paper order filled: %1 %2 @ %3")
                                      .arg(ev.side, ev.symbol)
                                      .arg(ev.fill_price, 0, 'f', 2));
                    // Refresh paper portfolio for focused account
                    auto acct = AccountManager::instance().get_account(self->focused_account_id_);
                    if (acct.trading_mode == "paper" && !acct.paper_portfolio_id.isEmpty()) {
                        try {
                            auto portfolio = pt_get_portfolio(acct.paper_portfolio_id);
                            auto positions = pt_get_positions(acct.paper_portfolio_id);
                            auto orders = pt_get_orders(acct.paper_portfolio_id);
                            auto trades = pt_get_trades(acct.paper_portfolio_id, 50);
                            auto stats = pt_get_stats(acct.paper_portfolio_id);
                            self->order_entry_->set_balance(portfolio.balance);
                            self->bottom_panel_->set_paper_positions(positions);
                            self->bottom_panel_->set_paper_orders(orders);
                            self->bottom_panel_->set_paper_trades(trades);
                            self->bottom_panel_->set_paper_stats(stats);
                        } catch (...) {
                            LOG_WARN(TAG, "Exception refreshing paper portfolio on fill");
                        }
                    }
                },
                Qt::QueuedConnection);
        });
    }

    // Load pending paper orders into matcher
    if (!account.paper_portfolio_id.isEmpty()) {
        auto pending = pt_get_orders(account.paper_portfolio_id, "pending");
        for (const auto& o : pending) {
            if (o.order_type != "market")
                OrderMatcher::instance().add_order(o);
        }
    }

    // Start the data stream for focused account
    auto& dsm = DataStreamManager::instance();
    dsm.start_stream(focused_account_id_);
    auto* stream = dsm.stream_for(focused_account_id_);
    if (stream) {
        stream->set_selected_symbol(selected_symbol_, selected_exchange_);
        stream->subscribe_symbols(watchlist_symbols_);
        stream->fetch_candles(selected_symbol_, chart_->current_timeframe());
        stream->fetch_orderbook(selected_symbol_);
        stream->fetch_time_sales(selected_symbol_);
        stream->fetch_calendar();
        stream->fetch_clock();
    }

    update_account_menu();
    update_connection_status();
    LOG_INFO(TAG, QString("Initialized focused account: %1").arg(focused_account_id_));
}

// ============================================================================
// Account menu and status
// ============================================================================

void EquityTradingScreen::update_account_menu() {
    account_menu_->clear();
    const auto accounts = AccountManager::instance().active_accounts();

    if (accounts.isEmpty()) {
        account_btn_->setText("NO ACCOUNT");
        return;
    }

    for (const auto& acct : accounts) {
        auto* broker = BrokerRegistry::instance().get(acct.broker_id);
        const QString broker_name = broker ? broker->profile().display_name : acct.broker_id;
        const QString text = QString("%1 [%2]").arg(acct.display_name, broker_name);
        auto* action = account_menu_->addAction(text, this, [this, id = acct.account_id]() {
            on_account_changed(id);
        });
        if (acct.account_id == focused_account_id_)
            action->setCheckable(true), action->setChecked(true);
    }

    // Update button text to focused account name
    auto focused = AccountManager::instance().get_account(focused_account_id_);
    if (!focused.account_id.isEmpty())
        account_btn_->setText(focused.display_name.toUpper());
}

void EquityTradingScreen::update_connection_status() {
    const auto accounts = AccountManager::instance().active_accounts();
    int connected = 0;
    for (const auto& a : accounts) {
        if (a.state == ConnectionState::Connected)
            ++connected;
    }
    if (accounts.isEmpty()) {
        conn_label_->setText("○ NO ACCOUNTS");
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::TEXT_TERTIARY()));
    } else if (connected == accounts.size()) {
        conn_label_->setText(QString("● %1/%2 CONNECTED").arg(connected).arg(accounts.size()));
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::POSITIVE()));
    } else if (connected > 0) {
        conn_label_->setText(QString("◐ %1/%2 CONNECTED").arg(connected).arg(accounts.size()));
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::WARNING()));
    } else {
        conn_label_->setText(QString("○ 0/%1 CONNECTED").arg(accounts.size()));
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::TEXT_TERTIARY()));
    }
}

// ============================================================================
// DataStream signal handlers
// ============================================================================

void EquityTradingScreen::on_stream_quote_updated(const QString& account_id, const QString& symbol,
                                                   const BrokerQuote& quote) {
    // Update UI only for focused account's selected symbol
    if (account_id == focused_account_id_ && symbol == selected_symbol_) {
        current_price_ = quote.ltp;
        ticker_bar_->update_quote(quote.ltp, quote.change, quote.change_pct, quote.high, quote.low,
                                  quote.volume, quote.bid, quote.ask);
        order_entry_->set_current_price(quote.ltp);
    }

    // Feed ALL account quotes into paper trading engine for order matching
    auto account = AccountManager::instance().get_account(account_id);
    if (account.trading_mode == "paper" && !account.paper_portfolio_id.isEmpty() && quote.ltp > 0) {
        pt_update_position_price(account.paper_portfolio_id, symbol, quote.ltp);
        PriceData pd;
        pd.last = quote.ltp;
        pd.bid = quote.bid;
        pd.ask = quote.ask;
        pd.timestamp = quote.timestamp;
        OrderMatcher::instance().check_orders(symbol, pd, account.paper_portfolio_id);
        OrderMatcher::instance().check_sl_tp_triggers(account.paper_portfolio_id, symbol, quote.ltp);
    }
}

void EquityTradingScreen::on_stream_watchlist_updated(const QString& account_id,
                                                       const QVector<BrokerQuote>& quotes) {
    if (account_id == focused_account_id_)
        watchlist_->update_quotes(quotes);

    // Feed into paper trading for all accounts
    auto account = AccountManager::instance().get_account(account_id);
    if (account.trading_mode == "paper" && !account.paper_portfolio_id.isEmpty()) {
        for (const auto& q : quotes) {
            if (q.ltp <= 0)
                continue;
            pt_update_position_price(account.paper_portfolio_id, q.symbol, q.ltp);
            PriceData pd;
            pd.last = q.ltp;
            pd.bid = q.bid;
            pd.ask = q.ask;
            pd.timestamp = q.timestamp;
            OrderMatcher::instance().check_orders(q.symbol, pd, account.paper_portfolio_id);
        }
    }
}

void EquityTradingScreen::on_stream_positions_updated(const QString& account_id,
                                                       const QVector<BrokerPosition>& positions) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_positions(positions);
}

void EquityTradingScreen::on_stream_holdings_updated(const QString& account_id,
                                                      const QVector<BrokerHolding>& holdings) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_holdings(holdings);
}

void EquityTradingScreen::on_stream_orders_updated(const QString& account_id,
                                                    const QVector<BrokerOrderInfo>& orders) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_orders(orders);
}

void EquityTradingScreen::on_stream_funds_updated(const QString& account_id, const BrokerFunds& funds) {
    if (account_id == focused_account_id_) {
        bottom_panel_->set_funds(funds);
        order_entry_->set_balance(funds.available_balance);
    }
}

void EquityTradingScreen::on_stream_candles_fetched(const QString& account_id,
                                                     const QVector<BrokerCandle>& candles) {
    if (account_id == focused_account_id_)
        chart_->set_candles(candles);
}

void EquityTradingScreen::on_stream_orderbook_fetched(const QString& account_id,
                                                       const QVector<QPair<double, double>>& bids,
                                                       const QVector<QPair<double, double>>& asks,
                                                       double spread, double spread_pct) {
    if (account_id == focused_account_id_)
        orderbook_->set_data(bids, asks, spread, spread_pct);
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

// ============================================================================
// Slot Handlers
// ============================================================================

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

    // Ensure stream is running and configure it
    auto& dsm = DataStreamManager::instance();
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
        update_account_menu();
        // If no focused account, auto-focus the new one
        if (focused_account_id_.isEmpty())
            on_account_changed(account_id);
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
static QString yfinance_symbol_for(const QString& symbol, const QString& exchange) {
    static const QHash<QString, QString> suffix_map = {
        {"NSE", ".NS"},        {"BSE", ".BO"},  {"HKEX", ".HK"}, {"TSE", ".T"},
        {"KRX", ".KS"},        {"SGX", ".SI"},  {"ASX", ".AX"},  {"IDX", ".JK"},
        {"MYX", ".KL"},        {"SET", ".BK"},  {"PSE", ".PS"},  {"XETR", ".DE"},
        {"FWB", ".F"},         {"LSE", ".L"},   {"BME", ".MC"},  {"MIL", ".MI"},
        {"SIX", ".SW"},        {"TSX", ".TO"},  {"TSXV", ".V"},  {"BMFBOVESPA", ".SA"},
        {"BIST", ".IS"},       {"EGX", ".CA"},
    };
    const QString sym = symbol.trimmed().toUpper();
    const auto it = suffix_map.find(exchange.trimmed().toUpper());
    if (it == suffix_map.end() || sym.isEmpty() || sym.contains('.'))
        return sym;
    return sym + it.value();
}

void EquityTradingScreen::on_import_holdings_requested(const QVector<trading::BrokerHolding>& holdings) {
    if (holdings.isEmpty())
        return;
    if (focused_account_id_.isEmpty()) {
        QMessageBox::warning(this, "Import Holdings", "No account selected.");
        return;
    }
    const auto account = trading::AccountManager::instance().get_account(focused_account_id_);
    const QString broker_id = account.broker_id;
    auto* broker = trading::BrokerRegistry::instance().get(broker_id);
    const QString default_currency = (broker ? broker->profile().currency : QStringLiteral("USD"));
    const QString suggested_name = account.display_name.isEmpty()
                                       ? QString("%1 Holdings").arg(broker_id.toUpper())
                                       : QString("%1 - Holdings").arg(account.display_name);

    QDialog dlg(this);
    dlg.setWindowTitle("Import holdings into portfolio");
    dlg.setMinimumSize(780, 560);

    auto* v = new QVBoxLayout(&dlg);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(10);

    auto* info = new QLabel(QString("Importing <b>%1</b> holdings from <b>%2</b>. "
                                    "Tickers are auto-mapped to Yahoo Finance format "
                                    "(NSE→.NS, BSE→.BO). Edit the <i>Yahoo Ticker</i> column "
                                    "if any symbol needs a manual override.")
                                .arg(holdings.size())
                                .arg(account.display_name.isEmpty() ? broker_id.toUpper()
                                                                    : account.display_name));
    info->setTextFormat(Qt::RichText);
    info->setWordWrap(true);
    v->addWidget(info);

    // ── Portfolio target selection ────────────────────────────────────────
    auto* mode_new = new QRadioButton("Create a new portfolio");
    auto* mode_existing = new QRadioButton("Add to existing portfolio");
    mode_new->setChecked(true);

    auto* mode_row = new QHBoxLayout;
    mode_row->addWidget(mode_new);
    mode_row->addWidget(mode_existing);
    mode_row->addStretch();
    v->addLayout(mode_row);

    auto* new_row = new QWidget;
    auto* new_form = new QFormLayout(new_row);
    new_form->setContentsMargins(18, 0, 0, 0);
    auto* name_input = new QLineEdit(suggested_name);
    auto* currency_input = new QLineEdit(default_currency);
    new_form->addRow("Name:", name_input);
    new_form->addRow("Currency:", currency_input);
    v->addWidget(new_row);

    auto* existing_combo = new QComboBox;
    existing_combo->setEnabled(false);
    existing_combo->addItem("Loading portfolios...");
    auto* existing_row = new QWidget;
    auto* existing_form = new QFormLayout(existing_row);
    existing_form->setContentsMargins(18, 0, 0, 0);
    existing_form->addRow("Portfolio:", existing_combo);
    existing_row->setVisible(false);
    v->addWidget(existing_row);

    // ── Per-row holdings table with editable yfinance ticker ─────────────
    auto* table = new QTableWidget;
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(
        {"Import", "Broker Symbol", "Exchange", "Yahoo Ticker (edit)", "Quantity", "Avg Price"});
    table->setRowCount(holdings.size());
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::AllEditTriggers);
    table->setAlternatingRowColors(true);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->verticalScrollBar()->setSingleStep(8);
    {
        auto* hh = table->horizontalHeader();
        hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(3, QHeaderView::Stretch);
        hh->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    }

    auto make_readonly = [](const QString& text, bool right_align = false) {
        auto* it = new QTableWidgetItem(text);
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        if (right_align)
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return it;
    };

    for (int r = 0; r < holdings.size(); ++r) {
        const auto& h = holdings[r];

        // Col 0: import checkbox (centered via a container widget)
        auto* cb_cell = new QWidget;
        auto* cb_lay = new QHBoxLayout(cb_cell);
        cb_lay->setContentsMargins(0, 0, 0, 0);
        auto* cb = new QCheckBox;
        cb->setChecked(h.quantity > 0 && !h.symbol.isEmpty());
        cb->setProperty("row_index", r);
        cb_lay->addWidget(cb, 0, Qt::AlignCenter);
        table->setCellWidget(r, 0, cb_cell);

        table->setItem(r, 1, make_readonly(h.symbol));
        table->setItem(r, 2, make_readonly(h.exchange));

        auto* yf_item = new QTableWidgetItem(yfinance_symbol_for(h.symbol, h.exchange));
        yf_item->setFlags(yf_item->flags() | Qt::ItemIsEditable);
        table->setItem(r, 3, yf_item);

        table->setItem(r, 4, make_readonly(QString::number(h.quantity, 'f', 2), true));
        table->setItem(r, 5, make_readonly(QString::number(h.avg_price, 'f', 2), true));
    }
    v->addWidget(table, 1);

    // Select/Deselect all helpers
    auto* select_row = new QHBoxLayout;
    auto* select_all_btn = new QPushButton("Select all");
    auto* deselect_all_btn = new QPushButton("Deselect all");
    select_row->addWidget(select_all_btn);
    select_row->addWidget(deselect_all_btn);
    select_row->addStretch();
    v->addLayout(select_row);

    auto set_all_checked = [table](bool on) {
        for (int r = 0; r < table->rowCount(); ++r) {
            if (auto* w = table->cellWidget(r, 0)) {
                if (auto* cb = w->findChild<QCheckBox*>())
                    cb->setChecked(on);
            }
        }
    };
    QObject::connect(select_all_btn, &QPushButton::clicked, &dlg, [set_all_checked]() { set_all_checked(true); });
    QObject::connect(deselect_all_btn, &QPushButton::clicked, &dlg, [set_all_checked]() { set_all_checked(false); });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("IMPORT");
    v->addWidget(buttons);

    auto sync_mode = [&]() {
        const bool is_new = mode_new->isChecked();
        new_row->setVisible(is_new);
        existing_row->setVisible(!is_new);
    };
    QObject::connect(mode_new, &QRadioButton::toggled, &dlg, sync_mode);
    QObject::connect(mode_existing, &QRadioButton::toggled, &dlg, sync_mode);
    sync_mode();

    // Load existing portfolios asynchronously.
    auto& ps = fincept::services::PortfolioService::instance();
    QPointer<QComboBox> combo_guard = existing_combo;
    QPointer<QRadioButton> existing_guard = mode_existing;
    auto conn = QObject::connect(
        &ps, &fincept::services::PortfolioService::portfolios_loaded, &dlg,
        [combo_guard, existing_guard](const QVector<fincept::portfolio::Portfolio>& list) {
            if (!combo_guard) return;
            combo_guard->clear();
            if (list.isEmpty()) {
                combo_guard->addItem("(no portfolios yet)");
                combo_guard->setEnabled(false);
                if (existing_guard) existing_guard->setEnabled(false);
            } else {
                for (const auto& p : list)
                    combo_guard->addItem(QString("%1 (%2)").arg(p.name, p.currency), p.id);
                combo_guard->setEnabled(true);
            }
        });
    ps.load_portfolios();

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        QObject::disconnect(conn);
        return;
    }
    QObject::disconnect(conn);

    // Collect selected rows with their (possibly edited) yfinance tickers.
    struct ImportRow {
        QString symbol;
        double quantity = 0;
        double avg_price = 0;
    };
    QVector<ImportRow> rows;
    rows.reserve(holdings.size());
    for (int r = 0; r < holdings.size(); ++r) {
        auto* w = table->cellWidget(r, 0);
        auto* cb = w ? w->findChild<QCheckBox*>() : nullptr;
        if (!cb || !cb->isChecked())
            continue;
        const QString yf = table->item(r, 3)->text().trimmed().toUpper();
        if (yf.isEmpty())
            continue;
        rows.push_back({yf, holdings[r].quantity, holdings[r].avg_price});
    }
    if (rows.isEmpty()) {
        QMessageBox::information(this, "Import Holdings", "Nothing selected to import.");
        return;
    }

    auto do_add_assets = [rows](const QString& portfolio_id) {
        auto& svc = fincept::services::PortfolioService::instance();
        const QString today = QDate::currentDate().toString(Qt::ISODate);
        for (const auto& row : rows) {
            if (row.quantity <= 0 || row.symbol.isEmpty())
                continue;
            svc.add_asset(portfolio_id, row.symbol, row.quantity, row.avg_price, today);
        }
    };

    if (mode_new->isChecked()) {
        const QString name = name_input->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "Import Holdings", "Portfolio name is required.");
            return;
        }
        const QString currency = currency_input->text().trimmed().isEmpty()
                                     ? default_currency
                                     : currency_input->text().trimmed().toUpper();
        QPointer<EquityTradingScreen> self = this;
        auto once = std::make_shared<QMetaObject::Connection>();
        const int count = rows.size();
        *once = connect(&ps, &fincept::services::PortfolioService::portfolio_created, this,
                        [self, once, do_add_assets, name, count](const fincept::portfolio::Portfolio& p) {
                            if (!self || p.name != name)
                                return;
                            QObject::disconnect(*once);
                            do_add_assets(p.id);
                            QMessageBox::information(self, "Import Holdings",
                                                     QString("Imported %1 holdings into portfolio \"%2\".")
                                                         .arg(count)
                                                         .arg(p.name));
                        });
        ps.create_portfolio(name, account.display_name, currency, "Imported from " + broker_id.toUpper());
    } else {
        const QString pid = existing_combo->currentData().toString();
        if (pid.isEmpty()) {
            QMessageBox::warning(this, "Import Holdings", "Select a portfolio first.");
            return;
        }
        do_add_assets(pid);
        QMessageBox::information(this, "Import Holdings",
                                 QString("Imported %1 holdings.").arg(rows.size()));
    }
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

void EquityTradingScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid() || ref.symbol == selected_symbol_)
        return;
    // Route through the existing selection path so sub-panels sync.
    on_symbol_selected(ref.symbol);
}

SymbolRef EquityTradingScreen::current_symbol() const {
    if (selected_symbol_.isEmpty())
        return {};
    return SymbolRef::equity(selected_symbol_, selected_exchange_);
}

} // namespace fincept::screens
