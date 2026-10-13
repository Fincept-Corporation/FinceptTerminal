#pragma once
// TradingBridgeLogic.h — pure order-sizing / pricing rules used by the workflow
// trading bridges (ServiceBridges.cpp). Kept free of broker/session types so the
// rules can be exercised in isolation.

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace fincept::workflow::bridge_logic {

/// Direction of an open position: +1 long, -1 short, 0 flat. Brokers disagree on
/// the convention — some sign the quantity, others report an absolute quantity and
/// put the direction in `side` ("LONG"/"SHORT", "buy"/"sell", "long"/"short").
inline int position_direction(double quantity, const QString& side) {
    if (quantity == 0.0 || !std::isfinite(quantity))
        return 0;
    if (quantity < 0)
        return -1;
    const QString s = side.trimmed().toLower();
    if (s == QLatin1String("short") || s == QLatin1String("sell") || s == QLatin1String("s"))
        return -1;
    return 1;
}

/// Comparable form of a tradingsymbol: upper case, without an "EXCHANGE:" prefix or
/// an NSE series suffix ("-EQ", "-BE"). "NSE:RELIANCE-EQ" and "reliance" match;
/// "RELIANCE" and "RELIANCEPP" do not (no substring matching).
inline QString normalize_symbol(const QString& raw) {
    QString s = raw.trimmed().toUpper();
    const int colon = s.indexOf(QLatin1Char(':'));
    if (colon >= 0)
        s = s.mid(colon + 1);
    if (s.endsWith(QLatin1String("-EQ")) || s.endsWith(QLatin1String("-BE")))
        s.chop(3);
    return s;
}

inline bool symbols_match(const QString& a, const QString& b) {
    const QString na = normalize_symbol(a);
    return !na.isEmpty() && na == normalize_symbol(b);
}

/// Quantity to close / protect: the requested quantity capped at the position size
/// (a close must never flip the position), or the whole position when none was
/// requested (<= 0). Returns -1 when there is no position.
inline double close_quantity(double requested, double position_abs) {
    if (!(position_abs > 0))
        return -1;
    if (!(requested > 0))
        return position_abs;
    return std::min(requested, position_abs);
}

/// Round a price to the exchange tick: 0.05 on Indian cash/derivative venues,
/// 0.01 elsewhere. Rounded AWAY from the market (down for a sell stop, up for a buy
/// stop) so the stop is never tighter than requested.
inline double round_to_tick(double price, const QString& exchange, int direction) {
    const QString ex = exchange.toUpper();
    const double tick = (ex == QLatin1String("NSE") || ex == QLatin1String("BSE") || ex == QLatin1String("NFO") ||
                         ex == QLatin1String("BFO"))
                            ? 0.05
                            : 0.01;
    const double steps = price / tick;
    const double r = direction > 0 ? std::floor(steps + 1e-9) : std::ceil(steps - 1e-9);
    return std::round(r * tick * 100.0) / 100.0;
}

/// Protective stop price for a position, `trail_value` away from the last price —
/// in percent (trail_type "percent") or price points (anything else). Below the
/// price for a long (direction +1), above it for a short. Returns -1 when the
/// inputs cannot produce a valid stop (no price, no position, zero/negative trail,
/// a percent trail of 100% or more).
inline double trailing_stop_price(double ltp, int direction, const QString& trail_type, double trail_value) {
    if (!(ltp > 0) || direction == 0 || !(trail_value > 0))
        return -1;
    const bool percent = (trail_type == QLatin1String("percent"));
    if (percent && trail_value >= 100.0)
        return -1;
    const double distance = percent ? ltp * trail_value / 100.0 : trail_value;
    const double stop = direction > 0 ? ltp - distance : ltp + distance;
    return stop > 0 ? stop : -1;
}

/// Reference price carried by the node's upstream data (a quote, a risk-check
/// payload...): the first positive price / ltp / last / close of the first object
/// input. Returns 0 when there is none.
inline double reference_price(const QVector<QJsonValue>& inputs) {
    for (const QJsonValue& in : inputs) {
        if (!in.isObject())
            continue;
        const QJsonObject o = in.toObject();
        for (const char* key : {"price", "ltp", "last", "close"}) {
            const double v = o.value(QLatin1String(key)).toDouble(0);
            if (v > 0 && std::isfinite(v))
                return v;
        }
    }
    return 0;
}

} // namespace fincept::workflow::bridge_logic
