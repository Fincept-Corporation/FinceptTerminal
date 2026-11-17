"""The "bt" engine rebased on base/ (audit bt part, acceptance tests T-T1..T-T10), offline on the CSV
fixtures through a FakeYahoo-warmed price store (no network)."""
import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

HERE = Path(__file__).resolve().parent
BT_DIR = HERE.parent / 'bt'
if str(BT_DIR) not in sys.path:
    sys.path.insert(0, str(BT_DIR))

import reference as ref  # noqa: E402
from base.base_provider import json_response  # noqa: E402
from base.contract import validate_envelope  # noqa: E402
from base.price_store import PriceStore  # noqa: E402
from helpers import FakeYahoo, yahoo_layout  # noqa: E402

import bt_engine as E  # noqa: E402
import bt_provider as BP  # noqa: E402
import bt_strategies as ST  # noqa: E402

ETF5 = ['SPY', 'QQQ', 'TLT', 'GLD', 'EFA']
CUT = '2022-06-30'
SIGNALS = ['sma_crossover', 'ema_crossover', 'macd', 'adx_trend', 'mean_reversion', 'bollinger_bands', 'rsi',
           'stochastic', 'breakout', 'momentum']
ALLOC = ['buy_and_hold', 'equal_weight', 'inv_vol', 'mean_var', 'min_var', 'risk_parity', 'target_vol',
         'momentum_topn', 'momentum_inv_vol', 'dual_momentum']


def _ohlc(o, c):
    return pd.DataFrame({'Open': o, 'High': np.maximum(o, c), 'Low': np.minimum(o, c), 'Close': c, 'Volume': 1e6},
                        index=c.index)


@pytest.fixture(scope='module')
def frames(spy, etf5, irx, cal):
    opens, closes = etf5
    f = {'SPY': spy}
    for s in ETF5[1:]:
        f[s] = _ohlc(opens[s], closes[s])
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
    for s in ('BTC-USD', 'RELIANCE.NS', '^NSEI'):
        f[s] = cal[s]
    return f


@pytest.fixture(scope='module', autouse=True)
def warm(frames, meta):
    tz = {s: (meta.get(s) or {}).get('tz') or 'America/New_York' for s in frames}
    tz['^IRX'] = 'America/Chicago'
    fake = FakeYahoo({s: yahoo_layout(f, tz=tz[s]) for s, f in frames.items()},
                     meta={s: {'currency': (meta.get(s) or {}).get('currency')} for s in frames if meta.get(s)})
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
         'commission': comm, 'slippage': slip, 'positionSizing': 'percent', 'positionSizeValue': 100,
         'benchmarkSymbol': 'SPY', 'riskFreeRate': 0.0,
         'strategy': {'type': strategy, 'params': dict({'fastPeriod': 20, 'slowPeriod': 50}
                                                       if params is None and strategy == 'sma_crossover'
                                                       else (params or {}))}}
    r.update(kw)
    return r


def eq_series(d):
    return pd.Series([p['equity'] for p in d['equity']], index=pd.to_datetime([p['date'] for p in d['equity']]))


def run_inproc(r):
    """(spec, panel, SimResult) of one run through the engine's own functions (simulator path)."""
    spec = E.parse_request(r)
    data = E.load_data(spec, r)
    panel = E.make_panel(data)
    from base.metrics import infer_periods_per_year
    if spec.kind == 'signals':
        res = E.simulate(spec, E.make_simulator(spec, panel), E.SignalBook(panel, data.frames, data.symbols), None,
                         spec.params)
    else:
        res = E.simulate(spec, E.make_simulator(spec, panel), None,
                         E.DecisionBook(panel, float(infer_periods_per_year(panel.dates))), spec.params)
    return spec, panel, res


# ── T-T1: the reference cases (state SMA, Wilder RSI levels, whole shares) ────

