"""
The Backtesting.py engine rebased on the shared core (base/): every run is executed by the
backtesting.py LIBRARY (see btp_executor for what runs inside it and the broker hooks).

  Bars        base.price_store (one on-disk cache; zero downloads when warm) or broker candles the
              host injected (brokerCandles); the FIRST symbol only (backtesting.py tests one
              instrument per run - further symbols are listed under ignoredSettings with an
              IGNORED_SYMBOLS warning).
  Signals     btp_strategies (vectorised, causal) executed bar by bar by btp_executor.SignalExecutor
              inside backtesting.py; custom strategy code is a backtesting.py Strategy subclass.
  Fills       request fillTiming: next_open (library default: market orders at the next open);
              same_close (library trade_on_close: the signal bar's close); next_close (the decision
              of bar t is placed one bar later with trade_on_close, so it fills at bar t+1's close).
  Sizing      positionSizing percent / fixed / risk / vol_target / kelly -> the order's size as a
              fraction of the available margin x leverage; backtesting.py rounds DOWN to whole units.
  Leverage    margin = 1 / leverage (the library's margin model; it liquidates when equity hits 0).
  Shorts      allowShort: an exit signal opens a short (library sell), borrow fee accrued.
  Stops       stopLoss / takeProfit / trailingStop (fractions) as the library's contingent SL / TP
              orders, levels from the entry fill price.
  Costs       base.costs.CostModel.from_request: commission through the library's commission
              (float, or a callable for per-share / per-trade / minimum / maximum schedules),
              slippage on every fill price through the broker's spread hook, borrow fee; all
              itemised per fill / trade / total.
  Results     base.result_normalizer.build_backtest_result: headline metrics from base.metrics, open
              positions listed as open trades (exitReason 'open', marked to market); the library's
              own summary statistics under engineReported.library.
  Optimise /  base.robustness over btp_evaluate.evaluate: every combination is a full backtesting.py
  walk-fwd   run with the same costs, fills, sizing, stops, leverage and shorting as run_backtest.
"""

from __future__ import annotations

import math
import time
from dataclasses import asdict, dataclass, field
from typing import Any, Dict, List, Optional

import numpy as np

import btp_strategies as ST

ENGINE = 'backtestingpy'
ENGINE_VERSION = '2.0.0'
SIZING_MODES = ('percent', 'fixed', 'kelly', 'vol_target', 'risk')
KELLY_MIN_TRADES = 10
VOL_LOOKBACK = 20


class EngineInputError(ValueError):
    """A request the engine cannot run (plain sentence for the user)."""


class EngineDataError(RuntimeError):
    """Market data could not be loaded (plain sentence, DATA error code)."""


def _log(msg: str) -> None:
    import sys
    print(f'[backtestingpy] {msg}', file=sys.stderr, flush=True)


def lib_version() -> str:
    try:
        from importlib.metadata import version
        return version('backtesting')
    except Exception:
        return '?'


def implementation() -> str:
    return (f'backtesting.py library {lib_version()} (its event loop, broker and order matching run every bar; '
            f'whole shares)')


def _num(v, default=None):
    try:
        x = float(v)
        return x if math.isfinite(x) else default
    except (TypeError, ValueError):
        return default


def _frac(req, key) -> float:
    v = _num(req.get(key), 0.0) or 0.0
    return v if v > 0 else 0.0


# ════════════════════════════════════════════════════════════════════════════
# Request
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class Spec:
    symbol: str
    requested: List[str]
    strategy: str
    params: Dict[str, Any]
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
    leverage: float
    ignored: List[str] = field(default_factory=list)
    notes: List[str] = field(default_factory=list)
    custom_code: Optional[str] = None

    @property
    def lag(self) -> int:
        return 1 if self.fill == 'next_close' else 0

    @property
    def trade_on_close(self) -> bool:
        return self.fill != 'next_open'

    def to_dict(self) -> Dict[str, Any]:
        """Picklable description for optimisation workers (the cost model as a dict)."""
        d = {k: getattr(self, k) for k in self.__dataclass_fields__ if k != 'costs'}
        d['costs'] = asdict(self.costs)
        return d


def spec_from_dict(d: Dict[str, Any]) -> Spec:
    from base.costs import CostModel
    kw = dict(d)
    kw['costs'] = CostModel(**d['costs'])
    return Spec(**kw)


def resolve_sizing(req: Dict[str, Any], stop_loss: float) -> Dict[str, Any]:
    """{'mode', 'value'} - value is the order size as a fraction of available margin (percent,
    risk), the money amount (fixed), the annual volatility target (vol_target) or the Kelly cap."""
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
        return {'mode': 'fixed', 'value': float(value)}
    if mode == 'risk':
        if not stop_loss:
            raise EngineInputError('Risk-per-trade sizing needs a STOP LOSS: the position size is the amount risked '
                                   'divided by the distance to the stop. Set a stop-loss or choose another sizing '
                                   'method.')
        risk = min(value, 100.0) / 100.0
        return {'mode': 'risk', 'value': min(1.0, risk / stop_loss), 'risk': risk, 'stop': float(stop_loss)}
    if mode == 'vol_target':
        return {'mode': 'vol_target', 'value': value / 100.0, 'lookback': VOL_LOOKBACK}
    if mode == 'kelly':
        return {'mode': 'kelly', 'value': min(value, 100.0) / 100.0, 'min_trades': KELLY_MIN_TRADES}
    return {'mode': 'percent', 'value': min(value, 100.0) / 100.0}


