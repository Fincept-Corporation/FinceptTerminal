"""
The backtesting.py side of the engine: the library loader, the Strategy that executes the
catalogue signals, the broker hooks and one run of the library.

WHAT RUNS. backtesting.py's own Backtest.run(): its bar loop, _Broker order matching (market
orders at the next open or - trade_on_close - at the close, contingent stop-loss / take-profit
orders against each bar's high / low with gap fills at the open and the stop checked before the
target), whole-share sizing from the available margin, margin = 1 / leverage, liquidation when
equity reaches 0, and the equity record per bar. The engine subclasses two library classes:

  FinceptBroker(_Broker)   - slippage: the library's spread hook (_adjusted_price) moves the fill
                             price of every ENTRY by the request's per-fill slippage (per-bar
                             fractions, so vol-scaled slippage works too); the same hook is applied
                             to every EXIT (the library itself only adjusts entries). Take-profit
                             fills are limit orders and pay no slippage.
                           - borrow fee for every night a short position is held (cost model's
                             annual rate, ACT/360 on the previous close), debited from cash and from
                             the bar's equity record.
                           - stop-loss / take-profit / trailing levels are set from the actual
                             entry FILL price (an order carries a placeholder SL so the library
                             re-checks its contingent orders inside the fill bar).
                           - a fill log (bar, quantity, price, price before slippage, commission,
                             exit reason) from which the trade list is rebuilt (open positions
                             included) - the library drops open trades from its statistics.
  FinceptBacktest(Backtest) - installs the broker and returns the raw run without the library's
                             statistics (base.metrics computes every figure; the library's own
                             summary is still produced for run_backtest under engineReported).

LOADING. backtesting's plotting module (bokeh, ~0.8-1.3 s to import) is replaced by a stub that
raises if called - the engine never plots; tqdm progress bars are switched off (progress goes to
@@BT_PROGRESS lines instead).
"""

from __future__ import annotations

import functools
import math
import sys
import types
import warnings
from dataclasses import dataclass, field
from math import copysign
from typing import Any, Dict, List, Optional

import numpy as np

REASON_SIGNAL, REASON_SL, REASON_TP, REASON_TRAIL = 0, 1, 2, 3
REASON_MARGIN = 98
PHASE_OPEN, PHASE_INTRABAR, PHASE_CLOSE = 0, 1, 2
_FULL = 1.0 - sys.float_info.epsilon        # backtesting.py's "all available equity" order size
DUMMY_SL_LONG = 1e-9                        # placeholder stop levels (replaced at the fill)
DUMMY_SL_SHORT = 1e12


def _load():
    """Import backtesting with plotting stubbed out (only when nothing imported it yet)."""
    if 'backtesting' not in sys.modules and 'backtesting._plotting' not in sys.modules:
        stub = types.ModuleType('backtesting._plotting')

        def _no_plot(*_a, **_k):
            raise RuntimeError('Plotting is not available in the Fincept backtesting engine.')
        stub.plot = stub.plot_heatmaps = stub.set_bokeh_output = _no_plot
        stub.__fincept_stub__ = True
        sys.modules['backtesting._plotting'] = stub
    import backtesting
    import backtesting.backtesting as BB
    import backtesting._stats as BS
    BB._tqdm = lambda seq, **_kw: seq          # no tqdm bars on stderr
    return types.SimpleNamespace(module=backtesting, BB=BB, compute_stats=BS.compute_stats,
                                 Backtest=BB.Backtest, Strategy=BB.Strategy, Broker=BB._Broker,
                                 OutOfMoney=BB._OutOfMoneyError, version=str(getattr(backtesting, '__version__', '')))


L = _load()
LIB_VERSION = L.version


def library_ok() -> Optional[str]:
    """None when the installed backtesting.py has every hook the engine uses, else the reason."""
    for name in ('_adjusted_price', '_open_trade', '_close_trade', '_process_orders', 'next'):
        if not callable(getattr(L.Broker, name, None)):
            return f'backtesting.py {LIB_VERSION} has no _Broker.{name}'
    return None