@pytest.mark.parametrize('fill', ['next_open', 'next_close'])
@pytest.mark.parametrize('kind', ['bh', 'sma', 'rsi'])
@pytest.mark.parametrize('cost', ['zero', 'costs'])
def test_tt1_reference_cases(spy, kind, cost, fill):
    import reference_harness as H
    comm, slip = H.COSTS[cost]
    sid = {'bh': 'buy_and_hold', 'sma': 'sma_crossover', 'rsi': 'rsi'}[kind]
    env = call('run_backtest', req(sid, H.DEFAULT_PARAMS[kind], comm=comm, slip=slip, fillTiming=fill))
    conv = {'fill': fill, 'shares': 'whole', 'sma_rule': 'state', 'sma_gate': True, 'rsi': 'wilder',
            'rsi_rule': 'level', 'ew_when': 'first_bar_of_month', 'close_at_end': False}
    out = H.compare_case(env['data'], H.reference_case(kind, {'SPY': spy}, conv, comm, slip), kind, conv, cost)
    assert out['pass'], out
    assert out['final_rel_diff'] < 1e-6 and out['max_bar_rel_diff'] < 1e-6
    d = env['data']
    codes = {w['code'] for w in d['warnings']}
    assert not codes & {'TRADE_LIST_INCOMPLETE', 'SLIPPAGE_NOT_ITEMISED', 'REBALANCE_TRADE_STATS'}
    if kind == 'bh' and fill == 'next_close':          # buy & hold is a bt stack: bt.Backtest executed it
        assert 'bt.Backtest executes' in d['engineImplementation'] and d['provenance']['executedBy'] == 'bt'
    if cost == 'costs':
        assert d['costs']['totalSlippage'] > 0 and d['statistics']['totalSlippage'] == d['costs']['totalSlippage']


def test_tt1_equal_weight_monthly(etf5):
    opens, closes = etf5
    base = dict(symbols=ETF5, start='2020-01-01', end='2024-12-31', slip=0.0)
    for comm in (0.0, 0.001):     # fractional shares on base.simulator: the reference to rounding
        d = call('run_backtest', req('equal_weight', {'rebalancePeriod': 'monthly'}, comm=comm,
                                     fractionalShares=True, **base))['data']
        r = ref.ew_monthly(closes, opens, commission=comm, fill='next_open')
        e = eq_series(d)
        assert float(np.max(np.abs(e.values / r['equity'].reindex(e.index).values - 1))) < 1e-6
        if comm:
            assert d['costs']['totalCommission'] == pytest.approx(r['fees'], rel=1e-6)
        assert d['costs']['tradedNotional'] == pytest.approx(r['turnover'], rel=1e-6)
    for fill in ('next_open', 'next_close'):   # whole shares (default): simulator / bt.Backtest within 0.5 %
        d = call('run_backtest', req('equal_weight', {'rebalancePeriod': 'monthly'}, fillTiming=fill, **base))['data']
        r = ref.ew_monthly(closes, opens, commission=0.001, fill=fill)
        e = eq_series(d)
        assert float(np.max(np.abs(e.values / r['equity'].reindex(e.index).values - 1))) < 0.005
        assert d['costs']['totalCommission'] == pytest.approx(r['fees'], rel=0.01)
        assert d['provenance']['executedBy'] == ('bt' if fill == 'next_close' else 'base.simulator')
        assert not any('bt charged' in n for n in d['assumptions']['notes'])   # native fees reconcile


# ── T-T2: every strategy is the strategy its name says ────────────────────────

def test_tt2_every_catalogue_strategy_runs_and_is_itself(spy):
    """Engine functions directly (the result normaliser is not what this checks; every strategy also runs
    through the provider in test_tt6 / the catalogue run)."""
    finals = {}
    for sid in SIGNALS + ['buy_and_hold']:
        _, _, res = run_inproc(req(sid, None if sid == 'sma_crossover' else {}, start='2020-01-01'))
        finals[sid] = float(res.equity[-1])
        assert len(res.fill_arrays['bar']) > 0, sid
    for sid in ALLOC:
        spec, panel, res = run_inproc(req(sid, {}, symbols=ETF5, start='2020-01-01'))
        finals['alloc_' + sid] = float(res.equity[-1])
        assert len(res.fill_arrays['bar']) > 0, sid
    vals = [round(v, 2) for v in finals.values()]
    assert len(set(vals)) == len(vals), finals
    assert len({finals['adx_trend'], finals['stochastic'], finals['buy_and_hold']}) == 3
    assert finals['momentum'] != finals['alloc_dual_momentum']
    assert finals['alloc_mean_var'] != finals['alloc_min_var'] != finals['alloc_risk_parity']
    assert len(set(ST.get_strategy_catalog()[i]['type'] for i in range(20))) == 20 == len(SIGNALS) + len(ALLOC)


