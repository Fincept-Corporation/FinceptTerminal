#pragma once
// BrokerClientOrderId — per-attempt idempotency reference for order placement.
//
// BrokerHttp aborts a request client-side after 8 s and reports "Request timed
// out", but a timeout is not proof the order was rejected: the broker may have
// accepted it and only the response was lost. The trader (or the algo engine)
// then retries and a *second* live order is created.
//
// Nearly every broker API deduplicates on a caller-supplied reference —
// Alpaca client_order_id, IBKR cOID, Zerodha/Upstox/Tradier/Motilal tag,
// IIFL orderUniqueIdentifier, Shoonya/Flattrade remarks, AliceBlue orderTag,
// ICICI user_remark, Saxo ExternalReference, Groww order_reference_id,
// Dhan correlationId, FivePaisa RemoteOrderID. Sending a constant ("fincept")
// buys nothing; sending a genuinely unique value per order turns the duplicate
// into a broker-side rejection instead of a second position.
//
// UnifiedOrder::client_order_id (TradingTypes.h) carries ONE reference per order
// intent: UnifiedTrading stamps it the first time an order enters the routing
// layer and never re-mints it, so a re-submit of the same UnifiedOrder after a
// client-side timeout carries the same value. Broker adapters therefore build
// their reference through client_order_ref_for(), which prefers that stable
// value and only mints a fresh one for callers that bypass UnifiedTrading.
// (make_client_order_ref() on its own differs on every attempt and so
// deduplicates nothing across a retry.)

#include "trading/TradingTypes.h"

#include <QString>
#include <QUuid>

namespace fincept::trading {

// Compact, purely alphanumeric reference (several Indian broker APIs reject
// punctuation in their tag/remark fields), e.g. "fin7f3a9c1e4b2d9081".
//
// `max_len` clamps to the narrowest field a given broker accepts — Motilal's
// tag is 10 chars, Zerodha's/ICICI's 20, Dhan's 25, Saxo's 50, Alpaca's 128.
// Keep at least 12 so the random tail stays wide enough to not collide.
inline QString make_client_order_ref(int max_len = 20) {
    const QString id = QStringLiteral("fin") + QUuid::createUuid().toString(QUuid::Id128);
    return (max_len > 0 && id.size() > max_len) ? id.left(max_len) : id;
}

// Reference for `order`: the stable per-intent client_order_id when UnifiedTrading
// stamped one, clamped deterministically (.left) so a retry still yields the same
// value; a fresh per-attempt reference otherwise.
inline QString client_order_ref_for(const UnifiedOrder& order, int max_len = 20) {
    if (order.client_order_id.isEmpty())
        return make_client_order_ref(max_len);
    return (max_len > 0 && order.client_order_id.size() > max_len) ? order.client_order_id.left(max_len)
                                                                  : order.client_order_id;
}

} // namespace fincept::trading
