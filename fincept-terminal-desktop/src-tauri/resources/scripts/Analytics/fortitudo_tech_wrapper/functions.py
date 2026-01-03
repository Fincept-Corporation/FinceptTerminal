"""
Fortitudo.tech Portfolio Functions Wrapper
===========================================
Portfolio risk metrics and matrix calculations

Includes fallback implementations using NumPy/SciPy for Python 3.14+ compatibility.

Usage:
    python functions.py
"""

import pandas as pd
import numpy as np
import json
from typing import Dict, Any, Optional, Union, Tuple
import warnings

warnings.filterwarnings('ignore')

# Try to import fortitudo.tech, fallback to pure NumPy implementations
try:
    import fortitudo.tech as ft
    FORTITUDO_AVAILABLE = True
except ImportError:
    FORTITUDO_AVAILABLE = False
    ft = None


# ============================================================================
# FALLBACK IMPLEMENTATIONS (Pure NumPy/SciPy)
# ============================================================================

def _fallback_portfolio_vol(weights: np.ndarray, returns: np.ndarray,
                            probabilities: Optional[np.ndarray] = None) -> float:
    """Fallback portfolio volatility calculation using NumPy."""
    # Flatten weights for calculation
    w = weights.flatten()

    if isinstance(returns, pd.DataFrame):
        returns = returns.values

    # Calculate weighted covariance matrix
    if probabilities is not None:
        if probabilities.ndim == 2:
            p = probabilities.flatten()
        else:
            p = probabilities
        # Weighted mean
        mean_returns = returns.T @ p
        # Weighted covariance
        centered = returns - mean_returns
        cov = (centered.T * p) @ centered
    else:
        cov = np.cov(returns, rowvar=False)

    # Portfolio variance: w' * cov * w
    port_var = w @ cov @ w
    return float(np.sqrt(port_var))


def _fallback_portfolio_var(weights: np.ndarray, returns: np.ndarray,
                            probabilities: Optional[np.ndarray] = None,
                            alpha: float = 0.05, demean: bool = False) -> float:
    """Fallback VaR calculation using NumPy."""
    # Always flatten weights to ensure 1D array for matrix multiplication
    w = weights.flatten()

    if isinstance(returns, pd.DataFrame):
        returns = returns.values

    # Calculate portfolio returns (result should be 1D)
    port_returns = returns @ w

    if demean:
        if probabilities is not None:
            p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
            port_returns = port_returns - np.sum(port_returns * p)
        else:
            port_returns = port_returns - np.mean(port_returns)

    if probabilities is not None:
        # Weighted quantile
        p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
        sorted_idx = np.argsort(port_returns)
        sorted_returns = port_returns[sorted_idx]
        sorted_probs = p[sorted_idx]
        cumsum = np.cumsum(sorted_probs)
        var_idx = np.searchsorted(cumsum, alpha)
        var = -sorted_returns[min(var_idx, len(sorted_returns) - 1)]
    else:
        var = -np.percentile(port_returns, alpha * 100)

    return float(var)


def _fallback_portfolio_cvar(weights: np.ndarray, returns: np.ndarray,
                             probabilities: Optional[np.ndarray] = None,
                             alpha: float = 0.05, demean: bool = False) -> float:
    """Fallback CVaR (Expected Shortfall) calculation using NumPy."""
    # Always flatten weights to ensure 1D array for matrix multiplication
    w = weights.flatten()

    if isinstance(returns, pd.DataFrame):
        returns = returns.values

    # Calculate portfolio returns (result should be 1D)
    port_returns = returns @ w

    if demean:
        if probabilities is not None:
            p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
            port_returns = port_returns - np.sum(port_returns * p)
        else:
            port_returns = port_returns - np.mean(port_returns)

    if probabilities is not None:
        p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
        sorted_idx = np.argsort(port_returns)
        sorted_returns = port_returns[sorted_idx]
        sorted_probs = p[sorted_idx]
        cumsum = np.cumsum(sorted_probs)
        tail_mask = cumsum <= alpha
        if np.any(tail_mask):
            tail_probs = sorted_probs[tail_mask]
            tail_returns = sorted_returns[tail_mask]
            cvar = -np.sum(tail_returns * tail_probs) / np.sum(tail_probs)
        else:
            cvar = -sorted_returns[0]
    else:
        threshold = np.percentile(port_returns, alpha * 100)
        tail_returns = port_returns[port_returns <= threshold]
        cvar = -np.mean(tail_returns) if len(tail_returns) > 0 else -threshold

    return float(cvar)


def _fallback_covariance_matrix(returns: np.ndarray,
                                probabilities: Optional[np.ndarray] = None) -> pd.DataFrame:
    """Fallback covariance matrix calculation."""
    if isinstance(returns, pd.DataFrame):
        columns = returns.columns
        returns_arr = returns.values
    else:
        columns = [f'Asset_{i}' for i in range(returns.shape[1])]
        returns_arr = returns

    if probabilities is not None:
        p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
        mean_returns = returns_arr.T @ p
        centered = returns_arr - mean_returns
        cov = (centered.T * p) @ centered
    else:
        cov = np.cov(returns_arr, rowvar=False)

    return pd.DataFrame(cov, index=columns, columns=columns)


def _fallback_correlation_matrix(returns: np.ndarray,
                                 probabilities: Optional[np.ndarray] = None) -> pd.DataFrame:
    """Fallback correlation matrix calculation."""
    cov = _fallback_covariance_matrix(returns, probabilities)
    std = np.sqrt(np.diag(cov.values))
    corr = cov.values / np.outer(std, std)
    np.fill_diagonal(corr, 1.0)
    return pd.DataFrame(corr, index=cov.index, columns=cov.columns)


