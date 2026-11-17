"""
The "VectorBT" engine's backtest core, rebased on the shared backtesting core (base/).

WHAT RUNS
  Bars        base.price_store (one on-disk cache; zero downloads when warm), or broker candles the
              host injected (brokerCandles) when they cover every symbol.
  Signals     vbt_strategies builders on each symbol's OWN bars (vectorised, causal, memoised
              indicators from vbt_indicators.TA), mapped onto the portfolio timeline.
  Simulation  base.simulator: ONE portfolio with shared cash for any number of symbols. Signal
              strategies use its signals mode (entry size = sizing rule x equity / symbols, capped by
              cash; intrabar stop-loss / take-profit / trailing stop on the bar's high / low, gaps fill
              at the open); allocation strategies (rebalance_periodic) its weights mode (a real target-
              weight rebalance; costs paid out of the portfolio). The numba-compiled kernel is used when
              it pays off (optimisation, many symbols / bars); small single runs use the identical
              Python kernel (no numba import or JIT load). The vectorbt LIBRARY is not used - the
              engine keeps its name for continuity and every result says what ran.
  Fills       request fillTiming: next_open (default) | next_close | same_close.
  Costs       base.costs.CostModel.from_request: commission and slippage itemised per fill / trade.
  Results     base.result_normalizer.build_backtest_result: every headline metric from base.metrics,
              open positions listed as open trades (exitReason 'open', marked to market).

SIZING (request positionSizing / positionSizeValue, as the screen sends them)
  percent     value % of the symbol's slot (equity / number of symbols) per entry
  fixed       value in money per entry (capped by cash)
  risk        value % of equity lost if the stop-loss is hit: notional = equity x risk / stop (needs
              stopLoss; capped by cash)
  vol_target  slot x min(1, target % / realised 20-bar annualised volatility at the decision bar);
              flat while the volatility is not yet measurable
  kelly       slot x min(cap %, Kelly fraction of the trades CLOSED before the decision bar); the cap is
              used until 10 trades have closed (no look-ahead; solved as a fixed point because the
              trades depend on the sizes)
  `weights` (one per symbol) scale each symbol's slot (w_j / sum w of the equity instead of 1 / k).
"""

from __future__ import annotations

import math
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

import vbt_indicators as I
import vbt_strategies as ST

ENGINE = 'vectorbt'
ENGINE_VERSION = '2.0.0'
SIZING_MODES = ('percent', 'fixed', 'kelly', 'vol_target', 'risk')
KELLY_MIN_TRADES = 10
VOL_LOOKBACK = 20


def implementation(backend: str) -> str:
    if backend == 'events':
        return ('Fincept vector simulator (base.simulator arithmetic replayed on fill bars only - bit-identical to '
                'base.simulator for one long/flat symbol) - the vectorbt library is not used')
    k = 'numba' if backend == 'numba' else 'Python kernel'
    return f'Fincept vector simulator (base.simulator, {k}) - the vectorbt library is not used'


def _log(msg: str) -> None:
    import sys
    print(f'[vbt] {msg}', file=sys.stderr, flush=True)


class EngineInputError(ValueError):
    """A request the engine cannot run (plain sentence for the user)."""


class EngineDataError(RuntimeError):
    """Market data could not be loaded (plain sentence, DATA error code)."""


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
    weights: Optional[np.ndarray]
    fractional: bool
    ignored: List[str] = field(default_factory=list)
    notes: List[str] = field(default_factory=list)
    custom_code: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        """Picklable description for optimisation workers (the cost model as a dict)."""
        from dataclasses import asdict
        d = {k: getattr(self, k) for k in self.__dataclass_fields__ if k != 'costs'}
        d['costs'] = asdict(self.costs)
        d['weights'] = None if self.weights is None else [float(x) for x in self.weights]
        return d


def spec_from_dict(d: Dict[str, Any]) -> Spec:
    from base.costs import CostModel
    kw = dict(d)
    kw['costs'] = CostModel(**d['costs'])
    kw['weights'] = None if d.get('weights') is None else np.asarray(d['weights'], dtype=float)
    return Spec(**kw)


def resolve_sizing(req: Dict[str, Any], stop_loss: float) -> Dict[str, Any]:
    mode = req.get('positionSizing', 'percent')
    if isinstance(mode, dict):
        mode = mode.get('mode', 'percent')
    mode = str(mode or 'percent').strip().lower().replace('-', '_').replace(' ', '_')
    mode = {'volatility': 'vol_target', 'voltarget': 'vol_target', 'pct': 'percent', 'percentage': 'percent',
            'fixed_amount': 'fixed', 'risk_per_trade': 'risk'}.get(mode, mode)
    if mode not in SIZING_MODES:
        raise EngineInputError(f'position sizing must be one of {", ".join(SIZING_MODES)}, not {mode!r}')
    value = _num(req.get('positionSizeValue'), None)
    if value is None or value <= 0:
        value = 100.0 if mode in ('percent', 'kelly') else (10.0 if mode == 'vol_target' else
                                                           (1.0 if mode == 'risk' else 10000.0))
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
    if mode == 'kelly':
        return {'mode': 'kelly', 'cap': min(value, 100.0) / 100.0, 'min_trades': KELLY_MIN_TRADES}
    return {'mode': 'percent', 'fraction': min(value, 100.0) / 100.0}


