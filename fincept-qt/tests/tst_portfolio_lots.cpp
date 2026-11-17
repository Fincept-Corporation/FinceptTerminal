// Unit tests for src/services/portfolio/PortfolioLots.{h,cpp} (pure, Qt Core only):
// tax-lot relief methods (FIFO, LIFO, HIFO, average cost, specific lot), fees and
// taxes in the basis, holding periods, splits, minor currency units, trade-date FX,
// unmatched sells, dividend entitlement at the ex-date, cash balances and the
// external flows behind TWR / MWR. Plus PortfolioPerformance (TWR flow timing,
// XIRR, windows) which consumes those flows.
//
// Sources: PortfolioLots.cpp, PortfolioPerformance.cpp, core/market/InstrumentCurrency.cpp,
// core/market/MarketSessions.cpp.

#include "services/portfolio/PortfolioLots.h"
#include "services/portfolio/PortfolioPerformance.h"

#include <QDate>
#include <QString>
#include <QTest>

#include <cmath>

using namespace fincept::portfolio;

namespace {

int tpl_seq = 0;

Transaction tpl_row(const char* date, const char* type, const char* sym, double qty, double price, double fees = 0,
                    double taxes = 0, const char* ccy = "", double fx = 0, const char* lot_id = "") {
    Transaction t;
    t.id = QStringLiteral("t%1").arg(++tpl_seq);
    t.portfolio_id = QStringLiteral("p");
    t.transaction_date = QString::fromLatin1(date);
    t.transaction_type = QString::fromLatin1(type);
    t.symbol = QString::fromLatin1(sym);
    t.quantity = qty;
    t.price = price;
    t.total_value = qty * price;
    t.fees = fees;
    t.taxes = taxes;
    t.currency = QString::fromLatin1(ccy);
    t.fx_rate = fx;
    t.lot_id = QString::fromLatin1(lot_id);
    t.created_at = QStringLiteral("%1T00:00:%2").arg(t.transaction_date.left(10)).arg(tpl_seq, 2, 10, QChar('0'));
    return t;
}

LedgerOptions tpl_opts(LotMethod m, const char* base = "USD") {
    LedgerOptions o;
    o.method = m;
    o.base_currency = QString::fromLatin1(base);
    o.as_of = QDate(2026, 1, 1);
    o.long_term_days = 365;
    o.instrument_currency = [](const QString& s) {
        if (s.endsWith(QLatin1String(".L")))
            return QStringLiteral("GBp");
        return QStringLiteral("USD");
    };
    return o;
}

/// Three buys at 10, 30, 20 (fees 1 each) then one sell of 15 @ 40 (fee 3).
QVector<Transaction> tpl_three_buys_one_sell() {
    return {tpl_row("2023-01-02", "BUY", "AAA", 10, 10, 1), tpl_row("2023-06-01", "BUY", "AAA", 10, 30, 1),
            tpl_row("2025-03-01", "BUY", "AAA", 10, 20, 1), tpl_row("2025-06-02", "SELL", "AAA", 15, 40, 3)};
}

bool tpl_near(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) <= eps * std::max(1.0, std::abs(b));
}

} // namespace

class TstPortfolioLots : public QObject {
    Q_OBJECT

  private slots:
    void method_keys();
    void fifo_relief_and_fees();
    void lifo_relief();
    void hifo_relief();
    void average_cost_relief();
    void specific_lot_relief();
    void holding_period_threshold();
    void split_adjusts_lots_not_cost();
    void minor_units_and_trade_fx();
    void unknown_fx_is_absent_not_zero();
    void unmatched_sell_realises_nothing();
    void dividend_entitlement_at_ex_date();
    void cash_and_flows_untracked();
    void cash_and_flows_tracked();
    void newest_first_input_order();

    void twr_trade_day_is_exact();
    void twr_outflow_end_of_day_and_suspect_kept();
    void twr_no_capital_is_invalid();
    void xirr_known_answer();
    void windows_and_mwr();
};

void TstPortfolioLots::method_keys() {
    QCOMPARE(lot_method_from_key("fifo"), LotMethod::Fifo);
    QCOMPARE(lot_method_from_key("LIFO"), LotMethod::Lifo);
    QCOMPARE(lot_method_from_key("hifo"), LotMethod::Hifo);
    QCOMPARE(lot_method_from_key("wac"), LotMethod::AverageCost);
    QCOMPARE(lot_method_from_key("Specific"), LotMethod::SpecificLot);
    QCOMPARE(lot_method_from_key("bogus"), LotMethod::Fifo);
    QCOMPARE(lot_method_key(LotMethod::AverageCost), QStringLiteral("AVERAGE"));
}

