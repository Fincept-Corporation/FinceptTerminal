"""base.metrics: closed-form and hand-computed cases, the independent reference, and test-time
cross-checks against empyrical / scipy when installed (never runtime dependencies)."""
import math

import numpy as np
import pandas as pd
import pytest

import reference as ref
from base import metrics as M
from base import signals as S
from base.costs import CostModel
from base.simulator import Simulator, panel_from_arrays


def _curve(spy, rf=0.0):
    f, s = S.sma(spy['Close'], 20), S.sma(spy['Close'], 50)
    p = panel_from_arrays(spy['Close'], spy['Open'], spy['High'], spy['Low'], dates=spy.index, symbols=['SPY'])
    res = Simulator(p, costs=CostModel(commission_value=0.001, slippage_value=0.0005)).run(
        entries=S.crossover(f, s), exits=S.crossunder(f, s))
    return pd.Series(res.equity, index=spy.index)


# ── closed form / hand computed ─────────────────────────────────────────────

def test_cagr_calendar_basis():
    d = pd.to_datetime(['2020-01-01', '2021-01-01', '2022-01-01'])
    e = np.array([100.0, 110.0, 121.0])
    assert M.cagr(e, d) == pytest.approx(1.21 ** (365.25 / 731) - 1, abs=1e-15)
    assert M.total_return(e) == pytest.approx(0.21)


def test_drawdown_hand_computed():
    d = pd.bdate_range('2024-01-01', periods=6)
    e = np.array([100, 120, 90, 95, 130, 117], dtype=float)
    assert M.max_drawdown(e) == pytest.approx(0.25)
    dd = M.drawdown_series(e)
    assert np.all(dd <= 0) and dd[2] == pytest.approx(-0.25)
    assert M.drawdown_durations(e, d)['bars'] == 2
    tab = M.drawdown_table(e, d)
    assert [t['rank'] for t in tab] == [1, 2]
    a, b = tab
    assert a['depth'] == pytest.approx(0.25) and a['peakDate'] == '2024-01-02' and a['troughDate'] == '2024-01-03'
    assert a['recoveryDate'] == '2024-01-05' and a['toTroughBars'] == 1 and a['recoveryBars'] == 2
    assert b['depth'] == pytest.approx(0.1) and b['recoveryDate'] is None and b['recoveryBars'] is None


def test_sharpe_sortino_hand_computed():
    r = np.array([0.01, -0.02, 0.03, 0.0, -0.01])
    rf = 0.0252 / 252
    ex = r - rf
    sh = ex.mean() / ex.std(ddof=1) * math.sqrt(252)
    so = ex.mean() * math.sqrt(252) / math.sqrt(np.mean(np.minimum(ex, 0) ** 2))
    assert M.sharpe_ratio(r, rf, 252) == pytest.approx(sh, rel=1e-12)
    assert M.sortino_ratio(r, rf, 252) == pytest.approx(so, rel=1e-12)
    assert M.downside_deviation(r, rf, 252) == pytest.approx(math.sqrt(np.mean(np.minimum(ex, 0) ** 2)) * math.sqrt(252))


def test_undefined_is_none_with_reason():
    why = {}
    flat = np.full(50, 100.0)
    r = M.simple_returns(flat)
    assert M.sharpe_ratio(r, why=why) is None and 'sharpeRatio' in why
    assert M.sortino_ratio(r, why=why) is None and 'sortinoRatio' in why
    assert M.calmar_ratio(0.1, 0.0, why=why) is None and 'calmarRatio' in why
    assert M.volatility([0.01], why=why) is None and 'volatility' in why
    h = M.headline(flat, pd.bdate_range('2024-01-01', periods=50), why=why)
    for k, v in h.items():
        assert v is None or (isinstance(v, (int, float)) and math.isfinite(v)), k
    assert h['maxDrawdown'] == 0.0 and h['sharpeRatio'] is None


def test_nan_safe_inputs():
    e = np.array([100.0, 101.0, np.nan, 103.0, 102.0, 104.0])
    r = M.simple_returns(e)
    assert M.volatility(r) is not None and math.isfinite(M.volatility(r))
    assert M.skewness(r) is not None


def test_periods_per_year_cases():
    assert M.infer_periods_per_year(pd.bdate_range('2024-01-01', periods=300)) == 252
    assert M.infer_periods_per_year(pd.date_range('2024-01-01', periods=300)) == 365
    assert M.infer_periods_per_year(pd.date_range('2020-01-03', periods=100, freq='W-FRI')) == 52
    assert M.infer_periods_per_year(pd.date_range('2015-01-31', periods=60, freq='ME')) == 12
    # B17: Mon-Thu daily (no gap > 1.5 days) is an exchange calendar, not 24x7
    assert M.infer_periods_per_year(pd.to_datetime(['2024-01-01', '2024-01-02', '2024-01-03', '2024-01-04'])) == 252
    assert M.infer_periods_per_year(pd.to_datetime(['2024-01-06', '2024-01-07'])) == 365
    hours = pd.DatetimeIndex([d + pd.Timedelta(hours=h) for d in pd.bdate_range('2024-01-01', periods=20)
                              for h in (9.5, 10.5, 11.5, 12.5, 13.5, 14.5, 15.5)])
    assert M.infer_periods_per_year(hours) == 7 * 252


