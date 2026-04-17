// Crypto Trading Screen — coordinator
#include "screens/crypto_trading/CryptoTradingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/crypto_trading/CryptoBottomPanel.h"
#include "screens/crypto_trading/CryptoChart.h"
#include "screens/crypto_trading/CryptoCredentials.h"
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "screens/crypto_trading/CryptoOrderEntry.h"
#include "screens/crypto_trading/CryptoTickerBar.h"
#include "screens/crypto_trading/CryptoWatchlist.h"
#include "trading/ExchangeService.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QCompleter>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPointer>
#include <QSplitter>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::crypto;

static const QString TAG = "CryptoTrading";

// ============================================================================
// Constructor / Destructor
// ============================================================================

CryptoTradingScreen::CryptoTradingScreen(QWidget* parent) : QWidget(parent) {
    setup_ui();
    setup_timers();
}

CryptoTradingScreen::~CryptoTradingScreen() {
    auto& es = ExchangeService::instance();
    if (ws_price_cb_id_ >= 0)
        es.remove_price_callback(ws_price_cb_id_);
    if (ws_ob_cb_id_ >= 0)
        es.remove_orderbook_callback(ws_ob_cb_id_);
    if (ws_candle_cb_id_ >= 0)
        es.remove_candle_callback(ws_candle_cb_id_);
    if (ws_trade_cb_id_ >= 0)
        es.remove_trade_callback(ws_trade_cb_id_);
}

// ============================================================================
// Visibility — start/stop timers
// ============================================================================

void CryptoTradingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (ticker_timer_)
        ticker_timer_->start();
    if (ob_timer_)
        ob_timer_->start();
    if (portfolio_timer_)
        portfolio_timer_->start();
    if (watchlist_timer_)
        watchlist_timer_->start();
    if (market_info_timer_)
        market_info_timer_->start();
    if (clock_timer_)
        clock_timer_->start();
    if (ws_flush_timer_)
        ws_flush_timer_->start();
    LOG_INFO(TAG, "Screen visible — timers started");
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
    for (const auto& ex :
         {"kraken", "binance", "bybit", "okx", "coinbase", "bitget", "gate", "kucoin", "mexc", "htx"}) {
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

    // WS status
    ws_status_ = new QLabel("REST");
    ws_status_->setObjectName("cryptoWsStatus");
    ws_status_->setStyleSheet(QString("color: %1;").arg(ui::colors::WARNING()));
    cmd_layout->addWidget(ws_status_);

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
    watchlist_timer_->setInterval(15000);

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
        refresh_ticker(); // cheap — reads ExchangeService's in-memory cache

        auto* es = &trading::ExchangeService::instance();
        auto run_initial_fetches = [this]() {
            if (startup_fetches_done_)
                return;
            startup_fetches_done_ = true;
            refresh_orderbook();
            refresh_watchlist();
            async_fetch_candles(selected_symbol_, chart_->current_timeframe());
        };

        if (es->wait_for_daemon_ready(0)) {
            // Daemon already warm (pre-warmed by ExchangeService ctor or a
            // previous session). Fire immediately.
            run_initial_fetches();
        } else {
            // Connect once; disconnect after first fire.
            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn = connect(es, &trading::ExchangeService::daemon_ready, this, [this, conn, run_initial_fetches]() {
                QObject::disconnect(*conn);
                run_initial_fetches();
            });
            // Safety net: if daemon never comes up within 8s, proceed anyway
            // so the user eventually sees data.
            QTimer::singleShot(8000, this, [this, conn, run_initial_fetches]() {
                if (startup_fetches_done_)
                    return;
                QObject::disconnect(*conn);
                LOG_WARN(TAG, "Daemon not ready after 8s — firing startup fetches via script fallback");
                run_initial_fetches();
            });
        }
    });
}

void CryptoTradingScreen::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));

    // WS status label — only reapply text + stylesheet on state change. A
    // setStyleSheet call triggers a CSS reparse and repaint; doing that every
    // second on an unchanging state was pure waste.
    const int connected = ExchangeService::instance().is_ws_connected() ? 1 : 0;
    if (connected == last_ws_status_label_state_)
        return;
    last_ws_status_label_state_ = connected;
    ws_status_->setText(connected ? "LIVE" : "REST");
    ws_status_->setStyleSheet(QString("color: %1;").arg(connected ? ui::colors::POSITIVE() : ui::colors::WARNING()));
}

