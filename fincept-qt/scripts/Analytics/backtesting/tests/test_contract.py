"""Contract v2 (base.contract) is the gate for every engine batch: results built through base/
validate, the legacy normaliser path validates, violations are caught, saved engine outputs
(FINCEPT_BT_ENGINE_OUTPUTS=<dir of '<engine>_<kind>.json' provider outputs>) validate too."""
import json
import os
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

from base import robustness as R
from base import signals as S
from base.contract import validate, validate_envelope
from base.costs import CostModel
from base.monte_carlo import monte_carlo_from_result
from base.price_store import PriceStore
from base.result_normalizer import EngineIdentity, build_backtest_result, normalize_response
from base.simulator import Simulator, panel_from_arrays
from helpers import FakeYahoo, sma_context, sma_evaluate, yahoo_layout

HERE = Path(__file__).resolve().parent
IDENT = EngineIdentity('vectorbt', 'Fincept reference simulator (base.simulator)', None, version='test',
                       fill_mode='next_open', slippage_itemised=True)


@pytest.fixture(scope='module')
def warm_store(spy, irx):
    """Put SPY and ^IRX in the session's (offline) price store, as a warm cache would have them."""
    irx_df = pd.DataFrame({'Open': irx, 'High': irx, 'Low': irx, 'Close': irx, 'Volume': 0.0})
    fake = FakeYahoo({'SPY': yahoo_layout(spy), '^IRX': yahoo_layout(irx_df, tz='America/Chicago')})
    st = PriceStore(fetcher=fake, offline=False)
    st.get(['SPY', '^IRX'], '2015-01-01', '2024-12-31', progress=False)
    return st


def _run(spy, req):
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    p = panel_from_arrays(spy['Close'], spy['Open'], spy['High'], spy['Low'], spy['Volume'], dates=spy.index,
                          symbols=['SPY'])
    cm = CostModel.from_request(req)
    res = Simulator(p, costs=cm).run(entries=S.crossover(f, s), exits=S.crossunder(f, s))
    ex = res.exposure_series()
    return build_backtest_result(req, dates=spy.index, equity=res.equity, identity=IDENT, trades=res.trades(),
                                 fills=res.fills(), exposure=ex['gross'], net_exposure=ex['net'], cash=res.cash,
                                 costs=res.costs_summary(), cost_model=cm, frames={'SPY': spy}, warmup_bars=50)


REQ = {'symbols': ['SPY'], 'startDate': '2015-01-01', 'endDate': '2024-12-31', 'initialCapital': 100000,
       'commission': 0.001, 'slippage': 0.0005, 'benchmarkSymbol': 'SPY', 'riskFreeRate': 0.0,
       'strategy': {'type': 'sma_crossover', 'name': 'SMA', 'params': {'fastPeriod': 20, 'slowPeriod': 50}}}


def test_built_result_validates(spy, warm_store):
    data = _run(spy, REQ)
    assert validate('run_backtest', data) == []
    assert data['schemaVersion'] == 2 and data['assumptions']['riskFreeSource'] == '^IRX'
    assert data['costs']['totalSlippage'] > 0 and data['costs']['itemised'] == {'commission': True, 'slippage': True,
                                                                                 'borrow': True}
    assert data['trades'][-1]['exitReason'] == 'open'
    pnl = sum(t['pnl'] for t in data['trades'])
    assert pnl == pytest.approx(data['equity'][-1]['equity'] - 100000, abs=0.01)
    assert data['benchmark']['sharpeRatio'] is not None and data['provenance']['engine'] == 'vectorbt'
    assert data['equity'][0]['exposure'] == 0.0 and 'cash' in data['equity'][0]
    codes = {w['code'] for w in data['warnings']}
    assert 'OPEN_POSITION_AT_END' in codes and 'SLIPPAGE_NOT_ITEMISED' not in codes