def describe_sizing(s: Dict[str, Any], k: int, weighted: bool = False) -> str:
    slot = 'equity' if k == 1 else (f'the symbol\'s weight x equity (portfolio weights)' if weighted else
                                    f'the symbol\'s slot (equity / {k})')
    m = s['mode']
    if m == 'fixed':
        return f"fixed: {s['amount']:,.2f} per entry (capped at available cash)"
    if m == 'risk':
        return (f"risk: {s['risk'] * 100:.2f}% of equity lost if the {s['stop'] * 100:.2f}% stop is hit - notional "
                f"= equity x {min(1.0, s['risk'] / s['stop']):.3f} per entry (capped at available cash)")
    if m == 'vol_target':
        return (f"vol_target: {slot} x min(1, {s['target'] * 100:.1f}% / realised {s['lookback']}-bar volatility at "
                f"the decision bar); no entry until the volatility can be measured; no leverage")
    if m == 'kelly':
        return (f"kelly: {slot} x min({s['cap'] * 100:.1f}%, Kelly fraction of the trades closed before the "
                f"decision bar); the cap is used until {s['min_trades']} trades have closed (no look-ahead)")
    return f"percent: {s['fraction'] * 100:.1f}% of {slot} per entry"


def parse_request(req: Dict[str, Any]) -> Spec:
    from base.costs import CostModel
    from base.execution import fill_mode_from_request
    from base.price_store import normalize_symbol
    symbols = req.get('symbols') or [a.get('symbol') for a in (req.get('assets') or []) if isinstance(a, dict)]
    syms: List[str] = []
    for s in symbols or []:
        ns = normalize_symbol(s)
        if ns and ns not in syms:
            syms.append(ns)
    if not syms:
        raise EngineInputError('No symbols provided.')
    strat = req.get('strategy') if isinstance(req.get('strategy'), dict) else {}
    stype = str(strat.get('type') or 'sma_crossover')
    code = None
    if stype == 'code':
        c = strat.get('code')
        code = c.get('source', '') if isinstance(c, dict) else str(c or '')
        if not code.strip():
            raise EngineInputError('A custom code strategy needs its source in strategy.code.')
        sid, kind = 'code', 'signals'
    else:
        try:
            sid = ST.resolve(stype)
        except ValueError as e:
            raise EngineInputError(str(e))
        kind = ST.kind_of(sid)
    given = strat.get('params') if isinstance(strat.get('params'), dict) else (
        strat.get('parameters') if isinstance(strat.get('parameters'), dict) else {})
    params = {}
    if sid != 'code':
        cat = next((c for c in ST.CATALOG if c['type'] == sid), None)
        params = {p['id']: p['default'] for p in (cat or {}).get('parameters', [])}
    params.update(given or {})
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
    sl, tp, ts = _frac(req, 'stopLoss'), _frac(req, 'takeProfit'), _frac(req, 'trailingStop')
    sizing = resolve_sizing(req, sl)
    w = req.get('weights')
    weights = None
    if isinstance(w, (list, tuple)) and len(w) == len(syms):
        arr = np.array([_num(x, 0.0) or 0.0 for x in w], dtype=float)
        if np.any(arr < 0):
            raise EngineInputError('Portfolio weights must not be negative.')
        if arr.sum() > 0:
            weights = arr / arr.sum()
    spec = Spec(symbols=syms, strategy=sid, params=params, kind=kind,
                start=str(req.get('startDate') or '')[:10], end=str(req.get('endDate') or '')[:10],
                interval=str(req.get('interval') or '1d'), initial_capital=float(cap), fill=fill, costs=costs,
                sizing=sizing, stop_loss=sl, take_profit=tp, trailing_stop=ts,
                allow_short=bool(req.get('allowShort', False)), weights=weights,
                fractional=bool(req.get('fractionalShares', True)), custom_code=code)
    if not spec.start or not spec.end:
        raise EngineInputError('startDate and endDate are required.')
    lev = _num(req.get('leverage'), 1.0) or 1.0
    if lev != 1.0:
        spec.ignored.append('leverage (positions are capped at available cash; no margin is modelled)')
    if kind == 'allocation':
        if sl or tp or ts:
            spec.ignored.append('stop-loss / take-profit / trailing stop (not applied to a rebalancing strategy)')
        if spec.allow_short:
            spec.ignored.append('short selling (a rebalancing strategy holds long target weights)')
        if sizing['mode'] not in ('percent',):
            spec.ignored.append(f"position sizing '{sizing['mode']}' (a rebalancing strategy sets its own weights; "
                                f"only 'percent' scales them)")
    return spec


# ════════════════════════════════════════════════════════════════════════════
# Data
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class Data:
    frames: Dict[str, Any]                 # symbol -> OHLCV DataFrame (traded symbols + benchmark)
    symbols: List[str]                     # symbols that loaded, in request order
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
    """Bars of every requested symbol (+ the benchmark) in ONE price-store call. A symbol that cannot
    be loaded is dropped with a SYMBOL_DROPPED warning; when none loads the run fails with each
    symbol's reason (synthetic prices only when the user allowed them)."""
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
    """Rows [a, e) of a Panel (walk-forward windows)."""
    from base.execution import Panel
    if a == 0 and e == panel.n:
        return panel
    sl = slice(a, e)
    return Panel(dates=panel.dates[sl], symbols=list(panel.symbols), open=panel.open[sl], high=panel.high[sl],
                 low=panel.low[sl], close=panel.close[sl], volume=panel.volume[sl], tradable=panel.tradable[sl],
                 dividends=None if panel.dividends is None else panel.dividends[sl],
                 splits=None if panel.splits is None else panel.splits[sl], adjustment=panel.adjustment)


