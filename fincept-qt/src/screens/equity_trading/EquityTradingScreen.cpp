// Equity Trading Screen — multi-account coordinator.
//
// Core: ctor/dtor, show/hide events, setup_ui, setup_timers, update_clock,
// connect_data_stream_signals, init_focused_account, update_account_menu,
// update_connection_status, on_group_symbol_changed, current_symbol. Split
// concerns:
//   - EquityTradingScreen_Streams.cpp  — on_stream_* slots from DataStreamManager
//   - EquityTradingScreen_Handlers.cpp — user-action handlers
//   - EquityTradingScreen_Holdings.cpp — broker→portfolio holdings import
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

EquityTradingScreen::EquityTradingScreen(QWidget* parent) : QWidget(parent) {
    watchlist_symbols_ = DEFAULT_WATCHLIST;
    setup_ui();
    setup_timers();
    connect_data_stream_signals();

    // Accept symbol drops anywhere on the screen — route through the
    // existing selection path so sub-panels resync and the linked group is
    // published to.
    symbol_dnd::installDropFilter(this, [this](const SymbolRef& ref, SymbolGroup) {
        if (ref.is_valid())
            on_symbol_selected(ref.symbol);
    });
}

EquityTradingScreen::~EquityTradingScreen() {
    if (fill_cb_id_ >= 0)
        OrderMatcher::instance().remove_fill_callback(fill_cb_id_);
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
