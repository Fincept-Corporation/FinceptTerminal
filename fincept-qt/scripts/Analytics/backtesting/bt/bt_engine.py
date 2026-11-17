"""
The "bt" engine's backtest core, rebased on the shared backtesting core (base/).

WHAT RUNS (per strategy kind - every result states it in engine / engineImplementation /
assumptions / provenance)
  Bars         base.price_store (one on-disk cache, exchange session dates; zero downloads warm),
               or broker candles the host injected (brokerCandles) when they cover every symbol.
               Several symbols are aligned on the union of their session dates (base.execution.
               align_panel): a closed market's price is carried forward for valuation only and a
               CALENDAR_MISMATCH warning says so ([BTC-USD, SPY] and [SPY, RELIANCE.NS] run; v1
               refused them after a UTC shift - audit T1.1 / T1.2).
  signals      bt_strategies rules on each symbol's own bars -> base.simulator signals mode (one
               portfolio, shared cash; an entry takes the symbol's slot = equity / symbols x the
               percent size; intrabar stop-loss / take-profit / trailing stop; shorts on request).
               The bt library is NOT used for these and the identity says "base.simulator".
  allocation   the bt library's algo stack decides the target weights (bt_algos.decide: bt's
               schedule / selection / weighting algos + bar-lookback algos calling ffn), then
                 fillTiming next_open (default) / same_close -> base.simulator weights mode;
                 fillTiming next_close -> bt.Backtest executes them natively (bt's accounting,
                 fills at the close, whole shares unless fractionalShares) - bt's own convention.
               (request btExecution "simulator" keeps next_close on base.simulator; a calendar
               mismatch also does, because bt would trade a closed market at its last close.)
  Trades       signals: round trips. allocation: POSITION EPISODES (base.execution.
               position_episodes): one trade per symbol position from its first buy out of flat
               until it is flat again - rebalancing trims stay inside the episode (v1 listed FIFO
               fragments: 363 "trades", 91.8 % win rate for monthly equal weight, audit T2.1).
               Rebalance count, turnover (costs.annualTurnover) and attribution[] describe the
               rebalancing.
  Costs        base.costs.CostModel.from_request: commission (pct / per share / per trade, minimum,
               maximum) and slippage itemised per fill / trade / total (bt's native path books
               both through bt's cost hook and the engine itemises them again).
  Results      base.result_normalizer.build_backtest_result (every headline metric from base.metrics,
               the request's / ^IRX risk-free rate; equity from the first bar, flat in warm-up).
"""

from __future__ import annotations

import math
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

import bt_strategies as ST

ENGINE = 'bt'
ENGINE_VERSION = '2.0.0'
SIZING_MODES = ('percent',)


class EngineInputError(ValueError):
    """A request the engine cannot run (plain sentence for the user)."""


class EngineDataError(RuntimeError):
    """Market data could not be loaded (plain sentence, DATA error code)."""


def _log(msg: str) -> None:
    import sys
    print(f'[bt] {msg}', file=sys.stderr, flush=True)


def lib_version() -> Optional[str]:
    from bt_data import library_version
    return library_version()


def implementation(kind: str, executor: str = 'simulator', backend: str = 'python', stack_text: str = '',
                   fill: str = 'next_open', fractional: bool = False) -> str:
    """engineImplementation text: which code decided and which executed."""
    if kind == 'signals':
        k = 'numba' if backend == 'numba' else 'Python kernel'
        return f'base.simulator (signals mode, {k}) - the bt library is not used for signal strategies'
    v = lib_version() or '?'
    st = f' ({stack_text})' if stack_text else ''
    if executor == 'bt':
        return (f'bt library {v}: its algo stack decides the target weights{st} and bt.Backtest executes them '
                f'(native accounting, fills at the next bar\'s close, {"fractional" if fractional else "whole"} '
                f'shares)')
    k = 'numba' if backend == 'numba' else 'Python kernel'
    return (f'bt library {v} algo stack decides the target weights{st}; base.simulator executes them (weights '
            f'mode, {fill}, {k})')


def short_implementation() -> str:
    v = lib_version() or 'not installed'
    return (f'bt engine: bt library {v} algo stack for allocation strategies (executed by base.simulator, or by bt '
            f'itself at next_close); base.simulator for signal strategies')


# ════════════════════════════════════════════════════════════════════════════
# Request
# ════════════════════════════════════════════════════════════════════════════

def _num(v, default=None):
    try:
        x = float(v)
        return x if math.isfinite(x) else default
    except (TypeError, ValueError):
        return default


def _frac(req, key) -> float:
    v = _num(req.get(key), 0.0) or 0.0
    return v if v > 0 else 0.0


@dataclass
class Spec:
    symbols: List[str]
    strategy: str
    params: Dict[str, Any]
    kind: str
    start: str
    end: str
    interval: str
    initial_capital: float
    fill: str
    costs: Any
    fraction: float
    stop_loss: float
    take_profit: float
    trailing_stop: float
    allow_short: bool
    fractional: bool
    execution: str = 'auto'
    ignored: List[str] = field(default_factory=list)
    notes: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        from dataclasses import asdict
        d = {k: getattr(self, k) for k in self.__dataclass_fields__ if k != 'costs'}
        d['costs'] = asdict(self.costs)
        return d


def spec_from_dict(d: Dict[str, Any]) -> Spec:
    from base.costs import CostModel
    kw = dict(d)
    kw['costs'] = CostModel(**d['costs'])
    return Spec(**kw)


