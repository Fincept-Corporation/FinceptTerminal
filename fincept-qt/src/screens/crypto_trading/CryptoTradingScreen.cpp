// Crypto Trading Screen — coordinator.
//
// Core: ctor/dtor, show/hide events, setup_ui, setup_timers, update_clock,
// hub subscribe/unsubscribe, init_exchange, load_portfolio, save_state/restore_state,
// current_symbol / on_group_symbol_changed. Other concerns:
//   - CryptoTradingScreen_Handlers.cpp   — UI action handlers
//   - CryptoTradingScreen_Refresh.cpp    — per-panel refresh + WS feed-mode
//   - CryptoTradingScreen_AsyncFetch.cpp — REST fetch helpers
#include "screens/crypto_trading/CryptoTradingScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/common/CommandRows.h"
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
#include "python/PythonSetupManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QCoreApplication>
#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QPointer>
#include <QSplitter>
#include <QStringListModel>
#include <QStyle>
#include <QThread>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <utility>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::crypto;

static const QString TAG = "CryptoTrading";

CryptoTradingScreen::CryptoTradingScreen(QWidget* parent) : QWidget(parent) {
    LOG_DEBUG(TAG, "Constructing CryptoTradingScreen");
    setup_ui();
    setup_timers();
    // The header search box's "buy 0.5 btc": prefill this window's ticket (never sent).
    EventBus::instance().subscribe(this, QStringLiteral("crypto.open_order_ticket"),
                                   [this](const QVariantMap& d) { on_external_order_ticket(d); });
    LOG_DEBUG(TAG, "CryptoTradingScreen construction complete");
}

