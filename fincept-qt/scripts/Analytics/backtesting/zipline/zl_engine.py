"""
The "zipline" engine's backtest core, rebased on the shared backtesting core (base/).

WHAT RUNS (every result states it in engine / engineImplementation / assumptions / provenance)
  Bars       base.price_store (one on-disk cache, exchange SESSION dates - v1 shifted every non-US
             bar to the previous UTC day, audit Z4; zero downloads warm), or broker candles the host
             injected (brokerCandles) when they cover every symbol. Several symbols are aligned on the
             union of their session dates (base.execution.align_panel; CALENDAR_MISMATCH warning).
             Bars of the request's interval (v1 fetched daily bars whatever was asked, Z15).
  Decisions  the catalogue algorithm (zl_strategies) through the Fincept zipline-style API (zl_api):
             initialize(context) asks for full-history indicators, handle_data(context, data) /
             scheduled functions run once per bar and record order_target_percent targets.
  Execution  base.simulator with the shared cost model at the request's fillTiming (next_open by
             default): signal algorithms in signals mode (cash-checked sizing at the fill price -
             v1 sized on the previous close and let cash go to -1,109, Z7; intrabar stops; shorts on
             request with borrow fee; percent / fixed / risk / vol_target sizing), allocation
             algorithms in weights mode (rebalance to the targets, costs paid out of the portfolio).
             ONE portfolio with shared cash for any number of symbols (v1 traded the first symbol
             only).
  Trades     signals: FIFO round trips incl. the position open at the end (exitReason 'open', v1 lost
             it, Z5); allocation: position episodes (base.execution.position_episodes).
  Costs      commission and slippage itemised per fill / trade / total (v1 reported 0, Z6),
             costs.byYear, costs.sensitivity (0x / 1x / 2x / 3x).
  Results    base.result_normalizer.build_backtest_result: every headline metric from base.metrics
             with the request's / ^IRX risk-free rate (v1 ignored rf, Z3; annualised on n bars, Z31).
  optimize / walk_forward   base.robustness over zl_evaluate.evaluate (bars loaded once, decisions
             memoised per parameter set; v1 re-downloaded per combination, Z12).
zipline-reloaded is not imported (see zipline_provider.py for why).
"""

from __future__ import annotations

import math
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

import zl_api as A
import zl_strategies as ST

ENGINE = 'zipline'
ENGINE_VERSION = '2.0.0'
SIZING_MODES = ('percent', 'fixed', 'risk', 'vol_target')
VOL_LOOKBACK = 20


class EngineInputError(ValueError):
    """A request the engine cannot run (plain sentence for the user)."""


class EngineDataError(RuntimeError):
    """Market data could not be loaded (plain sentence, DATA error code)."""


def _log(msg: str) -> None:
    import sys
    print(f'[zipline] {msg}', file=sys.stderr, flush=True)


def zipline_reloaded() -> Tuple[bool, Optional[str]]:
    """(installed, version) of the zipline-reloaded distribution - read from its metadata, never imported."""
    try:
        from importlib.metadata import version
        return True, version('zipline-reloaded')
    except Exception:
        return False, None


def implementation(kind: str, backend: str = 'python', fill: Optional[str] = None) -> str:
    k = 'numba' if backend == 'numba' else 'Python kernel'
    if kind == 'allocation':
        return (f'Fincept zipline-style algorithm API (schedule_function + order_target_percent target weights) '
                f'decides; base.simulator executes (weights mode, {k}) - zipline-reloaded is not used')
    return (f'Fincept zipline-style algorithm API (initialize / handle_data on full-history indicators) decides; '
            f'base.simulator executes (signals mode, {k}) - zipline-reloaded is not used')


def short_implementation() -> str:
    return ('Fincept zipline-style algorithm API (initialize / handle_data, schedule_function, order_target_percent) '
            'on base.simulator - zipline-reloaded is not used')


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
    sizing: Dict[str, Any]
    stop_loss: float
    take_profit: float
    trailing_stop: float
    allow_short: bool
    fractional: bool
    ignored: List[str] = field(default_factory=list)
    notes: List[str] = field(default_factory=list)

    @property
    def fraction(self) -> float:
        return float(self.sizing.get('fraction', 1.0)) if self.sizing.get('mode') == 'percent' else 1.0

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