// ============================================================================
// Init
// ============================================================================

void CryptoTradingScreen::init_exchange() {
    auto& es = ExchangeService::instance();
    es.set_exchange(exchange_id_);

    if (!es.is_feed_running())
        es.start_price_feed(5);

    es.start_ws_stream(selected_symbol_, watchlist_symbols_);

    // WS price callback → accumulate into pending buffers (flushed at 10fps by ws_flush_timer_)
    // This coalesces ~50 msgs/sec into ~10 UI updates/sec.
    // QPointer guard: callbacks are invoked from the WS reader thread, so the
    // widget could be torn down between dispatch and our lambda body running.
    QPointer<CryptoTradingScreen> self = this;
    ws_price_cb_id_ = es.on_price_update([self](const QString& symbol, const TickerData& ticker) {
        if (!self) return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, symbol, ticker]() {
                if (!self) return;
                self->pending_tickers_[symbol] = ticker; // latest wins per symbol
                if (symbol == self->selected_symbol_) {
                    self->pending_primary_ticker_ = ticker;
                    self->has_pending_primary_ = true;
                }
            },
            Qt::QueuedConnection);
    });

    // WS orderbook callback → buffer latest; flushed at 10fps by ws_flush_timer_
    ws_ob_cb_id_ = es.on_orderbook_update([self](const QString& symbol, const OrderBookData& ob) {
        if (!self || symbol != self->selected_symbol_)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, ob]() {
                if (!self) return;
                self->pending_orderbook_ = ob;
                self->has_pending_orderbook_ = true;
            },
            Qt::QueuedConnection);
    });

    // WS candle callback → buffer pending candles; flushed at 10fps by ws_flush_timer_
    ws_candle_cb_id_ = es.on_candle_update([self](const QString& symbol, const Candle& candle) {
        if (!self || symbol != self->selected_symbol_)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, candle]() {
                if (!self) return;
                self->pending_candles_.append(candle);
            },
            Qt::QueuedConnection);
    });

    // WS trade callback → buffer trade entries; flushed at 10fps by ws_flush_timer_
    ws_trade_cb_id_ = es.on_trade_update([self](const QString& symbol, const TradeData& trade) {
        if (!self || symbol != self->selected_symbol_)
            return;
        TradeEntry entry;
        entry.side = trade.side;
        entry.price = trade.price;
        entry.amount = trade.amount;
        QMetaObject::invokeMethod(
            self.data(),
            [self, entry]() {
                if (!self) return;
                self->pending_trades_.append(entry);
            },
            Qt::QueuedConnection);
    });

    initialized_ = true;
}

void CryptoTradingScreen::load_portfolio() {
    auto existing = pt_find_portfolio("Crypto Paper", exchange_id_);
    if (existing) {
        portfolio_ = *existing;
        portfolio_id_ = portfolio_.id;
    } else {
        portfolio_ =
            pt_create_portfolio("Crypto Paper", DEFAULT_PAPER_BALANCE, "USD", 1.0, "cross", 0.001, exchange_id_);
        portfolio_id_ = portfolio_.id;
    }
    ExchangeService::instance().watch_symbol(selected_symbol_, portfolio_id_);
    refresh_portfolio();
}

// ============================================================================
// Async Fetchers
// ============================================================================

