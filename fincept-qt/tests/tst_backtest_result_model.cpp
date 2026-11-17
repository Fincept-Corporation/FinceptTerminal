// tests/tst_backtest_result_model.cpp
//
// Contract v2 parsing (src/services/backtesting/BacktestResultModel) on the backend's real samples
// (tests/data/backtesting/sample_{run_backtest,optimize,walk_forward,monte_carlo}.json, copied from
// scripts/Analytics/backtesting/tests/data; override the folder with FINCEPT_BT_TEST_DATA), the v1
// fallbacks on a hand-made v1 payload, and the export text (src/services/backtesting/BacktestExportText).
#include "services/backtesting/BacktestExportText.h"
#include "services/backtesting/BacktestResultModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest/QtTest>

#include <cmath>

using namespace fincept::services::backtest;

namespace {

QJsonObject sample_data(const char* name) {
    QString root = qEnvironmentVariable("FINCEPT_BT_TEST_DATA");
    if (root.isEmpty())
        root = QFINDTESTDATA("data/backtesting");
    QFile f(root + QLatin1Char('/') + QLatin1String(name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject env = QJsonDocument::fromJson(f.readAll()).object();
    return env.value(QStringLiteral("data")).toObject();
}

bool close_to(double a, double b, double tol = 1e-9) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b));
}

} // namespace

class TestBacktestResultModel : public QObject {
    Q_OBJECT

  private slots:
    void time_parsing() {
        QCOMPARE(bt_time_ms(QStringLiteral("1970-01-02")), 86400000.0);
        QCOMPARE(bt_time_ms(QStringLiteral("2023-01-03")), 1672704000000.0);
        QCOMPARE(bt_time_ms(QStringLiteral("2023-01-03 14:30:00")), 1672704000000.0 + 52200000.0);
        QCOMPARE(bt_time_ms(QStringLiteral("2023-01-03T14:30:00")), 1672704000000.0 + 52200000.0);
        QVERIFY(std::isnan(bt_time_ms(QStringLiteral("not a date"))));
        QVERIFY(std::isnan(bt_time_ms(QStringLiteral("2023-13-01"))));
        QVERIFY(std::isnan(bt_time_ms(QString())));
        QVERIFY(bt_is_intraday(QStringLiteral("2023-01-03 14:30:00")));
        QVERIFY(!bt_is_intraday(QStringLiteral("2023-01-03 00:00:00")));
        QVERIFY(!bt_is_intraday(QStringLiteral("2023-01-03")));
    }

    void backtest_sample_identity_and_provenance() {
        const QJsonObject d = sample_data("sample_run_backtest.json");
        QVERIFY2(!d.isEmpty(), "sample_run_backtest.json not found");
        const BtRun r = parse_backtest(d);
        QCOMPARE(r.schema_version, 2);
        QCOMPARE(r.engine, QStringLiteral("vectorbt"));
        QCOMPARE(r.engine_implementation, QStringLiteral("Fincept reference simulator (base.simulator)"));
        QCOMPARE(r.engine_version, QStringLiteral("sample"));
        QCOMPARE(r.currency, QStringLiteral("USD"));
        QVERIFY(!r.synthetic);
        QVERIFY(r.provenance.present);
        QCOMPARE(r.provenance.data_source, QStringLiteral("yahoo"));
        QCOMPARE(r.provenance.adjustment, QStringLiteral("split+dividend"));
        QCOMPARE(r.provenance.bars, 502);
        QCOMPARE(r.provenance.first_bar, QStringLiteral("2023-01-03"));
        QCOMPARE(r.provenance.last_bar, QStringLiteral("2024-12-31"));
        QCOMPARE(r.provenance.warmup_bars, 50);
        QCOMPARE(r.provenance.strategy_type, QStringLiteral("sma_crossover"));
        QCOMPARE(r.provenance.parameters.value(QStringLiteral("fastPeriod")).toInt(), 20);
        QCOMPARE(r.provenance.symbols.size(), 1);
        QCOMPARE(r.provenance.symbols.first().symbol, QStringLiteral("SPY"));
        QCOMPARE(r.symbols(), QStringList{QStringLiteral("SPY")});
    }