def test_periods_per_year_exchange_with_rare_weekend_sessions():
    """NSE trades a few special weekend sessions (budget day 2020-02-01 was a Saturday, Diwali muhurat
    sessions): still an exchange calendar (252), not a 24x7 market (365)."""
    d = pd.bdate_range('2015-01-01', '2024-12-31')
    d = d.append(pd.to_datetime(['2020-02-01', '2023-11-12'])).sort_values()
    assert M.infer_periods_per_year(d) == 252
    btc_spy = pd.date_range('2015-01-01', '2024-12-31')          # union with a 24x7 market
    assert M.infer_periods_per_year(btc_spy) == 365


def test_rf_series_per_bar():
    d = pd.to_datetime(['2024-01-05', '2024-01-08', '2024-01-09'])
    y = np.array([0.05, 0.05, 0.04])
    rb = M.rf_per_bar(2, y, 252, d)
    assert rb[0] == pytest.approx(1.05 ** (3 / 365) - 1)   # Friday -> Monday accrues 3 days at Friday's yield
    assert rb[1] == pytest.approx(1.05 ** (1 / 365) - 1)
    assert M.rf_per_bar(3, 0.0252, 252)[0] == pytest.approx(0.0001)


# ── reference agreement (A0.2) ──────────────────────────────────────────────

@pytest.mark.parametrize('rf', [0.0, 0.04])
def test_headline_equals_reference(spy, rf):
    eq = _curve(spy)
    bench = spy['Close']
    h = M.headline(eq.values, eq.index, rf=rf, ppy=252, bench_equity=bench.values, bench_dates=bench.index)
    r = ref.metrics(eq, rf=rf, ppy=252, bench=bench)
    pairs = {'totalReturn': 'totalReturn', 'cagr': 'cagr', 'annReturnBars': 'annReturnBars', 'volatility': 'volatility',
             'sharpeRatio': 'sharpe', 'sortinoRatio': 'sortino', 'maxDrawdown': 'maxDrawdown', 'calmarRatio': 'calmar',
             'downsideDeviation': 'downsideDeviation', 'beta': 'beta', 'alpha': 'alpha',
             'trackingError': 'trackingError', 'informationRatio': 'informationRatio', 'correlation': 'correlation'}
    for k, rk in pairs.items():
        assert h[k] == pytest.approx(r[rk], rel=1e-9, abs=1e-12), k
    assert h['maxDrawdownDuration'] == r['maxDrawdownDurationBars']
    rr = M.simple_returns(eq.values)
    assert M.value_at_risk(rr, 0.95) == pytest.approx(r['var95'], rel=1e-12)
    assert M.conditional_var(rr, 0.95) == pytest.approx(r['cvar95'], rel=1e-12)


def test_strategy_and_benchmark_share_the_function(spy):
    """B3: the benchmark summary must be the same Sharpe definition as the strategy's."""
    c = spy['Close']
    h = M.headline(c.values, c.index, rf=0.0, ppy=252)
    assert h['sharpeRatio'] == pytest.approx(ref.metrics(c, 0.0, 252)['sharpe'], rel=1e-12)


def test_rolling_matches_pandas_with_same_dates(spy):
    """B2: the rolling value of a window is dated at the window's LAST bar; the last window exists."""
    eq = _curve(spy)
    w = 126
    out = M.rolling_series(eq.values, eq.index, w, rf_bar=0.0, ppy=252, bench_returns=None)
    r = eq.pct_change()
    pv = (r.rolling(w).std(ddof=1) * math.sqrt(252)).dropna()
    got = {x['date']: x['value'] for x in out['rollingVolatility']}
    assert list(got)[0] == pv.index[0].strftime('%Y-%m-%d')
    assert list(got)[-1] == eq.index[-1].strftime('%Y-%m-%d')
    for d, v in pv.items():
        assert got[d.strftime('%Y-%m-%d')] == pytest.approx(v, abs=1e-6)
    ps = (r.rolling(w).mean() / r.rolling(w).std(ddof=1) * math.sqrt(252)).dropna()
    gs = {x['date']: x['value'] for x in out['rollingSharpe']}
    for d, v in ps.items():
        assert gs[d.strftime('%Y-%m-%d')] == pytest.approx(v, abs=1e-6)