void CryptoTradingScreen::async_fetch_candles(const QString& symbol, const QString& timeframe) {
    if (candles_fetching_.exchange(true))
        return;
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self, symbol, timeframe]() {
        auto candles = ExchangeService::instance().fetch_ohlcv(symbol, timeframe, OHLCV_FETCH_COUNT);
        if (!self)
            return;
        self->candles_fetching_ = false;
        QMetaObject::invokeMethod(
            self,
            [self, candles]() {
                if (!self)
                    return;
                self->chart_->set_candles(candles);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_positions() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) {
            // Widget destroyed before dispatch — no counter to decrement.
            return;
        }
        auto result = ExchangeService::instance().fetch_positions_live(self->selected_symbol_);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                if (result.contains("positions"))
                    self->bottom_panel_->set_live_positions(result.value("positions").toArray());
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_orders() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self)
            return;
        auto result = ExchangeService::instance().fetch_open_orders_live(self->selected_symbol_);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                if (result.contains("orders"))
                    self->bottom_panel_->set_live_orders(result.value("orders").toArray());
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_balance() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self)
            return;
        auto result = ExchangeService::instance().fetch_balance();
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                double total = result.value("total").toObject().value("USDT").toDouble();
                double free = result.value("free").toObject().value("USDT").toDouble();
                double used = result.value("used").toObject().value("USDT").toDouble();
                self->bottom_panel_->set_live_balance(free, total, used);
                self->order_entry_->set_balance(free);
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

// ============================================================================
// Slot Handlers
// ============================================================================

void CryptoTradingScreen::on_exchange_changed(const QString& exchange) {
    if (exchange == exchange_id_)
        return;
    exchange_id_ = exchange;
    exchange_btn_->setText(exchange.toUpper());

    auto& es = ExchangeService::instance();

    // Tear down current WS stream + drop stale callbacks so ticks from the
    // previous exchange can't race into buffers post-switch.
    es.stop_ws_stream();
    if (ws_price_cb_id_ >= 0) { es.remove_price_callback(ws_price_cb_id_); ws_price_cb_id_ = -1; }
    if (ws_ob_cb_id_ >= 0) { es.remove_orderbook_callback(ws_ob_cb_id_); ws_ob_cb_id_ = -1; }
    if (ws_candle_cb_id_ >= 0) { es.remove_candle_callback(ws_candle_cb_id_); ws_candle_cb_id_ = -1; }
    if (ws_trade_cb_id_ >= 0) { es.remove_trade_callback(ws_trade_cb_id_); ws_trade_cb_id_ = -1; }

    // Clear accumulated buffers — stale data from the old exchange is useless.
    pending_tickers_.clear();
    pending_orderbook_ = {};
    has_pending_orderbook_ = false;
    pending_primary_ticker_ = {};
    has_pending_primary_ = false;
    pending_candles_.clear();
    pending_trades_.clear();
    last_ws_state_ = -1;  // re-evaluate feed mode after new stream connects

    es.set_exchange(exchange_id_);

    // Re-initialize — this re-registers all four WS callbacks + starts the stream.
    initialized_ = false;
    init_exchange();

    load_portfolio();
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    ScreenStateManager::instance().notify_changed(this);
}

void CryptoTradingScreen::on_symbol_selected(const QString& symbol) {
    if (symbol.isEmpty() || symbol == selected_symbol_)
        return;
    switch_symbol(symbol);
    ScreenStateManager::instance().notify_changed(this);
}

void CryptoTradingScreen::switch_symbol(const QString& symbol) {
    auto& es = ExchangeService::instance();
    es.unwatch_symbol(selected_symbol_, portfolio_id_);
    selected_symbol_ = symbol;
    symbol_input_->setText(symbol);
    ticker_bar_->set_symbol(symbol);
    order_entry_->set_symbol(symbol);
    watchlist_->set_active_symbol(symbol);

    // Drop per-symbol buffers tied to the old symbol to prevent cross-symbol leakage.
    has_pending_primary_ = false;
    pending_primary_ticker_ = {};
    has_pending_orderbook_ = false;
    pending_orderbook_ = {};
    pending_candles_.clear();
    pending_trades_.clear();
    market_info_cache_ = {};

    es.watch_symbol(selected_symbol_, portfolio_id_);
    es.set_ws_primary_symbol(symbol);
    refresh_ticker();
    refresh_orderbook();
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void CryptoTradingScreen::on_mode_toggled() {
    const bool is_live = mode_btn_->isChecked();
    trading_mode_ = is_live ? TradingMode::Live : TradingMode::Paper;
    mode_btn_->setText(is_live ? "LIVE" : "PAPER");
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);

    if (is_live) {
        live_data_timer_->start(5000);
        refresh_live_data();
    } else {
        live_data_timer_->stop();
        refresh_portfolio();
    }
}