    void backtest_sample_assumptions_and_warnings() {
        const BtRun r = parse_backtest(sample_data("sample_run_backtest.json"));
        QCOMPARE(r.assumptions.fill_at, QStringLiteral("next_open"));
        QCOMPARE(r.assumptions.fill_signal_at, QStringLiteral("close"));
        QVERIFY(r.assumptions.fill_description.contains(QStringLiteral("OPEN")));
        QCOMPARE(r.assumptions.risk_free_source, QStringLiteral("^IRX"));
        QVERIFY(close_to(r.assumptions.risk_free_rate, 0.048936510671928876));
        QCOMPARE(r.assumptions.not_modelled.size(), 11);
        QVERIFY(r.assumptions.not_modelled.contains(QStringLiteral("bid/ask spread")));
        QVERIFY(!r.assumptions.notes.isEmpty());
        QCOMPARE(r.warnings.size(), 3);
        QCOMPARE(r.warnings.first().code.isEmpty(), false);
        // most severe first
        for (int i = 1; i < r.warnings.size(); ++i)
            QVERIFY(bt_warning_rank(r.warnings[i - 1].severity) <= bt_warning_rank(r.warnings[i].severity));
    }

    void backtest_sample_metrics() {
        const BtRun r = parse_backtest(sample_data("sample_run_backtest.json"));
        QVERIFY(close_to(r.metric(QStringLiteral("performance.totalReturn")), 0.3106672546881064));
        QVERIFY(close_to(r.metric(QStringLiteral("performance.cagr")), 0.14537569487562996));
        QVERIFY(close_to(r.metric(QStringLiteral("performance.sharpeRatio")), 0.9350912898153321));
        QVERIFY(close_to(r.metric(QStringLiteral("performance.maxDrawdown")), 0.08405625272741213));
        QVERIFY(r.metric(QStringLiteral("performance.maxDrawdown")) > 0); // positive by contract
        QVERIFY(close_to(r.metric(QStringLiteral("exposure.timeInMarket")), 0.6792828685258964));
        QVERIFY(close_to(r.metric(QStringLiteral("advancedMetrics.var95")), -0.009137575237362137));
        QVERIFY(close_to(r.metric(QStringLiteral("periodStats.bestWeek")), 0.047544866687652565));
        QVERIFY(close_to(r.metric(QStringLiteral("benchmark.totalReturn")), 0.58242665844419));
        QCOMPARE(r.metric(QStringLiteral("tradeStats.profitFactorInfinite")), 1.0);
        // null -> NaN with a reason
        QVERIFY(std::isnan(r.metric(QStringLiteral("tradeStats.profitFactor"))));
        QVERIFY(r.why_blank(QStringLiteral("tradeStats.profitFactor")).contains(QStringLiteral("∞")));
        QVERIFY(r.why_blank(QStringLiteral("performance.sharpeRatio")).isEmpty());
        QVERIFY(!r.why_blank(QStringLiteral("performance.noSuchMetric")).isEmpty());
        QCOMPARE(r.closed_trades(), 3);
        QVERIFY(close_to(r.initial_capital(), 100000.0));
        QVERIFY(close_to(r.final_equity(), 131066.72546881063));
    }

    void backtest_sample_series() {
        const BtRun r = parse_backtest(sample_data("sample_run_backtest.json"));
        QCOMPARE(r.t.size(), 502);
        QCOMPARE(r.equity.size(), 502);
        QCOMPARE(r.drawdown.size(), 502);
        QCOMPARE(r.returns.size(), 502);
        QCOMPARE(r.benchmark.size(), 502);
        QVERIFY(r.has_benchmark());
        QVERIFY(r.has_exposure());
        QCOMPARE(r.cash.size(), 502);
        for (int i = 1; i < r.t.size(); ++i)
            QVERIFY(r.t[i] > r.t[i - 1]);
        for (double dd : r.drawdown)
            QVERIFY(!(dd > 0));
        QCOMPARE(r.dates.first(), QStringLiteral("2023-01-03"));
        QVERIFY(!r.intraday);
        QVERIFY(close_to(r.years(), (r.t.last() - r.t.first()) / (365.25 * 86400000.0)));
        QCOMPARE(r.rolling_window, 126);
        QCOMPARE(r.rolling_sharpe.t.size(), 376);
        QCOMPARE(r.rolling_beta.t.size(), 376);
        QCOMPARE(r.rolling_drawdown.t.size(), 502);
    }

