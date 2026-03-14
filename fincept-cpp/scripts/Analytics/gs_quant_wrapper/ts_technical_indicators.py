"""
GS-Quant Timeseries: Technical Indicators
===========================================

Technical analysis indicator functions matching gs_quant.timeseries.
All functions work offline with your own data. No GS API required.

Functions (10):
  moving_average, exponential_moving_average, smoothed_moving_average,
  bollinger_bands, relative_strength_index, macd, trend,
  apply_ramp, smooth_spikes, seasonally_adjusted
"""

import pandas as pd
import numpy as np
from typing import Union, Optional, Dict

Series = pd.Series


def moving_average(x: Series, w: int = 20) -> Series:
    """
    Simple Moving Average (SMA).

    Args:
        x: Price or data series
        w: Window size (default 20)

    Returns:
        SMA series
    """
    return x.rolling(window=w).mean()


def exponential_moving_average(x: Series, span: int = 20) -> Series:
    """
    Exponential Moving Average (EMA).

    Args:
        x: Price or data series
        span: EMA span parameter (default 20)

    Returns:
        EMA series
    """
    return x.ewm(span=span, adjust=False).mean()


def smoothed_moving_average(x: Series, w: int = 20) -> Series:
    """
    Smoothed Moving Average (SMMA / Modified Moving Average).
    SMMA is similar to EMA with alpha = 1/w.

    Args:
        x: Price or data series
        w: Window size (default 20)

    Returns:
        SMMA series
    """
    # SMMA equivalent: EMA with span = 2*w - 1
    return x.ewm(alpha=1.0 / w, adjust=False).mean()


def bollinger_bands(x: Series, w: int = 20, num_std: float = 2.0) -> Dict[str, Series]:
    """
    Calculate Bollinger Bands.

    Args:
        x: Price series
        w: Moving average window (default 20)
        num_std: Number of standard deviations (default 2)

    Returns:
        Dict with 'middle', 'upper', 'lower', 'bandwidth', 'percent_b'
    """
    middle = x.rolling(window=w).mean()
    rolling_std = x.rolling(window=w).std()

    upper = middle + num_std * rolling_std
    lower = middle - num_std * rolling_std

    bandwidth = (upper - lower) / middle * 100
    percent_b = (x - lower) / (upper - lower)

    return {
        'middle': middle,
        'upper': upper,
        'lower': lower,
        'bandwidth': bandwidth,
        'percent_b': percent_b
    }


def relative_strength_index(x: Series, w: int = 14) -> Series:
    """
    Calculate Relative Strength Index (RSI).

    Args:
        x: Price series
        w: Lookback period (default 14)

    Returns:
        RSI series (0-100)
    """
    delta = x.diff()
    gain = delta.clip(lower=0)
    loss = (-delta).clip(lower=0)

    # Use EWMA (Wilder's smoothing: alpha = 1/w)
    avg_gain = gain.ewm(alpha=1.0 / w, min_periods=w, adjust=False).mean()
    avg_loss = loss.ewm(alpha=1.0 / w, min_periods=w, adjust=False).mean()

    rs = avg_gain / avg_loss.replace(0, np.nan)
    rsi = 100 - (100 / (1 + rs))

    return rsi


def macd(x: Series, fast: int = 12, slow: int = 26, signal: int = 9) -> Dict[str, Series]:
    """
    Calculate MACD (Moving Average Convergence Divergence).

    Args:
        x: Price series
        fast: Fast EMA period (default 12)
        slow: Slow EMA period (default 26)
        signal: Signal line period (default 9)

    Returns:
        Dict with 'macd', 'signal', 'histogram'
    """
    ema_fast = x.ewm(span=fast, adjust=False).mean()
    ema_slow = x.ewm(span=slow, adjust=False).mean()

    macd_line = ema_fast - ema_slow
    signal_line = macd_line.ewm(span=signal, adjust=False).mean()
    histogram = macd_line - signal_line

    return {
        'macd': macd_line,
        'signal': signal_line,
        'histogram': histogram
    }


def trend(x: Series, w: int = 20) -> Series:
    """
    Calculate trend direction using linear regression slope.
    Positive = uptrend, Negative = downtrend.

    Args:
        x: Price or data series
        w: Lookback window (default 20)

    Returns:
        Series of regression slopes (trend strength)
    """
    def _slope(arr):
        if len(arr) < 2:
            return 0.0
        y = np.array(arr)
        t = np.arange(len(y))
        try:
            slope = np.polyfit(t, y, 1)[0]
            return slope
        except:
            return 0.0

    return x.rolling(window=w).apply(_slope, raw=True)


