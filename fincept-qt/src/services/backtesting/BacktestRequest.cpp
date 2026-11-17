// src/services/backtesting/BacktestRequest.cpp
#include "services/backtesting/BacktestRequest.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLocale>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace fincept::services::backtest {

namespace {

QString bt_req_tr(const char* text) {
    return QCoreApplication::translate("fincept::services::backtest::BtRequest", text);
}

struct BtPresetFractions {
    const char* id;
    double commission;
    double slippage;
};

// Legacy `commission` / `slippage` fractions per preset: what an engine that does not read `costModel`
// yet should charge. Spread presets add the half-spread (1 bp for 2 bp quoted) to the slippage.
constexpr BtPresetFractions kBtPresetFractions[] = {
    {"none", 0.0, 0.0},
    {"generic_bps", 0.001, 0.0005},
    {"us_equity_retail", 0.0, 0.0003},
    {"us_equity_ibkr_fixed", 0.0001, 0.0003},
    {"india_equity_delivery", 0.0012, 0.0005},
    {"crypto_spot", 0.001, 0.0006},
};

QString bt_req_bp(double fraction) {
    const double bp = fraction * 10000.0;
    const double rounded = std::round(bp * 10.0) / 10.0;
    return QLocale().toString(rounded, 'f', std::fabs(rounded - std::round(rounded)) < 1e-9 ? 0 : 1);
}

} // namespace

QVector<BtChoice> cost_presets() {
    return {
        {QStringLiteral("us_equity_retail"), bt_req_tr("US equity retail"),
         bt_req_tr("0 commission, 2 bp slippage + 2 bp spread (zero-commission US brokers)")},
        {QStringLiteral("us_equity_ibkr_fixed"), bt_req_tr("US equity IBKR fixed"),
         bt_req_tr("USD 0.005 per share (min USD 1, max 1 %), 2 bp slippage + 2 bp spread")},
        {QStringLiteral("india_equity_delivery"), bt_req_tr("India equity delivery"),
         bt_req_tr("Statutory charges ~12 bp per fill (STT, exchange, stamp, SEBI, GST), 5 bp slippage")},
        {QStringLiteral("crypto_spot"), bt_req_tr("Crypto spot"),
         bt_req_tr("0.10 % taker fee, 5 bp slippage + 2 bp spread")},
        {QStringLiteral("generic_bps"), bt_req_tr("Generic 10 bp + 5 bp"),
         bt_req_tr("10 bp commission + 5 bp slippage per fill (cross-engine comparison convention)")},
        {QStringLiteral("none"), bt_req_tr("Zero (gross)"),
         bt_req_tr("No commission and no slippage: a GROSS result")},
        {QStringLiteral("custom"), bt_req_tr("Custom"),
         bt_req_tr("Commission and slippage from the fields below (percent of notional / price per fill)")},
    };
}

QString default_cost_preset_for(const QString& symbol) {
    const QString s = symbol.trimmed().toUpper();
    if (s.endsWith(QLatin1String("-USD")) || s.endsWith(QLatin1String("-USDT")) || s.endsWith(QLatin1String("-EUR")) ||
        s.endsWith(QLatin1String("-BTC")) || s.endsWith(QLatin1String("USDT")))
        return QStringLiteral("crypto_spot");
    if (s.endsWith(QLatin1String(".NS")) || s.endsWith(QLatin1String(".BO")) || s.startsWith(QLatin1String("NSE:")) ||
        s.startsWith(QLatin1String("BSE:")))
        return QStringLiteral("india_equity_delivery");
    return QStringLiteral("us_equity_retail");
}

bool cost_preset_fractions(const QString& preset, double* commission, double* slippage) {
    for (const auto& p : kBtPresetFractions) {
        if (preset == QLatin1String(p.id)) {
            if (commission)
                *commission = p.commission;
            if (slippage)
                *slippage = p.slippage;
            return true;
        }
    }
    return false;
}

QString cost_preset_short(const QString& preset, double commission_pct, double slippage_pct) {
    if (preset == QLatin1String("none"))
        return bt_req_tr("GROSS — no costs");
    if (preset == QLatin1String("us_equity_retail"))
        return bt_req_tr("0 bp + 2 bp slip + spread");
    if (preset == QLatin1String("us_equity_ibkr_fixed"))
        return bt_req_tr("$0.005/sh + 2 bp slip + spread");
    if (preset == QLatin1String("india_equity_delivery"))
        return bt_req_tr("12 bp + 5 bp slip");
    if (preset == QLatin1String("crypto_spot"))
        return bt_req_tr("10 bp + 5 bp slip + spread");
    if (preset == QLatin1String("generic_bps"))
        return bt_req_tr("10 bp + 5 bp slip");
    const double c = commission_pct / 100.0;
    const double s = slippage_pct / 100.0;
    if (c <= 0 && s <= 0)
        return bt_req_tr("GROSS — no costs");
    return bt_req_tr("%1 bp + %2 bp slip").arg(bt_req_bp(c), bt_req_bp(s));
}