def resolve_sizing(req: Dict[str, Any], stop_loss: float, ignored: List[str]) -> Dict[str, Any]:
    mode = req.get('positionSizing', 'percent')
    if isinstance(mode, dict):
        mode = mode.get('mode', 'percent')
    mode = str(mode or 'percent').strip().lower().replace('-', '_').replace(' ', '_')
    mode = {'volatility': 'vol_target', 'voltarget': 'vol_target', 'pct': 'percent', 'percentage': 'percent',
            'fixed_amount': 'fixed', 'risk_per_trade': 'risk'}.get(mode, mode)
    value = _num(req.get('positionSizeValue'), None)
    if mode == 'kelly':
        ignored.append("position sizing 'kelly' (not implemented by this engine: 100 % of each symbol's slot was "
                       "used)")
        return {'mode': 'percent', 'fraction': 1.0}
    if mode not in SIZING_MODES:
        raise EngineInputError(f'position sizing must be one of {", ".join(SIZING_MODES)}, not {mode!r}')
    if value is None or value <= 0:
        value = {'percent': 100.0, 'vol_target': 10.0, 'risk': 1.0}.get(mode, 10000.0)
    if mode == 'fixed':
        return {'mode': 'fixed', 'amount': float(value)}
    if mode == 'risk':
        if not stop_loss:
            raise EngineInputError('Risk-per-trade sizing needs a STOP LOSS: the position size is the amount risked '
                                   'divided by the distance to the stop. Set a stop-loss or choose another sizing '
                                   'method.')
        return {'mode': 'risk', 'risk': min(value, 100.0) / 100.0, 'stop': float(stop_loss)}
    if mode == 'vol_target':
        return {'mode': 'vol_target', 'target': value / 100.0, 'lookback': VOL_LOOKBACK}
    return {'mode': 'percent', 'fraction': min(value, 100.0) / 100.0}


def describe_sizing(s: Dict[str, Any], k: int) -> str:
    slot = 'equity' if k == 1 else f'the symbol\'s slot (equity / {k})'
    m = s['mode']
    if m == 'fixed':
        return f"fixed: {s['amount']:,.2f} per entry (capped at available cash)"
    if m == 'risk':
        return (f"risk: {s['risk'] * 100:.2f}% of equity lost if the {s['stop'] * 100:.2f}% stop is hit - notional = "
                f"equity x {min(1.0, s['risk'] / s['stop']):.3f} per entry (capped at available cash)")
    if m == 'vol_target':
        return (f"vol_target: {slot} x min(1, {s['target'] * 100:.1f}% / realised {s['lookback']}-bar volatility at the "
                f"decision bar); no entry until the volatility can be measured; no leverage")
    return f"percent: {s['fraction'] * 100:.1f}% of {slot} per entry, capped by cash"


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
        sid = ST.resolve(strat.get('type') or strat.get('id') or 'sma_crossover')
    except ValueError as e:
        raise EngineInputError(str(e))
    kind = ST.kind_of(sid)
    given = strat.get('params') if isinstance(strat.get('params'), dict) else (
        strat.get('parameters') if isinstance(strat.get('parameters'), dict) else {})
    params = ST.defaults(sid)
    params.update({k: v for k, v in (given or {}).items() if v is not None})
    try:
        ST.build(sid, params)            # parameter types / ranges (a bad value is a plain INPUT error)
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
    ignored: List[str] = []
    sl, tp, ts = _frac(req, 'stopLoss'), _frac(req, 'takeProfit'), _frac(req, 'trailingStop')
    sizing = resolve_sizing(req, sl, ignored)
    allow_short = bool(req.get('allowShort', False))
    if kind == 'allocation':
        if sl or tp or ts:
            ignored.append('stop-loss / take-profit / trailing stop (not applied to an allocation algorithm: it '
                           'rebalances to its target weights)')
            sl = tp = ts = 0.0
        if allow_short:
            ignored.append('short selling (the allocation algorithms hold long target weights)')
            allow_short = False
        if sizing['mode'] != 'percent':
            ignored.append(f"position sizing '{sizing['mode']}' (an allocation algorithm sets its own weights; only "
                           f"'percent' scales them)")
            sizing = {'mode': 'percent', 'fraction': 1.0}
    lev = _num(req.get('leverage'), 1.0) or 1.0
    if lev != 1.0:
        ignored.append('leverage (positions are capped at available cash; no margin is modelled)')
    spec = Spec(symbols=syms, strategy=sid, params=params, kind=kind,
                start=str(req.get('startDate') or '')[:10], end=str(req.get('endDate') or '')[:10],
                interval=str(req.get('interval') or '1d'), initial_capital=float(cap), fill=fill, costs=costs,
                sizing=sizing, stop_loss=sl, take_profit=tp, trailing_stop=ts, allow_short=allow_short,
                fractional=bool(req.get('fractionalShares', True)), ignored=ignored)
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