def test_monthly_and_yearly_compound_to_total(spy):
    eq = _curve(spy)
    mt = M.monthly_table(eq.values, eq.index)
    g = np.prod([1 + m for row in mt for m in row['months'] if m is not None])
    assert g - 1 == pytest.approx(eq.iloc[-1] / eq.iloc[0] - 1, rel=1e-6)
    yt = M.yearly_table(eq.values, eq.index, spy['Close'].values, spy.index)
    assert np.prod([1 + y['strategy'] for y in yt]) - 1 == pytest.approx(eq.iloc[-1] / eq.iloc[0] - 1, rel=1e-9)
    assert all(y['excess'] == pytest.approx(y['strategy'] - y['benchmark']) for y in yt)


def test_period_stats_calendar_weeks(spy):
    st = M.period_stats(spy['Close'].values, spy.index)
    wk = spy['Close'].resample('W-FRI').last()
    wr = wk.pct_change()
    wr.iloc[0] = wk.iloc[0] / spy['Close'].iloc[0] - 1
    assert st['bestWeek'] == pytest.approx(wr.max()) and st['worstWeek'] == pytest.approx(wr.min())


def test_trade_stats_closed_only():
    trades = [{'exitDate': '2024-01-05', 'pnl': 100.0, 'pnlPercent': 0.1, 'holdingPeriod': 3, 'side': 'long'},
              {'exitDate': '2024-01-09', 'pnl': -50.0, 'pnlPercent': -0.05, 'holdingPeriod': 2, 'side': 'long'},
              {'exitDate': '2024-01-12', 'pnl': 30.0, 'pnlPercent': 0.03, 'holdingPeriod': 1, 'side': 'short'},
              {'exitDate': None, 'pnl': 999.0, 'exitReason': 'open', 'side': 'long'}]
    st = M.trade_stats(trades, 1000.0)
    assert st['closedTrades'] == 3 and st['openTrades'] == 1 and st['openPnl'] == 999.0
    assert st['winRate'] == pytest.approx(2 / 3) and st['profitFactor'] == pytest.approx(130 / 50)
    assert st['expectancy'] == pytest.approx(80 / 3) and st['netPnl'] == pytest.approx(80.0)
    assert st['shortWinRate'] == 1.0 and st['longWinRate'] == 0.5
    assert st['payoffRatio'] == pytest.approx(65 / 50)
    assert st['kellyFraction'] == pytest.approx(2 / 3 - (1 / 3) / (65 / 50))


def test_zero_trades_stats_are_null():
    st = M.trade_stats([], 1000.0)
    assert st['winRate'] is None and st['profitFactor'] is None and st['expectancy'] is None
    assert st['closedTrades'] == 0


# ── cross-checks against installed libraries (test-time only) ──────────────

def test_against_empyrical(spy):
    emp = pytest.importorskip('empyrical')
    eq = _curve(spy)
    r = eq.pct_change().dropna()
    b = spy['Close'].pct_change().dropna()
    rf = 0.03
    assert M.sharpe_ratio(r.values, rf / 252, 252) == pytest.approx(emp.sharpe_ratio(r, risk_free=rf / 252), rel=1e-9)
    assert M.sortino_ratio(r.values, rf / 252, 252) == pytest.approx(
        emp.sortino_ratio(r, required_return=rf / 252), rel=1e-9)
    assert M.volatility(r.values, 252) == pytest.approx(emp.annual_volatility(r), rel=1e-9)
    assert M.max_drawdown(eq.values) == pytest.approx(-emp.max_drawdown(r), rel=1e-9)
    assert M.annualized_return_bars(eq.values, 252) == pytest.approx(emp.cagr(r), rel=1e-9)
    rel = M.relative_metrics(r.values, b.values, 0.0, 252)
    assert rel['beta'] == pytest.approx(emp.beta(r, b), rel=1e-9)
    assert rel['upCapture'] == pytest.approx(emp.up_capture(r, b), rel=1e-9)
    assert rel['downCapture'] == pytest.approx(emp.down_capture(r, b), rel=1e-9)
    assert M.tail_ratio(r.values) == pytest.approx(emp.tail_ratio(r), rel=1e-9)
    assert M.omega_ratio(r.values, 0.0) == pytest.approx(emp.omega_ratio(r), rel=1e-9)


def test_moments_against_scipy(spy):
    st = pytest.importorskip('scipy.stats')
    r = spy['Close'].pct_change().dropna().values
    assert M.skewness(r) == pytest.approx(st.skew(r), rel=1e-10)
    assert M.excess_kurtosis(r) == pytest.approx(st.kurtosis(r), rel=1e-10)


def test_flat_curve_with_rf_series_is_undefined_not_absurd():
    d = pd.bdate_range('2024-01-01', periods=60)
    flat = np.full(60, 1e5)
    y = np.linspace(0.05, 0.051, 60)
    h = M.headline(flat, d, rf=y, ppy=252)
    assert h['sharpeRatio'] is None and h['sortinoRatio'] is None
    r = np.array([0.004, 0.006, 0.006, 0.004, 0.005])          # all gains: VaR is a positive return, kept as is
    assert M.value_at_risk(r, 0.95) > 0
