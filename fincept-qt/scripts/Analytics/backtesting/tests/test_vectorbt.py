"""The "VectorBT" engine rebased on base/ (audit Part 3, acceptance tests T1-T12), offline on the
CSV fixtures through a FakeYahoo-warmed price store (no network)."""
import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

HERE = Path(__file__).resolve().parent
VBT_DIR = HERE.parent / 'vectorbt'
if str(VBT_DIR) not in sys.path:
    sys.path.insert(0, str(VBT_DIR))

import reference as ref  # noqa: E402
from base.base_provider import json_response  # noqa: E402
from base.contract import validate_envelope  # noqa: E402
from base.price_store import PriceStore  # noqa: E402
from helpers import FakeYahoo, yahoo_layout  # noqa: E402

import vbt_engine as E  # noqa: E402
import vbt_evaluate as VE  # noqa: E402
import vbt_indicators as I  # noqa: E402
import vbt_strategies as ST  # noqa: E402
import vectorbt_provider as VP  # noqa: E402

ETF5 = ['SPY', 'QQQ', 'TLT', 'GLD', 'EFA']
CUT = '2022-06-30'


def _ohlc(o, c):
    return pd.DataFrame({'Open': o, 'High': np.maximum(o, c), 'Low': np.minimum(o, c), 'Close': c, 'Volume': 1e6},
                        index=c.index)


@pytest.fixture(scope='module')
def frames(spy, etf5, irx):
    opens, closes = etf5
    f = {'SPY': spy}
    for s in ETF5[1:]:
        f[s] = _ohlc(opens[s], closes[s])
    # periodic returns (identical statistics in every window) for the walk-forward degradation test
    pat = np.array([0.004, -0.002, 0.003, -0.001, 0.002, -0.003, 0.005, -0.002, 0.001, -0.001] * 2)
    r = pat[np.arange(len(spy)) % 20]
    c = pd.Series(100.0 * np.cumprod(1.0 + r), index=spy.index)
    f['PER'] = _ohlc(c.shift(1).fillna(100.0), c)
    # SPY with every bar after CUT replaced by a random walk (look-ahead test)
    rng = np.random.default_rng(3)
    p = spy.copy()
    after = p.index > pd.Timestamp(CUT)
    k = np.cumprod(1.0 + rng.normal(0, 0.03, after.sum()))
    for col in ('Open', 'High', 'Low', 'Close'):
        p.loc[after, col] = p.loc[after, col].values * k
    p.loc[after, 'High'] = p.loc[after, ['Open', 'High', 'Low', 'Close']].max(axis=1)
    p.loc[after, 'Low'] = p.loc[after, ['Open', 'High', 'Low', 'Close']].min(axis=1)
    f['SPYP'] = p
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


def call(cmd, req):
    env = json.loads(json_response(VP.dispatch(cmd, req)))
    if cmd in ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo') and env.get('success'):
        assert validate_envelope(env, cmd) == [], validate_envelope(env, cmd)[:5]
    return env


def req(strategy='sma_crossover', params=None, symbols=('SPY',), start='2015-01-01', end='2024-12-31', comm=0.001,
        slip=0.0005, **kw):
    r = {'symbols': list(symbols), 'startDate': start, 'endDate': end, 'interval': '1d', 'initialCapital': 100000,
         'commission': comm, 'slippage': slip, 'positionSizing': 'percent', 'positionSizeValue': 100,
         'benchmarkSymbol': 'SPY', 'riskFreeRate': 0.0,
         'strategy': {'type': strategy, 'params': dict({'fastPeriod': 20, 'slowPeriod': 50}
                                                       if params is None and strategy == 'sma_crossover'
                                                       else (params or {}))}}
    r.update(kw)
    return r


# ── T1: the canonical table against the independent reference ───────────────

