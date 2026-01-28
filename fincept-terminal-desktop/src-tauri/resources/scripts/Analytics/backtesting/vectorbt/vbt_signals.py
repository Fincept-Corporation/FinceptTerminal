"""
VBT Signals Module

Covers all vectorbt signal generators and factory:

SignalFactory:
  - Custom signal generation from user-defined functions
  - Combinatorial signal generation

Random Generators:
  - RAND: Random entry signals (N entries)
  - RANDX: Random entry/exit pairs (N pairs)
  - RANDNX: Random N entries with exits
  - RPROB: Probability-based random entries
  - RPROBX: Probability-based entry/exit pairs
  - RPROBCX: Probability-based with cooldown
  - RPROBNX: Probability-based N entries with exits

Stop/Take-Profit Generators:
  - STX: Stop-loss/take-profit from close-only data
  - STCX: Stop with trailing from close-only
  - OHLCSTX: Stop-loss/take-profit from OHLC data
  - OHLCSTCX: OHLC stop with trailing
"""

import numpy as np
import pandas as pd
from typing import Optional, Callable, Dict, Any, Tuple, List


# ============================================================================
# Signal Factory
# ============================================================================

class SignalFactory:
    """
    Factory for creating custom signal generators.

    Mimics vectorbt's SignalFactory - allows defining custom entry/exit
    logic via callables and composing them.
    """

    def __init__(self, name: str = 'custom'):
        self._name = name
        self._entry_func = None
        self._exit_func = None
        self._choice_func = None

    @classmethod
    def from_choice_func(
        cls,
        entry_choice_func: Callable,
        exit_choice_func: Optional[Callable] = None,
        name: str = 'custom',
    ) -> 'SignalFactory':
        """
        Create signal factory from choice functions.

        entry_choice_func(close, **kwargs) -> np.ndarray[bool]
        exit_choice_func(close, entries, **kwargs) -> np.ndarray[bool]
        """
        factory = cls(name)
        factory._entry_func = entry_choice_func
        factory._exit_func = exit_choice_func
        return factory

    def run(
        self,
        close: pd.Series,
        **kwargs,
    ) -> Tuple[pd.Series, pd.Series]:
        """
        Run signal factory to generate entries and exits.

        Returns:
            (entries, exits) as pd.Series[bool]
        """
        idx = close.index

        if self._entry_func is not None:
            entries = self._entry_func(close.values, **kwargs)
            entries = np.asarray(entries, dtype=bool)
        else:
            entries = np.zeros(len(close), dtype=bool)

        if self._exit_func is not None:
            exits = self._exit_func(close.values, entries, **kwargs)
            exits = np.asarray(exits, dtype=bool)
        else:
            exits = np.zeros(len(close), dtype=bool)

        return pd.Series(entries, index=idx), pd.Series(exits, index=idx)

    @classmethod
    def from_apply_func(
        cls,
        apply_func: Callable,
        name: str = 'custom',
    ) -> 'SignalFactory':
        """
        Create from an apply function that returns (entries, exits).

        apply_func(close, **kwargs) -> Tuple[np.ndarray, np.ndarray]
        """
        factory = cls(name)

        def entry_func(close, **kwargs):
            entries, _ = apply_func(close, **kwargs)
            return entries

        def exit_func(close, entries, **kwargs):
            _, exits = apply_func(close, **kwargs)
            return exits

        factory._entry_func = entry_func
        factory._exit_func = exit_func
        return factory


# ============================================================================
# Random Signal Generators
# ============================================================================

def RAND(
    close: pd.Series,
    n: int = 10,
    seed: Optional[int] = None,
) -> pd.Series:
    """
    Generate N random entry signals.

    Mimics vbt.RAND: places exactly N entry signals at random positions.
    """
    if seed is not None:
        np.random.seed(seed)
    idx = close.index
    positions = np.random.choice(len(close), size=min(n, len(close)), replace=False)
    entries = np.zeros(len(close), dtype=bool)
    entries[positions] = True
    return pd.Series(entries, index=idx, name='RAND Entries')


