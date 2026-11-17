"""
Fincept reference simulator: bar-by-bar portfolio accounting shared by every engine that
does not run a third-party library (today's "VectorBT" and "Zipline" loops are rebased onto it).

    from base.simulator import Simulator, simulate
    sim = Simulator(panel, costs=CostModel.from_request(req), fill='next_open', initial_capital=100_000)
    res = sim.run(entries=fast_x_up, exits=fast_x_down)          # signal strategy (long/flat)
    res = sim.run(weights=target_w)                               # target-weight rebalancing
    r   = sim.returns(entries=e, exits=x)                         # fast path for optimisation
    res.trades(), res.fills(), res.equity, res.costs_summary(), res.exposure()

MODEL (one kernel, written once in plain Python; compiled with numba when numba is installed -
the SAME source runs in both backends, tests compare them bar for bar)
  bar i, in this order:
    1. corporate actions (raw prices only): split ratio multiplies shares, dividends are cash
       (shorts pay) on the ex-date, before the open;
    2. accruals for (t_{i-1}, t_i]: interest on free cash (cash net of short proceeds) or
       financing on a negative balance; borrow fee on short market value (ACT/360 by default);
    3. next_open: pending orders fill at the OPEN;
    4. intrabar stop-loss / take-profit / trailing stop on the bar's high and low (signals
       mode): gap through the level -> fill at the open; stop before target when both are
       touched in one bar; stops are market orders (slippage), targets are limits (no slippage);
    5. next_close: pending orders fill at the CLOSE; same_close: the bar's own signals fill at
       its close (explicit opt-in only);
    6. equity = cash + sum(shares x close);
    7. the bar's signals become pending orders (next_* modes).
  Orders for a symbol only fill on bars where it really traded (Panel.tradable); signals on
  bars where its market was closed are ignored (never forward-filled).

  SIGNALS MODE  entries / exits (and short_entries / short_exits) are booleans decided on a
    bar's close. Long entry when flat and not exiting; short entry likewise; an exit and an
    opposite entry on the same bar reverse the position. Entry size = size x equity / slots
    (slots defaults to the number of symbols), capped by cash for longs unless
    max_leverage > 1; quantity = largest that fits with commission at the fill price;
    whole shares when fractional=False. This reproduces the audit's independent reference
    (tests/reference.py simulate) exactly.
    Per-entry sizes: run(size=<(n, k) array>, size_type='fraction' | 'value') sizes each entry
    by the value on its DECISION bar (the bar whose close produced the entry signal):
    'fraction' -> value x equity / slots (the scalar rule with a per-bar value), 'value' ->
    that amount of money; 0 / NaN = skip the entry. (Sizing rules such as volatility targets
    or fixed amounts; a scalar `size` keeps the rule above.)
  WEIGHTS MODE  target weights (fraction of equity, negative = short) decided on a bar's
    close; NaN = no change. On the fill bar every listed symbol is traded to its target;
    commission and slippage are paid out of the portfolio (4-step fixed point, the same as
    the reference monthly equal-weight rebalance); sells before buys.
"""

from __future__ import annotations

import math
import os
import types
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

import numpy as np

from base.costs import CostModel, adv as _adv, daily_vol
from base.execution import (FILL_MODE_CODES, PHASE_CLOSE, Panel, align_panel, fifo_round_trips,
                            fills_table, normalize_fill_mode, positions_table, year_fractions)

_COMM_CODES = {'pct_notional': 0, 'per_share': 1, 'per_trade': 2}
_REASON_SIGNAL, _REASON_STOP, _REASON_TP, _REASON_TRAIL, _REASON_END, _REASON_REBAL = 0, 1, 2, 3, 4, 5


# ════════════════════════════════════════════════════════════════════════════
# Kernel (plain Python; numba-compiled copy built on demand)
# ════════════════════════════════════════════════════════════════════════════

def _fee(q, p, ct, cv, cmin, cmax):
    """Commission for |q| units at price p."""
    aq = abs(q)
    if aq <= 0.0:
        return 0.0
    if ct == 0:
        f = aq * p * cv
    elif ct == 1:
        f = aq * cv
    else:
        f = cv
    if cmin > 0.0 and f < cmin:
        f = cmin
    if cmax > 0.0 and f > aq * p * cmax:
        f = aq * p * cmax
    return f


