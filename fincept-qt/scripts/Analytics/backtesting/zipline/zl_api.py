"""
Fincept zipline-style algorithm API - the strategy layer of the "zipline" engine.

zipline-reloaded is NOT used (3.2.1 is installed in the backtesting environment, but it runs only
on ingested data bundles - an asset database and bcolz bar files written to disk per symbol set -
on one exchange calendar, and would add nothing base.simulator does not model; see
zipline_provider.py). What this module keeps from zipline is the WAY a strategy is written:

    def initialize(context):
        context.fast = context.indicator('sma', 20)          # full-history, computed once
        context.slow = context.indicator('sma', 50)

    def handle_data(context, data):                         # called once per bar
        for asset in context.assets:
            f, s = data.current(asset, context.fast), data.current(asset, context.slow)
            if f == f and s == s:                           # both defined (NaN during warm-up)
                context.order_target_percent(asset, 1.0 if f > s else 0.0)

ZIPLINE NAMES KEPT
  context.assets / context.asset (the first symbol) / context.params
  context.schedule_function(func, date_rule)   func(context, data) runs BEFORE handle_data on the bars
                                               its rule selects (zipline's order of events)
  context.order_target_percent(asset, target)  the order (see EXECUTION)
  context.get_datetime()                       the current bar's timestamp
  data.current(asset, field)                   'open' 'high' 'low' 'close' 'price' 'volume' or an
                                               indicator line from context.indicator(...)
  data.history(asset, field, bar_count)        the last bar_count values up to this bar (numpy)
  data.can_trade(asset)                        the asset's market traded on this bar
  date_rules.every_day() / week_start() / month_start() / quarter_start() / year_start() /
  every_n_bars(n) (Fincept: every n-th bar, zipline v1's day counter) / from_period(text or n)

WHAT IS DIFFERENT FROM zipline (and why)
  * Indicators are computed ONCE on each symbol's own full history (the Fincept indicator table,
    vectorbt\\vbt_indicators.py - the definitions the research commands use: Wilder RSI / ATR / ADX
    on high / low / close, SMA-seeded EMA, population-std Bollinger bands ...) when initialize asks
    for them, and mapped onto the run's bar timeline; handle_data reads the value of the current
    bar. O(1) per bar and causal (the value at bar t uses bars <= t only). v1 recomputed every
    indicator on a 10-30 bar window each bar, so recursive indicators never converged (RSI off by
    3.5 points on average, audit Z2) and a 10-year run spent 1.5 s in pandas slices (Z22).
  * DECIDE, THEN EXECUTE. run_algorithm() walks the bars once and records the ORDERS (a target per
    asset per bar, NaN = no order); base.simulator executes them afterwards with the shared cost
    model at the request's fillTiming (cash-checked sizing at the fill price). The algorithm
    therefore cannot read its fills (no context.portfolio): order_target_percent is idempotent (a
    target the position already has places no trade), so a rule states the target it wants and the
    executor knows the real position - v1's `context.invested` flag, set when an order was PLACED
    rather than filled, is gone (Z17).
  * Execution mode per algorithm (catalogue `kind`):
      signals     long / flat per asset: target > 0 = be long (an entry while flat), 0 = be flat (an
                  exit while holding). The entry size is the request's position sizing on the asset's
                  slot (equity / number of symbols) times the target. allowShort turns "be flat" into
                  a short (the cross-engine convention); stop-loss / take-profit / trailing stop act
                  intrabar. No drift rebalancing while a position is held.
      allocation  target = fraction of the portfolio equity; every order is a rebalance of that asset
                  to its weight (base.simulator weights mode: sells before buys, costs paid out of the
                  portfolio). Assets without an order keep their position.
"""

from __future__ import annotations

import math
import os
import sys
from typing import Any, Callable, Dict, List, Optional, Tuple

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
_VBT = os.path.join(os.path.dirname(_HERE), 'vectorbt')
if _VBT not in sys.path:
    sys.path.append(_VBT)          # appended: the zipline engine's own modules keep precedence

