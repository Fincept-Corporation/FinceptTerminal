"""The "zipline" engine rebased on base/ (audit zipline part Z1-Z34, acceptance tests T-Z1..T-Z11), offline on the
CSV fixtures through a FakeYahoo-warmed price store (no network)."""
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
ZL_DIR = HERE.parent / 'zipline'
if str(ZL_DIR) not in sys.path:
    sys.path.insert(0, str(ZL_DIR))

import reference as ref  # noqa: E402
from base.base_provider import json_response  # noqa: E402
from base.contract import validate_envelope  # noqa: E402
from base.price_store import PriceStore  # noqa: E402
from helpers import FakeYahoo, yahoo_layout  # noqa: E402

import zipline_provider as P  # noqa: E402
import zl_api as A  # noqa: E402
import zl_engine as E  # noqa: E402
import zl_strategies as ST  # noqa: E402

ETF5 = ['SPY', 'QQQ', 'TLT', 'GLD', 'EFA']
CUT = '2022-06-30'


def _ohlc(o, c):
    return pd.DataFrame({'Open': o, 'High': np.maximum(o, c), 'Low': np.minimum(o, c), 'Close': c, 'Volume': 1e6},
                        index=c.index)


def _hourly():
    days = pd.bdate_range('2026-09-01', '2026-10-30')
    idx = pd.DatetimeIndex([d + pd.Timedelta(hours=9, minutes=30) + pd.Timedelta(hours=h) for d in days
                            for h in range(7)])
    rng = np.random.default_rng(11)
    c = 100.0 * np.cumprod(1.0 + rng.normal(0.0, 0.003, len(idx)))
    o = np.concatenate([[100.0], c[:-1]])
    return pd.DataFrame({'Open': o, 'High': np.maximum(o, c) * 1.001, 'Low': np.minimum(o, c) * 0.999, 'Close': c,
                         'Volume': 1e5}, index=idx)


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
    for s in ('BTC-USD', 'RELIANCE.NS', '^NSEI', '7203.T'):
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
    hourly = FakeYahoo({'HRLY': yahoo_layout(_hourly(), tz='America/New_York')})
    hst = PriceStore(fetcher=hourly, offline=False)
    hres = hst.get(['HRLY'], '2026-09-01', '2026-10-30', interval='1h', progress=False)
    os.environ['FINCEPT_BT_PROGRESS'] = '0'
    return st, hres


def call(cmd, r):
    env = json.loads(json_response(P.dispatch(cmd, r)))
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


def trade_dates(d):
    return [(t['entryDate'], t['exitDate']) for t in d['trades']]


# ── T-Z1: full-history indicators = the reference to 1e-9, same fills ────────

@pytest.mark.parametrize('kind,cost,fill', [(k, c, f) for k in ('bh', 'sma', 'rsi') for c in ('zero', 'costs')
                                            for f in ('next_open', 'next_close')] +
                         [('sma', 'costs', 'same_close'), ('rsi', 'costs', 'same_close')])
def test_tz1_reference_cases(spy, kind, cost, fill):
    import reference_harness as H
    comm, slip = H.COSTS[cost]
    sid = {'bh': 'buy_and_hold', 'sma': 'sma_crossover', 'rsi': 'rsi'}[kind]
    env = call('run_backtest', req(sid, H.DEFAULT_PARAMS[kind], comm=comm, slip=slip, fillTiming=fill))
    conv = {'fill': fill, 'shares': 'fractional', 'sma_rule': 'state', 'sma_gate': True, 'rsi': 'wilder',
            'rsi_rule': 'level', 'ew_when': 'first_bar_of_month', 'close_at_end': False}
    out = H.compare_case(env['data'], H.reference_case(kind, {'SPY': spy}, conv, comm, slip), kind, conv, cost)
    assert out['pass'], out
    # the residual is the normaliser's 4-dp rounding of the equity rows (<= 5e-10 at 100,000)
    assert out['final_rel_diff'] < 1e-9 and out['max_bar_rel_diff'] < 1e-9
    d = env['data']
    codes = {w['code'] for w in d['warnings']}
    assert not codes & {'TRADE_LIST_INCOMPLETE', 'SLIPPAGE_NOT_ITEMISED', 'REBALANCE_TRADE_STATS'}
    assert d['assumptions']['fillModel']['fillAt'] == fill


