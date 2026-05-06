// test_schema_python_mirror — assert scripts/alpha_arena/schema.json mirrors
// the constants in AlphaArenaSchema.h. Catches drift at CI time.
//
// The test resolves the schema.json path via the FINCEPT_SCHEMA_JSON CMake
// definition; falls back to walking up from the test executable.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 2.

#include "services/alpha_arena/AlphaArenaSchema.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTest>

using namespace fincept::services::alpha_arena;

namespace {

QString locate_schema_json() {
#ifdef FINCEPT_SCHEMA_JSON
    QString hard = QStringLiteral(FINCEPT_SCHEMA_JSON);
    if (QFile::exists(hard)) return hard;
#endif
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        QString candidate = d.absoluteFilePath(QStringLiteral("scripts/alpha_arena/schema.json"));
        if (QFile::exists(candidate)) return candidate;
        if (!d.cdUp()) break;
    }
    return QString();
}

} // namespace

class TestSchemaPythonMirror : public QObject {
    Q_OBJECT

  private:
    QJsonObject schema_;

  private slots:

    void initTestCase() {
        const QString path = locate_schema_json();
        QVERIFY2(!path.isEmpty(), "Could not locate scripts/alpha_arena/schema.json");
        QFile f(path);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(QStringLiteral("cannot open %1").arg(path)));
        const auto bytes = f.readAll();
        f.close();
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(bytes, &err);
        QVERIFY2(err.error == QJsonParseError::NoError, qPrintable(err.errorString()));
        QVERIFY(doc.isObject());
        schema_ = doc.object();
    }

    void signals_match() {
        QStringList wire;
        for (auto s : {Signal::BuyToEnter, Signal::SellToEnter, Signal::Hold, Signal::Close})
            wire << signal_to_wire(s);
        QStringList from_json;
        for (const auto& v : schema_.value(QStringLiteral("signals")).toArray())
            from_json << v.toString();
        QCOMPARE(from_json, wire);
    }

    void modes_match() {
        QStringList wire;
        for (auto m : {CompetitionMode::Baseline, CompetitionMode::Monk, CompetitionMode::Situational})
            wire << mode_to_wire(m);
        QStringList from_json;
        for (const auto& v : schema_.value(QStringLiteral("modes")).toArray())
            from_json << v.toString();
        QCOMPARE(from_json, wire);
    }

    void perp_universe_matches() {
        QStringList from_json;
        for (const auto& v : schema_.value(QStringLiteral("perp_universe")).toArray())
            from_json << v.toString();
        QCOMPARE(from_json, kPerpUniverse());
    }

    void limits_match() {
        const auto lim = schema_.value(QStringLiteral("limits")).toObject();
        QCOMPARE(lim.value(QStringLiteral("min_leverage")).toInt(), limits::kMinLeverage);
        QCOMPARE(lim.value(QStringLiteral("max_leverage")).toInt(), limits::kMaxLeverage);
        QCOMPARE(lim.value(QStringLiteral("invalidation_condition_max_chars")).toInt(),
                 limits::kInvalidationConditionMaxChars);
        QCOMPARE(lim.value(QStringLiteral("justification_max_chars")).toInt(),
                 limits::kJustificationMaxChars);
        QCOMPARE(lim.value(QStringLiteral("risk_usd_recompute_tolerance_frac")).toDouble(),
                 limits::kRiskUsdRecomputeToleranceFrac);
        QCOMPARE(lim.value(QStringLiteral("max_risk_per_trade_frac")).toDouble(),
                 limits::kMaxRiskPerTradeFrac);
        QCOMPARE(lim.value(QStringLiteral("min_liquidation_buffer_frac")).toDouble(),
                 limits::kMinLiquidationBufferFrac);
    }
};

QTEST_MAIN(TestSchemaPythonMirror)
#include "test_schema_python_mirror.moc"
