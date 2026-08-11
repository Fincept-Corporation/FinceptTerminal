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
// TODO(cross-file): UnifiedOrder (src/trading/TradingTypes.h) has no field to
// carry this, so the reference is minted here and therefore differs on every
// attempt. That deduplicates nothing across a *retry* — it only makes each
// submission individually identifiable in the broker's order book, which is
// what the reconciliation path needs today. Add `QString client_order_id` to
// UnifiedOrder, populate it once at the call site that builds the order, and
// change these helpers to prefer it when non-empty; only then does a retry of
// the same intent collapse to one order broker-side.

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

} // namespace fincept::trading
