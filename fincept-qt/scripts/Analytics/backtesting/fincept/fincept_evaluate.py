"""
Module-level evaluate(params, context, window) of the "fincept" engine for base.robustness (optimise and
walk-forward), importable by spawn-pool workers on Windows.

Every trial is a FULL run of the strategy's own code in the Fincept bar-loop engine with the trial's
parameters set (QCAlgorithm.get_parameter), the same costs and fill timing as run_backtest. The context
is a plain picklable dict built once per command (fe_engine._context): strategy id, symbols, request
dates, cash, cost model fields, fill timing, base parameters, the bar axis (dates of the reference run)
and the host's cancel-flag path. Bars are read through fincept_engine.price_feed, whose per-process memo
serves every trial after the first from memory (one price-store read per symbol per worker).

  window None                       the full period, from the first bar like run_backtest
  {'dataStart','scoreStart','end'}  the run covers dates[scoreStart] .. dates[end - 1]; the strategy's
                                    registered indicators are seeded with the bars before scoreStart
                                    (the engine's automatic indicator warm-up - strategy state is not
                                    replayed, orders cannot happen before scoreStart); nothing after `end`
                                    is loaded into the loop.

Returns {'returns': per-bar returns of the scored bars (first = the change from the initial cash),
'trades', 'winRate', 'profitFactor', 'sqn'} (closed round trips).

Cancellation: a trial checks the cancel flag every bar and raises (never exits: a pool worker must not
print an envelope); the parent's TaskRunner then exits with the cancelled envelope.
"""

from __future__ import annotations

import os
import sys
import time
from typing import Any, Dict, Optional

_HERE = os.path.dirname(os.path.abspath(__file__))
_BT = os.path.dirname(_HERE)
for _p in (_BT, os.path.join(_BT, 'base'), _HERE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

_STATE: Dict[str, Any] = {'token': None}


class TrialCancelled(RuntimeError):
    pass


def _state(ctx: Dict[str, Any]) -> Dict[str, Any]:
    if _STATE.get('token') == ctx['token']:
        return _STATE
    import fe_engine as E
    from base.costs import CostModel
    spec = E.Spec(ctx['strategy_id'], ctx['strategy_id'], list(ctx['symbols']), ctx['start'], ctx['end'],
                  float(ctx['cash']), dict(ctx['base_params']), CostModel(**ctx['cost_model']), ctx['fill'])
    _STATE.clear()
    _STATE.update({'token': ctx['token'], 'spec': spec, 'dates': list(ctx['dates'])})
    return _STATE


def _cancel_checker(path: Optional[str]):
    if not path:
        return None
    last = [0.0]

    def on_bar(i, n, day, warming):
        now = time.monotonic()
        if now - last[0] < 0.2:
            return
        last[0] = now
        if os.path.exists(path):
            raise TrialCancelled('Cancelled by user')
    return on_bar


def evaluate(params: Dict[str, Any], context: Dict[str, Any], window: Optional[Dict[str, int]]) -> Dict[str, Any]:
    import numpy as np
    import fe_engine as E
    st = _state(context)
    spec, dates = st['spec'], st['dates']
    n = len(dates)
    s, e = (0, n) if window is None else (int(window['scoreStart']), int(window['end']))
    p = dict(spec.params)
    p.update(params or {})
    inner = E.run_engine(spec, p, start=dates[s], end=dates[e - 1], on_bar=_cancel_checker(context.get('cancel_file')),
                         auto_warm=window is not None)
    eq = {E._day(r.get('time')): float(r.get('equity')) for r in inner.get('equity') or []}
    want = dates[s:e]
    vals = np.empty(len(want))
    last = float(spec.cash)
    for i, d in enumerate(want):          # the engine's own calendar; a missing day carries the last value
        last = eq.get(d, last)
        vals[i] = last
    rets = np.empty(len(vals))
    rets[0] = vals[0] / float(spec.cash) - 1.0
    rets[1:] = vals[1:] / np.where(vals[:-1] != 0, vals[:-1], np.nan) - 1.0
    rets = np.where(np.isfinite(rets), rets, 0.0)
    trades = E.round_trips(inner.get('trades') or [], inner.get('open_positions'), inner.get('carry_open'),
                           inner.get('corporate_events'))
    out = {'returns': rets}
    out.update(E.trade_extras(trades))
    return out
