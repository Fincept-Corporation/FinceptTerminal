"""The "Backtesting.py" engine rebased on base/ (audit Part 4, acceptance tests T-P1..T-P10), offline on the
CSV fixtures through a FakeYahoo-warmed price store (no network). Every run is a real backtesting.py run."""
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

HERE = Path(__file__).resolve().parent
BTP_DIR = HERE.parent / 'backtestingpy'
if str(BTP_DIR) not in sys.path:
    sys.path.insert(0, str(BTP_DIR))

import reference as ref  # noqa: E402
from base.base_provider import json_response  # noqa: E402
from base.contract import validate_envelope  # noqa: E402
from base.price_store import PriceStore  # noqa: E402
from helpers import FakeYahoo, yahoo_layout  # noqa: E402

import backtestingpy_provider as BP  # noqa: E402
import btp_engine as E  # noqa: E402
import btp_evaluate as BE  # noqa: E402
import btp_executor as X  # noqa: E402
import btp_strategies as ST  # noqa: E402

CUT = '2022-06-30'
WHOLE = {'fill': 'next_open', 'shares': 'whole', 'sma_rule': 'cross', 'sma_gate': True, 'rsi': 'simple',
         'rsi_rule': 'level', 'ew_when': 'first_bar_of_month', 'close_at_end': False}


@pytest.fixture(scope='module')
def frames(spy, irx):
    f = {'SPY': spy.astype(float)}
    rng = np.random.default_rng(5)
    p = spy.astype(float).copy()
    after = p.index > pd.Timestamp(CUT)
    k = np.cumprod(1.0 + rng.normal(0, 0.03, after.sum()))
    for col in ('Open', 'High', 'Low', 'Close'):
        p.loc[after, col] = p.loc[after, col].values * k
    p.loc[after, 'High'] = p.loc[after, ['Open', 'High', 'Low', 'Close']].max(axis=1)
    p.loc[after, 'Low'] = p.loc[after, ['Open', 'High', 'Low', 'Close']].min(axis=1)
    f['SPYP'] = p
    f['LATE'] = spy.astype(float).loc['2020-12-10':]
    f['^IRX'] = pd.DataFrame({'Open': irx, 'High': irx, 'Low': irx, 'Close': irx, 'Volume': 0.0})
    return f


@pytest.fixture(scope='module', autouse=True)
def warm(frames):
    fake = FakeYahoo({s: yahoo_layout(f, tz='America/Chicago' if s == '^IRX' else 'America/New_York')
                      for s, f in frames.items()})
    st = PriceStore(fetcher=fake, offline=False)
    res = st.get(list(frames), '2014-01-01', '2024-12-31', progress=False)
    assert not res.errors
    os.environ['FINCEPT_BT_PROGRESS'] = '0'
    return st


def call(cmd, r):
    env = json.loads(json_response(BP.dispatch(cmd, r)))
    if cmd in ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo') and env.get('success'):
        assert validate_envelope(env, cmd) == [], validate_envelope(env, cmd)[:5]
    return env


def req(strategy='sma_crossover', params=None, symbols=('SPY',), start='2015-01-01', end='2024-12-31', comm=0.001,
        slip=0.0005, **kw):
    r = {'symbols': list(symbols), 'startDate': start, 'endDate': end, 'interval': '1d', 'initialCapital': 100000,
         'commission': comm, 'slippage': slip, 'leverage': 1, 'positionSizing': 'percent', 'positionSizeValue': 100,
         'allowShort': False, 'benchmarkSymbol': 'SPY', 'riskFreeRate': 0.0,
         'strategy': {'type': strategy, 'params': dict({'fastPeriod': 20, 'slowPeriod': 50}
                                                       if params is None and strategy == 'sma_crossover'
                                                       else (params or {}))}}
    r.update(kw)
    return r


def engine_run(frame, sid, params=None, sym='SPY', **kw):
    """One backtesting.py run through the engine functions (no normaliser): (spec, RunOut, trades)."""
    spec = E.parse_request(req(sid, params, symbols=(sym,), **kw))
    f = E.clean_frame(frame, sym, [])
    ta = E.make_ta(f)
    sig = ST.build_signals(spec.strategy, ta, spec.params)
    run = E.run_once(spec, E.make_backtest(spec, f), sig, E.make_plan(spec, ta, E.ppy_of(f)))
    return spec, run, E.trades_from(run, f, sym), f


