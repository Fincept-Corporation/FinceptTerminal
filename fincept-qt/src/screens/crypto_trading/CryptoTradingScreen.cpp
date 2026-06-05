// Crypto Trading Screen — coordinator.
//
// Core: ctor/dtor, show/hide events, setup_ui, setup_timers, update_clock,
// hub subscribe/unsubscribe, init_exchange, load_portfolio, save_state/restore_state,
// current_symbol / on_group_symbol_changed. Other concerns:
//   - CryptoTradingScreen_Handlers.cpp   — UI action handlers
//   - CryptoTradingScreen_Refresh.cpp    — per-panel refresh + WS feed-mode
//   - CryptoTradingScreen_AsyncFetch.cpp — REST fetch helpers
#include "screens/crypto_trading/CryptoTradingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
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
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
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

CryptoTradingScreen::CryptoTradingScreen(QWidget* parent) : QWidget(parent) {
    LOG_INFO(TAG, "Constructing CryptoTradingScreen");
    setup_ui();
    setup_timers();
    LOG_INFO(TAG, "CryptoTradingScreen construction complete");
}

CryptoTradingScreen::~CryptoTradingScreen() {
    LOG_INFO(TAG, "Destroying CryptoTradingScreen");
    // Direct WS connections are auto-cleaned via the ws_subscription_owner_
    // child QObject (parented to this); Qt drops them in the parent dtor.
}

// ============================================================================
// Visibility — start/stop timers
// ============================================================================

void CryptoTradingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // ── WS-only data path ──────────────────────────────────────────────────
    // ticker / orderbook / watchlist / portfolio polling timers are
    // intentionally NOT started here. The hub WS stream is the sole data
    // path. market_info_timer_ stays (perp funding/OI have no WS topic on
    // Kraken spot), clock_timer_ stays (wall clock), ws_flush_timer_ is the
    // 10fps UI flush coalescer, not data fetching.
    if (market_info_timer_)
        market_info_timer_->start();
    if (clock_timer_)
        clock_timer_->start();
    if (ws_flush_timer_)
        ws_flush_timer_->start();
    LOG_INFO(TAG, "Screen visible — WS-only mode (no REST polling)");
}

void CryptoTradingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (ticker_timer_)
        ticker_timer_->stop();
    if (ob_timer_)
        ob_timer_->stop();
    if (portfolio_timer_)
        portfolio_timer_->stop();
    if (watchlist_timer_)
        watchlist_timer_->stop();
    if (market_info_timer_)
        market_info_timer_->stop();
    if (live_data_timer_)
        live_data_timer_->stop();
    if (clock_timer_)
        clock_timer_->stop();
    if (ws_flush_timer_)
        ws_flush_timer_->stop();

    // Reset WS/state trackers so apply_feed_mode and update_clock re-emit
    // their state when the tab becomes visible again.
    last_ws_state_ = -1;
    last_ws_status_label_state_ = -1;

    // Drop any buffered WS updates — they'll be stale by next show and would
    // cause a spurious full-burst flush right after the screen reappears.
    pending_tickers_.clear();
    pending_primary_ticker_ = {};
    has_pending_primary_ = false;
    pending_orderbook_ = {};
    has_pending_orderbook_ = false;
    pending_candles_.clear();
    pending_trades_.clear();

    // Reset async-fetch guards — if a fetch was in-flight when we hid, its
    // reply will still fire (widget isn't destroyed, just hidden) and will
    // decrement live_inflight_; we just zero the counter so a spurious extra
    // decrement from a stuck task doesn't underflow into negative values.
    candles_fetching_.store(false);
    live_inflight_.store(0);
    // paper_bookkeeping_in_flight_ is NOT cleared — a worker thread may still
    // be running and will reset it via the invokeMethod callback when done.

    LOG_INFO(TAG, "Screen hidden — timers stopped, buffers cleared");
}

// ============================================================================
// UI Setup — 4-zone layout
// ============================================================================

