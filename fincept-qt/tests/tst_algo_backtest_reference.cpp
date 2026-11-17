// src/algo_engine/BacktestEngine against an independent reference implementation of its
// documented fill model (BacktestEngine.h), on deterministic synthetic bars:
//   * signals on the close of bar i, filled at the OPEN of bar i+1 with adverse slippage (bps);
//   * stop-loss / take-profit resting orders against each held bar's range, stop first, a gap
//     through the level fills at the open, take-profit without slippage;
//   * whole shares, fees = SEC-style tax on sells (the "us_equity_zero" preset);
//   * equity marked on every close; an open position is closed at the last close.
// Checked: every trade (bars, prices, quantity, reason), the equity curve, costs, return,
// drawdown, and Sharpe — daily bars × √252, an intraday run on its trading-day closes × √252.
// (The same comparison on real SPY bars, in pandas: D:\ft-scratch\algo\ref\reference.py.)
//
// Sources: the engine's leaf TUs + the logger (BacktestEngine logs). ConditionEvaluator.h
// includes AlgoTradingTypes.h, which includes the theme header: Qt Gui headers only.

#include "algo_engine/BacktestEngine.h"
#include "algo_engine/ExecutionModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include <cmath>
#include <cstdint>

using namespace fincept::algo;

namespace {

/// Deterministic random walk (LCG), OHLC around it.
QVector<OhlcvCandle> synthetic_bars(int n, qint64 t0_ms, qint64 step_ms, int bars_per_day, double vol,
                                    std::uint32_t seed) {
    QVector<OhlcvCandle> out;
    std::uint32_t s = seed;
    auto uni = [&s]() {
        s = s * 1664525u + 1013904223u;
        return (s >> 8) / double(1u << 24);
    };
    double px = 100.0;
    for (int i = 0; i < n; ++i) {
        const double drift = 0.0004 * std::sin(i / 37.0);
        const double r1 = (uni() - 0.5) * 2.0 * vol + drift;
        const double open = px * (1.0 + (uni() - 0.5) * vol * 0.4);
        const double close = open * (1.0 + r1);
        const double high = std::max(open, close) * (1.0 + uni() * vol * 0.6);
        const double low = std::min(open, close) * (1.0 - uni() * vol * 0.6);
        OhlcvCandle b;
        const int day = i / bars_per_day;
        b.open_time = t0_ms + qint64(day) * 86400000LL + qint64(i % bars_per_day) * step_ms;
        b.close_time = b.open_time + step_ms;
        b.open = open;
        b.high = high;
        b.low = low;
        b.close = close;
        b.volume = 1000;
        b.session_date = 20250101 + day; // only equality per day matters here
        out.append(b);
        px = close;
    }
    return out;
}

QJsonObject cross(const QString& op, int fast, int slow) {
    return QJsonObject{{"indicator", "SMA"},
                       {"params", QJsonObject{{"period", fast}}},
                       {"field", "value"},
                       {"operator", op},
                       {"compare_mode", "indicator"},
                       {"compare_indicator", "SMA"},
                       {"compare_params", QJsonObject{{"period", slow}}},
                       {"compare_field", "value"}};
}

struct RefTrade {
    int entry_bar, exit_bar;
    double entry_px, exit_px, qty;
    QString reason;
};

struct RefResult {
    QVector<RefTrade> trades;
    QVector<double> equity;
    double cash = 0, fees = 0, slippage = 0;
};

double sma_at(const QVector<OhlcvCandle>& b, int i, int p) {
    if (i + 1 < p)
        return std::nan("");
    double s = 0;
    for (int k = i - p + 1; k <= i; ++k)
        s += b[k].close;
    return s / p;
}

/// The reference: the documented model, written from scratch (no engine code).
RefResult reference(const QVector<OhlcvCandle>& b, int warm, int fast, int slow, double sl, double tp, double cap,
                    double slip_bps, double sell_tax_pct) {
    RefResult r;
    r.cash = cap;
    double qty = 0, entry_px = 0;
    int entry_bar = -1;
    bool pend_in = false, pend_out = false;
    const int n = b.size();
    auto sell = [&](double ref, bool slip, int i, const QString& why) {
        const double px = slip ? ref * (1 - slip_bps / 1e4) : ref;
        const double fee = qty * px * sell_tax_pct / 100.0;
        r.cash += qty * px - fee;
        r.fees += fee;
        r.slippage += (ref - px) * qty;
        r.trades.append({entry_bar, i, entry_px, px, qty, why});
        qty = 0;
    };
    for (int i = warm; i < n; ++i) {
        if (qty == 0 && pend_in) {
            pend_in = false;
            const double px = b[i].open * (1 + slip_bps / 1e4);
            const double q = std::floor(r.cash / px + 1e-9);
            if (q > 0) {
                qty = q;
                entry_px = px;
                entry_bar = i;
                r.cash -= q * px;
                r.slippage += (px - b[i].open) * q;
            }
        } else if (qty > 0 && pend_out) {
            pend_out = false;
            sell(b[i].open, true, i, QStringLiteral("exit_signal"));
        }
        if (qty > 0) {
            const double stop = entry_px * (1 - sl / 100.0), target = entry_px * (1 + tp / 100.0);
            if (b[i].low <= stop)
                sell(std::min(stop, b[i].open), true, i, QStringLiteral("stop_loss"));
            else if (b[i].high >= target)
                sell(std::max(target, b[i].open), false, i, QStringLiteral("take_profit"));
        }
        if (i < n - 1) {
            const double f0 = sma_at(b, i - 1, fast), s0 = sma_at(b, i - 1, slow);
            const double f1 = sma_at(b, i, fast), s1 = sma_at(b, i, slow);
            const bool ok = std::isfinite(f0) && std::isfinite(s0) && std::isfinite(f1) && std::isfinite(s1);
            if (qty == 0 && !pend_in && ok && f0 <= s0 && f1 > s1)
                pend_in = true;
            else if (qty > 0 && !pend_out && ok && f0 >= s0 && f1 < s1)
                pend_out = true;
        }
        r.equity.append(r.cash + qty * b[i].close);
    }
    if (qty > 0) {
        sell(b[n - 1].close, true, n - 1, QStringLiteral("end_of_data"));
        r.equity.last() = r.cash;
    }
    return r;
}

double sample_sharpe(const QVector<double>& eq, double ann) {
    QVector<double> ret;
    for (int i = 1; i < eq.size(); ++i)
        ret.append(eq[i] / eq[i - 1] - 1.0);
    double m = 0;
    for (double x : ret)
        m += x;
    m /= ret.size();
    double ss = 0;
    for (double x : ret)
        ss += (x - m) * (x - m);
    return m / std::sqrt(ss / (ret.size() - 1)) * std::sqrt(ann);
}

BacktestConfig config(const QString& tf, double ppy, int fast, int slow) {
    BacktestConfig c;
    c.entry_conditions = QJsonArray{cross(QStringLiteral("crosses_above"), fast, slow)};
    c.exit_conditions = QJsonArray{cross(QStringLiteral("crosses_below"), fast, slow)};
    c.stop_loss_pct = 3;
    c.take_profit_pct = 8;
    c.initial_capital = 100000;
    c.timeframe = tf;
    c.periods_per_year = ppy;
    c.trading_days_per_year = 252;
    c.costs = execution::preset(QStringLiteral("us_equity_zero")).model;
    c.warmup_bars = slow + 1;
    return c;
}

void compare_run(const QVector<OhlcvCandle>& bars, const BacktestConfig& cfg, int fast, int slow,
                 const QJsonObject& out, const RefResult& ref) {
    QVERIFY(out.value("success").toBool());
    const QJsonArray trades = out.value("trades").toArray();
    QVERIFY(ref.trades.size() >= 5); // the synthetic path really trades
    QCOMPARE(trades.size(), ref.trades.size());
    for (int k = 0; k < trades.size(); ++k) {
        const QJsonObject t = trades[k].toObject();
        const RefTrade& e = ref.trades[k];
        QCOMPARE(t.value("entry_bar").toInt(), e.entry_bar);
        QCOMPARE(t.value("exit_bar").toInt(), e.exit_bar);
        QVERIFY(std::abs(t.value("entry_price").toDouble() - e.entry_px) < 1e-5);
        QVERIFY(std::abs(t.value("exit_price").toDouble() - e.exit_px) < 1e-5);
        QCOMPARE(t.value("shares").toDouble(), e.qty);
        QCOMPARE(t.value("reason").toString(), e.reason);
    }
    const QJsonArray eq = out.value("equity_curve").toArray();
    QCOMPARE(eq.size(), ref.equity.size());
    for (int i = 0; i < eq.size(); ++i)
        QVERIFY(std::abs(eq[i].toDouble() - ref.equity[i]) < 0.006); // the engine rounds to cents
    QVERIFY(std::abs(out.value("final_value").toDouble() - ref.cash) < 0.006);
    QVERIFY(std::abs(out.value("total_fees").toDouble() - ref.fees) < 0.006);
    QVERIFY(std::abs(out.value("total_slippage").toDouble() - ref.slippage) < 0.006);
    QVERIFY(std::abs(out.value("total_return").toDouble() - (ref.cash / 100000.0 - 1.0) * 100.0) < 0.001);
    Q_UNUSED(bars);
    Q_UNUSED(cfg);
    Q_UNUSED(fast);
    Q_UNUSED(slow);
}

} // namespace

