#!/usr/bin/env python3
"""
PyPortfolioOpt service - portfolio optimisation with the PyPortfolioOpt library (runs in venv-numpy1).

Usage: python pyportfolioopt_service.py <operation> <json_data>
  json_data = {"prices_json": "<pandas DataFrame.to_json() of prices: date index, one column per asset>",
               "config": {PyPortfolioOptConfig fields, e.g. "optimization_method", "objective", "risk_free_rate",
                          "target_volatility", "target_return", "sector_mapper", "sector_constraints"}}
     instead of prices_json, prices may also be sent as
               "prices": {"SYM": [close, ...], ...}, "dates": ["YYYY-MM-DD", ...]   (one date per row)
  operations: optimize, efficient_frontier, discrete_allocation, backtest, risk_decomposition, black_litterman,
              hrp, generate_report, sensitivity_analysis, risk_parity, equal_weight, inverse_volatility,
              market_neutral, min_tracking_error, max_diversification, custom_constraints, views_optimization
Output: exactly one JSON object on stdout. Failures are {"error": "<message>"} with exit code 1 (success: 0),
including a missing PyPortfolioOpt / cvxpy. Diagnostics and tracebacks go to stderr only.
"""

import contextlib
import dataclasses
import io
import json
import math
import os
import sys
import traceback
from pathlib import Path

_HERE = Path(__file__).resolve().parent
# Wrapper directory first: core / additional_optimizers / advanced_objectives are imported by name.
sys.path.insert(0, str(_HERE))

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(str(_HERE.parent.parent))
    import fincept_json

_WRAPPER_MODULES = ("core", "additional_optimizers", "advanced_objectives")

_IMPORT_ERROR = None
try:
    import numpy as np
    import pandas as pd
    import pypfopt  # noqa: F401 - imported first so a missing library is the one named in the error
    from core import PyPortfolioOptAnalyticsEngine, PyPortfolioOptConfig
    from additional_optimizers import (
        optimize_risk_parity,
        optimize_equal_weighting,
        optimize_inverse_volatility,
        optimize_market_neutral,
        optimize_minimum_tracking_error,
        optimize_maximum_diversification,
    )
    from advanced_objectives import optimize_with_custom_constraints, optimize_with_views
except ImportError as _exc:  # numpy-ABI failures inside a library also arrive as ImportError
    _IMPORT_ERROR = _exc


class RequestError(ValueError):
    """A problem with the request itself (reported verbatim)."""


def _import_error_message(exc: ImportError) -> str:
    name = (getattr(exc, "name", None) or "").split(".")[0]
    detail = " ".join(str(exc).split()) or type(exc).__name__
    if name in _WRAPPER_MODULES:
        return f"PyPortfolioOpt wrapper failed to load: {detail}"
    return ("PyPortfolioOpt is not installed in this Python environment "
            f"({detail}). It belongs to the NumPy 1.x environment (venv-numpy1; "
            "resources/requirements-numpy1.txt pins PyPortfolioOpt==1.5.6).")


# ---------------------------------------------------------------------------
# Request helpers
# ---------------------------------------------------------------------------

def _prices_from_lists(prices, dates):
    """{"SYM": [closes]} + dates -> DataFrame (null entries become NaN; such rows are dropped later)."""
    if not isinstance(prices, dict) or not prices:
        raise RequestError("prices must be an object {symbol: [close, ...]}")
    lengths = set()
    for symbol, values in prices.items():
        if not isinstance(values, list):
            raise RequestError(f"prices[{symbol!r}] must be a list of closes")
        lengths.add(len(values))
    if len(lengths) != 1:
        raise RequestError("every prices list must have the same length")
    n_rows = lengths.pop()
    if not isinstance(dates, list):
        raise RequestError("dates (a list with one date per price row) is required with prices")
    if len(dates) != n_rows:
        raise RequestError(f"dates has {len(dates)} entries but each prices list has {n_rows}")
    index = pd.to_datetime(pd.Series(dates, dtype=object), errors="coerce")
    if index.isna().any():
        raise RequestError("dates contain values that are not dates")
    return pd.DataFrame(
        {str(symbol): pd.to_numeric(pd.Series(values, dtype=object), errors="coerce").values
         for symbol, values in prices.items()},
        index=pd.DatetimeIndex(index.values),
    )


