"""
Module-level evaluate(params, context, window) of the Backtesting.py engine for base.robustness
(optimise and walk-forward), importable by spawn-pool workers on Windows.

EVERY COMBINATION IS A BACKTESTING.PY RUN - the same FinceptBacktest / SignalExecutor / broker
hooks, costs, fill timing, sizing, stops, leverage and shorting as run_backtest. Only the
library's own statistics are skipped (returns are scored by base.robustness / base.metrics).

The context is a plain picklable dict (the symbol's bars and the run's Spec) built ONCE per
command and sent once per worker. Each worker keeps for the life of the context: one memoised
indicator accessor (btp_strategies.TA - each distinct indicator / window computed once for all
trials and windows), the signals of every parameter set it has seen, and one Backtest object per
window (Backtest.run is re-entrant: every run gets a fresh broker and strategy).

Windows: signals are computed on the FULL history (indicators are causal) and decisions before
window['scoreStart'] are dropped, so warm-up bars never trade and nothing after window['end'] is
used. The library then runs on bars [scoreStart - 1, end) only (the bar before the first scored
bar carries no decision; it anchors the first scored return). Custom strategy code cannot be
masked: it runs on [dataStart, end) and only bars from scoreStart are scored.

Returns per trial: {'returns': per-bar returns of the scored bars, 'trades': closed round trips,
'winRate', 'profitFactor', 'sqn'}.
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


def make_context(spec, frame, cancel_file: Optional[str] = None) -> Dict[str, Any]:
    return {'token': uuid.uuid4().hex, 'spec': spec.to_dict(), 'symbol': spec.symbol,
            't': np.asarray(frame.index.asi8, dtype=np.int64),
            'cols': {c: frame[c].to_numpy(dtype=float) for c in ('Open', 'High', 'Low', 'Close', 'Volume')},
            'cancelFile': cancel_file}


def _in_worker() -> bool:
    import multiprocessing as mp
    return mp.parent_process() is not None


def _state(ctx: Dict[str, Any]) -> Dict[str, Any]:
    if _STATE.get('token') == ctx['token']:
        return _STATE
    import pandas as pd
    import btp_engine as E
    spec = E.spec_from_dict(ctx['spec'])
    frame = pd.DataFrame(ctx['cols'], index=pd.DatetimeIndex(ctx['t']))
    hook = None
    if ctx.get('cancelFile') and not _in_worker():
        # in-process runs check the host's cancel flag inside long bar loops too (a worker never
        # exits on its own: the parent's task runner cancels the pool)
        from base.progress import CancelToken
        tok = CancelToken(path=ctx['cancelFile'])

        def hook(_i, _tok=tok):
            _tok.check()
    _STATE.clear()
    _STATE.update({'token': ctx['token'], 'spec': spec, 'frame': frame, 'ta': E.make_ta(frame),
                   'ppy': E.ppy_of(frame), 'fx': E.make_fx(spec, frame), 'bts': {}, 'sigs': {}, 'hook': hook,
                   'plans': {}})
    return _STATE


def _signals(st, params: Dict[str, Any]):
    import btp_strategies as ST
    key = tuple(sorted((k, repr(v)) for k, v in params.items()))
    sig = st['sigs'].get(key)
    if sig is None:
        sig = ST.build_signals(st['spec'].strategy, st['ta'], params)
        if len(st['sigs']) > 4096:
            st['sigs'].clear()
        st['sigs'][key] = sig
    return sig


def _backtest(st, r0: int, e: int):
    import btp_engine as E
    bt = st['bts'].get((r0, e))
    if bt is None:
        fx = {k: (v[r0:e] if isinstance(v, np.ndarray) else v) for k, v in st['fx'].items()}
        bt = E.make_backtest(st['spec'], st['frame'].iloc[r0:e], fx)
        if len(st['bts']) > 64:
            st['bts'].clear()
        st['bts'][(r0, e)] = bt
    return bt


def evaluate(params: Dict[str, Any], context: Dict[str, Any], window: Optional[Dict[str, int]]) -> Dict[str, Any]:
    import btp_engine as E
    import btp_executor as X
    import btp_strategies as ST
    st = _state(context)
    spec = st['spec']
    n = len(st['frame'])
    a, s, e = (0, 0, n) if window is None else (int(window['dataStart']), int(window['scoreStart']),
                                                 int(window['end']))
    p = dict(spec.params)
    p.update(params or {})
    if spec.strategy == 'code':
        run = E.run_once(spec, _backtest(st, a, e), None, None, params=p)
        return {'returns': X.returns_of(run.equity)[s - a:], **X.round_trip_extras(run.fills)}
    r0 = s - 1 if s > 0 else 0
    sig = _signals(st, p)
    if spec.strategy == 'buy_and_hold':
        sig = dict(sig)
        ent = np.zeros(n, dtype=bool)
        ent[s] = True
        sig['e'], sig['act'] = ent, ent | sig['x']
    else:
        sig = ST.mask_before(sig, s)
    sig = ST.slice_signals(sig, r0, e)
    plan = st['plans'].get('base')
    if plan is None:
        plan = st['plans']['base'] = E.make_plan(spec, st['ta'], st['ppy'], hook=st['hook'])
    if plan.get('vol') is not None:
        plan = dict(plan, vol=plan['vol'][r0:e])
    run = E.run_once(spec, _backtest(st, r0, e), sig, plan)
    r = X.returns_of(run.equity)
    return {'returns': r[s - r0:], **X.round_trip_extras(run.fills)}
