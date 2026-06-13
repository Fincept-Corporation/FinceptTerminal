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

#include <QAbstractItemView>
#include <QApplication>
#include <QMouseEvent>
#include "screens/equity_trading/AccountManagementDialog.h"
#include "screens/equity_trading/BroadcastOrderDialog.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChartPanel.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "screens/common/feeds/FeedPanel.h"
#include "screens/portfolio_monitor/PortfolioMonitorScreen.h"
#include "services/feeds/FeedMonitor.h"
#include "screens/equity_trading/PortfolioReplicationDialog.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/UnifiedTrading.h"
#include "trading/instruments/InstrumentService.h"
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

    // When InstrumentService finishes loading/downloading a broker's instrument
    // master, reload market data for the active account if its broker matches.
    // Account data works without this, but quotes/charts/depth need the numeric
    // securityId map that only exists once instruments are loaded.
    {
        QPointer<EquityTradingScreen> self = this;
        connect(&trading::InstrumentService::instance(), &trading::InstrumentService::refresh_done,
                this, [self](const QString& broker_id, int /*count*/) {
                    if (self)
                        self->on_instruments_ready(broker_id);
                });
        connect(&trading::InstrumentService::instance(), &trading::InstrumentService::refresh_failed,
                this, [](const QString& broker_id, const QString& error) {
                    LOG_WARN(TAG, QString("Instrument load failed for %1: %2").arg(broker_id, error));
                });
    }

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

    // Always make sure a configured broker is loaded. Re-checked on every show so a
    // broker configured after the tab first opened is picked up automatically,
    // instead of stranding the user on "NO ACCOUNT" while a broker exists in the DB.
    ensure_account_loaded();

    // Restore the rest of the persisted session state on first show.
    if (!initialized_) {
        const auto s = ScreenStateManager::instance().load_direct("equity_trading");
        if (!s.isEmpty()) {
            const QString sym = s.value("selected_symbol", selected_symbol_).toString();
            if (!sym.isEmpty() && sym != selected_symbol_)
                on_symbol_selected(sym);
            // Restore the feed column (visibility + width).
            if (s.value("feeds_visible").toBool()) {
                feeds_btn_->setChecked(true); // toggled() shows the panel
                const int w = s.value("feeds_width").toInt();
                if (w > 50 && main_splitter_) {
                    auto sizes = main_splitter_->sizes();
                    if (sizes.size() >= 4) {
                        sizes[3] = w;
                        main_splitter_->setSizes(sizes);
                    }
                }
            }
            // Restore the unified all-accounts column (visibility + width).
            if (s.value("unified_visible").toBool()) {
                unified_btn_->setChecked(true); // toggled() shows the panel
                const int w = s.value("unified_width").toInt();
                if (w > 50 && main_splitter_) {
                    auto sizes = main_splitter_->sizes();
                    if (sizes.size() >= 5) {
                        sizes[4] = w;
                        main_splitter_->setSizes(sizes);
                    }
                }
            }
        }
    }

    // Start all active account data streams
    DataStreamManager::instance().start_all_active();
    DataStreamManager::instance().resume_all();

    // Hub subscriptions for streaming data (D4)
    hub_subscribe_streaming();

    // On REOPEN (not first show — init_focused_account handles that), the stream
    // stayed running while hidden, so neither start() nor resume() re-fetches the
    // portfolio. Force an immediate live refresh so Holdings/Positions/Orders show
    // current broker data right away instead of staying stale (or blank) until the
    // 5-min poll. Paper data is repainted by refresh_paper_panels() just below.
    if (initialized_ && !focused_is_paper_ && !focused_account_id_.isEmpty())
        DataStreamManager::instance().refresh_portfolio(focused_account_id_);

    // Catch up intraday auto-square for paper portfolios (e.g. the app was closed
    // at 15:30, so yesterday's MIS positions never squared). Refresh picks up the
    // resulting state; it no-ops for live accounts.
    trading::pt_settle_intraday_all();
    refresh_paper_panels();

    // UI-local timers
    if (clock_timer_)
        clock_timer_->start();
    if (market_clock_timer_)
        market_clock_timer_->start();

    if (!initialized_) {
        init_focused_account();
        initialized_ = true;
    }

    fincept::feeds::FeedMonitor::instance().resume_all();

    LOG_INFO(TAG, "Screen visible — data streams resumed");
}

void EquityTradingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    hub_unsubscribe_all();
    DataStreamManager::instance().pause_all();
    if (clock_timer_)
        clock_timer_->stop();
    if (market_clock_timer_)
        market_clock_timer_->stop();

    fincept::feeds::FeedMonitor::instance().pause_all();

    ScreenStateManager::instance().save_direct("equity_trading", {
        {"focused_account_id", focused_account_id_},
        {"selected_symbol", selected_symbol_},
        {"selected_exchange", selected_exchange_},
        {"feeds_visible", feed_panel_ != nullptr && feed_panel_->isVisible()},
        {"feeds_width", main_splitter_ != nullptr ? main_splitter_->sizes().value(3) : 0},
        {"unified_visible", monitor_panel_ != nullptr && monitor_panel_->isVisible()},
        {"unified_width", main_splitter_ != nullptr ? main_splitter_->sizes().value(4) : 0},
    });
    LOG_INFO(TAG, "Screen hidden — data streams paused, hub unsubscribed");
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
    account_btn_ = new QPushButton(tr("NO ACCOUNT"));
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
    // Dynamic instrument-search completer (mirrors EquityWatchlist's add box):
    // the model is refilled from InstrumentService on every keystroke, so the
    // suggestion popup reflects the live broker catalog rather than a fixed list.
    // Ownership matters for safe teardown: parent the completer to the line edit
    // and the model to the completer. On destruction the completer disconnects
    // (and deletes its internal QCompletionModel) before its child source model
    // is freed — avoiding the QCompletionModel::filter-on-dying-model segfault
    // seen when the source model is a sibling destroyed first.
    symbol_completer_ = new QCompleter(symbol_input_);
    symbol_completer_model_ = new QStringListModel(symbol_completer_);
    symbol_completer_->setModel(symbol_completer_model_);
    symbol_completer_->setCaseSensitivity(Qt::CaseInsensitive);
    symbol_completer_->setMaxVisibleItems(10);
    symbol_completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    symbol_input_->setCompleter(symbol_completer_);
    connect(symbol_input_, &QLineEdit::textEdited, this, &EquityTradingScreen::on_symbol_search_changed);
    // Choosing a suggestion (click / Enter in the popup) activates that symbol.
    connect(symbol_completer_, qOverload<const QString&>(&QCompleter::activated), this,
            [this](const QString& choice) { apply_symbol_input(choice); });
    // A single mouse-click on a suggestion must select it. QCompleter::activated is
    // unreliable on click for a model rebuilt every keystroke, so wire the popup's
    // clicked() directly (and give the dropdown a clean dark style).
    if (auto* popup = symbol_completer_->popup()) {
        popup->setObjectName("eqSymbolPopup");
        popup->setStyleSheet(
            QString("QAbstractItemView{background:%1;color:%2;border:1px solid %3;outline:0;padding:2px;"
                    "font-size:11px;}"
                    "QAbstractItemView::item{padding:4px 8px;border-radius:2px;}"
                    "QAbstractItemView::item:selected,QAbstractItemView::item:hover{background:%4;color:%2;}")
                .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::TEXT_PRIMARY(),
                     fincept::ui::colors::BORDER_MED(), fincept::ui::colors::BG_HOVER()));
        connect(popup, &QAbstractItemView::clicked, this,
                [this](const QModelIndex& idx) { apply_symbol_input(idx.data(Qt::DisplayRole).toString()); });
    }
    connect(symbol_input_, &QLineEdit::returnPressed, this,
            [this]() { apply_symbol_input(symbol_input_->text()); });
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
    conn_label_ = new QLabel(tr("○ NO ACCOUNTS"));
    conn_label_->setObjectName("eqConnLabel");
    conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::TEXT_TERTIARY()));
    cmd_layout->addWidget(conn_label_);

    // Accounts management button (replaces api_btn_)
    accounts_btn_ = new QPushButton(tr("ACCOUNTS"));
    accounts_btn_->setObjectName("eqApiBtn");
    accounts_btn_->setFixedHeight(22);
    accounts_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(accounts_btn_);

    // Mode button (per-account mode toggle)
    mode_btn_ = new QPushButton(tr("PAPER"));
    mode_btn_->setObjectName("eqModeBtn");
    mode_btn_->setProperty("mode", "paper");
    mode_btn_->setCheckable(true);
    mode_btn_->setFixedHeight(22);
    mode_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(mode_btn_);

    // FEEDS — toggles the far-right feed monitor column.
    feeds_btn_ = new QPushButton(tr("FEEDS"));
    feeds_btn_->setObjectName("eqApiBtn");
    feeds_btn_->setFixedHeight(22);
    feeds_btn_->setCheckable(true);
    feeds_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(feeds_btn_);

    // UNIFIED — toggles the all-accounts portfolio monitor column (same
    // interaction as FEEDS: checkable button ↔ collapsible splitter column).
    unified_btn_ = new QPushButton(tr("UNIFIED"));
    unified_btn_->setObjectName("eqApiBtn");
    unified_btn_->setFixedHeight(22);
    unified_btn_->setCheckable(true);
    unified_btn_->setCursor(Qt::PointingHandCursor);
    unified_btn_->setToolTip(tr("All-accounts positions & holdings (every connected broker)"));
    cmd_layout->addWidget(unified_btn_);

    main_layout->addWidget(cmd_bar);

    // ── MAIN 3-PANEL SPLITTER ─────────────────────────────────────────────────
    main_splitter_ = new QSplitter(Qt::Horizontal);
    auto* main_splitter = main_splitter_;
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

    chart_ = new EquityChartPanel;
    center_splitter->addWidget(chart_);

    bottom_panel_ = new EquityBottomPanel;
    center_splitter->addWidget(bottom_panel_);

    center_splitter->setStretchFactor(0, 5);
    center_splitter->setStretchFactor(1, 2);
    // The chart (a WebEngine view in KLineChart mode) reports a tiny size hint
    // until its page loads, so without an explicit split the bottom panel's
    // tall table hints would swallow all the height. Pin a chart-dominant
    // initial split and forbid collapsing the chart pane to zero.
    center_splitter->setCollapsible(0, false);
    center_splitter->setSizes({560, 220});

    main_splitter->addWidget(center_splitter);

    // RIGHT: Order Book (top) + Order Entry (bottom)
    auto* right_splitter = new QSplitter(Qt::Vertical);
    right_splitter->setObjectName("eqRightSplitter");
    right_splitter->setHandleWidth(1);

    orderbook_ = new EquityOrderBook;
    right_splitter->addWidget(orderbook_);

    order_entry_ = new EquityOrderEntry;
    right_splitter->addWidget(order_entry_);

    // Order book is pinned to its content height; order entry absorbs the rest.
    right_splitter->setStretchFactor(0, 0);
    right_splitter->setStretchFactor(1, 1);
    right_splitter->setSizes({320, 800});

    main_splitter->addWidget(right_splitter);

    // FAR RIGHT: collapsible feed monitor column (hidden by default; toggled via the
    // FEEDS command-bar button). Reusable FeedPanel — see screens/common/feeds.
    feed_panel_ = new fincept::feeds::FeedPanel;
    feed_panel_->setVisible(false);
    main_splitter->addWidget(feed_panel_);

    // FAR RIGHT 2: collapsible all-accounts portfolio monitor (hidden by default;
    // toggled via the UNIFIED command-bar button). Aggregates positions/holdings
    // across every connected INR broker — see screens/portfolio_monitor.
    monitor_panel_ = new PortfolioMonitorScreen;
    monitor_panel_->setVisible(false);
    main_splitter->addWidget(monitor_panel_);

    main_splitter->setSizes({265, 555, 290, 0, 0});
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);
    main_splitter->setStretchFactor(3, 0);
    main_splitter->setStretchFactor(4, 0);

    main_layout->addWidget(main_splitter, 1);

    // ── Signal Connections ────────────────────────────────────────────────────
    connect(mode_btn_, &QPushButton::clicked, this, &EquityTradingScreen::on_mode_toggled);
    connect(feeds_btn_, &QPushButton::toggled, this, [this](bool on) {
        feed_panel_->setVisible(on);
        if (on) {
            auto sizes = main_splitter_->sizes();
            if (sizes.size() >= 4 && sizes[3] < 50) {
                sizes[3] = 300;
                main_splitter_->setSizes(sizes);
            }
        }
    });
    connect(unified_btn_, &QPushButton::toggled, this, [this](bool on) {
        if (monitor_float_) {
            // Detached: ON raises the floating window; OFF closes it (the
            // close re-docks the panel hidden — see redock_monitor()).
            if (on) {
                monitor_float_->raise();
                monitor_float_->activateWindow();
            } else {
                monitor_float_->close();
            }
            return;
        }
        monitor_panel_->setVisible(on); // showEvent → service activate + refresh
        if (on) {
            auto sizes = main_splitter_->sizes();
            if (sizes.size() >= 5 && sizes[4] < 50) {
                sizes[4] = 560; // wide enough for the 8-column tree
                main_splitter_->setSizes(sizes);
            }
        }
    });
    connect(monitor_panel_, &PortfolioMonitorScreen::float_requested, this,
            &EquityTradingScreen::float_monitor);
    connect(monitor_panel_, &PortfolioMonitorScreen::dock_requested, this,
            [this]() { if (monitor_float_) monitor_float_->close(); }); // close → redock
    connect(accounts_btn_, &QPushButton::clicked, this, &EquityTradingScreen::on_accounts_clicked);
    connect(watchlist_, &EquityWatchlist::symbol_selected, this, &EquityTradingScreen::on_symbol_selected);
    connect(watchlist_, &EquityWatchlist::symbol_added, this, &EquityTradingScreen::on_watchlist_symbol_added);
    connect(watchlist_, &EquityWatchlist::symbol_removed, this, &EquityTradingScreen::on_watchlist_symbol_removed);
    connect(watchlist_, &EquityWatchlist::watchlist_selected, this, &EquityTradingScreen::on_watchlist_selected);
    connect(watchlist_, &EquityWatchlist::watchlist_create_requested, this, &EquityTradingScreen::on_watchlist_create);
    connect(watchlist_, &EquityWatchlist::watchlist_rename_requested, this, &EquityTradingScreen::on_watchlist_rename);
    connect(watchlist_, &EquityWatchlist::watchlist_delete_requested, this, &EquityTradingScreen::on_watchlist_delete);
    connect(order_entry_, &EquityOrderEntry::order_submitted, this, &EquityTradingScreen::on_order_submitted);
    connect(order_entry_, &EquityOrderEntry::multi_broker_submit, this,
            &EquityTradingScreen::on_multi_broker_submit);
    connect(order_entry_, &EquityOrderEntry::strategy_order_submitted, this, &EquityTradingScreen::on_strategy_submitted);
    connect(order_entry_, &EquityOrderEntry::broadcast_requested, this, [this](const trading::UnifiedOrder& order) {
        auto* dlg = new BroadcastOrderDialog(order, this);
        dlg->exec();
        dlg->deleteLater();
    });
    connect(orderbook_, &EquityOrderBook::price_clicked, this, &EquityTradingScreen::on_ob_price_clicked);
    connect(bottom_panel_, &EquityBottomPanel::cancel_order_requested, this, &EquityTradingScreen::on_cancel_order);
    connect(bottom_panel_, &EquityBottomPanel::modify_order_requested, this, &EquityTradingScreen::async_modify_order);
    connect(bottom_panel_, &EquityBottomPanel::cancel_all_orders_requested, this,
            [this](const QString&) { on_cancel_all_orders(); });
    connect(bottom_panel_, &EquityBottomPanel::close_all_positions_requested, this,
            [this](const QString&) { on_close_all_positions(); });
    connect(bottom_panel_, &EquityBottomPanel::square_off_all_holdings_requested, this,
            &EquityTradingScreen::on_square_off_all_holdings);
    connect(bottom_panel_, &EquityBottomPanel::square_off_holding_requested, this,
            &EquityTradingScreen::on_square_off_holding);
    connect(bottom_panel_, &EquityBottomPanel::import_holdings_requested, this, &EquityTradingScreen::on_import_holdings_requested);
    connect(bottom_panel_, &EquityBottomPanel::replicate_portfolio_requested, this,
            &EquityTradingScreen::on_replicate_portfolio_requested);
    connect(bottom_panel_, &EquityBottomPanel::convert_position_requested, this,
            &EquityTradingScreen::on_convert_position);
    connect(bottom_panel_, &EquityBottomPanel::orders_day_changed, this, &EquityTradingScreen::on_orders_day_changed);
    connect(bottom_panel_, &EquityBottomPanel::square_off_group_requested, this,
            &EquityTradingScreen::on_square_off_group);
    connect(bottom_panel_, &EquityBottomPanel::trade_symbol_requested, this,
            &EquityTradingScreen::on_trade_symbol_requested);
    // Click a position/holding row → load that symbol on the chart (same slot the
    // watchlist uses, so it sets the selected symbol, fetches candles + orderbook).
    connect(bottom_panel_, &EquityBottomPanel::chart_symbol_requested, this,
            &EquityTradingScreen::on_symbol_selected);
    connect(chart_, &EquityChartPanel::timeframe_changed, this, [this](const QString& tf) {
        auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
        if (stream)
            stream->fetch_candles(selected_symbol_, tf);
    });
    connect(chart_, &EquityChartPanel::buy_requested, this, &EquityTradingScreen::on_chart_buy_requested);
    connect(chart_, &EquityChartPanel::sell_requested, this, &EquityTradingScreen::on_chart_sell_requested);
    connect(chart_, &EquityChartPanel::add_to_watchlist_requested, this,
            &EquityTradingScreen::on_chart_add_to_watchlist);
    connect(chart_, &EquityChartPanel::exit_position_requested, this,
            &EquityTradingScreen::on_chart_exit_position);
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
        if (stream && focused_is_us_market_)
            stream->fetch_clock(); // US-only market clock (Alpaca calendar tab)
        // Paper intraday auto-square at 15:30 IST. Cheap no-op until the cutoff;
        // refreshes the panels only when something was actually squared off.
        if (trading::pt_settle_intraday_all() > 0)
            refresh_paper_panels();
    });
    market_clock_timer_->setInterval(60000);

    // Deferred init — auto-select the configured broker if none is focused yet.
    // Routes through the single credential-aware resolver so it can't pick an
    // unconfigured (no-credentials) broker that merely sorts first in the map.
    QTimer::singleShot(100, this, [this]() {
        ensure_account_loaded();
        update_account_menu();
        update_connection_status();
    });
}