def parse_request(req: Dict[str, Any]) -> Spec:
    from base.costs import CostModel
    from base.execution import fill_mode_from_request
    from base.price_store import normalize_symbol
    symbols = req.get('symbols') or [a.get('symbol') for a in (req.get('assets') or []) if isinstance(a, dict)]
    if isinstance(symbols, str):
        symbols = [s.strip() for s in symbols.split(',')]
    syms: List[str] = []
    for s in symbols or []:
        ns = normalize_symbol(s)
        if ns and ns not in syms:
            syms.append(ns)
    if not syms:
        raise EngineInputError('No symbols provided.')
    strat = req.get('strategy') if isinstance(req.get('strategy'), dict) else {}
    try:
        sid = ST.resolve(strat.get('type') or 'sma_crossover')
    except ValueError as e:
        raise EngineInputError(str(e))
    kind = ST.kind_of(sid)
    given = strat.get('params') if isinstance(strat.get('params'), dict) else (
        strat.get('parameters') if isinstance(strat.get('parameters'), dict) else {})
    params = ST.defaults(sid)
    params.update({k: v for k, v in (given or {}).items() if v is not None})
    try:
        if kind == 'allocation':
            ST.allocation_stack(sid, params)
        else:   # signal parameters are validated when the rule is built; check they are numbers here
            for key, v in params.items():
                if not isinstance(v, (int, float)) or isinstance(v, bool):
                    float(v)
    except (TypeError, ValueError) as e:
        raise EngineInputError(f'{sid}: {e}')
    try:
        fill = fill_mode_from_request(req)
    except ValueError as e:
        raise EngineInputError(str(e))
    try:
        costs = CostModel.from_request(req, symbol=syms[0])
    except ValueError as e:
        raise EngineInputError(str(e))
    cap = _num(req.get('initialCapital'), 100000.0) or 100000.0
    if cap <= 0:
        raise EngineInputError('Initial capital must be positive.')
    mode = req.get('positionSizing', 'percent')
    if isinstance(mode, dict):
        mode = mode.get('mode', 'percent')
    mode = str(mode or 'percent').strip().lower()
    value = _num(req.get('positionSizeValue'), 100.0)
    ignored: List[str] = []
    fraction = 1.0
    if mode in ('percent', 'pct', 'percentage'):
        if value is not None and value > 0:
            fraction = min(value, 100.0) / 100.0
    else:
        ignored.append(f"position sizing '{mode}' (this engine sizes by percent of each symbol's slot / of the "
                       f"target weights)")
    sl, tp, ts = _frac(req, 'stopLoss'), _frac(req, 'takeProfit'), _frac(req, 'trailingStop')
    allow_short = bool(req.get('allowShort', False))
    if kind == 'allocation':
        if sl or tp or ts:
            ignored.append('stop-loss / take-profit / trailing stop (not applied to an allocation strategy: bt '
                           'rebalances to its target weights)')
            sl = tp = ts = 0.0
        if allow_short:
            ignored.append('short selling (the allocation strategies hold long target weights)')
            allow_short = False
    lev = _num(req.get('leverage'), 1.0) or 1.0
    if lev != 1.0:
        ignored.append('leverage (positions are capped at available cash; no margin is modelled)')
    execution = str(req.get('btExecution') or 'auto').strip().lower()
    if execution not in ('auto', 'simulator', 'bt'):
        raise EngineInputError('btExecution must be "auto", "simulator" or "bt".')
    spec = Spec(symbols=syms, strategy=sid, params=params, kind=kind,
                start=str(req.get('startDate') or '')[:10], end=str(req.get('endDate') or '')[:10],
                interval=str(req.get('interval') or '1d'), initial_capital=float(cap), fill=fill, costs=costs,
                fraction=fraction, stop_loss=sl, take_profit=tp, trailing_stop=ts, allow_short=allow_short,
                fractional=bool(req.get('fractionalShares', False)), execution=execution, ignored=ignored)
    if not spec.start or not spec.end:
        raise EngineInputError('startDate and endDate are required.')
    return spec


# ════════════════════════════════════════════════════════════════════════════
# Data
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class Data:
    frames: Dict[str, Any]
    symbols: List[str]
    provenance: Dict[str, Dict[str, Any]]
    warnings: List[Dict[str, Any]]
    synthetic: bool = False
    source: str = 'yahoo'
    network_calls: int = 0


def _broker_frames(candles: Dict[str, Any], symbols: List[str]) -> Optional[Dict[str, Any]]:
    """Frames from broker candles the host injected - only when they cover EVERY symbol."""
    import pandas as pd
    if not isinstance(candles, dict) or not candles:
        return None

    def canon(s):
        s = str(s or '').strip().upper()
        if ':' in s:
            s = s.split(':', 1)[1] or s
        for suf in ('.NS', '.BO'):
            if s.endswith(suf):
                s = s[:-len(suf)]
        return s
    by = {canon(k): v for k, v in candles.items()}
    out = {}
    for sym in symbols:
        rows = by.get(canon(sym))
        if not rows:
            return None
        idx = pd.to_datetime([int(r['t']) for r in rows], unit='s')
        f = pd.DataFrame({'Open': [float(r.get('o', r['c'])) for r in rows],
                          'High': [float(r.get('h', r['c'])) for r in rows],
                          'Low': [float(r.get('l', r['c'])) for r in rows],
                          'Close': [float(r['c']) for r in rows],
                          'Volume': [float(r.get('v', 0) or 0) for r in rows]}, index=idx).sort_index()
        f = f[~f.index.duplicated(keep='last')]
        if len(f) < 5:
            return None
        out[sym] = f
    return out


def synthetic_frame(symbol: str, start: str, end: str):
    """Deterministic random-walk OHLCV (only when the user ticked 'Allow synthetic data')."""
    import pandas as pd
    dates = pd.bdate_range(start=start, end=end)
    if len(dates) < 2:
        dates = pd.bdate_range(start='2023-01-02', periods=252)
    rng = np.random.default_rng(sum(ord(ch) for ch in symbol) % (2 ** 31))
    r = rng.normal(0.0003, 0.015, len(dates))
    c = 100.0 * np.cumprod(1.0 + r)
    o = np.concatenate([[100.0], c[:-1]]) * (1.0 + rng.normal(0, 0.003, len(dates)))
    h = np.maximum(o, c) * (1.0 + np.abs(rng.normal(0, 0.005, len(dates))))
    lo = np.minimum(o, c) * (1.0 - np.abs(rng.normal(0, 0.005, len(dates))))
    return pd.DataFrame({'Open': o, 'High': h, 'Low': lo, 'Close': c, 'Volume': 1e6}, index=dates)


def load_data(spec: Spec, req: Dict[str, Any], benchmark: Optional[str] = None) -> Data:
    """Bars of every requested symbol (+ the benchmark) in ONE price-store call. A symbol that cannot be
    loaded is dropped with a SYMBOL_DROPPED warning; when none loads the run fails with each symbol's
    reason (synthetic prices only when the user allowed them)."""
    from base.price_store import get_bars, normalize_symbol
    warnings: List[Dict[str, Any]] = []
    bench = normalize_symbol(benchmark) if benchmark else None
    broker = _broker_frames(req.get('brokerCandles') or {}, spec.symbols)
    if broker is not None:
        src = f"broker:{req.get('brokerDataSource') or 'broker'}"
        prov = {s: {'bars': len(f), 'firstBar': str(f.index[0])[:10], 'lastBar': str(f.index[-1])[:10],
                    'source': src} for s, f in broker.items()}
        frames = dict(broker)
        if bench and bench not in frames:
            b = get_bars([bench], spec.start, spec.end, interval=spec.interval, progress=False)
            frames.update(b.frames)
        return Data(frames=frames, symbols=list(spec.symbols), provenance=prov, warnings=warnings, source=src)
    want = list(spec.symbols) + ([bench] if bench and bench not in spec.symbols else [])
    try:
        res = get_bars(want, spec.start, spec.end, interval=spec.interval)
    except ValueError as e:
        raise EngineInputError(str(e))
    loaded = [s for s in spec.symbols if s in res.frames and len(res.frames[s]) >= 2]
    failed = {s: str(res.errors.get(s) or 'fewer than 2 bars in the period') for s in spec.symbols if s not in loaded}
    if not loaded:
        if bool(req.get('allowSyntheticData', False)):
            frames = {s: synthetic_frame(s, spec.start, spec.end) for s in spec.symbols}
            warnings.append({'code': 'SYNTHETIC_DATA', 'severity': 'error', 'symbol': None,
                             'message': 'Market data could not be loaded (' + '; '.join(failed.values())[:300] +
                                        '); this run used SYNTHETIC random-walk prices because synthetic data was '
                                        'allowed. The results have no market meaning.'})
            return Data(frames=frames, symbols=list(spec.symbols), provenance={}, warnings=warnings, synthetic=True,
                        source='synthetic')
        msg = '; '.join(f'{s}: {m}' for s, m in failed.items())
        raise EngineDataError(f'Market data could not be loaded for {", ".join(failed)} ({msg}). Refusing to run on '
                              f'synthetic prices - check the symbol (exchange suffix, e.g. RELIANCE.NS) and the dates, '
                              f'or tick "Allow synthetic data".')
    for s, m in failed.items():
        warnings.append({'code': 'SYMBOL_DROPPED', 'severity': 'warn', 'symbol': s,
                         'message': f'{s} was left out of the run: {m}'})
    frames = {s: res.frames[s] for s in loaded}
    if bench and bench in res.frames:
        frames[bench] = res.frames[bench]
    return Data(frames=frames, symbols=loaded, provenance=res.provenance, warnings=warnings,
                network_calls=res.network_calls)