    void backtest_sample_trades_and_tables() {
        const BtRun r = parse_backtest(sample_data("sample_run_backtest.json"));
        QCOMPARE(r.trades.size(), 4);
        QCOMPARE(r.open_trades, 1);
        QVERIFY(r.trades.last().open);           // open positions last
        QVERIFY(r.trades.last().exit_date.isEmpty());
        QCOMPARE(r.trades.last().exit_reason, QStringLiteral("open"));
        const BtTrade& t = r.trades.first();
        QCOMPARE(t.id, QStringLiteral("t1"));
        QCOMPARE(t.side, QStringLiteral("long"));
        QCOMPARE(t.holding_bars, 94);
        QVERIFY(t.mae <= 0 && t.mfe >= 0);
        QVERIFY(close_to(t.costs(), 207.31193499970556 + 103.6578713481067));
        QCOMPARE(r.drawdowns.size(), 10);
        QVERIFY(!r.drawdowns_computed);
        QCOMPARE(r.drawdowns.first().rank, 1);
        QVERIFY(r.drawdowns.first().depth > 0);
        QCOMPARE(r.drawdowns.first().trough_date, QStringLiteral("2024-08-05"));
        QVERIFY(r.drawdowns.first().recovered());
        QCOMPARE(r.monthly.size(), 2);
        QCOMPARE(r.benchmark_monthly.size(), 2);
        QCOMPARE(r.yearly.size(), 2);
        QVERIFY(!r.yearly_computed);
        QCOMPARE(r.attribution.size(), 1);
        QVERIFY(!r.attribution.first().computed);
        QCOMPARE(r.positions.size(), 1);
        QCOMPARE(r.positions.first().symbol, QStringLiteral("SPY"));
    }

    void backtest_sample_costs() {
        const BtRun r = parse_backtest(sample_data("sample_run_backtest.json"));
        QVERIFY(r.costs.present);
        QVERIFY(close_to(r.costs.total_commission, 801.9501253699789));
        QVERIFY(close_to(r.costs.total_slippage, 400.9503634100711));
        QVERIFY(close_to(r.costs.total_costs, 1202.90048878005));
        QVERIFY(close_to(r.costs.gross_pnl, 32269.625957590684));
        QCOMPARE(r.costs.fills, 7);
        QCOMPARE(r.costs.by_year.size(), 2);
        QVERIFY(r.costs.model.present);
        QCOMPARE(r.costs.model.commission_type, QStringLiteral("pct_notional"));
        QCOMPARE(r.costs.model.preset, QStringLiteral("request"));
        QVERIFY(r.costs.model.itemised_slippage);
    }