@pytest.mark.parametrize('kind', ['bh', 'sma', 'rsi'])
@pytest.mark.parametrize('cost', ['zero', 'costs'])
def test_t1_reference_cases(spy, kind, cost):
    import reference_harness as H
    comm, slip = H.COSTS[cost]
    sid = {'bh': 'buy_and_hold', 'sma': 'sma_crossover', 'rsi': 'rsi'}[kind]
    env = call('run_backtest', req(sid, H.DEFAULT_PARAMS[kind], comm=comm, slip=slip))
    conv = {'fill': 'next_open', 'shares': 'fractional', 'sma_rule': 'cross', 'sma_gate': True, 'rsi': 'wilder',
            'rsi_rule': 'cross', 'ew_when': 'first_bar_of_month', 'close_at_end': False}
    out = H.compare_case(env['data'], H.reference_case(kind, {'SPY': spy}, conv, comm, slip), kind, conv, cost)
    assert out['pass'], out
    assert out['final_rel_diff'] < 1e-6 and out['max_bar_rel_diff'] < 1e-6
    d = env['data']
    assert d['engine'] == 'vectorbt' and 'base.simulator' in d['engineImplementation']
    assert 'vectorbt library is not used' in d['engineImplementation']
    codes = {w['code'] for w in d['warnings']}
    assert not codes & {'TRADE_LIST_INCOMPLETE', 'SLIPPAGE_NOT_ITEMISED', 'REBALANCE_TRADE_STATS'}
    if cost == 'costs':
        assert d['costs']['totalSlippage'] > 0 and d['statistics']['totalSlippage'] == d['costs']['totalSlippage']


def test_real_vectorbt_library_is_not_imported():
    call('run_backtest', req(end='2016-12-31'))
    mod = sys.modules.get('vectorbt')
    assert mod is None or not hasattr(mod, 'Portfolio')


# ── T2: metrics are base.metrics on the engine's own curve ───────────────────

def test_t2_sharpe_sortino_standard_definitions():
    for rf in (0.0, 0.04):
        d = call('run_backtest', req(riskFreeRate=rf, riskFreeSource='rate'))['data']
        eq = pd.Series([p['equity'] for p in d['equity']], index=pd.to_datetime([p['date'] for p in d['equity']]))
        m = ref.metrics(eq, rf=rf)
        assert d['performance']['sharpeRatio'] == pytest.approx(m['sharpe'], rel=5e-3)
        assert d['performance']['sortinoRatio'] == pytest.approx(m['sortino'], rel=5e-3)


# ── T3: no look-ahead, every catalogue strategy and every sizing mode ─────────

def _run_engine(frames, sym, sid, params, **kw):
    r = req(sid, params, symbols=(sym,), **kw)
    spec = E.parse_request(r)
    data = E.Data(frames={sym: frames[sym]}, symbols=[sym], provenance={}, warnings=[])
    panel = E.make_panel(data)
    book = E.SignalBook(panel, data.frames, [sym])
    res = E.simulate(spec, E.make_simulator(spec, panel), book, spec.params,
                     ppy=E.periods_per_year_by_symbol(data.frames, [sym]))
    return panel, res


@pytest.mark.parametrize('sid', [c['type'] for c in ST.CATALOG if c['type'] != 'rebalance_periodic'])
def test_t3_no_lookahead_every_strategy(frames, sid):
    params = {p['id']: p['default'] for p in next(c for c in ST.CATALOG if c['type'] == sid)['parameters']}
    p1, a = _run_engine(frames, 'SPY', sid, params)
    p2, b = _run_engine(frames, 'SPYP', sid, params)
    cut = int(np.searchsorted(p1.dates, pd.Timestamp(CUT), side='right'))
    assert np.array_equal(a.equity[:cut], b.equity[:cut])
    fa, fb = a.fill_arrays, b.fill_arrays
    ka, kb = fa['bar'] < cut, fb['bar'] < cut
    assert np.array_equal(fa['qty'][ka], fb['qty'][kb]) and np.array_equal(fa['bar'][ka], fb['bar'][kb])


@pytest.mark.parametrize('sizing,value,extra', [('kelly', 50, {}), ('vol_target', 10, {}), ('fixed', 25000, {}),
                                                ('risk', 1, {'stopLoss': 0.05}), ('percent', 40, {})])