def make_panel(data: Data):
    from base.execution import align_panel
    return align_panel({s: data.frames[s] for s in data.symbols})


def panel_slice(panel, a: int, e: int):
    from base.execution import Panel
    if a == 0 and e == panel.n:
        return panel
    sl = slice(a, e)
    return Panel(dates=panel.dates[sl], symbols=list(panel.symbols), open=panel.open[sl], high=panel.high[sl],
                 low=panel.low[sl], close=panel.close[sl], volume=panel.volume[sl], tradable=panel.tradable[sl],
                 dividends=None if panel.dividends is None else panel.dividends[sl],
                 splits=None if panel.splits is None else panel.splits[sl], adjustment=panel.adjustment)


def calendar_mismatch(panel) -> bool:
    listed = np.isfinite(panel.close)
    return bool(np.any(listed & ~panel.tradable))


# ════════════════════════════════════════════════════════════════════════════
# Signals (base.simulator) and decisions (bt)
# ════════════════════════════════════════════════════════════════════════════

class SignalBook:
    """Per-symbol bars + signal matrices of one panel (memoised per parameter set)."""

    def __init__(self, panel, frames: Dict[str, Any], symbols: List[str]):
        import pandas as pd
        self.panel = panel
        self.symbols = list(symbols)
        self.bars = {}
        self.rows = {}
        for s in symbols:
            f = frames[s]
            self.bars[s] = ST.Bars(f['Open'].to_numpy(float) if 'Open' in f else None,
                                   f['High'].to_numpy(float) if 'High' in f else None,
                                   f['Low'].to_numpy(float) if 'Low' in f else None, f['Close'].to_numpy(float),
                                   f['Volume'].to_numpy(float) if 'Volume' in f else None)
            idx = pd.DatetimeIndex(f.index)
            if idx.tz is not None:
                idx = idx.tz_localize(None)
            self.rows[s] = panel.dates.get_indexer(idx)
        self._cache: Dict[tuple, Tuple[np.ndarray, np.ndarray]] = {}

    def signals(self, strategy: str, params: Dict[str, Any]):
        key = (strategy, tuple(sorted((k, repr(v)) for k, v in params.items())))
        hit = self._cache.get(key)
        if hit is not None:
            return hit
        n, k = self.panel.n, len(self.symbols)
        E = np.zeros((n, k), dtype=bool)
        X = np.zeros((n, k), dtype=bool)
        for j, s in enumerate(self.symbols):
            try:
                e, x = ST.build_signals(strategy, self.bars[s], params)
            except ValueError as err:
                raise EngineInputError(f'{strategy}: {err}')
            r = self.rows[s]
            ok = r >= 0
            E[r[ok], j] = e[ok]
            X[r[ok], j] = x[ok]
        if len(self._cache) > 4096:
            self._cache.clear()
        self._cache[key] = (E, X)
        return E, X


class DecisionBook:
    """bt decision passes of one panel (memoised per parameter set)."""

    def __init__(self, panel, ppy: float):
        self.panel = panel
        self.ppy = ppy
        self._frame = None
        self._cache: Dict[tuple, Tuple[np.ndarray, List[int], Dict[str, Any]]] = {}

    def frame(self):
        if self._frame is None:
            import bt_algos as A
            p = self.panel
            self._frame = A.BarFrame(p.dates, p.symbols, p.close, p.tradable, self.ppy)
        return self._frame

    def decisions(self, strategy: str, params: Dict[str, Any], cancel=None):
        key = (strategy, tuple(sorted((k, repr(v)) for k, v in params.items())))
        hit = self._cache.get(key)
        if hit is not None:
            return hit
        import bt_algos as A
        try:
            stack = ST.allocation_stack(strategy, params)
        except ValueError as e:
            raise EngineInputError(f'{strategy}: {e}')
        W, rows = A.decide(stack, self.frame(), cancel=cancel)
        if len(self._cache) > 512:
            self._cache.clear()
        self._cache[key] = (W, rows, stack)
        return W, rows, stack


def make_simulator(spec: Spec, panel, backend: str = 'auto'):
    from base.simulator import Simulator
    sig = spec.kind == 'signals'
    return Simulator(panel, costs=spec.costs, fill=spec.fill, initial_capital=spec.initial_capital,
                     size=spec.fraction, fractional=spec.fractional,
                     stop_loss=spec.stop_loss if sig else 0.0, take_profit=spec.take_profit if sig else 0.0,
                     trailing_stop=spec.trailing_stop if sig else 0.0, backend=backend)


def executor_for(spec: Spec, panel) -> Tuple[str, Optional[str]]:
    """('bt' | 'simulator', reason the native path was not used or None)."""
    if spec.kind != 'allocation':
        return 'simulator', None
    if spec.fill != 'next_close':
        if spec.execution == 'bt':
            return 'simulator', (f'btExecution "bt" needs fillTiming next_close (bt fills at the close); '
                                 f'{spec.fill} ran on base.simulator')
        return 'simulator', None
    if spec.execution == 'simulator':
        return 'simulator', None
    if calendar_mismatch(panel):
        return 'simulator', ('the symbols trade on different calendars: bt\'s Backtest would trade a closed market at '
                             'its last close, so base.simulator executed the bt decisions (it defers such orders to '
                             'the next session)')
    cm = spec.costs
    if cm.cash_rate_annual or cm.financing_rate_annual or cm.participation_cap or cm.impact_coef:
        return 'simulator', ('cash interest / financing / participation cap / impact are not available in bt\'s '
                             'native accounting, so base.simulator executed the bt decisions')
    import bt_algos as A
    if not A.NATIVE_OK:
        return 'simulator', (f'the installed bt {lib_version()} has no per-trade cost hook (bt.core.CostModel, bt '
                             f'1.4+), so base.simulator executed the bt decisions')
    return 'bt', None