void EquityTradingScreen::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void EquityTradingScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityTradingScreen::retranslateUi() {
    if (accounts_btn_) accounts_btn_->setText(tr("ACCOUNTS"));

    // Mode button reflects the focused account's live/paper state.
    if (mode_btn_)
        mode_btn_->setText(mode_btn_->isChecked() ? tr("LIVE") : tr("PAPER"));

    // Account button + aggregate connection status are data-driven; re-running
    // their builders re-applies the translated chrome (and the "NO ACCOUNT" /
    // "NO ACCOUNTS" placeholders) while preserving the live state.
    update_account_menu();
    update_connection_status();
    // exchange_label_ holds a market code (NSE/BSE/…), symbol_input_ the ticker,
    // clock_label_ a timestamp — all data, not translated.
}

// ============================================================================
// DataStreamManager signal connections
// ============================================================================

void EquityTradingScreen::ensure_instruments_loaded(const QString& account_id) {
    if (account_id.isEmpty())
        return;
    const auto creds = AccountManager::instance().load_credentials(account_id);
    const QString broker_id = creds.broker_id;
    if (broker_id.isEmpty())
        return;

    auto& svc = trading::InstrumentService::instance();
    if (svc.is_loaded(broker_id)) {
        // Already in memory — make sure market data reflects it.
        on_instruments_ready(broker_id);
        return;
    }

    QPointer<EquityTradingScreen> self = this;
    svc.load_from_db_async(broker_id, [self, broker_id, creds](int count) {
        if (!self)
            return;
        if (count > 0) {
            // Cache hit: instruments now in memory → reload market data.
            self->on_instruments_ready(broker_id);
            return;
        }
        // No cache on disk — download from the broker. refresh() emits
        // refresh_done on completion, which routes to on_instruments_ready().
        LOG_INFO(TAG, QString("Downloading %1 instruments from broker...").arg(broker_id));
        trading::InstrumentService::instance().refresh(broker_id, creds);
    });
}

