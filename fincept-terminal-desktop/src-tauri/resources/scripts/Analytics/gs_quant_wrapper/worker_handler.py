"""
GS-Quant Wrapper Worker Handler
================================
Dispatch router for Tauri Rust commands → Python gs_quant_wrapper operations.
Pattern: main(args) dispatches [operation, json_data].
"""

import sys
import os
import json
import traceback
import numpy as np
import pandas as pd
from datetime import datetime

# Add parent directory to path for absolute imports when run as script
_script_dir = os.path.dirname(os.path.abspath(__file__))
_parent_dir = os.path.dirname(_script_dir)
if _parent_dir not in sys.path:
    sys.path.insert(0, _parent_dir)


def _series_from_list(values, dates=None, name="series"):
    """Helper: Convert list of values (+ optional dates) to pd.Series."""
    if dates:
        idx = pd.to_datetime(dates)
    else:
        idx = pd.date_range("2020-01-01", periods=len(values), freq="B")
    return pd.Series(values, index=idx, name=name, dtype=float)


def _df_from_dict(data_dict, dates=None):
    """Helper: Convert dict-of-lists to pd.DataFrame."""
    if dates:
        idx = pd.to_datetime(dates)
    else:
        first_key = next(iter(data_dict))
        idx = pd.date_range("2020-01-01", periods=len(data_dict[first_key]), freq="B")
    return pd.DataFrame(data_dict, index=idx)


# ============================================================================
# OPERATION HANDLERS
# ============================================================================

def op_performance_analysis(data):
    """Full performance analysis: returns, risk, distribution metrics."""
    from gs_quant_wrapper.timeseries_analytics import TimeseriesAnalytics, TimeseriesConfig

    prices = data.get("prices", [])
    benchmark_prices = data.get("benchmark_prices", [])
    risk_free_rate = data.get("risk_free_rate", 0.02)
    window = data.get("window", 20)

    config = TimeseriesConfig(window=window)
    ts = TimeseriesAnalytics(config)

    price_series = _series_from_list(prices, data.get("dates"))
    returns = ts.calculate_returns(price_series)

    benchmark_ret = None
    if benchmark_prices:
        bench_series = _series_from_list(benchmark_prices, data.get("dates"))
        benchmark_ret = ts.calculate_returns(bench_series)

    result = ts.full_performance_analysis(returns, benchmark_ret, risk_free_rate)
    return result


def op_technical_analysis(data):
    """Technical indicators summary: SMA, EMA, RSI, MACD, Bollinger Bands, ATR."""
    from gs_quant_wrapper.timeseries_analytics import TimeseriesAnalytics, TimeseriesConfig

    prices = data.get("prices", [])
    high = data.get("high", [])
    low = data.get("low", [])
    dates = data.get("dates")

    config = TimeseriesConfig(window=data.get("window", 20))
    ts = TimeseriesAnalytics(config)

    price_series = _series_from_list(prices, dates)
    high_series = _series_from_list(high, dates) if high else None
    low_series = _series_from_list(low, dates) if low else None

    result = ts.technical_analysis_summary(price_series, high_series, low_series)
    return result


def op_risk_metrics(data):
    """Comprehensive risk metrics: volatility, VaR, CVaR, drawdown, ratios."""
    from gs_quant_wrapper import ts_risk_measures as risk

    returns_list = data.get("returns", [])
    risk_free_rate = data.get("risk_free_rate", 0.0)
    dates = data.get("dates")
    ret = _series_from_list(returns_list, dates, "returns")

    vol = risk.volatility(ret)
    dd = risk.max_drawdown(ret)
    sharpe = risk.sharpe_ratio(ret, risk_free_rate)
    sortino_val = risk.sortino_ratio(ret, risk_free_rate)
    calmar_val = risk.calmar_ratio(ret)
    omega_val = risk.omega_ratio(ret)
    var_95 = risk.value_at_risk(ret, 0.95)
    var_99 = risk.value_at_risk(ret, 0.99)
    dside = risk.downside_risk(ret)
    daily_r = risk.daily_risk(ret)
    annual_r = risk.annual_risk(ret)
    dl = risk.drawdown_length(ret)
    mrp = risk.max_recovery_period(ret)

    result = {
        "volatility_annualized": float(vol),
        "daily_risk": float(daily_r),
        "annual_risk": float(annual_r),
        "downside_risk": float(dside),
        "max_drawdown": dd["max_drawdown"],
        "max_drawdown_pct": dd["max_drawdown_pct"],
        "peak_date": str(dd["peak_date"]),
        "trough_date": str(dd["trough_date"]),
        "recovery_date": str(dd.get("recovery_date")),
        "max_drawdown_length": int(dl.max()),
        "max_recovery_period": int(mrp),
        "sharpe_ratio": float(sharpe),
        "sortino_ratio": float(sortino_val),
        "calmar_ratio": float(calmar_val),
        "omega_ratio": float(omega_val),
        "var_95": float(var_95),
        "var_99": float(var_99),
    }
    return result