def decided_weights(spec: Spec, W: np.ndarray) -> np.ndarray:
    """The decision rows scaled by the percent size (positionSizeValue < 100 keeps the rest in cash)."""
    return W * spec.fraction if spec.fraction != 1.0 else W


def simulate(spec: Spec, sim, sbook: Optional[SignalBook], dbook: Optional[DecisionBook], params: Dict[str, Any], *,
             start: int = 0, rows: Optional[slice] = None, runs_hint: int = 1, carry: bool = False, cancel=None):
    """Run one parameter set on `sim` (whose panel may be the slice `rows` of the books' panel). carry=True
    (walk-forward test windows of an allocation strategy): the latest decision before `start` is placed on
    the bar before it, so the window starts invested instead of waiting for the next scheduled decision."""
    rs = rows if rows is not None else slice(0, sim.n)
    if spec.kind == 'allocation':
        W, _, _ = dbook.decisions(spec.strategy, params, cancel=cancel)
        W = decided_weights(spec, W)
        Ww = W[rs].copy()
        if carry and start > 0:
            a = rs.start or 0
            prior = [i for i in range(0, a + start) if np.isfinite(W[i]).any()]
            if prior and not np.isfinite(Ww[start - 1]).any():
                Ww[start - 1] = W[prior[-1]]
        return sim.run(weights=Ww, start=start, runs_hint=runs_hint)
    E, X = sbook.signals(spec.strategy, params)
    E, X = E[rs], X[rs]
    kw = dict(entries=E, exits=X, start=start, runs_hint=runs_hint)
    if spec.allow_short:
        kw.update(short_entries=X, short_exits=E)
    return sim.run(**kw)


# ════════════════════════════════════════════════════════════════════════════
# Trades
# ════════════════════════════════════════════════════════════════════════════

def episode_trades(res, *, excursions: bool = True) -> List[Dict[str, Any]]:
    from base.execution import position_episodes
    p = res.panel
    return position_episodes(res.fill_arrays, p.dates, p.symbols, last_close=p.close[-1],
                             high=p.high if excursions else None, low=p.low if excursions else None,
                             borrow_per_bar=res.borrow, positions=res.positions,
                             dividends=p.dividends if p.adjustment == 'none' else None,
                             splits=p.splits if p.adjustment == 'none' else None, excursions=excursions)


def trade_extras_from_pnls(pnl: np.ndarray) -> Dict[str, Any]:
    n = len(pnl)
    if n == 0:
        return {'trades': 0, 'winRate': None, 'profitFactor': None, 'sqn': None}
    wins, losses = pnl[pnl > 0], pnl[pnl < 0]
    pf = float(wins.sum() / -losses.sum()) if len(losses) and losses.sum() < 0 else None
    sd = float(np.std(pnl, ddof=1)) if n > 1 else 0.0
    sqn = float(math.sqrt(n) * pnl.mean() / sd) if sd > 0 else None
    return {'trades': int(n), 'winRate': float(len(wins) / n), 'profitFactor': pf, 'sqn': sqn}


def closed_pnls(res) -> np.ndarray:
    """Net P&L of the closed trades (round trips of signal strategies = position episodes of allocation
    strategies: cash flows of a symbol's fills from flat to flat, commission and slippage included, borrow
    charged over the holding bars) - the optimiser's trade objectives. Same numbers as
    base.execution.position_episodes' closed trades, without building the trade dicts."""
    fa = res.fill_arrays
    bar, asset, qty = fa['bar'], fa['asset'], fa['qty']
    flow = -qty * fa['price'] - fa['commission']
    k = res.positions.shape[1]
    cum_b = np.cumsum(res.borrow, axis=0) if np.any(res.borrow) else None
    q = np.zeros(k)
    cash = np.zeros(k)
    start = np.zeros(k, dtype=np.int64)
    out: List[float] = []
    for i in np.lexsort((np.arange(len(bar)), bar)):
        j = int(asset[i])
        dq = float(qty[i])
        if q[j] == 0.0:
            start[j] = bar[i]
            cash[j] = 0.0
        nq = q[j] + dq
        if q[j] != 0.0 and (nq == 0.0 or abs(nq) <= 1e-9 * max(1.0, abs(q[j])) or (nq > 0) != (q[j] > 0)):
            part = min(1.0, abs(q[j]) / abs(dq))
            b = float(cum_b[bar[i], j] - cum_b[start[j], j]) if cum_b is not None else 0.0
            out.append(cash[j] + flow[i] * part - b)
            rest = nq if (nq != 0.0 and (nq > 0) != (q[j] > 0) and abs(nq) > 1e-9 * max(1.0, abs(q[j]))) else 0.0
            q[j] = rest
            cash[j] = flow[i] * (1.0 - part) if rest else 0.0
            start[j] = bar[i]
            continue
        q[j] = nq
        cash[j] += flow[i]
    return np.asarray(out, dtype=float)


# ════════════════════════════════════════════════════════════════════════════
# run_backtest
# ════════════════════════════════════════════════════════════════════════════

_EPISODE_NOTE = ('Trades of this allocation strategy are POSITION EPISODES: one trade per symbol from the fill that '
                 'opens its position until the position is flat again; rebalancing top-ups and trims stay inside the '
                 'episode (their cash flows are part of its P&L), positions still held at the end are open trades '
                 'marked to market. Win rate / profit factor therefore describe whole positions, not rebalance '
                 'fragments; turnover, rebalance count and per-symbol attribution describe the rebalancing.')


def _identity(spec: Spec, executor: str, backend: str, stack_text: str, notes: List[str]):
    from base.result_normalizer import EngineIdentity
    cm = spec.costs
    modelled = []
    if executor == 'simulator':
        if spec.allow_short and spec.kind == 'signals' and cm.borrow_rate_annual > 0:
            modelled.append('borrow cost')
        if cm.cash_rate_annual > 0:
            modelled.append('cash interest')
        if cm.impact_coef > 0:
            modelled.append('market impact')
        if cm.participation_cap:
            modelled += ['volume limits', 'partial fills']
    lib = 'bt' if spec.kind == 'allocation' else None
    return EngineIdentity(ENGINE, implementation(spec.kind, executor, backend, stack_text, spec.fill, spec.fractional),
                          library=lib, version=ENGINE_VERSION, fill_mode=spec.fill, commission_itemised=True,
                          slippage_itemised=True, modelled=tuple(modelled), trade_mode='signals', notes=list(notes))