void TstPortfolioLots::fifo_relief_and_fees() {
    const auto r = replay_ledger(tpl_three_buys_one_sell(), tpl_opts(LotMethod::Fifo));
    const auto& p = r.positions.value("AAA");
    QCOMPARE(p.quantity, 15.0);
    // Sold: all of lot 1 (cost 101) + 5 of lot 2 (cost 301 × 5/10 = 150.5). Proceeds 15×40 − 3 = 597.
    QCOMPARE(r.realized.size(), 2);
    QVERIFY(tpl_near(r.realized[0].cost_local, 101.0));
    QVERIFY(tpl_near(r.realized[1].cost_local, 150.5));
    QVERIFY(tpl_near(r.realized[0].proceeds_local + r.realized[1].proceeds_local, 597.0));
    QVERIFY(tpl_near(r.realized_base, 597.0 - 251.5));
    // Remaining: 5 of lot 2 (150.5) + lot 3 (201).
    QVERIFY(tpl_near(p.cost_local, 351.5));
    QCOMPARE(p.lots.size(), 2);
    QVERIFY(tpl_near(r.fees_base, 6.0));
}

void TstPortfolioLots::lifo_relief() {
    const auto r = replay_ledger(tpl_three_buys_one_sell(), tpl_opts(LotMethod::Lifo));
    // Sold: lot 3 (201) + 5 of lot 2 (150.5).
    QVERIFY(tpl_near(r.realized_base, 597.0 - 351.5));
    QVERIFY(tpl_near(r.positions.value("AAA").cost_local, 101.0 + 150.5));
}

void TstPortfolioLots::hifo_relief() {
    const auto r = replay_ledger(tpl_three_buys_one_sell(), tpl_opts(LotMethod::Hifo));
    // Highest unit cost first: lot 2 (30.1) fully, then 5 of lot 3 (20.1) = 301 + 100.5.
    QVERIFY(tpl_near(r.realized_base, 597.0 - 401.5));
    QVERIFY(tpl_near(r.positions.value("AAA").cost_local, 101.0 + 100.5));
}

void TstPortfolioLots::average_cost_relief() {
    const auto r = replay_ledger(tpl_three_buys_one_sell(), tpl_opts(LotMethod::AverageCost));
    // Pool 603 / 30 = 20.1 per unit; 15 sold → cost 301.5, remaining 301.5.
    QVERIFY(tpl_near(r.realized_base, 597.0 - 301.5));
    QVERIFY(tpl_near(r.positions.value("AAA").cost_local, 301.5));
    // Holding period of the consumed units is FIFO: lot 1 (2023) long-term, part of lot 2 long-term.
    for (const auto& rl : r.realized)
        QVERIFY(rl.long_term);
}

void TstPortfolioLots::specific_lot_relief() {
    auto txns = tpl_three_buys_one_sell();
    txns[3].lot_id = txns[2].id; // sell from the 2025 lot first
    const auto r = replay_ledger(txns, tpl_opts(LotMethod::SpecificLot));
    // Lot 3 (10 units, 201) then FIFO: 5 of lot 1 (50.5).
    QVERIFY(tpl_near(r.realized_base, 597.0 - 251.5));
    QCOMPARE(r.realized.first().lot_id, txns[2].id);
    QVERIFY(!r.realized.first().long_term); // bought 2025-03-01, sold 2025-06-02
    // Unknown lot id: warned and sold FIFO.
    txns[3].lot_id = QStringLiteral("nope");
    const auto r2 = replay_ledger(txns, tpl_opts(LotMethod::SpecificLot));
    QVERIFY(!r2.warnings.isEmpty());
    QVERIFY(tpl_near(r2.realized_base, 597.0 - 251.5));
}

void TstPortfolioLots::holding_period_threshold() {
    QVector<Transaction> t = {tpl_row("2024-01-01", "BUY", "B", 1, 100), tpl_row("2024-12-31", "SELL", "B", 1, 110)};
    auto o = tpl_opts(LotMethod::Fifo);
    auto r = replay_ledger(t, o);
    QCOMPARE(r.realized.first().holding_days, 365);
    QVERIFY(!r.realized.first().long_term); // must be MORE than 365 days
    QVERIFY(tpl_near(r.realized_short_base, 10.0));
    o.long_term_days = 180;
    r = replay_ledger(t, o);
    QVERIFY(r.realized.first().long_term);
    QVERIFY(tpl_near(r.realized_long_base, 10.0));
    // Open lots carry their holding period to as_of.
    const auto open = replay_ledger({tpl_row("2024-01-01", "BUY", "B", 1, 100)}, tpl_opts(LotMethod::Fifo));
    QCOMPARE(open.positions.value("B").lots.first().holding_days, static_cast<int>(QDate(2024, 1, 1).daysTo(QDate(2026, 1, 1))));
    QVERIFY(open.positions.value("B").lots.first().long_term);
}

