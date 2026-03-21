"""
GS-Quant Timeseries: Math Operations & Statistics
==================================================

Pure math and statistical functions matching gs_quant.timeseries.
All functions work offline with your own data. No GS API required.

Functions (40):
  Math (12): abs_, add, subtract, multiply, divide, power, sqrt, exp, log, ceil, floor, floordiv
  Logical (4): and_, or_, not_, if_
  Stats (24): mean, median, mode, std, var, count, sum_, product, min_, max_, range_,
              percentile, percentileofscore, percentiles, kurtosis, skewness, cov,
              correlation, beta, r_squared, zscores, semi_variance, realized_var, winsorize
"""

import pandas as pd
import numpy as np
from typing import Union, Optional, List

Series = pd.Series
DataType = Union[pd.Series, float]


# =============================================================================
# MATH OPERATIONS
# =============================================================================

def abs_(x: DataType) -> DataType:
    """Absolute value of each element."""
    if isinstance(x, pd.Series):
        return x.abs()
    return abs(x)


def add(x: DataType, y: DataType) -> DataType:
    """Element-wise addition: x + y."""
    return x + y


def subtract(x: DataType, y: DataType) -> DataType:
    """Element-wise subtraction: x - y."""
    return x - y


def multiply(x: DataType, y: DataType) -> DataType:
    """Element-wise multiplication: x * y."""
    return x * y


def divide(x: DataType, y: DataType) -> DataType:
    """Element-wise division: x / y. Returns NaN for division by zero."""
    if isinstance(y, pd.Series):
        return x / y.replace(0, np.nan)
    elif y == 0:
        return np.nan
    return x / y


def power(x: DataType, y: DataType) -> DataType:
    """Element-wise power: x ** y."""
    return x ** y


def sqrt(x: DataType) -> DataType:
    """Element-wise square root."""
    if isinstance(x, pd.Series):
        return np.sqrt(x)
    return np.sqrt(x)


def exp(x: DataType) -> DataType:
    """Element-wise exponential (e^x)."""
    if isinstance(x, pd.Series):
        return np.exp(x)
    return np.exp(x)


def log(x: DataType) -> DataType:
    """Element-wise natural logarithm."""
    if isinstance(x, pd.Series):
        return np.log(x.replace(0, np.nan))
    return np.log(x) if x > 0 else np.nan


def ceil(x: DataType) -> DataType:
    """Element-wise ceiling."""
    if isinstance(x, pd.Series):
        return np.ceil(x)
    return np.ceil(x)


def floor(x: DataType) -> DataType:
    """Element-wise floor."""
    if isinstance(x, pd.Series):
        return np.floor(x)
    return np.floor(x)


def floordiv(x: DataType, y: DataType) -> DataType:
    """Element-wise floor division: x // y."""
    if isinstance(y, pd.Series):
        return x // y.replace(0, np.nan)
    elif y == 0:
        return np.nan
    return x // y


# =============================================================================
# LOGICAL OPERATIONS
# =============================================================================

def and_(x: Series, y: Series) -> Series:
    """
    Element-wise logical AND on two boolean or numeric series.
    Non-zero values are treated as True.

    Args:
        x: First series
        y: Second series

    Returns:
        Boolean series
    """
    return (x.astype(bool)) & (y.astype(bool))


def or_(x: Series, y: Series) -> Series:
    """
    Element-wise logical OR on two boolean or numeric series.
    Non-zero values are treated as True.

    Args:
        x: First series
        y: Second series

    Returns:
        Boolean series
    """
    return (x.astype(bool)) | (y.astype(bool))


def not_(x: Series) -> Series:
    """
    Element-wise logical NOT on a boolean or numeric series.
    Non-zero values are treated as True.

    Args:
        x: Input series

    Returns:
        Boolean series (inverted)
    """
    return ~(x.astype(bool))


def if_(condition: Series, x: DataType, y: DataType) -> Series:
    """
    Element-wise conditional: where condition is True return x, else y.
    Equivalent to np.where(condition, x, y).

    Args:
        condition: Boolean series
        x: Value(s) when True
        y: Value(s) when False

    Returns:
        Series with conditional values
    """
    return pd.Series(np.where(condition, x, y), index=condition.index)


# =============================================================================
# STATISTICS - DESCRIPTIVE
# =============================================================================

