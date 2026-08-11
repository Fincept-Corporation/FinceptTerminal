#include "trading/OrderValidator.h"

namespace fincept::trading {

const QSet<QString>& OrderValidator::valid_exchanges() {
    static const QSet<QString> kExchanges = {
        // India
        "NSE", "BSE", "NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX", "NSE_INDEX", "BSE_INDEX", "MCX_INDEX",
        // US / global
        "NYSE", "NASDAQ", "AMEX", "ARCA", "BATS", "CBOE", "LSE", "TSX", "XETRA", "EURONEXT", "XNYS", "XNAS",
        // crypto / forex
        "CRYPTO", "FOREX"};
    return kExchanges;
}

bool OrderValidator::is_valid_exchange(const QString& exchange) {
    return valid_exchanges().contains(exchange.toUpper());
}

bool OrderValidator::requires_price(OrderType type) {
    return type == OrderType::Limit || type == OrderType::StopLossLimit;
}

bool OrderValidator::requires_trigger_price(OrderType type) {
    return type == OrderType::StopLoss || type == OrderType::StopLossLimit;
}

OrderValidator::ValidationResult OrderValidator::validate(const UnifiedOrder& order) {
    ValidationResult r;

    if (order.symbol.trimmed().isEmpty())
        r.errors.append("Symbol is required");
    if (order.exchange.trimmed().isEmpty())
        r.errors.append("Exchange is required");
    else if (!is_valid_exchange(order.exchange))
        r.errors.append("Invalid exchange: " + order.exchange);

    if (order.quantity <= 0)
        r.errors.append("Quantity must be positive");

    if (requires_price(order.order_type) && order.price <= 0)
        r.errors.append("Price is required for LIMIT / SL-Limit orders");

    if (requires_trigger_price(order.order_type) && order.stop_price <= 0)
        r.errors.append("Trigger price is required for SL / SL-M orders");

    r.valid = r.errors.isEmpty();
    return r;
}

OrderValidator::ValidationResult OrderValidator::validate_smart(const SmartOrder& order) {
    ValidationResult r;

    if (order.symbol.trimmed().isEmpty())
        r.errors.append("Symbol is required");
    if (order.exchange.trimmed().isEmpty())
        r.errors.append("Exchange is required");
    else if (!is_valid_exchange(order.exchange))
        r.errors.append("Invalid exchange: " + order.exchange);

    // position_size is the TARGET net position, and 0 is a legitimate target —
    // "flatten": the engine reads the live book and derives the quantity itself.
    // An explicit 0 is indistinguishable from an unset one, so requiring a
    // fallback quantity here would reject every flatten order. The engine already
    // reports the genuinely-empty case (no position AND no quantity) as a benign
    // "no action needed", so only a nonsensical negative fallback is an error.
    if (order.quantity < 0)
        r.errors.append("Quantity cannot be negative");

    if (requires_price(order.order_type) && order.price <= 0)
        r.errors.append("Price is required for LIMIT / SL-Limit orders");

    if (requires_trigger_price(order.order_type) && order.trigger_price <= 0)
        r.errors.append("Trigger price is required for SL / SL-M orders");

    r.valid = r.errors.isEmpty();
    return r;
}

OrderValidator::ValidationResult OrderValidator::validate_basket(const BasketOrderRequest& basket) {
    ValidationResult r;

    if (basket.orders.isEmpty()) {
        r.errors.append("Basket has no orders");
        r.valid = false;
        return r;
    }

    for (int i = 0; i < basket.orders.size(); ++i) {
        auto leg = validate(basket.orders[i]);
        if (!leg.valid) {
            for (const auto& e : leg.errors)
                r.errors.append(QString("Order %1 (%2): %3").arg(i + 1).arg(basket.orders[i].symbol, e));
        }
    }

    r.valid = r.errors.isEmpty();
    return r;
}

} // namespace fincept::trading
