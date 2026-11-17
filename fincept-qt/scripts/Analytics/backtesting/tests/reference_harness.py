"""Reference-comparison harness for the per-engine coders.

Runs an engine's provider (exactly like the terminal) on the four canonical strategies, with and
without costs, and compares its equity curve and trades with the independent pandas reference
(tests/reference.py) computed on the same bars (base.price_store, split+dividend adjusted).

    python reference_harness.py <engine> [--strategies bh,sma,rsi,ew] [--fill next_open]
        [--shares fractional|whole] [--sma-rule cross|state] [--sma-gate on|off]
        [--rsi wilder|simple] [--rsi-rule cross|level] [--ew-when first_bar_of_month|last_bar_of_month]
        [--start 2015-01-01] [--end 2024-12-31] [--symbol SPY] [--ew-symbols SPY,QQQ,TLT,GLD,EFA]
        [--ids bh=buy_and_hold,sma=sma_crossover,rsi=rsi,ew=equal_weight] [--close-at-end]
        [--data-dir DIR] [--json out.json]

Pick the conventions your engine CLAIMS (its assumptions.fillModel, whole vs fractional shares, its
crossover rule) - every remaining difference must then be explained or fixed. Tolerances (audit Part 2.1):
  same fill model, fractional shares : final equity within 0.05 %, every bar within 0.1 %, same trade dates
  whole shares                       : final equity within 0.5 %
  monthly equal weight (portfolio)   : 1e-4 zero cost, 1e-3 with costs (fractional); 0.5 % whole shares
  costs                              : total commission within 1 % of the reference
Exit code 0 when every case passes. Library use: compare_case(engine_data, case) -> dict.

Costs: 'zero' and 'costs' = 10 bp commission + 5 bp slippage per fill (the audit convention).
"""
import argparse
import json
import math
import sys
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import reference as ref  # noqa: E402
from run_provider import run  # noqa: E402

COSTS = {'zero': (0.0, 0.0), 'costs': (0.001, 0.0005)}
DEFAULT_IDS = {'bh': 'buy_and_hold', 'sma': 'sma_crossover', 'rsi': 'rsi', 'ew': 'equal_weight'}
DEFAULT_PARAMS = {'bh': {}, 'sma': {'fastPeriod': 20, 'slowPeriod': 50},
                  'rsi': {'period': 14, 'oversold': 30, 'overbought': 70}, 'ew': {'rebalancePeriod': 'monthly'}}


def payload(symbols, start, end, strategy_id, params, comm, slip, fill):
    return {'symbols': symbols, 'startDate': start, 'endDate': end, 'interval': '1d', 'allowSyntheticData': False,
            'initialCapital': 100000, 'commission': comm, 'slippage': slip, 'leverage': 1,
            'positionSizing': 'percent', 'positionSizeValue': 100, 'allowShort': False, 'benchmarkSymbol': 'SPY',
            'riskFreeRate': 0.0, 'fillTiming': fill,
            'strategy': {'type': strategy_id, 'name': strategy_id, 'category': '', 'params': params}}


def reference_case(kind, bars, conv, comm, slip):
    """Reference equity (and trades) for one case on `bars` ({symbol: OHLCV DataFrame})."""
    fill, frac = conv['fill'], conv['shares'] == 'fractional'
    if kind == 'ew':
        closes = pd.DataFrame({s: f['Close'] for s, f in bars.items()}).dropna()
        opens = pd.DataFrame({s: f['Open'] for s, f in bars.items()}).reindex(closes.index)
        r = ref.ew_monthly(closes, opens, commission=comm, when=conv['ew_when'], fill=fill)
        return {'equity': r['equity'], 'trades': None, 'fees': r['fees']}
    df = next(iter(bars.values()))
    kw = dict(fill=fill, commission=comm, slippage=slip, fractional=frac, close_at_end=conv['close_at_end'])
    if kind == 'bh':
        r = ref.buy_hold(df, **kw)
    elif kind == 'sma':
        if conv['sma_rule'] == 'state':
            f, s = ref.sma(df['Close'], 20), ref.sma(df['Close'], 50)
            above = (f > s) & s.notna()
            ent = above & ~above.shift(1, fill_value=False)
            exi = ~above & above.shift(1, fill_value=False) & s.notna()
            r = ref.simulate(df, ent, exi, **kw)
        else:
            r = ref.sma_cross(df, 20, 50, gate=conv['sma_gate'], **kw)
    else:
        r = ref.rsi_mr(df, 14, 30, 70, kind=conv['rsi'], cross=conv['rsi_rule'] == 'cross', **kw)
    fees = sum(t.get('fees', 0.0) for t in r['trades'])
    return {'equity': r['equity'], 'trades': r['trades'], 'fees': fees}