def _qty_for_budget(budget, fp, ct, cv, cmin, cmax, fractional):
    if budget <= 0.0 or fp <= 0.0:
        return 0.0
    if ct == 0:
        q = budget / (fp * (1.0 + cv))
    elif ct == 1:
        q = budget / (fp + cv)
    else:
        q = (budget - cv) / fp
    if q <= 0.0:
        return 0.0
    f = _fee(q, fp, ct, cv, cmin, cmax)
    if q * fp + f > budget * (1.0 + 1e-12):
        q = (budget - f) / fp
    if q <= 0.0:
        return 0.0
    if not fractional:
        q = math.floor(q)
    return q


def _kernel(o, h, l, c, tradable, mode, le, lx, se, sx, w, fill_mode, size_frac, slots, fractional,
            init_cash, ct, cv, cmin, cmax, slip, impact_coef, sigma, advv, pcap,
            borrow_rate, cash_rate, fin_rate, dt, div, spl, has_actions,
            sl, tp, ts, start, close_at_end, max_lev, sz, sz_mode,
            equity, cash_out, pos_out, borrow_out,
            f_bar, f_asset, f_qty, f_px, f_ref, f_comm, f_reason, f_phase, acc):
    n = c.shape[0]
    k = c.shape[1]
    cash = init_cash
    pos = np.zeros(k)
    entry_px = np.zeros(k)
    peak = np.zeros(k)
    p_sz = np.zeros(k)   # size of the pending entry (per-entry sizing, sz_mode 1 / 2)
    p_le = np.zeros(k, dtype=np.bool_)
    p_lx = np.zeros(k, dtype=np.bool_)
    p_se = np.zeros(k, dtype=np.bool_)
    p_sx = np.zeros(k, dtype=np.bool_)
    p_w = np.full(k, np.nan)
    has_stops = sl > 0.0 or tp > 0.0 or ts > 0.0
    nf = 0
    tgt = np.zeros(k)
    for i in range(n):
        trading = i >= start
        # 1. corporate actions (raw prices)
        if has_actions and i > 0:
            for j in range(k):
                r = spl[i, j]
                if r != 1.0 and pos[j] != 0.0:
                    pos[j] = pos[j] * r
                    entry_px[j] = entry_px[j] / r
                    peak[j] = peak[j] / r
                d = div[i, j]
                if d != 0.0 and pos[j] != 0.0:
                    cash += pos[j] * d
                    acc[4] += pos[j] * d
        # 2. accruals on positions held at the previous close
        if i > start and dt[i] > 0.0:
            sv = 0.0
            for j in range(k):
                if pos[j] < 0.0:
                    sv += -pos[j] * c[i - 1, j]
            free = cash - sv
            if free > 0.0:
                interest = free * cash_rate * dt[i]
            else:
                interest = free * fin_rate * dt[i]
            cash += interest
            if interest >= 0.0:
                acc[0] += interest
            else:
                acc[1] += -interest
            if sv > 0.0 and borrow_rate > 0.0:
                for j in range(k):
                    if pos[j] < 0.0:
                        b = -pos[j] * c[i - 1, j] * borrow_rate * dt[i]
                        cash -= b
                        borrow_out[i, j] = b
                        acc[2] += b
        for step in range(3):
            # step 0: next_open execution; step 1: intrabar stops; step 2: close execution
            if step == 1:
                if mode == 0 and has_stops and trading:
                    for j in range(k):
                        if pos[j] == 0.0 or not tradable[i, j]:
                            continue
                        hit = False
                        px = 0.0
                        reason = _REASON_STOP
                        is_limit = False
                        if pos[j] > 0.0:
                            stop_lvl = -1.0
                            if sl > 0.0:
                                stop_lvl = entry_px[j] * (1.0 - sl)
                            if ts > 0.0:
                                tl = peak[j] * (1.0 - ts)
                                if tl > stop_lvl:
                                    stop_lvl = tl
                                    reason = _REASON_TRAIL
                            tp_lvl = entry_px[j] * (1.0 + tp) if tp > 0.0 else 1e300
                            if stop_lvl > 0.0 and o[i, j] <= stop_lvl:
                                hit = True
                                px = o[i, j]
                            elif stop_lvl > 0.0 and l[i, j] <= stop_lvl:
                                hit = True
                                px = stop_lvl
                            elif tp > 0.0 and o[i, j] >= tp_lvl:
                                hit = True
                                px = o[i, j]
                                reason = _REASON_TP
                                is_limit = True
                            elif tp > 0.0 and h[i, j] >= tp_lvl:
                                hit = True
                                px = tp_lvl
                                reason = _REASON_TP
                                is_limit = True
                            if not hit and h[i, j] > peak[j]:
                                peak[j] = h[i, j]
                        else:
                            stop_lvl = 1e300
                            if sl > 0.0:
                                stop_lvl = entry_px[j] * (1.0 + sl)
                            if ts > 0.0:
                                tl = peak[j] * (1.0 + ts)
                                if tl < stop_lvl:
                                    stop_lvl = tl
                                    reason = _REASON_TRAIL
                            tp_lvl = entry_px[j] * (1.0 - tp) if tp > 0.0 else -1.0
                            if stop_lvl < 1e299 and o[i, j] >= stop_lvl:
                                hit = True
                                px = o[i, j]
                            elif stop_lvl < 1e299 and h[i, j] >= stop_lvl:
                                hit = True
                                px = stop_lvl
                            elif tp > 0.0 and o[i, j] <= tp_lvl:
                                hit = True
                                px = o[i, j]
                                reason = _REASON_TP
                                is_limit = True
                            elif tp > 0.0 and l[i, j] <= tp_lvl:
                                hit = True
                                px = tp_lvl
                                reason = _REASON_TP
                                is_limit = True
                            if not hit and l[i, j] < peak[j]:
                                peak[j] = l[i, j]
                        if hit:
                            q = -pos[j]
                            s = 0.0 if is_limit else slip[i, j]
                            fp = px * (1.0 + s) if q > 0.0 else px * (1.0 - s)
                            fee = _fee(q, fp, ct, cv, cmin, cmax)
                            cash -= q * fp + fee
                            pos[j] = 0.0
                            f_bar[nf] = i
                            f_asset[nf] = j
                            f_qty[nf] = q
                            f_px[nf] = fp
                            f_ref[nf] = px
                            f_comm[nf] = fee
                            f_reason[nf] = reason
                            f_phase[nf] = 1
                            nf += 1
                continue
            if not trading:
                continue
            if step == 0 and fill_mode != 0:
                continue
            if step == 2 and fill_mode == 0:
                continue
            if fill_mode == 2:
                # same_close: this bar's own decisions
                for j in range(k):
                    if tradable[i, j]:
                        if mode == 0:
                            p_le[j] = p_le[j] or le[i, j]
                            p_lx[j] = p_lx[j] or lx[i, j]
                            p_se[j] = p_se[j] or se[i, j]
                            p_sx[j] = p_sx[j] or sx[i, j]
                            if sz_mode != 0 and (le[i, j] or se[i, j]):
                                p_sz[j] = sz[i, j]
                    if mode == 1 and w[i, j] == w[i, j]:
                        p_w[j] = w[i, j]
            # ---- execute pending orders at price row px_row (open or close of bar i)
            phase = 0 if step == 0 else 2
            if mode == 0:
                # exits (and covers) first
                for j in range(k):
                    if not tradable[i, j]:
                        continue
                    px = o[i, j] if phase == 0 else c[i, j]
                    if (pos[j] > 0.0 and p_lx[j]) or (pos[j] < 0.0 and p_sx[j]):
                        q = -pos[j]
                        s = slip[i, j]
                        fp = px * (1.0 + s) if q > 0.0 else px * (1.0 - s)
                        fee = _fee(q, fp, ct, cv, cmin, cmax)
                        cash -= q * fp + fee
                        pos[j] = 0.0
                        f_bar[nf] = i
                        f_asset[nf] = j
                        f_qty[nf] = q
                        f_px[nf] = fp
                        f_ref[nf] = px
                        f_comm[nf] = fee
                        f_reason[nf] = _REASON_SIGNAL
                        f_phase[nf] = phase
                        nf += 1
                # entries
                eq_now = cash
                for j in range(k):
                    pj = o[i, j] if phase == 0 else c[i, j]
                    if pos[j] != 0.0 and pj == pj:
                        eq_now += pos[j] * pj
                for j in range(k):
                    if not tradable[i, j] or pos[j] != 0.0:
                        continue
                    go_long = p_le[j] and not p_lx[j]
                    go_short = (not go_long) and p_se[j] and not p_sx[j]
                    if not (go_long or go_short):
                        continue
                    px = o[i, j] if phase == 0 else c[i, j]
                    if not (px > 0.0):
                        continue
                    if sz_mode == 0:
                        budget = size_frac * eq_now / slots
                    elif sz_mode == 1:
                        budget = p_sz[j] * eq_now / slots
                    else:
                        budget = p_sz[j]
                    if not (budget > 0.0):
                        continue
                    if go_long:
                        avail = cash + (max_lev - 1.0) * eq_now
                        if budget > avail:
                            budget = avail
                    s = slip[i, j]
                    fp = px * (1.0 + s) if go_long else px * (1.0 - s)
                    q = _qty_for_budget(budget, fp, ct, cv, cmin, cmax, fractional)
                    if pcap > 0.0 and advv[i, j] > 0.0 and q > pcap * advv[i, j]:
                        q = pcap * advv[i, j]
                        if not fractional:
                            q = math.floor(q)
                        acc[3] += 1.0
                    if impact_coef > 0.0 and q > 0.0 and advv[i, j] > 0.0:
                        s = s + impact_coef * sigma[i, j] * math.sqrt(q / advv[i, j])
                        fp = px * (1.0 + s) if go_long else px * (1.0 - s)
                        q2 = _qty_for_budget(budget, fp, ct, cv, cmin, cmax, fractional)
                        if q2 < q:
                            q = q2
                    if q <= 0.0:
                        continue
                    sq = q if go_long else -q
                    fee = _fee(q, fp, ct, cv, cmin, cmax)
                    cash -= sq * fp + fee
                    pos[j] = sq
                    entry_px[j] = fp
                    peak[j] = fp
                    f_bar[nf] = i
                    f_asset[nf] = j
                    f_qty[nf] = sq
                    f_px[nf] = fp
                    f_ref[nf] = px
                    f_comm[nf] = fee
                    f_reason[nf] = _REASON_SIGNAL
                    f_phase[nf] = phase
                    nf += 1
                for j in range(k):
                    if tradable[i, j]:
                        p_le[j] = False
                        p_lx[j] = False
                        p_se[j] = False
                        p_sx[j] = False
            else:
                any_w = False
                for j in range(k):
                    if p_w[j] == p_w[j] and tradable[i, j]:
                        any_w = True
                if any_w:
                    val = cash
                    for j in range(k):
                        pj = o[i, j] if phase == 0 else c[i, j]
                        if pos[j] != 0.0 and pj == pj:
                            val += pos[j] * pj
                    gross = 0.0
                    for j in range(k):
                        if p_w[j] == p_w[j] and tradable[i, j]:
                            gross += abs(p_w[j])
                        else:
                            pj = o[i, j] if phase == 0 else c[i, j]
                            if pos[j] != 0.0 and val > 0.0:
                                gross += abs(pos[j] * pj) / val
                    scale = 1.0
                    if gross > max_lev and gross > 0.0:
                        scale = max_lev / gross
                    cost = 0.0
                    for it in range(4):
                        cost_new = 0.0
                        for j in range(k):
                            tgt[j] = pos[j]
                            if p_w[j] == p_w[j] and tradable[i, j]:
                                pj = o[i, j] if phase == 0 else c[i, j]
                                if pj > 0.0:
                                    tgt[j] = (val - cost) * p_w[j] * scale / pj
                                    if not fractional:
                                        tgt[j] = math.trunc(tgt[j])
                                    dq = tgt[j] - pos[j]
                                    if dq != 0.0:
                                        s = slip[i, j]
                                        fp = pj * (1.0 + s) if dq > 0.0 else pj * (1.0 - s)
                                        cost_new += _fee(dq, fp, ct, cv, cmin, cmax) + abs(dq) * pj * s
                        if it < 3:
                            cost = cost_new
                    for side in range(2):
                        # side 0: sells (dq < 0) first, side 1: buys
                        for j in range(k):
                            if not (p_w[j] == p_w[j] and tradable[i, j]):
                                continue
                            pj = o[i, j] if phase == 0 else c[i, j]
                            dq = tgt[j] - pos[j]
                            if dq == 0.0 or (side == 0 and dq > 0.0) or (side == 1 and dq < 0.0):
                                continue
                            s = slip[i, j]
                            fp = pj * (1.0 + s) if dq > 0.0 else pj * (1.0 - s)
                            if dq > 0.0 and max_lev <= 1.0:
                                need = dq * fp + _fee(dq, fp, ct, cv, cmin, cmax)
                                if need > cash + 1e-9:
                                    dq = _qty_for_budget(cash, fp, ct, cv, cmin, cmax, fractional)
                                    if dq <= 0.0:
                                        continue
                            fee = _fee(dq, fp, ct, cv, cmin, cmax)
                            cash -= dq * fp + fee
                            pos[j] += dq
                            f_bar[nf] = i
                            f_asset[nf] = j
                            f_qty[nf] = dq
                            f_px[nf] = fp
                            f_ref[nf] = pj
                            f_comm[nf] = fee
                            f_reason[nf] = _REASON_REBAL
                            f_phase[nf] = phase
                            nf += 1
                    for j in range(k):
                        if p_w[j] == p_w[j] and tradable[i, j]:
                            p_w[j] = np.nan
        # 6. mark to market at the close
        eq = cash
        for j in range(k):
            if pos[j] != 0.0:
                eq += pos[j] * c[i, j]
            pos_out[i, j] = pos[j]
        equity[i] = eq
        cash_out[i] = cash
        # 7. this bar's decisions become pending (next_open / next_close)
        if fill_mode != 2 and i + 1 >= start:
            for j in range(k):
                if mode == 0:
                    if tradable[i, j]:
                        p_le[j] = p_le[j] or le[i, j]
                        p_lx[j] = p_lx[j] or lx[i, j]
                        p_se[j] = p_se[j] or se[i, j]
                        p_sx[j] = p_sx[j] or sx[i, j]
                        if sz_mode != 0 and (le[i, j] or se[i, j]):
                            p_sz[j] = sz[i, j]
                elif w[i, j] == w[i, j]:
                    p_w[j] = w[i, j]
    if close_at_end and n > 0:
        i = n - 1
        for j in range(k):
            if pos[j] == 0.0:
                continue
            q = -pos[j]
            px = c[i, j]
            s = slip[i, j]
            fp = px * (1.0 + s) if q > 0.0 else px * (1.0 - s)
            fee = _fee(q, fp, ct, cv, cmin, cmax)
            cash -= q * fp + fee
            pos[j] = 0.0
            pos_out[i, j] = 0.0
            f_bar[nf] = i
            f_asset[nf] = j
            f_qty[nf] = q
            f_px[nf] = fp
            f_ref[nf] = px
            f_comm[nf] = fee
            f_reason[nf] = _REASON_END
            f_phase[nf] = 2
            nf += 1
        equity[i] = cash
        cash_out[i] = cash
    return nf