def frame_rows(panel, frame) -> np.ndarray:
    """Panel row of every bar of one symbol's own frame."""
    import pandas as pd
    idx = pd.DatetimeIndex(frame.index)
    if idx.tz is not None:
        idx = idx.tz_localize(None)
    rows = panel.dates.get_indexer(idx)
    return rows


def make_ta(frame) -> I.TA:
    f = frame
    return I.TA(f['Open'].to_numpy(float) if 'Open' in f else None, f['High'].to_numpy(float) if 'High' in f else None,
                f['Low'].to_numpy(float) if 'Low' in f else None, f['Close'].to_numpy(float),
                f['Volume'].to_numpy(float) if 'Volume' in f else None)


# ════════════════════════════════════════════════════════════════════════════
# Signals, weights, sizes
# ════════════════════════════════════════════════════════════════════════════

def _custom_signals(code: str, frame, params) -> Tuple[np.ndarray, np.ndarray]:
    """strategy.type 'code': runs the caller's Python AS THE LOCAL USER (the terminal never sends it on its
    own). The code gets pd, np, data (close Series), open / high / low / volume Series and params, and must
    set `entries` and `exits` (booleans, one per bar)."""
    import pandas as pd
    ctx = {'pd': pd, 'np': np, 'data': frame['Close'].astype(float), 'close': frame['Close'].astype(float),
           'open': frame.get('Open'), 'high': frame.get('High'), 'low': frame.get('Low'),
           'volume': frame.get('Volume'), 'params': dict(params or {})}
    exec(code, ctx)  # noqa: S102 - documented: custom code runs as the local user
    if 'entries' not in ctx or 'exits' not in ctx:
        raise EngineInputError('Custom code must set `entries` and `exits` (booleans, one per bar). Building a '
                               '`portfolio` object is no longer supported: the engine simulates the signals itself.')
    e = np.asarray(ctx['entries'], dtype=float)
    x = np.asarray(ctx['exits'], dtype=float)
    if len(e) != len(frame) or len(x) != len(frame):
        raise EngineInputError(f'Custom code returned {len(e)} entries / {len(x)} exits for {len(frame)} bars.')
    return np.nan_to_num(e) != 0, np.nan_to_num(x) != 0


class SignalBook:
    """Per-symbol TA accessors + signal matrices of one panel (memoised per parameter set)."""

    def __init__(self, panel, frames: Dict[str, Any], symbols: List[str]):
        self.panel = panel
        self.symbols = list(symbols)
        self.tas = {s: make_ta(frames[s]) for s in symbols}
        self.rows = {s: frame_rows(panel, frames[s]) for s in symbols}
        self.frames = frames
        self._cache: Dict[tuple, Tuple[np.ndarray, np.ndarray]] = {}

    def signals(self, strategy: str, params: Dict[str, Any], code: Optional[str] = None):
        key = (strategy, tuple(sorted((k, repr(v)) for k, v in params.items())))
        hit = self._cache.get(key)
        if hit is not None:
            return hit
        n, k = self.panel.n, len(self.symbols)
        E = np.zeros((n, k), dtype=bool)
        X = np.zeros((n, k), dtype=bool)
        for j, s in enumerate(self.symbols):
            if strategy == 'code':
                e, x = _custom_signals(code or '', self.frames[s], params)
            else:
                e, x = ST.build_signals(strategy, self.tas[s], params)
            r = self.rows[s]
            ok = r >= 0
            E[r[ok], j] = e[ok]
            X[r[ok], j] = x[ok]
        if len(self._cache) > 4096:
            self._cache.clear()
        self._cache[key] = (E, X)
        return E, X


def allocation_weights(spec: Spec, panel, params: Dict[str, Any]) -> np.ndarray:
    """Target-weight matrix of rebalance_periodic: weights on decision bars, NaN elsewhere. A symbol
    with no price yet (not listed) gets 0 and the others share its weight."""
    n, k = panel.n, len(panel.symbols)
    flag = ST.rebalance_bars(panel.dates, params.get('rebalancePeriod', 63))
    base = spec.weights if spec.weights is not None else np.full(k, 1.0 / k)
    invest = spec.sizing.get('fraction', 1.0) if spec.sizing['mode'] == 'percent' else 1.0
    listed = np.isfinite(panel.close)
    W = np.full((n, k), np.nan)
    for i in np.flatnonzero(flag):
        w = np.where(listed[i], base, 0.0)
        tot = w.sum()
        if tot > 0:
            W[i] = w / tot * invest
    return W


def slot_multipliers(spec: Spec, k: int) -> np.ndarray:
    """Per-symbol multiple of the equal slot (equity / k): k x w_j for portfolio weights, else 1."""
    if spec.weights is None:
        return np.ones(k)
    return spec.weights * k


def entry_sizes(spec: Spec, book: SignalBook, ppy: Dict[str, float]):
    """(size, size_type) for Simulator.run, or ('kelly', None). size is a scalar or an (n, k) array
    of per-entry sizes on the decision bars."""
    s = spec.sizing
    k = len(book.symbols)
    mult = slot_multipliers(spec, k)
    if s['mode'] == 'kelly':
        return 'kelly', None
    if s['mode'] == 'fixed':
        return np.tile(s['amount'] * mult, (book.panel.n, 1)), 'value'
    if s['mode'] == 'risk':
        # risk is a share of the WHOLE equity per trade (portfolio weights do not change it)
        return min(1.0, s['risk'] / s['stop']) * k, 'fraction'
    if s['mode'] == 'vol_target':
        n = book.panel.n
        out = np.zeros((n, k))
        for j, sym in enumerate(book.symbols):
            v = book.tas[sym].vol(s['lookback'], ppy.get(sym, 252.0))
            with np.errstate(divide='ignore', invalid='ignore'):
                f = np.where(np.isfinite(v) & (v > 0), np.minimum(1.0, s['target'] / v), 0.0)
            r = book.rows[sym]
            ok = r >= 0
            out[r[ok], j] = f[ok] * mult[j]
        return out, 'fraction'
    f = s['fraction']
    if spec.weights is None:
        return f, 'fraction'
    return np.tile(f * mult, (book.panel.n, 1)), 'fraction'