void CryptoTradingScreen::setup_ui() {
    setObjectName("cryptoScreen");
    setStyleSheet(ui::styles::crypto_trading_styles());

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // ── COMMAND BAR (34px) ────────────────────────────────────────────────────
    auto* cmd_bar = new QWidget(this);
    cmd_bar->setObjectName("cryptoCommandBar");
    cmd_bar->setFixedHeight(34);
    auto* cmd_layout = new QHBoxLayout(cmd_bar);
    cmd_layout->setContentsMargins(8, 0, 8, 0);
    cmd_layout->setSpacing(6);

    // Exchange button + menu
    exchange_btn_ = new QPushButton("KRAKEN");
    exchange_btn_->setObjectName("cryptoExchangeBtn");
    exchange_btn_->setFixedHeight(22);
    exchange_btn_->setCursor(Qt::PointingHandCursor);
    exchange_menu_ = new QMenu(exchange_btn_);
    // Exchanges come from the single canonical list on ExchangeSessionManager,
    // which also drives the hub allow-list, topic patterns, and per-exchange
    // policies — so the dropdown can never drift out of sync with what the hub
    // actually publishes. Add a new exchange there (one line) and it appears here.
    for (const QString& ex : ExchangeSessionManager::supported_exchange_ids()) {
        exchange_menu_->addAction(ex, this, [this, ex]() { on_exchange_changed(ex); });
    }
    exchange_btn_->setMenu(exchange_menu_);
    cmd_layout->addWidget(exchange_btn_);

    // Separator
    auto* sep = new QLabel("|");
    sep->setObjectName("cryptoCommandBarSep");
    cmd_layout->addWidget(sep);

    // Symbol input with autocomplete
    symbol_input_ = new QLineEdit("BTC/USDT");
    symbol_input_->setObjectName("cryptoSymbolInput");
    symbol_input_->setFixedWidth(120);
    symbol_input_->setFixedHeight(22);
    auto* completer = new QCompleter(watchlist_symbols_, symbol_input_);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    symbol_input_->setCompleter(completer);

    // The completer popup is an unstyled top-level QListView — give it a solid
    // background (it would otherwise render transparent over the screen) and wire
    // activated() so a mouse click / Enter / Arrow+Enter on a suggestion selects
    // the symbol, not merely pastes it into the box.
    if (auto* popup = completer->popup()) {
        popup->setObjectName("cryptoSymbolCompleterPopup");
        popup->setStyleSheet(
            QString("QListView { background:%1; color:%2; border:1px solid %3; outline:none;"
                    " font-family:%4; font-size:%5px; }"
                    "QListView::item { padding:4px 8px; border:none; }"
                    "QListView::item:selected { background:%6; color:%2; }")
                .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::TEXT_PRIMARY(),
                     fincept::ui::colors::BORDER_MED(), fincept::ui::fonts::DATA_FAMILY())
                .arg(fincept::ui::fonts::SMALL)
                .arg(fincept::ui::colors::BG_HOVER()));
    }
    connect(completer, QOverload<const QString&>::of(&QCompleter::activated), this,
            [this](const QString& choice) { on_symbol_selected(choice.trimmed().toUpper()); });
    connect(symbol_input_, &QLineEdit::returnPressed, this,
            [this]() { on_symbol_selected(symbol_input_->text().trimmed().toUpper()); });
    cmd_layout->addWidget(symbol_input_);

    // Separator
    auto* sep2 = new QLabel("|");
    sep2->setObjectName("cryptoCommandBarSep");
    cmd_layout->addWidget(sep2);

    // Price ribbon (embedded ticker bar)
    ticker_bar_ = new CryptoTickerBar;
    cmd_layout->addWidget(ticker_bar_, 1);

    // WS status pill — three states (live / connecting / offline) driven by
    // a Qt property so the global stylesheet handles colors. Tooltip explains
    // what's happening so the user can self-diagnose without reading the log.
    ws_status_ = new QLabel(tr("CONNECTING"));
    ws_status_->setObjectName("cryptoWsStatus");
    ws_status_->setProperty("ws", "connecting");
    ws_status_->setToolTip(tr("WebSocket feed status — green=live, amber=connecting, red=offline (REST polling)"));
    cmd_layout->addWidget(ws_status_);

    // Transport hint — all exchanges stream via ws_stream.py (ccxt.pro).
    ws_transport_ = new QLabel(tr("DAEMON"));
    ws_transport_->setObjectName("cryptoWsTransport");
    ws_transport_->setToolTip(tr("ws_stream.py via ccxt.pro — Python subprocess"));
    cmd_layout->addWidget(ws_transport_);

    // Clock
    clock_label_ = new QLabel("--:--:--");
    clock_label_->setObjectName("cryptoClock");
    cmd_layout->addWidget(clock_label_);

    // API button
    api_btn_ = new QPushButton(tr("API"));
    api_btn_->setObjectName("cryptoApiBtn");
    api_btn_->setFixedHeight(22);
    api_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(api_btn_);

    // Mode button
    mode_btn_ = new QPushButton(tr("PAPER"));
    mode_btn_->setObjectName("cryptoModeBtn");
    mode_btn_->setProperty("mode", "paper");
    mode_btn_->setCheckable(true);
    mode_btn_->setFixedHeight(22);
    mode_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(mode_btn_);

    main_layout->addWidget(cmd_bar);

    // ── MAIN 3-PANEL SPLITTER ─────────────────────────────────────────────────
    auto* main_splitter = new QSplitter(Qt::Horizontal);
    main_splitter->setObjectName("cryptoMainSplitter");
    main_splitter->setHandleWidth(1);

    // LEFT: Watchlist
    watchlist_ = new CryptoWatchlist;
    watchlist_->set_symbols(watchlist_symbols_);
    watchlist_->set_active_symbol(selected_symbol_);
    main_splitter->addWidget(watchlist_);

    // CENTER: Chart (top) + Bottom Panel (bottom)
    auto* center_splitter = new QSplitter(Qt::Vertical);
    center_splitter->setObjectName("cryptoCenterSplitter");
    center_splitter->setHandleWidth(1);

    chart_ = new CryptoChart;
    center_splitter->addWidget(chart_);

    bottom_panel_ = new CryptoBottomPanel;
    center_splitter->addWidget(bottom_panel_);

    center_splitter->setStretchFactor(0, 5); // chart 75%
    center_splitter->setStretchFactor(1, 2); // bottom 25%

    main_splitter->addWidget(center_splitter);

    // RIGHT: Order Book (top) + Order Entry (bottom)
    auto* right_splitter = new QSplitter(Qt::Vertical);
    right_splitter->setObjectName("cryptoRightSplitter");
    right_splitter->setHandleWidth(1);

    orderbook_ = new CryptoOrderBook;
    right_splitter->addWidget(orderbook_);

    order_entry_ = new CryptoOrderEntry;
    right_splitter->addWidget(order_entry_);

    right_splitter->setStretchFactor(0, 3); // OB 55%
    right_splitter->setStretchFactor(1, 2); // OE 45%

    main_splitter->addWidget(right_splitter);

    // Splitter proportions: watchlist 220, center stretch, right 290
    main_splitter->setSizes({220, 600, 290});
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);

    main_layout->addWidget(main_splitter, 1);

    // ── Signal Connections ────────────────────────────────────────────────────
    connect(mode_btn_, &QPushButton::clicked, this, &CryptoTradingScreen::on_mode_toggled);
    connect(api_btn_, &QPushButton::clicked, this, &CryptoTradingScreen::on_api_clicked);
    connect(watchlist_, &CryptoWatchlist::symbol_selected, this, &CryptoTradingScreen::on_symbol_selected);
    connect(watchlist_, &CryptoWatchlist::search_requested, this, &CryptoTradingScreen::on_search_requested);
    connect(order_entry_, &CryptoOrderEntry::order_submitted, this, &CryptoTradingScreen::on_order_submitted);
    connect(order_entry_, &CryptoOrderEntry::leverage_changed, this, &CryptoTradingScreen::async_set_leverage);
    connect(order_entry_, &CryptoOrderEntry::margin_mode_changed, this, &CryptoTradingScreen::async_set_margin_mode);
    connect(orderbook_, &CryptoOrderBook::price_clicked, this, &CryptoTradingScreen::on_ob_price_clicked);
    connect(bottom_panel_, &CryptoBottomPanel::cancel_order_requested, this, &CryptoTradingScreen::on_cancel_order);
    connect(bottom_panel_, &CryptoBottomPanel::close_position_requested, this, &CryptoTradingScreen::on_close_position);
    connect(bottom_panel_, &CryptoBottomPanel::cancel_all_orders_requested, this,
            [this](const QString&) { on_cancel_all_orders(); });
    connect(bottom_panel_, &CryptoBottomPanel::close_all_positions_requested, this,
            [this](const QString&) { on_close_all_positions(); });
    connect(chart_, &CryptoChart::timeframe_changed, this,
            [this](const QString& tf) {
                ExchangeService::instance().set_ws_timeframe(tf);
                hub_subscribe_topics();  // re-point the OHLC subscription to the new tf
                async_fetch_candles(selected_symbol_, tf);
            });
}

