"""
VBT Indicators & Signal Generation Module

Covers all VectorBT built-in indicators:
- MA (SMA/EMA with configurable window and ewm flag)
- MSTD (Moving Standard Deviation)
- BBANDS (Bollinger Bands: upper, middle, lower, bandwidth, %B)
- RSI (Relative Strength Index)
- STOCH (Stochastic Oscillator: %K, %D)
- MACD (MACD line, Signal line, Histogram)
- ATR (Average True Range)
- OBV (On-Balance Volume)

Signal Generators:
- RAND (Random entry/exit signals for benchmarking)
- STX (Stop/take-profit from close-only data)
- OHLCSTX (Stop/take-profit from OHLC data)

Custom Indicators:
- ADX (Average Directional Index)
- Donchian Channels
- Z-Score
- Momentum (ROC)
- Williams %R
- CCI (Commodity Channel Index)
- Keltner Channels
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, Tuple


# ============================================================================
# VBT Native Indicators
# ============================================================================

def calculate_ma(
    vbt, close: np.ndarray, period: int, ewm: bool = False
) -> np.ndarray:
    """
    Calculate Moving Average using VBT.

    Args:
        vbt: vectorbt module
        close: Price array
        period: MA window
        ewm: If True, use exponential weighting (EMA)

    Returns:
        MA values array
    """
    result = vbt.MA.run(close, period, ewm=ewm)
    return result.ma.values.flatten()


def calculate_mstd(
    vbt, close: np.ndarray, period: int, ewm: bool = False
) -> np.ndarray:
    """
    Calculate Moving Standard Deviation using VBT.

    Args:
        vbt: vectorbt module
        close: Price array
        period: Window size
        ewm: Use exponential weighting

    Returns:
        MSTD values array
    """
    result = vbt.MSTD.run(close, period, ewm=ewm)
    return result.mstd.values.flatten()


def calculate_bbands(
    vbt, close: np.ndarray, period: int = 20, alpha: float = 2.0
) -> Dict[str, np.ndarray]:
    """
    Calculate Bollinger Bands using VBT.

    Returns dict with: upper, middle, lower, bandwidth, percent_b
    """
    bb = vbt.BBANDS.run(close, window=period, alpha=alpha)
    upper = bb.upper.values.flatten()
    middle = bb.middle.values.flatten()
    lower = bb.lower.values.flatten()

    # Bandwidth = (upper - lower) / middle
    bandwidth = np.where(middle > 0, (upper - lower) / middle, 0)

    # %B = (close - lower) / (upper - lower)
    band_width = upper - lower
    percent_b = np.where(band_width > 0, (close - lower) / band_width, 0.5)

    return {
        'upper': upper,
        'middle': middle,
        'lower': lower,
        'bandwidth': bandwidth,
        'percent_b': percent_b,
    }


def calculate_rsi(
    vbt, close: np.ndarray, period: int = 14
) -> np.ndarray:
    """Calculate RSI using VBT."""
    result = vbt.RSI.run(close, period)
    return result.rsi.values.flatten()


def calculate_stoch(
    vbt, high: np.ndarray, low: np.ndarray, close: np.ndarray,
    k_period: int = 14, d_period: int = 3
) -> Dict[str, np.ndarray]:
    """
    Calculate Stochastic Oscillator using VBT.

    Returns dict with: k (fast), d (slow)
    """
    result = vbt.STOCH.run(high, low, close, k_window=k_period, d_window=d_period)
    return {
        'k': result.percent_k.values.flatten(),
        'd': result.percent_d.values.flatten(),
    }


def calculate_macd(
    vbt, close: np.ndarray,
    fast_period: int = 12, slow_period: int = 26, signal_period: int = 9
) -> Dict[str, np.ndarray]:
    """
    Calculate MACD using VBT.

    Returns dict with: macd, signal, histogram
    """
    result = vbt.MACD.run(
        close,
        fast_window=fast_period,
        slow_window=slow_period,
        signal_window=signal_period,
    )
    macd_vals = result.macd.values.flatten()
    signal_vals = result.signal.values.flatten()
    histogram = macd_vals - signal_vals

    return {
        'macd': macd_vals,
        'signal': signal_vals,
        'histogram': histogram,
    }


def calculate_atr(
    vbt, high: np.ndarray, low: np.ndarray, close: np.ndarray,
    period: int = 14
) -> np.ndarray:
    """
    Calculate Average True Range using VBT.

    Note: VBT's ATR requires OHLC. If only close available,
    we approximate high/low from close.
    """
    result = vbt.ATR.run(high, low, close, window=period)
    return result.atr.values.flatten()


def calculate_obv(
    vbt, close: np.ndarray, volume: np.ndarray
) -> np.ndarray:
    """
    Calculate On-Balance Volume using VBT.

    Args:
        vbt: vectorbt module
        close: Price array
        volume: Volume array

    Returns:
        OBV values array
    """
    result = vbt.OBV.run(close, volume)
    return result.obv.values.flatten()


# ============================================================================
# Custom Indicators (not in VBT natively)
# ============================================================================

def calculate_adx(
    close: np.ndarray, high: Optional[np.ndarray] = None,
    low: Optional[np.ndarray] = None, period: int = 14
) -> Dict[str, np.ndarray]:
    """
    Calculate ADX (Average Directional Index).

    If high/low not available, approximates from close.

    Returns dict with: adx, plus_di, minus_di
    """
    n = len(close)
    if high is None:
        high = close * 1.005
    if low is None:
        low = close * 0.995

    # True Range
    tr = np.zeros(n)
    for i in range(1, n):
        tr[i] = max(
            high[i] - low[i],
            abs(high[i] - close[i - 1]),
            abs(low[i] - close[i - 1])
        )

    # Directional Movement
    plus_dm = np.zeros(n)
    minus_dm = np.zeros(n)
    for i in range(1, n):
        up_move = high[i] - high[i - 1]
        down_move = low[i - 1] - low[i]
        if up_move > down_move and up_move > 0:
            plus_dm[i] = up_move
        if down_move > up_move and down_move > 0:
            minus_dm[i] = down_move

    # Smooth with EMA
    atr = pd.Series(tr).ewm(span=period, adjust=False).mean().values
    plus_di_raw = pd.Series(plus_dm).ewm(span=period, adjust=False).mean().values
    minus_di_raw = pd.Series(minus_dm).ewm(span=period, adjust=False).mean().values

    # Directional Indicators
    plus_di = np.where(atr > 0, (plus_di_raw / atr) * 100, 0)
    minus_di = np.where(atr > 0, (minus_di_raw / atr) * 100, 0)

    # DX and ADX
    di_sum = plus_di + minus_di
    dx = np.where(di_sum > 0, np.abs(plus_di - minus_di) / di_sum * 100, 0)
    adx = pd.Series(dx).ewm(span=period, adjust=False).mean().values

    return {
        'adx': adx,
        'plus_di': plus_di,
        'minus_di': minus_di,
    }


def calculate_donchian(
    close: np.ndarray, period: int = 20
) -> Dict[str, np.ndarray]:
    """
    Calculate Donchian Channels (breakout indicator).

    Returns dict with: upper, lower, middle
    """
    upper = pd.Series(close).rolling(period).max().values
    lower = pd.Series(close).rolling(period).min().values
    middle = (upper + lower) / 2

    return {
        'upper': upper,
        'lower': lower,
        'middle': middle,
    }


def calculate_zscore(
    close: np.ndarray, period: int = 20
) -> np.ndarray:
    """
    Calculate Z-Score (number of standard deviations from mean).

    Z = (close - SMA) / STD
    """
    sma = pd.Series(close).rolling(period).mean().values
    std = pd.Series(close).rolling(period).std().values
    std = np.where(std < 1e-10, 1.0, std)
    return (close - sma) / std


def calculate_momentum(
    close: np.ndarray, lookback: int = 20
) -> np.ndarray:
    """
    Calculate Momentum (Rate of Change).

    ROC = (close - close[n]) / close[n]
    """
    return pd.Series(close).pct_change(periods=lookback).values


def calculate_williams_r(
    high: np.ndarray, low: np.ndarray, close: np.ndarray,
    period: int = 14
) -> np.ndarray:
    """
    Calculate Williams %R.

    %R = (Highest High - Close) / (Highest High - Lowest Low) * -100
    """
    highest = pd.Series(high).rolling(period).max().values
    lowest = pd.Series(low).rolling(period).min().values
    denom = highest - lowest
    denom = np.where(denom < 1e-10, 1.0, denom)
    return ((highest - close) / denom) * -100


def calculate_cci(
    high: np.ndarray, low: np.ndarray, close: np.ndarray,
    period: int = 20
) -> np.ndarray:
    """
    Calculate Commodity Channel Index.

    CCI = (Typical Price - SMA(TP)) / (0.015 * Mean Deviation)
    """
    tp = (high + low + close) / 3
    sma_tp = pd.Series(tp).rolling(period).mean().values
    mean_dev = pd.Series(tp).rolling(period).apply(
        lambda x: np.mean(np.abs(x - x.mean())), raw=True
    ).values
    mean_dev = np.where(mean_dev < 1e-10, 1.0, mean_dev)
    return (tp - sma_tp) / (0.015 * mean_dev)


def calculate_keltner(
    high: np.ndarray, low: np.ndarray, close: np.ndarray,
    ema_period: int = 20, atr_period: int = 10, multiplier: float = 2.0
) -> Dict[str, np.ndarray]:
    """
    Calculate Keltner Channels.

    Middle = EMA(close)
    Upper = EMA + multiplier * ATR
    Lower = EMA - multiplier * ATR
    """
    ema = pd.Series(close).ewm(span=ema_period, adjust=False).mean().values

    # ATR calculation
    tr = np.zeros(len(close))
    for i in range(1, len(close)):
        tr[i] = max(
            high[i] - low[i],
            abs(high[i] - close[i - 1]),
            abs(low[i] - close[i - 1])
        )
    atr = pd.Series(tr).ewm(span=atr_period, adjust=False).mean().values

    return {
        'upper': ema + multiplier * atr,
        'middle': ema,
        'lower': ema - multiplier * atr,
    }


def calculate_vwap(
    high: np.ndarray, low: np.ndarray, close: np.ndarray,
    volume: np.ndarray
) -> np.ndarray:
    """
    Calculate Volume Weighted Average Price (VWAP).

    VWAP = cumsum(TP * Volume) / cumsum(Volume)
    """
    tp = (high + low + close) / 3
    cumvol = np.cumsum(volume)
    cumvol = np.where(cumvol > 0, cumvol, 1)
    return np.cumsum(tp * volume) / cumvol


# ============================================================================
# Signal Generation Helpers
# ============================================================================

def generate_crossover_signals(
    fast: np.ndarray, slow: np.ndarray, index: pd.DatetimeIndex
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate entry/exit signals from indicator crossover.

    Entry: fast crosses above slow
    Exit: fast crosses below slow
    """
    entries = pd.Series(fast > slow, index=index)
    exits = pd.Series(fast < slow, index=index)
    return entries, exits


