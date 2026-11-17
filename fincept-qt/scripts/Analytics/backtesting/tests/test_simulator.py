"""base.simulator / base.execution: reproduction of the independent reference, fill timing,
look-ahead, cost accounting identities, open positions, stops, shorts, corporate actions,
calendar alignment, numba == python."""
import math

import numpy as np
import pandas as pd
import pytest

import reference as ref
from base import signals as S
from base.costs import CostModel
from base.execution import align_panel, fill_model_block, max_affordable_qty
from base.simulator import Simulator, numba_available, panel_from_arrays, simulate

FILLS = ('next_open', 'next_close', 'same_close')
COSTS = ((0.0, 0.0), (0.001, 0.0005))


def _panel(df, sym='SPY'):
    return panel_from_arrays(df['Close'], df['Open'], df['High'], df['Low'], df.get('Volume'), dates=df.index,
                             symbols=[sym])


def _sim(df, fill='next_open', comm=0.0, slip=0.0, frac=True, **kw):
    return Simulator(_panel(df), costs=CostModel(commission_value=comm, slippage_value=slip), fill=fill,
                     fractional=frac, **kw)


def _cmp(res, r):
    rel = np.max(np.abs(res.equity / r['equity'].values - 1))
    a = {(t['entryDate'], t['exitDate']) for t in res.trades()}
    b = {(str(t['entry'].date()), str(t['exit'].date()) if t.get('exit') is not None else None) for t in r['trades']}
    return rel, a, b


# ── reference reproduction (the audit's canonical strategies) ───────────────

@pytest.mark.parametrize('fill', FILLS)
@pytest.mark.parametrize('comm,slip', COSTS)
@pytest.mark.parametrize('frac', [True, False])
def test_sma_cross_equals_reference(spy, fill, comm, slip, frac):
    ent, exi = ref.sma_cross_signals(spy['Close'], 20, 50, gate=True)
    r = ref.simulate(spy, ent, exi, fill=fill, commission=comm, slippage=slip, fractional=frac)
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    assert np.array_equal(S.crossover(f, s), ent.values) and np.array_equal(S.crossunder(f, s), exi.values)
    res = _sim(spy, fill, comm, slip, frac).run(entries=S.crossover(f, s), exits=S.crossunder(f, s))
    rel, a, b = _cmp(res, r)
    assert rel < 1e-12 and a == b


@pytest.mark.parametrize('fill', FILLS)
@pytest.mark.parametrize('comm,slip', COSTS)
def test_buy_hold_and_rsi_equal_reference(spy, fill, comm, slip):
    bh = ref.buy_hold(spy, fill=fill, commission=comm, slippage=slip)
    ent = np.zeros(len(spy), bool)
    ent[0] = True
    res = _sim(spy, fill, comm, slip).run(entries=ent, exits=np.zeros(len(spy), bool))
    assert np.max(np.abs(res.equity / bh['equity'].values - 1)) < 1e-12
    e, x = ref.rsi_signals(spy['Close'], 14, 30, 70, 'wilder', cross=True)
    rr = ref.simulate(spy, e, x, fill=fill, commission=comm, slippage=slip)
    rsi = S.rsi_wilder(spy['Close'], 14)
    assert np.allclose(np.nan_to_num(rsi), np.nan_to_num(ref.rsi_wilder(spy['Close'], 14).values), atol=1e-9)
    res = _sim(spy, fill, comm, slip).run(entries=S.cross_below_level(rsi, 30), exits=S.cross_above_level(rsi, 70))
    rel, a, b = _cmp(res, rr)
    assert rel < 1e-9 and a == b


