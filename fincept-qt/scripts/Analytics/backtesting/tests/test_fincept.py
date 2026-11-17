"""The "fincept" engine (Fincept's QCAlgorithm-compatible bar-loop engine) on the shared core: price-store data
hooks, engine identity, fills / costs / borrow, reference agreement, optimise / walk-forward through
base.robustness, catalogue, cancel. Offline on the CSV fixtures through a FakeYahoo-warmed price store."""
import json
import math
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
FE_DIR = HERE.parent / 'fincept'
SCRIPTS = HERE.parents[2]
for _p in (str(FE_DIR), str(HERE.parent / 'base')):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from base.base_provider import json_response  # noqa: E402
from base.contract import validate_envelope  # noqa: E402
from base.price_store import PriceStore  # noqa: E402
from helpers import FakeYahoo, raw_to_yahoo, yahoo_layout  # noqa: E402

import fe_catalog as CAT  # noqa: E402
import fe_engine as E  # noqa: E402
import fincept_provider as P  # noqa: E402
import fincept_strategy_runner  # noqa: E402,F401  (puts scripts/strategies on sys.path)
from fincept_engine import QCAlgorithm, price_feed, yahoo_cache  # noqa: E402

ETF5 = ['SPY', 'QQQ', 'TLT', 'GLD', 'EFA']
BH, EMA, BAND = 'FCT-1ADBAC8E', 'FCT-B8F7654B', 'FCT-3D4D4FC7'


@pytest.fixture(scope='module', autouse=True)
def warm(spy, etf5, irx, aapl_raw):
    opens, closes = etf5
    frames = {'SPY': yahoo_layout(spy)}
    for s in ETF5[1:]:
        o, c = opens[s], closes[s]
        frames[s] = yahoo_layout(pd.DataFrame({'Open': o, 'High': np.maximum(o, c), 'Low': np.minimum(o, c),
                                               'Close': c, 'Volume': 1e6}, index=c.index))
    meta = spy * 0.5
    frames['META'] = yahoo_layout(meta)
    frames['^IRX'] = yahoo_layout(pd.DataFrame({'Open': irx, 'High': irx, 'Low': irx, 'Close': irx, 'Volume': 0.0}),
                                  tz='America/Chicago')
    frames['AAPL'] = raw_to_yahoo(aapl_raw)
    st = PriceStore(fetcher=FakeYahoo(frames), offline=False)
    res = st.get(list(frames), '2010-01-01', '2024-12-31', progress=False)
    assert not res.errors, res.errors
    price_feed.set_default_store(None)          # a fresh memo over the (offline) default store
    os.environ['FINCEPT_BT_PROGRESS'] = '0'
    return frames


def call(cmd, r):
    env = json.loads(json_response(P.dispatch(cmd, r)))
    if cmd in ('run_backtest', 'optimize', 'walk_forward', 'monte_carlo') and env.get('success'):
        assert validate_envelope(env, cmd) == [], validate_envelope(env, cmd)[:5]
    return env


def req(sid=BH, symbols=('SPY',), start='2015-01-01', end='2024-12-31', comm=0.001, slip=0.0005, params=None, **kw):
    r = {'symbols': list(symbols), 'startDate': start, 'endDate': end, 'interval': '1d', 'initialCapital': 100000,
         'commission': comm, 'slippage': slip, 'positionSizing': 'percent', 'positionSizeValue': 100,
         'allowShort': False, 'benchmarkSymbol': 'SPY', 'riskFreeRate': 0.0,
         'strategy': {'type': sid, 'name': sid, 'params': dict(params or {})}}
    r.update(kw)
    return r


def eq_series(d):
    return pd.Series([p['equity'] for p in d['equity']], index=pd.to_datetime([p['date'] for p in d['equity']]))


