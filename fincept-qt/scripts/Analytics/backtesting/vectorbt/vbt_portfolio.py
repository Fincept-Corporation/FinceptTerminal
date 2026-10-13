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


def periods_per_year(index) -> float:
    """Bars per year implied by a price/equity index (used to annualise Sharpe/vol/CAGR).

    Everything here used to assume 252 bars/year, which is only right for DAILY equity
    bars: a 1h or 5m run was annualised as if each bar were a day (CAGR exponent and the
    sqrt(periods) factor both off by ~x6-x78), and 7-day crypto data was understated.
    - daily-like spacing: 252 when the index has weekend gaps (equities), 365 when it does
      not (crypto/FX 24x7); weekly 52; monthly 12
    - intraday: observed bars / calendar years spanned (so session length and holidays are
      measured, not guessed)
    """
    try:
        # Only a real datetime index carries a frequency; a RangeIndex would be silently
        # reinterpreted as 1970-epoch nanoseconds and yield an absurd bars/year.
        if not isinstance(index, pd.DatetimeIndex) and getattr(getattr(index, 'dtype', None), 'kind', '') != 'M':
            return 252.0
        idx = pd.DatetimeIndex(index)
        n = len(idx)
        if n < 3:
            return 252.0
        secs = idx.asi8.astype('float64')
        # asi8 is in the index's own resolution (ns/us/ms/s): normalise to seconds.
        unit = str(idx.dtype)
        if 'ns' in unit:
            secs = secs / 1e9
        elif 'us' in unit:
            secs = secs / 1e6
        elif 'ms' in unit:
            secs = secs / 1e3
        deltas = np.diff(secs)
        deltas = deltas[deltas > 0]
        if len(deltas) == 0:
            return 252.0
        med = float(np.median(deltas))
        if med < 1.0:  # sub-second spacing is not market data; keep the daily convention
            return 252.0
        if med >= 25 * 86400:
            return 12.0
        if med >= 6 * 86400:
            return 52.0
        if med >= 20 * 3600:
            return 252.0 if bool(np.any(deltas > 1.5 * 86400)) else 365.0
        span_years = float(secs[-1] - secs[0]) / (365.25 * 86400.0)
        if span_years <= 0:
            return 252.0
        return float(max(1.0, (n - 1) / span_years))
    except Exception:
        return 252.0


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

    @classmethod
    def from_equity_series(cls, equity: pd.Series, init_cash: float,
                           trade_records: Optional[pd.DataFrame] = None) -> 'SimplePortfolio':
        """Build a SimplePortfolio from a pre-computed equity series (e.g. multi-asset combined).

        `trade_records` (the per-symbol trades concatenated) feeds the trade statistics; without
        it a multi-asset run reported 0 trades / 0% win rate / 0 profit factor alongside a trade
        list that was not empty.
        """
        if trade_records is None or len(trade_records) == 0:
            trade_records = pd.DataFrame(columns=[
                'Entry Idx', 'Exit Idx', 'Size', 'Entry Price', 'Exit Price',
                'PnL', 'Return', 'Entry Fees', 'Exit Fees', 'Direction', 'Status',
            ])
        close_proxy = pd.Series(equity.values, index=equity.index, name='Close')
        return cls(
            equity_series=equity,
            close_series=close_proxy,
            trade_records=trade_records,
            init_cash=init_cash,
        )

    # ------------------------------------------------------------------
    # Core value / return methods (existing)
    # ------------------------------------------------------------------

    def value(self) -> pd.Series:
        return self._equity

    def periods_per_year(self) -> float:
        """Bars per year of this portfolio's equity index (annualisation basis)."""
        return periods_per_year(self._equity.index)

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

        # Annualisation basis follows the bar interval (see periods_per_year).
        ppy = self.periods_per_year()

        # Annualized return
        years = n / ppy
        if years > 0 and self._init_cash > 0 and equity[-1] > 0:
            ann_ret = (equity[-1] / self._init_cash) ** (1 / years) - 1
        else:
            ann_ret = 0.0
        s['Annualized Return [%]'] = ann_ret * 100

        # Volatility
        if len(daily_ret) > 1:
            daily_std = float(np.std(daily_ret, ddof=1))
            ann_vol = daily_std * np.sqrt(ppy)
        else:
            daily_std = 0.0
            ann_vol = 0.0
        s['Annualized Volatility [%]'] = ann_vol * 100

        # Sharpe
        if ann_vol > 1e-10:
            s['Sharpe Ratio'] = ann_ret / ann_vol
        else:
            s['Sharpe Ratio'] = 0.0

        # Sortino: standard downside deviation = sqrt(mean(min(r, 0)^2)) over ALL bars.
        # (It used the sample std of only the negative bars, which both re-centres them on
        # their own mean and divides by the number of losing bars — overstating downside risk
        # and understating Sortino whenever losses are the minority.)
        if len(daily_ret) > 1:
            downside_std = float(np.sqrt(np.mean(np.minimum(daily_ret, 0.0) ** 2))) * np.sqrt(ppy)
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
            # Winners and no losing trade -> +inf (the JSON layer turns it into null and the
            # UI shows "∞"); the old 0.0 read as "worst possible" for the best possible run.
            if total_losses > 0:
                s['Profit Factor'] = total_wins / total_losses
            else:
                s['Profit Factor'] = float('inf') if total_wins > 0 else 0.0
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
    open_series: Optional[pd.Series] = None,
) -> SimplePortfolio:
    """
    Build a portfolio from entry/exit signals.

    Execution model (stated in the result's `assumptions` block):
      - signals are evaluated on the CLOSE of bar t (they are computed from it);
      - the resulting order fills at the OPEN of bar t+1 (`open_series`), or at the
        close of bar t+1 when no open prices are available. Filling at the close
        that produced the signal is a look-ahead the old simulator made on every
        trade. `request['execution'] == 'same_close'` restores it for comparison.
      - stop-loss / take-profit / trailing stop are checked on each bar's close
        against the ACTUAL entry fill and exit at the next bar's open.

    Position sizing (request['positionSizing'] / ['positionSizeValue']) follows the
    UI's units: percent = % of equity, fixed = currency amount, risk = % of equity
    risked to the stop-loss, vol_target = target annualised volatility %, kelly =
    Kelly fraction estimated from trades closed so far, capped at the value %.
    """
    import sys as _sys
    request = request or {}

    commission = float(request.get('commission', 0.0) or 0.0)
    slippage = float(request.get('slippage', 0.0) or 0.0)

    def _frac(key):
        v = request.get(key, None)
        try:
            v = float(v) if v is not None else 0.0
        except (TypeError, ValueError):
            v = 0.0
        return v if v > 0 else None

    stop_loss = _frac('stopLoss')
    take_profit = _frac('takeProfit')
    trailing_stop = _frac('trailingStop')

    sizing = resolve_sizing(request, stop_loss)
    next_bar = str(request.get('execution', 'next_open')) != 'same_close'

    fill_prices = None
    if next_bar and open_series is not None:
        aligned = open_series.reindex(close_series.index).astype(float)
        # A missing open (data gap) falls back to that bar's close.
        fill_prices = aligned.where(np.isfinite(aligned) & (aligned > 0), close_series.astype(float)).values

    _sys.stderr.write(f'[PF-BUILD] commission={commission}, slippage={slippage}, sizing={sizing}, '
                      f'next_bar={next_bar}, open_prices={fill_prices is not None}, '
                      f'sl={stop_loss}, tp={take_profit}, ts={trailing_stop}\n')

    equity, trade_records, total_fees, per_bar = _simulate_signals(
        close_series, entries, exits,
        initial_capital=initial_capital,
        commission=commission,
        slippage=slippage,
        track_per_bar=True,
        next_bar=next_bar,
        fill_prices=fill_prices,
        stop_loss=stop_loss,
        take_profit=take_profit,
        trailing_stop=trailing_stop,
        sizing=sizing,
    )

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