@pytest.mark.parametrize('fill', FILLS)
@pytest.mark.parametrize('comm', [0.0, 0.001])
def test_monthly_equal_weight_equals_reference(etf5, fill, comm):
    opens, closes = etf5
    r = ref.ew_monthly(closes, opens, commission=comm, fill=fill)
    idx = closes.index
    m = idx.to_period('M')
    first = np.r_[True, m[1:] != m[:-1]]
    w = np.full(closes.shape, np.nan)
    w[first] = 1.0 / closes.shape[1]
    hi = np.maximum(opens.values, closes.values)
    lo = np.minimum(opens.values, closes.values)
    p = panel_from_arrays(closes.values, opens.values, hi, lo, dates=idx, symbols=list(closes.columns))
    res = Simulator(p, costs=CostModel(commission_value=comm), fill=fill).run(weights=w)
    assert np.max(np.abs(res.equity / r['equity'].values - 1)) < 1e-9
    cs = res.costs_summary()
    assert cs['totalCommission'] == pytest.approx(r['fees'], rel=1e-9, abs=1e-9)
    assert cs['tradedNotional'] == pytest.approx(r['turnover'], rel=1e-9)


@pytest.mark.skipif(not numba_available(), reason='numba not installed')
def test_numba_backend_is_bit_identical(spy, etf5):
    f, s = S.sma(spy['Close'], 10), S.sma(spy['Close'], 30)
    cm = CostModel(commission_type='per_share', commission_value=0.005, min_commission=1.0, max_commission_pct=0.01,
                   slippage_value=0.0003, spread_bps=2, borrow_rate_annual=0.01, cash_rate_annual=0.02)
    for kw in ({'stop_loss': 0.05, 'take_profit': 0.1}, {'trailing_stop': 0.07}, {}):
        a = Simulator(_panel(spy), costs=cm, backend='python', **kw).run(
            entries=S.crossover(f, s), exits=S.crossunder(f, s), short_entries=S.crossunder(f, s),
            short_exits=S.crossover(f, s))
        b = Simulator(_panel(spy), costs=cm, backend='numba', **kw).run(
            entries=S.crossover(f, s), exits=S.crossunder(f, s), short_entries=S.crossunder(f, s),
            short_exits=S.crossover(f, s))
        assert b.backend == 'numba'
        assert np.array_equal(a.equity, b.equity)
        for k in a.fill_arrays:
            assert np.array_equal(a.fill_arrays[k], b.fill_arrays[k]), k
    opens, closes = etf5
    w = np.full(closes.shape, np.nan)
    w[::21] = [0.4, 0.3, 0.2, 0.2, -0.1]
    p = panel_from_arrays(closes.values, opens.values, dates=closes.index, symbols=list(closes.columns))
    a = Simulator(p, costs=cm, backend='python').run(weights=w)
    b = Simulator(p, costs=cm, backend='numba').run(weights=w)
    assert np.array_equal(a.equity, b.equity)


@pytest.mark.parametrize('fill', FILLS)
def test_per_entry_size_array(spy, fill):
    """run(size=<(n, k) array>): the value on the DECISION bar sizes the entry ('fraction' of
    equity / slots or money 'value'); 0 skips it; a constant array equals the scalar rule;
    python == numba."""
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    ent, exi = S.crossover(f, s), S.crossunder(f, s)
    sim = _sim(spy, fill, 0.001, 0.0005)
    n = len(spy)
    half = sim.run(entries=ent, exits=exi, size=np.full(n, 0.5))
    assert np.array_equal(half.equity, _sim(spy, fill, 0.001, 0.0005, size=0.5).run(entries=ent, exits=exi).equity)
    # only the first entry signal gets a size: one trade, 30 % of equity at the fill
    first = int(np.argmax(ent))
    sz = np.zeros(n)
    sz[first] = 0.3
    one = sim.run(entries=ent, exits=exi, size=sz)
    buys = one.fill_arrays['qty'] > 0
    assert buys.sum() == 1
    notional = float(one.fill_arrays['qty'][buys][0] * one.fill_arrays['price'][buys][0] + one.fill_arrays['commission'][buys][0])
    assert notional == pytest.approx(30000.0, rel=1e-9)
    # money per entry (capped by cash), python == numba
    val = sim.run(entries=ent, exits=exi, size=np.full(n, 25000.0), size_type='value')
    b = val.fill_arrays['qty'] > 0
    paid = val.fill_arrays['qty'][b] * val.fill_arrays['price'][b] + val.fill_arrays['commission'][b]
    assert np.allclose(paid, 25000.0, rtol=1e-9) and b.sum() > 5
    if numba_available():
        nb = _sim(spy, fill, 0.001, 0.0005, backend='numba').run(entries=ent, exits=exi, size=np.full(n, 25000.0),
                                                                  size_type='value')
        assert np.array_equal(nb.equity, val.equity)
    with pytest.raises(ValueError):
        sim.run(entries=ent, exits=exi, size=sz, size_type='shares')