# ── T-P1: the canonical cases against the independent reference ─────────────

@pytest.mark.parametrize('kind', ['bh', 'sma', 'rsi'])
@pytest.mark.parametrize('cost', ['zero', 'costs'])
def test_tp1_reference_cases(spy, kind, cost):
    import reference_harness as H
    comm, slip = H.COSTS[cost]
    sid = {'bh': 'buy_and_hold', 'sma': 'sma_crossover', 'rsi': 'rsi'}[kind]
    env = call('run_backtest', req(sid, H.DEFAULT_PARAMS[kind], comm=comm, slip=slip))
    out = H.compare_case(env['data'], H.reference_case(kind, {'SPY': spy}, WHOLE, comm, slip), kind, WHOLE, cost)
    assert out['pass'], out
    assert out['final_rel_diff'] < 1e-9 and out['max_bar_rel_diff'] < 1e-8
    d = env['data']
    assert d['engine'] == 'backtestingpy' and d['engineImplementation'].startswith('backtesting.py library 0.6')
    assert d['provenance']['library'] == 'backtesting' and d['provenance']['libraryVersion'].startswith('0.6')
    codes = {w['code'] for w in d['warnings']}
    assert not codes & {'TRADE_LIST_INCOMPLETE', 'SLIPPAGE_NOT_ITEMISED'}
    assert d['engineReported']['library']['equityFinal'] == pytest.approx(d['statistics']['finalCapital'], rel=1e-9)


@pytest.mark.parametrize('fill', ['next_close', 'same_close'])
def test_tp1_fill_timings_match_the_reference(spy, fill):
    import reference_harness as H
    conv = dict(WHOLE, fill=fill)
    for kind, sid in (('sma', 'sma_crossover'), ('bh', 'buy_and_hold')):
        env = call('run_backtest', req(sid, H.DEFAULT_PARAMS[kind], fillTiming=fill))
        out = H.compare_case(env['data'], H.reference_case(kind, {'SPY': spy}, conv, 0.001, 0.0005), kind, conv,
                             'costs')
        assert out['pass'] and out['max_bar_rel_diff'] < 1e-8, out
        assert env['data']['assumptions']['fillModel']['fillAt'] == fill


def test_the_library_executes_the_run():
    import backtesting.backtesting as BB
    assert issubclass(X.FinceptBroker, BB._Broker) and issubclass(X.SignalExecutor, BB.Strategy)
    assert issubclass(X.FinceptBacktest, BB.Backtest)
    spec, run, trades, _ = engine_run(pd.read_csv(HERE / 'fixtures' / 'SPY_1d.csv', index_col=0, parse_dates=[0]),
                                      'sma_crossover')
    assert isinstance(run.raw.broker, X.FinceptBroker) and isinstance(run.raw.strategy, X.SignalExecutor)
    assert len(run.raw.broker.closed_trades) == sum(1 for t in trades if t['exitReason'] != 'open')
    assert sys.modules['backtesting._plotting'].__dict__.get('__fincept_stub__')   # bokeh never loaded


# ── T-P2 / T-P4: open position, reconciliation, itemised costs ───────────────