QVector<BtChoice> fill_timings() {
    return {
        {QStringLiteral("next_open"), bt_req_tr("Next bar open"),
         bt_req_tr("Signal on the close of bar t, filled at the OPEN of bar t+1 (default, no look-ahead)")},
        {QStringLiteral("next_close"), bt_req_tr("Next bar close"),
         bt_req_tr("Signal on the close of bar t, filled at the CLOSE of bar t+1")},
        {QStringLiteral("same_close"), bt_req_tr("Same bar close (optimistic)"),
         bt_req_tr("Filled at the close the signal was computed from — trades at a price only known once it "
                   "printed; optimistic")},
    };
}

QString fill_timing_text(const QString& id) {
    if (id == QLatin1String("next_close"))
        return bt_req_tr("signal at close → fill next close");
    if (id == QLatin1String("same_close"))
        return bt_req_tr("signal at close → fill same close (optimistic)");
    return bt_req_tr("signal at close → fill next open");
}

QVector<BtChoice> risk_free_sources() {
    return {
        {QStringLiteral("auto"), bt_req_tr("T-bill series"),
         bt_req_tr("The 13-week T-bill yield (^IRX) bar by bar for USD runs; the entered rate otherwise")},
        {QStringLiteral("rate"), bt_req_tr("Fixed rate"), bt_req_tr("The rate entered here, for the whole period")},
    };
}

QVector<BtChoice> monte_carlo_methods() {
    return {
        {QStringLiteral("trade_bootstrap"), bt_req_tr("Trade bootstrap"),
         bt_req_tr("Draws the run's closed trades with replacement")},
        {QStringLiteral("trade_shuffle"), bt_req_tr("Trade shuffle"),
         bt_req_tr("Re-orders the same trades: final equity is identical, drawdowns differ")},
        {QStringLiteral("returns_bootstrap"), bt_req_tr("Return bootstrap"),
         bt_req_tr("Draws bar returns with replacement (works with few trades)")},
        {QStringLiteral("block_bootstrap"), bt_req_tr("Block bootstrap"),
         bt_req_tr("Draws blocks of consecutive bar returns: keeps short-range dependence")},
    };
}

QVector<BtChoice> optimize_objective_choices() {
    return {
        {QStringLiteral("sharpe"), bt_req_tr("Sharpe"), bt_req_tr("Annualised Sharpe ratio")},
        {QStringLiteral("sortino"), bt_req_tr("Sortino"), bt_req_tr("Annualised Sortino ratio")},
        {QStringLiteral("calmar"), bt_req_tr("Calmar"), bt_req_tr("CAGR / max drawdown")},
        {QStringLiteral("return"), bt_req_tr("Total return"), bt_req_tr("Total return over the period")},
    };
}

QVector<BtChoice> optimize_method_choices() {
    return {
        {QStringLiteral("grid"), bt_req_tr("Grid"), bt_req_tr("Every combination of the ranges (up to the run cap)")},
        {QStringLiteral("random"), bt_req_tr("Random"), bt_req_tr("A random sample of the grid, up to the run cap")},
    };
}

QVector<BtChoice> sizing_method_choices() {
    return {
        {QStringLiteral("percent"), bt_req_tr("% of equity"), bt_req_tr("Each entry invests this share of equity")},
        {QStringLiteral("fixed"), bt_req_tr("Fixed amount"), bt_req_tr("Each entry invests this amount of money")},
        {QStringLiteral("kelly"), bt_req_tr("Kelly fraction"), bt_req_tr("Size from the Kelly criterion, scaled")},
        {QStringLiteral("vol_target"), bt_req_tr("Volatility target"),
         bt_req_tr("Size so the position's annual volatility is this percent")},
        {QStringLiteral("risk"), bt_req_tr("Risk per trade"),
         bt_req_tr("Size so a stop-loss hit loses this percent of equity")},
    };
}

int default_max_workers() {
    const int cores = QThread::idealThreadCount();
    return std::max(1, std::min(4, cores - 2));
}