# ── fill timing and look-ahead (decision 2) ─────────────────────────────────

def test_fill_timing_conventions():
    d = pd.bdate_range('2024-01-01', periods=6)
    o = np.array([10, 11, 12, 13, 14, 15], dtype=float)
    c = o + 0.5
    ent = np.array([False, True, False, False, False, False])
    p = panel_from_arrays(c, o, dates=d, symbols=['X'])
    expect = {'next_open': (2, 12.0), 'next_close': (2, 12.5), 'same_close': (1, 11.5)}
    for fill, (bar, px) in expect.items():
        res = Simulator(p, fill=fill).run(entries=ent, exits=np.zeros(6, bool))
        assert int(res.fill_arrays['bar'][0]) == bar and res.fill_arrays['price'][0] == px
    # a signal on the last bar never fills on that bar unless same_close was asked for
    last = np.zeros(6, bool)
    last[-1] = True
    assert len(Simulator(p).run(entries=last, exits=np.zeros(6, bool)).fill_arrays['bar']) == 0
    assert fill_model_block('next_open')['fillAt'] == 'next_open'
    w = Simulator(p, fill='same_close').run(entries=ent, exits=np.zeros(6, bool)).warnings
    assert any(x['code'] == 'SAME_BAR_FILL' for x in w)


@pytest.mark.parametrize('fill', ['next_open', 'next_close'])
def test_no_lookahead_future_bars_do_not_change_past(spy, fill):
    cut = spy.index.get_loc(pd.Timestamp('2022-06-30'))
    rng = np.random.default_rng(7)
    fac = np.ones(len(spy))
    fac[cut + 1:] = np.exp(np.cumsum(rng.normal(0, 0.03, len(spy) - cut - 1)))
    pert = spy.copy()
    for col in ('Open', 'High', 'Low', 'Close'):
        pert[col] = spy[col] * fac
    out = []
    for df in (spy, pert):
        f, s = S.sma(df['Close'], 20), S.sma(df['Close'], 50)
        out.append(_sim(df, fill, 0.001, 0.0005).run(entries=S.crossover(f, s), exits=S.crossunder(f, s)))
    a, b = out
    assert np.array_equal(a.equity[:cut + 1], b.equity[:cut + 1])
    fa = a.fill_arrays['bar'] <= cut
    fb = b.fill_arrays['bar'] <= cut
    assert np.array_equal(a.fill_arrays['price'][fa], b.fill_arrays['price'][fb])
    assert not np.array_equal(a.equity, b.equity)


# ── costs, open positions, identities (decisions 3, 4) ──────────────────────

def _identity(res, initial):
    trades = res.trades()
    pnl = sum(t['pnl'] for t in trades if t['pnl'] is not None)
    acc = res.accruals
    return res.equity[-1] - initial - (pnl + acc['interest'] - acc['financing'])


