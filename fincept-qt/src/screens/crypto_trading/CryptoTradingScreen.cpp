// Crypto Trading Screen — Bloomberg-style coordinator
#include "screens/crypto_trading/CryptoTradingScreen.h"

#include "core/logging/Logger.h"
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

#include <QCompleter>
#include <QDateTime>
#include <QHBoxLayout>
#include <QMutexLocker>
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
    if (ws_price_cb_id_ >= 0) es.remove_price_callback(ws_price_cb_id_);
    if (ws_ob_cb_id_ >= 0)    es.remove_orderbook_callback(ws_ob_cb_id_);
    if (ws_candle_cb_id_ >= 0) es.remove_candle_callback(ws_candle_cb_id_);
    if (ws_trade_cb_id_ >= 0)  es.remove_trade_callback(ws_trade_cb_id_);
}

// ============================================================================
// Visibility — start/stop timers
// ============================================================================

void CryptoTradingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (ticker_timer_)      ticker_timer_->start();
    if (ob_timer_)          ob_timer_->start();
    if (portfolio_timer_)   portfolio_timer_->start();
    if (watchlist_timer_)   watchlist_timer_->start();
    if (market_info_timer_) market_info_timer_->start();
    if (clock_timer_)       clock_timer_->start();
    if (ws_flush_timer_)    ws_flush_timer_->start();
    LOG_INFO(TAG, "Screen visible — timers started");
}

void CryptoTradingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (ticker_timer_)      ticker_timer_->stop();
    if (ob_timer_)          ob_timer_->stop();
    if (portfolio_timer_)   portfolio_timer_->stop();
    if (watchlist_timer_)   watchlist_timer_->stop();
    if (market_info_timer_) market_info_timer_->stop();
    if (live_data_timer_)   live_data_timer_->stop();
    if (clock_timer_)       clock_timer_->stop();
    if (ws_flush_timer_)    ws_flush_timer_->stop();
    LOG_INFO(TAG, "Screen hidden — timers stopped");
}

// ============================================================================
// UI Setup — Bloomberg 4-zone layout
// ============================================================================

void CryptoTradingScreen::setup_ui() {
    setObjectName("cryptoScreen");
    setStyleSheet(ui::styles::crypto_trading_styles());

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // ── COMMAND BAR (34px) ────────────────────────────────────────────────────
    auto* cmd_bar = new QWidget;
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
    for (const auto& ex : {"kraken", "binance", "bybit", "okx", "coinbase", "bitget", "gate", "kucoin", "mexc", "htx"}) {
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
    connect(symbol_input_, &QLineEdit::returnPressed, this, [this]() {
        on_symbol_selected(symbol_input_->text().trimmed().toUpper());
    });
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
    ws_status_->setStyleSheet("color: #ca8a04;");
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

    QTimer::singleShot(100, this, [this]() {
        init_exchange();
        load_portfolio();
        refresh_ticker();
        refresh_orderbook();
        refresh_watchlist();
        async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    });
}

void CryptoTradingScreen::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
    // Update WS status
    const bool connected = ExchangeService::instance().is_ws_connected();
    ws_status_->setText(connected ? "LIVE" : "REST");
    ws_status_->setStyleSheet(connected ? "color: #16a34a;" : "color: #ca8a04;");
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
    // This coalesces ~50 msgs/sec into ~10 UI updates/sec
    ws_price_cb_id_ = es.on_price_update([this](const QString& symbol, const TickerData& ticker) {
        QMetaObject::invokeMethod(this, [this, symbol, ticker]() {
            pending_tickers_[symbol] = ticker;  // latest wins per symbol
            if (symbol == selected_symbol_) {
                pending_primary_ticker_ = ticker;
                has_pending_primary_ = true;
            }
        }, Qt::QueuedConnection);
    });

    // WS orderbook callback
    ws_ob_cb_id_ = es.on_orderbook_update([this](const QString& symbol, const OrderBookData& ob) {
        if (symbol == selected_symbol_) {
            QMetaObject::invokeMethod(this, [this, ob]() {
                orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
                bottom_panel_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
                if (ob_timer_) ob_timer_->stop();
            }, Qt::QueuedConnection);
        }
    });

    // WS candle callback
    ws_candle_cb_id_ = es.on_candle_update([this](const QString& symbol, const Candle& candle) {
        if (symbol == selected_symbol_) {
            QMetaObject::invokeMethod(this, [this, candle]() { chart_->append_candle(candle); }, Qt::QueuedConnection);
        }
    });

    // WS trade callback → Time & Sales
    ws_trade_cb_id_ = es.on_trade_update([this](const QString& symbol, const TradeData& trade) {
        if (symbol == selected_symbol_) {
            TradeEntry entry;
            entry.side = trade.side;
            entry.price = trade.price;
            entry.amount = trade.amount;
            entry.timestamp = trade.timestamp;
            QMetaObject::invokeMethod(this, [this, entry]() {
                bottom_panel_->add_trade_entry(entry);
            }, Qt::QueuedConnection);
        }
    });

    initialized_ = true;
}

