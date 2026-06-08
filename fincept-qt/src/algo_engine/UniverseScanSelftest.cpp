// src/algo_engine/UniverseScanSelftest.cpp
#include "algo_engine/UniverseScanSelftest.h"

#include "algo_engine/ConditionEvaluator.h"
#include "algo_engine/RealtimeScanRunner.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

#include <cstdio>

namespace fincept::algo {

namespace {
OhlcvCandle bar(double close) {
    OhlcvCandle c;
    c.open = c.high = c.low = c.close = close;
    c.is_closed = true;
    return c;
}
} // namespace

int run_universe_scan_selftest() {
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", name);
        if (!ok)
            ++failures;
    };

    std::printf("universe-scan selftest:\n");

    // 1. required_bars: a pure price condition needs only the floor (2).
    {
        QJsonArray conds;
        QJsonObject c;
        c["indicator"] = "CLOSE";
        c["operator"] = ">";
        c["value"] = 100.0;
        conds.append(c);
        check(RealtimeScanRunner::required_bars(conds) == 2, "required_bars(CLOSE>100) == 2");
    }

    // 2. required_bars: SMA(200) needs >= 200 bars.
    {
        QJsonArray conds;
        QJsonObject c, p;
        p["period"] = 200;
        c["indicator"] = "SMA";
        c["params"] = p;
        c["operator"] = ">";
        c["value"] = 0.0;
        conds.append(c);
        check(RealtimeScanRunner::required_bars(conds) >= 200, "required_bars(SMA200) >= 200");
    }

    // 3. evaluate_group fires on a CLOSE-crosses-above window (99 -> 101).
    // Three bars are required: IndicatorEngine needs candles.size()>=2 even
    // after the offset+1 trim, so the crossing window must have at least 3 bars.
    {
        QJsonArray conds;
        QJsonObject c;
        c["indicator"] = "CLOSE";
        c["operator"] = "crosses_above";
        c["value"] = 100.0;
        conds.append(c);
        QVector<OhlcvCandle> w{bar(98.0), bar(99.0), bar(101.0)};
        auto r = ConditionEvaluator::evaluate_group(conds, "AND", w);
        check(r.triggered, "CLOSE crosses_above 100 (99->101) triggers");
    }

    // 4. evaluate_group does NOT fire when the level is not met.
    {
        QJsonArray conds;
        QJsonObject c;
        c["indicator"] = "CLOSE";
        c["operator"] = ">";
        c["value"] = 100.0;
        conds.append(c);
        QVector<OhlcvCandle> w{bar(98.0), bar(99.0)};
        auto r = ConditionEvaluator::evaluate_group(conds, "AND", w);
        check(!r.triggered, "CLOSE>100 (at 99) does NOT trigger");
    }

    std::printf("universe-scan selftest: %s\n",
                failures == 0 ? "PASS" : "FAILED");
    return failures == 0 ? 0 : 1;
}

} // namespace fincept::algo
