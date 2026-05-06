#pragma once
// RiskEngine — server-side, fail-closed validation of every model action.
//
// Runs *between* ContextBuilder/ModelDispatcher (which produce ProposedAction)
// and OrderRouter (which talks to the venue). Pure function, no I/O, no state
// — caller threads agent state in via the inputs.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §6 (Risk engine).
//
// Hard rules (rejection):
//   * One position per coin (caller passes existing_long/short).
//   * Liquidation buffer ≥ 15% of price-at-decision.
//   * Risk per trade ≤ 2% of account equity (recomputed by us).
//   * Max leverage 20×.
//
// Mode-specific (Monk):
//   * ≤ 1 entry per coin per 30 minutes.
//   * Notional position size ≤ 25% of equity.
//
// Drift handling:
//   * If the model's risk_usd differs from our recomputed value by > 5%,
//     we *amend* the action with the corrected value (not reject) and
//     surface the discrepancy as a benchmarkable trait of the model.

#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"

#include <QString>

namespace fincept::services::alpha_arena {

/// Inputs the engine needs about an agent's current world to decide a verdict.
/// Caller assembles this; it is not stored anywhere by the engine.
struct AgentRiskState {
    /// Current equity (used for 2%-of-equity and 25%-of-equity Monk caps).
    double equity = 0.0;
    /// Position the agent currently has in this coin (qty signed, 0 = none).
    double existing_qty_in_coin = 0.0;
    /// UTC ms of the last entry on this coin, or 0 if never. Drives Monk
    /// 30-min cooldown.
    qint64 last_entry_utc_ms_on_coin = 0;
    /// "Now" — the engine compares last_entry_utc_ms_on_coin against this.
    qint64 now_utc_ms = 0;
    /// Maintenance margin fraction for this coin (Hyperliquid metadata).
    /// Used when computing the liquidation price the model implicitly
    /// asks for. Default 5% if the venue hasn't supplied a value.
    double maintenance_margin_frac = 0.05;
    /// Mark price at decision time.
    double mark_price = 0.0;
};

/// Per-agent kill-switch state. Caller persists this between ticks.
struct CircuitState {
    int consecutive_parse_failures = 0;
    int consecutive_risk_rejects = 0;
    double max_drawdown_frac = 0.0;          // worst peak-to-trough so far
    bool open = false;                       // true => agent paused
};

namespace circuit_thresholds {
inline constexpr int kParseFailureLimit = 3;
inline constexpr int kRiskRejectLimit = 3;
inline constexpr double kDrawdownLimit = 0.50; // 50%
} // namespace circuit_thresholds

/// Pure: returns the verdict. The verdict's `amended` form (if any) is what
/// the caller should hand to the OrderRouter; on Reject, do nothing.
RiskVerdict evaluate_action(const ProposedAction& action,
                            const AgentRiskState& agent,
                            CompetitionMode mode);

/// Apply the outcome of one tick to the circuit. Caller passes how many
/// actions were proposed, how many parsed cleanly, and how many were
/// rejected by evaluate_action. Returns the updated state.
///
/// `current_drawdown_frac` is the agent's running peak-to-trough fraction.
/// When the circuit transitions to open, the caller is expected to halt the
/// agent (close positions on the venue) and post an `agent_circuit_open`
/// event into the audit log.
CircuitState advance_circuit(const CircuitState& prev,
                             bool tick_had_parse_failure,
                             bool any_action_rejected,
                             double current_drawdown_frac);

/// Helper used by both the engine and tests — recompute risk_usd from
/// |entry − stop_loss| × qty.
double recompute_risk_usd(const ProposedAction& a, double entry_price);

/// Helper — the price at which `action` would be liquidated, given mark
/// price, leverage and maintenance margin. For longs, this is below mark;
/// for shorts, above. Used in the 15%-buffer check.
double estimated_liquidation_price(double entry, int leverage, bool is_long, double mm_frac);

} // namespace fincept::services::alpha_arena