def periods_per_year_by_symbol(frames: Dict[str, Any], symbols: List[str]) -> Dict[str, float]:
    from base.metrics import infer_periods_per_year
    return {s: float(infer_periods_per_year(frames[s].index)) for s in symbols}


# ════════════════════════════════════════════════════════════════════════════
# Decisions (the zipline-style algorithm) and execution (base.simulator)
# ════════════════════════════════════════════════════════════════════════════

class DecisionBook:
    """The bars of one panel + the algorithm's orders per parameter set (memoised: an optimisation or a
    walk-forward computes each parameter set's decisions once on the full history)."""

    def __init__(self, panel, frames: Dict[str, Any], symbols: List[str]):
        self.panel = panel
        self.symbols = list(symbols)
        self.bars = A.Bars(panel, frames, symbols)
        self._ppy = periods_per_year_by_symbol(frames, symbols)
        self._cache: Dict[tuple, A.Decisions] = {}
        self._vol: Dict[tuple, np.ndarray] = {}

    def decisions(self, strategy: str, params: Dict[str, Any]) -> A.Decisions:
        key = (strategy, tuple(sorted((k, repr(v)) for k, v in params.items())))
        hit = self._cache.get(key)
        if hit is not None:
            return hit
        try:
            init, handle = ST.build(strategy, params)
            dec = A.run_algorithm(init, handle, self.bars, params)
        except ValueError as e:
            raise EngineInputError(f'{strategy}: {e}')
        if len(self._cache) > 1024:
            self._cache.clear()
        self._cache[key] = dec
        return dec

    def vol_fractions(self, target: float, lookback: int) -> np.ndarray:
        """(n, k) min(1, target / realised volatility) on each decision bar (0 while not measurable)."""
        key = (float(target), int(lookback))
        hit = self._vol.get(key)
        if hit is None:
            out = np.zeros((self.panel.n, len(self.symbols)))
            for j, s in enumerate(self.symbols):
                v = self.bars.tas[s].vol(lookback, self._ppy.get(s, 252.0))
                with np.errstate(divide='ignore', invalid='ignore'):
                    f = np.where(np.isfinite(v) & (v > 0), np.minimum(1.0, target / v), 0.0)
                r = self.bars.rows[s]
                ok = r >= 0
                out[r[ok], j] = f[ok]
            hit = out
            self._vol[key] = hit
        return hit


def entry_sizes(spec: Spec, dbook: DecisionBook, mult: Optional[np.ndarray]):
    """(size, size_type) for Simulator.run in signals mode: a scalar or an (n, k) array of per-entry sizes on
    the decision bars, times the algorithm's order targets when they differ from 1."""
    s = spec.sizing
    k = len(dbook.symbols)
    n = dbook.panel.n
    if s['mode'] == 'fixed':
        base, stype = np.full((n, k), float(s['amount'])), 'value'
    elif s['mode'] == 'risk':
        base, stype = min(1.0, s['risk'] / s['stop']) * k, 'fraction'   # a share of the WHOLE equity per trade
    elif s['mode'] == 'vol_target':
        base, stype = dbook.vol_fractions(s['target'], s['lookback']), 'fraction'
    else:
        base, stype = float(s['fraction']), 'fraction'
    if mult is None:
        return base, stype
    return mult * base, stype


def make_simulator(spec: Spec, panel, backend: str = 'auto'):
    from base.simulator import Simulator
    sig = spec.kind == 'signals'
    return Simulator(panel, costs=spec.costs, fill=spec.fill, initial_capital=spec.initial_capital,
                     fractional=spec.fractional, stop_loss=spec.stop_loss if sig else 0.0,
                     take_profit=spec.take_profit if sig else 0.0, trailing_stop=spec.trailing_stop if sig else 0.0,
                     backend=backend)