def _ema_ref(c: pd.Series, n: int) -> pd.Series:
    """Independent SMA-seeded EMA (alpha 2/(n+1)) by explicit recursion."""
    v = c.to_numpy(float)
    out = np.full(len(v), np.nan)
    out[n - 1] = v[:n].mean()
    a = 2.0 / (n + 1.0)
    for i in range(n, len(v)):
        out[i] = a * v[i] + (1 - a) * out[i - 1]
    return pd.Series(out, index=c.index)


@pytest.mark.parametrize('cost', ['zero', 'costs'])
def test_tz1_ema_crossover_full_history(spy, cost):
    comm, slip = (0.0, 0.0) if cost == 'zero' else (0.001, 0.0005)
    d = call('run_backtest', req('ema_crossover', {'fastPeriod': 10, 'slowPeriod': 20}, comm=comm, slip=slip))['data']
    f, s = _ema_ref(spy['Close'], 10), _ema_ref(spy['Close'], 20)
    above = (f > s) & s.notna()
    ent = above & ~above.shift(1, fill_value=False)
    exi = ~above & above.shift(1, fill_value=False) & s.notna()
    r = ref.simulate(spy, ent, exi, commission=comm, slippage=slip)
    e = eq_series(d)
    assert float(np.max(np.abs(e.values / r['equity'].values - 1))) < 1e-9
    tc = ref.compare_trades(d, r['trades'])
    assert tc['only_engine'] == [] and tc['only_ref'] == [] and tc['same'] >= 40


def test_tz1_indicators_are_full_history(spy):
    """The RSI an algorithm reads on bar t equals the classic full-history Wilder RSI on every bar (v1 re-seeded it
    on a 24-bar window each bar: 3.5 points off on average, audit Z2)."""
    bars = A.Bars(_panel(spy), {'SPY': spy}, ['SPY'])
    rsi = np.asarray(bars.line('rsi', (14,)).values[0])
    r = ref.rsi_wilder(spy['Close'], 14).to_numpy()
    ok = np.isfinite(r)
    assert np.array_equal(ok, np.isfinite(rsi)) and float(np.max(np.abs(rsi[ok] - r[ok]))) < 1e-9


def _panel(f):
    from base.execution import align_panel
    return align_panel({'SPY': f})


# ── T-Z2: cash never negative, buy & hold fully invested ──────────────────────

def test_tz2_cash_check_and_invested_fraction():
    for sid, params in (('sma_crossover', None), ('buy_and_hold', {}), ('rsi', {})):
        d = call('run_backtest', req(sid, params))['data']
        cash = np.array([p['cash'] for p in d['equity']])
        assert cash.min() >= -1e-6, sid
        if sid == 'buy_and_hold':
            first = d['trades'][0]['entryBar']
            expo = np.array([p['exposure'] for p in d['equity'][first:]])
            assert expo.min() >= 0.999


# ── T-Z3: risk-free rate, CAGR ────────────────────────────────────────────────

def test_tz3_sharpe_sortino_with_rf_and_cagr():
    for rf in (0.0, 0.04):
        d = call('run_backtest', req(riskFreeRate=rf, riskFreeSource='rate'))['data']
        m = ref.metrics(eq_series(d), rf=rf, ppy=d['periodsPerYear'])
        assert d['performance']['sharpeRatio'] == pytest.approx(m['sharpe'], rel=1e-6, abs=1e-6)
        assert d['performance']['sortinoRatio'] == pytest.approx(m['sortino'], rel=1e-6, abs=1e-6)
        assert d['performance']['annualizedReturn'] == pytest.approx(m['cagr'], rel=1e-6)
        assert d['performance']['cagr'] == pytest.approx(m['cagr'], rel=1e-6)


