#pragma once
// Crypto Trading Screen — Bloomberg-style coordinator

#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"

#include <QHideEvent>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMutex>
#include <QPushButton>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <atomic>

namespace fincept::screens::crypto {
class CryptoTickerBar;
class CryptoWatchlist;
class CryptoChart;
class CryptoOrderEntry;
class CryptoOrderBook;
class CryptoBottomPanel;
} // namespace fincept::screens::crypto

namespace fincept::screens {

class CryptoTradingScreen : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoTradingScreen(QWidget* parent = nullptr);
    ~CryptoTradingScreen();

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_exchange_changed(const QString& exchange);
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();
    void on_api_clicked();
    void on_order_submitted(const QString& side, const QString& order_type, double qty, double price, double stop_price,
                            double sl, double tp);
    void on_cancel_order(const QString& order_id);
    void on_ob_price_clicked(double price);
    void on_search_requested(const QString& filter);

    void refresh_ticker();
    void refresh_orderbook();
    void refresh_portfolio();
    void refresh_watchlist();
    void refresh_market_info();
    void refresh_candles();
    void refresh_live_data();
    void update_clock();

  private:
    void setup_ui();
    void setup_timers();
    void init_exchange();
    void load_portfolio();
    void switch_symbol(const QString& symbol);

    void async_fetch_candles(const QString& symbol, const QString& timeframe);
    void async_fetch_live_positions();
    void async_fetch_live_orders();
    void async_fetch_live_balance();
    void async_fetch_my_trades();
    void async_fetch_trading_fees();
    void async_fetch_mark_price();
    void async_set_leverage(int leverage);
    void async_set_margin_mode(const QString& mode);

    // ── Command bar widgets ──
    QPushButton* exchange_btn_ = nullptr;
    QMenu* exchange_menu_ = nullptr;
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* api_btn_ = nullptr;
    QLabel* ws_status_ = nullptr;
    QLabel* clock_label_ = nullptr;

    // ── Sub-widgets ──
    crypto::CryptoTickerBar* ticker_bar_ = nullptr;
    crypto::CryptoWatchlist* watchlist_ = nullptr;
    crypto::CryptoChart* chart_ = nullptr;
    crypto::CryptoOrderEntry* order_entry_ = nullptr;
    crypto::CryptoOrderBook* orderbook_ = nullptr;
    crypto::CryptoBottomPanel* bottom_panel_ = nullptr;

    // ── Timers ──
    QTimer* ticker_timer_ = nullptr;
    QTimer* ob_timer_ = nullptr;
    QTimer* portfolio_timer_ = nullptr;
    QTimer* watchlist_timer_ = nullptr;
    QTimer* market_info_timer_ = nullptr;
    QTimer* live_data_timer_ = nullptr;
    QTimer* clock_timer_ = nullptr;

    // ── State ──
    QString exchange_id_ = "kraken";
    QString selected_symbol_ = "BTC/USDT";
    crypto::TradingMode trading_mode_ = crypto::TradingMode::Paper;

    // Paper trading
    QString portfolio_id_;
    trading::PtPortfolio portfolio_;

    // WS callbacks
    int ws_price_cb_id_ = -1;
    int ws_ob_cb_id_ = -1;
    int ws_candle_cb_id_ = -1;
    int ws_trade_cb_id_ = -1;

    // Async fetch guards
    std::atomic<bool> candles_fetching_{false};
    std::atomic<bool> live_fetching_{false};

    QStringList watchlist_symbols_ = {
        "BTC/USDT",  "ETH/USDT",  "SOL/USDT",  "BNB/USDT",
        "XRP/USDT",  "DOGE/USDT", "ADA/USDT",  "AVAX/USDT",
        "TON/USDT",  "LINK/USDT", "DOT/USDT",  "MATIC/USDT",
        "UNI/USDT",  "ATOM/USDT", "LTC/USDT",  "BCH/USDT",
        "APT/USDT",  "ARB/USDT",  "OP/USDT",   "SUI/USDT",
        "TRX/USDT",  "INJ/USDT",  "NEAR/USDT", "WIF/USDT",
        "PEPE/USDT",
    };

    QMutex data_mutex_;
    bool initialized_ = false;

    // ── WS update coalescing (10fps UI flush) ──
    QTimer* ws_flush_timer_ = nullptr;
    QHash<QString, trading::TickerData> pending_tickers_; // accumulated since last flush
    trading::TickerData pending_primary_ticker_;          // latest for selected symbol
    bool has_pending_primary_ = false;
    void flush_ws_updates();
};

} // namespace fincept::screens
