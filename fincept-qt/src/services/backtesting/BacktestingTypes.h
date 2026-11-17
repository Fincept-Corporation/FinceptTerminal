// src/services/backtesting/BacktestingTypes.h
//
// This file is the UI's view of provider capability. The `commands` array on
// each Provider gates which buttons the BacktestingScreen renders and which
// commands the user can invoke. It is a SUBSET of what each Python provider
// actually dispatches — the source of truth for capability is each provider's
// scripts/Analytics/backtesting/{provider}/{provider}_provider.py::main()
// dispatcher. When a Python provider grows a new command, this file and the
// matching cmd_config_stack_ page in BacktestingScreen.cpp must be updated
// together to surface it.
#pragma once
#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::backtest {

// ── Provider definitions ────────────────────────────────────────────────────

struct Provider {
    QString slug;
    QString display_name;
    // Identity colour, written for the dark theme: draw it through
    // ui::colors::legible() (text, lines) or ui::colors::tint() (fills).
    QColor color;
    QStringList commands;
};

inline QVector<Provider> all_providers() {
    // Per-provider command list is the set of commands whose Python implementation
    // (1) accepts the args BacktestingScreen sends, AND (2) returns a payload that
    // display_result() can render. Commands that fail either test are deliberately
    // omitted — the UI's command sidebar is filtered against this list, so dropped
    // commands don't appear as broken buttons.
    // monte_carlo is served by every provider through the shared
    // scripts/Analytics/backtesting/base/monte_carlo.py (it reruns the provider's
    // own backtest and resamples the realised trades).
    return {
        {"vectorbt",
         "VectorBT",
         QColor("#00E5FF"),
         {"backtest", "optimize", "walk_forward", "indicator", "indicator_signals", "labels", "splits", "returns",
          "signals", "labels_to_signals", "indicator_sweep", "monte_carlo"}},
        {"backtestingpy",
         "Backtesting.py",
         QColor("#00D66F"),
         // Requires `pip install backtesting`. All commands surface a clean install
         // error when the dep is missing; kept in the provider list so users see
         // the option and can install it.
         {"backtest", "optimize", "walk_forward", "indicator", "monte_carlo"}},
        // (A "fasttrade" provider used to sit here. It needs fast-trade 2.x, which requires
        // plotly >= 6.5 while the NumPy 1.x environment pins plotly < 6 for PyPortfolioOpt, so
        // it could never be installed and every run failed. BacktestingScreen persists the
        // provider by slug and maps the old index order — keep that table in sync.)
        {"zipline",
         "Zipline",
         QColor("#FF3B8E"),
         {"backtest", "optimize", "walk_forward", "indicator", "indicator_signals", "labels", "splits", "returns",
          "signals", "labels_to_signals", "monte_carlo"}},
        {"bt",
         "BT",
         QColor("#FF6B35"),
         // BT's extended commands (indicator/labels/splits/signals/returns/...) return
         // per-symbol-nested shapes that don't match the frontend renderer; only the
         // backtest-shaped commands are exposed.
         {"backtest", "optimize", "walk_forward", "monte_carlo"}},
        {"fincept", "Fincept", QColor("#d97706"), {"backtest", "optimize", "walk_forward", "monte_carlo"}},
    };
}

// ── What each engine applies of the shared settings ─────────────────────────
// Mirrors each provider's `ignored_request_settings(..., supported)` list and its
// _assumptions() block (scripts/Analytics/backtesting/*/*_provider.py). The
// screen greys out / annotates the inputs an engine would ignore BEFORE the run;
// the provider's `assumptions.ignoredSettings` still reports them afterwards.
struct ProviderSupport {
    bool commission = true;
    bool slippage = true;
    bool stops = true;        ///< stop-loss / take-profit
    bool sizing = true;       ///< position sizing method + value
    bool short_selling = true;
    bool leverage = true;
    bool intraday = true;     ///< intervals other than 1d
    bool multi_symbol = true; ///< every symbol is traded (else only the first)
};