def describe_sizing(s: Dict[str, Any], lev: float) -> str:
    base = 'available margin (equity' + (f' x {lev:g} leverage' if lev != 1 else '') + ')'
    m = s['mode']
    if m == 'fixed':
        return (f"fixed: {s['value']:,.2f} per entry - sent to backtesting.py as that share of the {base} when the "
                f"order is placed; whole units at the fill price")
    if m == 'risk':
        return (f"risk: {s['risk'] * 100:.2f}% of equity lost if the {s['stop'] * 100:.2f}% stop is hit - "
                f"{s['value'] * 100:.1f}% of the {base} per entry; whole units")
    if m == 'vol_target':
        return (f"vol_target: {base} x min(1, {s['value'] * 100:.1f}% / realised {s['lookback']}-bar volatility at "
                f"the decision bar); no entry until the volatility can be measured; whole units")
    if m == 'kelly':
        return (f"kelly: {base} x min({s['value'] * 100:.1f}%, Kelly fraction of the trades closed by the decision "
                f"bar); the cap until {s['min_trades']} trades have closed (no look-ahead); whole units")
    return f"percent: {s['value'] * 100:.1f}% of the {base} per entry; backtesting.py rounds down to whole units"


def parse_request(req: Dict[str, Any]) -> Spec:
    from base.costs import CostModel
    from base.execution import fill_mode_from_request
    from base.price_store import normalize_symbol
    symbols = req.get('symbols') or [a.get('symbol') for a in (req.get('assets') or []) if isinstance(a, dict)]
    if isinstance(symbols, str):
        symbols = [s for s in symbols.split(',')]
    syms: List[str] = []
    for s in symbols or []:
        ns = normalize_symbol(s)
        if ns and ns not in syms:
            syms.append(ns)
    if not syms:
        raise EngineInputError('No symbols provided.')
    strat = req.get('strategy') if isinstance(req.get('strategy'), dict) else {}
    stype = str(strat.get('type') or 'sma_crossover')
    code = strat.get('code')
    code = (code.get('source', '') if isinstance(code, dict) else str(code or '')).strip()
    resolved = None
    try:
        resolved = ST.resolve(stype)
    except ValueError as e:
        if stype != 'code' and not code:
            raise EngineInputError(str(e))
    if stype == 'code' or (resolved is None and code):
        if not code:
            raise EngineInputError('A custom code strategy needs its source in strategy.code.')
        sid = 'code'
    else:
        sid = resolved
        code = None
    given = strat.get('params') if isinstance(strat.get('params'), dict) else (
        strat.get('parameters') if isinstance(strat.get('parameters'), dict) else {})
    params = ST.default_params(sid) if sid != 'code' else {}
    params.update(given or {})
    try:
        if not (req.get('fillTiming') or req.get('fillModel') or req.get('executionTiming')) and req.get('tradeOnClose'):
            fill = 'same_close'
        else:
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
    if sl >= 1:
        raise EngineInputError('stopLoss is a fraction of the entry price (0.05 = 5 %) and must be below 1.')
    sizing = resolve_sizing(req, sl)
    lev = _num(req.get('leverage'), 1.0) or 1.0
    ignored: List[str] = []
    notes: List[str] = []
    if lev < 1:
        ignored.append(f'leverage {lev:g} (below 1 is not a margin setting - use position sizing to invest less)')
        lev = 1.0
    if lev > 100:
        raise EngineInputError('Leverage above 100x is not supported.')
    spec = Spec(symbol=syms[0], requested=syms, strategy=sid, params=params,
                start=str(req.get('startDate') or '')[:10], end=str(req.get('endDate') or '')[:10],
                interval=str(req.get('interval') or '1d'), initial_capital=float(cap), fill=fill, costs=costs,
                sizing=sizing, stop_loss=sl, take_profit=tp, trailing_stop=ts,
                allow_short=bool(req.get('allowShort', False)), leverage=float(lev), ignored=ignored, notes=notes,
                custom_code=code)
    if not spec.start or not spec.end:
        raise EngineInputError('startDate and endDate are required.')
    if len(syms) > 1:
        spec.ignored.append(f'symbols after the first ({", ".join(syms[1:])}) - backtesting.py tests one instrument '
                            f'per run; only {syms[0]} was traded')
    if req.get('fractionalShares') is True:
        spec.ignored.append('fractional shares (backtesting.py trades whole units)')
    if isinstance(req.get('weights'), (list, tuple)) and len(req.get('weights')) > 1:
        spec.ignored.append('portfolio weights (one instrument per run)')
    cm = costs
    if cm.impact_coef:
        spec.ignored.append('market impact estimate (not modelled by this engine)')
    if cm.participation_cap:
        spec.ignored.append('volume participation cap (backtesting.py fills the whole order)')
    if cm.cash_rate_annual:
        spec.ignored.append('interest on cash (not modelled by this engine)')
    if cm.financing_rate_annual:
        spec.ignored.append('financing cost of leverage (not modelled by this engine)')
    if sid == 'code':
        if fill == 'next_close':
            raise EngineInputError('next_close fills are not available for custom strategy code on the Backtesting.py '
                                   'engine (the code places its own orders); choose next_open or same_close.')
        if sl or tp or ts:
            spec.ignored.append('stop-loss / take-profit / trailing stop (custom strategy code places its own orders)')
        if not (sizing['mode'] == 'percent' and sizing['value'] >= 1.0):
            spec.ignored.append('position sizing (custom strategy code sizes its own orders)')
        if spec.allow_short:
            spec.notes.append('Short selling is whatever the custom code does (allowShort is not enforced).')
    return spec