@pytest.fixture()
def custom(monkeypatch):
    """Register QCAlgorithm subclasses under test ids (the provider resolves strategies through the runner)."""
    r = E.runner()
    classes = {}
    orig_load, orig_info = r.load_strategy_class, r.get_strategy_info
    monkeypatch.setattr(r, 'load_strategy_class', lambda sid: classes[sid] if sid in classes else orig_load(sid))
    monkeypatch.setattr(r, 'get_strategy_info', lambda sid: {'name': classes[sid].__name__, 'path': 'test.py',
                                                             'category': 'test'} if sid in classes else orig_info(sid))
    return classes


# ── independent reference (LEAN set_holdings sizing, next open / close fills) ─────────────────────

def _ema(x, n):
    k, e, out = 2.0 / (n + 1), None, np.empty(len(x))
    for i, v in enumerate(x):
        e = v if e is None else v * k + e * (1 - k)
        out[i] = e
    return out


def reference(df, kind, comm, slip, fill='next_open', cash0=100000.0):
    o, c = df['Open'].to_numpy(float), df['Close'].to_numpy(float)
    f, s, need = {'ema': (_ema(c, 15), _ema(c, 30), 30), 'band': (_ema(c, 10), _ema(c, 20), 20)}.get(kind, (None, None, 0))
    cash, qty, pending, eq, fills = cash0, 0.0, None, np.empty(len(c)), []

    def fill_at(i, q, raw):
        nonlocal cash, qty
        px = raw * (1 + slip) if q > 0 else raw * (1 - slip)
        cash -= q * px + abs(q) * px * comm
        qty += q
        fills.append((df.index[i], q, px))

    for i in range(len(c)):
        if fill != 'same_close' and pending:
            fill_at(i, pending, o[i] if fill == 'next_open' else c[i])
        pending = None
        tpv = cash + qty * c[i]
        size = math.trunc(tpv * 0.9975 / (c[i] * (1 + comm + slip)) - qty)
        q = None
        if kind == 'bh':
            q = size if qty == 0 else None
        elif i + 1 >= need and kind == 'ema':
            q = size if (qty <= 0 and f[i] > s[i] * 1.00015) else (-qty if qty > 0 and f[i] < s[i] else None)
        elif i + 1 >= need:
            q = (size or None) if f[i] > s[i] * 1.001 else (-qty if f[i] < s[i] * 0.999 and qty else None)
        if q:
            if fill == 'same_close':
                fill_at(i, q, c[i])
            else:
                pending = q
        eq[i] = cash + qty * c[i]
    return pd.Series(eq, index=df.index), fills


@pytest.mark.parametrize('kind,sid,fill', [('bh', BH, 'next_open'), ('ema', EMA, 'next_open'), ('ema', EMA, 'next_close'),
                                           ('ema', EMA, 'same_close'), ('band', BAND, 'next_open')])
def test_engine_equals_independent_reference(spy, kind, sid, fill):
    env = call('run_backtest', req(sid, fillTiming=fill))
    assert env['success'], env
    d = env['data']
    e = eq_series(d)
    r_eq, fills = reference(spy.loc[e.index[0]:e.index[-1]], kind, 0.001, 0.0005, fill)
    assert float((e / r_eq.reindex(e.index) - 1).abs().max()) < 1e-9     # 4-dp equity rounding only
    got = [(f['date'], (1 if f['side'] == 'buy' else -1) * f['quantity'], f['price']) for f in d['fills']]
    assert len(got) == len(fills) > 0
    for a, b in zip(got, fills):
        assert a[0] == str(b[0].date()) and a[1] == pytest.approx(b[1]) and a[2] == pytest.approx(b[2], rel=1e-12)
    assert d['assumptions']['fillModel']['fillAt'] == fill
    codes = {w['code'] for w in d['warnings']}
    assert 'TRADE_LIST_INCOMPLETE' not in codes and ('SAME_BAR_FILL' in codes) == (fill == 'same_close')


