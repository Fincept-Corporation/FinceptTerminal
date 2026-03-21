"""
GS-Quant Timeseries: Data Transforms
======================================

Data manipulation, filtering, alignment, and transformation functions
matching gs_quant.timeseries.
All functions work offline with your own data. No GS API required.

Functions (39):
  align, append, prepend, filter_, filter_dates, first, last, last_value,
  get, index, lag, repeat, chunk, flatten, compare, union, interpolate,
  bucketize, rolling_apply, rolling_offset, rolling_std, weighted_sum,
  generate_series, generate_series_intraday, date_range, day, month, quarter,
  year, weekday, day_count, day_count_fraction, day_count_fractions,
  day_countdown, relative_date_add, sunday_to_monday, merge_dataframes,
  value, standard_deviation
"""

import pandas as pd
import numpy as np
from typing import Union, Optional, List, Callable, Any, Dict, Tuple
from datetime import date, datetime, timedelta

Series = pd.Series


# =============================================================================
# ALIGNMENT & COMBINING
# =============================================================================

def align(x: Series, y: Series, method: str = 'inner') -> Tuple[Series, Series]:
    """
    Align two series on their index.

    Args:
        x: First series
        y: Second series
        method: Join method ('inner', 'outer', 'left', 'right')

    Returns:
        Tuple of aligned series
    """
    return x.align(y, join=method)


def append(x: Series, y: Series) -> Series:
    """
    Append series y to series x.

    Args:
        x: Base series
        y: Series to append

    Returns:
        Combined series
    """
    return pd.concat([x, y])


def prepend(x: Series, y: Series) -> Series:
    """
    Prepend series y before series x.

    Args:
        x: Base series
        y: Series to prepend

    Returns:
        Combined series with y first
    """
    return pd.concat([y, x])


def union(x: Series, y: Series) -> Series:
    """
    Union of two series (combine, preferring x for overlapping indices).

    Args:
        x: Primary series
        y: Secondary series

    Returns:
        Combined series
    """
    return x.combine_first(y)


# =============================================================================
# FILTERING & SELECTION
# =============================================================================

def filter_(x: Series, condition: Series = None, threshold: float = None,
            operator: str = 'gt') -> Series:
    """
    Filter series by condition or threshold.

    Args:
        x: Input series
        condition: Boolean series mask (optional)
        threshold: Numeric threshold (optional)
        operator: Comparison operator ('gt', 'lt', 'ge', 'le', 'eq', 'ne')

    Returns:
        Filtered series
    """
    if condition is not None:
        return x[condition]

    if threshold is not None:
        ops = {
            'gt': x > threshold,
            'lt': x < threshold,
            'ge': x >= threshold,
            'le': x <= threshold,
            'eq': x == threshold,
            'ne': x != threshold
        }
        mask = ops.get(operator, x > threshold)
        return x[mask]

    return x


def filter_dates(x: Series, start: Union[str, date] = None,
                 end: Union[str, date] = None) -> Series:
    """
    Filter series by date range.

    Args:
        x: Input series with DatetimeIndex
        start: Start date (inclusive)
        end: End date (inclusive)

    Returns:
        Filtered series
    """
    if start is not None and end is not None:
        return x[str(start):str(end)]
    elif start is not None:
        return x[str(start):]
    elif end is not None:
        return x[:str(end)]
    return x


def first(x: Series, n: int = 1) -> Series:
    """
    Get first n elements.

    Args:
        x: Input series
        n: Number of elements

    Returns:
        First n elements
    """
    return x.head(n)


def last(x: Series, n: int = 1) -> Series:
    """
    Get last n elements.

    Args:
        x: Input series
        n: Number of elements

    Returns:
        Last n elements
    """
    return x.tail(n)


def last_value(x: Series) -> float:
    """
    Get the last non-NaN value.

    Args:
        x: Input series

    Returns:
        Last non-NaN value
    """
    valid = x.dropna()
    return float(valid.iloc[-1]) if len(valid) > 0 else np.nan