# ════════════════════════════════════════════════════════════════════════════
# Data
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class Data:
    frame: Any                             # OHLCV DataFrame of the traded symbol (clean, no NaN)
    frames: Dict[str, Any]                 # symbol -> frame (traded symbol + benchmark) for the normaliser
    provenance: Dict[str, Dict[str, Any]]
    warnings: List[Dict[str, Any]]
    synthetic: bool = False
    source: str = 'yahoo'
    network_calls: int = 0


def _broker_frames(candles: Dict[str, Any], symbols: List[str]) -> Optional[Dict[str, Any]]:
    """Frames from broker candles the host injected - only when they cover every symbol asked for."""
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


def clean_frame(f, symbol: str, warnings: List[Dict[str, Any]]):
    """OHLCV with a tz-naive sorted index and no missing OHLC (backtesting.py refuses NaN prices)."""
    import pandas as pd
    f = f.copy()
    idx = pd.DatetimeIndex(f.index)
    if idx.tz is not None:
        idx = idx.tz_localize(None)
    f.index = idx
    f = f[~f.index.duplicated(keep='last')].sort_index()
    for c in ('Open', 'High', 'Low'):
        if c not in f:
            f[c] = f['Close']
    if 'Volume' not in f:
        f['Volume'] = 0.0
    f = f[['Open', 'High', 'Low', 'Close', 'Volume']].astype(float)
    bad = f[['Open', 'High', 'Low', 'Close']].isna().any(axis=1) | ~(f['Close'] > 0)
    if bad.any():
        warnings.append({'code': 'ROWS_DROPPED', 'severity': 'info', 'symbol': symbol,
                         'message': f'{int(bad.sum())} bar(s) of {symbol} without a complete open / high / low / close '
                                    f'were left out (backtesting.py needs every price).'})
        f = f[~bad]
    f['Volume'] = f['Volume'].fillna(0.0)
    return f


def load_data(spec: Spec, req: Dict[str, Any], benchmark: Optional[str] = None) -> Data:
    """The traded symbol's bars (+ the benchmark) in ONE price-store call."""
    from base.price_store import get_bars, normalize_symbol
    warnings: List[Dict[str, Any]] = []
    sym = spec.symbol
    bench = normalize_symbol(benchmark) if benchmark else None
    broker = _broker_frames(req.get('brokerCandles') or {}, [sym])
    if broker is not None:
        src = f"broker:{req.get('brokerDataSource') or 'broker'}"
        f = clean_frame(broker[sym], sym, warnings)
        frames = {sym: f}
        if bench and bench != sym:
            b = get_bars([bench], spec.start, spec.end, interval=spec.interval, progress=False)
            frames.update(b.frames)
        prov = {sym: {'bars': len(f), 'firstBar': str(f.index[0])[:10], 'lastBar': str(f.index[-1])[:10],
                      'source': src}}
        return Data(frame=f, frames=frames, provenance=prov, warnings=warnings, source=src)
    want = [sym] + ([bench] if bench and bench != sym else [])
    try:
        res = get_bars(want, spec.start, spec.end, interval=spec.interval)
    except ValueError as e:
        raise EngineInputError(str(e))
    f = res.frames.get(sym)
    if f is None or len(f) < 2:
        reason = str(res.errors.get(sym) or 'fewer than 2 bars in the period')
        if bool(req.get('allowSyntheticData', False)):
            f = synthetic_frame(sym, spec.start, spec.end)
            warnings.append({'code': 'SYNTHETIC_DATA', 'severity': 'error', 'symbol': None,
                             'message': f'Market data could not be loaded ({reason[:300]}); this run used SYNTHETIC '
                                        f'random-walk prices because synthetic data was allowed. The results have no '
                                        f'market meaning.'})
            return Data(frame=f, frames={sym: f}, provenance={}, warnings=warnings, synthetic=True, source='synthetic')
        raise EngineDataError(f'Market data could not be loaded for {sym} ({reason}). Refusing to run on synthetic '
                              f'prices - check the symbol (exchange suffix, e.g. RELIANCE.NS) and the dates, or tick '
                              f'"Allow synthetic data".')
    f = clean_frame(f, sym, warnings)
    if len(f) < 3:
        raise EngineDataError(f'{sym} has only {len(f)} usable bars between {spec.start} and {spec.end}.')
    frames = {sym: f}
    if bench and bench in res.frames:
        frames[bench] = res.frames[bench]
    return Data(frame=f, frames=frames, provenance=res.provenance, warnings=warnings,
                network_calls=res.network_calls)


# ════════════════════════════════════════════════════════════════════════════
# One backtesting.py run
# ════════════════════════════════════════════════════════════════════════════

def make_ta(frame) -> ST.TA:
    return ST.TA.from_frame(frame)


def ppy_of(frame) -> float:
    from base.metrics import infer_periods_per_year
    return float(infer_periods_per_year(frame.index))