# ── numba compilation (same source, globals rebound to jitted helpers) ──────

_NB = {'kernel': None, 'tried': False, 'error': None}


def numba_available() -> bool:
    try:
        import importlib.util
        return importlib.util.find_spec('numba') is not None
    except Exception:
        return False


def _numba_kernel():
    if _NB['tried']:
        return _NB['kernel']
    _NB['tried'] = True
    try:
        if 'NUMBA_CACHE_DIR' not in os.environ:
            from base.paths import cache_root
            os.environ['NUMBA_CACHE_DIR'] = str(cache_root('numba'))
        import numba
        nb_fee = numba.njit(cache=True)(_fee)
        g = dict(_kernel.__globals__)
        g['_fee'] = nb_fee
        g['_qty_for_budget'] = numba.njit(cache=True)(
            types.FunctionType(_qty_for_budget.__code__, dict(g, _fee=nb_fee), '_qty_for_budget'))
        k = types.FunctionType(_kernel.__code__, g, '_kernel')
        _NB['kernel'] = numba.njit(cache=True, nogil=True)(k)
    except Exception as e:  # numba missing or failed: the Python kernel is used
        _NB['error'] = str(e)
        _NB['kernel'] = None
    return _NB['kernel']


# ════════════════════════════════════════════════════════════════════════════
# Public API
# ════════════════════════════════════════════════════════════════════════════