inline ProviderSupport provider_support(const QString& slug) {
    ProviderSupport s;
    if (slug == QLatin1String("vectorbt")) {
        s.short_selling = false; // long-only simulator
        s.leverage = false;      // positions capped at available cash
    } else if (slug == QLatin1String("backtestingpy")) {
        s.stops = false;
        s.sizing = false; // strategies buy with ~100% of equity
        s.multi_symbol = false;
    } else if (slug == QLatin1String("zipline")) {
        s.stops = false;
        s.leverage = false;
        s.intraday = false;
        s.multi_symbol = false;
    } else if (slug == QLatin1String("bt")) {
        s.slippage = false;
        s.stops = false;
        s.sizing = false; // strategy weights
        s.short_selling = false;
        s.leverage = false;
        s.intraday = false;
    } else if (slug == QLatin1String("fincept")) {
        // The strategy code sizes and exits its own orders. Commission and slippage
        // stay editable: the engine applies both to every fill.
        s.stops = false;
        s.sizing = false;
        s.short_selling = false;
        s.leverage = false;
        s.intraday = false;
    }
    return s;
}

/// What the engine says it applies — its get_capabilities reply (BacktestingService::load_capabilities,
/// capabilities_payload() in base_provider.py) — over the built-in table above: every flag the reply states
/// wins, a flag it leaves out keeps provider_support(slug), and a request key it lists under
/// requestKeys.ignored is not applied whatever the flags say. Empty `caps` (an engine without the command,
/// or before it answered) = provider_support(slug).
inline ProviderSupport provider_support_from_capabilities(const QString& slug, const QJsonObject& caps) {
    ProviderSupport s = provider_support(slug);
    if (caps.isEmpty())
        return s;
    const auto flag = [&caps](const char* key, bool fallback) {
        const QJsonValue v = caps.value(QLatin1String(key));
        return v.isBool() ? v.toBool() : fallback;
    };
    const auto listed = [&caps](const char* key, bool fallback) {
        const QJsonValue v = caps.value(QLatin1String(key));
        return v.isArray() ? !v.toArray().isEmpty() : fallback;
    };
    s.short_selling = flag("shortSelling", s.short_selling);
    s.leverage = flag("leverage", s.leverage);
    s.multi_symbol = flag("multiSymbol", s.multi_symbol);
    s.intraday = flag("intraday", s.intraday);
    s.stops = flag("stops", s.stops);
    s.sizing = listed("sizingMethods", s.sizing);
    s.commission = listed("commissionModels", s.commission);
    s.slippage = listed("slippageModels", s.slippage);
    const QJsonArray ignored =
        caps.value(QLatin1String("requestKeys")).toObject().value(QLatin1String("ignored")).toArray();
    for (const QJsonValue& v : ignored) {
        const QString key = v.toString();
        if (key == QLatin1String("allowShort"))
            s.short_selling = false;
        else if (key == QLatin1String("leverage"))
            s.leverage = false;
        else if (key == QLatin1String("stopLoss") || key == QLatin1String("takeProfit"))
            s.stops = false;
        else if (key == QLatin1String("positionSizing") || key == QLatin1String("positionSizeValue"))
            s.sizing = false;
        else if (key == QLatin1String("commission"))
            s.commission = false;
        else if (key == QLatin1String("slippage"))
            s.slippage = false;
        else if (key == QLatin1String("interval"))
            s.intraday = false;
        else if (key == QLatin1String("symbols"))
            s.multi_symbol = false;
    }
    return s;
}

// ── Command definitions ─────────────────────────────────────────────────────

struct CommandDef {
    QString id;
    QString label;
    QColor color; // identity colour for the dark theme — draw through legible() / tint()
};

