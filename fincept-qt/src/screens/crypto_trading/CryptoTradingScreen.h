#pragma once
// Crypto Trading Screen — coordinator that composes modular sub-widgets

#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"
#include <QWidget>
#include <QTimer>
#include <QMutex>
#include <QComboBox>
#include <QPushButton>
#include <QStringList>
#include <QJsonArray>
#include <QShowEvent>
#include <QHideEvent>
#include <atomic>

namespace fincept::screens::crypto {
    class CryptoTickerBar;
    class CryptoWatchlist;
    class CryptoChart;
    class CryptoOrderEntry;
    class CryptoOrderBook;
    class CryptoBottomPanel;
}

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
    void on_exchange_changed(int index);
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();
    void on_api_clicked();
    void on_order_submitted(const QString& side, const QString& order_type,
                             double qty, double price, double stop_price,
                             double sl, double tp);
    void on_cancel_order(const QString& order_id);
    void on_ob_price_clicked(double price);
    void on_search_requested(const QString& filter);

    // Timer-driven refreshes
    void refresh_ticker();
    void refresh_orderbook();
    void refresh_portfolio();
    void refresh_watchlist();
    void refresh_market_info();
    void refresh_candles();

    // Live data refresh
    void refresh_live_data();

private:
    void setup_ui();
    void setup_timers();
    void init_exchange();
    void load_portfolio();
    void switch_symbol(const QString& symbol);

    // Async helpers (run in thread, post result back to UI)
    void async_fetch_candles(const QString& symbol, const QString& timeframe);
    void async_fetch_live_positions();
    void async_fetch_live_orders();
    void async_fetch_live_balance();

    // Sub-widgets
    QComboBox* exchange_combo_ = nullptr;
    QComboBox* symbol_combo_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* api_btn_ = nullptr;

    crypto::CryptoTickerBar*   ticker_bar_ = nullptr;
    crypto::CryptoWatchlist*   watchlist_ = nullptr;
    crypto::CryptoChart*       chart_ = nullptr;
    crypto::CryptoOrderEntry*  order_entry_ = nullptr;
    crypto::CryptoOrderBook*   orderbook_ = nullptr;
    crypto::CryptoBottomPanel* bottom_panel_ = nullptr;

    // Timers
    QTimer* ticker_timer_ = nullptr;
    QTimer* ob_timer_ = nullptr;
    QTimer* portfolio_timer_ = nullptr;
    QTimer* watchlist_timer_ = nullptr;
    QTimer* market_info_timer_ = nullptr;
    QTimer* live_data_timer_ = nullptr;

    // State
    QString exchange_id_ = "binance";
    QString selected_symbol_ = "BTC/USDT";
    crypto::TradingMode trading_mode_ = crypto::TradingMode::Paper;

    // Paper trading
    QString portfolio_id_;
    trading::PtPortfolio portfolio_;

    // WS callbacks
    int ws_price_cb_id_ = -1;
    int ws_candle_cb_id_ = -1;

    // Async fetch guards
    std::atomic<bool> candles_fetching_{false};
    std::atomic<bool> live_fetching_{false};

    // Default watchlist
    QStringList watchlist_symbols_ = {"BTC/USDT", "ETH/USDT", "SOL/USDT",
                                       "BNB/USDT", "XRP/USDT", "ADA/USDT",
                                       "DOGE/USDT", "AVAX/USDT"};

    QMutex data_mutex_;
    bool initialized_ = false;
};

} // namespace fincept::screens