void TstPortfolioLots::split_adjusts_lots_not_cost() {
    QVector<Transaction> t = {tpl_row("2024-01-02", "BUY", "NV", 5, 900, 2), tpl_row("2024-06-10", "SPLIT", "NV", 10, 0),
                              tpl_row("2024-07-01", "SELL", "NV", 20, 120)};
    const auto r = replay_ledger(t, tpl_opts(LotMethod::Fifo));
    const auto& p = r.positions.value("NV");
    QCOMPARE(p.quantity, 30.0);
    // Cost 4502 for 50 post-split units; 20 sold → cost 1800.8.
    QVERIFY(tpl_near(r.realized.first().cost_local, 4502.0 * 20.0 / 50.0));
    QVERIFY(tpl_near(p.cost_local, 4502.0 * 30.0 / 50.0));
    QCOMPARE(p.lots.first().original_quantity, 50.0);
    QCOMPARE(quantity_held_before(t, "NV", QDate(2024, 6, 10)), 50.0); // split effective on its date
    QCOMPARE(quantity_held_before(t, "NV", QDate(2024, 6, 9)), 5.0);
}

void TstPortfolioLots::minor_units_and_trade_fx() {
    // 1000 VOD.L at 70.5p, fee £5 recorded in pence (500), trade FX 1.25 USD per GBP.
    QVector<Transaction> t = {tpl_row("2024-07-01", "BUY", "VOD.L", 1000, 70.5, 500, 0, "GBp", 1.25)};
    const auto r = replay_ledger(t, tpl_opts(LotMethod::Fifo));
    const auto& p = r.positions.value("VOD.L");
    QCOMPARE(p.currency, QStringLiteral("GBP"));
    QVERIFY(tpl_near(p.cost_local, 705.0 + 5.0));
    QVERIFY(tpl_near(p.cost_base, 710.0 * 1.25));
    QVERIFY(tpl_near(r.cash.value("GBP"), -710.0));
    // A legacy row (no currency) takes the instrument's quoted currency: pence too.
    const auto r2 = replay_ledger({tpl_row("2024-07-01", "BUY", "VOD.L", 1000, 70.5)}, tpl_opts(LotMethod::Fifo, "GBP"));
    QVERIFY(tpl_near(r2.positions.value("VOD.L").cost_local, 705.0));
    QVERIFY(tpl_near(r2.positions.value("VOD.L").cost_base, 705.0)); // base GBP: rate 1
}

void TstPortfolioLots::unknown_fx_is_absent_not_zero() {
    QVector<Transaction> t = {tpl_row("2024-01-02", "BUY", "SAP.DE", 10, 100, 0, 0, "EUR"),
                              tpl_row("2024-06-03", "SELL", "SAP.DE", 5, 120, 0, 0, "EUR")};
    auto o = tpl_opts(LotMethod::Fifo);
    auto r = replay_ledger(t, o);
    QVERIFY(std::isnan(r.positions.value("SAP.DE").cost_base));
    QVERIFY(std::isnan(r.realized.first().gain_base));
    QVERIFY(!r.realized_base_complete);
    QVERIFY(tpl_near(r.realized.first().gain_local, 100.0));
    QVERIFY(std::isnan(r.flows.first().amount_base));
    // With a daily FX history the base figures appear.
    o.historical_fx = [](const QString& ccy, const QDate& d) -> std::optional<double> {
        if (ccy != QLatin1String("EUR"))
            return std::nullopt;
        return d < QDate(2024, 3, 1) ? 1.10 : 1.08;
    };
    r = replay_ledger(t, o);
    QVERIFY(tpl_near(r.realized.first().gain_base, 5 * 120 * 1.08 - 5 * 100 * 1.10));
    QVERIFY(r.realized_base_complete);
}

void TstPortfolioLots::unmatched_sell_realises_nothing() {
    QVector<Transaction> t = {tpl_row("2024-01-02", "BUY", "X", 5, 10), tpl_row("2024-02-02", "SELL", "X", 8, 12)};
    const auto r = replay_ledger(t, tpl_opts(LotMethod::Fifo));
    QCOMPARE(r.positions.value("X").unmatched_sell_quantity, 3.0);
    QVERIFY(tpl_near(r.realized_base, 5 * 12 * (5.0 / 8.0) * 8.0 / 5.0 - 50.0)); // only the 5 matched units
    QVERIFY(!r.warnings.isEmpty());
}