def test_equity_change_equals_trade_pnl_with_costs(spy):
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    res = _sim(spy, 'next_open', 0.001, 0.0005).run(entries=S.crossover(f, s), exits=S.crossunder(f, s))
    trades = res.trades()
    assert trades[-1]['exitReason'] == 'open' and trades[-1]['exitDate'] is None
    assert all(t['exitDate'] for t in trades[:-1])
    assert abs(_identity(res, 100000.0)) < 1e-6
    cs = res.costs_summary()
    assert cs['totalCommission'] == pytest.approx(sum(t['commission'] for t in trades))
    assert cs['totalSlippage'] == pytest.approx(sum(t['slippageCost'] for t in trades))
    for t in trades:
        assert t['grossPnl'] == pytest.approx(t['pnl'] + t['commission'] + t['slippageCost'] + t['borrowCost']
                                              - t['dividends'], abs=1e-6)
    # the open trade is marked at the last close, net of its entry costs
    o = trades[-1]
    assert o['markPrice'] == spy['Close'].iloc[-1]
    assert o['pnl'] == pytest.approx((o['markPrice'] - o['entryPrice']) * o['quantity'] - o['commission'])


def test_shorts_borrow_and_interest_identity(spy):
    f, s = S.sma(spy['Close'], 10), S.sma(spy['Close'], 30)
    cm = CostModel(commission_value=0.0005, slippage_value=0.0002, borrow_rate_annual=0.03, cash_rate_annual=0.02,
                   financing_rate_annual=0.05)
    res = Simulator(_panel(spy), costs=cm).run(entries=S.crossover(f, s), exits=S.crossunder(f, s),
                                               short_entries=S.crossunder(f, s), short_exits=S.crossover(f, s))
    trades = res.trades()
    shorts = [t for t in trades if t['side'] == 'short']
    assert shorts and all(t['borrowCost'] > 0 for t in shorts if t['holdingPeriod'] > 1)
    assert res.accruals['borrow'] == pytest.approx(sum(t['borrowCost'] for t in trades), rel=1e-9)
    assert res.accruals['interest'] > 0
    assert abs(_identity(res, 100000.0)) < 1e-6


def test_weights_mode_identity_and_turnover(etf5):
    opens, closes = etf5
    w = np.full(closes.shape, np.nan)
    w[::21] = [0.3, 0.3, 0.2, 0.1, 0.1]
    p = panel_from_arrays(closes.values, opens.values, dates=closes.index, symbols=list(closes.columns))
    res = Simulator(p, costs=CostModel(commission_value=0.001, slippage_value=0.0005)).run(weights=w)
    assert abs(_identity(res, 100000.0)) < 1e-5
    assert res.costs_summary()['tradedNotional'] > 100000


def test_commission_models():
    cm = CostModel(commission_type='per_share', commission_value=0.005, min_commission=1.0, max_commission_pct=0.01)
    assert cm.commission(100, 50.0) == pytest.approx(1.0)        # 0.50 -> minimum 1.00
    assert cm.commission(1000, 50.0) == pytest.approx(5.0)
    assert cm.commission(1000, 0.10) == pytest.approx(1.0)       # cap 1% of 100 = 1.00
    assert CostModel(commission_type='per_trade', commission_value=4.95).commission(10, 1.0) == 4.95
    assert CostModel(commission_value=0.001).commission(10, 100.0) == pytest.approx(1.0)
    q = max_affordable_qty(10000.0, 50.0, cm)
    assert q * 50 + cm.commission(q, 50) <= 10000.0 + 1e-9
    with pytest.raises(ValueError):
        CostModel(commission_type='bogus')


def test_cost_presets_and_request():
    m = CostModel.from_request({'commission': 0.001, 'slippage': 0.0005, 'symbols': ['SPY']})
    assert m.commission_value == 0.001 and m.slippage_value == 0.0005 and m.preset == 'request'
    assert CostModel.from_request({'symbols': ['BTC-USD']}).preset == 'crypto_spot'
    assert CostModel.from_request({'symbols': ['RELIANCE.NS']}).preset == 'india_equity_delivery'
    m = CostModel.from_request({'symbols': ['SPY'], 'costModel': {'preset': 'us_equity_ibkr_fixed', 'minCommission': 2}})
    assert m.commission_type == 'per_share' and m.min_commission == 2.0
    d = m.to_contract()
    assert d['commissionType'] == 'per_share' and d['preset'] == 'us_equity_ibkr_fixed'
    with pytest.raises(ValueError):
        CostModel.from_request({'costModel': {'preset': 'nope'}})


