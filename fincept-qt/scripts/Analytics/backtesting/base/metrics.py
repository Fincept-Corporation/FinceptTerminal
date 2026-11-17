"""
ONE definition of every backtest metric, used by every engine (decision 1).

Engines do not compute Sharpe, CAGR, drawdown ... themselves any more: they hand an equity
curve (+ trades / positions / benchmark) to these functions, directly or through
result_normalizer.build_backtest_result(). Pure functions over numpy arrays / pandas
objects, vectorised (no per-bar Python loops), NaN-safe.

UNDEFINED VALUES. A metric that cannot be computed returns None, never 0 or NaN. Pass a dict
as `why=` to receive the reason under the metric's contract key:
    why = {}; s = sharpe_ratio(r, why=why)   ->  None, why == {'sharpeRatio': 'returns have no variation'}

DEFINITIONS (contract v2; n bars, r_t = E_t / E_{t-1} - 1 for t = 1..n-1, ppy = bars per year)
  totalReturn        E_end / E_0 - 1
  cagr               (E_end / E_0) ** (365.25 / calendar days between first and last bar) - 1
  annReturnBars      (E_end / E_0) ** (ppy / (n - 1)) - 1          (bar basis, diagnostic only)
  volatility         std(r, ddof=1) * sqrt(ppy)
  rf per bar         scalar annual rate rf -> rf / ppy (contract formula);
                     a yield SERIES y_t (annual fraction, e.g. ^IRX / 100) -> (1 + y_{t-1}) ** (days_t / 365) - 1
                     (the previous bar's yield accrued over the calendar days of the bar)
  sharpeRatio        mean(r - rf_bar) / std(r, ddof=1) * sqrt(ppy)   (undefined when r does not vary)
  sortinoRatio       mean(r - rf_bar) * sqrt(ppy) / sqrt(mean(min(r - rf_bar, 0) ** 2))   (all bars)
  downsideDeviation  sqrt(mean(min(r - rf_bar, 0) ** 2)) * sqrt(ppy)
  maxDrawdown        max_t (1 - E_t / max(E_0..E_t))     positive fraction; equity[].drawdown = E_t / peak - 1 <= 0
  maxDrawdownDuration      longest run of consecutive bars below a prior peak (bars)
  maxDrawdownDurationDays  longest peak -> recovery (or last bar) span in calendar days
  calmarRatio        cagr / maxDrawdown
  recoveryFactor     (E_end - E_0) / (maxDrawdown * E_0)
  beta               cov(r, b, ddof=1) / var(b, ddof=1) on the dates both series have (intersection)
  alpha (Jensen)     (mean(r - rf_bar) - beta * mean(b - rf_bar)) * ppy
  trackingError      std(r - b, ddof=1) * sqrt(ppy);  informationRatio = mean(r - b) * ppy / trackingError
  treynorRatio       (cagr - mean annual rf) / beta
  up/downCapture     annualised geometric return of r on bars with b > 0 (< 0) / the same of b (empyrical's)
  var95 / cvar95     historical 1-bar: 5th percentile of r (linear interpolation) / mean of r <= var95
                     (a return: negative = a loss; positive only when even the worst bars gained)
  omegaRatio         sum(max(r - rf_bar, 0)) / sum(max(rf_bar - r, 0))
  tailRatio          |p95(r) / p5(r)|
  skewness/kurtosis  population moments (scipy.stats.skew / kurtosis defaults, bias=True); kurtosis is EXCESS
  ulcerIndex         sqrt(mean(drawdown_pct ** 2)) in PERCENT POINTS (v1 unit, kept by contract rule R5);
                     ulcerIndexFraction is the same as a fraction
  winRate etc.       on CLOSED round trips; pnl is money NET of commission, slippage and borrow
  profitFactor       gross profit / gross loss (money); None when there is no losing trade
  expectancy         mean closed-trade pnl (money); expectancyPct = mean pnlPercent
  exposure           share of bars (at the close) with a position: entryBar <= t < exitBar
  annualTurnover     traded notional (buys + sells) / mean equity / years (calendar)
  periodsPerYear     252 daily bars without weekend dates, 365 with weekend dates (24x7 markets),
                     52 weekly, 12 monthly; intraday: measured bars per session day * 252 (or * 365)
"""

from __future__ import annotations

import math
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple

import numpy as np

TRADING_DAYS = 252.0
CALENDAR_DAYS = 365.25
_EPS = 1e-12


# ════════════════════════════════════════════════════════════════════════════
# Small helpers
# ════════════════════════════════════════════════════════════════════════════

def _note(why: Optional[dict], key: str, reason: str) -> None:
    if why is not None and key not in why:
        why[key] = reason


def finite_or_none(x) -> Optional[float]:
    """float(x) when finite, else None."""
    try:
        v = float(x)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


def as_array(x) -> np.ndarray:
    if x is None:
        return np.empty(0)
    if hasattr(x, 'to_numpy'):
        x = x.to_numpy()
    return np.asarray(x, dtype=float)


def to_datetime_index(dates):
    """pandas DatetimeIndex (tz-naive) of `dates` (strings, Timestamps, datetime64)."""
    import pandas as pd
    if dates is None:
        return None
    if isinstance(dates, pd.DatetimeIndex):
        idx = dates
    else:
        idx = pd.DatetimeIndex(pd.to_datetime(list(dates) if not isinstance(dates, np.ndarray) else dates))
    if idx.tz is not None:
        idx = idx.tz_localize(None)
    return idx


def simple_returns(equity) -> np.ndarray:
    """Per-bar simple returns r_t = E_t / E_{t-1} - 1, t = 1..n-1 (length n-1).
    A bar after equity reached <= 0 has an undefined return (NaN)."""
    e = as_array(equity)
    if len(e) < 2:
        return np.empty(0)
    prev = e[:-1]
    with np.errstate(divide='ignore', invalid='ignore'):
        r = np.where(prev > 0, e[1:] / prev - 1.0, np.nan)
    return r


def _clean(r: np.ndarray) -> np.ndarray:
    r = as_array(r)
    return r[np.isfinite(r)]


# ════════════════════════════════════════════════════════════════════════════
# Annualisation basis
# ════════════════════════════════════════════════════════════════════════════