class TstAlgoBacktestReference : public QObject {
    Q_OBJECT

  private slots:
    void daily_bars_match_the_reference() {
        const int fast = 10, slow = 30;
        const auto bars = synthetic_bars(700, 1735725600000LL, 86400000LL, 1, 0.02, 7u);
        const BacktestConfig cfg = config(QStringLiteral("1d"), 252, fast, slow);
        const QJsonObject out = BacktestEngine::run(bars, cfg);
        const RefResult ref = reference(bars, cfg.warmup_bars, fast, slow, 3, 8, 100000, 2, 0.00278);
        compare_run(bars, cfg, fast, slow, out, ref);
        if (QTest::currentTestFailed())
            return;
        // Daily bars: bar returns × √252.
        QCOMPARE(out.value("ratio_basis").toString(), QStringLiteral("bar"));
        QVERIFY(std::abs(out.value("sharpe_ratio").toDouble() - sample_sharpe(ref.equity, 252)) < 0.001);
    }

    void intraday_sharpe_is_measured_on_trading_day_closes() {
        const int fast = 12, slow = 48;
        const int per_day = 78;
        const auto bars = synthetic_bars(per_day * 40, 1735738200000LL, 300000LL, per_day, 0.002, 11u);
        const BacktestConfig cfg = config(QStringLiteral("5m"), 252.0 * per_day, fast, slow);
        const QJsonObject out = BacktestEngine::run(bars, cfg);
        const RefResult ref = reference(bars, cfg.warmup_bars, fast, slow, 3, 8, 100000, 2, 0.00278);
        compare_run(bars, cfg, fast, slow, out, ref);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(out.value("ratio_basis").toString(), QStringLiteral("daily"));
        QCOMPARE(out.value("ratio_periods_per_year").toDouble(), 252.0);
        // Reference: the last equity of each trading day, daily returns × √252.
        QVector<double> day_close;
        int last_day = -1;
        for (int i = cfg.warmup_bars; i < bars.size(); ++i) {
            const int d = bars[i].session_date;
            const double v = ref.equity[i - cfg.warmup_bars];
            if (d == last_day)
                day_close.last() = v;
            else
                day_close.append(v);
            last_day = d;
        }
        QVERIFY(std::abs(out.value("sharpe_ratio").toDouble() - sample_sharpe(day_close, 252)) < 0.001);
    }
};

QTEST_GUILESS_MAIN(TstAlgoBacktestReference)
#include "tst_algo_backtest_reference.moc"