# ── T-Z4: exchange session dates, local benchmark ─────────────────────────────

def test_tz4_session_dates_non_us(cal):
    win = dict(start='2024-01-01', end='2024-06-28')
    d = call('run_backtest', req('buy_and_hold', {}, symbols=['RELIANCE.NS'], benchmarkSymbol='^NSEI', **win))['data']
    idx = pd.to_datetime([p['date'] for p in d['equity']])
    assert list(idx) == list(cal['RELIANCE.NS'].index)
    assert int((idx.dayofweek >= 5).sum()) == 0
    assert d['performance']['beta'] > 0.2
    t = call('run_backtest', req('sma_crossover', {'fastPeriod': 5, 'slowPeriod': 20}, symbols=['7203.T'],
                                 benchmarkSymbol='', **win))['data']
    idx = pd.to_datetime([p['date'] for p in t['equity']])
    assert list(idx) == list(cal['7203.T'].index) and int((idx.dayofweek >= 5).sum()) == 0
    for tr in t['trades']:
        assert pd.Timestamp(tr['entryDate']) in cal['7203.T'].index


# ── T-Z5: complete trade list, costs itemised ─────────────────────────────────

def test_tz5_trades_reconcile_and_costs_itemised():
    d = call('run_backtest', req())['data']
    pnl = sum(t['pnl'] for t in d['trades'])
    assert pnl == pytest.approx(d['equity'][-1]['equity'] - d['equity'][0]['equity'], abs=0.01)
    opens = [t for t in d['trades'] if t['exitDate'] is None]
    assert len(opens) == 1 and opens[0]['exitReason'] == 'open' and d['tradeStats']['openTrades'] == 1
    assert d['statistics']['totalFees'] == pytest.approx(sum(f['commission'] for f in d['fills']), rel=1e-12)
    assert d['statistics']['totalSlippage'] > 0
    assert d['costs']['totalSlippage'] == pytest.approx(sum(t['slippageCost'] for t in d['trades']))
    rows = d['costs']['sensitivity']
    tr = [x['totalReturn'] for x in rows]
    assert [x['multiple'] for x in rows] == [0, 1, 2, 3] and tr[0] > tr[1] > tr[2] > tr[3]
    assert tr[1] == pytest.approx(d['performance']['totalReturn'], rel=1e-9)
    assert sum(r['commission'] for r in d['costs']['byYear']) == pytest.approx(d['costs']['totalCommission'])


# ── T-Z6: catalogue specs survive the screen; defaults = intended values ──────

def _decimals(step):
    s = repr(float(step))
    return len(s.split('.')[1].rstrip('0')) if '.' in s else 0


def test_tz6_catalogue_specs_complete():
    env = call('get_strategies', {})
    rows = env['data']
    assert env['total'] == len(rows) == len(ST.STRATEGIES) == 24 and env['runnableHere'] == 24 \
        and env['supported'] == 24
    for row in rows:
        assert row['supported'] and row['runnable'] and row['reason'] is None
        assert row['kind'] in ('signals', 'allocation') and row['id'] == row['type']
        for p in row['parameters']:
            assert {'id', 'name', 'type', 'default', 'min', 'max', 'step'} <= set(p), (row['id'], p)
            if p['type'] == 'choice':
                assert p['default'] in p['choices']
                continue
            assert p['type'] in ('int', 'float') and p['min'] <= p['default'] <= p['max'] and p['step'] > 0
            # the screen builds a spin box with the spec's range and step decimals: the default survives
            assert round(p['default'], _decimals(p['step'])) == p['default'], (row['id'], p)
            if p['type'] == 'int':
                assert float(p['default']).is_integer()


@pytest.mark.parametrize('sid,intended', [('momentum', {'period': 12, 'threshold': 0.02}),
                                          ('williams_r', {'period': 14, 'oversold': -80, 'overbought': -20})])