inline QVector<CommandDef> all_commands() {
    // New commands MUST be appended at the end. BacktestingScreen persists
    // the active command as an integer index in save_state(), and the
    // cmd_config_stack_ pages are looked up by the same index — reordering
    // would break saved-state restore for existing users.
    return {
        {"backtest", "Run Backtest", QColor("#FF6B35")},
        {"optimize", "Optimize", QColor("#00D66F")},
        {"walk_forward", "Walk-Forward", QColor("#0088FF")},
        {"indicator", "Indicators", QColor("#9D4EDD")},
        {"indicator_signals", "Indicator Signals", QColor("#FFC400")},
        {"labels", "ML Labels", QColor("#00E5FF")},
        {"splits", "CV Splits", QColor("#0088FF")},
        {"returns", "Returns Analysis", QColor("#00D66F")},
        {"signals", "Signal Generators", QColor("#FFC400")},
        {"labels_to_signals", "Labels -> Signals", QColor("#9D4EDD")},
        {"indicator_sweep", "Indicator Sweep", QColor("#00E5FF")},
        {"monte_carlo", "Monte Carlo", QColor("#FF3B8E")},
    };
}

// ── Strategy parameter definition ──────────────────────────────────────────

struct StrategyParam {
    QString name;
    QString label;
    double default_val = 14;
    double min_val = 1;
    double max_val = 200;
    double step = 1;
    QString type = QStringLiteral("float"); ///< int | float | bool | choice (catalogue "type"; inferred when absent)
    QStringList choices;                    ///< for type "choice"
    QString default_text;                   ///< default of a choice parameter
    QString description;
};

// ── Strategy definitions ────────────────────────────────────────────────────

/// A cross-parameter rule of the catalogue: {"lt": [a, b], "message": "..."} = a < b.
struct StrategyConstraint {
    QString less;    ///< parameter that must be smaller
    QString greater; ///< parameter that must be larger
    QString message;
};

struct Strategy {
    QString id;
    QString name;
    QString category;
    QVector<StrategyParam> params;
    QString description;
    QString kind;                 ///< "signals" | "allocation" | ... (vectorbt); empty when not given
    bool supported = true;        ///< false: listed with `reason`, cannot run in this environment / engine
    QString reason;               ///< why it cannot run (engine limitation or a missing package)
    QStringList missing_packages; ///< Python packages it needs that are not installed here
    QVector<StrategyConstraint> constraints;
};

/// A whole get_strategies reply: the strategies plus the engine's counts (Fincept engine:
/// total listed, supported by the engine, runnable in this Python environment, and the
/// packages whose absence blocks strategies).
struct StrategyCatalog {
    QVector<Strategy> strategies;
    int total = -1;
    int supported = -1;
    int runnable = -1;
    QHash<QString, QStringList> missing_packages; ///< package -> strategy ids
};

/// Returns empty — Python is the source of truth. Strategies are loaded dynamically
/// via BacktestingService::load_strategies() → result_ready("get_strategies").
inline QVector<Strategy> default_strategies() {
    return {};
}

/// Derive categories from a loaded strategy list (no hardcoded list needed).
inline QStringList categories_from_strategies(const QVector<Strategy>& strategies) {
    QStringList cats;
    for (const auto& s : strategies)
        if (!cats.contains(s.category))
            cats.append(s.category);
    return cats;
}

/// Parse a single strategy parameter from JSON.
/// Handles both BT shape {name, label, default, min, max, step}
/// and VectorBT shape {id, name, default, min, max, step}.
inline StrategyParam param_from_json(const QJsonObject& p) {
    StrategyParam sp;
    // The key sent back to the engine. In the VectorBT shape "name" is the display
    // label ("Fast Period") and "id" the key ("fastPeriod"): preferring "name" sent
    // {"Fast Period": 5} to the provider, which read params['fastPeriod'] — every
    // VectorBT run used the defaults whatever the user entered, and optimise /
    // walk-forward grids evaluated the same strategy for every combination.
    sp.name = p.contains("id") ? p.value("id").toString() : p.value("name").toString();
    sp.label = p.contains("label") ? p.value("label").toString() : p.value("name").toString(sp.name);
    sp.default_val = p.value("default").toDouble(14);
    sp.min_val = p.value("min").toDouble(1);
    sp.max_val = p.value("max").toDouble(200);
    sp.step = p.value("step").toDouble(1);
    sp.description = p.value("description").toString();
    for (const auto& c : p.value("choices").toArray())
        sp.choices << c.toString();
    QString type = p.value("type").toString().toLower();
    if (type == QLatin1String("integer"))
        type = QStringLiteral("int");
    if (type.isEmpty()) {
        // Old catalogues carry no type: whole default and step mean an integer parameter.
        if (!sp.choices.isEmpty())
            type = QStringLiteral("choice");
        else if (p.value("default").isBool())
            type = QStringLiteral("bool");
        else
            type = (sp.default_val == static_cast<double>(static_cast<qint64>(sp.default_val)) &&
                    sp.step == static_cast<double>(static_cast<qint64>(sp.step)) && sp.step >= 1)
                       ? QStringLiteral("int")
                       : QStringLiteral("float");
    }
    sp.type = type;
    if (type == QLatin1String("bool"))
        sp.default_val = p.value("default").toBool(sp.default_val != 0) ? 1.0 : 0.0;
    if (type == QLatin1String("choice"))
        sp.default_text = p.value("default").toString(sp.choices.value(0));
    return sp;
}