void EquityTradingScreen::on_instruments_ready(const QString& broker_id) {
    // Only act if the currently-focused account uses this broker.
    if (focused_account_id_.isEmpty())
        return;
    const QString focused_broker =
        AccountManager::instance().load_credentials(focused_account_id_).broker_id;
    if (focused_broker != broker_id)
        return;

    LOG_INFO(TAG, QString("Instruments ready for %1 — reloading market data").arg(broker_id));

    // Rebuild the watchlist so its completer/validation pick up the new cache,
    // then re-issue the data fetches for the active symbol + watchlist via the
    // stream (the same path on_account_changed uses on connect).
    watchlist_->set_symbols(watchlist_symbols_);

    auto* stream = DataStreamManager::instance().stream_for(focused_account_id_);
    if (stream) {
        stream->set_selected_symbol(selected_symbol_, selected_exchange_);
        stream->subscribe_symbols(QStringLiteral("equity:watchlist"), effective_symbols());
        stream->fetch_candles(selected_symbol_, chart_->current_timeframe());
        stream->fetch_orderbook(selected_symbol_);
        stream->fetch_time_sales(selected_symbol_);
    }

    // Re-subscribe hub topics so streaming quotes flow for the resolved symbols.
    if (isVisible())
        hub_subscribe_streaming();
}