    void v1_payload_gets_fallbacks() {
        // A v1 result: no schemaVersion, no drawdowns / yearly / attribution / provenance / exposure,
        // annualizedReturn without cagr, exposureTime in performance, fees in statistics.
        QJsonArray eq;
        const double vals[] = {100, 110, 99, 105, 121, 115, 130};
        const char* dates[] = {"2020-01-01", "2020-02-01", "2020-03-01", "2020-04-01",
                               "2020-05-01", "2020-06-01", "2020-07-01"};
        for (int i = 0; i < 7; ++i)
            eq.append(QJsonObject{{QStringLiteral("date"), QLatin1String(dates[i])}, {QStringLiteral("equity"), vals[i]}});
        QJsonObject d{
            {QStringLiteral("equity"), eq},
            {QStringLiteral("performance"), QJsonObject{{QStringLiteral("totalReturn"), 0.3},
                                                        {QStringLiteral("annualizedReturn"), 0.55},
                                                        {QStringLiteral("exposureTime"), 0.4},
                                                        {QStringLiteral("totalTrades"), 2}}},
            {QStringLiteral("statistics"), QJsonObject{{QStringLiteral("totalFees"), 12.5},
                                                       {QStringLiteral("initialCapital"), 100}}},
            {QStringLiteral("monthlyReturns"),
             QJsonArray{QJsonObject{{QStringLiteral("year"), 2020},
                                    {QStringLiteral("months"), QJsonArray{0.1, -0.1, 0.06, 0.15, -0.05, 0.13, QJsonValue(),
                                                                          QJsonValue(), QJsonValue(), QJsonValue(),
                                                                          QJsonValue(), QJsonValue()}},
                                    {QStringLiteral("yearTotal"), 0.3}}}},
            {QStringLiteral("trades"),
             QJsonArray{QJsonObject{{QStringLiteral("symbol"), QStringLiteral("AAA")}, {QStringLiteral("side"), QStringLiteral("long")},
                                    {QStringLiteral("entryDate"), QStringLiteral("2020-01-01")},
                                    {QStringLiteral("exitDate"), QStringLiteral("2020-03-01")},
                                    {QStringLiteral("pnl"), -1.0}, {QStringLiteral("holdingPeriod"), 2}},
                        QJsonObject{{QStringLiteral("symbol"), QStringLiteral("AAA")}, {QStringLiteral("side"), QStringLiteral("long")},
                                    {QStringLiteral("entryDate"), QStringLiteral("2020-03-01")},
                                    {QStringLiteral("exitDate"), QStringLiteral("2020-07-01")},
                                    {QStringLiteral("pnl"), 31.0}, {QStringLiteral("holdingPeriod"), 4}}}},
            {QStringLiteral("assumptions"), QJsonObject{{QStringLiteral("ignoredSettings"),
                                                         QJsonArray{QStringLiteral("leverage: not applied")}}}},
        };
        const BtRun r = parse_backtest(d);
        QCOMPARE(r.schema_version, 1);
        QCOMPARE(r.metric(QStringLiteral("performance.cagr")), 0.55);
        QCOMPARE(r.metric(QStringLiteral("exposure.timeInMarket")), 0.4);
        QCOMPARE(r.metric(QStringLiteral("costs.totalCommission")), 12.5);
        QCOMPARE(r.metric(QStringLiteral("tradeStats.closedTrades")), 2.0);
        // drawdown + returns columns computed
        QCOMPARE(r.drawdown.size(), 7);
        QVERIFY(close_to(r.drawdown[2], 99.0 / 110.0 - 1.0));
        QVERIFY(close_to(r.returns[1], 0.1));
        // drawdowns computed: 110 -> 99 (recovered at 121), 121 -> 115 (recovered at 130)
        QVERIFY(r.drawdowns_computed);
        QCOMPARE(r.drawdowns.size(), 2);
        QVERIFY(close_to(r.drawdowns[0].depth, 0.1));
        QCOMPARE(r.drawdowns[0].peak_date, QStringLiteral("2020-02-01"));
        QCOMPARE(r.drawdowns[0].trough_date, QStringLiteral("2020-03-01"));
        QCOMPARE(r.drawdowns[0].recovery_date, QStringLiteral("2020-05-01"));
        QCOMPARE(r.drawdowns[0].to_trough_bars, 1);
        QCOMPARE(r.drawdowns[0].recovery_bars, 2);
        QCOMPARE(r.drawdowns[0].duration_bars, 3);
        QCOMPARE(r.drawdowns[0].rank, 1);
        // yearly from monthly, attribution from trades
        QVERIFY(r.yearly_computed);
        QCOMPARE(r.yearly.size(), 1);
        QCOMPARE(r.yearly.first().strategy, 0.3);
        QVERIFY(std::isnan(r.yearly.first().benchmark));
        QCOMPARE(r.attribution.size(), 1);
        QVERIFY(r.attribution.first().computed);
        QCOMPARE(r.attribution.first().trades, 2);
        QCOMPARE(r.attribution.first().win_rate, 0.5);
        QCOMPARE(r.attribution.first().net_pnl, 30.0);
        QCOMPARE(r.attribution.first().contribution, 0.3);
        QCOMPARE(r.attribution.first().avg_hold_bars, 3.0);
        // no benchmark: the relative metrics say why they are blank
        QVERIFY(r.why_blank(QStringLiteral("performance.alpha")).contains(QStringLiteral("benchmark")));
        // v1 ignored settings become warnings
        QCOMPARE(r.warnings.size(), 1);
        QCOMPARE(r.warnings.first().code, QStringLiteral("IGNORED_SETTING"));
    }

