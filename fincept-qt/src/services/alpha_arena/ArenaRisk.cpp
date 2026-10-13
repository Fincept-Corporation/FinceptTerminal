#include "services/alpha_arena/ArenaRisk.h"
namespace fincept::arena {
RiskVerdict evaluate(const AgentAction& a, const AccountView& acct, const RiskLimits& lim,
                     double round_notional_so_far) {
    // Close (full or partial reduce) and hold never increase risk → always allowed.
    if (a.kind != ActionKind::Open)
        return {true, {}};
    if (acct.equity <= 0)
        return {false, QStringLiteral("account equity exhausted")};
    if (a.leverage < 1)
        return {false, QStringLiteral("leverage must be >= 1")};
    if (a.leverage > lim.max_leverage)
        return {false, QStringLiteral("leverage %1 > cap %2").arg(a.leverage).arg(lim.max_leverage)};
    // The cap is on the POSITION, not the order: a same-side open adds to the
    // existing position (PaperExchange averages in), so checking the order alone
    // let an agent pyramid one coin past the cap 50% at a time. Existing size is
    // valued at entry (the AccountView carries no marks). An opposite-side open
    // flips — the old position is closed first — so only the new leg counts.
    double existing = 0;
    for (const auto& p : acct.positions)
        if (p.coin == a.coin && p.side == a.side)
            existing += p.qty * p.entry_price;
    if (existing + a.size_usd > lim.max_position_equity_frac * acct.equity)
        return {false, QStringLiteral("position %1 (incl. %2 already open) > %3% of equity")
                           .arg(existing + a.size_usd, 0, 'f', 0)
                           .arg(existing, 0, 'f', 0)
                           .arg(lim.max_position_equity_frac * 100)};
    if (round_notional_so_far + a.size_usd > lim.max_round_notional_frac * acct.equity)
        return {false, QStringLiteral("round notional cap exceeded")};
    if (a.size_usd / a.leverage > acct.balance)
        return {false, QStringLiteral("insufficient free balance")};
    return {true, {}};
}
} // namespace fincept::arena
