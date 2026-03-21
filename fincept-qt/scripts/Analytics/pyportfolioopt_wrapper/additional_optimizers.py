"""
Additional Portfolio Optimizers
================================

This module provides additional optimization strategies not in the core wrapper:
- Minimum Tracking Error
- Risk Parity
- Equal Weighting
- Market Neutral
- Inverse Volatility
"""

import pandas as pd
import numpy as np
from typing import Dict, Optional, Union
from pypfopt import EfficientFrontier, objective_functions, risk_models
from pypfopt.expected_returns import mean_historical_return


def optimize_minimum_tracking_error(
    prices: pd.DataFrame,
    benchmark_weights: Union[Dict, pd.Series],
    target_return: Optional[float] = None
) -> Dict:
    """
    Minimize tracking error relative to a benchmark

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    benchmark_weights : Dict or pd.Series
        Benchmark portfolio weights
    target_return : float, optional
        Target return constraint

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    benchmark = {"AAPL": 0.3, "MSFT": 0.3, "GOOGL": 0.4}
    result = optimize_minimum_tracking_error(prices, benchmark)
    """
    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)

    if isinstance(benchmark_weights, dict):
        benchmark_weights = pd.Series(benchmark_weights)

    # Ensure benchmark weights align with asset order
    benchmark_weights = benchmark_weights.reindex(prices.columns, fill_value=0)

    ef = EfficientFrontier(mu, S)

    # Add tracking error as the objective to minimize
    ef.add_objective(
        objective_functions.ex_ante_tracking_error,
        benchmark_weights=benchmark_weights.values,
        cov_matrix=S
    )

    # Add return constraint if specified
    if target_return is not None:
        ef.add_constraint(lambda w: mu.values @ w >= target_return)

    # Solve
    weights = ef.convex_objective()
    cleaned_weights = ef.clean_weights()

    expected_return, volatility, sharpe = ef.portfolio_performance(verbose=False)

    # Calculate tracking error
    portfolio_weights = pd.Series(cleaned_weights).reindex(benchmark_weights.index, fill_value=0)
    weight_diff = portfolio_weights.values - benchmark_weights.values
    tracking_error = np.sqrt(weight_diff @ S.values @ weight_diff)

    return {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe,
            "tracking_error": tracking_error
        }
    }


def optimize_risk_parity(
    prices: pd.DataFrame,
    risk_measure: str = "volatility"
) -> Dict:
    """
    Risk Parity optimization - equal risk contribution from each asset

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    risk_measure : str
        Risk measure to use ('volatility' or 'cvar')

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_risk_parity(prices, risk_measure="volatility")
    """
    from pypfopt import risk_models

    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)

    # Simple risk parity: weight inversely proportional to volatility
    if risk_measure == "volatility":
        # Calculate individual asset volatilities
        vols = np.sqrt(np.diag(S))

        # Inverse volatility weights
        inv_vols = 1 / vols
        weights_raw = inv_vols / np.sum(inv_vols)

        # Create weights dictionary
        weights = dict(zip(prices.columns, weights_raw))

        # Calculate performance
        portfolio_return = mu.values @ weights_raw
        portfolio_vol = np.sqrt(weights_raw @ S.values @ weights_raw)
        sharpe = (portfolio_return - 0.02) / portfolio_vol  # Assuming 2% risk-free rate

    else:
        raise ValueError(f"Unsupported risk measure: {risk_measure}")

    return {
        "weights": weights,
        "performance": {
            "expected_return": float(portfolio_return),
            "volatility": float(portfolio_vol),
            "sharpe_ratio": float(sharpe)
        },
        "individual_volatilities": dict(zip(prices.columns, vols))
    }


def optimize_equal_weighting(
    prices: pd.DataFrame
) -> Dict:
    """
    Equal weighting (1/N) portfolio

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_equal_weighting(prices)
    """
    n_assets = len(prices.columns)
    equal_weight = 1.0 / n_assets

    weights = {asset: equal_weight for asset in prices.columns}

    # Calculate performance
    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)

    weights_array = np.array([equal_weight] * n_assets)
    portfolio_return = mu.values @ weights_array
    portfolio_vol = np.sqrt(weights_array @ S.values @ weights_array)
    sharpe = (portfolio_return - 0.02) / portfolio_vol

    return {
        "weights": weights,
        "performance": {
            "expected_return": float(portfolio_return),
            "volatility": float(portfolio_vol),
            "sharpe_ratio": float(sharpe)
        }
    }