def make_plan(spec: Spec, ta: ST.TA, ppy: float, hook=None) -> Dict[str, Any]:
    s = spec.sizing
    vol = None
    if s['mode'] == 'vol_target':
        v = ta.realised_vol(s['lookback'], ppy)
        with np.errstate(divide='ignore', invalid='ignore'):
            vol = np.where(np.isfinite(v) & (v > 0), np.minimum(1.0, s['value'] / v), 0.0)
    return {'lag': spec.lag, 'allow_short': spec.allow_short, 'size_mode': s['mode'], 'size_value': s['value'],
            'vol': vol, 'leverage': spec.leverage, 'kelly_min': s.get('min_trades', KELLY_MIN_TRADES),
            'stops': bool(spec.stop_loss or spec.take_profit or spec.trailing_stop), 'trail': spec.trailing_stop,
            'hook': hook}


def make_fx(spec: Spec, frame) -> Dict[str, Any]:
    """Broker hook settings: per-bar slippage fractions, borrow, accrual fractions, stop levels."""
    from base.execution import year_fractions
    cm = spec.costs
    slip = cm.slippage_fractions(frame['Close'].to_numpy(float), frame['Volume'].to_numpy(float))[:, 0]
    return {'slip': np.ascontiguousarray(slip, dtype=float),
            'borrow_rate': cm.borrow_rate_annual if spec.allow_short else 0.0,
            'dt': year_fractions(frame.index, cm.day_count),
            'sl': spec.stop_loss if spec.strategy != 'code' else 0.0,
            'tp': spec.take_profit if spec.strategy != 'code' else 0.0,
            'ts': spec.trailing_stop if spec.strategy != 'code' else 0.0}


def commission_arg(cm):
    """backtesting.py's `commission`: the native relative rate when it is one, else a callable
    (order_size, price) -> money (per share / per trade / minimum / maximum schedules)."""
    if cm.commission_type == 'pct_notional' and not cm.min_commission and cm.max_commission_pct is None \
            and 0.0 <= cm.commission_value < 0.1:
        return float(cm.commission_value)

    def fee(order_size, price, _cm=cm):
        return float(_cm.commission(abs(order_size), price))
    return fee


def make_backtest(spec: Spec, frame, fx: Optional[Dict[str, Any]] = None):
    import btp_executor as X
    strategy = X.custom_strategy_class(spec.custom_code) if spec.strategy == 'code' else X.SignalExecutor
    return X.FinceptBacktest(frame, strategy, cash=float(spec.initial_capital), commission=commission_arg(spec.costs),
                             margin=1.0 / float(spec.leverage), trade_on_close=spec.trade_on_close, hedging=False,
                             exclusive_orders=False, finalize_trades=False, spread=0.0,
                             fx=fx if fx is not None else make_fx(spec, frame))


def run_once(spec: Spec, bt, sig: Optional[Dict[str, Any]], plan: Optional[Dict[str, Any]], *,
             params: Optional[Dict[str, Any]] = None, record_warnings: bool = False):
    """One backtesting.py run -> btp_executor.RunOut."""
    import btp_executor as X
    if spec.strategy == 'code':
        kw = {}
        cls = bt._strategy
        for k, v in (params or {}).items():
            if hasattr(cls, k):
                kw[k] = v
        raw = bt.run_raw(record_warnings=record_warnings, **kw)
    else:
        raw = bt.run_raw(record_warnings=record_warnings, sig=sig, plan=plan)
    return X.collect(raw, spec.initial_capital)


def trades_from(run, frame, symbol: str) -> List[Dict[str, Any]]:
    """Contract trades (closed + open) from the run's fills (base.execution.fifo_round_trips)."""
    import btp_executor as X
    from base.execution import fifo_round_trips
    c = frame['Close'].to_numpy(float)
    trades = fifo_round_trips(run.fills, frame.index, [symbol], last_close=np.array([c[-1]]),
                              high=frame['High'].to_numpy(float)[:, None], low=frame['Low'].to_numpy(float)[:, None],
                              borrow_per_bar=run.borrow[:, None], positions=run.positions[:, None])
    mc = run.fills['reason'] == X.REASON_MARGIN
    if mc.any():
        bars = set(int(b) for b in run.fills['bar'][mc])
        for t in trades:
            if t.get('exitBar') in bars and t.get('exitReason') == 'signal':
                t['exitReason'] = 'margin_call'
    return trades


def cost_sensitivity_rows(spec: Spec, frame, sig, plan, run, rf: float) -> List[Dict[str, Any]]:
    """costs.sensitivity: the same strategy re-run in backtesting.py with every trading cost at 0x, 1x
    (this run), 2x and 3x the cost model (base.costs.cost_sensitivity). Whole-unit sizes follow the
    costs, exactly as a real run would."""
    from dataclasses import replace
    from base.costs import cost_sensitivity
    quiet = dict(plan, hook=None) if plan is not None else None

    def simulate(model):
        s2 = replace(spec, costs=model)
        return run_once(s2, make_backtest(s2, frame), sig, quiet, params=spec.params).equity
    return cost_sensitivity(simulate, spec.costs, frame.index, rf=rf, ppy=ppy_of(frame), base_equity=run.equity)


def costs_of(run) -> Dict[str, Any]:
    fa = run.fills
    q = np.abs(fa['qty'])
    return {'totalCommission': float(np.sum(fa['commission'])),
            'totalSlippage': float(np.sum(np.abs(fa['price'] - fa['ref_price']) * q)),
            'totalBorrow': float(np.sum(run.borrow)), 'tradedNotional': float(np.sum(q * fa['price'])),
            'fills': int(len(q))}


