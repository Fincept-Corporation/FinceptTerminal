"""
Additional Portfolio Optimizers
================================

This module provides additional optimization strategies not in the core wrapper:
- Minimum Tracking Error
- Risk Parity (equal risk contribution)
- Equal Weighting
- Market Neutral (dollar-neutral long/short)
- Inverse Volatility
- Maximum Diversification
"""

import cvxpy as cp
import numpy as np
import pandas as pd
from typing import Dict, List, Optional, Union
from pypfopt import EfficientFrontier, objective_functions, risk_models
from pypfopt.base_optimizer import BaseOptimizer, portfolio_performance
from pypfopt.expected_returns import mean_historical_return

try:  # imported as part of the pyportfolioopt_wrapper package
    from .core import solve_with_fallback
except ImportError:  # imported as a top-level module by pyportfolioopt_service.py
    from core import solve_with_fallback

# Used only when the caller gives no target (the target used is reported).
DEFAULT_TARGET_VOLATILITY = 0.15
DEFAULT_TARGET_RETURN = 0.12


def _performance(weights: np.ndarray, mu: pd.Series, S: pd.DataFrame, risk_free_rate: float) -> Dict:
    """Expected return, volatility and Sharpe ratio of `weights` (Sharpe is null for a zero-risk book)."""
    ret, vol, sharpe = portfolio_performance(np.asarray(weights, dtype=float), mu, S,
                                             risk_free_rate=risk_free_rate)
    return {
        "expected_return": float(ret),
        "volatility": float(vol),
        "sharpe_ratio": float(sharpe) if vol > 0 else None,
    }


def _clean(weights: np.ndarray, tickers: List[str]) -> Dict[str, float]:
    """PyPortfolioOpt's clean_weights (|w| < 1e-4 -> 0, rounded to 5 places) for a raw array."""
    holder = BaseOptimizer(len(tickers), tickers)
    holder.weights = np.asarray(weights, dtype=float)
    return holder.clean_weights()


def _weights_series(weights: Union[Dict, pd.Series], tickers: List[str], name: str) -> pd.Series:
    """{ticker: weight} -> float Series over `tickers` (missing tickers 0); unknown tickers rejected."""
    if weights is None or (hasattr(weights, "__len__") and len(weights) == 0):
        raise ValueError(f"{name} is required: an object {{ticker: weight}}")
    series = weights.copy() if isinstance(weights, pd.Series) else pd.Series(dict(weights), dtype=object)
    series.index = series.index.map(str)
    series = pd.to_numeric(series, errors="coerce")
    bad = [t for t, v in series.items() if not np.isfinite(v)]
    if bad:
        raise ValueError(f"{name} must be finite numbers (bad: " + ", ".join(bad) + ")")
    unknown = [t for t in series.index if t not in tickers]
    if unknown:
        raise ValueError(f"{name} reference tickers not in the price data: " + ", ".join(unknown))
    return series.reindex(tickers, fill_value=0.0).astype(float)


def equal_risk_contribution_weights(cov: np.ndarray, max_iter: int = 200) -> np.ndarray:
    """Long-only equal-risk-contribution (risk parity) weights.

    Spinu (2013): y* = argmin_{y > 0} 0.5 y'Sy - (1/n) sum(log y) is strictly convex, and its
    first-order condition y_i (Sy)_i = 1/n says every asset contributes the same variance;
    w = y / sum(y). Solved with damped Newton steps that keep y > 0.
    """
    S = np.asarray(cov, dtype=float)
    n = S.shape[0]
    scale = float(np.mean(np.diag(S)))
    if not np.isfinite(scale) or scale <= 0:
        raise ValueError("Risk parity needs positive asset variances")
    A = S / scale  # rescaling the covariance does not change ERC weights; it helps conditioning
    b = np.full(n, 1.0 / n)

    def f(y):
        return 0.5 * y @ A @ y - b @ np.log(y)

    y = 1.0 / np.sqrt(np.diag(A))
    y /= np.sqrt(y @ A @ y)
    for _ in range(max_iter):
        g = A @ y - b / y
        H = A + np.diag(b / y ** 2)
        step = np.linalg.solve(H, -g)
        decrement = float(-g @ step)  # squared Newton decrement
        if decrement < 1e-24:
            break
        t = 1.0
        shrinking = step < 0
        if shrinking.any():
            t = min(1.0, 0.99 * float(np.min(-y[shrinking] / step[shrinking])))
        fy = f(y)
        while f(y + t * step) > fy - 0.25 * t * decrement and t > 1e-14:
            t *= 0.5
        y = y + t * step
    w = y / y.sum()
    rc = w * (S @ w)
    spread = float((rc.max() - rc.min()) / rc.mean())
    if not np.isfinite(spread) or spread > 1e-6:
        raise ValueError(f"Risk parity did not converge (risk-contribution spread {spread:.2e})")
    return w


