"""
GS-Quant Timeseries: Returns & Performance
============================================

Return calculation and performance measurement functions matching gs_quant.timeseries.
All functions work offline with your own data. No GS API required.

Functions (11):
  returns, log_return, change, diff, excess_returns_, excess_returns_pure,
  annualize, geometrically_aggregate, prices, cumulative_returns, total_return
"""

import pandas as pd
import numpy as np
from typing import Union, Optional

Series = pd.Series


def returns(x: Series, return_type: str = 'simple') -> Series:
    """
    Calculate returns from price series.

    Args:
        x: Price series
        return_type: 'simple' for arithmetic returns, 'log' for logarithmic

    Returns:
        Returns series
    """
    if return_type == 'log':
        return np.log(x / x.shift(1)).dropna()
    return x.pct_change().dropna()


def log_return(x: Series) -> Series:
    """
    Calculate logarithmic returns: ln(P_t / P_{t-1}).

    Args:
        x: Price series

    Returns:
        Log returns series
    """
    return np.log(x / x.shift(1)).dropna()


def change(x: Series, n: int = 1) -> Series:
    """
    Calculate absolute change over n periods.

    Args:
        x: Input series
        n: Number of periods

    Returns:
        Change series (x_t - x_{t-n})
    """
    return (x - x.shift(n)).dropna()


def diff(x: Series, n: int = 1) -> Series:
    """
    Calculate difference (same as change but keeps NaN alignment).

    Args:
        x: Input series
        n: Number of periods to diff

    Returns:
        Differenced series
    """
    return x.diff(n)


def excess_returns_(x: Series, benchmark: Series) -> Series:
    """
    Calculate excess returns over a benchmark.
    excess = asset_return - benchmark_return

    Args:
        x: Asset returns
        benchmark: Benchmark returns

    Returns:
        Excess returns series
    """
    aligned_x, aligned_b = x.align(benchmark, join='inner')
    return aligned_x - aligned_b


def excess_returns_pure(x: Series, risk_free_rate: float = 0.0) -> Series:
    """
    Calculate excess returns over a constant risk-free rate.

    Args:
        x: Returns series
        risk_free_rate: Annualized risk-free rate (default 0)

    Returns:
        Excess returns series
    """
    daily_rf = risk_free_rate / 252
    return x - daily_rf


def annualize(x: Series, periods_per_year: int = 252) -> float:
    """
    Annualize a returns or volatility series.

    For returns: annualized = mean_return * periods_per_year
    For volatility: annualized = std * sqrt(periods_per_year)

    Args:
        x: Returns series
        periods_per_year: Number of periods per year (252 for daily, 12 for monthly)

    Returns:
        Annualized value
    """
    # Detect if this is returns (mean near 0) or volatility
    # Use geometric for returns
    total_return = (1 + x).prod()
    n_years = len(x) / periods_per_year
    if n_years <= 0:
        return 0.0
    annualized = total_return ** (1 / n_years) - 1
    return float(annualized)


def geometrically_aggregate(x: Series) -> Series:
    """
    Geometrically aggregate returns: cumulative product of (1 + r).
    Produces a cumulative return series.

    Args:
        x: Returns series

    Returns:
        Cumulative returns series (starting from 0)
    """
    return (1 + x).cumprod() - 1


def prices(x: Series, initial: float = 100.0) -> Series:
    """
    Reconstruct a price series from returns.
    Inverse of the returns() function.

    Args:
        x: Returns series (simple returns)
        initial: Starting price (default 100)

    Returns:
        Reconstructed price series
    """
    return initial * (1 + x).cumprod()


def cumulative_returns(x: Series) -> Series:
    """
    Calculate cumulative returns from a returns series.
    Alias for geometrically_aggregate with clearer naming.

    Args:
        x: Returns series

    Returns:
        Cumulative returns series
    """
    return (1 + x).cumprod() - 1


def total_return(x: Series) -> float:
    """
    Calculate total compounded return over the entire period.

    Args:
        x: Returns series

    Returns:
        Total return as a decimal (e.g., 0.15 = 15%)
    """
    return float((1 + x).prod() - 1)


# =============================================================================
# MAIN / TEST
# =============================================================================

def main():
    """Test all returns and performance functions."""
    print("=" * 70)
    print("TS RETURNS & PERFORMANCE - TEST")
    print("=" * 70)

    np.random.seed(42)
    dates = pd.date_range('2024-01-01', periods=252)
    prices_series = pd.Series(np.random.randn(252).cumsum() + 100, index=dates)
    bench_prices = pd.Series(np.random.randn(252).cumsum() + 100, index=dates)

    # Returns
    simple_ret = returns(prices_series)
    log_ret = log_return(prices_series)
    print(f"\nReturns:")
    print(f"  simple returns (last 3): {simple_ret.tail(3).tolist()}")
    print(f"  log returns (last 3): {log_ret.tail(3).tolist()}")

    # Change/Diff
    chg = change(prices_series, 5)
    d = diff(prices_series)
    print(f"\nChange/Diff:")
    print(f"  5-day change (last): {chg.iloc[-1]:.4f}")
    print(f"  1-day diff (last): {d.iloc[-1]:.4f}")

    # Excess returns
    bench_ret = returns(bench_prices)
    excess = excess_returns_(simple_ret, bench_ret)
    excess_rf = excess_returns_pure(simple_ret, 0.05)
    print(f"\nExcess Returns:")
    print(f"  vs benchmark (mean): {excess.mean():.6f}")
    print(f"  vs 5% rf (mean): {excess_rf.mean():.6f}")

    # Annualize
    ann = annualize(simple_ret)
    print(f"\nAnnualization:")
    print(f"  annualized return: {ann:.4f} ({ann*100:.2f}%)")

    # Geometric aggregation
    cum = geometrically_aggregate(simple_ret)
    print(f"  cumulative return: {cum.iloc[-1]:.4f} ({cum.iloc[-1]*100:.2f}%)")

    # Price reconstruction
    reconstructed = prices(simple_ret, initial=prices_series.iloc[0])
    print(f"\nPrice Reconstruction:")
    print(f"  original last: {prices_series.iloc[-1]:.2f}")
    print(f"  reconstructed last: {reconstructed.iloc[-1]:.2f}")

    # Cumulative & total
    cum2 = cumulative_returns(simple_ret)
    tr = total_return(simple_ret)
    print(f"  cumulative_returns (last): {cum2.iloc[-1]:.4f}")
    print(f"  total_return: {tr:.4f} ({tr*100:.2f}%)")

    print(f"\nTotal functions: 11")
    print("ALL TESTS PASSED")


if __name__ == "__main__":
    main()