def infer_periods_per_year(dates) -> float:
    """Bars per year of a date index (the annualisation basis).

    daily bars: 252 for an exchange calendar, 365 when at least 10 % of the bars fall on a
    Saturday / Sunday (24x7 markets: 2/7 = 29 %, or a union calendar that contains them). An
    exchange's occasional weekend session (NSE budget-day and Diwali muhurat sessions: a few bars in
    ten years) does not make it a 24x7 market - counting any weekend bar annualised RELIANCE.NS with
    365 instead of ~250 bars and inflated its volatility by 22 %. weekly 52; monthly 12; intraday:
    bars per distinct session day (median) * (252 or 365 by the same weekend rule).
    Fewer than 2 bars: 252 (nothing to annualise).
    """
    idx = to_datetime_index(dates)
    if idx is None or len(idx) < 2:
        return TRADING_DAYS
    secs = idx.asi8.astype('float64') / 1e9
    deltas = np.diff(secs)
    deltas = deltas[deltas > 0]
    if len(deltas) == 0:
        return TRADING_DAYS
    med = float(np.median(deltas))
    weekend = bool(np.mean(idx.dayofweek >= 5) >= 0.10)
    days_per_year = 365.0 if weekend else TRADING_DAYS
    if med >= 25 * 86400:
        return 12.0
    if med >= 6 * 86400:
        return 52.0
    if med >= 20 * 3600:
        return days_per_year
    # Intraday: bars per session day, measured.
    day = idx.normalize()
    _, counts = np.unique(day.asi8, return_counts=True)
    per_day = float(np.median(counts)) if len(counts) else 1.0
    return float(max(1.0, per_day) * days_per_year)


def describe_periods_per_year(ppy: float) -> str:
    if abs(ppy - 252.0) < 0.5:
        return '252 bars/year (daily bars on an exchange calendar)'
    if abs(ppy - 365.0) < 0.5:
        return '365 bars/year (daily bars including weekends: 24x7 market or mixed calendar)'
    if abs(ppy - 52.0) < 0.5:
        return '52 bars/year (weekly bars)'
    if abs(ppy - 12.0) < 0.5:
        return '12 bars/year (monthly bars)'
    return f'{ppy:,.0f} bars/year (intraday: measured bars per session x sessions per year)'


# ════════════════════════════════════════════════════════════════════════════
# Risk-free rate
# ════════════════════════════════════════════════════════════════════════════

def rf_per_bar(n_returns: int, rf: Any = 0.0, ppy: float = TRADING_DAYS, dates=None) -> np.ndarray:
    """Per-bar risk-free return for each of the n_returns bar returns.

    rf scalar (annual fraction)          -> rf / ppy for every bar (contract formula).
    rf array/Series of ANNUAL yields     -> aligned to the n_returns+1 bar dates (or to the
                                             n_returns returns): (1 + y_{t-1}) ** (days_t / 365) - 1.
    Non-finite yields are forward-filled, then 0.
    """
    if rf is None:
        return np.zeros(n_returns)
    if np.isscalar(rf):
        v = finite_or_none(rf) or 0.0
        return np.full(n_returns, v / float(ppy))
    y = as_array(rf)
    if len(y) == n_returns + 1:
        y_prev = y[:-1]
    elif len(y) == n_returns:
        y_prev = y
    else:
        raise ValueError(f'risk-free series has {len(y)} values for {n_returns} returns')
    y_prev = _ffill(y_prev)
    y_prev = np.where(np.isfinite(y_prev), y_prev, 0.0)
    if dates is not None:
        idx = to_datetime_index(dates)
        if len(idx) == n_returns + 1:
            days = np.diff(idx.asi8) / 86400e9
            return np.power(1.0 + y_prev, days / 365.0) - 1.0
    return y_prev / float(ppy)


def _ffill(a: np.ndarray) -> np.ndarray:
    a = np.asarray(a, dtype=float).copy()
    ok = np.isfinite(a)
    if ok.all() or not ok.any():
        return a
    idx = np.where(ok, np.arange(len(a)), 0)
    np.maximum.accumulate(idx, out=idx)
    out = a[idx]
    out[: np.argmax(ok)] = np.nan
    return out


def annual_rf_mean(rf_bar: np.ndarray, ppy: float) -> float:
    rb = _clean(rf_bar)
    return float(np.mean(rb) * ppy) if len(rb) else 0.0


# ════════════════════════════════════════════════════════════════════════════
# Return / risk metrics
# ════════════════════════════════════════════════════════════════════════════

def total_return(equity, why: Optional[dict] = None) -> Optional[float]:
    e = as_array(equity)
    if len(e) < 1 or not np.isfinite(e[0]) or e[0] <= 0 or not np.isfinite(e[-1]):
        _note(why, 'totalReturn', 'no valid starting equity')
        return None
    return float(e[-1] / e[0] - 1.0)


def cagr(equity, dates, why: Optional[dict] = None) -> Optional[float]:
    """Compound annual growth on CALENDAR time: (E_end/E_0) ** (365.25 / days) - 1."""
    e = as_array(equity)
    idx = to_datetime_index(dates)
    if len(e) < 2 or idx is None or len(idx) != len(e):
        _note(why, 'cagr', 'fewer than 2 bars')
        return None
    days = (idx[-1] - idx[0]).total_seconds() / 86400.0
    if days <= 0:
        _note(why, 'cagr', 'the period spans no calendar time')
        return None
    if e[0] <= 0 or not np.isfinite(e[0]):
        _note(why, 'cagr', 'no valid starting equity')
        return None
    if e[-1] <= 0:
        return -1.0
    return float((e[-1] / e[0]) ** (CALENDAR_DAYS / days) - 1.0)


def annualized_return_bars(equity, ppy: float = TRADING_DAYS, why: Optional[dict] = None) -> Optional[float]:
    """Bar-basis annual return (E_end/E_0) ** (ppy / (n-1)) - 1 (diagnostic; headline uses cagr)."""
    e = as_array(equity)
    if len(e) < 2 or e[0] <= 0:
        _note(why, 'annReturnBars', 'fewer than 2 bars')
        return None
    if e[-1] <= 0:
        return -1.0
    return float((e[-1] / e[0]) ** (ppy / (len(e) - 1)) - 1.0)


def volatility(returns, ppy: float = TRADING_DAYS, why: Optional[dict] = None) -> Optional[float]:
    r = _clean(returns)
    if len(r) < 2:
        _note(why, 'volatility', 'fewer than 2 bar returns')
        return None
    return float(np.std(r, ddof=1) * math.sqrt(ppy))