void CryptoTradingScreen::on_api_clicked() {
    auto* dlg = new CryptoCredentials(exchange_id_, this);
    connect(dlg, &CryptoCredentials::credentials_saved, this,
            [this](const QString& key, const QString& secret, const QString& pw) {
                ExchangeCredentials creds;
                creds.api_key = key;
                creds.secret = secret;
                creds.password = pw;
                ExchangeService::instance().set_credentials(creds);
            });
    dlg->exec();
    dlg->deleteLater();
}

void CryptoTradingScreen::on_order_submitted(const QString& side, const QString& order_type, double qty, double price,
                                             double stop_price, double sl, double tp) {
    try {
        if (trading_mode_ == TradingMode::Paper) {
            auto ticker = ExchangeService::instance().get_cached_price(selected_symbol_);
            std::optional<double> price_opt;
            if (order_type == "market")
                price_opt = ticker.last > 0 ? ticker.last : 1000.0;
            else if (price > 0)
                price_opt = price;

            std::optional<double> stop_opt;
            if (stop_price > 0)
                stop_opt = stop_price;

            auto order = pt_place_order(portfolio_id_, selected_symbol_, side, order_type, qty, price_opt, stop_opt);
            if (order_type == "market") {
                double fill = ticker.last > 0 ? ticker.last : price_opt.value_or(1000.0);
                pt_fill_order(order.id, fill);
            } else {
                OrderMatcher::instance().add_order(order);
                if (sl > 0 || tp > 0)
                    OrderMatcher::instance().set_sl_tp(portfolio_id_, selected_symbol_, order.id, sl, tp);
            }
            refresh_portfolio();
        } else {
            QPointer<CryptoTradingScreen> self = this;
            QtConcurrent::run([self, side, order_type, qty, price]() {
                if (!self)
                    return;
                ExchangeService::instance().place_exchange_order(self->selected_symbol_, side, order_type, qty, price);
                QMetaObject::invokeMethod(
                    self,
                    [self]() {
                        if (!self)
                            return;
                        self->refresh_live_data();
                    },
                    Qt::QueuedConnection);
            });
        }
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, QString("Order failed: %1").arg(e.what()));
    }
}

void CryptoTradingScreen::on_cancel_order(const QString& order_id) {
    if (trading_mode_ == TradingMode::Paper) {
        try {
            pt_cancel_order(order_id);
            OrderMatcher::instance().remove_order(order_id);
            refresh_portfolio();
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Cancel failed: %1").arg(e.what()));
        }
    } else {
        QPointer<CryptoTradingScreen> self = this;
        QtConcurrent::run([self, order_id]() {
            if (!self)
                return;
            ExchangeService::instance().cancel_exchange_order(order_id, self->selected_symbol_);
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    if (!self)
                        return;
                    self->refresh_live_data();
                },
                Qt::QueuedConnection);
        });
    }
}

void CryptoTradingScreen::on_ob_price_clicked(double price) {
    order_entry_->set_current_price(price);
}

void CryptoTradingScreen::on_search_requested(const QString& filter) {
    QPointer<CryptoTradingScreen> self = this;
    QString filter_copy = filter;
    QtConcurrent::run([self, filter_copy]() {
        if (!self)
            return;
        auto markets = ExchangeService::instance().fetch_markets("spot", filter_copy);
        QMetaObject::invokeMethod(
            self,
            [self, markets]() {
                if (!self)
                    return;
                self->watchlist_->set_search_results(markets);
            },
            Qt::QueuedConnection);
    });
}

// ============================================================================
// Refresh Functions
// ============================================================================

// ============================================================================
// WS Update Coalescing — flush accumulated data to UI at 10fps
// ============================================================================

