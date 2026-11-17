// Unit tests for src/algo_engine/PerformanceMetrics.{h,cpp}
//
// PerformanceMetrics.cpp includes only its own header (Qt Core: QDate,
// QDateTime, QTimeZone, QJsonObject). One extra translation unit.
//
// These pin the definitions the Algo Trading backtest report AND the live
// deployment monitor share: sample (n-1) volatility, Sharpe / Sortino with a
// risk-free rate, calendar-time CAGR, Calmar on CAGR, drawdown depth and
// duration on the full-resolution curve, exchange-day monthly buckets and the
// trade statistics (expectancy is the mean net P&L per trade).

#include "algo_engine/PerformanceMetrics.h"

#include <QDateTime>
#include <QTest>
#include <QTimeZone>

#include <cmath>

namespace perf = fincept::algo::perf;

namespace {

qint64 ms_of(int y, int m, int d, int h = 0) {
    return QDateTime(QDate(y, m, d), QTime(h, 0), QTimeZone::UTC).toMSecsSinceEpoch();
}

QVector<perf::Point> curve_of(const QVector<double>& values, qint64 t0, qint64 step_ms) {
    QVector<perf::Point> out;
    for (int i = 0; i < values.size(); ++i)
        out.append({t0 + i * step_ms, values[i], 0});
    return out;
}

} // namespace

class TstAlgoPerformanceMetrics : public QObject {
    Q_OBJECT

  private slots:
    void day_keys() {
        QCOMPARE(perf::day_key(QDate(2026, 11, 16)), 20261116);
        QCOMPARE(perf::day_from_key(20261116), QDate(2026, 11, 16));
        QCOMPARE(perf::day_key(QDate()), 0);
        // A stamped exchange date wins over the UTC date of the instant
        // (21:00 UTC on the 15th is the 16th in Tokyo).
        perf::Point p{ms_of(2026, 11, 15, 21), 1.0, 20261116};
        QCOMPARE(perf::point_date(p), QDate(2026, 11, 16));
        p.day = 0;
        QCOMPARE(perf::point_date(p), QDate(2026, 11, 15));
    }

    void drawdown_depth_and_duration() {
        // 100 → 120 (peak) → 90 (trough, -25 %) → 110 → 125 (recovered) → 120
        const auto c = curve_of({100, 120, 90, 110, 125, 120}, ms_of(2026, 1, 1), 86400000);
        const auto d = perf::drawdown(c);
        QCOMPARE(d.max_pct, 25.0);
        QCOMPARE(d.max_abs, 30.0);
        QCOMPARE(d.peak, 1);
        QCOMPARE(d.trough, 2);
        QCOMPARE(d.recovery, 4);
        QCOMPARE(d.longest_points, 3); // index 1 → 4
        QCOMPARE(d.longest_days, 3.0);
        const auto uw = perf::underwater_pct(c);
        QCOMPARE(uw.size(), 6);
        QCOMPARE(uw[2], -25.0);
        QCOMPARE(uw[4], 0.0);
        QVERIFY(uw[5] < 0.0);
    }

    void drawdown_open_at_end() {
        const auto c = curve_of({100, 90, 80, 85}, ms_of(2026, 1, 1), 86400000);
        const auto d = perf::drawdown(c);
        QCOMPARE(d.max_pct, 20.0);
        QCOMPARE(d.recovery, -1);
        QCOMPARE(d.longest_points, 3); // still under water at the last point
    }

    void drawdown_of_a_pnl_curve() {
        // Live P&L curves start at 0: the currency fall is what counts.
        const auto c = curve_of({0, 10, -490, 1000, 400}, ms_of(2026, 1, 1), 60000);
        const auto d = perf::drawdown(c);
        QCOMPARE(d.max_abs, 600.0);
    }

