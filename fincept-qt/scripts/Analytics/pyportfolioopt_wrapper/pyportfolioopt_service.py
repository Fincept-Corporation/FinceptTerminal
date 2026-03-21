#!/usr/bin/env python3
"""
PyPortfolioOpt Worker Handler
Provides a main() function interface for worker pool execution.
Covers 100% of pyportfolioopt_wrapper: core, additional_optimizers, advanced_objectives.
"""

import json
import sys
import numpy as np
from pathlib import Path

# Add wrapper directory to path
sys.path.insert(0, str(Path(__file__).parent))

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
import pandas as pd


def _parse_prices(data):
    """Parse prices from request data into a DataFrame."""
    prices_json = data.get("prices_json")
    return pd.read_json(prices_json)


def _build_engine(data):
    """Build a configured engine from request data."""
    config_dict = data.get("config", {})
    config = PyPortfolioOptConfig(**config_dict)
    engine = PyPortfolioOptAnalyticsEngine(config)
    prices_df = _parse_prices(data)
    engine.load_data(prices_df)
    return engine


def _safe_float(v):
    """Convert numpy types to native Python for JSON serialization."""
    if isinstance(v, (np.floating, np.integer)):
        return float(v)
    return v


def _safe_dict(d):
    """Recursively convert numpy types in a dict."""
    if isinstance(d, dict):
        return {k: _safe_dict(v) for k, v in d.items()}
    if isinstance(d, (list, tuple)):
        return [_safe_dict(i) for i in d]
    return _safe_float(d)


# ---------------------------------------------------------------------------
# Router
# ---------------------------------------------------------------------------