def op_portfolio_analytics(data):
    """Portfolio analytics with benchmark comparison."""
    from gs_quant_wrapper import ts_portfolio_analytics as port

    returns_list = data.get("returns", [])
    benchmark_list = data.get("benchmark_returns", [])
    risk_free_rate = data.get("risk_free_rate", 0.0)
    dates = data.get("dates")

    port_ret = _series_from_list(returns_list, dates, "portfolio")
    bench_ret = _series_from_list(benchmark_list, dates, "benchmark")

    result = {
        "pnl_final": float(port.portfolio_pnl(port_ret, 1.0).iloc[-1]),
        "alpha": float(port.portfolio_alpha(port_ret, bench_ret, risk_free_rate)),
        "annual_risk": float(port.portfolio_annual_risk(port_ret)),
        "sharpe_ratio": float(port.portfolio_sharpe_ratio(port_ret, risk_free_rate)),
        "sortino_ratio": float(port.portfolio_sortino_ratio(port_ret, risk_free_rate)),
        "calmar_ratio": float(port.portfolio_calmar_ratio(port_ret)),
        "treynor_measure": float(port.portfolio_treynor_measure(port_ret, bench_ret, risk_free_rate)),
        "modigliani_ratio": float(port.portfolio_modigliani_ratio(port_ret, bench_ret, risk_free_rate)),
        "max_drawdown": float(port.portfolio_max_drawdown(port_ret)),
        "drawdown_length": int(port.portfolio_drawdown_length(port_ret)),
        "tracking_error": float(port.portfolio_tracking_error(port_ret, bench_ret)),
        "information_ratio": float(port.portfolio_information_ratio(port_ret, bench_ret)),
        "hit_rate": float(port.portfolio_hit_rate(port_ret)),
        "r_squared": float(port.portfolio_r_squared(port_ret, bench_ret)),
        "skewness": float(port.portfolio_skewness(port_ret)),
        "kurtosis": float(port.portfolio_kurtosis(port_ret)),
        "jensen_alpha": float(port.portfolio_jensen_alpha(port_ret, bench_ret, risk_free_rate)),
        "jensen_alpha_bull": float(port.portfolio_jensen_alpha_bull(port_ret, bench_ret, risk_free_rate)),
        "jensen_alpha_bear": float(port.portfolio_jensen_alpha_bear(port_ret, bench_ret, risk_free_rate)),
    }

    capture = port.portfolio_capture_ratio(port_ret, bench_ret)
    result["up_capture"] = capture["up_capture"]
    result["down_capture"] = capture["down_capture"]
    result["capture_ratio"] = capture["capture_ratio"]

    return result


