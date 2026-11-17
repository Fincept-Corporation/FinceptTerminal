// tests/tst_backtest_request.cpp
//
// The request the Backtesting screen sends (src/services/backtesting/BacktestRequest): exact keys and
// units the providers read (scripts/Analytics/backtesting: base/costs.py costModel, base/execution.py
// fillTiming, base/result_normalizer.py riskFreeSource auto|rate, base/monte_carlo.py mcSeed /
// mcBlockSize / mcRuinLevel, vbt_engine holdoutFraction / wfMode / purgeBars / embargoBars), the
// round trip setup -> request -> setup, and the run signature.
#include "services/backtesting/BacktestRequest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest/QtTest>

using namespace fincept::services::backtest;

namespace {

BtSetup sample_setup() {
    BtSetup s;
    s.engine = QStringLiteral("vectorbt");
    s.symbols = {QStringLiteral("SPY")};
    s.start = QDate(2021, 11, 17);
    s.end = QDate(2026, 11, 16);
    s.capital = 100000;
    s.benchmark = QStringLiteral("SPY");
    s.cost_preset = QStringLiteral("us_equity_retail");
    s.fill_timing = QStringLiteral("next_open");
    s.risk_free_source = QStringLiteral("auto");
    s.risk_free_pct = 4.0;
    s.strategy_id = QStringLiteral("sma_crossover");
    s.strategy_name = QStringLiteral("SMA Crossover");
    s.strategy_category = QStringLiteral("Trend");
    s.strategy_params = QJsonObject{{QStringLiteral("fastPeriod"), 20}, {QStringLiteral("slowPeriod"), 50}};
    return s;
}

} // namespace

class TestBacktestRequest : public QObject {
    Q_OBJECT

  private slots:
    void backtest_request_has_the_provider_keys() {
        const QJsonObject r = build_backtest_request(sample_setup());
        QCOMPARE(r.value(QStringLiteral("symbols")).toArray(), QJsonArray{QStringLiteral("SPY")});
        QCOMPARE(r.value(QStringLiteral("startDate")).toString(), QStringLiteral("2021-11-17"));
        QCOMPARE(r.value(QStringLiteral("endDate")).toString(), QStringLiteral("2026-11-16"));
        QCOMPARE(r.value(QStringLiteral("interval")).toString(), QStringLiteral("1d"));
        QCOMPARE(r.value(QStringLiteral("initialCapital")).toDouble(), 100000.0);
        QCOMPARE(r.value(QStringLiteral("fillTiming")).toString(), QStringLiteral("next_open"));
        QCOMPARE(r.value(QStringLiteral("riskFreeSource")).toString(), QStringLiteral("auto"));
        QCOMPARE(r.value(QStringLiteral("riskFreeRate")).toDouble(), 0.04);
        QCOMPARE(r.value(QStringLiteral("benchmarkSymbol")).toString(), QStringLiteral("SPY"));
        QCOMPARE(r.value(QStringLiteral("allowSyntheticData")).toBool(true), false);
        const QJsonObject cm = r.value(QStringLiteral("costModel")).toObject();
        QCOMPARE(cm.value(QStringLiteral("preset")).toString(), QStringLiteral("us_equity_retail"));
        // Engines not yet on the shared cost model get the preset's fractions.
        QCOMPARE(r.value(QStringLiteral("commission")).toDouble(), 0.0);
        QCOMPARE(r.value(QStringLiteral("slippage")).toDouble(), 0.0003);
        const QJsonObject st = r.value(QStringLiteral("strategy")).toObject();
        QCOMPARE(st.value(QStringLiteral("type")).toString(), QStringLiteral("sma_crossover"));
        QCOMPARE(st.value(QStringLiteral("params")).toObject().value(QStringLiteral("fastPeriod")).toInt(), 20);
        QVERIFY(!r.contains(QStringLiteral("stopLoss")));
        QVERIFY(!r.contains(QStringLiteral("takeProfit")));
        QVERIFY(!r.contains(QStringLiteral("weights")));
    }

    void custom_costs_are_fractions_and_a_cost_model() {
        BtSetup s = sample_setup();
        s.cost_preset = QStringLiteral("custom");
        s.commission_pct = 0.10; // 10 bp
        s.slippage_pct = 0.05;   // 5 bp
        const QJsonObject r = build_backtest_request(s);
        QCOMPARE(r.value(QStringLiteral("commission")).toDouble(), 0.001);
        QCOMPARE(r.value(QStringLiteral("slippage")).toDouble(), 0.0005);
        const QJsonObject cm = r.value(QStringLiteral("costModel")).toObject();
        QVERIFY(!cm.contains(QStringLiteral("preset")));
        QCOMPARE(cm.value(QStringLiteral("commissionType")).toString(), QStringLiteral("pct_notional"));
        QCOMPARE(cm.value(QStringLiteral("commissionValue")).toDouble(), 0.001);
        QCOMPARE(cm.value(QStringLiteral("slippageType")).toString(), QStringLiteral("pct_price"));
        QCOMPARE(cm.value(QStringLiteral("slippageValue")).toDouble(), 0.0005);
    }

