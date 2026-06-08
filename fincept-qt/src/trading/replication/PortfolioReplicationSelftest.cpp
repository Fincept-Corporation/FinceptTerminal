#include "trading/replication/PortfolioReplicationSelftest.h"

#include "trading/replication/PortfolioReplicationService.h"

#include <cmath>
#include <cstdio>

namespace fincept::trading::replication {

namespace {
bool approx(double a, double b, double eps = 0.01) { return std::fabs(a - b) <= eps; }
} // namespace

int run_portfolio_replication_selftest() {
    int failures = 0;
    auto check = [&](const char* label, bool ok) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok)
            ++failures;
    };

    // Synthetic source: 1 holding, 1 short MIS position, 1 unmappable holding.
    QVector<SourceItem> items;
    items.push_back({ItemKind::Holding,  "RELIANCE", "NSE", "",    "",      10, 2400, 2500});
    items.push_back({ItemKind::Position, "TCS",      "NSE", "MIS", "SHORT",  5, 3500, 3600});
    items.push_back({ItemKind::Holding,  "UNKNOWNX", "NSE", "",    "",       3,  100,  110});

    // Resolver: identity-normalize, but UNKNOWNX is not tradable on target.
    SymbolResolver resolver = [](const QString& s, const QString&) -> ResolveResult {
        if (s == QStringLiteral("UNKNOWNX"))
            return {false, s};
        return {true, s};
    };

    ReplicationOptions both; // include_holdings + include_positions both true
    auto plan = build_plan(items, both, /*target_available=*/100000.0,
                           "srcAcct", "tgtAcct", "tgtPortfolio", resolver);

    check("emits one row per in-scope item", plan.orders.size() == 3);

    const auto& r0 = plan.orders[0]; // RELIANCE holding
    check("holding -> buy",        r0.side == QStringLiteral("buy"));
    check("holding -> CNC",        r0.product == QStringLiteral("CNC"));
    check("quantity is exact 1:1", approx(r0.quantity, 10));
    check("est_value = qty*ltp",   approx(r0.est_value, 25000.0)); // 10 * 2500
    check("mapped holding included", r0.included && r0.mapped);

    const auto& r1 = plan.orders[1]; // TCS short MIS
    check("short position -> sell", r1.side == QStringLiteral("sell"));
    check("position keeps product MIS", r1.product == QStringLiteral("MIS"));

    const auto& r2 = plan.orders[2]; // UNKNOWNX
    check("unmapped flagged",   !r2.mapped);
    check("unmapped unchecked", !r2.included);
    check("unmapped has warning", !r2.warning.isEmpty());

    // required_capital counts included rows only: 25000 (RELIANCE) + 18000 (TCS).
    check("required_capital excludes unmapped", approx(plan.required_capital, 43000.0));

    // Scope toggle: holdings only → positions dropped entirely.
    ReplicationOptions holdings_only{true, false};
    auto plan2 = build_plan(items, holdings_only, 100000.0, "s", "t", "p", resolver);
    check("holdings-only drops positions", plan2.orders.size() == 2);
    bool any_pos = false;
    for (const auto& o : plan2.orders)
        if (o.kind == ItemKind::Position)
            any_pos = true;
    check("holdings-only has no position rows", !any_pos);

    std::printf(failures == 0 ? "\nportfolio-replication selftest: ALL PASS\n"
                              : "\nportfolio-replication selftest: %d FAILURE(S)\n",
                failures);
    return failures == 0 ? 0 : 1;
}

} // namespace fincept::trading::replication