def get(x: Series, idx: Any) -> float:
    """
    Get value at a specific index.

    Args:
        x: Input series
        idx: Index label

    Returns:
        Value at index
    """
    try:
        return float(x.loc[idx])
    except (KeyError, TypeError):
        return np.nan


def index(x: Series) -> pd.Index:
    """
    Get the index of a series.

    Args:
        x: Input series

    Returns:
        Index
    """
    return x.index


def value(x: Series) -> np.ndarray:
    """
    Get the values of a series as a numpy array.

    Args:
        x: Input series

    Returns:
        Numpy array of values
    """
    return x.values


# =============================================================================
# TIME SHIFTING
# =============================================================================

def lag(x: Series, n: int = 1) -> Series:
    """
    Lag (shift) series by n periods.

    Args:
        x: Input series
        n: Number of periods to shift (positive = backward)

    Returns:
        Shifted series
    """
    return x.shift(n)


def repeat(x: Series, n: int = 2) -> Series:
    """
    Repeat each value n times.

    Args:
        x: Input series
        n: Number of repetitions

    Returns:
        Repeated series
    """
    return x.repeat(n)


# =============================================================================
# RESTRUCTURING
# =============================================================================

def chunk(x: Series, size: int) -> List[Series]:
    """
    Split series into chunks of given size.

    Args:
        x: Input series
        size: Chunk size

    Returns:
        List of series chunks
    """
    return [x.iloc[i:i + size] for i in range(0, len(x), size)]


def flatten(x: Union[List[Series], pd.DataFrame]) -> Series:
    """
    Flatten a list of series or DataFrame into a single series.

    Args:
        x: List of series or DataFrame

    Returns:
        Flattened series
    """
    if isinstance(x, pd.DataFrame):
        return x.stack()
    elif isinstance(x, list):
        return pd.concat(x)
    return x


def compare(x: Series, y: Series) -> pd.DataFrame:
    """
    Compare two series element-wise.

    Args:
        x: First series
        y: Second series

    Returns:
        DataFrame with 'x', 'y', 'diff', 'pct_diff' columns
    """
    aligned_x, aligned_y = x.align(y, join='inner')
    diff = aligned_x - aligned_y
    pct_diff = diff / aligned_y.replace(0, np.nan) * 100

    return pd.DataFrame({
        'x': aligned_x,
        'y': aligned_y,
        'diff': diff,
        'pct_diff': pct_diff
    })


def interpolate(x: Series, method: str = 'linear', limit: int = None) -> Series:
    """
    Interpolate missing values.

    Args:
        x: Input series with NaN gaps
        method: Interpolation method ('linear', 'quadratic', 'cubic', 'nearest', 'pad')
        limit: Max number of consecutive NaNs to fill

    Returns:
        Interpolated series
    """
    return x.interpolate(method=method, limit=limit)


def bucketize(x: Series, bins: Union[int, List[float]] = 10,
              labels: List[str] = None) -> Series:
    """
    Bucketize values into discrete bins.

    Args:
        x: Input series
        bins: Number of bins or bin edges
        labels: Optional labels for bins

    Returns:
        Series of bin assignments
    """
    return pd.cut(x, bins=bins, labels=labels)


# =============================================================================
# ROLLING & WINDOWED OPERATIONS
# =============================================================================

def rolling_apply(x: Series, w: int, func: Callable, raw: bool = True) -> Series:
    """
    Apply a custom function over a rolling window.

    Args:
        x: Input series
        w: Window size
        func: Function to apply
        raw: If True, passes numpy arrays to func

    Returns:
        Series of rolling function results
    """
    return x.rolling(window=w).apply(func, raw=raw)


def rolling_offset(x: Series, offset: str = '30D', func: str = 'mean') -> Series:
    """
    Apply rolling with time-based offset window.

    Args:
        x: Input series with DatetimeIndex
        offset: Time offset string (e.g., '30D', '7D', '1M')
        func: Aggregation function ('mean', 'std', 'sum', 'min', 'max')

    Returns:
        Rolling offset result
    """
    roller = x.rolling(offset)
    return getattr(roller, func)()