# ════════════════════════════════════════════════════════════════════════════
# run_backtest
# ════════════════════════════════════════════════════════════════════════════

def _execution_text(spec: Spec) -> str:
    from base.execution import fill_model_block
    t = fill_model_block(spec.fill)['description']
    t += {'next_open': ' backtesting.py market orders (its default: the next bar\'s open).',
          'same_close': ' backtesting.py trade_on_close (orders fill at the close of the bar that placed them).',
          'next_close': ' The decision of bar t is placed on bar t+1 with backtesting.py trade_on_close, so it fills '
                        'at bar t+1\'s close (the library has no native next-close fill).'}[spec.fill]
    t += ' Whole units: backtesting.py sizes each order from the available margin and rounds down.'
    if spec.strategy == 'code':
        return t + ' Custom strategy code places its own orders.'
    stops = []
    if spec.stop_loss:
        stops.append(f'stop-loss {spec.stop_loss * 100:g}%')
    if spec.take_profit:
        stops.append(f'take-profit {spec.take_profit * 100:g}%')
    if spec.trailing_stop:
        stops.append(f'trailing stop {spec.trailing_stop * 100:g}% (raised at each close from the best price since '
                     f'entry)')
    if stops:
        t += (' ' + ', '.join(stops) + ' as backtesting.py contingent orders, levels from the entry fill price: '
              'checked against each bar\'s high and low (also on the entry bar), a gap through the level fills at the '
              'open, the stop is checked before the target; stops pay slippage, targets are limit fills.')
    if spec.allow_short:
        t += ' Short selling allowed: an exit signal opens a short (library sell), an entry signal covers it; borrow ' \
             'fee accrued on the previous close (ACT/360).'
    if spec.leverage != 1:
        t += f' Leverage {spec.leverage:g}x: backtesting.py margin = {1 / spec.leverage:.4g} (positions up to ' \
             f'{spec.leverage:g}x equity; the library liquidates everything if equity reaches 0 - no maintenance ' \
             f'margin calls, no financing cost).'
    return t


def _identity(spec: Spec, notes: List[str]):
    from base.result_normalizer import EngineIdentity
    modelled = []
    if spec.allow_short and spec.costs.borrow_rate_annual > 0:
        modelled.append('borrow cost')
    return EngineIdentity(ENGINE, implementation(), library='backtesting', version=ENGINE_VERSION, fill_mode=spec.fill,
                          commission_itemised=True, slippage_itemised=True, modelled=tuple(modelled),
                          trade_mode='signals', notes=list(notes))


def engine_notes(spec: Spec) -> List[str]:
    n = [f'Executed by the backtesting.py {lib_version()} library: its bar loop, broker, order matching, whole-unit '
         f'sizing and contingent stop orders. Fincept hooks on the library\'s broker: slippage on every fill price '
         f'(the library\'s spread applies to entries only), borrow fee on shorts, stop levels from the fill price, and '
         f'a fill log from which the trade list is rebuilt (open positions included).']
    if spec.strategy in ST.USES_RSI:
        n.append('RSI is the simple-average (Cutler) RSI - mean gain / mean loss over the period - not Wilder\'s '
                 'smoothed RSI.')
    if spec.strategy == 'code':
        n.append('Custom strategy code (a backtesting.py Strategy subclass) was executed as the local user.')
    return n


def assumptions_block(spec: Spec, data: Data) -> Dict[str, Any]:
    from base.base_provider import _SURVIVORSHIP_NOTE
    if data.synthetic:
        src = 'SYNTHETIC random-walk prices (user opted in) - results have NO market meaning'
    elif data.source.startswith('broker:'):
        src = f'Broker candles ({data.source[7:]})'
    else:
        src = 'Yahoo Finance via the Fincept price cache (split/dividend-adjusted prices; trade prices are not ' \
              'historical quotes)'
    costs = spec.costs.describe() + '. Slippage moves the fill price (buys higher, sells lower) on entries and exits.'
    return {'engine': implementation(), 'execution': _execution_text(spec), 'costs': costs, 'dataSource': src,
            'survivorship': _SURVIVORSHIP_NOTE,
            'positionSizing': describe_sizing(spec.sizing, spec.leverage) if spec.strategy != 'code'
            else 'custom strategy code sizes its own orders (backtesting.py whole units)',
            'ignoredSettings': list(spec.ignored), 'notes': list(spec.notes) + engine_notes(spec), 'shares': 'whole',
            'leverage': spec.leverage}


def _warn(code, message, severity='warn', symbol=None):
    return {'code': code, 'severity': severity, 'message': message, 'symbol': symbol}