void CryptoTradingScreen::load_portfolio() {
    auto existing = pt_find_portfolio("Crypto Paper", exchange_id_);
    if (existing) {
        portfolio_ = *existing;
        portfolio_id_ = portfolio_.id;
    } else {
        portfolio_ = pt_create_portfolio("Crypto Paper", DEFAULT_PAPER_BALANCE, "USD", 1.0, "cross", 0.001, exchange_id_);
        portfolio_id_ = portfolio_.id;
    }
    ExchangeService::instance().watch_symbol(selected_symbol_, portfolio_id_);
    refresh_portfolio();
}

// ============================================================================
// Async Fetchers
// ============================================================================

void CryptoTradingScreen::async_fetch_candles(const QString& symbol, const QString& timeframe) {
    if (candles_fetching_.exchange(true)) return;
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self, symbol, timeframe]() {
        auto candles = ExchangeService::instance().fetch_ohlcv(symbol, timeframe, OHLCV_FETCH_COUNT);
        if (!self) return;
        self->candles_fetching_ = false;
        QMetaObject::invokeMethod(self, [self, candles]() {
            if (!self) return;
            self->chart_->set_candles(candles);
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_positions() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto result = ExchangeService::instance().fetch_positions_live(self->selected_symbol_);
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;
            if (result.contains("positions"))
                self->bottom_panel_->set_live_positions(result.value("positions").toArray());
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_orders() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto result = ExchangeService::instance().fetch_open_orders_live(self->selected_symbol_);
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;
            if (result.contains("orders"))
                self->bottom_panel_->set_live_orders(result.value("orders").toArray());
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::async_fetch_live_balance() {
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto result = ExchangeService::instance().fetch_balance();
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;
            double total = result.value("total").toObject().value("USDT").toDouble();
            double free = result.value("free").toObject().value("USDT").toDouble();
            double used = result.value("used").toObject().value("USDT").toDouble();
            self->bottom_panel_->set_live_balance(free, total, used);
            self->order_entry_->set_balance(free);
        }, Qt::QueuedConnection);
    });
}

// ============================================================================
// Slot Handlers
// ============================================================================

void CryptoTradingScreen::on_exchange_changed(const QString& exchange) {
    exchange_id_ = exchange;
    exchange_btn_->setText(exchange.toUpper());
    auto& es = ExchangeService::instance();
    es.set_exchange(exchange_id_);
    es.start_ws_stream(selected_symbol_, watchlist_symbols_);
    load_portfolio();
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void CryptoTradingScreen::on_symbol_selected(const QString& symbol) {
    if (symbol.isEmpty() || symbol == selected_symbol_) return;
    switch_symbol(symbol);
}

void CryptoTradingScreen::switch_symbol(const QString& symbol) {
    ExchangeService::instance().unwatch_symbol(selected_symbol_, portfolio_id_);
    selected_symbol_ = symbol;
    symbol_input_->setText(symbol);
    ticker_bar_->set_symbol(symbol);
    order_entry_->set_symbol(symbol);
    watchlist_->set_active_symbol(symbol);
    ExchangeService::instance().watch_symbol(selected_symbol_, portfolio_id_);
    ExchangeService::instance().set_ws_primary_symbol(symbol);
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
            if (stop_price > 0) stop_opt = stop_price;

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
                if (!self) return;
                ExchangeService::instance().place_exchange_order(self->selected_symbol_, side, order_type, qty, price);
                QMetaObject::invokeMethod(self, [self]() {
                    if (!self) return;
                    self->refresh_live_data();
                }, Qt::QueuedConnection);
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
            if (!self) return;
            ExchangeService::instance().cancel_exchange_order(order_id, self->selected_symbol_);
            QMetaObject::invokeMethod(self, [self]() {
                if (!self) return;
                self->refresh_live_data();
            }, Qt::QueuedConnection);
        });
    }
}

void CryptoTradingScreen::on_ob_price_clicked(double price) {
    order_entry_->set_current_price(price);
}

void CryptoTradingScreen::on_search_requested(const QString& filter) {
    Q_UNUSED(filter);
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto markets = ExchangeService::instance().fetch_markets("spot");
        QMetaObject::invokeMethod(self, [self, markets]() {
            if (!self) return;
            self->watchlist_->set_search_results(markets);
        }, Qt::QueuedConnection);
    });
}

// ============================================================================
// Refresh Functions
// ============================================================================

// ============================================================================
// WS Update Coalescing — flush accumulated data to UI at 10fps
// ============================================================================