void TstPortfolioLots::dividend_entitlement_at_ex_date() {
    QVector<Transaction> t = {tpl_row("2024-01-02", "BUY", "D", 100, 10), tpl_row("2024-03-15", "BUY", "D", 50, 11),
                              tpl_row("2024-03-20", "SELL", "D", 30, 12)};
    QCOMPARE(quantity_held_before(t, "D", QDate(2024, 3, 15)), 100.0); // the buy ON the ex-date is not entitled
    QCOMPARE(quantity_held_before(t, "D", QDate(2024, 3, 16)), 150.0);
    QCOMPARE(quantity_held_before(t, "D", QDate(2024, 4, 1)), 120.0);
    // Net dividend (gross − withholding) is income and, untracked, a flow out.
    t.append(tpl_row("2024-04-01", "DIVIDEND", "D", 120, 0.5, 0, 9));
    const auto r = replay_ledger(t, tpl_opts(LotMethod::Fifo));
    QVERIFY(tpl_near(r.dividends_base, 60.0 - 9.0));
    QVERIFY(tpl_near(r.withholding_base, 9.0));
    QVERIFY(tpl_near(r.flows.last().amount_base, -51.0));
}

void TstPortfolioLots::cash_and_flows_untracked() {
    QVector<Transaction> t = {tpl_row("2024-01-02", "BUY", "A", 10, 100, 5), tpl_row("2024-02-01", "SELL", "A", 4, 110, 2),
                              tpl_row("2024-03-01", "FEE", "", 12, 1)};
    const auto r = replay_ledger(t, tpl_opts(LotMethod::Fifo));
    QCOMPARE(r.flows.size(), 3);
    QVERIFY(tpl_near(r.flows[0].amount_base, 1005.0));  // paid in incl. the fee
    QVERIFY(tpl_near(r.flows[1].amount_base, -438.0));  // proceeds out net of the fee
    QVERIFY(tpl_near(r.flows[2].amount_base, 12.0));    // stand-alone fee paid from outside
    QVERIFY(tpl_near(r.cash.value("USD"), -1005.0 + 438.0 - 12.0));
}

void TstPortfolioLots::cash_and_flows_tracked() {
    QVector<Transaction> t = {tpl_row("2024-01-02", "DEPOSIT", "", 2000, 1), tpl_row("2024-01-03", "BUY", "A", 10, 100, 5),
                              tpl_row("2024-02-01", "INTEREST", "", 3, 1, 0, 1), tpl_row("2024-03-01", "WITHDRAWAL", "", 500, 1)};
    auto o = tpl_opts(LotMethod::Fifo);
    o.track_cash = true;
    const auto r = replay_ledger(t, o);
    QCOMPARE(r.flows.size(), 2); // only deposit and withdrawal cross the boundary
    QVERIFY(tpl_near(r.flows[0].amount_base, 2000.0));
    QVERIFY(tpl_near(r.flows[1].amount_base, -500.0));
    QVERIFY(tpl_near(r.cash.value("USD"), 2000.0 - 1005.0 + 2.0 - 500.0));
    QVERIFY(tpl_near(r.interest_base, 2.0));
}

void TstPortfolioLots::newest_first_input_order() {
    auto t = tpl_three_buys_one_sell();
    QVector<Transaction> rev(t.rbegin(), t.rend());
    const auto a = replay_ledger(t, tpl_opts(LotMethod::Fifo));
    const auto b = replay_ledger(rev, tpl_opts(LotMethod::Fifo));
    QVERIFY(tpl_near(a.realized_base, b.realized_base));
    QVERIFY(tpl_near(a.positions.value("AAA").cost_local, b.positions.value("AAA").cost_local));
}

// ── PortfolioPerformance ─────────────────────────────────────────────────────

namespace {
PortfolioSnapshot tpl_snap(const char* d, double v) {
    PortfolioSnapshot s;
    s.snapshot_date = QString::fromLatin1(d);
    s.total_value = v;
    return s;
}
ExternalFlow tpl_flow(const char* d, double a) {
    ExternalFlow f;
    f.date = QDate::fromString(QString::fromLatin1(d), Qt::ISODate);
    f.amount_base = a;
    return f;
}
} // namespace

