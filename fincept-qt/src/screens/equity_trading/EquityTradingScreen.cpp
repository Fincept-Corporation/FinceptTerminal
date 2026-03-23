// Equity Trading Screen — Bloomberg-style coordinator
#include "screens/equity_trading/EquityTradingScreen.h"

#include "core/logging/Logger.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChart.h"
#include "screens/equity_trading/EquityCredentials.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "trading/BrokerRegistry.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/UnifiedTrading.h"
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
using namespace fincept::screens::equity;

static const QString TAG = "EquityTrading";

// ============================================================================
// Constructor / Destructor
// ============================================================================

EquityTradingScreen::EquityTradingScreen(QWidget* parent) : QWidget(parent) {
    watchlist_symbols_ = DEFAULT_WATCHLIST;
    setup_ui();
    setup_timers();
}

EquityTradingScreen::~EquityTradingScreen() {
    if (fill_cb_id_ >= 0)
        OrderMatcher::instance().remove_fill_callback(fill_cb_id_);
}

// ============================================================================
// Visibility — start/stop timers (P3)
// ============================================================================

void EquityTradingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (quote_timer_)     quote_timer_->start();
    if (portfolio_timer_) portfolio_timer_->start();
    if (watchlist_timer_) watchlist_timer_->start();
    if (clock_timer_)     clock_timer_->start();
    LOG_INFO(TAG, "Screen visible — timers started");
}

void EquityTradingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (quote_timer_)     quote_timer_->stop();
    if (portfolio_timer_) portfolio_timer_->stop();
    if (watchlist_timer_) watchlist_timer_->stop();
    if (clock_timer_)     clock_timer_->stop();
    LOG_INFO(TAG, "Screen hidden — timers stopped");
}

// ============================================================================
// UI Setup — Bloomberg 4-zone layout
// ============================================================================

void EquityTradingScreen::setup_ui() {
    setObjectName("eqScreen");
    setStyleSheet(ui::styles::equity_trading_styles());

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // ── COMMAND BAR (34px) ────────────────────────────────────────────────────
    auto* cmd_bar = new QWidget;
    cmd_bar->setObjectName("eqCommandBar");
    cmd_bar->setFixedHeight(34);
    auto* cmd_layout = new QHBoxLayout(cmd_bar);
    cmd_layout->setContentsMargins(8, 0, 8, 0);
    cmd_layout->setSpacing(6);

    // Broker button + menu
    broker_btn_ = new QPushButton("FYERS");
    broker_btn_->setObjectName("eqBrokerBtn");
    broker_btn_->setFixedHeight(22);
    broker_btn_->setCursor(Qt::PointingHandCursor);
    broker_menu_ = new QMenu(broker_btn_);

    const auto brokers = BrokerRegistry::instance().list_brokers();
    for (const auto& b : brokers) {
        broker_menu_->addAction(b.toUpper(), this, [this, b]() { on_broker_changed(b); });
    }
    broker_btn_->setMenu(broker_menu_);
    cmd_layout->addWidget(broker_btn_);

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
    connect(symbol_input_, &QLineEdit::returnPressed, this, [this]() {
        on_symbol_selected(symbol_input_->text().trimmed().toUpper());
    });
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

    // API button
    api_btn_ = new QPushButton("API");
    api_btn_->setObjectName("eqApiBtn");
    api_btn_->setFixedHeight(22);
    api_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(api_btn_);

    // Mode button
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
    connect(api_btn_, &QPushButton::clicked, this, &EquityTradingScreen::on_api_clicked);
    connect(watchlist_, &EquityWatchlist::symbol_selected, this, &EquityTradingScreen::on_symbol_selected);
    connect(order_entry_, &EquityOrderEntry::order_submitted, this, &EquityTradingScreen::on_order_submitted);
    connect(orderbook_, &EquityOrderBook::price_clicked, this, &EquityTradingScreen::on_ob_price_clicked);
    connect(bottom_panel_, &EquityBottomPanel::cancel_order_requested, this, &EquityTradingScreen::on_cancel_order);
    connect(chart_, &EquityChart::timeframe_changed, this,
            [this](const QString& tf) { async_fetch_candles(selected_symbol_, tf); });
}

// ============================================================================
// Timers
// ============================================================================

