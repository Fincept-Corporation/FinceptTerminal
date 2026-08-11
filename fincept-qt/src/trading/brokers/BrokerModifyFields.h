#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <span>

/// Canonical readers for the untyped `modify_order(mods)` map.
///
/// The map is read under **32 distinct key names** across the 22 broker
/// adapters (`trigger_price` ×7 vs `triggerPrice` ×6 vs `auxPrice` vs
/// `stopPrice` vs `stop` vs `stoploss`; `orderType` ×9 vs `order_type` ×7 vs
/// `type`), while callers populate only a couple of spellings. Every mismatch
/// silently reads as absent and transmits the type's default — which for a
/// price field means the broker receives **0**.
///
/// Live consequences this header exists to stop:
///   * IIFL read `limitPrice` while callers send `price`, so a price modify
///     transmitted `modifiedLimitPrice = 0`.
///   * Shoonya, Flattrade and Tradejini read `triggerPrice` while callers send
///     `trigger_price` (or nothing), so an SL modify transmitted `trgprc = 0`.
///
/// The real fix is a typed `OrderModification` struct; until that lands, read
/// through these helpers so a broker accepts every spelling of a concept and a
/// missing value is distinguishable from a zero one.
namespace fincept::trading::modify_fields {

using KeySet = std::span<const char* const>;

/// First key that is actually present, else an undefined value.
/// Presence — not truthiness — decides: a legitimate 0 must win over a fallback.
inline QJsonValue first_present(const QJsonObject& mods, KeySet keys) {
    for (const char* k : keys) {
        const auto it = mods.constFind(QLatin1String(k));
        if (it != mods.constEnd() && !it->isUndefined() && !it->isNull())
            return *it;
    }
    return QJsonValue(QJsonValue::Undefined);
}

inline bool has_any(const QJsonObject& mods, KeySet keys) {
    return !first_present(mods, keys).isUndefined();
}

/// Numeric read tolerant of JSON numbers *and* numeric strings. Zerodha's
/// adapter used QJsonValue::toString() on numeric values, which returns an
/// empty string — hence `toVariant()` rather than a typed accessor.
inline double number(const QJsonObject& mods, KeySet keys, double fallback = 0.0) {
    const QJsonValue v = first_present(mods, keys);
    if (v.isUndefined())
        return fallback;
    bool ok = false;
    const double d = v.toVariant().toDouble(&ok);
    return ok ? d : fallback;
}

inline QString text(const QJsonObject& mods, KeySet keys, const QString& fallback = {}) {
    const QJsonValue v = first_present(mods, keys);
    if (v.isUndefined())
        return fallback;
    const QString s = v.toVariant().toString();
    return s.isEmpty() ? fallback : s;
}

// Concept key sets. Canonical caller spelling first, adapter spellings after.
// Plain arrays, not std::initializer_list: a namespace-scope constexpr
// initializer_list has a backing array whose lifetime rules are subtle, and
// span-over-array is unambiguously safe with static storage duration.
inline constexpr const char* kPrice[] = {"price", "limitPrice", "limit_price"};
inline constexpr const char* kTrigger[] = {"trigger_price", "triggerPrice", "trigPrice", "stopPrice",
                                           "stop",          "stoploss",     "auxPrice"};
inline constexpr const char* kQuantity[] = {"quantity", "qty"};
inline constexpr const char* kOrderType[] = {"order_type", "orderType", "type"};
inline constexpr const char* kProduct[] = {"product", "productType", "product_type"};

} // namespace fincept::trading::modify_fields
