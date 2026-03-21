"""
Fast-Trade Utilities Module

General utility functions from fast_trade.utils:

Resampling:
- resample(): Resample OHLCV data to different frequency
- resample_calendar(): Resample to calendar frequency (monthly, weekly)

Trend Detection:
- trending_up(): Detect uptrend (N consecutive higher values)
- trending_down(): Detect downtrend (N consecutive lower values)

Data Conversion:
- to_dataframe(): Convert list of tick dicts to DataFrame
- infer_frequency(): Detect frequency from DataFrame index
"""

import pandas as pd
import numpy as np
from typing import List, Optional


# ============================================================================
# Resampling
# ============================================================================

def resample(
    df: pd.DataFrame,
    interval: str
) -> pd.DataFrame:
    """
    Resample OHLCV DataFrame to a different frequency.

    Uses proper OHLCV aggregation:
    - open: first
    - high: max
    - low: min
    - close: last
    - volume: sum

    Args:
        df: OHLCV DataFrame with DatetimeIndex
        interval: Target interval string
            Supported: '1Min', '5Min', '15Min', '30Min',
                       '1H', '4H', '1D', '1W', '1M'

    Returns:
        Resampled DataFrame
    """
    try:
        from fast_trade.utils import resample as ft_resample
        return ft_resample(df, interval)
    except ImportError:
        # Map common strings to pandas offset aliases
        freq_map = {
            '1Min': '1min', '5Min': '5min', '15Min': '15min', '30Min': '30min',
            '1H': '1h', '4H': '4h', '1D': '1D', '1W': '1W', '1M': '1ME',
            '1min': '1min', '5min': '5min', '15min': '15min', '30min': '30min',
            '1h': '1h', '4h': '4h',
        }
        freq = freq_map.get(interval, interval)

        agg = {}
        if 'open' in df.columns:
            agg['open'] = 'first'
        if 'high' in df.columns:
            agg['high'] = 'max'
        if 'low' in df.columns:
            agg['low'] = 'min'
        if 'close' in df.columns:
            agg['close'] = 'last'
        if 'volume' in df.columns:
            agg['volume'] = 'sum'

        resampled = df.resample(freq).agg(agg)
        return resampled.dropna()


def resample_calendar(
    df: pd.DataFrame,
    offset: str
) -> pd.DataFrame:
    """
    Resample to calendar-based frequency.

    Similar to resample() but uses calendar offsets (month-end, week-end, etc.)

    Args:
        df: OHLCV DataFrame
        offset: Calendar offset string ('M', 'W', 'Q', 'Y')

    Returns:
        Resampled DataFrame
    """
    try:
        from fast_trade.utils import resample_calendar as ft_resample_cal
        return ft_resample_cal(df, offset)
    except ImportError:
        return resample(df, offset)


# ============================================================================
# Trend Detection
# ============================================================================

def trending_up(
    series: pd.Series,
    period: int
) -> pd.Series:
    """
    Detect uptrend: True when value has been increasing for N periods.

    Checks if the current value is greater than the value N periods ago,
    applied as a rolling comparison.

    Args:
        series: Price or indicator series
        period: Lookback period

    Returns:
        Boolean Series (True = trending up)
    """
    try:
        from fast_trade.utils import trending_up as ft_up
        return ft_up(series, period)
    except ImportError:
        return series > series.shift(period)


def trending_down(
    series: pd.Series,
    period: int
) -> pd.Series:
    """
    Detect downtrend: True when value has been decreasing for N periods.

    Checks if the current value is less than the value N periods ago.

    Args:
        series: Price or indicator series
        period: Lookback period

    Returns:
        Boolean Series (True = trending down)
    """
    try:
        from fast_trade.utils import trending_down as ft_down
        return ft_down(series, period)
    except ImportError:
        return series < series.shift(period)


# ============================================================================
# Data Conversion
# ============================================================================

def to_dataframe(ticks: List[dict]) -> pd.DataFrame:
    """
    Convert list of tick/candle dictionaries to DataFrame.

    Each dict should have: date, open, high, low, close, volume.

    Args:
        ticks: List of OHLCV dicts

    Returns:
        Standardized DataFrame with DatetimeIndex
    """
    try:
        from fast_trade.utils import to_dataframe as ft_to_df
        return ft_to_df(ticks)
    except ImportError:
        df = pd.DataFrame(ticks)
        if 'date' in df.columns:
            df['date'] = pd.to_datetime(df['date'])
            df = df.set_index('date')
        df.columns = [c.lower() for c in df.columns]
        return df


def infer_frequency(df: pd.DataFrame) -> str:
    """
    Detect data frequency from DataFrame index.

    Analyzes time differences between consecutive rows.

    Args:
        df: DataFrame with DatetimeIndex

    Returns:
        Frequency string ('1Min', '5Min', '1H', '1D', etc.)
    """
    try:
        from fast_trade.utils import infer_frequency as ft_infer
        return ft_infer(df)
    except ImportError:
        from .ft_data import infer_frequency as data_infer
        return data_infer(df)