def test_tz6_defaults_trade_like_the_intended_values(sid, intended):
    a = call('run_backtest', req(sid, {}))['data']
    b = call('run_backtest', req(sid, intended))['data']
    assert trade_dates(a) == trade_dates(b) and len(a['trades']) >= 30
    assert a['statistics']['finalCapital'] == b['statistics']['finalCapital']


# ── every catalogue algorithm runs and is itself (Z17, Z34) ───────────────────

def run_inproc(r):
    """(spec, SimResult, trades) of one run through the engine's own functions (no result normaliser)."""
    spec = E.parse_request(r)
    data = E.load_data(spec, r)
    panel = E.make_panel(data)
    res = E.simulate(spec, E.make_simulator(spec, panel), E.DecisionBook(panel, data.frames, data.symbols),
                     spec.params)
    return spec, res, (res.trades() if spec.kind == 'signals' else E.episode_trades(res))


def test_every_algorithm_runs_and_is_what_its_name_says():
    """Engine functions directly (every algorithm also runs through the provider in the catalogue run)."""
    finals = {}
    for sid, row in ST.STRATEGIES.items():
        syms = ETF5 if row['kind'] == 'allocation' else ('SPY',)
        spec, res, trades = run_inproc(req(sid, {}, symbols=syms, start='2018-01-01'))
        finals[sid] = float(res.equity[-1])
        assert trades and len(res.fill_arrays['bar']) > 0, sid
        pnl = sum(t['pnl'] for t in trades)
        assert pnl == pytest.approx(res.equity[-1] - 100000, abs=0.01), sid
        assert res.cash.min() >= -1e-6, sid
    vals = [round(v, 2) for v in finals.values()]
    assert len(set(vals)) == len(vals), finals
    assert finals['bollinger_bands'] != finals['mean_reversion'] and finals['macd'] != finals['macd_zero_cross']
    d = call('run_backtest', req('triple_ma', {}, start='2018-01-01'))['data']
    assert 'zipline-reloaded is not used' in d['engineImplementation'] and d['engine'] == 'zipline'


def test_rules_fire_on_their_own_levels(spy):
    d = call('run_backtest', req('rsi', {'period': 14, 'oversold': 30, 'overbought': 70}))['data']
    r = ref.rsi_wilder(spy['Close'], 14).to_numpy()
    closed = [t for t in d['trades'] if t['exitDate']]
    assert len(closed) >= 5
    for t in closed:                     # decided on the close before the fill (next_open)
        assert r[t['entryBar'] - 1] < 30 and r[t['exitBar'] - 1] > 70
    rm = call('run_backtest', req('rsi_macd', {'rsiLower': 45, 'rsiUpper': 55}))['data']
    rm2 = call('run_backtest', req('rsi_macd', {}))['data']
    assert trade_dates(rm) != trade_dates(rm2)          # the levels are parameters now (v1 hard-coded 40 / 60)


def test_allocation_algorithms_rank_and_rebalance():
    d = call('run_backtest', req('pipeline', {'topN': 2, 'rebalanceDays': 21, 'lookback': 63}, symbols=ETF5,
                                 start='2018-01-01'))['data']
    w = np.array([[x if x is not None else 0.0 for x in row] for row in d['weights']['w']])
    assert d['weights']['symbols'] == ETF5 and (np.count_nonzero(w > 0.01, axis=1) <= 2).all()
    assert d['algorithm']['executionMode'] == 'weights' and d['rebalancing']['decisions'] > 50
    assert any('POSITION EPISODES' in n for n in d['assumptions']['notes'])
    one = call('run_backtest', req('dual_momentum', {}, start='2016-01-01'))['data']
    assert 'TOPN_SELECTS_ALL' in {w['code'] for w in one['warnings']}
    five = call('run_backtest', req('dual_momentum', {'topN': 1}, symbols=ETF5, start='2016-01-01'))['data']
    held = {t['symbol'] for t in five['trades']}
    assert len(held) >= 2 and five['algorithm']['scheduled'] == ['first bar of each month']