# ════════════════════════════════════════════════════════════════════════════
# Simulation
# ════════════════════════════════════════════════════════════════════════════

def make_simulator(spec: Spec, panel, backend: str = 'auto'):
    from base.simulator import Simulator
    return Simulator(panel, costs=spec.costs, fill=spec.fill, initial_capital=spec.initial_capital,
                     fractional=spec.fractional, stop_loss=spec.stop_loss if spec.kind == 'signals' else 0.0,
                     take_profit=spec.take_profit if spec.kind == 'signals' else 0.0,
                     trailing_stop=spec.trailing_stop if spec.kind == 'signals' else 0.0, backend=backend)


def round_trips(fa: Dict[str, np.ndarray], k: int):
    """Closed round trips from simulator fills of a signals-mode run (one position per symbol):
    (asset, entry bar, exit bar, pnl, return on the entry notional). Open positions are skipped."""
    bar, asset, qty, px, comm = fa['bar'], fa['asset'], fa['qty'], fa['price'], fa['commission']
    open_q = np.zeros(k)
    open_cost = np.zeros(k)
    open_bar = np.zeros(k, dtype=np.int64)
    open_basis = np.zeros(k)
    out_a, out_e, out_x, out_p, out_r = [], [], [], [], []
    for i in range(len(bar)):
        j = int(asset[i])
        q = float(qty[i])
        cf = -q * float(px[i]) - float(comm[i])
        if open_q[j] == 0.0:
            open_q[j] = q
            open_cost[j] = cf
            open_bar[j] = bar[i]
            open_basis[j] = abs(q) * float(px[i])
            continue
        if abs(open_q[j] + q) <= 1e-9 * max(1.0, abs(q)):
            pnl = open_cost[j] + cf
            out_a.append(j)
            out_e.append(int(open_bar[j]))
            out_x.append(int(bar[i]))
            out_p.append(pnl)
            out_r.append(pnl / open_basis[j] if open_basis[j] > 0 else 0.0)
            open_q[j] = 0.0
        else:  # partial change (not produced by signals mode): fold into the open lot
            open_q[j] += q
            open_cost[j] += cf
    return (np.asarray(out_a, dtype=np.int64), np.asarray(out_e, dtype=np.int64), np.asarray(out_x, dtype=np.int64),
            np.asarray(out_p, dtype=float), np.asarray(out_r, dtype=float))


def trade_extras(fa: Dict[str, np.ndarray], k: int) -> Dict[str, Any]:
    _, _, _, pnl, _ = round_trips(fa, k)
    n = len(pnl)
    if n == 0:
        return {'trades': 0, 'winRate': None, 'profitFactor': None, 'sqn': None}
    wins, losses = pnl[pnl > 0], pnl[pnl < 0]
    pf = float(wins.sum() / -losses.sum()) if len(losses) and losses.sum() < 0 else None
    sd = float(np.std(pnl, ddof=1)) if n > 1 else 0.0
    sqn = float(math.sqrt(n) * pnl.mean() / sd) if sd > 0 else None
    return {'trades': int(n), 'winRate': float(len(wins) / n), 'profitFactor': pf, 'sqn': sqn}


def kelly_sizes(fa, n: int, k: int, cap: float, min_trades: int, mult: np.ndarray) -> np.ndarray:
    """Per-decision-bar Kelly fraction from the trades closed by each bar (v1 rule: losses are
    returns <= 0; no wins -> 0; no losses -> cap; cap until min_trades have closed)."""
    a, e, x, pnl, ret = round_trips(fa, k)
    order = np.argsort(x, kind='stable')
    frac = np.full(n, cap)
    closed: List[float] = []
    last = 0
    for t in order:
        bar = int(x[t])
        if bar > last:
            frac[last:bar] = _kelly(closed, cap, min_trades)
            last = bar
        closed.append(float(ret[t]))
    frac[last:] = _kelly(closed, cap, min_trades)
    return frac[:, None] * mult[None, :]


def _kelly(rets: List[float], cap: float, min_trades: int) -> float:
    if len(rets) < min_trades:
        return cap
    r = np.asarray(rets)
    wins, losses = r[r > 0], r[r <= 0]
    if len(wins) == 0:
        return 0.0
    if len(losses) == 0 or abs(losses.mean()) < 1e-12:
        return cap
    w = len(wins) / len(r)
    payoff = wins.mean() / abs(losses.mean())
    return float(min(cap, max(0.0, w - (1.0 - w) / payoff)))