def _parse_prices(data):
    """Parse prices from request data into a DataFrame."""
    prices_json = data.get("prices_json")
    if prices_json is not None:
        if isinstance(prices_json, (dict, list)):
            prices_json = json.dumps(prices_json)
        if not isinstance(prices_json, str) or not prices_json.strip():
            raise RequestError("prices_json must be a pandas DataFrame.to_json() string")
        # A literal JSON string must be wrapped: pandas >= 2.1 deprecates (and 3.x rejects) parsing it as a path.
        df = pd.read_json(io.StringIO(prices_json))
    elif data.get("prices") is not None:
        df = _prices_from_lists(data.get("prices"), data.get("dates"))
    else:
        raise RequestError("Price data is required: prices_json (pandas DataFrame.to_json()) "
                           "or prices {symbol: [closes]} with dates")
    if not isinstance(df, pd.DataFrame) or df.empty:
        raise RequestError("prices are empty")
    return df


def _make_config(data):
    """PyPortfolioOptConfig from request["config"]; top-level targets / risk_free_rate fill gaps."""
    config = data.get("config") or {}
    if not isinstance(config, dict):
        raise RequestError("config must be an object")
    config = dict(config)
    for key in ("target_volatility", "target_return", "risk_free_rate"):
        if config.get(key) is None and data.get(key) is not None:
            config[key] = data[key]
    valid = {f.name for f in dataclasses.fields(PyPortfolioOptConfig)}
    unknown = sorted(set(config) - valid)
    if unknown:
        raise RequestError("Unknown config field(s): " + ", ".join(unknown))
    bounds = config.get("weight_bounds")
    if isinstance(bounds, list) and len(bounds) == 2 and not isinstance(bounds[0], (list, tuple)):
        config["weight_bounds"] = tuple(bounds)
    return PyPortfolioOptConfig(**config)


def _build_engine(data, config=None):
    """Build a configured engine from request data."""
    engine = PyPortfolioOptAnalyticsEngine(config if config is not None else _make_config(data))
    engine.load_data(_parse_prices(data))
    return engine


def _opt_float(data, key, default):
    """A number from the request's top level, else its config, else `default`."""
    value = data.get(key)
    if value is None:
        value = (data.get("config") or {}).get(key)
    if value is None:
        return default
    try:
        number = float(value)
    except (TypeError, ValueError):
        raise RequestError(f"{key} must be a number")
    if not math.isfinite(number):
        raise RequestError(f"{key} must be a finite number")
    return number


def _parse_market_caps(data):
    """market_caps_json (pandas Series.to_json() string) or market_caps object -> {ticker: cap} or None.

    Parsed with json, not pd.read_json(typ="series"): read_json's date inference turns caps such
    as 3e11 into timestamps.
    """
    raw = data.get("market_caps_json")
    if raw is None:
        raw = data.get("market_caps")
    if raw is None:
        return None
    if isinstance(raw, str):
        try:
            raw = json.loads(raw)
        except ValueError:
            raise RequestError("market_caps_json must be a JSON object {ticker: market cap} (pandas Series.to_json())")
    if not isinstance(raw, dict) or not raw:
        raise RequestError("market caps must be an object {ticker: market cap}")
    return raw


def _flush_c_stdio():
    """Flush C-level stdio buffers (solver banners) so they are written while fd 1 points at stderr."""
    try:
        import ctypes
        ctypes.CDLL(None).fflush(None)
    except Exception:  # no libc handle (e.g. Windows): nothing more to do
        pass


@contextlib.contextmanager
def _stdout_to_stderr():
    """Send everything written to stdout - Python prints AND native solver output such as the
    HiGHS banner, which bypasses sys.stdout - to stderr, so the JSON payload is alone on stdout."""
    sys.stdout.flush()
    try:
        saved_fd = os.dup(1)
        os.dup2(2, 1)
    except OSError:  # no usable file descriptors (embedded use): Python-level redirect only
        saved_fd = None
    try:
        with contextlib.redirect_stdout(sys.stderr):
            yield
    finally:
        sys.stderr.flush()
        if saved_fd is not None:
            _flush_c_stdio()
            os.dup2(saved_fd, 1)
            os.close(saved_fd)