def test_whole_shares_never_overdraw(spy):
    f, s = S.sma(spy['Close'], 5), S.sma(spy['Close'], 15)
    res = _sim(spy, 'next_open', 0.001, 0.0005, frac=False).run(entries=S.crossover(f, s), exits=S.crossunder(f, s))
    assert res.cash.min() >= -1e-9
    assert np.all(np.mod(np.abs(res.fill_arrays['qty']), 1) == 0)


def test_participation_cap_and_impact(spy):
    df = spy.copy()
    df['Volume'] = 1000.0
    ent = np.zeros(len(df), bool)
    ent[30] = True
    cm = CostModel(participation_cap=0.1, impact_coef=0.5)
    res = Simulator(_panel(df), costs=cm).run(entries=ent, exits=np.zeros(len(df), bool))
    assert res.fill_arrays['qty'][0] == pytest.approx(100.0)
    assert any(w['code'] == 'VOLUME_CAPPED' for w in res.warnings)
    assert res.fill_arrays['price'][0] > res.fill_arrays['ref_price'][0]


# ── stops (intrabar, gap handling) ──────────────────────────────────────────

def test_stop_loss_take_profit_gap_and_order():
    d = pd.bdate_range('2024-01-01', periods=6)
    o = np.array([100, 100, 100, 96, 90, 90], dtype=float)
    h = np.array([101, 101, 101, 97, 91, 91], dtype=float)
    lo = np.array([99, 99, 94, 95, 89, 89], dtype=float)
    c = np.array([100, 100, 96, 96, 90, 90], dtype=float)
    p = panel_from_arrays(c, o, h, lo, dates=d, symbols=['X'])
    ent = np.array([True, False, False, False, False, False])
    res = Simulator(p, stop_loss=0.05).run(entries=ent, exits=np.zeros(6, bool))
    t = res.trades()[0]
    assert t['exitReason'] == 'stop_loss' and t['exitPrice'] == pytest.approx(95.0) and t['exitDate'] == '2024-01-03'
    # gap through the stop: filled at the (worse) open
    o2, lo2 = o.copy(), lo.copy()
    o2[2], lo2[2] = 93.0, 92.0
    res = Simulator(panel_from_arrays(c, o2, h, lo2, dates=d, symbols=['X']), stop_loss=0.05).run(
        entries=ent, exits=np.zeros(6, bool))
    assert res.trades()[0]['exitPrice'] == pytest.approx(93.0)
    # stop and target in the same bar: the stop wins (conservative)
    h3 = h.copy()
    h3[2] = 115.0
    res = Simulator(panel_from_arrays(c, o, h3, lo, dates=d, symbols=['X']), stop_loss=0.05, take_profit=0.1).run(
        entries=ent, exits=np.zeros(6, bool))
    assert res.trades()[0]['exitReason'] == 'stop_loss'


def test_stop_never_loses_more_than_stop_plus_gap(spy):
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    res = _sim(spy, 'next_open', stop_loss=0.05).run(entries=S.crossover(f, s), exits=S.crossunder(f, s))
    op = spy['Open'].values
    for t in res.trades():
        if t['exitReason'] == 'stop_loss':
            gap_px = op[t['exitBar']]
            assert t['exitPrice'] >= min(t['entryPrice'] * 0.95, gap_px) - 1e-9


# ── corporate actions on raw prices ─────────────────────────────────────────