# ── T-Z7: research (shared tools): signal modes, OHLC indicators ──────────────

def _atr_ref(h, lo, c, n):
    tr = np.empty(len(c))
    tr[0] = h[0] - lo[0]
    for i in range(1, len(c)):
        tr[i] = max(h[i] - lo[i], abs(h[i] - c[i - 1]), abs(lo[i] - c[i - 1]))
    out = np.full(len(c), np.nan)
    out[n - 1] = tr[:n].mean()
    for i in range(n, len(c)):
        out[i] = (out[i - 1] * (n - 1) + tr[i]) / n
    return out


def _wilder_loop(x, n, first):
    out = np.full(len(x), np.nan)
    out[first + n - 1] = np.mean(x[first:first + n])
    for i in range(first + n, len(x)):
        out[i] = (out[i - 1] * (n - 1) + x[i]) / n
    return out


def _adx_ref(h, lo, c, n):
    m = len(c)
    tr, pdm, mdm = np.zeros(m), np.zeros(m), np.zeros(m)
    for i in range(1, m):
        tr[i] = max(h[i] - lo[i], abs(h[i] - c[i - 1]), abs(lo[i] - c[i - 1]))
        up, dn = h[i] - h[i - 1], lo[i - 1] - lo[i]
        pdm[i] = up if (up > dn and up > 0) else 0.0
        mdm[i] = dn if (dn > up and dn > 0) else 0.0
    a, p, q = _wilder_loop(tr, n, 1), _wilder_loop(pdm, n, 1), _wilder_loop(mdm, n, 1)
    pdi, mdi = 100 * p / a, 100 * q / a
    dx = 100 * np.abs(pdi - mdi) / (pdi + mdi)
    return _wilder_loop(dx, n, n)


def test_tz7_indicator_signals_and_ohlc_indicators(spy):
    base = {'symbols': ['SPY'], 'startDate': '2015-01-01', 'endDate': '2024-12-31', 'interval': '1d'}
    pr = {'fast_period': 10, 'slow_period': 20, 'fast_indicator': 'ma', 'slow_indicator': 'ma', 'period': 14,
          'channel': 'donchian', 'z_entry': 2, 'z_exit': 0, 'base_indicator': 'rsi', 'base_period': 14,
          'filter_indicator': 'adx', 'filter_period': 14, 'filter_threshold': 20, 'filter_type': 'above'}
    for mode in ('crossover_signals', 'threshold_signals', 'breakout_signals', 'mean_reversion_signals',
                 'signal_filter'):
        env = call('indicator_signals', dict(base, indicator='rsi', mode=mode, params=dict(pr)))
        assert env['success'] and env['data']['entryCount'] > 0, (mode, env.get('error'))
        assert env['data']['engine'] == 'zipline'
    h, lo, c = (spy[x].to_numpy(float) for x in ('High', 'Low', 'Close'))
    atr = call('calculate_indicator', dict(base, indicator='atr', params={'period': 14}))['data']['stats']
    ra = _atr_ref(h, lo, c, 14)
    assert atr['last'] == pytest.approx(ra[-1], rel=1e-9) and atr['mean'] == pytest.approx(np.nanmean(ra), rel=1e-9)
    adx = call('calculate_indicator', dict(base, indicator='adx', params={'period': 14}))['data']['stats']['adx']
    rx = _adx_ref(h, lo, c, 14)
    assert adx['last'] == pytest.approx(rx[-1], rel=1e-9) and adx['mean'] == pytest.approx(np.nanmean(rx), rel=1e-9)
    # the algorithms read the same lines (one indicator table)
    bars = A.Bars(_panel(spy), {'SPY': spy}, ['SPY'])
    assert np.asarray(bars.line('atr', (14,)).values[0])[-1] == pytest.approx(ra[-1], rel=1e-12)