def _safe_float(v):
    """Convert numpy types to native Python for JSON serialization (NaN / inf -> None)."""
    if isinstance(v, (np.floating, np.integer)):
        v = float(v)
    if isinstance(v, float) and not np.isfinite(v):
        return None
    return v


def _safe_dict(d):
    """Recursively convert numpy / pandas types in a dict (a Series keeps its labels)."""
    if isinstance(d, pd.Series):
        d = d.to_dict()
    elif isinstance(d, np.ndarray):
        d = d.tolist()
    if isinstance(d, dict):
        return {(k if isinstance(k, str) else str(k)): _safe_dict(v) for k, v in d.items()}
    if isinstance(d, (list, tuple)):
        return [_safe_dict(i) for i in d]
    return _safe_float(d)


def _run_info(engine):
    """What the last optimisation actually did (objective, targets, fallbacks)."""
    info = {"objective_used": engine.objective_used}
    if engine.targets_used:
        info["targets_used"] = dict(engine.targets_used)
    if engine.notes:
        info["notes"] = list(engine.notes)
    return info


def _perf(ret, vol, sharpe):
    return {"expected_annual_return": ret, "annual_volatility": vol, "sharpe_ratio": sharpe}


# ---------------------------------------------------------------------------
# Core operations
# ---------------------------------------------------------------------------

def optimize_portfolio(data):
    """Optimize portfolio using PyPortfolioOpt"""
    engine = _build_engine(data)
    weights = engine.optimize_portfolio()
    result = {"weights": weights, "performance_metrics": _perf(*engine.portfolio_performance())}
    result.update(_run_info(engine))
    return result


def generate_efficient_frontier(data):
    """Generate efficient frontier"""
    num_points = data.get("num_points", 20)
    engine = _build_engine(data)
    frontier_data = engine.generate_efficient_frontier(num_points)

    return {
        "returns": frontier_data["returns"],
        "volatility": frontier_data["volatility"],
        "sharpe_ratios": frontier_data["sharpe_ratios"],
        "num_points": frontier_data["num_points"],
        "skipped_points": frontier_data["skipped_points"],
    }


def calculate_discrete_allocation(data):
    """Calculate discrete allocation"""
    weights = data.get("weights")
    if not isinstance(weights, dict) or not weights:
        raise RequestError("weights {ticker: weight} is required")
    total_portfolio_value = data.get("total_portfolio_value", 10000.0)

    engine = _build_engine(data, config=PyPortfolioOptConfig())
    engine.weights = weights

    allocation_result = engine.discrete_allocation(total_portfolio_value=total_portfolio_value)

    return {
        "allocation": allocation_result["allocation"],
        "leftover_cash": allocation_result["leftover_cash"],
        "total_value": allocation_result["total_value"],
        "allocated_value": allocation_result["allocated_value"],
        "allocation_method": allocation_result["allocation_method"],
    }


def run_backtest(data):
    """Run rolling-window backtest using core engine"""
    engine = _build_engine(data)
    rebalance_frequency = data.get("rebalance_frequency", 21)
    lookback_period = data.get("lookback_period", 252)
    start_date = data.get("start_date", None)

    backtest_df = engine.backtest_strategy(
        rebalance_frequency=rebalance_frequency,
        lookback_period=lookback_period,
        start_date=start_date
    )
    results = engine.backtest_results

    return {
        "dates": [d.isoformat() for d in backtest_df.index],
        "cumulative_returns": backtest_df["cumulative_return"].tolist(),
        "drawdowns": backtest_df["drawdown"].tolist(),
        "annual_return": results["annual_return"],
        "annual_volatility": results["annual_volatility"],
        "sharpe_ratio": results["sharpe_ratio"],
        "max_drawdown": results["max_drawdown"],
        "calmar_ratio": results["calmar_ratio"],
        "num_rebalances": results["num_rebalances"],
        "failed_rebalances": results["failed_rebalances"],
    }


def calculate_risk_decomposition(data):
    """Calculate risk decomposition using core engine"""
    engine = _build_engine(data)
    weights_input = data.get("weights", None)

    if weights_input:
        engine.weights = weights_input
    else:
        engine.optimize_portfolio()

    decomp = engine.risk_decomposition()

    return {
        "marginal_contribution": decomp["marginal_contribution"].to_dict(),
        "component_contribution": decomp["component_contribution"].to_dict(),
        "percentage_contribution": decomp["percentage_contribution"].to_dict(),
        "portfolio_volatility": float(decomp["portfolio_volatility"])
    }