QJsonObject build_backtest_request(const BtSetup& setup) {
    QJsonObject args;
    QJsonArray symbols;
    for (const QString& s : setup.symbols) {
        const QString t = s.trimmed();
        if (!t.isEmpty())
            symbols.append(t);
    }
    args[QStringLiteral("symbols")] = symbols;
    args[QStringLiteral("startDate")] = setup.start.toString(QStringLiteral("yyyy-MM-dd"));
    args[QStringLiteral("endDate")] = setup.end.toString(QStringLiteral("yyyy-MM-dd"));
    args[QStringLiteral("interval")] = setup.interval.isEmpty() ? QStringLiteral("1d") : setup.interval;
    if (!setup.weights.isEmpty() && setup.weights.size() == symbols.size())
        args[QStringLiteral("weights")] = setup.weights;
    args[QStringLiteral("allowSyntheticData")] = setup.allow_synthetic;

    args[QStringLiteral("initialCapital")] = setup.capital;
    double commission = setup.commission_pct / 100.0;
    double slippage = setup.slippage_pct / 100.0;
    QJsonObject cost_model;
    if (setup.cost_preset != QLatin1String("custom") && cost_preset_fractions(setup.cost_preset, &commission, &slippage)) {
        cost_model[QStringLiteral("preset")] = setup.cost_preset;
    } else {
        cost_model[QStringLiteral("commissionType")] = QStringLiteral("pct_notional");
        cost_model[QStringLiteral("commissionValue")] = commission;
        cost_model[QStringLiteral("slippageType")] = QStringLiteral("pct_price");
        cost_model[QStringLiteral("slippageValue")] = slippage;
    }
    args[QStringLiteral("commission")] = commission;
    args[QStringLiteral("slippage")] = slippage;
    args[QStringLiteral("costModel")] = cost_model;
    args[QStringLiteral("fillTiming")] = setup.fill_timing.isEmpty() ? QStringLiteral("next_open") : setup.fill_timing;
    args[QStringLiteral("riskFreeSource")] =
        setup.risk_free_source == QLatin1String("rate") ? QStringLiteral("rate") : QStringLiteral("auto");
    args[QStringLiteral("riskFreeRate")] = setup.risk_free_pct / 100.0;
    args[QStringLiteral("benchmarkSymbol")] = setup.benchmark.trimmed();
    args[QStringLiteral("leverage")] = setup.leverage;
    args[QStringLiteral("positionSizing")] = setup.sizing;
    args[QStringLiteral("positionSizeValue")] = setup.sizing_value;
    args[QStringLiteral("allowShort")] = setup.allow_short;
    if (setup.stop_loss_pct > 0)
        args[QStringLiteral("stopLoss")] = setup.stop_loss_pct / 100.0;
    if (setup.take_profit_pct > 0)
        args[QStringLiteral("takeProfit")] = setup.take_profit_pct / 100.0;

    QJsonObject strategy;
    strategy[QStringLiteral("type")] = setup.strategy_id;
    strategy[QStringLiteral("name")] = setup.strategy_name.isEmpty() ? setup.strategy_id : setup.strategy_name;
    strategy[QStringLiteral("category")] = setup.strategy_category;
    strategy[QStringLiteral("params")] = setup.strategy_params;
    args[QStringLiteral("strategy")] = strategy;
    return args;
}

QJsonObject build_optimize_request(const BtSetup& setup, const BtOptimizeSettings& s) {
    QJsonObject args = build_backtest_request(setup);
    args[QStringLiteral("optimizeObjective")] = s.objective;
    args[QStringLiteral("optimizeMethod")] = s.method;
    args[QStringLiteral("maxIterations")] = std::max(1, s.max_iterations);
    args[QStringLiteral("paramRanges")] = s.param_ranges;
    if (s.holdout_fraction > 0 && s.holdout_fraction < 0.9)
        args[QStringLiteral("holdoutFraction")] = s.holdout_fraction;
    args[QStringLiteral("seed")] = s.seed;
    args[QStringLiteral("maxWorkers")] = default_max_workers();
    return args;
}

QJsonObject build_walk_forward_request(const BtSetup& setup, const BtWalkForwardSettings& s) {
    QJsonObject args = build_backtest_request(setup);
    args[QStringLiteral("wfSplits")] = std::max(2, s.windows);
    args[QStringLiteral("wfTrainRatio")] = s.train_ratio;
    args[QStringLiteral("wfMode")] = s.mode == QLatin1String("anchored") ? QStringLiteral("anchored")
                                                                          : QStringLiteral("rolling");
    args[QStringLiteral("purgeBars")] = std::max(0, s.purge_bars);
    args[QStringLiteral("embargoBars")] = std::max(0, s.embargo_bars);
    args[QStringLiteral("optimizeObjective")] = s.objective;
    args[QStringLiteral("paramRanges")] = s.param_ranges;
    args[QStringLiteral("maxWorkers")] = default_max_workers();
    return args;
}

QJsonObject build_monte_carlo_request(const BtSetup& setup, const BtMonteCarloSettings& s) {
    QJsonObject args = build_backtest_request(setup);
    args[QStringLiteral("mcSimulations")] = std::max(10, s.simulations);
    args[QStringLiteral("mcMethod")] = s.method;
    args[QStringLiteral("mcSeed")] = s.seed;
    args[QStringLiteral("mcBlockSize")] = std::max(1, s.block_size);
    args[QStringLiteral("mcRuinLevel")] = s.ruin_level;
    return args;
}