# ════════════════════════════════════════════════════════════════════════════
# Broker and Backtest
# ════════════════════════════════════════════════════════════════════════════

class FinceptBroker(L.Broker):
    def __init__(self, *, fx: Dict[str, Any], **kw):
        super().__init__(**kw)
        n = len(kw['index'])
        self._slip = fx.get('slip')
        self._borrow_rate = float(fx.get('borrow_rate') or 0.0)
        self._dt = fx.get('dt')
        self._sl_pct = float(fx.get('sl') or 0.0)
        self._tp_pct = float(fx.get('tp') or 0.0)
        self._ts_pct = float(fx.get('ts') or 0.0)
        self._i = 0
        self._last_adj = None
        self.fill_log: List[tuple] = []        # (bar, qty, price, ref, commission, reason, phase, deferred)
        self._qty_by_bar = np.zeros(n)          # net quantity filled per bar (fill bar = time index)
        self._pos_prev = 0.0                    # net position at the close of the previous bar
        self.borrow_bar = np.zeros(n)
        self.trailed: set = set()
        self.margin_call_bar: Optional[int] = None

    # slippage: the library's spread hook, per bar, applied to entries (library) and exits (below)
    def _adjusted_price(self, size=None, price=None) -> float:
        raw = price or self.last_price
        s = float(self._slip[self._i]) if self._slip is not None else self._spread
        adj = raw * (1 + copysign(s, size))
        self._last_adj = (raw, adj)
        return adj

    def next(self):
        i = self._i = len(self._data) - 1
        try:
            super().next()
        except L.OutOfMoney:
            self.margin_call_bar = i
            raise
        if i > 0:
            # Borrow for the night (i-1, i] on the short position held at the close of bar i-1. It is
            # known only now: fills at bar i-1's close (trade_on_close) are processed by the library
            # at the start of bar i. Debited from cash and from the equity just recorded for bar i.
            self._pos_prev += self._qty_by_bar[i - 1]
            if self._borrow_rate > 0 and self._pos_prev < -1e-12:
                dt = float(self._dt[i]) if self._dt is not None else 1.0 / 360.0
                fee = -self._pos_prev * float(self._data.Close[-2]) * self._borrow_rate * dt
                self._cash -= fee
                self._equity[i] -= fee
                self.borrow_bar[i] += fee

    def _levels(self, price: float, size: float, tag):
        long = size > 0
        cands = []
        if self._sl_pct > 0:
            cands.append((price * (1 - self._sl_pct) if long else price * (1 + self._sl_pct), False))
        if self._ts_pct > 0:
            cands.append((price * (1 - self._ts_pct) if long else price * (1 + self._ts_pct), True))
        if isinstance(tag, tuple) and len(tag) == 2:
            sd, td = tag
            if math.isfinite(sd) and sd > 0:
                cands.append((price - sd if long else price + sd, False))
            if math.isfinite(td) and td > 0:
                cands.append((price - td if long else price + td, True))
        cands = [c for c in cands if c[0] > 0]
        sl, trailed = None, False
        if cands:
            sl, trailed = (max(cands) if long else min(cands))
        tp = None
        if self._tp_pct > 0:
            tp = price * (1 + self._tp_pct) if long else price * (1 - self._tp_pct)
            if not tp > 0:
                tp = None
        return sl, tp, trailed

    def _open_trade(self, price, size, sl, tp, time_index, tag):
        la = self._last_adj
        raw = la[0] if (la is not None and la[1] == price) else price
        comm = float(self._commission(size, price))
        deferred = time_index < self._i
        trailed = False
        if sl is not None and (sl == DUMMY_SL_LONG or sl == DUMMY_SL_SHORT):
            sl, tp2, trailed = self._levels(price, size, tag)
            tp = tp if tp is not None else tp2
        self.fill_log.append((int(time_index), float(size), float(price), float(raw), comm, REASON_SIGNAL,
                              PHASE_CLOSE if deferred else PHASE_OPEN, deferred))
        self._qty_by_bar[time_index] += size
        super()._open_trade(price, size, sl, tp, time_index, tag)
        if trailed and self.trades:
            self.trailed.add(id(self.trades[-1]))

    def _exit_reason(self, trade, price) -> int:
        rec = self._equity[self._i]
        if rec == rec and rec <= 0:                        # equity recorded <= 0: the library liquidates
            return REASON_MARGIN
        slo = trade._sl_order
        if slo is not None and slo.stop is None:          # the stop order triggered on this bar
            return REASON_TRAIL if id(trade) in self.trailed else REASON_SL
        for o in self.orders:
            if o.parent_trade is trade and not o.is_contingent:
                return REASON_SIGNAL                        # the strategy's position.close()
        tpo = trade._tp_order
        if tpo is not None and tpo.limit is not None:
            if (trade.size > 0 and price >= tpo.limit) or (trade.size < 0 and price <= tpo.limit):
                return REASON_TP
        return REASON_SIGNAL

    def _close_trade(self, trade, price, time_index):
        reason = self._exit_reason(trade, price)
        px = price if reason == REASON_TP else self._adjusted_price(-trade.size, price)
        comm = float(self._commission(trade.size, px))
        deferred = time_index < self._i
        phase = PHASE_INTRABAR if reason in (REASON_SL, REASON_TP, REASON_TRAIL) else \
            (PHASE_CLOSE if (deferred or reason == REASON_MARGIN) else PHASE_OPEN)
        self.fill_log.append((int(time_index), float(-trade.size), float(px), float(price), comm, reason, phase,
                              deferred))
        self._qty_by_bar[time_index] -= trade.size
        super()._close_trade(trade, px, time_index)


