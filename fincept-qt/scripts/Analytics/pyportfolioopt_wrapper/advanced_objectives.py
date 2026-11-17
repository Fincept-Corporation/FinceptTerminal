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

import cvxpy as cp
import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Callable, Union
from pypfopt import EfficientFrontier, objective_functions
from pypfopt.expected_returns import mean_historical_return
from pypfopt.risk_models import sample_cov

try:  # imported as part of the pyportfolioopt_wrapper package
    from .core import normalise_market_caps, normalise_views, solve_with_fallback
except ImportError:  # imported as a top-level module by pyportfolioopt_service.py
    from core import normalise_market_caps, normalise_views, solve_with_fallback


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
    ef.add_sector_constraints(sector_mapper, sector_lower or {}, sector_upper or {})
    return ef


def add_tracking_error_constraint(
    ef: EfficientFrontier,
    benchmark_weights: Union[Dict, pd.Series],
    max_tracking_error: float
) -> EfficientFrontier:
    """
    Add tracking error constraint to stay close to benchmark

    Constrains the ex-ante tracking error sqrt((w - w_b)' S (w - w_b)) to at most
    `max_tracking_error`. The constraint is not homogeneous, so do not combine it with
    max_sharpe (whose variable change rescales constraints); use min_volatility,
    max_quadratic_utility, efficient_risk or efficient_return.

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

    benchmark = benchmark_weights.reindex(ef.tickers, fill_value=0).astype(float).values
    limit = float(max_tracking_error) ** 2
    cov = ef.cov_matrix

    ef.add_constraint(lambda w: cp.quad_form(w - benchmark, cov, assume_PSD=True) <= limit)

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
    current = current_weights.reindex(ef.tickers, fill_value=0).astype(float).values
    limit = float(max_turnover)

    # Sum of absolute weight changes <= max_turnover (cvxpy atoms: numpy functions do not work on w)
    ef.add_constraint(lambda w: cp.sum(cp.abs(w - current)) <= limit)

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
    # PyPortfolioOpt has no L1_reg objective; this is gamma * ||w||_1 as a cvxpy expression.
    ef.add_objective(lambda w, gamma=1.0: gamma * cp.norm(w, 1), gamma=gamma)
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
    custom_objectives: Optional[List[tuple]] = None,
    risk_free_rate: float = 0.0,
    risk_aversion: float = 1.0
) -> Dict:
    """
    Optimize portfolio with multiple custom constraints and objectives

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    objective : str
        Primary objective ('max_sharpe', 'min_volatility', 'max_quadratic_utility')
    constraints : List[Callable], optional
        List of constraint functions
    sector_mapper : Dict[str, str], optional
        Asset to sector mapping (required with sector_lower / sector_upper)
    sector_lower : Dict[str, float], optional
        Minimum sector weights (either bound may be given alone)
    sector_upper : Dict[str, float], optional
        Maximum sector weights
    weight_bounds : tuple
        Min and max weight bounds per asset
    custom_objectives : List[tuple], optional
        List of (objective_function, kwargs) tuples
    risk_free_rate : float
        Risk-free rate for max_sharpe and the reported Sharpe ratio (default 0)
    risk_aversion : float
        Risk aversion for max_quadratic_utility (default 1)

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
    if objective not in ("max_sharpe", "min_volatility", "max_quadratic_utility"):
        raise ValueError(f"Unknown objective: {objective} (expected max_sharpe, min_volatility or max_quadratic_utility)")

    # Calculate expected returns and covariance
    mu = mean_historical_return(prices)
    S = sample_cov(prices)

    # Sector constraints: a mapper plus at least one of the bound sets (never silently skipped)
    notes = []
    sectors = None
    if sector_lower or sector_upper:
        if not sector_mapper:
            raise ValueError("sector_lower / sector_upper need sector_mapper ({ticker: sector})")
        mapper = {str(k): str(v) for k, v in sector_mapper.items()}
        lower = {str(k): float(v) for k, v in (sector_lower or {}).items()}
        upper = {str(k): float(v) for k, v in (sector_upper or {}).items()}
        known = set(mapper.values())
        unknown = sorted((set(lower) | set(upper)) - known)
        if unknown:
            raise ValueError("sector bounds name sectors with no assets in sector_mapper: " + ", ".join(unknown))
        sectors = (mapper, lower, upper)
    elif sector_mapper:
        notes.append("sector_mapper given without sector_lower / sector_upper: no sector constraint applied")

    def build(solver):
        # Create efficient frontier
        ef = EfficientFrontier(mu, S, weight_bounds=weight_bounds, solver=solver)

        # Add custom constraints
        if constraints:
            for constraint in constraints:
                ef.add_constraint(constraint)

        if sectors is not None:
            ef.add_sector_constraints(*sectors)

        # Add custom objectives
        if custom_objectives:
            for obj_func, obj_kwargs in custom_objectives:
                ef.add_objective(obj_func, **obj_kwargs)
        return ef

    def solve(ef):
        # Optimize based on primary objective
        if objective == "max_sharpe":
            return ef.max_sharpe(risk_free_rate=risk_free_rate)
        if objective == "min_volatility":
            return ef.min_volatility()
        return ef.max_quadratic_utility(risk_aversion=risk_aversion)

    ef, _ = solve_with_fallback(build, solve, notes)

    # Get cleaned weights
    cleaned_weights = ef.clean_weights()

    # Calculate performance
    expected_return, volatility, sharpe = ef.portfolio_performance(risk_free_rate=risk_free_rate)

    result = {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe if volatility > 0 else None
        },
        "risk_free_rate": risk_free_rate,
    }
    if notes:
        result["notes"] = notes
    return result


