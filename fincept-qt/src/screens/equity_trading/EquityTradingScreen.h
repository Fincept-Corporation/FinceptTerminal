#pragma once
// Equity Trading Screen — Bloomberg-style coordinator
// Routes to live brokers (16 supported) or paper trading engine.

#include "screens/IStatefulScreen.h"
#include "screens/equity_trading/EquityTypes.h"
#include "services/workspace/IWorkspaceParticipant.h"
#include "trading/TradingTypes.h"
#include "trading/websocket/AngelOneTickTypes.h"

#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMutex>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

#include <atomic>

namespace fincept::trading {
class AngelOneWebSocket;
} // namespace fincept::trading

namespace fincept::screens::equity {
class EquityTickerBar;
class EquityWatchlist;
class EquityChart;
class EquityOrderEntry;
class EquityOrderBook;
class EquityBottomPanel;
} // namespace fincept::screens::equity

namespace fincept::screens {

class EquityTradingScreen : public QWidget, public IWorkspaceParticipant {
    Q_OBJECT
  public:
    explicit EquityTradingScreen(QWidget* parent = nullptr);
    ~EquityTradingScreen();

    QJsonObject save_state() const override;
    void restore_state(const QJsonObject& state) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_broker_changed(const QString& broker);
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();
    void on_api_clicked();
    void handle_token_expired();
    void on_order_submitted(const trading::UnifiedOrder& order);
    void on_cancel_order(const QString& order_id);
    void on_ob_price_clicked(double price);

    void refresh_quote();
    void refresh_portfolio();
    void refresh_watchlist();
    void refresh_candles();
    void update_clock();

    // WebSocket slots
    void on_ws_tick(const trading::AoTick& tick);
    void on_ws_connected();
    void on_ws_disconnected();

  private:
    void setup_ui();
    void setup_timers();
    void init_broker();
    void load_portfolio();
    void switch_symbol(const QString& symbol);

    // WebSocket helpers
    void ws_init();           // create + connect ws_ for current broker (Angel One only)
    void ws_teardown();       // close + delete ws_
    void ws_resubscribe();    // push current symbol + watchlist tokens to ws_
    bool ws_active() const;   // true when ws_ is non-null and connected

    void async_fetch_quote();
    void async_fetch_candles(const QString& symbol, const QString& timeframe);
    void async_fetch_positions();
    void async_fetch_holdings();
    void async_fetch_orders();
    void async_fetch_funds();
    void async_fetch_watchlist_quotes();
    void async_fetch_orderbook();
    void async_fetch_calendar();
    void async_fetch_clock();
    void async_fetch_time_sales();   // historical trades for current symbol
    void async_fetch_latest_trade(); // single latest trade — appended to T&S on each poll
    void async_fetch_auctions();
    void async_fetch_condition_codes();
    void async_modify_order(const QString& order_id, double qty, double price);

    // ── Command bar widgets ──
    QPushButton* broker_btn_ = nullptr;
    QMenu* broker_menu_ = nullptr;
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* api_btn_ = nullptr;
    QLabel* exchange_label_ = nullptr;
    QLabel* clock_label_ = nullptr;
    QLabel* conn_label_ = nullptr; // connection status indicator

    // ── Sub-widgets ──
    equity::EquityTickerBar* ticker_bar_ = nullptr;
    equity::EquityWatchlist* watchlist_ = nullptr;
    equity::EquityChart* chart_ = nullptr;
    equity::EquityOrderEntry* order_entry_ = nullptr;
    equity::EquityOrderBook* orderbook_ = nullptr;
    equity::EquityBottomPanel* bottom_panel_ = nullptr;

    // ── WebSocket (Angel One only — null for other brokers) ──
    trading::AngelOneWebSocket* ws_ = nullptr;

    // ── Timers ──
    QTimer* quote_timer_ = nullptr;
    QTimer* portfolio_timer_ = nullptr;
    QTimer* watchlist_timer_ = nullptr;
    QTimer* clock_timer_ = nullptr;
    QTimer* market_clock_timer_ = nullptr;

    // ── State ──
    QString broker_id_ = "fyers";
    QString selected_symbol_ = "RELIANCE";
    QString selected_exchange_ = "NSE";
    equity::TradingMode trading_mode_ = equity::TradingMode::Paper;

    // Paper trading
    QString portfolio_id_;
    trading::PtPortfolio portfolio_;
    int fill_cb_id_ = -1; // OrderMatcher fill callback

    // Async guards
    std::atomic<bool> token_expired_shown_{false};
    std::atomic<bool> quote_fetching_{false};
    std::atomic<bool> candles_fetching_{false};
    std::atomic<bool> portfolio_fetching_{false};
    std::atomic<bool> time_sales_fetching_{false};

    QStringList watchlist_symbols_;
    double current_price_ = 0.0; // last known LTP for selected symbol

    QMutex data_mutex_;
    bool initialized_ = false;
};

} // namespace fincept::screens