def test_raw_prices_split_and_dividend_cash(aapl_raw):
    df = aapl_raw.rename(columns={'open': 'Open', 'high': 'High', 'low': 'Low', 'close': 'Close', 'volume': 'Volume',
                                  'dividends': 'Dividends', 'splits': 'Splits'})
    panel = align_panel({'AAPL': df[['Open', 'High', 'Low', 'Close', 'Volume', 'Dividends', 'Splits']]})
    panel.adjustment = 'none'
    ent = np.zeros(panel.n, bool)
    ent[0] = True
    res = Simulator(panel, costs=CostModel(commission_value=0.0)).run(entries=ent, exits=np.zeros(panel.n, bool))
    split_bar = int(np.flatnonzero(df['Splits'].values == 4.0)[0])
    q = res.positions[:, 0]
    assert q[split_bar] == pytest.approx(4 * q[split_bar - 1])
    div = res.accruals['dividends']
    expected = sum(q[i - 1] * df['Dividends'].values[i] for i in range(1, panel.n) if df['Dividends'].values[i] > 0)
    assert div == pytest.approx(expected) and div > 0
    t = res.trades()[0]
    assert t['dividends'] == pytest.approx(div)
    assert res.equity[-1] - 100000 == pytest.approx(t['pnl'], abs=1e-6)


# ── calendars (decision 5) ──────────────────────────────────────────────────

def test_union_calendar_defers_orders_and_ignores_closed_market_signals(cal):
    frames = {'BTC-USD': cal['BTC-USD'], 'SPY': cal['SPY']}
    panel = align_panel(frames)
    assert any(w['code'] == 'CALENDAR_MISMATCH' and w['symbol'] == 'SPY' for w in panel.warnings)
    j = panel.col('SPY')
    sat = np.flatnonzero(panel.dates.dayofweek == 5)[0]
    assert not panel.tradable[sat, j] and panel.tradable[sat, panel.col('BTC-USD')]
    assert panel.close[sat, j] == panel.close[sat - 1, j]
    ent = np.zeros((panel.n, 2), bool)
    ent[sat - 1, j] = True       # Friday decision -> fills Monday's open (Saturday is not tradable)
    ent[sat, j] = True           # a signal on a closed-market bar is ignored, not carried
    res = Simulator(panel).run(entries=ent, exits=np.zeros_like(ent))
    bars = res.fill_arrays['bar']
    assert len(bars) == 1 and panel.dates[bars[0]].dayofweek == 0
    assert res.fill_arrays['price'][0] == panel.open[bars[0], j]


def test_session_dates_are_kept(cal):
    rel = cal['RELIANCE.NS']
    assert int((rel.index.dayofweek >= 5).sum()) <= 2       # only real special sessions, no shifted bars
    assert rel.index[0] == pd.Timestamp('2024-01-01')       # NSE trades on 1 January; no move to 31 December


def test_simulate_one_call_form(spy):
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    res = simulate({'SPY': spy}, entries=S.crossover(f, s), exits=S.crossunder(f, s),
                   costs=CostModel(commission_value=0.001))
    assert res.equity[-1] > 0 and res.mode == 'signals'


def test_signals_work_on_matrices(etf5):
    _, closes = etf5
    c = closes.values
    f2, s2 = S.sma(c, 20), S.sma(c, 50)
    x2 = S.crossover(f2, s2)
    assert x2.shape == c.shape
    for j in range(c.shape[1]):
        assert np.array_equal(x2[:, j], S.crossover(S.sma(c[:, j], 20), S.sma(c[:, j], 50)))
        assert np.allclose(np.nan_to_num(S.rsi_wilder(c, 14)[:, j]), np.nan_to_num(S.rsi_wilder(c[:, j], 14)))
        assert np.allclose(np.nan_to_num(S.ema(c, 10)[:, j]), np.nan_to_num(S.ema(c[:, j], 10)))
    assert S.cross_below_level(S.rsi_wilder(c, 14), 30).shape == c.shape