NAN = float('nan')
FIELDS = ('open', 'high', 'low', 'close', 'price', 'volume')


# ════════════════════════════════════════════════════════════════════════════
# Assets, indicator lines, date rules
# ════════════════════════════════════════════════════════════════════════════

class Asset:
    """zipline's Equity: sid = column of the run's panel."""
    __slots__ = ('sid', 'symbol')

    def __init__(self, sid: int, symbol: str):
        self.sid = sid
        self.symbol = symbol

    def __repr__(self) -> str:
        return f'Equity({self.sid} [{self.symbol}])'


class Indicator:
    """One indicator line for every asset on the run's bar timeline (Python lists: O(1) reads)."""
    __slots__ = ('key', 'values')

    def __init__(self, key: tuple, values: List[List[float]]):
        self.key = key
        self.values = values


class DateRule:
    """When a scheduled function runs: a boolean mask over the run's bars."""

    def __init__(self, kind: str, n: int = 1):
        self.kind = kind
        self.n = int(n)

    def mask(self, dates) -> np.ndarray:
        import pandas as pd
        idx = pd.DatetimeIndex(dates)
        m = len(idx)
        if m == 0:
            return np.zeros(0, dtype=bool)
        if self.kind == 'every_day':
            return np.ones(m, dtype=bool)
        if self.kind == 'every_n_bars':
            return (np.arange(1, m + 1) % max(1, self.n)) == 0      # zipline v1's day counter
        freq = {'week_start': 'W', 'month_start': 'M', 'quarter_start': 'Q', 'year_start': 'Y'}[self.kind]
        per = (idx.tz_localize(None) if idx.tz is not None else idx).to_period(freq)
        out = np.ones(m, dtype=bool)
        out[1:] = per[1:] != per[:-1]
        return out

    def describe(self) -> str:
        return {'every_day': 'every bar', 'week_start': 'first bar of each week',
                'month_start': 'first bar of each month', 'quarter_start': 'first bar of each quarter',
                'year_start': 'first bar of each year'}.get(self.kind, f'every {self.n} bars')


PERIODS = ('daily', 'weekly', 'monthly', 'quarterly', 'yearly')
_PERIOD_ALIASES = {'day': 'daily', 'd': 'daily', 'week': 'weekly', 'w': 'weekly', 'month': 'monthly', 'm': 'monthly',
                   'quarter': 'quarterly', 'q': 'quarterly', 'year': 'yearly', 'annual': 'yearly',
                   'annually': 'yearly', 'y': 'yearly'}


class date_rules:  # noqa: N801  (zipline.api.date_rules)
    @staticmethod
    def every_day() -> DateRule:
        return DateRule('every_day')

    @staticmethod
    def week_start() -> DateRule:
        return DateRule('week_start')

    @staticmethod
    def month_start() -> DateRule:
        return DateRule('month_start')

    @staticmethod
    def quarter_start() -> DateRule:
        return DateRule('quarter_start')

    @staticmethod
    def year_start() -> DateRule:
        return DateRule('year_start')

    @staticmethod
    def every_n_bars(n: int) -> DateRule:
        if int(n) < 1:
            raise ValueError(f'every_n_bars needs n >= 1 (got {n})')
        return DateRule('every_n_bars', int(n))

    @staticmethod
    def from_period(value: Any) -> DateRule:
        """'daily' | 'weekly' | 'monthly' | 'quarterly' | 'yearly' (first bar of each period) or a number
        of bars."""
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            return date_rules.every_n_bars(int(round(float(value))))
        s = str(value or 'monthly').strip().lower()
        s = _PERIOD_ALIASES.get(s, s)
        try:
            return date_rules.every_n_bars(int(round(float(s))))
        except ValueError:
            pass
        rule = {'daily': 'every_day', 'weekly': 'week_start', 'monthly': 'month_start',
                'quarterly': 'quarter_start', 'yearly': 'year_start'}.get(s)
        if rule is None:
            raise ValueError(f'rebalance period must be one of {", ".join(PERIODS)} or a number of bars, '
                             f'not {value!r}')
        return DateRule(rule)


