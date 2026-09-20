// Unit tests for src/algo_engine/IndicatorEngine.{h,cpp}
//
// IndicatorEngine.cpp includes only its own header, which includes
// algo_engine/AlgoEngineTypes.h (Qt Core types + plain aggregate structs, no
// .cpp of its own). One extra translation unit, no service, network or broker
// linkage.
//
// The headline case is the NaN prefix. sma_series() and ema_series() both emit
// a series whose first period-1 samples are NaN, and four call sites feed such
// a series straight back in: DEMA and TEMA (EMA of an EMA), MACD (signal line
// over the MACD line) and Stochastic (%D over %K). A seed window that starts at
// index 0 therefore sums NaN, and neither the rolling sum nor the EMA recursion
// ever recovers. DEMA, TEMA and MACD report "insufficient data" on any history
// at all, and %D silently degrades to %K.

#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/IndicatorEngine.h"

#include <QJsonObject>
#include <QString>
#include <QTest>
#include <QVector>

#include <cmath>

using fincept::algo::IndicatorEngine;
using fincept::algo::IndicatorResult;
using fincept::algo::OhlcvCandle;

namespace {

// A rising series, far longer than any default period in the catalogue
// (the largest is Ichimoku's senkou = 52). Nothing here is borderline: if an
// indicator cannot produce a value from 500 bars, it cannot produce one at all.
QVector<OhlcvCandle> rising_candles(int n = 500) {
    QVector<OhlcvCandle> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        OhlcvCandle c;
        c.open_time = i;
        c.close_time = i;
        c.close = 100.0 + i * 0.5;
        c.open = c.close - 0.25;
        c.high = c.close + 1.0;
        c.low = c.close - 1.0;
        c.volume = 1000;
        c.is_closed = true;
        out.append(c);
    }
    return out;
}

// %K oscillates only if the price does. A straight line pins %K to a constant,
// which would make the %D-equals-%K defect invisible.
QVector<OhlcvCandle> oscillating_candles(int n = 200) {
    QVector<OhlcvCandle> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        OhlcvCandle c;
        c.open_time = i;
        c.close_time = i;
        c.close = 100.0 + 10.0 * std::sin(i * 0.3);
        c.open = c.close;
        c.high = c.close + 0.5;
        c.low = c.close - 0.5;
        c.volume = 1000;
        c.is_closed = true;
        out.append(c);
    }
    return out;
}

QString why(const IndicatorResult& r) {
    return r.valid ? QStringLiteral("(valid)") : r.error;
}

} // namespace

class TstIndicatorEngine : public QObject {
    Q_OBJECT

  private slots:
    // Baseline: the two helpers work when nobody feeds them a NaN prefix.
    void sma_and_ema_produce_values();

    // EMA of an EMA.
    void dema_produces_a_value_from_ample_history();
    void tema_produces_a_value_from_ample_history();

    // EMA of the MACD line.
    void macd_produces_a_signal_line_from_ample_history();

    // SMA of %K.
    void stochastic_d_is_the_average_of_the_last_k_values();

    // The clamp in compute() must keep holding.
    void unknown_indicator_is_reported_as_such();
};

void TstIndicatorEngine::sma_and_ema_produce_values() {
    const auto candles = rising_candles();
    QJsonObject p;
    p["period"] = 20;

    const auto sma = IndicatorEngine::compute(QStringLiteral("SMA"), candles, p, QStringLiteral("value"));
    QVERIFY2(sma.valid, qPrintable(why(sma)));
    QVERIFY(std::isfinite(sma.current.value(QStringLiteral("value"))));

    const auto ema = IndicatorEngine::compute(QStringLiteral("EMA"), candles, p, QStringLiteral("value"));
    QVERIFY2(ema.valid, qPrintable(why(ema)));
    QVERIFY(std::isfinite(ema.current.value(QStringLiteral("value"))));
}

