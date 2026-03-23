#pragma once
// Equity Trading Screen — Bloomberg-style coordinator
// Routes to live brokers (16 supported) or paper trading engine.

#include "screens/equity_trading/EquityTypes.h"
#include "trading/TradingTypes.h"

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

namespace fincept::screens::equity {
class EquityTickerBar;
class EquityWatchlist;
class EquityChart;
class EquityOrderEntry;
class EquityOrderBook;
class EquityBottomPanel;
} // namespace fincept::screens::equity

namespace fincept::screens {

class EquityTradingScreen : public QWidget {
    Q_OBJECT
  public:
    explicit EquityTradingScreen(QWidget* parent = nullptr);
    ~EquityTradingScreen();

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_broker_changed(const QString& broker);
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();
    void on_api_clicked();
    void on_order_submitted(const trading::UnifiedOrder& order);
    void on_cancel_order(const QString& order_id);
    void on_ob_price_clicked(double price);

    void refresh_quote();
    void refresh_portfolio();
    void refresh_watchlist();
    void refresh_candles();
    void update_clock();

  private:
    void setup_ui();
    void setup_timers();
    void init_broker();
    void load_portfolio();
    void switch_symbol(const QString& symbol);

    void async_fetch_quote();
    void async_fetch_candles(const QString& symbol, const QString& timeframe);
    void async_fetch_positions();
    void async_fetch_holdings();
    void async_fetch_orders();
    void async_fetch_funds();
    void async_fetch_watchlist_quotes();

    // ── Command bar widgets ──
    QPushButton* broker_btn_ = nullptr;
    QMenu* broker_menu_ = nullptr;
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* api_btn_ = nullptr;
    QLabel* exchange_label_ = nullptr;
    QLabel* clock_label_ = nullptr;

    // ── Sub-widgets ──
    equity::EquityTickerBar* ticker_bar_ = nullptr;
    equity::EquityWatchlist* watchlist_ = nullptr;
    equity::EquityChart* chart_ = nullptr;
    equity::EquityOrderEntry* order_entry_ = nullptr;
    equity::EquityOrderBook* orderbook_ = nullptr;
    equity::EquityBottomPanel* bottom_panel_ = nullptr;

    // ── Timers ──
    QTimer* quote_timer_ = nullptr;
    QTimer* portfolio_timer_ = nullptr;
    QTimer* watchlist_timer_ = nullptr;
    QTimer* clock_timer_ = nullptr;

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
    std::atomic<bool> quote_fetching_{false};
    std::atomic<bool> candles_fetching_{false};
    std::atomic<bool> portfolio_fetching_{false};

    QStringList watchlist_symbols_;
    double current_price_ = 0.0; // last known LTP for selected symbol

    QMutex data_mutex_;
    bool initialized_ = false;
};

} // namespace fincept::screens