def _execution_text(spec: Spec, executor: str, stack: Optional[Dict[str, Any]] = None) -> str:
    from base.execution import fill_model_block
    t = fill_model_block(spec.fill)['description']
    if spec.kind == 'allocation':
        sched = ST.describe_period(stack['schedule']) if stack else 'scheduled'
        t += (f' The bt algo stack runs on its schedule ({sched if sched != "once" else "once, on the first bar"}): '
              f'the weights decided on a bar\'s close use bars up to that close and are traded at the fill above. ')
        if executor == 'bt':
            t += ('bt.Backtest executes them: target value = weight x portfolio value at the fill bar\'s close, '
                  'positions with weight 0 are closed, commission and slippage paid out of the portfolio through bt\'s '
                  'cost hook.')
        else:
            t += ('base.simulator executes them: every listed symbol is traded to its target (sells before buys; '
                  'commission and slippage paid out of the portfolio; an order for a closed market waits for its '
                  'next session).')
        return t
    stops = []
    if spec.stop_loss:
        stops.append(f'stop-loss {spec.stop_loss * 100:g}%')
    if spec.take_profit:
        stops.append(f'take-profit {spec.take_profit * 100:g}%')
    if spec.trailing_stop:
        stops.append(f'trailing stop {spec.trailing_stop * 100:g}%')
    if stops:
        t += (' ' + ', '.join(stops) + ' checked INSIDE each bar against its high and low from the entry fill; a gap '
              'through the level fills at the open; stop before target when both are touched in one bar.')
    if spec.allow_short:
        t += ' Short selling allowed: an exit signal opens a short, an entry signal covers it (borrow fee accrued).'
    return t


def assumptions_block(spec: Spec, data: Data, executor: str, backend: str, k: int,
                      stack: Optional[Dict[str, Any]] = None, extra_notes: Optional[List[str]] = None) -> Dict[str, Any]:
    from base.base_provider import _SURVIVORSHIP_NOTE
    if data.synthetic:
        src = 'SYNTHETIC random-walk prices (user opted in) - results have NO market meaning'
    elif data.source.startswith('broker:'):
        src = f'Broker candles ({data.source[7:]})'
    else:
        src = 'Yahoo Finance via the Fincept price cache (split/dividend-adjusted prices; trade prices are not ' \
              'historical quotes)'
    notes = list(spec.notes) + list(extra_notes or [])
    stack_text = ST.describe_stack(stack) if stack else ''
    if spec.kind == 'allocation':
        notes.append(f'bt algo stack: {stack_text}. Lookbacks are in bars of the run\'s interval.')
        notes.append(_EPISODE_NOTE)
        sizing = (f'target weights decided by the bt algo stack' +
                  (f', scaled to {spec.fraction * 100:.0f}% invested' if spec.fraction < 1 else '') +
                  ('; whole shares' if not spec.fractional else '; fractional shares'))
    else:
        slot = 'equity' if k == 1 else f'the symbol\'s slot (equity / {k})'
        sizing = (f'percent: {spec.fraction * 100:.1f}% of {slot} per entry, capped by cash; ' +
                  ('fractional shares' if spec.fractional else 'whole shares'))
        if k > 1:
            notes.append(f'One portfolio of {k} symbols with shared cash: each entry is sized on its slot of the '
                         f'equity (1/{k}) and held until its exit (bt\'s v1 rebalanced the symbols that were "on" to '
                         f'equal weights every bar).')
    return {'engine': implementation(spec.kind, executor, backend, stack_text, spec.fill, spec.fractional),
            'execution': _execution_text(spec, executor, stack), 'costs': spec.costs.describe(), 'dataSource': src,
            'survivorship': _SURVIVORSHIP_NOTE, 'positionSizing': sizing, 'ignoredSettings': list(spec.ignored),
            'notes': notes, 'shares': 'fractional' if spec.fractional else 'whole',
            'executedBy': 'bt' if executor == 'bt' else 'base.simulator',
            'decidedBy': 'bt algo stack' if spec.kind == 'allocation' else 'bt_strategies signal rules'}


def cost_sensitivity_rows(spec: Spec, panel, sbook, dbook, executor: str, base_equity, req, native_ms: float):
    """costs.sensitivity: the same decisions / signals and fills re-run with the trading costs at 0x, 2x, 3x
    (1x = this run), on the SAME executor. bt's native path is re-run only when one run is cheap (< 0.35 s);
    otherwise the block is omitted (logged)."""
    from dataclasses import replace
    from base.costs import cost_sensitivity
    if spec.costs is None or spec.costs.is_zero():
        return None, None
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    if executor == 'bt':
        if native_ms > 350:
            return None, (f'costs.sensitivity omitted: each native bt run takes {native_ms / 1000:.1f} s (it is '
                          f'computed when one run costs < 0.35 s)')
        import bt_algos as A
        W, _, _ = dbook.decisions(spec.strategy, spec.params)

        def sim_eq(cm):
            return A.native_execute(panel, decided_weights(spec, W), cm, fractional=spec.fractional,
                                    initial_capital=spec.initial_capital).equity
        return cost_sensitivity(sim_eq, spec.costs, panel.dates, rf=rf, base_equity=base_equity), None

    def sim_eq(cm):
        sp = replace(spec, costs=cm, notes=[], ignored=[])
        return simulate(sp, make_simulator(sp, panel), sbook, dbook, sp.params).equity
    return cost_sensitivity(sim_eq, spec.costs, panel.dates, rf=rf, base_equity=base_equity), None