@dataclass
class RawRun:
    strategy: Any
    broker: Any
    lib_warnings: List[str] = field(default_factory=list)


class FinceptBacktest(L.Backtest):
    """backtesting.py's Backtest with FinceptBroker installed; run_raw() skips the library's
    statistics (base computes them) and returns the strategy / broker of the run."""

    def __init__(self, data, strategy, *, fx: Optional[Dict[str, Any]] = None, **kw):
        with warnings.catch_warnings():
            warnings.simplefilter('ignore')
            super().__init__(data, strategy, **kw)
        base = self._broker
        if not isinstance(base, functools.partial):
            raise RuntimeError(f'backtesting.py {LIB_VERSION}: unexpected Backtest internals (broker factory)')
        self._broker = functools.partial(FinceptBroker, fx=dict(fx or {}), **base.keywords)

    def run_raw(self, record_warnings: bool = False, **params) -> RawRun:
        holder: Dict[str, Any] = {}

        def capture(*, trades=None, equity=None, ohlc_data=None, strategy_instance=None, **_kw):
            holder['strategy'] = strategy_instance

        BB = L.BB
        orig = BB.compute_stats
        BB.compute_stats = capture
        caught: List[Any] = []
        try:
            with warnings.catch_warnings(record=True) as w:
                warnings.simplefilter('always' if record_warnings else 'ignore')
                L.Backtest.run(self, **params)
                caught = list(w or [])
        finally:
            BB.compute_stats = orig
        st = holder['strategy']
        return RawRun(strategy=st, broker=st._broker, lib_warnings=[str(x.message) for x in caught])