def simulate(spec: Spec, sim, dbook: DecisionBook, params: Dict[str, Any], *, start: int = 0,
             rows: Optional[slice] = None, runs_hint: int = 1, carry: bool = False):
    """Run one parameter set on `sim` (whose panel may be the slice `rows` of the book's panel). carry=True
    (walk-forward test windows of an allocation algorithm): the latest decision before `start` is placed on
    the bar before it, so the window starts invested instead of waiting for the next scheduled decision."""
    rs = rows if rows is not None else slice(0, sim.n)
    dec = dbook.decisions(spec.strategy, params)
    if spec.kind == 'allocation':
        W = dec.targets * spec.fraction if spec.fraction != 1.0 else dec.targets
        Ww = W[rs].copy()
        if carry and start > 0:
            a = rs.start or 0
            prior = np.flatnonzero(np.isfinite(W[:a + start]).any(axis=1))
            if len(prior) and not np.isfinite(Ww[start - 1]).any():
                Ww[start - 1] = W[prior[-1]]
        return sim.run(weights=Ww, start=start, runs_hint=runs_hint)
    E, X, mult = dec.signals()
    kw = dict(entries=E[rs], exits=X[rs], start=start, runs_hint=runs_hint)
    if spec.allow_short:
        kw.update(short_entries=X[rs], short_exits=E[rs])
    size, stype = entry_sizes(spec, dbook, mult)
    if isinstance(size, np.ndarray):
        size = size[rs]
    return sim.run(size=size, size_type=stype, **kw)


def episode_trades(res) -> List[Dict[str, Any]]:
    from base.execution import position_episodes
    p = res.panel
    return position_episodes(res.fill_arrays, p.dates, p.symbols, last_close=p.close[-1], high=p.high, low=p.low,
                             borrow_per_bar=res.borrow, positions=res.positions,
                             dividends=p.dividends if p.adjustment == 'none' else None,
                             splits=p.splits if p.adjustment == 'none' else None)


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
    """Net P&L of the closed trades (round trips of signal algorithms = position episodes of allocation
    algorithms: a symbol's fill cash flows from flat to flat, commission and slippage included, borrow over the
    holding bars) - the optimiser's trade objectives, without building the trade dicts."""
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

EPISODE_NOTE = ('Trades of this allocation algorithm are POSITION EPISODES: one trade per symbol from the fill that '
                'opens its position until the position is flat again; rebalancing top-ups and trims stay inside the '
                'episode (their cash flows are part of its P&L), positions still held at the end are open trades '
                'marked to market. Win rate / profit factor describe whole positions, not rebalance fragments; '
                'turnover and per-symbol attribution describe the rebalancing.')
API_NOTE = ('Decided by the Fincept zipline-style algorithm API: initialize(context) requests full-history indicators '
            '(Fincept indicator table, computed once on each symbol\'s own bars - causal), handle_data(context, data) / '
            'schedule_function callbacks run once per bar after its close and place order_target_percent orders; '
            'base.simulator then executes the orders (the algorithm does not see its fills: order_target_percent is '
            'idempotent, a target the position already has places no trade).')


def _zipline_note() -> str:
    ok, v = zipline_reloaded()
    if ok:
        return (f'zipline-reloaded {v} is installed but not used: it runs only on ingested data bundles (an asset '
                f'database and bar files written per symbol set) on one exchange calendar, and its fills / costs '
                f'are modelled by base.simulator here (shared with the other engines).')
    return 'zipline-reloaded is not installed and not needed: the zipline-style API above runs on base.simulator.'


def _identity(spec: Spec, backend: str):
    from base.result_normalizer import EngineIdentity
    cm = spec.costs
    modelled = []
    if spec.allow_short and spec.kind == 'signals' and cm.borrow_rate_annual > 0:
        modelled.append('borrow cost')
    if cm.cash_rate_annual > 0:
        modelled.append('cash interest')
    if cm.impact_coef > 0:
        modelled.append('market impact')
    if cm.participation_cap:
        modelled += ['volume limits', 'partial fills']
    return EngineIdentity(ENGINE, implementation(spec.kind, backend, spec.fill), library=None, version=ENGINE_VERSION,
                          fill_mode=spec.fill, commission_itemised=True, slippage_itemised=True,
                          modelled=tuple(modelled), trade_mode='signals', notes=[])


def _execution_text(spec: Spec, sched: Optional[List[str]] = None) -> str:
    from base.execution import fill_model_block
    t = fill_model_block(spec.fill)['description']
    if spec.kind == 'allocation':
        when = ', '.join(sched or []) or 'on its schedule'
        return t + (f' The algorithm decides target weights at the close of the scheduled bars ({when}); each order '
                    f'rebalances that symbol to its weight at the fill above (sells before buys; commission and '
                    f'slippage paid out of the portfolio; an order for a closed market waits for its next session).')
    t += (' An order_target_percent(asset, 1) while flat is an entry, a target of 0 while holding an exit; the '
          'quantity is sized at the FILL price and capped by the available cash.')
    stops = []
    if spec.stop_loss:
        stops.append(f'stop-loss {spec.stop_loss * 100:g}%')
    if spec.take_profit:
        stops.append(f'take-profit {spec.take_profit * 100:g}%')
    if spec.trailing_stop:
        stops.append(f'trailing stop {spec.trailing_stop * 100:g}%')
    if stops:
        t += (' ' + ', '.join(stops) + ' checked INSIDE each bar against its high and low from the entry fill; a gap '
              'through the level fills at the open; stop before target when both are touched in one bar; stops pay '
              'slippage, targets fill at the level.')
    if spec.allow_short:
        t += ' Short selling allowed: a flat target opens a short, a long target covers it (borrow fee accrued).'
    return t