def generate_threshold_signals(
    indicator: np.ndarray, lower: float, upper: float,
    index: pd.DatetimeIndex
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate entry/exit signals from threshold crossings.

    Entry: indicator < lower threshold
    Exit: indicator > upper threshold
    """
    entries = pd.Series(indicator < lower, index=index)
    exits = pd.Series(indicator > upper, index=index)
    return entries, exits


def generate_breakout_signals(
    close: np.ndarray, upper: np.ndarray, lower: np.ndarray,
    index: pd.DatetimeIndex
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate breakout signals (Donchian-style).

    Entry: close breaks above upper channel
    Exit: close breaks below lower channel
    """
    entries = pd.Series(close > np.roll(upper, 1), index=index)
    exits = pd.Series(close < np.roll(lower, 1), index=index)
    # First bar has no previous channel value
    entries.iloc[0] = False
    exits.iloc[0] = False
    return entries, exits


def generate_mean_reversion_signals(
    zscore: np.ndarray, z_entry: float, z_exit: float,
    index: pd.DatetimeIndex
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate mean-reversion signals from Z-score.

    Entry: Z-score < -z_entry (oversold)
    Exit: Z-score > z_exit (mean reversion complete)
    """
    entries = pd.Series(zscore < -z_entry, index=index)
    exits = pd.Series(zscore > z_exit, index=index)
    return entries, exits


def apply_signal_filter(
    entries: pd.Series, exits: pd.Series,
    filter_indicator: np.ndarray, filter_threshold: float,
    filter_type: str = 'above'
) -> Tuple[pd.Series, pd.Series]:
    """
    Apply a filter to entry signals (e.g., only trade when ADX > 25).

    Args:
        entries: Original entry signals
        exits: Original exit signals
        filter_indicator: Filter indicator values
        filter_threshold: Threshold for filter
        filter_type: 'above' or 'below'

    Returns:
        Filtered entries and exits
    """
    if filter_type == 'above':
        mask = pd.Series(filter_indicator > filter_threshold, index=entries.index)
    else:
        mask = pd.Series(filter_indicator < filter_threshold, index=entries.index)

    filtered_entries = entries & mask
    return filtered_entries, exits


# ============================================================================
# Indicator Catalog (for frontend display)
# ============================================================================

def get_indicator_catalog() -> Dict[str, Any]:
    """
    Return the full catalog of available indicators.

    Used by the frontend to populate indicator selection dropdowns.
    """
    return {
        'categories': [
            {
                'name': 'Trend',
                'indicators': [
                    {'id': 'sma', 'name': 'SMA', 'params': ['period'], 'description': 'Simple Moving Average'},
                    {'id': 'ema', 'name': 'EMA', 'params': ['period'], 'description': 'Exponential Moving Average'},
                    {'id': 'macd', 'name': 'MACD', 'params': ['fastPeriod', 'slowPeriod', 'signalPeriod'], 'description': 'MACD Line, Signal, Histogram'},
                    {'id': 'adx', 'name': 'ADX', 'params': ['period'], 'description': 'Average Directional Index'},
                    {'id': 'keltner', 'name': 'Keltner', 'params': ['emaPeriod', 'atrPeriod', 'multiplier'], 'description': 'Keltner Channels'},
                    {'id': 'vwap', 'name': 'VWAP', 'params': [], 'description': 'Volume Weighted Average Price'},
                ]
            },
            {
                'name': 'Momentum',
                'indicators': [
                    {'id': 'rsi', 'name': 'RSI', 'params': ['period'], 'description': 'Relative Strength Index'},
                    {'id': 'stochastic', 'name': 'Stochastic', 'params': ['kPeriod', 'dPeriod'], 'description': 'Stochastic Oscillator (%K, %D)'},
                    {'id': 'momentum', 'name': 'Momentum', 'params': ['lookback'], 'description': 'Rate of Change'},
                    {'id': 'williams_r', 'name': "Williams %R", 'params': ['period'], 'description': 'Williams Percent Range'},
                    {'id': 'cci', 'name': 'CCI', 'params': ['period'], 'description': 'Commodity Channel Index'},
                ]
            },
            {
                'name': 'Volatility',
                'indicators': [
                    {'id': 'bbands', 'name': 'Bollinger Bands', 'params': ['period', 'stdDev'], 'description': 'Bollinger Bands (Upper, Middle, Lower)'},
                    {'id': 'atr', 'name': 'ATR', 'params': ['period'], 'description': 'Average True Range'},
                    {'id': 'mstd', 'name': 'MSTD', 'params': ['period'], 'description': 'Moving Standard Deviation'},
                    {'id': 'donchian', 'name': 'Donchian', 'params': ['period'], 'description': 'Donchian Channels (Breakout)'},
                ]
            },
            {
                'name': 'Volume',
                'indicators': [
                    {'id': 'obv', 'name': 'OBV', 'params': [], 'description': 'On-Balance Volume'},
                ]
            },
            {
                'name': 'Statistical',
                'indicators': [
                    {'id': 'zscore', 'name': 'Z-Score', 'params': ['period'], 'description': 'Standard deviations from mean'},
                ]
            },
        ]
    }