def _as_matrix(x, n: int, k: int, dtype, fill) -> np.ndarray:
    if x is None:
        return np.full((n, k), fill, dtype=dtype)
    if hasattr(x, 'to_numpy'):
        x = x.to_numpy()
    a = np.asarray(x)
    if a.ndim == 1:
        a = a[:, None]
    if a.shape[0] != n:
        raise ValueError(f'signal / weight array has {a.shape[0]} rows for {n} bars')
    if a.shape[1] == 1 and k > 1:
        raise ValueError(f'signal / weight array has 1 column for {k} symbols')
    if dtype is np.bool_:
        a = np.where(np.isnan(a.astype(float)), False, a.astype(float) != 0) if a.dtype != np.bool_ else a
    return np.ascontiguousarray(a, dtype=dtype)


def panel_from_arrays(close, open_=None, high=None, low=None, volume=None, dates=None, symbols=None) -> Panel:
    """A Panel from plain arrays (one symbol: 1-D arrays)."""
    import pandas as pd
    c = np.asarray(close, dtype=float)
    if c.ndim == 1:
        c = c[:, None]
    n, k = c.shape

    def m(x, default):
        if x is None:
            return default.copy()
        a = np.asarray(x, dtype=float)
        return a[:, None] if a.ndim == 1 else a
    o = m(open_, c)
    hi = m(high, np.maximum(o, c))
    lo = m(low, np.minimum(o, c))
    v = m(volume, np.zeros_like(c))
    if dates is None:
        dates = pd.bdate_range('2000-01-03', periods=n)
    return Panel(dates=pd.DatetimeIndex(dates), symbols=list(symbols or [f'S{j}' for j in range(k)]),
                 open=o, high=hi, low=lo, close=c, volume=v, tradable=np.isfinite(c))