def library_stats(raw: RawRun, frame) -> Dict[str, Any]:
    """backtesting.py's own statistics of the run (closed trades only, its own Sharpe / volatility
    definitions, risk-free 0) - reported under engineReported.library for comparison only."""
    import pandas as pd
    br = raw.broker
    eq = pd.Series(br._equity).bfill().fillna(br._cash).values
    with warnings.catch_warnings(), np.errstate(all='ignore'):
        warnings.simplefilter('ignore')
        st = L.compute_stats(trades=br.closed_trades, equity=eq, ohlc_data=frame, risk_free_rate=0.0,
                             strategy_instance=raw.strategy)
    keys = {'Return [%]': ('totalReturnPct', 1), 'Return (Ann.) [%]': ('annualReturnPct', 1),
            'CAGR [%]': ('cagrPct', 1), 'Volatility (Ann.) [%]': ('volatilityPct', 1),
            'Sharpe Ratio': ('sharpeRatio', 1), 'Sortino Ratio': ('sortinoRatio', 1),
            'Calmar Ratio': ('calmarRatio', 1), 'Max. Drawdown [%]': ('maxDrawdownPct', 1),
            '# Trades': ('closedTrades', 1), 'Win Rate [%]': ('winRatePct', 1),
            'Profit Factor': ('profitFactor', 1), 'Expectancy [%]': ('expectancyPct', 1), 'SQN': ('sqn', 1),
            'Kelly Criterion': ('kellyCriterion', 1), 'Exposure Time [%]': ('exposureTimePct', 1),
            'Commissions [$]': ('commissions', 1), 'Equity Final [$]': ('equityFinal', 1)}
    out: Dict[str, Any] = {}
    for k, (name, _s) in keys.items():
        try:
            v = float(st.get(k))
            out[name] = v if math.isfinite(v) else None
        except (TypeError, ValueError):
            continue
    out['note'] = ('backtesting.py ' + LIB_VERSION + "'s own summary: closed trades only (an open position is "
                   'excluded), its own volatility / Sharpe / Sortino formulas, risk-free 0. The headline metrics '
                   'come from base.metrics on the same equity curve.')
    return out


# ════════════════════════════════════════════════════════════════════════════
# The Strategy that executes the catalogue signals
# ════════════════════════════════════════════════════════════════════════════

def kelly_fraction(rets: List[float], cap: float, min_trades: int) -> float:
    """Kelly fraction of closed-trade returns (losses = returns <= 0); the cap until min_trades
    trades have closed, 0 without a winner, the cap without a loser."""
    if len(rets) < min_trades:
        return cap
    r = np.asarray(rets, dtype=float)
    wins, losses = r[r > 0], r[r <= 0]
    if len(wins) == 0:
        return 0.0
    if len(losses) == 0 or abs(losses.mean()) < 1e-12:
        return cap
    w = len(wins) / len(r)
    payoff = wins.mean() / abs(losses.mean())
    return float(min(cap, max(0.0, w - (1.0 - w) / payoff)))