    void synthetic_and_engine_disclosures() {
        const QJsonObject d{{QStringLiteral("usingSyntheticData"), true},
                            {QStringLiteral("usesModelPricedDerivatives"), true},
                            {QStringLiteral("modelPricedDisclosures"), QJsonArray{QStringLiteral("SPY options priced by BS")}}};
        const BtRun r = parse_backtest(d);
        QVERIFY(r.synthetic);
        QCOMPARE(r.warnings.first().code, QStringLiteral("SYNTHETIC_DATA"));
        QCOMPARE(r.warnings.first().severity, QStringLiteral("error"));
        QCOMPARE(r.data_disclosures.size(), 2);
        QCOMPARE(r.serious_warning_count(), 1);
    }

    void undefined_metrics_reason() {
        const QJsonObject d{{QStringLiteral("performance"), QJsonObject{{QStringLiteral("sharpeRatio"), QJsonValue()}}},
                            {QStringLiteral("undefinedMetrics"),
                             QJsonObject{{QStringLiteral("sharpeRatio"), QStringLiteral("returns have no variation")}}}};
        const BtRun r = parse_backtest(d);
        QVERIFY(std::isnan(r.metric(QStringLiteral("performance.sharpeRatio"))));
        QCOMPARE(r.why_blank(QStringLiteral("performance.sharpeRatio")), QStringLiteral("returns have no variation"));
    }

    void optimize_sample() {
        const QJsonObject d = sample_data("sample_optimize.json");
        QVERIFY2(!d.isEmpty(), "sample_optimize.json not found");
        const BtOptimizeResult r = parse_optimize(d);
        QCOMPARE(r.schema_version, 2);
        QCOMPARE(r.objective, QStringLiteral("sharpe"));
        QCOMPARE(r.method, QStringLiteral("grid"));
        QCOMPARE(r.iterations, 25);
        QCOMPARE(r.total_combinations, 25);
        QCOMPARE(r.rows.size(), 25);
        QCOMPARE(r.best_params.value(QStringLiteral("fastPeriod")).toInt(), 10);
        QVERIFY(close_to(r.best_objective, 0.8332632663983027));
        QVERIFY(close_to(r.rows.first().objective, r.best_objective));
        QCOMPARE(r.param_names.size(), 2);
        QCOMPARE(r.param_grid.value(QStringLiteral("fastPeriod")).size(), 5);
        QVERIFY(r.has_overfitting);
        QCOMPARE(r.n_trials, 25);
        QVERIFY(close_to(r.deflated_sharpe, 0.9786420259501882));
        QVERIFY(close_to(r.pbo, 0.818958818958819));
        QVERIFY(close_to(r.expected_max_sharpe, 0.17665616573253998));
        QVERIFY(r.has_holdout);
        QCOMPARE(r.holdout_split_date, QStringLiteral("2021-12-30"));
        QVERIFY(close_to(r.holdout_out_sample.value(QStringLiteral("sharpeRatio")), -0.07738945747872901));
        QVERIFY(close_to(r.plateau_score, 0.8398184939721387));
        QCOMPARE(r.neighbours, 5);
        QVERIFY(r.has_significance);
    }

