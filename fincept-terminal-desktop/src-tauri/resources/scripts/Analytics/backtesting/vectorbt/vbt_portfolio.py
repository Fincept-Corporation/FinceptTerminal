"""
VBT Portfolio Construction Module

Pure numpy/pandas portfolio simulator that produces objects compatible
with the metrics module's expected interface (value(), total_return(),
stats(), trades, final_value(), total_fees(), close).

Covers:
- from_signals: Entry/exit boolean arrays
- from_orders: Direct order arrays (size, direction)
- from_holding: Buy-and-hold benchmark
- from_random_signals: Random entry/exit for statistical benchmarking

Also handles:
- Stop-loss / Take-profit
- Position sizing (fixed, kelly, volatility-targeted, fractional)
- Multi-asset portfolio construction
- Short selling support
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, Tuple
from dataclasses import dataclass, field


# ============================================================================
# Simple Portfolio Object (compatible with vbt_metrics expectations)
# ============================================================================

class _TradesAccessor:
    """Mimics vbt.Portfolio.trades for metrics extraction."""

    def __init__(self, records_df: pd.DataFrame):
        self._records = records_df

    @property
    def records_readable(self) -> pd.DataFrame:
        return self._records

    @property
    def records(self):
        return self._records


class SimplePortfolio:
    """
    Lightweight portfolio object that exposes the same interface
    consumed by vbt_metrics.py and vectorbt_provider.py.
    """

    def __init__(
        self,
        equity_series: pd.Series,
        close_series: pd.Series,
        trade_records: pd.DataFrame,
        init_cash: float,
        total_fees_value: float = 0.0,
        freq: str = '1D',
    ):
        self._equity = equity_series
        self._close = close_series
        self._trade_records = trade_records
        self._init_cash = init_cash
        self._total_fees = total_fees_value
        self._freq = freq
        self._trades_accessor = _TradesAccessor(trade_records)
        self._stats_cache = None

    # --- Public interface expected by metrics / provider ---

    def value(self) -> pd.Series:
        return self._equity

    @property
    def close(self) -> pd.Series:
        return self._close

    def total_return(self) -> float:
        if len(self._equity) < 2 or self._init_cash == 0:
            return 0.0
        return float(self._equity.iloc[-1] / self._init_cash - 1.0)

    def final_value(self) -> float:
        return float(self._equity.iloc[-1]) if len(self._equity) > 0 else self._init_cash

    def total_fees(self) -> float:
        return self._total_fees

    @property
    def trades(self):
        return self._trades_accessor

    def stats(self) -> Dict[str, Any]:
        """Return a dict mimicking vbt.Portfolio.stats() keys."""
        if self._stats_cache is not None:
            return self._stats_cache

        equity = self._equity.values.astype(float)
        n = len(equity)
        s = {}

        # Total return
        total_ret = self.total_return()
        s['Total Return [%]'] = total_ret * 100

        # Daily returns
        if n > 1:
            daily_ret = np.diff(equity) / np.where(equity[:-1] != 0, equity[:-1], 1.0)
            daily_ret = daily_ret[np.isfinite(daily_ret)]
        else:
            daily_ret = np.array([])

        # Annualized return
        years = n / 252.0
        if years > 0 and self._init_cash > 0 and equity[-1] > 0:
            ann_ret = (equity[-1] / self._init_cash) ** (1 / years) - 1
        else:
            ann_ret = 0.0
        s['Annualized Return [%]'] = ann_ret * 100

        # Volatility
        if len(daily_ret) > 1:
            daily_std = float(np.std(daily_ret, ddof=1))
            ann_vol = daily_std * np.sqrt(252)
        else:
            daily_std = 0.0
            ann_vol = 0.0
        s['Annualized Volatility [%]'] = ann_vol * 100

        # Sharpe
        if ann_vol > 1e-10:
            s['Sharpe Ratio'] = ann_ret / ann_vol
        else:
            s['Sharpe Ratio'] = 0.0

        # Sortino
        if len(daily_ret) > 1:
            neg = daily_ret[daily_ret < 0]
            downside_std = float(np.std(neg, ddof=1)) * np.sqrt(252) if len(neg) > 1 else 0.0
        else:
            downside_std = 0.0
        s['Sortino Ratio'] = ann_ret / downside_std if downside_std > 1e-10 else 0.0

        # Max drawdown
        peak = np.maximum.accumulate(equity)
        dd = (equity - peak) / np.where(peak > 0, peak, 1.0)
        max_dd = float(np.min(dd)) if len(dd) > 0 else 0.0
        s['Max Drawdown [%]'] = max_dd * 100

        # Calmar
        s['Calmar Ratio'] = ann_ret / abs(max_dd) if abs(max_dd) > 1e-10 else 0.0

        # Trade stats
        records = self._trade_records
        n_trades = len(records)
        s['Total Trades'] = n_trades

        if n_trades > 0 and 'PnL' in records.columns:
            pnl = records['PnL'].values.astype(float)
            pnl = pnl[np.isfinite(pnl)]
            winners = pnl[pnl > 0]
            losers = pnl[pnl <= 0]
            s['Win Rate [%]'] = float(len(winners) / len(pnl) * 100) if len(pnl) > 0 else 0.0
            total_wins = float(np.sum(winners)) if len(winners) > 0 else 0.0
            total_losses = float(np.sum(np.abs(losers))) if len(losers) > 0 else 0.0
            s['Profit Factor'] = total_wins / total_losses if total_losses > 0 else 0.0
        else:
            s['Win Rate [%]'] = 0.0
            s['Profit Factor'] = 0.0

        self._stats_cache = s
        return s


# ============================================================================
# Portfolio Builders
# ============================================================================

def build_portfolio(
    vbt,
    close_series: pd.Series,
    entries: pd.Series,
    exits: pd.Series,
    initial_capital: float = 100000,
    request: Optional[Dict[str, Any]] = None,
) -> SimplePortfolio:
    """
    Build a portfolio from entry/exit signals.

    Args:
        vbt: vectorbt module reference (unused, kept for API compat)
        close_series: Price series (pd.Series with DatetimeIndex)
        entries: Boolean entry signals
        exits: Boolean exit signals
        initial_capital: Starting capital
        request: Full request dict with advanced params

    Returns:
        SimplePortfolio instance
    """
    request = request or {}

    commission = float(request.get('commission', 0.0))
    slippage = float(request.get('slippage', 0.0))
    allow_short = request.get('allowShort', False)

    # Position sizing - handle both string ('fixed') and dict ({'mode': 'fixed', ...}) formats
    position_sizing_raw = request.get('positionSizing', 'fixed')
    if isinstance(position_sizing_raw, str):
        size_mode = position_sizing_raw
        position_sizing_params = {
            'mode': position_sizing_raw,
            'percentage': float(request.get('positionSizeValue', 1.0)) * 100,
        }
    elif isinstance(position_sizing_raw, dict):
        size_mode = position_sizing_raw.get('mode', 'fixed')
        position_sizing_params = position_sizing_raw
    else:
        size_mode = 'fixed'
        position_sizing_params = {}

    # Stop-loss / Take-profit (filter out zero values which mean "disabled")
    stop_loss = request.get('stopLoss', None)
    take_profit = request.get('takeProfit', None)
    trailing_stop = request.get('trailingStop', None)
    if stop_loss is not None and float(stop_loss) == 0:
        stop_loss = None
    if take_profit is not None and float(take_profit) == 0:
        take_profit = None
    if trailing_stop is not None and float(trailing_stop) == 0:
        trailing_stop = None

    # Apply stop-loss/take-profit
    if stop_loss or take_profit or trailing_stop:
        entries, exits = _apply_exit_signals(
            close_series, entries, exits,
            stop_loss=stop_loss,
            take_profit=take_profit,
            trailing_stop=trailing_stop,
        )

    # Calculate position sizes
    size_series = _calculate_position_size(
        close_series, initial_capital, size_mode, position_sizing_params, entries
    )

    # Simulate
    equity, trade_records, total_fees = _simulate_signals(
        close_series, entries, exits,
        initial_capital=initial_capital,
        commission=commission,
        slippage=slippage,
        size_series=size_series,
        allow_short=allow_short,
    )

    return SimplePortfolio(
        equity_series=equity,
        close_series=close_series,
        trade_records=trade_records,
        init_cash=initial_capital,
        total_fees_value=total_fees,
    )


def build_portfolio_from_orders(
    vbt,
    close_series: pd.Series,
    order_sizes: pd.Series,
    initial_capital: float = 100000,
    request: Optional[Dict[str, Any]] = None,
) -> SimplePortfolio:
    """Build portfolio from direct order size arrays."""
    request = request or {}
    commission = float(request.get('commission', 0.0))

    close_vals = close_series.values.astype(float)
    sizes = order_sizes.values.astype(float)
    n = len(close_vals)

    cash = initial_capital
    position = 0.0
    equity_vals = np.zeros(n)
    total_fees = 0.0
    trades = []

    entry_price = 0.0
    entry_idx = 0

    for i in range(n):
        order = sizes[i]
        if order != 0:
            cost = abs(order) * close_vals[i]
            fee = cost * commission
            total_fees += fee

            if order > 0 and position <= 0:
                # Opening or flipping long
                if position < 0:
                    # Close short
                    pnl = (entry_price - close_vals[i]) * abs(position) - fee
                    trades.append(_make_trade_record(
                        entry_idx, i, entry_price, close_vals[i], abs(position), pnl, fee
                    ))
                    cash += pnl
                entry_price = close_vals[i]
                entry_idx = i
                position = order
                cash -= fee
            elif order < 0 and position >= 0:
                # Closing long or opening short
                if position > 0:
                    pnl = (close_vals[i] - entry_price) * position - fee
                    trades.append(_make_trade_record(
                        entry_idx, i, entry_price, close_vals[i], position, pnl, fee
                    ))
                    cash += pnl + entry_price * position
                entry_price = close_vals[i]
                entry_idx = i
                position = order
                cash -= fee

        equity_vals[i] = cash + position * close_vals[i]

    equity = pd.Series(equity_vals, index=close_series.index, name='Equity')
    trade_df = pd.DataFrame(trades) if trades else _empty_trade_df()

    return SimplePortfolio(
        equity_series=equity,
        close_series=close_series,
        trade_records=trade_df,
        init_cash=initial_capital,
        total_fees_value=total_fees,
    )


def build_holding_portfolio(
    vbt,
    close_series: pd.Series,
    initial_capital: float = 100000,
) -> SimplePortfolio:
    """Build a buy-and-hold portfolio for benchmark comparison."""
    close_vals = close_series.values.astype(float)
    if close_vals[0] > 0:
        shares = initial_capital / close_vals[0]
    else:
        shares = 0.0

    equity_vals = shares * close_vals
    equity = pd.Series(equity_vals, index=close_series.index, name='Equity')

    # Single trade record
    pnl = float(equity_vals[-1] - initial_capital)
    trade_df = pd.DataFrame([_make_trade_record(
        0, len(close_vals) - 1, close_vals[0], close_vals[-1], shares, pnl, 0.0
    )])

    return SimplePortfolio(
        equity_series=equity,
        close_series=close_series,
        trade_records=trade_df,
        init_cash=initial_capital,
    )


def build_random_portfolio(
    vbt,
    close_series: pd.Series,
    initial_capital: float = 100000,
    n_trials: int = 100,
    entry_prob: float = 0.05,
    exit_prob: float = 0.05,
    seed: Optional[int] = None,
) -> SimplePortfolio:
    """
    Build random signal portfolios for statistical benchmarking.

    For simplicity, runs n_trials simulations and returns the one
    with the median total return. The get_random_benchmark_stats
    function below aggregates all trials.
    """
    if seed is not None:
        np.random.seed(seed)

    n = len(close_series)
    returns_list = []
    portfolios = []

    for _ in range(n_trials):
        rand_entries = pd.Series(np.random.random(n) < entry_prob, index=close_series.index)
        rand_exits = pd.Series(np.random.random(n) < exit_prob, index=close_series.index)

        equity, trade_records, total_fees = _simulate_signals(
            close_series, rand_entries, rand_exits,
            initial_capital=initial_capital,
        )
        ret = float(equity.iloc[-1] / initial_capital - 1.0) if initial_capital > 0 else 0.0
        returns_list.append(ret)
        portfolios.append((equity, trade_records, total_fees))

    # Return median portfolio
    median_idx = int(np.argsort(returns_list)[len(returns_list) // 2])
    eq, tr, tf = portfolios[median_idx]

    pf = SimplePortfolio(
        equity_series=eq,
        close_series=close_series,
        trade_records=tr,
        init_cash=initial_capital,
        total_fees_value=tf,
    )
    # Attach all returns for benchmark stats
    pf._all_random_returns = np.array(returns_list)
    return pf


def get_random_benchmark_stats(
    vbt,
    close_series: pd.Series,
    initial_capital: float = 100000,
    n_trials: int = 100,
    seed: int = 42,
) -> Dict[str, Any]:
    """
    Get statistical summary of random trading performance.

    Used to calculate p-value of strategy returns.
    """
    pf = build_random_portfolio(
        vbt, close_series, initial_capital, n_trials, seed=seed
    )

    returns_arr = getattr(pf, '_all_random_returns', np.array([pf.total_return()]))
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
# Core Simulation Engine
# ============================================================================

def _simulate_signals(
    close_series: pd.Series,
    entries: pd.Series,
    exits: pd.Series,
    initial_capital: float = 100000,
    commission: float = 0.0,
    slippage: float = 0.0,
    size_series: Optional[pd.Series] = None,
    allow_short: bool = False,
) -> Tuple[pd.Series, pd.DataFrame, float]:
    """
    Simulate a long-only (or long/short) portfolio from boolean signals.

    Returns:
        (equity_series, trade_records_df, total_fees)
    """
    close_vals = close_series.values.astype(float)
    entry_mask = entries.values.astype(bool)
    exit_mask = exits.values.astype(bool)
    n = len(close_vals)

    cash = initial_capital
    position = 0.0
    equity_vals = np.zeros(n)
    total_fees = 0.0
    trades = []

    entry_price = 0.0
    entry_idx = 0
    in_position = False

    for i in range(n):
        price = close_vals[i]

        # Apply slippage
        buy_price = price * (1 + slippage)
        sell_price = price * (1 - slippage)

        if in_position:
            # Check exit
            if exit_mask[i]:
                # Close position
                fee = abs(position) * sell_price * commission
                total_fees += fee
                pnl = (sell_price - entry_price) * position - fee
                duration = max(1, i - entry_idx)  # bars (integer)
                ret = (sell_price / entry_price - 1.0) if entry_price > 0 else 0.0

                trades.append(_make_trade_record(
                    entry_idx, i, entry_price, sell_price, position, pnl, fee,
                    duration=duration, ret=ret,
                ))
                cash += position * sell_price - fee
                position = 0.0
                in_position = False
        else:
            # Check entry
            if entry_mask[i]:
                if size_series is not None:
                    shares = float(size_series.iloc[i])
                    if shares <= 0:
                        shares = cash / buy_price if buy_price > 0 else 0
                else:
                    shares = cash / buy_price if buy_price > 0 else 0

                if shares > 0:
                    fee = shares * buy_price * commission
                    total_fees += fee
                    cost = shares * buy_price + fee
                    if cost <= cash:
                        cash -= cost
                        position = shares
                        entry_price = buy_price
                        entry_idx = i
                        in_position = True

        equity_vals[i] = cash + position * price

    # Close any open position at end
    if in_position and position > 0:
        final_price = close_vals[-1]
        fee = position * final_price * commission
        total_fees += fee
        pnl = (final_price - entry_price) * position - fee
        duration = max(1, n - 1 - entry_idx)  # bars (integer)
        ret = (final_price / entry_price - 1.0) if entry_price > 0 else 0.0

        trades.append(_make_trade_record(
            entry_idx, n - 1, entry_price, final_price, position, pnl, fee,
            duration=duration, ret=ret,
        ))
        cash += position * final_price - fee
        equity_vals[-1] = cash

    equity = pd.Series(equity_vals, index=close_series.index, name='Equity')
    trade_df = pd.DataFrame(trades) if trades else _empty_trade_df()

    return equity, trade_df, total_fees


# ============================================================================
# Internal Helpers
# ============================================================================

def _make_trade_record(
    entry_idx: int,
    exit_idx: int,
    entry_price: float,
    exit_price: float,
    size: float,
    pnl: float,
    fees: float,
    duration=None,
    ret: float = 0.0,
) -> Dict[str, Any]:
    """Create a trade record dict matching vbt's records_readable format."""
    if duration is None:
        duration = max(1, exit_idx - entry_idx)  # integer: number of bars
    if ret == 0.0 and entry_price > 0:
        ret = (exit_price / entry_price - 1.0)
    return {
        'Entry Idx': entry_idx,
        'Exit Idx': exit_idx,
        'Entry Price': entry_price,
        'Exit Price': exit_price,
        'Size': size,
        'PnL': pnl,
        'Return': ret,
        'Entry Fees': fees / 2,
        'Exit Fees': fees / 2,
        'Duration': duration,
        'Status': 'Closed',
    }