CryptoTradingScreen::~CryptoTradingScreen() {
    LOG_DEBUG(TAG, "Destroying CryptoTradingScreen");
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
    // Re-attach the hub WS subscriptions dropped by hideEvent (§D3). Same
    // topic set: hub_subscribe_topics() derives every topic from the current
    // exchange_id_ / selected_symbol_ / watchlist_symbols_ / chart timeframe,
    // none of which hideEvent touches. Gated on initialized_ so the very
    // first show doesn't subscribe ahead of init_exchange() — that deferred
    // call owns the initial subscribe and would immediately tear this one
    // down anyway.
    if (initialized_)
        hub_subscribe_topics();
    // Restart the live positions/P&L poller if we're in LIVE mode. hideEvent
    // stopped it and it was only ever (re)started from the mode toggle, so
    // tabbing away and back froze live P&L until the mode was re-toggled.
    if (live_data_timer_ && trading_mode_ == crypto::TradingMode::Live)
        live_data_timer_->start(5000);
    LOG_DEBUG(TAG, "Screen visible — WS-only mode (no REST polling)");
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

    // Drop the hub WS subscriptions (§D3). Stopping ws_flush_timer_ above only
    // stops the *drain* — the subscription callbacks keep appending to
    // pending_trades_ / pending_candles_ at exchange tick rate for as long as
    // the screen stays hidden, so the buffers grew without bound and the first
    // flush after re-showing dumped the whole accumulation into bottom_panel_
    // in one multi-second freeze. showEvent re-subscribes the same topic set.
    hub_unsubscribe_topics();

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

    LOG_DEBUG(TAG, "Screen hidden — timers stopped, buffers cleared");
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
    cmd_bar->setMinimumHeight(34);
    auto* cmd_layout = new QHBoxLayout(cmd_bar);
    cmd_layout->setContentsMargins(8, 0, 8, 0);
    cmd_layout->setSpacing(6);

    // Exchange button + menu
    exchange_btn_ = new QPushButton("KRAKEN");
    exchange_btn_->setObjectName("cryptoExchangeBtn");
    exchange_btn_->setMinimumHeight(22);
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
    // 120 px at the default font size; wide enough for a 9-character pair at any size.
    const auto fit_symbol_input = [this]() {
        const QFontMetrics fm(ui::fonts::font(-1, QFont::Bold));
        symbol_input_->setFixedWidth(std::max(120, fm.horizontalAdvance(QStringLiteral("XXXXXXXXX")) + 20));
    };
    fit_symbol_input();
    // A width from the font: re-fitted once the new appearance has settled (a
    // geometry change inside theme_changed makes Qt lay the window out mid-restyle).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, symbol_input_,
            [fit_symbol_input]() { fit_symbol_input(); });
    symbol_input_->setMinimumHeight(22);
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
        popup->setStyleSheet(QString("QListView { background:%1; color:%2; border:1px solid %3; outline:none;"
                                     " font-family:%4; font-size:%5px; }"
                                     "QListView::item { padding:4px 8px; border:none; }"
                                     "QListView::item:selected { background:%6; color:%2; }")
                                 .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::TEXT_PRIMARY(),
                                      fincept::ui::colors::BORDER_MED(), fincept::ui::fonts::DATA_FAMILY())
                                 .arg(fincept::ui::fonts::SMALL.get())
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

    // Why live prices are down, in words (ExchangeSession::ws_failed): a missing
    // streaming library or an exchange the library cannot stream used to reach
    // only the log while the pill just said OFFLINE. Cleared on reconnect.
    ws_error_label_ = new QLabel;
    ws_error_label_->setObjectName("cryptoOeStatus");
    ws_error_label_->setProperty("severity", "error");
    ws_error_label_->setVisible(false);
    ws_error_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cmd_layout->addWidget(ws_error_label_);

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
    api_btn_->setMinimumHeight(22);
    api_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(api_btn_);

    // Mode button
    mode_btn_ = new QPushButton(tr("PAPER"));
    mode_btn_->setObjectName("cryptoModeBtn");
    mode_btn_->setProperty("mode", "paper");
    mode_btn_->setProperty("fincept_agent_risk", "account"); // paper <-> live switch
    mode_btn_->setCheckable(true);
    mode_btn_->setMinimumHeight(22);
    mode_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(mode_btn_);

    main_layout->addWidget(cmd_bar);

    // ── MAIN 3-PANEL SPLITTER ─────────────────────────────────────────────────
    // Built in place: the splitter joins the screen before it is filled and
    // every panel is created inside its final parent (moving finished panels
    // into a new parent re-computes the stylesheet of every widget in them).
    auto* main_splitter = new QSplitter(Qt::Horizontal, this);
    main_splitter->setObjectName("cryptoMainSplitter");
    main_splitter->setHandleWidth(1);
    main_layout->addWidget(main_splitter, 1);

    // LEFT: Watchlist
    watchlist_ = new CryptoWatchlist(main_splitter);
    watchlist_->set_symbols(watchlist_symbols_);
    watchlist_->set_active_symbol(selected_symbol_);
    main_splitter->addWidget(watchlist_);

    // CENTER: Chart (top) + Bottom Panel (bottom)
    auto* center_splitter = new QSplitter(Qt::Vertical, main_splitter);
    center_splitter->setObjectName("cryptoCenterSplitter");
    center_splitter->setHandleWidth(1);
    main_splitter->addWidget(center_splitter);

    chart_ = new CryptoChart(center_splitter);
    chart_->set_loading_text(tr("Loading %1 %2 candles from %3…")
                                 .arg(selected_symbol_, chart_->current_timeframe(), exchange_id_.toUpper()));
    center_splitter->addWidget(chart_);

    bottom_panel_ = new CryptoBottomPanel(center_splitter);
    center_splitter->addWidget(bottom_panel_);

    center_splitter->setStretchFactor(0, 5); // chart 75%
    center_splitter->setStretchFactor(1, 2); // bottom 25%

    // RIGHT: Order Book (top) + Order Entry (bottom)
    auto* right_splitter = new QSplitter(Qt::Vertical, main_splitter);
    right_splitter->setObjectName("cryptoRightSplitter");
    right_splitter->setHandleWidth(1);
    main_splitter->addWidget(right_splitter);

    // The book in a feed-mode pane: a skeleton until the first snapshot, the
    // stream's error (RETRY) when it is down, the Python setup page when the
    // environment is not ready — never an empty grid of headers.
    ob_pane_ = new ui::AutoLoadPane(right_splitter);
    ob_pane_->set_feed_mode(true);
    ob_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::List);
    ob_pane_->set_loading_text(
        tr("Waiting for the %1 order book from %2…").arg(selected_symbol_, exchange_id_.toUpper()));
    ob_pane_->set_requirements({ui::Requirement::python()});
    orderbook_ = new CryptoOrderBook(ob_pane_->content_parent());
    ob_pane_->set_content(orderbook_);
    right_splitter->addWidget(ob_pane_);

    order_entry_ = new CryptoOrderEntry(right_splitter);
    right_splitter->addWidget(order_entry_);

    right_splitter->setStretchFactor(0, 3); // OB 55%
    right_splitter->setStretchFactor(1, 2); // OE 45%

    // Every price widget prints the initial pair's quote asset until its market resolves.
    apply_price_market();

    // Splitter proportions: watchlist 220, center stretch, right 290
    main_splitter->setSizes({220, 600, 290});
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);

    // ── Signal Connections ────────────────────────────────────────────────────
    connect(mode_btn_, &QPushButton::clicked, this, &CryptoTradingScreen::on_mode_toggled);
    connect(api_btn_, &QPushButton::clicked, this, &CryptoTradingScreen::on_api_clicked);
    connect(watchlist_, &CryptoWatchlist::symbol_selected, this, &CryptoTradingScreen::on_symbol_selected);
    connect(watchlist_, &CryptoWatchlist::search_requested, this, &CryptoTradingScreen::on_search_requested);
    connect(order_entry_, &CryptoOrderEntry::order_submitted, this, &CryptoTradingScreen::on_order_submitted);
    // Leverage / margin mode reach the account only through APPLY + confirmation.
    connect(order_entry_, &CryptoOrderEntry::apply_leverage_requested, this, &CryptoTradingScreen::on_apply_leverage);
    connect(order_entry_, &CryptoOrderEntry::market_choice_requested, this, &CryptoTradingScreen::on_market_choice);
    connect(orderbook_, &CryptoOrderBook::price_clicked, this, &CryptoTradingScreen::on_ob_price_clicked);
    connect(bottom_panel_, &CryptoBottomPanel::cancel_order_requested, this, &CryptoTradingScreen::on_cancel_order);
    connect(bottom_panel_, &CryptoBottomPanel::close_position_requested, this, &CryptoTradingScreen::on_close_position);
    connect(bottom_panel_, &CryptoBottomPanel::cancel_all_orders_requested, this,
            [this](const QString&) { on_cancel_all_orders(); });
    connect(bottom_panel_, &CryptoBottomPanel::close_all_positions_requested, this,
            [this](const QString&) { on_close_all_positions(); });
    // ── Never-blank panes: the chart plot and the book ask for data when shown ──
    connect(chart_, &CryptoChart::feed_wanted, this, &CryptoTradingScreen::ensure_market_feed);
    ob_pane_->set_feed_subscription([this]() { ensure_market_feed(std::exchange(ob_feed_retry_, false)); },
                                    [this]() { ob_feed_retry_ = ob_pane_->isVisible(); });
    // Python finished setting up while the setup page shows: load now.
    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::core_ready, this, [this]() {
        chart_->recheck_requirements();
        ob_pane_->recheck_requirements();
    });

    // ── Bottom panel designed states → their actions (none places or changes anything) ──
    connect(bottom_panel_, &CryptoBottomPanel::order_ticket_requested, this,
            [this]() { order_entry_->focus_quantity(); });
    connect(bottom_panel_, &CryptoBottomPanel::live_refresh_requested, this, [this]() {
        push_key_state();
        refresh_live_data();
    });
    connect(bottom_panel_, &CryptoBottomPanel::api_key_requested, this, &CryptoTradingScreen::on_api_clicked);
    connect(bottom_panel_, &CryptoBottomPanel::credentials_retry_requested, this, [this]() {
        // "Stored API keys unreadable — RETRY": read them again (never asks for new ones).
        if (auto* s = session())
            s->load_stored_credentials();
        push_key_state();
        if (trading_mode_ == TradingMode::Live)
            refresh_live_data();
    });
    connect(bottom_panel_, &CryptoBottomPanel::fees_wanted, this, &CryptoTradingScreen::maybe_fetch_fees);
    connect(bottom_panel_, &CryptoBottomPanel::market_info_wanted, this, [this]() {
        if (initialized_ && startup_fetches_done_ &&
            market_info_key_ != exchange_id_ + QLatin1Char('|') + selected_symbol_)
            refresh_market_info();
    });
    connect(bottom_panel_, &CryptoBottomPanel::paper_retry_requested, this, &CryptoTradingScreen::load_portfolio);

    connect(chart_, &CryptoChart::timeframe_changed, this, [this](const QString& tf) {
        // Bars of the old timeframe (on screen, or buffered from the WS) must not
        // be mixed into the new one: a 1m bar appended to a 1h series is a bogus
        // candle until the REST history replaces the set.
        chart_->set_loading_text(
            tr("Loading %1 %2 candles from %3…").arg(selected_symbol_, tf, exchange_id_.toUpper()));
        chart_->clear();
        chart_symbol_.clear();
        pending_candles_.clear();
        session()->set_ws_timeframe(tf);
        hub_subscribe_topics(); // re-point the OHLC subscription to the new tf
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
            async_seed_trade_tape();     // recent prints, so the tape is not empty until the first WS trade
            async_fetch_market_limits(); // order minimums for the ticket
            // Funding / mark / open interest once now (the 30 s timer's first tick is
            // 30 s away) when they are on screen: the MARKET tab, or a perp's ribbon.
            if (bottom_panel_->market_view_visible() || is_perp_market())
                refresh_market_info();
            if (bottom_panel_->fee_view_visible())
                maybe_fetch_fees(false); // FEES / MARKET opened before the daemon was ready
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
    const int connected = session()->is_ws_connected() ? 1 : 0;
    if (connected == last_ws_status_label_state_)
        return;
    last_ws_status_label_state_ = connected;
    if (connected && ws_error_label_)
        ws_error_label_->setVisible(false); // the stream is back: the failure no longer applies

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
    if (api_btn_)
        api_btn_->setText(tr("API"));

    // Mode button reflects the live trading mode.
    if (mode_btn_)
        mode_btn_->setText(trading_mode_ == crypto::TradingMode::Live ? tr("LIVE") : tr("PAPER"));

    // WS status pill — re-apply the word matching the current state. -1 (unknown)
    // maps to the connecting placeholder.
    if (ws_status_) {
        if (last_ws_status_label_state_ == 1)
            ws_status_->setText(tr("LIVE"));
        else if (last_ws_status_label_state_ == 0)
            ws_status_->setText(tr("OFFLINE"));
        else
            ws_status_->setText(tr("CONNECTING"));
        ws_status_->setToolTip(tr("WebSocket feed status — green=live, amber=connecting, red=offline (REST polling)"));
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
    auto& hub = fincept::datahub::DataHub::instance();

    // Drop any prior subscriptions (handles symbol/exchange swap). This runs
    // from signal handlers (chart timeframe change, symbol switch) that can
    // themselves be reached from a hub dispatch, so per P13 the owner is
    // retired with deleteLater() rather than `delete`. Unsubscribing first
    // means the deferred destruction can't deliver a stale tick in between.
    if (ws_subscription_owner_) {
        hub.unsubscribe(ws_subscription_owner_);
        ws_subscription_owner_->deleteLater();
        ws_subscription_owner_ = nullptr;
    }

    // ws_subscription_owner_ is a context QObject the subscriptions are bound
    // to. Destroying it auto-cancels every subscription on swap.
    ws_subscription_owner_ = new QObject(this);
    const QString ex = exchange_id_;
    QPointer<CryptoTradingScreen> self = this;

    // Ticker — every watchlist symbol plus the selected one (drives the
    // watchlist prices and the primary last/bid/ask).
    QStringList ticker_syms = watchlist_symbols_;
    if (!ticker_syms.contains(selected_symbol_))
        ticker_syms << selected_symbol_;
    for (const QString& sym : ticker_syms) {
        hub.subscribe<fincept::trading::TickerData>(ws_subscription_owner_,
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
        ws_subscription_owner_, QStringLiteral("ws:") + ex + QStringLiteral(":orderbook:") + selected_symbol_,
        [self](const fincept::trading::OrderBookData& ob) {
            if (!self || ob.symbol != self->selected_symbol_)
                return;
            self->pending_orderbook_ = ob;
            self->has_pending_orderbook_ = true;
        });

    // Trades — selected symbol only. ExchangeSessionManager publishes one
    // QVector<TradeData> per ~100 ms window (arrival order) so no print is lost
    // to coalescing; a single TradeData payload is still accepted.
    const auto push_trade = [self](const fincept::trading::TradeData& td) {
        if (!self || td.symbol != self->selected_symbol_)
            return;
        crypto::TradeEntry e;
        e.id = td.id;
        e.side = td.side;
        e.price = td.price;
        e.amount = td.amount;
        // The tape's TIME column renders e.timestamp; leaving it at the 0
        // default stamped every row 00:00:00 (epoch). Fall back to receive
        // time if the exchange sent none.
        e.timestamp = td.timestamp > 0 ? td.timestamp : QDateTime::currentMSecsSinceEpoch();
        // Hard cap — belt and braces against the drain ever stalling
        // (hidden screen, blocked event loop). Newest wins: the time &
        // sales tape only ever renders the most recent rows anyway.
        if (self->pending_trades_.size() >= kMaxPendingTrades)
            self->pending_trades_.remove(0, self->pending_trades_.size() - kMaxPendingTrades + 1);
        self->pending_trades_.append(e);
    };
    hub.subscribe(ws_subscription_owner_, QStringLiteral("ws:") + ex + QStringLiteral(":trades:") + selected_symbol_,
                  [push_trade](const QVariant& v) {
                      if (v.metaType() == QMetaType::fromType<QVector<fincept::trading::TradeData>>()) {
                          const auto batch = v.value<QVector<fincept::trading::TradeData>>();
                          for (const auto& td : batch)
                              push_trade(td);
                      } else if (v.canConvert<fincept::trading::TradeData>()) {
                          push_trade(v.value<fincept::trading::TradeData>());
                      }
                  });

    // OHLC — selected symbol at the chart's current timeframe.
    const QString interval = chart_ ? chart_->current_timeframe() : QStringLiteral("1m");
    hub.subscribe<fincept::trading::Candle>(ws_subscription_owner_,
                                            QStringLiteral("ws:") + ex + QStringLiteral(":ohlc:") + selected_symbol_ +
                                                QLatin1Char(':') + interval,
                                            [self](const fincept::trading::Candle& c) {
                                                if (!self)
                                                    return;
                                                // Same cap rationale as pending_trades_ above.
                                                if (self->pending_candles_.size() >= kMaxPendingCandles)
                                                    self->pending_candles_.remove(
                                                        0, self->pending_candles_.size() - kMaxPendingCandles + 1);
                                                self->pending_candles_.append(c);
                                            });

    LOG_DEBUG(TAG, QString("Hub WS subscriptions active (%1 / %2)").arg(ex, selected_symbol_));
}

void CryptoTradingScreen::hub_unsubscribe_topics() {
    if (ws_subscription_owner_) {
        // Same reasoning as hub_subscribe_topics(): drop the subscriptions
        // synchronously, destroy the context object on the next event-loop
        // turn (P13 — never `delete` a QObject that may be inside a dispatch).
        fincept::datahub::DataHub::instance().unsubscribe(ws_subscription_owner_);
        ws_subscription_owner_->deleteLater();
        ws_subscription_owner_ = nullptr;
    }
    LOG_DEBUG(TAG, "Direct WS signals detached");
}

trading::ExchangeSession* CryptoTradingScreen::session() const {
    return ExchangeSessionManager::instance().session(exchange_id_);
}

void CryptoTradingScreen::connect_session_signals() {
    // One context object per attached session: replacing it drops the old
    // session's connections in one move on an exchange switch.
    if (session_conn_owner_) {
        session_conn_owner_->deleteLater();
        session_conn_owner_ = nullptr;
    }
    session_conn_owner_ = new QObject(this);
    if (ws_error_label_)
        ws_error_label_->setVisible(false);
    auto* s = session();
    if (!s)
        return;
    QPointer<CryptoTradingScreen> self = this;
    connect(s, &ExchangeSession::ws_failed, session_conn_owner_, [self](const QString& ex, const QString& message) {
        if (!self || ex != self->exchange_id_ || !self->ws_error_label_)
            return;
        // Long reasons are elided in the bar; the tooltip carries the whole text.
        const QFontMetrics fm(self->ws_error_label_->font());
        const int max_px = std::max(240, fm.horizontalAdvance(QLatin1Char('M')) * 40);
        self->ws_error_label_->setText(fm.elidedText(message, Qt::ElideRight, max_px));
        self->ws_error_label_->setToolTip(message);
        self->ws_error_label_->setVisible(true);
        if (self->ws_status_)
            self->ws_status_->setToolTip(message);
        // The chart and the book say it too: an error page (RETRY) when they hold
        // nothing yet, a "live updates stopped" notice over what they show.
        self->report_live_feed(message);
    });
    connect(s, &ExchangeSession::ws_state_changed, session_conn_owner_, [self](const QString& ex) {
        if (!self || ex != self->exchange_id_)
            return;
        if (self->session()->is_ws_connected() && self->ws_error_label_) {
            self->ws_error_label_->setVisible(false);
            if (self->ws_status_)
                self->ws_status_->setToolTip(
                    tr("WebSocket feed status — green=live, amber=connecting, red=offline (REST polling)"));
            self->report_live_feed(QString());
        }
    });
}

void CryptoTradingScreen::report_live_feed(const QString& message) {
    if (chart_)
        chart_->set_live_error(message);
    if (bottom_panel_)
        bottom_panel_->set_live_feed_error(message); // empty T&S tape / depth chart say why
    if (!ob_pane_ || message.isEmpty())
        return; // back up: the next snapshot clears the book's notice (feed_data)
    ob_feed_reported_ = false;
    ob_pane_->feed_error(message, QString(), tr("Live order book unavailable"));
}

void CryptoTradingScreen::reset_market_panes() {
    const QString ex = exchange_id_.toUpper();
    // The bottom panel drops its fee schedule and MARKET figures on a switch
    // (reset_market_data): whatever was fetched before must be fetched again.
    fees_key_.clear();
    market_info_key_.clear();
    if (chart_)
        chart_->set_loading_text(
            tr("Loading %1 %2 candles from %3…").arg(selected_symbol_, chart_->current_timeframe(), ex));
    ob_feed_reported_ = false;
    if (ob_pane_) {
        ob_pane_->set_loading_text(tr("Waiting for the %1 order book from %2…").arg(selected_symbol_, ex));
        ob_pane_->clear_result(); // back to the skeleton until the new pair's first snapshot
    }
}

void CryptoTradingScreen::ensure_market_feed(bool retry) {
    // Public market data only (history + stream). Nothing is placed, changed or
    // connected to an account here.
    if (!initialized_)
        return; // init_exchange() owns the first subscription and history fetch
    if (retry) {
        auto* s = session();
        if (s && !s->is_ws_active() && !s->is_ws_wanted()) {
            // The stream gave up (a fatal error or its respawn budget): the user asked again.
            report_live_feed(QString());
            if (ws_error_label_)
                ws_error_label_->setVisible(false);
            if (s->start_ws(selected_symbol_, watchlist_symbols_)) {
                if (chart_ && chart_->current_timeframe() != QStringLiteral("1m"))
                    s->set_ws_timeframe(chart_->current_timeframe());
                hub_subscribe_topics();
            }
        }
    }
    // No history drawn (first show after a failure, RETRY): fetch it once for this visit.
    if (startup_fetches_done_ && chart_ && chart_symbol_.isEmpty())
        async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void CryptoTradingScreen::push_key_state() {
    auto* s = session();
    if (!s || !bottom_panel_)
        return;
    const ExchangeCredentials c = s->get_credentials();
    const bool has_keys = !c.api_key.isEmpty() || !c.private_key.isEmpty() || !c.wallet_address.isEmpty();
    bottom_panel_->set_live_key_state(exchange_id_.toUpper(), has_keys, s->credentials_read_error());
}

void CryptoTradingScreen::maybe_fetch_fees(bool force) {
    // After the start-up gate: before the exchange daemon is up a call would fall
    // back to a one-off script run (run_initial_fetches asks again once it is).
    if (!initialized_ || !startup_fetches_done_ || !bottom_panel_)
        return;
    const QString key = exchange_id_ + QLatin1Char('|') + selected_symbol_ + QLatin1Char('|') +
                        (trading_mode_ == TradingMode::Live ? QStringLiteral("live") : QStringLiteral("paper"));
    if (!force && key == fees_key_)
        return; // already loaded / loading for this exchange, pair and mode
    fees_key_ = key;
    bottom_panel_->set_fees_loading();
    async_fetch_trading_fees();
}

void CryptoTradingScreen::init_exchange() {
    auto& es = ExchangeService::instance();
    // The facade's "current exchange" now only drives the paper-trading price
    // feed; this screen's own calls go to session() (see the header).
    es.set_exchange(exchange_id_);

    if (!es.is_feed_running())
        es.start_price_feed(5);

    auto* sess = session();
    connect_session_signals();
    order_entry_->set_venue(exchange_id_);

    // Phase 3: only spawn the WS subprocess if this session doesn't already
    // have one running. On switch-back to a warm exchange, the session's WS
    // is still up (ExchangeSessionManager keeps sessions warm for the app
    // lifetime) and we should just attach our callbacks to its live stream.
    // A stream the DataHub demand path started is taken over (its pairs are
    // folded into the launch list by start_ws).
    if (!(sess->is_ws_active() && !sess->is_ws_hub_owned())) {
        const bool ws_spawned = sess->start_ws(selected_symbol_, watchlist_symbols_);
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
        LOG_DEBUG(TAG, "WS already active for " + exchange_id_ + " — attaching to warm session");
    }

    // On an exchange switch the freshly-spawned ws_stream defaults to 1m OHLC;
    // if the chart is on a different timeframe, re-point the new stream to match.
    if (chart_ && chart_->current_timeframe() != QStringLiteral("1m"))
        sess->set_ws_timeframe(chart_->current_timeframe());

    // Hub is the only consumer path. Every exchange shown in the dropdown
    // must have a matching producer registered on ExchangeSessionManager.
    hub_subscribe_topics();
    initialized_ = true;
    update_futures_visibility(); // reflect perp-ness of the initial market
    push_key_state();            // what the LIVE views (and MY TRADES) say about this exchange's keys
    LOG_DEBUG(TAG, QString("Exchange initialised via hub: %1 / %2").arg(exchange_id_, selected_symbol_));
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
        QString failure; // set when the paper account could not be opened
        try {
            auto existing = pt_find_portfolio("Crypto Paper", exch);
            if (existing) {
                portfolio = *existing;
            } else {
                // The paper book's cash currency, fixed when the portfolio is created
                // (an existing one keeps its own). Pairs quoted in another asset (USDT,
                // EUR, BTC…) are booked into it at a real rate (prepare_symbol_fx).
                portfolio = pt_create_portfolio("Crypto Paper", DEFAULT_PAPER_BALANCE, QStringLiteral("USD"), 1.0,
                                                "cross", 0.001, exch);
            }
        } catch (const std::exception& e) {
            failure = QString::fromUtf8(e.what());
            LOG_ERROR("CryptoTradingScreen", fincept::logf("load_portfolio failed", {{"exchange", exch},
                                                                                     {"portfolio", "Crypto Paper"},
                                                                                     {"error", failure}}));
        } catch (...) {
            failure = QStringLiteral("unknown exception");
            LOG_ERROR("CryptoTradingScreen", fincept::logf("load_portfolio failed", {{"exchange", exch},
                                                                                     {"portfolio", "Crypto Paper"},
                                                                                     {"error", failure}}));
        }
        if (!failure.isEmpty()) {
            if (!self)
                return;
            // POSITIONS / ORDERS / HISTORY said "loading" — say why, with RETRY.
            QMetaObject::invokeMethod(
                self,
                [self, exch, failure]() {
                    if (!self || self->exchange_id_ != exch)
                        return;
                    self->bottom_panel_->set_paper_unavailable(
                        tr("The paper account for %1 could not be opened: %2").arg(exch.toUpper(), failure));
                },
                Qt::QueuedConnection);
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

// ============================================================================
// Deep links + the search box's order ticket
// ============================================================================

namespace {

/// The top-level window `w` lives in, through floating dock containers and dialogs (each is
/// parented, transitively, to its WindowFrame) — how the shell resolves the active window.
QWidget* ct_frame_of(QWidget* w) {
    while (w && w->parentWidget())
        w = w->parentWidget();
    return w;
}

/// The ack token of the last ticket request a Crypto Trading panel took: one panel acts.
QString& ct_taken_ticket_token() {
    static QString token;
    return token;
}

} // namespace

bool CryptoTradingScreen::open_section(const QString& section, const QString& focus) {
    const QString key = section.trimmed().toLower();
    const QString f = focus.trimmed();
    if (key.isEmpty()) {
        // A pair alone: what typing it into the symbol box does.
        if (f.contains(QLatin1Char('/')) && f != selected_symbol_)
            on_symbol_selected(f);
        return true;
    }
    if (key == QLatin1String("ticket")) {
        // After the shell has finished its navigation (it may still be raising the panel).
        QPointer<CryptoTradingScreen> self(this);
        QMetaObject::invokeMethod(
            this,
            [self]() {
                if (self && self->order_entry_)
                    self->order_entry_->focus_quantity();
            },
            Qt::QueuedConnection);
        return true;
    }
    return bottom_panel_ && bottom_panel_->show_section(key);
}

void CryptoTradingScreen::on_external_order_ticket(const QVariantMap& request) {
    // The box publishes on the GUI thread and waits for the ack in the same call; a publish
    // from anywhere else is not for a widget.
    if (QThread::currentThread() != thread())
        return;
    // Only one window acts: the one the request names ("window", the box's own frame, when the
    // publisher adds it), else the window the user works in (the shell's rule for
    // equity.open_order_ticket: the active window, resolved to its frame). With no such window,
    // or no Crypto Trading in it, nobody acknowledges and the box opens Crypto Trading on the pair
    // instead (nav.open_symbol, routed by the shell to its target window).
    QWidget* target = qobject_cast<QWidget*>(request.value(QStringLiteral("window")).value<QObject*>());
    if (!target)
        target = QApplication::activeWindow();
    if (!target || ct_frame_of(target) != ct_frame_of(this))
        return;
    if (!initialized_ || !order_entry_)
        return; // never shown in this window yet: the box's fallback opens it on the pair
    const QString token = request.value(QStringLiteral("ack_token")).toString();
    if (!token.isEmpty() && ct_taken_ticket_token() == token)
        return; // another Crypto Trading panel of this window took it
    const QString pair = request.value(QStringLiteral("symbol")).toString().trimmed();
    const QString exchange = request.value(QStringLiteral("exchange")).toString().trimmed();
    if (!pair.contains(QLatin1Char('/')))
        return; // not a BASE/QUOTE pair: the fallback resolves it
    if (!exchange.isEmpty() && exchange.compare(exchange_id_, Qt::CaseInsensitive) != 0)
        return; // another venue: a link never switches the exchange
    if (pair != selected_symbol_)
        on_symbol_selected(pair); // the symbol box's own path (fresh ticket, linked group)
    if (selected_symbol_ != pair)
        return;
    if (!token.isEmpty())
        ct_taken_ticket_token() = token;

    // Filled in as typed; the BUY / SELL button (and LIVE's confirmation) stay in charge.
    order_entry_->prefill(request.value(QStringLiteral("is_buy")).toBool(),
                          request.value(QStringLiteral("quantity")).toDouble(),
                          request.value(QStringLiteral("order_type")).toString(),
                          request.value(QStringLiteral("limit_price")).toDouble());
    // Behind another panel of this window: bring it forward without closing anything.
    if (!isVisible())
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                     {{QStringLiteral("screen_id"), QStringLiteral("crypto_trading")},
                                      {QStringLiteral("exclusive"), false}});
    // Queued: after the box's popup has closed (and after a raise above).
    QPointer<CryptoTradingScreen> self(this);
    QMetaObject::invokeMethod(
        this,
        [self]() {
            if (self && self->order_entry_)
                self->order_entry_->focus_quantity();
        },
        Qt::QueuedConnection);
    LOG_INFO(TAG, "Order ticket prefilled from the search box"); // never the order details
    if (!token.isEmpty())
        EventBus::instance().publish(QStringLiteral("crypto.open_order_ticket.ack"),
                                     {{QStringLiteral("token"), token}});
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> ct_command_rows() {
    namespace rows = command_rows;
    const QString screen = QStringLiteral("crypto_trading");
    const QString where = CryptoTradingScreen::tr("Crypto Trading");
    QList<command::CommandItem> out = {
        rows::section(screen, QStringLiteral("positions"), CryptoTradingScreen::tr("Crypto positions"),
                      {QStringLiteral("positions"), QStringLiteral("open positions"), QStringLiteral("p&l"),
                       QStringLiteral("holdings")},
                      where),
        rows::section(screen, QStringLiteral("orders"), CryptoTradingScreen::tr("Crypto open orders"),
                      {QStringLiteral("orders"), QStringLiteral("open orders"), QStringLiteral("pending orders"),
                       QStringLiteral("working orders")},
                      where),
        rows::section(screen, QStringLiteral("history"), CryptoTradingScreen::tr("Crypto order history"),
                      {QStringLiteral("history"), QStringLiteral("past orders"), QStringLiteral("paper trades")},
                      where, CryptoTradingScreen::tr("Crypto Trading › History (paper account)")),
        rows::section(screen, QStringLiteral("my_trades"), CryptoTradingScreen::tr("Crypto fills"),
                      {QStringLiteral("my trades"), QStringLiteral("fills"), QStringLiteral("executions"),
                       QStringLiteral("filled orders")},
                      where),
        rows::section(screen, QStringLiteral("fees"), CryptoTradingScreen::tr("Crypto trading fees"),
                      {QStringLiteral("fees"), QStringLiteral("maker"), QStringLiteral("taker"),
                       QStringLiteral("fee schedule"), QStringLiteral("commission")},
                      where),
        rows::section(screen, QStringLiteral("market"), CryptoTradingScreen::tr("Funding rate & open interest"),
                      {QStringLiteral("funding"), QStringLiteral("funding rate"), QStringLiteral("open interest"),
                       QStringLiteral("mark price"), QStringLiteral("perpetual"), QStringLiteral("perp")},
                      where),
        rows::section(screen, QStringLiteral("stats"), CryptoTradingScreen::tr("Crypto trading stats"),
                      {QStringLiteral("statistics"), QStringLiteral("win rate"), QStringLiteral("performance"),
                       QStringLiteral("p&l")},
                      where),
        rows::section(screen, QStringLiteral("tape"), CryptoTradingScreen::tr("Time & sales"),
                      {QStringLiteral("tape"), QStringLiteral("time and sales"), QStringLiteral("prints"),
                       QStringLiteral("recent trades"), QStringLiteral("t&s")},
                      where),
        rows::section(screen, QStringLiteral("depth"), CryptoTradingScreen::tr("Depth chart"),
                      {QStringLiteral("market depth"), QStringLiteral("order book depth"), QStringLiteral("liquidity")},
                      where),
        rows::section(screen, QStringLiteral("ticket"), CryptoTradingScreen::tr("Crypto order ticket"),
                      {QStringLiteral("buy crypto"), QStringLiteral("sell crypto"), QStringLiteral("order entry"),
                       QStringLiteral("place order"), QStringLiteral("trade bitcoin")},
                      where, CryptoTradingScreen::tr("Crypto Trading › order ticket (nothing is sent until you press BUY / SELL)")),
    };
    return out;
}

void ct_install_commands() {
    command_rows::install(QStringLiteral("crypto_trading"), &ct_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(ct_install_commands)

} // namespace fincept::screens