def assumptions_block(spec: Spec, data: Data, backend: str, k: int, sched: Optional[List[str]] = None,
                      extra_notes: Optional[List[str]] = None) -> Dict[str, Any]:
    from base.base_provider import _SURVIVORSHIP_NOTE
    if data.synthetic:
        src = 'SYNTHETIC random-walk prices (user opted in) - results have NO market meaning'
    elif data.source.startswith('broker:'):
        src = f'Broker candles ({data.source[7:]})'
    else:
        src = 'Yahoo Finance via the Fincept price cache (split/dividend-adjusted prices; trade prices are not ' \
              'historical quotes)'
    notes = [API_NOTE] + list(spec.notes) + list(extra_notes or [])
    if spec.kind == 'allocation':
        notes.append(EPISODE_NOTE)
        sizing = 'target weights from the algorithm' + (f', scaled to {spec.fraction * 100:.0f}% invested'
                                                        if spec.fraction < 1 else '')
    else:
        sizing = describe_sizing(spec.sizing, k)
        if k > 1:
            notes.append(f'One portfolio of {k} symbols with shared cash: each entry is sized on its slot of the equity '
                         f'(1/{k}) and held until its exit (v1 traded only the first symbol).')
    notes.append(_zipline_note())
    return {'engine': implementation(spec.kind, backend, spec.fill), 'execution': _execution_text(spec, sched),
            'costs': spec.costs.describe(), 'dataSource': src, 'survivorship': _SURVIVORSHIP_NOTE,
            'positionSizing': sizing + ('; fractional shares' if spec.fractional else '; whole shares'),
            'ignoredSettings': list(spec.ignored), 'notes': notes,
            'shares': 'fractional' if spec.fractional else 'whole', 'executedBy': 'base.simulator',
            'decidedBy': f'zipline-style algorithm {spec.strategy} (zl_strategies)'}


def cost_sensitivity_rows(spec: Spec, panel, dbook: DecisionBook, base_equity, req) -> Optional[List[Dict[str, Any]]]:
    """costs.sensitivity: the same orders, fills and sizing re-simulated with the trading costs at 0x, 2x and 3x
    the model (1x = this run); Sharpe at the request's constant rate."""
    from dataclasses import replace
    from base.costs import cost_sensitivity
    if spec.costs is None or spec.costs.is_zero():
        return None

    def sim_eq(cm):
        sp = replace(spec, costs=cm, notes=[], ignored=[])
        return simulate(sp, make_simulator(sp, panel), dbook, sp.params).equity
    return cost_sensitivity(sim_eq, spec.costs, panel.dates, rf=_num(req.get('riskFreeRate'), 0.0) or 0.0,
                            base_equity=base_equity)


def _ranking_warning(spec: Spec, k: int) -> Optional[Dict[str, Any]]:
    if spec.strategy not in ST.RANKS_SYMBOLS:
        return None
    top = int(round(float(spec.params.get('topN', 1) or 1)))
    if k == 1:
        what = 'only the positive-return screen applies' if spec.strategy == 'pipeline' else \
            'only absolute momentum (return > 0) applies'
        return {'code': 'TOPN_SELECTS_ALL', 'severity': 'info', 'symbol': None,
                'message': f'{spec.strategy} ranks symbols against each other; with one symbol the ranking has no '
                           f'effect and {what}. Add symbols to rank them.'}
    if top >= k:
        return {'code': 'TOPN_SELECTS_ALL', 'severity': 'info', 'symbol': None,
                'message': f'topN {top} >= {k} symbols: every symbol passes the ranking, so only the screen '
                           f'(positive return) selects.'}
    return None


