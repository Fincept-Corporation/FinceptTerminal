#include "services/alpha_arena/RiskEngine.h"

#include <cmath>

namespace fincept::services::alpha_arena {

namespace {

/// 30-minute cooldown for Monk mode, expressed in ms.
constexpr qint64 kMonkCooldownMs = 30LL * 60LL * 1000LL;

/// Notional size cap in Monk mode (25% of equity).
constexpr double kMonkMaxNotionalFrac = 0.25;

inline RiskVerdict reject(const QString& reason) {
    RiskVerdict v;
    v.outcome = RiskVerdict::Outcome::Reject;
    v.reason = reason;
    return v;
}

inline RiskVerdict accept() {
    RiskVerdict v;
    v.outcome = RiskVerdict::Outcome::Accept;
    return v;
}

inline RiskVerdict amend(const ProposedAction& a, const QString& reason) {
    RiskVerdict v;
    v.outcome = RiskVerdict::Outcome::Amend;
    v.reason = reason;
    v.amended = a;
    return v;
}

bool is_entry_signal(Signal s) {
    return s == Signal::BuyToEnter || s == Signal::SellToEnter;
}

} // namespace

double recompute_risk_usd(const ProposedAction& a, double entry_price) {
    if (a.stop_loss <= 0.0 || entry_price <= 0.0 || a.quantity <= 0.0) return 0.0;
    return std::fabs(entry_price - a.stop_loss) * a.quantity;
}

double estimated_liquidation_price(double entry, int leverage, bool is_long, double mm_frac) {
    if (entry <= 0.0 || leverage < 1) return 0.0;
    // Hyperliquid: liq_price = entry × (1 ± (1/leverage) × (1 - mm_frac))
    //                       (long: −, short: +)
    const double term = (1.0 / leverage) * (1.0 - mm_frac);
    return is_long ? entry * (1.0 - term) : entry * (1.0 + term);
}

RiskVerdict evaluate_action(const ProposedAction& action,
                            const AgentRiskState& agent,
                            CompetitionMode mode) {
    // ── Trivial accepts: hold/close don't go through entry-side checks. ─────
    if (action.signal == Signal::Hold) return accept();
    if (action.signal == Signal::Close) {
        if (agent.existing_qty_in_coin == 0.0) {
            return reject(QStringLiteral("close requested but no position"));
        }
        return accept();
    }

    // ── Entry signals from here ─────────────────────────────────────────────

    // 1. One position per coin.
    if (agent.existing_qty_in_coin != 0.0) {
        return reject(QStringLiteral("position_exists"));
    }

    // 2. Leverage cap.
    if (action.leverage < limits::kMinLeverage || action.leverage > limits::kMaxLeverage) {
        return reject(QStringLiteral("leverage out of range [1,20]"));
    }

    // 3. Quantity & stops sanity (already enforced by parser; double-check).
    if (action.quantity <= 0.0 || action.profit_target <= 0.0 || action.stop_loss <= 0.0) {
        return reject(QStringLiteral("entry requires qty/profit_target/stop_loss > 0"));
    }
    if (agent.mark_price <= 0.0) {
        return reject(QStringLiteral("no mark price for coin"));
    }
    if (agent.equity <= 0.0) {
        return reject(QStringLiteral("non-positive equity"));
    }

    const bool is_long = (action.signal == Signal::BuyToEnter);

    // 4. Stops must be on the right side of mark.
    if (is_long) {
        if (action.stop_loss >= agent.mark_price) {
            return reject(QStringLiteral("long stop_loss must be below mark"));
        }
        if (action.profit_target <= agent.mark_price) {
            return reject(QStringLiteral("long profit_target must be above mark"));
        }
    } else {
        if (action.stop_loss <= agent.mark_price) {
            return reject(QStringLiteral("short stop_loss must be above mark"));
        }
        if (action.profit_target >= agent.mark_price) {
            return reject(QStringLiteral("short profit_target must be below mark"));
        }
    }

    // 5. Liquidation buffer ≥ 15% of mark.
    const double liq = estimated_liquidation_price(agent.mark_price, action.leverage,
                                                   is_long, agent.maintenance_margin_frac);
    const double buffer_frac = std::fabs(agent.mark_price - liq) / agent.mark_price;
    if (buffer_frac < limits::kMinLiquidationBufferFrac) {
        return reject(QStringLiteral("liquidation buffer %1%% below 15%% minimum")
                          .arg(buffer_frac * 100.0, 0, 'f', 2));
    }

    // 6. Risk-per-trade ≤ 2% of equity (server-recomputed).
    const double recomputed_risk = recompute_risk_usd(action, agent.mark_price);
    const double max_risk = agent.equity * limits::kMaxRiskPerTradeFrac;
    if (recomputed_risk > max_risk) {
        return reject(QStringLiteral("risk_usd %1 exceeds 2%% of equity (%2)")
                          .arg(recomputed_risk, 0, 'f', 2)
                          .arg(max_risk, 0, 'f', 2));
    }

    // 7. Mode-specific (Monk).
    if (mode == CompetitionMode::Monk) {
        // 7a. 30-min cooldown per coin.
        if (agent.last_entry_utc_ms_on_coin > 0 &&
            (agent.now_utc_ms - agent.last_entry_utc_ms_on_coin) < kMonkCooldownMs) {
            return reject(QStringLiteral("monk: 30-min cooldown active for coin"));
        }
        // 7b. Notional ≤ 25% equity.
        const double notional = action.quantity * agent.mark_price;
        if (notional > agent.equity * kMonkMaxNotionalFrac) {
            return reject(QStringLiteral("monk: notional %1 exceeds 25%% of equity")
                              .arg(notional, 0, 'f', 2));
        }
    }

    // 8. risk_usd drift check — amend (not reject) on > 5% deviation.
    if (action.risk_usd > 0.0) {
        const double drift = std::fabs(action.risk_usd - recomputed_risk) /
                             std::max(action.risk_usd, recomputed_risk);
        if (drift > limits::kRiskUsdRecomputeToleranceFrac) {
            ProposedAction corrected = action;
            corrected.risk_usd = recomputed_risk;
            return amend(corrected,
                         QStringLiteral("risk_usd drift %1%% > 5%% — server overrode")
                             .arg(drift * 100.0, 0, 'f', 2));
        }
    }

    return accept();
}

CircuitState advance_circuit(const CircuitState& prev,
                             bool tick_had_parse_failure,
                             bool any_action_rejected,
                             double current_drawdown_frac) {
    CircuitState next = prev;

    next.consecutive_parse_failures = tick_had_parse_failure ? prev.consecutive_parse_failures + 1 : 0;
    next.consecutive_risk_rejects = any_action_rejected ? prev.consecutive_risk_rejects + 1 : 0;
    next.max_drawdown_frac = std::max(prev.max_drawdown_frac, current_drawdown_frac);

    // Once open, stays open until a manual resume (caller resets the struct).
    if (prev.open) {
        next.open = true;
        return next;
    }

    if (next.consecutive_parse_failures >= circuit_thresholds::kParseFailureLimit) next.open = true;
    if (next.consecutive_risk_rejects >= circuit_thresholds::kRiskRejectLimit) next.open = true;
    if (next.max_drawdown_frac >= circuit_thresholds::kDrawdownLimit) next.open = true;

    return next;
}

} // namespace fincept::services::alpha_arena
