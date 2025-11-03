#!/usr/bin/env python
"""
Portfolio Analytics Service
============================
Simplified wrapper for portfolio analytics accessible from Rust/Tauri
"""

import sys
import json
import numpy as np
import pandas as pd
from typing import Dict, List, Optional

# Add Analytics path
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'Analytics'))


def calculate_portfolio_metrics(returns_data: Dict[str, List[float]],
                                weights: Optional[List[float]] = None,
                                risk_free_rate: float = 0.03) -> Dict:
    """Calculate basic portfolio metrics"""

    # Convert to DataFrame
    df = pd.DataFrame(returns_data)

    # Default equal weights
    if weights is None:
        weights = np.ones(len(df.columns)) / len(df.columns)
    else:
        weights = np.array(weights)

    # Calculate statistics
    returns = df.values
    expected_returns = np.mean(returns, axis=0) * 252  # Annualized
    cov_matrix = np.cov(returns.T) * 252
    individual_stds = np.sqrt(np.diag(cov_matrix))

    # Portfolio metrics
    portfolio_return = np.dot(weights, expected_returns)
    portfolio_variance = np.dot(weights, np.dot(cov_matrix, weights))
    portfolio_std = np.sqrt(portfolio_variance)

    # Sharpe ratio
    portfolio_returns_series = np.dot(returns, weights)
    excess_returns = portfolio_returns_series - (risk_free_rate / 252)
    sharpe_ratio = np.mean(excess_returns) / np.std(excess_returns) * np.sqrt(252)

    # Risk metrics
    var_95 = np.percentile(portfolio_returns_series, 5)
    var_99 = np.percentile(portfolio_returns_series, 1)

    # Max drawdown
    cumulative = np.cumprod(1 + portfolio_returns_series)
    running_max = np.maximum.accumulate(cumulative)
    drawdown = (cumulative - running_max) / running_max
    max_drawdown = np.min(drawdown)

    return {
        "portfolio_return": float(portfolio_return),
        "portfolio_volatility": float(portfolio_std),
        "sharpe_ratio": float(sharpe_ratio),
        "value_at_risk_95": float(var_95),
        "value_at_risk_99": float(var_99),
        "max_drawdown": float(max_drawdown),
        "weights": weights.tolist(),
        "asset_returns": expected_returns.tolist(),
        "asset_volatilities": individual_stds.tolist()
    }