def test_research_commands_are_labelled_and_errors_are_input():
    base = {'symbols': ['SPY'], 'startDate': '2019-01-01', 'endDate': '2024-12-31', 'interval': '1d'}
    cases = [('generate_labels', {'labelType': 'FIXLB', 'params': {'horizon': 5, 'threshold': 0.02}}),
             ('labels_to_signals', {'labelType': 'FIXLB', 'entryLabel': 1, 'exitLabel': -1}),
             ('generate_splits', {'splitterType': 'PurgedKFoldSplitter', 'params': {'n_splits': 5}}),
             ('analyze_returns', {'riskFreeRate': 0.02, 'benchmarkSymbol': 'QQQ'}),
             ('generate_signals', {'generatorType': 'RAND', 'params': {'n': 5, 'seed': 1}})]
    for cmd, extra in cases:
        env = call(cmd, dict(base, **extra))
        assert env['success'], (cmd, env.get('error'))
        assert env['data']['engine'] == 'zipline' and 'zipline-reloaded is not used' in env['data']['engineImplementation']
    assert call('calculate_indicator', dict(base, indicator='nope'))['errorCode'] == 'INPUT'
    ind = call('get_indicators', {})['data']['indicators']
    ids = {i['id'] for cat in ind.values() for i in cat}
    assert {'sma', 'ema', 'rsi', 'macd', 'atr', 'adx', 'cci', 'williams_r', 'keltner', 'obv', 'vwap'} <= ids


# ── T-Z8: no look-ahead ───────────────────────────────────────────────────────

@pytest.mark.parametrize('sid,params', [('sma_crossover', None), ('rsi', {}), ('ema_crossover', {}),
                                        ('macd_adx', {}), ('scheduled_rebalance', {})])
def test_tz8_no_look_ahead(sid, params):
    a = call('run_backtest', req(sid, params))['data']
    b = call('run_backtest', req(sid, params, symbols=('SPYP',), benchmarkSymbol='SPYP'))['data']
    ea, eb = eq_series(a), eq_series(b)
    m = ea.index <= pd.Timestamp(CUT)
    assert np.array_equal(ea[m].values, eb[m].values)
    ta = [x for x in trade_dates(a) if x[1] and x[1] <= CUT]
    tb = [x for x in trade_dates(b) if x[1] and x[1] <= CUT]
    assert ta == tb and len(ta) > 0


# ── T-Z9: settings honoured or reported; interval; warnings ───────────────────

def test_tz9_sizing_short_stops_honoured_and_leverage_reported():
    base = req(start='2018-01-01')
    plain = call('run_backtest', base)['data']
    fixed = call('run_backtest', dict(base, positionSizing='fixed', positionSizeValue=10000))['data']
    assert all(abs(t['quantity'] * t['entryPrice'] - 10000) < 15 for t in fixed['trades'])
    short = call('run_backtest', dict(base, allowShort=True))['data']
    assert {t['side'] for t in short['trades']} == {'long', 'short'} and short['costs']['totalBorrow'] > 0
    stop = call('run_backtest', dict(base, stopLoss=0.03))['data']
    assert any(t['exitReason'] == 'stop_loss' for t in stop['trades'])
    for d in (fixed, short, stop):
        assert d['statistics']['finalCapital'] != plain['statistics']['finalCapital']
        assert not d['assumptions']['ignoredSettings']
    lev = call('run_backtest', dict(base, leverage=2, positionSizing='kelly'))['data']
    ign = ' '.join(lev['assumptions']['ignoredSettings'])
    assert 'leverage' in ign and 'kelly' in ign
    assert 'IGNORED_SETTING' in {w['code'] for w in lev['warnings']}
    alloc = call('run_backtest', req('pipeline', {}, symbols=ETF5, start='2018-01-01', allowShort=True,
                                     stopLoss=0.05))['data']
    assert any('short' in s for s in alloc['assumptions']['ignoredSettings'])


