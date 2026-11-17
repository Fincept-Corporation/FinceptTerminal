"""
Module-level evaluate(params, context, window) of the vectorbt engine for base.robustness
(optimise and walk-forward), importable by spawn-pool workers on Windows.

The context is a plain picklable dict (bars of every symbol, the run's settings) built ONCE per
command by make_context() and sent once per worker. Each worker keeps, for the life of the
context: one memoised indicator accessor per symbol (each distinct indicator / window is computed
once for all trials and all walk-forward windows), the signal matrices of every parameter set it
has seen, and one base.simulator.Simulator per window. Signals are computed on the FULL history
(indicators are causal - the value at bar t uses bars <= t only), then the window's rows are
simulated: positions open only from window['scoreStart'] on, so warm-up never trades and nothing
after window['end'] is used.

Returns per trial: {'returns': per-bar returns of the scored bars, 'trades': closed round trips,
'winRate', 'profitFactor', 'sqn'} - the same simulator and costs as run_backtest.

EVENT PATH. The most common optimisation (one symbol, long/flat signals, percent sizing, no stops,
commission as a fraction of notional, price-fraction slippage, fractional shares) does not need the
bar loop at all: the position can only change on the few bars where a pending entry / exit fills.
long_flat_events() replays base.simulator's arithmetic (same operations in the same order) on
those bars only and fills the equity between them with numpy - bit-identical to base.simulator
(tests/test_vectorbt.py checks it against both kernels) and ~100x cheaper than loading numba for
a few hundred trials. Every other configuration runs base.simulator itself.
"""

from __future__ import annotations