def optimize_minimum_tracking_error(
    prices: pd.DataFrame,
    benchmark_weights: Union[Dict, pd.Series],
    target_return: Optional[float] = None,
    risk_free_rate: float = 0.0
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
    risk_free_rate : float
        Risk-free rate for the reported Sharpe ratio (default 0)

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
    tickers = [str(t) for t in prices.columns]

    # Benchmark weights aligned with the asset order (tickers it does not hold get 0)
    benchmark = _weights_series(benchmark_weights, tickers, "benchmark_weights")

    if target_return is not None:
        target_return = float(target_return)

    def build(solver):
        ef = EfficientFrontier(mu, S, solver=solver)
        # Add return constraint if specified
        if target_return is not None:
            ef.add_constraint(lambda w: mu.values @ w >= target_return)
        return ef

    # Solve: the (squared) ex-ante tracking error IS the objective
    notes = []
    ef, _ = solve_with_fallback(build, lambda ef: ef.convex_objective(
        objective_functions.ex_ante_tracking_error,
        cov_matrix=S.values,
        benchmark_weights=benchmark.values,
    ), notes)
    cleaned_weights = ef.clean_weights()

    expected_return, volatility, sharpe = ef.portfolio_performance(risk_free_rate=risk_free_rate)

    # Calculate tracking error
    portfolio_weights = pd.Series(cleaned_weights).reindex(benchmark.index, fill_value=0)
    weight_diff = portfolio_weights.values - benchmark.values
    tracking_error = float(np.sqrt(max(weight_diff @ S.values @ weight_diff, 0.0)))

    result = {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe if volatility > 0 else None,
            "tracking_error": tracking_error
        },
        "benchmark_weights": benchmark.to_dict(),
        "risk_free_rate": risk_free_rate,
    }
    if target_return is not None:
        result["target_return"] = target_return
    if notes:
        result["notes"] = notes
    return result


def optimize_risk_parity(
    prices: pd.DataFrame,
    risk_measure: str = "volatility",
    risk_free_rate: float = 0.02
) -> Dict:
    """
    Risk Parity optimization - equal risk contribution from each asset

    Long-only equal-risk-contribution portfolio on the sample covariance: every asset
    contributes the same share of portfolio variance (not inverse volatility, which only
    equalises contributions when all correlations are equal).

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    risk_measure : str
        Risk measure to use (only 'volatility' is supported)
    risk_free_rate : float
        Risk-free rate for the Sharpe ratio (default 0.02)

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_risk_parity(prices, risk_measure="volatility")
    """
    if risk_measure != "volatility":
        raise ValueError(f"Unsupported risk measure: {risk_measure} (only 'volatility' is supported)")

    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)
    tickers = [str(t) for t in prices.columns]

    # Individual asset volatilities
    vols = np.sqrt(np.diag(S))

    weights_raw = equal_risk_contribution_weights(S.values)

    # Share of portfolio variance contributed by each asset (all equal to 1/n)
    contrib = weights_raw * (S.values @ weights_raw)
    contrib_share = contrib / contrib.sum()

    return {
        "weights": dict(zip(tickers, weights_raw)),
        "performance": _performance(weights_raw, mu, S, risk_free_rate),
        "individual_volatilities": dict(zip(tickers, vols)),
        "risk_contributions": dict(zip(tickers, contrib_share)),
        "method": "equal_risk_contribution",
        "risk_free_rate": risk_free_rate,
    }


def optimize_equal_weighting(
    prices: pd.DataFrame,
    risk_free_rate: float = 0.02
) -> Dict:
    """
    Equal weighting (1/N) portfolio

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    risk_free_rate : float
        Risk-free rate for the Sharpe ratio (default 0.02)

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_equal_weighting(prices)
    """
    n_assets = len(prices.columns)
    equal_weight = 1.0 / n_assets

    weights = {str(asset): equal_weight for asset in prices.columns}

    # Calculate performance
    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)

    weights_array = np.array([equal_weight] * n_assets)

    return {
        "weights": weights,
        "performance": _performance(weights_array, mu, S, risk_free_rate),
        "risk_free_rate": risk_free_rate,
    }