def optimize_portfolio(returns_data: Dict[str, List[float]],
                      method: str = "max_sharpe",
                      risk_free_rate: float = 0.03) -> Dict:
    """Optimize portfolio weights"""
    from scipy import optimize

    # Convert to numpy
    df = pd.DataFrame(returns_data)
    returns = df.values

    expected_returns = np.mean(returns, axis=0) * 252
    cov_matrix = np.cov(returns.T) * 252
    n_assets = len(expected_returns)

    def portfolio_stats(weights):
        portfolio_return = np.dot(weights, expected_returns)
        portfolio_std = np.sqrt(np.dot(weights, np.dot(cov_matrix, weights)))
        sharpe = (portfolio_return - risk_free_rate) / portfolio_std
        return portfolio_return, portfolio_std, sharpe

    # Constraints and bounds
    constraints = ({'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0})
    bounds = tuple((0, 1) for _ in range(n_assets))

    if method == "max_sharpe":
        # Maximize Sharpe ratio
        def neg_sharpe(weights):
            return -portfolio_stats(weights)[2]

        result = optimize.minimize(neg_sharpe, np.ones(n_assets) / n_assets,
                                  method='SLSQP', bounds=bounds, constraints=constraints)

    elif method == "min_volatility":
        # Minimize volatility
        def portfolio_volatility(weights):
            return np.sqrt(np.dot(weights, np.dot(cov_matrix, weights)))

        result = optimize.minimize(portfolio_volatility, np.ones(n_assets) / n_assets,
                                  method='SLSQP', bounds=bounds, constraints=constraints)

    else:
        return {"error": f"Unknown optimization method: {method}"}

    if result.success:
        optimal_weights = result.x
        ret, vol, sharpe = portfolio_stats(optimal_weights)

        return {
            "optimal_weights": optimal_weights.tolist(),
            "expected_return": float(ret),
            "volatility": float(vol),
            "sharpe_ratio": float(sharpe),
            "method": method
        }
    else:
        return {"error": "Optimization failed to converge"}


def generate_efficient_frontier(returns_data: Dict[str, List[float]],
                                num_points: int = 50,
                                risk_free_rate: float = 0.03) -> Dict:
    """Generate efficient frontier"""
    from scipy import optimize

    df = pd.DataFrame(returns_data)
    returns = df.values

    expected_returns = np.mean(returns, axis=0) * 252
    cov_matrix = np.cov(returns.T) * 252
    n_assets = len(expected_returns)

    def portfolio_stats(weights):
        portfolio_return = np.dot(weights, expected_returns)
        portfolio_std = np.sqrt(np.dot(weights, np.dot(cov_matrix, weights)))
        return portfolio_return, portfolio_std

    # Find min and max return portfolios
    def portfolio_return(weights):
        return np.dot(weights, expected_returns)

    constraints = ({'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0})
    bounds = tuple((0, 1) for _ in range(n_assets))

    # Min variance portfolio
    def portfolio_volatility(weights):
        return np.sqrt(np.dot(weights, np.dot(cov_matrix, weights)))

    min_var_result = optimize.minimize(portfolio_volatility, np.ones(n_assets) / n_assets,
                                      method='SLSQP', bounds=bounds, constraints=constraints)

    min_ret = portfolio_return(min_var_result.x)
    max_ret = np.max(expected_returns)

    # Generate frontier
    target_returns = np.linspace(min_ret, max_ret, num_points)
    frontier_volatilities = []
    frontier_weights = []

    for target_ret in target_returns:
        constraints_with_return = (
            {'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0},
            {'type': 'eq', 'fun': lambda w: portfolio_return(w) - target_ret}
        )

        result = optimize.minimize(portfolio_volatility, np.ones(n_assets) / n_assets,
                                  method='SLSQP', bounds=bounds, constraints=constraints_with_return)

        if result.success:
            frontier_volatilities.append(float(np.sqrt(np.dot(result.x, np.dot(cov_matrix, result.x)))))
            frontier_weights.append(result.x.tolist())
        else:
            frontier_volatilities.append(None)
            frontier_weights.append(None)

    # Calculate Sharpe ratios
    sharpe_ratios = [(r - risk_free_rate) / v if v and v > 0 else 0
                     for r, v in zip(target_returns, frontier_volatilities)]

    return {
        "returns": target_returns.tolist(),
        "volatilities": frontier_volatilities,
        "sharpe_ratios": sharpe_ratios,
        "weights": frontier_weights
    }


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "calculate_metrics":
            # Example: python portfolio_analytics_service.py calculate_metrics '{"AAPL": [0.01, -0.02, ...], "GOOGL": [...]}'
            data = json.loads(sys.argv[2])
            weights = json.loads(sys.argv[3]) if len(sys.argv) > 3 else None
            risk_free_rate = float(sys.argv[4]) if len(sys.argv) > 4 else 0.03

            result = calculate_portfolio_metrics(data, weights, risk_free_rate)
            print(json.dumps(result))

        elif command == "optimize":
            data = json.loads(sys.argv[2])
            method = sys.argv[3] if len(sys.argv) > 3 else "max_sharpe"
            risk_free_rate = float(sys.argv[4]) if len(sys.argv) > 4 else 0.03

            result = optimize_portfolio(data, method, risk_free_rate)
            print(json.dumps(result))

        elif command == "efficient_frontier":
            data = json.loads(sys.argv[2])
            num_points = int(sys.argv[3]) if len(sys.argv) > 3 else 50
            risk_free_rate = float(sys.argv[4]) if len(sys.argv) > 4 else 0.03

            result = generate_efficient_frontier(data, num_points, risk_free_rate)
            print(json.dumps(result))

        else:
            print(json.dumps({"error": f"Unknown command: {command}"}))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
