// Crypto Trading Screen — coordinator with async fetching and live data support

#include "screens/crypto_trading/CryptoTradingScreen.h"
#include "screens/crypto_trading/CryptoTickerBar.h"
#include "screens/crypto_trading/CryptoWatchlist.h"
#include "screens/crypto_trading/CryptoChart.h"
#include "screens/crypto_trading/CryptoOrderEntry.h"
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "screens/crypto_trading/CryptoBottomPanel.h"
#include "screens/crypto_trading/CryptoCredentials.h"
#include "trading/PaperTrading.h"
#include "trading/OrderMatcher.h"
#include "trading/ExchangeService.h"
#include "core/logging/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QMutexLocker>
#include <QPointer>
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
    if (ws_candle_cb_id_ >= 0) es.remove_candle_callback(ws_candle_cb_id_);
}

// ============================================================================
// Visibility — start/stop timers only when screen is shown
// ============================================================================

void CryptoTradingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (ticker_timer_)      ticker_timer_->start();
    if (ob_timer_)          ob_timer_->start();
    if (portfolio_timer_)   portfolio_timer_->start();
    if (watchlist_timer_)   watchlist_timer_->start();
    if (market_info_timer_) market_info_timer_->start();
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
    LOG_INFO(TAG, "Screen hidden — timers stopped");
}

// ============================================================================
// UI Setup
// ============================================================================

void CryptoTradingScreen::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(2);

    // ── Top Control Bar ──────────────────────────────────────────────────────
    auto* top_bar = new QHBoxLayout;
    top_bar->setSpacing(8);

    exchange_combo_ = new QComboBox;
    exchange_combo_->addItems({"binance", "bybit", "okx", "kraken", "coinbase",
                                "bitget", "gate", "kucoin", "mexc", "htx"});
    exchange_combo_->setFixedWidth(110);
    top_bar->addWidget(new QLabel("Exchange:"));
    top_bar->addWidget(exchange_combo_);

    symbol_combo_ = new QComboBox;
    symbol_combo_->setEditable(true);
    symbol_combo_->addItems(watchlist_symbols_);
    symbol_combo_->setCurrentText("BTC/USDT");
    symbol_combo_->setFixedWidth(140);
    top_bar->addWidget(symbol_combo_);

    top_bar->addStretch();

    api_btn_ = new QPushButton("API");
    api_btn_->setStyleSheet("QPushButton { background: #1a1a2e; color: #888; border: 1px solid #333; "
                             "padding: 3px 10px; border-radius: 3px; font-size: 11px; }"
                             "QPushButton:hover { color: #ccc; border-color: #555; }");
    top_bar->addWidget(api_btn_);

    mode_btn_ = new QPushButton("PAPER");
    mode_btn_->setCheckable(true);
    mode_btn_->setStyleSheet(
        "QPushButton { background: #1a5c2e; color: #00ff88; border: 1px solid #00ff88; "
        "padding: 3px 10px; border-radius: 3px; font-weight: bold; font-size: 11px; }"
        "QPushButton:checked { background: #5c1a1a; color: #ff4444; border-color: #ff4444; }");
    top_bar->addWidget(mode_btn_);

    main_layout->addLayout(top_bar);

    // ── Ticker Bar ───────────────────────────────────────────────────────────
    ticker_bar_ = new CryptoTickerBar;
    main_layout->addWidget(ticker_bar_);

    // ── Main 3-panel Splitter ────────────────────────────────────────────────
    auto* main_splitter = new QSplitter(Qt::Horizontal);

    // Left: Watchlist
    watchlist_ = new CryptoWatchlist;
    watchlist_->set_symbols(watchlist_symbols_);
    main_splitter->addWidget(watchlist_);

    // Center: Chart + Bottom
    auto* center = new QWidget;
    auto* center_layout = new QVBoxLayout(center);
    center_layout->setContentsMargins(0, 0, 0, 0);
    center_layout->setSpacing(2);

    chart_ = new CryptoChart;
    center_layout->addWidget(chart_, 3);

    bottom_panel_ = new CryptoBottomPanel;
    bottom_panel_->setMaximumHeight(260);
    center_layout->addWidget(bottom_panel_, 1);

    main_splitter->addWidget(center);

    // Right: Order Entry + Order Book
    auto* right = new QWidget;
    auto* right_layout = new QVBoxLayout(right);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(2);

    order_entry_ = new CryptoOrderEntry;
    right_layout->addWidget(order_entry_);

    orderbook_ = new CryptoOrderBook;
    right_layout->addWidget(orderbook_, 1);

    right->setFixedWidth(270);
    main_splitter->addWidget(right);

    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);

    main_layout->addWidget(main_splitter, 1);

    // ── Signal Connections ───────────────────────────────────────────────────
    connect(exchange_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CryptoTradingScreen::on_exchange_changed);
    connect(symbol_combo_, &QComboBox::currentTextChanged,
            this, &CryptoTradingScreen::on_symbol_selected);
    connect(mode_btn_, &QPushButton::clicked,
            this, &CryptoTradingScreen::on_mode_toggled);
    connect(api_btn_, &QPushButton::clicked,
            this, &CryptoTradingScreen::on_api_clicked);
    connect(watchlist_, &CryptoWatchlist::symbol_selected,
            this, &CryptoTradingScreen::on_symbol_selected);
    connect(watchlist_, &CryptoWatchlist::search_requested,
            this, &CryptoTradingScreen::on_search_requested);
    connect(order_entry_, &CryptoOrderEntry::order_submitted,
            this, &CryptoTradingScreen::on_order_submitted);
    connect(orderbook_, &CryptoOrderBook::price_clicked,
            this, &CryptoTradingScreen::on_ob_price_clicked);
    connect(bottom_panel_, &CryptoBottomPanel::cancel_order_requested,
            this, &CryptoTradingScreen::on_cancel_order);

    // Chart timeframe change → async fetch
    connect(chart_, &CryptoChart::timeframe_changed, this, [this](const QString& tf) {
        async_fetch_candles(selected_symbol_, tf);
    });
}