def op_volatility_surface(data):
    """Forward volatility and variance term structure."""
    from gs_quant_wrapper import ts_risk_measures as risk

    returns_list = data.get("returns", [])
    tenors = data.get("tenors", [21, 63, 126, 252])
    dates = data.get("dates")

    ret = _series_from_list(returns_list, dates, "returns")

    fvt = risk.forward_vol_term(ret, tenors)
    fvar = risk.forward_var_term(ret, tenors)

    vol_term = []
    for _, row in fvt.iterrows():
        vol_term.append({
            "tenor_days": int(row["tenor_days"]),
            "realized_vol": float(row["realized_vol"]) if not np.isnan(row["realized_vol"]) else None,
            "forward_vol": float(row["forward_vol"]) if not np.isnan(row["forward_vol"]) else None,
        })

    var_term = []
    for _, row in fvar.iterrows():
        var_term.append({
            "tenor_days": int(row["tenor_days"]),
            "realized_var": float(row["realized_var"]) if not np.isnan(row["realized_var"]) else None,
            "forward_var": float(row["forward_var"]) if not np.isnan(row["forward_var"]) else None,
        })

    return {"vol_term_structure": vol_term, "var_term_structure": var_term}


def op_statistics(data):
    """Descriptive statistics: mean, std, skew, kurtosis, percentiles, z-scores."""
    from gs_quant_wrapper import ts_math_statistics as ms

    values = data.get("values", [])
    dates = data.get("dates")
    series = _series_from_list(values, dates, "data")

    pcts = ms.percentiles(series)

    result = {
        "mean": ms.mean(series),
        "median": ms.median(series),
        "std": ms.std(series),
        "variance": ms.var(series),
        "min": ms.min_(series),
        "max": ms.max_(series),
        "range": ms.range_(series),
        "skewness": ms.skewness(series),
        "kurtosis": ms.kurtosis(series),
        "count": ms.count(series),
        "sum": ms.sum_(series),
        "semi_variance": ms.semi_variance(series),
        "realized_variance": ms.realized_var(series),
        "percentiles": {str(int(k)): float(v) for k, v in pcts.items()},
    }
    return result


def op_correlation_analysis(data):
    """Correlation, beta, R-squared between two series."""
    from gs_quant_wrapper import ts_math_statistics as ms

    x_values = data.get("x", [])
    y_values = data.get("y", [])
    dates = data.get("dates")

    x = _series_from_list(x_values, dates, "x")
    y = _series_from_list(y_values, dates, "y")

    result = {
        "correlation": ms.correlation(x, y),
        "covariance": ms.cov(x, y),
        "beta": ms.beta(x, y),
        "r_squared": ms.r_squared(x, y),
    }
    return result


def op_backtest(data):
    """Run a backtest strategy: buy-and-hold, momentum, mean-reversion, rebalancing."""
    from gs_quant_wrapper.backtest_analytics import BacktestEngine, BacktestConfig

    prices_dict = data.get("prices", {})
    strategy = data.get("strategy", "buy_and_hold")
    initial_capital = data.get("initial_capital", 100000)
    commission = data.get("commission", 0.001)
    lookback = data.get("lookback", 20)
    rebalance_freq = data.get("rebalance_freq", 20)
    dates = data.get("dates")

    config = BacktestConfig(
        initial_capital=initial_capital,
        commission=commission,
    )
    engine = BacktestEngine(config)

    # prices can be a dict of ticker->list or a single list
    if isinstance(prices_dict, dict):
        df = _df_from_dict(prices_dict, dates)
    else:
        df = pd.DataFrame({"asset": prices_dict}, index=pd.date_range("2020-01-01", periods=len(prices_dict), freq="B"))

    if strategy == "buy_and_hold":
        ticker = data.get("ticker", df.columns[0])
        prices_series = df[ticker] if ticker in df.columns else df.iloc[:, 0]
        result = engine.backtest_buy_and_hold(prices_series)
    elif strategy == "momentum":
        ticker = data.get("ticker", df.columns[0])
        prices_series = df[ticker] if ticker in df.columns else df.iloc[:, 0]
        result = engine.backtest_momentum(prices_series, lookback=lookback)
    elif strategy == "mean_reversion":
        ticker = data.get("ticker", df.columns[0])
        prices_series = df[ticker] if ticker in df.columns else df.iloc[:, 0]
        result = engine.backtest_mean_reversion(prices_series, lookback=lookback)
    elif strategy == "rebalancing":
        weights = data.get("weights", {col: 1.0 / len(df.columns) for col in df.columns})
        result = engine.backtest_rebalancing(df, weights, rebalance_freq=rebalance_freq)
    else:
        result = engine.backtest_buy_and_hold(df.iloc[:, 0])

    # Serialize
    serializable = {}
    for k, v in result.items():
        if isinstance(v, pd.Series):
            serializable[k] = {str(d): float(val) for d, val in v.items()}
        elif isinstance(v, pd.DataFrame):
            serializable[k] = {col: {str(d): float(val) for d, val in v[col].items()} for col in v.columns}
        elif isinstance(v, (np.floating, np.integer)):
            serializable[k] = float(v)
        elif isinstance(v, dict):
            serializable[k] = {str(kk): float(vv) if isinstance(vv, (float, int, np.floating, np.integer)) else str(vv) for kk, vv in v.items()}
        else:
            serializable[k] = v

    return serializable