def test_tp2_tp4_open_position_and_itemised_costs():
    d = call('run_backtest', req())['data']
    tr = d['trades']
    assert tr[-1]['exitReason'] == 'open' and tr[-1]['exitDate'] is None and tr[-1]['markPrice'] > 0
    assert sum(1 for t in tr if t['exitReason'] == 'open') == 1 and d['positions'][0]['quantity'] == tr[-1]['quantity']
    change = d['statistics']['finalCapital'] - d['statistics']['initialCapital']
    assert sum(t['pnl'] for t in tr) == pytest.approx(change, abs=0.01)
    c = d['costs']
    assert c['totalSlippage'] > 0 and c['totalBorrow'] == 0.0 and c['grossPnl'] is not None
    assert d['statistics']['totalFees'] == pytest.approx(c['totalCommission']) and \
        d['statistics']['totalSlippage'] == pytest.approx(c['totalSlippage'])
    assert all(t['commission'] > 0 and t['slippageCost'] > 0 for t in tr)
    assert sum(t['commission'] + t['slippageCost'] for t in tr) == pytest.approx(c['totalCosts'], rel=1e-9)
    f0 = d['fills'][0]
    assert f0['side'] == 'buy' and f0['slippageCost'] == pytest.approx(f0['quantity'] * f0['price'] * 0.0005 / 1.0005)
    assert f0['commission'] == pytest.approx(f0['quantity'] * f0['price'] * 0.001)
    assert {w['code'] for w in d['warnings']} >= {'OPEN_POSITION_AT_END'}
    assert not {w['code'] for w in d['warnings']} & {'TRADE_LIST_INCOMPLETE', 'SLIPPAGE_NOT_ITEMISED'}
    # cost sensitivity: 0x / 1x / 2x / 3x the cost model, each a backtesting.py run; 1x = this run
    sens = c['sensitivity']
    assert [r['multiple'] for r in sens] == [0, 1, 2, 3]
    assert sens[1]['totalReturn'] == pytest.approx(d['performance']['totalReturn'], rel=1e-12)
    assert sens[0]['totalReturn'] > sens[1]['totalReturn'] > sens[2]['totalReturn'] > sens[3]['totalReturn']
    # per-symbol provenance from the price store
    ps = d['provenance']['symbols'][0]
    assert ps['symbol'] == 'SPY' and ps['bars'] == len(d['equity']) and ps['firstBar'] == '2015-01-02'


# ── T-P3: Sharpe / Sortino / volatility from base.metrics ────────────────────

def test_tp3_metrics_are_the_standard_definitions():
    for rf in (0.0, 0.04):
        d = call('run_backtest', req(riskFreeRate=rf, riskFreeSource='rate'))['data']
        eq = pd.Series([p['equity'] for p in d['equity']], index=pd.to_datetime([p['date'] for p in d['equity']]))
        m = ref.metrics(eq, rf=rf)
        assert d['performance']['sharpeRatio'] == pytest.approx(m['sharpe'], rel=1e-6)
        assert d['performance']['sortinoRatio'] == pytest.approx(m['sortino'], rel=1e-6)
        assert d['performance']['volatility'] == pytest.approx(m['volatility'], rel=1e-6)
        assert d['engineReported']['library']['sharpeRatio'] != pytest.approx(m['sharpe'], rel=1e-3)


# ── T-P5 / T-P6: optimiser ranges, independent levels, settings, seed ────────

def test_tp5_float_ranges_independent_levels_rows_and_seed():
    d = call('optimize', req('bollinger_bands', {'period': 20},
                             paramRanges={'stdDev': {'min': 1.5, 'max': 2.5, 'step': 0.5}}))['data']
    assert sorted(r['parameters']['stdDev'] for r in d['allResults']) == [1.5, 2.0, 2.5]
    assert all(set(r['performance']) >= {'totalReturn', 'sharpeRatio', 'maxDrawdown', 'totalTrades'}
               for r in d['allResults'])
    d = call('optimize', req('cci', {'period': 20}, paramRanges={'lower': {'min': -150, 'max': -50, 'step': 50},
                                                                 'upper': {'min': 50, 'max': 150, 'step': 50}}))['data']
    assert len({(r['parameters']['lower'], r['parameters']['upper']) for r in d['allResults']}) == 9
    assert len({round(r['performance']['totalReturn'], 10) for r in d['allResults']}) == 9
    rr = {'fastPeriod': {'min': 5, 'max': 40, 'step': 1}, 'slowPeriod': {'min': 50, 'max': 150, 'step': 5}}
    a = call('optimize', req(paramRanges=rr, optimizeMethod='random', maxIterations=12, seed=11))['data']
    b = call('optimize', req(paramRanges=rr, optimizeMethod='random', maxIterations=12, seed=11))['data']
    assert a['method'] == 'random' and a['iterations'] == 12 and a['seed'] == 11
    assert [r['parameters'] for r in a['allResults']] == [r['parameters'] for r in b['allResults']]
    bad = call('optimize', req(paramRanges={'nope': {'min': 1, 'max': 2, 'step': 1}}))
    assert bad['success'] is False and bad['errorCode'] == 'INPUT'