def simulate(spec: Spec, sim, book: Optional[SignalBook], params: Dict[str, Any], *, start: int = 0,
             rows: Optional[slice] = None, runs_hint: int = 1, ppy: Optional[Dict[str, float]] = None,
             notes: Optional[List[str]] = None):
    """Run one parameter set on `sim` (whose panel may be a slice `rows` of the book's panel)."""
    rs = rows if rows is not None else slice(0, sim.n)
    if spec.kind == 'allocation':
        W = allocation_weights(spec, book.panel, params)[rs]
        return sim.run(weights=W, start=start, runs_hint=runs_hint)
    E, X = book.signals(spec.strategy, params, spec.custom_code)
    E, X = E[rs], X[rs]
    kw = dict(entries=E, exits=X, start=start, runs_hint=runs_hint)
    if spec.allow_short:
        kw.update(short_entries=X, short_exits=E)
    size, stype = entry_sizes(spec, book, ppy or {})
    if isinstance(size, str) and size == 'kelly':
        k = len(book.symbols)
        mult = slot_multipliers(spec, k)
        sz = np.full((sim.n, k), spec.sizing['cap']) * mult[None, :]
        res = None
        for it in range(8):
            res = sim.run(size=sz, size_type='fraction', **kw)
            new = kelly_sizes(res.fill_arrays, sim.n, k, spec.sizing['cap'], spec.sizing['min_trades'], mult)
            ent_rows = E | X if spec.allow_short else E
            if np.allclose(new[ent_rows], sz[ent_rows], rtol=0, atol=1e-12):
                break
            sz = new
        else:
            if notes is not None:
                notes.append('Kelly sizing did not reach a fixed point in 8 passes; the last pass is reported.')
        return res
    if isinstance(size, np.ndarray):
        size = size[rs]
    return sim.run(size=size, size_type=stype, **kw)


def periods_per_year_by_symbol(frames: Dict[str, Any], symbols: List[str]) -> Dict[str, float]:
    from base.metrics import infer_periods_per_year
    return {s: float(infer_periods_per_year(frames[s].index)) for s in symbols}


# ════════════════════════════════════════════════════════════════════════════
# run_backtest
# ════════════════════════════════════════════════════════════════════════════

def _identity(spec: Spec, backend: str, notes: List[str]):
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
    # Allocation runs report POSITION EPISODES (base.execution.position_episodes), not FIFO fragments of the
    # rebalance fills, so their trade list is a complete strategy-level list ('signals' trade mode).
    return EngineIdentity(ENGINE, implementation(backend), library=None, version=ENGINE_VERSION, fill_mode=spec.fill,
                          commission_itemised=True, slippage_itemised=True, modelled=tuple(modelled),
                          trade_mode='signals', notes=list(notes))


EPISODE_NOTE = ('Trades of this rebalancing strategy are POSITION EPISODES: one trade per symbol from the fill that '
                'opens its position until it is flat again; rebalancing top-ups and trims stay inside the episode '
                '(their cash flows are part of its P&L) and positions still held at the end are open trades marked '
                'to market. Win rate / profit factor describe whole positions, not rebalance fragments; turnover and '
                'per-symbol attribution describe the rebalancing.')


def episode_trades(res) -> List[Dict[str, Any]]:
    """Trades of a weights-mode (rebalancing) run as position episodes."""
    from base.execution import position_episodes
    p = res.panel
    return position_episodes(res.fill_arrays, p.dates, p.symbols, last_close=p.close[-1], high=p.high, low=p.low,
                             borrow_per_bar=res.borrow, positions=res.positions,
                             dividends=p.dividends if p.adjustment == 'none' else None,
                             splits=p.splits if p.adjustment == 'none' else None)


def _execution_text(spec: Spec) -> str:
    from base.execution import fill_model_block
    t = fill_model_block(spec.fill)['description']
    if spec.kind == 'allocation':
        return t + ' Target weights are traded at that fill (sells before buys; commission and slippage paid out of ' \
                   'the portfolio).'
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
        t += ' Short selling allowed: an exit signal opens a short, an entry signal covers it (borrow fee accrued).'
    return t


def assumptions_block(spec: Spec, data: Data, backend: str, k: int) -> Dict[str, Any]:
    from base.base_provider import _SURVIVORSHIP_NOTE
    if data.synthetic:
        src = 'SYNTHETIC random-walk prices (user opted in) - results have NO market meaning'
    elif data.source.startswith('broker:'):
        src = f'Broker candles ({data.source[7:]})'
    else:
        src = 'Yahoo Finance via the Fincept price cache (split/dividend-adjusted prices; trade prices are not ' \
              'historical quotes)'
    notes = list(spec.notes)
    if k > 1:
        notes.append(f'One portfolio of {k} symbols with shared cash' + (
            ': target weights rebalanced by the strategy.' if spec.kind == 'allocation' else
            f': each entry is sized on its slot of the equity (1/{k}' +
            (' scaled by the portfolio weights' if spec.weights is not None else '') + '), capped by cash.'))
    if spec.strategy == 'code':
        notes.append('Custom strategy code was executed as the local user.')
    return {'engine': implementation(backend), 'execution': _execution_text(spec), 'costs': spec.costs.describe(),
            'dataSource': src, 'survivorship': _SURVIVORSHIP_NOTE,
            'positionSizing': ('target weights ' + ('from the request' if spec.weights is not None else 'equal') +
                               (f", scaled to {spec.sizing['fraction'] * 100:.0f}% invested"
                                if spec.sizing.get('fraction', 1.0) < 1 else '')) if spec.kind == 'allocation'
            else describe_sizing(spec.sizing, k, spec.weights is not None and k > 1),
            'ignoredSettings': list(spec.ignored), 'notes': notes, 'shares': 'fractional' if spec.fractional
            else 'whole'}


