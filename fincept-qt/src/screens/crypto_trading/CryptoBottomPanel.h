#pragma once
// Crypto Bottom Panel — tabbed: Positions, Orders, Trades, My Trades, Fees, Market Info, Stats, Time&Sales, Depth

#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"

#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QStackedWidget>
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
    /// Live balance could not be fetched (bad API key / daemon error). Shows an
    /// explicit UNAVAILABLE state instead of a misleading $0.00 (which is
    /// indistinguishable from a genuinely empty account). `reason` goes into a
    /// tooltip.
    void set_balance_unavailable(const QString& reason);
    void set_mode(bool is_paper);

    // New live tabs
    void update_my_trades(const QJsonObject& data);
    void update_fees(const QJsonObject& data);

    // New widget forwarding
    void add_trade_entry(const TradeEntry& trade);
    void set_depth_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                        double spread, double spread_pct);

    /// Patch the positions table with fresh WS ticker prices. Works in both
    /// paper and live mode — recalculates current_price, unrealized PnL, and
    /// PnL% from the latest mark price without hitting SQLite or the daemon.
    /// Call at every WS flush tick.
    void update_position_prices(const QHash<QString, double>& last_prices);

    void set_account_id(const QString& account_id);

  signals:
    void cancel_order_requested(const QString& order_id);
    void close_position_requested(const QString& symbol);
    void cancel_all_orders_requested(const QString& account_id);
    void close_all_positions_requested(const QString& account_id);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();
    void setup_positions_tab();
    void setup_orders_tab();
    void setup_trades_tab();
    void setup_my_trades_tab();
    void setup_fees_tab();
    void setup_market_info_tab();
    void setup_stats_tab();

    static QTableWidgetItem* ensure_item(QTableWidget* table, int row, int col);
    void update_empty_state(QTableWidget* table, QStackedWidget* stack, int row_count);

    QTabWidget* tabs_ = nullptr;

    QTableWidget* positions_table_ = nullptr;
    QTableWidget* orders_table_ = nullptr;
    QTableWidget* trades_table_ = nullptr;
    QTableWidget* my_trades_table_ = nullptr;
    QTableWidget* fees_table_ = nullptr;

    // Each table is wrapped in a QStackedWidget so we can swap between the
    // table (page 0) and an empty-state placeholder (page 1) when the data
    // set is empty. Avoids the "blank table with header only" UX.
    QStackedWidget* positions_stack_ = nullptr;
    QStackedWidget* orders_stack_ = nullptr;
    QStackedWidget* trades_stack_ = nullptr;
    QStackedWidget* my_trades_stack_ = nullptr;
    QStackedWidget* fees_stack_ = nullptr;

    // Empty-state placeholder labels (cached for retranslateUi)
    QLabel* positions_empty_label_ = nullptr;
    QLabel* orders_empty_label_ = nullptr;
    QLabel* trades_empty_label_ = nullptr;
    QLabel* my_trades_empty_label_ = nullptr;
    QLabel* fees_empty_label_ = nullptr;

    // Tab indices for the host containers (cached for retranslateUi tab text)
    int positions_tab_idx_ = -1;
    int orders_tab_idx_ = -1;
    int trades_tab_idx_ = -1;
    int my_trades_tab_idx_ = -1;
    int fees_tab_idx_ = -1;
    int time_sales_tab_idx_ = -1;
    int depth_tab_idx_ = -1;
    int market_tab_idx_ = -1;
    int stats_tab_idx_ = -1;

    // Market Info
    QLabel* funding_label_ = nullptr;
    QLabel* mark_label_ = nullptr;
    QLabel* index_label_ = nullptr;
    QLabel* oi_label_ = nullptr;
    QLabel* fees_label_ = nullptr;
    QLabel* next_funding_label_ = nullptr;

    // Market Info card title labels (cached for retranslateUi)
    QLabel* funding_title_ = nullptr;
    QLabel* mark_title_ = nullptr;
    QLabel* index_title_ = nullptr;
    QLabel* oi_title_ = nullptr;
    QLabel* fees_title_ = nullptr;
    QLabel* next_funding_title_ = nullptr;

    // Stats (data grid)
    QLabel* stat_values_[5] = {};
    QLabel* stat_titles_[5] = {};

    // Live balance
    QLabel* live_balance_label_ = nullptr;
    QLabel* live_equity_label_ = nullptr;
    QLabel* live_margin_label_ = nullptr;

    // Bulk action buttons
    class QPushButton* cancel_all_btn_ = nullptr;
    class QPushButton* close_all_btn_ = nullptr;

    // New widgets
    CryptoTimeSales* time_sales_ = nullptr;
    CryptoDepthChart* depth_chart_ = nullptr;

    QString account_id_;
    bool is_paper_ = true;
};

} // namespace fincept::screens::crypto