def test_tz9_interval_is_honoured():
    d = call('run_backtest', req('sma_crossover', {'fastPeriod': 5, 'slowPeriod': 20}, symbols=['HRLY'],
                                 benchmarkSymbol='', start='2026-09-01', end='2026-10-30', interval='1h'))['data']
    assert d['provenance']['interval'] == '1h' and len(d['equity']) > 250
    assert ':' in d['equity'][0]['date'] and d['periodsPerYear'] > 1000
    assert len(d['trades']) > 5


def test_tz9_degenerate_inputs_warn():
    short = call('run_backtest', req(start='2024-11-01'))['data']           # 40 bars < 2 x 50
    codes = {w['code'] for w in short['warnings']}
    assert 'SHORT_HISTORY' in codes and 'ZERO_TRADES' in codes
    assert short['performance']['winRate'] is None
    late = call('run_backtest', req(symbols=['RELIANCE.NS'], start='2023-06-01', end='2024-06-28',
                                    benchmarkSymbol=''))['data']
    assert 'START_AFTER_REQUEST' in {w['code'] for w in late['warnings']}
    inv = call('run_backtest', req(params={'fastPeriod': 50, 'slowPeriod': 20}))['data']
    assert 'PARAMETER_ORDER' in {w['code'] for w in inv['warnings']}


# ── T-Z10: optimise / walk-forward through base.robustness ────────────────────

def test_tz10_optimise_all_rows_seeded_one_load(monkeypatch):
    loads = []
    real = E.load_data
    monkeypatch.setattr(E, 'load_data', lambda *a, **k: loads.append(1) or real(*a, **k))
    rng = {'fastPeriod': {'min': 5, 'max': 54, 'step': 1}, 'slowPeriod': {'min': 60, 'max': 150, 'step': 10}}
    o1 = call('optimize', req(start='2019-01-01', paramRanges=rng, optimizeMethod='random', maxIterations=60,
                              seed=3))['data']
    o2 = call('optimize', req(start='2019-01-01', paramRanges=rng, optimizeMethod='random', maxIterations=60,
                              seed=3))['data']
    assert loads == [1, 1]
    p1 = [r['parameters'] for r in o1['allResults']]
    assert len(p1) == 60 == o1['iterations'] and len({(p['fastPeriod'], p['slowPeriod']) for p in p1}) == 60
    assert [r['objectiveValue'] for r in o1['allResults']] == [r['objectiveValue'] for r in o2['allResults']]
    assert all(isinstance(p['fastPeriod'], int) for p in p1)
    assert all(r['performance']['maxDrawdown'] is not None and r['performance']['totalTrades'] is not None
               for r in o1['allResults'])
    assert o1['totalCombinations'] == 500 and o1['seed'] == 3 and o1['overfitting']['nTrials'] == 60
    assert 'zipline-reloaded is not used' in o1['engineImplementation']


def test_tz10_single_combination_optimise_equals_run_backtest():
    params = {'fastPeriod': 20, 'slowPeriod': 50}
    rng = {k: {'min': v, 'max': v, 'step': 1} for k, v in params.items()}
    opt = call('optimize', req('sma_crossover', params, start='2020-01-01', paramRanges=rng))['data']
    bt = call('run_backtest', req('sma_crossover', params, start='2020-01-01'))['data']
    assert opt['bestPerformance']['totalReturn'] == pytest.approx(bt['performance']['totalReturn'], rel=1e-6)
    rng2 = {'topN': {'min': 2, 'max': 2, 'step': 1}}
    po = call('optimize', req('pipeline', {'topN': 2}, symbols=ETF5, start='2020-01-01', paramRanges=rng2))['data']
    pb = call('run_backtest', req('pipeline', {'topN': 2}, symbols=ETF5, start='2020-01-01'))['data']
    assert po['bestPerformance']['totalReturn'] == pytest.approx(pb['performance']['totalReturn'], rel=1e-6)