def test_identity_provenance_costs_and_statistics():
    a = call('run_backtest', req(EMA, start='2019-01-01', end='2021-12-31'))['data']
    b = call('run_backtest', req(EMA, start='2019-01-01', end='2021-12-31'))['data']
    assert a['engine'] == 'fincept' and a['engineImplementation'] == E.IMPLEMENTATION
    assert 'base.simulator is not used' in a['engineImplementation']
    assert a['provenance']['engine'] == 'fincept' and a['provenance']['engineVersion'] == E.ENGINE_VERSION
    sym = a['provenance']['symbols'][0]
    assert sym['symbol'] == 'SPY' and sym['bars'] == len(a['equity']) and sym['firstBar'] == a['equity'][0]['date']
    assert sym['lastBar'] == a['equity'][-1]['date'] and sym['loadedBy'] == 'price_store'
    assert a['provenance']['priceSeries'][0]['adjustment'] == 'split+dividend'
    assert a['id'] != b['id'] and a['id'] != EMA                            # F4: run id unique
    st = a['statistics']
    assert st['winningDays'] > 0 and st['losingDays'] > 0 and st['bestDay'] > 0 > st['worstDay']   # F4: real
    c = a['costs']
    assert c['totalCommission'] > 0 and c['totalSlippage'] > 0 and c['itemised']['slippage']
    assert c['totalCommission'] == pytest.approx(sum(f['commission'] for f in a['fills']))
    assert c['totalSlippage'] == pytest.approx(sum(f['slippageCost'] for f in a['fills']))
    pnl = sum(t['pnl'] for t in a['trades'])
    assert pnl == pytest.approx(a['equity'][-1]['equity'] - 100000, abs=0.01)
    assert 'sensitivity' not in c and 'not computed' in c['sensitivityNote']


def test_cost_model_per_share_minimum_and_ignored_parts():
    d = call('run_backtest', req(EMA, start='2019-01-01', end='2021-12-31',
                                 costModel={'preset': 'us_equity_ibkr_fixed', 'impactCoef': 0.1}))['data']
    assert d['costs']['model']['commissionType'] == 'per_share'
    for f in d['fills']:
        assert f['commission'] == pytest.approx(min(max(0.005 * f['quantity'], 1.0), 0.01 * f['quantity'] * f['price']))
        assert f['price'] / f['rawPrice'] - 1 == pytest.approx((0.0002 + 0.0001) * (1 if f['side'] == 'buy' else -1))
    assert any('impactCoef' in s for s in d['assumptions']['ignoredSettings'])
    assert any(w['code'] == 'IGNORED_SETTING' for w in d['warnings'])


class _ShortHold(QCAlgorithm):
    def initialize(self):
        self.add_equity('SPY')
        self.n = 0

    def on_data(self, data):
        self.n += 1
        if self.n == 1:
            self.set_holdings('SPY', -0.5)
        elif self.n == 60:
            self.liquidate('SPY')


def test_borrow_fee_charged_and_trades_reconcile(custom, spy):
    custom['TEST-SHORT'] = _ShortHold
    d = call('run_backtest', req('TEST-SHORT', start='2019-01-01', end='2019-12-31'))['data']
    (t,) = d['trades']
    assert t['side'] == 'short'
    q = t['quantity']
    idx = spy.index
    i0, i1 = idx.get_loc(pd.Timestamp(t['entryDate'])), idx.get_loc(pd.Timestamp(t['exitDate']))
    # ACT/360 at 0.25 % a year: at every close the position is short, its value x the days since the previous close
    exp = sum(q * spy['Close'].iloc[k] * 0.0025 * (idx[k] - idx[k - 1]).days / 360.0 for k in range(i0, i1))
    assert d['costs']['totalBorrow'] == pytest.approx(exp, rel=1e-9)
    assert t['borrowCost'] == pytest.approx(exp, rel=1e-9)
    assert t['pnl'] == pytest.approx(d['equity'][-1]['equity'] - 100000, abs=0.01)
    assert not any(w['code'] in ('TRADE_LIST_INCOMPLETE', 'BORROW_NOT_MODELLED') for w in d['warnings'])
    assert any('allowShort' in s for s in d['assumptions']['ignoredSettings'])