    void every_preset_maps_to_fractions() {
        for (const BtChoice& c : cost_presets()) {
            double comm = -1, slip = -1;
            const bool known = cost_preset_fractions(c.id, &comm, &slip);
            if (c.id == QLatin1String("custom")) {
                QVERIFY(!known);
                continue;
            }
            QVERIFY2(known, qPrintable(c.id));
            QVERIFY(comm >= 0 && slip >= 0);
        }
        double c = -1, s = -1;
        QVERIFY(cost_preset_fractions(QStringLiteral("none"), &c, &s));
        QCOMPARE(c, 0.0);
        QCOMPARE(s, 0.0);
    }

    void default_cost_preset_follows_the_venue() {
        QCOMPARE(default_cost_preset_for(QStringLiteral("SPY")), QStringLiteral("us_equity_retail"));
        QCOMPARE(default_cost_preset_for(QStringLiteral("RELIANCE.NS")), QStringLiteral("india_equity_delivery"));
        QCOMPARE(default_cost_preset_for(QStringLiteral("btc-usd")), QStringLiteral("crypto_spot"));
    }

    void rate_source_and_fill_choices() {
        BtSetup s = sample_setup();
        s.risk_free_source = QStringLiteral("rate");
        s.fill_timing = QStringLiteral("same_close");
        const QJsonObject r = build_backtest_request(s);
        QCOMPARE(r.value(QStringLiteral("riskFreeSource")).toString(), QStringLiteral("rate"));
        QCOMPARE(r.value(QStringLiteral("fillTiming")).toString(), QStringLiteral("same_close"));
        s.risk_free_source = QStringLiteral("series"); // anything else = the T-bill series
        QCOMPARE(build_backtest_request(s).value(QStringLiteral("riskFreeSource")).toString(), QStringLiteral("auto"));
    }

    void stops_and_weights_only_when_set() {
        BtSetup s = sample_setup();
        s.stop_loss_pct = 5;
        s.take_profit_pct = 12.5;
        s.symbols = {QStringLiteral("SPY"), QStringLiteral("QQQ")};
        s.weights = QJsonArray{0.6, 0.4};
        const QJsonObject r = build_backtest_request(s);
        QCOMPARE(r.value(QStringLiteral("stopLoss")).toDouble(), 0.05);
        QCOMPARE(r.value(QStringLiteral("takeProfit")).toDouble(), 0.125);
        QCOMPARE(r.value(QStringLiteral("weights")).toArray().size(), 2);
        s.weights = QJsonArray{1.0};
        QVERIFY(!build_backtest_request(s).contains(QStringLiteral("weights")));
    }

    void optimize_request() {
        BtOptimizeSettings o;
        o.objective = QStringLiteral("sortino");
        o.method = QStringLiteral("random");
        o.max_iterations = 200;
        o.holdout_fraction = 0.3;
        o.seed = 11;
        o.param_ranges = QJsonObject{{QStringLiteral("fastPeriod"),
                                      QJsonObject{{QStringLiteral("min"), 5}, {QStringLiteral("max"), 30},
                                                  {QStringLiteral("step"), 5}}}};
        const QJsonObject r = build_optimize_request(sample_setup(), o);
        QCOMPARE(r.value(QStringLiteral("optimizeObjective")).toString(), QStringLiteral("sortino"));
        QCOMPARE(r.value(QStringLiteral("optimizeMethod")).toString(), QStringLiteral("random"));
        QCOMPARE(r.value(QStringLiteral("maxIterations")).toInt(), 200);
        QCOMPARE(r.value(QStringLiteral("holdoutFraction")).toDouble(), 0.3);
        QCOMPARE(r.value(QStringLiteral("seed")).toInt(), 11);
        QVERIFY(r.value(QStringLiteral("maxWorkers")).toInt() >= 1);
        QVERIFY(r.value(QStringLiteral("maxWorkers")).toInt() <= 4);
        QVERIFY(r.value(QStringLiteral("paramRanges")).toObject().contains(QStringLiteral("fastPeriod")));
        o.holdout_fraction = 0;
        QVERIFY(!build_optimize_request(sample_setup(), o).contains(QStringLiteral("holdoutFraction")));
    }