// ============================================================================
// Timers
// ============================================================================

void CryptoTradingScreen::setup_timers() {
    ticker_timer_ = new QTimer(this);
    connect(ticker_timer_, &QTimer::timeout, this, &CryptoTradingScreen::refresh_ticker);
    ticker_timer_->setInterval(3000);

    ob_timer_ = new QTimer(this);
    connect(ob_timer_, &QTimer::timeout, this, &CryptoTradingScreen::refresh_orderbook);
    ob_timer_->setInterval(3000);

    portfolio_timer_ = new QTimer(this);
    connect(portfolio_timer_, &QTimer::timeout, this, &CryptoTradingScreen::refresh_portfolio);
    portfolio_timer_->setInterval(1500);

    watchlist_timer_ = new QTimer(this);
    connect(watchlist_timer_, &QTimer::timeout, this, &CryptoTradingScreen::refresh_watchlist);
    // 10 s — the watchlist is always polled in parallel with the WS stream
    // (see apply_feed_mode) because illiquid tickers don't tick often on WS.
    watchlist_timer_->setInterval(10000);

    market_info_timer_ = new QTimer(this);
    connect(market_info_timer_, &QTimer::timeout, this, &CryptoTradingScreen::refresh_market_info);
    market_info_timer_->setInterval(30000);

    live_data_timer_ = new QTimer(this);
    connect(live_data_timer_, &QTimer::timeout, this, &CryptoTradingScreen::refresh_live_data);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &CryptoTradingScreen::update_clock);
    clock_timer_->setInterval(CLOCK_UPDATE_MS);

    // WS update coalescing: flush accumulated data to UI at 10fps
    ws_flush_timer_ = new QTimer(this);
    connect(ws_flush_timer_, &QTimer::timeout, this, &CryptoTradingScreen::flush_ws_updates);
    ws_flush_timer_->setInterval(100); // 10fps

    // Defer to next event loop tick — start the WS stream and portfolio (both
    // cheap / local) immediately, then gate the *daemon-bound* fetches on the
    // daemon-ready signal so they don't race the cold boot and fall back to
    // raw-QProcess script calls (which each block a threadpool worker for up
    // to 30s). If the daemon never comes up, a single-shot timer still kicks
    // the fetches after 8s so the user sees degraded-but-not-empty data.
    QTimer::singleShot(0, this, [this]() {
        init_exchange();
        load_portfolio();
        // First WS tick from Kraken fills the price cache within ~1s of the
        // handshake; we no longer prime it via REST.

        auto* es = &trading::ExchangeService::instance();
        // Only one REST call survives the WS-only refactor: fetching the
        // historical OHLCV for the chart. Kraken WS only streams the *current*
        // candle (interval_begin = now); we still need the daemon's REST
        // fetch_ohlcv for the bars to the left of "now". After this single
        // call, the chart updates entirely from WS ohlc events.
        auto run_initial_fetches = [this]() {
            if (startup_fetches_done_)
                return;
            startup_fetches_done_ = true;
            async_fetch_candles(selected_symbol_, chart_->current_timeframe());
        };

        if (es->wait_for_daemon_ready(0)) {
            run_initial_fetches();
        } else {
            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn = connect(es, &trading::ExchangeService::daemon_ready, this, [conn, run_initial_fetches]() {
                QObject::disconnect(*conn);
                run_initial_fetches();
            });
            QTimer::singleShot(8000, this, [this, conn, run_initial_fetches]() {
                if (startup_fetches_done_)
                    return;
                QObject::disconnect(*conn);
                LOG_WARN(TAG, "Daemon not ready after 8s — chart history skipped (WS will fill in)");
                run_initial_fetches();
            });
        }
    });
}