def RANDX(
    close: pd.Series,
    n: int = 10,
    seed: Optional[int] = None,
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate N random entry/exit pairs.

    Each entry is followed by an exit before the next entry.
    """
    if seed is not None:
        np.random.seed(seed)
    length = len(close)
    idx = close.index
    entries = np.zeros(length, dtype=bool)
    exits = np.zeros(length, dtype=bool)

    # Generate non-overlapping entry-exit pairs
    available = list(range(length))
    np.random.shuffle(available)
    pairs_placed = 0
    i = 0

    sorted_positions = sorted(available[:min(n * 3, length)])
    in_trade = False
    entry_pos = -1

    for pos in sorted_positions:
        if pairs_placed >= n:
            break
        if not in_trade:
            entries[pos] = True
            entry_pos = pos
            in_trade = True
        else:
            if pos > entry_pos:
                exits[pos] = True
                in_trade = False
                pairs_placed += 1

    return pd.Series(entries, index=idx), pd.Series(exits, index=idx)


def RANDNX(
    close: pd.Series,
    n: int = 10,
    seed: Optional[int] = None,
    min_hold: int = 1,
    max_hold: int = 20,
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate N random entries with random hold durations.
    """
    if seed is not None:
        np.random.seed(seed)
    length = len(close)
    idx = close.index
    entries = np.zeros(length, dtype=bool)
    exits = np.zeros(length, dtype=bool)

    placed = 0
    i = 0
    while placed < n and i < length - min_hold:
        # Random entry point
        entry = np.random.randint(i, min(i + (length // n) + 1, length - min_hold))
        hold = np.random.randint(min_hold, min(max_hold + 1, length - entry))
        exit_pos = min(entry + hold, length - 1)

        entries[entry] = True
        exits[exit_pos] = True
        placed += 1
        i = exit_pos + 1

    return pd.Series(entries, index=idx), pd.Series(exits, index=idx)


def RPROB(
    close: pd.Series,
    entry_prob: float = 0.1,
    seed: Optional[int] = None,
) -> pd.Series:
    """
    Generate probability-based random entry signals.

    Each bar has entry_prob chance of being an entry.
    """
    if seed is not None:
        np.random.seed(seed)
    entries = np.random.random(len(close)) < entry_prob
    return pd.Series(entries, index=close.index, name='RPROB Entries')


def RPROBX(
    close: pd.Series,
    entry_prob: float = 0.1,
    exit_prob: float = 0.1,
    seed: Optional[int] = None,
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate probability-based entry/exit pairs (non-overlapping).
    """
    if seed is not None:
        np.random.seed(seed)
    length = len(close)
    entries = np.zeros(length, dtype=bool)
    exits = np.zeros(length, dtype=bool)
    in_trade = False

    for i in range(length):
        if not in_trade:
            if np.random.random() < entry_prob:
                entries[i] = True
                in_trade = True
        else:
            if np.random.random() < exit_prob:
                exits[i] = True
                in_trade = False

    return pd.Series(entries, index=close.index), pd.Series(exits, index=close.index)


def RPROBCX(
    close: pd.Series,
    entry_prob: float = 0.1,
    exit_prob: float = 0.1,
    cooldown: int = 5,
    seed: Optional[int] = None,
) -> Tuple[pd.Series, pd.Series]:
    """
    Probability-based entry/exit with cooldown between trades.
    """
    if seed is not None:
        np.random.seed(seed)
    length = len(close)
    entries = np.zeros(length, dtype=bool)
    exits = np.zeros(length, dtype=bool)
    in_trade = False
    last_exit = -cooldown - 1

    for i in range(length):
        if not in_trade:
            if i - last_exit > cooldown and np.random.random() < entry_prob:
                entries[i] = True
                in_trade = True
        else:
            if np.random.random() < exit_prob:
                exits[i] = True
                in_trade = False
                last_exit = i

    return pd.Series(entries, index=close.index), pd.Series(exits, index=close.index)


def RPROBNX(
    close: pd.Series,
    n: int = 10,
    entry_prob: float = 0.1,
    exit_prob: float = 0.2,
    seed: Optional[int] = None,
) -> Tuple[pd.Series, pd.Series]:
    """
    Probability-based with max N trades.
    """
    if seed is not None:
        np.random.seed(seed)
    length = len(close)
    entries = np.zeros(length, dtype=bool)
    exits = np.zeros(length, dtype=bool)
    in_trade = False
    trade_count = 0

    for i in range(length):
        if trade_count >= n:
            break
        if not in_trade:
            if np.random.random() < entry_prob:
                entries[i] = True
                in_trade = True
        else:
            if np.random.random() < exit_prob:
                exits[i] = True
                in_trade = False
                trade_count += 1

    return pd.Series(entries, index=close.index), pd.Series(exits, index=close.index)


# ============================================================================
# Stop-Loss / Take-Profit Signal Generators
# ============================================================================

def STX(
    close: pd.Series,
    entries: pd.Series,
    stop_loss: Optional[float] = None,
    take_profit: Optional[float] = None,
) -> pd.Series:
    """
    Stop-loss / take-profit exit generator from close-only data.

    Mimics vbt.STX.

    Args:
        close: Price series
        entries: Entry signals
        stop_loss: Stop-loss as fraction (0.05 = 5%)
        take_profit: Take-profit as fraction (0.10 = 10%)

    Returns:
        Exit signals
    """
    close_vals = close.values.astype(float)
    entry_mask = entries.values.astype(bool)
    n = len(close_vals)
    exits = np.zeros(n, dtype=bool)

    in_trade = False
    entry_price = 0.0

    for i in range(n):
        if entry_mask[i] and not in_trade:
            in_trade = True
            entry_price = close_vals[i]

        if in_trade:
            if stop_loss is not None and close_vals[i] <= entry_price * (1 - stop_loss):
                exits[i] = True
                in_trade = False
                continue
            if take_profit is not None and close_vals[i] >= entry_price * (1 + take_profit):
                exits[i] = True
                in_trade = False

    return pd.Series(exits, index=close.index, name='STX Exits')


def STCX(
    close: pd.Series,
    entries: pd.Series,
    stop_loss: Optional[float] = None,
    take_profit: Optional[float] = None,
    trailing_stop: Optional[float] = None,
) -> pd.Series:
    """
    Stop with trailing from close-only data.

    Mimics vbt.STCX.

    Args:
        close: Price series
        entries: Entry signals
        stop_loss: Fixed stop-loss fraction
        take_profit: Take-profit fraction
        trailing_stop: Trailing stop fraction from peak
    """
    close_vals = close.values.astype(float)
    entry_mask = entries.values.astype(bool)
    n = len(close_vals)
    exits = np.zeros(n, dtype=bool)

    in_trade = False
    entry_price = 0.0
    peak_price = 0.0

    for i in range(n):
        if entry_mask[i] and not in_trade:
            in_trade = True
            entry_price = close_vals[i]
            peak_price = close_vals[i]

        if in_trade:
            peak_price = max(peak_price, close_vals[i])

            if stop_loss is not None and close_vals[i] <= entry_price * (1 - stop_loss):
                exits[i] = True
                in_trade = False
                continue
            if take_profit is not None and close_vals[i] >= entry_price * (1 + take_profit):
                exits[i] = True
                in_trade = False
                continue
            if trailing_stop is not None and close_vals[i] <= peak_price * (1 - trailing_stop):
                exits[i] = True
                in_trade = False

    return pd.Series(exits, index=close.index, name='STCX Exits')


def OHLCSTX(
    open_: pd.Series,
    high: pd.Series,
    low: pd.Series,
    close: pd.Series,
    entries: pd.Series,
    stop_loss: Optional[float] = None,
    take_profit: Optional[float] = None,
) -> Tuple[pd.Series, pd.Series]:
    """
    Stop-loss / take-profit from OHLC data.

    Mimics vbt.OHLCSTX. Uses high/low for more accurate stop detection
    (intra-bar stop triggers).

    Returns:
        (exits, stop_type) - exits and type ('sl', 'tp', or '')
    """
    close_vals = close.values.astype(float)
    high_vals = high.values.astype(float)
    low_vals = low.values.astype(float)
    entry_mask = entries.values.astype(bool)
    n = len(close_vals)
    exits = np.zeros(n, dtype=bool)
    stop_types = [''] * n

    in_trade = False
    entry_price = 0.0

    for i in range(n):
        if entry_mask[i] and not in_trade:
            in_trade = True
            entry_price = close_vals[i]

        if in_trade:
            # Check stop-loss on low
            if stop_loss is not None and low_vals[i] <= entry_price * (1 - stop_loss):
                exits[i] = True
                stop_types[i] = 'sl'
                in_trade = False
                continue
            # Check take-profit on high
            if take_profit is not None and high_vals[i] >= entry_price * (1 + take_profit):
                exits[i] = True
                stop_types[i] = 'tp'
                in_trade = False

    return (
        pd.Series(exits, index=close.index, name='OHLCSTX Exits'),
        pd.Series(stop_types, index=close.index, name='Stop Type'),
    )


def OHLCSTCX(
    open_: pd.Series,
    high: pd.Series,
    low: pd.Series,
    close: pd.Series,
    entries: pd.Series,
    stop_loss: Optional[float] = None,
    take_profit: Optional[float] = None,
    trailing_stop: Optional[float] = None,
) -> Tuple[pd.Series, pd.Series]:
    """
    OHLC stop with trailing stop support.

    Mimics vbt.OHLCSTCX.

    Returns:
        (exits, stop_type)
    """
    close_vals = close.values.astype(float)
    high_vals = high.values.astype(float)
    low_vals = low.values.astype(float)
    entry_mask = entries.values.astype(bool)
    n = len(close_vals)
    exits = np.zeros(n, dtype=bool)
    stop_types = [''] * n

    in_trade = False
    entry_price = 0.0
    peak_price = 0.0

    for i in range(n):
        if entry_mask[i] and not in_trade:
            in_trade = True
            entry_price = close_vals[i]
            peak_price = high_vals[i]

        if in_trade:
            peak_price = max(peak_price, high_vals[i])

            # Fixed stop-loss
            if stop_loss is not None and low_vals[i] <= entry_price * (1 - stop_loss):
                exits[i] = True
                stop_types[i] = 'sl'
                in_trade = False
                continue
            # Take-profit
            if take_profit is not None and high_vals[i] >= entry_price * (1 + take_profit):
                exits[i] = True
                stop_types[i] = 'tp'
                in_trade = False
                continue
            # Trailing stop
            if trailing_stop is not None and low_vals[i] <= peak_price * (1 - trailing_stop):
                exits[i] = True
                stop_types[i] = 'ts'
                in_trade = False

    return (
        pd.Series(exits, index=close.index, name='OHLCSTCX Exits'),
        pd.Series(stop_types, index=close.index, name='Stop Type'),
    )


# ============================================================================
# Utility: Clean overlapping signals
# ============================================================================

def clean_signals(
    entries: pd.Series,
    exits: pd.Series,
) -> Tuple[pd.Series, pd.Series]:
    """
    Remove overlapping signals: ensure alternating entry->exit->entry pattern.

    If an entry occurs while already in a position, it's removed.
    If an exit occurs while not in a position, it's removed.
    """
    n = len(entries)
    clean_e = np.zeros(n, dtype=bool)
    clean_x = np.zeros(n, dtype=bool)
    in_trade = False

    e = entries.values.astype(bool)
    x = exits.values.astype(bool)

    for i in range(n):
        if not in_trade and e[i]:
            clean_e[i] = True
            in_trade = True
        elif in_trade and x[i]:
            clean_x[i] = True
            in_trade = False

    return (
        pd.Series(clean_e, index=entries.index),
        pd.Series(clean_x, index=exits.index),
    )