def test_tt2_rsi_exits_above_overbought_and_reversion_rules_differ(spy):
    d = call('run_backtest', req('rsi', {'period': 14, 'oversold': 30, 'overbought': 70}))['data']
    r = ST.rsi(spy['Close'].to_numpy(), 14)
    closed = [t for t in d['trades'] if t['exitDate']]
    assert len(closed) >= 5
    for t in closed:             # decided on the close before the fill (next_open)
        assert r[t['entryBar'] - 1] < 30 and r[t['exitBar'] - 1] > 70
    mr = call('run_backtest', req('mean_reversion', {'window': 20, 'threshold': 2.0}))['data']
    bb = call('run_backtest', req('bollinger_bands', {'period': 20, 'stdDev': 2.0}))['data']
    assert mr['statistics']['finalCapital'] != bb['statistics']['finalCapital']
    assert [t['exitDate'] for t in mr['trades']] != [t['exitDate'] for t in bb['trades']]


# ── T-T3: mixed calendars, session dates ──────────────────────────────────────

def test_tt3_mixed_calendars_run_with_a_warning(cal):
    win = dict(start='2024-01-01', end='2024-06-28')
    for syms, sid, params in ((['BTC-USD', 'SPY'], 'equal_weight', {'rebalancePeriod': 'monthly'}),
                              (['BTC-USD', 'SPY'], 'sma_crossover', {'fastPeriod': 5, 'slowPeriod': 20}),
                              (['SPY', 'RELIANCE.NS'], 'equal_weight', {'rebalancePeriod': 'monthly'})):
        for fill in ('next_open', 'next_close'):
            d = call('run_backtest', req(sid, params, symbols=syms, fillTiming=fill, **win))['data']
            assert {w['symbol'] for w in d['warnings'] if w['code'] == 'CALENDAR_MISMATCH'}, syms
            for f in d['fills']:      # every fill on a session of its own market
                assert pd.Timestamp(f['date']) in cal[f['symbol']].index if f['symbol'] != 'SPY' else \
                    pd.Timestamp(f['date']).dayofweek < 5
            if sid == 'equal_weight' and fill == 'next_close':
                assert d['provenance']['executedBy'] == 'base.simulator'
                assert 'NATIVE_EXECUTION_UNAVAILABLE' in {w['code'] for w in d['warnings']}
    d = call('run_backtest', req('buy_and_hold', {}, symbols=['RELIANCE.NS'], benchmarkSymbol='^NSEI', **win))['data']
    idx = pd.to_datetime([p['date'] for p in d['equity']])
    assert list(idx) == list(cal['RELIANCE.NS'].index[(cal['RELIANCE.NS'].index >= '2024-01-01')])
    assert int((idx.dayofweek >= 5).sum()) == 0
    assert d['performance']['beta'] > 0.2


# ── T-T4: equity from the first bar, flat in warm-up ──────────────────────────

def test_tt4_equity_starts_at_the_first_bar():
    d = call('run_backtest', req())['data']
    assert d['equity'][0]['date'] == d['statistics']['startDate'] == '2015-01-02'
    assert d['statistics']['endDate'] == '2024-12-31' and len(d['equity']) == 2516
    first = d['trades'][0]['entryBar']
    assert first >= 50 and all(p['equity'] == 100000 for p in d['equity'][:first])
    a = call('run_backtest', req('inv_vol', {'lookback': 60}, symbols=ETF5, start='2020-01-01'))['data']
    assert a['equity'][0]['date'] == '2020-01-02' and a['rebalancing']['firstDecision'] >= '2020-03-27'
    entry = min(t['entryBar'] for t in a['trades'])
    assert entry > 60 and all(p['equity'] == 100000 for p in a['equity'][:entry])


# ── T-T5: risk-free rate, trade statistics, allocation reporting ──────────────

