// Headless self-test of the UnifiedPortfolioService merge layer (no GUI /
// network / broker / keys). Asserts: per-(symbol,exchange) grouping, weighted
// average price, signed-quantity netting across accounts, summed P&L, the
// non-INR account exclusion, quote-tick patching, and summary math.
//
// Run: QT_QPA_PLATFORM=offscreen FinceptTerminal --selftest-portfolio-monitor
// Returns 0 when every assertion passes, 1 otherwise.

#include "trading/UnifiedPortfolioService.h"

#include <QString>

#include <cmath>
#include <cstdio>

namespace fincept::trading {

namespace {
bool approx(double a, double b, double eps = 0.01) {
    return std::fabs(a - b) <= eps;
}

const AggRow* find_row(const QVector<AggRow>& rows, const QString& symbol) {
    for (const auto& r : rows)
        if (r.symbol == symbol)
            return &r;
    return nullptr;
}
} // namespace

int run_portfolio_monitor_selftest() {
    int failures = 0;
    auto check = [&](const char* label, bool ok) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok)
            ++failures;
    };

    auto& svc = UnifiedPortfolioService::instance();
    svc.test_clear();
    svc.test_register_account("acct-fyers", "Fyers — T", "INR");
    svc.test_register_account("acct-dhan", "Dhan — T", "INR");
    svc.test_register_account("acct-alpaca", "Alpaca — US", "USD"); // must be excluded

    check("non-INR account excluded", svc.accounts().size() == 2);

    // The default mode is Paper; the live-merge assertions below need Live.
    svc.set_mode(UnifiedPortfolioService::Mode::Live);
    check("mode switched to live", svc.mode() == UnifiedPortfolioService::Mode::Live);

    // ── Positions: same symbol in two accounts ───────────────────────────────
    BrokerPosition p1; // long 10 @ 1280 on Fyers
    p1.symbol = "RELIANCE";
    p1.exchange = "NSE";
    p1.product_type = "MIS";
    p1.quantity = 10;
    p1.avg_price = 1280;
    p1.ltp = 1300;
    p1.pnl = 200;
    p1.side = "LONG";

    BrokerPosition p2; // long 5 @ 1296 on Dhan
    p2.symbol = "RELIANCE";
    p2.exchange = "NSE";
    p2.product_type = "INTRADAY";
    p2.quantity = 5;
    p2.avg_price = 1296;
    p2.ltp = 1300;
    p2.pnl = 20;
    p2.side = "LONG";

    BrokerPosition p3; // unrelated symbol, only on Dhan
    p3.symbol = "TCS";
    p3.exchange = "NSE";
    p3.quantity = 2;
    p3.avg_price = 3400;
    p3.ltp = 3380;
    p3.pnl = -40;
    p3.side = "LONG";

    svc.ingest_positions("acct-fyers", {p1});
    svc.ingest_positions("acct-dhan", {p2, p3});
    // Ingest for an untracked account must be ignored.
    svc.ingest_positions("acct-alpaca", {p3});

    const auto positions = svc.positions();
    check("two merged position rows", positions.size() == 2);

    const AggRow* rel = find_row(positions, "RELIANCE");
    check("RELIANCE row exists", rel != nullptr);
    if (rel) {
        check("RELIANCE has 2 children", rel->children.size() == 2);
        check("RELIANCE total qty 15", approx(rel->total_qty, 15));
        // weighted avg = (10*1280 + 5*1296) / 15 = 1285.33
        check("RELIANCE weighted avg", approx(rel->w_avg_price, 1285.33, 0.01));
        check("RELIANCE pnl summed", approx(rel->pnl, 220));
    }

    // ── Short netting: long 10 in one account, short 10 in another ───────────
    BrokerPosition s1;
    s1.symbol = "INFY";
    s1.exchange = "NSE";
    s1.quantity = 10;
    s1.avg_price = 1500;
    s1.side = "LONG";
    BrokerPosition s2;
    s2.symbol = "INFY";
    s2.exchange = "NSE";
    s2.quantity = 10; // unsigned with SHORT side → counts as -10
    s2.avg_price = 1510;
    s2.side = "SHORT";
    svc.ingest_positions("acct-fyers", {p1, s1});
    svc.ingest_positions("acct-dhan", {p2, p3, s2});
    const AggRow* infy = find_row(svc.positions(), "INFY");
    check("INFY net qty 0 (long+short)", infy && approx(infy->total_qty, 0));
    check("INFY zero-qty avg guarded", infy && approx(infy->w_avg_price, 1505, 0.01));

    // ── Quote patch updates LTP + recomputes P&L ────────────────────────────
    BrokerQuote q;
    q.ltp = 1310;
    svc.ingest_quote("acct-fyers", "RELIANCE", q);
    rel = find_row(svc.positions(), "RELIANCE");
    if (rel) {
        check("patched parent LTP", approx(rel->ltp, 1310));
        // Fyers child: (1310-1280)*10 = 300; Dhan child untouched at 20.
        check("patched pnl sum", approx(rel->pnl, 320));
    }

    // ── Holdings merge + invested/current ───────────────────────────────────
    BrokerHolding h1;
    h1.symbol = "ITC";
    h1.exchange = "NSE";
    h1.quantity = 100;
    h1.avg_price = 400;
    h1.ltp = 410;
    h1.invested_value = 40000;
    h1.current_value = 41000;
    h1.pnl = 1000;
    h1.prev_close = 405;

    BrokerHolding h2;
    h2.symbol = "ITC";
    h2.exchange = "NSE";
    h2.quantity = 50;
    h2.avg_price = 380;
    h2.ltp = 0; // broker that doesn't hydrate LTP (e.g. Dhan) → derived fields guarded
    h2.invested_value = 0; // must derive qty*avg = 19000
    h2.current_value = 0;
    h2.pnl = 0;

    svc.ingest_holdings("acct-fyers", {h1});
    svc.ingest_holdings("acct-dhan", {h2});

    const auto holdings = svc.holdings();
    check("one merged holding row", holdings.size() == 1);
    const AggRow* itc = find_row(holdings, "ITC");
    if (itc) {
        check("ITC qty 150", approx(itc->total_qty, 150));
        check("ITC invested 59000", approx(itc->invested, 59000));
        // current: 41000 + 0 (no LTP for child 2 yet)
        check("ITC current 41000 pre-tick", approx(itc->current, 41000));
        // day pnl: (410-405)*100 = 500 from child 1 only
        check("ITC day pnl 500", approx(itc->day_pnl, 500));
    }

    // A quote tick for the Dhan account hydrates child 2.
    BrokerQuote q2;
    q2.ltp = 410;
    q2.close = 405;
    svc.ingest_quote("acct-dhan", "ITC", q2);
    itc = find_row(svc.holdings(), "ITC");
    if (itc) {
        check("ITC current hydrated by tick", approx(itc->current, 41000 + 50 * 410.0));
        check("ITC day pnl includes child 2", approx(itc->day_pnl, 500 + 50 * 5.0));
    }

    // ── Paper/live separation: switching to Paper must NOT show live rows ───
    // (test accounts have no paper portfolio, so the paper view is empty even
    // though the live caches above are populated — proving the views never mix.)
    svc.set_mode(UnifiedPortfolioService::Mode::Paper);
    check("paper view empty (no blending of live rows)",
          svc.positions().isEmpty() && svc.holdings().isEmpty());
    // Live ingestion while in paper mode must be cached but not displayed.
    BrokerPosition px_live;
    px_live.symbol = "SBIN";
    px_live.exchange = "NSE";
    px_live.quantity = 5;
    px_live.avg_price = 1000;
    px_live.side = "LONG";
    svc.ingest_positions("acct-fyers", {px_live});
    check("live ingest hidden in paper mode", svc.positions().isEmpty());
    svc.set_mode(UnifiedPortfolioService::Mode::Live);
    check("cached live rows appear after switch back", find_row(svc.positions(), "SBIN") != nullptr);

    svc.test_clear();
    std::printf(failures == 0 ? "portfolio-monitor selftest: ALL PASS\n"
                              : "portfolio-monitor selftest: %d FAILURE(S)\n",
                failures);
    return failures == 0 ? 0 : 1;
}

} // namespace fincept::trading