    void optimize_surface_matrix() {
        const BtOptimizeResult r = parse_optimize(sample_data("sample_optimize.json"));
        const BtSurface s = optimize_surface(r, QStringLiteral("fastPeriod"), QStringLiteral("slowPeriod"));
        QCOMPARE(s.x_values.size(), 5);
        QCOMPARE(s.y_values.size(), 5);
        QCOMPARE(s.values.size(), 5);
        QVERIFY(s.best_row >= 0 && s.best_col >= 0);
        QVERIFY(close_to(s.values[s.best_row][s.best_col], r.best_objective));
        // every evaluated cell is filled for a full grid
        int finite = 0;
        for (const auto& row : s.values)
            for (double v : row)
                finite += std::isfinite(v) ? 1 : 0;
        QCOMPARE(finite, 25);
        const BtSurface one = optimize_surface(r, QStringLiteral("fastPeriod"), QStringLiteral("fastPeriod"));
        QCOMPARE(one.values.size(), 1);
        QCOMPARE(one.values.first().size(), 5);
        const BtSurface dd = optimize_surface(r, QStringLiteral("fastPeriod"), QStringLiteral("slowPeriod"),
                                              QStringLiteral("maxDrawdown"));
        QVERIFY(!dd.higher_is_better);
    }

    void walk_forward_sample() {
        const QJsonObject d = sample_data("sample_walk_forward.json");
        QVERIFY2(!d.isEmpty(), "sample_walk_forward.json not found");
        const BtWalkForwardResult r = parse_walk_forward(d);
        QCOMPARE(r.schema_version, 2);
        QCOMPARE(r.mode, QStringLiteral("rolling"));
        QCOMPARE(r.n_windows, 3);
        QCOMPARE(r.windows.size(), 3);
        QCOMPARE(r.windows.first().train_start, QStringLiteral("2015-08-07"));
        QCOMPARE(r.windows.first().test_end, QStringLiteral("2021-04-05"));
        QCOMPARE(r.windows.first().best_params.value(QStringLiteral("fastPeriod")).toInt(), 15);
        QCOMPARE(r.purge_bars, 0);
        QCOMPARE(r.combinations_per_window, 9);
        QCOMPARE(r.oos_equity.t.size(), 1413);
        QVERIFY(close_to(r.wf_efficiency, 0.8726863661012082));
        QVERIFY(close_to(r.oos_coverage, 0.5616057233704292));
        QVERIFY(close_to(r.robustness, 0.6666666666666666));
        QCOMPARE(r.param_stability.size(), 2);
        QCOMPARE(r.param_stability.value(QStringLiteral("slowPeriod")).first.size(), 3);
        QVERIFY(close_to(r.param_stability.value(QStringLiteral("fastPeriod")).second, 2.8867513459481287));
    }

    void monte_carlo_sample() {
        const QJsonObject d = sample_data("sample_monte_carlo.json");
        QVERIFY2(!d.isEmpty(), "sample_monte_carlo.json not found");
        const BtMonteCarloResult r = parse_monte_carlo(d);
        QCOMPARE(r.schema_version, 2);
        QCOMPARE(r.method_id, QStringLiteral("block_bootstrap"));
        QCOMPARE(r.n_simulations, 500);
        QCOMPARE(r.x.size(), 400);
        QCOMPARE(r.p5.size(), 400);
        QCOMPARE(r.p95.size(), 400);
        QCOMPARE(r.original.size(), 400);
        QCOMPARE(r.sample_paths.size(), 20);
        QCOMPARE(r.final_values.size(), 500);
        QCOMPARE(r.drawdown_values.size(), 500);
        QCOMPARE(r.x_dates.size(), 400);
        QVERIFY(close_to(r.stat(QStringLiteral("probRuin")), 0.0));
        QVERIFY(close_to(r.stat(QStringLiteral("originalPercentile")), 0.524));
        QVERIFY(close_to(r.stat(QStringLiteral("ddP95")), 0.12627540007554866));
        QVERIFY(close_to(r.initial_capital, 100000.0));
        QVERIFY(close_to(r.block_size, 20.0));
        QCOMPARE(r.assumptions.fill_at, QStringLiteral("next_open"));
        QCOMPARE(r.provenance.engine, QStringLiteral("vectorbt"));
    }

