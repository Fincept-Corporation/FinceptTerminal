// Unit tests for src/algo_engine/ScanSignal.h — header-only, zero app sources.
//
// The scanner used to read direction off the operator: `RSI < 30` showed as
// BEARISH (because of "<") although an oversold RSI is the classic bullish
// reversal reading. Direction now comes from what the rule means.

#include "algo_engine/ScanSignal.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

namespace ss = fincept::algo::scan_signal;

namespace {

QJsonObject leaf(const QString& ind, const QString& op, double value, const QString& field = QStringLiteral("value")) {
    QJsonObject c;
    c["indicator"] = ind;
    c["field"] = field;
    c["operator"] = op;
    c["compare_mode"] = QStringLiteral("value");
    c["value"] = value;
    return c;
}

QJsonObject cross(const QString& ind, int p, const QString& op, const QString& rhs, int rp,
                  const QString& field = QStringLiteral("value"), const QString& rhs_field = QStringLiteral("value")) {
    QJsonObject c;
    c["indicator"] = ind;
    c["params"] = QJsonObject{{"period", p}};
    c["field"] = field;
    c["operator"] = op;
    c["compare_mode"] = QStringLiteral("indicator");
    c["compare_indicator"] = rhs;
    c["compare_params"] = QJsonObject{{"period", rp}};
    c["compare_field"] = rhs_field;
    return c;
}

} // namespace

class TstAlgoScanSignal : public QObject {
    Q_OBJECT

  private slots:
    void oscillator_zones() {
        QCOMPARE(ss::classify(leaf("RSI", "<", 30)).bias, ss::Bias::Bullish);          // oversold
        QCOMPARE(ss::classify(leaf("RSI", ">", 70)).bias, ss::Bias::Bearish);          // overbought
        QCOMPARE(ss::classify(leaf("RSI", "crosses_above", 30)).bias, ss::Bias::Bullish);
        QCOMPARE(ss::classify(leaf("RSI", "crosses_below", 70)).bias, ss::Bias::Bearish);
        QCOMPARE(ss::classify(leaf("RSI", ">", 55)).bias, ss::Bias::Bullish);          // momentum up
        QCOMPARE(ss::classify(leaf("WILLIAMS_R", "<", -80)).bias, ss::Bias::Bullish);
        QCOMPARE(ss::classify(leaf("STOCHASTIC", ">", 80, "k")).bias, ss::Bias::Bearish);
        QCOMPARE(ss::classify(leaf("CCI", "<", -100)).bias, ss::Bias::Bullish);
    }

    void crossovers() {
        QCOMPARE(ss::classify(cross("SMA", 50, "crosses_above", "SMA", 200)).bias, ss::Bias::Bullish);
        QCOMPARE(ss::classify(cross("SMA", 200, ">", "SMA", 50)).bias, ss::Bias::Bearish);
        QCOMPARE(ss::classify(cross("CLOSE", 0, ">", "EMA", 20)).bias, ss::Bias::Bullish);
        QCOMPARE(ss::classify(cross("CLOSE", 0, "crosses_below", "SUPERTREND", 10)).bias, ss::Bias::Bearish);
        QCOMPARE(ss::classify(cross("CLOSE", 0, "<", "BOLLINGER", 20, "value", "lower")).bias, ss::Bias::Bullish);
        QCOMPARE(ss::classify(cross("MACD", 0, "crosses_above", "MACD", 0, "line", "signal_line")).bias,
                 ss::Bias::Bullish);
    }

    void filters_have_no_direction() {
        QCOMPARE(ss::classify(leaf("VOLUME", ">", 1e6)).bias, ss::Bias::Neutral);
        QCOMPARE(ss::classify(leaf("BOLLINGER", "<", 0.05, "width")).bias, ss::Bias::Neutral);
        QCOMPARE(ss::classify(leaf("ATR", ">", 2)).bias, ss::Bias::Neutral);
        QCOMPARE(ss::classify(leaf("ADX", ">", 25)).bias, ss::Bias::Neutral);
    }

    void zero_line_and_levels() {
        QCOMPARE(ss::classify(leaf("MACD", ">", 0, "histogram")).bias, ss::Bias::Bullish);
        QCOMPARE(ss::classify(leaf("ROC", "<", 0)).bias, ss::Bias::Bearish);
        QCOMPARE(ss::classify(leaf("CLOSE", "crosses_above", 150)).bias, ss::Bias::Bullish);
        QCOMPARE(ss::classify(leaf("SMA", "rising", 0)).bias, ss::Bias::Bullish);
    }

    void combine_matched_rules() {
        QJsonArray bull{leaf("RSI", "<", 30), leaf("VOLUME", ">", 1e6)};
        QCOMPARE(ss::combine(bull), ss::Bias::Bullish);
        QJsonArray mixed{leaf("RSI", "<", 30), leaf("ROC", "<", 0)};
        QCOMPARE(ss::combine(mixed), ss::Bias::Neutral);
        QJsonArray none{leaf("VOLUME", ">", 1e6)};
        QCOMPARE(ss::combine(none), ss::Bias::Neutral);
    }
};

QTEST_GUILESS_MAIN(TstAlgoScanSignal)
#include "tst_algo_scan_signal.moc"