def test_tt5_rf_and_trade_statistics():
    for rf in (0.0, 0.04):
        d = call('run_backtest', req(riskFreeRate=rf, riskFreeSource='rate'))['data']
        m = ref.metrics(eq_series(d), rf=rf)
        assert d['performance']['sharpeRatio'] == pytest.approx(m['sharpe'], rel=5e-3)
        assert d['performance']['winRate'] is not None and d['performance']['profitFactor'] is not None
    d = call('run_backtest', req('equal_weight', {'rebalancePeriod': 'monthly'}, symbols=ETF5, start='2020-01-01'))
    d = d['data']
    codes = {w['code'] for w in d['warnings']}
    assert not codes & {'REBALANCE_TRADE_STATS', 'TRADE_LIST_INCOMPLETE'}
    assert len(d['trades']) == 5 and all(t['episode'] and t['exitReason'] == 'open' for t in d['trades'])
    assert sum(t['pnl'] for t in d['trades']) == pytest.approx(d['equity'][-1]['equity'] - 100000, abs=1.0)
    assert d['costs']['annualTurnover'] > 0 and d['rebalancing']['rebalances'] == d['rebalancing']['decisions'] == 60
    assert len(d['attribution']) == 5
    assert sum(a['contribution'] for a in d['attribution']) == pytest.approx(d['performance']['totalReturn'], abs=1e-5)
    assert sum(r['tradedNotional'] for r in d['costs']['byYear']) == pytest.approx(d['costs']['tradedNotional'])
    assert any('POSITION EPISODES' in n for n in d['assumptions']['notes'])


@pytest.mark.parametrize('case', ['mean_var', 'sma_short'])
def test_tt5_optimiser_trade_pnls_equal_the_reported_trades(case):
    r = req('mean_var', {}, symbols=ETF5, start='2020-01-01') if case == 'mean_var' else \
        req(start='2018-01-01', allowShort=True)
    _, _, res = run_inproc(r)
    eps = [t['pnl'] for t in E.episode_trades(res) if t['exitDate']]
    assert len(eps) > 10
    # closed_pnls lists them in exit order, the trade list in entry order (objectives ignore the order)
    assert np.sort(E.closed_pnls(res)) == pytest.approx(np.sort(np.array(eps)), rel=1e-9, abs=1e-6)


def test_cross_sectional_allocation_needs_two_symbols():
    """With one symbol these stacks would hold it 100 %: buy_and_hold's result under another name (T0.1 / T0.2)."""
    for sid in ('equal_weight', 'mean_var', 'min_var', 'risk_parity', 'inv_vol', 'momentum_topn'):
        env = call('run_backtest', req(sid, {}))
        assert not env['success'] and env['errorCode'] == 'INPUT' and 'at least 2' in env['error'], sid
    assert call('run_backtest', req('target_vol', {}, start='2020-01-01'))['success']     # meaningful on one symbol


# ── T-T6: catalogue specs ─────────────────────────────────────────────────────

def test_tt6_catalogue_specs_and_rebalance_choice():
    env = call('get_strategies', {})
    rows = env['data']
    # json_response camel-cases the counts (runnableHere; the screen reads runnableHere or runnable_here)
    assert env['total'] == len(rows) == 20 and env['runnableHere'] == 20 and env['supported'] == 20
    for row in rows:
        assert row['supported'] and row['runnable'] and row['reason'] is None and row['kind'] in ('signals',
                                                                                                    'allocation')
        for p in row['parameters']:
            assert {'id', 'name', 'type', 'default'} <= set(p)
            if p['type'] == 'choice':
                assert p['default'] in p['choices']
            else:
                assert p['type'] in ('int', 'float') and p['min'] <= p['default'] <= p['max'] and p['step'] > 0
    mom = {p['id']: p for p in ST.catalog_entry('momentum')['parameters']}
    assert mom['threshold']['type'] == 'float' and mom['threshold']['default'] == 0.02 and \
        mom['threshold']['step'] < 0.02
    counts = {}
    for per in ('weekly', 'monthly', 'quarterly', 14):
        d = call('run_backtest', req('equal_weight', {'rebalancePeriod': per}, symbols=ETF5, start='2020-01-01'))
        counts[per] = d['data']['rebalancing']['decisions']
    assert counts['weekly'] > counts['monthly'] == 60 > counts['quarterly'] == 20
    assert counts[14] == int(np.ceil(1258 / 14))


# ── T-T7: optimise / walk-forward through base.robustness ─────────────────────