    void ratios_sample_std_and_risk_free() {
        const QVector<double> r = {0.01, -0.005, 0.02, 0.0, 0.005};
        const auto x = perf::ratios(r, 252, 0.0);
        QCOMPARE(x.n, 5);
        double mean = 0;
        for (double v : r)
            mean += v;
        mean /= r.size();
        double ss = 0;
        for (double v : r)
            ss += (v - mean) * (v - mean);
        const double sd = std::sqrt(ss / 4.0); // n - 1
        QVERIFY(std::abs(x.stdev - sd) < 1e-12);
        QVERIFY(std::abs(x.sharpe - mean / sd * std::sqrt(252.0)) < 1e-9);
        QVERIFY(std::abs(x.ann_vol_pct - sd * std::sqrt(252.0) * 100.0) < 1e-9);
        // A positive risk-free rate lowers Sharpe.
        const auto y = perf::ratios(r, 252, 5.0);
        QVERIFY(y.sharpe < x.sharpe);
        // Sortino uses downside deviation over ALL periods.
        const double rf_p = 0.0;
        double down = 0;
        for (double v : r)
            if (v - rf_p < 0)
                down += (v - rf_p) * (v - rf_p);
        const double dd = std::sqrt(down / r.size());
        QVERIFY(std::abs(x.sortino - mean / dd * std::sqrt(252.0)) < 1e-9);
        // Too few samples → zeros, not NaN.
        const auto z = perf::ratios({0.01}, 252, 0.0);
        QCOMPARE(z.sharpe, 0.0);
    }

    void sharpe_units_do_not_depend_on_the_bar_size() {
        // The same daily equity path seen through 78 five-minute points a day (moves only at
        // each day's last bar) and through one point a day must report the same Sharpe:
        // an intraday run is measured on its trading-day closes × √trading days.
        const QVector<double> daily_ret = {0.004, -0.002, 0.003, -0.001, 0.0025, -0.0015, 0.001, 0.002, -0.003,
                                           0.0045, -0.0005, 0.0015, -0.002, 0.003, 0.001, -0.001, 0.002,
                                           -0.0025, 0.0035, 0.0005, -0.001, 0.002};
        QVector<perf::Point> daily, intraday;
        double eq = 100000;
        for (int d = 0; d < daily_ret.size(); ++d) {
            const int key = 20260101 + d;
            for (int b = 0; b < 78; ++b) {
                const bool last = b == 77;
                if (last)
                    eq *= 1.0 + daily_ret[d];
                intraday.append({ms_of(2026, 1, 1) + qint64(d) * 86400000 + qint64(b) * 300000, eq, key});
            }
            daily.append({ms_of(2026, 1, 1) + qint64(d) * 86400000, eq, key});
        }
        const auto day_basis = perf::ratio_basis(daily, false, 252, 252);
        const auto bar_basis = perf::ratio_basis(intraday, true, 252 * 78, 252);
        QVERIFY(!day_basis.daily);
        QVERIFY(bar_basis.daily);
        QCOMPARE(bar_basis.periods_per_year, 252.0);
        QCOMPARE(bar_basis.returns.size(), day_basis.returns.size());
        const auto a = perf::ratios(day_basis.returns, day_basis.periods_per_year, 0.0);
        const auto b = perf::ratios(bar_basis.returns, bar_basis.periods_per_year, 0.0);
        QVERIFY(std::abs(a.sharpe - b.sharpe) < 1e-9);
        QVERIFY(std::abs(a.ann_vol_pct - b.ann_vol_pct) < 1e-9);
        // The annualised volatility is a yearly figure, not a per-bar one: ~0.24 % daily
        // deviation x √252 ≈ 3.8 % — never the per-bar deviation x √(bars per year) of a
        // series whose 77 bars a day do not move.
        QVERIFY(b.ann_vol_pct > 2.0 && b.ann_vol_pct < 6.0);
        // A short intraday run (fewer than 20 trading days) keeps its bar returns.
        const auto short_basis = perf::ratio_basis(intraday.mid(0, 78 * 5), true, 252 * 78, 252);
        QVERIFY(!short_basis.daily);
        QCOMPARE(short_basis.periods_per_year, 252.0 * 78);
        // Daily bars annualise with √252: mean / sd × √252.
        QVERIFY(std::abs(a.sharpe - a.mean / a.stdev * std::sqrt(252.0)) < 1e-9);
    }

    void cagr_is_calendar_time() {
        // Doubling over two calendar years ≈ 41.42 % a year.
        const double c = perf::cagr_pct(100, 200, ms_of(2024, 1, 1), ms_of(2026, 1, 1));
        QVERIFY(std::abs(c - (std::sqrt(2.0) - 1.0) * 100.0) < 0.05);
        QVERIFY(std::isnan(perf::cagr_pct(0, 200, ms_of(2024, 1, 1), ms_of(2026, 1, 1))));
        QCOMPARE(perf::cagr_pct(100, -5, ms_of(2024, 1, 1), ms_of(2026, 1, 1)), -100.0);
    }

