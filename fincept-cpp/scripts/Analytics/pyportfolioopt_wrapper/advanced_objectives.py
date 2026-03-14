"""
Advanced Objectives and Constraints for PyPortfolioOpt
=======================================================

This module provides advanced optimization objectives and constraints
that are part of PyPortfolioOpt but not included in the core wrapper.

Features:
- Custom objective functions
- Sector constraints
- Tracking error constraints
- Turnover constraints
- L1 regularization
- Transaction cost modeling
"""

import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Callable, Union
from pypfopt import EfficientFrontier, objective_functions
from pypfopt.expected_returns import mean_historical_return
from pypfopt.risk_models import sample_cov


def add_custom_objective(
    ef: EfficientFrontier,
    objective_function: Callable,
    **kwargs
) -> EfficientFrontier:
    """
    Add a custom objective function to the optimization

    Parameters:
    -----------
    ef : EfficientFrontier
        Efficient frontier optimizer instance
    objective_function : Callable
        Custom objective function
    **kwargs : dict
        Additional parameters for the objective function

    Returns:
    --------
    EfficientFrontier with custom objective added
    """
    ef.add_objective(objective_function, **kwargs)
    return ef


def add_sector_constraints(
    ef: EfficientFrontier,
    sector_mapper: Dict[str, str],
    sector_lower: Dict[str, float],
    sector_upper: Dict[str, float]
) -> EfficientFrontier:
    """
    Add sector constraints to limit exposure to specific sectors

    Parameters:
    -----------
    ef : EfficientFrontier
        Efficient frontier optimizer instance
    sector_mapper : Dict[str, str]
        Maps each asset to its sector
    sector_lower : Dict[str, float]
        Minimum weight for each sector
    sector_upper : Dict[str, float]
        Maximum weight for each sector

    Returns:
    --------
    EfficientFrontier with sector constraints added

    Example:
    --------
    sector_mapper = {
        "AAPL": "Technology",
        "MSFT": "Technology",
        "JPM": "Finance",
        "XOM": "Energy"
    }
    sector_lower = {"Technology": 0.1, "Finance": 0.05, "Energy": 0.0}
    sector_upper = {"Technology": 0.4, "Finance": 0.3, "Energy": 0.2}
    """
    ef.add_sector_constraints(sector_mapper, sector_lower, sector_upper)
    return ef


def add_tracking_error_constraint(
    ef: EfficientFrontier,
    benchmark_weights: Union[Dict, pd.Series],
    max_tracking_error: float
) -> EfficientFrontier:
    """
    Add tracking error constraint to stay close to benchmark

    Parameters:
    -----------
    ef : EfficientFrontier
        Efficient frontier optimizer instance
    benchmark_weights : Dict or pd.Series
        Benchmark portfolio weights
    max_tracking_error : float
        Maximum allowed tracking error

    Returns:
    --------
    EfficientFrontier with tracking error constraint
    """
    if isinstance(benchmark_weights, dict):
        benchmark_weights = pd.Series(benchmark_weights)

    # Add tracking error objective
    ef.add_objective(
        objective_functions.ex_ante_tracking_error,
        benchmark_weights=benchmark_weights,
        cov_matrix=ef.cov_matrix
    )

    return ef


def add_turnover_constraint(
    ef: EfficientFrontier,
    current_weights: Union[Dict, pd.Series],
    max_turnover: float
) -> EfficientFrontier:
    """
    Add turnover constraint to limit portfolio changes

    Parameters:
    -----------
    ef : EfficientFrontier
        Efficient frontier optimizer instance
    current_weights : Dict or pd.Series
        Current portfolio weights
    max_turnover : float
        Maximum allowed turnover (0 to 1)

    Returns:
    --------
    EfficientFrontier with turnover constraint

    Example:
    --------
    # Limit turnover to 20%
    add_turnover_constraint(ef, current_weights, max_turnover=0.2)
    """
    if isinstance(current_weights, dict):
        current_weights = pd.Series(current_weights)

    # Convert current_weights to match ef's asset order
    current_weights = current_weights.reindex(ef.tickers, fill_value=0)

    # Add constraint: sum of absolute differences <= max_turnover
    def turnover_constraint(w):
        return np.sum(np.abs(w - current_weights.values)) - max_turnover

    ef.add_constraint(lambda w: turnover_constraint(w) <= 0)

    return ef


def add_l1_regularization(
    ef: EfficientFrontier,
    gamma: float = 1.0
) -> EfficientFrontier:
    """
    Add L1 regularization to encourage sparse portfolios

    Parameters:
    -----------
    ef : EfficientFrontier
        Efficient frontier optimizer instance
    gamma : float
        Regularization parameter (higher = more sparse)

    Returns:
    --------
    EfficientFrontier with L1 regularization
    """
    ef.add_objective(objective_functions.L1_reg, gamma=gamma)
    return ef