def _excess(returns, rf_bar, with_raw: bool = False):
    r = as_array(returns)
    rb = np.zeros_like(r) if rf_bar is None else (np.full_like(r, float(rf_bar)) if np.isscalar(rf_bar) else as_array(rf_bar))
    ok = np.isfinite(r) & np.isfinite(rb)
    return ((r - rb)[ok], r[ok]) if with_raw else (r - rb)[ok]


def sharpe_ratio(returns, rf_bar=0.0, ppy: float = TRADING_DAYS, why: Optional[dict] = None) -> Optional[float]:
    """mean(r - rf_bar) / std(r, ddof=1) * sqrt(ppy) (contract definition). rf_bar: per-bar rf
    (scalar or array). Undefined when the strategy's own returns do not vary (a flat curve would
    otherwise get an absurd ratio from the risk-free series' own wiggle)."""
    ex, raw = _excess(returns, rf_bar, with_raw=True)
    if len(ex) < 2:
        _note(why, 'sharpeRatio', 'fewer than 2 bar returns')
        return None
    sd = float(np.std(raw, ddof=1))
    if not sd > _EPS:
        _note(why, 'sharpeRatio', 'returns have no variation (flat equity)')
        return None
    return float(np.mean(ex) / sd * math.sqrt(ppy))


def downside_deviation(returns, rf_bar=0.0, ppy: float = TRADING_DAYS, why: Optional[dict] = None) -> Optional[float]:
    ex = _excess(returns, rf_bar)
    if len(ex) < 2:
        _note(why, 'downsideDeviation', 'fewer than 2 bar returns')
        return None
    return float(math.sqrt(float(np.mean(np.minimum(ex, 0.0) ** 2))) * math.sqrt(ppy))


def sortino_ratio(returns, rf_bar=0.0, ppy: float = TRADING_DAYS, why: Optional[dict] = None) -> Optional[float]:
    """mean(r - rf_bar) * sqrt(ppy) / sqrt(mean(min(r - rf_bar, 0)^2)) (downside over ALL bars)."""
    ex, raw = _excess(returns, rf_bar, with_raw=True)
    if len(ex) < 2:
        _note(why, 'sortinoRatio', 'fewer than 2 bar returns')
        return None
    if not float(np.std(raw, ddof=1)) > _EPS:
        _note(why, 'sortinoRatio', 'returns have no variation (flat equity)')
        return None
    dn = math.sqrt(float(np.mean(np.minimum(ex, 0.0) ** 2)))
    if not dn > _EPS:
        _note(why, 'sortinoRatio', 'no bar returned less than the risk-free rate')
        return None
    return float(np.mean(ex) * math.sqrt(ppy) / dn)


def drawdown_series(equity) -> np.ndarray:
    """E_t / running peak - 1 (<= 0). Bars before the first positive equity read 0."""
    e = as_array(equity)
    if len(e) == 0:
        return e
    peak = np.maximum.accumulate(np.where(np.isfinite(e), e, -np.inf))
    with np.errstate(divide='ignore', invalid='ignore'):
        dd = np.where(peak > 0, e / peak - 1.0, 0.0)
    return np.minimum(np.where(np.isfinite(dd), dd, 0.0), 0.0)


def max_drawdown(equity, why: Optional[dict] = None) -> Optional[float]:
    dd = drawdown_series(equity)
    if len(dd) == 0:
        _note(why, 'maxDrawdown', 'empty equity curve')
        return None
    return float(-np.min(dd))


def _underwater_segments(dd: np.ndarray) -> List[Tuple[int, int]]:
    """[(start, end_exclusive)] of maximal runs with dd < 0."""
    under = dd < -_EPS
    if not under.any():
        return []
    d = np.diff(np.concatenate([[0], under.astype(np.int8), [0]]))
    starts = np.flatnonzero(d == 1)
    ends = np.flatnonzero(d == -1)
    return list(zip(starts.tolist(), ends.tolist()))


def drawdown_durations(equity, dates=None) -> Dict[str, Optional[int]]:
    """{'bars': longest run of bars under water, 'days': longest peak->recovery (or end) span in days}."""
    dd = drawdown_series(equity)
    segs = _underwater_segments(dd)
    if not segs:
        return {'bars': 0, 'days': 0 if dates is not None else None}
    bars = max(e - s for s, e in segs)
    days = None
    idx = to_datetime_index(dates) if dates is not None else None
    if idx is not None and len(idx) == len(dd):
        n = len(dd)
        spans = []
        for s, e in segs:
            peak_i = max(0, s - 1)
            end_i = e if e < n else n - 1
            spans.append((idx[end_i] - idx[peak_i]).days)
        days = int(max(spans))
    return {'bars': int(bars), 'days': days}


def drawdown_table(equity, dates, top: int = 10) -> List[Dict[str, Any]]:
    """Top drawdown episodes by depth (contract `drawdowns[]`), vectorised.

    peak = last bar at the running peak before the episode; trough = deepest bar;
    recovery = first bar back at (or above) the peak, None when still under water.
    """
    e = as_array(equity)
    dd = drawdown_series(e)
    segs = _underwater_segments(dd)
    if not segs:
        return []
    idx = to_datetime_index(dates)
    n = len(e)
    starts = np.array([s for s, _ in segs])
    ends = np.array([x for _, x in segs])
    mins = np.minimum.reduceat(dd, starts)
    # argmin per segment
    troughs = np.array([s + int(np.argmin(dd[s:x])) for s, x in segs])
    order = np.argsort(mins, kind='stable')[:top]
    out = []
    for rank, k in enumerate(order, 1):
        s, x, tr = int(starts[k]), int(ends[k]), int(troughs[k])
        peak = max(0, s - 1)
        rec = x if x < n else None
        end_i = rec if rec is not None else n - 1
        out.append({
            'rank': rank,
            'depth': float(-mins[k]),
            'peakDate': _fmt_date(idx[peak]),
            'troughDate': _fmt_date(idx[tr]),
            'recoveryDate': _fmt_date(idx[rec]) if rec is not None else None,
            'toTroughBars': int(tr - peak),
            'recoveryBars': int(rec - tr) if rec is not None else None,
            'durationBars': int(end_i - peak),
            'durationDays': int((idx[end_i] - idx[peak]).days),
            'peakEquity': float(e[peak]),
            'troughEquity': float(e[tr]),
            'peakIdx': peak,
            'troughIdx': tr,
            'recoveryIdx': rec,
        })
    return out