BtSetup setup_from_request(const QJsonObject& r, const QString& engine) {
    BtSetup s;
    s.engine = engine;
    s.symbols.clear();
    for (const QJsonValue& v : r.value(QStringLiteral("symbols")).toArray())
        if (!v.toString().trimmed().isEmpty())
            s.symbols << v.toString().trimmed();
    s.start = QDate::fromString(r.value(QStringLiteral("startDate")).toString(), QStringLiteral("yyyy-MM-dd"));
    s.end = QDate::fromString(r.value(QStringLiteral("endDate")).toString(), QStringLiteral("yyyy-MM-dd"));
    s.interval = r.value(QStringLiteral("interval")).toString(QStringLiteral("1d"));
    s.weights = r.value(QStringLiteral("weights")).toArray();
    s.allow_synthetic = false; // never carried over: an explicit, per-session choice
    s.capital = r.value(QStringLiteral("initialCapital")).toDouble(s.capital);
    const QJsonObject cm = r.value(QStringLiteral("costModel")).toObject();
    s.commission_pct = r.value(QStringLiteral("commission")).toDouble(s.commission_pct / 100.0) * 100.0;
    s.slippage_pct = r.value(QStringLiteral("slippage")).toDouble(s.slippage_pct / 100.0) * 100.0;
    if (cm.contains(QStringLiteral("preset")))
        s.cost_preset = cm.value(QStringLiteral("preset")).toString();
    else if (!cm.isEmpty() || r.contains(QStringLiteral("commission")) || r.contains(QStringLiteral("slippage")))
        s.cost_preset = QStringLiteral("custom");
    if (cm.contains(QStringLiteral("commissionValue")))
        s.commission_pct = cm.value(QStringLiteral("commissionValue")).toDouble() * 100.0;
    if (cm.contains(QStringLiteral("slippageValue")))
        s.slippage_pct = cm.value(QStringLiteral("slippageValue")).toDouble() * 100.0;
    s.fill_timing = r.value(QStringLiteral("fillTiming")).toString(QStringLiteral("next_open"));
    s.risk_free_source = r.value(QStringLiteral("riskFreeSource")).toString(QStringLiteral("auto")) ==
                                 QLatin1String("rate")
                             ? QStringLiteral("rate")
                             : QStringLiteral("auto");
    if (r.contains(QStringLiteral("riskFreeRate")))
        s.risk_free_pct = r.value(QStringLiteral("riskFreeRate")).toDouble() * 100.0;
    s.benchmark = r.value(QStringLiteral("benchmarkSymbol")).toString();
    s.leverage = r.value(QStringLiteral("leverage")).toDouble(1.0);
    s.sizing = r.value(QStringLiteral("positionSizing")).toString(QStringLiteral("percent"));
    s.sizing_value = r.value(QStringLiteral("positionSizeValue")).toDouble(100.0);
    s.allow_short = r.value(QStringLiteral("allowShort")).toBool(false);
    s.stop_loss_pct = r.value(QStringLiteral("stopLoss")).toDouble(0.0) * 100.0;
    s.take_profit_pct = r.value(QStringLiteral("takeProfit")).toDouble(0.0) * 100.0;
    const QJsonObject st = r.value(QStringLiteral("strategy")).toObject();
    s.strategy_id = st.value(QStringLiteral("type")).toString();
    s.strategy_name = st.value(QStringLiteral("name")).toString();
    s.strategy_category = st.value(QStringLiteral("category")).toString();
    s.strategy_params = st.value(QStringLiteral("params")).toObject();
    return s;
}

QString request_signature(const QJsonObject& request) {
    QJsonObject r = request;
    for (const char* k : {"cancelFile", "brokerCandles", "brokerDataSource", "maxWorkers"})
        r.remove(QLatin1String(k));
    const QByteArray bytes = QJsonDocument(r).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
}

QString run_signature(const QString& engine, const QString& command, const QJsonObject& request) {
    const QByteArray bytes =
        (engine + QLatin1Char('|') + command + QLatin1Char('|') + request_signature(request)).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
}

QString validate_setup(const BtSetup& setup) {
    bool any = false;
    for (const QString& s : setup.symbols)
        any = any || !s.trimmed().isEmpty();
    if (!any)
        return bt_req_tr("Add at least one symbol.");
    if (!setup.start.isValid() || !setup.end.isValid())
        return bt_req_tr("Set a start and an end date.");
    if (setup.start >= setup.end)
        return bt_req_tr("The start date must be before the end date.");
    if (!(setup.capital > 0))
        return bt_req_tr("Capital must be above zero.");
    if (setup.strategy_id.isEmpty())
        return bt_req_tr("Pick a strategy.");
    return {};
}

} // namespace fincept::services::backtest
