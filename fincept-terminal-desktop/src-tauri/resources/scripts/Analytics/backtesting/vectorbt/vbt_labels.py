"""
VBT Labels Module

Label generators for supervised ML and strategy evaluation.
Covers all vectorbt label generators:

- FIXLB: Fixed-horizon forward return labels
- MEANLB: Mean-reversion labels (deviation from rolling mean)
- LEXLB: Local extrema labels (peaks and troughs)
- TRENDLB: Trend labels (up/down/sideways based on linear regression)
- BOLB: Bollinger Band labels (position relative to bands)
"""

import numpy as np
import pandas as pd
from typing import Optional, Tuple


def FIXLB(
    close: pd.Series,
    horizon: int = 5,
    threshold: float = 0.0,
) -> pd.Series:
    """
    Fixed-horizon forward return labels.

    Mimics vbt.FIXLB: computes forward return over `horizon` bars
    and labels as 1 (above threshold), -1 (below -threshold), 0 (neutral).

    Args:
        close: Price series
        horizon: Number of bars to look forward
        threshold: Min return to classify as positive/negative (0.01 = 1%)

    Returns:
        Label series: 1 (buy), -1 (sell), 0 (hold)
    """
    fwd_return = close.shift(-horizon) / close - 1
    labels = np.where(fwd_return > threshold, 1, np.where(fwd_return < -threshold, -1, 0))
    # Last `horizon` bars have no forward data
    labels[-horizon:] = 0
    return pd.Series(labels, index=close.index, name='FIXLB')


def MEANLB(
    close: pd.Series,
    window: int = 20,
    threshold: float = 1.0,
) -> pd.Series:
    """
    Mean-reversion labels.

    Mimics vbt.MEANLB: labels based on z-score distance from rolling mean.
    When price is far below mean -> buy label (1)
    When price is far above mean -> sell label (-1)

    Args:
        close: Price series
        window: Rolling window for mean/std calculation
        threshold: Z-score threshold for labeling

    Returns:
        Label series: 1 (oversold), -1 (overbought), 0 (neutral)
    """
    rolling_mean = close.rolling(window).mean()
    rolling_std = close.rolling(window).std()
    zscore = (close - rolling_mean) / rolling_std.replace(0, np.nan)
    zscore = zscore.fillna(0)

    labels = np.where(zscore < -threshold, 1, np.where(zscore > threshold, -1, 0))
    return pd.Series(labels, index=close.index, name='MEANLB')


def LEXLB(
    close: pd.Series,
    window: int = 5,
) -> pd.Series:
    """
    Local extrema labels.

    Mimics vbt.LEXLB: identifies local peaks (-1 = sell at peak)
    and local troughs (1 = buy at trough).

    Args:
        close: Price series
        window: Look-back/forward window to confirm extrema

    Returns:
        Label series: 1 (local min = buy), -1 (local max = sell), 0 (neither)
    """
    n = len(close)
    vals = close.values.astype(float)
    labels = np.zeros(n, dtype=int)

    for i in range(window, n - window):
        local_window = vals[i - window:i + window + 1]
        if vals[i] == np.max(local_window):
            labels[i] = -1  # Local peak -> sell
        elif vals[i] == np.min(local_window):
            labels[i] = 1  # Local trough -> buy

    return pd.Series(labels, index=close.index, name='LEXLB')


def TRENDLB(
    close: pd.Series,
    window: int = 20,
    threshold: float = 0.0,
    method: str = 'slope',
) -> pd.Series:
    """
    Trend labels based on linear regression slope.

    Mimics vbt.TRENDLB: fits rolling linear regression and labels
    based on slope direction.

    Args:
        close: Price series
        window: Rolling window for regression
        threshold: Minimum slope magnitude to label as trending
        method: 'slope' (linear reg slope) or 'pct' (percentage change)

    Returns:
        Label series: 1 (uptrend), -1 (downtrend), 0 (sideways)
    """
    n = len(close)
    vals = close.values.astype(float)
    labels = np.zeros(n, dtype=int)

    if method == 'slope':
        x = np.arange(window, dtype=float)
        x_mean = np.mean(x)
        x_var = np.sum((x - x_mean) ** 2)

        for i in range(window - 1, n):
            y = vals[i - window + 1:i + 1]
            y_mean = np.mean(y)
            if x_var > 0:
                slope = np.sum((x - x_mean) * (y - y_mean)) / x_var
                # Normalize slope by price level
                norm_slope = slope / y_mean if y_mean > 0 else 0
                if norm_slope > threshold:
                    labels[i] = 1
                elif norm_slope < -threshold:
                    labels[i] = -1
    else:
        # Percentage change method
        pct = close.pct_change(window).values
        for i in range(n):
            if np.isfinite(pct[i]):
                if pct[i] > threshold:
                    labels[i] = 1
                elif pct[i] < -threshold:
                    labels[i] = -1

    return pd.Series(labels, index=close.index, name='TRENDLB')


def BOLB(
    close: pd.Series,
    window: int = 20,
    alpha: float = 2.0,
) -> pd.Series:
    """
    Bollinger Band labels.

    Mimics vbt.BOLB: labels position relative to Bollinger Bands.

    Args:
        close: Price series
        window: BB period
        alpha: Number of standard deviations

    Returns:
        Label series:
            2: above upper band (strongly overbought)
            1: between middle and upper (mildly bullish)
            0: at middle band
           -1: between lower and middle (mildly bearish)
           -2: below lower band (strongly oversold)
    """
    sma = close.rolling(window).mean()
    std = close.rolling(window).std()
    upper = sma + alpha * std
    lower = sma - alpha * std

    labels = np.zeros(len(close), dtype=int)
    c = close.values
    u = upper.values
    l = lower.values
    m = sma.values

    for i in range(len(c)):
        if np.isnan(u[i]):
            continue
        if c[i] > u[i]:
            labels[i] = 2
        elif c[i] > m[i]:
            labels[i] = 1
        elif c[i] < l[i]:
            labels[i] = -2
        elif c[i] < m[i]:
            labels[i] = -1

    return pd.Series(labels, index=close.index, name='BOLB')


# ============================================================================
# Utility: Convert labels to signals
# ============================================================================

def labels_to_signals(
    labels: pd.Series,
    entry_label: int = 1,
    exit_label: int = -1,
) -> Tuple[pd.Series, pd.Series]:
    """
    Convert label series to entry/exit boolean signals.

    Args:
        labels: Label series (output of any label generator)
        entry_label: Label value that triggers entry
        exit_label: Label value that triggers exit

    Returns:
        (entries, exits)
    """
    entries = labels == entry_label
    exits = labels == exit_label
    return entries, exits