void EquityTradingScreen::setup_timers() {
    quote_timer_ = new QTimer(this);
    connect(quote_timer_, &QTimer::timeout, this, &EquityTradingScreen::refresh_quote);
    quote_timer_->setInterval(QUOTE_POLL_MS);

    portfolio_timer_ = new QTimer(this);
    connect(portfolio_timer_, &QTimer::timeout, this, &EquityTradingScreen::refresh_portfolio);
    portfolio_timer_->setInterval(PORTFOLIO_POLL_MS);

    watchlist_timer_ = new QTimer(this);
    connect(watchlist_timer_, &QTimer::timeout, this, &EquityTradingScreen::refresh_watchlist);
    watchlist_timer_->setInterval(WATCHLIST_POLL_MS);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &EquityTradingScreen::update_clock);
    clock_timer_->setInterval(CLOCK_UPDATE_MS);

    // Deferred init — don't block constructor (P1)
    QTimer::singleShot(100, this, [this]() {
        init_broker();
        load_portfolio();
        refresh_quote();
        async_fetch_watchlist_quotes();
        async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    });
}

void EquityTradingScreen::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

// ============================================================================
// Init
// ============================================================================

void EquityTradingScreen::init_broker() {
    auto& ut = UnifiedTrading::instance();
    ut.init_session(broker_id_, "paper", portfolio_id_);
    initialized_ = true;
    LOG_INFO(TAG, QString("Initialized broker: %1").arg(broker_id_));
}

void EquityTradingScreen::load_portfolio() {
    auto existing = pt_find_portfolio("Equity Paper", broker_id_);
    if (existing) {
        portfolio_ = *existing;
        portfolio_id_ = portfolio_.id;
    } else {
        portfolio_ = pt_create_portfolio("Equity Paper", DEFAULT_PAPER_BALANCE, "INR", 1.0, "cross", 0.001, broker_id_);
        portfolio_id_ = portfolio_.id;
    }

    // Register order fill callback — auto-refresh portfolio when limit/stop orders fill
    if (fill_cb_id_ < 0) {
        QPointer<EquityTradingScreen> self = this;
        fill_cb_id_ = OrderMatcher::instance().on_order_fill([self](const OrderFillEvent& ev) {
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, ev]() {
                if (!self) return;
                LOG_INFO(TAG, QString("Paper order filled: %1 %2 @ %3")
                                  .arg(ev.side, ev.symbol).arg(ev.fill_price, 0, 'f', 2));
                self->refresh_portfolio();
            }, Qt::QueuedConnection);
        });
    }

    // Load any pending limit/stop orders into the matcher
    auto pending = pt_get_orders(portfolio_id_, "pending");
    for (const auto& o : pending) {
        if (o.order_type != "market")
            OrderMatcher::instance().add_order(o);
    }

    refresh_portfolio();
}

// ============================================================================
// Async Fetchers (P8 — QPointer guard)
// ============================================================================

void EquityTradingScreen::async_fetch_quote() {
    if (quote_fetching_.exchange(true)) return;
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) { self->quote_fetching_ = false; return; }
        auto creds = broker->load_credentials();
        auto result = broker->get_quotes(creds, {self->selected_symbol_});
        self->quote_fetching_ = false;
        if (!result.success || !result.data || result.data->isEmpty()) return;
        const auto quote = result.data->first();
        QMetaObject::invokeMethod(self, [self, quote]() {
            if (!self) return;
            self->current_price_ = quote.ltp;
            self->ticker_bar_->update_quote(
                quote.ltp, quote.change, quote.change_pct,
                quote.high, quote.low, quote.volume,
                quote.bid, quote.ask);
            self->order_entry_->set_current_price(quote.ltp);

            // Feed price into paper trading engine for position P&L + order matching
            if (self->trading_mode_ == TradingMode::Paper && !self->portfolio_id_.isEmpty() && quote.ltp > 0) {
                pt_update_position_price(self->portfolio_id_, self->selected_symbol_, quote.ltp);
                PriceData pd;
                pd.last = quote.ltp;
                pd.bid = quote.bid;
                pd.ask = quote.ask;
                pd.timestamp = quote.timestamp;
                OrderMatcher::instance().check_orders(self->selected_symbol_, pd, self->portfolio_id_);
                OrderMatcher::instance().check_sl_tp_triggers(self->portfolio_id_, self->selected_symbol_, quote.ltp);
            }
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_candles(const QString& symbol, const QString& timeframe) {
    if (candles_fetching_.exchange(true)) return;
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self, symbol, timeframe]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) { self->candles_fetching_ = false; return; }
        auto creds = broker->load_credentials();
        auto result = broker->get_history(creds, symbol, timeframe, "", "");
        self->candles_fetching_ = false;
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, data = *result.data]() {
            if (!self) return;
            self->chart_->set_candles(data);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_positions() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        auto result = broker->get_positions(creds);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, data = *result.data]() {
            if (!self) return;
            self->bottom_panel_->set_positions(data);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_holdings() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        auto result = broker->get_holdings(creds);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, data = *result.data]() {
            if (!self) return;
            self->bottom_panel_->set_holdings(data);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_orders() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        auto result = broker->get_orders(creds);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, data = *result.data]() {
            if (!self) return;
            self->bottom_panel_->set_orders(data);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_funds() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        auto result = broker->get_funds(creds);
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, data = *result.data]() {
            if (!self) return;
            self->bottom_panel_->set_funds(data);
            self->order_entry_->set_balance(data.available_balance);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_watchlist_quotes() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        auto result = broker->get_quotes(creds, self->watchlist_symbols_.toVector());
        if (!result.success || !result.data) return;
        QMetaObject::invokeMethod(self, [self, data = *result.data]() {
            if (!self) return;
            self->watchlist_->update_quotes(data);

            // Feed watchlist prices into paper trading engine for all symbols
            if (self->trading_mode_ == TradingMode::Paper && !self->portfolio_id_.isEmpty()) {
                for (const auto& q : data) {
                    if (q.ltp <= 0) continue;
                    pt_update_position_price(self->portfolio_id_, q.symbol, q.ltp);
                    PriceData pd;
                    pd.last = q.ltp;
                    pd.bid = q.bid;
                    pd.ask = q.ask;
                    pd.timestamp = q.timestamp;
                    OrderMatcher::instance().check_orders(q.symbol, pd, self->portfolio_id_);
                }
            }
        }, Qt::QueuedConnection);
    });
}