def _dollar_neutral_max_sharpe(mu: np.ndarray, S: np.ndarray, long_cap: float, short_cap: float) -> np.ndarray:
    """Maximum-Sharpe zero-net-investment portfolio, scaled to the largest size the caps allow.

    The Sharpe ratio mu'w / sqrt(w'Sw) of a dollar-neutral book is scale-free, so its maximiser is
    the direction y = argmin y'Sy s.t. mu'y = 1, sum(y) = 0 (closed form below); any positive
    multiple has the same Sharpe. It is scaled down until sum(long) <= long_cap,
    sum(short) <= short_cap and every |w_i| <= 1.
    """
    n = len(mu)
    A = np.vstack([mu, np.ones(n)])           # 2 x n: [expected returns; ones]
    S_inv_At = np.linalg.solve(S, A.T)        # n x 2
    M = A @ S_inv_At                          # 2 x 2
    det = np.linalg.det(M)
    if not np.isfinite(det) or abs(det) <= 1e-12 * abs(M[0, 0] * M[1, 1]):
        raise ValueError("max_sharpe is undefined for a dollar-neutral book when every asset has the same "
                         "expected return (no zero-net position earns anything)")
    lam = np.linalg.solve(M, np.array([1.0, 0.0]))
    y = S_inv_At @ lam                        # mu'y = 1, sum(y) = 0
    longs = float(y[y > 0].sum())
    shorts = float(-y[y < 0].sum())
    scale = min(long_cap / longs, short_cap / shorts, 1.0 / float(np.max(np.abs(y))))
    return y * scale


def optimize_market_neutral(
    prices: pd.DataFrame,
    long_exposure: float = 1.0,
    short_exposure: float = -1.0,
    objective: str = "max_sharpe",
    risk_aversion: float = 1.0,
    target_volatility: Optional[float] = None,
    target_return: Optional[float] = None
) -> Dict:
    """
    Market neutral portfolio (long/short with net zero exposure)

    Weights sum to zero, each between -1 and 1; the long side is capped at `long_exposure`
    and the short side at |short_exposure| (they are equal in a dollar-neutral book, so the
    smaller cap binds).

    Objectives:
    - max_sharpe: the Sharpe-maximising zero-net direction (closed form), scaled to the caps.
      No risk-free rate is subtracted: a zero-net book is self-financing.
    - max_quadratic_utility (risk_aversion), efficient_risk (target_volatility),
      efficient_return (target_return): PyPortfolioOpt's market_neutral=True solvers.
    - min_volatility is undefined (the least-risk dollar-neutral book holds nothing): error.

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    long_exposure : float
        Maximum total long exposure (default: 1.0)
    short_exposure : float
        Maximum total short exposure, negative (default: -1.0)
    objective : str
        'max_sharpe', 'max_quadratic_utility', 'efficient_risk' or 'efficient_return'

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    # 50/50 long/short book
    result = optimize_market_neutral(prices, long_exposure=0.5, short_exposure=-0.5)
    """
    long_cap = float(long_exposure)
    short_cap = abs(float(short_exposure))
    if not (np.isfinite(long_cap) and long_cap > 0 and np.isfinite(short_cap) and short_cap > 0):
        raise ValueError("long_exposure must be positive and short_exposure negative (both non-zero)")
    if objective == "min_volatility":
        raise ValueError("min_volatility is undefined for a dollar-neutral book: the zero-net portfolio "
                         "with the least risk holds nothing. Use max_sharpe, max_quadratic_utility, "
                         "efficient_risk or efficient_return.")
    if objective not in ("max_sharpe", "max_quadratic_utility", "efficient_risk", "efficient_return"):
        raise ValueError(f"Unknown objective: {objective} (expected max_sharpe, max_quadratic_utility, "
                         "efficient_risk or efficient_return)")

    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)
    tickers = [str(t) for t in prices.columns]
    notes = []
    targets = {}

    if objective == "max_sharpe":
        # PyPortfolioOpt's max_sharpe cannot do this: its variable change adds sum(w) == k with
        # k > 0 and rescales sum(w) == 0 to 0 == 0 * k, forcing k = 0 (w / k undefined).
        raw = _dollar_neutral_max_sharpe(mu.values, S.values, long_cap, short_cap)
        ef = EfficientFrontier(mu, S, weight_bounds=(-1, 1))
        ef.set_weights(dict(zip(tickers, raw)))
        notes.append("max_sharpe: Sharpe-maximising zero-net direction scaled to the exposure caps; "
                     "no risk-free rate is subtracted because a zero-net book is self-financing")
    else:
        if objective == "max_quadratic_utility":
            targets["risk_aversion"] = float(risk_aversion)
        elif objective == "efficient_risk":
            if target_volatility is None:
                target_volatility = DEFAULT_TARGET_VOLATILITY
                notes.append(f"target_volatility not given; used the default {DEFAULT_TARGET_VOLATILITY:g}")
            targets["target_volatility"] = float(target_volatility)
        else:
            if target_return is None:
                target_return = DEFAULT_TARGET_RETURN
                notes.append(f"target_return not given; used the default {DEFAULT_TARGET_RETURN:g}")
            targets["target_return"] = float(target_return)

        def build(solver):
            # Allow short positions
            ef = EfficientFrontier(mu, S, weight_bounds=(-1, 1), solver=solver)
            # Long/short exposure caps (convex: cp.pos / cp.neg are the positive / negative parts)
            ef.add_constraint(lambda w: cp.sum(cp.pos(w)) <= long_cap)
            ef.add_constraint(lambda w: cp.sum(cp.neg(w)) <= short_cap)
            return ef

        def solve(ef):
            if objective == "max_quadratic_utility":
                return ef.max_quadratic_utility(risk_aversion=float(risk_aversion), market_neutral=True)
            if objective == "efficient_risk":
                return ef.efficient_risk(float(target_volatility), market_neutral=True)
            return ef.efficient_return(float(target_return), market_neutral=True)

        ef, _ = solve_with_fallback(build, solve, notes)

    cleaned_weights = ef.clean_weights()
    expected_return, volatility, sharpe = ef.portfolio_performance()

    # Calculate exposures
    weights_array = np.array([cleaned_weights[asset] for asset in tickers])
    actual_long = np.sum(weights_array[weights_array > 0])
    actual_short = np.sum(weights_array[weights_array < 0])

    result = {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe if volatility > 0 else None
        },
        "exposures": {
            "long_exposure": float(actual_long),
            "short_exposure": float(actual_short),
            "net_exposure": float(actual_long + actual_short),
            "gross_exposure": float(actual_long - actual_short)
        },
        "objective": objective,
    }
    if targets:
        result["targets_used"] = targets
    if notes:
        result["notes"] = notes
    return result