def drawdown_analysis_v1(equity, dates) -> Dict[str, Any]:
    """The v1 `drawdownAnalysis` block the screen reads today (deepestDrawdowns with peakIdx /
    valleyIdx / depth / duration, counts and averages), for engines that do not send one."""
    e = as_array(equity)
    dd = drawdown_series(e)
    segs = _underwater_segments(dd)
    n = len(e)
    if not segs:
        return {'drawdownCount': 0, 'avgDrawdown': None, 'avgDuration': None, 'avgRecoveryDuration': None,
                'longestRecovery': None, 'activeDrawdown': 0.0, 'activeDuration': 0, 'deepestDrawdowns': []}
    depths = np.array([-float(np.min(dd[s:x])) for s, x in segs])
    durations = np.array([x - max(0, s - 1) for s, x in segs], dtype=float)
    rec_durs = []
    for s, x in segs:
        if x < n:
            tr = s + int(np.argmin(dd[s:x]))
            rec_durs.append(x - tr)
    active = segs[-1][1] >= n
    table = drawdown_table(e, dates, top=10)
    return {
        'drawdownCount': len(segs),
        'avgDrawdown': float(np.mean(depths)),
        'avgDuration': float(np.mean(durations)),
        'avgRecoveryDuration': float(np.mean(rec_durs)) if rec_durs else None,
        'longestRecovery': int(max(rec_durs)) if rec_durs else None,
        'activeDrawdown': float(-dd[-1]) if active else 0.0,
        'activeDuration': int(n - 1 - max(0, segs[-1][0] - 1)) if active else 0,
        'deepestDrawdowns': [{'peakIdx': d['peakIdx'], 'valleyIdx': d['troughIdx'], 'recoveryIdx': d['recoveryIdx'],
                              'depth': d['depth'], 'duration': d['durationBars']} for d in table],
    }


def calmar_ratio(cagr_value: Optional[float], mdd: Optional[float], why: Optional[dict] = None) -> Optional[float]:
    if cagr_value is None or mdd is None:
        _note(why, 'calmarRatio', 'CAGR or max drawdown undefined')
        return None
    if mdd <= _EPS:
        _note(why, 'calmarRatio', 'no drawdown')
        return None
    return float(cagr_value / mdd)


def ulcer_index(equity) -> Optional[float]:
    """In PERCENT POINTS (v1 unit)."""
    dd = drawdown_series(equity)
    if len(dd) == 0:
        return None
    return float(math.sqrt(float(np.mean((dd * 100.0) ** 2))))


def value_at_risk(returns, level: float = 0.95) -> Optional[float]:
    r = _clean(returns)
    if len(r) < 2:
        return None
    return float(np.percentile(r, (1.0 - level) * 100.0))


def conditional_var(returns, level: float = 0.95) -> Optional[float]:
    r = _clean(returns)
    if len(r) < 2:
        return None
    v = np.percentile(r, (1.0 - level) * 100.0)
    tail = r[r <= v]
    return float(np.mean(tail)) if len(tail) else float(v)


def omega_ratio(returns, rf_bar=0.0) -> Optional[float]:
    ex = _excess(returns, rf_bar)
    if len(ex) == 0:
        return None
    loss = float(-ex[ex < 0].sum())
    if not loss > 0:
        return None
    return float(ex[ex > 0].sum() / loss)


def tail_ratio(returns) -> Optional[float]:
    r = _clean(returns)
    if len(r) < 2:
        return None
    p5 = float(np.percentile(r, 5))
    if abs(p5) <= 1e-10:
        return None
    return float(abs(float(np.percentile(r, 95)) / p5))


def skewness(returns) -> Optional[float]:
    """Population skewness m3 / m2^1.5 (scipy.stats.skew, bias=True)."""
    r = _clean(returns)
    if len(r) < 3:
        return None
    d = r - r.mean()
    m2 = float(np.mean(d ** 2))
    if m2 <= 1e-20:
        return None
    return float(np.mean(d ** 3) / m2 ** 1.5)


def excess_kurtosis(returns) -> Optional[float]:
    """Population excess kurtosis m4 / m2^2 - 3 (scipy.stats.kurtosis, fisher=True, bias=True)."""
    r = _clean(returns)
    if len(r) < 4:
        return None
    d = r - r.mean()
    m2 = float(np.mean(d ** 2))
    if m2 <= 1e-20:
        return None
    return float(np.mean(d ** 4) / m2 ** 2 - 3.0)


def histogram(returns, bins: int = 20) -> List[Dict[str, Any]]:
    """v1 `returnsHistogram` rows {bin (centre), count} + lo / hi edges (additive)."""
    r = _clean(returns)
    if len(r) < 2:
        return []
    counts, edges = np.histogram(r, bins=bins)
    return [{'bin': round(float((edges[i] + edges[i + 1]) / 2), 6), 'count': int(counts[i]),
             'lo': float(edges[i]), 'hi': float(edges[i + 1])} for i in range(len(counts))]


# ════════════════════════════════════════════════════════════════════════════
# Benchmark-relative
# ════════════════════════════════════════════════════════════════════════════

def align_on_dates(eq_dates, equity, bench_dates, bench_values):
    """Equity and benchmark on the dates BOTH have (intersection, no filling).

    Returns (dates, equity_aligned, bench_aligned). Session dates are compared as given
    (daily bars: normalised dates; intraday: wall time)."""
    import pandas as pd
    e = pd.Series(as_array(equity), index=to_datetime_index(eq_dates))
    b = pd.Series(as_array(bench_values), index=to_datetime_index(bench_dates))
    e = e[~e.index.duplicated(keep='last')]
    b = b[~b.index.duplicated(keep='last')]
    common = e.index.intersection(b.index)
    e2, b2 = e.reindex(common), b.reindex(common)
    ok = np.isfinite(e2.values) & np.isfinite(b2.values) & (b2.values > 0)
    return common[ok], e2.values[ok], b2.values[ok]