def resolve_sizing(request: Dict[str, Any], stop_loss: Optional[float]) -> Dict[str, Any]:
    """Translate the UI's sizing mode + value into the simulator's sizing spec.

    Every mode used to fall through to "100% of cash": 'percent', 'vol_target' and
    'risk' matched no branch, and 'fixed' multiplied the currency amount by 100 and
    then treated anything >= 100% as all-in. A 10% sizing therefore ran (and
    reported) a 10x larger position than the user asked for.
    """
    mode = request.get('positionSizing', 'percent')
    if isinstance(mode, dict):
        mode = mode.get('mode', 'percent')
    mode = str(mode or 'percent').lower()
    try:
        value = float(request.get('positionSizeValue', 100) or 100)
    except (TypeError, ValueError):
        value = 100.0

    if mode == 'fixed':
        return {'mode': 'fixed', 'amount': max(0.0, value)}
    if mode == 'risk':
        if not stop_loss:
            raise ValueError('Risk-per-trade sizing needs a STOP LOSS: the position size is the amount '
                             'risked divided by the distance to the stop. Set a stop-loss or choose '
                             'another sizing method.')
        return {'mode': 'risk', 'risk': min(max(value, 0.0), 100.0) / 100.0, 'stop': stop_loss}
    if mode in ('vol_target', 'volatility'):
        return {'mode': 'vol_target', 'target': max(value, 0.0) / 100.0, 'lookback': 20}
    if mode == 'kelly':
        return {'mode': 'kelly', 'cap': min(max(value, 0.0), 100.0) / 100.0, 'min_trades': 10}
    # percent (default)
    return {'mode': 'percent', 'fraction': min(max(value, 0.0), 100.0) / 100.0}


