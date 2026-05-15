#pragma once
// Phase 4.0 scaffolding — typed enum mapping shared across broker adapters.
//
// Every broker historically reimplemented three switch statements:
//   <broker>_order_type(OrderType)   → "MARKET" / 2 / …
//   <broker>_side(OrderSide)         → "BUY"    / 1 / …
//   <broker>_product(ProductType)    → "MIS"    / "INTRADAY" / …
//
// They differ in the wire type (QString vs int) and the values, but never in
// shape. `BrokerEnumMap<T>` makes the table the data and removes the switch.

#include "trading/TradingTypes.h"

#include <QHash>

#include <optional>

namespace fincept::trading {

/// Typed enum→wire-value mapping. `T` is the broker's wire type
/// (typically `QString` or `int`).
///
/// Usage:
/// ```
/// static const BrokerEnumMap<QString>& maps() {
///     static const auto m = []{
///         BrokerEnumMap<QString> x;
///         x.set(OrderType::Market, "MARKET");
///         x.set(OrderType::Limit,  "LIMIT");
///         x.set(OrderSide::Buy,    "BUY");
///         x.set(ProductType::Intraday, "MIS");
///         return x;
///     }();
///     return m;
/// }
///
/// const QString s = maps().for_order_type(order.order_type).value_or("MARKET");
/// ```
///
/// Lookup is O(1) on QHash. Tables are immutable after population — typically
/// built once in a `static const auto m = []{...}()` initializer.
template <typename T>
class BrokerEnumMap {
  public:
    BrokerEnumMap& set(OrderType k, T v) {
        order_type_.insert(static_cast<int>(k), std::move(v));
        return *this;
    }
    BrokerEnumMap& set(OrderSide k, T v) {
        side_.insert(static_cast<int>(k), std::move(v));
        return *this;
    }
    BrokerEnumMap& set(ProductType k, T v) {
        product_.insert(static_cast<int>(k), std::move(v));
        return *this;
    }

    std::optional<T> for_order_type(OrderType k) const { return lookup(order_type_, static_cast<int>(k)); }
    std::optional<T> for_side(OrderSide k) const { return lookup(side_, static_cast<int>(k)); }
    std::optional<T> for_product(ProductType k) const { return lookup(product_, static_cast<int>(k)); }

    /// Convenience: return value or fallback.
    T order_type_or(OrderType k, T fallback) const { return for_order_type(k).value_or(std::move(fallback)); }
    T side_or(OrderSide k, T fallback) const { return for_side(k).value_or(std::move(fallback)); }
    T product_or(ProductType k, T fallback) const { return for_product(k).value_or(std::move(fallback)); }

  private:
    static std::optional<T> lookup(const QHash<int, T>& m, int k) {
        auto it = m.find(k);
        if (it == m.end())
            return std::nullopt;
        return it.value();
    }

    QHash<int, T> order_type_;
    QHash<int, T> side_;
    QHash<int, T> product_;
};

} // namespace fincept::trading
