"""
Module-level evaluate(params, context, window) of the "bt" engine for base.robustness (optimise
and walk-forward), importable by spawn-pool workers on Windows.

The context is a plain picklable dict (bars of every symbol, the run's settings) built ONCE per
command by make_context(); bars are loaded once for all combinations (v1 re-downloaded them for
every combination - 8.8 s each, audit T5.2). Each worker keeps, for the life of the context:
one SignalBook (memoised indicators + signal matrices per parameter set) or one DecisionBook (the
bt algo stack's decisions per parameter set, computed on the full history once and reused by
every walk-forward window), and one base.simulator.Simulator per window.

  signals     the strategy's entries / exits on the full history (causal), the window's rows
              simulated by base.simulator signals mode - positions open only from
              window['scoreStart'] on, nothing after window['end'] is used.
  allocation  the bt algo stack's decisions on the full history (lag 0: a decision on bar t uses
              bars <= t), executed by base.simulator weights mode on the window's rows; a test
              window starts from the latest decision made before it (filled on its first bar).

Returns per trial: {'returns': per-bar returns of the scored bars, 'trades': closed trades
(round trips / position episodes), 'winRate', 'profitFactor', 'sqn'} - same costs, fills and
sizing as run_backtest's base.simulator path.
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
    import bt_engine as E
    from base.execution import Panel
    from base.metrics import infer_periods_per_year
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
    _STATE.update({'token': ctx['token'], 'spec': spec, 'panel': panel, 'sims': {}, 'backend': ctx['backend']})
    if spec.kind == 'signals':
        _STATE['sbook'] = E.SignalBook(panel, frames, list(ctx['symbols']))
        _STATE['dbook'] = None
    else:
        _STATE['sbook'] = None
        _STATE['dbook'] = E.DecisionBook(panel, float(infer_periods_per_year(dates)))
    return _STATE


def evaluate(params: Dict[str, Any], context: Dict[str, Any], window: Optional[Dict[str, int]]) -> Dict[str, Any]:
    import bt_engine as E
    st = _state(context)
    spec, panel = st['spec'], st['panel']
    n = panel.n
    a, s, e = (0, 0, n) if window is None else (int(window['dataStart']), int(window['scoreStart']),
                                                 int(window['end']))
    sim = st['sims'].get((a, e))
    if sim is None:
        sim = E.make_simulator(spec, E.panel_slice(panel, a, e), backend=st['backend'])
        st['sims'][(a, e)] = sim
    p = dict(spec.params)
    p.update(params or {})
    res = E.simulate(spec, sim, st['sbook'], st['dbook'], p, start=s - a, rows=slice(a, e),
                     carry=window is not None)
    out = {'returns': res.returns[s - a:]}
    out.update(E.trade_extras_from_pnls(E.closed_pnls(res)))
    return out