import os
import sys
import uuid
from typing import Any, Dict, Optional

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
_BT = os.path.dirname(_HERE)
for _p in (_BT, _HERE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

_STATE: Dict[str, Any] = {'token': None}


def make_context(spec, panel, frames: Dict[str, Any], symbols, backend: str) -> Dict[str, Any]:
    fr = {}
    for s in symbols:
        f = frames[s]
        fr[s] = {'t': np.asarray(f.index.asi8, dtype=np.int64),
                 **{c: f[c].to_numpy(dtype=float) for c in ('Open', 'High', 'Low', 'Close', 'Volume') if c in f}}
    return {'token': uuid.uuid4().hex, 'spec': spec.to_dict(), 'symbols': list(symbols),
            'dates': np.asarray(panel.dates.asi8, dtype=np.int64),
            'panel': {'open': panel.open, 'high': panel.high, 'low': panel.low, 'close': panel.close,
                      'volume': panel.volume, 'tradable': panel.tradable},
            'frames': fr, 'backend': backend}


def _state(ctx: Dict[str, Any]) -> Dict[str, Any]:
    if _STATE.get('token') == ctx['token']:
        return _STATE
    import pandas as pd
    import vbt_engine as E
    from base.execution import Panel
    spec = E.spec_from_dict(ctx['spec'])
    dates = pd.DatetimeIndex(ctx['dates'])
    p = ctx['panel']
    panel = Panel(dates=dates, symbols=list(ctx['symbols']), open=p['open'], high=p['high'], low=p['low'],
                  close=p['close'], volume=p['volume'], tradable=p['tradable'])
    frames = {}
    for s, d in ctx['frames'].items():
        frames[s] = pd.DataFrame({c: d[c] for c in ('Open', 'High', 'Low', 'Close', 'Volume') if c in d},
                                 index=pd.DatetimeIndex(d['t']))
    _STATE.clear()
    _STATE.update({'token': ctx['token'], 'spec': spec, 'panel': panel,
                   'book': E.SignalBook(panel, frames, list(ctx['symbols'])), 'sims': {},
                   'ppy': E.periods_per_year_by_symbol(frames, list(ctx['symbols'])), 'backend': ctx['backend']})
    return _STATE


def event_path_ok(spec, panel) -> bool:
    """True when long_flat_events() reproduces base.simulator exactly for this run."""
    cm = spec.costs
    return (spec.kind == 'signals' and spec.strategy != 'code' and len(panel.symbols) == 1 and not spec.allow_short
            and not (spec.stop_loss or spec.take_profit or spec.trailing_stop) and spec.sizing['mode'] == 'percent'
            and spec.weights is None and spec.fractional and cm.commission_type == 'pct_notional'
            and not cm.min_commission and not cm.max_commission_pct and not cm.impact_coef
            and cm.participation_cap is None and not cm.cash_rate_annual and not cm.financing_rate_annual
            and cm.slippage_type in ('pct_price', 'none') and bool(np.all(panel.tradable))
            and bool(np.all(np.isfinite(panel.close))) and panel.adjustment != 'none')


def long_flat_events(open_, close, entries, exits, *, fill: str, size: float, commission: float, slip,
                     init_cash: float, start: int = 0):
    """(equity (n,), fills dict) of one long/flat symbol - the arithmetic of base.simulator's signals
    mode (pct-notional commission, price-fraction slippage, fractional shares, size x equity per
    entry capped by cash) evaluated only on the bars where an order fills."""
    c = np.asarray(close, dtype=float)
    n = len(c)
    o = np.asarray(open_, dtype=float)
    o = np.where(np.isfinite(o), o, c)
    le = np.asarray(entries, dtype=bool)
    lx = np.asarray(exits, dtype=bool)
    sl = np.broadcast_to(np.asarray(slip, dtype=float), (n,))
    en = np.zeros(n, dtype=bool)
    ex = np.zeros(n, dtype=bool)
    if fill == 'same_close':
        en[:] = le
        ex[:] = lx
        px = c
    else:
        en[1:] = le[:-1]
        ex[1:] = lx[:-1]
        px = o if fill == 'next_open' else c
    if start > 0:
        en[:start] = False
        ex[:start] = False
    cv = float(commission)
    cash = float(init_cash)
    pos = 0.0
    eq = np.empty(n)
    f_bar, f_qty, f_px, f_ref, f_comm = [], [], [], [], []
    last = 0          # equity filled for bars < last
    entry_bar = -1
    for i in np.flatnonzero(en | ex):
        i = int(i)
        p = float(px[i])
        s = float(sl[i])
        if pos > 0.0 and ex[i]:
            eq[last:i] = cash_after + pos * c[last:i]
            q = -pos
            fp = p * (1.0 - s)
            fee = abs(q) * fp * cv
            cash -= q * fp + fee
            pos = 0.0
            f_bar.append(i)
            f_qty.append(q)
            f_px.append(fp)
            f_ref.append(p)
            f_comm.append(fee)
            last = i
            entry_bar = -1
        if pos == 0.0 and en[i] and not ex[i]:
            if not (p > 0.0):
                continue
            budget = size * cash / 1.0
            if budget > cash:
                budget = cash
            if not (budget > 0.0):
                continue
            fp = p * (1.0 + s)
            q = budget / (fp * (1.0 + cv))
            fee0 = abs(q) * fp * cv
            if q * fp + fee0 > budget * (1.0 + 1e-12):
                q = (budget - fee0) / fp
            if q <= 0.0:
                continue
            fee = abs(q) * fp * cv
            eq[last:i] = cash
            cash -= q * fp + fee
            pos = q
            cash_after = cash
            f_bar.append(i)
            f_qty.append(q)
            f_px.append(fp)
            f_ref.append(p)
            f_comm.append(fee)
            last = i
            entry_bar = i
    if pos > 0.0:
        eq[last:] = cash + pos * c[last:]
    else:
        eq[last:] = cash
    m = len(f_bar)
    fills = {'bar': np.asarray(f_bar, dtype=np.int64), 'asset': np.zeros(m, dtype=np.int64),
             'qty': np.asarray(f_qty, dtype=float), 'price': np.asarray(f_px, dtype=float),
             'ref_price': np.asarray(f_ref, dtype=float), 'commission': np.asarray(f_comm, dtype=float),
             'reason': np.zeros(m, dtype=np.int64), 'phase': np.full(m, 0 if fill == 'next_open' else 2, np.int64)}
    return eq, fills


def _returns(eq: np.ndarray) -> np.ndarray:
    r = np.zeros(len(eq))
    with np.errstate(divide='ignore', invalid='ignore'):
        r[1:] = np.where(eq[:-1] > 0, eq[1:] / eq[:-1] - 1.0, 0.0)
    return r


def evaluate(params: Dict[str, Any], context: Dict[str, Any], window: Optional[Dict[str, int]]) -> Dict[str, Any]:
    import vbt_engine as E
    st = _state(context)
    spec, panel = st['spec'], st['panel']
    n = panel.n
    a, s, e = (0, 0, n) if window is None else (int(window['dataStart']), int(window['scoreStart']),
                                                 int(window['end']))
    if st['backend'] == 'events':
        p = dict(spec.params)
        p.update(params or {})
        ent, exi = st['book'].signals(spec.strategy, p)
        if 'slip' not in st:
            st['slip'] = spec.costs.slippage_fractions(panel.close, panel.volume)[:, 0]
        eq, fills = long_flat_events(panel.open[a:e, 0], panel.close[a:e, 0], ent[a:e, 0], exi[a:e, 0],
                                     fill=spec.fill, size=float(spec.sizing['fraction']),
                                     commission=spec.costs.commission_value, slip=st['slip'][a:e],
                                     init_cash=spec.initial_capital, start=s - a)
        out = {'returns': _returns(eq)[s - a:]}
        out.update(E.trade_extras(fills, 1))
        return out
    sim = st['sims'].get((a, e))
    if sim is None:
        sim = E.make_simulator(spec, E.panel_slice(panel, a, e), backend=st['backend'])
        st['sims'][(a, e)] = sim
    p = dict(spec.params)
    p.update(params or {})
    res = E.simulate(spec, sim, st['book'], p, start=s - a, rows=slice(a, e), ppy=st['ppy'])
    out = {'returns': res.returns[s - a:]}
    if spec.kind == 'signals':
        out.update(E.trade_extras(res.fill_arrays, len(panel.symbols)))
    else:
        out.update({'trades': int(len(res.fill_arrays['bar'])), 'winRate': None, 'profitFactor': None, 'sqn': None})
    return out