class _Renamed(QCAlgorithm):
    def initialize(self):
        self.add_equity('FB')

    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings('FB', 1)


class _Pair(QCAlgorithm):
    def initialize(self):
        self.add_equity('SPY')
        self.add_equity('QQQ')

    def on_data(self, data):
        if not self.portfolio.invested:
            self.set_holdings('SPY', 0.5)
            self.set_holdings('QQQ', 0.5)


class _MorningBuy(QCAlgorithm):
    def initialize(self):
        self.add_equity('SPY')
        self.schedule.on(self.date_rules.every_day('SPY'), self.time_rules.after_market_open('SPY', 1), self.buy)

    def buy(self):
        if not self.portfolio.invested:
            self.market_order('SPY', 100)


def test_first_bar_fill_counts_from_the_starting_capital(custom, spy):
    custom['TEST-MORNING'] = _MorningBuy
    d = call('run_backtest', req('TEST-MORNING', start='2019-01-02', end='2019-03-29'))['data']
    assert d['fills'][0]['date'] == '2019-01-02'                    # a morning event fills at the first open
    assert d['equity'][0] == {**d['equity'][0], 'date': '2018-12-31', 'equity': 100000.0}
    assert sum(t['pnl'] for t in d['trades']) == pytest.approx(d['equity'][-1]['equity'] - 100000, abs=0.01)
    assert not any(w['code'] == 'TRADE_LIST_INCOMPLETE' for w in d['warnings'])
    assert d['performance']['totalReturn'] == pytest.approx(d['equity'][-1]['equity'] / 100000 - 1, abs=1e-9)


def test_ticker_substitution_reported(custom):
    custom['TEST-FB'] = _Renamed
    d = call('run_backtest', req('TEST-FB', symbols=('FB',), start='2019-01-01', end='2019-06-30'))['data']
    subs = d['provenance']['substitutions']
    assert subs == [{'requested': 'FB', 'used': 'META', 'kind': 'substituted', 'reason': subs[0]['reason']}]
    assert 'META' in subs[0]['reason'] and d['provenance']['symbolMap']['FB'] == 'META'
    w = [x for x in d['warnings'] if x['code'] == 'TICKER_SUBSTITUTED']
    assert w and w[0]['severity'] == 'info' and w[0]['symbol'] == 'FB'