def optimize_with_views(
    prices: pd.DataFrame,
    views: Dict[str, float],
    view_confidences: Optional[List[float]] = None,
    market_caps: Optional[pd.Series] = None,
    risk_aversion: float = 1.0,
    risk_free_rate: float = 0.0,
    tau: float = 0.05
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
        Confidence in each view (0 to 1), same order as `views` (or {asset: confidence});
        when given, the view uncertainty uses Idzorek's method
    market_caps : pd.Series, optional
        Market capitalizations for each asset (market-implied prior); without them the
        prior is the mean historical return
    risk_aversion : float
        Risk aversion parameter (default: 1.0)
    risk_free_rate : float
        Risk-free rate for the prior, max_sharpe and the Sharpe ratio (default 0)
    tau : float
        Weight-on-views scalar (default 0.05, PyPortfolioOpt's default)

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
    tickers = list(S.columns)

    views_clean, confidences = normalise_views(views, view_confidences, tickers)
    if not views_clean:
        raise ValueError("views_optimization needs at least one view: {ticker: expected annual return}")

    # Calculate market-implied returns if market caps provided
    if market_caps is not None:
        caps = normalise_market_caps(market_caps, tickers)
        prior = market_implied_prior_returns(caps, risk_aversion, S, risk_free_rate=risk_free_rate)
        prior_source = "market_implied"
    else:
        prior = mean_historical_return(prices)
        prior_source = "mean_historical_return"

    # Create Black-Litterman model (views go to the constructor; confidences -> Idzorek omega)
    bl = BlackLittermanModel(
        S,
        pi=prior,
        absolute_views=views_clean,
        omega="idzorek" if confidences is not None else None,
        view_confidences=confidences,
        tau=tau,
        risk_aversion=risk_aversion,
    )

    # Get posterior estimates
    ret_bl = bl.bl_returns()
    S_bl = bl.bl_cov()

    # Optimize
    notes = []
    ef, _ = solve_with_fallback(lambda solver: EfficientFrontier(ret_bl, S_bl, solver=solver),
                                lambda ef: ef.max_sharpe(risk_free_rate=risk_free_rate), notes)
    cleaned_weights = ef.clean_weights()

    expected_return, volatility, sharpe = ef.portfolio_performance(risk_free_rate=risk_free_rate)

    result = {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe if volatility > 0 else None
        },
        "bl_returns": ret_bl.to_dict(),
        "prior_returns": prior.to_dict() if isinstance(prior, pd.Series) else prior,
        "prior_source": prior_source,
        "views_applied": views_clean,
        "omega_method": "idzorek" if confidences is not None else "he_litterman_default",
        "risk_free_rate": risk_free_rate,
    }
    if notes:
        result["notes"] = notes
    return result