void CryptoTradingScreen::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));

    // WS status pill — only restyle on state change. Setting a Qt property
    // and re-polishing is cheaper than setStyleSheet (no CSS reparse) but
    // still triggers a repaint, so we gate on edge transitions.
    const int connected = ExchangeService::instance().is_ws_connected() ? 1 : 0;
    if (connected == last_ws_status_label_state_)
        return;
    last_ws_status_label_state_ = connected;

    ws_status_->setText(connected ? tr("LIVE") : tr("OFFLINE"));
    ws_status_->setProperty("ws", connected ? "live" : "offline");
    ws_status_->style()->unpolish(ws_status_);
    ws_status_->style()->polish(ws_status_);
}

void CryptoTradingScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CryptoTradingScreen::retranslateUi() {
    if (api_btn_) api_btn_->setText(tr("API"));

    // Mode button reflects the live trading mode.
    if (mode_btn_)
        mode_btn_->setText(trading_mode_ == crypto::TradingMode::Live ? tr("LIVE") : tr("PAPER"));

    // WS status pill — re-apply the word matching the current state. -1 (unknown)
    // maps to the connecting placeholder.
    if (ws_status_) {
        if (last_ws_status_label_state_ == 1)      ws_status_->setText(tr("LIVE"));
        else if (last_ws_status_label_state_ == 0) ws_status_->setText(tr("OFFLINE"));
        else                                       ws_status_->setText(tr("CONNECTING"));
        ws_status_->setToolTip(
            tr("WebSocket feed status — green=live, amber=connecting, red=offline (REST polling)"));
    }

    // Transport hint — all exchanges stream via ws_stream.py (ccxt.pro).
    if (ws_transport_) {
        ws_transport_->setText(tr("DAEMON"));
        ws_transport_->setToolTip(tr("ws_stream.py via ccxt.pro — Python subprocess"));
    }
    // exchange_btn_ shows the exchange name (data); symbol_input_ holds the
    // selected pair (data); clock_label_ is a live timestamp — none translated.
}