class SignalExecutor(L.Strategy):
    """Trades precomputed signals (btp_strategies) with backtesting.py's order API. Run parameters
    (Backtest.run keyword arguments - the library's parameter mechanism):
      sig   {'e', 'x', 'f', 'act', 'sl', 'tr'}: arrays over the run's bars (decision bars)
      plan  {'lag': 0 | 1, 'allow_short', 'size_mode', 'size_value', 'vol', 'leverage', 'kelly_min',
             'stops', 'trail', 'hook'}
    lag 1 (next_close fills) acts on bar d's signal at bar d+1's close with trade_on_close, so the
    order fills at bar d+1's close; lag 0 acts on the bar's own signal."""
    sig = None
    plan = None

    def init(self):
        s, p = self.sig, self.plan or {}
        self._e, self._x, self._f = s['e'], s['x'], s.get('f')
        self._sld, self._trd, self._act = s.get('sl'), s.get('tr'), s['act']
        self._lag = int(p.get('lag', 0))
        self._short = bool(p.get('allow_short'))
        self._hook = p.get('hook')
        self._stops = bool(p.get('stops')) or self._sld is not None or self._trd is not None
        self._trail_pct = float(p.get('trail') or 0.0)
        self._trail_on = self._trail_pct > 0 or self._trd is not None
        self._mode = p.get('size_mode', 'percent')
        self._sv = float(p.get('size_value', 1.0))
        self._vol = p.get('vol')
        self._lev = float(p.get('leverage', 1.0) or 1.0)
        self._kmin = int(p.get('kelly_min', 10))
        self._krets: List[float] = []
        self._kseen = 0
        self._ext: Dict[int, float] = {}
        # The library starts its loop at bar 1, so a decision on bar 0 is placed here, before the
        # loop; it fills like any other order (bar 1's open, or bar 0's close with trade_on_close).
        if self._lag == 0 and len(self._act) and self._act[0]:
            self._decide(0)

    def next(self):
        i = len(self._data) - 1
        if self._hook is not None and not (i & 255):
            self._hook(i)
        if self._trail_on and self._broker.trades:
            self._trail(i)
        d = i - self._lag
        if d >= 0 and self._act[d]:
            self._decide(d)

    def _decide(self, d: int) -> None:
        e = bool(self._e[d])
        x = bool(self._x[d])
        f = bool(self._f[d]) if self._f is not None else False
        pos = self.position.size
        if pos > 0:
            if x:
                self.position.close()
                if self._short and not e:
                    self._open(-1, d)
            elif f:
                self.position.close()
        elif pos < 0:
            if e:
                self.position.close()
                if not x:
                    self._open(1, d)
            elif f:
                self.position.close()
        else:
            if e and not x:
                self._open(1, d)
            elif x and not e and self._short:
                self._open(-1, d)

    def _size(self, d: int) -> float:
        m = self._mode
        if m == 'fixed':
            eq = self.equity
            f = self._sv / (eq * self._lev) if eq > 0 else 0.0
        elif m == 'vol_target':
            f = float(self._vol[d]) if self._vol is not None else 0.0
        elif m == 'kelly':
            ct = self._broker.closed_trades
            while self._kseen < len(ct) and ct[self._kseen].exit_bar <= d:
                t = ct[self._kseen]
                basis = abs(t.size) * t.entry_price
                self._krets.append(t.pl / basis if basis > 0 else 0.0)
                self._kseen += 1
            f = kelly_fraction(self._krets, self._sv, self._kmin)
        else:                                   # percent / risk: a fraction of equity x leverage
            f = self._sv
        if not (f > 0) or not math.isfinite(f):
            return 0.0
        return min(f, _FULL)

    def _open(self, side: int, d: int) -> None:
        f = self._size(d)
        if f <= 0:
            return
        kw: Dict[str, Any] = {'size': f}
        if self._sld is not None or self._trd is not None:
            kw['tag'] = (float(self._sld[d]) if self._sld is not None else float('nan'),
                         float(self._trd[d]) if self._trd is not None else float('nan'))
        if self._stops:
            kw['sl'] = DUMMY_SL_LONG if side > 0 else DUMMY_SL_SHORT
        (self.buy if side > 0 else self.sell)(**kw)

    def _trail(self, i: int) -> None:
        """Ratchet each open trade's stop at this close: the request's trailing % from the best
        price since entry (highs / lows), or the strategy's ATR distance from the close."""
        data = self._data
        hi, lo, c = float(data.High[-1]), float(data.Low[-1]), float(data.Close[-1])
        dist = float(self._trd[i]) if self._trd is not None else float('nan')
        br = self._broker
        for t in list(br.trades):
            key = id(t)
            cur = t.sl
            cands = []
            if t.size > 0:
                if self._trail_pct > 0:
                    pk = max(self._ext.get(key, t.entry_price), hi)
                    self._ext[key] = pk
                    cands.append(pk * (1.0 - self._trail_pct))
                if math.isfinite(dist):
                    cands.append(c - dist)
                lvl = max(cands) if cands else 0.0
                better = cur is None or lvl > cur
            else:
                if self._trail_pct > 0:
                    tr = min(self._ext.get(key, t.entry_price), lo)
                    self._ext[key] = tr
                    cands.append(tr * (1.0 + self._trail_pct))
                if math.isfinite(dist):
                    cands.append(c + dist)
                lvl = min(cands) if cands else 0.0
                better = cur is None or lvl < cur
            if lvl > 0 and better:
                t.sl = lvl
                br.trailed.add(key)


# ════════════════════════════════════════════════════════════════════════════
# Custom strategy code (a backtesting.py Strategy subclass written by the user)
# ════════════════════════════════════════════════════════════════════════════

_FORBIDDEN_MODULES = {'os', 'sys', 'subprocess', 'shutil', 'socket', 'http', 'urllib', 'requests', 'ftplib',
                      'smtplib', 'telnetlib', 'ctypes', 'multiprocessing', 'threading', 'signal', 'importlib',
                      'builtins', '__builtin__', 'code', 'codeop', 'compile', 'compileall', 'py_compile',
                      'zipimport', 'pkgutil', 'modulefinder', 'runpy', 'pathlib', 'glob', 'tempfile', 'io',
                      'pickle', 'shelve', 'marshal', 'dbm', 'sqlite3', 'webbrowser', 'cmd', 'pdb'}