def merge_open_lots(trades: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """A rebalanced portfolio leaves one open FIFO lot per buy; list ONE open position per symbol
    (quantity-weighted entry price, summed money fields, earliest entry) - closed fragments stay."""
    closed = [t for t in trades if t.get('exitDate')]
    opens: Dict[str, List[Dict[str, Any]]] = {}
    for t in trades:
        if not t.get('exitDate'):
            opens.setdefault(f"{t.get('symbol')}|{t.get('side')}", []).append(t)
    merged = []
    for lots in opens.values():
        if len(lots) == 1:
            merged.append(lots[0])
            continue
        q = sum(float(x['quantity']) for x in lots)
        first = min(lots, key=lambda x: x['entryBar'])
        m = dict(first)
        m['quantity'] = q
        m['entryPrice'] = sum(float(x['quantity']) * float(x['entryPrice']) for x in lots) / q if q > 0 else None
        for key in ('pnl', 'grossPnl', 'commission', 'slippageCost', 'borrowCost', 'dividends'):
            vals = [x.get(key) for x in lots]
            m[key] = None if any(v is None for v in vals) else float(sum(vals))
        if m.get('pnl') is not None and m.get('entryPrice'):
            m['pnlPercent'] = m['pnl'] / (m['entryPrice'] * q)
        maes = [x['mae'] for x in lots if x.get('mae') is not None]
        mfes = [x['mfe'] for x in lots if x.get('mfe') is not None]
        m['mae'] = min(maes) if maes else None
        m['mfe'] = max(mfes) if mfes else None
        m['mergedLots'] = len(lots)
        merged.append(m)
    merged.sort(key=lambda t: t['entryBar'])
    out = closed + merged
    for i, t in enumerate(out, 1):
        t['id'] = f't{i}'
    return out


def weights_block(res, panel, max_rows: int = 600) -> Optional[Dict[str, Any]]:
    k = len(panel.symbols)
    if k < 2:
        return None
    n = panel.n
    step = max(1, int(math.ceil(n / max_rows)), 5 if n > 5 * 52 else 1)
    vals = res.position_values()
    eq = res.equity
    rows = list(range(0, n, step))
    if rows[-1] != n - 1:
        rows.append(n - 1)
    from base.metrics import _fmt_date
    return {'t': [_fmt_date(panel.dates[i]) for i in rows], 'symbols': list(panel.symbols),
            'w': [[round(float(vals[i, j] / eq[i]), 5) if eq[i] > 0 else None for j in range(k)] for i in rows]}


def cost_sensitivity_rows(spec: Spec, panel, book, ppy, base_equity, req) -> Optional[List[Dict[str, Any]]]:
    """costs.sensitivity: the same strategy, fills and sizing re-simulated with the trading costs at 0x, 2x
    and 3x the model (1x = this run). Sharpe column at the request's constant rate (the headline Sharpe may
    use the ^IRX series). None for a cost-free model (every row would be identical)."""
    from dataclasses import replace
    from base.costs import cost_sensitivity
    if spec.costs is None or spec.costs.is_zero():
        return None

    def sim_eq(cm):
        sp = replace(spec, costs=cm, notes=[])
        return simulate(sp, make_simulator(sp, panel), book, sp.params, ppy=ppy, notes=[]).equity
    return cost_sensitivity(sim_eq, spec.costs, panel.dates, rf=_num(req.get('riskFreeRate'), 0.0) or 0.0,
                            base_equity=base_equity)


def run_backtest(req: Dict[str, Any]) -> Dict[str, Any]:
    """The run_backtest command: {'success': True, 'data': contract v2} (raises EngineInputError /
    EngineDataError with a plain sentence)."""
    from base.progress import CancelToken, Progress
    from base.result_normalizer import build_backtest_result
    t0 = time.perf_counter()
    cancel = CancelToken(req)
    spec = parse_request(req)
    bench = str(req.get('benchmarkSymbol') or req.get('benchmark') or '').strip() or None
    data = load_data(spec, req, benchmark=bench)
    cancel.check()
    panel = make_panel(data)
    k = len(data.symbols)
    prog = Progress('run', total=k + 1, message=f'Simulating {spec.strategy} on {k} symbol(s)')
    book = SignalBook(panel, data.frames, data.symbols)
    if spec.kind == 'signals':
        book.signals(spec.strategy, spec.params, spec.custom_code)   # builds (and caches) every symbol's signals
    prog.update(k, 'Signals ready')
    cancel.check()
    notes: List[str] = []
    sim = make_simulator(spec, panel)
    ppy = periods_per_year_by_symbol(data.frames, data.symbols)
    res = simulate(spec, sim, book, spec.params, ppy=ppy, notes=notes)
    prog.finish('Simulation done')
    spec.notes.extend(notes)
    ex = res.exposure_series()
    shorts = spec.allow_short and spec.kind == 'signals'
    warmup = ST.default_warmup(spec.strategy, spec.params) if spec.strategy != 'code' else None
    nreq = dict(req)
    nreq['symbols'] = list(data.symbols)
    strat = dict(req.get('strategy') or {}) if isinstance(req.get('strategy'), dict) else {}
    strat['type'] = spec.strategy
    strat['params'] = dict(spec.params)
    nreq['strategy'] = strat
    if bench:
        nreq['benchmarkSymbol'] = bench
    warnings = list(data.warnings) + [w for w in res.warnings if w not in data.warnings]
    if spec.kind == 'signals' and spec.strategy not in ('buy_and_hold', 'code'):
        n_e = int(book.signals(spec.strategy, spec.params, spec.custom_code)[0].sum())
        if n_e == 0:
            warnings.append({'code': 'NO_SIGNALS', 'severity': 'warn', 'symbol': None,
                             'message': f'The {spec.strategy} rule produced no entry signal in this period with these '
                                        f'parameters.'})
    logs = [f'{len(panel.dates)} bars x {k} symbol(s) {", ".join(data.symbols)}; strategy {spec.strategy} '
            f'{spec.params}; fill {spec.fill}; simulator backend {res.backend}.',
            f'{len(res.fill_arrays["bar"])} fills; final equity {float(res.equity[-1]):,.2f}.']
    extra = {}
    wb = weights_block(res, panel)
    if wb:
        extra['weights'] = wb
    costs = res.costs_summary()
    sens = cost_sensitivity_rows(spec, panel, book, ppy, res.equity, req)
    if sens:
        costs['sensitivity'] = sens
    if spec.kind == 'allocation':
        from base.costs import costs_by_year
        trades = episode_trades(res)
        costs['byYear'] = costs_by_year(res.fill_arrays, panel.dates, res.equity)
        if EPISODE_NOTE not in spec.notes:
            spec.notes.append(EPISODE_NOTE)
    else:
        trades = res.trades()
    data_v2 = build_backtest_result(
        nreq, dates=panel.dates, equity=res.equity, identity=_identity(spec, res.backend, []),
        trades=trades, fills=res.fills(), exposure=ex['gross'], net_exposure=ex['net'] if shorts else None,
        cash=res.cash, costs=costs, cost_model=spec.costs, frames=data.frames,
        price_provenance=data.provenance if not data.synthetic else None,
        assumptions=assumptions_block(spec, data, res.backend, k), warnings=warnings, logs=logs,
        warmup_bars=warmup, using_synthetic=data.synthetic, extra=extra)
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
    data_v2['provenance']['simulatorBackend'] = res.backend
    return {'success': True, 'data': data_v2}


# ════════════════════════════════════════════════════════════════════════════
# optimize / walk_forward (base.robustness + vbt_evaluate.evaluate)
# ════════════════════════════════════════════════════════════════════════════

def _param_ranges(req: Dict[str, Any], spec: Spec) -> Dict[str, Any]:
    ranges = req.get('paramRanges') or req.get('parameters')
    if not isinstance(ranges, dict) or not ranges:
        raise EngineInputError('No parameter ranges supplied (paramRanges with min / max / step per parameter).')
    if spec.strategy == 'code':
        return ranges
    cat = next((c for c in ST.CATALOG if c['type'] == spec.strategy), None)
    known = {p['id']: p for p in (cat or {}).get('parameters', [])}
    if not known:
        raise EngineInputError(f'Strategy {spec.strategy} has no numeric parameters to optimise.')
    unknown = [k for k in ranges if k not in known]
    if unknown:
        raise EngineInputError(f'{", ".join(unknown)} {"is not a parameter" if len(unknown) == 1 else "are not parameters"}'
                               f' of {spec.strategy} (parameters: {", ".join(known)}).')
    out = {}
    for k, v in ranges.items():
        if isinstance(v, dict) and known[k]['type'] == 'float':
            v = dict(v, type='float')
        out[k] = v
    return out


def _constraint_ok(strategy: str, params: Dict[str, Any]) -> bool:
    cat = next((c for c in ST.CATALOG if c['type'] == strategy), None)
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
    if spec.strategy != 'code':
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


def _choose_backend(n: int, k: int, runs: int) -> str:
    from base.simulator import numba_available
    return 'numba' if (numba_available() and n * k * max(1, runs) >= 200_000) else 'python'


def _workers(req: Dict[str, Any], backend: str, trials: int, n: int, k: int) -> int:
    """Worker processes for base.robustness: a spawn worker costs ~1.5 s to start on Windows (imports, numba
    load), so the pool is used only when the estimated single-process compute exceeds ~3 s (and never
    above the robustness / FINCEPT_BT_MAX_WORKERS / maxWorkers cap)."""
    from base import robustness as R
    if req.get('maxWorkers'):                       # an explicit request is honoured (still capped)
        return R.max_workers(req, trials)
    per = {'events': 0.0006 + 2e-7 * n, 'numba': 0.0005 + 3e-7 * n * k}.get(backend, 1.5e-5 * n * k)
    if trials * per < 3.0:
        return 1
    return R.max_workers(req, trials)


def _robust_meta(out: Dict[str, Any], spec: Spec, data: Data, panel, req: Dict[str, Any], backend: str,
                 command: str, extra_warnings: List[Dict[str, Any]], rf: float, t0: float) -> Dict[str, Any]:
    import platform
    from base.metrics import _fmt_date
    from base.execution import fill_model_block
    impl = (f'{implementation(backend)}; every combination scored by base.robustness (same costs, fills and sizing '
            f'as run_backtest)')
    out['engine'] = ENGINE
    out['engineImplementation'] = impl
    out['engineVersion'] = ENGINE_VERSION
    out['usingSyntheticData'] = data.synthetic
    out['strategy'] = spec.strategy
    out['symbols'] = list(data.symbols)
    out['warnings'] = list(data.warnings) + list(extra_warnings) + list(out.get('warnings') or [])
    out['assumptions'] = {
        'engine': impl, 'execution': _execution_text(spec), 'fillModel': fill_model_block(spec.fill),
        'costs': spec.costs.describe(),
        'positionSizing': describe_sizing(spec.sizing, len(data.symbols)) if spec.kind == 'signals' else
        'target weights (rebalancing strategy)',
        'riskFreeRate': rf, 'riskFreeSource': 'request',
        'dataSource': ('SYNTHETIC random-walk prices (user opted in)' if data.synthetic else
                       (f'Broker candles ({data.source[7:]})' if data.source.startswith('broker:') else
                        'Yahoo Finance via the Fincept price cache (split/dividend-adjusted prices)')),
        'objectiveNote': f'Objectives use the request\'s risk-free rate ({rf * 100:.2f}% a year) as a constant; '
                         f'Sharpe = mean excess bar return / std x sqrt(bars per year) (base.metrics).',
        'ignoredSettings': list(spec.ignored), 'notes': list(spec.notes)}
    out['provenance'] = {
        'engine': ENGINE, 'engineImplementation': impl, 'engineVersion': ENGINE_VERSION, 'library': None,
        'libraryVersion': None, 'pythonVersion': platform.python_version(), 'command': command,
        'dataSource': data.source, 'interval': spec.interval, 'bars': int(panel.n),
        'firstBar': _fmt_date(panel.dates[0]), 'lastBar': _fmt_date(panel.dates[-1]),
        'requestedStart': spec.start, 'requestedEnd': spec.end, 'symbols': list(data.symbols),
        'strategy': {'type': spec.strategy, 'name': (req.get('strategy') or {}).get('name')
                     if isinstance(req.get('strategy'), dict) else None},
        'baseParameters': dict(spec.params), 'seed': out.get('seed'), 'simulatorBackend': backend,
        'durationMs': int((time.perf_counter() - t0) * 1000), 'schemaVersion': 2}
    out.setdefault('logs', []).append(
        f'{panel.n} bars x {len(data.symbols)} symbol(s); {out.get("iterations") or out.get("combinationsPerWindow")} '
        f'combinations; workers {out.get("workers")}; backend {backend}; {out["provenance"]["durationMs"]} ms.')
    return out


def optimize(req: Dict[str, Any]) -> Dict[str, Any]:
    import vbt_evaluate as VE
    from base import robustness as R
    from base.metrics import infer_periods_per_year
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec = parse_request(req)
    data = load_data(spec, req)
    panel = make_panel(data)
    combos, info, seed, warns = _combos(req, spec)
    try:
        objective = R.normalize_objective(req.get('optimizeObjective') or req.get('objective') or 'sharpe')
    except ValueError as e:
        raise EngineInputError(str(e))
    backend = 'events' if VE.event_path_ok(spec, panel) else _choose_backend(panel.n, len(data.symbols), len(combos))
    ctx = VE.make_context(spec, panel, data.frames, data.symbols, backend)
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    hold = _num(req.get('holdoutFraction'), 0.0) or 0.0
    out = R.run_optimization(VE.evaluate, combos, context=ctx, objective=objective,
                             ppy=infer_periods_per_year(panel.dates), dates=panel.dates, rf=rf, info=info,
                             request=req, seed=seed, holdout_fraction=hold, cancel=CancelToken(req),
                             workers=_workers(req, backend, len(combos), panel.n, len(data.symbols)))
    out['seed'] = seed
    return {'success': True, 'data': _robust_meta(out, spec, data, panel, req, backend, 'optimize', warns, rf, t0)}


def walk_forward(req: Dict[str, Any]) -> Dict[str, Any]:
    import vbt_evaluate as VE
    from base import robustness as R
    from base.metrics import infer_periods_per_year
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec = parse_request(req)
    data = load_data(spec, req)
    panel = make_panel(data)
    combos, info, seed, warns = _combos(req, spec, default_max=200)
    try:
        objective = R.normalize_objective(req.get('optimizeObjective') or req.get('objective') or 'sharpe')
    except ValueError as e:
        raise EngineInputError(str(e))
    k_win = int(_num(req.get('wfSplits') or req.get('nSplits'), 5) or 5)
    ratio = float(_num(req.get('wfTrainRatio') or req.get('trainRatio'), 0.7) or 0.7)
    mode = str(req.get('wfMode') or req.get('walkForwardMode') or 'rolling').lower()
    purge = int(_num(req.get('purgeBars'), 0) or 0)
    embargo = int(_num(req.get('embargoBars'), 0) or 0)
    warm = max(ST.default_warmup(spec.strategy, dict(spec.params, **c)) for c in combos) \
        if spec.strategy != 'code' else 0
    try:
        windows = R.walk_forward_windows(panel.n, k_win, ratio, mode, purge_bars=purge, embargo_bars=embargo,
                                         warmup_bars=warm)
    except ValueError as e:
        raise EngineInputError(str(e))
    backend = 'events' if VE.event_path_ok(spec, panel) else \
        _choose_backend(panel.n, len(data.symbols), len(combos) * (len(windows) + 1))
    ctx = VE.make_context(spec, panel, data.frames, data.symbols, backend)
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    out = R.run_walk_forward(VE.evaluate, combos, windows, context=ctx, objective=objective,
                             ppy=infer_periods_per_year(panel.dates), dates=panel.dates, rf=rf,
                             initial_capital=spec.initial_capital, mode=mode, purge_bars=purge, embargo_bars=embargo,
                             request=req, seed=seed, cancel=CancelToken(req),
                             workers=_workers(req, backend, len(combos) * (len(windows) + 1), panel.n,
                                              len(data.symbols)))
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
    return {'success': True, 'data': _robust_meta(out, spec, data, panel, req, backend, 'walk_forward', warns, rf, t0)}