// ============================================================================
// Init
// ============================================================================

// ============================================================================
// DataHub WS subscription — uniform live-data path for ALL exchanges.
//
// ExchangeSession runs the Python ws_stream.py (ccxt.pro) feed for every
// exchange and publishes each update on DataHub topics:
//   ws:<ex>:ticker:<pair> | :orderbook:<pair> | :trades:<pair> | :ohlc:<pair>:<interval>
// We subscribe to the explicit topics for the current exchange + symbol and
// route payloads into the same pending_* buffers that flush_ws_updates()
// drains at 10fps. Explicit per-topic subscriptions (not wildcard patterns)
// keep fan-out cheap even under high BBO rates; the producer additionally
// coalesces ticker at ~20Hz.
// ============================================================================

void CryptoTradingScreen::hub_subscribe_topics() {
    // Drop any prior connections to this owner (handles symbol/exchange swap).
    if (ws_subscription_owner_) {
        delete ws_subscription_owner_;
        ws_subscription_owner_ = nullptr;
    }

    // ws_subscription_owner_ is a context QObject the subscriptions are bound
    // to. Destroying it auto-cancels every subscription on swap.
    ws_subscription_owner_ = new QObject(this);
    auto& hub = fincept::datahub::DataHub::instance();
    const QString ex = exchange_id_;
    QPointer<CryptoTradingScreen> self = this;

    // Ticker — every watchlist symbol plus the selected one (drives the
    // watchlist prices and the primary last/bid/ask).
    QStringList ticker_syms = watchlist_symbols_;
    if (!ticker_syms.contains(selected_symbol_))
        ticker_syms << selected_symbol_;
    for (const QString& sym : ticker_syms) {
        hub.subscribe<fincept::trading::TickerData>(
            ws_subscription_owner_,
            QStringLiteral("ws:") + ex + QStringLiteral(":ticker:") + sym,
            [self](const fincept::trading::TickerData& t) {
                if (!self || t.symbol.isEmpty())
                    return;
                self->pending_tickers_[t.symbol] = t;
                if (t.symbol == self->selected_symbol_) {
                    self->pending_primary_ticker_ = t;
                    self->has_pending_primary_ = true;
                }
            });
    }

    // Orderbook — selected symbol only.
    hub.subscribe<fincept::trading::OrderBookData>(
        ws_subscription_owner_,
        QStringLiteral("ws:") + ex + QStringLiteral(":orderbook:") + selected_symbol_,
        [self](const fincept::trading::OrderBookData& ob) {
            if (!self || ob.symbol != self->selected_symbol_)
                return;
            self->pending_orderbook_ = ob;
            self->has_pending_orderbook_ = true;
        });

    // Trades — selected symbol only.
    hub.subscribe<fincept::trading::TradeData>(
        ws_subscription_owner_,
        QStringLiteral("ws:") + ex + QStringLiteral(":trades:") + selected_symbol_,
        [self](const fincept::trading::TradeData& td) {
            if (!self || td.symbol != self->selected_symbol_)
                return;
            crypto::TradeEntry e;
            e.side = td.side;
            e.price = td.price;
            e.amount = td.amount;
            self->pending_trades_.append(e);
        });

    // OHLC — selected symbol at the chart's current timeframe.
    const QString interval = chart_ ? chart_->current_timeframe() : QStringLiteral("1m");
    hub.subscribe<fincept::trading::Candle>(
        ws_subscription_owner_,
        QStringLiteral("ws:") + ex + QStringLiteral(":ohlc:") + selected_symbol_ +
            QLatin1Char(':') + interval,
        [self](const fincept::trading::Candle& c) {
            if (!self)
                return;
            self->pending_candles_.append(c);
        });

    LOG_INFO(TAG, QString("Hub WS subscriptions active (%1 / %2)").arg(ex, selected_symbol_));
}