void TstPortfolioLots::twr_trade_day_is_exact() {
    // Day 1: 10 units at 100 (V=1000). Day 2: buy 10 more at 105 (flow 1050), close 110 → V=2200.
    const auto twr = build_twr({tpl_snap("2024-01-02", 1000), tpl_snap("2024-01-03", 2200)},
                               {tpl_flow("2024-01-03", 1050)});
    QCOMPARE(twr.size(), 2);
    QVERIFY(twr[1].valid);
    // Start-of-day inflow: 2200 / (1000 + 1050) − 1.
    QVERIFY(tpl_near(twr[1].period_return, 2200.0 / 2050.0 - 1.0));
    // First BUY into an empty book: V0 = 0, returns close / trade price − 1.
    const auto first = build_twr({tpl_snap("2024-01-01", 0), tpl_snap("2024-01-02", 1100)}, {tpl_flow("2024-01-02", 1000)});
    QVERIFY(tpl_near(first[1].period_return, 0.10));
}

void TstPortfolioLots::twr_outflow_end_of_day_and_suspect_kept() {
    // Sell half at 120 (proceeds 600) from V0=1000, the rest closes at 600 → (600 + 600)/1000 − 1.
    const auto twr = build_twr({tpl_snap("2024-01-02", 1000), tpl_snap("2024-01-03", 600)}, {tpl_flow("2024-01-03", -600)});
    QVERIFY(tpl_near(twr[1].period_return, 0.20));
    // An unrecorded deposit looks like a 100 % gain: kept and flagged, not dropped.
    const auto j = build_twr({tpl_snap("2024-01-02", 1000), tpl_snap("2024-01-03", 2000)}, {});
    QVERIFY(j[1].valid);
    QVERIFY(j[1].suspect);
    QVERIFY(tpl_near(j[1].cumulative, 1.0));
}

void TstPortfolioLots::twr_no_capital_is_invalid() {
    const auto twr = build_twr({tpl_snap("2024-01-02", 0), tpl_snap("2024-01-03", 0)}, {});
    QVERIFY(!twr[1].valid);
    // A flow of unknown FX makes the period unmeasurable.
    ExternalFlow f = tpl_flow("2024-01-03", 0);
    f.amount_base = kAbsent;
    const auto u = build_twr({tpl_snap("2024-01-02", 1000), tpl_snap("2024-01-03", 1500)}, {f});
    QVERIFY(!u[1].valid);
}

void TstPortfolioLots::xirr_known_answer() {
    // −1000 today, +1100 in exactly one (365.25-day) year → 10 %.
    const QDate d0(2020, 1, 1);
    const auto r = xirr({{d0, -1000.0}, {d0.addDays(365), 1100.0}});
    QVERIFY(r.has_value());
    QVERIFY(std::abs(*r - (std::pow(1.1, 365.25 / 365.0) - 1.0)) < 1e-6);
    QVERIFY(!xirr({{d0, 100.0}, {d0.addDays(10), 50.0}}).has_value()); // no sign change
}

void TstPortfolioLots::windows_and_mwr() {
    QCOMPARE(perf_window_base_date(PerfWindow::MTD, QDate(2026, 3, 15)), QDate(2026, 2, 28));
    QCOMPARE(perf_window_base_date(PerfWindow::QTD, QDate(2026, 5, 15)), QDate(2026, 3, 31));
    QCOMPARE(perf_window_base_date(PerfWindow::YTD, QDate(2026, 5, 15)), QDate(2025, 12, 31));
    QCOMPARE(perf_window_from_key("3y"), PerfWindow::Y3);
    // Two years of +0.05 % per weekday, no flows: TWR == MWR, annualised only ≥ 1 year.
    PerfInputs in;
    double v = 1000;
    for (QDate d(2024, 1, 1); d <= QDate(2025, 12, 31); d = d.addDays(1)) {
        if (d.dayOfWeek() > 5)
            continue;
        in.snapshots.append(tpl_snap(d.toString(Qt::ISODate).toLatin1().constData(), v));
        v *= 1.0005;
    }
    in.window = PerfWindow::All;
    ComputedMetrics m;
    compute_realised_metrics(in, m);
    QVERIFY(m.total_return.has_value());
    QVERIFY(m.annualized_return.has_value());
    QVERIFY(m.mwr.has_value());
    QVERIFY(std::abs(*m.mwr - *m.annualized_return) < 1e-3);
    QVERIFY(m.realised_volatility.has_value() && *m.realised_volatility < 1e-6);
    QCOMPARE(m.perf_suspect, 0);
    ComputedMetrics mtd;
    in.window = PerfWindow::MTD;
    compute_realised_metrics(in, mtd);
    QCOMPARE(mtd.perf_start, QStringLiteral("2025-11-28")); // last close of November
    QVERIFY(!mtd.annualized_return.has_value());
    QVERIFY(mtd.mwr_period.has_value());
}

QTEST_GUILESS_MAIN(TstPortfolioLots)
#include "tst_portfolio_lots.moc"