def op_basket_backtest(data):
    """Basket/composite portfolio backtest."""
    from gs_quant_wrapper import ts_portfolio_analytics as port

    components_dict = data.get("components", {})
    weights = data.get("weights")
    initial_value = data.get("initial_value", 100.0)
    rebalance = data.get("rebalance", "daily")
    dates = data.get("dates")

    components_df = _df_from_dict(components_dict, dates)

    bt = port.backtest_basket(components_df, weights, initial_value, rebalance)

    result = {
        "final_value": float(bt["basket_value"].iloc[-1]),
        "total_return": float((bt["basket_value"].iloc[-1] / initial_value) - 1),
        "max_drawdown": float(bt["drawdown"].min()),
        "basket_values": {str(d): float(v) for d, v in bt["basket_value"].items()},
        "drawdown_series": {str(d): float(v) for d, v in bt["drawdown"].items()},
    }
    return result


def op_datetime_utils(data):
    """Date/time utilities: business days, day count fractions, date ranges."""
    from gs_quant_wrapper.datetime_utils import DateTimeUtils

    dt = DateTimeUtils()
    operation = data.get("sub_operation", "business_days")

    if operation == "business_days":
        start = data.get("start_date", "2024-01-01")
        end = data.get("end_date", "2024-12-31")
        result = {"business_days": dt.count_business_days(start, end)}
    elif operation == "day_count_fraction":
        start = data.get("start_date", "2024-01-01")
        end = data.get("end_date", "2024-12-31")
        convention = data.get("convention", "ACT/365")
        result = {"day_count_fraction": dt.calculate_day_count_fraction(start, end, convention)}
    elif operation == "add_business_days":
        date_str = data.get("date", "2024-01-01")
        n = data.get("days", 10)
        new_date = dt.add_business_days(date_str, n)
        result = {"result_date": str(new_date)}
    elif operation == "date_range":
        start = data.get("start_date", "2024-01-01")
        end = data.get("end_date", "2024-12-31")
        rng = dt.generate_date_range(start, end)
        result = {"dates": [str(d.date()) for d in rng], "count": len(rng)}
    elif operation == "analyze":
        start = data.get("start_date", "2024-01-01")
        end = data.get("end_date", "2024-12-31")
        result = dt.analyze_date_range(start, end)
    else:
        result = {"error": f"Unknown sub_operation: {operation}"}

    return result


def op_greeks(data):
    """Calculate Greeks for a derivative instrument."""
    from gs_quant_wrapper.risk_analytics import RiskAnalytics, RiskConfig

    risk = RiskAnalytics(RiskConfig())

    spot = data.get("spot", 100)
    strike = data.get("strike", 100)
    expiry = data.get("expiry", 0.25)
    rate = data.get("rate", 0.05)
    vol = data.get("vol", 0.2)
    option_type = data.get("option_type", "call")

    greeks = risk.calculate_all_greeks(spot, strike, expiry, rate, vol, option_type)
    return greeks.__dict__ if hasattr(greeks, "__dict__") else greeks


