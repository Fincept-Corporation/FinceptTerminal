#pragma once
// Order Matcher — Touch-based order matching for paper trading
// Qt-adapted port from order_matcher.h/.cpp

#include "trading/TradingTypes.h"

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QVector>

#include <functional>

namespace fincept::trading {

struct OrderFillEvent {
    QString order_id;
    QString symbol;
    QString side;
    QString order_type;
    double fill_price;
    double quantity;
    int64_t timestamp;
};

using OrderFillCallback = std::function<void(const OrderFillEvent&)>;

class OrderMatcher {
  public:
    static OrderMatcher& instance();

    void add_order(const PtOrder& order);
    void remove_order(const QString& order_id);
    QVector<PtOrder> get_pending_orders(const QString& symbol = "", const QString& portfolio_id = "");

    void check_orders(const QString& symbol, const PriceData& price, const QString& portfolio_id);

    void load_orders(const QVector<PtOrder>& orders);
    void clear_portfolio_orders(const QString& portfolio_id);
    void clear_all();

    int on_order_fill(OrderFillCallback callback);
    void remove_fill_callback(int id);
    int pending_order_count() const;

    // SL/TP trigger engine
    void set_sl_tp(const QString& portfolio_id, const QString& symbol, const QString& order_id, double sl_price,
                   double tp_price);

    void check_sl_tp_triggers(const QString& portfolio_id, const QString& symbol, double current_price);

    OrderMatcher(const OrderMatcher&) = delete;
    OrderMatcher& operator=(const OrderMatcher&) = delete;

  private:
    OrderMatcher() = default;

    QHash<QString, QVector<PtOrder>> pending_orders_;
    mutable QSet<QString> triggered_stops_;
    QHash<int, OrderFillCallback> fill_callbacks_;
    int next_callback_id_ = 1;
    mutable QMutex mutex_;

    struct SLTPTrigger {
        QString portfolio_id;
        QString symbol;
        QString order_id;
        QString position_side;
        double sl_price = 0.0;
        double tp_price = 0.0;
        bool triggered = false;
    };
    QVector<SLTPTrigger> sl_tp_triggers_;
    QMutex sl_tp_mutex_;

    QString scoped_key(const QString& portfolio_id, const QString& symbol) const;
    bool should_fill(const PtOrder& order, const PriceData& price) const;
    bool check_limit(const PtOrder& order, const PriceData& price) const;
    bool check_stop(const PtOrder& order, const PriceData& price) const;
    bool check_stop_limit(const PtOrder& order, const PriceData& price) const;
    double get_fill_price(const PtOrder& order, const PriceData& price) const;
};

} // namespace fincept::trading
