"""base.execution.position_episodes, base.costs.costs_by_year and the normaliser's costs.byYear
pass-through (added for rebalancing portfolios: audit T2.1 / REBALANCE_TRADE_STATS)."""
import numpy as np
import pandas as pd
import pytest

from base.costs import CostModel, costs_by_year
from base.execution import fifo_round_trips, position_episodes
from base.simulator import Simulator, panel_from_arrays


def _fills(rows):
    """rows: (bar, asset, qty, price, ref, commission, reason)"""
    a = np.array(rows, dtype=float)
    return {'bar': a[:, 0].astype(np.int64), 'asset': a[:, 1].astype(np.int64), 'qty': a[:, 2], 'price': a[:, 3],
            'ref_price': a[:, 4], 'commission': a[:, 5], 'reason': a[:, 6].astype(np.int64),
            'phase': np.zeros(len(a), dtype=np.int64)}


DATES = pd.bdate_range('2024-01-01', periods=10)


def test_trims_stay_inside_one_episode_and_pnl_reconciles():
    # buy 10 @100, trim 2 @110 (rebalance), add 3 @105, sell all 11 @120 ; then reopen 5 @118, open at the end
    f = _fills([(1, 0, 10, 100.0, 100.0, 1.0, 5), (3, 0, -2, 110.0, 110.0, 0.5, 5), (4, 0, 3, 105.0, 105.0, 0.3, 5),
                (6, 0, -11, 120.0, 120.0, 1.2, 5), (7, 0, 5, 118.0, 118.0, 0.6, 5)])
    last = np.array([125.0])
    eps = position_episodes(f, DATES, ['AAA'], last_close=last)
    assert len(eps) == 2
    a, b = eps
    assert a['exitReason'] == 'rebalance' and a['fills'] == 4 and a['adds'] == 2 and a['reduces'] == 2
    cash = -10 * 100 - 1 + 2 * 110 - 0.5 - 3 * 105 - 0.3 + 11 * 120 - 1.2
    assert a['pnl'] == pytest.approx(cash)
    assert a['quantity'] == pytest.approx(11) and a['entryPrice'] == pytest.approx((1000 + 315) / 13)
    assert a['exitPrice'] == pytest.approx((220 + 1320) / 13)
    assert a['commission'] == pytest.approx(3.0) and a['entryDate'] == '2024-01-02' and a['exitDate'] == '2024-01-09'
    assert b['exitReason'] == 'open' and b['exitDate'] is None and b['pnl'] == pytest.approx(5 * 125 - 590 - 0.6)
    # FIFO pairing of the same fills makes fragments: 3 closed pieces (+ the open lot)
    assert len([t for t in fifo_round_trips(f, DATES, ['AAA'], last_close=last) if t['exitDate']]) == 3
    total_cash = sum(-q * p - c for q, p, c in zip(f['qty'], f['price'], f['commission']))
    assert sum(t['pnl'] for t in eps) == pytest.approx(total_cash + 5 * 125)


def test_flip_through_zero_splits_the_fill_pro_rata():
    f = _fills([(1, 0, 10, 100.0, 99.9, 2.0, 0), (5, 0, -15, 90.0, 90.1, 3.0, 0)])
    eps = position_episodes(f, DATES, ['AAA'], last_close=np.array([80.0]))
    assert [e['side'] for e in eps] == ['long', 'short']
    assert eps[0]['pnl'] == pytest.approx(10 * (90 - 100) - 2.0 - 3.0 * 10 / 15)
    assert eps[0]['slippageCost'] == pytest.approx(10 * 0.1 + 10 * 0.1)
    assert eps[1]['quantity'] == pytest.approx(5) and eps[1]['pnl'] == pytest.approx(5 * (90 - 80) - 1.0)
    assert eps[1]['commission'] == pytest.approx(1.0)