# ════════════════════════════════════════════════════════════════════════════
# Bars of one run (+ memoised full-history indicators per symbol)
# ════════════════════════════════════════════════════════════════════════════

def _zscore_sample(ta, n: int):
    import vbt_indicators as I

    def calc():
        m, sd = I.sma(ta.c, n), I.rolling_std(ta.c, n, ddof=1)
        with np.errstate(divide='ignore', invalid='ignore'):
            z = (ta.c - m) / sd
        return np.where(np.isfinite(z), z, np.nan)
    return ta.memo(('z_sample', int(n)), calc)


def _obv_sma(ta, n: int):
    import vbt_indicators as I
    return ta.memo(('obv_sma', int(n)), lambda: I.sma(ta.obv(), n))


# name -> (fn(ta, *args) -> array | {output: array}); every definition is vbt_indicators' except the
# two derived lines marked (zipline)
INDICATORS: Dict[str, Callable[..., Any]] = {
    'sma': lambda ta, n: ta.sma(n),
    'ema': lambda ta, n: ta.ema(n),
    'rsi': lambda ta, n: ta.rsi(n),                                   # Wilder
    'macd': lambda ta, f, s, g=9: ta.macd(f, s, g),                   # macd / signal / histogram
    'atr': lambda ta, n: ta.atr(n),                                   # Wilder, true range on H / L / C
    'adx': lambda ta, n: ta.adx(n),                                   # adx / plus_di / minus_di
    'stochastic': lambda ta, k, d: ta.stoch(k, d),                    # k / d
    'williams_r': lambda ta, n: ta.williams_r(n),
    'cci': lambda ta, n: ta.cci(n),
    'bbands': lambda ta, n, k: ta.bbands(n, k),                       # upper / middle / lower (population std)
    'keltner': lambda ta, n, atr_n, mult: ta.keltner(n, atr_n, mult),  # upper / middle / lower
    'donchian': lambda ta, n: ta.donchian(n),                         # upper / lower / middle
    'zscore': lambda ta, n: ta.zscore(n),                             # population std
    'zscore_sample': _zscore_sample,                                  # (zipline) sample std, ddof 1
    'mstd': lambda ta, n: ta.mstd(n),
    'momentum': lambda ta, n: ta.momentum(n),                         # C_t / C_t-n - 1
    'obv': lambda ta: ta.obv(),
    'obv_sma': _obv_sma,                                              # (zipline) SMA of OBV
    'vol': lambda ta, n, ppy: ta.vol(n, ppy),                         # annualised realised volatility
}