// ============================================================================
// Slot Handlers
// ============================================================================

void EquityTradingScreen::on_broker_changed(const QString& broker) {
    broker_id_ = broker;
    broker_btn_->setText(broker.toUpper());

    // Update default watchlist based on broker region
    auto* b = BrokerRegistry::instance().get(broker);
    if (b) {
        auto bid = b->id();
        // Indian brokers
        if (bid == BrokerId::Fyers || bid == BrokerId::Zerodha || bid == BrokerId::Upstox ||
            bid == BrokerId::Dhan || bid == BrokerId::Kotak || bid == BrokerId::Groww ||
            bid == BrokerId::AliceBlue || bid == BrokerId::AngelOne || bid == BrokerId::FivePaisa ||
            bid == BrokerId::IIFL || bid == BrokerId::Motilal || bid == BrokerId::Shoonya) {
            watchlist_symbols_ = DEFAULT_WATCHLIST;
            selected_exchange_ = "NSE";
            if (selected_symbol_ == "AAPL" || selected_symbol_ == "MSFT")
                selected_symbol_ = "RELIANCE";
        } else {
            watchlist_symbols_ = US_WATCHLIST;
            selected_exchange_ = "NASDAQ";
            if (selected_symbol_ == "RELIANCE" || selected_symbol_ == "HDFCBANK")
                selected_symbol_ = "AAPL";
        }
    }

    exchange_label_->setText(selected_exchange_);
    symbol_input_->setText(selected_symbol_);
    watchlist_->set_symbols(watchlist_symbols_);
    ticker_bar_->set_symbol(selected_symbol_);
    order_entry_->set_symbol(selected_symbol_);
    order_entry_->set_exchange(selected_exchange_);

    init_broker();
    load_portfolio();
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    async_fetch_watchlist_quotes();
}

void EquityTradingScreen::on_symbol_selected(const QString& symbol) {
    if (symbol.isEmpty() || symbol == selected_symbol_) return;
    switch_symbol(symbol);
}

void EquityTradingScreen::switch_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    symbol_input_->setText(symbol);
    ticker_bar_->set_symbol(symbol);
    order_entry_->set_symbol(symbol);
    watchlist_->set_active_symbol(symbol);
    refresh_quote();
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void EquityTradingScreen::on_mode_toggled() {
    const bool is_live = mode_btn_->isChecked();
    trading_mode_ = is_live ? TradingMode::Live : TradingMode::Paper;
    mode_btn_->setText(is_live ? "LIVE" : "PAPER");
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);

    auto& ut = UnifiedTrading::instance();
    ut.switch_mode(is_live ? "live" : "paper");

    if (is_live) {
        async_fetch_positions();
        async_fetch_holdings();
        async_fetch_orders();
        async_fetch_funds();
    } else {
        refresh_portfolio();
    }
}

void EquityTradingScreen::on_api_clicked() {
    auto* dlg = new EquityCredentials(broker_id_, this);
    connect(dlg, &EquityCredentials::credentials_saved, this,
            [this](const QString& bid, const QString& key, const QString& secret, const QString& auth) {
                // P1: exchange_token() does HTTP — must not block UI thread
                QPointer<EquityTradingScreen> self = this;
                QtConcurrent::run([self, bid, key, secret, auth]() {
                    if (!self) return;
                    auto* broker = BrokerRegistry::instance().get(bid);
                    if (!broker) return;

                    auto token_result = broker->exchange_token(key, secret, auth);
                    QMetaObject::invokeMethod(self, [self, bid, key, secret, token_result]() {
                        if (!self) return;
                        if (token_result.success) {
                            auto* broker = BrokerRegistry::instance().get(bid);
                            if (!broker) return;
                            BrokerCredentials creds;
                            creds.broker_id = bid;
                            creds.api_key = key;
                            creds.api_secret = secret;
                            creds.access_token = token_result.access_token;
                            creds.user_id = token_result.user_id;
                            broker->save_credentials(creds);
                            LOG_INFO(TAG, QString("Connected to %1").arg(bid));
                            // Refresh data after successful auth
                            self->refresh_quote();
                            self->async_fetch_watchlist_quotes();
                        } else {
                            LOG_ERROR(TAG, QString("Auth failed for %1: %2").arg(bid, token_result.error));
                        }
                    }, Qt::QueuedConnection);
                });
            });
    dlg->exec();
    dlg->deleteLater();
}