def test_tt7_optimise_one_load_seeded_ints_all_rows(monkeypatch):
    loads = []
    real = E.load_data
    monkeypatch.setattr(E, 'load_data', lambda *a, **k: loads.append(1) or real(*a, **k))
    rng = {'fastPeriod': {'min': 5, 'max': 54, 'step': 1}, 'slowPeriod': {'min': 60, 'max': 150, 'step': 10}}
    o1 = call('optimize', req(start='2019-01-01', paramRanges=rng, optimizeMethod='random', maxIterations=40,
                              seed=3))['data']
    o2 = call('optimize', req(start='2019-01-01', paramRanges=rng, optimizeMethod='random', maxIterations=40,
                              seed=3))['data']
    assert loads == [1, 1]
    p1 = [r['parameters'] for r in o1['allResults']]
    assert len(p1) == 40 == o1['iterations'] and len({(p['fastPeriod'], p['slowPeriod']) for p in p1}) == 40
    assert sorted(map(str, p1)) == sorted(str(r['parameters']) for r in o2['allResults'])
    assert all(isinstance(p['fastPeriod'], int) and isinstance(p['slowPeriod'], int) for p in p1)
    assert all(r['performance'] and r['performance'].get('totalTrades') is not None for r in o1['allResults'])
    assert o1['totalCombinations'] == 500 and o1['seed'] == 3 and o1['overfitting']['nTrials'] == 40
    assert 'base.simulator' in o1['engineImplementation']


@pytest.mark.parametrize('sid,params,syms', [('sma_crossover', {'fastPeriod': 20, 'slowPeriod': 50}, ('SPY',)),
                                             ('inv_vol', {'lookback': 40}, ETF5)])
def test_tt7_single_combination_optimise_equals_run_backtest(sid, params, syms):
    rng = {k: {'min': v, 'max': v, 'step': 1} for k, v in params.items()}
    opt = call('optimize', req(sid, params, symbols=syms, start='2020-01-01', paramRanges=rng))['data']
    bt = call('run_backtest', req(sid, params, symbols=syms, start='2020-01-01'))['data']
    # run_backtest's equity rows are rounded to 4 dp by the normaliser; the optimiser scores unrounded returns
    assert opt['bestPerformance']['totalReturn'] == pytest.approx(bt['performance']['totalReturn'], rel=1e-6)
    if sid == 'inv_vol':
        assert 'bt library' in opt['engineImplementation'] and opt['provenance']['library'] == 'bt'


def test_tt7_walk_forward_optimises_each_window_and_choices():
    rng = {'fastPeriod': {'min': 10, 'max': 30, 'step': 10}, 'slowPeriod': {'min': 50, 'max': 100, 'step': 50}}
    wf = call('walk_forward', req(paramRanges=rng, wfSplits=3, wfTrainRatio=0.7, wfMode='anchored', purgeBars=5,
                                  embargoBars=5))['data']
    assert wf['mode'] == 'anchored' and wf['purgeBars'] == 5 and len(wf['windows']) == 3 and wf['oosEquity']
    assert all(w['bestParams'] and w['inSampleSharpe'] is not None for w in wf['windows'])
    assert 'WALK_FORWARD_NOT_OPTIMISED' not in {w['code'] for w in wf['warnings']}
    wa = call('walk_forward', req('equal_weight', {}, symbols=ETF5, start='2020-01-01', wfSplits=2,
                                  paramRanges={'rebalancePeriod': ['monthly', 'quarterly']}))['data']
    assert {w['bestParams']['rebalancePeriod'] for w in wa['windows']} <= {'monthly', 'quarterly'}
    o = call('optimize', req('equal_weight', {}, symbols=ETF5, start='2020-01-01', holdoutFraction=0.3,
                             paramRanges={'rebalancePeriod': ['weekly', 'monthly', 'quarterly']}))['data']
    assert o['iterations'] == 3 and 'holdout' in o


# ── T-T8: slippage itemised on rebalances; per-share commission with a minimum ─

def test_tt8_slippage_and_per_share_commission():
    for fill in ('next_open', 'next_close'):
        d = call('run_backtest', req('equal_weight', {}, symbols=ETF5, start='2020-01-01', fillTiming=fill))['data']
        c = d['costs']
        assert c['totalSlippage'] > 0 and c['totalSlippage'] == pytest.approx(sum(t['slippageCost'] for t in d['trades']))
        assert c['totalSlippage'] == pytest.approx(sum(f['slippageCost'] for f in d['fills']))
        assert c['totalCommission'] == pytest.approx(sum(f['commission'] for f in d['fills']))
        ib = call('run_backtest', req('equal_weight', {}, symbols=ETF5, start='2020-01-01', fillTiming=fill,
                                      costModel={'preset': 'us_equity_ibkr_fixed'}))['data']
        assert ib['costs']['model']['commissionType'] == 'per_share' and ib['costs']['model']['minCommission'] == 1.0
        for f in ib['fills']:
            assert f['commission'] >= min(1.0, 0.01 * f['quantity'] * f['price']) - 1e-9
            assert f['commission'] == pytest.approx(min(max(0.005 * f['quantity'], 1.0),
                                                        0.01 * f['quantity'] * f['price']))
        assert not any('bt charged' in n for n in ib['assumptions']['notes'])