def op_var_analysis(data):
    """Value at Risk analysis: parametric, historical, Monte Carlo, CVaR."""
    from gs_quant_wrapper.risk_analytics import RiskAnalytics, RiskConfig

    returns_list = data.get("returns", [])
    confidence = data.get("confidence", 0.95)
    position_value = data.get("position_value", 1000000)
    dates = data.get("dates")

    ret = _series_from_list(returns_list, dates, "returns")
    risk = RiskAnalytics(RiskConfig())

    parametric = risk.calculate_parametric_var(ret, confidence)
    historical = risk.calculate_historical_var(ret, confidence)
    mc = risk.calculate_monte_carlo_var(ret, confidence)
    cvar = risk.calculate_cvar(ret, confidence)

    def _var_to_dict(v):
        if isinstance(v, dict):
            return {k: float(val) if isinstance(val, (float, int, np.floating, np.integer)) else str(val) for k, val in v.items()}
        if hasattr(v, "__dict__"):
            return {k: float(val) if isinstance(val, (float, int, np.floating, np.integer)) else str(val) for k, val in v.__dict__.items()}
        return {"value": float(v)}

    result = {
        "parametric_var": _var_to_dict(parametric),
        "historical_var": _var_to_dict(historical),
        "monte_carlo_var": _var_to_dict(mc),
        "cvar": _var_to_dict(cvar),
        "position_value": position_value,
        "confidence": confidence,
    }
    return result


def op_stress_test(data):
    """Stress testing with standard market scenarios."""
    from gs_quant_wrapper.risk_analytics import RiskAnalytics, RiskConfig

    returns_list = data.get("returns", [])
    position_value = data.get("position_value", 1000000)
    dates = data.get("dates")

    ret = _series_from_list(returns_list, dates, "returns")
    risk_analytics = RiskAnalytics(RiskConfig())

    scenarios = risk_analytics.run_all_standard_scenarios(ret, position_value)

    # Serialize scenario results
    serializable = {}
    for name, scenario in scenarios.items():
        if isinstance(scenario, dict):
            s = {}
            for k, v in scenario.items():
                if isinstance(v, (float, int, np.floating, np.integer)):
                    s[k] = float(v)
                elif isinstance(v, pd.Series):
                    s[k] = float(v.iloc[-1]) if len(v) > 0 else 0
                else:
                    s[k] = str(v)
            serializable[name] = s
        else:
            serializable[name] = str(scenario)

    return serializable


def op_instrument_create(data):
    """Create and summarize a financial instrument."""
    from gs_quant_wrapper.instrument_wrapper import InstrumentFactory

    factory = InstrumentFactory()
    instrument_type = data.get("instrument_type", "equity")

    creators = {
        "equity": lambda: factory.create_equity(data.get("ticker", "AAPL"), data.get("exchange", "NYSE")),
        "etf": lambda: factory.create_etf(data.get("ticker", "SPY"), data.get("exchange", "NYSE")),
        "bond": lambda: factory.create_bond(
            data.get("issuer", "US TREASURY"),
            data.get("coupon", 0.05),
            data.get("maturity_years", 10),
        ),
        "option": lambda: factory.create_equity_option(
            data.get("underlier", "AAPL"),
            data.get("strike", 150),
            data.get("expiry", "2025-06-20"),
            data.get("option_type", "call"),
        ),
        "swap": lambda: factory.create_interest_rate_swap(
            data.get("notional", 1000000),
            data.get("fixed_rate", 0.03),
            data.get("tenor", "5y"),
        ),
        "fx_spot": lambda: factory.create_fx_spot(
            data.get("pair", "EUR/USD"),
        ),
        "cds": lambda: factory.create_cds(
            data.get("reference_entity", "AAPL"),
            data.get("notional", 10000000),
        ),
        "future": lambda: factory.create_equity_future(
            data.get("underlier", "SPX"),
            data.get("expiry", "2025-03-21"),
        ),
    }

    creator = creators.get(instrument_type)
    if creator:
        instrument = creator()
        summary = factory.get_instrument_summary(instrument)
        if isinstance(summary, dict):
            return summary
        return {"instrument": str(summary)}
    else:
        return {"error": f"Unknown instrument type: {instrument_type}"}