def relative_metrics(r, b, rf_bar=0.0, ppy: float = TRADING_DAYS, cagr_value: Optional[float] = None,
                     rf_annual: float = 0.0, why: Optional[dict] = None) -> Dict[str, Optional[float]]:
    """alpha, beta, trackingError, informationRatio, correlation, rSquared, treynorRatio,
    upCapture, downCapture from paired per-bar returns (same length, already aligned)."""
    r = as_array(r)
    b = as_array(b)
    rb = np.zeros_like(r) if np.isscalar(rf_bar) and float(rf_bar) == 0 else (
        np.full_like(r, float(rf_bar)) if np.isscalar(rf_bar) else as_array(rf_bar))
    ok = np.isfinite(r) & np.isfinite(b) & np.isfinite(rb)
    r, b, rb = r[ok], b[ok], rb[ok]
    out = {k: None for k in ('alpha', 'beta', 'trackingError', 'informationRatio', 'correlation', 'rSquared',
                             'treynorRatio', 'upCapture', 'downCapture')}
    if len(r) < 3:
        for k in out:
            _note(why, k, 'fewer than 3 bars shared with the benchmark')
        return out
    vb = float(np.var(b, ddof=1))
    if vb > 1e-20:
        beta = float(np.cov(r, b, ddof=1)[0, 1] / vb)
        out['beta'] = beta
        out['alpha'] = float((np.mean(r - rb) - beta * np.mean(b - rb)) * ppy)
        if cagr_value is not None and abs(beta) > 1e-6:
            out['treynorRatio'] = float((cagr_value - rf_annual) / beta)
    else:
        _note(why, 'beta', 'the benchmark did not move')
    act = r - b
    te = float(np.std(act, ddof=1) * math.sqrt(ppy))
    out['trackingError'] = te
    if te > 1e-12:
        out['informationRatio'] = float(np.mean(act) * ppy / te)
    else:
        _note(why, 'informationRatio', 'strategy equals the benchmark')
    if np.std(r) > 1e-20 and vb > 1e-20:
        c = float(np.corrcoef(r, b)[0, 1])
        if math.isfinite(c):
            out['correlation'] = c
            out['rSquared'] = c * c
    out['upCapture'] = _capture(r, b, b > 0, ppy)
    out['downCapture'] = _capture(r, b, b < 0, ppy)
    return out


def _capture(r, b, mask, ppy) -> Optional[float]:
    if mask.sum() < 2:
        return None

    def ann(x):
        g = float(np.prod(1.0 + x))
        return g ** (ppy / len(x)) - 1.0 if g > 0 else -1.0
    db = ann(b[mask])
    if abs(db) < 1e-12:
        return None
    return float(ann(r[mask]) / db)


# ════════════════════════════════════════════════════════════════════════════
# Calendar tables, rolling series, period statistics
# ════════════════════════════════════════════════════════════════════════════

def _fmt_date(ts) -> str:
    if ts.hour or ts.minute or ts.second:
        return ts.strftime('%Y-%m-%d %H:%M:%S')
    return ts.strftime('%Y-%m-%d')


def _period_returns(equity, dates, rule: str):
    """Compounded return of each calendar period ('W-FRI', 'ME', 'YE'); the first period
    is measured from the first bar's equity."""
    import pandas as pd
    s = pd.Series(as_array(equity), index=to_datetime_index(dates))
    s = s[~s.index.duplicated(keep='last')]
    last = s.resample(rule).last().dropna()
    if len(last) == 0:
        return last
    prev = last.shift(1)
    prev.iloc[0] = s.iloc[0]
    return (last / prev - 1.0).replace([np.inf, -np.inf], np.nan)


def monthly_table(equity, dates) -> List[Dict[str, Any]]:
    """[{year, months[12] (fraction|None), yearTotal}] - compounding to the total return."""
    m = _period_returns(equity, dates, 'ME')
    if len(m) == 0:
        return []
    rows = {}
    for ts, v in m.items():
        rows.setdefault(ts.year, [None] * 12)[ts.month - 1] = None if not np.isfinite(v) else round(float(v), 8)
    out = []
    for y in sorted(rows):
        vals = [x for x in rows[y] if x is not None]
        out.append({'year': int(y), 'months': rows[y],
                    'yearTotal': round(float(np.prod([1 + x for x in vals]) - 1.0), 8) if vals else None})
    return out


def yearly_table(equity, dates, bench_equity=None, bench_dates=None) -> List[Dict[str, Any]]:
    y = _period_returns(equity, dates, 'YE')
    yb = _period_returns(bench_equity, bench_dates if bench_dates is not None else dates, 'YE') \
        if bench_equity is not None else None
    out = []
    for ts, v in y.items():
        b = None
        if yb is not None:
            hit = yb[yb.index.year == ts.year]
            b = finite_or_none(hit.iloc[0]) if len(hit) else None
        s = finite_or_none(v)
        out.append({'year': int(ts.year), 'strategy': s, 'benchmark': b,
                    'excess': (s - b) if (s is not None and b is not None) else None})
    return out


def default_rolling_window(ppy: float) -> int:
    """Half a year of bars (126 for daily equities), at least 20."""
    return int(max(20, round(ppy / 2.0)))


def rolling_series(equity, dates, window: int, rf_bar=0.0, ppy: float = TRADING_DAYS,
                   bench_returns=None, digits: int = 6) -> Dict[str, List[Dict[str, Any]]]:
    """Rolling Sharpe / volatility / beta over `window` BAR RETURNS, each value dated at the
    LAST bar of its window (fixes B2: v1 stamped one bar late and dropped the last window),
    plus the drawdown series (v1 rollingDrawdown). Returns {name: [{date, value}]}."""
    import pandas as pd
    idx = to_datetime_index(dates)
    e = as_array(equity)
    r = simple_returns(e)
    ridx = idx[1:]
    rb = rf_per_bar(len(r), rf_bar, ppy) if np.isscalar(rf_bar) else as_array(rf_bar)
    rs = pd.Series(r, index=ridx)
    ex = pd.Series(r - rb, index=ridx)
    mean = ex.rolling(window, min_periods=window).mean()
    sd = ex.rolling(window, min_periods=window).std(ddof=1)
    sharpe = (mean / sd.where(sd > _EPS) * math.sqrt(ppy))
    vol = rs.rolling(window, min_periods=window).std(ddof=1) * math.sqrt(ppy)
    dates_s = [_fmt_date(t) for t in ridx]

    def rows(series):
        v = series.values
        ok = np.isfinite(v)
        return [{'date': dates_s[i], 'value': round(float(v[i]), digits)} for i in np.flatnonzero(ok)]
    out = {'rollingSharpe': rows(sharpe), 'rollingVolatility': rows(vol)}
    if bench_returns is not None:
        bs = pd.Series(as_array(bench_returns), index=ridx)
        cov = rs.rolling(window, min_periods=window).cov(bs)
        var = bs.rolling(window, min_periods=window).var()
        out['rollingBeta'] = rows(cov / var.where(var > 1e-20))
    dd = drawdown_series(e)
    all_dates = [_fmt_date(t) for t in idx]
    out['rollingDrawdown'] = [{'date': all_dates[i], 'value': round(float(dd[i]), digits)} for i in range(len(dd))]
    return out