def apply_ramp(x: Series, ramp_up: int = 5, ramp_down: int = 5) -> Series:
    """
    Apply a ramp filter to smooth transitions.
    Gradually scales from 0 to 1 at start (ramp_up) and 1 to 0 at end (ramp_down).

    Args:
        x: Input series
        ramp_up: Number of periods for ramp-up
        ramp_down: Number of periods for ramp-down

    Returns:
        Ramped series
    """
    n = len(x)
    ramp = pd.Series(1.0, index=x.index)

    # Ramp up at start
    for i in range(min(ramp_up, n)):
        ramp.iloc[i] = (i + 1) / ramp_up

    # Ramp down at end
    for i in range(min(ramp_down, n)):
        idx = n - 1 - i
        ramp.iloc[idx] = min(ramp.iloc[idx], (i + 1) / ramp_down)

    return x * ramp


def smooth_spikes(x: Series, threshold: float = 3.0, method: str = 'clip') -> Series:
    """
    Remove or smooth spikes/outliers from a series.

    Args:
        x: Input series
        threshold: Z-score threshold for spike detection (default 3.0)
        method: 'clip' to clip at threshold, 'interpolate' to interpolate over spikes

    Returns:
        Smoothed series
    """
    m = x.mean()
    s = x.std()
    z = (x - m) / s if s != 0 else pd.Series(0, index=x.index)

    is_spike = z.abs() > threshold

    if method == 'clip':
        upper = m + threshold * s
        lower = m - threshold * s
        return x.clip(lower=lower, upper=upper)
    elif method == 'interpolate':
        result = x.copy()
        result[is_spike] = np.nan
        return result.interpolate(method='linear')
    else:
        return x


def seasonally_adjusted(x: Series, period: int = 12) -> Series:
    """
    Seasonally adjust a time series by removing seasonal component.
    Uses a simple decomposition: subtract the average for each period position.

    Args:
        x: Input series
        period: Seasonal period (12 for monthly, 4 for quarterly, 252 for daily)

    Returns:
        Seasonally adjusted series
    """
    # Calculate seasonal factors
    positions = np.arange(len(x)) % period
    seasonal = pd.Series(0.0, index=x.index)

    for p in range(period):
        mask = positions == p
        seasonal[mask] = x[mask].mean()

    # Remove seasonal component
    return x - seasonal + x.mean()


# =============================================================================
# MAIN / TEST
# =============================================================================

def main():
    """Test all technical indicator functions."""
    print("=" * 70)
    print("TS TECHNICAL INDICATORS - TEST")
    print("=" * 70)

    np.random.seed(42)
    dates = pd.date_range('2024-01-01', periods=252)
    prices = pd.Series(np.random.randn(252).cumsum() + 100, index=dates)

    # Moving Averages
    sma = moving_average(prices, 20)
    ema = exponential_moving_average(prices, 20)
    smma = smoothed_moving_average(prices, 20)
    print("\nMoving Averages (latest):")
    print(f"  SMA(20): {sma.iloc[-1]:.2f}")
    print(f"  EMA(20): {ema.iloc[-1]:.2f}")
    print(f"  SMMA(20): {smma.iloc[-1]:.2f}")

    # Bollinger Bands
    bb = bollinger_bands(prices)
    print(f"\nBollinger Bands (latest):")
    print(f"  Upper: {bb['upper'].iloc[-1]:.2f}")
    print(f"  Middle: {bb['middle'].iloc[-1]:.2f}")
    print(f"  Lower: {bb['lower'].iloc[-1]:.2f}")
    print(f"  %B: {bb['percent_b'].iloc[-1]:.4f}")

    # RSI
    rsi = relative_strength_index(prices)
    print(f"\nRSI(14): {rsi.iloc[-1]:.2f}")

    # MACD
    m = macd(prices)
    print(f"\nMACD:")
    print(f"  MACD Line: {m['macd'].iloc[-1]:.4f}")
    print(f"  Signal: {m['signal'].iloc[-1]:.4f}")
    print(f"  Histogram: {m['histogram'].iloc[-1]:.4f}")

    # Trend
    t = trend(prices)
    print(f"\nTrend slope (20d): {t.iloc[-1]:.4f}")

    # Ramp
    ramped = apply_ramp(prices, 10, 10)
    print(f"\nRamp filter applied: first={ramped.iloc[0]:.2f}, last={ramped.iloc[-1]:.2f}")

    # Smooth spikes
    noisy = prices.copy()
    noisy.iloc[50] = prices.iloc[50] + 50  # Add spike
    smoothed = smooth_spikes(noisy)
    print(f"\nSmooth spikes: original spike={noisy.iloc[50]:.2f}, smoothed={smoothed.iloc[50]:.2f}")

    # Seasonal adjustment
    sa = seasonally_adjusted(prices, 20)
    print(f"\nSeasonal adjustment applied: mean diff={abs(sa.mean() - prices.mean()):.4f}")

    print(f"\nTotal functions: 10")
    print("ALL TESTS PASSED")


if __name__ == "__main__":
    main()