void EquityTradingScreen::connect_data_stream_signals() {
    auto& dsm = DataStreamManager::instance();
    // Streaming data (quotes, positions, holdings, orders, funds) now comes
    // via DataHub subscriptions — see hub_subscribe_streaming().
    // Only on-demand / one-shot signals remain wired here.
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
    connect(&dsm, &DataStreamManager::connection_state_changed,
            this, [this](const QString& /*account_id*/, ConnectionState /*state*/) {
        update_connection_status();
    });
    connect(&dsm, &DataStreamManager::token_expired,
            this, &EquityTradingScreen::handle_token_expired);
}

// ============================================================================
// Init
// ============================================================================

void EquityTradingScreen::ensure_account_loaded() {
    auto& am = AccountManager::instance();

    // Respect the current focus when it's a live session, or — once the user has
    // started interacting (post-init) — any valid account, so manual broker switches
    // are never undone. The smart auto-pick only happens at startup / when stranded.
    const bool focus_valid = !focused_account_id_.isEmpty() && am.has_account(focused_account_id_);
    if (focus_valid) {
        const bool live = am.get_account(focused_account_id_).state == ConnectionState::Connected;
        if (live || initialized_)
            return;
    }

    const QString saved =
        ScreenStateManager::instance().load_direct("equity_trading").value("focused_account_id").toString();

    // Rank every account so the *connected* broker wins over one whose token is
    // expired or that was merely added. Tiers (×2 so the saved tie-breaker can only
    // reorder within a tier, never across):
    //   Connected (live session)            → 4   ← what the user means by "configured"
    //   Connecting (token, unknown expiry)  → 3
    //   has credentials (api_key) but dead  → 1
    //   added, no credentials               → 0
    auto score = [&am](const BrokerAccount& a) -> int {
        switch (a.state) {
            case ConnectionState::Connected: return 4;
            case ConnectionState::Connecting: return 3;
            default: break; // TokenExpired / Error / Disconnected
        }
        return am.load_credentials(a.account_id).api_key.isEmpty() ? 0 : 1;
    };

    QString best;
    int best_rank = -1;
    for (const auto& a : am.list_accounts()) {
        const int rank = score(a) * 2 + (a.account_id == saved ? 1 : 0);
        if (rank > best_rank) {
            best_rank = rank;
            best = a.account_id;
        }
    }

    // on_account_changed() configures the UI and (when credentials exist) loads the
    // instrument master + starts the data stream; it is a no-op for a focused match.
    if (!best.isEmpty() && best != focused_account_id_)
        on_account_changed(best);
}

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

    // Load the broker instrument master (numeric securityId map) for the
    // restored/active account so quotes, charts and depth resolve on app
    // restart / account switch. Loads from the SQLite cache when present,
    // else downloads; on_instruments_ready() reloads market data when done.
    ensure_instruments_loaded(focused_account_id_);

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
                    // Refresh paper panels for the focused account (positions,
                    // orders, trades, stats, funds) — single source of truth.
                    self->refresh_paper_panels();
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
        stream->subscribe_symbols(QStringLiteral("equity:watchlist"), effective_symbols());
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
        account_btn_->setText(tr("NO ACCOUNT"));
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
    int expired = 0;
    int errored = 0;
    QString first_error;
    for (const auto& a : accounts) {
        if (a.state == ConnectionState::Connected)
            ++connected;
        else if (a.state == ConnectionState::TokenExpired)
            ++expired;
        else if (a.state == ConnectionState::Error) {
            ++errored;
            if (first_error.isEmpty())
                first_error = a.error_message;
        }
    }
    // Default: no hover detail. Only the Error branch attaches the broker's reason
    // (e.g. "no active Kite Connect subscription") as a tooltip.
    conn_label_->setToolTip(QString());
    if (accounts.isEmpty()) {
        conn_label_->setText(tr("○ NO ACCOUNTS"));
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::TEXT_TERTIARY()));
    } else if (expired > 0 && connected == 0) {
        conn_label_->setText(tr("\xe2\x9a\xa0 TOKEN EXPIRED \xe2\x80\x94 click ACCOUNTS"));
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::NEGATIVE()));
    } else if (errored > 0 && connected == 0) {
        // Authenticated but the broker refused live market data (e.g. Kite 403 — no
        // active Kite Connect subscription). Account data may still load; only the
        // live feed is gated. Full reason on hover.
        conn_label_->setText(tr("\xe2\x9a\xa0 NO MARKET DATA \xe2\x80\x94 hover for details"));
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::NEGATIVE()));
        conn_label_->setToolTip(first_error);
    } else if (connected == accounts.size()) {
        conn_label_->setText(tr("● %1/%2 CONNECTED").arg(connected).arg(accounts.size()));
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::POSITIVE()));
    } else if (connected > 0) {
        conn_label_->setText(tr("◐ %1/%2 CONNECTED").arg(connected).arg(accounts.size()));
        conn_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700;").arg(ui::colors::WARNING()));
    } else {
        conn_label_->setText(tr("○ 0/%1 CONNECTED").arg(accounts.size()));
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

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap EquityTradingScreen::save_state() const {
    return {
        {"symbol", selected_symbol_},
        {"exchange", selected_exchange_},
        {"account_id", focused_account_id_},
        {"watchlist_id", active_watchlist_id_},
    };
}

void EquityTradingScreen::restore_state(const QVariantMap& state) {
    // Set the active watchlist first so load_watchlists() (via on_account_changed
    // or the trailing call below) selects it.
    const QString wl = state.value("watchlist_id").toString();
    if (!wl.isEmpty())
        active_watchlist_id_ = wl;
    const QString sym = state.value("symbol", "RELIANCE").toString();
    if (!sym.isEmpty() && sym != selected_symbol_)
        on_symbol_selected(sym);
    // Account selection goes through the single connection-aware resolver (which
    // reads the saved account itself) rather than forcing state["account_id"] — that
    // direct path used to override the resolver and load an expired/unconfigured broker.
    ensure_account_loaded();
    // Honour the restored list even when the account didn't change (idempotent).
    if (!wl.isEmpty() && !focused_account_id_.isEmpty())
        load_watchlists();
}

void EquityTradingScreen::on_replicate_portfolio_requested() {
    fincept::screens::PortfolioReplicationDialog dlg(this);
    dlg.exec();
    // Reflect any new paper positions/holdings/funds in the panels. Safe to call
    // unconditionally — it no-ops unless the focused account is paper.
    refresh_paper_panels();
}

// ============================================================================
// UNIFIED monitor detach / re-dock (feeds pop-out pattern)
// ============================================================================

void EquityTradingScreen::float_monitor() {
    if (monitor_float_)
        return; // already detached
    // Top-level child window (Qt::Window flag, parented for auto-cleanup —
    // same construction FloatingFeedWindow uses).
    monitor_float_ = new QWidget(this, Qt::Window);
    monitor_float_->setWindowTitle(tr("Unified Portfolio — All Accounts"));
    monitor_float_->setObjectName("floatingMonitorWindow");
    monitor_float_->setStyleSheet(
        QString("#floatingMonitorWindow{background:%1;}").arg(fincept::ui::colors::BG_BASE()));
    monitor_float_->resize(980, 620);
    auto* lay = new QVBoxLayout(monitor_float_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(monitor_panel_); // reparents out of the splitter
    monitor_panel_->set_floating(true);
    monitor_panel_->setVisible(true);
    monitor_float_->installEventFilter(this); // Close → redock_monitor()
    monitor_float_->show();
}

void EquityTradingScreen::redock_monitor() {
    if (!monitor_float_)
        return;
    auto* win = monitor_float_;
    monitor_float_ = nullptr; // clear first — eventFilter fires during teardown
    // Back into splitter slot 4 (after the feed column).
    main_splitter_->insertWidget(4, monitor_panel_);
    main_splitter_->setStretchFactor(4, 0);
    monitor_panel_->set_floating(false);
    const bool show = unified_btn_ != nullptr && unified_btn_->isChecked();
    monitor_panel_->setVisible(show);
    if (show) {
        auto sizes = main_splitter_->sizes();
        if (sizes.size() >= 5 && sizes[4] < 50) {
            sizes[4] = 560;
            main_splitter_->setSizes(sizes);
        }
    }
    win->removeEventFilter(this);
    win->deleteLater();
}

bool EquityTradingScreen::eventFilter(QObject* watched, QEvent* event) {
    if (watched == monitor_float_ && event->type() == QEvent::Close) {
        redock_monitor();
        return false; // let the (now empty) window finish closing
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace fincept::screens
