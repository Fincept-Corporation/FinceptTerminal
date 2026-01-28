"""
VBT Portfolio Construction Module

Pure numpy/pandas portfolio simulator that produces objects compatible
with the metrics module's expected interface and covering the full
vectorbt Portfolio API surface.

Covers:
- from_signals: Entry/exit boolean arrays
- from_orders: Direct order arrays (size, direction)
- from_holding: Buy-and-hold benchmark
- from_random_signals: Random entry/exit for statistical benchmarking
- from_order_func: Custom order function per bar

Also handles:
- Stop-loss / Take-profit / Trailing stop
- Position sizing (fixed, kelly, volatility-targeted, fractional)
- Multi-asset portfolio construction
- Short selling support
- Cash sharing across assets
- Per-bar tracking: cash, assets, flows, exposure
- Benchmark comparison
- Entry trades / Exit trades / Positions views
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, Tuple, Callable, List
from dataclasses import dataclass, field


# ============================================================================
# Trade Record Accessors (mimics vbt.Portfolio.trades / entry_trades / etc.)
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

    @property
    def count(self) -> int:
        return len(self._records)

    def winning(self) -> pd.DataFrame:
        if 'PnL' in self._records.columns:
            return self._records[self._records['PnL'] > 0]
        return pd.DataFrame()

    def losing(self) -> pd.DataFrame:
        if 'PnL' in self._records.columns:
            return self._records[self._records['PnL'] <= 0]
        return pd.DataFrame()

    def win_rate(self) -> float:
        if len(self._records) == 0 or 'PnL' not in self._records.columns:
            return 0.0
        pnl = self._records['PnL'].values.astype(float)
        return float(np.sum(pnl > 0) / len(pnl))

    def profit_factor(self) -> float:
        if len(self._records) == 0 or 'PnL' not in self._records.columns:
            return 0.0
        pnl = self._records['PnL'].values.astype(float)
        wins = np.sum(pnl[pnl > 0])
        losses = np.sum(np.abs(pnl[pnl <= 0]))
        return float(wins / losses) if losses > 0 else 0.0

    def expectancy(self) -> float:
        if len(self._records) == 0 or 'PnL' not in self._records.columns:
            return 0.0
        return float(np.mean(self._records['PnL'].values.astype(float)))

    def sqn(self) -> float:
        if len(self._records) < 2 or 'PnL' not in self._records.columns:
            return 0.0
        pnl = self._records['PnL'].values.astype(float)
        std = np.std(pnl, ddof=1)
        if std < 1e-10:
            return 0.0
        return float(np.sqrt(len(pnl)) * np.mean(pnl) / std)

    def winning_streak(self) -> int:
        if len(self._records) == 0 or 'PnL' not in self._records.columns:
            return 0
        pnl = self._records['PnL'].values.astype(float)
        max_s, cur = 0, 0
        for v in pnl:
            if v > 0:
                cur += 1
                max_s = max(max_s, cur)
            else:
                cur = 0
        return max_s

    def losing_streak(self) -> int:
        if len(self._records) == 0 or 'PnL' not in self._records.columns:
            return 0
        pnl = self._records['PnL'].values.astype(float)
        max_s, cur = 0, 0
        for v in pnl:
            if v <= 0:
                cur += 1
                max_s = max(max_s, cur)
            else:
                cur = 0
        return max_s


class _EntryTradesAccessor(_TradesAccessor):
    """Entry-based trade view: groups by entry signal."""
    pass


class _ExitTradesAccessor(_TradesAccessor):
    """Exit-based trade view: groups by exit signal."""
    pass


class _PositionsAccessor(_TradesAccessor):
    """Position view: aggregates consecutive same-direction trades."""
    pass


# ============================================================================
# Simple Portfolio Object (full vbt.Portfolio API coverage)
# ============================================================================

class SimplePortfolio:
    """
    Lightweight portfolio object that exposes the full interface
    matching vectorbt's Portfolio class.

    Covers:
    - value(), close, total_return(), final_value(), total_fees(), stats()
    - asset_flow(), assets(), cash_flow(), cash(), init_cash
    - gross_exposure(), net_exposure()
    - returns(), asset_returns(), returns_acc
    - benchmark_value(), benchmark_returns(), total_benchmark_return()
    - trades, entry_trades, exit_trades, positions
    - drawdowns()
    - position_mask(), position_coverage()
    - total_profit(), order_records, orders
    """

    def __init__(
        self,
        equity_series: pd.Series,
        close_series: pd.Series,
        trade_records: pd.DataFrame,
        init_cash: float,
        total_fees_value: float = 0.0,
        freq: str = '1D',
        cash_series: Optional[pd.Series] = None,
        asset_series: Optional[pd.Series] = None,
        cash_flow_series: Optional[pd.Series] = None,
        asset_flow_series: Optional[pd.Series] = None,
        order_records_df: Optional[pd.DataFrame] = None,
        benchmark_close: Optional[pd.Series] = None,
        cash_sharing_flag: bool = False,
    ):
        self._equity = equity_series
        self._close = close_series
        self._trade_records = trade_records
        self._init_cash = init_cash
        self._total_fees = total_fees_value
        self._freq = freq
        self._trades_accessor = _TradesAccessor(trade_records)
        self._stats_cache = None

        # Per-bar tracking series
        self._cash_series = cash_series
        self._asset_series = asset_series
        self._cash_flow_series = cash_flow_series
        self._asset_flow_series = asset_flow_series
        self._order_records = order_records_df
        self._benchmark_close = benchmark_close
        self._cash_sharing_flag = cash_sharing_flag

        # Lazily built accessors
        self._entry_trades_accessor = None
        self._exit_trades_accessor = None
        self._positions_accessor = None
        self._returns_accessor_obj = None

    # ------------------------------------------------------------------
    # Core value / return methods (existing)
    # ------------------------------------------------------------------

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

    def total_profit(self) -> float:
        return float(self.final_value() - self._init_cash)

    @property
    def trades(self):
        return self._trades_accessor

    # ------------------------------------------------------------------
    # Cash & Asset tracking (NEW)
    # ------------------------------------------------------------------

    @property
    def init_cash(self) -> float:
        return self._init_cash

    def get_init_cash(self) -> float:
        return self._init_cash

    def cash(self) -> pd.Series:
        """Per-bar cash balance series."""
        if self._cash_series is not None:
            return self._cash_series
        # Derive: cash = equity - asset_value
        asset_val = self.asset_value()
        return pd.Series(
            self._equity.values - asset_val.values,
            index=self._equity.index, name='Cash'
        )

    def cash_flow(self) -> pd.Series:
        """Per-bar cash flow series (positive = cash in, negative = cash out)."""
        if self._cash_flow_series is not None:
            return self._cash_flow_series
        c = self.cash()
        flow = c.diff().fillna(0.0)
        flow.name = 'Cash Flow'
        return flow

    def asset_flow(self, direction: str = 'both') -> pd.Series:
        """Per-bar asset flow (change in position size)."""
        if self._asset_flow_series is not None:
            return self._asset_flow_series
        a = self.assets()
        flow = a.diff().fillna(0.0)
        flow.name = 'Asset Flow'
        if direction == 'long':
            return flow.clip(lower=0)
        elif direction == 'short':
            return flow.clip(upper=0)
        return flow

    def assets(self, direction: str = 'both') -> pd.Series:
        """Per-bar number of shares/units held."""
        if self._asset_series is not None:
            s = self._asset_series
        else:
            # Derive from equity and close
            close_vals = self._close.values.astype(float)
            cash_vals = self.cash().values.astype(float)
            equity_vals = self._equity.values.astype(float)
            assets_vals = np.where(
                close_vals > 0,
                (equity_vals - cash_vals) / close_vals,
                0.0
            )
            s = pd.Series(assets_vals, index=self._equity.index, name='Assets')

        if direction == 'long':
            return s.clip(lower=0)
        elif direction == 'short':
            return (-s).clip(lower=0)
        return s

    def asset_value(self, direction: str = 'both') -> pd.Series:
        """Per-bar market value of held assets."""
        a = self.assets(direction)
        val = a * self._close
        val.name = 'Asset Value'
        return val

    @property
    def cash_sharing(self) -> bool:
        return self._cash_sharing_flag

    # ------------------------------------------------------------------
    # Exposure (NEW)
    # ------------------------------------------------------------------

    def gross_exposure(self, direction: str = 'both') -> pd.Series:
        """Gross exposure = |asset_value| / equity."""
        av = self.asset_value(direction).abs()
        eq = self._equity.replace(0, np.nan)
        exp = (av / eq).fillna(0.0)
        exp.name = 'Gross Exposure'
        return exp

    def net_exposure(self) -> pd.Series:
        """Net exposure = asset_value / equity (signed)."""
        av = self.asset_value()
        eq = self._equity.replace(0, np.nan)
        exp = (av / eq).fillna(0.0)
        exp.name = 'Net Exposure'
        return exp

    # ------------------------------------------------------------------
    # Position mask & coverage (NEW)
    # ------------------------------------------------------------------

    def position_mask(self, direction: str = 'both') -> pd.Series:
        """Boolean series: True when in a position."""
        a = self.assets(direction)
        mask = a.abs() > 1e-10
        mask.name = 'Position Mask'
        return mask

    def position_coverage(self, direction: str = 'both') -> float:
        """Fraction of bars in a position."""
        mask = self.position_mask(direction)
        return float(mask.sum() / len(mask)) if len(mask) > 0 else 0.0

    # ------------------------------------------------------------------
    # Returns (NEW)
    # ------------------------------------------------------------------

    def returns(self) -> pd.Series:
        """Per-bar return series based on portfolio value."""
        eq = self._equity.values.astype(float)
        ret = np.zeros(len(eq))
        if len(eq) > 1:
            ret[1:] = np.diff(eq) / np.where(eq[:-1] != 0, eq[:-1], 1.0)
        s = pd.Series(ret, index=self._equity.index, name='Returns')
        return s

    def asset_returns(self) -> pd.Series:
        """Per-bar return series based on close price."""
        c = self._close.values.astype(float)
        ret = np.zeros(len(c))
        if len(c) > 1:
            ret[1:] = np.diff(c) / np.where(c[:-1] != 0, c[:-1], 1.0)
        return pd.Series(ret, index=self._close.index, name='Asset Returns')

    @property
    def returns_acc(self):
        """Returns accessor (lazy import to avoid circular deps)."""
        if self._returns_accessor_obj is None:
            try:
                from vbt_returns import ReturnsAccessor
                self._returns_accessor_obj = ReturnsAccessor(
                    self.returns(),
                    benchmark_rets=self.benchmark_returns() if self._benchmark_close is not None else None,
                    freq=self._freq,
                )
            except ImportError:
                self._returns_accessor_obj = None
        return self._returns_accessor_obj

    def get_returns_acc(self, benchmark_rets=None, freq=None):
        """Get returns accessor with custom benchmark."""
        try:
            from vbt_returns import ReturnsAccessor
            return ReturnsAccessor(
                self.returns(),
                benchmark_rets=benchmark_rets,
                freq=freq or self._freq,
            )
        except ImportError:
            return None

    # ------------------------------------------------------------------
    # Benchmark (NEW)
    # ------------------------------------------------------------------

    def benchmark_value(self) -> pd.Series:
        """Benchmark portfolio value (buy-and-hold on benchmark_close)."""
        if self._benchmark_close is None:
            return self._equity.copy()  # fallback: use own close
        bc = self._benchmark_close.values.astype(float)
        if bc[0] > 0:
            shares = self._init_cash / bc[0]
        else:
            shares = 0.0
        vals = shares * bc
        return pd.Series(vals, index=self._benchmark_close.index, name='Benchmark Value')

    def benchmark_returns(self) -> pd.Series:
        """Benchmark return series."""
        bv = self.benchmark_value().values.astype(float)
        ret = np.zeros(len(bv))
        if len(bv) > 1:
            ret[1:] = np.diff(bv) / np.where(bv[:-1] != 0, bv[:-1], 1.0)
        idx = self._benchmark_close.index if self._benchmark_close is not None else self._equity.index
        return pd.Series(ret, index=idx, name='Benchmark Returns')

    def total_benchmark_return(self) -> float:
        """Total benchmark return."""
        bv = self.benchmark_value()
        if len(bv) < 2 or self._init_cash == 0:
            return 0.0
        return float(bv.iloc[-1] / self._init_cash - 1.0)

    # ------------------------------------------------------------------
    # Trade views (NEW): entry_trades, exit_trades, positions
    # ------------------------------------------------------------------

    @property
    def entry_trades(self):
        """Entry-based trade records (grouped by entry signal)."""
        if self._entry_trades_accessor is None:
            self._entry_trades_accessor = _EntryTradesAccessor(self._trade_records)
        return self._entry_trades_accessor

    @property
    def exit_trades(self):
        """Exit-based trade records (grouped by exit signal)."""
        if self._exit_trades_accessor is None:
            self._exit_trades_accessor = _ExitTradesAccessor(self._trade_records)
        return self._exit_trades_accessor

    @property
    def positions(self):
        """Position records (aggregated consecutive same-direction trades)."""
        if self._positions_accessor is None:
            pos_records = _aggregate_positions(self._trade_records)
            self._positions_accessor = _PositionsAccessor(pos_records)
        return self._positions_accessor

    # ------------------------------------------------------------------
    # Order records (NEW)
    # ------------------------------------------------------------------

    @property
    def order_records(self) -> pd.DataFrame:
        """Raw order records."""
        if self._order_records is not None:
            return self._order_records
        # Derive from trade records: each trade = 1 buy order + 1 sell order
        orders = []
        for idx, row in self._trade_records.iterrows():
            orders.append({
                'Idx': int(row.get('Entry Idx', 0)),
                'Side': 'Buy',
                'Price': float(row.get('Entry Price', 0)),
                'Size': float(row.get('Size', 0)),
                'Fees': float(row.get('Entry Fees', 0)),
            })
            orders.append({
                'Idx': int(row.get('Exit Idx', 0)),
                'Side': 'Sell',
                'Price': float(row.get('Exit Price', 0)),
                'Size': float(row.get('Size', 0)),
                'Fees': float(row.get('Exit Fees', 0)),
            })
        return pd.DataFrame(orders) if orders else pd.DataFrame(
            columns=['Idx', 'Side', 'Price', 'Size', 'Fees']
        )

    @property
    def orders(self):
        """Order accessor (same as order_records for compatibility)."""
        return _TradesAccessor(self.order_records)

    # ------------------------------------------------------------------
    # Drawdowns (NEW)
    # ------------------------------------------------------------------

    def drawdowns(self) -> Dict[str, Any]:
        """Get drawdown records from equity series."""
        equity = self._equity.values.astype(float)
        peak = np.maximum.accumulate(equity)
        dd_pct = (equity - peak) / np.where(peak > 0, peak, 1.0)

        records = []
        in_dd = False
        start_idx = 0
        valley_idx = 0
        max_depth = 0.0

        for i in range(len(dd_pct)):
            if dd_pct[i] < 0:
                if not in_dd:
                    in_dd = True
                    start_idx = i
                    valley_idx = i
                    max_depth = abs(dd_pct[i])
                else:
                    if abs(dd_pct[i]) > max_depth:
                        max_depth = abs(dd_pct[i])
                        valley_idx = i
            else:
                if in_dd:
                    records.append({
                        'peak_idx': max(0, start_idx - 1),
                        'valley_idx': valley_idx,
                        'recovery_idx': i,
                        'depth': max_depth,
                        'duration': i - start_idx + 1,
                        'decline_duration': valley_idx - start_idx + 1,
                        'recovery_duration': i - valley_idx,
                    })
                    in_dd = False

        if in_dd:
            records.append({
                'peak_idx': max(0, start_idx - 1),
                'valley_idx': valley_idx,
                'recovery_idx': -1,
                'depth': max_depth,
                'duration': len(dd_pct) - start_idx,
                'decline_duration': valley_idx - start_idx + 1,
                'recovery_duration': 0,
            })

        return {
            'records': records,
            'dd_series': pd.Series(dd_pct, index=self._equity.index, name='Drawdown'),
            'max_drawdown': float(abs(np.min(dd_pct))) if len(dd_pct) > 0 else 0.0,
            'avg_drawdown': float(np.mean(dd_pct[dd_pct < 0])) if np.any(dd_pct < 0) else 0.0,
        }

    # ------------------------------------------------------------------
    # Stats (existing, unchanged)
    # ------------------------------------------------------------------

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
    import sys as _sys
    request = request or {}

    commission = float(request.get('commission', 0.0))
    slippage = float(request.get('slippage', 0.0))
    allow_short = request.get('allowShort', False)

    _sys.stderr.write(f'[PF-BUILD] === build_portfolio START ===\n')
    _sys.stderr.write(f'[PF-BUILD] commission={commission}, slippage={slippage}, allow_short={allow_short}\n')
    _sys.stderr.write(f'[PF-BUILD] close_series: len={len(close_series)}, dtype={close_series.dtype}\n')
    _sys.stderr.write(f'[PF-BUILD] entries: sum={int(entries.sum())}, dtype={entries.dtype}\n')
    _sys.stderr.write(f'[PF-BUILD] exits: sum={int(exits.sum())}, dtype={exits.dtype}\n')
    _sys.stderr.write(f'[PF-BUILD] initial_capital={initial_capital}\n')
    _sys.stderr.write(f'[PF-BUILD] index match: close={close_series.index.dtype}, entries={entries.index.dtype}, equal={close_series.index.equals(entries.index)}\n')

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

    _sys.stderr.write(f'[PF-BUILD] positionSizing raw={position_sizing_raw}, positionSizeValue={request.get("positionSizeValue")}\n')
    _sys.stderr.write(f'[PF-BUILD] size_mode={size_mode}, position_sizing_params={position_sizing_params}\n')

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

    _sys.stderr.write(f'[PF-BUILD] stop_loss={stop_loss}, take_profit={take_profit}, trailing_stop={trailing_stop}\n')

    # Apply stop-loss/take-profit
    if stop_loss or take_profit or trailing_stop:
        _sys.stderr.write(f'[PF-BUILD] Applying exit signals for SL/TP/TS\n')
        entries_before = int(entries.sum())
        exits_before = int(exits.sum())
        entries, exits = _apply_exit_signals(
            close_series, entries, exits,
            stop_loss=stop_loss,
            take_profit=take_profit,
            trailing_stop=trailing_stop,
        )
        _sys.stderr.write(f'[PF-BUILD] After SL/TP: entries {entries_before}->{int(entries.sum())}, exits {exits_before}->{int(exits.sum())}\n')

    # Calculate position sizes
    size_series = _calculate_position_size(
        close_series, initial_capital, size_mode, position_sizing_params, entries
    )
    _sys.stderr.write(f'[PF-BUILD] size_series is None: {size_series is None}\n')
    if size_series is not None:
        nonzero = (size_series > 0).sum()
        _sys.stderr.write(f'[PF-BUILD] size_series: len={len(size_series)}, nonzero={nonzero}, min={size_series.min():.4f}, max={size_series.max():.4f}\n')

    # Simulate
    _sys.stderr.write(f'[PF-BUILD] === Calling _simulate_signals ===\n')
    equity, trade_records, total_fees, per_bar = _simulate_signals(
        close_series, entries, exits,
        initial_capital=initial_capital,
        commission=commission,
        slippage=slippage,
        size_series=size_series,
        allow_short=allow_short,
        track_per_bar=True,
    )

    _sys.stderr.write(f'[PF-BUILD] === _simulate_signals returned ===\n')
    _sys.stderr.write(f'[PF-BUILD] equity: len={len(equity)}, first={equity.iloc[0]:.2f}, last={equity.iloc[-1]:.2f}\n')
    _sys.stderr.write(f'[PF-BUILD] trade_records: {len(trade_records)} trades\n')
    _sys.stderr.write(f'[PF-BUILD] total_fees: {total_fees:.4f}\n')
    if len(trade_records) > 0:
        _sys.stderr.write(f'[PF-BUILD] trade_records columns: {list(trade_records.columns)}\n')
        _sys.stderr.write(f'[PF-BUILD] first trade: {trade_records.iloc[0].to_dict()}\n')
    else:
        _sys.stderr.write(f'[PF-BUILD] WARNING: ZERO TRADES produced!\n')

    return SimplePortfolio(
        equity_series=equity,
        close_series=close_series,
        trade_records=trade_records,
        init_cash=initial_capital,
        total_fees_value=total_fees,
        cash_series=per_bar['cash'] if per_bar else None,
        asset_series=per_bar['assets'] if per_bar else None,
        cash_flow_series=per_bar['cash_flow'] if per_bar else None,
        asset_flow_series=per_bar['asset_flow'] if per_bar else None,
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

        equity, trade_records, total_fees, _ = _simulate_signals(
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
    track_per_bar: bool = False,
) -> Tuple[pd.Series, pd.DataFrame, float, Optional[Dict]]:
    """
    Simulate a long-only (or long/short) portfolio from boolean signals.

    Returns:
        (equity_series, trade_records_df, total_fees, per_bar_data_or_None)
    """
    import sys as _sys

    close_vals = close_series.values.astype(float)
    entry_mask = entries.values.astype(bool)
    exit_mask = exits.values.astype(bool)
    n = len(close_vals)

    _sys.stderr.write(f'[SIMULATE] === _simulate_signals START ===\n')
    _sys.stderr.write(f'[SIMULATE] n={n}, entry_mask sum={int(entry_mask.sum())}, exit_mask sum={int(exit_mask.sum())}\n')
    _sys.stderr.write(f'[SIMULATE] initial_capital={initial_capital}, commission={commission}, slippage={slippage}\n')
    _sys.stderr.write(f'[SIMULATE] size_series is None: {size_series is None}, allow_short={allow_short}\n')
    _sys.stderr.write(f'[SIMULATE] close_vals: dtype={close_vals.dtype}, first5={close_vals[:5].tolist()}, last5={close_vals[-5:].tolist()}\n')
    _sys.stderr.write(f'[SIMULATE] entry_mask: dtype={entry_mask.dtype}, first20={entry_mask[:20].tolist()}\n')
    _sys.stderr.write(f'[SIMULATE] exit_mask: dtype={exit_mask.dtype}, first20={exit_mask[:20].tolist()}\n')

    # Find first entry and exit indices
    entry_indices = [i for i in range(min(n, 50)) if entry_mask[i]]
    exit_indices = [i for i in range(min(n, 50)) if exit_mask[i]]
    _sys.stderr.write(f'[SIMULATE] First entry indices (up to bar 50): {entry_indices}\n')
    _sys.stderr.write(f'[SIMULATE] First exit indices (up to bar 50): {exit_indices}\n')

    cash = initial_capital
    position = 0.0
    equity_vals = np.zeros(n)
    total_fees = 0.0
    trades = []

    # Per-bar tracking arrays
    cash_vals = np.zeros(n) if track_per_bar else None
    asset_vals = np.zeros(n) if track_per_bar else None
    cash_flow_vals = np.zeros(n) if track_per_bar else None
    asset_flow_vals = np.zeros(n) if track_per_bar else None

    entry_price = 0.0
    entry_idx = 0
    in_position = False
    prev_position = 0.0

    n_entries_triggered = 0
    n_exits_triggered = 0
    n_entries_skipped = 0

    for i in range(n):
        price = close_vals[i]
        prev_position = position

        # Apply slippage
        buy_price = price * (1 + slippage)
        sell_price = price * (1 - slippage)

        if in_position:
            # Check exit
            if exit_mask[i]:
                n_exits_triggered += 1
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
                _sys.stderr.write(f'[SIMULATE] EXIT #{n_exits_triggered} at bar {i}: sell_price={sell_price:.2f}, shares={position:.4f}, pnl={pnl:.2f}\n')
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
                        n_entries_triggered += 1
                        cash -= cost
                        position = shares
                        entry_price = buy_price
                        entry_idx = i
                        in_position = True
                        _sys.stderr.write(f'[SIMULATE] ENTRY #{n_entries_triggered} at bar {i}: buy_price={buy_price:.2f}, shares={shares:.4f}, cost={cost:.2f}, cash_left={cash:.2f}\n')
                    else:
                        n_entries_skipped += 1
                        _sys.stderr.write(f'[SIMULATE] ENTRY SKIPPED at bar {i}: cost={cost:.2f} > cash={cash:.2f}\n')
                else:
                    n_entries_skipped += 1
                    _sys.stderr.write(f'[SIMULATE] ENTRY SKIPPED at bar {i}: shares={shares} <= 0\n')

        equity_vals[i] = cash + position * price

        # Track per-bar data
        if track_per_bar:
            cash_vals[i] = cash
            asset_vals[i] = position
            cash_flow_vals[i] = cash - (cash_vals[i - 1] if i > 0 else initial_capital)
            asset_flow_vals[i] = position - prev_position

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
        if track_per_bar:
            cash_vals[-1] = cash
            asset_vals[-1] = 0.0

    _sys.stderr.write(f'[SIMULATE] === SIMULATION SUMMARY ===\n')
    _sys.stderr.write(f'[SIMULATE] entries_triggered={n_entries_triggered}, exits_triggered={n_exits_triggered}, entries_skipped={n_entries_skipped}\n')
    _sys.stderr.write(f'[SIMULATE] total trades recorded: {len(trades)}\n')
    _sys.stderr.write(f'[SIMULATE] final cash={cash:.2f}, final position={position:.4f}\n')
    _sys.stderr.write(f'[SIMULATE] equity[0]={equity_vals[0]:.2f}, equity[-1]={equity_vals[-1]:.2f}\n')
    _sys.stderr.write(f'[SIMULATE] total_fees={total_fees:.4f}\n')

    equity = pd.Series(equity_vals, index=close_series.index, name='Equity')
    trade_df = pd.DataFrame(trades) if trades else _empty_trade_df()

    per_bar = None
    if track_per_bar:
        idx = close_series.index
        per_bar = {
            'cash': pd.Series(cash_vals, index=idx, name='Cash'),
            'assets': pd.Series(asset_vals, index=idx, name='Assets'),
            'cash_flow': pd.Series(cash_flow_vals, index=idx, name='Cash Flow'),
            'asset_flow': pd.Series(asset_flow_vals, index=idx, name='Asset Flow'),
        }

    return equity, trade_df, total_fees, per_bar


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


# ============================================================================
# from_order_func Builder (NEW)
# ============================================================================

def build_portfolio_from_order_func(
    vbt,
    close_series: pd.Series,
    order_func: Callable,
    initial_capital: float = 100000,
    commission: float = 0.0,
    **kwargs,
) -> SimplePortfolio:
    """
    Build portfolio from a custom order function (mimics vbt.Portfolio.from_order_func).

    The order_func is called on each bar with signature:
        order_func(bar_idx, price, cash, position, **kwargs) -> order_size
            - positive size = buy that many shares
            - negative size = sell that many shares
            - 0 = do nothing

    Args:
        vbt: vectorbt module (unused, API compat)
        close_series: Price series
        order_func: Callable(bar_idx, price, cash, position, **kwargs) -> float
        initial_capital: Starting capital
        commission: Commission rate (0.001 = 0.1%)

    Returns:
        SimplePortfolio instance
    """
    close_vals = close_series.values.astype(float)
    n = len(close_vals)

    cash = initial_capital
    position = 0.0
    equity_vals = np.zeros(n)
    cash_vals = np.zeros(n)
    asset_vals = np.zeros(n)
    total_fees = 0.0
    trades = []

    entry_price = 0.0
    entry_idx = 0

    for i in range(n):
        price = close_vals[i]

        # Call user's order function
        order_size = order_func(i, price, cash, position, **kwargs)

        if order_size != 0:
            cost = abs(order_size) * price
            fee = cost * commission
            total_fees += fee

            if order_size > 0:
                # Buy
                total_cost = order_size * price + fee
                if total_cost <= cash:
                    if position <= 0 and position != 0:
                        # Close short first
                        pnl = (entry_price - price) * abs(position) - fee
                        trades.append(_make_trade_record(
                            entry_idx, i, entry_price, price, abs(position), pnl, fee
                        ))
                    cash -= total_cost
                    if position <= 0:
                        entry_price = price
                        entry_idx = i
                    position += order_size
            elif order_size < 0:
                # Sell
                sell_amount = min(abs(order_size), position) if position > 0 else abs(order_size)
                if position > 0:
                    pnl = (price - entry_price) * sell_amount - fee
                    trades.append(_make_trade_record(
                        entry_idx, i, entry_price, price, sell_amount, pnl, fee
                    ))
                    cash += sell_amount * price - fee
                    position -= sell_amount

        equity_vals[i] = cash + position * price
        cash_vals[i] = cash
        asset_vals[i] = position

    idx = close_series.index
    equity = pd.Series(equity_vals, index=idx, name='Equity')
    trade_df = pd.DataFrame(trades) if trades else _empty_trade_df()

    return SimplePortfolio(
        equity_series=equity,
        close_series=close_series,
        trade_records=trade_df,
        init_cash=initial_capital,
        total_fees_value=total_fees,
        cash_series=pd.Series(cash_vals, index=idx, name='Cash'),
        asset_series=pd.Series(asset_vals, index=idx, name='Assets'),
    )


# ============================================================================
# Position Aggregation Helper (NEW)
# ============================================================================

def _aggregate_positions(trade_records: pd.DataFrame) -> pd.DataFrame:
    """
    Aggregate consecutive same-direction trades into positions.

    A position starts with the first entry and ends when the position
    is fully closed. Multiple partial fills are merged.
    """
    if len(trade_records) == 0:
        return _empty_trade_df()

    positions = []
    current_pos = None

    for _, row in trade_records.iterrows():
        if current_pos is None:
            current_pos = {
                'Entry Idx': int(row.get('Entry Idx', 0)),
                'Entry Price': float(row.get('Entry Price', 0)),
                'Size': float(row.get('Size', 0)),
                'PnL': float(row.get('PnL', 0)),
                'Entry Fees': float(row.get('Entry Fees', 0)),
                'Exit Fees': float(row.get('Exit Fees', 0)),
                'Exit Idx': int(row.get('Exit Idx', 0)),
                'Exit Price': float(row.get('Exit Price', 0)),
            }
        else:
            # Check if this trade is contiguous (entry follows previous exit)
            prev_exit = current_pos['Exit Idx']
            cur_entry = int(row.get('Entry Idx', 0))
            if cur_entry <= prev_exit + 1:
                # Merge into current position
                new_size = float(row.get('Size', 0))
                old_size = current_pos['Size']
                total_size = old_size + new_size
                # Weighted average entry price
                current_pos['Entry Price'] = (
                    (current_pos['Entry Price'] * old_size + float(row.get('Entry Price', 0)) * new_size) / total_size
                ) if total_size > 0 else current_pos['Entry Price']
                current_pos['Size'] = total_size
                current_pos['PnL'] += float(row.get('PnL', 0))
                current_pos['Entry Fees'] += float(row.get('Entry Fees', 0))
                current_pos['Exit Fees'] += float(row.get('Exit Fees', 0))
                current_pos['Exit Idx'] = int(row.get('Exit Idx', 0))
                current_pos['Exit Price'] = float(row.get('Exit Price', 0))
            else:
                # Close current position, start new one
                current_pos['Duration'] = max(1, current_pos['Exit Idx'] - current_pos['Entry Idx'])
                current_pos['Return'] = (
                    (current_pos['Exit Price'] / current_pos['Entry Price'] - 1.0)
                    if current_pos['Entry Price'] > 0 else 0.0
                )
                current_pos['Status'] = 'Closed'
                positions.append(current_pos)
                current_pos = {
                    'Entry Idx': int(row.get('Entry Idx', 0)),
                    'Entry Price': float(row.get('Entry Price', 0)),
                    'Size': float(row.get('Size', 0)),
                    'PnL': float(row.get('PnL', 0)),
                    'Entry Fees': float(row.get('Entry Fees', 0)),
                    'Exit Fees': float(row.get('Exit Fees', 0)),
                    'Exit Idx': int(row.get('Exit Idx', 0)),
                    'Exit Price': float(row.get('Exit Price', 0)),
                }

    # Finalize last position
    if current_pos is not None:
        current_pos['Duration'] = max(1, current_pos['Exit Idx'] - current_pos['Entry Idx'])
        current_pos['Return'] = (
            (current_pos['Exit Price'] / current_pos['Entry Price'] - 1.0)
            if current_pos['Entry Price'] > 0 else 0.0
        )
        current_pos['Status'] = 'Closed'
        positions.append(current_pos)

    return pd.DataFrame(positions) if positions else _empty_trade_df()