@dataclass
class SimResult:
    panel: Panel
    equity: np.ndarray
    cash: np.ndarray
    positions: np.ndarray
    borrow: np.ndarray
    fill_arrays: Dict[str, np.ndarray]
    accruals: Dict[str, float]
    initial_capital: float
    fill_mode: str
    cost_model: CostModel
    mode: str
    backend: str
    warnings: List[Dict[str, Any]] = field(default_factory=list)
    _trades: Optional[List[Dict[str, Any]]] = field(default=None, repr=False)

    @property
    def dates(self):
        return self.panel.dates

    @property
    def returns(self) -> np.ndarray:
        """Per-bar returns, length n (first bar 0)."""
        e = self.equity
        r = np.zeros(len(e))
        with np.errstate(divide='ignore', invalid='ignore'):
            r[1:] = np.where(e[:-1] > 0, e[1:] / e[:-1] - 1.0, 0.0)
        return r

    def position_values(self) -> np.ndarray:
        return self.positions * np.nan_to_num(self.panel.close)

    def exposure_series(self) -> Dict[str, np.ndarray]:
        v = self.position_values()
        with np.errstate(divide='ignore', invalid='ignore'):
            gross = np.where(self.equity > 0, np.abs(v).sum(axis=1) / self.equity, np.nan)
            net = np.where(self.equity > 0, v.sum(axis=1) / self.equity, np.nan)
        count = (np.abs(self.positions) > 1e-12).sum(axis=1)
        return {'gross': gross, 'net': net, 'count': count}

    def weights_block(self, max_rows: int = 600) -> Optional[Dict[str, Any]]:
        """Contract `weights` (multi-asset runs only; None for one symbol): base.execution.weights_block."""
        from base.execution import weights_block
        return weights_block(self.panel.dates, self.panel.symbols, self.position_values(), self.equity, max_rows)

    def trades(self) -> List[Dict[str, Any]]:
        if self._trades is None:
            self._trades = self._make_trades()
        return [dict(t) for t in self._trades]

    def _make_trades(self) -> List[Dict[str, Any]]:
        p = self.panel
        return fifo_round_trips(self.fill_arrays, p.dates, p.symbols, last_close=p.close[-1], high=p.high,
                                low=p.low, borrow_per_bar=self.borrow, positions=self.positions,
                                dividends=p.dividends if p.adjustment == 'none' else None,
                                splits=p.splits if p.adjustment == 'none' else None)

    def fills(self) -> List[Dict[str, Any]]:
        return fills_table(self.fill_arrays, self.panel.dates, self.panel.symbols)

    def open_positions(self) -> List[Dict[str, Any]]:
        p = self.panel
        last = self.positions[-1]
        avg = np.zeros(len(p.symbols))
        for t in self.trades():
            if t.get('exitReason') == 'open':
                j = p.symbols.index(t['symbol'])
                avg[j] = t['entryPrice']
        return positions_table(p.symbols, last, avg, p.close[-1], float(self.equity[-1]))

    def costs_summary(self) -> Dict[str, float]:
        fa = self.fill_arrays
        q = np.abs(fa['qty'])
        return {
            'totalCommission': float(np.sum(fa['commission'])),
            'totalSlippage': float(np.sum(np.abs(fa['price'] - fa['ref_price']) * q)),
            'totalBorrow': float(self.accruals['borrow']),
            'cashInterest': float(self.accruals['interest']),
            'financingPaid': float(self.accruals['financing']),
            'dividends': float(self.accruals['dividends']),
            'tradedNotional': float(np.sum(q * fa['price'])),
            'fills': int(len(q)),
            'cappedFills': int(self.accruals['capped']),
        }