class Bars:
    """The bars of one run on the panel timeline (union of the symbols' session dates) and each
    symbol's memoised indicators (computed on the symbol's OWN bars, then mapped to the panel rows;
    rows where its market did not trade are NaN)."""

    def __init__(self, panel, frames: Dict[str, Any], symbols: List[str]):
        import pandas as pd
        import vbt_indicators as I
        self.panel = panel
        self.symbols = list(symbols)
        self.n, self.k = panel.n, len(self.symbols)
        self.tas = {}
        self.rows = {}
        self.identity = True
        for s in self.symbols:
            f = frames[s]
            self.tas[s] = I.TA(f['Open'].to_numpy(float) if 'Open' in f else None,
                               f['High'].to_numpy(float) if 'High' in f else None,
                               f['Low'].to_numpy(float) if 'Low' in f else None, f['Close'].to_numpy(float),
                               f['Volume'].to_numpy(float) if 'Volume' in f else None)
            idx = pd.DatetimeIndex(f.index)
            if idx.tz is not None:
                idx = idx.tz_localize(None)
            r = panel.dates.get_indexer(idx)
            self.rows[s] = r
            if not (len(r) == self.n and np.array_equal(r, np.arange(self.n))):
                self.identity = False
        self._lines: Dict[tuple, Indicator] = {}
        self._fields: Dict[str, List[List[float]]] = {}
        self._tradable: Optional[List[List[bool]]] = None
        self._masks: Dict[tuple, np.ndarray] = {}

    def array(self, name: str, args: tuple, output: Optional[str] = None, lag: int = 0) -> np.ndarray:
        """(n, k) panel-aligned array of one indicator line."""
        fn = INDICATORS.get(name)
        if fn is None:
            raise ValueError(f'Unknown indicator {name!r} (available: {", ".join(sorted(INDICATORS))})')
        out = np.full((self.n, self.k), np.nan)
        for j, s in enumerate(self.symbols):
            v = fn(self.tas[s], *args)
            if isinstance(v, dict):
                if output is None or output not in v:
                    raise ValueError(f'Indicator {name} has outputs {", ".join(v)}: name one with output=...')
                v = v[output]
            v = np.asarray(v, dtype=float)
            if lag:
                w = np.full(len(v), np.nan)
                if lag < len(v):
                    w[lag:] = v[:-lag]
                v = w
            r = self.rows[s]
            if self.identity:
                out[:, j] = v
            else:
                ok = r >= 0
                out[r[ok], j] = v[ok]
        return out

    def line(self, name: str, args: tuple, output: Optional[str] = None, lag: int = 0) -> Indicator:
        key = (name, tuple(args), output, int(lag))
        hit = self._lines.get(key)
        if hit is None:
            a = self.array(name, tuple(args), output, int(lag))
            hit = Indicator(key, [a[:, j].tolist() for j in range(self.k)])
            if len(self._lines) > 2048:
                self._lines.clear()
            self._lines[key] = hit
        return hit

    def field(self, name: str) -> List[List[float]]:
        hit = self._fields.get(name)
        if hit is None:
            p = self.panel
            arr = {'open': p.open, 'high': p.high, 'low': p.low, 'close': p.close, 'price': p.close,
                   'volume': p.volume}.get(name)
            if arr is None:
                raise ValueError(f'Unknown field {name!r} (fields: {", ".join(FIELDS)})')
            hit = [np.asarray(arr[:, j], dtype=float).tolist() for j in range(self.k)]
            self._fields[name] = hit
        return hit

    def tradable(self) -> List[List[bool]]:
        if self._tradable is None:
            t = self.panel.tradable & np.isfinite(self.panel.close)
            self._tradable = [t[:, j].tolist() for j in range(self.k)]
        return self._tradable

    def schedule_mask(self, rule: DateRule) -> np.ndarray:
        key = (rule.kind, rule.n)
        hit = self._masks.get(key)
        if hit is None:
            hit = rule.mask(self.panel.dates)
            self._masks[key] = hit
        return hit


# ════════════════════════════════════════════════════════════════════════════
# context / data
# ════════════════════════════════════════════════════════════════════════════

class BarData:
    """zipline's `data` for the current bar."""
    __slots__ = ('_i', '_bars', '_trad')

    def __init__(self, bars: Bars):
        self._i = 0
        self._bars = bars
        self._trad = None

    def current(self, asset: Asset, field):
        if type(field) is Indicator:
            return field.values[asset.sid][self._i]
        return self._bars.field(field)[asset.sid][self._i]

    def history(self, asset: Asset, field, bar_count: int, frequency: str = '1d') -> np.ndarray:
        """The last bar_count values of a field / indicator line up to and including this bar."""
        vals = field.values[asset.sid] if type(field) is Indicator else self._bars.field(field)[asset.sid]
        a = max(0, self._i - int(bar_count) + 1)
        return np.asarray(vals[a:self._i + 1], dtype=float)

    def can_trade(self, asset: Asset) -> bool:
        if self._trad is None:
            self._trad = self._bars.tradable()
        return self._trad[asset.sid][self._i]