def run_backtest(req: Dict[str, Any]) -> Dict[str, Any]:
    """The run_backtest command: {'success': True, 'data': contract v2} (raises EngineInputError /
    EngineDataError with a plain sentence)."""
    from base.costs import costs_by_year
    from base.metrics import _fmt_date, infer_periods_per_year
    from base.progress import CancelToken, Progress
    from base.result_normalizer import build_backtest_result
    t0 = time.perf_counter()
    cancel = CancelToken(req)
    spec = parse_request(req)
    bench = str(req.get('benchmarkSymbol') or req.get('benchmark') or '').strip() or None
    data = load_data(spec, req, benchmark=bench)
    cancel.check()
    k = len(data.symbols)
    need = ST.MIN_SYMBOLS.get(spec.strategy, 1)
    if k < need:
        raise EngineInputError(f'{spec.strategy} allocates across symbols and needs at least {need} (got '
                               f'{", ".join(data.symbols)}): with one symbol it would simply hold it, i.e. buy and '
                               f'hold under another name. Add symbols, or choose buy_and_hold / a signal strategy.')
    panel = make_panel(data)
    ppy = float(infer_periods_per_year(panel.dates))
    prog = Progress('run', total=3, message=f'{spec.strategy} on {k} symbol(s)')
    sbook = dbook = None
    stack = None
    warnings = list(data.warnings)
    notes: List[str] = []
    executor, why_not_native = executor_for(spec, panel)
    if why_not_native:
        notes.append(f'base.simulator executed this run: {why_not_native}.')
        warnings.append({'code': 'NATIVE_EXECUTION_UNAVAILABLE', 'severity': 'info', 'symbol': None,
                         'message': f'bt\'s own Backtest did not execute this run: {why_not_native}.'})
    decision_rows: List[int] = []
    native_ms = 0.0
    if spec.kind == 'signals':
        sbook = SignalBook(panel, data.frames, data.symbols)
        E, X = sbook.signals(spec.strategy, spec.params)
        prog.update(1, 'Signals ready')
        cancel.check()
        res = simulate(spec, make_simulator(spec, panel), sbook, None, spec.params)
        if spec.strategy and int(E.sum()) == 0:
            warnings.append({'code': 'NO_SIGNALS', 'severity': 'warn', 'symbol': None,
                             'message': f'The {spec.strategy} rule produced no entry signal in this period with these '
                                        f'parameters.'})
        trades = res.trades()
    else:
        dbook = DecisionBook(panel, ppy)
        W, decision_rows, stack = dbook.decisions(spec.strategy, spec.params, cancel=cancel)
        prog.update(1, f'bt decided {len(decision_rows)} rebalances')
        cancel.check()
        if executor == 'bt':
            import bt_algos as A
            tn = time.perf_counter()
            res = A.native_execute(panel, decided_weights(spec, W), spec.costs, fractional=spec.fractional,
                                   initial_capital=spec.initial_capital)
            native_ms = (time.perf_counter() - tn) * 1000.0
            fa = res.fill_arrays
            mine = float(np.sum(fa['commission']) + np.sum(np.abs(fa['price'] - fa['ref_price']) * np.abs(fa['qty'])))
            if res.bt_fees is not None and abs(mine - res.bt_fees) > 1e-6 * max(1.0, res.bt_fees):
                notes.append(f'bt charged {res.bt_fees:,.4f} in costs; the itemised commission + slippage add up to '
                             f'{mine:,.4f}.')
        else:
            res = simulate(spec, make_simulator(spec, panel), None, dbook, spec.params)
        if not decision_rows:
            warnings.append({'code': 'NO_DECISIONS', 'severity': 'warn', 'symbol': None,
                             'message': f'The bt algo stack of {spec.strategy} made no allocation decision in this '
                                        f'period (lookback {stack.get("lookback")} bars is longer than the data, or '
                                        f'the schedule never fired).'})
        if spec.strategy in ('momentum_topn', 'momentum_inv_vol') and int(spec.params.get('topN', 2)) >= k:
            warnings.append({'code': 'TOPN_SELECTS_ALL', 'severity': 'info', 'symbol': None,
                             'message': f'topN {spec.params.get("topN")} >= {k} symbols: every symbol is selected, so '
                                        f'the momentum ranking has no effect.'})
        trades = episode_trades(res)
    prog.update(2, 'Execution done')
    for w in res.warnings:
        if w not in warnings:
            warnings.append(w)
    backend = res.backend if executor == 'simulator' else 'bt'
    stack_text = ST.describe_stack(stack) if stack else ''
    nreq = dict(req)
    nreq['symbols'] = list(data.symbols)
    strat = dict(req.get('strategy') or {}) if isinstance(req.get('strategy'), dict) else {}
    strat['type'] = spec.strategy
    strat['params'] = dict(spec.params)
    nreq['strategy'] = strat
    if bench:
        nreq['benchmarkSymbol'] = bench
    ex = res.exposure_series()
    shorts = spec.allow_short and spec.kind == 'signals'
    costs = res.costs_summary()
    costs['byYear'] = costs_by_year(res.fill_arrays, panel.dates, res.equity)
    sens, sens_note = cost_sensitivity_rows(spec, panel, sbook, dbook, executor, res.equity, req, native_ms)
    if sens:
        costs['sensitivity'] = sens
    extra: Dict[str, Any] = {}
    wb = res.weights_block()
    if wb:
        extra['weights'] = wb
    fill_bars = sorted(set(int(b) for b in res.fill_arrays['bar']))
    if spec.kind == 'allocation':
        extra['rebalancing'] = {
            'schedule': ST.describe_period(stack['schedule']), 'stack': stack_text,
            'decisions': len(decision_rows), 'rebalances': len(fill_bars),
            'firstDecision': _fmt_date(panel.dates[decision_rows[0]]) if decision_rows else None,
            'lastDecision': _fmt_date(panel.dates[decision_rows[-1]]) if decision_rows else None,
            'decidedBy': f'bt library {lib_version()} algo stack', 'executedBy': 'bt' if executor == 'bt' else
            'base.simulator', 'fills': int(len(res.fill_arrays['bar']))}
    logs = [f'{panel.n} bars x {k} symbol(s) {", ".join(data.symbols)}; strategy {spec.strategy} {spec.params}; '
            f'fill {spec.fill}; executed by {"bt.Backtest" if executor == "bt" else "base.simulator " + backend}.',
            f'{len(res.fill_arrays["bar"])} fills on {len(fill_bars)} bars; final equity {float(res.equity[-1]):,.2f}.']
    if spec.kind == 'allocation':
        logs.append(f'bt algo stack {stack_text}: {len(decision_rows)} decisions.')
    if sens_note:
        logs.append(sens_note)
    warm = ST.default_warmup(spec.strategy, spec.params)
    data_v2 = build_backtest_result(
        nreq, dates=panel.dates, equity=res.equity, identity=_identity(spec, executor, backend, stack_text, []),
        trades=trades, fills=res.fills(), exposure=ex['gross'], net_exposure=ex['net'] if shorts else None,
        cash=res.cash, costs=costs, cost_model=spec.costs, frames=data.frames,
        price_provenance=data.provenance if not data.synthetic else None,
        assumptions=assumptions_block(spec, data, executor, backend, k, stack, notes), warnings=warnings, logs=logs,
        warmup_bars=warm, using_synthetic=data.synthetic, extra=extra)
    # v1 blocks the screen still reads (BacktestTearsheet: BUY & HOLD, TIME IN MARKET, downside deviation)
    c = panel.close
    first = np.array([c[np.argmax(np.isfinite(c[:, j])), j] for j in range(k)])
    bh = float(np.nanmean(c[-1] / first - 1.0)) if k else None
    data_v2['extendedStats'] = {'buyAndHoldReturn': bh, 'exposureTime': data_v2['exposure'].get('timeInMarket'),
                                'buyAndHoldNote': 'holding ' + ('the symbol' if k == 1 else
                                                                f'the {k} symbols in equal parts') +
                                                  ' from the first to the last bar, no costs'}
    r = np.diff(res.equity) / np.where(res.equity[:-1] > 0, res.equity[:-1], np.nan)
    moved = r[np.isfinite(r) & (r != 0)]
    # v1 bt key (DETAILS table): share of up bars among the bars whose equity moved
    data_v2['performance']['positiveDayRate'] = float((moved > 0).mean()) if len(moved) else None
    data_v2['riskMetrics'] = {'downsideDeviation': data_v2['performance'].get('downsideDeviation'),
                              'volatility': data_v2['performance'].get('volatility'),
                              'maxDrawdown': data_v2['performance'].get('maxDrawdown')}
    data_v2['provenance']['engineMs'] = int((time.perf_counter() - t0) * 1000)
    data_v2['provenance']['simulatorBackend'] = backend
    data_v2['provenance']['executedBy'] = 'bt' if executor == 'bt' else 'base.simulator'
    prog.finish('Done')
    return {'success': True, 'data': data_v2}