    void periods_per_year_from_spacing() {
        // Daily points over one calendar year → ~365 per year.
        QVector<perf::Point> c;
        for (int i = 0; i <= 365; ++i)
            c.append({ms_of(2025, 1, 1) + qint64(i) * 86400000, 100.0 + i, 0});
        QVERIFY(std::abs(perf::estimate_periods_per_year(c) - 365.0 * 365.25 / 365.0) < 0.5);
        QCOMPARE(perf::estimate_periods_per_year({}), 0.0);
    }

    void monthly_returns_by_exchange_day() {
        // The last point sits at 20:00 UTC on Jan 31, which is Feb 1 at the
        // exchange (stamped day): it must count towards February.
        QVector<perf::Point> c;
        c.append({ms_of(2026, 1, 5), 100.0, 20260105});
        c.append({ms_of(2026, 1, 30), 110.0, 20260130});
        c.append({ms_of(2026, 1, 31, 20), 121.0, 20260201});
        const auto m = perf::monthly_returns(c, 100.0);
        QCOMPARE(m.size(), 2);
        QCOMPARE(m[0].first, 202601);
        QVERIFY(std::abs(m[0].second - 10.0) < 1e-9);
        QCOMPARE(m[1].first, 202602);
        QVERIFY(std::abs(m[1].second - 10.0) < 1e-9);
    }

    void daily_close_keeps_last_point_of_each_day() {
        QVector<perf::Point> c;
        c.append({ms_of(2026, 3, 2, 10), 1.0, 20260302});
        c.append({ms_of(2026, 3, 2, 15), 2.0, 20260302});
        c.append({ms_of(2026, 3, 3, 10), 3.0, 20260303});
        const auto d = perf::daily_close(c);
        QCOMPARE(d.size(), 2);
        QCOMPARE(d[0].v, 2.0);
        QCOMPARE(d[1].v, 3.0);
    }

    void trade_stats_definitions() {
        QVector<perf::ClosedTrade> t;
        auto add = [&t](double pnl, double ret, int bars) {
            perf::ClosedTrade c;
            c.net_pnl = pnl;
            c.ret_pct = ret;
            c.bars_held = bars;
            c.fees = 1.0;
            t.append(c);
        };
        add(100, 2.0, 5);
        add(-50, -1.0, 3);
        add(200, 4.0, 10);
        add(-50, -1.0, 2);
        add(0, 0.0, 1);
        const auto s = perf::trade_stats(t);
        QCOMPARE(s.trades, 5);
        QCOMPARE(s.wins, 2);
        QCOMPARE(s.losses, 2);
        QCOMPARE(s.win_rate_pct, 40.0);
        QCOMPARE(s.gross_profit, 300.0);
        QCOMPARE(s.gross_loss, 100.0);
        QCOMPARE(s.profit_factor, 3.0);
        QVERIFY(!s.profit_factor_infinite);
        QCOMPARE(s.avg_win, 150.0);
        QCOMPARE(s.avg_loss, 50.0);
        QCOMPARE(s.payoff_ratio, 3.0);
        // Expectancy = mean net P&L = win% × avg win − loss% × avg loss.
        QCOMPARE(s.expectancy, 40.0);
        QVERIFY(std::abs(s.expectancy - (0.4 * 150.0 - 0.4 * 50.0)) < 1e-9);
        QCOMPARE(s.expectancy_pct, 0.8);
        QCOMPARE(s.avg_bars_held, 4.2);
        QCOMPARE(s.total_fees, 5.0);
        QCOMPARE(s.max_consecutive_wins, 1);
        QCOMPARE(s.max_consecutive_losses, 1);
        QCOMPARE(s.largest_win, 200.0);
        QCOMPARE(s.largest_loss, 50.0);

        const auto none = perf::trade_stats({});
        QCOMPARE(none.trades, 0);
        QCOMPARE(none.expectancy, 0.0);

        QVector<perf::ClosedTrade> only_wins(1);
        only_wins[0].net_pnl = 10;
        const auto w = perf::trade_stats(only_wins);
        QVERIFY(w.profit_factor_infinite);
        const auto j = perf::trade_stats_json(w);
        QVERIFY(j.value("profit_factor").isNull());
        QVERIFY(j.value("profit_factor_infinite").toBool());
    }
};

QTEST_GUILESS_MAIN(TstAlgoPerformanceMetrics)
#include "tst_algo_performance_metrics.moc"