def _fallback_simulation_moments(returns: np.ndarray,
                                 probabilities: Optional[np.ndarray] = None) -> pd.DataFrame:
    """Fallback statistical moments calculation."""
    if isinstance(returns, pd.DataFrame):
        columns = returns.columns
        returns_arr = returns.values
    else:
        columns = [f'Asset_{i}' for i in range(returns.shape[1])]
        returns_arr = returns

    if probabilities is not None:
        p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
        mean = returns_arr.T @ p
        centered = returns_arr - mean
        var = (centered ** 2).T @ p
        std = np.sqrt(var)
        skew = ((centered ** 3).T @ p) / (std ** 3)
        kurt = ((centered ** 4).T @ p) / (std ** 4) - 3
    else:
        mean = np.mean(returns_arr, axis=0)
        std = np.std(returns_arr, axis=0, ddof=1)
        from scipy import stats
        skew = stats.skew(returns_arr, axis=0)
        kurt = stats.kurtosis(returns_arr, axis=0)

    moments = pd.DataFrame({
        'Mean': mean,
        'Volatility': std,
        'Skewness': skew,
        'Kurtosis': kurt
    }, index=columns)

    return moments


def _fallback_exp_decay_probs(returns: np.ndarray, half_life: int = 252) -> np.ndarray:
    """Fallback exponential decay probabilities."""
    if isinstance(returns, pd.DataFrame):
        n = len(returns)
    else:
        n = returns.shape[0]

    # Calculate decay factor
    decay = np.log(2) / half_life

    # Generate weights (most recent = highest weight)
    weights = np.exp(-decay * np.arange(n)[::-1])

    # Normalize to probabilities
    probs = weights / np.sum(weights)

    return probs.reshape(-1, 1)


def _fallback_normal_exp_decay_calib(returns: np.ndarray,
                                     half_life: int = 252) -> Tuple[np.ndarray, np.ndarray]:
    """Fallback normal distribution calibration with exponential decay."""
    probs = _fallback_exp_decay_probs(returns, half_life)

    if isinstance(returns, pd.DataFrame):
        returns_arr = returns.values
    else:
        returns_arr = returns

    p = probs.flatten()
    mean = returns_arr.T @ p
    centered = returns_arr - mean
    cov = (centered.T * p) @ centered

    return mean, cov


def calculate_portfolio_volatility(
    weights: np.ndarray,
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None
) -> float:
    """Calculate portfolio volatility"""
    if FORTITUDO_AVAILABLE:
        # Ensure weights are 2D
        if weights.ndim == 1:
            weights = weights.reshape(-1, 1)
        vol = ft.portfolio_vol(e=weights, R=returns, p=probabilities)
        return float(vol)
    else:
        return _fallback_portfolio_vol(weights, returns, probabilities)


def calculate_portfolio_var(
    weights: np.ndarray,
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None,
    alpha: float = 0.05,
    demean: bool = False
) -> float:
    """Calculate portfolio Value-at-Risk"""
    if FORTITUDO_AVAILABLE:
        if weights.ndim == 1:
            weights = weights.reshape(-1, 1)
        var = ft.portfolio_var(e=weights, R=returns, p=probabilities, alpha=alpha, demean=demean)
        return float(var)
    else:
        return _fallback_portfolio_var(weights, returns, probabilities, alpha, demean)


def calculate_portfolio_cvar(
    weights: np.ndarray,
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None,
    alpha: float = 0.05,
    demean: bool = False
) -> float:
    """Calculate portfolio Conditional Value-at-Risk"""
    if FORTITUDO_AVAILABLE:
        if weights.ndim == 1:
            weights = weights.reshape(-1, 1)
        cvar = ft.portfolio_cvar(e=weights, R=returns, p=probabilities, alpha=alpha, demean=demean)
        return float(cvar)
    else:
        return _fallback_portfolio_cvar(weights, returns, probabilities, alpha, demean)


def calculate_covariance_matrix(
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None
) -> pd.DataFrame:
    """Calculate covariance matrix"""
    if FORTITUDO_AVAILABLE:
        cov = ft.covariance_matrix(R=returns, p=probabilities)
        return cov
    else:
        return _fallback_covariance_matrix(returns, probabilities)


def calculate_correlation_matrix(
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None
) -> pd.DataFrame:
    """Calculate correlation matrix"""
    if FORTITUDO_AVAILABLE:
        corr = ft.correlation_matrix(R=returns, p=probabilities)
        return corr
    else:
        return _fallback_correlation_matrix(returns, probabilities)


def calculate_simulation_moments(
    returns: Union[pd.DataFrame, np.ndarray],
    probabilities: Optional[np.ndarray] = None
) -> pd.DataFrame:
    """Calculate statistical moments (mean, volatility, skewness, kurtosis)"""
    if FORTITUDO_AVAILABLE:
        moments = ft.simulation_moments(R=returns, p=probabilities)
        return moments
    else:
        return _fallback_simulation_moments(returns, probabilities)


def calculate_exp_decay_probabilities(
    returns: Union[pd.DataFrame, np.ndarray],
    half_life: int = 252
) -> np.ndarray:
    """Calculate exponentially decaying probabilities"""
    if FORTITUDO_AVAILABLE:
        probs = ft.exp_decay_probs(R=returns, half_life=half_life)
        return probs
    else:
        return _fallback_exp_decay_probs(returns, half_life)


def calculate_normal_calibration(
    returns: Union[pd.DataFrame, np.ndarray],
    half_life: int = 252
) -> Tuple:
    """Calibrate normal distribution with exponential decay"""
    if FORTITUDO_AVAILABLE:
        mean, cov = ft.normal_exp_decay_calib(R=returns, half_life=half_life)
        return mean, cov
    else:
        return _fallback_normal_exp_decay_calib(returns, half_life)


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