def _empty_trade_df() -> pd.DataFrame:
    """Return an empty DataFrame with the expected columns."""
    return pd.DataFrame(columns=[
        'Entry Idx', 'Exit Idx', 'Entry Price', 'Exit Price',
        'Size', 'PnL', 'Return', 'Entry Fees', 'Exit Fees',
        'Duration', 'Status',
    ])


def _apply_exit_signals(
    close_series: pd.Series,
    entries: pd.Series,
    exits: pd.Series,
    stop_loss: Optional[float] = None,
    take_profit: Optional[float] = None,
    trailing_stop: Optional[float] = None,
) -> Tuple[pd.Series, pd.Series]:
    """Apply stop-loss/take-profit exits manually."""
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

    Returns None for default (100% of available capital).
    """
    if not isinstance(params, dict):
        return None  # Use default: 100% capital

    if mode == 'fixed':
        pct = float(params.get('percentage', 100))
        # percentage may already be 0-1 scale or 0-100 scale
        if pct > 1.0 and pct <= 100.0:
            pct = pct / 100.0
        elif pct <= 0:
            return None  # Default: 100% capital
        if pct >= 1.0:
            return None  # 100% capital = default behavior
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
        kelly_f = max(0.0, min(kelly_f, 1.0))
        shares = (initial_capital * kelly_f) / close_series
        return shares.where(entries, 0)

    elif mode == 'volatility':
        target_vol = float(params.get('targetVol', 0.15))
        lookback = int(params.get('lookback', 20))
        daily_returns = close_series.pct_change()
        rolling_vol = daily_returns.rolling(lookback).std() * np.sqrt(252)
        rolling_vol = rolling_vol.replace(0, np.nan).fillna(target_vol)
        vol_scalar = target_vol / rolling_vol
        vol_scalar = vol_scalar.clip(0.1, 3.0)
        shares = (initial_capital * vol_scalar) / close_series
        return shares.where(entries, 0)

    elif mode == 'fractional':
        risk_pct = float(params.get('riskPercent', 1.0)) / 100.0
        shares = (initial_capital * risk_pct) / close_series
        return shares.where(entries, 0)

    return None
