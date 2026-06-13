#pragma once
// ArenaRisk — pre-trade caps (spec §3.8). Pure functions; circuit counting
// lives on AgentRow.consecutive_failures via ArenaStore.
#include "services/alpha_arena/ArenaTypes.h"
#include "services/alpha_arena/PaperExchange.h"
namespace fincept::arena {
struct RiskVerdict { bool approved = false; QString reason; };
struct RiskLimits {
    double max_leverage = 10;
    double max_position_equity_frac = 0.5;    // per-position notional ≤ 50% equity
    double max_round_notional_frac = 1.0;     // Σ new notional per round ≤ 100% equity
    int circuit_threshold = 3;
};
RiskVerdict evaluate(const AgentAction& a, const AccountView& acct,
                     const RiskLimits& lim, double round_notional_so_far);
} // namespace fincept::arena