def op_comprehensive_risk_report(data):
    """Full risk report combining Greeks, VaR, scenarios."""
    from gs_quant_wrapper.risk_analytics import RiskAnalytics, RiskConfig

    returns_list = data.get("returns", [])
    position_value = data.get("position_value", 1000000)
    confidence = data.get("confidence", 0.95)
    dates = data.get("dates")

    ret = _series_from_list(returns_list, dates, "returns")
    risk_analytics = RiskAnalytics(RiskConfig())

    report = risk_analytics.comprehensive_risk_report(ret, position_value, confidence)

    # Serialize
    def _serialize(obj):
        if isinstance(obj, dict):
            return {k: _serialize(v) for k, v in obj.items()}
        elif isinstance(obj, (list, tuple)):
            return [_serialize(v) for v in obj]
        elif isinstance(obj, pd.Series):
            return {str(d): float(v) for d, v in obj.head(50).items()}
        elif isinstance(obj, pd.DataFrame):
            return {col: {str(d): float(v) for d, v in obj[col].head(50).items()} for col in obj.columns}
        elif isinstance(obj, (np.floating, np.integer)):
            return float(obj)
        elif isinstance(obj, np.ndarray):
            return obj.tolist()
        elif hasattr(obj, "__dict__"):
            return _serialize(obj.__dict__)
        elif isinstance(obj, (datetime,)):
            return str(obj)
        else:
            return obj

    return _serialize(report)


def op_historical_simulation(data):
    """Historical simulation for PnL estimation."""
    from gs_quant_wrapper import ts_portfolio_analytics as port

    returns_list = data.get("returns", [])
    position_value = data.get("position_value", 1000000)
    scenarios = data.get("scenarios")
    dates = data.get("dates")

    ret = _series_from_list(returns_list, dates, "returns")
    sim = port.historical_simulation_estimated_pnl(ret, position_value, scenarios)

    result = {
        "scenarios_count": len(sim),
        "var_5_pnl": float(sim[sim["percentile"] <= 5]["pnl"].iloc[-1]) if len(sim[sim["percentile"] <= 5]) > 0 else 0,
        "var_1_pnl": float(sim[sim["percentile"] <= 1]["pnl"].iloc[-1]) if len(sim[sim["percentile"] <= 1]) > 0 else 0,
        "median_pnl": float(sim[sim["percentile"].between(49, 51)]["pnl"].mean()) if len(sim[sim["percentile"].between(49, 51)]) > 0 else 0,
        "best_pnl": float(sim["pnl"].max()),
        "worst_pnl": float(sim["pnl"].min()),
        "pnl_distribution": sim[["percentile", "pnl"]].to_dict(orient="records"),
    }
    return result


# ============================================================================
# DISPATCH TABLE
# ============================================================================

OPERATIONS = {
    "performance_analysis": op_performance_analysis,
    "technical_analysis": op_technical_analysis,
    "risk_metrics": op_risk_metrics,
    "portfolio_analytics": op_portfolio_analytics,
    "volatility_surface": op_volatility_surface,
    "statistics": op_statistics,
    "correlation_analysis": op_correlation_analysis,
    "backtest": op_backtest,
    "basket_backtest": op_basket_backtest,
    "datetime_utils": op_datetime_utils,
    "greeks": op_greeks,
    "var_analysis": op_var_analysis,
    "stress_test": op_stress_test,
    "instrument_create": op_instrument_create,
    "comprehensive_risk_report": op_comprehensive_risk_report,
    "historical_simulation": op_historical_simulation,
}


def dispatch(operation, data):
    """Dispatch to operation handler."""
    handler = OPERATIONS.get(operation)
    if handler is None:
        return {"error": f"Unknown operation: {operation}", "available": list(OPERATIONS.keys())}
    return handler(data)


def main(args):
    """Entry point: args = [operation, json_data]"""
    if len(args) < 2:
        result = {"error": "Usage: worker_handler.py <operation> <json_data>"}
        print(json.dumps(result))
        return

    operation = args[0]
    try:
        data = json.loads(args[1])
    except json.JSONDecodeError as e:
        result = {"error": f"Invalid JSON: {e}"}
        print(json.dumps(result))
        return

    try:
        result = dispatch(operation, data)
        print(json.dumps(result, default=str))
    except Exception as e:
        result = {"error": str(e), "traceback": traceback.format_exc()}
        print(json.dumps(result, default=str))


if __name__ == "__main__":
    main(sys.argv[1:])