def run_backtest(req: Dict[str, Any]) -> Dict[str, Any]:
    """The run_backtest command: {'success': True, 'data': contract v2} (raises EngineInputError /
    EngineDataError with a plain sentence)."""
    from base.execution import fills_table
    from base.progress import CancelToken, Progress
    from base.result_normalizer import build_backtest_result
    t0 = time.perf_counter()
    cancel = CancelToken(req)
    spec = parse_request(req)
    bench = str(req.get('benchmarkSymbol') or req.get('benchmark') or '').strip() or None
    data = load_data(spec, req, benchmark=bench)
    cancel.check()
    frame = data.frame
    n = len(frame)
    prog = Progress('run', total=n, message=f'backtesting.py: {spec.strategy} on {spec.symbol} ({n} bars)')

    def hook(i):
        cancel.check()
        prog.update(i, f'bar {i} / {n}')

    ta = make_ta(frame)
    sig = None
    if spec.strategy != 'code':
        try:
            sig = ST.build_signals(spec.strategy, ta, spec.params)
        except ValueError as e:
            raise EngineInputError(f'{spec.strategy}: {e}')
    plan = make_plan(spec, ta, ppy_of(frame), hook=hook) if sig is not None else None
    bt = make_backtest(spec, frame)
    t_run = time.perf_counter()
    run = run_once(spec, bt, sig, plan, params=spec.params, record_warnings=True)
    run_ms = int((time.perf_counter() - t_run) * 1000)
    prog.finish('backtesting.py run done')
    cancel.check()
    sym = spec.symbol
    trades = trades_from(run, frame, sym)
    c = frame['Close'].to_numpy(float)
    eq = run.equity
    pv = run.positions * c
    with np.errstate(divide='ignore', invalid='ignore'):
        gross = np.where(eq > 0, np.abs(pv) / eq, np.nan)
        net = np.where(eq > 0, pv / eq, np.nan)
    warnings = list(data.warnings)
    if len(spec.requested) > 1:
        warnings.append(_warn('IGNORED_SYMBOLS', f'Only {sym} was traded: backtesting.py tests one instrument per run '
                                                 f'({len(spec.requested) - 1} more symbol(s) ignored).'))
    cancelled = sum(1 for w in run.raw.lib_warnings if 'canceled' in w.lower() or 'cancelled' in w.lower())
    if cancelled:
        warnings.append(_warn('ORDERS_CANCELLED', f'backtesting.py cancelled {cancelled} order(s) for lack of cash / '
                                                  f'margin (whole units: the order was smaller than one unit at the '
                                                  f'fill price).', 'warn', sym))
    if float(np.nanmax(c)) > spec.initial_capital * spec.leverage:
        warnings.append(_warn('WHOLE_UNITS', f'{sym} traded above the capital ({np.nanmax(c):,.2f} > '
                                             f'{spec.initial_capital * spec.leverage:,.2f}): backtesting.py trades whole '
                                             f'units, so some orders could not be filled at all.', 'warn', sym))
    if run.margin_call_bar is not None:
        deficit = -(sum(float(t['pnl'] or 0.0) for t in trades) - (float(eq[-1]) - spec.initial_capital))
        warnings.append(_warn('MARGIN_CALL', f'Equity reached zero on {str(frame.index[run.margin_call_bar])[:10]}: '
                                             f'backtesting.py closed every position at that close and stopped the run. '
                                             f'The closing loss exceeded the account by {max(0.0, deficit):,.2f}; the '
                                             f'library floors the account at zero, so the trade list shows more loss '
                                             f'than the equity curve.', 'error', sym))
    if sig is not None and spec.strategy != 'buy_and_hold' and not sig['e'].any() and not \
            (spec.allow_short and sig['x'].any()):
        warnings.append(_warn('NO_SIGNALS', f'The {spec.strategy} rule produced no entry signal in this period with '
                                            f'these parameters.'))
    logs = [f'{n} bars of {sym} {str(frame.index[0])[:10]}..{str(frame.index[-1])[:10]}; strategy {spec.strategy} '
            f'{spec.params}; fill {spec.fill}; backtesting.py {lib_version()} run {run_ms} ms.',
            f'{len(run.fills["bar"])} fills; {len(trades)} trades; final equity {float(eq[-1]):,.2f}.']
    nreq = dict(req)
    nreq['symbols'] = [sym]
    strat = dict(req.get('strategy') or {}) if isinstance(req.get('strategy'), dict) else {}
    strat['type'] = spec.strategy
    strat['params'] = dict(spec.params)
    nreq['strategy'] = strat
    if bench:
        nreq['benchmarkSymbol'] = bench
    warmup = ST.default_warmup(spec.strategy, spec.params) if spec.strategy != 'code' else None
    costs = costs_of(run)
    t_s = time.perf_counter()
    rf_const = _num(req.get('riskFreeRate'), 0.0) or 0.0
    costs['sensitivity'] = cost_sensitivity_rows(spec, frame, sig, plan, run, rf_const)
    logs.append(f'Cost sensitivity (0x / 1x / 2x / 3x the cost model, each a backtesting.py run): '
                f'{int((time.perf_counter() - t_s) * 1000)} ms; its Sharpe column uses the request\'s risk-free rate '
                f'({rf_const * 100:.2f}% a year) as a constant.')
    cancel.check()
    fills = fills_table(run.fills, frame.index, [sym])
    import btp_executor as X
    for k in np.flatnonzero(run.fills['reason'] == X.REASON_MARGIN):
        fills[int(k)]['reason'] = 'margin_call'
    data_v2 = build_backtest_result(
        nreq, dates=frame.index, equity=eq, identity=_identity(spec, []), trades=trades,
        fills=fills, exposure=gross, net_exposure=net if spec.allow_short else None,
        cash=eq - pv, costs=costs, cost_model=spec.costs, frames=data.frames,
        price_provenance=data.provenance if not data.synthetic else None, assumptions=assumptions_block(spec, data),
        warnings=warnings, logs=logs, warmup_bars=warmup, using_synthetic=data.synthetic)
    first, last = float(c[0]), float(c[-1])
    data_v2['extendedStats'] = {
        'buyAndHoldReturn': last / first - 1.0 if first > 0 else None,
        'buyAndHoldNote': f'holding {sym} from the first to the last close, no costs',
        'exposureTime': data_v2['exposure'].get('timeInMarket'),
        'sqn': data_v2['tradeStats'].get('sqn'), 'kellyCriterion': data_v2['tradeStats'].get('kellyFraction')}
    data_v2['riskMetrics'] = {'downsideDeviation': data_v2['performance'].get('downsideDeviation'),
                              'volatility': data_v2['performance'].get('volatility'),
                              'maxDrawdown': data_v2['performance'].get('maxDrawdown')}
    try:
        import btp_executor as X
        data_v2.setdefault('engineReported', {})['library'] = X.library_stats(run.raw, frame)
    except Exception as e:  # diagnostic block only
        data_v2.setdefault('engineReported', {})['library'] = {'error': f'{type(e).__name__}: {e}'}
    data_v2['provenance']['engineMs'] = int((time.perf_counter() - t0) * 1000)
    data_v2['provenance']['libraryRunMs'] = run_ms
    return {'success': True, 'data': data_v2}