def _longest_streak(mask: np.ndarray) -> int:
    if len(mask) == 0 or not mask.any():
        return 0
    d = np.diff(np.concatenate([[0], mask.astype(np.int8), [0]]))
    return int(np.max(np.flatnonzero(d == -1) - np.flatnonzero(d == 1)))


def period_stats(equity, dates) -> Dict[str, Any]:
    """Contract `periodStats`: best/worst day (bar), calendar week / month / year, positive
    shares, average up/down bar, longest win/loss streaks (bars)."""
    r = _clean(simple_returns(equity))
    out: Dict[str, Any] = {k: None for k in ('bestDay', 'worstDay', 'bestWeek', 'worstWeek', 'bestMonth', 'worstMonth',
                                              'bestYear', 'worstYear', 'positiveDays', 'positiveMonths', 'avgUpDay',
                                              'avgDownDay', 'longestWinStreak', 'longestLossStreak')}
    if len(r) == 0:
        return out
    out['bestDay'], out['worstDay'] = float(r.max()), float(r.min())
    out['positiveDays'] = float(np.mean(r > 0))
    up, dn = r[r > 0], r[r < 0]
    out['avgUpDay'] = float(up.mean()) if len(up) else None
    out['avgDownDay'] = float(dn.mean()) if len(dn) else None
    out['longestWinStreak'] = _longest_streak(r > 0)
    out['longestLossStreak'] = _longest_streak(r < 0)
    for rule, a, b in (('W-FRI', 'bestWeek', 'worstWeek'), ('ME', 'bestMonth', 'worstMonth'),
                       ('YE', 'bestYear', 'worstYear')):
        p = _period_returns(equity, dates, rule).dropna()
        if len(p):
            out[a], out[b] = float(p.max()), float(p.min())
            if rule == 'ME':
                out['positiveMonths'] = float(np.mean(p.values > 0))
    return out


def day_statistics(equity, dates) -> Dict[str, Any]:
    """v1 `statistics` day figures computed from the curve (fincept sent zeros: F4)."""
    e = as_array(equity)
    r = simple_returns(e)
    rc = _clean(r)
    idx = to_datetime_index(dates)
    out = {'winningDays': int(np.sum(rc > 0)), 'losingDays': int(np.sum(rc < 0)),
           'averageDailyReturn': float(rc.mean()) if len(rc) else None,
           'bestDay': float(rc.max()) if len(rc) else None, 'worstDay': float(rc.min()) if len(rc) else None,
           'consecutiveWins': _longest_streak(rc > 0), 'consecutiveLosses': _longest_streak(rc < 0),
           'maxDrawdownDate': None, 'recoveryTime': None}
    table = drawdown_table(e, idx, top=1)
    if table:
        out['maxDrawdownDate'] = table[0]['troughDate']
        out['recoveryTime'] = table[0]['recoveryBars']
    return out


# ════════════════════════════════════════════════════════════════════════════
# Trades, exposure, turnover, costs, attribution
# ════════════════════════════════════════════════════════════════════════════

def _num(t: dict, k: str) -> Optional[float]:
    return finite_or_none(t.get(k))


def is_closed(t: dict) -> bool:
    return bool(t.get('exitDate')) and _num(t, 'pnl') is not None and str(t.get('exitReason') or '') != 'open'


def trade_stats(trades: Sequence[dict], initial_capital: Optional[float]) -> Dict[str, Any]:
    """Uniform trade summary (contract `tradeStats`). Win rate, profit factor, expectancy ... on
    CLOSED round trips; open positions are counted (openTrades) and their marked P&L reported
    separately (openPnl) - they are not wins or losses yet."""
    closed = [t for t in trades if is_closed(t)]
    opened = [t for t in trades if not is_closed(t)]
    pnl = np.array([_num(t, 'pnl') for t in closed], dtype=float)
    pct = np.array([_num(t, 'pnlPercent') if _num(t, 'pnlPercent') is not None else np.nan for t in closed],
                   dtype=float)
    hold = np.array([_num(t, 'holdingPeriod') if _num(t, 'holdingPeriod') is not None else np.nan for t in closed],
                    dtype=float)
    n = len(closed)
    win, loss = pnl > 0, pnl < 0
    gp, gl = float(pnl[win].sum()) if n else 0.0, float(-pnl[loss].sum()) if n else 0.0

    def m(a):
        a = a[np.isfinite(a)]
        return float(a.mean()) if len(a) else None

    def streak(mask):
        return _longest_streak(mask) if n else 0
    avg_w, avg_l = m(pnl[win]), m(pnl[loss])
    sides = [str(t.get('side') or 'long') for t in closed]
    longs = np.array([s == 'long' for s in sides], dtype=bool)
    shorts = ~longs if n else longs
    mae = np.array([_num(t, 'mae') for t in closed if _num(t, 'mae') is not None], dtype=float)
    mfe = np.array([_num(t, 'mfe') for t in closed if _num(t, 'mfe') is not None], dtype=float)
    win_rate = float(win.mean()) if n else None
    payoff = (avg_w / -avg_l) if (avg_w is not None and avg_l) else None
    sd = float(np.std(pnl, ddof=1)) if n > 1 else 0.0
    open_pnl = [x for x in (_num(t, 'pnl') for t in opened) if x is not None]
    return {
        'closedTrades': n,
        'openTrades': len(opened),
        'winningTrades': int(win.sum()),
        'losingTrades': int(loss.sum()),
        'winRate': win_rate,
        'grossProfit': gp,
        'grossLoss': -gl,
        'netPnl': float(pnl.sum()) if n else 0.0,
        'netPnlPct': float(pnl.sum() / initial_capital) if (initial_capital and n) else (0.0 if initial_capital else None),
        'profitFactor': (gp / gl) if gl > 0 else None,
        'profitFactorInfinite': bool(gl == 0 and gp > 0),
        'avgWin': avg_w,
        'avgLoss': avg_l,
        'avgWinPct': m(pct[win]) if n else None,
        'avgLossPct': m(pct[loss]) if n else None,
        'largestWin': float(pnl[win].max()) if win.any() else None,
        'largestLoss': float(pnl[loss].min()) if loss.any() else None,
        'payoffRatio': payoff,
        'expectancy': m(pnl) if n else None,
        'expectancyPct': m(pct) if n else None,
        'avgHoldingBars': m(hold) if n else None,
        'avgWinHoldingBars': m(hold[win]) if n else None,
        'avgLossHoldingBars': m(hold[loss]) if n else None,
        'maxConsecutiveWins': streak(win),
        'maxConsecutiveLosses': streak(loss),
        'totalCommission': float(sum(abs(_num(t, 'commission') or 0.0) for t in trades)),
        'longTrades': int(longs.sum()) if n else 0,
        'shortTrades': int(shorts.sum()) if n else 0,
        'longWinRate': float(win[longs].mean()) if n and longs.any() else None,
        'shortWinRate': float(win[shorts].mean()) if n and shorts.any() else None,
        'avgMae': float(mae.mean()) if len(mae) else None,
        'avgMfe': float(mfe.mean()) if len(mfe) else None,
        'worstMae': float(mae.min()) if len(mae) else None,
        'bestMfe': float(mfe.max()) if len(mfe) else None,
        'sqn': float(math.sqrt(n) * pnl.mean() / sd) if n > 1 and sd > 0 else None,
        'kellyFraction': (float(win_rate - (1 - win_rate) / payoff) if (win_rate is not None and payoff) else None),
        'openPnl': float(sum(open_pnl)) if open_pnl else (0.0 if not opened else None),
    }