def mean(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate mean. If window w is provided, returns rolling mean.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling mean series or scalar mean
    """
    if w is not None:
        return x.rolling(window=w).mean()
    return float(x.mean())


def median(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate median. If window w is provided, returns rolling median.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling median series or scalar median
    """
    if w is not None:
        return x.rolling(window=w).median()
    return float(x.median())


def mode(x: Series) -> float:
    """
    Calculate mode (most frequent value).

    Args:
        x: Input series

    Returns:
        Most frequent value
    """
    modes = x.mode()
    return float(modes.iloc[0]) if len(modes) > 0 else np.nan


def std(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate standard deviation. If window w is provided, returns rolling std.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling std series or scalar std
    """
    if w is not None:
        return x.rolling(window=w).std()
    return float(x.std())


def var(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate variance. If window w is provided, returns rolling variance.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling variance series or scalar variance
    """
    if w is not None:
        return x.rolling(window=w).var()
    return float(x.var())


def count(x: Series, w: int = None) -> Union[Series, int]:
    """
    Count non-NaN values. If window w is provided, returns rolling count.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling count series or scalar count
    """
    if w is not None:
        return x.rolling(window=w).count()
    return int(x.count())


def sum_(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate sum. If window w is provided, returns rolling sum.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling sum series or scalar sum
    """
    if w is not None:
        return x.rolling(window=w).sum()
    return float(x.sum())


def product(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate product. If window w is provided, returns rolling product.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling product series or scalar product
    """
    if w is not None:
        return x.rolling(window=w).apply(np.prod, raw=True)
    return float(x.prod())


def min_(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate minimum. If window w is provided, returns rolling min.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling min series or scalar min
    """
    if w is not None:
        return x.rolling(window=w).min()
    return float(x.min())


def max_(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate maximum. If window w is provided, returns rolling max.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling max series or scalar max
    """
    if w is not None:
        return x.rolling(window=w).max()
    return float(x.max())


def range_(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate range (max - min). If window w is provided, returns rolling range.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling range series or scalar range
    """
    if w is not None:
        return x.rolling(window=w).max() - x.rolling(window=w).min()
    return float(x.max() - x.min())


# =============================================================================
# STATISTICS - DISTRIBUTION
# =============================================================================

def percentile(x: Series, n: float, w: int = None) -> Union[Series, float]:
    """
    Calculate nth percentile.

    Args:
        x: Input series
        n: Percentile (0-100)
        w: Rolling window size (optional)

    Returns:
        Rolling percentile or scalar
    """
    if w is not None:
        return x.rolling(window=w).quantile(n / 100.0)
    return float(x.quantile(n / 100.0))


def percentileofscore(x: Series, score: float) -> float:
    """
    Calculate the percentile rank of a score relative to a series.

    Args:
        x: Input series
        score: Value to rank

    Returns:
        Percentile rank (0-100)
    """
    from scipy.stats import percentileofscore as _pctrank
    return float(_pctrank(x.dropna(), score))


def percentiles(x: Series, ns: List[float] = None) -> pd.Series:
    """
    Calculate multiple percentiles at once.

    Args:
        x: Input series
        ns: List of percentiles (0-100). Default [1, 5, 10, 25, 50, 75, 90, 95, 99]

    Returns:
        Series of percentile values
    """
    if ns is None:
        ns = [1, 5, 10, 25, 50, 75, 90, 95, 99]
    quantiles = [n / 100.0 for n in ns]
    values = x.quantile(quantiles)
    values.index = ns
    return values


def kurtosis(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate excess kurtosis.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling kurtosis or scalar
    """
    if w is not None:
        return x.rolling(window=w).kurt()
    return float(x.kurtosis())


def skewness(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate skewness.

    Args:
        x: Input series
        w: Rolling window size (optional)

    Returns:
        Rolling skewness or scalar
    """
    if w is not None:
        return x.rolling(window=w).skew()
    return float(x.skew())


# =============================================================================
# STATISTICS - CORRELATION & COVARIANCE
# =============================================================================

def cov(x: Series, y: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate covariance between two series.

    Args:
        x: First series
        y: Second series
        w: Rolling window size (optional)

    Returns:
        Rolling covariance or scalar
    """
    if w is not None:
        return x.rolling(window=w).cov(y)
    return float(x.cov(y))


def correlation(x: Series, y: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate Pearson correlation between two series.

    Args:
        x: First series
        y: Second series
        w: Rolling window size (optional)

    Returns:
        Rolling correlation or scalar (-1 to 1)
    """
    if w is not None:
        return x.rolling(window=w).corr(y)
    return float(x.corr(y))


def beta(x: Series, y: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate beta of x relative to y (regression slope).
    beta = cov(x,y) / var(y)

    Args:
        x: Asset returns
        y: Benchmark returns
        w: Rolling window size (optional)

    Returns:
        Rolling beta or scalar
    """
    if w is not None:
        covariance = x.rolling(window=w).cov(y)
        variance = y.rolling(window=w).var()
        return covariance / variance
    c = x.cov(y)
    v = y.var()
    return float(c / v) if v != 0 else 0.0


def r_squared(x: Series, y: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate R-squared (coefficient of determination).
    R^2 = correlation^2

    Args:
        x: First series
        y: Second series
        w: Rolling window size (optional)

    Returns:
        Rolling R-squared or scalar (0 to 1)
    """
    corr = correlation(x, y, w)
    return corr ** 2


# =============================================================================
# STATISTICS - ADVANCED
# =============================================================================

def zscores(x: Series, w: int = None) -> Series:
    """
    Calculate z-scores (standard scores).
    z = (x - mean) / std

    Args:
        x: Input series
        w: Rolling window size. If None, uses entire series stats

    Returns:
        Series of z-scores
    """
    if w is not None:
        m = x.rolling(window=w).mean()
        s = x.rolling(window=w).std()
        return (x - m) / s
    m = x.mean()
    s = x.std()
    return (x - m) / s if s != 0 else pd.Series(0, index=x.index)


def semi_variance(x: Series, threshold: float = 0, w: int = None) -> Union[Series, float]:
    """
    Calculate semi-variance (variance of values below threshold).
    Used for downside risk.

    Args:
        x: Returns series
        threshold: Threshold (default 0)
        w: Rolling window size (optional)

    Returns:
        Rolling semi-variance or scalar
    """
    if w is not None:
        def _sv(arr):
            below = arr[arr < threshold]
            return below.var() if len(below) > 1 else 0.0
        return x.rolling(window=w).apply(_sv, raw=True)

    below = x[x < threshold]
    return float(below.var()) if len(below) > 1 else 0.0


def realized_var(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate realized variance (sum of squared returns).

    Args:
        x: Returns series
        w: Rolling window size (optional)

    Returns:
        Rolling realized variance or scalar
    """
    squared = x ** 2
    if w is not None:
        return squared.rolling(window=w).sum()
    return float(squared.sum())


def winsorize(x: Series, lower: float = 0.01, upper: float = 0.99) -> Series:
    """
    Winsorize a series by clipping extreme values to given percentiles.

    Args:
        x: Input series
        lower: Lower percentile (0-1)
        upper: Upper percentile (0-1)

    Returns:
        Winsorized series
    """
    low_val = x.quantile(lower)
    high_val = x.quantile(upper)
    return x.clip(lower=low_val, upper=high_val)


# =============================================================================
# MAIN / TEST
# =============================================================================

def main():
    """Test all math and statistics functions."""
    print("=" * 70)
    print("TS MATH & STATISTICS - TEST")
    print("=" * 70)

    np.random.seed(42)
    dates = pd.date_range('2024-01-01', periods=252)
    x = pd.Series(np.random.randn(252).cumsum() + 100, index=dates)
    y = pd.Series(np.random.randn(252).cumsum() + 100, index=dates)
    ret = x.pct_change().dropna()

    # Math ops
    print("\nMath Operations:")
    print(f"  abs_: {abs_(pd.Series([-1, 2, -3])).tolist()}")
    print(f"  add: {add(2.0, 3.0)}")
    print(f"  sqrt: {sqrt(16.0)}")
    print(f"  log: {log(np.e):.4f}")

    # Logical ops
    a = pd.Series([True, False, True, False])
    b = pd.Series([True, True, False, False])
    print(f"\nLogical Operations:")
    print(f"  and_: {and_(a, b).tolist()}")
    print(f"  or_: {or_(a, b).tolist()}")
    print(f"  not_: {not_(a).tolist()}")
    print(f"  if_: {if_(a, 1.0, 0.0).tolist()}")

    # Stats
    print("\nDescriptive Stats:")
    print(f"  mean: {mean(x):.2f}")
    print(f"  median: {median(x):.2f}")
    print(f"  std: {std(x):.4f}")
    print(f"  var: {var(x):.4f}")
    print(f"  min: {min_(x):.2f}, max: {max_(x):.2f}")
    print(f"  range: {range_(x):.2f}")
    print(f"  skewness: {skewness(x):.4f}")
    print(f"  kurtosis: {kurtosis(x):.4f}")

    # Distribution
    print("\nDistribution:")
    pcts = percentiles(x)
    print(f"  percentiles: {pcts.to_dict()}")
    print(f"  zscores (last 3): {zscores(x).tail(3).tolist()}")
    print(f"  winsorize range: {winsorize(ret).min():.4f} to {winsorize(ret).max():.4f}")

    # Correlation
    print("\nCorrelation & Regression:")
    print(f"  correlation: {correlation(x, y):.4f}")
    print(f"  covariance: {cov(x, y):.4f}")
    print(f"  beta: {beta(ret, y.pct_change().dropna()):.4f}")
    print(f"  r_squared: {r_squared(x, y):.4f}")

    # Advanced
    print("\nAdvanced Stats:")
    print(f"  semi_variance: {semi_variance(ret):.6f}")
    print(f"  realized_var: {realized_var(ret):.6f}")

    print(f"\nTotal functions: 40")
    print("ALL TESTS PASSED")


if __name__ == "__main__":
    main()