def test_signals_mode_episodes_are_the_round_trips(spy):
    """For a strategy that enters and exits whole positions, episodes == FIFO round trips."""
    from base import signals as S
    c = spy['Close'].to_numpy()
    p = panel_from_arrays(c, spy['Open'].to_numpy(), spy['High'].to_numpy(), spy['Low'].to_numpy(), dates=spy.index,
                          symbols=['SPY'])
    f, w = S.sma(c, 20), S.sma(c, 50)
    res = Simulator(p, costs=CostModel(commission_value=0.001, slippage_value=0.0005), backend='python',
                    fractional=False).run(entries=S.crossover(f, w), exits=S.crossunder(f, w))
    fifo = res.trades()
    eps = position_episodes(res.fill_arrays, p.dates, p.symbols, last_close=p.close[-1], high=p.high, low=p.low)
    assert len(eps) == len(fifo) > 10
    for a, b in zip(eps, fifo):
        for key in ('entryDate', 'exitDate', 'quantity', 'exitReason'):
            assert a[key] == b[key]
        for key in ('pnl', 'entryPrice', 'commission', 'slippageCost', 'mae', 'mfe'):
            assert a[key] == pytest.approx(b[key], rel=1e-12, abs=1e-9)


def test_rebalanced_portfolio_episodes_reconcile_with_equity(etf5):
    opens, closes = etf5
    p = panel_from_arrays(closes.to_numpy(), opens.to_numpy(), dates=closes.index, symbols=list(closes.columns))
    W = np.full(closes.shape, np.nan)
    month = closes.index.to_period('M')
    W[np.r_[True, month[1:] != month[:-1]]] = 0.2
    W[100:400, 1] = np.nan
    W[200, :] = [0.5, 0.0, 0.5, 0.0, 0.0]          # one rebalance that closes three positions
    res = Simulator(p, costs=CostModel(commission_value=0.001, slippage_value=0.0005), backend='python').run(weights=W)
    eps = position_episodes(res.fill_arrays, p.dates, p.symbols, last_close=p.close[-1], high=p.high, low=p.low)
    assert len(eps) == 8 and sum(1 for e in eps if e['exitDate']) == 3
    assert sum(e['pnl'] for e in eps) == pytest.approx(res.equity[-1] - 100000, abs=1e-6)
    assert all(-1 <= e['mae'] <= 0 <= e['mfe'] for e in eps)
    assert sum(e['commission'] for e in eps) == pytest.approx(res.fill_arrays['commission'].sum())
    # costs.byYear from the fills: exact totals
    by = costs_by_year(res.fill_arrays, p.dates, res.equity)
    cs = res.costs_summary()
    assert [r['year'] for r in by] == [2020, 2021, 2022, 2023, 2024]
    assert sum(r['tradedNotional'] for r in by) == pytest.approx(cs['tradedNotional'])
    assert sum(r['slippage'] for r in by) == pytest.approx(cs['totalSlippage'])
    assert sum(r['commission'] for r in by) == pytest.approx(cs['totalCommission'])


def test_normaliser_keeps_engine_by_year():
    from base.result_normalizer import EngineIdentity, build_backtest_result
    dates = pd.bdate_range('2023-01-02', periods=300)
    eq = np.linspace(100000, 110000, 300)
    by = [{'year': 2023, 'commission': 5.0, 'slippage': 1.0, 'tradedNotional': 5000.0, 'turnover': 0.05},
          {'year': 2024, 'commission': 1.0, 'slippage': 0.5, 'tradedNotional': 1000.0, 'turnover': 0.01}]
    d = build_backtest_result({'initialCapital': 100000, 'symbols': ['SPY'], 'riskFreeSource': 'rate'}, dates=dates,
                              equity=eq, identity=EngineIdentity('x', 'test'), trades=[],
                              costs={'totalCommission': 6.0, 'totalSlippage': 1.5, 'totalBorrow': 0.0,
                                     'tradedNotional': 6000.0, 'fills': 4, 'byYear': by})
    assert d['costs']['byYear'] == by