def test_negative_equity_is_flagged_and_drawdown_capped(spy, warm_store):
    """An account that loses more than its capital (fincept IndexOptionIronCondorAlgorithm ended at -776) is a true
    result: the contract's maxDrawdown / drawdowns[].depth stay fractions (1.0 = everything lost), the raw depth is
    kept, and a NEGATIVE_EQUITY error-level warning says how far below zero the equity went."""
    idx = spy.index[:300]
    eq = np.linspace(100000.0, 20000.0, 300)
    eq[250:] = np.linspace(20000.0, -776.0, 50)
    req = dict(REQ, riskFreeSource='rate', benchmarkSymbol='')
    data = build_backtest_result(req, dates=idx, equity=eq, identity=IDENT, trades=[], frames={'SPY': spy.iloc[:300]})
    assert validate('run_backtest', data) == []
    assert data['performance']['maxDrawdown'] == 1.0 and data['engineReported']['maxDrawdownRaw'] > 1.0
    assert data['drawdowns'][0]['depth'] == 1.0 and data['drawdowns'][0]['depthRaw'] > 1.0
    w = [x for x in data['warnings'] if x['code'] == 'NEGATIVE_EQUITY']
    assert w and w[0]['severity'] == 'error' and '-776.00' in w[0]['message']
    ok = build_backtest_result(req, dates=idx, equity=np.linspace(100000.0, 90000.0, 300), identity=IDENT, trades=[],
                               frames={'SPY': spy.iloc[:300]})
    assert not any(x['code'] == 'NEGATIVE_EQUITY' for x in ok['warnings'])


def test_risk_free_source_series_is_auto(spy, warm_store):
    """The rebuilt screen sends riskFreeSource "series" for "the T-bill series": it must mean auto (^IRX for
    USD), not a ticker called SERIES."""
    data = _run(spy, dict(REQ, riskFreeRate=0.04, riskFreeSource='series'))
    auto = _run(spy, dict(REQ, riskFreeRate=0.04, riskFreeSource='auto'))
    assert data['assumptions']['riskFreeSource'] == '^IRX' == auto['assumptions']['riskFreeSource']
    assert data['performance']['sharpeRatio'] == auto['performance']['sharpeRatio']
    assert not any(w.get('symbol') == 'SERIES' for w in data['warnings'])


def test_explicit_rate_is_used_when_asked(spy, warm_store):
    data = _run(spy, dict(REQ, riskFreeRate=0.04, riskFreeSource='rate'))
    assert data['assumptions']['riskFreeSource'] == 'request' and data['assumptions']['riskFreeRate'] == 0.04
    eq = pd.Series([p['equity'] for p in data['equity']], index=spy.index)
    import reference as ref
    assert data['performance']['sharpeRatio'] == pytest.approx(ref.metrics(eq, rf=0.04)['sharpe'], rel=1e-6)


def test_legacy_engine_dict_normalises_and_validates(spy, warm_store):
    """The path every unchanged engine takes: a v1, partly snake_case dict through normalize_response."""
    idx = spy.index[:300]
    eq = 100000 * spy['Close'].values[:300] / spy['Close'].values[0]
    legacy = {'success': True, 'data': {
        'id': 'sma_crossover', 'status': 'completed',
        'performance': {'total_return': float(eq[-1] / eq[0] - 1), 'sharpe_ratio': 9.9},
        'statistics': {'start_date': '2015-01-01', 'end_date': '2016-03-31', 'total_fees': 0.0},
        'equity': [{'date': d.strftime('%Y-%m-%d 00:00:00'), 'equity': float(v)} for d, v in zip(idx, eq)],
        'trades': [{'id': 'x1', 'symbol': 'SPY', 'side': 'long', 'entry_date': idx[0].strftime('%Y-%m-%d'),
                    'entry_price': float(spy['Open'].iloc[0]), 'quantity': 100.0, 'commission': 1.0,
                    'exit_reason': 'open_at_end', 'exit_date': idx[-1].strftime('%Y-%m-%d'),
                    'exit_price': float(spy['Close'].iloc[299]), 'pnl': 123.0}],
        'assumptions': {'engine': 'x', 'execution': 'next open', 'ignored_settings': ['leverage']},
        'logs': 'one line'}}
    req = dict(REQ, endDate='2016-03-31', strategy={'type': 'sma_crossover', 'params': {'fast_period': 20}})
    out = normalize_response('run_backtest', req, legacy)
    data = out['data']
    assert validate('run_backtest', data) == []
    assert data['engineReported']['performance']['sharpeRatio'] == 9.9          # engine figure kept, not shown
    assert data['performance']['sharpeRatio'] != 9.9
    assert data['statistics']['startDate'] == idx[0].strftime('%Y-%m-%d')      # first bar, not the request
    assert data['trades'][0]['exitReason'] == 'open' and data['trades'][0]['exitDate'] is None
    assert data['provenance']['parameters'] == {'fast_period': 20}             # data-map keys untouched (B7)
    assert any(w['code'] == 'IGNORED_SETTING' for w in data['warnings'])