_FORBIDDEN_CALLS = {'exec', 'eval', 'compile', '__import__', 'open', 'input', 'breakpoint', 'exit', 'quit',
                    'globals', 'locals', 'vars', 'dir', 'delattr'}
_FORBIDDEN_ATTRS = {'__class__', '__subclasses__', '__bases__', '__mro__', '__globals__', '__code__', '__closure__',
                    '__func__', '__self__', '__module__', '__import__', '__builtins__', '__qualname__', '__wrapped__'}
_SAFE_BUILTINS = ('range', 'len', 'abs', 'min', 'max', 'sum', 'round', 'int', 'float', 'bool', 'str', 'list', 'dict',
                  'tuple', 'enumerate', 'zip', 'map', 'filter', 'sorted', 'reversed', 'isinstance', 'issubclass',
                  'hasattr', 'getattr', 'setattr', 'property', 'staticmethod', 'classmethod', 'super', 'type',
                  'any', 'all', 'ValueError', 'Exception', '__build_class__')


def validate_code(code: str) -> Optional[str]:
    """Reason the custom code is rejected, or None (AST screen: no imports of system modules, no
    exec / eval / open, no dunder escapes, at most 10,000 characters)."""
    import ast
    if len(code) > 10000:
        return 'Strategy code too long (max 10000 characters)'
    try:
        tree = ast.parse(code)
    except SyntaxError as e:
        return f'Syntax error in strategy code: {e}'
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for a in node.names:
                if a.name.split('.')[0] in _FORBIDDEN_MODULES:
                    return f'Forbidden import: {a.name}'
        if isinstance(node, ast.ImportFrom) and node.module and node.module.split('.')[0] in _FORBIDDEN_MODULES:
            return f'Forbidden import from: {node.module}'
        if isinstance(node, ast.Call):
            fn = node.func
            name = fn.id if isinstance(fn, ast.Name) else (fn.attr if isinstance(fn, ast.Attribute) else None)
            if name in _FORBIDDEN_CALLS:
                return f'Forbidden function call: {name}'
        if isinstance(node, ast.Attribute) and node.attr in _FORBIDDEN_ATTRS:
            return f'Forbidden attribute access: {node.attr}'
    return None


_CODE_CACHE: Dict[str, Any] = {}


def custom_strategy_class(code: str):
    """The backtesting.py Strategy subclass defined by `code` (screened, restricted builtins; runs
    as the local user). Cached per process."""
    hit = _CODE_CACHE.get(code)
    if hit is not None:
        return hit
    import builtins
    import pandas as pd
    from backtesting.lib import crossover
    err = validate_code(code)
    if err:
        raise ValueError(f'Strategy code rejected: {err}')
    safe = {k: getattr(builtins, k) for k in _SAFE_BUILTINS}
    safe.update({'True': True, 'False': False, 'None': None})
    g = {'__builtins__': safe, '__name__': 'fincept_custom_strategy', 'Strategy': L.Strategy,
         'crossover': crossover, 'pd': pd, 'np': np}
    try:
        exec(code, g)  # noqa: S102 - screened above
    except Exception as e:
        raise ValueError(f'Custom strategy code failed to load: {e}')
    for obj in g.values():
        if isinstance(obj, type) and issubclass(obj, L.Strategy) and obj is not L.Strategy:
            _CODE_CACHE[code] = obj
            return obj
    raise ValueError('Custom strategy code defines no backtesting.py Strategy subclass.')


# ════════════════════════════════════════════════════════════════════════════
# One run -> curves and fills
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class RunOut:
    equity: np.ndarray
    fills: Dict[str, np.ndarray]
    positions: np.ndarray
    borrow: np.ndarray
    raw: RawRun
    deferred_costs: float = 0.0
    margin_call_bar: Optional[int] = None


