"""Write one contract-v2 sample payload per command to tests/data/ (the UI coders' fixtures).

    FINCEPT_SAMPLES_DATA_DIR=<scratch dir> python make_samples.py   (offline: fixtures only; a throwaway
    price store in that dir - a temp dir when unset - is warmed from them)

sample_run_backtest.json        SMA 20/50 on SPY 2023-2024 through base.simulator + build_backtest_result
                                (per-symbol provenance from the price store, costs.sensitivity 0x..3x)
sample_run_backtest_multi.json  monthly equal weight on SPY QQQ TLT GLD EFA (weights mode: weights{} block,
                                attribution, positions, rebalance trades)
sample_optimize.json            25-combination grid with overfitting statistics and a hold-out (base.robustness)
sample_walk_forward.json        3 rolling windows with warm-up (base.robustness)
sample_monte_carlo.json         block bootstrap of the backtest (base.monte_carlo; engine identity carried)
Each file is the full stdout envelope {"success": true, "data": {...}} as json_response writes it.
"""
import os
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))
os.environ['FINCEPT_DATA_DIR'] = os.environ.get('FINCEPT_SAMPLES_DATA_DIR') or tempfile.mkdtemp(prefix='fincept_samples_')
os.environ['FINCEPT_BT_PROGRESS'] = '0'

import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402

from base import robustness as R  # noqa: E402
from base import signals as S  # noqa: E402
from base.base_provider import json_response  # noqa: E402
from base.contract import validate_envelope  # noqa: E402
from base.costs import CostModel, cost_sensitivity  # noqa: E402
from base.execution import align_panel  # noqa: E402
from base.monte_carlo import monte_carlo_from_result  # noqa: E402
from base.price_store import PriceStore, get_bars  # noqa: E402
from base.result_normalizer import EngineIdentity, build_backtest_result  # noqa: E402
from base.simulator import Simulator, panel_from_arrays  # noqa: E402
from helpers import FakeYahoo, sma_context, sma_evaluate, yahoo_layout  # noqa: E402