// ============================================================================
// Timers
// ============================================================================

void CryptoTradingScreen::setup_timers() {
    // Timers are configured but NOT started here.
    // They start/stop via showEvent/hideEvent to avoid work when screen is hidden.

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

    // Live data timer (active only in live mode)
    live_data_timer_ = new QTimer(this);
    connect(live_data_timer_, &QTimer::timeout, this, &CryptoTradingScreen::refresh_live_data);

    // Initial load — deferred until widget is actually shown
    QTimer::singleShot(100, this, [this]() {
        init_exchange();
        load_portfolio();
        refresh_ticker();
        refresh_orderbook();
        refresh_watchlist();
        async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    });
}

// ============================================================================
// Init
// ============================================================================

void CryptoTradingScreen::init_exchange() {
    auto& es = ExchangeService::instance();
    es.set_exchange(exchange_id_);

    // WS price callback → update ticker bar + order entry price
    ws_price_cb_id_ = es.on_price_update(
        [this](const QString& symbol, const TickerData& ticker) {
            if (symbol == selected_symbol_) {
                QMetaObject::invokeMethod(this, [this, ticker]() {
                    ticker_bar_->update_data(ticker.last, ticker.percentage,
                                              ticker.high, ticker.low,
                                              ticker.base_volume,
                                              ExchangeService::instance().is_ws_connected());
                    order_entry_->set_current_price(ticker.last);
                }, Qt::QueuedConnection);
            }
        });

    // WS candle callback → live chart updates
    ws_candle_cb_id_ = es.on_candle_update(
        [this](const QString& symbol, const Candle& candle) {
            if (symbol == selected_symbol_) {
                QMetaObject::invokeMethod(this, [this, candle]() {
                    chart_->append_candle(candle);
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
        portfolio_ = pt_create_portfolio("Crypto Paper", DEFAULT_PAPER_BALANCE, "USD",
                                          1.0, "cross", 0.001, exchange_id_);
        portfolio_id_ = portfolio_.id;
    }
    ExchangeService::instance().watch_symbol(selected_symbol_, portfolio_id_);
    refresh_portfolio();
}

// ============================================================================
// Async Fetchers (don't block UI thread)
// ============================================================================

void CryptoTradingScreen::async_fetch_candles(const QString& symbol, const QString& timeframe) {
    if (candles_fetching_.exchange(true)) return; // already fetching
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
            if (result.contains("positions")) {
                self->bottom_panel_->set_live_positions(result.value("positions").toArray());
            }
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
            if (result.contains("orders")) {
                self->bottom_panel_->set_live_orders(result.value("orders").toArray());
            }
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

void CryptoTradingScreen::on_exchange_changed(int index) {
    exchange_id_ = exchange_combo_->itemText(index);
    ExchangeService::instance().set_exchange(exchange_id_);
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
    symbol_combo_->setCurrentText(symbol);
    ticker_bar_->set_symbol(symbol);
    ExchangeService::instance().watch_symbol(selected_symbol_, portfolio_id_);
    ExchangeService::instance().set_ws_primary_symbol(symbol);
    refresh_ticker();
    refresh_orderbook();
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void CryptoTradingScreen::on_mode_toggled() {
    bool is_live = mode_btn_->isChecked();
    trading_mode_ = is_live ? crypto::TradingMode::Live : crypto::TradingMode::Paper;
    mode_btn_->setText(is_live ? "LIVE" : "PAPER");
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);

    if (is_live) {
        live_data_timer_->start(5000);
        refresh_live_data(); // immediate first fetch
    } else {
        live_data_timer_->stop();
        refresh_portfolio(); // switch back to paper data
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

void CryptoTradingScreen::on_order_submitted(const QString& side, const QString& order_type,
                                               double qty, double price, double stop_price,
                                               double sl, double tp) {
    try {
        if (trading_mode_ == crypto::TradingMode::Paper) {
            auto ticker = ExchangeService::instance().get_cached_price(selected_symbol_);
            std::optional<double> price_opt;
            if (order_type == "market") {
                price_opt = ticker.last > 0 ? ticker.last : 1000.0;
            } else if (price > 0) {
                price_opt = price;
            }
            std::optional<double> stop_opt;
            if (stop_price > 0) stop_opt = stop_price;

            auto order = pt_place_order(portfolio_id_, selected_symbol_,
                                         side, order_type, qty, price_opt, stop_opt);
            if (order_type == "market") {
                double fill = ticker.last > 0 ? ticker.last : price_opt.value_or(1000.0);
                pt_fill_order(order.id, fill);
            } else {
                OrderMatcher::instance().add_order(order);
                if (sl > 0 || tp > 0) {
                    OrderMatcher::instance().set_sl_tp(portfolio_id_, selected_symbol_,
                                                        order.id, sl, tp);
                }
            }
            refresh_portfolio();
        } else {
            // Live mode — async exchange order
            QPointer<CryptoTradingScreen> self = this;
            QtConcurrent::run([self, side, order_type, qty, price]() {
                if (!self) return;
                ExchangeService::instance().place_exchange_order(
                    self->selected_symbol_, side, order_type, qty, price);
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
    if (trading_mode_ == crypto::TradingMode::Paper) {
        try {
            pt_cancel_order(order_id);
            OrderMatcher::instance().remove_order(order_id);
            refresh_portfolio();
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Cancel failed: %1").arg(e.what()));
        }
    } else {
        // Live cancel — async
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

void CryptoTradingScreen::refresh_ticker() {
    if (!initialized_) return;
    auto cached = ExchangeService::instance().get_cached_price(selected_symbol_);
    if (cached.last > 0) {
        ticker_bar_->update_data(cached.last, cached.percentage, cached.high, cached.low,
                                  cached.base_volume, ExchangeService::instance().is_ws_connected());
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
        }, Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::refresh_portfolio() {
    if (portfolio_id_.isEmpty()) return;
    if (trading_mode_ == crypto::TradingMode::Live) return; // live mode uses refresh_live_data

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
    } catch (...) {}
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
    if (trading_mode_ != crypto::TradingMode::Live) return;
    if (live_fetching_.exchange(true)) return;

    async_fetch_live_positions();
    async_fetch_live_orders();
    async_fetch_live_balance();

    live_fetching_ = false;
}

} // namespace fincept::screens