# ════════════════════════════════════════════════════════════════════════════
# optimize / walk_forward (base.robustness + bt_evaluate.evaluate)
# ════════════════════════════════════════════════════════════════════════════

def _param_ranges(req: Dict[str, Any], spec: Spec) -> Dict[str, Any]:
    ranges = req.get('paramRanges') or req.get('parameters')
    if not isinstance(ranges, dict) or not ranges:
        raise EngineInputError('No parameter ranges supplied (paramRanges with min / max / step per parameter).')
    cat = ST.catalog_entry(spec.strategy)
    known = {p['id']: p for p in (cat or {}).get('parameters', [])}
    if not known:
        raise EngineInputError(f'Strategy {spec.strategy} has no parameters to optimise.')
    unknown = [x for x in ranges if x not in known]
    if unknown:
        raise EngineInputError(f'{", ".join(unknown)} {"is not a parameter" if len(unknown) == 1 else "are not parameters"}'
                               f' of {spec.strategy} (parameters: {", ".join(known)}).')
    out = {}
    for key, v in ranges.items():
        typ = known[key]['type']
        if typ == 'choice':
            vals = v if isinstance(v, list) else (v.get('values') if isinstance(v, dict) else [v])
            if not vals:
                raise EngineInputError(f'{key} is a choice ({", ".join(map(str, known[key].get("choices") or []))}): '
                                       f'give a list of values to try.')
            out[key] = list(vals)
            continue
        if isinstance(v, dict) and typ == 'float':
            v = dict(v, type='float')
        out[key] = v
    return out


def _constraint_ok(strategy: str, params: Dict[str, Any]) -> bool:
    cat = ST.catalog_entry(strategy)
    for c in (cat or {}).get('constraints', []):
        a, b = c['lt']
        try:
            if not float(params[a]) < float(params[b]):
                return False
        except (KeyError, TypeError, ValueError):
            continue
    return True


def _combos(req: Dict[str, Any], spec: Spec, default_max: Optional[int] = 500):
    from base import robustness as R
    ranges = _param_ranges(req, spec)
    method = str(req.get('optimizeMethod') or req.get('method') or 'grid')
    max_it = req.get('maxIterations', default_max)
    seed = req.get('seed', req.get('optimizeSeed'))
    seed = int(_num(seed, 7)) if seed is not None else 7
    try:
        combos, info = R.sample_combinations(ranges, method=method, max_iterations=max_it, seed=seed)
    except ValueError as e:
        raise EngineInputError(str(e))
    warnings = []
    base = dict(spec.params)
    keep = [c for c in combos if _constraint_ok(spec.strategy, dict(base, **c))]
    dropped = len(combos) - len(keep)
    if dropped:
        if not keep:
            raise EngineInputError(f'Every one of the {len(combos)} parameter combinations breaks a rule of '
                                   f'{spec.strategy} (for example fast period >= slow period).')
        warnings.append({'code': 'COMBINATIONS_SKIPPED', 'severity': 'info', 'symbol': None,
                         'message': f'{dropped} of {len(combos)} combinations break a rule of {spec.strategy} '
                                    f'(e.g. fast period >= slow period) and were not evaluated.'})
        combos = keep
        info = dict(info, iterations=len(combos), skippedByConstraints=dropped)
    return combos, info, seed, warnings


def choose_backend(spec: Spec, n: int, k: int, runs: int) -> str:
    from base.simulator import numba_available
    return 'numba' if (numba_available() and n * k * max(1, runs) >= 200_000) else 'python'


def _workers(req: Dict[str, Any], spec: Spec, backend: str, trials: int, n: int, k: int) -> int:
    """Worker processes for base.robustness: a spawn worker costs ~2-3 s on Windows (imports; bt + ffn +
    scikit-learn for allocation strategies), so a pool is used only when the estimated single-process compute
    exceeds ~4 s (never above the robustness / FINCEPT_BT_MAX_WORKERS / maxWorkers cap)."""
    from base import robustness as R
    if req.get('maxWorkers'):                       # an explicit request is honoured (still capped)
        return R.max_workers(req, trials)
    if spec.kind == 'allocation':
        per = 0.15 + 2e-5 * n * k
    else:
        per = (0.0005 + 3e-7 * n * k) if backend == 'numba' else 1.5e-5 * n * k
    if trials * per < 4.0:
        return 1
    return R.max_workers(req, trials)


def _robust_impl(spec: Spec, backend: str) -> str:
    if spec.kind == 'signals':
        return (f'{implementation("signals", backend=backend)}; every combination scored by base.robustness (same '
                f'signals, costs, fills and sizing as run_backtest)')
    v = lib_version() or '?'
    k = 'numba' if backend == 'numba' else 'Python kernel'
    native = (' run_backtest at fillTiming next_close executes the same decisions in bt.Backtest itself; here every '
              'combination is executed by base.simulator at next_close (bt\'s accounting differs only by its share '
              'rounding).' if spec.fill == 'next_close' else '')
    return (f'bt library {v} algo stack decides the target weights of every combination; base.simulator executes '
            f'them (weights mode, {spec.fill}, {k}) and base.robustness scores them.{native}')