def main():
    fx = HERE / 'fixtures'
    spy_all = pd.read_csv(fx / 'SPY_1d.csv', index_col=0, parse_dates=[0])
    irx = pd.read_csv(fx / 'IRX_1d.csv', index_col=0, parse_dates=[0])['Close']
    irx_df = pd.DataFrame({'Open': irx, 'High': irx, 'Low': irx, 'Close': irx, 'Volume': 0.0})
    etf = pd.read_csv(fx / 'ETF5_1d.csv', index_col=0, parse_dates=[0])
    etf_syms = ['SPY', 'QQQ', 'TLT', 'GLD', 'EFA']
    etf_frames = {}
    for sym in etf_syms[1:]:
        o, c = etf[f'{sym}_Open'], etf[f'{sym}_Close']
        etf_frames[sym] = pd.DataFrame({'Open': o, 'High': pd.concat([o, c], axis=1).max(axis=1),
                                        'Low': pd.concat([o, c], axis=1).min(axis=1), 'Close': c, 'Volume': 1e6})
    fake = {'SPY': yahoo_layout(spy_all), '^IRX': yahoo_layout(irx_df, tz='America/Chicago')}
    fake.update({sym: yahoo_layout(fr.dropna()) for sym, fr in etf_frames.items()})
    PriceStore(fetcher=FakeYahoo(fake), offline=False).get(list(fake), '2015-01-01', '2024-12-31', progress=False)
    os.environ['FINCEPT_PRICE_CACHE_OFFLINE'] = '1'
    req = {'symbols': ['SPY'], 'startDate': '2023-01-01', 'endDate': '2024-12-31', 'interval': '1d',
           'initialCapital': 100000, 'commission': 0.001, 'slippage': 0.0005, 'benchmarkSymbol': 'SPY',
           'riskFreeRate': 0.0375, 'riskFreeSource': 'auto', 'fillTiming': 'next_open',
           'strategy': {'type': 'sma_crossover', 'name': 'SMA Crossover', 'category': 'Trend',
                        'params': {'fastPeriod': 20, 'slowPeriod': 50}}}
    bars = get_bars(['SPY'], req['startDate'], req['endDate'], progress=False)   # served by the warm store
    spy = bars.frames['SPY']
    ident = EngineIdentity('vectorbt', 'Fincept reference simulator (base.simulator)', None, version='sample',
                           fill_mode='next_open', slippage_itemised=True)
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    p = panel_from_arrays(spy['Close'], spy['Open'], spy['High'], spy['Low'], spy['Volume'], dates=spy.index,
                          symbols=['SPY'])
    cm = CostModel.from_request(req)
    ent, exi = S.crossover(f, s), S.crossunder(f, s)
    res = Simulator(p, costs=cm).run(entries=ent, exits=exi)
    ex = res.exposure_series()
    costs = res.costs_summary()
    costs['sensitivity'] = cost_sensitivity(lambda m: Simulator(p, costs=m).run(entries=ent, exits=exi).equity, cm,
                                            spy.index, rf=req['riskFreeRate'], base_equity=res.equity)
    bt = build_backtest_result(req, dates=spy.index, equity=res.equity, identity=ident, trades=res.trades(),
                               fills=res.fills(), exposure=ex['gross'], net_exposure=ex['net'], cash=res.cash,
                               costs=costs, cost_model=cm, frames={'SPY': spy}, price_provenance=bars.provenance,
                               warmup_bars=50)
    # multi-asset: monthly equal weight, decided on the first bar of each month, filled at the next open
    mreq = dict(req, symbols=etf_syms, slippage=0.0, strategy={'type': 'equal_weight', 'name': 'Equal Weight',
                                                               'category': 'Allocation', 'params': {}})
    mbars = get_bars(etf_syms, mreq['startDate'], mreq['endDate'], progress=False)
    panel = align_panel({sym: mbars.frames[sym] for sym in etf_syms})
    months = panel.dates.to_period('M')
    W = np.full(panel.close.shape, np.nan)
    W[np.r_[True, months[1:] != months[:-1]]] = 1.0 / len(etf_syms)
    mcm = CostModel.from_request(mreq)
    mres = Simulator(panel, costs=mcm).run(weights=W)
    mex = mres.exposure_series()
    mid = EngineIdentity('vectorbt', 'Fincept reference simulator (base.simulator)', None, version='sample',
                         fill_mode='next_open', slippage_itemised=True, trade_mode='rebalance')
    mbt = build_backtest_result(mreq, dates=panel.dates, equity=mres.equity, identity=mid, trades=mres.trades(),
                                fills=mres.fills(), exposure=mex['gross'], net_exposure=mex['net'], cash=mres.cash,
                                costs=mres.costs_summary(), cost_model=mcm, frames=mbars.frames,
                                price_provenance=mbars.provenance, extra={'weights': mres.weights_block()})
    ctx = sma_context()
    combos, info = R.sample_combinations({'fastPeriod': {'min': 10, 'max': 30, 'step': 5},
                                          'slowPeriod': {'min': 50, 'max': 150, 'step': 25}}, 'grid', 500)
    opt = R.run_optimization(sma_evaluate, combos, context=ctx, objective='sharpe', ppy=252, dates=ctx['dates'],
                             info=info, workers=1, progress=False, holdout_fraction=0.3)
    ws = R.walk_forward_windows(len(ctx['close']), 3, 0.7, warmup_bars=150)
    wf = R.run_walk_forward(sma_evaluate, combos[:9], ws, context=ctx, objective='sharpe', ppy=252,
                            dates=ctx['dates'], workers=1, progress=False)
    mc = monte_carlo_from_result(bt, {'mcMethod': 'block_bootstrap', 'mcSimulations': 500, 'mcSeed': 42})
    out = HERE / 'data'
    out.mkdir(exist_ok=True)
    for name, cmd, data in (('run_backtest', 'run_backtest', bt), ('run_backtest_multi', 'run_backtest', mbt),
                            ('optimize', 'optimize', opt), ('walk_forward', 'walk_forward', wf),
                            ('monte_carlo', 'monte_carlo', mc)):
        text = json_response({'success': True, 'data': data})
        import json
        errs = validate_envelope(json.loads(text), cmd)
        assert not errs, (cmd, errs[:5])
        (out / f'sample_{name}.json').write_text(text + '\n', encoding='utf-8')
        print(f'sample_{name}.json  {len(text) / 1024:.0f} KB')


if __name__ == '__main__':
    main()