def test_t3_no_lookahead_sizing_modes(frames, sizing, value, extra):
    kw = dict(positionSizing=sizing, positionSizeValue=value, **extra)
    p1, a = _run_engine(frames, 'SPY', 'sma_crossover', {'fastPeriod': 10, 'slowPeriod': 30}, **kw)
    p2, b = _run_engine(frames, 'SPYP', 'sma_crossover', {'fastPeriod': 10, 'slowPeriod': 30}, **kw)
    cut = int(np.searchsorted(p1.dates, pd.Timestamp(CUT), side='right'))
    assert np.array_equal(a.equity[:cut], b.equity[:cut])


# ── T4 / T5: one portfolio, a real rebalance ─────────────────────────────────

@pytest.mark.parametrize('comm', [0.0, 0.001])
def test_t4_monthly_equal_weight_is_a_real_rebalance(etf5, comm):
    opens, closes = etf5
    env = call('run_backtest', req('rebalance_periodic', {'rebalancePeriod': 'monthly'}, symbols=ETF5,
                                   start='2020-01-01', end='2024-12-31', comm=comm, slip=0.0))
    d = env['data']
    r = ref.ew_monthly(closes, opens, commission=comm, fill='next_open')
    eng = pd.Series([p['equity'] for p in d['equity']], index=pd.to_datetime([p['date'] for p in d['equity']]))
    rel = float(np.max(np.abs(eng.values / r['equity'].reindex(eng.index).values - 1)))
    assert rel <= (1e-4 if comm == 0 else 1e-3)
    if comm:
        assert d['costs']['totalCommission'] == pytest.approx(r['fees'], rel=0.01)
    assert d['costs']['tradedNotional'] == pytest.approx(r['turnover'], rel=0.01)
    codes = {w['code'] for w in d['warnings']}
    # trades are position episodes (base.execution.position_episodes): one per symbol, held through every
    # rebalance - no FIFO fragments of the trims, so no REBALANCE_TRADE_STATS and the P&L reconciles
    assert not codes & {'REBALANCE_TRADE_STATS', 'TRADE_LIST_INCOMPLETE'}
    opens_ = [t for t in d['trades'] if t['exitReason'] == 'open']
    assert sorted(t['symbol'] for t in opens_) == sorted(ETF5) and len(d['trades']) == 5
    assert all(t['episode'] and t['fills'] > 50 for t in d['trades'])
    assert sum(t['pnl'] for t in d['trades']) == pytest.approx(d['equity'][-1]['equity'] - 100000, abs=1.0)
    assert sum(r['tradedNotional'] for r in d['costs']['byYear']) == pytest.approx(d['costs']['tradedNotional'],
                                                                                    rel=1e-9)
    assert any('POSITION EPISODES' in n for n in d['assumptions']['notes'])
    assert len(d['weights']['symbols']) == 5


def test_t5_multi_symbol_shared_cash():
    d = call('run_backtest', req(symbols=('SPY', 'QQQ'), start='2020-01-01'))['data']
    assert d['statistics']['totalFees'] == pytest.approx(sum(t['commission'] for t in d['trades']), rel=1e-9)
    ex = [p['exposure'] for p in d['equity'] if p.get('exposure') is not None]
    assert ex and min(ex) >= 0 and max(ex) <= 1 + 1e-9
    assert 0 <= d['exposure']['timeInMarket'] <= 1
    assert {t['symbol'] for t in d['trades']} == {'SPY', 'QQQ'}
    pnl = sum(t['pnl'] for t in d['trades'])
    assert pnl == pytest.approx(d['equity'][-1]['equity'] - 100000, abs=1.0)
    assert 'TRADE_LIST_INCOMPLETE' not in {w['code'] for w in d['warnings']}


def test_t5_optimise_runs_the_same_portfolio_as_the_backtest():
    for syms in (('SPY',), ('SPY', 'QQQ')):
        start = '2020-01-01'
        opt = call('optimize', req(symbols=syms, start=start, paramRanges={'fastPeriod': {'min': 20, 'max': 20, 'step': 1},
                                                                            'slowPeriod': {'min': 50, 'max': 50, 'step': 1}}))
        bt = call('run_backtest', req(symbols=syms, start=start))['data']
        tr = bt['equity'][-1]['equity'] / bt['equity'][0]['equity'] - 1
        assert opt['data']['bestPerformance']['totalReturn'] == pytest.approx(tr, rel=1e-6)
        assert opt['data']['symbols'] == list(syms)


