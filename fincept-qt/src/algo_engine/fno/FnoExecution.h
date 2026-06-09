// src/algo_engine/fno/FnoExecution.h
// Pure F&O execution helpers: expiry-rule resolution, entry/exit leg construction.
// No I/O, no Qt event loop dependencies. All helpers are free functions in the
// fincept::algo::fno namespace so they can be tested headlessly without a broker.
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/fno/FnoAlgoTypes.h"
#include "services/options/OptionChainTypes.h"
#include "trading/TradingTypes.h"

#include <QDate>
#include <QHash>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::algo::fno {

// ---------------------------------------------------------------------------
// resolve_expiry
// ---------------------------------------------------------------------------
// Resolve an expiry RULE against the list of available expiries returned by
// OptionChainService::list_expiries() (format "DD-MMM-YY", e.g. "26-JUN-25").
//
//   ABSOLUTE    -> return value as-is if it appears in the list; return value
//                  anyway (best-effort) if the list doesn't contain it.
//   NEAREST_DTE -> first future expiry (date >= today) whose DTE (in calendar
//                  days, inclusive of expiry date) is >= value.toInt().
//   WEEKLY      -> first future expiry (date >= today) — nearest expiry.
//   MONTHLY     -> among future expiries, pick the last expiry of the earliest
//                  calendar month that has at least one future expiry.
//
// "Future" means date >= today. The list is arbitrary-ordered on input; the
// function sorts internally.
//
// Returns "" when the list is empty or no expiry satisfies the rule.
QString resolve_expiry(const QString& mode, const QString& value,
                       const QStringList& available_expiries,
                       const QDate& today);

// ---------------------------------------------------------------------------
// resolve_entry_legs
// ---------------------------------------------------------------------------
// Translate a QJsonArray of AlgoFnoLeg rules (strategy_.legs) into concrete
// AlgoOrderLeg objects suitable for submitting as an entry order basket.
//
// For each valid resolved leg the chain row is looked up by instrument token
// (ce_token for CE legs, pe_token for PE legs) and the order quantity is
// computed as resolved.lots * row.lot_size. The price field is set to the
// current LTP from the chain (entry reference for the paper simulator / P&L
// marking). Invalid/FUT legs and legs whose chain row cannot be found are
// silently skipped.
QVector<fincept::algo::AlgoOrderLeg>
resolve_entry_legs(const fincept::services::options::OptionChain& chain,
                   const QJsonArray& strategy_legs);

// ---------------------------------------------------------------------------
// build_exit_legs
// ---------------------------------------------------------------------------
// Build exit order legs from open AlgoLegPosition entries by flipping the
// side (long BUY → exit SELL; short SELL → exit BUY). symbol, token, and
// quantity are preserved; price is taken from the leg's current_price mark.
QVector<fincept::algo::AlgoOrderLeg>
build_exit_legs(const QVector<fincept::algo::AlgoLegPosition>& open_legs);

// ---------------------------------------------------------------------------
// build_basket_request
// ---------------------------------------------------------------------------
// Build a UnifiedTrading BasketOrderRequest from resolved order legs for the
// LIVE broker path. Every leg becomes one NFO market UnifiedOrder; product_type
// maps "CNC"/"delivery" -> Delivery, anything else (NRML/MIS/"") -> Margin
// (F&O is always margin/NRML). Used only on the live branch of
// AlgoEngine::execute_basket; the paper branch never touches the broker.
fincept::trading::BasketOrderRequest
build_basket_request(const QVector<fincept::algo::AlgoOrderLeg>& legs,
                     const QString& product_type);

// ---------------------------------------------------------------------------
// leg_positions_to_json / leg_positions_from_json
// ---------------------------------------------------------------------------
// Serialize an open multi-leg basket position to/from JSON so it can be stored
// on the algo_deployments row (resolved_legs_json) and reattached after an app
// restart. Round-trips symbol, instrument_token, is_call, strike, side_sign,
// quantity, entry_price. On parse, current_price seeds to entry_price and
// unrealized_pnl to 0 (re-marked live on the next tick).
QJsonArray
leg_positions_to_json(const QVector<fincept::algo::AlgoLegPosition>& legs);

QVector<fincept::algo::AlgoLegPosition>
leg_positions_from_json(const QJsonArray& arr);

// ---------------------------------------------------------------------------
// leg_marks_from_chain
// ---------------------------------------------------------------------------
// Map each open leg to its current LTP from a chain snapshot, matched by
// instrument token (ce_token for calls, pe_token for puts). Returns symbol->ltp
// for every leg whose contract is found with a positive LTP; legs not present in
// the snapshot (or with ltp<=0) are omitted so a stale 0 never overwrites a mark.
QHash<QString, double>
leg_marks_from_chain(const QVector<fincept::algo::AlgoLegPosition>& legs,
                     const fincept::services::options::OptionChain& chain);

} // namespace fincept::algo::fno
