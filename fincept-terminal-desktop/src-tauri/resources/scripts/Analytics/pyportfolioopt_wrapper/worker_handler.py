#!/usr/bin/env python3
"""
PyPortfolioOpt Worker Handler
Provides a main() function interface for worker pool execution

"""

import json
import sys
from pathlib import Path

# Add wrapper directory to path
sys.path.insert(0, str(Path(__file__).parent))

from core import PyPortfolioOptAnalyticsEngine, PyPortfolioOptConfig
import pandas as pd


def main(args):
    """
    Main entry point for worker pool
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

        if operation == "optimize":
            return optimize_portfolio(request_data)
        elif operation == "efficient_frontier":
            return generate_efficient_frontier(request_data)
        elif operation == "discrete_allocation":
            return calculate_discrete_allocation(request_data)
        elif operation == "backtest":
            return run_backtest(request_data)
        elif operation == "risk_decomposition":
            return calculate_risk_decomposition(request_data)
        elif operation == "black_litterman":
            return black_litterman_optimization(request_data)
        elif operation == "hrp":
            return hrp_optimization(request_data)
        else:
            return json.dumps({"error": f"Unknown operation: {operation}"})

    except Exception as e:
        import traceback
        return json.dumps({
            "error": f"PyPortfolioOpt error: {str(e)}",
            "traceback": traceback.format_exc()
        })


def optimize_portfolio(data):
    """Optimize portfolio using PyPortfolioOpt"""
    prices_json = data.get("prices_json")
    config_dict = data.get("config", {})

    # Create config object
    config = PyPortfolioOptConfig(**config_dict)

    # Create engine
    engine = PyPortfolioOptAnalyticsEngine(config)

    # Parse prices
    prices_df = pd.read_json(prices_json)

    # Load data
    engine.load_data(prices_df)

    # Optimize
    weights = engine.optimize_portfolio()

    # Get performance
    expected_return, volatility, sharpe = engine.portfolio_performance()

    result = {
        "weights": weights,
        "performance_metrics": {
            "expected_annual_return": expected_return,
            "annual_volatility": volatility,
            "sharpe_ratio": sharpe
        }
    }

    return json.dumps(result)


def generate_efficient_frontier(data):
    """Generate efficient frontier"""
    prices_json = data.get("prices_json")
    config_dict = data.get("config", {})
    num_points = data.get("num_points", 20)

    config = PyPortfolioOptConfig(**config_dict)
    engine = PyPortfolioOptAnalyticsEngine(config)

    prices_df = pd.read_json(prices_json)
    engine.load_data(prices_df)

    frontier_data = engine.generate_efficient_frontier(num_points)

    result = {
        "returns": frontier_data["returns"],
        "volatility": frontier_data["volatility"],
        "sharpe_ratios": frontier_data["sharpe_ratios"],
        "num_points": num_points
    }

    return json.dumps(result)


def calculate_discrete_allocation(data):
    """Calculate discrete allocation"""
    prices_json = data.get("prices_json")
    weights = data.get("weights", {})
    total_portfolio_value = data.get("total_portfolio_value", 10000.0)

    # Create default config
    config = PyPortfolioOptConfig()
    engine = PyPortfolioOptAnalyticsEngine(config)

    prices_df = pd.read_json(prices_json)
    engine.load_data(prices_df)

    # Set weights manually
    engine.weights = weights

    # Calculate allocation
    allocation_result = engine.discrete_allocation(total_portfolio_value=total_portfolio_value)

    result = {
        "allocation": allocation_result["allocation"],
        "leftover_cash": allocation_result["leftover_cash"],
        "total_value": allocation_result["total_value"],
        "allocated_value": allocation_result["allocated_value"]
    }

    return json.dumps(result)


def run_backtest(data):
    """Run backtest (stub for now)"""
    # Simplified stub - implement full backtest later
    result = {
        "annual_return": 0.12,
        "annual_volatility": 0.15,
        "sharpe_ratio": 0.8,
        "max_drawdown": -0.10,
        "calmar_ratio": 1.2
    }

    return json.dumps(result)


def calculate_risk_decomposition(data):
    """Calculate risk decomposition (stub for now)"""
    result = {
        "marginal_contribution": {},
        "component_contribution": {},
        "percentage_contribution": {},
        "portfolio_volatility": 0.15
    }

    return json.dumps(result)


def black_litterman_optimization(data):
    """Black-Litterman optimization (stub for now)"""
    result = {
        "weights": {},
        "performance_metrics": {
            "expected_annual_return": 0.12,
            "annual_volatility": 0.15,
            "sharpe_ratio": 0.8
        }
    }

    return json.dumps(result)


def hrp_optimization(data):
    """HRP optimization (stub for now)"""
    result = {
        "weights": {},
        "performance_metrics": {
            "expected_annual_return": 0.12,
            "annual_volatility": 0.15,
            "sharpe_ratio": 0.8
        }
    }

    return json.dumps(result)


if __name__ == '__main__':
    # For standalone testing
    if len(sys.argv) > 1:
        print(main(sys.argv[1:]))
    else:
        print("Usage: worker_handler.py <operation> <json_data>")