void CryptoTradingScreen::apply_feed_mode(bool ws_connected) {
    // Called on WS-state edge transitions. When WS is live we can stop REST
    // polling; when it drops we must restart so the screen keeps updating.
    const int desired = ws_connected ? 1 : 0;
    if (desired == last_ws_state_)
        return;
    last_ws_state_ = desired;

    if (ws_connected) {
        if (ticker_timer_ && ticker_timer_->isActive()) ticker_timer_->stop();
        if (ob_timer_ && ob_timer_->isActive()) ob_timer_->stop();
        if (watchlist_timer_ && watchlist_timer_->isActive()) watchlist_timer_->stop();
        if (portfolio_timer_ && portfolio_timer_->isActive()) portfolio_timer_->stop();
        LOG_INFO(TAG, "WS connected — REST polling paused");
    } else {
        if (isVisible()) {
            if (ticker_timer_ && !ticker_timer_->isActive()) ticker_timer_->start();
            if (ob_timer_ && !ob_timer_->isActive()) ob_timer_->start();
            if (watchlist_timer_ && !watchlist_timer_->isActive()) watchlist_timer_->start();
            if (portfolio_timer_ && !portfolio_timer_->isActive()) portfolio_timer_->start();
            LOG_WARN(TAG, "WS disconnected — REST polling resumed");
        }
    }
}

void CryptoTradingScreen::flush_ws_updates() {
    apply_feed_mode(ExchangeService::instance().is_ws_connected());

    // Flush primary symbol ticker → header bar + order entry
    if (has_pending_primary_) {
        const auto& t = pending_primary_ticker_;
        ticker_bar_->update_data(t.last, t.percentage, t.high, t.low, t.base_volume,
                                 ExchangeService::instance().is_ws_connected());
        if (t.bid > 0 && t.ask > 0)
            ticker_bar_->update_bid_ask(t.bid, t.ask, std::abs(t.ask - t.bid));
        order_entry_->set_current_price(t.last);
        has_pending_primary_ = false;
    }

    // Flush latest orderbook snapshot (one repaint per flush tick, not per WS msg)
    if (has_pending_orderbook_) {
        const auto& ob = pending_orderbook_;
        orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
        bottom_panel_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
        has_pending_orderbook_ = false;
    }

    // Flush buffered candles in order (chart handles intrabar updates via timestamp match)
    if (!pending_candles_.isEmpty()) {
        for (const auto& c : pending_candles_)
            chart_->append_candle(c);
        pending_candles_.clear();
    }

    // Flush buffered trade entries — one loop instead of one invokeMethod per trade
    if (!pending_trades_.isEmpty()) {
        for (const auto& entry : pending_trades_)
            bottom_panel_->add_trade_entry(entry);
        pending_trades_.clear();
    }

    // Flush all accumulated tickers → watchlist + paper trading engine
    if (!pending_tickers_.isEmpty()) {
        QVector<TickerData> batch;
        batch.reserve(pending_tickers_.size());
        for (auto it = pending_tickers_.constBegin(); it != pending_tickers_.constEnd(); ++it)
            batch.append(it.value());
        pending_tickers_.clear();
        watchlist_->update_prices(batch);

        // Feed WS prices into paper trading engine on a worker thread — every tick
        // would otherwise do 3N+ SQLite ops on the UI thread (price update, order
        // match, SL/TP check) plus 1-5 queries for UI refresh at 10Hz.
        if (trading_mode_ == TradingMode::Paper && !portfolio_id_.isEmpty()) {
            // Drop this tick if the previous batch is still running — keeps us
            // from queueing work faster than the DB can drain it.
            bool expected = false;
            if (paper_bookkeeping_in_flight_.compare_exchange_strong(expected, true)) {
                QPointer<CryptoTradingScreen> self = this;
                const QString pid = portfolio_id_;
                QtConcurrent::run([self, pid, batch]() {
                    struct Result {
                        QVector<PtPosition> positions;
                        bool fill_occurred = false;
                        PtPortfolio portfolio;
                        QVector<PtOrder> orders;
                        QVector<PtTrade> trades;
                        PtStats stats;
                    };
                    Result r;
                    try {
                        const int orders_before = pt_get_orders(pid, "open").size();
                        for (const auto& ticker : batch) {
                            if (ticker.last <= 0)
                                continue;
                            pt_update_position_price(pid, ticker.symbol, ticker.last);
                            PriceData pd;
                            pd.last = ticker.last;
                            pd.bid = ticker.bid;
                            pd.ask = ticker.ask;
                            pd.timestamp = ticker.timestamp;
                            OrderMatcher::instance().check_orders(ticker.symbol, pd, pid);
                            OrderMatcher::instance().check_sl_tp_triggers(pid, ticker.symbol, ticker.last);
                        }
                        r.positions = pt_get_positions(pid);
                        const int orders_after = pt_get_orders(pid, "open").size();
                        r.fill_occurred = orders_after < orders_before;
                        if (r.fill_occurred) {
                            r.portfolio = pt_get_portfolio(pid);
                            r.orders = pt_get_orders(pid);
                            r.trades = pt_get_trades(pid, 50);
                            r.stats = pt_get_stats(pid);
                        }
                    } catch (...) {
                        if (self)
                            LOG_WARN("CryptoTradingScreen", "Paper bookkeeping worker threw");
                    }

                    if (!self)
                        return;  // widget destroyed — nothing to reset
                    QMetaObject::invokeMethod(self, [self, r]() {
                        if (!self)
                            return;
                        self->paper_bookkeeping_in_flight_.store(false);
                        self->bottom_panel_->set_positions(r.positions);
                        if (r.fill_occurred) {
                            self->portfolio_ = r.portfolio;
                            self->order_entry_->set_balance(r.portfolio.balance);
                            self->bottom_panel_->set_orders(r.orders);
                            self->bottom_panel_->set_trades(r.trades);
                            self->bottom_panel_->set_stats(r.stats);
                        }
                    }, Qt::QueuedConnection);
                });
            }
        }
        // Polling timers are managed centrally by apply_feed_mode() at the top
        // of this function — no per-flush stop/start here.
    }
}