def test_optimize_walk_forward_monte_carlo_validate(spy, warm_store):
    ctx = sma_context()
    combos, info = R.sample_combinations({'fastPeriod': {'min': 10, 'max': 30, 'step': 5},
                                          'slowPeriod': {'min': 50, 'max': 150, 'step': 25}}, 'grid', 100)
    opt = R.run_optimization(sma_evaluate, combos, context=ctx, objective='sharpe', ppy=252, dates=ctx['dates'],
                             info=info, workers=1, progress=False)
    assert validate('optimize', opt) == []
    assert opt['overfitting']['pbo'] is not None and 0 <= opt['overfitting']['pbo'] <= 1
    ws = R.walk_forward_windows(len(ctx['close']), 3, 0.7, warmup_bars=150)
    wf = R.run_walk_forward(sma_evaluate, combos[:6], ws, context=ctx, objective='sharpe', ppy=252,
                            dates=ctx['dates'], workers=1, progress=False)
    assert validate('walk_forward', wf) == []
    data = _run(spy, REQ)
    for m in ('bootstrap', 'reshuffle', 'returns_bootstrap', 'block_bootstrap'):
        mc = monte_carlo_from_result(data, {'mcMethod': m, 'mcSimulations': 200})
        assert validate('monte_carlo', mc) == [], m


def test_validator_catches_violations(spy, warm_store):
    data = json.loads(json.dumps(_run(spy, REQ)))
    bad = json.loads(json.dumps(data))
    bad['performance']['sharpeRatio'] = float('nan')
    bad['performance']['max_drawdown'] = 0.1
    bad['equity'][5]['drawdown'] = 0.2
    bad['trades'].append(dict(bad['trades'][0]))
    bad['assumptions']['notModelled'].append('vibes')
    errs = validate('run_backtest', bad)
    joined = '\n'.join(errs)
    for needle in ('non-finite', 'camelCase', 'drawdown', 'closed trade listed after an open position', 'vibes'):
        assert needle in joined, needle
    assert validate_envelope({'success': False, 'error': 'x'}, 'run_backtest')


@pytest.mark.skipif(not os.environ.get('FINCEPT_BT_ENGINE_OUTPUTS'), reason='no saved engine outputs given')
def test_saved_engine_outputs_validate():
    kinds = {'single': 'run_backtest', 'single_rf': 'run_backtest', 'ew': 'run_backtest', 'multi3': 'run_backtest',
             'opt6': 'optimize', 'opt500': 'optimize', 'wf3': 'walk_forward', 'mc': 'monte_carlo'}
    d = Path(os.environ['FINCEPT_BT_ENGINE_OUTPUTS'])
    checked = 0
    for p in sorted(d.glob('*.json')):
        kind = p.stem.split('_', 1)[1] if '_' in p.stem else ''
        if kind not in kinds:
            continue
        lines = [ln for ln in p.read_text(encoding='utf-8').splitlines() if ln.strip()]
        assert len(lines) == 1, p.name
        errs = validate_envelope(json.loads(lines[0]), kinds[kind])
        assert errs == [], (p.name, errs[:5])
        checked += 1
    assert checked > 0


def test_sample_payloads_validate():
    d = HERE / 'data'
    files = sorted(d.glob('sample_*.json')) if d.exists() else []
    if not files:
        pytest.skip('no sample payloads (run tests/make_samples.py)')
    cmds = ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo')
    for p in files:
        name = p.stem.replace('sample_', '')
        cmd = next((c for c in cmds if name == c or name.startswith(c + '_')), name)   # sample_run_backtest_multi
        assert validate_envelope(json.loads(p.read_text(encoding='utf-8')), cmd) == [], p.name
    names = {p.stem for p in files}
    assert {'sample_run_backtest', 'sample_run_backtest_multi', 'sample_optimize', 'sample_walk_forward',
            'sample_monte_carlo'} <= names


def test_benchmark_uses_the_same_risk_free_as_strategy(spy, irx, warm_store):
    """B3: strategy and benchmark Sharpe share the function AND the risk-free series."""
    from base import metrics as M
    data = _run(spy, REQ)
    assert data['assumptions']['riskFreeSource'] == '^IRX'
    y = (irx / 100.0).reindex(spy.index).ffill().bfill().values
    want = M.sharpe_ratio(M.simple_returns(spy['Close'].values), M.rf_per_bar(len(spy) - 1, y, 252, spy.index), 252)
    assert data['benchmark']['sharpeRatio'] == pytest.approx(want, rel=1e-9)