def exposure_from_trades(trades: Sequence[dict], n_bars: int) -> Dict[str, Any]:
    """Positions held at each bar's close from trades' entryBar / exitBar (entryBar <= t < exitBar;
    open trades to the last bar). Returns {'count': int array, 'bySymbol': {sym: bool array}}."""
    count = np.zeros(n_bars, dtype=np.int32)
    by_sym: Dict[str, np.ndarray] = {}
    for t in trades:
        a = t.get('entryBar')
        b = t.get('exitBar')
        if a is None:
            continue
        a = int(a)
        b = n_bars if (b is None or not t.get('exitDate')) else int(b)
        if b <= a:
            continue
        a, b = max(0, a), min(n_bars, b)
        count[a:b] += 1
        sym = str(t.get('symbol') or '')
        m = by_sym.setdefault(sym, np.zeros(n_bars, dtype=bool))
        m[a:b] = True
    return {'count': count, 'bySymbol': by_sym}


def exposure_summary(n_positions: np.ndarray, gross: Optional[np.ndarray] = None,
                     net: Optional[np.ndarray] = None) -> Dict[str, Optional[float]]:
    """Contract `exposure` block. n_positions: positions held at each bar's close; gross / net:
    position value / equity per bar (None when not known)."""
    cnt = as_array(n_positions)
    out = {'timeInMarket': None, 'avgGrossExposure': None, 'maxGrossExposure': None, 'avgNetExposure': None,
           'avgPositions': None, 'maxPositions': None, 'longShare': None, 'shortShare': None}
    if len(cnt):
        out['timeInMarket'] = float(np.mean(cnt > 0))
        out['avgPositions'] = float(np.mean(cnt))
        out['maxPositions'] = float(np.max(cnt))
    if gross is not None and len(as_array(gross)):
        g = _clean(gross)
        if len(g):
            out['avgGrossExposure'] = float(g.mean())
            out['maxGrossExposure'] = float(g.max())
    if net is not None and len(as_array(net)):
        nn = as_array(net)
        g2 = as_array(gross) if gross is not None else None
        nc = _clean(nn)
        if len(nc):
            out['avgNetExposure'] = float(nc.mean())
        if g2 is not None and len(g2) == len(nn):
            long_v = (g2 + nn) / 2.0
            short_v = (g2 - nn) / 2.0
            tot = np.nansum(long_v) + np.nansum(short_v)
            if tot > 0:
                out['longShare'] = float(np.nansum(long_v) / tot)
                out['shortShare'] = float(np.nansum(short_v) / tot)
    return out


def annual_turnover(traded_notional: float, equity, dates) -> Optional[float]:
    e = _clean(equity)
    idx = to_datetime_index(dates)
    if len(e) < 2 or idx is None or len(idx) < 2:
        return None
    years = (idx[-1] - idx[0]).total_seconds() / 86400.0 / CALENDAR_DAYS
    me = float(np.mean(e))
    if years <= 0 or me <= 0:
        return None
    return float(traded_notional / me / years)


def cost_drag(equity, dates, total_costs: Optional[float]) -> Optional[float]:
    """Annualised return lost to costs: CAGR of (equity + cumulative costs at the end) minus CAGR."""
    if total_costs is None:
        return None
    e = as_array(equity)
    if len(e) < 2:
        return None
    net = cagr(e, dates)
    gross_e = e.copy()
    gross_e[-1] = e[-1] + float(total_costs)
    gross = cagr(gross_e, dates)
    if net is None or gross is None:
        return None
    return float(gross - net)


def attribution(trades: Sequence[dict], initial_capital: float, n_bars: Optional[int] = None) -> List[Dict[str, Any]]:
    """Per-symbol contribution (contract `attribution[]`); open positions count at their mark."""
    by: Dict[str, List[dict]] = {}
    for t in trades:
        by.setdefault(str(t.get('symbol') or ''), []).append(t)
    expo = exposure_from_trades(trades, n_bars) if n_bars else None
    out = []
    for sym, ts in by.items():
        closed = [t for t in ts if is_closed(t)]
        pnl_all = [x for x in (_num(t, 'pnl') for t in ts) if x is not None]
        gross = [x for x in (_num(t, 'grossPnl') for t in ts) if x is not None]
        costs = sum(abs(_num(t, 'commission') or 0.0) + abs(_num(t, 'slippageCost') or 0.0) +
                    abs(_num(t, 'borrowCost') or 0.0) for t in ts)
        holds = [x for x in (_num(t, 'holdingPeriod') for t in closed) if x is not None]
        wins = sum(1 for t in closed if (_num(t, 'pnl') or 0) > 0)
        net = float(sum(pnl_all))
        tim = None
        if expo is not None and sym in expo['bySymbol']:
            tim = float(np.mean(expo['bySymbol'][sym]))
        out.append({'symbol': sym, 'trades': len(ts), 'winRate': (wins / len(closed)) if closed else None,
                    'netPnl': net, 'grossPnl': float(sum(gross)) if len(gross) == len(ts) and gross else None,
                    'costs': float(costs), 'contribution': (net / initial_capital) if initial_capital else None,
                    'avgHoldingBars': float(np.mean(holds)) if holds else None, 'timeInMarket': tim})
    out.sort(key=lambda x: -abs(x['netPnl']))
    return out