def run_backtest(req: Dict[str, Any]) -> Dict[str, Any]:
    """The run_backtest command: {'success': True, 'data': contract v2} (raises EngineInputError /
    EngineDataError with a plain sentence)."""
    from base.costs import costs_by_year
    from base.metrics import _fmt_date
    from base.progress import CancelToken, Progress
    from base.result_normalizer import build_backtest_result
    t0 = time.perf_counter()
    cancel = CancelToken(req)
    spec = parse_request(req)
    bench = str(req.get('benchmarkSymbol') or req.get('benchmark') or '').strip() or None
    data = load_data(spec, req, benchmark=bench)
    cancel.check()
    k = len(data.symbols)
    panel = make_panel(data)
    prog = Progress('run', total=3, message=f'{spec.strategy} on {k} symbol(s)')
    dbook = DecisionBook(panel, data.frames, data.symbols)
    dec = dbook.decisions(spec.strategy, spec.params)
    prog.update(1, f'Algorithm placed {dec.orders} orders')
    cancel.check()
    warnings = list(data.warnings)
    res = simulate(spec, make_simulator(spec, panel), dbook, spec.params)
    prog.update(2, 'Execution done')
    cancel.check()
    for w in res.warnings:
        if w not in warnings:
            warnings.append(w)
    rw = _ranking_warning(spec, k)
    if rw:
        warnings.append(rw)
    decision_bars = dec.decision_bars()
    if spec.kind == 'signals':
        E = dec.signals()[0]
        if spec.strategy != 'buy_and_hold' and int(E.sum()) == 0:
            warnings.append({'code': 'NO_SIGNALS', 'severity': 'warn', 'symbol': None,
                             'message': f'The {spec.strategy} algorithm placed no entry order in this period with '
                                        f'these parameters.'})
        trades = res.trades()
    else:
        if not decision_bars:
            warnings.append({'code': 'NO_DECISIONS', 'severity': 'warn', 'symbol': None,
                             'message': f'The {spec.strategy} algorithm made no allocation decision in this period '
                                        f'(its lookback is longer than the data, or the schedule never fired).'})
        trades = episode_trades(res)
    backend = res.backend
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
    sens = cost_sensitivity_rows(spec, panel, dbook, res.equity, req)
    if sens:
        costs['sensitivity'] = sens
    extra: Dict[str, Any] = {}
    wb = res.weights_block()
    if wb:
        extra['weights'] = wb
    fill_bars = sorted(set(int(b) for b in res.fill_arrays['bar']))
    zl_ok, zl_v = zipline_reloaded()
    extra['algorithm'] = {
        'api': 'Fincept zipline-style (initialize / handle_data / schedule_function / order_target_percent)',
        'strategy': spec.strategy, 'executionMode': 'weights' if spec.kind == 'allocation' else 'signals',
        'orders': int(dec.orders), 'decisionBars': len(decision_bars), 'scheduled': list(dec.scheduled),
        'firstDecision': _fmt_date(panel.dates[decision_bars[0]]) if decision_bars else None,
        'lastDecision': _fmt_date(panel.dates[decision_bars[-1]]) if decision_bars else None,
        'decidedBy': 'zipline-style algorithm (zl_strategies)', 'executedBy': 'base.simulator',
        'ziplineReloaded': {'installed': zl_ok, 'version': zl_v, 'used': False}}
    if spec.kind == 'allocation':
        extra['rebalancing'] = {
            'schedule': ', '.join(dec.scheduled), 'decisions': len(decision_bars), 'rebalances': len(fill_bars),
            'firstDecision': extra['algorithm']['firstDecision'], 'lastDecision': extra['algorithm']['lastDecision'],
            'decidedBy': 'zipline-style algorithm (zl_strategies)', 'executedBy': 'base.simulator',
            'fills': int(len(res.fill_arrays['bar']))}
    logs = [f'{panel.n} bars x {k} symbol(s) {", ".join(data.symbols)}; strategy {spec.strategy} {spec.params}; '
            f'fill {spec.fill}; base.simulator {backend} ({res.mode} mode).',
            f'Algorithm: {dec.orders} orders on {len(decision_bars)} bars; {len(res.fill_arrays["bar"])} fills on '
            f'{len(fill_bars)} bars; final equity {float(res.equity[-1]):,.2f}.']
    warm = ST.default_warmup(spec.strategy, spec.params)
    data_v2 = build_backtest_result(
        nreq, dates=panel.dates, equity=res.equity, identity=_identity(spec, backend),
        trades=trades, fills=res.fills(), exposure=ex['gross'], net_exposure=ex['net'] if shorts else None,
        cash=res.cash, costs=costs, cost_model=spec.costs, frames=data.frames,
        price_provenance=data.provenance if not data.synthetic else None,
        assumptions=assumptions_block(spec, data, backend, k, dec.scheduled), warnings=warnings, logs=logs,
        warmup_bars=warm, using_synthetic=data.synthetic, extra=extra)
    # v1 blocks the screen still reads (BacktestTearsheet: BUY & HOLD, TIME IN MARKET, downside deviation)
    c = panel.close
    first = np.array([c[np.argmax(np.isfinite(c[:, j])), j] for j in range(k)])
    bh = float(np.nanmean(c[-1] / first - 1.0)) if k else None
    data_v2['extendedStats'] = {'buyAndHoldReturn': bh, 'exposureTime': data_v2['exposure'].get('timeInMarket'),
                                'buyAndHoldNote': 'holding ' + ('the symbol' if k == 1 else
                                                                f'the {k} symbols in equal parts') +
                                                  ' from the first to the last bar, no costs'}
    data_v2['riskMetrics'] = {'downsideDeviation': data_v2['performance'].get('downsideDeviation'),
                              'volatility': data_v2['performance'].get('volatility'),
                              'maxDrawdown': data_v2['performance'].get('maxDrawdown')}
    data_v2['provenance']['engineMs'] = int((time.perf_counter() - t0) * 1000)
    data_v2['provenance']['simulatorBackend'] = backend
    data_v2['provenance']['executedBy'] = 'base.simulator'
    data_v2['provenance']['decidedBy'] = 'zipline-style algorithm'
    prog.finish('Done')
    return {'success': True, 'data': data_v2}