/// The fields every catalogue shape shares (description, support, constraints).
inline void strategy_extras_from_json(const QJsonObject& obj, Strategy& s) {
    s.description = obj.value("description").toString();
    s.kind = obj.value("kind").toString();
    if (obj.contains("supported"))
        s.supported = obj.value("supported").toBool(true);
    else if (obj.contains("runnable"))
        s.supported = obj.value("runnable").toBool(true);
    s.reason = obj.value("reason").toString();
    const QJsonValue mp = obj.contains("missingPackages") ? obj.value("missingPackages") : obj.value("missing_packages");
    for (const auto& p : mp.toArray())
        s.missing_packages << p.toString();
    for (const auto& p : obj.value("requires").toArray())
        if (!s.supported && !s.missing_packages.contains(p.toString()) && s.reason.contains(p.toString()))
            s.missing_packages << p.toString();
    for (const auto& cv : obj.value("constraints").toArray()) {
        const QJsonObject c = cv.toObject();
        const QJsonArray lt = c.value("lt").toArray();
        if (lt.size() == 2)
            s.constraints.append({lt.at(0).toString(), lt.at(1).toString(), c.value("message").toString()});
    }
}

/// Parse strategies from a Python get_strategies JSON response.
///
/// Handles two shapes:
///   BT/backtestingpy: {"success":true,"data":{"strategies":{"category":[{id,name,params:[]}]}}}
///   VectorBT:         {"success":true,"data":[{"type","category","parameters":[{id,name,...}]}]}
inline QVector<Strategy> strategies_from_json(const QJsonObject& root) {
    QVector<Strategy> out;
    auto data_val = root.value("data");

    // VectorBT shape: data is an array
    if (data_val.isArray()) {
        for (const auto& item : data_val.toArray()) {
            auto obj = item.toObject();
            Strategy s;
            s.id = obj.value("type").toString();
            s.name = obj.value("name").toString(s.id);
            s.category = obj.value("category").toString("other");
            for (const auto& pv : obj.value("parameters").toArray())
                s.params.append(param_from_json(pv.toObject()));
            strategy_extras_from_json(obj, s);
            if (!s.id.isEmpty())
                out.append(s);
        }
        return out;
    }

    // BT/backtestingpy shape: data.strategies is an object keyed by category
    auto data_obj = data_val.isObject() ? data_val.toObject() : root;
    auto strategies_val = data_obj.value("strategies");
    if (!strategies_val.isObject())
        return out;

    auto cats_obj = strategies_val.toObject();
    for (const auto& cat : cats_obj.keys()) {
        for (const auto& sv : cats_obj.value(cat).toArray()) {
            auto obj = sv.toObject();
            Strategy s;
            s.id = obj.value("id").toString();
            s.name = obj.value("name").toString(s.id);
            s.category = cat;
            for (const auto& pv : obj.value("params").toArray())
                s.params.append(param_from_json(pv.toObject()));
            strategy_extras_from_json(obj, s);
            if (!s.id.isEmpty())
                out.append(s);
        }
    }
    return out;
}

