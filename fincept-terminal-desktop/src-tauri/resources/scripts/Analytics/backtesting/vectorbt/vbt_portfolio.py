"""
VBT Portfolio Construction Module

Covers all VectorBT Portfolio creation methods:
- from_signals: Entry/exit boolean arrays
- from_orders: Direct order arrays (size, direction)
- from_order_func: Custom order function (event-driven)
- from_holding: Buy-and-hold benchmark
- from_random_signals: Random entry/exit for statistical benchmarking

Also handles:
- Stop-loss / Take-profit via VBT's built-in STX/OHLCSTX signal generators
- Position sizing (fixed, kelly, volatility-targeted, fractional)
- Multi-asset portfolio construction
- Short selling support
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, Tuple


def build_portfolio(
    vbt,
    close_series: pd.Series,
    entries: pd.Series,
    exits: pd.Series,
    initial_capital: float = 100000,
    request: Optional[Dict[str, Any]] = None,
) -> Any:
    """
    Build a VBT Portfolio from signals with full feature support.

    Handles:
    - Stop-loss / take-profit integration
    - Position sizing modes
    - Commission and slippage
    - Short selling
    - Frequency specification

    Args:
        vbt: vectorbt module reference
        close_series: Price series (pd.Series with DatetimeIndex)
        entries: Boolean entry signals
        exits: Boolean exit signals
        initial_capital: Starting capital
        request: Full request dict with advanced params

    Returns:
        vbt.Portfolio instance
    """
    request = request or {}

    # Extract advanced parameters
    commission = float(request.get('commission', 0.0))
    slippage = float(request.get('slippage', 0.0))
    freq = request.get('frequency', '1D')
    allow_short = request.get('allowShort', False)
    trade_on_close = request.get('tradeOnClose', True)

    # Position sizing
    position_sizing = request.get('positionSizing', {})
    size_mode = position_sizing.get('mode', 'fixed') if isinstance(position_sizing, dict) else 'fixed'

    # Stop-loss / Take-profit
    stop_loss = request.get('stopLoss', None)
    take_profit = request.get('takeProfit', None)
    trailing_stop = request.get('trailingStop', None)

    # Apply stop-loss/take-profit using VBT signal generators if available
    if stop_loss or take_profit or trailing_stop:
        entries, exits = _apply_exit_signals(
            vbt, close_series, entries, exits,
            stop_loss=stop_loss,
            take_profit=take_profit,
            trailing_stop=trailing_stop,
        )

    # Calculate position size array
    size = _calculate_position_size(
        close_series, initial_capital, size_mode, position_sizing, entries
    )

    # Build portfolio kwargs
    pf_kwargs = {
        'close': close_series,
        'entries': entries,
        'exits': exits,
        'init_cash': initial_capital,
        'freq': freq,
        'fees': commission,
        'slippage': slippage,
    }

    # Add size if not default
    if size is not None:
        pf_kwargs['size'] = size
        pf_kwargs['size_type'] = 'amount'

    # Short selling support
    if allow_short:
        # Use short_entries/short_exits for short positions
        short_entries = exits.copy()
        short_exits = entries.copy()
        pf_kwargs['short_entries'] = short_entries
        pf_kwargs['short_exits'] = short_exits

    try:
        portfolio = vbt.Portfolio.from_signals(**pf_kwargs)
    except TypeError:
        # Fallback for older VBT versions without some params
        basic_kwargs = {
            'close': close_series,
            'entries': entries,
            'exits': exits,
            'init_cash': initial_capital,
            'freq': freq,
        }
        if commission > 0:
            basic_kwargs['fees'] = commission
        portfolio = vbt.Portfolio.from_signals(**basic_kwargs)

    return portfolio


def build_portfolio_from_orders(
    vbt,
    close_series: pd.Series,
    order_sizes: pd.Series,
    initial_capital: float = 100000,
    request: Optional[Dict[str, Any]] = None,
) -> Any:
    """
    Build portfolio from direct order size arrays.

    Useful for strategies that specify exact position sizes per bar
    rather than entry/exit signals.

    Args:
        vbt: vectorbt module
        close_series: Price series
        order_sizes: Series of order sizes (positive=buy, negative=sell, 0=hold)
        initial_capital: Starting capital
        request: Additional parameters

    Returns:
        vbt.Portfolio instance
    """
    request = request or {}
    commission = float(request.get('commission', 0.0))
    freq = request.get('frequency', '1D')

    portfolio = vbt.Portfolio.from_orders(
        close=close_series,
        size=order_sizes,
        init_cash=initial_capital,
        fees=commission,
        freq=freq,
    )
    return portfolio


def build_holding_portfolio(
    vbt,
    close_series: pd.Series,
    initial_capital: float = 100000,
) -> Any:
    """
    Build a buy-and-hold portfolio for benchmark comparison.

    Args:
        vbt: vectorbt module
        close_series: Price series
        initial_capital: Starting capital

    Returns:
        vbt.Portfolio instance (buy-and-hold)
    """
    portfolio = vbt.Portfolio.from_holding(
        close=close_series,
        init_cash=initial_capital,
        freq='1D',
    )
    return portfolio


def build_random_portfolio(
    vbt,
    close_series: pd.Series,
    initial_capital: float = 100000,
    n_trials: int = 100,
    entry_prob: float = 0.05,
    exit_prob: float = 0.05,
    seed: Optional[int] = None,
) -> Any:
    """
    Build random signal portfolios for statistical benchmarking.

    Generates n_trials random entry/exit signals to compare strategy
    performance against random trading.

    Args:
        vbt: vectorbt module
        close_series: Price series
        initial_capital: Starting capital
        n_trials: Number of random portfolios to generate
        entry_prob: Probability of entry signal per bar
        exit_prob: Probability of exit signal per bar
        seed: Random seed for reproducibility

    Returns:
        vbt.Portfolio instance (multi-column with n_trials)
    """
    if seed is not None:
        np.random.seed(seed)

    n = len(close_series)

    # Generate random entries and exits
    rand_entries = pd.DataFrame(
        np.random.random((n, n_trials)) < entry_prob,
        index=close_series.index,
    )
    rand_exits = pd.DataFrame(
        np.random.random((n, n_trials)) < exit_prob,
        index=close_series.index,
    )

    # Broadcast close to match columns
    close_broadcast = pd.DataFrame(
        np.column_stack([close_series.values] * n_trials),
        index=close_series.index,
    )

    portfolio = vbt.Portfolio.from_signals(
        close=close_broadcast,
        entries=rand_entries,
        exits=rand_exits,
        init_cash=initial_capital,
        freq='1D',
    )
    return portfolio


def get_random_benchmark_stats(
    vbt,
    close_series: pd.Series,
    initial_capital: float = 100000,
    n_trials: int = 100,
    seed: int = 42,
) -> Dict[str, Any]:
    """
    Get statistical summary of random trading performance.

    Used to calculate p-value of strategy returns:
    "What % of random strategies would have done better?"

    Args:
        vbt: vectorbt module
        close_series: Price series
        initial_capital: Starting capital
        n_trials: Number of random trials
        seed: Random seed

    Returns:
        Dict with percentile distribution of random returns
    """
    pf = build_random_portfolio(
        vbt, close_series, initial_capital, n_trials, seed=seed
    )

    total_returns = pf.total_return()
    if hasattr(total_returns, 'values'):
        returns_arr = total_returns.values.astype(float)
    else:
        returns_arr = np.array([float(total_returns)])

    returns_arr = returns_arr[np.isfinite(returns_arr)]

    return {
        'mean': float(np.mean(returns_arr)),
        'median': float(np.median(returns_arr)),
        'std': float(np.std(returns_arr)),
        'p5': float(np.percentile(returns_arr, 5)),
        'p25': float(np.percentile(returns_arr, 25)),
        'p75': float(np.percentile(returns_arr, 75)),
        'p95': float(np.percentile(returns_arr, 95)),
        'min': float(np.min(returns_arr)),
        'max': float(np.max(returns_arr)),
        'nTrials': n_trials,
    }


# ============================================================================
# Internal Helpers
# ============================================================================

def _apply_exit_signals(
    vbt,
    close_series: pd.Series,
    entries: pd.Series,
    exits: pd.Series,
    stop_loss: Optional[float] = None,
    take_profit: Optional[float] = None,
    trailing_stop: Optional[float] = None,
) -> Tuple[pd.Series, pd.Series]:
    """
    Apply stop-loss/take-profit using VBT's OHLCSTX or basic price checks.

    VBT provides:
    - vbt.OHLCSTX: Stop/take-profit with OHLC data
    - vbt.STX: Stop/take-profit with close-only data

    Falls back to manual calculation if generators unavailable.
    """
    try:
        # Try VBT's built-in stop signal generators
        if hasattr(vbt, 'OHLCSTX'):
            # Full OHLC stop generator (most accurate)
            stx = vbt.OHLCSTX.run(
                entries,
                close_series,  # open (approximation)
                close_series * 1.005,  # high (approximation)
                close_series * 0.995,  # low (approximation)
                close_series,  # close
                sl_stop=stop_loss / 100 if stop_loss else np.nan,
                tp_stop=take_profit / 100 if take_profit else np.nan,
                ts_stop=trailing_stop / 100 if trailing_stop else np.nan,
            )
            new_exits = stx.exits
            if isinstance(new_exits, pd.DataFrame):
                new_exits = new_exits.iloc[:, 0]
            # Combine with original exits
            combined_exits = exits | new_exits.astype(bool)
            return entries, combined_exits

        elif hasattr(vbt, 'STX'):
            # Close-only stop generator
            stx = vbt.STX.run(
                entries,
                close_series,
                sl_stop=stop_loss / 100 if stop_loss else np.nan,
                tp_stop=take_profit / 100 if take_profit else np.nan,
            )
            new_exits = stx.exits
            if isinstance(new_exits, pd.DataFrame):
                new_exits = new_exits.iloc[:, 0]
            combined_exits = exits | new_exits.astype(bool)
            return entries, combined_exits

    except Exception:
        pass

    # Manual fallback: calculate stops from entry prices
    if stop_loss or take_profit or trailing_stop:
        new_exits = _manual_stops(
            close_series, entries, stop_loss, take_profit, trailing_stop
        )
        combined_exits = exits | new_exits
        return entries, combined_exits

    return entries, exits


def _manual_stops(
    close_series: pd.Series,
    entries: pd.Series,
    stop_loss: Optional[float],
    take_profit: Optional[float],
    trailing_stop: Optional[float],
) -> pd.Series:
    """Calculate stop-loss/take-profit exits manually."""
    n = len(close_series)
    close_vals = close_series.values.astype(float)
    entry_mask = entries.values.astype(bool)
    exit_mask = np.zeros(n, dtype=bool)

    in_position = False
    entry_price = 0.0
    peak_price = 0.0

    for i in range(n):
        if entry_mask[i] and not in_position:
            in_position = True
            entry_price = close_vals[i]
            peak_price = close_vals[i]

        if in_position:
            peak_price = max(peak_price, close_vals[i])

            # Stop-loss check
            if stop_loss and close_vals[i] <= entry_price * (1 - stop_loss / 100):
                exit_mask[i] = True
                in_position = False
                continue

            # Take-profit check
            if take_profit and close_vals[i] >= entry_price * (1 + take_profit / 100):
                exit_mask[i] = True
                in_position = False
                continue

            # Trailing stop check
            if trailing_stop and close_vals[i] <= peak_price * (1 - trailing_stop / 100):
                exit_mask[i] = True
                in_position = False
                continue

    return pd.Series(exit_mask, index=close_series.index)


def _calculate_position_size(
    close_series: pd.Series,
    initial_capital: float,
    mode: str,
    params: Dict[str, Any],
    entries: pd.Series,
) -> Optional[pd.Series]:
    """
    Calculate position sizes based on sizing mode.

    Modes:
    - fixed: Fixed percentage of capital per trade
    - kelly: Kelly criterion (requires win_rate, avg_win, avg_loss)
    - volatility: Volatility-targeted (ATR-based)
    - fractional: Fixed fractional (risk % per trade)
    - equal: Equal weight across assets

    Returns None for default (100% of available capital).
    """
    if not isinstance(params, dict):
        return None

    if mode == 'fixed':
        pct = float(params.get('percentage', 100)) / 100.0
        if pct >= 1.0:
            return None  # Default behavior
        # Fixed fraction of capital
        shares = (initial_capital * pct) / close_series
        return shares.where(entries, 0)

    elif mode == 'kelly':
        win_rate = float(params.get('winRate', 0.5))
        avg_win = float(params.get('avgWin', 0.02))
        avg_loss = float(params.get('avgLoss', 0.01))
        if avg_loss > 0:
            kelly_f = win_rate - (1 - win_rate) / (avg_win / avg_loss)
        else:
            kelly_f = 0.5
        kelly_f = max(0.0, min(kelly_f, 1.0))  # Clamp 0-1
        shares = (initial_capital * kelly_f) / close_series
        return shares.where(entries, 0)

    elif mode == 'volatility':
        target_vol = float(params.get('targetVol', 0.15))  # 15% annual vol target
        lookback = int(params.get('lookback', 20))
        daily_returns = close_series.pct_change()
        rolling_vol = daily_returns.rolling(lookback).std() * np.sqrt(252)
        rolling_vol = rolling_vol.replace(0, np.nan).fillna(target_vol)
        vol_scalar = target_vol / rolling_vol
        vol_scalar = vol_scalar.clip(0.1, 3.0)  # Clamp leverage
        shares = (initial_capital * vol_scalar) / close_series
        return shares.where(entries, 0)

    elif mode == 'fractional':
        risk_pct = float(params.get('riskPercent', 1.0)) / 100.0
        # Risk a fixed % of capital per trade
        shares = (initial_capital * risk_pct) / close_series
        return shares.where(entries, 0)

    return None