@pytest.mark.parametrize('kw', [{'leverage': 2}, {'allowShort': True}, {'stopLoss': 0.05, 'takeProfit': 0.1},
                                {'fillTiming': 'next_close'}, {'positionSizing': 'vol_target', 'positionSizeValue': 10}])
def test_tp6_optimise_runs_the_same_backtest(kw):
    r = req(end='2019-12-31', **kw)
    single = call('run_backtest', r)['data']
    o = call('optimize', dict(r, paramRanges={'fastPeriod': {'min': 20, 'max': 20, 'step': 1},
                                              'slowPeriod': {'min': 50, 'max': 50, 'step': 1}}))['data']
    # base.robustness keeps per-bar returns as float32: equal within that precision
    assert o['bestPerformance']['totalReturn'] == pytest.approx(single['performance']['totalReturn'], rel=1e-6)
    assert 'backtesting.py library' in o['engineImplementation']


def test_evaluate_window_masks_warmup_and_matches_full_run(spy):
    spec = E.parse_request(req())
    f = E.clean_frame(spy.astype(float), 'SPY', [])
    ctx = BE.make_context(spec, f)
    full = BE.evaluate({'fastPeriod': 20, 'slowPeriod': 50}, ctx, None)
    _, run, _, _ = engine_run(spy, 'sma_crossover')
    assert np.allclose(full['returns'], X.returns_of(run.equity), rtol=0, atol=1e-15)
    w = BE.evaluate({'fastPeriod': 20, 'slowPeriod': 50}, ctx, {'dataStart': 900, 'scoreStart': 1000, 'end': 1500})
    assert len(w['returns']) == 500 and w['returns'][0] == 0.0
    bh = BE.evaluate({}, BE.make_context(E.parse_request(req('buy_and_hold')), f),
                     {'dataStart': 900, 'scoreStart': 1000, 'end': 1500})
    assert bh['returns'][0] == 0.0 and np.count_nonzero(bh['returns']) > 400     # enters at the first scored bar


def test_pool_workers_give_the_same_table(spy):
    from base import robustness as R
    spec = E.parse_request(req())
    f = E.clean_frame(spy.astype(float), 'SPY', [])
    combos = [{'fastPeriod': a, 'slowPeriod': b} for a in (10, 20) for b in (50, 100)]
    one = R.run_optimization(BE.evaluate, combos, context=BE.make_context(spec, f), workers=1, dates=f.index,
                             progress=False)
    two = R.run_optimization(BE.evaluate, combos, context=BE.make_context(spec, f), workers=2, dates=f.index,
                             progress=False)
    assert two['workers'] == 2
    assert [(r['parameters'], r['objectiveValue']) for r in one['allResults']] == \
        [(r['parameters'], r['objectiveValue']) for r in two['allResults']]


# ── T-P7: walk-forward through base.robustness ───────────────────────────────

def test_tp7_walk_forward_reoptimises_and_tiles():
    d = call('walk_forward', req(paramRanges={'fastPeriod': {'min': 10, 'max': 30, 'step': 10},
                                              'slowPeriod': {'min': 50, 'max': 100, 'step': 50}},
                                 wfSplits=3, wfTrainRatio=0.7))['data']
    w = d['windows']
    assert len(w) == 3 and all(x['bestParams'] and x['inSampleSharpe'] is not None for x in w)
    assert all(w[i + 1]['testStartIdx'] == w[i]['testEndIdx'] + 1 for i in range(2))
    assert len(d['oosEquity']) == sum(x['testBars'] for x in w) and d['warmupBars'] == 100
    assert d['degradationRatio'] is not None and d['walkForwardEfficiency'] is not None
    assert not {x['code'] for x in d['warnings']} & {'WALK_FORWARD_NOT_OPTIMISED'}


# ── T-P8: stops through the library's contingent orders ──────────────────────