/// The whole catalogue reply: strategies + the engine's counts (absent counts stay -1;
/// `supported` / `runnable` are then derived from the strategies' own flags).
inline StrategyCatalog catalog_from_json(const QJsonObject& root) {
    StrategyCatalog c;
    c.strategies = strategies_from_json(root);
    const QJsonValue dv = root.value("data");
    const QJsonObject d = dv.isObject() ? dv.toObject() : root;
    c.total = d.value("total").toInt(static_cast<int>(c.strategies.size()));
    c.supported = d.value("supported").toInt(-1);
    c.runnable = d.contains("runnableHere") ? d.value("runnableHere").toInt(-1) : d.value("runnable_here").toInt(-1);
    if (c.runnable < 0) {
        int n = 0;
        for (const auto& s : c.strategies)
            n += s.supported ? 1 : 0;
        c.runnable = n;
    }
    if (c.supported < 0)
        c.supported = c.runnable;
    const QJsonObject mp =
        d.contains("missingPackages") ? d.value("missingPackages").toObject() : d.value("missing_packages").toObject();
    for (auto it = mp.constBegin(); it != mp.constEnd(); ++it) {
        QStringList ids;
        for (const auto& v : it.value().toArray())
            ids << v.toString();
        c.missing_packages.insert(it.key(), ids);
    }
    return c;
}

// ── Indicator definitions ───────────────────────────────────────────────────

struct Indicator {
    QString id;
    QString label;
    QVector<StrategyParam> params;
};

/// Returns empty — Python is the source of truth. Indicators are loaded dynamically
/// via BacktestingService::execute(provider, "get_indicators", {}) → result_ready.
inline QVector<Indicator> all_indicators() {
    return {};
}

// ── Command options ─────────────────────────────────────────────────────────
// Compile-time fallbacks: the screen resets every option combo to these when the
// provider changes, and keeps them when that provider's "get_command_options"
// call fails. A successful call replaces them via
// BacktestingService::load_command_options → command_options_loaded →
// BacktestingScreen::on_command_options_loaded(). The names must be ones the
// providers accept (vectorbt rejects the short mode / analysis names).

inline QStringList position_sizing_methods() {
    return {"percent", "fixed", "kelly", "vol_target", "risk"};
}

inline QStringList optimize_objectives() {
    return {"sharpe", "sortino", "calmar", "return"};
}

inline QStringList optimize_methods() {
    return {"grid", "random"};
}

inline QStringList label_types() {
    return {"FIXLB", "MEANLB", "LEXLB", "TRENDLB", "BOLB"};
}

inline QStringList splitter_types() {
    return {"RollingSplitter", "ExpandingSplitter", "PurgedKFoldSplitter"};
}

inline QStringList signal_generators() {
    return {"RAND", "RANDX", "RANDNX", "RPROB", "RPROBX"};
}

inline QStringList indicator_signal_modes() {
    return {"crossover_signals", "threshold_signals", "breakout_signals", "mean_reversion_signals", "signal_filter"};
}

inline QStringList returns_analysis_types() {
    return {"returns_stats", "drawdowns", "ranges", "rolling"};
}

// ── Metric display helpers ──────────────────────────────────────────────────

/// Keys whose values are ratios, not percentages
inline QStringList ratio_metric_keys() {
    // alpha is an annualised return FRACTION (Jensen's alpha) and lives in pct_metric_keys().
    return {"sharpe_ratio",      "sortino_ratio",  "calmar_ratio",     "treynor_ratio",   "information_ratio",
            "profit_factor",     "beta",           "sharpeRatio",      "sortinoRatio",    "calmarRatio",
            "treynorRatio",      "informationRatio", "profitFactor",   "omegaRatio",      "tailRatio",
            "payoffRatio",       "benchmarkBeta",  "skewness",         "kurtosis",        "sqn",
            "inSampleSharpe",    "outOfSampleSharpe", "avgOosSharpe",  "degradationRatio", "zScore",
            "bestObjectiveValue", "objective",     "objectiveValue",   "benchmarkCorrelation"};
}

