#include "services/alpha_arena/ArenaRisk.h"
namespace fincept::arena {
RiskVerdict evaluate(const AgentAction& a, const AccountView& acct,
                     const RiskLimits& lim, double round_notional_so_far) {
    // Close (full or partial reduce) and hold never increase risk → always allowed.
    if (a.kind != ActionKind::Open) return {true, {}};
    if (acct.equity <= 0)
        return {false, QStringLiteral("account equity exhausted")};
    if (a.leverage < 1)
        return {false, QStringLiteral("leverage must be >= 1")};
    if (a.leverage > lim.max_leverage)
        return {false, QStringLiteral("leverage %1 > cap %2").arg(a.leverage).arg(lim.max_leverage)};
    if (a.size_usd > lim.max_position_equity_frac * acct.equity)
        return {false, QStringLiteral("position %1 > %2% of equity")
                           .arg(a.size_usd).arg(lim.max_position_equity_frac * 100)};
    if (round_notional_so_far + a.size_usd > lim.max_round_notional_frac * acct.equity)
        return {false, QStringLiteral("round notional cap exceeded")};
    if (a.size_usd / a.leverage > acct.balance)
        return {false, QStringLiteral("insufficient free balance")};
    return {true, {}};
}
} // namespace fincept::arena
