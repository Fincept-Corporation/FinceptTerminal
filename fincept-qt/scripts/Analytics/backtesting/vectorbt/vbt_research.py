"""
Research commands of the vectorbt engine: calculate_indicator, indicator_signals (5 modes),
indicator_param_sweep, generate_signals, generate_labels, labels_to_signals, generate_splits,
analyze_returns.

Common rules (audit T6 and the research findings):
  * bars come from base.price_store at the request's `interval` (broker candles when the host
    injected them for the symbol) - never a second download of the same symbol;
  * single-symbol commands use the FIRST requested symbol; when several are given the result
    says so (`symbol`, warnings[IGNORED_SYMBOLS]) instead of mixing two symbols' data;
  * indicator ids come from ONE table (vbt_indicators.INDICATORS); an unknown id is an error,
    never a silent substitute; a mode that cannot use an indicator says so;
  * dates are 'YYYY-MM-DD' for daily bars ('YYYY-MM-DD HH:MM:SS' intraday);
  * analyze_returns uses base.metrics (one definition of every metric) with the request's
    riskFreeRate and the bars-per-year of the data (252 / 365 / 52 / intraday);
  * signal statistics come from base.simulator (one position at a time, next-open fills, the
    request's commission / slippage when given) - not close-to-close from the signal bar.
"""

from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

import vbt_indicators as I

MAX_EVENTS = 500


class ResearchInputError(ValueError):
    pass


# ════════════════════════════════════════════════════════════════════════════
# Bars
# ════════════════════════════════════════════════════════════════════════════

def _fmt(ts) -> str:
    from base.metrics import _fmt_date
    return _fmt_date(ts)


def _num(v, default=None):
    try:
        x = float(v)
        return x if math.isfinite(x) else default
    except (TypeError, ValueError):
        return default


def load_one(req: Dict[str, Any], default_symbol: str = 'SPY'):
    """(symbol, OHLCV frame, meta) for the first requested symbol."""
    import vbt_engine as E
    from base.price_store import normalize_symbol
    raw = req.get('symbols') or ([req.get('symbol')] if req.get('symbol') else [default_symbol])
    syms = []
    for s in raw:
        ns = normalize_symbol(s)
        if ns and ns not in syms:
            syms.append(ns)
    if not syms:
        raise ResearchInputError('No symbol provided.')
    sym = syms[0]
    start = str(req.get('startDate') or '')[:10]
    end = str(req.get('endDate') or '')[:10]
    if not start or not end:
        raise ResearchInputError('startDate and endDate are required.')
    interval = str(req.get('interval') or '1d')
    warnings = []
    if len(syms) > 1:
        warnings.append({'code': 'IGNORED_SYMBOLS', 'severity': 'warn', 'symbol': None,
                         'message': f'This command analyses one symbol: {sym} was used; '
                                    f'{", ".join(syms[1:])} {"was" if len(syms) == 2 else "were"} ignored.'})
    spec = E.Spec(symbols=[sym], strategy='buy_and_hold', params={}, kind='signals', start=start, end=end,
                  interval=interval, initial_capital=1.0, fill='next_open', costs=None, sizing={'mode': 'percent'},
                  stop_loss=0.0, take_profit=0.0, trailing_stop=0.0, allow_short=False, weights=None,
                  fractional=True)
    data = E.load_data(spec, req)
    frame = data.frames[sym]
    if len(frame) < 2:
        raise ResearchInputError(f'{sym} has fewer than 2 bars between {start} and {end}.')
    warnings += [w for w in data.warnings if w.get('code') != 'SYMBOL_DROPPED']
    meta = {'symbol': sym, 'interval': interval, 'usingSyntheticData': data.synthetic, 'warnings': warnings,
            'dataSource': data.source}
    return sym, frame, meta


def _base(meta: Dict[str, Any], frame, **extra) -> Dict[str, Any]:
    out = {'symbol': meta['symbol'], 'interval': meta['interval'], 'totalBars': int(len(frame)),
           'usingSyntheticData': meta['usingSyntheticData'], 'dataSource': meta['dataSource'],
           'dateRange': {'start': _fmt(frame.index[0]), 'end': _fmt(frame.index[-1])},
           'warnings': list(meta['warnings']), 'engine': 'vectorbt',
           'engineImplementation': 'Fincept research tools (vbt_indicators / base.metrics / base.simulator) - '
                                   'the vectorbt library is not used'}
    out.update(extra)
    return out


def _dates(index, mask) -> List[str]:
    pos = np.flatnonzero(np.asarray(mask, dtype=bool))
    return [_fmt(index[i]) for i in pos[:MAX_EVENTS]]


# ════════════════════════════════════════════════════════════════════════════
# calculate_indicator
# ════════════════════════════════════════════════════════════════════════════

