"""Shared helpers the rebuilt Backtesting tab needs from every engine: cost sensitivity (costs.sensitivity),
CostModel.scaled, and the multi-asset weights block (base.execution.weights_block / SimResult.weights_block)."""
import numpy as np
import pandas as pd
import pytest

from base import signals as S
from base.contract import validate
from base.costs import CostModel, cost_sensitivity
from base.execution import align_panel, weights_block
from base.result_normalizer import EngineIdentity, build_backtest_result
from base.simulator import Simulator, panel_from_arrays

IDENT = EngineIdentity('vectorbt', 'Fincept reference simulator (base.simulator)', None, version='test',
                       fill_mode='next_open', slippage_itemised=True)


def test_scaled_multiplies_trading_costs_only():
    m = CostModel.preset_named('us_equity_ibkr_fixed')
    s = m.scaled(2)
    assert s.commission_value == pytest.approx(2 * m.commission_value)
    assert s.min_commission == pytest.approx(2 * m.min_commission)
    assert s.slippage_value == pytest.approx(2 * m.slippage_value) and s.spread_bps == pytest.approx(2 * m.spread_bps)
    assert s.borrow_rate_annual == m.borrow_rate_annual and s.max_commission_pct == m.max_commission_pct
    assert m.scaled(0).is_zero() and m.commission_value == 0.005          # the original is untouched
    with pytest.raises(ValueError):
        m.scaled(-1)


def test_cost_sensitivity_rows(spy):
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    ent, ex = S.crossover(f, s), S.crossunder(f, s)
    p = panel_from_arrays(spy['Close'], spy['Open'], spy['High'], spy['Low'], spy['Volume'], dates=spy.index,
                          symbols=['SPY'])
    model = CostModel(commission_value=0.001, slippage_value=0.0005)

    def sim_eq(cm):
        return Simulator(p, costs=cm).run(entries=ent, exits=ex).equity

    base_eq = sim_eq(model)
    rows = cost_sensitivity(sim_eq, model, spy.index, base_equity=base_eq)
    assert [r['multiple'] for r in rows] == [0, 1, 2, 3]
    tr = [r['totalReturn'] for r in rows]
    assert tr[0] > tr[1] > tr[2] > tr[3]
    assert tr[1] == pytest.approx(base_eq[-1] / base_eq[0] - 1)
    assert tr[0] == pytest.approx(sim_eq(CostModel())[-1] / 100000 - 1)
    assert all(r['sharpeRatio'] is not None and r['cagr'] is not None for r in rows)
    # a failing multiple is reported with nulls, the others survive
    rows = cost_sensitivity(lambda cm: (_ for _ in ()).throw(RuntimeError('x')) if cm.commission_value > 0.0025
                            else sim_eq(cm), model, spy.index)
    assert rows[3]['totalReturn'] is None and rows[1]['totalReturn'] is not None


def test_weights_block_multi_asset(etf5):
    opens, closes = etf5
    frames = {s: pd.DataFrame({'Open': opens[s], 'High': np.maximum(opens[s], closes[s]),
                               'Low': np.minimum(opens[s], closes[s]), 'Close': closes[s], 'Volume': 1e6})
              for s in closes.columns}
    panel = align_panel(frames)
    n, k = panel.close.shape
    w = np.full((n, k), np.nan)
    months = panel.dates.to_period('M')
    first = np.r_[True, months[1:] != months[:-1]]
    w[first] = 1.0 / k
    res = Simulator(panel, costs=CostModel(commission_value=0.001)).run(weights=w)
    wb = res.weights_block()
    assert wb['symbols'] == list(panel.symbols) and len(wb['t']) == len(wb['w']) <= 600
    assert wb['t'][-1] == panel.dates[-1].strftime('%Y-%m-%d')
    last = np.array(wb['w'][-1], dtype=float)
    gross = res.exposure_series()['gross'][-1]
    assert last.sum() == pytest.approx(gross, abs=1e-4) and np.all(last > 0.1)
    # one symbol -> no block; mismatched shapes -> None
    assert weights_block(panel.dates, ['SPY'], res.position_values()[:, :1], res.equity) is None
    assert weights_block(panel.dates, list(panel.symbols), res.position_values()[:10], res.equity) is None
    # the block survives the result builder (extra) and the contract
    req = {'symbols': list(panel.symbols), 'startDate': '2015-01-01', 'endDate': '2024-12-31',
           'initialCapital': 100000, 'commission': 0.001, 'slippage': 0.0, 'riskFreeRate': 0.0,
           'riskFreeSource': 'rate', 'strategy': {'type': 'equal_weight', 'name': 'EW', 'params': {}}}
    data = build_backtest_result(req, dates=panel.dates, equity=res.equity, identity=IDENT, trades=res.trades(),
                                 fills=res.fills(), costs=res.costs_summary(), frames=frames,
                                 extra={'weights': wb})
    assert validate('run_backtest', data) == [] and data['weights']['symbols'] == list(panel.symbols)
