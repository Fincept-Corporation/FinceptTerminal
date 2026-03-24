#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens::polymarket {

class PolymarketOrderBook;
class PolymarketPriceChart;
class PolymarketActivityFeed;

/// Right detail panel: 7-tab stacked widget with embedded sub-widgets.
class PolymarketDetailPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketDetailPanel(QWidget* parent = nullptr);

    void set_market(const services::polymarket::Market& market);
    void set_price_summary(const services::polymarket::PriceSummary& summary);
    void set_order_book(const services::polymarket::OrderBook& book);
    void set_price_history(const services::polymarket::PriceHistory& history);
    void set_trades(const QVector<services::polymarket::Trade>& trades);
    void set_top_holders(const QVector<services::polymarket::TopHolder>& holders);
    void set_comments(const QVector<services::polymarket::Comment>& comments);
    void set_related_markets(const QVector<services::polymarket::Market>& markets);
    void set_open_interest(double oi);
    void clear();

  signals:
    void tab_changed(int index);
    void interval_changed(const QString& interval);
    void outcome_changed(int index);
    void related_market_clicked(const services::polymarket::Market& market);

  private:
    void build_ui();
    QWidget* create_overview_page();
    QWidget* create_holders_page();
    QWidget* create_comments_page();
    QWidget* create_related_page();

    void set_active_tab(int tab);

    QList<QPushButton*> tab_btns_;
    QStackedWidget* stack_ = nullptr;

    // Overview
    QLabel* question_label_ = nullptr;
    QLabel* volume_label_ = nullptr;
    QLabel* liquidity_label_ = nullptr;
    QLabel* end_date_label_ = nullptr;
    QLabel* midpoint_label_ = nullptr;
    QLabel* spread_label_ = nullptr;
    QLabel* last_trade_label_ = nullptr;
    QLabel* oi_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QWidget* outcome_container_ = nullptr;
    QLabel* description_label_ = nullptr;

    // Embedded sub-widgets
    PolymarketOrderBook* orderbook_ = nullptr;
    PolymarketPriceChart* price_chart_ = nullptr;
    PolymarketActivityFeed* activity_feed_ = nullptr;

    // Holders
    QTableWidget* holders_table_ = nullptr;

    // Comments
    QWidget* comments_container_ = nullptr;

    // Related
    QWidget* related_container_ = nullptr;
};

} // namespace fincept::screens::polymarket
