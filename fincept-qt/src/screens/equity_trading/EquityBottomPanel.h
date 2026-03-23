#pragma once
// Equity Bottom Panel — tabbed: Positions, Holdings, Orders, Funds, Stats

#include "screens/equity_trading/EquityTypes.h"
#include "trading/TradingTypes.h"

#include <QLabel>
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

    void set_mode(bool is_paper);

  signals:
    void cancel_order_requested(const QString& order_id);

  private:
    void setup_positions_tab();
    void setup_holdings_tab();
    void setup_orders_tab();
    void setup_funds_tab();
    void setup_stats_tab();

    static QTableWidgetItem* ensure_item(QTableWidget* table, int row, int col);

    QTabWidget* tabs_ = nullptr;

    QTableWidget* positions_table_ = nullptr;
    QTableWidget* holdings_table_ = nullptr;
    QTableWidget* orders_table_ = nullptr;

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