def _robust_meta(out: Dict[str, Any], spec: Spec, data: Data, panel, req: Dict[str, Any], backend: str,
                 command: str, extra_warnings: List[Dict[str, Any]], rf: float, t0: float) -> Dict[str, Any]:
    import platform
    from base.metrics import _fmt_date
    from base.execution import fill_model_block
    impl = _robust_impl(spec, backend)
    out['engine'] = ENGINE
    out['engineImplementation'] = impl
    out['engineVersion'] = ENGINE_VERSION
    out['usingSyntheticData'] = data.synthetic
    out['strategy'] = spec.strategy
    out['symbols'] = list(data.symbols)
    out['warnings'] = list(data.warnings) + list(extra_warnings) + list(out.get('warnings') or [])
    notes = list(spec.notes)
    if spec.kind == 'allocation':
        notes.append(_EPISODE_NOTE)
    out['assumptions'] = {
        'engine': impl, 'execution': _execution_text(spec, 'simulator'), 'fillModel': fill_model_block(spec.fill),
        'costs': spec.costs.describe(),
        'positionSizing': (f'target weights from the bt algo stack x {spec.fraction * 100:.0f}%'
                           if spec.kind == 'allocation' else f'percent: {spec.fraction * 100:.1f}% of each symbol\'s '
                                                             f'slot per entry'),
        'shares': 'fractional' if spec.fractional else 'whole',
        'riskFreeRate': rf, 'riskFreeSource': 'request',
        'dataSource': ('SYNTHETIC random-walk prices (user opted in)' if data.synthetic else
                       (f'Broker candles ({data.source[7:]})' if data.source.startswith('broker:') else
                        'Yahoo Finance via the Fincept price cache (split/dividend-adjusted prices)')),
        'objectiveNote': f'Objectives use the request\'s risk-free rate ({rf * 100:.2f}% a year) as a constant; '
                         f'Sharpe = mean excess bar return / std x sqrt(bars per year) (base.metrics).',
        'ignoredSettings': list(spec.ignored), 'notes': notes}
    out['provenance'] = {
        'engine': ENGINE, 'engineImplementation': impl, 'engineVersion': ENGINE_VERSION,
        'library': 'bt' if spec.kind == 'allocation' else None,
        'libraryVersion': lib_version() if spec.kind == 'allocation' else None,
        'pythonVersion': platform.python_version(), 'command': command, 'dataSource': data.source,
        'interval': spec.interval, 'bars': int(panel.n), 'firstBar': _fmt_date(panel.dates[0]),
        'lastBar': _fmt_date(panel.dates[-1]), 'requestedStart': spec.start, 'requestedEnd': spec.end,
        'symbols': list(data.symbols), 'strategy': {'type': spec.strategy, 'name': (req.get('strategy') or {}).get('name')
                                                    if isinstance(req.get('strategy'), dict) else None},
        'baseParameters': dict(spec.params), 'seed': out.get('seed'), 'simulatorBackend': backend,
        'executedBy': 'base.simulator', 'decidedBy': 'bt algo stack' if spec.kind == 'allocation' else
        'bt_strategies signal rules', 'durationMs': int((time.perf_counter() - t0) * 1000), 'schemaVersion': 2}
    out.setdefault('logs', []).append(
        f'{panel.n} bars x {len(data.symbols)} symbol(s); {out.get("iterations") or out.get("combinationsPerWindow")} '
        f'combinations; workers {out.get("workers")}; backend {backend}; {out["provenance"]["durationMs"]} ms.')
    return out


def _setup_robust(req: Dict[str, Any], default_max: int):
    spec = parse_request(req)
    data = load_data(spec, req)
    need = ST.MIN_SYMBOLS.get(spec.strategy, 1)
    if len(data.symbols) < need:
        raise EngineInputError(f'{spec.strategy} needs at least {need} symbols (got {", ".join(data.symbols)}).')
    panel = make_panel(data)
    combos, info, seed, warns = _combos(req, spec, default_max=default_max)
    from base import robustness as R
    try:
        objective = R.normalize_objective(req.get('optimizeObjective') or req.get('objective') or 'sharpe')
    except ValueError as e:
        raise EngineInputError(str(e))
    return spec, data, panel, combos, info, seed, warns, objective


def optimize(req: Dict[str, Any]) -> Dict[str, Any]:
    import bt_evaluate as BE
    from base import robustness as R
    from base.metrics import infer_periods_per_year
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec, data, panel, combos, info, seed, warns, objective = _setup_robust(req, 500)
    backend = choose_backend(spec, panel.n, len(data.symbols), len(combos))
    ctx = BE.make_context(spec, panel, data.frames, data.symbols, backend)
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    hold = _num(req.get('holdoutFraction'), 0.0) or 0.0
    try:
        out = R.run_optimization(BE.evaluate, combos, context=ctx, objective=objective,
                                 ppy=infer_periods_per_year(panel.dates), dates=panel.dates, rf=rf, info=info,
                                 request=req, seed=seed, holdout_fraction=hold, cancel=CancelToken(req),
                                 workers=_workers(req, spec, backend, len(combos), panel.n, len(data.symbols)))
    except ValueError as e:
        raise EngineInputError(str(e))
    out['seed'] = seed
    return {'success': True, 'data': _robust_meta(out, spec, data, panel, req, backend, 'optimize', warns, rf, t0)}


def walk_forward(req: Dict[str, Any]) -> Dict[str, Any]:
    import bt_evaluate as BE
    from base import robustness as R
    from base.metrics import infer_periods_per_year
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec, data, panel, combos, info, seed, warns, objective = _setup_robust(req, 200)
    k_win = int(_num(req.get('wfSplits') or req.get('nSplits'), 5) or 5)
    ratio = float(_num(req.get('wfTrainRatio') or req.get('trainRatio'), 0.7) or 0.7)
    mode = str(req.get('wfMode') or req.get('walkForwardMode') or 'rolling').lower()
    purge = int(_num(req.get('purgeBars'), 0) or 0)
    embargo = int(_num(req.get('embargoBars'), 0) or 0)
    warm = max(ST.default_warmup(spec.strategy, dict(spec.params, **c)) for c in combos)
    try:
        windows = R.walk_forward_windows(panel.n, k_win, ratio, mode, purge_bars=purge, embargo_bars=embargo,
                                         warmup_bars=warm)
    except ValueError as e:
        raise EngineInputError(str(e))
    backend = choose_backend(spec, panel.n, len(data.symbols), len(combos) * (len(windows) + 1))
    ctx = BE.make_context(spec, panel, data.frames, data.symbols, backend)
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    try:
        out = R.run_walk_forward(BE.evaluate, combos, windows, context=ctx, objective=objective,
                                 ppy=infer_periods_per_year(panel.dates), dates=panel.dates, rf=rf,
                                 initial_capital=spec.initial_capital, mode=mode, purge_bars=purge,
                                 embargo_bars=embargo, request=req, seed=seed, cancel=CancelToken(req),
                                 workers=_workers(req, spec, backend, len(combos) * (len(windows) + 1), panel.n,
                                                  len(data.symbols)))
    except ValueError as e:
        raise EngineInputError(str(e))
    out['seed'] = seed
    out['warmupBars'] = warm
    spans = [(w.get('testStart'), w.get('testEnd'), w.get('window')) for w in out.get('windows') or []]
    for row in out.get('oosEquity') or []:      # v1 rows carried their window number
        for a, b, wi in spans:
            if a and b and a <= row['date'] <= b:
                row['window'] = wi
                break
    out['combinationsTotal'] = info.get('totalCombinations')
    out['sampling'] = info.get('sampling')
    if spec.kind == 'allocation':
        out.setdefault('logs', []).append('Each test window starts from the latest bt decision made before it (filled '
                                          'on its first bar), then follows the schedule.')
    return {'success': True, 'data': _robust_meta(out, spec, data, panel, req, backend, 'walk_forward', warns, rf, t0)}
