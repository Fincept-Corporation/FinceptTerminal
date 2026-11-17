#pragma once
// OrderApprovalRule.h — the approval decision for one order, as a pure function.
//
// The Action Center's per-account "Order approval mode" (Auto / Semi-Auto) decides
// whether orders made WITHOUT a person at the ticket wait for a human. Callers that
// know they are automated consult ActionCenter::should_queue() themselves; this rule
// is the same decision made again at UnifiedTrading's order entry points (live AND
// paper), from UnifiedOrder::source, so an automated order that did not route itself
// to the queue still cannot be placed directly (defence in depth).
//
//   came from an approved queue entry .................................. PLACE
//   source kind is not automated (empty, ticket, basket, split, smart,
//     replication, kill_switch, auto_square_off, session_close) ......... PLACE
//   exit kind (workflow_exit, algo_exit) that the entry point itself
//     verified reduce-only at submission (ReduceOnly::Yes), any mode ..... PLACE
//   automated kind (agent, workflow, algo, script, an exit kind NOT
//     verified reduce-only — No / Unknown —, or any kind this terminal
//     does not know — fail closed), approval mode unknown
//     (no account: the legacy session path) ............................ REFUSE
//   automated kind, account in Semi-Auto (also when its mode cannot be
//     read — ActionCenter fails closed to Semi-Auto) .................... QUEUE
//   automated kind, account in Auto ..................................... PLACE
//
// Reduce-only (computed by the caller, passed in — this rule stays pure): an open
// position exists for the account + symbol, the order is on the opposite side and
// its quantity is at most the position's absolute quantity — or the order is the
// engine's own close / flatten path, which sizes itself from the position. Only the
// exit kinds read it; for every other kind it changes nothing.
//
// Qt Core only; no state. Unit-tested by tests/tst_order_approval_rule.cpp.

#include "trading/TradingTypes.h"

#include <QString>

namespace fincept::trading::approval_rule {

enum class Route { Place, Queue, Refuse };

inline const char* route_name(Route r) {
    switch (r) {
        case Route::Place:
            return "place";
        case Route::Queue:
            return "queue";
        case Route::Refuse:
            return "refuse";
    }
    return "refuse";
}

/// Kinds a person creates at a screen, or the terminal's own risk-reducing jobs.
inline bool is_attended_kind(const QString& kind) {
    static const char* const kAttended[] = {"",            "ticket",      "basket",          "split",        "smart",
                                            "replication", "kill_switch", "auto_square_off", "session_close"};
    for (const char* k : kAttended)
        if (kind == QLatin1String(k))
            return true;
    return false;
}

/// True for a source whose orders must wait for a human when the account is in
/// Semi-Auto: agent, workflow, algo, script, workflow_exit / algo_exit (unless
/// verified reduce-only — see decide()) — and any unknown non-empty kind.
inline bool needs_human_approval(const QString& source) {
    return !is_attended_kind(order_source_kind(source));
}

/// Automated orders that only close / reduce a position: a workflow's close position,
/// trailing stop or flatten (workflow_exit:<workflow id>), an algo deployment's exit,
/// stop-loss or rollback (algo_exit:<deployment id>).
inline bool is_exit_kind(const QString& kind) {
    return kind == QLatin1String("workflow_exit") || kind == QLatin1String("algo_exit");
}

inline bool is_exit_source(const QString& source) {
    return is_exit_kind(order_source_kind(source));
}

/// The entry point's own reduce-only check of an order, made at submission time:
/// Yes (verified), No (it is not: no such position, same side, or larger than the
/// position), Unknown (the position could not be read, or this entry point does not
/// check).
enum class ReduceOnly { Yes, No, Unknown };

inline const char* reduce_only_name(ReduceOnly r) {
    switch (r) {
        case ReduceOnly::Yes:
            return "yes";
        case ReduceOnly::No:
            return "no";
        case ReduceOnly::Unknown:
            return "unknown";
    }
    return "unknown";
}

/// The decision. `semi_auto`: the account's approval mode requires approval
/// (ActionCenter::should_queue). `mode_known`: false when there is no account to
/// read a mode from. `came_from_approval`: the order is being executed from an
/// Action Center entry a person approved (ApprovedOrderScope). `reduce_only`: the
/// entry point's own check (read for the exit kinds only; Unknown when not checked).
inline Route decide(const QString& source, bool semi_auto, bool came_from_approval, bool mode_known = true,
                    ReduceOnly reduce_only = ReduceOnly::Unknown) {
    if (came_from_approval)
        return Route::Place;
    if (!needs_human_approval(source))
        return Route::Place;
    if (reduce_only == ReduceOnly::Yes && is_exit_source(source))
        return Route::Place; // a verified protective exit is never held for approval
    if (!mode_known)
        return Route::Refuse;
    return semi_auto ? Route::Queue : Route::Place;
}

/// The source an order gets at an entry point: an agent action in progress wins
/// over an empty or screen-set source (ticket / basket / split / smart — the agent
/// pressed that screen's button), never over an automated or system source.
/// `agent_source` is core::agent::current_agent_order_source() (empty when none).
inline QString effective_source(const QString& stated, const QString& agent_source) {
    if (agent_source.isEmpty())
        return stated;
    const QString kind = order_source_kind(stated);
    if (kind.isEmpty() || kind == QLatin1String("ticket") || kind == QLatin1String("basket") ||
        kind == QLatin1String("split") || kind == QLatin1String("smart"))
        return agent_source;
    return stated;
}

} // namespace fincept::trading::approval_rule