void CryptoTradingScreen::hub_unsubscribe_topics() {
    if (ws_subscription_owner_) {
        delete ws_subscription_owner_;
        ws_subscription_owner_ = nullptr;
    }
    LOG_INFO(TAG, "Direct WS signals detached");
}

void CryptoTradingScreen::init_exchange() {
    auto& es = ExchangeService::instance();
    es.set_exchange(exchange_id_);

    if (!es.is_feed_running())
        es.start_price_feed(5);

    // Phase 3: only spawn the WS subprocess if this session doesn't already
    // have one running. On switch-back to a warm exchange, the session's WS
    // is still up (ExchangeSessionManager keeps sessions warm for the app
    // lifetime) and we should just attach our callbacks to its live stream.
    if (!es.is_ws_active()) {
        const bool ws_spawned = es.start_ws_stream(selected_symbol_, watchlist_symbols_);
        if (!ws_spawned) {
            // WS failed to launch (bad paths, missing broker creds, etc.). Surface
            // the state on the status label and keep REST polling so the screen
            // still displays data via scripts.
            LOG_ERROR(TAG, "WS stream failed to start — remaining on REST polling for " + exchange_id_);
            if (ws_status_) {
                ws_status_->setText(tr("OFFLINE"));
                ws_status_->setProperty("ws", "offline");
                ws_status_->style()->unpolish(ws_status_);
                ws_status_->style()->polish(ws_status_);
            }
            last_ws_status_label_state_ = 0;
        }
    } else {
        LOG_INFO(TAG, "WS already active for " + exchange_id_ + " — attaching to warm session");
    }

    // On an exchange switch the freshly-spawned ws_stream defaults to 1m OHLC;
    // if the chart is on a different timeframe, re-point the new stream to match.
    if (chart_ && chart_->current_timeframe() != QStringLiteral("1m"))
        es.set_ws_timeframe(chart_->current_timeframe());

    // Hub is the only consumer path. Every exchange shown in the dropdown
    // must have a matching producer registered on ExchangeSessionManager.
    hub_subscribe_topics();
    initialized_ = true;
    update_futures_visibility(); // reflect perp-ness of the initial market
    LOG_INFO(TAG, QString("Exchange initialised via hub: %1 / %2").arg(exchange_id_, selected_symbol_));
}

