#pragma once
// Crypto Bottom Panel — tabbed: Positions, Orders, Trades, Market Info, Stats, Time&Sales, Depth

#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"

#include <QJsonArray>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

namespace fincept::screens::crypto {

class CryptoTimeSales;
class CryptoDepthChart;

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

    // Live mode data
    void set_live_positions(const QJsonArray& positions);
    void set_live_orders(const QJsonArray& orders);
    void set_live_balance(double balance, double equity, double used_margin);
    void set_mode(bool is_paper);

    // New widget forwarding
    void add_trade_entry(const TradeEntry& trade);
    void set_depth_data(const QVector<QPair<double, double>>& bids,
                        const QVector<QPair<double, double>>& asks,
                        double spread, double spread_pct);

  signals:
    void cancel_order_requested(const QString& order_id);
    void close_position_requested(const QString& symbol);

  private:
    void setup_positions_tab();
    void setup_orders_tab();
    void setup_trades_tab();
    void setup_market_info_tab();
    void setup_stats_tab();

    static QTableWidgetItem* ensure_item(QTableWidget* table, int row, int col);

    QTabWidget* tabs_ = nullptr;

    QTableWidget* positions_table_ = nullptr;
    QTableWidget* orders_table_ = nullptr;
    QTableWidget* trades_table_ = nullptr;

    // Market Info
    QLabel* funding_label_ = nullptr;
    QLabel* mark_label_ = nullptr;
    QLabel* index_label_ = nullptr;
    QLabel* oi_label_ = nullptr;
    QLabel* fees_label_ = nullptr;
    QLabel* next_funding_label_ = nullptr;

    // Stats (data grid)
    QLabel* stat_values_[5] = {};

    // Live balance
    QLabel* live_balance_label_ = nullptr;
    QLabel* live_equity_label_ = nullptr;
    QLabel* live_margin_label_ = nullptr;

    // New widgets
    CryptoTimeSales* time_sales_ = nullptr;
    CryptoDepthChart* depth_chart_ = nullptr;

    bool is_paper_ = true;
};

} // namespace fincept::screens::crypto