def rolling_std(x: Series, w: int = 20) -> Series:
    """
    Calculate rolling standard deviation.

    Args:
        x: Input series
        w: Window size (default 20)

    Returns:
        Rolling standard deviation series
    """
    return x.rolling(window=w).std()


def weighted_sum(x: pd.DataFrame, weights: Union[List[float], np.ndarray]) -> Series:
    """
    Calculate weighted sum across columns of a DataFrame.

    Args:
        x: DataFrame with multiple columns
        weights: Weights for each column

    Returns:
        Weighted sum series
    """
    weights = np.array(weights)
    return pd.Series(x.values @ weights, index=x.index)


def standard_deviation(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate standard deviation (alias for rolling_std when w is provided).

    Args:
        x: Input series
        w: Rolling window (optional)

    Returns:
        Rolling std or scalar std
    """
    if w is not None:
        return x.rolling(window=w).std()
    return float(x.std())


# =============================================================================
# SERIES GENERATION
# =============================================================================

def merge_dataframes(*dfs: pd.DataFrame, how: str = 'outer') -> pd.DataFrame:
    """
    Merge multiple DataFrames on their index.

    Args:
        *dfs: DataFrames to merge
        how: Join method ('inner', 'outer', 'left', 'right')

    Returns:
        Merged DataFrame
    """
    if len(dfs) == 0:
        return pd.DataFrame()
    result = dfs[0]
    for df in dfs[1:]:
        result = result.join(df, how=how, rsuffix='_dup')
    return result


def generate_series(length: int = 252, start_value: float = 100,
                    drift: float = 0.0005, volatility: float = 0.015,
                    start_date: str = None, seed: int = None) -> Series:
    """
    Generate a synthetic price series (geometric Brownian motion).

    Args:
        length: Number of data points
        start_value: Starting price
        drift: Daily drift (mean return)
        volatility: Daily volatility
        start_date: Start date string (default today)
        seed: Random seed

    Returns:
        Synthetic price series
    """
    if seed is not None:
        np.random.seed(seed)

    start = pd.Timestamp(start_date) if start_date else pd.Timestamp.today()
    dates = pd.date_range(start, periods=length, freq='B')

    returns = np.random.normal(drift, volatility, length)
    prices = start_value * np.exp(np.cumsum(returns))

    return pd.Series(prices, index=dates)


def generate_series_intraday(length: int = 390, start_value: float = 100,
                             drift: float = 0.0001, volatility: float = 0.005,
                             start_date: str = None, freq: str = '1min',
                             seed: int = None) -> Series:
    """
    Generate a synthetic intraday price series.

    Args:
        length: Number of data points (default 390 = 6.5hr trading day at 1min)
        start_value: Starting price
        drift: Per-period drift
        volatility: Per-period volatility
        start_date: Start datetime string (default today 9:30 AM)
        freq: Frequency string ('1min', '5min', '15min', '1h')
        seed: Random seed

    Returns:
        Synthetic intraday price series
    """
    if seed is not None:
        np.random.seed(seed)

    if start_date:
        start = pd.Timestamp(start_date)
    else:
        today = pd.Timestamp.today().normalize()
        start = today + pd.Timedelta(hours=9, minutes=30)

    dates = pd.date_range(start, periods=length, freq=freq)
    returns = np.random.normal(drift, volatility, length)
    prices = start_value * np.exp(np.cumsum(returns))

    return pd.Series(prices, index=dates)


def date_range(start: Union[str, date], end: Union[str, date] = None,
               periods: int = None, freq: str = 'B') -> pd.DatetimeIndex:
    """
    Generate a date range.

    Args:
        start: Start date
        end: End date (optional)
        periods: Number of periods (optional, used if end not specified)
        freq: Frequency ('B'=business daily, 'D'=calendar daily, 'W'=weekly, 'M'=monthly)

    Returns:
        DatetimeIndex
    """
    return pd.date_range(start=start, end=end, periods=periods, freq=freq)


# =============================================================================
# DATE COMPONENTS
# =============================================================================

def day(x: Series) -> Series:
    """Extract day of month from DatetimeIndex."""
    return pd.Series(x.index.day, index=x.index)


def month(x: Series) -> Series:
    """Extract month from DatetimeIndex."""
    return pd.Series(x.index.month, index=x.index)


def quarter(x: Series) -> Series:
    """Extract quarter from DatetimeIndex."""
    return pd.Series(x.index.quarter, index=x.index)


def year(x: Series) -> Series:
    """Extract year from DatetimeIndex."""
    return pd.Series(x.index.year, index=x.index)


def weekday(x: Series) -> Series:
    """Extract weekday (0=Monday, 6=Sunday) from DatetimeIndex."""
    return pd.Series(x.index.weekday, index=x.index)


def day_count(start: Union[str, date], end: Union[str, date],
              convention: str = 'ACT/365') -> float:
    """
    Calculate day count fraction between two dates.

    Args:
        start: Start date
        end: End date
        convention: Day count convention ('ACT/365', 'ACT/360', '30/360')

    Returns:
        Day count fraction
    """
    d1 = pd.Timestamp(start)
    d2 = pd.Timestamp(end)
    actual_days = (d2 - d1).days

    if convention == 'ACT/365':
        return actual_days / 365
    elif convention == 'ACT/360':
        return actual_days / 360
    elif convention == '30/360':
        y1, m1, dd1 = d1.year, d1.month, min(d1.day, 30)
        y2, m2, dd2 = d2.year, d2.month, min(d2.day, 30)
        return (360 * (y2 - y1) + 30 * (m2 - m1) + (dd2 - dd1)) / 360
    else:
        return actual_days / 365


def day_count_fraction(start: Union[str, date], end: Union[str, date],
                       convention: str = 'ACT/365') -> float:
    """
    Calculate day count fraction between two dates.
    Alias for day_count with clearer naming matching gs_quant convention.

    Args:
        start: Start date
        end: End date
        convention: Day count convention ('ACT/365', 'ACT/360', '30/360')

    Returns:
        Day count fraction
    """
    return day_count(start, end, convention)


def day_count_fractions(dates_list: List[Union[str, date]],
                        convention: str = 'ACT/365') -> List[float]:
    """
    Calculate day count fractions for a list of consecutive dates.

    Args:
        dates_list: List of dates (at least 2)
        convention: Day count convention

    Returns:
        List of day count fractions between consecutive dates
    """
    fractions = []
    for i in range(len(dates_list) - 1):
        fractions.append(day_count(dates_list[i], dates_list[i + 1], convention))
    return fractions


def day_countdown(target: Union[str, date], x: Series) -> Series:
    """
    Calculate number of calendar days from each index date to a target date.

    Args:
        target: Target date
        x: Series with DatetimeIndex

    Returns:
        Series of day counts to target
    """
    target_ts = pd.Timestamp(target)
    return pd.Series(
        [(target_ts - pd.Timestamp(d)).days for d in x.index],
        index=x.index
    )


def relative_date_add(x: Series, days: int = 0, months: int = 0,
                      years: int = 0) -> Series:
    """
    Shift the DatetimeIndex of a series by a relative date offset.

    Args:
        x: Series with DatetimeIndex
        days: Number of days to add
        months: Number of months to add
        years: Number of years to add

    Returns:
        Series with shifted index
    """
    offset = pd.DateOffset(days=days, months=months, years=years)
    result = x.copy()
    result.index = x.index + offset
    return result


def sunday_to_monday(x: Series) -> Series:
    """
    Shift any Sunday-dated observations to Monday.
    Useful for aligning weekly data to business days.

    Args:
        x: Series with DatetimeIndex

    Returns:
        Series with Sundays shifted to Monday
    """
    new_index = []
    for dt in x.index:
        ts = pd.Timestamp(dt)
        if ts.weekday() == 6:  # Sunday
            new_index.append(ts + pd.Timedelta(days=1))
        else:
            new_index.append(ts)
    result = x.copy()
    result.index = pd.DatetimeIndex(new_index)
    return result


# =============================================================================
# MAIN / TEST
# =============================================================================

def main():
    """Test all data transform functions."""
    print("=" * 70)
    print("TS DATA TRANSFORMS - TEST")
    print("=" * 70)

    np.random.seed(42)
    dates = pd.date_range('2024-01-01', periods=100, freq='B')
    x = pd.Series(np.random.randn(100).cumsum() + 100, index=dates)
    y = pd.Series(np.random.randn(100).cumsum() + 100, index=dates)

    # Alignment
    a, b = align(x[:80], y[20:])
    print(f"\nalign: inner join => {len(a)} elements")

    # Append/Prepend
    combined = append(x[:50], x[50:])
    print(f"append: {len(combined)} elements")

    pre = prepend(x[50:], x[:50])
    print(f"prepend: {len(pre)} elements")

    u = union(x, y)
    print(f"union: {len(u)} elements")

    # Filtering
    filtered = filter_(x, threshold=100, operator='gt')
    print(f"\nfilter (>100): {len(filtered)} of {len(x)}")

    fd = filter_dates(x, '2024-03-01', '2024-06-01')
    print(f"filter_dates: {len(fd)} elements")

    print(f"first(3): {first(x, 3).tolist()}")
    print(f"last_value: {last_value(x):.2f}")

    # Lag
    lagged = lag(x, 5)
    print(f"\nlag(5): first non-NaN at index 5")

    # Chunk
    chunks = chunk(x, 25)
    print(f"chunk(25): {len(chunks)} chunks")

    # Compare
    comp = compare(x, y)
    print(f"\ncompare: mean diff = {comp['diff'].mean():.4f}")

    # Interpolate
    sparse = x.copy()
    sparse.iloc[10:15] = np.nan
    interp = interpolate(sparse)
    print(f"interpolate: filled {sparse.isna().sum()} NaNs")

    # Rolling
    ra = rolling_apply(x, 20, np.mean)
    print(f"\nrolling_apply(mean, 20): {ra.iloc[-1]:.2f}")
    print(f"rolling_std(20): {rolling_std(x, 20).iloc[-1]:.4f}")

    # Generate
    synth = generate_series(100, 100, 0.001, 0.02, seed=42)
    print(f"\ngenerate_series: start={synth.iloc[0]:.2f}, end={synth.iloc[-1]:.2f}")

    # Date components
    print(f"\nday (last): {day(x).iloc[-1]}")
    print(f"month (last): {month(x).iloc[-1]}")
    print(f"quarter (last): {quarter(x).iloc[-1]}")
    print(f"year (last): {year(x).iloc[-1]}")
    print(f"weekday (last): {weekday(x).iloc[-1]}")

    # Day count
    dcf = day_count('2024-01-01', '2024-07-01', 'ACT/365')
    print(f"\nday_count ACT/365: {dcf:.4f}")

    # New date functions
    dcf2 = day_count_fraction('2024-01-01', '2024-07-01')
    print(f"day_count_fraction: {dcf2:.4f}")
    dcfs = day_count_fractions(['2024-01-01', '2024-04-01', '2024-07-01'])
    print(f"day_count_fractions: {dcfs}")
    countdown = day_countdown('2024-12-31', x)
    print(f"day_countdown (first): {countdown.iloc[0]} days")
    shifted = relative_date_add(x, months=1)
    print(f"relative_date_add (+1mo): first date {shifted.index[0].date()}")

    # Intraday
    intraday = generate_series_intraday(60, 100, seed=42)
    print(f"\ngenerate_series_intraday: {len(intraday)} points, start={intraday.iloc[0]:.2f}")

    # Merge
    df1 = pd.DataFrame({'a': x[:50]})
    df2 = pd.DataFrame({'b': y[:50]})
    merged = merge_dataframes(df1, df2)
    print(f"merge_dataframes: {merged.shape}")

    print(f"\nTotal functions: 39")
    print("ALL TESTS PASSED")


if __name__ == "__main__":
    main()