def compare_case(data, refc, kind, conv, cost_name):
    eng = ref.engine_equity(data)
    j = eng.index.intersection(refc['equity'].index)
    rel = (eng.reindex(j) / refc['equity'].reindex(j) - 1).abs()
    final_rel = abs(eng.iloc[-1] / refc['equity'].reindex(eng.index).dropna().iloc[-1] - 1) if len(j) else math.inf
    whole = conv['shares'] == 'whole'
    if kind == 'ew':
        tol_final = 0.005 if whole else (1e-4 if cost_name == 'zero' else 1e-3)
        tol_bar = tol_final * 2
    else:
        tol_final = 0.005 if whole else 0.0005
        tol_bar = 0.01 if whole else 0.001
    out = {'kind': kind, 'costs': cost_name, 'bars_compared': int(len(j)), 'final_engine': float(eng.iloc[-1]),
           'final_reference': float(refc['equity'].iloc[-1]), 'final_rel_diff': float(final_rel),
           'max_bar_rel_diff': float(rel.max()) if len(rel) else None, 'tolerance_final': tol_final}
    ok = len(j) > 0 and final_rel <= tol_final and (rel.max() <= tol_bar)
    if refc['trades'] is not None:
        tc = ref.compare_trades(data, refc['trades'])
        out['trades'] = tc
        ok = ok and tc['only_engine'] == [] and tc['only_ref'] == []
    if cost_name == 'costs':
        fees = (data.get('costs') or {}).get('totalCommission')
        if fees is None:
            fees = (data.get('statistics') or {}).get('totalFees')
        out['fees_engine'], out['fees_reference'] = fees, refc['fees']
        if fees is not None and refc['fees']:
            ok = ok and abs(fees / refc['fees'] - 1) <= 0.01
    out['pass'] = bool(ok)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('engine')
    ap.add_argument('--strategies', default='bh,sma,rsi,ew')
    ap.add_argument('--fill', default='next_open', choices=['next_open', 'next_close', 'same_close'])
    ap.add_argument('--shares', default='fractional', choices=['fractional', 'whole'])
    ap.add_argument('--sma-rule', default='cross', choices=['cross', 'state'])
    ap.add_argument('--sma-gate', default='on', choices=['on', 'off'])
    ap.add_argument('--rsi', default='wilder', choices=['wilder', 'simple'])
    ap.add_argument('--rsi-rule', default='cross', choices=['cross', 'level'])
    ap.add_argument('--ew-when', default='first_bar_of_month', choices=['first_bar_of_month', 'last_bar_of_month'])
    ap.add_argument('--start', default='2015-01-01')
    ap.add_argument('--end', default='2024-12-31')
    ap.add_argument('--symbol', default='SPY')
    ap.add_argument('--ew-symbols', default='SPY,QQQ,TLT,GLD,EFA')
    ap.add_argument('--ids', default='')
    ap.add_argument('--close-at-end', action='store_true', help='engine liquidates open positions on the last bar')
    ap.add_argument('--data-dir', default=None)
    ap.add_argument('--json', default=None)
    a = ap.parse_args(argv)
    ids = dict(DEFAULT_IDS)
    for part in filter(None, a.ids.split(',')):
        k, v = part.split('=')
        ids[k] = v
    conv = {'fill': a.fill, 'shares': a.shares, 'sma_rule': a.sma_rule, 'sma_gate': a.sma_gate == 'on',
            'rsi': a.rsi, 'rsi_rule': a.rsi_rule, 'ew_when': a.ew_when, 'close_at_end': a.close_at_end}
    if a.data_dir:
        import os
        os.environ['FINCEPT_DATA_DIR'] = a.data_dir
    from base.price_store import get_bars
    results = []
    for kind in a.strategies.split(','):
        syms = a.ew_symbols.split(',') if kind == 'ew' else [a.symbol]
        bars_res = get_bars(syms, a.start, a.end, progress=False)
        bars_res.raise_first_error()
        for cost_name, (comm, slip) in COSTS.items():
            if kind == 'ew':
                slip = 0.0
            pl = payload(syms, a.start, a.end, ids[kind], DEFAULT_PARAMS[kind], comm, slip, a.fill)
            r = run(a.engine, 'run_backtest', pl, data_dir=a.data_dir)
            if not r.data:
                results.append({'kind': kind, 'costs': cost_name, 'pass': False,
                                'error': (r.envelope or {}).get('error') or r.problems})
                continue
            # Simulate the reference over the engine's own first..last bar (an engine whose data ends early
            # - e.g. an exclusive end date - is reported in `dateRange`, not mixed into the equity check).
            eq_dates = pd.to_datetime([str(p['date'])[:10] for p in r.data.get('equity') or []])
            frames = {s: f.loc[eq_dates.min():eq_dates.max()] for s, f in bars_res.frames.items()} if len(eq_dates)                 else bars_res.frames
            refc = reference_case(kind, frames, conv, comm, slip)
            c = compare_case(r.data, refc, kind, conv, cost_name)
            first = min(f.index[0] for f in bars_res.frames.values())
            last = max(f.index[-1] for f in bars_res.frames.values())
            c['dateRange'] = {'engine': [str(eq_dates.min().date()), str(eq_dates.max().date())] if len(eq_dates) else None,
                              'data': [str(first.date()), str(last.date())]}
            c['hygiene'] = r.problems
            c['pass'] = c['pass'] and not r.problems
            results.append(c)
    for c in results:
        flag = 'PASS' if c['pass'] else 'FAIL'
        print(f"{flag} {c['kind']:4s} {c['costs']:5s} " + ' '.join(f'{k}={v}' for k, v in c.items()
                                                                   if k not in ('kind', 'costs', 'pass')))
    if a.json:
        Path(a.json).write_text(json.dumps(results, indent=1, default=str), encoding='utf-8')
    return 0 if all(c['pass'] for c in results) else 1


if __name__ == '__main__':
    sys.exit(main())