def test_tz10_walk_forward_optimised_and_covers_60_percent():
    rng = {'fastPeriod': {'min': 10, 'max': 30, 'step': 10}, 'slowPeriod': {'min': 50, 'max': 100, 'step': 50}}
    wf = call('walk_forward', req(paramRanges=rng, wfSplits=5, wfTrainRatio=0.7))['data']
    assert len(wf['windows']) == 5 and wf['oosCoverage'] >= 0.6 and wf['warmupBars'] >= 100
    assert all(w['bestParams'] and w['inSampleSharpe'] is not None for w in wf['windows'])
    assert 'WALK_FORWARD_NOT_OPTIMISED' not in {w['code'] for w in wf['warnings']}
    an = call('walk_forward', req(paramRanges=rng, wfSplits=3, wfMode='anchored', purgeBars=5, embargoBars=5,
                                  holdoutFraction=0.2))['data']
    assert an['mode'] == 'anchored' and an['purgeBars'] == 5 and an['embargoBars'] == 5
    o = call('optimize', req(paramRanges=rng, holdoutFraction=0.3))['data']
    assert 'holdout' in o
    wa = call('walk_forward', req('dual_momentum', {}, symbols=ETF5, start='2016-01-01', wfSplits=2,
                                  paramRanges={'rebalancePeriod': ['monthly', 'quarterly']}))['data']
    assert {w['bestParams']['rebalancePeriod'] for w in wa['windows']} <= {'monthly', 'quarterly'}


# ── identity, capabilities ────────────────────────────────────────────────────

def test_identity_capabilities_and_no_zipline_import():
    d = call('run_backtest', req(start='2020-01-01'))['data']
    assert d['engine'] == 'zipline' and d['provenance']['library'] is None
    assert 'zipline-style algorithm API' in d['engineImplementation'] and 'base.simulator' in d['engineImplementation']
    assert d['algorithm']['ziplineReloaded']['used'] is False and d['provenance']['executedBy'] == 'base.simulator'
    assert d['provenance']['symbols'][0]['bars'] > 0 and d['provenance']['symbols'][0]['firstBar'] == '2020-01-02'
    assert not any(m == 'zipline.api' or m.startswith('zipline.finance') for m in sys.modules)
    cap = call('get_capabilities', {})['data']
    assert cap['engine'] == 'zipline' and cap['library'] is None and cap['multiSymbol'] and cap['stops']
    assert cap['fillTimings'] == ['next_open', 'next_close', 'same_close'] and 'us_equity_retail' in cap['costPresets']
    rk = cap['requestKeys']
    assert {'holdoutFraction', 'wfMode', 'purgeBars', 'embargoBars', 'seed', 'maxWorkers', 'costModel',
            'allowShort', 'positionSizing'} <= set(rk['honoured']) and rk['ignored'] == ['leverage']
    assert set(P.RESEARCH_COMMANDS) <= set(cap['commands'])
    assert 'fill_timings' in call('get_command_options', {})['data'] or \
        'fillTimings' in call('get_command_options', {})['data']


def test_cost_model_preset_per_share():
    d = call('run_backtest', req(start='2020-01-01', costModel={'preset': 'us_equity_ibkr_fixed'}))['data']
    assert d['costs']['model']['commissionType'] == 'per_share'
    for f in d['fills']:
        assert f['commission'] == pytest.approx(min(max(0.005 * f['quantity'], 1.0), 0.01 * f['quantity'] * f['price']))


# ── T-Z11 / progress + cancel (subprocess, like the app) ──────────────────────

def test_cancel_inside_optimise_and_progress(tmp_path):
    cf = tmp_path / 'cancel.flag'
    payload = req(cancelFile=str(cf), paramRanges={'fastPeriod': {'min': 2, 'max': 60, 'step': 1},
                                                   'slowPeriod': {'min': 61, 'max': 300, 'step': 1}},
                  maxIterations=5000)
    p = subprocess.Popen([sys.executable, str(ZL_DIR / 'zipline_provider.py'), 'optimize', '--stdin'],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                         env=dict(os.environ, FINCEPT_BT_MAX_WORKERS='1', FINCEPT_BT_PROGRESS='1'))
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
