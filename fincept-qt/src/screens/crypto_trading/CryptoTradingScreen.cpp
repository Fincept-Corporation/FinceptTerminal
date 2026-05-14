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
    // Only exchanges registered as DataHub producers (see
    // ExchangeSessionManager::topic_patterns) can be consumed by this screen
    // post-Phase 6. Adding a new exchange here is a two-step job: add the
    // topic patterns on the manager AND register the C++ metatypes with the
    // hub, then add it to this list.
    for (const auto& ex : {"kraken", "hyperliquid"}) {
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
    ws_status_ = new QLabel("CONNECTING");
    ws_status_->setObjectName("cryptoWsStatus");
    ws_status_->setProperty("ws", "connecting");
    ws_status_->setToolTip("WebSocket feed status — green=live, amber=connecting, red=offline (REST polling)");
    cmd_layout->addWidget(ws_status_);

    // Transport hint — small text right of the pill. Kraken uses the native
    // QWebSocket client; everyone else still goes through ws_stream.py.
    ws_transport_ = new QLabel(exchange_id_ == "kraken" ? "NATIVE" : "DAEMON");
    ws_transport_->setObjectName("cryptoWsTransport");
    ws_transport_->setToolTip(exchange_id_ == "kraken"
                                  ? "Native C++ WebSocket — direct connection, no Python subprocess"
                                  : "ws_stream.py via ccxt.pro — Python subprocess");
    cmd_layout->addWidget(ws_transport_);

    // Clock
    clock_label_ = new QLabel("--:--:--");
    clock_label_->setObjectName("cryptoClock");
    cmd_layout->addWidget(clock_label_);

    // API button
    api_btn_ = new QPushButton("API");
    api_btn_->setObjectName("cryptoApiBtn");
    api_btn_->setFixedHeight(22);
    api_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(api_btn_);

    // Mode button
    mode_btn_ = new QPushButton("PAPER");
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
    connect(chart_, &CryptoChart::timeframe_changed, this,
            [this](const QString& tf) { async_fetch_candles(selected_symbol_, tf); });
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

    ws_status_->setText(connected ? "LIVE" : "OFFLINE");
    ws_status_->setProperty("ws", connected ? "live" : "offline");
    ws_status_->style()->unpolish(ws_status_);
    ws_status_->style()->polish(ws_status_);
}

// ============================================================================
// Init
// ============================================================================

// ============================================================================
// Direct Kraken WS subscription (intentionally bypasses DataHub).
//
// Earlier iterations published Kraken events through DataHub topic patterns
// (`ws:kraken:ticker:*` etc.) and consumed them here via subscribe_pattern.
// That path crashed under high BBO update rates — the hub's coalesce timer
// + pattern matching + fan-out machinery was the wrong tool for a
// single-consumer single-producer crypto stream. The native client emits
// plain Qt signals; we wire them straight to the same pending_* buffers
// the flush_ws_updates timer drains at 10fps.
// ============================================================================

void CryptoTradingScreen::hub_subscribe_topics() {
    // Drop any prior connections to this owner (handles symbol/exchange swap).
    if (ws_subscription_owner_) {
        delete ws_subscription_owner_;
        ws_subscription_owner_ = nullptr;
    }

    if (exchange_id_ != "kraken")
        return; // Hyperliquid still uses the daemon Python path

    auto& session = fincept::trading::ExchangeSessionManager::instance();
    auto* sess = session.session(exchange_id_);
    if (!sess)
        return;
    auto* ws = sess->kraken_ws_client();
    if (!ws) {
        LOG_WARN(TAG, "kraken_ws_client() returned null — start_ws hasn't run yet");
        return;
    }

    // ws_subscription_owner_ is a context QObject the connections are
    // parented to. Destroying it auto-disconnects every connection on swap.
    ws_subscription_owner_ = new QObject(this);

    QPointer<CryptoTradingScreen> self = this;
    QString primary = selected_symbol_;

    connect(ws, &fincept::trading::kraken::KrakenWsClient::ticker_received,
            ws_subscription_owner_, [self, primary](const fincept::trading::TickerData& t) {
                if (!self || t.symbol.isEmpty())
                    return;
                self->pending_tickers_[t.symbol] = t;
                if (t.symbol == self->selected_symbol_) {
                    self->pending_primary_ticker_ = t;
                    self->has_pending_primary_ = true;
                }
            });

    connect(ws, &fincept::trading::kraken::KrakenWsClient::orderbook_received,
            ws_subscription_owner_, [self](const fincept::trading::OrderBookData& ob) {
                if (!self || ob.symbol != self->selected_symbol_)
                    return;
                self->pending_orderbook_ = ob;
                self->has_pending_orderbook_ = true;
            });

    connect(ws, &fincept::trading::kraken::KrakenWsClient::trade_received,
            ws_subscription_owner_, [self](const fincept::trading::TradeData& td) {
                if (!self || td.symbol != self->selected_symbol_)
                    return;
                crypto::TradeEntry e;
                e.side = td.side;
                e.price = td.price;
                e.amount = td.amount;
                self->pending_trades_.append(e);
            });

    connect(ws, &fincept::trading::kraken::KrakenWsClient::candle_received,
            ws_subscription_owner_,
            [self](const QString& sym, const QString& /*interval*/,
                   const fincept::trading::Candle& c) {
                if (!self || sym != self->selected_symbol_)
                    return;
                self->pending_candles_.append(c);
            });

    LOG_INFO(TAG, QString("Direct WS signals connected (kraken / %1)").arg(primary));
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
                ws_status_->setText("OFFLINE");
                ws_status_->setProperty("ws", "offline");
                ws_status_->style()->unpolish(ws_status_);
                ws_status_->style()->polish(ws_status_);
            }
            last_ws_status_label_state_ = 0;
        }
    } else {
        LOG_INFO(TAG, "WS already active for " + exchange_id_ + " — attaching to warm session");
    }

    // Hub is the only consumer path. Every exchange shown in the dropdown
    // must have a matching producer registered on ExchangeSessionManager.
    hub_subscribe_topics();
    initialized_ = true;
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