def test_tp8_stop_loss_take_profit_intrabar(spy):
    spec, run, trades, f = engine_run(spy, 'rsi', {}, stopLoss=0.05, takeProfit=0.10)
    o = f['Open'].to_numpy()
    fa = run.fills
    by_exit = {(t['exitBar'], round(t['quantity'], 6)): t for t in trades if t['exitBar'] is not None}
    seen = {'stop_loss': 0, 'take_profit': 0}
    for k in range(len(fa['bar'])):
        reason = int(fa['reason'][k])
        if reason not in (X.REASON_SL, X.REASON_TP):
            continue
        b, q = int(fa['bar'][k]), abs(float(fa['qty'][k]))
        t = by_exit[(b, round(q, 6))]
        entry = t['entryPrice']
        refp = float(fa['ref_price'][k])
        if reason == X.REASON_SL:
            lvl = entry * 0.95
            assert refp == pytest.approx(lvl, rel=1e-12) or (refp == pytest.approx(o[b]) and o[b] < lvl)
            assert refp / entry - 1 >= -0.05 - 1e-12 or o[b] < lvl          # no worse than the stop unless a gap
            assert fa['price'][k] == pytest.approx(refp * (1 - 0.0005))      # stops pay slippage
            seen['stop_loss'] += 1
        else:
            lvl = entry * 1.10
            assert refp == pytest.approx(lvl, rel=1e-12) or (refp == pytest.approx(o[b]) and o[b] > lvl)
            assert fa['price'][k] == refp                                    # targets are limit fills
            seen['take_profit'] += 1
    assert seen['stop_loss'] > 3 and seen['take_profit'] > 0
    assert sum(t['pnl'] for t in trades) == pytest.approx(run.equity[-1] - run.equity[0], abs=1e-6)
    _, run2, trades2, _ = engine_run(spy, 'sma_crossover', trailingStop=0.05)
    assert any(t['exitReason'] == 'trailing_stop' for t in trades2)
    assert sum(t['pnl'] for t in trades2) == pytest.approx(run2.equity[-1] - run2.equity[0], abs=1e-6)


# ── T-P9: zero trades, short history, dates ──────────────────────────────────

def test_tp9_zero_trades_short_history_and_dates():
    d = call('run_backtest', req(start='2024-10-01', end='2024-11-27'))['data']
    codes = {w['code'] for w in d['warnings']}
    assert {'ZERO_TRADES', 'SHORT_HISTORY'} <= codes
    p = d['performance']
    assert p['winRate'] is None and p['profitFactor'] is None and p['sharpeRatio'] is None and p['lossRate'] is None
    assert d['statistics']['startDate'] == '2024-10-01' and d['statistics']['endDate'] == '2024-11-27'
    d = call('run_backtest', req(symbols=('LATE',)))['data']
    assert d['statistics']['startDate'] == '2020-12-10' and d['statistics']['endDate'] == '2024-12-31'
    assert 'START_AFTER_REQUEST' in {w['code'] for w in d['warnings']}


# ── look-ahead, catalogue, strategies ────────────────────────────────────────

def test_no_lookahead_and_every_strategy_runs_and_differs(frames):
    cut = int(np.searchsorted(frames['SPY'].index, pd.Timestamp(CUT), side='right')) - 1
    finals = {}
    for c in ST.CATALOG:
        params = {p['id']: p['default'] for p in c['parameters']}
        _, a, ta, _ = engine_run(frames['SPY'], c['type'], params, allowShort=True)
        _, b, tb, _ = engine_run(frames['SPYP'], c['type'], params, sym='SPYP', allowShort=True)
        assert np.array_equal(a.equity[:cut + 1], b.equity[:cut + 1]), c['type']
        fa = a.fills['bar'] <= cut
        fb = b.fills['bar'] <= cut
        assert np.array_equal(a.fills['qty'][fa], b.fills['qty'][fb]) and \
            np.array_equal(a.fills['price'][fa], b.fills['price'][fb]), c['type']
        assert sum(t['pnl'] for t in ta) == pytest.approx(a.equity[-1] - a.equity[0], abs=1e-6), c['type']
        finals[c['type']] = round(float(a.equity[-1]), 6)
    assert len(set(finals.values())) == len(finals), finals             # no strategy returns another's result
    assert len(ST.CATALOG) == 27 and 'buy_and_hold' in finals


