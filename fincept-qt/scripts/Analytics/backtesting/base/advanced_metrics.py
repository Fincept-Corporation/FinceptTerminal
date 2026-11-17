"""
Advanced metrics (legacy entry point, kept for the engines that import it).

calculate_all() keeps its signature and output keys but is now a thin wrapper over
base.metrics (one definition of every metric), vectorised - the per-bar Python loops are gone:
  * rolling series are dated at the LAST bar of their window and include the window ending on
    the last bar (B2: they were stamped one bar late and the last window was missing);
  * an undefined value is None, never 0 (B11: `_empty_metrics` returned 0 for alpha / beta /
    omega);
  * skewness / kurtosis are the population moments (scipy defaults; kurtosis is EXCESS);
  * ulcerIndex stays in PERCENT POINTS (the unit the screen shows today; ulcerIndexFraction is
    the same value as a fraction);
  * rolling windows default to 126 bars for Sharpe and 20 for volatility as before (pass
    rolling_window to change both).
New code should call base.metrics directly or let result_normalizer build the result.
"""

from typing import Any, Dict, List, Optional

import numpy as np


def calculate_all(
    equity_series: np.ndarray,
    benchmark_series: Optional[np.ndarray] = None,
    risk_free_rate: float = 0.0,
    dates: Optional[List[str]] = None,
    periods_per_year: float = 252.0,
    rolling_window: Optional[int] = None,
) -> Dict[str, Any]:
    from base import metrics as M
    e = np.asarray(equity_series, dtype=float)
    if len(e) < 3:
        return _empty_metrics()
    ppy = float(periods_per_year) if periods_per_year and periods_per_year > 0 else 252.0
    r = M.simple_returns(e)
    rc = r[np.isfinite(r)]
    if len(rc) < 2:
        return _empty_metrics()
    rf_bar = M.rf_per_bar(len(r), risk_free_rate, ppy)
    out: Dict[str, Any] = {
        'var95': M.value_at_risk(r, 0.95), 'var99': M.value_at_risk(r, 0.99),
        'cvar95': M.conditional_var(r, 0.95), 'cvar99': M.conditional_var(r, 0.99),
        'ulcerIndex': M.ulcer_index(e), 'ulcerIndexFraction': (M.ulcer_index(e) or 0.0) / 100.0,
        'omegaRatio': M.omega_ratio(r, rf_bar), 'tailRatio': M.tail_ratio(r),
        'kurtosis': M.excess_kurtosis(r), 'skewness': M.skewness(r),
        'avgDailyReturn': float(rc.mean()), 'dailyReturnStd': float(rc.std(ddof=1)),
        'dailyReturnP5': float(np.percentile(rc, 5)), 'dailyReturnP25': float(np.percentile(rc, 25)),
        'dailyReturnP75': float(np.percentile(rc, 75)), 'dailyReturnP95': float(np.percentile(rc, 95)),
        'returnsHistogram': M.histogram(r, 20),
        'monthlyReturns': [], 'rollingSharpe': [], 'rollingVolatility': [], 'rollingDrawdown': [],
        'benchmarkAlpha': None, 'benchmarkBeta': None, 'informationRatio': None, 'trackingError': None,
        'rSquared': None, 'benchmarkCorrelation': None,
    }
    if dates is not None and len(dates) == len(e):
        idx = M.to_datetime_index(dates)
        out['monthlyReturns'] = M.monthly_table(e, idx)
        win = int(rolling_window) if rolling_window else 126
        roll = M.rolling_series(e, idx, win, rf_bar=risk_free_rate, ppy=ppy)
        out['rollingSharpe'] = roll['rollingSharpe']
        out['rollingDrawdown'] = roll['rollingDrawdown']
        vol = M.rolling_series(e, idx, int(rolling_window) if rolling_window else 20, ppy=ppy)
        out['rollingVolatility'] = vol['rollingVolatility']
    if benchmark_series is not None and len(benchmark_series) == len(e):
        b = np.asarray(benchmark_series, dtype=float)
        rb = M.simple_returns(b)
        rel = M.relative_metrics(r, rb, rf_bar, ppy)
        out.update({'benchmarkAlpha': rel['alpha'], 'benchmarkBeta': rel['beta'],
                    'informationRatio': rel['informationRatio'], 'trackingError': rel['trackingError'],
                    'rSquared': rel['rSquared'], 'benchmarkCorrelation': rel['correlation'],
                    'upCapture': rel['upCapture'], 'downCapture': rel['downCapture']})
    return out


def _empty_metrics() -> Dict[str, Any]:
    """Too little data: every value undefined (None), lists empty."""
    return {
        'var95': None, 'var99': None, 'cvar95': None, 'cvar99': None,
        'ulcerIndex': None, 'ulcerIndexFraction': None, 'omegaRatio': None, 'tailRatio': None,
        'kurtosis': None, 'skewness': None,
        'avgDailyReturn': None, 'dailyReturnStd': None,
        'dailyReturnP5': None, 'dailyReturnP25': None,
        'dailyReturnP75': None, 'dailyReturnP95': None,
        'returnsHistogram': [],
        'monthlyReturns': [],
        'rollingSharpe': [], 'rollingVolatility': [], 'rollingDrawdown': [],
        'benchmarkAlpha': None, 'benchmarkBeta': None,
        'informationRatio': None, 'trackingError': None,
        'rSquared': None, 'benchmarkCorrelation': None,
    }
