#pragma once
// Equity Bottom Panel — tabbed: Positions, Holdings, Orders, Funds, Stats

#include "screens/equity_trading/EquityTypes.h"
#include "trading/TradingTypes.h"
#include "trading/BrokerInterface.h"

#include <QLabel>
#include <QMap>
#include <QTabWidget>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

namespace fincept::screens::equity {

class EquityBottomPanel : public QWidget {
    Q_OBJECT
  public:
    explicit EquityBottomPanel(QWidget* parent = nullptr);

    // Paper mode data
    void set_paper_positions(const QVector<trading::PtPosition>& positions);
    void set_paper_orders(const QVector<trading::PtOrder>& orders);
    void set_paper_trades(const QVector<trading::PtTrade>& trades);
    void set_paper_stats(const trading::PtStats& stats);

    // Live broker data
    void set_positions(const QVector<trading::BrokerPosition>& positions);
    void set_holdings(const QVector<trading::BrokerHolding>& holdings);
    void set_orders(const QVector<trading::BrokerOrderInfo>& orders);
    void set_funds(const trading::BrokerFunds& funds);
    void set_calendar(const QVector<trading::MarketCalendarDay>& days);
    void set_clock(const trading::MarketClock& clock);
    void set_auctions(const QVector<trading::BrokerAuction>& auctions);
    void set_time_sales(const QVector<trading::BrokerTrade>& trades);  // bulk load
    void prepend_trade(const trading::BrokerTrade& trade);             // live append at top
    void set_condition_codes(const QMap<QString, QString>& codes);

    void set_mode(bool is_paper);

  signals:
    void cancel_order_requested(const QString& order_id);
    void modify_order_requested(const QString& order_id, double new_qty, double new_price);

  private:
    void setup_positions_tab();
    void setup_holdings_tab();
    void setup_orders_tab();
    void setup_funds_tab();
    void setup_stats_tab();
    void setup_calendar_tab();
    void setup_time_sales_tab();
    void setup_auctions_tab();

    static QTableWidgetItem* ensure_item(QTableWidget* table, int row, int col);

    QTabWidget* tabs_ = nullptr;

    QTableWidget* positions_table_ = nullptr;
    QTableWidget* holdings_table_ = nullptr;
    QTableWidget* orders_table_ = nullptr;
    QTableWidget* calendar_table_    = nullptr;
    QTableWidget* time_sales_table_  = nullptr;
    QString       time_sales_symbol_;          // track last symbol to detect change
    QMap<QString, QString> condition_codes_; // code → description cache
    QTableWidget* auctions_table_ = nullptr;

    // Clock banner labels
    QLabel* clock_status_label_    = nullptr;
    QLabel* clock_next_label_      = nullptr;

    // Funds labels
    QLabel* available_label_ = nullptr;
    QLabel* used_margin_label_ = nullptr;
    QLabel* total_label_ = nullptr;
    QLabel* collateral_label_ = nullptr;

    // Stats labels
    QLabel* stat_values_[5] = {};

    bool is_paper_ = true;
};

} // namespace fincept::screens::equity
