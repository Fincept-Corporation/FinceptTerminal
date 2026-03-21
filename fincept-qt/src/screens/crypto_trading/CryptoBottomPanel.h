#pragma once
// Crypto Bottom Panel — tabbed view: Positions, Orders, Trades, MarketInfo, Stats

#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"
#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>
#include <QVector>
#include <QJsonArray>

namespace fincept::screens::crypto {

class CryptoBottomPanel : public QWidget {
    Q_OBJECT
public:
    explicit CryptoBottomPanel(QWidget* parent = nullptr);

    // Paper mode data
    void set_positions(const QVector<trading::PtPosition>& positions);
    void set_orders(const QVector<trading::PtOrder>& orders);
    void set_trades(const QVector<trading::PtTrade>& trades);
    void set_stats(const trading::PtStats& stats);
    void set_market_info(const MarketInfoData& info);

    // Live mode data (from exchange API)
    void set_live_positions(const QJsonArray& positions);
    void set_live_orders(const QJsonArray& orders);
    void set_live_balance(double balance, double equity, double used_margin);
    void set_mode(bool is_paper);

signals:
    void cancel_order_requested(const QString& order_id);
    void close_position_requested(const QString& symbol);

private:
    void setup_positions_tab();
    void setup_orders_tab();
    void setup_trades_tab();
    void setup_market_info_tab();
    void setup_stats_tab();

    QTabWidget* tabs_ = nullptr;

    // Positions
    QTableWidget* positions_table_ = nullptr;

    // Orders
    QTableWidget* orders_table_ = nullptr;

    // Trades
    QTableWidget* trades_table_ = nullptr;

    // Market Info
    QLabel* funding_label_ = nullptr;
    QLabel* mark_label_ = nullptr;
    QLabel* index_label_ = nullptr;
    QLabel* oi_label_ = nullptr;
    QLabel* fees_label_ = nullptr;
    QLabel* next_funding_label_ = nullptr;

    // Stats
    QLabel* pnl_label_ = nullptr;
    QLabel* winrate_label_ = nullptr;
    QLabel* trades_count_label_ = nullptr;
    QLabel* best_trade_label_ = nullptr;
    QLabel* worst_trade_label_ = nullptr;

    // Live balance display
    QLabel* live_balance_label_ = nullptr;
    QLabel* live_equity_label_ = nullptr;
    QLabel* live_margin_label_ = nullptr;

    bool is_paper_ = true;
};

} // namespace fincept::screens::crypto