# ── event path == base.simulator ─────────────────────────────────────────────

def test_event_path_is_bit_identical_to_the_simulator(spy):
    from base.costs import CostModel
    from base.simulator import Simulator, numba_available, panel_from_arrays
    p = panel_from_arrays(spy['Close'], spy['Open'], spy['High'], spy['Low'], spy['Volume'], dates=spy.index,
                          symbols=['SPY'])
    rng = np.random.default_rng(11)
    n = len(spy)
    backends = ['python'] + (['numba'] if numba_available() else [])
    for t in range(18):
        le, lx = rng.random(n) < rng.uniform(0.01, 0.3), rng.random(n) < rng.uniform(0.01, 0.3)
        fill = ('next_open', 'next_close', 'same_close')[t % 3]
        cm = CostModel(commission_value=(0.0, 0.001, 0.002)[t % 3], slippage_value=(0.0, 0.0005)[t % 2],
                       spread_bps=(0.0, 2.0)[(t // 2) % 2])
        size, start = (1.0, 0.5, 0.33)[t % 3], (0, 250, 1)[t % 3]
        eq, fills = VE.long_flat_events(spy['Open'].values, spy['Close'].values, le, lx, fill=fill, size=size,
                                        commission=cm.commission_value,
                                        slip=cm.slippage_fractions(spy['Close'].values)[:, 0], init_cash=100000.0,
                                        start=start)
        for be in backends[t % len(backends):][:1]:
            res = Simulator(p, costs=cm, fill=fill, size=size, backend=be).run(entries=le, exits=lx, start=start)
            assert np.array_equal(eq, res.equity), (t, be)
            for k in ('bar', 'qty', 'price', 'commission'):
                assert np.array_equal(fills[k], res.fill_arrays[k]), (t, k)


# ── T7: objectives, seeds, float ranges, maxIterations ───────────────────────

def test_t7_objectives_seed_float_ranges():
    ranges = {'fastPeriod': {'min': 10, 'max': 40, 'step': 10}, 'slowPeriod': {'min': 50, 'max': 150, 'step': 25}}
    a = call('optimize', req(start='2019-01-01', paramRanges=ranges, optimizeObjective='sharpe'))['data']
    b = call('optimize', req(start='2019-01-01', paramRanges=ranges, optimizeObjective='omega'))['data']
    assert a['objective'] == 'sharpe' and b['objective'] == 'omega'
    assert [r['objectiveValue'] for r in a['allResults']] != [r['objectiveValue'] for r in b['allResults']]
    big = {'fastPeriod': {'min': 5, 'max': 40, 'step': 1}, 'slowPeriod': {'min': 50, 'max': 150, 'step': 5}}
    r1 = call('optimize', req(start='2019-01-01', paramRanges=big, optimizeMethod='random', maxIterations=30.0,
                              seed=5))['data']
    r2 = call('optimize', req(start='2019-01-01', paramRanges=big, optimizeMethod='random', maxIterations=30.0,
                              seed=5))['data']
    assert r1['iterations'] == 30 and r1['seed'] == 5
    assert [x['parameters'] for x in r1['allResults']] == [x['parameters'] for x in r2['allResults']]
    k = call('optimize', req('keltner_breakout', {}, start='2019-01-01',
                             paramRanges={'multiplier': {'min': 1.0, 'max': 1.3, 'step': 0.1}}))['data']
    assert sorted(x['parameters']['multiplier'] for x in k['allResults']) == [1.0, 1.1, 1.2, 1.3]
    bad = call('optimize', req(start='2019-01-01', paramRanges={'nope': {'min': 1, 'max': 2, 'step': 1}}))
    assert bad['success'] is False and bad['errorCode'] == 'INPUT' and 'nope' in bad['error']


# ── T8: walk-forward ─────────────────────────────────────────────────────────

def test_t8_walk_forward_tiles_and_degradation():
    ranges = {'fastPeriod': {'min': 10, 'max': 30, 'step': 10}, 'slowPeriod': {'min': 50, 'max': 100, 'step': 50}}
    d = call('walk_forward', req(paramRanges=ranges, wfSplits=4, wfTrainRatio=0.6))['data']
    w = d['windows']
    assert len(w) == 4 and all(x['bestParams'] for x in w) and all(x['inSampleSharpe'] is not None for x in w)
    for x, y in zip(w, w[1:]):
        assert pd.Timestamp(y['testStart']) > pd.Timestamp(x['testEnd'])
        assert (pd.Timestamp(y['testStart']) - pd.Timestamp(x['testEnd'])).days <= 5      # contiguous tiles
    assert w[-1]['testEnd'] == '2024-12-31'
    assert 'WALK_FORWARD_NOT_OPTIMISED' not in {x['code'] for x in d['warnings']}
    # PER repeats one 20-bar return pattern: 20 / 40-bar momentum is constant and positive, so the strategy is
    # always in the market and every window has the same statistics -> degradation 1
    per = call('walk_forward', req('momentum', {'lookback': 20, 'threshold': 0.0}, symbols=('PER',), comm=0, slip=0,
                                   paramRanges={'lookback': {'min': 20, 'max': 40, 'step': 20}}, wfSplits=5,
                                   wfTrainRatio=0.6))['data']
    assert per['degradationRatio'] == pytest.approx(1.0, abs=0.05)
    short = call('walk_forward', req(start='2024-06-01', paramRanges=ranges, wfSplits=5))
    assert short['success'] is False and short['errorCode'] == 'INPUT'
    assert 'warm-up' in short['error'] or 'too few' in short['error']


# ── T9: degenerate inputs are flagged; zero trades are null, not 0 ───────────

def test_t9_warnings_and_zero_trades():
    z = call('run_backtest', req(start='2024-10-01', end='2024-11-27'))['data']
    codes = {w['code'] for w in z['warnings']}
    assert {'ZERO_TRADES', 'SHORT_HISTORY'} <= codes
    assert z['performance']['winRate'] is None and z['performance']['profitFactor'] is None
    assert len({p['equity'] for p in z['equity']}) == 1
    inv = call('run_backtest', req(params={'fastPeriod': 50, 'slowPeriod': 20}, end='2016-12-31'))['data']
    assert 'PARAMETER_ORDER' in {w['code'] for w in inv['warnings']}
    pre = call('run_backtest', req(start='2014-12-01', end='2016-12-31'))['data']
    assert 'START_AFTER_REQUEST' in {w['code'] for w in pre['warnings']}
    full = call('run_backtest', req(end='2024-12-31'))['data']
    assert full['statistics']['endDate'] == '2024-12-31'                         # end date inclusive
    mix = call('run_backtest', req(symbols=('SPY', 'ZZZNOTREAL'), end='2016-12-31'))
    assert mix['success'] and {w['code'] for w in mix['data']['warnings']} >= {'SYMBOL_DROPPED'}
    bad = call('run_backtest', req(symbols=('ZZZNOTREAL',)))
    assert bad['success'] is False and bad['errorCode'] == 'DATA' and 'ZZZNOTREAL' in bad['error']


# ── T10: intrabar stops ─────────────────────────────────────────────────────

def test_t10_intrabar_stop_loss(spy):
    d = call('run_backtest', req(params={'fastPeriod': 10, 'slowPeriod': 30}, stopLoss=0.05))['data']
    stops = [t for t in d['trades'] if t['exitReason'] == 'stop_loss']
    assert stops
    o = spy['Open']
    for t in stops:
        level = t['entryPrice'] * 0.95
        bar_open = float(o.loc[t['exitDate']])
        ref_px = min(level, bar_open)
        assert t['exitPrice'] == pytest.approx(ref_px * (1 - 0.0005), rel=1e-9)
        gap = max(0.0, level / bar_open - 1.0) if bar_open < level else 0.0
        assert t['pnlPercent'] >= -(0.05 + gap + 0.003)
    assert 'inside each bar' in d['assumptions']['execution'].lower()


# ── sizing modes ─────────────────────────────────────────────────────────────

def test_sizing_modes():
    def first_notional(d):
        t = d['trades'][0]
        return t['quantity'] * t['entryPrice']
    p = call('run_backtest', req(positionSizing='percent', positionSizeValue=50, end='2016-12-31'))['data']
    assert first_notional(p) == pytest.approx(50000 / 1.001, rel=1e-6)
    f = call('run_backtest', req(positionSizing='fixed', positionSizeValue=25000, end='2016-12-31'))['data']
    assert first_notional(f) == pytest.approx(25000 / 1.001, rel=1e-6)
    r = call('run_backtest', req(positionSizing='risk', positionSizeValue=1, stopLoss=0.05, end='2016-12-31'))['data']
    assert first_notional(r) == pytest.approx(20000 / 1.001, rel=1e-6)
    v = call('run_backtest', req(positionSizing='vol_target', positionSizeValue=5, end='2016-12-31'))['data']
    assert first_notional(v) < 100000 * 0.9
    k = call('run_backtest', req(positionSizing='kelly', positionSizeValue=30,
                                 params={'fastPeriod': 5, 'slowPeriod': 15}))['data']
    closed = [t for t in k['trades'] if t['exitDate']]
    # the cap until 10 trades have closed, then the Kelly fraction of those trades (no look-ahead)
    assert len(closed) >= 10 and first_notional(k) == pytest.approx(30000 / 1.001, rel=1e-6)
    rets = [t['pnl'] / (t['quantity'] * t['entryPrice']) for t in closed[:10]]
    want = E._kelly([float(x) for x in rets], 0.3, 10)
    if len(k['trades']) > 10:
        t11 = k['trades'][10]
        eq_before = [p['equity'] for p in k['equity'] if p['date'] < t11['entryDate']][-1]
        assert t11['quantity'] * t11['entryPrice'] == pytest.approx(want * eq_before / 1.001, rel=0.02)
    else:
        assert want == 0.0
    no_stop = call('run_backtest', req(positionSizing='risk', positionSizeValue=1))
    assert no_stop['success'] is False and no_stop['errorCode'] == 'INPUT' and 'STOP LOSS' in no_stop['error']
    w = call('run_backtest', req('buy_and_hold', {}, symbols=('SPY', 'TLT'), start='2020-01-01',
                                 weights=[0.75, 0.25]))['data']
    first = {t['symbol']: t['quantity'] * t['entryPrice'] for t in w['trades']}
    assert first['SPY'] / first['TLT'] == pytest.approx(3.0, rel=1e-3)


def test_fill_timing_and_shorts():
    for fill in ('next_open', 'next_close', 'same_close'):
        d = call('run_backtest', req(fillTiming=fill, end='2017-12-31'))['data']
        assert d['assumptions']['fillModel']['fillAt'] == fill
    assert 'SAME_BAR_FILL' in {w['code'] for w in d['warnings']}
    s = call('run_backtest', req(allowShort=True, end='2018-12-31'))['data']
    assert any(t['side'] == 'short' for t in s['trades']) and s['costs']['totalBorrow'] > 0
    assert 'borrow cost' not in s['assumptions']['notModelled']
    bad = call('run_backtest', req(fillTiming='whenever'))
    assert bad['success'] is False and bad['errorCode'] == 'INPUT'


# ── catalogue ───────────────────────────────────────────────────────────────

def test_catalogue_specs_complete_and_every_strategy_runs():
    cat = VP.get_strategies({})['data']
    assert len(cat) == 23
    for s in cat:
        for p in s['parameters']:
            assert {'id', 'name', 'type', 'default', 'min', 'max', 'step'} <= set(p), (s['type'], p)
            assert p['min'] <= p['default'] <= p['max'], (s['type'], p)
        params = {p['id']: p['default'] for p in s['parameters']}
        syms = ('SPY', 'TLT') if s['type'] == 'rebalance_periodic' else ('SPY',)
        env = call('run_backtest', req(s['type'], params, symbols=syms, start='2018-01-01'))
        assert env['success'], (s['type'], env.get('error'))


def test_screen_contract_additions():
    """What the rebuilt Backtesting tab reads: costs.sensitivity (0x..3x the cost model), supported / runnable /
    reason on every catalogue row, get_capabilities with request keys, riskFreeSource "series" = auto."""
    d = call('run_backtest', req(start='2018-01-01', riskFreeSource='series',
                                 costModel={'preset': 'us_equity_retail'}))['data']
    sens = d['costs']['sensitivity']
    assert [r['multiple'] for r in sens] == [0, 1, 2, 3]
    assert sens[1]['totalReturn'] == pytest.approx(d['performance']['totalReturn'], abs=1e-6)
    assert sens[0]['totalReturn'] > sens[1]['totalReturn'] > sens[3]['totalReturn']
    assert d['assumptions']['riskFreeSource'] == '^IRX'
    assert all(s['symbol'] and s['bars'] and s['firstBar'] and s['lastBar'] for s in d['provenance']['symbols'])
    z = call('run_backtest', req(start='2018-01-01', comm=0.0, slip=0.0))['data']
    assert 'sensitivity' not in z['costs']                    # nothing to scale
    for s in VP.get_strategies({})['data']:
        assert s['supported'] is True and s['runnable'] is True and 'reason' in s
    cap = VP.dispatch('get_capabilities', {})['data']
    rk = cap['requestKeys']
    for k in ('fillTiming', 'costModel', 'holdoutFraction', 'wfMode', 'purgeBars', 'embargoBars', 'seed', 'maxWorkers'):
        assert k in rk['honoured'], k
    assert 'leverage' in rk['ignored'] and cap['fillTimings'] == ['next_open', 'next_close', 'same_close']
    assert '1d' in cap['intervals'] and 'us_equity_retail' in cap['costPresets']
    assert 'borrow cost' not in cap['notModelled'] and 'FX conversion' in cap['notModelled']


# ── T6 / research commands ──────────────────────────────────────────────────

def test_t6_indicator_table_every_id():
    cat = VP.get_indicators({})['data']['indicators']
    ids = [i for c in cat.values() for i in c]
    base = {'symbols': ['SPY'], 'startDate': '2019-01-01', 'endDate': '2024-12-31'}
    for it in ids:
        iid = it['id']
        c = call('calculate_indicator', dict(base, indicator=iid, params={'period': 14}))
        assert c['success'], (iid, c.get('error'))
        thr = call('indicator_signals', dict(base, indicator=iid, mode='threshold_signals', params={'period': 14}))
        assert thr['success'] == it['thresholdSignals'] == ('threshold_signals' in it['signalModes']), iid
        sw = call('indicator_param_sweep', dict(base, indicator=iid, paramRange={'min': 5, 'max': 25, 'step': 5}))
        assert sw['success'] == it['sweepable'], (iid, sw.get('error'))
        if sw['success']:
            assert sw['data']['totalCombinations'] > 0
        for mode in ('crossover_signals', 'breakout_signals', 'mean_reversion_signals'):
            assert call('indicator_signals', dict(base, indicator=iid, mode=mode, params={'period': 20}))['success']
    sf = call('indicator_signals', dict(base, indicator='rsi', mode='signal_filter',
                                        params={'base_indicator': 'rsi', 'base_period': 14, 'filter_indicator': 'adx',
                                                'filter_period': 14, 'filter_threshold': 25, 'filter_type': 'above'}))
    assert sf['success'] and sf['data']['filteredEntryCount'] <= sf['data']['originalEntryCount']
    st = call('indicator_param_sweep', dict(base, indicator='stochastic', paramRange={'min': 5, 'max': 20, 'step': 5}))
    assert st['success'] and st['data']['totalCombinations'] == 4
    for bad in ('nope', 'stochastic_x'):
        e = call('calculate_indicator', dict(base, indicator=bad))
        assert e['success'] is False and e['errorCode'] == 'INPUT'
    e = call('indicator_signals', dict(base, indicator='macd_x', mode='threshold_signals'))
    assert e['success'] is False


def test_research_uses_the_requested_symbol_and_rf(spy):
    d = call('calculate_indicator', {'symbols': ['SPY', 'QQQ'], 'startDate': '2020-01-01', 'endDate': '2024-12-31',
                                     'indicator': 'rsi', 'params': {'period': 14}})['data']
    assert d['symbol'] == 'SPY' and any(w['code'] == 'IGNORED_SYMBOLS' for w in d['warnings'])
    want = I.rsi(spy.loc['2020-01-01':, 'Close'].values, 14)
    assert d['stats']['last'] == pytest.approx(float(want[-1]), rel=1e-12)
    assert d['series'][0]['date'] == '2020-01-02'
    a0 = call('analyze_returns', {'symbols': ['SPY'], 'startDate': '2015-01-01', 'endDate': '2024-12-31',
                                  'analysisType': 'returns_stats', 'riskFreeRate': 0.0})['data']['stats']
    a4 = call('analyze_returns', {'symbols': ['SPY'], 'startDate': '2015-01-01', 'endDate': '2024-12-31',
                                  'analysisType': 'returns_stats', 'riskFreeRate': 0.04})['data']['stats']
    m0, m4 = ref.metrics(spy['Close'], rf=0.0), ref.metrics(spy['Close'], rf=0.04)
    assert a0['Sharpe Ratio'] == pytest.approx(m0['sharpe'], rel=1e-9)
    assert a4['Sharpe Ratio'] == pytest.approx(m4['sharpe'], rel=1e-9)
    assert a4['Sortino Ratio'] == pytest.approx(m4['sortino'], rel=1e-9)
    roll = call('analyze_returns', {'symbols': ['SPY'], 'startDate': '2015-01-01', 'endDate': '2024-12-31',
                                    'analysisType': 'rolling', 'params': {'window': 63, 'metric': 'sharpe'}})['data']
    assert roll['dataPoints'] == len(spy) - 1 - 62                       # no fabricated warm-up zeros


def test_labels_splits_generators():
    base = {'symbols': ['SPY'], 'startDate': '2019-01-01', 'endDate': '2024-12-31'}
    for lt in ('FIXLB', 'MEANLB', 'LEXLB', 'TRENDLB', 'BOLB'):
        g = call('generate_labels', dict(base, labelType=lt, params={}))['data']
        assert g['labeledBars'] < g['totalBars'] and sum(g['distribution'].values()) == g['labeledBars']
        assert call('labels_to_signals', dict(base, labelType=lt, params={}))['success']
    for st in ('RollingSplitter', 'ExpandingSplitter', 'PurgedKFoldSplitter'):
        s = call('generate_splits', dict(base, splitterType=st, params={'window_len': 252, 'test_len': 63,
                                                                         'step': 63, 'n_splits': 5}))['data']
        assert s['nSplits'] > 0 and s['splits'][0]['testStart'][:4].isdigit() and ' ' not in s['splits'][0]['testStart']
    a = call('generate_signals', dict(base, generatorType='RPROBX', params={'entry_prob': 0.05, 'exit_prob': 0.1}))
    b = call('generate_signals', dict(base, generatorType='RPROBX', params={'entry_prob': 0.05, 'exit_prob': 0.1}))
    assert a['data']['entries'] == b['data']['entries'] and a['data']['seed'] == 42


# ── cancel (exit code 3 + the cancelled envelope) ────────────────────────────

def test_cancel_optimise_exits_with_code_3(tmp_path):
    cf = tmp_path / 'cancel.flag'
    cf.write_text('x')
    payload = req(paramRanges={'fastPeriod': {'min': 5, 'max': 50, 'step': 1},
                               'slowPeriod': {'min': 60, 'max': 150, 'step': 10}}, cancelFile=str(cf))
    p = subprocess.run([sys.executable, str(VBT_DIR / 'vectorbt_provider.py'), 'optimize', '--stdin'],
                       input=json.dumps(payload), capture_output=True, text=True, timeout=120,
                       env=dict(os.environ, FINCEPT_BT_MAX_WORKERS='1'))
    assert p.returncode == 3, p.stderr[-800:]
    lines = [x for x in p.stdout.splitlines() if x.strip()]
    assert len(lines) == 1 and json.loads(lines[0])['errorCode'] == 'CANCELLED'