def black_litterman_optimization(data):
    """Black-Litterman optimization using core engine"""
    engine = _build_engine(data)

    weights = engine.black_litterman_optimization(
        market_caps=_parse_market_caps(data),
        views=data.get("views", None),
        view_confidences=data.get("view_confidences", None),
        tau=data.get("tau", None),
    )
    details = engine.bl_details

    result = {
        "weights": dict(weights),
        # Measured on the Black-Litterman posterior the weights were optimised on
        "performance_metrics": _perf(*engine.portfolio_performance()),
        "performance_basis": "black_litterman_posterior",
        "performance_metrics_historical": _perf(*engine.historical_performance()),
        "prior_returns": details["prior_returns"],
        "posterior_returns": details["posterior_returns"],
        "market_weights": details["market_weights"],
        "market_caps_source": details["market_caps_source"],
        "views_applied": details["views_applied"],
        "view_confidences": details["view_confidences"],
        "omega_method": details["omega_method"],
        "tau": details["tau"],
        "risk_aversion": details["risk_aversion"],
    }
    result.update(_run_info(engine))
    return result


def hrp_optimization(data):
    """Hierarchical Risk Parity optimization using core engine"""
    engine = _build_engine(data)
    weights = engine.hrp_optimization()

    result = {"weights": dict(weights), "performance_metrics": _perf(*engine.portfolio_performance())}
    result.update(_run_info(engine))
    return result


def generate_report(data):
    """Generate comprehensive portfolio report"""
    engine = _build_engine(data)
    engine.optimize_portfolio()
    return engine.generate_report()


def sensitivity_analysis(data):
    """Run sensitivity analysis on a parameter"""
    engine = _build_engine(data)
    parameter = data.get("parameter", "risk_free_rate")
    values = data.get("values", None)

    engine.optimize_portfolio()
    results_df = engine.sensitivity_analysis(parameter=parameter, values=values)

    return {
        "parameter": parameter,
        "results": results_df.to_dict(orient="records")
    }


# ---------------------------------------------------------------------------
# Additional optimizers (additional_optimizers.py)
# ---------------------------------------------------------------------------

def risk_parity(data):
    """Risk Parity (equal risk contribution) optimization"""
    prices_df = _build_engine(data, config=PyPortfolioOptConfig()).prices
    risk_measure = data.get("risk_measure", "volatility")
    return optimize_risk_parity(prices_df, risk_measure=risk_measure,
                                risk_free_rate=_opt_float(data, "risk_free_rate", 0.02))


def equal_weight(data):
    """Equal weighting (1/N) portfolio"""
    prices_df = _build_engine(data, config=PyPortfolioOptConfig()).prices
    return optimize_equal_weighting(prices_df, risk_free_rate=_opt_float(data, "risk_free_rate", 0.02))


def inverse_volatility(data):
    """Inverse volatility weighting"""
    prices_df = _build_engine(data, config=PyPortfolioOptConfig()).prices
    return optimize_inverse_volatility(prices_df, risk_free_rate=_opt_float(data, "risk_free_rate", 0.02))


def market_neutral(data):
    """Market neutral (dollar-neutral long/short) portfolio"""
    prices_df = _build_engine(data, config=PyPortfolioOptConfig()).prices
    return optimize_market_neutral(
        prices_df,
        long_exposure=data.get("long_exposure", 1.0),
        short_exposure=data.get("short_exposure", -1.0),
        objective=data.get("objective", "max_sharpe"),
        risk_aversion=_opt_float(data, "risk_aversion", 1.0),
        target_volatility=_opt_float(data, "target_volatility", None),
        target_return=_opt_float(data, "target_return", None),
    )


def min_tracking_error(data):
    """Minimum tracking error relative to benchmark"""
    prices_df = _build_engine(data, config=PyPortfolioOptConfig()).prices
    return optimize_minimum_tracking_error(
        prices_df,
        benchmark_weights=data.get("benchmark_weights", {}),
        target_return=_opt_float(data, "target_return", None),
        risk_free_rate=_opt_float(data, "risk_free_rate", 0.0),
    )