void CryptoTradingScreen::load_portfolio() {
    // pt_find_portfolio / pt_create_portfolio hit SQLite. Run on a worker so
    // the UI thread isn't blocked on exchange-switch or cold boot. Capture the
    // exchange id by value — if the user switches again before the worker
    // returns, the result will be applied under the *new* id, which is fine
    // because watch_symbol is called under the then-current selected_symbol_.
    QPointer<CryptoTradingScreen> self = this;
    const QString exch = exchange_id_;
    (void)QtConcurrent::run([self, exch]() {
        if (!self)
            return;
        trading::PtPortfolio portfolio;
        try {
            auto existing = pt_find_portfolio("Crypto Paper", exch);
            if (existing) {
                portfolio = *existing;
            } else {
                portfolio = pt_create_portfolio("Crypto Paper", DEFAULT_PAPER_BALANCE, "USD", 1.0, "cross", 0.001, exch);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("CryptoTradingScreen", QString("load_portfolio failed: %1").arg(e.what()));
            return;
        } catch (...) {
            LOG_ERROR("CryptoTradingScreen", "load_portfolio failed: unknown exception");
            return;
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, portfolio]() {
                if (!self)
                    return;
                self->portfolio_ = portfolio;
                self->portfolio_id_ = portfolio.id;
                ExchangeService::instance().watch_symbol(self->selected_symbol_, self->portfolio_id_);
                self->refresh_portfolio();
            },
            Qt::QueuedConnection);
    });
}

// ============================================================================
// Async Fetchers
// ============================================================================


QVariantMap CryptoTradingScreen::save_state() const {
    return {
        {"exchange_id", exchange_id_},
        {"selected_symbol", selected_symbol_},
    };
}

void CryptoTradingScreen::restore_state(const QVariantMap& state) {
    const QString exch = state.value("exchange_id", "kraken").toString();
    const QString sym = state.value("selected_symbol", "BTC/USDT").toString();

    const bool exch_changed = (exch != exchange_id_);
    const bool sym_changed = (sym != selected_symbol_);

    if (!exch_changed && !sym_changed)
        return;

    // When both change, avoid two teardown/re-subscribe cycles: set the
    // symbol first so the exchange-change path re-initializes on the NEW
    // symbol directly, and skip the follow-up switch_symbol call.
    if (exch_changed) {
        if (sym_changed) {
            selected_symbol_ = sym;
            symbol_input_->setText(sym);
            ticker_bar_->set_symbol(sym);
            order_entry_->set_symbol(sym);
            watchlist_->set_active_symbol(sym);
        }
        on_exchange_changed(exch);
        return;
    }

    if (sym_changed)
        on_symbol_selected(sym);
}

} // namespace fincept::screens