def _sample(index, arr, points: int = 200) -> List[Dict[str, Any]]:
    a = np.asarray(arr, dtype=float)
    stride = max(1, len(a) // points)
    idx = list(range(0, len(a), stride))
    if idx and idx[-1] != len(a) - 1:
        idx.append(len(a) - 1)
    return [{'date': _fmt(index[i]), 'value': (None if not np.isfinite(a[i]) else float(a[i]))} for i in idx]


def _stats(arr) -> Optional[Dict[str, float]]:
    v = np.asarray(arr, dtype=float)
    v = v[np.isfinite(v)]
    if not len(v):
        return None
    return {'mean': float(v.mean()), 'std': float(v.std()), 'min': float(v.min()), 'max': float(v.max()),
            'last': float(v[-1])}


def calculate_indicator(req: Dict[str, Any]) -> Dict[str, Any]:
    import vbt_engine as E
    iid = I.resolve_id(req.get('indicator', 'sma'))
    sym, frame, meta = load_one(req)
    ta = E.make_ta(frame)
    vals, used = I.compute(iid, ta, req.get('params') or {})
    sp = I.sweep_param(iid)
    out = _base(meta, frame, indicator=iid, params=used, period=used.get(sp) if sp else None)
    if isinstance(vals, dict):
        out['series'] = {k: _sample(frame.index, v) for k, v in vals.items()}
        out['stats'] = {k: s for k, s in ((k, _stats(v)) for k, v in vals.items()) if s is not None}
    else:
        out['series'] = _sample(frame.index, vals)
        st = _stats(vals)
        if st:
            out['stats'] = st
    warm = int(np.argmax(np.isfinite(I.primary_line(iid, vals)))) if np.isfinite(I.primary_line(iid, vals)).any() \
        else len(frame)
    out['warmupBars'] = warm
    return {'success': True, 'data': out}


# ════════════════════════════════════════════════════════════════════════════
# indicator_signals
# ════════════════════════════════════════════════════════════════════════════

def signal_metrics(frame, entries, exits, req: Dict[str, Any]) -> Dict[str, Any]:
    """Statistics of the signals as ONE position at a time on base.simulator (next-open fills; the
    request's commission / slippage when given, else none)."""
    import vbt_engine as E
    from base.costs import CostModel
    from base.simulator import Simulator, panel_from_arrays
    n = len(frame)
    out = {'totalSignals': int(np.sum(entries)), 'signalDensity': float(np.sum(entries) / n) if n else 0.0}
    cm = CostModel(commission_value=_num(req.get('commission'), 0.0) or 0.0,
                   slippage_value=_num(req.get('slippage'), 0.0) or 0.0)
    p = panel_from_arrays(frame['Close'].to_numpy(float), frame['Open'].to_numpy(float),
                          frame['High'].to_numpy(float), frame['Low'].to_numpy(float), dates=frame.index,
                          symbols=['X'])
    res = Simulator(p, costs=cm, backend='python').run(entries=np.asarray(entries, bool), exits=np.asarray(exits, bool))
    _, e_bar, x_bar, pnl, ret = E.round_trips(res.fill_arrays, 1)
    out['trades'] = int(len(ret))
    out['metricsNote'] = ('Trades of the signals with one position at a time, filled at the next bar\'s open'
                          + (', after the request\'s commission / slippage' if not cm.is_zero() else ', no costs')
                          + '; an open position at the end is not counted.')
    if len(ret):
        out.update({'avgReturn': float(ret.mean()), 'medianReturn': float(np.median(ret)),
                    'winRate': float(np.mean(ret > 0)), 'bestTrade': float(ret.max()), 'worstTrade': float(ret.min()),
                    'avgHoldingPeriod': float(np.mean(x_bar - e_bar)),
                    'profitFactor': (float(ret[ret > 0].sum() / -ret[ret < 0].sum()) if np.any(ret < 0) else None),
                    'totalReturn': float(res.equity[-1] / res.equity[0] - 1.0)})
    return out


def _period(params, key='period', default=14) -> int:
    v = _num(params.get(key), None)
    v = default if v is None else int(round(v))
    if v < 1:
        raise ResearchInputError(f'{key} must be at least 1')
    return v


def _ma(ta, kind: str, n: int):
    k = str(kind or 'ma').strip().lower()
    if k in ('ma', 'sma'):
        return ta.sma(n), 'sma'
    if k == 'ema':
        return ta.ema(n), 'ema'
    raise ResearchInputError(f'moving-average type must be ma, sma or ema, not {kind!r}')


def _threshold_line(iid: str, ta, period: int):
    if not I.supports_threshold(iid):
        raise ResearchInputError(f'{I.INDICATORS[iid]["name"]} is a {I.INDICATORS[iid]["scale"]}-scale indicator; '
                                 f'threshold signals need an oscillator ('
                                 f'{", ".join(k for k in I.INDICATORS if I.supports_threshold(k))}).')
    vals, used = I.compute(iid, ta, {'period': period})
    return I.primary_line(iid, vals), used


def indicator_signals(req: Dict[str, Any]) -> Dict[str, Any]:
    import vbt_engine as E
    mode = I.resolve_mode(req.get('mode'))
    params = dict(req.get('params') or {})
    sel = req.get('indicator')
    sel_id = I.resolve_id(sel) if sel not in (None, '') else None
    sym, frame, meta = load_one(req)
    ta = E.make_ta(frame)
    idx = frame.index
    c = ta.c
    out = _base(meta, frame, mode=mode, indicator=sel_id)
    extra_e: Dict[str, Any] = {}
    if mode == 'crossover_signals':
        fp = _period(params, 'fast_period', _num(params.get('fastPeriod'), 10) or 10)
        sp = _period(params, 'slow_period', _num(params.get('slowPeriod'), 20) or 20)
        default_type = sel_id if sel_id in ('sma', 'ema') else 'ma'
        fl, ft = _ma(ta, params.get('fast_indicator') or params.get('maType') or default_type, fp)
        sl, st = _ma(ta, params.get('slow_indicator') or params.get('maType') or default_type, sp)
        ent, ex = I.cross_above(fl, sl), I.cross_below(fl, sl)
        out.update({'lines': {'fast': f'{ft}({fp})', 'slow': f'{st}({sp})'}, 'indicatorUsed': sel_id in ('sma', 'ema'),
                    'note': 'Crossover of two moving averages of the close (MA type from the request); entries '
                            'and exits need both averages valid on the previous bar.'})

        def ctx(i):
            return {'fastMA': float(fl[i]), 'slowMA': float(sl[i])}
    elif mode == 'threshold_signals':
        iid = sel_id or 'rsi'
        per = _period(params, 'period', 14)
        line, used = _threshold_line(iid, ta, per)
        lo_d, hi_d = I.INDICATORS[iid]['levels']
        lo = _num(params.get('lower'), lo_d)
        hi = _num(params.get('upper'), hi_d)
        ent, ex = I.cross_below(line, lo), I.cross_above(line, hi)
        out.update({'indicator': iid, 'params': used, 'lower': lo, 'upper': hi, 'indicatorUsed': True,
                    'note': f'Entry when {I.INDICATORS[iid]["name"]} crosses below {lo:g}, exit when it crosses '
                            f'above {hi:g}.'})

        def ctx(i, _l=line, _lo=lo, _hi=hi):
            return {'indicatorValue': float(_l[i])}
        extra_e = {'thresholdEntry': lo, 'thresholdExit': hi}
    elif mode == 'breakout_signals':
        ch = str(params.get('channel') or (sel_id if sel_id in ('donchian', 'bbands', 'keltner') else 'donchian'))
        ch = I.resolve_id(ch)
        if not I.INDICATORS[ch].get('channel'):
            raise ResearchInputError(f'breakout channel must be donchian, bbands or keltner, not {ch!r}')
        per = _period(params, 'period', 20)
        vals, used = I.compute(ch, ta, {'period': per})
        up, lo_ = I.prev(vals['upper']), I.prev(vals['lower'])
        ent, ex = I.gt(c, up), I.lt(c, lo_)
        out.update({'channel': ch, 'params': used, 'indicatorUsed': sel_id == ch,
                    'note': f'Entry on a close above the previous bar\'s upper {ch} band, exit on a close below the '
                            f'previous bar\'s lower band.'})

        def ctx(i, _u=up, _l=lo_):
            return {'upper': float(_u[i]), 'lower': float(_l[i])}
    elif mode == 'mean_reversion_signals':
        per = _period(params, 'period', 20)
        z = ta.zscore(per)
        ze = _num(params.get('z_entry'), 2.0)
        zx = _num(params.get('z_exit'), 0.0)
        ent, ex = I.cross_below(z, -abs(ze)), I.cross_above(z, zx)
        out.update({'zEntry': ze, 'zExit': zx, 'period': per, 'indicatorUsed': sel_id == 'zscore',
                    'note': f'Z-score of the close over {per} bars: entry when it crosses below -{abs(ze):g}, exit '
                            f'when it crosses above {zx:g}.'})

        def ctx(i, _z=z):
            return {'indicatorValue': float(_z[i])}
    else:  # signal_filter
        base_id = I.resolve_id(params.get('base_indicator') or sel_id or 'rsi')
        per = _period(params, 'base_period', _num(params.get('period'), 14) or 14)
        line, used = _threshold_line(base_id, ta, per)
        lo_d, hi_d = I.INDICATORS[base_id]['levels']
        lo, hi = _num(params.get('lower'), lo_d), _num(params.get('upper'), hi_d)
        ent0, ex = I.cross_below(line, lo), I.cross_above(line, hi)
        fid = I.resolve_id(params.get('filter_indicator') or 'adx')
        if fid not in I.FILTER_INDICATORS:
            raise ResearchInputError(f'filter indicator must be one of {", ".join(I.FILTER_INDICATORS)}, not {fid!r}')
        fvals, fused = I.compute(fid, ta, {'period': _period(params, 'filter_period', 14)})
        fline = I.primary_line(fid, fvals)
        fth = _num(params.get('filter_threshold'), 25.0)
        ftype = str(params.get('filter_type') or 'above').lower()
        if ftype not in ('above', 'below'):
            raise ResearchInputError(f'filter_type must be above or below, not {ftype!r}')
        mask = I.gt(fline, fth) if ftype == 'above' else I.lt(fline, fth)
        ent = ent0 & mask
        out.update({'baseIndicator': base_id, 'baseParams': used, 'lower': lo, 'upper': hi, 'filterIndicator': fid,
                    'filterParams': fused, 'filterThreshold': fth, 'filterType': ftype,
                    'originalEntryCount': int(ent0.sum()), 'filteredEntryCount': int(ent.sum()), 'indicatorUsed': True,
                    'note': f'{I.INDICATORS[base_id]["name"]} threshold entries ({lo:g} / {hi:g}) kept only while '
                            f'{I.INDICATORS[fid]["name"]} is {ftype} {fth:g}.'})

        def ctx(i, _l=line, _f=fline):
            return {'indicatorValue': float(_l[i]), 'filterValue': float(_f[i])}
    ent = np.asarray(ent, bool)
    ex = np.asarray(ex, bool)
    out['entryCount'] = int(ent.sum())
    out['exitCount'] = int(ex.sum())
    out['entries'] = _dates(idx, ent)
    out['exits'] = _dates(idx, ex)

    def rows(mask, thr_key):
        r = []
        for i in np.flatnonzero(mask)[:MAX_EVENTS]:
            d = {'date': _fmt(idx[i]), 'price': float(c[i])}
            d.update({k: (v if np.isfinite(v) else None) for k, v in ctx(i).items()})
            if thr_key in extra_e:
                d['threshold'] = extra_e[thr_key]
            r.append(d)
        return r
    out['entrySignals'] = rows(ent, 'thresholdEntry')
    out['exitSignals'] = rows(ex, 'thresholdExit')
    out['eventsTruncated'] = bool(ent.sum() > MAX_EVENTS or ex.sum() > MAX_EVENTS)
    out.update(signal_metrics(frame, ent, ex, req))
    return {'success': True, 'data': out}


# ════════════════════════════════════════════════════════════════════════════
# indicator_param_sweep
# ════════════════════════════════════════════════════════════════════════════

def indicator_param_sweep(req: Dict[str, Any]) -> Dict[str, Any]:
    import vbt_engine as E
    from base import robustness as R
    from base.progress import CancelToken, Progress
    iid = I.resolve_id(req.get('indicator', 'rsi'))
    spec = I.INDICATORS[iid]
    sp = I.sweep_param(iid)
    if sp is None:
        raise ResearchInputError(f'{spec["name"]} has no parameter to sweep.')
    ranges = req.get('paramRanges')
    if not isinstance(ranges, dict) or not ranges:
        pr = req.get('paramRange')
        if not isinstance(pr, dict):
            raise ResearchInputError('Give paramRange {min, max, step} (or paramRanges per parameter).')
        ranges = {sp: pr}
    known = {p['id']: p for p in spec['params']}
    alias = {'period': sp}
    norm = {}
    for k, v in ranges.items():
        kk = k if k in known else alias.get(k, k)
        if kk not in known:
            raise ResearchInputError(f'{k} is not a parameter of {spec["name"]} ({", ".join(known)}).')
        norm[kk] = v
    names = list(norm)
    values = []
    for k in names:
        try:
            vals = R.param_values(norm[k])
        except ValueError as e:
            raise ResearchInputError(str(e))
        if known[k]['type'] == 'int':
            vals = sorted({int(round(float(x))) for x in vals})
        values.append(vals)
    total = 1
    for v in values:
        total *= len(v)
    if total > 5000:
        raise ResearchInputError(f'{total} combinations is too many for a sweep (at most 5,000).')
    sym, frame, meta = load_one(req)
    ta = E.make_ta(frame)
    import itertools
    cancel = CancelToken(req)
    prog = Progress('sweep', total=total, message=f'Sweeping {spec["name"]} over {total} combinations')
    results, errors = [], []
    for i, combo in enumerate(itertools.product(*values), 1):
        params = dict(zip(names, combo))
        try:
            vals, used = I.compute(iid, ta, params)
        except ValueError as e:
            errors.append(f'{params}: {e}')
            continue
        row: Dict[str, Any] = {'params': params}
        prim = I.primary_line(iid, vals)
        st = _stats(prim)
        if st:
            row.update(st)
        if isinstance(vals, dict):
            row['outputs'] = {k: s for k, s in ((k, _stats(v)) for k, v in vals.items()) if s is not None}
        row['validBars'] = int(np.isfinite(prim).sum())
        results.append(row)
        if i % 50 == 0:
            cancel.check()
        prog.update(i, f'{i} / {total}')
    prog.finish('Sweep done')
    if not results:
        raise ResearchInputError('Every sweep combination failed: ' + (errors[0] if errors else 'no values'))
    out = _base(meta, frame, indicator=iid, totalCombinations=len(results), requestedCombinations=total,
                paramGrid={n: v for n, v in zip(names, values)}, results=results[:2000],
                primaryOutput=spec.get('primary') or 'value')
    if errors:
        out['warnings'].append({'code': 'TRIALS_FAILED', 'severity': 'warn', 'symbol': None,
                                'message': f'{len(errors)} of {total} combinations failed (first: {errors[0][:160]}).'})
    return {'success': True, 'data': out}


# ════════════════════════════════════════════════════════════════════════════
# Signal generators, labels, splits
# ════════════════════════════════════════════════════════════════════════════

def generate_signals(req: Dict[str, Any]) -> Dict[str, Any]:
    import pandas as pd
    import vbt_signals as sig
    gen = str(req.get('generatorType') or 'RAND').upper()
    p = dict(req.get('params') or {})

    def g(*keys, default=None):
        for k in keys:
            if p.get(k) is not None:
                return p[k]
        return default
    seed = int(_num(g('seed'), 42))
    sym, frame, meta = load_one(req)
    close = frame['Close'].astype(float)
    n_ = int(_num(g('n'), 10))
    ep, xp = float(_num(g('entry_prob', 'entryProb'), 0.1)), float(_num(g('exit_prob', 'exitProb'), 0.1))
    gens = {
        'RAND': lambda: sig.RAND(close, n=n_, seed=seed),
        'RANDX': lambda: sig.RANDX(close, n=n_, seed=seed),
        'RANDNX': lambda: sig.RANDNX(close, n=n_, seed=seed, min_hold=int(_num(g('min_hold', 'minHold'), 1)),
                                     max_hold=int(_num(g('max_hold', 'maxHold'), 20))),
        'RPROB': lambda: sig.RPROB(close, entry_prob=ep, seed=seed),
        'RPROBX': lambda: sig.RPROBX(close, entry_prob=ep, exit_prob=xp, seed=seed),
        'RPROBCX': lambda: sig.RPROBCX(close, entry_prob=ep, exit_prob=xp, cooldown=int(_num(g('cooldown'), 5)),
                                       seed=seed),
        'RPROBNX': lambda: sig.RPROBNX(close, n=n_, entry_prob=ep, exit_prob=float(_num(g('exit_prob', 'exitProb'),
                                                                                        0.2)), seed=seed),
    }
    out = _base(meta, frame, generatorType=gen, seed=seed)
    entries = exits = None
    if gen in gens:
        o = gens[gen]()
        if isinstance(o, tuple):
            entries, exits = o
        else:
            entries = o
    elif gen in ('STX', 'STCX', 'OHLCSTX', 'OHLCSTCX'):
        entries = sig.RPROB(close, entry_prob=float(_num(g('entry_prob', 'entryProb'), 0.05)), seed=seed)
        sl, tp, ts = _num(g('stop_loss', 'stopLoss')), _num(g('take_profit', 'takeProfit')), \
            _num(g('trailing_stop', 'trailingStop'))
        if not (sl or tp or ts):
            raise ResearchInputError(f'{gen} needs stop_loss, take_profit or trailing_stop in params.')
        if gen == 'STX':
            exits = sig.STX(close, entries, stop_loss=sl, take_profit=tp)
        elif gen == 'STCX':
            exits = sig.STCX(close, entries, stop_loss=sl, take_profit=tp, trailing_stop=ts)
        elif gen == 'OHLCSTX':
            o = sig.OHLCSTX(frame['Open'], frame['High'], frame['Low'], close, entries, stop_loss=sl, take_profit=tp)
            exits = o[0] if isinstance(o, tuple) else o
        else:
            o = sig.OHLCSTCX(frame['Open'], frame['High'], frame['Low'], close, entries, stop_loss=sl,
                             take_profit=tp, trailing_stop=ts)
            exits = o[0] if isinstance(o, tuple) else o
        out['baseEntryCount'] = int(np.asarray(entries).sum())
    else:
        raise ResearchInputError(f'Unknown generator type {gen!r}; choose one of '
                                 f'{", ".join(list(gens) + ["STX", "STCX", "OHLCSTX", "OHLCSTCX"])}.')
    e = np.asarray(entries, dtype=bool) if entries is not None else np.zeros(len(frame), bool)
    x = np.asarray(exits, dtype=bool) if exits is not None else None
    out['entryCount'] = int(e.sum())
    out['entries'] = _dates(frame.index, e)
    if x is not None:
        out['exitCount'] = int(x.sum())
        out['exits'] = _dates(frame.index, x)
    if bool(p.get('clean') or req.get('clean')) and x is not None:
        ce, cx = sig.clean_signals(pd.Series(e, index=frame.index), pd.Series(x, index=frame.index))
        out.update({'cleanedEntryCount': int(np.asarray(ce).sum()), 'cleanedExitCount': int(np.asarray(cx).sum()),
                    'cleaned': True})
    return {'success': True, 'data': out}


_LABELS = ('FIXLB', 'MEANLB', 'LEXLB', 'TRENDLB', 'BOLB')


def _labels(label_type: str, close, p: Dict[str, Any]):
    """(labels as float with NaN where a label cannot be known, description)."""
    import vbt_labels as lb
    t = str(label_type or 'FIXLB').upper()
    n = len(close)
    if t == 'FIXLB':
        h = int(_num(p.get('horizon'), 5))
        lab = lb.FIXLB(close, horizon=h, threshold=float(_num(p.get('threshold'), 0.0))).astype(float)
        lab.iloc[max(0, n - h):] = np.nan
        return lab, f'sign of the {h}-bar forward return (last {h} bars unknown)'
    if t == 'MEANLB':
        w = int(_num(p.get('window'), 20))
        lab = lb.MEANLB(close, window=w, threshold=float(_num(p.get('threshold'), 1.0))).astype(float)
        lab.iloc[:w - 1] = np.nan
        return lab, f'z-score of the close vs its {w}-bar mean'
    if t == 'LEXLB':
        w = int(_num(p.get('window'), 5))
        lab = lb.LEXLB(close, window=w).astype(float)
        lab.iloc[:w] = np.nan
        lab.iloc[max(0, n - w):] = np.nan
        return lab, f'local extrema within +-{w} bars (edges unknown)'
    if t == 'TRENDLB':
        w = int(_num(p.get('window'), 20))
        lab = lb.TRENDLB(close, window=w, threshold=float(_num(p.get('threshold'), 0.0))).astype(float)
        lab.iloc[:w - 1] = np.nan
        return lab, f'sign of the {w}-bar regression slope'
    if t == 'BOLB':
        w = int(_num(p.get('window'), 20))
        lab = lb.BOLB(close, window=w, alpha=float(_num(p.get('alpha'), 2.0))).astype(float)
        lab.iloc[:w - 1] = np.nan
        return lab, f'position vs {w}-bar Bollinger bands'
    raise ResearchInputError(f'Unknown label type {label_type!r}; choose one of {", ".join(_LABELS)}.')


def generate_labels(req: Dict[str, Any]) -> Dict[str, Any]:
    sym, frame, meta = load_one(req)
    lab, desc = _labels(req.get('labelType'), frame['Close'].astype(float), dict(req.get('params') or {}))
    v = lab.to_numpy()
    ok = np.isfinite(v)
    u, cnt = np.unique(v[ok].astype(int), return_counts=True)
    out = _base(meta, frame, labelType=str(req.get('labelType') or 'FIXLB').upper(), labeledBars=int(ok.sum()),
                distribution={str(int(a)): int(b) for a, b in zip(u, cnt)}, description=desc,
                sampleLabels=[{'date': _fmt(lab.index[i]), 'label': int(v[i])} for i in np.flatnonzero(ok)[:50]],
                lastLabels=[{'date': _fmt(lab.index[i]), 'label': int(v[i])} for i in np.flatnonzero(ok)[-20:]])
    return {'success': True, 'data': out}


def labels_to_signals(req: Dict[str, Any]) -> Dict[str, Any]:
    sym, frame, meta = load_one(req)
    lab, desc = _labels(req.get('labelType'), frame['Close'].astype(float), dict(req.get('params') or {}))
    el = int(_num(req.get('entryLabel'), 1))
    xl = int(_num(req.get('exitLabel'), -1))
    v = lab.to_numpy()
    ent = np.isfinite(v) & (v == el)
    ex = np.isfinite(v) & (v == xl)
    out = _base(meta, frame, labelType=str(req.get('labelType') or 'FIXLB').upper(), entryLabel=el, exitLabel=xl,
                entryCount=int(ent.sum()), exitCount=int(ex.sum()), entries=_dates(frame.index, ent),
                exits=_dates(frame.index, ex), description=desc,
                note='Labels look into the future by construction (they are training targets): these signals are '
                     'NOT tradable as they stand.')
    return {'success': True, 'data': out}


def generate_splits(req: Dict[str, Any]) -> Dict[str, Any]:
    import pandas as pd
    import vbt_splitters as sp
    from base import robustness as R
    st = str(req.get('splitterType') or 'RollingSplitter')
    st = {'PurgedKFold': 'PurgedKFoldSplitter', 'Rolling': 'RollingSplitter', 'Expanding': 'ExpandingSplitter',
          'Range': 'RangeSplitter'}.get(st, st)
    p = dict(req.get('params') or {})

    def g(*keys, default=None):
        for k in keys:
            if p.get(k) not in (None, '', 0):
                return p[k]
        return default
    meta = None
    if req.get('symbols') or req.get('symbol'):
        sym, frame, meta = load_one(req)
        index = frame.index
    else:
        tb = int(_num(req.get('totalBars'), 0) or 0)
        start = str(req.get('startDate') or '2020-01-01')[:10]
        index = pd.bdate_range(start=start, periods=tb) if tb else pd.bdate_range(start=start, end=str(
            req.get('endDate') or '2024-01-01')[:10])
        frame = pd.DataFrame(index=index)
    n = len(index)
    splits = []

    def add(fold, tr_ranges, te):
        tr_bars = sum(b - a for a, b in tr_ranges)
        if tr_bars <= 0 or te[1] <= te[0]:
            return
        splits.append({'fold': fold, 'trainStart': _fmt(index[tr_ranges[0][0]]),
                       'trainEnd': _fmt(index[tr_ranges[-1][1] - 1]), 'trainSize': int(tr_bars),
                       'trainRanges': [[_fmt(index[a]), _fmt(index[b - 1])] for a, b in tr_ranges],
                       'testStart': _fmt(index[te[0]]), 'testEnd': _fmt(index[te[1] - 1]),
                       'testSize': int(te[1] - te[0])})
    if st == 'PurgedKFoldSplitter':
        try:
            folds = R.purged_kfold(n, int(_num(g('n_splits', 'nSplits'), 5)),
                                   int(_num(p.get('purge_len', p.get('purgeLen')), 5) or 0),
                                   int(_num(p.get('embargo_len', p.get('embargoLen')), 5) or 0))
        except ValueError as e:
            raise ResearchInputError(str(e))
        for f in folds:
            add(f['fold'] - 1, [tuple(r) for r in f['train']] or [(0, 0)], f['test'])
    elif st == 'RangeSplitter':
        rr = p.get('ranges') or []
        if not rr:
            raise ResearchInputError('RangeSplitter needs params.ranges [{trainStart, trainEnd, testStart, testEnd}].')
        sp_ = sp.RangeSplitter(ranges=[(r['trainStart'], r['trainEnd'], r['testStart'], r['testEnd']) for r in rr])
        for i, (trm, tem) in enumerate(sp_.split(index)):
            tr, te = np.flatnonzero(trm), np.flatnonzero(tem)
            add(i, [(int(tr[0]), int(tr[-1]) + 1)], (int(te[0]), int(te[-1]) + 1))
    elif st in ('RollingSplitter', 'ExpandingSplitter'):
        wl = int(_num(g('window_len', 'windowLen', 'windowLength'), 60))
        stp = int(_num(g('step', 'stepSize'), max(1, wl // 4)))
        tl = int(_num(g('test_len', 'testLen', 'testLength'), max(5, wl // 3)))
        if st == 'RollingSplitter':
            spl = sp.RollingSplitter(window_len=wl, test_len=tl, step=stp)
        else:
            spl = sp.ExpandingSplitter(min_len=int(_num(g('min_len', 'minLen'), wl)), test_len=tl, step=stp)
        for i, (trm, tem) in enumerate(spl.split(index)):
            tr, te = np.flatnonzero(trm), np.flatnonzero(tem)
            if len(tr) and len(te):
                add(i, [(int(tr[0]), int(tr[-1]) + 1)], (int(te[0]), int(te[-1]) + 1))
    else:
        raise ResearchInputError(f'Unknown splitter type {st!r}; choose RollingSplitter, ExpandingSplitter, '
                                 f'PurgedKFoldSplitter or RangeSplitter.')
    if meta is not None:
        out = _base(meta, frame)
    else:
        out = {'totalBars': n, 'usingSyntheticData': False, 'warnings': []}
    out.update({'splitterType': st, 'nSplits': len(splits), 'indexStart': _fmt(index[0]) if n else None,
                'indexEnd': _fmt(index[-1]) if n else None, 'splits': splits})
    if not splits:
        out['warnings'].append({'code': 'NO_SPLITS', 'severity': 'warn', 'symbol': None,
                                'message': f'{n} bars are too few for these splitter settings.'})
    return {'success': True, 'data': out}


# ════════════════════════════════════════════════════════════════════════════
# analyze_returns
# ════════════════════════════════════════════════════════════════════════════

def _rolling(r, window: int, metric: str, ppy: float, rf_bar: float, bench=None):
    import pandas as pd
    s = pd.Series(r)
    ex = s - rf_bar
    sq = math.sqrt(ppy)
    if metric == 'total':
        return (1 + s).rolling(window).apply(np.prod, raw=True) - 1
    if metric == 'annualized':
        tot = (1 + s).rolling(window).apply(np.prod, raw=True)
        return tot ** (ppy / window) - 1
    if metric == 'volatility':
        return s.rolling(window).std(ddof=1) * sq
    if metric == 'sharpe':
        sd = s.rolling(window).std(ddof=1)
        return ex.rolling(window).mean() / sd.where(sd > 1e-12) * sq
    if metric == 'sortino':
        dd = np.sqrt((np.minimum(ex, 0.0) ** 2).rolling(window).mean())
        return ex.rolling(window).mean() * sq / dd.where(dd > 1e-12)
    if metric == 'downside_risk':
        return np.sqrt((np.minimum(ex, 0.0) ** 2).rolling(window).mean()) * sq
    if metric == 'omega':
        g = np.maximum(ex, 0.0).rolling(window).sum()
        lo = np.maximum(-ex, 0.0).rolling(window).sum()
        return g / lo.where(lo > 1e-12)
    if metric == 'calmar':
        def calmar(w):
            eq = np.cumprod(1 + w)
            eq = np.concatenate([[1.0], eq])
            mdd = float(np.max(1 - eq / np.maximum.accumulate(eq)))
            ann = eq[-1] ** (ppy / len(w)) - 1
            return ann / mdd if mdd > 1e-12 else np.nan
        return s.rolling(window).apply(calmar, raw=True)
    if metric == 'info_ratio':
        if bench is None:
            raise ResearchInputError('rolling information ratio needs a benchmark symbol')
        act = s - pd.Series(bench)
        sd = act.rolling(window).std(ddof=1)
        return act.rolling(window).mean() * ppy / (sd.where(sd > 1e-12) * sq)
    raise ResearchInputError(f'Unknown rolling metric {metric!r}; choose total, annualized, volatility, sharpe, '
                             f'sortino, calmar, omega, info_ratio or downside_risk.')


def analyze_returns(req: Dict[str, Any]) -> Dict[str, Any]:
    import pandas as pd
    import vbt_generic as gen
    from base import metrics as M
    from base import robustness as R
    at = str(req.get('analysisType') or 'returns_stats')
    p = dict(req.get('params') or {})
    if 'rollingWindow' in req and 'window' not in p:
        p['window'] = req['rollingWindow']
    sym, frame, meta = load_one(req)
    close = frame['Close'].astype(float)
    idx = close.index
    ppy = M.infer_periods_per_year(idx)
    rf = _num(req.get('riskFreeRate'), None)
    if rf is None:
        rf = _num(p.get('risk_free'), 0.0) or 0.0
    rf_bar = rf / ppy
    r_full = M.simple_returns(close.to_numpy())
    r = np.where(np.isfinite(r_full), r_full, 0.0)
    bench_sym = str(req.get('benchmarkSymbol') or req.get('benchmark') or '').strip()
    b = None
    if bench_sym:
        try:
            breq = dict(req, symbols=[bench_sym])
            _, bf, _ = load_one(breq)
            bc = bf['Close'].astype(float).reindex(bf.index.union(idx)).ffill().reindex(idx)
            b = M.simple_returns(bc.to_numpy())
        except Exception as e:  # benchmark is optional
            meta['warnings'].append({'code': 'BENCHMARK_UNAVAILABLE', 'severity': 'info', 'symbol': bench_sym,
                                     'message': f'Benchmark {bench_sym} could not be loaded ({e}).'})
            b = None
    out = _base(meta, frame, analysisType=at, returnBars=int(len(r)), periodsPerYear=ppy, riskFreeRate=rf,
                benchmarkSymbol=bench_sym or None)
    if at == 'returns_stats':
        eq = close.to_numpy()
        why: Dict[str, str] = {}
        cg = M.cagr(eq, idx, why)
        mdd = M.max_drawdown(eq, why)
        sr = M.sharpe_ratio(r, rf_bar, ppy, why)
        rr = r - rf_bar
        n_trials = int(_num(p.get('n_trials'), 1) or 1)
        sd = float(np.std(rr, ddof=1)) if len(rr) > 2 else 0.0
        sr_bar = float(np.mean(rr) / sd) if sd > 0 else None
        skew = M.skewness(r)
        kex = M.excess_kurtosis(r)
        psr = dsr = None
        if sr_bar is not None and skew is not None and kex is not None:
            psr = R.probabilistic_sharpe(sr_bar, 0.0, len(rr), skew, kex + 3.0)
            sr0 = R.expected_max_sharpe(n_trials, 1.0 / max(1, len(rr) - 1)) if n_trials > 1 else 0.0
            dsr = R.probabilistic_sharpe(sr_bar, sr0, len(rr), skew, kex + 3.0)
        stats = {'Total Return': M.total_return(eq), 'Annualized Return': cg,
                 'Annualized Volatility': M.volatility(r, ppy), 'Sharpe Ratio': sr,
                 'Sortino Ratio': M.sortino_ratio(r, rf_bar, ppy), 'Calmar Ratio': M.calmar_ratio(cg, mdd),
                 'Omega Ratio': M.omega_ratio(r, rf_bar), 'Max Drawdown': (-mdd if mdd is not None else None),
                 'Downside Risk': M.downside_deviation(r, rf_bar, ppy), 'Skewness': skew,
                 'Kurtosis': (kex + 3.0) if kex is not None else None, 'Excess Kurtosis': kex,
                 'Probabilistic Sharpe': psr, 'Deflated Sharpe': dsr, 'Trials': n_trials,
                 'VaR 95%': M.value_at_risk(r, 0.95), 'CVaR 95%': M.conditional_var(r, 0.95),
                 'Risk-Free Rate': rf, 'Periods Per Year': ppy}
        if b is not None:
            rel = M.relative_metrics(r, np.where(np.isfinite(b), b, 0.0), rf_bar, ppy, cagr_value=cg)
            stats.update({'Information Ratio': rel.get('informationRatio'), 'Beta': rel.get('beta'),
                          'Alpha': rel.get('alpha'), 'Tracking Error': rel.get('trackingError'),
                          'Up Capture': rel.get('upCapture'), 'Down Capture': rel.get('downCapture'),
                          'Correlation': rel.get('correlation')})
            uc, dc = rel.get('upCapture'), rel.get('downCapture')
            stats['Up/Down Ratio'] = (uc / dc) if (uc is not None and dc not in (None, 0)) else None
        out['stats'] = stats
        out['definitions'] = {'Annualized Return': 'calendar CAGR', 'Sharpe Ratio': 'mean excess bar return / std '
                              'x sqrt(bars per year) (base.metrics)', 'Max Drawdown': 'negative fraction (v1 sign)',
                              'Kurtosis': 'raw (normal = 3); Excess Kurtosis = Kurtosis - 3',
                              'Deflated Sharpe': 'probability the true Sharpe exceeds the best of `Trials` zero-skill '
                                                 'trials (Bailey & Lopez de Prado); with 1 trial = Probabilistic Sharpe'}
        if why:
            out['undefinedMetrics'] = why
        cum = np.cumprod(1.0 + r) - 1.0
        step = max(1, len(cum) // 100)
        pos = list(range(0, len(cum), step))
        if pos[-1] != len(cum) - 1:
            pos.append(len(cum) - 1)
        out['cumulativeReturns'] = [{'date': _fmt(idx[i + 1]), 'value': float(cum[i])} for i in pos]
    elif at == 'drawdowns':
        eq = pd.Series(close.to_numpy(), index=idx)
        dd = gen.Drawdowns.from_ts(eq)
        out['stats'] = {k: float(v) if isinstance(v, (int, float, np.floating, np.integer)) else str(v)
                        for k, v in dd.stats().items()}
        out['totalDrawdowns'] = dd.count
        out['maxDrawdown'] = float(dd.max_drawdown())
        out['avgDrawdown'] = float(dd.avg_drawdown())
        out['activeDrawdown'] = float(dd.active_drawdown())
        out['activeDuration'] = int(dd.active_duration())
        out['records'] = [{k: v for k, v in d.items() if k not in ('peakIdx', 'troughIdx', 'recoveryIdx')}
                          for d in M.drawdown_table(close.to_numpy(), idx, top=50)]
        dser = dd.drawdown()
        step = max(1, len(dser) // 100)
        out['drawdownSeries'] = [{'date': _fmt(dser.index[i]), 'value': float(dser.iloc[i])}
                                 for i in range(0, len(dser), step)]
    elif at == 'ranges':
        th = float(_num(p.get('threshold'), 0.0))
        rs = pd.Series(r, index=idx[1:])
        rng = gen.Ranges.from_ts(rs, threshold=th)
        out['stats'] = {k: float(v) if isinstance(v, (int, float, np.floating, np.integer)) else str(v)
                        for k, v in rng.stats().items()}
        out['totalRanges'] = rng.count
        out['avgDuration'] = float(rng.avg_duration())
        out['maxDuration'] = int(rng.max_duration())
        out['coverage'] = float(rng.coverage())
        out['note'] = f'Runs of consecutive bars with a return above {th:g}.'
        out['records'] = [{'Start Date': _fmt(rs.index[x.start_idx]), 'End Date': _fmt(rs.index[x.end_idx]),
                           'Duration': int(x.duration),
                           'Return': float(np.prod(1 + r[x.start_idx:x.end_idx + 1]) - 1)} for x in rng.records[:50]]
    elif at == 'rolling':
        window = int(_num(p.get('window'), 252) or 252)
        if window < 2 or window >= len(r):
            raise ResearchInputError(f'rolling window must be between 2 and {len(r) - 1} bars, not {window}')
        metric = str(p.get('metric') or 'sharpe').lower()
        ser = _rolling(r, window, metric, ppy, rf_bar, None if b is None else np.where(np.isfinite(b), b, 0.0))
        v = np.asarray(ser, dtype=float)
        ok = np.flatnonzero(np.isfinite(v))
        step = max(1, len(ok) // 200)
        sel = list(ok[::step])
        if len(ok) and sel[-1] != ok[-1]:
            sel.append(ok[-1])
        out.update({'metric': metric, 'window': window, 'dataPoints': int(len(ok)),
                    'series': [{'date': _fmt(idx[i + 1]), 'value': float(v[i])} for i in sel]})
    else:
        raise ResearchInputError(f'Unknown analysis type {at!r}; choose returns_stats, drawdowns, ranges or rolling.')
    return {'success': True, 'data': out}