def add_transaction_cost(
    ef: EfficientFrontier,
    current_weights: Union[Dict, pd.Series],
    transaction_cost_pct: float = 0.001
) -> EfficientFrontier:
    """
    Add transaction cost model to optimization

    Parameters:
    -----------
    ef : EfficientFrontier
        Efficient frontier optimizer instance
    current_weights : Dict or pd.Series
        Current portfolio weights
    transaction_cost_pct : float
        Transaction cost as percentage (default: 0.1%)

    Returns:
    --------
    EfficientFrontier with transaction costs
    """
    if isinstance(current_weights, dict):
        current_weights = pd.Series(current_weights)

    current_weights = current_weights.reindex(ef.tickers, fill_value=0)

    ef.add_objective(
        objective_functions.transaction_cost,
        w_prev=current_weights.values,
        k=transaction_cost_pct
    )

    return ef


def optimize_with_custom_constraints(
    prices: pd.DataFrame,
    objective: str = "max_sharpe",
    constraints: Optional[List[Callable]] = None,
    sector_mapper: Optional[Dict[str, str]] = None,
    sector_lower: Optional[Dict[str, float]] = None,
    sector_upper: Optional[Dict[str, float]] = None,
    weight_bounds: tuple = (0, 1),
    custom_objectives: Optional[List[tuple]] = None
) -> Dict:
    """
    Optimize portfolio with multiple custom constraints and objectives

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    objective : str
        Primary objective ('max_sharpe', 'min_volatility', etc.)
    constraints : List[Callable], optional
        List of constraint functions
    sector_mapper : Dict[str, str], optional
        Asset to sector mapping
    sector_lower : Dict[str, float], optional
        Minimum sector weights
    sector_upper : Dict[str, float], optional
        Maximum sector weights
    weight_bounds : tuple
        Min and max weight bounds per asset
    custom_objectives : List[tuple], optional
        List of (objective_function, kwargs) tuples

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_with_custom_constraints(
        prices=df,
        objective="max_sharpe",
        constraints=[lambda w: w[0] >= 0.05],  # Min 5% in first asset
        sector_mapper={"AAPL": "Tech", "JPM": "Finance"},
        sector_lower={"Tech": 0.1, "Finance": 0.1},
        sector_upper={"Tech": 0.5, "Finance": 0.4}
    )
    """
    # Calculate expected returns and covariance
    mu = mean_historical_return(prices)
    S = sample_cov(prices)

    # Create efficient frontier
    ef = EfficientFrontier(mu, S, weight_bounds=weight_bounds)

    # Add custom constraints
    if constraints:
        for constraint in constraints:
            ef.add_constraint(constraint)

    # Add sector constraints
    if sector_mapper and sector_lower and sector_upper:
        ef.add_sector_constraints(sector_mapper, sector_lower, sector_upper)

    # Add custom objectives
    if custom_objectives:
        for obj_func, obj_kwargs in custom_objectives:
            ef.add_objective(obj_func, **obj_kwargs)

    # Optimize based on primary objective
    if objective == "max_sharpe":
        weights = ef.max_sharpe()
    elif objective == "min_volatility":
        weights = ef.min_volatility()
    elif objective == "max_quadratic_utility":
        weights = ef.max_quadratic_utility()
    else:
        raise ValueError(f"Unknown objective: {objective}")

    # Get cleaned weights
    cleaned_weights = ef.clean_weights()

    # Calculate performance
    expected_return, volatility, sharpe = ef.portfolio_performance(verbose=False)

    return {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe
        }
    }


def optimize_with_views(
    prices: pd.DataFrame,
    views: Dict[str, float],
    view_confidences: Optional[List[float]] = None,
    market_caps: Optional[pd.Series] = None,
    risk_aversion: float = 1.0
) -> Dict:
    """
    Optimize using Black-Litterman with investor views

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    views : Dict[str, float]
        Dictionary of absolute views {asset: expected_return}
    view_confidences : List[float], optional
        Confidence in each view (0 to 1)
    market_caps : pd.Series, optional
        Market capitalizations for each asset
    risk_aversion : float
        Risk aversion parameter (default: 1.0)

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    views = {
        "AAPL": 0.20,  # Expect 20% return
        "MSFT": 0.15   # Expect 15% return
    }
    result = optimize_with_views(prices, views, view_confidences=[0.8, 0.6])
    """
    from pypfopt import BlackLittermanModel
    from pypfopt.black_litterman import market_implied_prior_returns

    S = sample_cov(prices)

    # Calculate market-implied returns if market caps provided
    if market_caps is not None:
        prior = market_implied_prior_returns(market_caps, risk_aversion, S)
    else:
        prior = mean_historical_return(prices)

    # Create Black-Litterman model
    bl = BlackLittermanModel(S, pi=prior, absolute_views=views)

    # Get posterior estimates
    ret_bl = bl.bl_returns()
    S_bl = bl.bl_cov()

    # Optimize
    ef = EfficientFrontier(ret_bl, S_bl)
    weights = ef.max_sharpe()
    cleaned_weights = ef.clean_weights()

    expected_return, volatility, sharpe = ef.portfolio_performance(verbose=False)

    return {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe
        },
        "bl_returns": ret_bl.to_dict(),
        "prior_returns": prior.to_dict() if isinstance(prior, pd.Series) else prior
    }
