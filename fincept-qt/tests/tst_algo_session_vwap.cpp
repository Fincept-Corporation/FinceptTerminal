// Unit tests for the session-anchored VWAP in src/algo_engine/IndicatorEngine.cpp
//
// Same single extra translation unit as tst_indicator_engine (IndicatorEngine.cpp
// includes only its own header + AlgoEngineTypes.h).
//
// VWAP used to accumulate over whatever window it was handed — 500 bars in a
// backtest, the live buffer in a deployment, the fetched history in a scan — so
// one rule read three different VWAPs. It is now anchored to the bar's trading
// session: OhlcvCandle::session_date when the data layer stamped it, else a gap
// rule, and daily-or-longer bars are each their own session.

#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/IndicatorEngine.h"

#include <QJsonObject>
#include <QTest>
#include <QVector>

using fincept::algo::IndicatorEngine;
using fincept::algo::OhlcvCandle;

namespace {

constexpr int64_t kMin = 60 * 1000;
constexpr int64_t kDay = 24 * 60 * kMin;

OhlcvCandle bar(int64_t t, double price, double vol, int session = 0) {
    OhlcvCandle c;
    c.open_time = t;
    c.close_time = t + 5 * kMin;
    c.open = c.high = c.low = c.close = price; // typical price == price
    c.volume = vol;
    c.is_closed = true;
    c.session_date = session;
    return c;
}

double vwap(const QVector<OhlcvCandle>& c) {
    const auto r = IndicatorEngine::compute(QStringLiteral("VWAP"), c, QJsonObject(), QStringLiteral("value"));
    return r.valid ? r.current.value(QStringLiteral("value")) : -1.0;
}

double vwap_prev(const QVector<OhlcvCandle>& c) {
    const auto r = IndicatorEngine::compute(QStringLiteral("VWAP"), c, QJsonObject(), QStringLiteral("value"));
    return r.valid ? r.previous.value(QStringLiteral("value")) : -1.0;
}

} // namespace

class TstAlgoSessionVwap : public QObject {
    Q_OBJECT

  private slots:
    void resets_at_stamped_session_change() {
        // Day 1 trades at 100, day 2 at 200 / 210. Day 2's VWAP must not remember day 1.
        QVector<OhlcvCandle> c;
        c << bar(0, 100, 1000, 20260302) << bar(5 * kMin, 100, 1000, 20260302)
          << bar(kDay, 200, 100, 20260303) << bar(kDay + 5 * kMin, 210, 300, 20260303);
        QCOMPARE(vwap(c), (200.0 * 100 + 210.0 * 300) / 400.0);
        // The previous bar's VWAP is ITS session's value (the first bar of day 2).
        QCOMPARE(vwap_prev(c), 200.0);
    }

    void same_value_whatever_the_window() {
        // The same session read through a long and a short window agrees.
        QVector<OhlcvCandle> longw;
        for (int i = 0; i < 50; ++i)
            longw << bar(i * 5 * kMin, 90 + i % 3, 500, 20260302);
        for (int i = 0; i < 10; ++i)
            longw << bar(kDay + i * 5 * kMin, 120 + i, 100 + i * 10, 20260303);
        const QVector<OhlcvCandle> shortw = longw.mid(45);
        QCOMPARE(vwap(longw), vwap(shortw));
    }

    void gap_rule_without_stamps() {
        // Unstamped intraday bars: an overnight gap starts a new session.
        QVector<OhlcvCandle> c;
        c << bar(0, 100, 1000) << bar(5 * kMin, 100, 1000) << bar(kDay, 300, 10) << bar(kDay + 5 * kMin, 300, 10);
        QCOMPARE(vwap(c), 300.0);
    }

    void daily_bars_are_their_own_session() {
        QVector<OhlcvCandle> c;
        c << bar(0, 100, 1000, 20260302) << bar(kDay, 150, 1, 20260303);
        QCOMPARE(vwap(c), 150.0);
        QVector<OhlcvCandle> unstamped;
        unstamped << bar(0, 100, 1000) << bar(kDay, 150, 1);
        QCOMPARE(vwap(unstamped), 150.0);
    }

    void zero_volume_falls_back_to_close() {
        QVector<OhlcvCandle> c;
        c << bar(0, 100, 0, 20260302) << bar(5 * kMin, 101, 0, 20260302);
        QCOMPARE(vwap(c), 101.0);
    }
};

QTEST_GUILESS_MAIN(TstAlgoSessionVwap)
#include "tst_algo_session_vwap.moc"
