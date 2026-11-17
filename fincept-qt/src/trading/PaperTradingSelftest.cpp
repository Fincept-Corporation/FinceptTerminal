#include "trading/PaperTradingSelftest.h"

#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/TradingTypes.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QTimeZone>

#include <cmath>
#include <cstdio>
#include <functional>
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

// ── Built-in paper account: delayed-price simulation ─────────────────────────
// Table-driven checks of pt_submit_delayed_order / pt_match_delayed_quote /
// pt_settle_delayed_sessions / pt_modify_order and the FX factor, on a throwaway
// fee-free INR portfolio. Times are relative to `now` so the rules about quote age
// and "printed after the order" are exercised exactly.
using CheckFn = std::function<void(const char*, bool)>;

const char* outcome_name(PtSimOutcome o) {
    switch (o) {
        case PtSimOutcome::Filled:
            return "Filled";
        case PtSimOutcome::Resting:
            return "Resting";
        case PtSimOutcome::QueuedForOpen:
            return "QueuedForOpen";
        case PtSimOutcome::Rejected:
            return "Rejected";
    }
    return "?";
}

void run_delayed_price_cases(const CheckFn& check) {
    const QString name = QStringLiteral("__selftest_delayed__");
    const QString tag = QStringLiteral("SELFTEST_DELAYED");
    if (auto existing = pt_find_portfolio(name, tag))
        pt_delete_portfolio(existing->id);
    PtPortfolio pf = pt_create_portfolio(name, 1'000'000.0, QStringLiteral("INR"), 1.0, QStringLiteral("cross"), 0.0, tag);
    const QString pid = pf.id;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    auto quote = [now](double px, qint64 age_ms, bool open, qint64 printed_offset_ms = 0) {
        PtDelayedQuote q;
        q.price = px;
        q.received_ms = px > 0.0 ? now - age_ms : 0;
        q.printed_ms = px > 0.0 ? now + printed_offset_ms : 0;
        q.session_open = open;
        return q;
    };
    auto request = [&pid](const QString& sym, const char* type, const char* side, double qty, double limit,
                          double stop, const char* product = "CNC") {
        PtSimRequest r;
        r.portfolio_id = pid;
        r.symbol = sym;
        r.side = QString::fromLatin1(side);
        r.order_type = QString::fromLatin1(type);
        r.quantity = qty;
        if (limit > 0.0)
            r.price = limit;
        if (stop > 0.0)
            r.stop_price = stop;
        r.exchange = QStringLiteral("NSE");
        r.product = QString::fromLatin1(product);
        return r;
    };
    auto label = [](const char* what, const char* name) {
        static thread_local QByteArray buf; // label lifetime for printf only
        buf = QByteArray(what) + ": " + name;
        return buf.constData();
    };

    // ── 8-13. Submission rules (one fresh symbol per row) ────────────────────────
    struct SubmitCase {
        const char* name;
        const char* type;
        const char* side;
        double qty;
        double limit;
        double stop;
        double quote_px; // 0 = no delayed price has arrived
        qint64 age_ms;   // how long ago the quote was received
        bool open;       // venue in its regular session
        PtSimOutcome want;
        double want_fill; // entry price of the resulting position (Filled only)
    };
    const SubmitCase submit_cases[] = {
        {"market fills at the latest delayed price", "market", "buy", 10, 0, 0, 101.5, 20'000, true,
         PtSimOutcome::Filled, 101.5},
        {"market refused when no price has arrived", "market", "buy", 10, 0, 0, 0.0, 0, true, PtSimOutcome::Rejected,
         0},
        {"market refused on a stale price while open", "market", "buy", 10, 0, 0, 101.5, 10 * 60'000, true,
         PtSimOutcome::Rejected, 0},
        {"market queued while the market is closed", "market", "buy", 10, 0, 0, 101.5, 6 * 3600'000, false,
         PtSimOutcome::QueuedForOpen, 0},
        {"insufficient funds refused", "market", "buy", 100'000, 0, 0, 50.0, 1'000, true, PtSimOutcome::Rejected, 0},
        {"marketable limit fills at its limit", "limit", "buy", 5, 102.0, 0, 101.0, 1'000, true, PtSimOutcome::Filled,
         102.0},
        {"limit below the market rests", "limit", "buy", 5, 95.0, 0, 101.0, 1'000, true, PtSimOutcome::Resting, 0},
        {"limit placed while closed rests", "limit", "buy", 5, 120.0, 0, 101.0, 1'000, false, PtSimOutcome::Resting, 0},
    };
    int row = 0;
    for (const auto& c : submit_cases) {
        const QString sym = QStringLiteral("SUB%1.NS").arg(++row);
        const auto res = pt_submit_delayed_order(request(sym, c.type, c.side, c.qty, c.limit, c.stop),
                                                 quote(c.quote_px, c.age_ms, c.open), now);
        const bool outcome_ok = res.outcome == c.want;
        if (!outcome_ok)
            std::printf("       got %s: %s\n", outcome_name(res.outcome), res.message.toUtf8().constData());
        check(label("submit", c.name), outcome_ok);
        const auto pos = position_for(pid, sym);
        if (c.want == PtSimOutcome::Filled)
            check(label("submit fill price", c.name), pos && approx(pos->entry_price, c.want_fill));
        else
            check(label("submit opens no position", c.name), !pos.has_value());
        if (c.want == PtSimOutcome::Rejected)
            check(label("rejection explains itself", c.name), !res.message.trimmed().isEmpty());
    }

    // ── 14-17. Resting / queued orders matched on later delayed prices ───────────
    struct Step {
        double px;
        qint64 printed_offset_ms; // relative to the order's placement (≈ now)
        bool open;
        bool filled_after;        // the order has filled once this price is processed
    };
    struct MatchCase {
        const char* name;
        const char* type;
        const char* side;
        double limit;
        double stop;
        double submit_px;
        bool submit_open;
        PtSimOutcome submitted;
        Step steps[4];
        int n_steps;
        double want_fill;
    };
    const MatchCase match_cases[] = {
        {"limit buy rests, then fills on a crossing delayed price",
         "limit", "buy", 100.0, 0, 105.0, true, PtSimOutcome::Resting,
         {{103.0, 5'000, true, false}, {99.0, 6'000, true, true}}, 2, 100.0},
        {"queued market order fills at the first price printed after the open",
         "market", "buy", 0, 0, 50.0, false, PtSimOutcome::QueuedForOpen,
         {{51.0, 5'000, false, false}, {52.0, -60'000, true, false}, {53.0, 7'000, true, true}}, 3, 53.0},
        {"stop sell triggers on the delayed price",
         "stop", "sell", 0, 95.0, 100.0, true, PtSimOutcome::Resting,
         {{96.0, 5'000, true, false}, {94.0, 6'000, true, true}}, 2, 94.0},
        {"stop-limit buy triggers, then fills at its limit",
         "stop_limit", "buy", 106.0, 105.0, 100.0, true, PtSimOutcome::Resting,
         {{104.0, 5'000, true, false}, {105.5, 6'000, true, true}}, 2, 106.0},
    };
    for (const auto& c : match_cases) {
        const QString sym = QStringLiteral("MAT%1.NS").arg(++row);
        const auto res = pt_submit_delayed_order(request(sym, c.type, c.side, 10, c.limit, c.stop),
                                                 quote(c.submit_px, 1'000, c.submit_open), now);
        check(label("match setup", c.name), res.outcome == c.submitted);
        bool steps_ok = true;
        for (int i = 0; i < c.n_steps; ++i) {
            const Step& s = c.steps[i];
            pt_match_delayed_quote(pid, sym, quote(s.px, 0, s.open, s.printed_offset_ms));
            const bool filled = position_for(pid, sym).has_value();
            if (filled != s.filled_after) {
                std::printf("       step %d (px %.2f, open %d): filled=%d, expected %d\n", i + 1, s.px, s.open ? 1 : 0,
                            filled ? 1 : 0, s.filled_after ? 1 : 0);
                steps_ok = false;
            }
        }
        check(label("match sequence", c.name), steps_ok);
        const auto pos = position_for(pid, sym);
        check(label("match fill price", c.name), pos && approx(pos->entry_price, c.want_fill));
    }

    // ── 18. Multi-currency: cash in INR, prices in USD ───────────────────────────
    {
        const QString sym = QStringLiteral("AAPL");
        const double bal0 = pt_get_portfolio(pid).balance;
        auto buy = request(sym, "market", "buy", 10, 0, 0);
        buy.exchange = QStringLiteral("NASDAQ"); // 1x leverage outside NSE / BSE
        buy.fx_to_account = 84.0;
        const auto r1 = pt_submit_delayed_order(buy, quote(200.0, 1'000, true), now);
        const double bal1 = pt_get_portfolio(pid).balance;
        check("fx: USD buy locks 10 x 200 x 84 = 168000 INR", r1.outcome == PtSimOutcome::Filled &&
                                                                  approx(bal0 - bal1, 168'000.0, 1.0));
        const auto pos = position_for(pid, sym);
        check("fx: position kept in USD (entry 200)", pos && approx(pos->entry_price, 200.0));
        pt_update_position_price(pid, sym, 210.0);
        const auto marked = position_for(pid, sym);
        check("fx: unrealized P&L stays in USD (+100)", marked && approx(marked->unrealized_pnl, 100.0));
        auto sell = request(sym, "market", "sell", 10, 0, 0);
        sell.exchange = QStringLiteral("NASDAQ");
        sell.fx_to_account = 85.0;
        pt_submit_delayed_order(sell, quote(210.0, 1'000, true), now);
        const double bal2 = pt_get_portfolio(pid).balance;
        // margin 168000 released + realized (210-200) x 10 x 85 = 8500
        check("fx: close releases margin + realized P&L converted at the closing rate (+8500 INR)",
              approx(bal2 - bal0, 8'500.0, 1.0));
        bool trade_pnl_inr = false;
        for (const auto& t : pt_get_trades(pid, 50))
            if (t.symbol == sym && t.side == QLatin1String("sell") && approx(t.pnl, 8'500.0, 1.0))
                trade_pnl_inr = true;
        check("fx: trade record carries the INR P&L", trade_pnl_inr);
    }

    // ── 19. Square-off at the venue's session close + DAY-order expiry ───────────
    {
        const QString sym = QStringLiteral("SQ1.NS");
        pt_submit_delayed_order(request(sym, "market", "buy", 10, 0, 0, "MIS"), quote(100.0, 1'000, true), now);
        pt_update_position_price(pid, sym, 104.0);
        const QString lim = QStringLiteral("EXP1.NS");
        const auto resting =
            pt_submit_delayed_order(request(lim, "limit", "buy", 10, 90.0, 0), quote(100.0, 1'000, true), now);
        const double bal_before = pt_get_portfolio(pid).balance;
        const QDateTime now_dt = QDateTime::fromMSecsSinceEpoch(now, QTimeZone::UTC);
        // Session still running: nothing happens.
        int n = pt_settle_delayed_sessions(
            pid, [&](const QString&, const QDateTime&) { return now_dt.addSecs(3600); }, now_dt);
        check("session close: nothing settles before the close", n == 0 && position_for(pid, sym).has_value());
        // The close has passed.
        n = pt_settle_delayed_sessions(
            pid, [&](const QString& s, const QDateTime&) { return s == sym || s == lim ? now_dt.addSecs(-60) : QDateTime(); },
            now_dt);
        check("session close: intraday position squared off", !position_for(pid, sym).has_value());
        bool expired = false;
        for (const auto& o : pt_get_orders(pid, QStringLiteral("cancelled")))
            if (o.id == resting.order.id)
                expired = true;
        check("session close: resting DAY order expired", expired && n >= 2);
        // Square-off releases the MIS margin (10 x 100 / 5x = 200) plus +40 realized
        // ((104 - 100) x 10 at the last mark); the expired order hands back its 900.
        check("session close: square-off at the last mark books +40, margins released",
              approx(pt_get_portfolio(pid).balance - bal_before, 200.0 + 40.0 + 900.0, 1.0));
    }

    // ── 20. Modify a resting order (cancel + replace) ────────────────────────────
    {
        const QString sym = QStringLiteral("MOD1.NS");
        const auto res =
            pt_submit_delayed_order(request(sym, "limit", "buy", 10, 90.0, 0), quote(100.0, 1'000, true), now);
        bool ok = false;
        try {
            const PtOrder repl = pt_modify_order(res.order.id, 20, 92.0);
            const auto pending = OrderMatcher::instance().get_pending_orders(sym, pid);
            ok = repl.quantity == 20.0 && repl.price && approx(*repl.price, 92.0) && pending.size() == 1 &&
                 pending.first().id == repl.id;
        } catch (const std::exception& e) {
            std::printf("       modify threw: %s\n", e.what());
        }
        check("modify: replacement rests in the matcher with the new qty / price", ok);
        bool old_cancelled = false;
        for (const auto& o : pt_get_orders(pid, QStringLiteral("cancelled")))
            if (o.id == res.order.id)
                old_cancelled = true;
        check("modify: the original order is cancelled", old_cancelled);
    }

    // ── 21. Insufficient funds on a resting order is refused up front ────────────
    {
        const auto res = pt_submit_delayed_order(request(QStringLiteral("BIG.NS"), "limit", "buy", 1'000'000, 50.0, 0),
                                                 quote(60.0, 1'000, true), now);
        check("funds: an unaffordable limit order is rejected with the margin reason",
              res.outcome == PtSimOutcome::Rejected && res.message.contains(QStringLiteral("margin"), Qt::CaseInsensitive));
    }

    OrderMatcher::instance().clear_portfolio_orders(pid);
    pt_delete_portfolio(pid);
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
    auto open_market = [&](const QString& sym, const QString& side, double qty, double px, const QString& product,
                           const QString& exchange) {
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
        check("short: position opened as short",
              p && p->side.compare(QStringLiteral("short"), Qt::CaseInsensitive) == 0);
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
        check("zero-tick: P&L is NOT the full premium (+10000)", p && !approx(p->unrealized_pnl, 10000.0, 1.0));
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

    run_delayed_price_cases(check); // built-in paper account: delayed-price rules (cases 8-21)

    std::printf("\npaper-trading selftest: %s (%d failure%s)\n", failures == 0 ? "OK" : "FAILED", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

} // namespace fincept::trading