# ════════════════════════════════════════════════════════════════════════════
# optimize / walk_forward (base.robustness + btp_evaluate.evaluate)
# ════════════════════════════════════════════════════════════════════════════

def _param_ranges(req: Dict[str, Any], spec: Spec) -> Dict[str, Any]:
    ranges = req.get('paramRanges') or req.get('parameters')
    if not isinstance(ranges, dict) or not ranges:
        raise EngineInputError('No parameter ranges supplied (paramRanges with min / max / step per parameter).')
    if spec.strategy == 'code':
        import btp_executor as X
        cls = X.custom_strategy_class(spec.custom_code)
        missing = [k for k in ranges if not hasattr(cls, k)]
        if missing:
            raise EngineInputError(f'{", ".join(missing)} {"is not a class attribute" if len(missing) == 1 else "are not class attributes"}'
                                   f' of the custom strategy {cls.__name__} (backtesting.py optimises class-level '
                                   f'parameters).')
        return ranges
    cat = ST.catalog_entry(spec.strategy)
    known = {p['id']: p for p in (cat or {}).get('parameters', [])}
    if not known:
        raise EngineInputError(f'Strategy {spec.strategy} has no parameters to optimise.')
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


def combos_for(req: Dict[str, Any], spec: Spec, default_max: Optional[int] = 500):
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
        keep = [c for c in combos if ST.constraint_ok(spec.strategy, dict(spec.params, **c))]
        dropped = len(combos) - len(keep)
        if dropped:
            if not keep:
                raise EngineInputError(f'Every one of the {len(combos)} parameter combinations breaks a rule of '
                                       f'{spec.strategy} (for example fast period >= slow period).')
            warnings.append(_warn('COMBINATIONS_SKIPPED', f'{dropped} of {len(combos)} combinations break a rule of '
                                                          f'{spec.strategy} (e.g. fast period >= slow period) and were '
                                                          f'not evaluated.', 'info'))
            combos = keep
            info = dict(info, iterations=len(combos), skippedByConstraints=dropped)
    return combos, info, seed, warnings


SPAWN_COST_S = 1.6           # one spawn worker on Windows: interpreter, numpy / pandas, backtesting


def choose_workers(req: Dict[str, Any], runs: int, bars: int) -> int:
    """Worker processes for base.robustness: the request's maxWorkers / FINCEPT_BT_MAX_WORKERS are a
    CAP; a pool is used only when the estimated single-process time clearly exceeds the start-up
    cost of the workers (each backtesting.py run costs ~12 microseconds per bar)."""
    from base import robustness as R
    cap = R.max_workers(req, runs)
    est = runs * (0.004 + 1.2e-5 * bars)
    if cap <= 1 or est < 2.5 * SPAWN_COST_S:
        return 1
    return int(max(1, min(cap, math.ceil(est / (2.0 * SPAWN_COST_S)))))