void CryptoTradingScreen::flush_ws_updates() {
    // Flush primary symbol ticker → header bar + order entry
    if (has_pending_primary_) {
        const auto& t = pending_primary_ticker_;
        ticker_bar_->update_data(t.last, t.percentage, t.high, t.low,
                                 t.base_volume, ExchangeService::instance().is_ws_connected());
        if (t.bid > 0 && t.ask > 0)
            ticker_bar_->update_bid_ask(t.bid, t.ask, std::abs(t.ask - t.bid));
        order_entry_->set_current_price(t.last);
        has_pending_primary_ = false;
    }

    // Flush all accumulated tickers → watchlist + paper trading engine
    if (!pending_tickers_.isEmpty()) {
        QVector<TickerData> batch;
        batch.reserve(pending_tickers_.size());
        for (auto it = pending_tickers_.constBegin(); it != pending_tickers_.constEnd(); ++it)
            batch.append(it.value());
        pending_tickers_.clear();
        watchlist_->update_prices(batch);

        // Feed WS prices into paper trading engine (position prices + order triggers)
        if (trading_mode_ == TradingMode::Paper && !portfolio_id_.isEmpty()) {
            for (const auto& ticker : batch) {
                if (ticker.last <= 0) continue;
                // Update position current_price and unrealized P&L
                pt_update_position_price(portfolio_id_, ticker.symbol, ticker.last);
                // Check limit/stop order fills
                PriceData pd;
                pd.last = ticker.last;
                pd.bid = ticker.bid;
                pd.ask = ticker.ask;
                pd.timestamp = ticker.timestamp;
                OrderMatcher::instance().check_orders(ticker.symbol, pd, portfolio_id_);
                OrderMatcher::instance().check_sl_tp_triggers(portfolio_id_, ticker.symbol, ticker.last);
            }
        }

        // Stop redundant polling timers while WS is delivering data
        if (ticker_timer_ && ticker_timer_->isActive())
            ticker_timer_->stop();
        if (watchlist_timer_ && watchlist_timer_->isActive())
            watchlist_timer_->stop();
    }
}

// ============================================================================
// Refresh Functions
// ============================================================================

void CryptoTradingScreen::refresh_ticker() {
    if (!initialized_) return;
    const auto cached = ExchangeService::instance().get_cached_price(selected_symbol_);
    if (cached.last > 0) {
        ticker_bar_->update_data(cached.last, cached.percentage, cached.high, cached.low,
                                 cached.base_volume, ExchangeService::instance().is_ws_connected());
        if (cached.bid > 0 && cached.ask > 0)
            ticker_bar_->update_bid_ask(cached.bid, cached.ask, std::abs(cached.ask - cached.bid));
        order_entry_->set_current_price(cached.last);
    }
}

void CryptoTradingScreen::refresh_orderbook() {
    if (!initialized_) return;
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto ob = ExchangeService::instance().fetch_orderbook(self->selected_symbol_, OB_MAX_DISPLAY_LEVELS);
        QMetaObject::invokeMethod(self, [self, ob]() {
            if (!self) return;
            self->orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
            self->bottom_panel_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_portfolio() {
    if (portfolio_id_.isEmpty()) return;
    if (trading_mode_ == TradingMode::Live) return;

    try {
        portfolio_ = pt_get_portfolio(portfolio_id_);
        auto positions = pt_get_positions(portfolio_id_);
        auto orders = pt_get_orders(portfolio_id_);
        auto trades = pt_get_trades(portfolio_id_, 50);
        auto stats = pt_get_stats(portfolio_id_);

        order_entry_->set_balance(portfolio_.balance);
        bottom_panel_->set_positions(positions);
        bottom_panel_->set_orders(orders);
        bottom_panel_->set_trades(trades);
        bottom_panel_->set_stats(stats);
    } catch (...) {
    }
}

void CryptoTradingScreen::refresh_watchlist() {
    if (!initialized_) return;
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto tickers = ExchangeService::instance().fetch_tickers(self->watchlist_symbols_);
        QMetaObject::invokeMethod(self, [self, tickers]() {
            if (!self) return;
            self->watchlist_->update_prices(tickers);
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_market_info() {
    if (!initialized_) return;
    QPointer<CryptoTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto fr = ExchangeService::instance().fetch_funding_rate(self->selected_symbol_);
        auto oi = ExchangeService::instance().fetch_open_interest(self->selected_symbol_);
        MarketInfoData info;
        info.funding_rate = fr.funding_rate;
        info.mark_price = fr.mark_price;
        info.index_price = fr.index_price;
        info.next_funding_time = fr.next_funding_timestamp;
        info.open_interest = oi.open_interest;
        info.open_interest_value = oi.open_interest_value;
        info.has_data = true;
        QMetaObject::invokeMethod(self, [self, info]() {
            if (!self) return;
            self->bottom_panel_->set_market_info(info);
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_candles() {
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void CryptoTradingScreen::refresh_live_data() {
    if (trading_mode_ != TradingMode::Live) return;
    if (live_fetching_.exchange(true)) return;

    async_fetch_live_positions();
    async_fetch_live_orders();
    async_fetch_live_balance();

    live_fetching_ = false;
}

} // namespace fincept::screens