// ============================================================================
// Refresh Functions
// ============================================================================

void CryptoTradingScreen::refresh_ticker() {
    if (!initialized_)
        return;
    const auto cached = ExchangeService::instance().get_cached_price(selected_symbol_);
    if (cached.last > 0) {
        ticker_bar_->update_data(cached.last, cached.percentage, cached.high, cached.low, cached.base_volume,
                                 ExchangeService::instance().is_ws_connected());
        if (cached.bid > 0 && cached.ask > 0)
            ticker_bar_->update_bid_ask(cached.bid, cached.ask, std::abs(cached.ask - cached.bid));
        order_entry_->set_current_price(cached.last);
    }
}

void CryptoTradingScreen::refresh_orderbook() {
    if (!initialized_)
        return;
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self)
            return;
        auto ob = ExchangeService::instance().fetch_orderbook(self->selected_symbol_, OB_MAX_DISPLAY_LEVELS);
        QMetaObject::invokeMethod(
            self,
            [self, ob]() {
                if (!self)
                    return;
                self->orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
                self->bottom_panel_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_portfolio() {
    if (portfolio_id_.isEmpty())
        return;
    if (trading_mode_ == TradingMode::Live)
        return;

    // Perform 5 SQLite reads on a worker thread — with 1.5s timer + order
    // placement/cancel flushes, this was one of the loudest UI-thread stalls.
    QPointer<CryptoTradingScreen> self = this;
    const QString pid = portfolio_id_;
    QtConcurrent::run([self, pid]() {
        struct Snapshot {
            PtPortfolio portfolio;
            QVector<PtPosition> positions;
            QVector<PtOrder> orders;
            QVector<PtTrade> trades;
            PtStats stats;
            bool ok = false;
        };
        Snapshot s;
        try {
            s.portfolio = pt_get_portfolio(pid);
            s.positions = pt_get_positions(pid);
            s.orders = pt_get_orders(pid);
            s.trades = pt_get_trades(pid, 50);
            s.stats = pt_get_stats(pid);
            s.ok = true;
        } catch (...) {
            if (self)
                LOG_WARN("CryptoTradingScreen", "Exception refreshing portfolio panel");
        }
        if (!self || !s.ok)
            return;
        QMetaObject::invokeMethod(self, [self, s]() {
            if (!self)
                return;
            self->portfolio_ = s.portfolio;
            self->order_entry_->set_balance(s.portfolio.balance);
            self->bottom_panel_->set_positions(s.positions);
            self->bottom_panel_->set_orders(s.orders);
            self->bottom_panel_->set_trades(s.trades);
            self->bottom_panel_->set_stats(s.stats);
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_watchlist() {
    if (!initialized_)
        return;
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self)
            return;
        auto tickers = ExchangeService::instance().fetch_tickers(self->watchlist_symbols_);
        QMetaObject::invokeMethod(
            self,
            [self, tickers]() {
                if (!self)
                    return;
                self->watchlist_->update_prices(tickers);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_market_info() {
    if (!initialized_)
        return;
    // Fire the two daemon calls on separate worker threads so their UI-thread
    // return paths run independently. The daemon serializes via its own mutex,
    // but the funding-rate branch updates the ticker mark price as soon as it
    // lands instead of waiting for open-interest too. Each worker writes its
    // portion into market_info_cache_ on the UI thread, then pushes the union
    // to the bottom panel — cache access is UI-thread-only, no lock needed.
    QPointer<CryptoTradingScreen> self = this;
    const QString symbol = selected_symbol_;

    QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        auto fr = ExchangeService::instance().fetch_funding_rate(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, symbol, fr]() {
                if (!self)
                    return;
                if (self->selected_symbol_ != symbol)
                    return; // user switched symbols — discard stale result
                self->market_info_cache_.funding_rate = fr.funding_rate;
                self->market_info_cache_.mark_price = fr.mark_price;
                self->market_info_cache_.index_price = fr.index_price;
                self->market_info_cache_.next_funding_time = fr.next_funding_timestamp;
                self->market_info_cache_.has_data = true;
                self->ticker_bar_->update_mark_price(fr.mark_price, fr.index_price);
                self->bottom_panel_->set_market_info(self->market_info_cache_);
            },
            Qt::QueuedConnection);
    });

    QtConcurrent::run([self, symbol]() {
        if (!self)
            return;
        auto oi = ExchangeService::instance().fetch_open_interest(symbol);
        QMetaObject::invokeMethod(
            self,
            [self, symbol, oi]() {
                if (!self)
                    return;
                if (self->selected_symbol_ != symbol)
                    return; // user switched symbols — discard stale result
                self->market_info_cache_.open_interest = oi.open_interest;
                self->market_info_cache_.open_interest_value = oi.open_interest_value;
                self->market_info_cache_.has_data = true;
                self->bottom_panel_->set_market_info(self->market_info_cache_);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_candles() {
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void CryptoTradingScreen::refresh_live_data() {
    if (trading_mode_ != TradingMode::Live)
        return;
    // Skip this tick entirely if any of the 4 live fetches from the previous
    // tick are still in-flight — prevents burst-stacking against the exchange
    // API and against the daemon's request pipeline.
    if (live_inflight_.load() > 0)
        return;

    live_inflight_.store(4);
    async_fetch_live_positions();
    async_fetch_live_orders();
    async_fetch_live_balance();
    async_fetch_my_trades();
}

void CryptoTradingScreen::async_fetch_my_trades() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self)
            return;
        auto result = ExchangeService::instance().fetch_my_trades(self->selected_symbol_);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                self->bottom_panel_->update_my_trades(result);
                self->live_inflight_.fetch_sub(1);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_trading_fees() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self)
            return;
        auto result = ExchangeService::instance().fetch_trading_fees(self->selected_symbol_);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                self->bottom_panel_->update_fees(result);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_mark_price() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self)
            return;
        auto mp = ExchangeService::instance().fetch_mark_price(self->selected_symbol_);
        QMetaObject::invokeMethod(
            self,
            [self, mp]() {
                if (!self)
                    return;
                self->ticker_bar_->update_mark_price(mp.mark_price, mp.index_price);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_set_leverage(int leverage) {
    const QString symbol = selected_symbol_;
    QtConcurrent::run([symbol, leverage]() { ExchangeService::instance().set_leverage(symbol, leverage); });
}

void CryptoTradingScreen::async_set_margin_mode(const QString& mode) {
    const QString symbol = selected_symbol_;
    const QString m = mode;
    QtConcurrent::run([symbol, m]() { ExchangeService::instance().set_margin_mode(symbol, m); });
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

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
