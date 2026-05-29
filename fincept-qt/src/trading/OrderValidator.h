#pragma once
// OrderValidator — pre-flight validation for orders before they reach a broker.
// Mirrors OpenAlgo's restx_api/schemas.py checks: required fields, valid exchange,
// positive quantity, price required for LIMIT, trigger required for SL/SL-M.

#include "trading/TradingTypes.h"

#include <QStringList>
#include <QSet>

namespace fincept::trading {

class OrderValidator {
  public:
    struct ValidationResult {
        bool valid = true;
        QStringList errors;
    };

    static ValidationResult validate(const UnifiedOrder& order);
    static ValidationResult validate_smart(const SmartOrder& order);
    static ValidationResult validate_basket(const BasketOrderRequest& basket);

    static bool is_valid_exchange(const QString& exchange);

  private:
    static bool requires_price(OrderType type);
    static bool requires_trigger_price(OrderType type);
    static const QSet<QString>& valid_exchanges();
};

} // namespace fincept::trading