def optimize_inverse_volatility(
    prices: pd.DataFrame,
    risk_free_rate: float = 0.02
) -> Dict:
    """
    Inverse volatility weighting portfolio

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    risk_free_rate : float
        Risk-free rate for the Sharpe ratio (default 0.02)

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_inverse_volatility(prices)
    """
    S = risk_models.sample_cov(prices)
    mu = mean_historical_return(prices)
    tickers = [str(t) for t in prices.columns]

    # Calculate individual volatilities
    vols = np.sqrt(np.diag(S))

    # Inverse volatility weights
    inv_vols = 1 / vols
    weights_raw = inv_vols / np.sum(inv_vols)

    weights = dict(zip(tickers, weights_raw))

    return {
        "weights": weights,
        "performance": _performance(weights_raw, mu, S, risk_free_rate),
        "individual_volatilities": dict(zip(tickers, vols)),
        "risk_free_rate": risk_free_rate,
    }


def optimize_maximum_diversification(
    prices: pd.DataFrame,
    risk_free_rate: float = 0.0
) -> Dict:
    """
    Maximum Diversification Portfolio
    Maximizes the diversification ratio = weighted avg volatility / portfolio volatility

    The ratio (w'sigma) / sqrt(w'Sw) is scale-free, so (Choueifaty & Coignard) it is maximised
    over long-only w by solving the convex problem  min y'Sy  s.t.  sigma'y = 1, y >= 0  and
    normalising w = y / sum(y).

    Parameters:
    -----------
    prices : pd.DataFrame
        Historical price data
    risk_free_rate : float
        Risk-free rate for the Sharpe ratio (default 0)

    Returns:
    --------
    Dict with weights and performance metrics

    Example:
    --------
    result = optimize_maximum_diversification(prices)
    """
    mu = mean_historical_return(prices)
    S = risk_models.sample_cov(prices)
    tickers = [str(t) for t in prices.columns]

    # Individual volatilities
    vols = np.sqrt(np.diag(S))

    y = cp.Variable(len(tickers), nonneg=True)
    problem = cp.Problem(cp.Minimize(cp.quad_form(y, S.values, assume_PSD=True)), [vols @ y == 1])
    problem.solve()
    if problem.status not in ("optimal", "optimal_inaccurate") or y.value is None:
        raise ValueError(f"Maximum-diversification solve failed (solver status: {problem.status})")
    raw = np.clip(np.asarray(y.value, dtype=float), 0.0, None)
    raw = raw / raw.sum()

    ef = EfficientFrontier(mu, S)
    ef.set_weights(dict(zip(tickers, raw)))
    cleaned_weights = ef.clean_weights()
    expected_return, volatility, sharpe = ef.portfolio_performance(risk_free_rate=risk_free_rate)

    # Calculate diversification ratio
    diversification_ratio = (raw @ vols) / np.sqrt(raw @ S.values @ raw)

    return {
        "weights": cleaned_weights,
        "performance": {
            "expected_return": expected_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe if volatility > 0 else None,
            "diversification_ratio": float(diversification_ratio)
        },
        "risk_free_rate": risk_free_rate,
    }