class Simulator:
    """Reusable simulator over one Panel (precomputes slippage, volatility, ADV, accrual
    fractions once; run() / returns() are then cheap enough for optimisation loops)."""

    def __init__(self, panel: Panel, *, costs: Optional[CostModel] = None, fill: str = 'next_open',
                 initial_capital: float = 100_000.0, size: float = 1.0, slots: Optional[int] = None,
                 fractional: bool = True, max_leverage: float = 1.0, stop_loss: float = 0.0,
                 take_profit: float = 0.0, trailing_stop: float = 0.0, close_at_end: bool = False,
                 backend: str = 'auto'):
        if not isinstance(panel, Panel):
            raise TypeError('Simulator needs a base.execution.Panel (align_panel / panel_from_arrays)')
        self.panel = panel
        self.costs = costs or CostModel(preset='none')
        self.fill_mode = normalize_fill_mode(fill)
        self.initial_capital = float(initial_capital)
        if not self.initial_capital > 0:
            raise ValueError('initial capital must be positive')
        self.size = float(size)
        self.slots = int(slots) if slots else len(panel.symbols)
        self.fractional = bool(fractional)
        self.max_leverage = float(max_leverage or 1.0)
        self.stop_loss, self.take_profit, self.trailing_stop = float(stop_loss or 0), float(take_profit or 0), \
            float(trailing_stop or 0)
        self.close_at_end = bool(close_at_end)
        self.backend = backend
        p = panel
        n, k = p.close.shape
        self.n, self.k = n, k
        self._o = np.ascontiguousarray(np.where(np.isfinite(p.open), p.open, p.close), dtype=float)
        self._h = np.ascontiguousarray(np.where(np.isfinite(p.high), p.high, p.close), dtype=float)
        self._l = np.ascontiguousarray(np.where(np.isfinite(p.low), p.low, p.close), dtype=float)
        self._c = np.ascontiguousarray(p.close, dtype=float)
        self._tr = np.ascontiguousarray(p.tradable & np.isfinite(p.close), dtype=np.bool_)
        cm = self.costs
        self._slip = np.ascontiguousarray(cm.slippage_fractions(self._c, p.volume), dtype=float)
        need_vol = cm.impact_coef > 0 or cm.participation_cap is not None
        self._sigma = np.ascontiguousarray(daily_vol(self._c) if need_vol else np.zeros((n, k)))
        self._adv = np.ascontiguousarray(_adv(p.volume) if need_vol else np.zeros((n, k)))
        self._dt = year_fractions(p.dates, cm.day_count)
        raw = p.adjustment == 'none' and p.dividends is not None
        self._div = np.ascontiguousarray(p.dividends if raw else np.zeros((n, k)), dtype=float)
        self._spl = np.ascontiguousarray(p.splits if (raw and p.splits is not None) else np.ones((n, k)), dtype=float)
        self._has_actions = bool(raw)

    def _pick_backend(self, runs_hint: int) -> str:
        b = self.backend
        if b == 'python':
            return 'python'
        if b == 'numba' or (b == 'auto' and self.n * self.k * max(1, runs_hint) >= 200_000):
            return 'numba' if _numba_kernel() is not None else 'python'
        return 'python'

    def run(self, *, entries=None, exits=None, short_entries=None, short_exits=None, weights=None,
            start: int = 0, runs_hint: int = 1, size=None, size_type: str = 'fraction') -> SimResult:
        """size: None (the Simulator's scalar size), a scalar, or an (n, k) array of per-entry sizes
        indexed by the DECISION bar (see module doc); size_type 'fraction' (x equity / slots) or
        'value' (money). Signals mode only."""
        n, k = self.n, self.k
        mode = 1 if weights is not None else 0
        if mode == 0 and entries is None and short_entries is None:
            raise ValueError('give entries (and exits) for a signal strategy, or weights for a rebalancing one')
        if size_type not in ('fraction', 'value'):
            raise ValueError(f"size_type must be 'fraction' or 'value', not {size_type!r}")
        size_frac = self.size
        sz_mode = 0
        sz = np.zeros((1, 1))
        if size is not None:
            if np.isscalar(size) and size_type == 'fraction':
                size_frac = float(size)
            else:
                a = np.full((n, k), float(size)) if np.isscalar(size) else _as_matrix(size, n, k, float, 0.0)
                sz = np.ascontiguousarray(np.where(np.isfinite(a) & (a > 0), a, 0.0), dtype=float)
                sz_mode = 1 if size_type == 'fraction' else 2
        le = _as_matrix(entries, n, k, np.bool_, False)
        lx = _as_matrix(exits, n, k, np.bool_, False)
        se = _as_matrix(short_entries, n, k, np.bool_, False)
        sx = _as_matrix(short_exits, n, k, np.bool_, False)
        w = _as_matrix(weights, n, k, float, np.nan) if mode == 1 else np.full((1, 1), np.nan)
        if mode == 1 and w.shape != (n, k):
            w = np.full((n, k), np.nan)
        cap = n * k * 3 + k + 8
        fa = {'bar': np.zeros(cap, np.int64), 'asset': np.zeros(cap, np.int64), 'qty': np.zeros(cap),
              'price': np.zeros(cap), 'ref_price': np.zeros(cap), 'commission': np.zeros(cap),
              'reason': np.zeros(cap, np.int64), 'phase': np.zeros(cap, np.int64)}
        equity = np.zeros(n)
        cash = np.zeros(n)
        pos = np.zeros((n, k))
        borrow = np.zeros((n, k))
        acc = np.zeros(5)
        cm = self.costs
        backend = self._pick_backend(runs_hint)
        fn = _numba_kernel() if backend == 'numba' else _kernel
        nf = fn(self._o, self._h, self._l, self._c, self._tr, mode, le, lx, se, sx,
                w if mode == 1 else np.full((n, k), np.nan), FILL_MODE_CODES[self.fill_mode], float(size_frac),
                float(self.slots), self.fractional, self.initial_capital,
                _COMM_CODES[cm.commission_type], cm.commission_value, cm.min_commission,
                float(cm.max_commission_pct or 0.0), self._slip, cm.impact_coef, self._sigma, self._adv,
                float(cm.participation_cap or 0.0), cm.borrow_rate_annual, cm.cash_rate_annual,
                cm.financing_rate_annual, self._dt, self._div, self._spl, self._has_actions,
                self.stop_loss, self.take_profit, self.trailing_stop, int(start), self.close_at_end,
                self.max_leverage, sz, int(sz_mode), equity, cash, pos, borrow,
                fa['bar'], fa['asset'], fa['qty'], fa['price'], fa['ref_price'], fa['commission'],
                fa['reason'], fa['phase'], acc)
        fills = {key: v[:nf].copy() for key, v in fa.items()}
        warnings = list(self.panel.warnings)
        if self.fill_mode == 'same_close':
            warnings.append({'code': 'SAME_BAR_FILL', 'severity': 'warn', 'symbol': None,
                             'message': 'Orders fill at the close of the bar that produced the signal (requested '
                                        'explicitly). This is optimistic: the close is only known once it prints.'})
        if acc[3] > 0:
            warnings.append({'code': 'VOLUME_CAPPED', 'severity': 'info', 'symbol': None,
                             'message': f'{int(acc[3])} fills were reduced to the participation cap of the average '
                                        f'daily volume; the rest of those orders was not filled.'})
        return SimResult(panel=self.panel, equity=equity, cash=cash, positions=pos, borrow=borrow,
                         fill_arrays=fills,
                         accruals={'interest': float(acc[0]), 'financing': float(acc[1]), 'borrow': float(acc[2]),
                                   'capped': float(acc[3]), 'dividends': float(acc[4])},
                         initial_capital=self.initial_capital, fill_mode=self.fill_mode, cost_model=cm,
                         mode='weights' if mode == 1 else 'signals', backend=backend, warnings=warnings)

    def returns(self, **kw) -> np.ndarray:
        """Per-bar returns only (length n, first 0) - the optimisation fast path."""
        kw.setdefault('runs_hint', 100)
        return self.run(**kw).returns


def simulate(data, **kw) -> SimResult:
    """One-call form: simulate(panel_or_frames, entries=..., exits=..., fill='next_open', costs=..., ...).
    `data` is a Panel or {symbol: OHLCV DataFrame} (aligned on the union of session dates)."""
    run_keys = ('entries', 'exits', 'short_entries', 'short_exits', 'weights', 'start', 'runs_hint')
    run_kw = {k2: kw.pop(k2) for k2 in run_keys if k2 in kw}
    panel = data if isinstance(data, Panel) else align_panel(data)
    return Simulator(panel, **kw).run(**run_kw)