/// Keys whose values are already percentages (0-100 scale or 0-1 scale)
inline QStringList pct_metric_keys() {
    // pnlPercent / pnl_percent are per-trade return FRACTIONS in every provider
    // (vectorbt Return, backtesting.py ReturnPct, zipline) — they rendered as a
    // bare 0.0304 in the DETAILS table instead of 3.04%. lossRate is 1 - winRate.
    // Walk-forward returns are fractions too (every provider's windows / summary).
    return {"total_return",   "annualized_return", "max_drawdown",    "win_rate",         "volatility",
            "totalReturn",    "annualizedReturn",  "maxDrawdown",     "winRate",          "pnlPercent",
            "pnl_percent",    "lossRate",          "loss_rate",       "positiveDayRate",  "avgOosReturn",
            "oosReturnStd",   "inSampleReturn",    "outOfSampleReturn", "alpha",          "benchmarkAlpha",
            "trackingError",  "var95",             "var99",           "cvar95",           "cvar99",
            "mae",            "mfe",               "avgWinPct",       "avgLossPct",       "expectancyPct",
            "netPnlPct",      "inSampleMaxDrawdown", "outOfSampleMaxDrawdown", "compoundedOosReturn",
            "robustnessScore", "rSquared",         "exposureTime",    "buyAndHoldReturn", "cagr",
            "longWinRate",    "shortWinRate",      "avgMae",          "avgMfe",           "probLoss",
            "avgReturn",      "medianReturn",      "bestTrade",       "worstTrade",       "signalDensity"};
}

/// Keys whose values are counts (integers). Anything not listed here, in the
/// percent / ratio lists or in currency_metric_keys() renders as a plain number —
/// a bar count of 2,520 used to come out as "$2.52K".
inline QStringList count_metric_keys() {
    return {"total_trades",       "winning_trades",     "losing_trades",  "totalTrades",       "winningTrades",
            "losingTrades",       "winning_days",       "losing_days",    "consecutive_wins",  "consecutive_losses",
            "totalBars",          "labeledBars",        "returnBars",     "entryCount",        "exitCount",
            "originalEntryCount", "filteredEntryCount", "totalSignals",   "nWindows",          "nSplits",
            "iterations",         "totalCombinations",  "dataPoints",     "trainSize",         "testSize",
            "trainBars",          "testBars",           "window",         "fold",              "period",
            "totalDrawdowns",     "totalRanges",        "holdingPeriod",  "closedTrades",      "openTrades",
            "nTrades",            "nSimulations",       "nTested",        "resultsReturned",   "outOfSampleTrades",
            "drawdownCount",      "maxDrawdownDuration", "activeDuration", "longestRecovery",  "maxDuration",
            "minDuration",        "maxConsecutiveWins", "maxConsecutiveLosses", "longTrades",  "shortTrades",
            "winningStreak",      "losingStreak",       "positiveDays",   "negativeDays",      "zeroDays",
            "longestWinStreak",   "longestLossStreak",  "seed"};
}

/// Keys that hold an amount of money (account currency). averageWin / largestWin /
/// expectancy are NOT here: they are money for Zipline and Fincept but per-trade
/// return fractions for the other engines. The normalised `tradeStats` block
/// (avgWin / avgLoss in money, avgWinPct / avgLossPct as fractions) is unambiguous.
inline QStringList currency_metric_keys() {
    return {"pnl",          "commission",    "initialCapital", "initial_capital", "finalCapital", "final_capital",
            "finalValue",   "final_value",   "equity",         "totalFees",       "total_fees",   "totalSlippage",
            "total_slippage", "grossProfit", "grossLoss",      "netPnl",          "avgWin",       "avgLoss",
            "totalCommission", "avgPnl",     "stdPnl",         "finalP5",         "finalP50",     "finalP95",
            "finalMean",    "originalFinal"};
}

/// Keys that hold an instrument price (quote currency, no symbol, instrument precision).
inline QStringList price_metric_keys() {
    return {"entryPrice", "exitPrice", "entry_price", "exit_price", "price", "close", "open", "high", "low"};
}

} // namespace fincept::services::backtest
