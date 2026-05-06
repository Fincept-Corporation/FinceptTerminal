// test_indicators — sanity-check tests for EMA / RSI / MACD / ATR.
//
// Strict pandas-ta parity-on-real-BTC-data is left as a CI-extension fixture
// (load 30 days of BTC 3m bars from a CSV, compare to a captured pandas-ta
// result file). This file covers the algebraic invariants we can assert
// without external data, plus a small hand-computed reference vector.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 3.

#include "services/alpha_arena/Indicators.h"

#include <QObject>
#include <QTest>
#include <QVector>

#include <cmath>
#include <limits>

using namespace fincept::services::alpha_arena;

namespace {

QVector<double> linspace(double a, double b, int n) {
    QVector<double> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) v.push_back(a + (b - a) * i / (n - 1));
    return v;
}

bool nearly_equal(double x, double y, double eps = 1e-9) {
    if (std::isnan(x) || std::isnan(y)) return std::isnan(x) && std::isnan(y);
    return std::fabs(x - y) <= eps * std::max({1.0, std::fabs(x), std::fabs(y)});
}

} // namespace

class TestIndicators : public QObject {
    Q_OBJECT

  private slots:

    void ema_constant_series_returns_constant() {
        QVector<double> v(50, 100.0);
        auto out = ema(v, 12);
        for (int i = 0; i < 11; ++i) QVERIFY(std::isnan(out[i]));
        for (int i = 11; i < 50; ++i) QVERIFY(nearly_equal(out[i], 100.0, 1e-12));
    }

    void ema_seed_is_sma() {
        QVector<double> v = {1, 2, 3, 4, 5};
        auto out = ema(v, 3);
        QVERIFY(std::isnan(out[0]));
        QVERIFY(std::isnan(out[1]));
        QVERIFY(nearly_equal(out[2], (1.0 + 2.0 + 3.0) / 3.0, 1e-12));
        // alpha = 2/(3+1) = 0.5
        QVERIFY(nearly_equal(out[3], 0.5 * 4.0 + 0.5 * 2.0, 1e-12));
        QVERIFY(nearly_equal(out[4], 0.5 * 5.0 + 0.5 * out[3], 1e-12));
    }

    void rsi_pure_uptrend_is_100() {
        QVector<double> v = linspace(100, 200, 50);
        auto out = rsi_wilder(v, 14);
        QVERIFY(nearly_equal(out[14], 100.0, 1e-9));
        QVERIFY(nearly_equal(out[49], 100.0, 1e-9));
    }

    void rsi_pure_downtrend_is_zero() {
        QVector<double> v = linspace(200, 100, 50);
        auto out = rsi_wilder(v, 14);
        QVERIFY(nearly_equal(out[14], 0.0, 1e-9));
        QVERIFY(nearly_equal(out[49], 0.0, 1e-9));
    }

    void rsi_warmup_is_nan() {
        QVector<double> v(20, 100.0);
        auto out = rsi_wilder(v, 14);
        for (int i = 0; i < 14; ++i) QVERIFY(std::isnan(out[i]));
    }

    void macd_zero_input_zero_output() {
        QVector<double> v(60, 50.0);
        auto m = macd(v, 12, 26, 9);
        // After warm-up everything should be ~0.
        for (int i = 35; i < 60; ++i) {
            if (!std::isnan(m.macd[i])) QVERIFY(nearly_equal(m.macd[i], 0.0, 1e-9));
            if (!std::isnan(m.signal[i])) QVERIFY(nearly_equal(m.signal[i], 0.0, 1e-9));
            if (!std::isnan(m.hist[i])) QVERIFY(nearly_equal(m.hist[i], 0.0, 1e-9));
        }
    }

    void macd_hist_equals_macd_minus_signal() {
        QVector<double> v;
        for (int i = 0; i < 80; ++i) v.push_back(100.0 + std::sin(i * 0.3) * 5.0);
        auto m = macd(v, 12, 26, 9);
        for (int i = 0; i < v.size(); ++i) {
            if (std::isnan(m.macd[i]) || std::isnan(m.signal[i])) {
                continue;
            }
            QVERIFY(nearly_equal(m.hist[i], m.macd[i] - m.signal[i], 1e-12));
        }
    }

    void atr_constant_range_is_constant() {
        QVector<double> highs(30, 105.0);
        QVector<double> lows(30, 95.0);
        QVector<double> closes(30, 100.0);
        auto out = atr_wilder(highs, lows, closes, 14);
        for (int i = 14; i < 30; ++i) QVERIFY(nearly_equal(out[i], 10.0, 1e-9));
    }

    void atr_warmup_is_nan() {
        QVector<double> highs(20, 105.0);
        QVector<double> lows(20, 95.0);
        QVector<double> closes(20, 100.0);
        auto out = atr_wilder(highs, lows, closes, 14);
        // ATR is undefined for the first (period - 1) points; the seed lands at index period-1.
        for (int i = 0; i < 13; ++i) QVERIFY(std::isnan(out[i]));
        QVERIFY(!std::isnan(out[13]));
    }

    void empty_input_returns_empty() {
        QVector<double> v;
        QCOMPARE(ema(v, 3).size(), 0);
        QCOMPARE(rsi_wilder(v, 14).size(), 0);
        QCOMPARE(atr_wilder(v, v, v, 14).size(), 0);
    }
};

QTEST_MAIN(TestIndicators)
#include "test_indicators.moc"
