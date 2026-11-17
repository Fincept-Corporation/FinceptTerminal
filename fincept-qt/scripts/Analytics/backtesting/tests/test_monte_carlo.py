"""base.monte_carlo: methods, seeds, invested fraction from the result (B4), shuffle semantics (B5),
block bootstrap fidelity, ruin, open positions, errors, speed (A0.6)."""
import time

import numpy as np
import pandas as pd
import pytest

from base import monte_carlo as MC
from base import signals as S
from base.costs import CostModel
from base.simulator import Simulator, panel_from_arrays


def _result(spy, size=1.0):
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    p = panel_from_arrays(spy['Close'], spy['Open'], spy['High'], spy['Low'], dates=spy.index, symbols=['SPY'])
    res = Simulator(p, costs=CostModel(commission_value=0.001), size=size).run(
        entries=S.crossover(f, s), exits=S.crossunder(f, s))
    equity = [{'date': d.strftime('%Y-%m-%d'), 'equity': float(v)} for d, v in zip(spy.index, res.equity)]
    return {'equity': equity, 'trades': res.trades(), 'statistics': {'initialCapital': 100000.0},
            'performance': {'totalReturn': res.equity[-1] / 1e5 - 1}}, res


def test_seeded_and_reproducible(spy):
    data, _ = _result(spy)
    a = MC.monte_carlo_from_result(data, {'mcMethod': 'block_bootstrap', 'mcSimulations': 500, 'mcSeed': 3})
    b = MC.monte_carlo_from_result(data, {'mcMethod': 'block_bootstrap', 'mcSimulations': 500, 'mcSeed': 3})
    c = MC.monte_carlo_from_result(data, {'mcMethod': 'block_bootstrap', 'mcSimulations': 500, 'mcSeed': 4})
    assert a['finalEquity'] == b['finalEquity'] and a['finalEquity'] != c['finalEquity']
    assert a['seed'] == 3 and a['methodId'] == 'block_bootstrap' and a['blockSize'] == 20


def test_trade_paths_use_invested_fraction_from_result(spy):
    """B4: half-sized trades must not be compounded as if they used the whole account."""
    data, res = _result(spy, size=0.5)
    mc = MC.monte_carlo_from_result(data, {'mcMethod': 'bootstrap', 'mcSimulations': 300, 'positionSizeValue': 100})
    assert mc['positionFraction'] == pytest.approx(0.5, abs=0.02)
    # the open position is included at its mark, so the original path reconciles with the run
    assert mc['stats']['originalFinal'] == pytest.approx(res.equity[-1], rel=0.005)
    assert mc['method'] == 'bootstrap' and mc['methodId'] == 'trade_bootstrap'
    assert not any(w['code'] == 'MC_NOT_RECONCILED' for w in mc['warnings'])


def test_shuffle_reports_no_final_percentiles(spy):
    data, _ = _result(spy)
    mc = MC.monte_carlo_from_result(data, {'mcMethod': 'reshuffle', 'mcSimulations': 300})
    st = mc['stats']
    assert st['finalP5'] is None and st['finalP50'] is None and st['finalMean'] is None
    assert np.allclose(mc['finalEquity'], mc['finalEquity'][0], rtol=1e-9)
    assert st['ddP95'] >= st['ddP50'] > 0


def test_block_bootstrap_keeps_volatility(spy):
    data, res = _result(spy)
    realised = np.std(np.diff(res.equity) / res.equity[:-1], ddof=1)
    eq = np.array([p['equity'] for p in data['equity']])
    r = eq[1:] / eq[:-1] - 1
    rng = np.random.default_rng(0)
    idx = MC.stationary_bootstrap_indices(rng, 200, len(r), len(r), 20)
    sims = np.std(r[idx], axis=1, ddof=1)
    assert abs(np.median(sims) / realised - 1) < 0.10
    mc = MC.monte_carlo_from_result(data, {'mcMethod': 'block_bootstrap', 'mcSimulations': 300})
    assert 0.0 <= mc['stats']['probRuin'] <= 1.0 and mc['stats']['ruinLevel'] == 0.5
    assert mc['xDates'][0] == data['equity'][0]['date'] and mc['horizonBars'] == len(r)
    assert len(mc['percentileBands']['p50']) == len(mc['x'])


def test_errors_are_plain():
    with pytest.raises(ValueError, match='Unknown Monte Carlo method'):
        MC.normalize_method('tea_leaves')
    out = MC.run_monte_carlo(lambda req: {'success': True, 'data': {}}, {'mcMethod': 'tea_leaves'})
    assert out['success'] is False and out['errorCode'] == 'INPUT'
    data = {'equity': [{'date': f'2024-01-{d:02d}', 'equity': 100.0 + d} for d in range(1, 10)], 'trades': []}
    out = MC.run_monte_carlo(lambda req: {'success': True, 'data': data}, {'mcMethod': 'bootstrap'})
    assert out['success'] is False and 'block_bootstrap' in out['error']


def test_engine_identity_carried_from_backtest(spy):
    """Decision 7: the Monte Carlo result names the engine / implementation of the run it resampled."""
    data, _ = _result(spy)
    mc = MC.monte_carlo_from_result(data, {'mcMethod': 'block_bootstrap', 'mcSimulations': 200})
    assert mc['engine'] is None and mc['engineImplementation'] is None
    data['engine'], data['engineImplementation'] = 'zipline', 'Fincept event simulator (base.simulator)'
    mc = MC.monte_carlo_from_result(data, {'mcMethod': 'block_bootstrap', 'mcSimulations': 200})
    assert mc['engine'] == 'zipline' and mc['engineImplementation'].startswith('Fincept event')
    data2 = {k: v for k, v in data.items() if k not in ('engine', 'engineImplementation')}
    data2['provenance'] = {'engine': 'bt', 'engineImplementation': 'bt library'}
    mc = MC.monte_carlo_from_result(data2, {'mcMethod': 'block_bootstrap', 'mcSimulations': 200})
    assert mc['engine'] == 'bt' and mc['engineImplementation'] == 'bt library'


def test_speed_10k_paths_by_2515_bars(spy):
    r = np.diff(np.log(spy['Close'].values[:2516]))
    t = time.perf_counter()
    sim = MC.simulate_paths('block_bootstrap', r, n_sims=10000, seed=1, initial=1e5, horizon=2515)
    dt = time.perf_counter() - t
    assert len(sim['finals']) == 10000
    assert dt < 6.0, f'{dt:.2f} s'