def main(args):
    """
    Main entry point for worker pool.
    Args: [operation, json_data]
    Returns: JSON string
    """
    try:
        if len(args) < 2:
            return json.dumps({
                "error": "Invalid arguments. Expected: [operation, json_data]"
            })

        operation = args[0]
        request_data = json.loads(args[1])

        dispatch = {
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

        handler = dispatch.get(operation)
        if handler is None:
            return json.dumps({"error": f"Unknown operation: {operation}"})

        return handler(request_data)

    except Exception as e:
        import traceback
        return json.dumps({
            "error": f"PyPortfolioOpt error: {str(e)}",
            "traceback": traceback.format_exc()
        })


# ---------------------------------------------------------------------------
# Core operations
# ---------------------------------------------------------------------------

def optimize_portfolio(data):
    """Optimize portfolio using PyPortfolioOpt"""
    engine = _build_engine(data)
    weights = engine.optimize_portfolio()
    expected_return, volatility, sharpe = engine.portfolio_performance()

    result = {
        "weights": weights,
        "performance_metrics": {
            "expected_annual_return": expected_return,
            "annual_volatility": volatility,
            "sharpe_ratio": sharpe
        }
    }
    return json.dumps(_safe_dict(result))


def generate_efficient_frontier(data):
    """Generate efficient frontier"""
    num_points = data.get("num_points", 20)
    engine = _build_engine(data)
    frontier_data = engine.generate_efficient_frontier(num_points)

    result = {
        "returns": frontier_data["returns"],
        "volatility": frontier_data["volatility"],
        "sharpe_ratios": frontier_data["sharpe_ratios"],
        "num_points": frontier_data["num_points"]
    }
    return json.dumps(_safe_dict(result))


def calculate_discrete_allocation(data):
    """Calculate discrete allocation"""
    weights = data.get("weights", {})
    total_portfolio_value = data.get("total_portfolio_value", 10000.0)

    config = PyPortfolioOptConfig()
    engine = PyPortfolioOptAnalyticsEngine(config)
    prices_df = _parse_prices(data)
    engine.load_data(prices_df)
    engine.weights = weights

    allocation_result = engine.discrete_allocation(total_portfolio_value=total_portfolio_value)

    result = {
        "allocation": allocation_result["allocation"],
        "leftover_cash": allocation_result["leftover_cash"],
        "total_value": allocation_result["total_value"],
        "allocated_value": allocation_result["allocated_value"]
    }
    return json.dumps(_safe_dict(result))


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

    # Convert cumulative return series for charting
    cum_returns = backtest_df["cumulative_return"].tolist()
    drawdowns = backtest_df["drawdown"].tolist()
    dates = [d.isoformat() for d in backtest_df.index]

    result = {
        "dates": dates,
        "cumulative_returns": cum_returns,
        "drawdowns": drawdowns,
        "annual_return": engine.backtest_results["annual_return"],
        "annual_volatility": engine.backtest_results["annual_volatility"],
        "sharpe_ratio": engine.backtest_results["sharpe_ratio"],
        "max_drawdown": engine.backtest_results["max_drawdown"],
        "calmar_ratio": engine.backtest_results["calmar_ratio"]
    }
    return json.dumps(_safe_dict(result))


def calculate_risk_decomposition(data):
    """Calculate risk decomposition using core engine"""
    engine = _build_engine(data)
    weights_input = data.get("weights", None)

    if weights_input:
        engine.weights = weights_input
    else:
        engine.optimize_portfolio()

    decomp = engine.risk_decomposition()

    result = {
        "marginal_contribution": decomp["marginal_contribution"].to_dict(),
        "component_contribution": decomp["component_contribution"].to_dict(),
        "percentage_contribution": decomp["percentage_contribution"].to_dict(),
        "portfolio_volatility": float(decomp["portfolio_volatility"])
    }
    return json.dumps(_safe_dict(result))


def black_litterman_optimization(data):
    """Black-Litterman optimization using core engine"""
    engine = _build_engine(data)

    views = data.get("views", None)
    view_confidences = data.get("view_confidences", None)
    tau = data.get("tau", None)

    market_caps = None
    market_caps_json = data.get("market_caps_json", None)
    if market_caps_json:
        market_caps = pd.read_json(market_caps_json, typ="series")

    weights = engine.black_litterman_optimization(
        market_caps=market_caps,
        views=views,
        view_confidences=view_confidences,
        tau=tau
    )
    expected_return, volatility, sharpe = engine.portfolio_performance()

    result = {
        "weights": dict(weights),
        "performance_metrics": {
            "expected_annual_return": expected_return,
            "annual_volatility": volatility,
            "sharpe_ratio": sharpe
        }
    }
    return json.dumps(_safe_dict(result))


def hrp_optimization(data):
    """Hierarchical Risk Parity optimization using core engine"""
    engine = _build_engine(data)
    weights = engine.hrp_optimization()
    expected_return, volatility, sharpe = engine.portfolio_performance()

    result = {
        "weights": dict(weights),
        "performance_metrics": {
            "expected_annual_return": expected_return,
            "annual_volatility": volatility,
            "sharpe_ratio": sharpe
        }
    }
    return json.dumps(_safe_dict(result))


def generate_report(data):
    """Generate comprehensive portfolio report"""
    engine = _build_engine(data)
    engine.optimize_portfolio()
    report = engine.generate_report()
    return json.dumps(_safe_dict(report), default=str)


def sensitivity_analysis(data):
    """Run sensitivity analysis on a parameter"""
    engine = _build_engine(data)
    parameter = data.get("parameter", "risk_free_rate")
    values = data.get("values", None)

    engine.optimize_portfolio()
    results_df = engine.sensitivity_analysis(parameter=parameter, values=values)

    result = {
        "parameter": parameter,
        "results": results_df.to_dict(orient="records")
    }
    return json.dumps(_safe_dict(result))


# ---------------------------------------------------------------------------
# Additional optimizers (additional_optimizers.py)
# ---------------------------------------------------------------------------

def risk_parity(data):
    """Risk Parity optimization"""
    prices_df = _parse_prices(data)
    risk_measure = data.get("risk_measure", "volatility")
    result = optimize_risk_parity(prices_df, risk_measure=risk_measure)
    return json.dumps(_safe_dict(result))


def equal_weight(data):
    """Equal weighting (1/N) portfolio"""
    prices_df = _parse_prices(data)
    result = optimize_equal_weighting(prices_df)
    return json.dumps(_safe_dict(result))


def inverse_volatility(data):
    """Inverse volatility weighting"""
    prices_df = _parse_prices(data)
    result = optimize_inverse_volatility(prices_df)
    return json.dumps(_safe_dict(result))


def market_neutral(data):
    """Market neutral (long/short) portfolio"""
    prices_df = _parse_prices(data)
    long_exposure = data.get("long_exposure", 1.0)
    short_exposure = data.get("short_exposure", -1.0)
    objective = data.get("objective", "max_sharpe")
    result = optimize_market_neutral(
        prices_df,
        long_exposure=long_exposure,
        short_exposure=short_exposure,
        objective=objective
    )
    return json.dumps(_safe_dict(result))


def min_tracking_error(data):
    """Minimum tracking error relative to benchmark"""
    prices_df = _parse_prices(data)
    benchmark_weights = data.get("benchmark_weights", {})
    target_return = data.get("target_return", None)
    result = optimize_minimum_tracking_error(
        prices_df,
        benchmark_weights=benchmark_weights,
        target_return=target_return
    )
    return json.dumps(_safe_dict(result))


def max_diversification(data):
    """Maximum diversification portfolio"""
    prices_df = _parse_prices(data)
    result = optimize_maximum_diversification(prices_df)
    return json.dumps(_safe_dict(result))


# ---------------------------------------------------------------------------
# Advanced objectives (advanced_objectives.py)
# ---------------------------------------------------------------------------

def custom_constraints(data):
    """Optimize with custom sector constraints"""
    prices_df = _parse_prices(data)
    objective = data.get("objective", "max_sharpe")
    sector_mapper = data.get("sector_mapper", None)
    sector_lower = data.get("sector_lower", None)
    sector_upper = data.get("sector_upper", None)
    weight_bounds_list = data.get("weight_bounds", [0, 1])
    weight_bounds = tuple(weight_bounds_list)

    result = optimize_with_custom_constraints(
        prices=prices_df,
        objective=objective,
        sector_mapper=sector_mapper,
        sector_lower=sector_lower,
        sector_upper=sector_upper,
        weight_bounds=weight_bounds
    )
    return json.dumps(_safe_dict(result))


def views_optimization(data):
    """Optimize using Black-Litterman with investor views (advanced_objectives)"""
    prices_df = _parse_prices(data)
    views = data.get("views", {})
    view_confidences = data.get("view_confidences", None)
    risk_aversion = data.get("risk_aversion", 1.0)

    market_caps = None
    market_caps_json = data.get("market_caps_json", None)
    if market_caps_json:
        market_caps = pd.read_json(market_caps_json, typ="series")

    result = optimize_with_views(
        prices=prices_df,
        views=views,
        view_confidences=view_confidences,
        market_caps=market_caps,
        risk_aversion=risk_aversion
    )
    return json.dumps(_safe_dict(result))


if __name__ == '__main__':
    # For standalone testing
    if len(sys.argv) > 1:
        print(main(sys.argv[1:]))
    else:
        print("Usage: worker_handler.py <operation> <json_data>")