def test_weights_for_multi_asset_runs(custom):
    custom['TEST-PAIR'] = _Pair
    d = call('run_backtest', req('TEST-PAIR', symbols=('SPY', 'QQQ'), start='2021-01-01', end='2021-12-31'))['data']
    w = d['weights']
    assert sorted(w['symbols']) == ['QQQ', 'SPY'] and len(w['t']) == len(w['w'])
    mid = w['w'][len(w['w']) // 2]
    assert all(0.4 < x < 0.6 for x in mid) and 0.98 < sum(mid) <= 1.0


def test_price_feed_formats_memo_and_actions(tmp_path, spy, aapl_raw):
    fake = FakeYahoo({'SPY': yahoo_layout(spy), 'AAPL': raw_to_yahoo(aapl_raw)})
    feed = price_feed.PriceFeed(PriceStore(root=tmp_path, fetcher=fake, offline=False))
    rows = feed.daily_dicts(['SPY'], '2019-01-01', '2019-02-01', 'adj')['SPY']
    assert rows[0]['time'] == '2019-01-02 00:00:00' and rows[-1]['time'] == '2019-01-31 00:00:00'   # end EXCLUSIVE
    assert rows[0]['close'] == pytest.approx(spy.loc['2019-01-02', 'Close'])
    n_calls = len(fake.calls)
    feed.daily_dicts(['SPY'], '2019-01-07', '2019-01-15', 'adj')
    assert len(fake.calls) == n_calls                       # served from the memo
    a = feed.actions('AAPL', '2020-01-01', None)
    assert a['splits'] == [['2020-08-31', 4.0]]
    for (d1, split_basis), (d2, raw) in zip(a['dividends'], a['dividends_raw']):
        assert d1 == d2 and split_basis == pytest.approx(raw / (4.0 if d1 < '2020-08-31' else 1.0))
    raw_rows = feed.daily_dicts(['AAPL'], '2020-08-25', '2020-09-03', 'raw')['AAPL']
    assert all('adj_close' in r for r in raw_rows)
    b = feed.bars(['AAPL'], '2020-07-01', '2020-08-01', '1d', 'raw')['AAPL']
    assert b['splits'] == [['2020-08-31', 4.0]]          # later splits listed: raw prices can be rebuilt
    assert len(b['bars'][0]) == 7


def test_yahoo_cache_is_an_adapter_and_caches_under_cache_dir(monkeypatch, tmp_path):
    from base import paths
    fp = paths._fincept_paths()
    assert yahoo_cache.cache_root() == fp.cache_dir('strategy_engine')
    got = yahoo_cache.fetch_daily_dicts(['SPY'], '2019-01-01', '2019-01-10')
    assert [r['time'][:10] for r in got['SPY']] == ['2019-01-02', '2019-01-03', '2019-01-04', '2019-01-07',
                                                   '2019-01-08', '2019-01-09']
    root = fp.data_root()
    assert not list(root.glob('strategy_engine/yahoo_*')) and not list(root.glob('cache/scripts/strategy_engine/yahoo_*'))
    from fincept_engine import universe_data
    assert universe_data._default_fetch_splits(['AAPL', 'SPY'], '2020-01-01', '2021-01-01') == {'AAPL': [], 'SPY': []}


def test_round_trips_multiplier_expiry_carry_and_splits():
    fills = [
        {'time': '2022-01-03 00:00:00', 'symbol': 'OPT', 'quantity': 10, 'price': 2.0, 'commission': 2.0,
         'slippage': 1.0, 'multiplier': 100.0, 'type': 'MARKET'},
        {'time': '2022-01-21 00:00:00', 'symbol': 'OPT', 'quantity': -10, 'price': 0.0, 'commission': 0.0,
         'slippage': 0.0, 'multiplier': 100.0, 'type': 'OPTION_EXERCISE'},          # expired worthless
        {'time': '2022-01-03 00:00:00', 'symbol': 'XYZ', 'quantity': -100, 'price': 50.0, 'commission': 5.0,
         'slippage': 0.0, 'type': 'MARKET'},
        {'time': '2022-02-01 00:00:00', 'symbol': 'XYZ', 'quantity': 100, 'price': 45.0, 'commission': 4.5,
         'slippage': 0.0, 'borrow': 3.0, 'type': 'STOP_MARKET'},
        {'time': '2022-01-03 00:00:00', 'symbol': 'RAW', 'quantity': 10, 'price': 400.0, 'commission': 0.0,
         'slippage': 0.0, 'type': 'MARKET'},
        {'time': '2022-03-01 00:00:00', 'symbol': 'RAW', 'quantity': -40, 'price': 110.0, 'commission': 0.0,
         'slippage': 0.0, 'dividends': 8.0, 'type': 'MARKET'},
    ]
    events = [{'time': '2022-02-01 00:00:00', 'symbol': 'RAW', 'split': 4.0}]
    t = {x['symbol']: x for x in E.round_trips(fills, corporate_events=events)}
    assert t['OPT']['quantity'] == 1000 and t['OPT']['pnl'] == pytest.approx(-2000 - 2.0)
    assert t['OPT']['contracts'] == 10 and t['OPT']['exitPrice'] == 0.0
    assert t['XYZ']['side'] == 'short' and t['XYZ']['pnl'] == pytest.approx(500 - 9.5 - 3.0)
    assert t['XYZ']['exitReason'] == 'stop_loss' and t['XYZ']['borrowCost'] == 3.0
    assert t['RAW']['quantity'] == 40 and t['RAW']['entryPrice'] == pytest.approx(100.0)
    assert t['RAW']['pnl'] == pytest.approx(40 * 10.0 + 8.0)                     # split-adjusted basis + dividends


def test_get_parameter_and_param_text():
    from fincept_strategy_runner import _param_text
    a = QCAlgorithm()
    a.set_parameters({'n': _param_text(20.0), 'x': _param_text(0.5)})
    assert a.get_parameter('n', 5) == 20 and a.get_parameter('x', 1.0) == 0.5
    a.set_parameters({'n': '30.0'})
    assert a.get_parameter('n', 5) == 30


def test_optimise_parameterised_strategy_and_refuse_parameterless():
    rng = {'ema-fast': {'min': 8, 'max': 12, 'step': 4}, 'ema-slow': {'min': 20, 'max': 30, 'step': 10}}
    env = call('optimize', req(BAND, start='2018-01-01', end='2021-12-31', paramRanges=rng, optimizeObjective='sharpe'))
    assert env['success'], env
    d = env['data']
    assert d['engine'] == 'fincept' and 'base.robustness' in d['engineImplementation']
    assert d['iterations'] == 4 and len(d['allResults']) == 4 and d['overfitting']['nTrials'] == 4
    assert {tuple(sorted(r['parameters'].items())) for r in d['allResults']} == {
        (('ema-fast', f), ('ema-slow', s)) for f in (8, 12) for s in (20, 30)}
    one = call('optimize', req(BAND, start='2018-01-01', end='2021-12-31', paramRanges={'ema-fast': [12],
                                                                                         'ema-slow': [30]}))['data']
    run = call('run_backtest', req(BAND, start='2018-01-01', end='2021-12-31',
                                   params={'ema-fast': 12, 'ema-slow': 30}))['data']
    # base.robustness keeps trial returns as float32: agreement to ~1e-7
    assert one['allResults'][0]['performance']['totalReturn'] == pytest.approx(run['performance']['totalReturn'],
                                                                                rel=1e-6)
    bad = call('optimize', req(EMA, paramRanges={'fast': {'min': 10, 'max': 20, 'step': 5}}))
    assert not bad['success'] and bad['errorCode'] == 'INPUT' and 'get_parameter' in bad['error']
    unknown = call('optimize', req(BAND, paramRanges={'nope': [1, 2]}))
    assert not unknown['success'] and unknown['errorCode'] == 'INPUT' and 'ema-fast' in unknown['error']


def test_walk_forward_reoptimises_every_window():
    rng = {'ema-fast': {'min': 8, 'max': 12, 'step': 4}, 'ema-slow': {'min': 20, 'max': 30, 'step': 10}}
    env = call('walk_forward', req(BAND, start='2016-01-01', end='2024-12-31', paramRanges=rng, wfSplits=2,
                                   wfTrainRatio=0.6, wfMode='anchored', purgeBars=3, embargoBars=2))
    assert env['success'], env
    d = env['data']
    assert d['mode'] == 'anchored' and d['nWindows'] == 2 and d['purgeBars'] == 3
    assert all(w['bestParams'] and w['inSampleSharpe'] is not None for w in d['windows'])
    assert not any(w['code'] == 'WALK_FORWARD_NOT_OPTIMISED' for w in d['warnings'])
    w1, w2 = d['windows']
    assert pd.Timestamp(w2['testStart']) > pd.Timestamp(w1['testEnd'])
    assert (pd.Timestamp(w2['testStart']) - pd.Timestamp(w1['testEnd'])).days <= 4      # test windows tile
    assert len(d['oosEquity']) == w1['testBars'] + w2['testBars']


def test_catalogue_rows_counts_and_capabilities():
    data = call('get_strategies', {'includeUnsupported': True})['data']
    rows = [r for c in data['strategies'].values() for r in c]
    assert data['total'] == len(rows) and data['runnableHere'] == sum(r['runnable'] for r in rows)
    for r in rows:
        assert isinstance(r['supported'], bool) and isinstance(r['runnable'], bool)
        assert (r['reason'] is not None) == (not r['runnable'])
        assert isinstance(r['optimizable'], bool) and (r['optimizableReason'] is None) == r['optimizable']
    band = next(r for r in rows if r['id'] == BAND)
    assert band['optimizable'] and [p['id'] for p in band['params']] == ['ema-fast', 'ema-slow']
    fast = band['params'][0]
    assert fast['type'] == 'int' and fast['default'] == 10 and fast['min'] <= 10 <= fast['max'] and fast['step'] >= 1
    assert band['constraints'] == [{'lt': ['ema-fast', 'ema-slow'], 'message': 'ema-fast must be smaller than ema-slow'}]
    ema = next(r for r in rows if r['id'] == EMA)
    assert not ema['optimizable'] and 'get_parameter' in ema['optimizableReason']
    caps = call('get_capabilities', {})['data']
    assert caps['engine'] == 'fincept' and caps['fillTimings'] == ['next_open', 'next_close', 'same_close']
    assert 'costModel' in caps['requestKeys']['honoured'] and 'positionSizing' in caps['requestKeys']['ignored']
    assert 'borrow cost' not in caps['notModelled'] and 'market impact' in caps['notModelled']


def test_monte_carlo_carries_identity():
    env = call('monte_carlo', req(EMA, start='2019-01-01', end='2021-12-31', mcSimulations=100))
    assert env['success'] and env['data']['engine'] == 'fincept'
    assert env['data']['engineImplementation'] == E.IMPLEMENTATION


def test_data_and_input_errors():
    e1 = call('run_backtest', req('FCT-NOPE'))
    assert e1['errorCode'] == 'INPUT'
    e2 = call('run_backtest', req(BH, symbols=('QQQ',), start='2019-01-01', end='2019-03-01'))
    assert e2['errorCode'] == 'INPUT' and 'Enter SPY as the symbol' in e2['error']


def _provider(cmd, payload, env_extra=None):
    p = subprocess.Popen([sys.executable, str(FE_DIR / 'fincept_provider.py'), cmd, '--stdin'], stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                         env=dict(os.environ, FINCEPT_BT_MAX_WORKERS='1', FINCEPT_BT_PROGRESS='1', **(env_extra or {})))
    err = []
    t = threading.Thread(target=lambda: err.extend(iter(p.stderr.readline, '')), daemon=True)
    t.start()
    p.stdin.write(json.dumps(payload))
    p.stdin.close()
    return p, err, t


def test_cancel_inside_the_bar_loop_and_optimise(tmp_path):
    cf = tmp_path / 'cancel.flag'
    p, err, t = _provider('optimize', req(BAND, cancelFile=str(cf), maxIterations=5000, paramRanges={
        'ema-fast': {'min': 2, 'max': 60, 'step': 1}, 'ema-slow': {'min': 61, 'max': 200, 'step': 1}}))
    t_end = time.monotonic() + 120
    while time.monotonic() < t_end and p.poll() is None and sum('"phase":"optimize"' in x for x in err) < 2:
        time.sleep(0.05)
    cf.write_text('x')
    t0 = time.monotonic()
    p.wait(timeout=60)
    latency = time.monotonic() - t0
    out = p.stdout.read()
    t.join(timeout=5)
    assert p.returncode == 3, ''.join(err)[-800:]
    lines = [x for x in out.splitlines() if x.strip()]
    assert len(lines) == 1 and json.loads(lines[0])['errorCode'] == 'CANCELLED'
    assert latency < 2.0
    assert any('"phase":"run"' in x for x in err)             # the reference run's bar loop reports progress
    # a cancel flag already set: the bar loop stops at its first bar
    p2, err2, t2 = _provider('run_backtest', req(EMA, cancelFile=str(cf)))
    p2.wait(timeout=60)
    assert p2.returncode == 3 and json.loads(p2.stdout.read().strip())['errorCode'] == 'CANCELLED'