def _robust_meta(out: Dict[str, Any], spec: Spec, data: Data, req: Dict[str, Any], command: str,
                 extra_warnings: List[Dict[str, Any]], rf: float, t0: float) -> Dict[str, Any]:
    import platform
    from base.execution import fill_model_block
    from base.metrics import _fmt_date
    frame = data.frame
    impl = (f'{implementation()}; every combination is a full backtesting.py run with the same costs, fills, sizing, '
            f'stops, leverage and shorting as run_backtest (the library\'s own statistics are skipped; returns are '
            f'scored by base.robustness / base.metrics)')
    out['engine'] = ENGINE
    out['engineImplementation'] = impl
    out['engineVersion'] = ENGINE_VERSION
    out['usingSyntheticData'] = data.synthetic
    out['strategy'] = spec.strategy
    out['symbols'] = [spec.symbol]
    w = list(data.warnings) + list(extra_warnings) + list(out.get('warnings') or [])
    if len(spec.requested) > 1:
        w.append(_warn('IGNORED_SYMBOLS', f'Only {spec.symbol} was used: backtesting.py tests one instrument per run.'))
    out['warnings'] = w
    out['assumptions'] = {
        'engine': impl, 'execution': _execution_text(spec), 'fillModel': fill_model_block(spec.fill),
        'costs': spec.costs.describe() + '. Slippage moves the fill price on entries and exits.',
        'positionSizing': describe_sizing(spec.sizing, spec.leverage) if spec.strategy != 'code'
        else 'custom strategy code sizes its own orders', 'leverage': spec.leverage, 'shares': 'whole',
        'riskFreeRate': rf, 'riskFreeSource': 'request',
        'dataSource': ('SYNTHETIC random-walk prices (user opted in)' if data.synthetic else
                       (f'Broker candles ({data.source[7:]})' if data.source.startswith('broker:') else
                        'Yahoo Finance via the Fincept price cache (split/dividend-adjusted prices)')),
        'objectiveNote': f'Objectives use the request\'s risk-free rate ({rf * 100:.2f}% a year) as a constant; '
                         f'Sharpe = mean excess bar return / std x sqrt(bars per year) (base.metrics).',
        'ignoredSettings': list(spec.ignored), 'notes': list(spec.notes) + engine_notes(spec)}
    out['provenance'] = {
        'engine': ENGINE, 'engineImplementation': impl, 'engineVersion': ENGINE_VERSION, 'library': 'backtesting',
        'libraryVersion': lib_version(), 'pythonVersion': platform.python_version(), 'command': command,
        'dataSource': data.source, 'interval': spec.interval, 'bars': int(len(frame)),
        'firstBar': _fmt_date(frame.index[0]), 'lastBar': _fmt_date(frame.index[-1]),
        'requestedStart': spec.start, 'requestedEnd': spec.end, 'symbols': [spec.symbol],
        'strategy': {'type': spec.strategy, 'name': (req.get('strategy') or {}).get('name')
                     if isinstance(req.get('strategy'), dict) else None},
        'baseParameters': dict(spec.params), 'seed': out.get('seed'),
        'durationMs': int((time.perf_counter() - t0) * 1000), 'schemaVersion': 2}
    out.setdefault('logs', []).append(
        f'{len(frame)} bars of {spec.symbol}; {out.get("iterations") or out.get("combinationsPerWindow")} combinations; '
        f'workers {out.get("workers")}; {out["provenance"]["durationMs"]} ms.')
    return out


def optimize(req: Dict[str, Any]) -> Dict[str, Any]:
    import btp_evaluate as BE
    from base import robustness as R
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec = parse_request(req)
    data = load_data(spec, req)
    combos, info, seed, warns = combos_for(req, spec)
    try:
        objective = R.normalize_objective(req.get('optimizeObjective') or req.get('objective') or 'sharpe')
    except ValueError as e:
        raise EngineInputError(str(e))
    frame = data.frame
    ctx = BE.make_context(spec, frame, req.get('cancelFile'))
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    hold = _num(req.get('holdoutFraction'), 0.0) or 0.0
    try:
        out = R.run_optimization(BE.evaluate, combos, context=ctx, objective=objective, ppy=ppy_of(frame),
                                 dates=frame.index, rf=rf, info=info, request=req, seed=seed, holdout_fraction=hold,
                                 cancel=CancelToken(req), workers=choose_workers(req, len(combos), len(frame)))
    except ValueError as e:
        raise EngineInputError(str(e))
    out['seed'] = seed
    return {'success': True, 'data': _robust_meta(out, spec, data, req, 'optimize', warns, rf, t0)}


def walk_forward(req: Dict[str, Any]) -> Dict[str, Any]:
    import btp_evaluate as BE
    from base import robustness as R
    from base.progress import CancelToken
    t0 = time.perf_counter()
    spec = parse_request(req)
    data = load_data(spec, req)
    combos, info, seed, warns = combos_for(req, spec, default_max=200)
    try:
        objective = R.normalize_objective(req.get('optimizeObjective') or req.get('objective') or 'sharpe')
    except ValueError as e:
        raise EngineInputError(str(e))
    frame = data.frame
    k_win = int(_num(req.get('wfSplits') or req.get('nSplits'), 5) or 5)
    ratio = float(_num(req.get('wfTrainRatio') or req.get('trainRatio'), 0.7) or 0.7)
    mode = str(req.get('wfMode') or req.get('walkForwardMode') or 'rolling').lower()
    purge = int(_num(req.get('purgeBars'), 0) or 0)
    embargo = int(_num(req.get('embargoBars'), 0) or 0)
    warm = max(ST.default_warmup(spec.strategy, dict(spec.params, **c)) for c in combos) \
        if spec.strategy != 'code' else int(_num(req.get('warmupBars'), 0) or 0)
    try:
        windows = R.walk_forward_windows(len(frame), k_win, ratio, mode, purge_bars=purge, embargo_bars=embargo,
                                         warmup_bars=warm)
    except ValueError as e:
        raise EngineInputError(str(e))
    ctx = BE.make_context(spec, frame, req.get('cancelFile'))
    rf = _num(req.get('riskFreeRate'), 0.0) or 0.0
    runs = len(combos) * (len(windows) + 1)
    avg_bars = int(np.mean([w.train_end - w.train_data_start for w in windows])) if windows else len(frame)
    try:
        out = R.run_walk_forward(BE.evaluate, combos, windows, context=ctx, objective=objective, ppy=ppy_of(frame),
                                 dates=frame.index, rf=rf, initial_capital=spec.initial_capital, mode=mode,
                                 purge_bars=purge, embargo_bars=embargo, request=req, seed=seed,
                                 cancel=CancelToken(req), workers=choose_workers(req, runs, avg_bars))
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
    return {'success': True, 'data': _robust_meta(out, spec, data, req, 'walk_forward', warns, rf, t0)}