    void walk_forward_request() {
        BtWalkForwardSettings w;
        w.windows = 6;
        w.train_ratio = 0.75;
        w.mode = QStringLiteral("anchored");
        w.purge_bars = 5;
        w.embargo_bars = 2;
        const QJsonObject r = build_walk_forward_request(sample_setup(), w);
        QCOMPARE(r.value(QStringLiteral("wfSplits")).toInt(), 6);
        QCOMPARE(r.value(QStringLiteral("wfTrainRatio")).toDouble(), 0.75);
        QCOMPARE(r.value(QStringLiteral("wfMode")).toString(), QStringLiteral("anchored"));
        QCOMPARE(r.value(QStringLiteral("purgeBars")).toInt(), 5);
        QCOMPARE(r.value(QStringLiteral("embargoBars")).toInt(), 2);
        QCOMPARE(r.value(QStringLiteral("optimizeObjective")).toString(), QStringLiteral("sharpe"));
    }

    void monte_carlo_request() {
        BtMonteCarloSettings m;
        m.method = QStringLiteral("block_bootstrap");
        m.simulations = 2000;
        m.block_size = 15;
        m.ruin_level = 0.4;
        m.seed = 7;
        const QJsonObject r = build_monte_carlo_request(sample_setup(), m);
        QCOMPARE(r.value(QStringLiteral("mcMethod")).toString(), QStringLiteral("block_bootstrap"));
        QCOMPARE(r.value(QStringLiteral("mcSimulations")).toInt(), 2000);
        QCOMPARE(r.value(QStringLiteral("mcBlockSize")).toInt(), 15);
        QCOMPARE(r.value(QStringLiteral("mcRuinLevel")).toDouble(), 0.4);
        QCOMPARE(r.value(QStringLiteral("mcSeed")).toInt(), 7);
        QCOMPARE(monte_carlo_methods().size(), 4);
    }

    void setup_round_trip() {
        BtSetup s = sample_setup();
        s.cost_preset = QStringLiteral("custom");
        s.commission_pct = 0.12;
        s.slippage_pct = 0.03;
        s.stop_loss_pct = 4;
        s.allow_synthetic = true;
        const BtSetup back = setup_from_request(build_backtest_request(s), QStringLiteral("vectorbt"));
        QCOMPARE(back.symbols, s.symbols);
        QCOMPARE(back.start, s.start);
        QCOMPARE(back.end, s.end);
        QCOMPARE(back.cost_preset, QStringLiteral("custom"));
        QCOMPARE(qRound(back.commission_pct * 1000), 120);
        QCOMPARE(qRound(back.slippage_pct * 1000), 30);
        QCOMPARE(qRound(back.stop_loss_pct), 4);
        QCOMPARE(back.strategy_id, s.strategy_id);
        QCOMPARE(back.strategy_params, s.strategy_params);
        QVERIFY(!back.allow_synthetic); // never carried over
        const BtSetup preset = setup_from_request(build_backtest_request(sample_setup()), QStringLiteral("bt"));
        QCOMPARE(preset.cost_preset, QStringLiteral("us_equity_retail"));
        QCOMPARE(preset.engine, QStringLiteral("bt"));
    }

    void signature_is_stable_and_input_sensitive() {
        const QJsonObject a = build_backtest_request(sample_setup());
        QJsonObject b = a;
        b.insert(QStringLiteral("cancelFile"), QStringLiteral("C:/tmp/x.flag"));
        QCOMPARE(request_signature(a), request_signature(b));
        BtSetup s = sample_setup();
        s.strategy_params.insert(QStringLiteral("fastPeriod"), 21);
        QVERIFY(request_signature(a) != request_signature(build_backtest_request(s)));
        QVERIFY(run_signature(QStringLiteral("vectorbt"), QStringLiteral("backtest"), a) !=
                run_signature(QStringLiteral("bt"), QStringLiteral("backtest"), a));
    }

    void validation() {
        QVERIFY(validate_setup(sample_setup()).isEmpty());
        BtSetup s = sample_setup();
        s.symbols.clear();
        QVERIFY(!validate_setup(s).isEmpty());
        s = sample_setup();
        s.end = s.start;
        QVERIFY(!validate_setup(s).isEmpty());
        s = sample_setup();
        s.strategy_id.clear();
        QVERIFY(!validate_setup(s).isEmpty());
    }

    void cost_short_text() {
        QVERIFY(cost_preset_short(QStringLiteral("none"), 0, 0).contains(QStringLiteral("GROSS")));
        QCOMPARE(cost_preset_short(QStringLiteral("custom"), 0.10, 0.05), QStringLiteral("10 bp + 5 bp slip"));
    }
};

QTEST_GUILESS_MAIN(TestBacktestRequest)
#include "tst_backtest_request.moc"