    void thinning() {
        QVector<double> x, y;
        for (int i = 0; i < 20001; ++i) {
            x << i;
            y << i * 2;
        }
        QVector<double> tx, ty;
        bt_thin(x, y, 5000, &tx, &ty);
        QVERIFY(tx.size() <= 5001);
        QCOMPARE(tx.first(), 0.0);
        QCOMPARE(tx.last(), 20000.0);
        QCOMPARE(ty.last(), 40000.0);
        bt_thin(x.mid(0, 10), y.mid(0, 10), 5000, &tx, &ty);
        QCOMPARE(tx.size(), 10);
    }

    void export_text() {
        const BtRun r = parse_backtest(sample_data("sample_run_backtest.json"));
        BtExportMeta meta = bt_export_meta(r);
        meta.generated_at = QStringLiteral("2026-11-17T12:00:00");
        QCOMPARE(meta.currency, QStringLiteral("USD"));
        QCOMPARE(meta.engine, QStringLiteral("vectorbt"));
        const QStringList head = bt_export_header(meta);
        QVERIFY(head.join(QLatin1Char('\n')).contains(QStringLiteral("currency: USD")));
        QVERIFY(head.join(QLatin1Char('\n')).contains(QStringLiteral("fill model:")));
        for (const QString& h : head)
            QVERIFY(h.startsWith(QStringLiteral("# ")));
        const QString trades = bt_trades_csv(r, meta);
        const QStringList lines = trades.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), head.size() + 1 + 4);
        QVERIFY(lines.at(head.size()).startsWith(QStringLiteral("id,symbol,side,status")));
        QVERIFY(lines.last().contains(QStringLiteral(",open,")));
        QVERIFY(lines.last().endsWith(QStringLiteral(",USD")));
        // plain numbers: no currency symbols, no percent signs, no grouping
        for (int i = head.size() + 1; i < lines.size(); ++i) {
            QVERIFY(!lines.at(i).contains(QLatin1Char('$')));
            QVERIFY(!lines.at(i).contains(QLatin1Char('%')));
        }
        const QString eq = bt_equity_csv(r, meta);
        QCOMPARE(eq.count(QLatin1Char('\n')), head.size() + 1 + 502);
        QVERIFY(eq.contains(QStringLiteral("2023-01-03,100000,0,0,100000,0,0,100000,USD")));
        const QString metrics = bt_metrics_csv(r, meta);
        QVERIFY(metrics.contains(QStringLiteral("performance.sharpeRatio,0.935091289815332")));
        QCOMPARE(bt_csv_number(0.1), QStringLiteral("0.1"));
        QCOMPARE(bt_csv_number(100000.0), QStringLiteral("100000"));
        QVERIFY(bt_csv_number(std::nan("")).isEmpty());
        QCOMPARE(bt_csv_field(QStringLiteral("a,b")), QStringLiteral("\"a,b\""));
        const QByteArray json = bt_result_json(r.payload, meta);
        const QJsonObject root = QJsonDocument::fromJson(json).object();
        QCOMPARE(root.value(QStringLiteral("export")).toObject().value(QStringLiteral("currency")).toString(),
                 QStringLiteral("USD"));
        QCOMPARE(root.value(QStringLiteral("result")).toObject().value(QStringLiteral("schemaVersion")).toInt(), 2);
        const BtOptimizeResult o = parse_optimize(sample_data("sample_optimize.json"));
        const QString ocsv = bt_optimize_csv(o, bt_export_meta(o.provenance, o.assumptions, QStringLiteral("optimize")));
        QVERIFY(ocsv.contains(QStringLiteral("rank,fastPeriod,slowPeriod,objective_sharpe")));
    }
};

QTEST_GUILESS_MAIN(TestBacktestResultModel)
#include "tst_backtest_result_model.moc"
