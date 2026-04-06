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
#include "storage/repositories/SettingsRepository.h"
#include "trading/BrokerRegistry.h"
#include "trading/OrderMatcher.h"
#include "trading/brokers/angelone/AngelOneBroker.h"
#include "trading/instruments/InstrumentService.h"
#include "trading/PaperTrading.h"
#include "trading/UnifiedTrading.h"
#include "trading/websocket/AngelOneWebSocket.h"
#include "ui/theme/StyleSheets.h"

#include <QCompleter>
#include <QDate>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
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
    ws_teardown();
    if (fill_cb_id_ >= 0)
        OrderMatcher::instance().remove_fill_callback(fill_cb_id_);
}

// ============================================================================
// Visibility — start/stop timers (P3)
// ============================================================================

void EquityTradingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    // Start (or reconnect) WebSocket if available for this broker
    if (ws_) {
        if (!ws_->is_connected())
            ws_->open();
        else
            ws_resubscribe();
    }

    // Only run polling timers when WebSocket is not active
    const bool use_polling = !ws_active();
    if (use_polling) {
        if (quote_timer_)    quote_timer_->start();
        if (watchlist_timer_) watchlist_timer_->start();
    }
    // Portfolio + clock timers always run (WS doesn't replace these)
    if (portfolio_timer_)    portfolio_timer_->start();
    if (clock_timer_)        clock_timer_->start();
    if (market_clock_timer_) market_clock_timer_->start();

    LOG_INFO(TAG, ws_active() ? "Screen visible — WebSocket active, polling suppressed"
                              : "Screen visible — polling timers started");
}

void EquityTradingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (quote_timer_)        quote_timer_->stop();
    if (portfolio_timer_)    portfolio_timer_->stop();
    if (watchlist_timer_)    watchlist_timer_->stop();
    if (clock_timer_)        clock_timer_->stop();
    if (market_clock_timer_) market_clock_timer_->stop();
    // Keep WebSocket connected in background — it will auto-reconnect on show
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

    // Connection status indicator
    conn_label_ = new QLabel("○ NOT CONNECTED");
    conn_label_->setObjectName("eqConnLabel");
    conn_label_->setStyleSheet("color: #525252; font-size: 10px; font-weight: 700;");
    cmd_layout->addWidget(conn_label_);

    // API button
    api_btn_ = new QPushButton("API KEYS");
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
    connect(watchlist_, &EquityWatchlist::symbol_added, this, [this](const QString& sym) {
        if (!watchlist_symbols_.contains(sym))
            watchlist_symbols_.append(sym);
        if (ws_active())
            ws_resubscribe();
    });
    connect(order_entry_, &EquityOrderEntry::order_submitted, this, &EquityTradingScreen::on_order_submitted);
    connect(orderbook_, &EquityOrderBook::price_clicked, this, &EquityTradingScreen::on_ob_price_clicked);
    connect(bottom_panel_, &EquityBottomPanel::cancel_order_requested, this, &EquityTradingScreen::on_cancel_order);
    connect(bottom_panel_, &EquityBottomPanel::modify_order_requested,
            this, &EquityTradingScreen::async_modify_order);
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

    // Market clock — poll every 60s (is_open changes only at open/close boundary)
    market_clock_timer_ = new QTimer(this);
    connect(market_clock_timer_, &QTimer::timeout, this, &EquityTradingScreen::async_fetch_clock);
    market_clock_timer_->setInterval(60000);

    // Deferred init — don't block constructor (P1)
    QTimer::singleShot(100, this, [this]() {
        // Restore last selected broker
        auto saved_broker = SettingsRepository::instance().get("equity.broker", broker_id_);
        if (saved_broker.is_ok() && !saved_broker.value().isEmpty()
            && BrokerRegistry::instance().get(saved_broker.value())) {
            on_broker_changed(saved_broker.value());
        }
        init_broker();
        load_portfolio();

        // Check if credentials already saved — show connected status and load data
        auto* broker = BrokerRegistry::instance().get(broker_id_);
        if (broker) {
            auto creds = broker->load_credentials(); // finds best available env
            if (!creds.api_key.isEmpty()) {
                const QString acct = creds.user_id.isEmpty() ? broker_id_.toUpper() : creds.user_id;
                // additional_data may be JSON — extract env or skip display
                QString env_str;
                if (!creds.additional_data.isEmpty()) {
                    auto _doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
                    if (_doc.isObject()) {
                        QString cc = _doc.object().value("client_code").toString();
                        if (!cc.isEmpty() && cc != acct) env_str = QString(" [%1]").arg(cc);
                    } else {
                        env_str = QString(" [%1]").arg(creds.additional_data.toUpper());
                    }
                }
                const QString env  = env_str;
                conn_label_->setText(QString("● %1%2").arg(acct, env));
                conn_label_->setStyleSheet("color: #16a34a; font-size: 10px; font-weight: 700;");
                async_fetch_positions();
                async_fetch_holdings();
                async_fetch_orders();
                async_fetch_funds();
                async_fetch_calendar();
                async_fetch_clock();
                async_fetch_time_sales();
                async_fetch_auctions();
                async_fetch_condition_codes();

                // Start WebSocket streaming if this broker supports it.
                // Guard: on_broker_changed() may have already called ws_init().
                if (!ws_)
                    ws_init();
            }
        }

        // Only do initial REST polls if WebSocket is not already active
        if (!ws_active()) {
            refresh_quote();
            async_fetch_watchlist_quotes();
        }
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
        auto* b_profile = BrokerRegistry::instance().get(broker_id_);
        const double paper_bal = b_profile ? b_profile->profile().default_paper_balance : DEFAULT_PAPER_BALANCE;
        const QString paper_cur = b_profile ? b_profile->profile().currency : "INR";
        portfolio_ = pt_create_portfolio("Equity Paper", paper_bal, paper_cur, 1.0, "cross", 0.001, broker_id_);
        portfolio_id_ = portfolio_.id;
    }

    // Register order fill callback — auto-refresh portfolio when limit/stop orders fill
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
                    self->refresh_portfolio();
                },
                Qt::QueuedConnection);
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
    if (quote_fetching_.exchange(true))
        return;
    // Capture strings by value on the main thread — QPointer is NOT thread-safe to dereference
    const QString broker_id  = broker_id_;
    const QString symbol     = selected_symbol_;
    QPointer<EquityTradingScreen> self = this;
    LOG_DEBUG(TAG, QString("async_fetch_quote: broker=%1 symbol=%2").arg(broker_id, symbol));
    QtConcurrent::run([self, broker_id, symbol]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            LOG_WARN(TAG, QString("async_fetch_quote: broker not found: %1").arg(broker_id));
            self->quote_fetching_ = false;
            return;
        }
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) {
            LOG_WARN(TAG, QString("async_fetch_quote: no credentials for %1").arg(broker_id));
            self->quote_fetching_ = false;
            return;
        }
        LOG_DEBUG(TAG, QString("async_fetch_quote: calling get_quotes for %1").arg(symbol));
        auto result = broker->get_quotes(creds, {symbol});
        self->quote_fetching_ = false;
        if (!result.success || !result.data || result.data->isEmpty()) {
            LOG_WARN(TAG, QString("get_quotes failed: %1").arg(result.error));
            if (result.error.startsWith("[TOKEN_EXPIRED]"))
                QMetaObject::invokeMethod(self, &EquityTradingScreen::handle_token_expired, Qt::QueuedConnection);
            return;
        }
        const auto quote = result.data->first();
        QMetaObject::invokeMethod(
            self,
            [self, quote]() {
                if (!self)
                    return;
                self->current_price_ = quote.ltp;
                self->ticker_bar_->update_quote(quote.ltp, quote.change, quote.change_pct, quote.high, quote.low,
                                                quote.volume, quote.bid, quote.ask);
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
                    OrderMatcher::instance().check_sl_tp_triggers(self->portfolio_id_, self->selected_symbol_,
                                                                  quote.ltp);
                }
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_candles(const QString& symbol, const QString& timeframe) {
    if (candles_fetching_.exchange(true))
        return;
    const QString broker_id = broker_id_;
    QPointer<EquityTradingScreen> self = this;
    LOG_DEBUG(TAG, QString("async_fetch_candles: broker=%1 symbol=%2 tf=%3").arg(broker_id, symbol, timeframe));
    QtConcurrent::run([self, broker_id, symbol, timeframe]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            LOG_WARN(TAG, QString("async_fetch_candles: broker not found: %1").arg(broker_id));
            self->candles_fetching_ = false;
            return;
        }
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) {
            LOG_WARN(TAG, QString("async_fetch_candles: no credentials for %1").arg(broker_id));
            self->candles_fetching_ = false;
            return;
        }
        // Build date range — AngelOne rejects empty dates
        // For intraday go back 5 days to cover weekends/holidays
        const QString to_dt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
        QString from_dt;
        if (timeframe == "1d" || timeframe == "D" || timeframe == "1w" || timeframe == "W")
            from_dt = QDate::currentDate().addYears(-2).toString("yyyy-MM-dd") + " 00:00";
        else if (timeframe == "1h" || timeframe == "60")
            from_dt = QDate::currentDate().addDays(-30).toString("yyyy-MM-dd") + " 00:00";
        else if (timeframe == "30m")
            from_dt = QDate::currentDate().addDays(-10).toString("yyyy-MM-dd") + " 00:00";
        else if (timeframe == "15m")
            from_dt = QDate::currentDate().addDays(-7).toString("yyyy-MM-dd") + " 00:00";
        else if (timeframe == "5m")
            from_dt = QDate::currentDate().addDays(-5).toString("yyyy-MM-dd") + " 00:00";
        else  // 1m — last 5 days covers weekends
            from_dt = QDate::currentDate().addDays(-5).toString("yyyy-MM-dd") + " 00:00";
        auto result = broker->get_history(creds, symbol, timeframe, from_dt, to_dt);
        self->candles_fetching_ = false;
        if (!result.success || !result.data) {
            LOG_WARN(TAG, QString("get_history failed: %1").arg(result.error));
            if (result.error.startsWith("[TOKEN_EXPIRED]"))
                QMetaObject::invokeMethod(self, &EquityTradingScreen::handle_token_expired, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(
            self,
            [self, data = *result.data]() {
                if (!self)
                    return;
                self->chart_->set_candles(data);
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_positions() {
    const QString broker_id = broker_id_;
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self, broker_id]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            LOG_WARN(TAG, QString("async_fetch_positions: broker not found: %1").arg(broker_id));
            return;
        }
        auto creds = broker->load_credentials();
        auto result = broker->get_positions(creds);
        LOG_DEBUG(TAG, QString("async_fetch_positions: success=%1 error=%2").arg(result.success).arg(result.error));
        if (!result.success || !result.data) {
            if (result.error.startsWith("[TOKEN_EXPIRED]"))
                QMetaObject::invokeMethod(self, &EquityTradingScreen::handle_token_expired, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(
            self,
            [self, data = *result.data]() {
                if (!self)
                    return;
                self->bottom_panel_->set_positions(data);
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_holdings() {
    const QString broker_id = broker_id_;
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self, broker_id]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            LOG_WARN(TAG, QString("async_fetch_holdings: broker not found: %1").arg(broker_id));
            return;
        }
        auto creds = broker->load_credentials();
        auto result = broker->get_holdings(creds);
        LOG_DEBUG(TAG, QString("async_fetch_holdings: success=%1 error=%2").arg(result.success).arg(result.error));
        if (!result.success || !result.data) {
            if (result.error.startsWith("[TOKEN_EXPIRED]"))
                QMetaObject::invokeMethod(self, &EquityTradingScreen::handle_token_expired, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(
            self,
            [self, data = *result.data]() {
                if (!self)
                    return;
                self->bottom_panel_->set_holdings(data);
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_orders() {
    const QString broker_id = broker_id_;
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self, broker_id]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            LOG_WARN(TAG, QString("async_fetch_orders: broker not found: %1").arg(broker_id));
            return;
        }
        auto creds = broker->load_credentials();
        auto result = broker->get_orders(creds);
        LOG_DEBUG(TAG, QString("async_fetch_orders: success=%1 error=%2").arg(result.success).arg(result.error));
        if (!result.success || !result.data) {
            if (result.error.startsWith("[TOKEN_EXPIRED]"))
                QMetaObject::invokeMethod(self, &EquityTradingScreen::handle_token_expired, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(
            self,
            [self, data = *result.data]() {
                if (!self)
                    return;
                self->bottom_panel_->set_orders(data);
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_funds() {
    const QString broker_id = broker_id_;
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self, broker_id]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            LOG_WARN(TAG, QString("async_fetch_funds: broker not found: %1").arg(broker_id));
            return;
        }
        auto creds = broker->load_credentials();
        auto result = broker->get_funds(creds);
        LOG_DEBUG(TAG, QString("async_fetch_funds: success=%1 error=%2").arg(result.success).arg(result.error));
        if (!result.success || !result.data) {
            if (result.error.startsWith("[TOKEN_EXPIRED]"))
                QMetaObject::invokeMethod(self, &EquityTradingScreen::handle_token_expired, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(
            self,
            [self, data = *result.data]() {
                if (!self)
                    return;
                self->bottom_panel_->set_funds(data);
                self->order_entry_->set_balance(data.available_balance);
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_watchlist_quotes() {
    const QString broker_id       = broker_id_;
    const QStringList wl_symbols  = watchlist_symbols_;
    QPointer<EquityTradingScreen> self = this;
    LOG_DEBUG(TAG, QString("async_fetch_watchlist_quotes: broker=%1 symbols=%2").arg(broker_id).arg(wl_symbols.join(",")));
    QtConcurrent::run([self, broker_id, wl_symbols]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            LOG_WARN(TAG, QString("async_fetch_watchlist_quotes: broker not found: %1").arg(broker_id));
            return;
        }
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) {
            LOG_WARN(TAG, QString("async_fetch_watchlist_quotes: no credentials for %1").arg(broker_id));
            return;
        }
        LOG_DEBUG(TAG, QString("async_fetch_watchlist_quotes: calling get_quotes for %1 symbols").arg(wl_symbols.size()));
        auto result = broker->get_quotes(creds, wl_symbols.toVector());
        if (!result.success || !result.data) {
            LOG_WARN(TAG, QString("async_fetch_watchlist_quotes: get_quotes failed: %1").arg(result.error));
            return;
        }
        LOG_DEBUG(TAG, QString("async_fetch_watchlist_quotes: got %1 quotes").arg(result.data->size()));
        QMetaObject::invokeMethod(
            self,
            [self, data = *result.data]() {
                if (!self)
                    return;
                self->watchlist_->update_quotes(data);

                // Feed watchlist prices into paper trading engine for all symbols
                if (self->trading_mode_ == TradingMode::Paper && !self->portfolio_id_.isEmpty()) {
                    for (const auto& q : data) {
                        if (q.ltp <= 0)
                            continue;
                        pt_update_position_price(self->portfolio_id_, q.symbol, q.ltp);
                        PriceData pd;
                        pd.last = q.ltp;
                        pd.bid = q.bid;
                        pd.ask = q.ask;
                        pd.timestamp = q.timestamp;
                        OrderMatcher::instance().check_orders(q.symbol, pd, self->portfolio_id_);
                    }
                }
            },
            Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_orderbook() {
    if (current_price_ <= 0)
        return;
    const QString broker_id = broker_id_;
    const QString symbol    = selected_symbol_;
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self, broker_id, symbol]() {
        auto* broker = BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            LOG_WARN(TAG, QString("async_fetch_orderbook: broker not found: %1").arg(broker_id));
            return;
        }
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) return;

        // Fetch recent quote ticks — reconstruct L2 by aggregating bid/ask sizes per price level
        // Use today's quotes limited to 1000 ticks which gives real depth
        const QString today = QDate::currentDate().toString("yyyy-MM-dd");
        auto result = broker->get_historical_quotes_single(creds, symbol,
                                                           today + "T00:00:00Z", "", 1000);

        // Fall back to snapshot best bid/ask if quotes unavailable (pre-market, free tier limits)
        if (!result.success || !result.data || result.data->isEmpty()) {
            LOG_DEBUG(TAG, QString("async_fetch_orderbook: falling back to snapshot for %1").arg(symbol));
            auto snap = broker->get_quotes(creds, {symbol});
            if (!snap.success || !snap.data || snap.data->isEmpty()) return;
            const auto& q = snap.data->first();
            // Use LTP as fallback if bid/ask not available (some brokers omit depth in basic mode)
            const double bid = q.bid > 0 ? q.bid : (q.ltp > 0 ? q.ltp * 0.9995 : 0);
            const double ask = q.ask > 0 ? q.ask : (q.ltp > 0 ? q.ltp * 1.0005 : 0);
            if (bid <= 0 || ask <= 0) return;
            const double bid_sz = q.bid_size > 0 ? q.bid_size : 100.0;
            const double ask_sz = q.ask_size > 0 ? q.ask_size : 100.0;
            QVector<QPair<double, double>> bids{{bid, bid_sz}}, asks_{{ask, ask_sz}};
            const double spread = ask - bid;
            const double spread_pct = bid > 0 ? (spread / bid) * 100.0 : 0.0;
            QMetaObject::invokeMethod(self, [self, bids, asks_, spread, spread_pct]() {
                if (!self) return;
                self->orderbook_->set_data(bids, asks_, spread, spread_pct);
            }, Qt::QueuedConnection);
            return;
        }

        // Aggregate: accumulate size at each price level
        QMap<double, double> bid_map, ask_map;
        for (const auto& q : *result.data) {
            if (q.bid > 0) bid_map[q.bid] += q.bid_size > 0 ? q.bid_size : 1.0;
            if (q.ask > 0) ask_map[q.ask] += q.ask_size > 0 ? q.ask_size : 1.0;
        }

        // Sort: bids descending (best first), asks ascending (best first)
        QVector<QPair<double, double>> bids, asks_v;
        auto bid_keys = bid_map.keys();
        std::sort(bid_keys.begin(), bid_keys.end(), std::greater<double>());
        for (const double p : bid_keys.mid(0, 10))
            bids.append({p, bid_map[p]});

        auto ask_keys = ask_map.keys();
        std::sort(ask_keys.begin(), ask_keys.end());
        for (const double p : ask_keys.mid(0, 10))
            asks_v.append({p, ask_map[p]});

        if (bids.isEmpty() || asks_v.isEmpty()) return;

        const double best_bid = bids.first().first;
        const double best_ask = asks_v.first().first;
        const double spread     = best_ask - best_bid;
        const double spread_pct = best_bid > 0 ? (spread / best_bid) * 100.0 : 0.0;

        QMetaObject::invokeMethod(self, [self, bids, asks_v, spread, spread_pct]() {
            if (!self) return;
            self->orderbook_->set_data(bids, asks_v, spread, spread_pct);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_time_sales() {
    if (time_sales_fetching_.exchange(true)) return;
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) { self->time_sales_fetching_ = false; return; }
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) { self->time_sales_fetching_ = false; return; }
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) { self->time_sales_fetching_ = false; return; }

        // Fetch today's trades — limit 500 for initial load
        const QString today = QDate::currentDate().toString("yyyy-MM-dd");
        auto result = broker->get_historical_trades_single(creds, self->selected_symbol_,
                                                           today + "T00:00:00Z", "", 500);
        self->time_sales_fetching_ = false;
        if (!result.success || !result.data) return;

        auto trades = *result.data;
        QMetaObject::invokeMethod(self, [self, trades]() {
            if (!self) return;
            self->bottom_panel_->set_time_sales(trades);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_latest_trade() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) return;

        auto result = broker->get_latest_trade(creds, self->selected_symbol_);
        if (!result.success || !result.data) return;

        auto trade = *result.data;
        QMetaObject::invokeMethod(self, [self, trade]() {
            if (!self) return;
            self->bottom_panel_->prepend_trade(trade);
        }, Qt::QueuedConnection);
    });
}

// Fetch the market calendar for the next 30 days (and past 5 for context)
void EquityTradingScreen::async_fetch_calendar() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) return;

        const QString start = QDate::currentDate().addDays(-5).toString("yyyy-MM-dd");
        const QString end   = QDate::currentDate().addDays(30).toString("yyyy-MM-dd");
        auto result = broker->get_calendar(creds, start, end);
        if (!result.success || !result.data) return;

        auto days = *result.data;
        QMetaObject::invokeMethod(self, [self, days]() {
            if (!self) return;
            self->bottom_panel_->set_calendar(days);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_clock() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) return;

        auto result = broker->get_clock(creds);
        if (!result.success || !result.data) return;

        auto clock = *result.data;
        QMetaObject::invokeMethod(self, [self, clock]() {
            if (!self) return;
            self->bottom_panel_->set_clock(clock);
        }, Qt::QueuedConnection);
    });
}

// ============================================================================
// Slot Handlers
// ============================================================================

void EquityTradingScreen::on_broker_changed(const QString& broker) {
    broker_id_ = broker;
    broker_btn_->setText(broker.toUpper());
    SettingsRepository::instance().set("equity.broker", broker, "equity_trading");

    // Reset connection status, then check if credentials already saved
    conn_label_->setText("○ NOT CONNECTED");
    conn_label_->setStyleSheet("color: #525252; font-size: 10px; font-weight: 700;");

    // Update default watchlist based on broker region
    auto* b = BrokerRegistry::instance().get(broker);
    if (b) {
        const auto prof = b->profile();

        // Update watchlist and selected symbol/exchange from profile
        watchlist_symbols_ = QStringList(prof.default_watchlist.begin(), prof.default_watchlist.end());
        selected_symbol_   = prof.default_symbol;
        selected_exchange_ = prof.default_exchange;

        // Configure order entry for this broker's product types / exchanges
        order_entry_->configure_for_broker(prof);
        order_entry_->set_broker_id(broker_id_);
        watchlist_->set_broker_id(broker_id_);

        // For brokers with native paper trading, always use Live mode
        // (their paper server handles simulation); for others, stay in Paper mode
        if (prof.has_native_paper) {
            trading_mode_ = TradingMode::Live;
            mode_btn_->setChecked(true);
            mode_btn_->setText("LIVE");
            mode_btn_->setProperty("mode", "live");
            mode_btn_->style()->unpolish(mode_btn_);
            mode_btn_->style()->polish(mode_btn_);
            order_entry_->set_mode(false);
            bottom_panel_->set_mode(false);
            UnifiedTrading::instance().switch_mode("live");
        } else {
            trading_mode_ = TradingMode::Paper;
            mode_btn_->setChecked(false);
            mode_btn_->setText("PAPER");
            mode_btn_->setProperty("mode", "paper");
            mode_btn_->style()->unpolish(mode_btn_);
            mode_btn_->style()->polish(mode_btn_);
            order_entry_->set_mode(true);
            bottom_panel_->set_mode(true);
            UnifiedTrading::instance().switch_mode("paper");
        }
    }

    exchange_label_->setText(selected_exchange_);
    symbol_input_->setText(selected_symbol_);
    watchlist_->set_symbols(watchlist_symbols_);
    ticker_bar_->set_symbol(selected_symbol_);
    ticker_bar_->set_currency(currency_symbol(exchange_currency(selected_exchange_)));
    order_entry_->set_symbol(selected_symbol_);
    order_entry_->set_exchange(selected_exchange_);

    init_broker();
    load_portfolio();
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    async_fetch_watchlist_quotes();

    // Check if credentials already saved for this broker
    if (b) {
        auto creds = b->load_credentials();
        if (!creds.api_key.isEmpty()) {
            const QString acct = creds.user_id.isEmpty() ? broker_id_.toUpper() : creds.user_id;
            QString env_str;
            if (!creds.additional_data.isEmpty()) {
                auto _doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
                if (_doc.isObject()) {
                    QString cc = _doc.object().value("client_code").toString();
                    if (!cc.isEmpty()) env_str = QString(" [%1]").arg(cc);
                } else {
                    env_str = QString(" [%1]").arg(creds.additional_data.toUpper());
                }
            }
            conn_label_->setText(QString("● %1%2").arg(acct, env_str));
            conn_label_->setStyleSheet("color: #16a34a; font-size: 10px; font-weight: 700;");
            async_fetch_positions();
            async_fetch_holdings();
            async_fetch_orders();
            async_fetch_funds();
            async_fetch_calendar();
            async_fetch_clock();
            async_fetch_time_sales();
            async_fetch_auctions();
            async_fetch_condition_codes();

            // (Re)start WebSocket for the new broker
            ws_teardown();
            ws_init();
        }
    }
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

    if (ws_active()) {
        // Update WS subscriptions to include new symbol; REST poll not needed
        ws_resubscribe();
    } else {
        refresh_quote();
    }
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    if (trading_mode_ == TradingMode::Live) {
        async_fetch_time_sales();
        async_fetch_auctions();
    }
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
        async_fetch_calendar();
        async_fetch_clock();
        async_fetch_time_sales();
        async_fetch_auctions();
        async_fetch_condition_codes();
    } else {
        refresh_portfolio();
    }
}

void EquityTradingScreen::handle_token_expired() {
    // Guard: only show one re-auth prompt at a time
    bool expected = false;
    if (!token_expired_shown_.compare_exchange_strong(expected, true))
        return;

    conn_label_->setText("⚠ TOKEN EXPIRED");
    conn_label_->setStyleSheet("color: #dc2626; font-size: 10px; font-weight: 700;");
    LOG_WARN(TAG, QString("Access token expired for broker %1 — prompting re-auth").arg(broker_id_));

    // Re-open the credentials dialog so the user can paste a new request_token
    on_api_clicked();

    token_expired_shown_.store(false);
}

void EquityTradingScreen::on_api_clicked() {
    auto* broker_for_dlg = BrokerRegistry::instance().get(broker_id_);
    auto prof = broker_for_dlg ? broker_for_dlg->profile() : trading::BrokerProfile{};
    auto* dlg = new EquityCredentials(broker_id_, prof, this);

    QPointer<EquityCredentials> dlg_ptr = dlg;
    QPointer<EquityTradingScreen> self = this;

    connect(dlg, &EquityCredentials::credentials_saved, this,
            [self, dlg_ptr](const QString& bid, const QString& key, const QString& secret, const QString& auth) {
                if (!self) return;
                self->conn_label_->setText("◌ CONNECTING...");
                self->conn_label_->setStyleSheet("color: #ca8a04; font-size: 10px; font-weight: 700;");
                self->api_btn_->setEnabled(false);

                QtConcurrent::run([self, dlg_ptr, bid, key, secret, auth]() {
                    if (!self) return;
                    auto* broker = BrokerRegistry::instance().get(bid);
                    if (!broker) return;

                    auto token_result = broker->exchange_token(key, secret, auth);
                    QMetaObject::invokeMethod(self, [self, dlg_ptr, bid, key, secret, token_result, auth]() {
                        if (!self) return;
                        self->api_btn_->setEnabled(true);

                        if (token_result.success) {
                            auto* broker = BrokerRegistry::instance().get(bid);
                            if (!broker) return;

                            // Save credentials — env is in token_result.additional_data (auto-detected)
                            BrokerCredentials creds;
                            creds.broker_id       = bid;
                            creds.api_key         = key;
                            creds.api_secret      = secret;
                            creds.access_token    = token_result.access_token;
                            creds.user_id         = token_result.user_id;
                            creds.additional_data = token_result.additional_data;
                            broker->save_credentials(creds);
                            LOG_INFO(TAG, QString("Connected to %1 env=%2 user=%3")
                                             .arg(bid, token_result.additional_data, token_result.user_id));

                            // Kick off async instrument refresh so token lookups
                            // (get_history, symbol search) work immediately after login.
                            // Skips download if instruments are already fresh.
                            trading::InstrumentService::instance().refresh(bid, creds);

                            const QString acct = token_result.user_id.isEmpty()
                                                     ? bid.toUpper() : token_result.user_id;
                            self->conn_label_->setText(QString("● %1").arg(acct));
                            self->conn_label_->setStyleSheet(
                                "color: #16a34a; font-size: 10px; font-weight: 700;");

                            // Update dialog status — stays open so user can add other env
                            if (dlg_ptr)
                                dlg_ptr->mark_connected(token_result.additional_data, acct);

                            // (Re)start WebSocket with fresh credentials
                            self->ws_teardown();
                            self->ws_init();

                            // Full data refresh — skip REST quote/watchlist if WS is now active
                            if (!self->ws_active()) {
                                self->refresh_quote();
                                self->async_fetch_watchlist_quotes();
                            }
                            self->async_fetch_candles(self->selected_symbol_,
                                                      self->chart_->current_timeframe());
                            self->async_fetch_positions();
                            self->async_fetch_holdings();
                            self->async_fetch_orders();
                            self->async_fetch_funds();
                        } else {
                            LOG_ERROR(TAG, QString("Auth failed %1: %2").arg(bid, token_result.error));
                            self->conn_label_->setText(QString("✕ %1").arg(token_result.error));
                            self->conn_label_->setStyleSheet(
                                "color: #dc2626; font-size: 10px; font-weight: 700;");
                            if (dlg_ptr) dlg_ptr->mark_error(token_result.error);
                        }
                    }, Qt::QueuedConnection);
                });
            });

    dlg->exec();
    dlg->deleteLater();
}

void EquityTradingScreen::on_order_submitted(const UnifiedOrder& order) {
    if (trading_mode_ == TradingMode::Paper) {
        LOG_INFO(TAG, QString("Paper order: %1 %2 x%3 type=%4 portfolio=%5")
                          .arg(order.side == OrderSide::Buy ? "BUY" : "SELL")
                          .arg(order.symbol)
                          .arg(order.quantity)
                          .arg(order_type_str(order.order_type))
                          .arg(portfolio_id_));

        if (portfolio_id_.isEmpty()) {
            LOG_ERROR(TAG, "Paper order rejected: no active portfolio — create one first");
            order_entry_->show_order_status("No active portfolio — create one first", false);
            return;
        }

        const QString side = order.side == OrderSide::Buy ? "buy" : "sell";
        const QString type = order_type_str(order.order_type);
        std::optional<double> price_opt;
        if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
            price_opt = order.price;
        } else if (order.order_type == OrderType::Market) {
            // Pass current price as reference for margin calculation.
            // For market orders this is not a limit price — it's only used to check
            // if the account has enough margin. Falls back to manually entered price.
            const double ref = current_price_ > 0 ? current_price_
                             : (order.price > 0   ? order.price : 0.0);
            if (ref > 0)
                price_opt = ref;
        }
        std::optional<double> stop_opt;
        if (order.stop_price > 0)
            stop_opt = order.stop_price;

        if (order.order_type == OrderType::Market && !price_opt) {
            LOG_WARN(TAG, QString("Paper market order blocked: no price for %1 — instruments may still be loading").arg(order.symbol));
            order_entry_->show_order_status("Price not available yet — wait for quotes to load", false);
            return;
        }

        try {
            auto pt_order = pt_place_order(portfolio_id_, order.symbol, side, type, order.quantity, price_opt, stop_opt);
            LOG_INFO(TAG, QString("Paper order placed: id=%1 symbol=%2 side=%3 qty=%4")
                              .arg(pt_order.id).arg(pt_order.symbol).arg(pt_order.side).arg(pt_order.quantity));

            if (order.order_type == OrderType::Market) {
                // Use the live ticker price for paper fill; fall back to order price or last known
                double fill_price = current_price_ > 0 ? current_price_ : (order.price > 0 ? order.price : 0.0);
                if (fill_price <= 0) {
                    LOG_WARN(TAG, "No price available for paper market order — skipping fill");
                    order_entry_->show_order_status("No price available for fill", false);
                    return;
                }
                LOG_INFO(TAG, QString("Filling paper order %1 at %2").arg(pt_order.id).arg(fill_price));
                pt_fill_order(pt_order.id, fill_price);
                order_entry_->show_order_status(
                    QString("Paper order filled: %1 @ %2").arg(order.symbol).arg(fill_price, 0, 'f', 2), true);
            } else {
                OrderMatcher::instance().add_order(pt_order);
                order_entry_->show_order_status(
                    QString("Paper order queued: %1").arg(order.symbol), true);
            }
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Paper order failed: %1 | symbol=%2 side=%3 qty=%4 price=%5 portfolio=%6")
                              .arg(e.what())
                              .arg(order.symbol).arg(side).arg(order.quantity)
                              .arg(price_opt.value_or(0.0))
                              .arg(portfolio_id_));
            order_entry_->show_order_status(QString("Order failed: %1").arg(e.what()), false);
            return;
        }
        refresh_portfolio();
    } else {
        // Route to live broker via UnifiedTrading
        QPointer<EquityTradingScreen> self = this;
        auto order_copy = order;
        QtConcurrent::run([self, order_copy]() {
            if (!self)
                return;
            auto result = UnifiedTrading::instance().place_order(order_copy);
            QMetaObject::invokeMethod(
                self,
                [self, result]() {
                    if (!self)
                        return;
                    if (result.success) {
                        LOG_INFO(TAG, QString("Order placed: %1").arg(result.order_id));
                        self->order_entry_->show_order_status(
                            QString("Order placed: %1").arg(result.order_id), true);
                        self->async_fetch_orders();
                        self->async_fetch_positions();
                        self->async_fetch_funds();
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
    if (trading_mode_ == TradingMode::Paper) {
        try {
            pt_cancel_order(order_id);
            OrderMatcher::instance().remove_order(order_id);
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Paper cancel failed: %1 order_id=%2").arg(e.what(), order_id));
        }
        refresh_portfolio();
    } else {
        QPointer<EquityTradingScreen> self = this;
        QtConcurrent::run([self, order_id]() {
            if (!self)
                return;
            UnifiedTrading::instance().cancel_order(order_id);
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    if (!self)
                        return;
                    self->async_fetch_orders();
                },
                Qt::QueuedConnection);
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
    if (!initialized_)
        return;
    // WebSocket provides real-time quotes — skip REST poll when active
    if (ws_active())
        return;
    async_fetch_quote();
    async_fetch_orderbook();
    if (trading_mode_ == TradingMode::Live)
        async_fetch_latest_trade();
}

void EquityTradingScreen::refresh_portfolio() {
    if (portfolio_id_.isEmpty())
        return;
    if (trading_mode_ == TradingMode::Live) {
        if (portfolio_fetching_.exchange(true))
            return;
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
        LOG_WARN("EquityTradingScreen", "Exception refreshing portfolio panel");
    }
}

void EquityTradingScreen::refresh_watchlist() {
    if (!initialized_)
        return;
    // WebSocket pushes watchlist prices in real-time — skip REST poll when active
    if (ws_active())
        return;
    async_fetch_watchlist_quotes();
}

void EquityTradingScreen::refresh_candles() {
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
}

void EquityTradingScreen::async_fetch_auctions() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) return;

        const QString start = QDate::currentDate().addDays(-30).toString("yyyy-MM-dd");
        const QString end   = QDate::currentDate().toString("yyyy-MM-dd");
        auto result = broker->get_historical_auctions_single(creds, self->selected_symbol_, start, end);
        if (!result.success || !result.data) return;

        auto auctions = *result.data;
        QMetaObject::invokeMethod(self, [self, auctions]() {
            if (!self) return;
            self->bottom_panel_->set_auctions(auctions);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_fetch_condition_codes() {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) return;

        // Fetch trade condition codes for tape C (most common)
        auto result = broker->get_condition_codes(creds, "trades", "C");
        if (!result.success || !result.data) return;

        QMap<QString, QString> codes;
        for (const auto& e : *result.data)
            codes[e.code] = e.description;

        QMetaObject::invokeMethod(self, [self, codes]() {
            if (!self) return;
            self->bottom_panel_->set_condition_codes(codes);
        }, Qt::QueuedConnection);
    });
}

void EquityTradingScreen::async_modify_order(const QString& order_id, double qty, double price) {
    QPointer<EquityTradingScreen> self = this;
    QtConcurrent::run([self, order_id, qty, price]() {
        if (!self) return;
        auto* broker = BrokerRegistry::instance().get(self->broker_id_);
        if (!broker) return;
        auto creds = broker->load_credentials();
        if (creds.api_key.isEmpty()) return;

        QJsonObject mods;
        if (qty > 0)   mods["qty"]         = QString::number(qty, 'f', 0);
        if (price > 0) mods["limit_price"] = QString::number(price, 'f', 2);

        auto result = broker->modify_order(creds, order_id, mods);
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;
            if (result.success) {
                self->order_entry_->show_order_status("Order modified", true);
                self->async_fetch_orders();
            } else {
                self->order_entry_->show_order_status(
                    result.error.isEmpty() ? "Modify failed" : result.error, false);
            }
        }, Qt::QueuedConnection);
    });
}

// ============================================================================
// WebSocket — Angel One SmartStream integration
// ============================================================================

bool EquityTradingScreen::ws_active() const {
    return ws_ && ws_->is_connected();
}

void EquityTradingScreen::ws_init() {
    // Only Angel One supports SmartStream WebSocket
    if (broker_id_ != "angelone")
        return;

    auto* broker = BrokerRegistry::instance().get(broker_id_);
    if (!broker)
        return;

    auto creds = broker->load_credentials();
    if (creds.api_key.isEmpty())
        return;

    // Extract feed_token and client_code from additional_data JSON
    QString feed_token, client_code;
    if (!creds.additional_data.isEmpty()) {
        auto doc = QJsonDocument::fromJson(creds.additional_data.toUtf8());
        if (doc.isObject()) {
            feed_token  = doc.object().value("feed_token").toString();
            client_code = doc.object().value("client_code").toString();
        }
    }

    if (feed_token.isEmpty() || client_code.isEmpty()) {
        LOG_WARN(TAG, "AngelOne: feed_token or client_code missing — WebSocket unavailable, falling back to polling");
        return;
    }

    // Ensure instrument cache is populated so token→symbol lookup works for tick enrichment.
    // load_from_db() is instant (reads SQLite). refresh() kicks off a background download
    // only if the cache is stale (> 12 hours old), so this is safe to call every time.
    auto& inst_svc = trading::InstrumentService::instance();
    if (!inst_svc.is_loaded("angelone"))
        inst_svc.load_from_db("angelone");
    inst_svc.refresh("angelone", creds); // no-op if already fresh

    // If instruments are still being downloaded, ws_resubscribe() called in on_ws_connected()
    // will find empty tokens. Reconnect once the download finishes.
    connect(&inst_svc, &trading::InstrumentService::refresh_done, this,
            [this](const QString& broker_id) {
                if (broker_id != "angelone") return;
                if (ws_active())
                    ws_resubscribe();
                // Instruments just loaded — immediately fetch quotes via REST
                // (handles the case where WS is unavailable / still connecting)
                QMetaObject::invokeMethod(this, [this]() {
                    refresh_quote();
                    refresh_watchlist();
                }, Qt::QueuedConnection);
            }, Qt::UniqueConnection);

    ws_ = new trading::AngelOneWebSocket(creds.api_key, client_code, feed_token, this);
    connect(ws_, &trading::AngelOneWebSocket::tick_received,   this, &EquityTradingScreen::on_ws_tick);
    connect(ws_, &trading::AngelOneWebSocket::connected,       this, &EquityTradingScreen::on_ws_connected);
    connect(ws_, &trading::AngelOneWebSocket::disconnected,    this, &EquityTradingScreen::on_ws_disconnected);
    connect(ws_, &trading::AngelOneWebSocket::error_occurred,  this, [this](const QString& err) {
        LOG_WARN(TAG, QString("AngelOne WS error: %1 — falling back to polling").arg(err));
        // Re-enable polling timers so prices don't freeze on WS error
        if (quote_timer_ && !quote_timer_->isActive())
            quote_timer_->start();
        if (watchlist_timer_ && !watchlist_timer_->isActive())
            watchlist_timer_->start();
    });

    ws_->open();
    LOG_INFO(TAG, "AngelOne WebSocket initialised");
}

void EquityTradingScreen::ws_teardown() {
    if (!ws_)
        return;
    ws_->close();
    ws_->deleteLater();
    ws_ = nullptr;
    LOG_INFO(TAG, "AngelOne WebSocket torn down");
}

void EquityTradingScreen::ws_resubscribe() {
    if (!ws_)
        return;

    // Build subscription list: selected symbol + all watchlist symbols
    QVector<trading::AngelOneWebSocket::Subscription> subs;
    auto add_symbol = [&](const QString& sym) {
        // Determine exchange type from symbol prefix (e.g. "NSE:RELIANCE" → NSE_CM)
        QString exchange = selected_exchange_;
        QString bare_sym = sym;
        if (sym.contains(':')) {
            exchange = sym.section(':', 0, 0);
            bare_sym = sym.section(':', 1);
        }

        trading::AoExchangeType ex_type = trading::AoExchangeType::NSE_CM;
        if (exchange == "BSE")       ex_type = trading::AoExchangeType::BSE_CM;
        else if (exchange == "NFO")  ex_type = trading::AoExchangeType::NSE_FO;
        else if (exchange == "BFO")  ex_type = trading::AoExchangeType::BSE_FO;
        else if (exchange == "MCX")  ex_type = trading::AoExchangeType::MCX_FO;
        else if (exchange == "NCDEX") ex_type = trading::AoExchangeType::NCX_FO;
        else if (exchange == "CDS")  ex_type = trading::AoExchangeType::CDE_FO;

        quint32 token = trading::AngelOneBroker::lookup_token_int(bare_sym, exchange);
        if (token == 0)
            return;

        trading::AngelOneWebSocket::Subscription sub;
        sub.token         = QString::number(token);
        sub.exchange_type = ex_type;
        subs.append(sub);
    };

    add_symbol(selected_symbol_);
    for (const auto& sym : std::as_const(watchlist_symbols_))
        add_symbol(sym);

    if (subs.isEmpty())
        return;

    ws_->set_subscriptions(subs, trading::AoSubMode::SnapQuote);
    LOG_INFO(TAG, QString("WS subscribed to %1 symbols").arg(subs.size()));
}

void EquityTradingScreen::on_ws_connected() {
    LOG_INFO(TAG, "AngelOne WebSocket connected — stopping polling timers");
    // Stop quote/watchlist polling — WebSocket now provides prices
    if (quote_timer_)     quote_timer_->stop();
    if (watchlist_timer_) watchlist_timer_->stop();

    // Append WS indicator to existing account label (e.g. "● CLIENTID [WS]")
    QString existing = conn_label_->text();
    if (!existing.contains("[WS]"))
        conn_label_->setText(existing + " [WS]");
    conn_label_->setStyleSheet("color: #16a34a; font-size: 10px; font-weight: 700;");

    ws_resubscribe();
}

void EquityTradingScreen::on_ws_disconnected() {
    LOG_WARN(TAG, "AngelOne WebSocket disconnected — restarting polling timers");

    // Strip [WS] indicator — no longer streaming
    QString lbl = conn_label_->text();
    lbl.remove(" [WS]");
    conn_label_->setText(lbl);

    // Fall back to polling while disconnected / reconnecting
    if (isVisible()) {
        if (quote_timer_ && !quote_timer_->isActive())
            quote_timer_->start();
        if (watchlist_timer_ && !watchlist_timer_->isActive())
            watchlist_timer_->start();
    }
}

void EquityTradingScreen::on_ws_tick(const trading::AoTick& tick) {
    // Derive display symbol — prefer enriched symbol, fall back to token
    const QString sym = tick.symbol.isEmpty() ? tick.token : tick.symbol;
    if (sym.isEmpty())
        return;

    const double ltp = tick.ltp;
    if (ltp <= 0.0)
        return;

    // ── Update watchlist for any symbol ───────────────────────────────────
    // Build a minimal BrokerQuote from the tick and pass it to the watchlist
    {
        trading::BrokerQuote q;
        q.symbol     = sym;
        q.ltp        = ltp;
        q.open       = tick.open;
        q.high       = tick.high;
        q.low        = tick.low;
        q.close      = tick.close;
        q.volume     = static_cast<double>(tick.volume);
        q.change     = (tick.close > 0.0) ? (ltp - tick.close) : 0.0;
        q.change_pct = (tick.close > 0.0) ? ((ltp - tick.close) / tick.close * 100.0) : 0.0;
        // Best bid/ask from depth (SnapQuote mode)
        q.bid      = (tick.bids[0].price > 0) ? tick.bids[0].price / 100.0 : 0.0;
        q.ask      = (tick.asks[0].price > 0) ? tick.asks[0].price / 100.0 : 0.0;
        q.bid_size = static_cast<double>(tick.bids[0].quantity);
        q.ask_size = static_cast<double>(tick.asks[0].quantity);
        q.timestamp = tick.exchange_timestamp.isValid()
                      ? tick.exchange_timestamp.toMSecsSinceEpoch() / 1000
                      : QDateTime::currentSecsSinceEpoch();

        watchlist_->update_quotes({q});

        // Feed into paper trading engine for all symbols
        if (trading_mode_ == TradingMode::Paper && !portfolio_id_.isEmpty()) {
            pt_update_position_price(portfolio_id_, sym, ltp);
            trading::PriceData pd;
            pd.last      = ltp;
            pd.bid       = q.bid;
            pd.ask       = q.ask;
            pd.timestamp = q.timestamp;
            trading::OrderMatcher::instance().check_orders(sym, pd, portfolio_id_);
        }
    }

    // ── Update selected symbol UI ──────────────────────────────────────────
    if (sym != selected_symbol_)
        return;

    current_price_ = ltp;

    const double change     = (tick.close > 0.0) ? (ltp - tick.close) : 0.0;
    const double change_pct = (tick.close > 0.0) ? (change / tick.close * 100.0) : 0.0;
    const double bid        = (tick.bids[0].price > 0) ? tick.bids[0].price / 100.0 : 0.0;
    const double ask        = (tick.asks[0].price > 0) ? tick.asks[0].price / 100.0 : 0.0;

    ticker_bar_->update_quote(ltp, change, change_pct, tick.high, tick.low,
                              static_cast<double>(tick.volume), bid, ask);
    order_entry_->set_current_price(ltp);

    // Order book depth from SnapQuote best-5
    if (tick.mode == trading::AoSubMode::SnapQuote) {
        QVector<QPair<double, double>> bids_v, asks_v;
        for (int i = 0; i < 5; ++i) {
            if (tick.bids[i].price > 0)
                bids_v.append({tick.bids[i].price / 100.0, static_cast<double>(tick.bids[i].quantity)});
            if (tick.asks[i].price > 0)
                asks_v.append({tick.asks[i].price / 100.0, static_cast<double>(tick.asks[i].quantity)});
        }
        if (!bids_v.isEmpty() && !asks_v.isEmpty()) {
            const double spread     = asks_v[0].first - bids_v[0].first;
            const double spread_pct = bids_v[0].first > 0 ? (spread / bids_v[0].first) * 100.0 : 0.0;
            orderbook_->set_data(bids_v, asks_v, spread, spread_pct);
        }
    }

    // Paper trading SL/TP triggers for selected symbol
    if (trading_mode_ == TradingMode::Paper && !portfolio_id_.isEmpty())
        trading::OrderMatcher::instance().check_sl_tp_triggers(portfolio_id_, sym, ltp);
}

} // namespace fincept::screens