# ── identity, capabilities, look-ahead, sensitivity ──────────────────────────

def test_identity_says_what_ran():
    s = call('run_backtest', req(start='2020-01-01'))['data']
    assert 'base.simulator' in s['engineImplementation'] and 'bt library is not used' in s['engineImplementation']
    assert s['provenance']['library'] is None and s['engine'] == 'bt'
    a = call('run_backtest', req('equal_weight', {}, symbols=ETF5, start='2020-01-01'))['data']
    assert 'algo stack decides' in a['engineImplementation'] and 'base.simulator executes' in a['engineImplementation']
    assert a['provenance']['library'] == 'bt' and a['provenance']['libraryVersion']
    n = call('run_backtest', req('equal_weight', {}, symbols=ETF5, start='2020-01-01', fillTiming='next_close'))['data']
    assert 'bt.Backtest executes' in n['engineImplementation'] and n['assumptions']['fillModel']['fillAt'] == 'next_close'
    sim = call('run_backtest', req('equal_weight', {}, symbols=ETF5, start='2020-01-01', fillTiming='next_close',
                                   btExecution='simulator'))['data']
    assert sim['provenance']['executedBy'] == 'base.simulator'
    assert abs(sim['statistics']['finalCapital'] / n['statistics']['finalCapital'] - 1) < 0.005


def test_capabilities_and_options():
    cap = call('get_capabilities', {})['data']
    assert cap['engine'] == 'bt' and cap['library'] == 'bt' and cap['installed'] and cap['multiSymbol']
    assert cap['fillTimings'] == ['next_open', 'next_close', 'same_close'] and 'us_equity_retail' in cap['costPresets']
    rk = cap['requestKeys']
    assert {'holdoutFraction', 'wfMode', 'purgeBars', 'embargoBars', 'seed', 'maxWorkers', 'costModel'} <= \
        set(rk['honoured'])
    assert 'leverage' in rk['ignored'] and cap['executors']['allocation']['next_close'] == 'bt.Backtest'
    assert 'next_close' in call('get_command_options', {})['data']['fillTimings']
    assert call('calculate_indicator', {'indicator': 'rsi', 'symbol': 'SPY', 'startDate': '2024-01-01',
                                        'endDate': '2024-12-31'})['success']
    assert call('calculate_indicator', {'indicator': 'nope', 'symbol': 'SPY'})['errorCode'] == 'INPUT'


def test_no_look_ahead():
    a = call('run_backtest', req(symbols=('SPY',)))['data']
    b = call('run_backtest', req(symbols=('SPYP',), benchmarkSymbol='SPYP'))['data']
    ea, eb = eq_series(a), eq_series(b)
    m = ea.index <= pd.Timestamp(CUT)
    assert np.array_equal(ea[m].values, eb[m].values)
    a = call('run_backtest', req('inv_vol', {'lookback': 40}, symbols=('SPY', 'QQQ', 'TLT'), start='2020-01-01'))['data']
    b = call('run_backtest', req('inv_vol', {'lookback': 40}, symbols=('SPYP', 'QQQ', 'TLT'), start='2020-01-01'))['data']
    ea, eb = eq_series(a), eq_series(b)
    m = ea.index <= pd.Timestamp(CUT)
    assert np.array_equal(ea[m].values, eb[m].values)


def test_cost_sensitivity_rows():
    for r in (req(start='2020-01-01'), req('equal_weight', {}, symbols=ETF5, start='2020-01-01',
                                           fillTiming='next_close')):
        d = call('run_backtest', r)['data']
        rows = d['costs']['sensitivity']
        assert [x['multiple'] for x in rows] == [0, 1, 2, 3]
        tr = [x['totalReturn'] for x in rows]
        assert tr[0] > tr[1] > tr[2] > tr[3] and tr[1] == pytest.approx(d['performance']['totalReturn'], rel=1e-9)


# ── T-T9 / T-T10: light signal path, progress + cancel ────────────────────────