def test_catalogue_specs_complete():
    env = call('get_strategies', {})
    cat = env['data']
    assert isinstance(cat, list) and len(cat) == 27
    assert env['total'] == 27 and env['supported'] == 27 and env['runnableHere'] == 27
    for s in cat:
        assert s['id'] == s['type'] and s['name'] and s['category'] and s['description']
        assert s['supported'] is True and s['runnable'] is True and s['reason'] is None
        for p in s['parameters']:
            assert {'id', 'name', 'type', 'default', 'min', 'max', 'step'} <= set(p), (s['id'], p)
            assert p['min'] <= p['default'] <= p['max'], (s['id'], p)
            if not float(p['step']).is_integer():
                assert p['type'] == 'float', (s['id'], p)
    rsi = next(s for s in cat if s['id'] == 'rsi')
    assert 'simple-average' in rsi['description'] and rsi['rsiFlavour'] == 'simple'
    macd = next(s for s in cat if s['id'] == 'macd')
    zero = next(s for s in cat if s['id'] == 'macd_zero_cross')
    assert 'signal line' in macd['description'] and 'zero' in zero['description']


def test_shorts_borrow_leverage_and_sizing(spy):
    for fill in ('next_open', 'next_close', 'same_close'):
        _, run, trades, f = engine_run(spy, 'rsi', {}, allowShort=True, fillTiming=fill)
        shorts = [t for t in trades if t['side'] == 'short']
        assert shorts and run.borrow.sum() > 0, fill
        assert sum(t['borrowCost'] for t in trades) == pytest.approx(run.borrow.sum(), rel=1e-9), fill
        assert sum(t['pnl'] for t in trades) == pytest.approx(run.equity[-1] - run.equity[0], abs=1e-6), fill
        # borrow = short shares held at the previous close x that close x 0.25 % / 360 per calendar day
        c = f['Close'].to_numpy()
        days = np.diff(f.index.asi8) / 86400e9
        expect = np.where(run.positions[:-1] < 0, -run.positions[:-1] * c[:-1] * 0.0025 * days / 360.0, 0.0)
        assert np.allclose(run.borrow[1:], expect, rtol=1e-12, atol=1e-9), fill
    _, lev, _, f = engine_run(spy, 'buy_and_hold', leverage=2)
    expo = np.abs(lev.positions) * f['Close'].to_numpy() / lev.equity
    assert expo[1] == pytest.approx(2.0, rel=0.03) and lev.positions[0] == 0
    _, half, _, f = engine_run(spy, 'buy_and_hold', positionSizing='percent', positionSizeValue=50)
    assert (np.abs(half.positions) * f['Close'].to_numpy() / half.equity)[1] == pytest.approx(0.5, rel=0.03)
    _, fixed, t_fix, _ = engine_run(spy, 'buy_and_hold', positionSizing='fixed', positionSizeValue=20000)
    assert t_fix[0]['quantity'] * t_fix[0]['entryPrice'] == pytest.approx(20000, rel=0.01)
    for mode, val, extra in (('risk', 1, {'stopLoss': 0.05}), ('vol_target', 10, {}), ('kelly', 50, {})):
        _, r, t, _ = engine_run(spy, 'sma_crossover', positionSizing=mode, positionSizeValue=val, **extra)
        assert t and sum(x['pnl'] for x in t) == pytest.approx(r.equity[-1] - r.equity[0], abs=1e-6), mode
    bad = call('run_backtest', req(positionSizing='risk', positionSizeValue=1))
    assert bad['success'] is False and bad['errorCode'] == 'INPUT'


def test_buy_and_hold_first_fill_and_ignored_symbols():
    d = call('run_backtest', req('buy_and_hold', {}, symbols=('SPY', 'LATE')))['data']
    assert d['trades'][0]['entryDate'] == '2015-01-05' and d['trades'][0]['exitReason'] == 'open'
    assert 'IGNORED_SYMBOLS' in {w['code'] for w in d['warnings']}
    assert any('symbols after the first' in s for s in d['assumptions']['ignoredSettings'])