# ════════════════════════════════════════════════════════════════════════════
# optimize / walk_forward (base.robustness + zl_evaluate.evaluate)
# ════════════════════════════════════════════════════════════════════════════

def _param_ranges(req: Dict[str, Any], spec: Spec) -> Dict[str, Any]:
    ranges = req.get('paramRanges') or req.get('parameters')
    if not isinstance(ranges, dict) or not ranges:
        raise EngineInputError('No parameter ranges supplied (paramRanges with min / max / step per parameter).')
    known = {p['id']: p for p in ST.catalog_entry(spec.strategy)['parameters']}
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
    keep = [c for c in combos if ST.check_constraints(spec.strategy, dict(base, **c)) is None]
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


def choose_backend(n: int, k: int, runs: int) -> str:
    from base.simulator import numba_available
    return 'numba' if (numba_available() and n * k * max(1, runs) >= 200_000) else 'python'


def _workers(req: Dict[str, Any], backend: str, trials: int, n: int, k: int) -> int:
    """Worker processes for base.robustness: a spawn worker costs ~1.5-2 s on Windows (imports), so a pool is
    used only when the estimated single-process compute exceeds ~4 s (never above the robustness /
    FINCEPT_BT_MAX_WORKERS / maxWorkers cap). Per trial: the algorithm's bar loop (~2.5 us per bar and symbol)
    + the simulation."""
    from base import robustness as R
    if req.get('maxWorkers'):                       # an explicit request is honoured (still capped)
        return R.max_workers(req, trials)
    per = 2.5e-6 * n * k + ((0.0005 + 3e-7 * n * k) if backend == 'numba' else 1.5e-5 * n * k)
    if trials * per < 4.0:
        return 1
    return R.max_workers(req, trials)