def _provider(cmd, payload, extra_env=None, text=True):
    return subprocess.Popen([sys.executable, str(BT_DIR / 'bt_provider.py'), cmd, '--stdin'], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=text,
                            env=dict(os.environ, FINCEPT_BT_MAX_WORKERS='1', **(extra_env or {})))


def test_tt9_signal_strategies_do_not_import_the_bt_library():
    code = ('import sys, json; sys.path.insert(0, r"%s"); import bt_provider as P; '
            'r = P.dispatch("run_backtest", json.loads(sys.argv[1])); '
            'm = sys.modules.get("bt"); print(json.dumps([r["success"], bool(m is not None and hasattr(m, "algos")), '
            '"matplotlib" in sys.modules]))' % str(BT_DIR))
    out = subprocess.run([sys.executable, '-c', code, json.dumps(req(start='2023-01-01'))], capture_output=True,
                         text=True, env=dict(os.environ, FINCEPT_BT_PROGRESS='0'), timeout=120)
    assert json.loads(out.stdout.strip().splitlines()[-1]) == [True, False, False], out.stderr[-500:]


def test_tt10_cancel_inside_optimise_and_progress(tmp_path):
    cf = tmp_path / 'cancel.flag'
    payload = req('momentum_topn', {}, symbols=ETF5, start='2020-01-01', cancelFile=str(cf),
                  paramRanges={'lookback': {'min': 10, 'max': 259, 'step': 1}, 'topN': {'min': 1, 'max': 2, 'step': 1}})
    p = _provider('optimize', payload, {'FINCEPT_BT_PROGRESS': '1'})
    err_lines = []
    reader = threading.Thread(target=lambda: err_lines.extend(iter(p.stderr.readline, '')), daemon=True)
    reader.start()
    p.stdin.write(json.dumps(payload))
    p.stdin.close()
    t_end = time.monotonic() + 90
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


def test_deferred_ffn_imports_still_load_the_real_modules():
    """bt_data defers ffn's plotting / clustering imports (matplotlib.pyplot, sklearn.cluster / manifold) while bt
    is imported; afterwards sys.modules holds no stand-in and ffn's helpers reach the real modules."""
    import bt_data
    lib = bt_data.import_bt_library()
    assert lib is not None and hasattr(lib, 'algos')
    assert not any(isinstance(sys.modules.get(n), bt_data._LazyModule) for n in bt_data._LAZY)
    r = pd.DataFrame(np.random.default_rng(1).normal(0, 0.01, (300, 4)), columns=list('abcd'))
    assert sorted(sum(lib.ffn.calc_clusters(r, n=2, plot=False).values(), [])) == list('abcd')
    import ffn.core as fc
    assert fc.plt.get_backend()


def test_research_commands_served_by_the_shared_tools():
    """Extend, don't remove: the research commands v1 shipped for bt (they downloaded from Yahoo and returned
    shapes the screen hid) are served by the shared Fincept research tools on price-store bars, labelled truthfully."""
    base = {'symbols': ['SPY'], 'startDate': '2019-01-01', 'endDate': '2024-12-31', 'interval': '1d'}
    cases = [('generate_labels', {'labelType': 'FIXLB', 'params': {'horizon': 5, 'threshold': 0.02}}),
             ('labels_to_signals', {'labelType': 'FIXLB', 'entryLabel': 1, 'exitLabel': -1}),
             ('generate_splits', {'splitterType': 'RollingSplitter', 'nSplits': 3}),
             ('analyze_returns', {'riskFreeRate': 0.02}),
             ('generate_signals', {'generatorType': 'RAND', 'params': {'n': 5, 'seed': 1}}),
             ('indicator_signals', {'indicator': 'rsi', 'mode': 'threshold_signals', 'params': {'period': 14}})]
    for cmd, extra in cases:
        env = call(cmd, dict(base, **extra))
        assert env['success'], (cmd, env.get('error'))
        d = env['data']
        assert d['engine'] == 'bt' and 'bt library is not used' in d['engineImplementation'], cmd
        assert d['symbol'] == 'SPY'
    bad = call('indicator_signals', dict(base, indicator='no_such_indicator', mode='threshold_signals'))
    assert bad['success'] is False and bad['errorCode'] == 'INPUT'
    assert set(BP.RESEARCH_COMMANDS) <= set(BP.dispatch('get_capabilities', {})['data']['commands'])
