"""
Fortitudo.tech Portfolio Functions Wrapper
===========================================
Portfolio risk metrics and matrix calculations

Usage:
    python functions.py
"""

import pandas as pd
import numpy as np
import json
from typing import Dict, Any, Optional, Union, Tuple
import warnings
import fortitudo.tech as ft

warnings.filterwarnings('ignore')


def calculate_portfolio_volatility(
    weights: np.ndarray,
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None
) -> float:
    """Calculate portfolio volatility"""
    # Ensure weights are 2D
    if weights.ndim == 1:
        weights = weights.reshape(-1, 1)

    vol = ft.portfolio_vol(e=weights, R=returns, p=probabilities)
    return float(vol)


def calculate_portfolio_var(
    weights: np.ndarray,
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None,
    alpha: float = 0.05,
    demean: bool = False
) -> float:
    """Calculate portfolio Value-at-Risk"""
    if weights.ndim == 1:
        weights = weights.reshape(-1, 1)

    var = ft.portfolio_var(e=weights, R=returns, p=probabilities, alpha=alpha, demean=demean)
    return float(var)


def calculate_portfolio_cvar(
    weights: np.ndarray,
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None,
    alpha: float = 0.05,
    demean: bool = False
) -> float:
    """Calculate portfolio Conditional Value-at-Risk"""
    if weights.ndim == 1:
        weights = weights.reshape(-1, 1)

    cvar = ft.portfolio_cvar(e=weights, R=returns, p=probabilities, alpha=alpha, demean=demean)
    return float(cvar)


def calculate_covariance_matrix(
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None
) -> pd.DataFrame:
    """Calculate covariance matrix"""
    cov = ft.covariance_matrix(R=returns, p=probabilities)
    return cov


def calculate_correlation_matrix(
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None
) -> pd.DataFrame:
    """Calculate correlation matrix"""
    corr = ft.correlation_matrix(R=returns, p=probabilities)
    return corr


def calculate_simulation_moments(
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None
) -> pd.DataFrame:
    """Calculate statistical moments (mean, volatility, skewness, kurtosis)"""
    moments = ft.simulation_moments(R=returns, p=probabilities)
    return moments


def calculate_exp_decay_probabilities(
    returns: Union[pd.DataFrame, np.ndarray],
    half_life: int = 252
) -> np.ndarray:
    """Calculate exponentially decaying probabilities"""
    probs = ft.exp_decay_probs(R=returns, half_life=half_life)
    return probs


def calculate_normal_calibration(
    returns: Union[pd.DataFrame, np.ndarray],
    half_life: int = 252
) -> Tuple:
    """Calibrate normal distribution with exponential decay"""
    mean, cov = ft.normal_exp_decay_calib(R=returns, half_life=half_life)
    return mean, cov


def calculate_all_metrics(
    weights: np.ndarray,
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Calculate all portfolio metrics"""
    if weights.ndim == 1:
        weights = weights.reshape(-1, 1)

    # Calculate metrics
    vol = calculate_portfolio_volatility(weights, returns, probabilities)
    var = calculate_portfolio_var(weights, returns, probabilities, alpha)
    cvar = calculate_portfolio_cvar(weights, returns, probabilities, alpha)

    # Calculate expected return
    if isinstance(returns, pd.DataFrame):
        returns_array = returns.values
    else:
        returns_array = returns

    if probabilities is not None:
        if probabilities.ndim == 2:
            probs = probabilities.flatten()
        else:
            probs = probabilities
        mean_returns = (returns_array.T @ probs)
    else:
        mean_returns = returns_array.mean(axis=0)

    expected_return = float((weights.flatten() @ mean_returns))
    sharpe = expected_return / vol if vol > 0 else 0.0

    return {
        'expected_return': expected_return,
        'volatility': vol,
        'var': var,
        'cvar': cvar,
        'sharpe_ratio': sharpe,
        'alpha': alpha
    }


def main():
    """Test all functions"""
    print("=== Fortitudo.tech Functions Test ===\n")

    # Generate test data
    np.random.seed(42)
    n_scenarios = 200
    n_assets = 4
    returns = np.random.randn(n_scenarios, n_assets) * 0.01 + np.array([0.08, 0.10, 0.12, 0.06]) / 252
    returns_df = pd.DataFrame(returns, columns=['Stocks', 'Bonds', 'Commodities', 'Cash'])
    weights = np.array([0.4, 0.3, 0.2, 0.1])

    print("1. Portfolio Metrics")
    print("-" * 50)
    metrics = calculate_all_metrics(weights, returns_df, alpha=0.05)
    for key, value in metrics.items():
        print(f"  {key}: {value:.6f}")

    print("\n2. Matrix Calculations")
    print("-" * 50)
    cov = calculate_covariance_matrix(returns_df)
    print(f"  Covariance matrix shape: {cov.shape}")
    print(cov)

    corr = calculate_correlation_matrix(returns_df)
    print(f"\n  Correlation matrix shape: {corr.shape}")
    print(corr)

    print("\n3. Simulation Moments")
    print("-" * 50)
    moments = calculate_simulation_moments(returns_df)
    print(moments)

    print("\n4. Exponential Decay Probabilities")
    print("-" * 50)
    probs = calculate_exp_decay_probabilities(returns_df, half_life=100)
    print(f"  Shape: {probs.shape}")
    print(f"  Sum: {probs.sum():.6f}")
    print(f"  First 3: {probs[:3].flatten()}")
    print(f"  Last 3: {probs[-3:].flatten()}")

    # Recalculate with weighted probabilities
    metrics_weighted = calculate_all_metrics(weights, returns_df, probabilities=probs, alpha=0.05)
    print("\n  Metrics with exp decay weighting:")
    for key, value in metrics_weighted.items():
        print(f"    {key}: {value:.6f}")

    print("\n5. JSON Export")
    print("-" * 50)
    json_output = json.dumps(metrics, indent=2)
    print(json_output)

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