void TstIndicatorEngine::dema_produces_a_value_from_ample_history() {
    const auto candles = rising_candles();
    QJsonObject p;
    p["period"] = 20; // the catalogue default in AlgoTradingTypes.h

    const auto r = IndicatorEngine::compute(QStringLiteral("DEMA"), candles, p, QStringLiteral("value"));
    QVERIFY2(r.valid, qPrintable(why(r)));
    const double v = r.current.value(QStringLiteral("value"));
    QVERIFY(std::isfinite(v));

    // DEMA exists to cut the lag of a plain EMA. On a strictly rising series it
    // must therefore sit above it, which also rules out a value that merely
    // happens to be finite.
    const auto ema = IndicatorEngine::compute(QStringLiteral("EMA"), candles, p, QStringLiteral("value"));
    QVERIFY(ema.valid);
    QVERIFY2(v > ema.current.value(QStringLiteral("value")),
             qPrintable(QStringLiteral("DEMA %1 <= EMA %2").arg(v).arg(ema.current.value(QStringLiteral("value")))));
}

void TstIndicatorEngine::tema_produces_a_value_from_ample_history() {
    const auto candles = rising_candles();
    QJsonObject p;
    p["period"] = 20;

    const auto r = IndicatorEngine::compute(QStringLiteral("TEMA"), candles, p, QStringLiteral("value"));
    QVERIFY2(r.valid, qPrintable(why(r)));
    const double v = r.current.value(QStringLiteral("value"));
    QVERIFY(std::isfinite(v));

    // Same lag argument as DEMA. Not compared against DEMA: on a perfectly
    // linear ramp both land exactly on the current price, so that comparison
    // would assert an artefact of the test data rather than the indicator.
    const auto ema = IndicatorEngine::compute(QStringLiteral("EMA"), candles, p, QStringLiteral("value"));
    QVERIFY(ema.valid);
    QVERIFY2(v > ema.current.value(QStringLiteral("value")),
             qPrintable(QStringLiteral("TEMA %1 <= EMA %2").arg(v).arg(ema.current.value(QStringLiteral("value")))));
}

void TstIndicatorEngine::macd_produces_a_signal_line_from_ample_history() {
    const auto candles = rising_candles();
    QJsonObject p; // catalogue defaults: fast 12, slow 26, signal 9

    const auto r = IndicatorEngine::compute(QStringLiteral("MACD"), candles, p, QStringLiteral("line"));
    QVERIFY2(r.valid, qPrintable(why(r)));

    const double line = r.current.value(QStringLiteral("line"));
    const double signal = r.current.value(QStringLiteral("signal_line"));
    const double hist = r.current.value(QStringLiteral("histogram"));
    QVERIFY(std::isfinite(line));
    QVERIFY(std::isfinite(signal));
    QCOMPARE(hist, line - signal);
}

void TstIndicatorEngine::stochastic_d_is_the_average_of_the_last_k_values() {
    const auto candles = oscillating_candles();
    QJsonObject p;
    p["k_period"] = 14;
    p["d_period"] = 3;

    // %D is the d_period SMA of %K. ConditionEvaluator reaches earlier bars by
    // trimming the candle window, so read %K the same way rather than
    // recomputing the stochastic formula here.
    const int n = candles.size();
    double k[3];
    for (int back = 0; back < 3; ++back) {
        const auto r =
            IndicatorEngine::compute(QStringLiteral("STOCHASTIC"), candles.mid(0, n - back), p, QStringLiteral("k"));
        QVERIFY2(r.valid, qPrintable(why(r)));
        k[back] = r.current.value(QStringLiteral("k"));
        QVERIFY(std::isfinite(k[back]));
    }

    const auto r = IndicatorEngine::compute(QStringLiteral("STOCHASTIC"), candles, p, QStringLiteral("d"));
    QVERIFY2(r.valid, qPrintable(why(r)));
    const double d = r.current.value(QStringLiteral("d"));

    // Guard the guard: on this series the three %K samples differ, so a %D that
    // simply echoes %K cannot pass by coincidence.
    QVERIFY(std::abs(k[0] - k[1]) > 1e-6);
    QCOMPARE(d, (k[0] + k[1] + k[2]) / 3.0);
}

void TstIndicatorEngine::unknown_indicator_is_reported_as_such() {
    const auto r =
        IndicatorEngine::compute(QStringLiteral("NOPE"), rising_candles(10), QJsonObject(), QStringLiteral("value"));
    QVERIFY(!r.valid);
    QVERIFY2(r.error.contains(QStringLiteral("Unknown indicator")), qPrintable(r.error));
}

QTEST_GUILESS_MAIN(TstIndicatorEngine)
#include "tst_indicator_engine.moc"
