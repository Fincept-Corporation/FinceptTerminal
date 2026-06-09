#include "trading/PaperTradingSelftest.h"

#include "trading/PaperTrading.h"
#include "trading/TradingTypes.h"

#include <QString>

#include <cmath>
#include <cstdio>
#include <optional>

namespace fincept::trading {

namespace {

// Find an open position by exact symbol (selftest symbols are unambiguous).
std::optional<PtPosition> position_for(const QString& portfolio_id, const QString& symbol) {
    for (const auto& p : pt_get_positions(portfolio_id))
        if (p.symbol == symbol)
            return p;
    return std::nullopt;
}

bool approx(double a, double b, double eps = 0.01) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int run_paper_trading_selftest() {
    int failures = 0;
    auto check = [&](const char* label, bool ok) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok)
            ++failures;
    };

    // Throwaway portfolio. fee_rate = 0 so realized/unrealized P&L is exact and
    // assertable; INR + NFO so the option-leverage and instrument-type paths run.
    const QString name = QStringLiteral("__selftest_paper__");
    const QString pf_exchange = QStringLiteral("NFO");
    if (auto existing = pt_find_portfolio(name, pf_exchange))
        pt_delete_portfolio(existing->id);
    PtPortfolio pf = pt_create_portfolio(name, /*balance=*/1'000'000.0, /*currency=*/QStringLiteral("INR"),
                                         /*leverage=*/1.0, /*margin_mode=*/QStringLiteral("cross"),
                                         /*fee_rate=*/0.0, /*exchange=*/pf_exchange);
    const QString pid = pf.id;

    // Place a market order and fill it immediately at `px` (the path every screen
    // uses for a market order: place -> fill).
    auto open_market = [&](const QString& sym, const QString& side, double qty, double px,
                           const QString& product, const QString& exchange) {
        auto o = pt_place_order(pid, sym, side, QStringLiteral("market"), qty, px, std::nullopt,
                                /*reduce_only=*/false, exchange, product);
        pt_fill_order(o.id, px);
    };

    const QString NSE = QStringLiteral("NSE");
    const QString NFO = QStringLiteral("NFO");

    // ── 1. Long equity marks to market with the right sign + magnitude ──────
    {
        const QString sym = QStringLiteral("RELIANCE");
        open_market(sym, QStringLiteral("buy"), 10, 100.0, QStringLiteral("MIS"), NSE);
        pt_update_position_price(pid, sym, 110.0);
        auto p = position_for(pid, sym);
        check("long: position opened", p.has_value());
        check("long: side == long", p && p->side.compare(QStringLiteral("long"), Qt::CaseInsensitive) == 0);
        check("long: unrealized P&L = +100 at 110", p && approx(p->unrealized_pnl, 100.0));
        open_market(sym, QStringLiteral("sell"), 10, 110.0, QStringLiteral("MIS"), NSE);
        check("long: exits on opposite fill", !position_for(pid, sym).has_value());
    }

    // ── 2. Short option opens flat (P&L 0 at entry) ─────────────────────────
    const QString opt = QStringLiteral("NIFTY24DEC2450000CE");
    {
        open_market(opt, QStringLiteral("sell"), 50, 200.0, QStringLiteral("NRML"), NFO);
        auto p = position_for(pid, opt);
        check("short: position opened as short", p && p->side.compare(QStringLiteral("short"), Qt::CaseInsensitive) == 0);
        check("short: unrealized P&L ~0 at entry", p && approx(p->unrealized_pnl, 0.0));
    }

    // ── 3. THE PHANTOM-PROFIT BUG: a zero/garbage tick on a short must NOT ───
    //     book the full premium (entry*qty) as profit. This is exactly what the
    //     user saw: "square up shows total trade value in profit". Pre-fix the
    //     engine writes unrealized = (200 - 0)*50 = +10000; post-fix the 0 tick
    //     is ignored and P&L holds at its last good value (~0 here).
    {
        pt_update_position_price(pid, opt, 0.0);
        auto p = position_for(pid, opt);
        check("zero-tick: P&L is NOT the full premium (+10000)",
              p && !approx(p->unrealized_pnl, 10000.0, 1.0));
        check("zero-tick: P&L holds at last good value (~0)", p && approx(p->unrealized_pnl, 0.0));

        // A real tick still marks correctly: short 50 @ 200, now 150 -> +2500.
        pt_update_position_price(pid, opt, 150.0);
        p = position_for(pid, opt);
        check("short: unrealized P&L = +2500 at 150", p && approx(p->unrealized_pnl, 2500.0));
    }

    // ── 4. Square-off a short: it actually exits AND books the right realized ──
    {
        // Close the short (50 @ 200, marked 150) with a buy 50 @ 150.
        open_market(opt, QStringLiteral("buy"), 50, 150.0, QStringLiteral("NRML"), NFO);
        check("square-off: short position exited", !position_for(pid, opt).has_value());
        bool booked = false;
        for (const auto& t : pt_get_trades(pid))
            if (t.symbol == opt && t.side.compare(QStringLiteral("buy"), Qt::CaseInsensitive) == 0 &&
                approx(t.pnl, 2500.0))
                booked = true;
        check("square-off: realized P&L = +2500 recorded", booked);
    }

    // ── 5. Netting: an equal opposite fill closes out, never leaves a phantom ──
    //     reverse position (the failure mode behind "doesn't exit").
    {
        const QString sym = QStringLiteral("TCS");
        open_market(sym, QStringLiteral("buy"), 5, 1000.0, QStringLiteral("MIS"), NSE);
        open_market(sym, QStringLiteral("sell"), 5, 1100.0, QStringLiteral("MIS"), NSE);
        check("netting: flat after equal opposite fill", !position_for(pid, sym).has_value());
    }

    // ── 6. Multi-leg (builder-style) strategy: every leg actually fills ─────
    {
        const QString pe = QStringLiteral("NIFTY24DEC2449000PE");
        const QString ce = QStringLiteral("NIFTY24DEC2451000CE");
        open_market(pe, QStringLiteral("buy"), 50, 120.0, QStringLiteral("NRML"), NFO);
        open_market(ce, QStringLiteral("sell"), 50, 90.0, QStringLiteral("NRML"), NFO);
        check("multi-leg: both legs opened as positions",
              position_for(pid, pe).has_value() && position_for(pid, ce).has_value());
    }

    // ── 7. Margin lock on open, full release (+P&L) on close ────────────────
    {
        const QString sym = QStringLiteral("INFY");
        const double bal0 = pt_get_portfolio(pid).balance;
        open_market(sym, QStringLiteral("buy"), 10, 500.0, QStringLiteral("CNC"), NSE); // CNC 1x -> locks 5000
        const double bal1 = pt_get_portfolio(pid).balance;
        check("margin: balance drops by locked margin on open", bal1 < bal0 - 4999.0);
        pt_update_position_price(pid, sym, 520.0);
        open_market(sym, QStringLiteral("sell"), 10, 520.0, QStringLiteral("CNC"), NSE); // realize +200, release
        const double bal2 = pt_get_portfolio(pid).balance;
        check("margin: balance restored + realized P&L on close", approx(bal2, bal0 + 200.0, 1.0));
    }

    // Clean up the throwaway portfolio (positions/orders/trades/blocks cascade).
    pt_delete_portfolio(pid);

    std::printf("\npaper-trading selftest: %s (%d failure%s)\n", failures == 0 ? "OK" : "FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

} // namespace fincept::trading
