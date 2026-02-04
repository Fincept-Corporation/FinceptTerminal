"""
Backtesting.py Signals Module

Wraps backtesting.lib signal utilities: SignalStrategy, TrailingStrategy,
cross, crossover, barssince, quantile.
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional


def _get_lib():
    """Lazy import backtesting.lib"""
    from backtesting.lib import (
        crossover, cross, barssince, SignalStrategy, TrailingStrategy, quantile
    )
    return crossover, cross, barssince, SignalStrategy, TrailingStrategy, quantile


# ============================================================================
# Signal Utility Functions
# ============================================================================

def check_crossover(series1: pd.Series, series2: pd.Series) -> pd.Series:
    """True where series1 crosses above series2"""
    prev1 = series1.shift(1)
    prev2 = series2.shift(1)
    return (prev1 <= prev2) & (series1 > series2)


def check_crossunder(series1: pd.Series, series2: pd.Series) -> pd.Series:
    """True where series1 crosses below series2"""
    prev1 = series1.shift(1)
    prev2 = series2.shift(1)
    return (prev1 >= prev2) & (series1 < series2)


def check_cross(series1: pd.Series, series2: pd.Series) -> pd.Series:
    """True where series1 crosses series2 in either direction"""
    return check_crossover(series1, series2) | check_crossunder(series1, series2)


def bars_since(condition: pd.Series) -> pd.Series:
    """Number of bars since condition was last True"""
    result = pd.Series(np.nan, index=condition.index)
    count = np.nan
    for i in range(len(condition)):
        if condition.iloc[i]:
            count = 0
        elif not np.isnan(count):
            count += 1
        result.iloc[i] = count
    return result


def quantile_series(series: pd.Series, quantile_val: float = 0.5) -> float:
    """Return the quantile value of a series"""
    return float(series.quantile(quantile_val))


# ============================================================================
# Signal Generation from Indicators
# ============================================================================

def generate_crossover_signals(fast: pd.Series, slow: pd.Series) -> Dict[str, pd.Series]:
    """Generate entry/exit signals from two indicator crossovers"""
    entries = check_crossover(fast, slow)
    exits = check_crossunder(fast, slow)
    return {'entries': entries, 'exits': exits}


def generate_threshold_signals(indicator: pd.Series,
                               lower: float, upper: float) -> Dict[str, pd.Series]:
    """Generate signals from oscillator threshold crossings"""
    entries = check_crossunder(indicator, pd.Series(lower, index=indicator.index))
    exits = check_crossover(indicator, pd.Series(upper, index=indicator.index))
    return {'entries': entries, 'exits': exits}


def generate_breakout_signals(close: pd.Series,
                              upper: pd.Series,
                              lower: pd.Series) -> Dict[str, pd.Series]:
    """Generate signals from channel breakout"""
    entries = close > upper
    exits = close < lower
    return {'entries': entries, 'exits': exits}


def generate_mean_reversion_signals(zscore_series: pd.Series,
                                    z_entry: float = 2.0,
                                    z_exit: float = 0.0) -> Dict[str, pd.Series]:
    """Generate mean reversion signals from z-score"""
    entries = zscore_series < -z_entry
    exits = zscore_series > z_exit
    return {'entries': entries, 'exits': exits}


# ============================================================================
# SignalStrategy and TrailingStrategy Wrappers
# ============================================================================

def build_signal_strategy(entry_signal_func, exit_signal_func=None):
    """
    Build a backtesting.py SignalStrategy from signal functions.

    entry_signal_func: function(self) -> bool array
    exit_signal_func: function(self) -> bool array (optional)
    """
    try:
        _, _, _, SignalStrategy, _, _ = _get_lib()

        class CustomSignalStrategy(SignalStrategy):
            def init(self):
                super().init()
                entry_size = entry_signal_func(self)
                self.set_signal(entry_size=entry_size)

        return CustomSignalStrategy
    except ImportError:
        return None


def build_trailing_strategy(entry_signal_func, trailing_pct: float = 0.03):
    """
    Build a backtesting.py TrailingStrategy.

    entry_signal_func: function(self) -> bool array for entries
    trailing_pct: trailing stop percentage
    """
    try:
        _, _, _, _, TrailingStrategy, _ = _get_lib()

        class CustomTrailingStrategy(TrailingStrategy):
            _trailing_pct = trailing_pct

            def init(self):
                super().init()
                self.set_trailing_sl(self._trailing_pct)

            def next(self):
                super().next()

        return CustomTrailingStrategy
    except ImportError:
        return None


# ============================================================================
# Convert signals to backtest-ready format
# ============================================================================

def signals_to_dict(entries: pd.Series, exits: pd.Series,
                    index: pd.Index = None) -> List[Dict[str, Any]]:
    """Convert boolean signal series to list of signal dicts for frontend"""
    result = []
    idx = index if index is not None else entries.index
    for i, dt in enumerate(idx):
        if i < len(entries) and entries.iloc[i]:
            result.append({'date': str(dt), 'type': 'entry', 'direction': 'long'})
        if i < len(exits) and exits.iloc[i]:
            result.append({'date': str(dt), 'type': 'exit', 'direction': 'long'})
    return result