void EquityTradingScreen::on_order_submitted(const UnifiedOrder& order) {
    if (trading_mode_ == TradingMode::Paper) {
        const QString side = order.side == OrderSide::Buy ? "buy" : "sell";
        const QString type = order_type_str(order.order_type);
        std::optional<double> price_opt;
        if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit)
            price_opt = order.price;
        std::optional<double> stop_opt;
        if (order.stop_price > 0) stop_opt = order.stop_price;

        auto pt_order = pt_place_order(portfolio_id_, order.symbol, side, type,
                                        order.quantity, price_opt, stop_opt);
        if (order.order_type == OrderType::Market) {
            // Use the live ticker price for paper fill; fall back to order price or last known
            double fill_price = current_price_ > 0 ? current_price_
                              : (order.price > 0 ? order.price : 0.0);
            if (fill_price <= 0) {
                LOG_WARN(TAG, "No price available for paper market order — skipping fill");
                return;
            }
            pt_fill_order(pt_order.id, fill_price);
        } else {
            OrderMatcher::instance().add_order(pt_order);
        }
        refresh_portfolio();
    } else {
        // Route to live broker via UnifiedTrading
        QPointer<EquityTradingScreen> self = this;
        auto order_copy = order;
        QtConcurrent::run([self, order_copy]() {
            if (!self) return;
            auto result = UnifiedTrading::instance().place_order(order_copy);
            QMetaObject::invokeMethod(self, [self, result]() {
                if (!self) return;
                if (result.success) {
                    LOG_INFO(TAG, QString("Order placed: %1").arg(result.order_id));
                    self->async_fetch_orders();
                    self->async_fetch_positions();
                    self->async_fetch_funds();
                } else {
                    LOG_ERROR(TAG, QString("Order failed: %1").arg(result.message));
                }
            }, Qt::QueuedConnection);
        });
    }
}

void EquityTradingScreen::on_cancel_order(const QString& order_id) {
    if (trading_mode_ == TradingMode::Paper) {
        pt_cancel_order(order_id);
        OrderMatcher::instance().remove_order(order_id);
        refresh_portfolio();
    } else {
        QPointer<EquityTradingScreen> self = this;
        QtConcurrent::run([self, order_id]() {
            if (!self) return;
            UnifiedTrading::instance().cancel_order(order_id);
            QMetaObject::invokeMethod(self, [self]() {
                if (!self) return;
                self->async_fetch_orders();
            }, Qt::QueuedConnection);
        });
    }
}

void EquityTradingScreen::on_ob_price_clicked(double price) {
    order_entry_->set_current_price(price);
}

// ============================================================================
// Refresh Functions
// ============================================================================

void EquityTradingScreen::refresh_quote() {
    if (!initialized_) return;
    async_fetch_quote();
}

void EquityTradingScreen::refresh_portfolio() {
    if (portfolio_id_.isEmpty()) return;
    if (trading_mode_ == TradingMode::Live) {
        if (portfolio_fetching_.exchange(true)) return;
        async_fetch_positions();
        async_fetch_holdings();
        async_fetch_orders();
        async_fetch_funds();
        portfolio_fetching_ = false;
        return;
    }

    // Paper mode
    try {
        portfolio_ = pt_get_portfolio(portfolio_id_);
        auto positions = pt_get_positions(portfolio_id_);
        auto orders = pt_get_orders(portfolio_id_);
        auto trades = pt_get_trades(portfolio_id_, 50);
        auto stats = pt_get_stats(portfolio_id_);

        order_entry_->set_balance(portfolio_.balance);
        bottom_panel_->set_paper_positions(positions);
        bottom_panel_->set_paper_orders(orders);
        bottom_panel_->set_paper_trades(trades);
        bottom_panel_->set_paper_stats(stats);
    } catch (...) {
    }
}

void EquityTradingScreen::refresh_watchlist() {
    if (!initialized_) return;
    async_fetch_watchlist_quotes();
}

void EquityTradingScreen::refresh_candles() {
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

} // namespace fincept::screens