def optimize_market_neutral(
    prices: pd.DataFrame,
    long_exposure: float = 1.0,
    short_exposure: float = -1.0,
    objective: str = "max_sharpe"
) -> Dict:
    """
    Market neutral portfolio (long/short with net zero exposure)

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    long_exposure : float
        Total long exposure (default: 1.0)
    short_exposure : float
        Total short exposure (default: -1.0)
    objective : str
        Optimization objective ('max_sharpe' or 'min_volatility')

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    # 130/30 portfolio (130% long, 30% short)
    result = optimize_market_neutral(prices, long_exposure=1.3, short_exposure=-0.3)
    """
    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)

    # Allow short positions
    ef = EfficientFrontier(mu, S, weight_bounds=(-1, 1))

    # Add market neutral constraint: sum of weights = 0
    ef.add_constraint(lambda w: np.sum(w) == 0)

    # Add long/short exposure constraints
    ef.add_constraint(lambda w: np.sum(w[w > 0]) <= long_exposure)
    ef.add_constraint(lambda w: np.sum(w[w < 0]) >= short_exposure)

    # Optimize
    if objective == "max_sharpe":
        weights = ef.max_sharpe()
    elif objective == "min_volatility":
        weights = ef.min_volatility()
    else:
        raise ValueError(f"Unknown objective: {objective}")

    cleaned_weights = ef.clean_weights()
    expected_return, volatility, sharpe = ef.portfolio_performance(verbose=False)

    # Calculate exposures
    weights_array = np.array([cleaned_weights[asset] for asset in prices.columns])
    actual_long = np.sum(weights_array[weights_array > 0])
    actual_short = np.sum(weights_array[weights_array < 0])

    return {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe
        },
        "exposures": {
            "long_exposure": float(actual_long),
            "short_exposure": float(actual_short),
            "net_exposure": float(actual_long + actual_short),
            "gross_exposure": float(actual_long - actual_short)
        }
    }


def optimize_inverse_volatility(
    prices: pd.DataFrame
) -> Dict:
    """
    Inverse volatility weighting portfolio

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_inverse_volatility(prices)
    """
    S = risk_models.sample_cov(prices)
    mu = mean_historical_return(prices)

    # Calculate individual volatilities
    vols = np.sqrt(np.diag(S))

    # Inverse volatility weights
    inv_vols = 1 / vols
    weights_raw = inv_vols / np.sum(inv_vols)

    weights = dict(zip(prices.columns, weights_raw))

    # Calculate performance
    portfolio_return = mu.values @ weights_raw
    portfolio_vol = np.sqrt(weights_raw @ S.values @ weights_raw)
    sharpe = (portfolio_return - 0.02) / portfolio_vol

    return {
        "weights": weights,
        "performance": {
            "expected_return": float(portfolio_return),
            "volatility": float(portfolio_vol),
            "sharpe_ratio": float(sharpe)
        },
        "individual_volatilities": dict(zip(prices.columns, vols))
    }


def optimize_maximum_diversification(
    prices: pd.DataFrame
) -> Dict:
    """
    Maximum Diversification Portfolio
    Maximizes the diversification ratio = weighted avg volatility / portfolio volatility

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_maximum_diversification(prices)
    """
    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)

    # Individual volatilities
    vols = np.sqrt(np.diag(S))

    ef = EfficientFrontier(mu, S)

    # Custom objective: maximize diversification ratio
    # diversification_ratio = (w @ vols) / sqrt(w @ S @ w)
    # We minimize the negative of this
    def neg_diversification_ratio(w, vols, S):
        weighted_vol = w @ vols
        portfolio_vol = np.sqrt(w @ S @ w)
        return -weighted_vol / portfolio_vol

    ef.convex_objective(
        lambda w: neg_diversification_ratio(w, vols, S.values)
    )

    cleaned_weights = ef.clean_weights()
    expected_return, volatility, sharpe = ef.portfolio_performance(verbose=False)

    # Calculate diversification ratio
    weights_array = np.array([cleaned_weights[asset] for asset in prices.columns])
    diversification_ratio = (weights_array @ vols) / volatility

    return {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe,
            "diversification_ratio": float(diversification_ratio)
        }
    }