class Context:
    """zipline's `context` (see the module doc for what is kept and what differs)."""

    def __init__(self, bars: Bars, params: Dict[str, Any]):
        self._bars = bars
        self._i = 0
        self.assets = [Asset(j, s) for j, s in enumerate(bars.symbols)]
        self.asset = self.assets[0]
        self.params = dict(params or {})
        self._targets = [[NAN] * bars.n for _ in range(bars.k)]
        self._scheduled: List[Tuple[Callable, DateRule]] = []
        self.orders = 0

    # --- data preparation (initialize) ---------------------------------------
    def indicator(self, name: str, *args, output: Optional[str] = None, lag: int = 0) -> Indicator:
        """A full-history indicator line for every asset (see INDICATORS). lag=1 = the previous bar's value
        (e.g. a channel of the PREVIOUS n bars)."""
        return self._bars.line(name, tuple(args), output, lag)

    def schedule_function(self, func: Callable, date_rule: Optional[DateRule] = None, time_rule: Any = None):
        """func(context, data) on the bars date_rule selects, before handle_data (time_rule is accepted for
        zipline compatibility; on bar data every decision is taken at the bar's close)."""
        self._scheduled.append((func, date_rule or date_rules.every_day()))

    # --- orders ----------------------------------------------------------------
    def order_target_percent(self, asset: Asset, target: float) -> None:
        t = float(target)
        if t != t or t in (math.inf, -math.inf):
            raise ValueError(f'order_target_percent({asset.symbol}, {target!r}): the target must be a finite number')
        self._targets[asset.sid][self._i] = t
        self.orders += 1

    # --- clock -----------------------------------------------------------------
    def get_datetime(self):
        return self._bars.panel.dates[self._i]


class Decisions:
    """The orders of one algorithm run: targets (n, k), NaN = no order."""
    __slots__ = ('targets', 'orders', 'scheduled', '_sig')

    def __init__(self, targets: np.ndarray, orders: int, scheduled: List[str]):
        self.targets = targets
        self.orders = orders
        self.scheduled = scheduled
        self._sig = None

    def signals(self) -> Tuple[np.ndarray, np.ndarray, Optional[np.ndarray]]:
        """(entries, exits, size multipliers or None) for base.simulator signals mode: target > 0 = be long,
        target <= 0 = be flat (a negative target is flat unless allowShort turns flat into short); the
        multipliers are the entry targets when any differs from 1."""
        if self._sig is None:
            T = self.targets
            ok = np.isfinite(T)
            E = ok & (T > 0)
            X = ok & (T <= 0)
            mult = None
            if E.any() and not np.all(T[E] == 1.0):
                mult = np.where(E, T, 0.0)
            self._sig = (E, X, mult)
        return self._sig

    def decision_bars(self) -> List[int]:
        return [int(i) for i in np.flatnonzero(np.isfinite(self.targets).any(axis=1))]


def run_algorithm(initialize: Callable, handle_data: Optional[Callable], bars: Bars,
                  params: Optional[Dict[str, Any]] = None) -> Decisions:
    """Call initialize(context) once, then for every bar the scheduled functions whose rule selects it
    and handle_data(context, data); return the recorded orders."""
    ctx = Context(bars, params or {})
    data = BarData(bars)
    if initialize is not None:
        initialize(ctx)
    sched = [(fn, bars.schedule_mask(rule)) for fn, rule in ctx._scheduled]
    n = bars.n
    if sched and handle_data is None and len(sched) == 1:
        fn, m = sched[0]
        for i in np.flatnonzero(m).tolist():
            ctx._i = data._i = i
            fn(ctx, data)
    elif sched:
        for i in range(n):
            ctx._i = data._i = i
            for fn, m in sched:
                if m[i]:
                    fn(ctx, data)
            if handle_data is not None:
                handle_data(ctx, data)
    elif handle_data is not None:
        for i in range(n):
            ctx._i = data._i = i
            handle_data(ctx, data)
    T = np.array(ctx._targets, dtype=float).T if bars.k else np.zeros((n, 0))
    return Decisions(T.reshape(n, bars.k), ctx.orders, [rule.describe() for _, rule in ctx._scheduled])