def collect(raw: RawRun, initial_capital: float) -> RunOut:
    """Equity per bar, the fill arrays (base.execution format) and positions from a raw run.

    Equity is the library's per-bar record. Bars before its loop started (bar 0) hold the initial
    cash. Fills at a bar's CLOSE (trade_on_close) are processed by the library at the next bar,
    after it recorded that bar's equity; their commission and slippage are booked on their own
    fill bar here, so the curve matches the fill timing (the position itself is marked at that
    close either way)."""
    br = raw.broker
    eq = np.array(br._equity, dtype=float)
    n = len(eq)
    ok = np.isfinite(eq)
    if not ok.any():
        eq[:] = initial_capital
    else:
        first = int(np.argmax(ok))
        eq[:first] = initial_capital
        bad = ~np.isfinite(eq)
        if bad.any():                          # defensive: carry the last value
            for k in np.flatnonzero(bad):
                eq[k] = eq[k - 1] if k > 0 else initial_capital
    log = br.fill_log
    m = len(log)
    bar = np.fromiter((r[0] for r in log), dtype=np.int64, count=m)
    qty = np.fromiter((r[1] for r in log), dtype=float, count=m)
    px = np.fromiter((r[2] for r in log), dtype=float, count=m)
    ref = np.fromiter((r[3] for r in log), dtype=float, count=m)
    comm = np.fromiter((r[4] for r in log), dtype=float, count=m)
    reason = np.fromiter((r[5] for r in log), dtype=np.int64, count=m)
    phase = np.fromiter((r[6] for r in log), dtype=np.int64, count=m)
    deferred = np.fromiter((r[7] for r in log), dtype=bool, count=m)
    dcost = 0.0
    for k in np.flatnonzero(deferred):
        c = comm[k] + abs(qty[k]) * abs(px[k] - ref[k])
        if 0 <= bar[k] < n:
            eq[bar[k]] -= c
            dcost += c
    pos = np.zeros(n)
    if m:
        np.add.at(pos, np.clip(bar, 0, n - 1), qty)
        pos = np.cumsum(pos)
        pos[np.abs(pos) < 1e-9] = 0.0
    fills = {'bar': bar, 'asset': np.zeros(m, dtype=np.int64), 'qty': qty, 'price': px, 'ref_price': ref,
             'commission': comm, 'reason': reason, 'phase': phase}
    return RunOut(equity=eq, fills=fills, positions=pos, borrow=np.array(br.borrow_bar, dtype=float), raw=raw,
                  deferred_costs=dcost, margin_call_bar=br.margin_call_bar)


def returns_of(eq: np.ndarray) -> np.ndarray:
    r = np.zeros(len(eq))
    with np.errstate(divide='ignore', invalid='ignore'):
        r[1:] = np.where(eq[:-1] > 0, eq[1:] / eq[:-1] - 1.0, 0.0)
    return r


def round_trip_extras(fills: Dict[str, np.ndarray]) -> Dict[str, Any]:
    """Closed round trips' count / win rate / profit factor / SQN from the fill arrays (one position
    at a time: an entry and its exit; open positions are skipped)."""
    q, p, c = fills['qty'], fills['price'], fills['commission']
    pnl = []
    open_q, open_cash = 0.0, 0.0
    for k in range(len(q)):
        cf = -q[k] * p[k] - c[k]
        if open_q == 0.0:
            open_q, open_cash = q[k], cf
            continue
        if abs(open_q + q[k]) <= 1e-9 * max(1.0, abs(q[k])):
            pnl.append(open_cash + cf)
            open_q, open_cash = 0.0, 0.0
        else:
            open_q += q[k]
            open_cash += cf
    n = len(pnl)
    if n == 0:
        return {'trades': 0, 'winRate': None, 'profitFactor': None, 'sqn': None}
    a = np.asarray(pnl)
    wins, losses = a[a > 0], a[a < 0]
    pf = float(wins.sum() / -losses.sum()) if len(losses) and losses.sum() < 0 else None
    sd = float(np.std(a, ddof=1)) if n > 1 else 0.0
    return {'trades': int(n), 'winRate': float(len(wins) / n), 'profitFactor': pf,
            'sqn': float(math.sqrt(n) * a.mean() / sd) if sd > 0 else None}