def max_diversification(data):
    """Maximum diversification portfolio"""
    prices_df = _build_engine(data, config=PyPortfolioOptConfig()).prices
    return optimize_maximum_diversification(prices_df, risk_free_rate=_opt_float(data, "risk_free_rate", 0.0))


# ---------------------------------------------------------------------------
# Advanced objectives (advanced_objectives.py)
# ---------------------------------------------------------------------------

def custom_constraints(data):
    """Optimize with custom sector constraints"""
    prices_df = _build_engine(data, config=PyPortfolioOptConfig()).prices
    weight_bounds_list = data.get("weight_bounds", [0, 1])
    if not isinstance(weight_bounds_list, (list, tuple)) or len(weight_bounds_list) != 2:
        raise RequestError("weight_bounds must be [lower, upper]")

    return optimize_with_custom_constraints(
        prices=prices_df,
        objective=data.get("objective", "max_sharpe"),
        sector_mapper=data.get("sector_mapper", None),
        sector_lower=data.get("sector_lower", None),
        sector_upper=data.get("sector_upper", None),
        weight_bounds=tuple(weight_bounds_list),
        risk_free_rate=_opt_float(data, "risk_free_rate", 0.0),
        risk_aversion=_opt_float(data, "risk_aversion", 1.0),
    )


def views_optimization(data):
    """Optimize using Black-Litterman with investor views (advanced_objectives)"""
    prices_df = _build_engine(data, config=PyPortfolioOptConfig()).prices
    market_caps = _parse_market_caps(data)

    return optimize_with_views(
        prices=prices_df,
        views=data.get("views", {}),
        view_confidences=data.get("view_confidences", None),
        market_caps=pd.Series(market_caps) if market_caps is not None else None,
        risk_aversion=_opt_float(data, "risk_aversion", 1.0),
        risk_free_rate=_opt_float(data, "risk_free_rate", 0.0),
        tau=_opt_float(data, "tau", 0.05),
    )


# ---------------------------------------------------------------------------
# Router
# ---------------------------------------------------------------------------

_DISPATCH = {
    # --- Core operations ---
    "optimize": optimize_portfolio,
    "efficient_frontier": generate_efficient_frontier,
    "discrete_allocation": calculate_discrete_allocation,
    "backtest": run_backtest,
    "risk_decomposition": calculate_risk_decomposition,
    "black_litterman": black_litterman_optimization,
    "hrp": hrp_optimization,
    "generate_report": generate_report,
    "sensitivity_analysis": sensitivity_analysis,
    # --- Additional optimizers ---
    "risk_parity": risk_parity,
    "equal_weight": equal_weight,
    "inverse_volatility": inverse_volatility,
    "market_neutral": market_neutral,
    "min_tracking_error": min_tracking_error,
    "max_diversification": max_diversification,
    # --- Advanced objectives ---
    "custom_constraints": custom_constraints,
    "views_optimization": views_optimization,
}


def run(args):
    """Run one operation; returns the payload dict ({"error": ...} on failure). Never raises."""
    if _IMPORT_ERROR is not None:
        return {"error": _import_error_message(_IMPORT_ERROR)}
    if len(args) < 2:
        return {"error": "Invalid arguments. Expected: <operation> <json_data>"}

    operation = args[0]
    try:
        request_data = json.loads(args[1])
    except ValueError as exc:
        return {"error": f"Invalid JSON request: {exc}"}
    if not isinstance(request_data, dict):
        return {"error": "The request must be a JSON object"}

    handler = _DISPATCH.get(operation)
    if handler is None:
        return {"error": f"Unknown operation: {operation}"}

    try:
        # Anything a library or solver prints must not land in front of the JSON payload.
        with _stdout_to_stderr():
            return _safe_dict(handler(request_data))
    except Exception as exc:
        traceback.print_exc(file=sys.stderr)
        message = " ".join(str(exc).split()) or type(exc).__name__
        return {"error": f"PyPortfolioOpt error: {message}"}


def main(args):
    """
    Main entry point for worker pool.
    Args: [operation, json_data]
    Returns: JSON string (one line; NaN / Infinity become null)
    """
    return fincept_json.dumps(run(args))


if __name__ == '__main__':
    payload = run(sys.argv[1:])
    fincept_json.emit(payload)
    sys.exit(1 if isinstance(payload, dict) and "error" in payload else 0)