def describe_sizing(sizing: Dict[str, Any]) -> str:
    """One-line description of a resolve_sizing() spec for the assumptions block."""
    m = sizing.get('mode')
    if m == 'fixed':
        return f"fixed: {sizing['amount']:,.2f} per entry (capped at available cash)"
    if m == 'risk':
        return (f"risk: {sizing['risk'] * 100:.2f}% of equity lost if the {sizing['stop'] * 100:.2f}% "
                f"stop is hit (capped at available cash)")
    if m == 'vol_target':
        return (f"vol_target: exposure = {sizing['target'] * 100:.1f}% / realised {sizing['lookback']}-bar "
                f"volatility, max 100% of equity (no leverage)")
    if m == 'kelly':
        return (f"kelly: Kelly fraction from trades closed so far (no look-ahead), capped at "
                f"{sizing['cap'] * 100:.1f}% of equity; the cap is used until {sizing['min_trades']} trades closed")
    return f"percent: {sizing.get('fraction', 1.0) * 100:.1f}% of equity per entry"


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
        # Kept so callers can compute an empirical p-value (share of random runs
        # that did at least as well as the strategy).
        '_returns': returns_arr,
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
    track_per_bar: bool = False,
    next_bar: bool = False,
    fill_prices: Optional[np.ndarray] = None,
    stop_loss: Optional[float] = None,
    take_profit: Optional[float] = None,
    trailing_stop: Optional[float] = None,
    sizing: Optional[Dict[str, Any]] = None,
) -> Tuple[pd.Series, pd.DataFrame, float, Optional[Dict]]:
    """
    Simulate a long-only portfolio from boolean signals.

    next_bar=False: an order fills at the close of the bar whose signal produced it
                    (legacy, look-ahead-prone; used by the random benchmark only).
    next_bar=True : the signal on bar t's close is executed on bar t+1 at
                    fill_prices[t+1] (the open; the close when no open is given).
    Stops are evaluated on each bar's close against the actual entry fill and,
    like signals, execute on the next bar when next_bar is set.

    Returns:
        (equity_series, trade_records_df, total_fees, per_bar_data_or_None)
    """
    close_vals = close_series.values.astype(float)
    entry_mask = entries.reindex(close_series.index).fillna(False).values.astype(bool)
    exit_mask = exits.reindex(close_series.index).fillna(False).values.astype(bool)
    n = len(close_vals)
    fills = fill_prices if fill_prices is not None else close_vals
    sizing = sizing or {'mode': 'percent', 'fraction': 1.0}

    # Realised volatility known at each bar's close (vol_target sizing).
    vol_known = None
    if sizing.get('mode') == 'vol_target':
        ppy = periods_per_year(close_series.index)
        rets = pd.Series(close_vals).pct_change()
        vol_known = (rets.rolling(int(sizing.get('lookback', 20))).std() * np.sqrt(ppy)).values

    cash = float(initial_capital)
    position = 0.0
    equity_vals = np.zeros(n)
    total_fees = 0.0
    trades = []
    closed_pnl_returns = []  # per-trade returns of closed trades (kelly sizing)

    cash_vals = np.zeros(n) if track_per_bar else None
    asset_vals = np.zeros(n) if track_per_bar else None
    cash_flow_vals = np.zeros(n) if track_per_bar else None
    asset_flow_vals = np.zeros(n) if track_per_bar else None

    entry_price = 0.0
    entry_fee = 0.0
    entry_idx = 0
    peak_close = 0.0
    in_position = False
    pend_entry = False
    pend_exit = False
    pend_reason = ''
    n_skipped = 0

    def stop_reason(i):
        c = close_vals[i]
        if stop_loss and c <= entry_price * (1 - stop_loss):
            return 'stop_loss'
        if take_profit and c >= entry_price * (1 + take_profit):
            return 'take_profit'
        if trailing_stop and c <= peak_close * (1 - trailing_stop):
            return 'trailing_stop'
        return ''

    def target_fraction(decision_idx):
        mode = sizing.get('mode')
        if mode == 'vol_target':
            v = vol_known[decision_idx] if vol_known is not None and decision_idx >= 0 else np.nan
            if not np.isfinite(v) or v <= 0:
                return 0.0  # not enough history to measure volatility yet: stay flat
            return float(min(1.0, sizing.get('target', 0.0) / v))
        if mode == 'kelly':
            cap = float(sizing.get('cap', 1.0))
            if len(closed_pnl_returns) < int(sizing.get('min_trades', 10)):
                return cap
            r = np.asarray(closed_pnl_returns)
            wins, losses = r[r > 0], r[r <= 0]
            if len(wins) == 0:
                return 0.0
            if len(losses) == 0 or abs(losses.mean()) < 1e-12:
                return cap
            w = len(wins) / len(r)
            payoff = wins.mean() / abs(losses.mean())
            return float(min(cap, max(0.0, w - (1 - w) / payoff)))
        if mode == 'percent':
            return float(sizing.get('fraction', 1.0))
        return 1.0

    def open_long(i, price_raw, decision_idx):
        nonlocal cash, position, entry_price, entry_fee, entry_idx, in_position, total_fees, peak_close, n_skipped
        buy_price = price_raw * (1 + slippage)
        if buy_price <= 0:
            n_skipped += 1
            return
        max_shares = cash / (buy_price * (1 + commission))
        mode = sizing.get('mode')
        if mode == 'fixed':
            shares = min(float(sizing.get('amount', 0.0)), cash) / (buy_price * (1 + commission))
        elif mode == 'risk':
            shares = (cash * float(sizing['risk'])) / (buy_price * float(sizing['stop']))
        else:
            shares = max_shares * target_fraction(decision_idx)
        shares = min(shares, max_shares)
        if shares <= 0:
            n_skipped += 1
            return
        fee = shares * buy_price * commission
        cost = shares * buy_price + fee
        total_fees += fee
        cash = max(cash - cost, 0.0)
        position = shares
        entry_price = buy_price
        entry_fee = fee
        entry_idx = i
        peak_close = buy_price
        in_position = True

    def close_long(i, price_raw, reason):
        nonlocal cash, position, in_position, total_fees
        sell_price = price_raw * (1 - slippage)
        fee = abs(position) * sell_price * commission
        total_fees += fee
        # Trade P&L is NET of both legs' commission.
        pnl = (sell_price - entry_price) * position - entry_fee - fee
        cost_basis = entry_price * position + entry_fee
        ret = (pnl / cost_basis) if cost_basis > 0 else 0.0
        trades.append(_make_trade_record(
            entry_idx, i, entry_price, sell_price, position, pnl, entry_fee + fee,
            duration=max(1, i - entry_idx), ret=ret, entry_fee=entry_fee, exit_reason=reason,
        ))
        closed_pnl_returns.append(ret)
        cash += position * sell_price - fee
        position = 0.0
        in_position = False

    for i in range(n):
        prev_position = position

        if next_bar:
            # 1) Execute what was decided on the previous bar's close.
            if pend_exit and in_position:
                close_long(i, fills[i], pend_reason or 'signal')
            elif pend_entry and not in_position:
                open_long(i, fills[i], i - 1)
            pend_entry = pend_exit = False
            pend_reason = ''
            # 2) Decide on this bar's close.
            if in_position:
                peak_close = max(peak_close, close_vals[i])
                reason = stop_reason(i)
                if reason or exit_mask[i]:
                    pend_exit = True
                    pend_reason = reason or 'signal'
            elif entry_mask[i]:
                pend_entry = True
        else:
            if in_position:
                peak_close = max(peak_close, close_vals[i])
                reason = stop_reason(i)
                if reason or exit_mask[i]:
                    close_long(i, close_vals[i], reason or 'signal')
            elif entry_mask[i]:
                open_long(i, close_vals[i], i)

        equity_vals[i] = cash + position * close_vals[i]
        if track_per_bar:
            cash_vals[i] = cash
            asset_vals[i] = position
            cash_flow_vals[i] = cash - (cash_vals[i - 1] if i > 0 else initial_capital)
            asset_flow_vals[i] = position - prev_position

    # Mark an open position out at the last close (with exit costs) so the final
    # equity and the trade list agree.
    if in_position and position > 0:
        close_long(n - 1, close_vals[-1], 'end_of_data')
        equity_vals[-1] = cash
        if track_per_bar:
            cash_vals[-1] = cash
            asset_vals[-1] = 0.0

    if n_skipped:
        import sys as _sys
        _sys.stderr.write(f'[SIMULATE] {n_skipped} entry signal(s) skipped (zero size)\n')

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
    entry_fee: Optional[float] = None,
    exit_reason: Optional[str] = None,
) -> Dict[str, Any]:
    """Create a trade record dict matching vbt's records_readable format.

    `fees` is the round-trip total. When the entry leg's fee is known (`entry_fee`) it is
    reported exactly; otherwise the total is split evenly between the two legs.
    """
    if duration is None:
        duration = max(1, exit_idx - entry_idx)  # integer: number of bars
    if ret == 0.0 and entry_price > 0:
        ret = (exit_price / entry_price - 1.0)
    if entry_fee is None:
        entry_fee = fees / 2
    rec = {
        'Entry Idx': entry_idx,
        'Exit Idx': exit_idx,
        'Entry Price': entry_price,
        'Exit Price': exit_price,
        'Size': size,
        'PnL': pnl,
        'Return': ret,
        'Entry Fees': entry_fee,
        'Exit Fees': fees - entry_fee,
        'Duration': duration,
        'Status': 'Closed',
    }
    if exit_reason:
        rec['Exit Reason'] = exit_reason
    return rec


def _empty_trade_df() -> pd.DataFrame:
    """Return an empty DataFrame with the expected columns."""
    return pd.DataFrame(columns=[
        'Entry Idx', 'Exit Idx', 'Entry Price', 'Exit Price',
        'Size', 'PnL', 'Return', 'Entry Fees', 'Exit Fees',
        'Duration', 'Status',
    ])


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