def test_custom_code_strategy_runs_in_the_library():
    code = ('class MyCross(Strategy):\n    n1 = 20\n    n2 = 50\n    def init(self):\n'
            '        c = self.data.Close\n'
            '        self.a = self.I(lambda x, n: pd.Series(x).rolling(n).mean(), c, self.n1)\n'
            '        self.b = self.I(lambda x, n: pd.Series(x).rolling(n).mean(), c, self.n2)\n'
            '    def next(self):\n        if crossover(self.a, self.b):\n            self.buy()\n'
            '        elif crossover(self.b, self.a) and self.position:\n            self.position.close()\n')
    custom = call('run_backtest', dict(req(), strategy={'type': 'code', 'code': code, 'params': {}}))['data']
    cat = call('run_backtest', req())['data']
    assert custom['statistics']['finalCapital'] == pytest.approx(cat['statistics']['finalCapital'], rel=1e-12)
    bad = call('run_backtest', dict(req(), strategy={'type': 'code', 'code': 'import os\n', 'params': {}}))
    assert bad['success'] is False and 'Forbidden import' in bad['error']


def test_monte_carlo_resamples_the_complete_run():
    bt = call('run_backtest', req())['data']
    mc = call('monte_carlo', req(mcSimulations=200, mcMethod='trade_bootstrap'))['data']
    assert mc['engine'] == 'backtestingpy'
    assert mc['stats']['originalFinal'] == pytest.approx(bt['statistics']['finalCapital'], rel=1e-6)


def test_indicators_and_capabilities():
    d = call('calculate_indicator', {'indicator': 'rsi', 'symbols': ['SPY'], 'startDate': '2020-01-01',
                                     'endDate': '2020-12-31', 'params': {'period': 14}})['data']
    assert d['period'] == 14 and d['totalBars'] > 200 and len(d['series']) == d['totalBars']
    bad = call('calculate_indicator', {'indicator': 'nope', 'symbols': ['SPY']})
    assert bad['success'] is False and bad['errorCode'] == 'INPUT'
    cap = call('get_capabilities', {})['data']
    assert cap['library'] == 'backtesting' and cap['stops'] and cap['shortSelling'] and cap['leverage']
    assert cap['multiSymbol'] is False and cap['maxSymbolsPerRun'] == 1 and cap['weightsBlock'] is False
    assert set(cap['sizingMethods']) == {'percent', 'fixed', 'kelly', 'vol_target', 'risk'}
    rk = cap['requestKeys']
    assert {'holdoutFraction', 'wfMode', 'purgeBars', 'embargoBars', 'seed', 'maxWorkers', 'leverage'} <= set(rk['honoured'])
    assert 'weights' in rk['ignored'] and 'block_bootstrap' in cap['monteCarloMethods']
    assert cap['fillTimings'] == ['next_open', 'next_close', 'same_close'] and 'us_equity_retail' in cap['costPresets']
    assert 'next_close' in call('get_command_options', {})['data']['fillTimings']


# ── T-P10: cancel + progress (subprocess, exit code 3) ───────────────────────

def test_tp10_cancel_inside_optimise_and_progress(tmp_path):
    cf = tmp_path / 'cancel.flag'
    payload = req(paramRanges={'fastPeriod': {'min': 5, 'max': 54, 'step': 1},
                               'slowPeriod': {'min': 60, 'max': 150, 'step': 10}}, cancelFile=str(cf))
    import threading
    p = subprocess.Popen([sys.executable, str(BTP_DIR / 'backtestingpy_provider.py'), 'optimize', '--stdin'],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                         env=dict(os.environ, FINCEPT_BT_MAX_WORKERS='1', FINCEPT_BT_PROGRESS='1'))
    err_lines = []
    reader = threading.Thread(target=lambda: err_lines.extend(iter(p.stderr.readline, '')), daemon=True)
    reader.start()
    p.stdin.write(json.dumps(payload))
    p.stdin.close()
    t_end = time.monotonic() + 60
    while time.monotonic() < t_end and p.poll() is None and \
            sum(1 for x in err_lines if '"phase":"optimize"' in x) < 2:
        time.sleep(0.05)
    cf.write_text('x')
    t = time.monotonic()
    p.wait(timeout=60)
    latency = time.monotonic() - t
    out = p.stdout.read()
    reader.join(timeout=5)
    assert p.returncode == 3, ''.join(err_lines)[-800:]
    lines = [x for x in out.splitlines() if x.strip()]
    assert len(lines) == 1 and json.loads(lines[0])['errorCode'] == 'CANCELLED'
    assert latency < 2.0
    assert any(x.startswith('@@BT_PROGRESS {"phase":"optimize"') for x in err_lines)