# ════════════════════════════════════════════════════════════════════════════
# Headline block
# ════════════════════════════════════════════════════════════════════════════

def headline(equity, dates, *, rf: Any = 0.0, ppy: Optional[float] = None, bench_equity=None,
             bench_dates=None, why: Optional[dict] = None) -> Dict[str, Any]:
    """Every headline metric of contract `performance` that depends only on the curves.

    equity / dates: the strategy curve. rf: annual scalar or annual-yield series aligned to
    `dates`. bench_equity / bench_dates: benchmark closes (any scale); aligned on the
    intersection with the strategy dates. Returns a dict with contract keys (+ 'periodsPerYear',
    'rfAnnual', 'benchmarkBars').
    """
    why = why if why is not None else {}
    e = as_array(equity)
    idx = to_datetime_index(dates)
    ppy = float(ppy) if ppy else infer_periods_per_year(idx)
    r = simple_returns(e)
    rb = rf_per_bar(len(r), rf, ppy, idx)
    out: Dict[str, Any] = {'periodsPerYear': ppy, 'rfAnnual': annual_rf_mean(rb, ppy)}
    out['totalReturn'] = total_return(e, why)
    out['cagr'] = cagr(e, idx, why)
    out['annualizedReturn'] = out['cagr']
    out['annReturnBars'] = annualized_return_bars(e, ppy, why)
    out['volatility'] = volatility(r, ppy, why)
    out['sharpeRatio'] = sharpe_ratio(r, rb, ppy, why)
    out['sortinoRatio'] = sortino_ratio(r, rb, ppy, why)
    out['downsideDeviation'] = downside_deviation(r, rb, ppy, why)
    out['maxDrawdown'] = max_drawdown(e, why)
    dur = drawdown_durations(e, idx)
    out['maxDrawdownDuration'] = dur['bars']
    out['maxDrawdownDurationDays'] = dur['days']
    out['calmarRatio'] = calmar_ratio(out['cagr'], out['maxDrawdown'], why)
    mdd = out['maxDrawdown']
    out['recoveryFactor'] = (float((e[-1] - e[0]) / (mdd * e[0])) if (mdd and mdd > _EPS and len(e) and e[0] > 0)
                             else None)
    if out['recoveryFactor'] is None:
        _note(why, 'recoveryFactor', 'no drawdown')
    rel_keys = ('alpha', 'beta', 'informationRatio', 'trackingError', 'treynorRatio', 'correlation', 'rSquared',
                'upCapture', 'downCapture')
    for k in rel_keys:
        out[k] = None
    out['benchmarkBars'] = 0
    if bench_equity is not None:
        bd, ea, ba = align_on_dates(idx, e, bench_dates if bench_dates is not None else idx, bench_equity)
        out['benchmarkBars'] = int(len(bd))
        if len(bd) >= 3:
            ra, rba = simple_returns(ea), simple_returns(ba)
            rfa = rf_per_bar(len(ra), rf, ppy, bd) if np.isscalar(rf) else _rf_on(rf, idx, bd, ppy)
            rel = relative_metrics(ra, rba, rfa, ppy, cagr_value=out['cagr'], rf_annual=out['rfAnnual'], why=why)
            out.update(rel)
        else:
            for k in rel_keys:
                _note(why, k, 'fewer than 3 bars shared with the benchmark')
    else:
        for k in rel_keys:
            _note(why, k, 'no benchmark')
    return out


def _rf_on(rf_series, idx, sub_idx, ppy) -> np.ndarray:
    """Per-bar rf for returns on a sub-index (dates both curves share), from a yield series on idx."""
    import pandas as pd
    y = pd.Series(as_array(rf_series), index=idx).reindex(sub_idx)
    return rf_per_bar(len(sub_idx) - 1, y.values, ppy, sub_idx)


def quick_stats(returns, ppy: float = TRADING_DAYS, rf_bar=0.0, years: Optional[float] = None) -> Dict[str, Optional[float]]:
    """Fast objective metrics of one return series (optimisation inner loop)."""
    r = as_array(returns)
    r = np.where(np.isfinite(r), r, 0.0)
    if len(r) == 0:
        return {k: None for k in ('totalReturn', 'cagr', 'sharpeRatio', 'sortinoRatio', 'maxDrawdown', 'volatility',
                                  'calmarRatio')}
    eq = np.concatenate([[1.0], np.cumprod(1.0 + r)])
    tr = float(eq[-1] - 1.0)
    yrs = years if years else len(r) / ppy
    cg = (float(eq[-1] ** (1.0 / yrs) - 1.0) if (yrs > 0 and eq[-1] > 0) else (-1.0 if eq[-1] <= 0 else None))
    mdd = float(-np.min(drawdown_series(eq)))
    return {'totalReturn': tr, 'cagr': cg, 'sharpeRatio': sharpe_ratio(r, rf_bar, ppy),
            'sortinoRatio': sortino_ratio(r, rf_bar, ppy), 'maxDrawdown': mdd, 'volatility': volatility(r, ppy),
            'calmarRatio': (cg / mdd) if (cg is not None and mdd > _EPS) else None}


def sharpe_columns(M: np.ndarray, ppy: float = TRADING_DAYS, rf_bar=0.0, annualize: bool = True) -> np.ndarray:
    """Sharpe ratio of every column of a (T x N) return matrix (NaN where undefined)."""
    X = np.asarray(M, dtype=float)
    if np.ndim(rf_bar) == 0:
        X = X - float(rf_bar)
    else:
        X = X - np.asarray(rf_bar, dtype=float)[:, None]
    mu = np.nanmean(X, axis=0)
    sd = np.nanstd(X, axis=0, ddof=1)
    with np.errstate(divide='ignore', invalid='ignore'):
        s = np.where(sd > _EPS, mu / sd, np.nan)
    return s * math.sqrt(ppy) if annualize else s