def _robust_impl(spec: Spec, backend: str) -> str:
    return (f'{implementation(spec.kind, backend, spec.fill)}; every combination is the same algorithm, costs, fills '
            f'and sizing as run_backtest, scored by base.robustness')


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
    rw = _ranking_warning(spec, len(data.symbols))
    if rw:
        out['warnings'].append(rw)
    notes = [API_NOTE] + list(spec.notes)
    if spec.kind == 'allocation':
        notes.append(EPISODE_NOTE)
    out['assumptions'] = {
        'engine': impl, 'execution': _execution_text(spec), 'fillModel': fill_model_block(spec.fill),
        'costs': spec.costs.describe(),
        'positionSizing': ('target weights from the algorithm' + (f' x {spec.fraction * 100:.0f}%'
                                                                  if spec.fraction < 1 else '')
                           if spec.kind == 'allocation' else describe_sizing(spec.sizing, len(data.symbols))),
        'shares': 'fractional' if spec.fractional else 'whole',
        'riskFreeRate': rf, 'riskFreeSource': 'request',
        'dataSource': ('SYNTHETIC random-walk prices (user opted in)' if data.synthetic else
                       (f'Broker candles ({data.source[7:]})' if data.source.startswith('broker:') else
                        'Yahoo Finance via the Fincept price cache (split/dividend-adjusted prices)')),
        'objectiveNote': f'Objectives use the request\'s risk-free rate ({rf * 100:.2f}% a year) as a constant; '
                         f'Sharpe = mean excess bar return / std x sqrt(bars per year) (base.metrics).',
        'ignoredSettings': list(spec.ignored), 'notes': notes}
    for s in spec.ignored:
        out['warnings'].append({'code': 'IGNORED_SETTING', 'severity': 'warn', 'symbol': None,
                                'message': f'Setting not applied by this engine: {s}.'})
    zl_ok, zl_v = zipline_reloaded()
    out['provenance'] = {
        'engine': ENGINE, 'engineImplementation': impl, 'engineVersion': ENGINE_VERSION, 'library': None,
        'libraryVersion': None, 'ziplineReloaded': {'installed': zl_ok, 'version': zl_v, 'used': False},
        'pythonVersion': platform.python_version(), 'command': command, 'dataSource': data.source,
        'interval': spec.interval, 'bars': int(panel.n), 'firstBar': _fmt_date(panel.dates[0]),
        'lastBar': _fmt_date(panel.dates[-1]), 'requestedStart': spec.start, 'requestedEnd': spec.end,
        'symbols': list(data.symbols), 'strategy': {'type': spec.strategy, 'name': (req.get('strategy') or {}).get('name')
                                                    if isinstance(req.get('strategy'), dict) else None},
        'baseParameters': dict(spec.params), 'seed': out.get('seed'), 'simulatorBackend': backend,
        'executedBy': 'base.simulator', 'decidedBy': 'zipline-style algorithm',
        'durationMs': int((time.perf_counter() - t0) * 1000), 'schemaVersion': 2}
    out.setdefault('logs', []).append(
        f'{panel.n} bars x {len(data.symbols)} symbol(s); {out.get("iterations") or out.get("combinationsPerWindow")} '
        f'combinations; workers {out.get("workers")}; backend {backend}; {out["provenance"]["durationMs"]} ms.')
    return out


def _setup_robust(req: Dict[str, Any], default_max: int):
    spec = parse_request(req)
    data = load_data(spec, req)
    panel = make_panel(data)
    combos, info, seed, warns = _combos(req, spec, default_max=default_max)
    from base import robustness as R
    try:
        objective = R.normalize_objective(req.get('optimizeObjective') or req.get('objective') or 'sharpe')
    except ValueError as e:
        raise EngineInputError(str(e))
    return spec, data, panel, combos, info, seed, warns, objective


def optimize(req: Dict[str, Any]) -> Dict[str, Any]:
    import zl_evaluate as ZE
    from base import robustness as R
    from base.metrics import infer_periods_per_year
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec, data, panel, combos, info, seed, warns, objective = _setup_robust(req, 500)
    backend = choose_backend(panel.n, len(data.symbols), len(combos))
    ctx = ZE.make_context(spec, panel, data.frames, data.symbols, backend)
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    hold = _num(req.get('holdoutFraction'), 0.0) or 0.0
    try:
        out = R.run_optimization(ZE.evaluate, combos, context=ctx, objective=objective,
                                 ppy=infer_periods_per_year(panel.dates), dates=panel.dates, rf=rf, info=info,
                                 request=req, seed=seed, holdout_fraction=hold, cancel=CancelToken(req),
                                 workers=_workers(req, backend, len(combos), panel.n, len(data.symbols)))
    except ValueError as e:
        raise EngineInputError(str(e))
    out['seed'] = seed
    return {'success': True, 'data': _robust_meta(out, spec, data, panel, req, backend, 'optimize', warns, rf, t0)}


def walk_forward(req: Dict[str, Any]) -> Dict[str, Any]:
    import zl_evaluate as ZE
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
    backend = choose_backend(panel.n, len(data.symbols), len(combos) * (len(windows) + 1))
    ctx = ZE.make_context(spec, panel, data.frames, data.symbols, backend)
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    try:
        out = R.run_walk_forward(ZE.evaluate, combos, windows, context=ctx, objective=objective,
                                 ppy=infer_periods_per_year(panel.dates), dates=panel.dates, rf=rf,
                                 initial_capital=spec.initial_capital, mode=mode, purge_bars=purge,
                                 embargo_bars=embargo, request=req, seed=seed, cancel=CancelToken(req),
                                 workers=_workers(req, backend, len(combos) * (len(windows) + 1), panel.n,
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
        out.setdefault('logs', []).append('Each test window starts from the latest decision made before it (filled on '
                                          'its first bar), then follows the schedule.')
    return {'success': True, 'data': _robust_meta(out, spec, data, panel, req, backend, 'walk_forward', warns, rf, t0)}
