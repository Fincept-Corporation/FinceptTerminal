"""
Advanced Metrics Module

Calculates institutional-grade performance analytics:
- VaR / CVaR (Expected Shortfall)
- Ulcer Index, Omega Ratio, Tail Ratio
- Higher moments (Kurtosis, Skewness)
- Monthly returns table
- Rolling metrics (Sharpe, Volatility, Drawdown)
- Benchmark-relative metrics (Alpha, Beta, IR, Tracking Error, R²)
- Returns distribution histogram
"""

import numpy as np
from typing import Dict, Any, List, Optional


def calculate_all(
    equity_series: np.ndarray,
    benchmark_series: Optional[np.ndarray] = None,
    risk_free_rate: float = 0.0,
    dates: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """
    Calculate all advanced metrics from equity curve and optional benchmark.

    Args:
        equity_series: Daily portfolio equity values
        benchmark_series: Daily benchmark equity values (same length), or None
        risk_free_rate: Annual risk-free rate (default 0)
        dates: ISO date strings corresponding to equity_series

    Returns:
        Dictionary with all advanced metrics (camelCase keys for JSON)
    """
    if len(equity_series) < 2:
        return _empty_metrics()

    # Daily returns
    returns = np.diff(equity_series) / equity_series[:-1]
    returns = returns[np.isfinite(returns)]

    if len(returns) < 2:
        return _empty_metrics()

    daily_rf = risk_free_rate / 252.0

    # --- Risk Metrics ---
    var95 = float(np.percentile(returns, 5))
    var99 = float(np.percentile(returns, 1))
    cvar95 = float(np.mean(returns[returns <= var95])) if np.any(returns <= var95) else var95
    cvar99 = float(np.mean(returns[returns <= var99])) if np.any(returns <= var99) else var99

    # Ulcer Index (root mean square of drawdowns)
    peak = np.maximum.accumulate(equity_series)
    dd_pct = (equity_series - peak) / np.where(peak > 0, peak, 1.0) * 100
    ulcer_index = float(np.sqrt(np.mean(dd_pct ** 2)))

    # Omega Ratio (probability-weighted ratio of gains vs losses relative to threshold)
    threshold = daily_rf
    excess = returns - threshold
    gains = excess[excess > 0].sum()
    losses = -excess[excess < 0].sum()
    omega_ratio = float(gains / losses) if losses > 0 else 99.0

    # Tail Ratio (95th percentile / abs(5th percentile))
    p95 = np.percentile(returns, 95)
    p5 = np.percentile(returns, 5)
    tail_ratio = float(abs(p95 / p5)) if abs(p5) > 1e-10 else 99.0

    # Higher moments
    kurtosis = float(_excess_kurtosis(returns))
    skewness = float(_skewness(returns))

    # Daily return statistics
    avg_daily_return = float(np.mean(returns))
    daily_return_std = float(np.std(returns, ddof=1))
    daily_return_p5 = float(np.percentile(returns, 5))
    daily_return_p25 = float(np.percentile(returns, 25))
    daily_return_p75 = float(np.percentile(returns, 75))
    daily_return_p95 = float(np.percentile(returns, 95))

    # Returns histogram (20 bins)
    hist_counts, hist_edges = np.histogram(returns, bins=20)
    returns_histogram = []
    for i in range(len(hist_counts)):
        bin_center = (hist_edges[i] + hist_edges[i + 1]) / 2
        returns_histogram.append({
            'bin': round(float(bin_center), 6),
            'count': int(hist_counts[i]),
        })

    # --- Monthly Returns Table ---
    monthly_returns = _calculate_monthly_returns(equity_series, dates)

    # --- Rolling Metrics ---
    rolling_sharpe = _rolling_sharpe(returns, window=60, rf=daily_rf, dates=dates)
    rolling_volatility = _rolling_volatility(returns, window=20, dates=dates)
    rolling_drawdown = _rolling_drawdown(equity_series, dates=dates)

    # --- Benchmark-relative metrics ---
    benchmark_alpha = 0.0
    benchmark_beta = 0.0
    information_ratio = 0.0
    tracking_error = 0.0
    r_squared = 0.0
    benchmark_correlation = 0.0

    if benchmark_series is not None and len(benchmark_series) == len(equity_series):
        bench_returns = np.diff(benchmark_series) / benchmark_series[:-1]
        bench_returns = bench_returns[:len(returns)]  # align lengths

        if len(bench_returns) > 1 and np.std(bench_returns) > 1e-10:
            # Beta = Cov(r, rb) / Var(rb)
            cov_matrix = np.cov(returns[:len(bench_returns)], bench_returns)
            benchmark_beta = float(cov_matrix[0, 1] / cov_matrix[1, 1]) if cov_matrix[1, 1] > 1e-10 else 0.0

            # Alpha (annualized Jensen's alpha)
            benchmark_alpha = float(
                (np.mean(returns[:len(bench_returns)]) - daily_rf -
                 benchmark_beta * (np.mean(bench_returns) - daily_rf)) * 252
            )

            # Tracking Error
            active_returns = returns[:len(bench_returns)] - bench_returns
            tracking_error = float(np.std(active_returns, ddof=1) * np.sqrt(252))

            # Information Ratio
            if tracking_error > 1e-10:
                information_ratio = float(np.mean(active_returns) * 252 / tracking_error)

            # R-squared
            correlation = np.corrcoef(returns[:len(bench_returns)], bench_returns)[0, 1]
            benchmark_correlation = float(correlation) if np.isfinite(correlation) else 0.0
            r_squared = float(correlation ** 2) if np.isfinite(correlation) else 0.0

    return {
        'var95': var95,
        'var99': var99,
        'cvar95': cvar95,
        'cvar99': cvar99,
        'ulcerIndex': ulcer_index,
        'omegaRatio': omega_ratio,
        'tailRatio': tail_ratio,
        'kurtosis': kurtosis,
        'skewness': skewness,
        'avgDailyReturn': avg_daily_return,
        'dailyReturnStd': daily_return_std,
        'dailyReturnP5': daily_return_p5,
        'dailyReturnP25': daily_return_p25,
        'dailyReturnP75': daily_return_p75,
        'dailyReturnP95': daily_return_p95,
        'returnsHistogram': returns_histogram,
        'monthlyReturns': monthly_returns,
        'rollingSharpe': rolling_sharpe,
        'rollingVolatility': rolling_volatility,
        'rollingDrawdown': rolling_drawdown,
        'benchmarkAlpha': benchmark_alpha,
        'benchmarkBeta': benchmark_beta,
        'informationRatio': information_ratio,
        'trackingError': tracking_error,
        'rSquared': r_squared,
        'benchmarkCorrelation': benchmark_correlation,
    }


# ============================================================================
# Internal Helpers
# ============================================================================

def _excess_kurtosis(returns: np.ndarray) -> float:
    """Fisher's excess kurtosis (normal = 0)"""
    n = len(returns)
    if n < 4:
        return 0.0
    mean = np.mean(returns)
    std = np.std(returns, ddof=1)
    if std < 1e-10:
        return 0.0
    m4 = np.mean((returns - mean) ** 4)
    return m4 / (std ** 4) - 3.0


def _skewness(returns: np.ndarray) -> float:
    """Sample skewness"""
    n = len(returns)
    if n < 3:
        return 0.0
    mean = np.mean(returns)
    std = np.std(returns, ddof=1)
    if std < 1e-10:
        return 0.0
    m3 = np.mean((returns - mean) ** 3)
    return m3 / (std ** 3)


def _calculate_monthly_returns(
    equity_series: np.ndarray,
    dates: Optional[List[str]] = None,
) -> List[Dict[str, Any]]:
    """Calculate monthly returns grid (year x 12 months)"""
    if dates is None or len(dates) != len(equity_series):
        return []

    # Parse dates to year/month
    monthly_data: Dict[int, Dict[int, float]] = {}  # year -> {month -> return}

    # Find month boundaries
    prev_month = None
    prev_equity = equity_series[0]
    month_start_equity = equity_series[0]

    for i, date_str in enumerate(dates):
        try:
            parts = str(date_str).split('T')[0].split('-')
            year = int(parts[0])
            month = int(parts[1])
        except (ValueError, IndexError):
            continue

        current_key = (year, month)

        if prev_month is not None and current_key != prev_month:
            # Month ended - calculate return
            py, pm = prev_month
            if py not in monthly_data:
                monthly_data[py] = {}
            ret = (prev_equity - month_start_equity) / month_start_equity if month_start_equity > 0 else 0
            monthly_data[py][pm] = ret
            month_start_equity = equity_series[i]

        prev_month = current_key
        prev_equity = equity_series[i]

    # Handle last month
    if prev_month is not None:
        py, pm = prev_month
        if py not in monthly_data:
            monthly_data[py] = {}
        ret = (prev_equity - month_start_equity) / month_start_equity if month_start_equity > 0 else 0
        monthly_data[py][pm] = ret

    # Build result rows
    result = []
    for year in sorted(monthly_data.keys()):
        months = [None] * 12
        year_total = 1.0
        for m in range(1, 13):
            if m in monthly_data[year]:
                months[m - 1] = round(monthly_data[year][m], 6)
                year_total *= (1 + monthly_data[year][m])
        result.append({
            'year': year,
            'months': months,
            'yearTotal': round(year_total - 1.0, 6),
        })

    return result


def _rolling_sharpe(
    returns: np.ndarray,
    window: int = 60,
    rf: float = 0.0,
    dates: Optional[List[str]] = None,
) -> List[Dict[str, Any]]:
    """Calculate rolling Sharpe ratio"""
    result = []
    for i in range(window, len(returns)):
        window_returns = returns[i - window:i]
        excess = window_returns - rf
        std = np.std(excess, ddof=1)
        sharpe = float(np.mean(excess) / std * np.sqrt(252)) if std > 1e-10 else 0.0
        date = dates[i + 1] if dates and i + 1 < len(dates) else str(i)
        result.append({'date': str(date).split('T')[0], 'value': round(sharpe, 4)})
    return result


def _rolling_volatility(
    returns: np.ndarray,
    window: int = 20,
    dates: Optional[List[str]] = None,
) -> List[Dict[str, Any]]:
    """Calculate rolling annualized volatility"""
    result = []
    for i in range(window, len(returns)):
        window_returns = returns[i - window:i]
        vol = float(np.std(window_returns, ddof=1) * np.sqrt(252))
        date = dates[i + 1] if dates and i + 1 < len(dates) else str(i)
        result.append({'date': str(date).split('T')[0], 'value': round(vol, 6)})
    return result


def _rolling_drawdown(
    equity_series: np.ndarray,
    dates: Optional[List[str]] = None,
) -> List[Dict[str, Any]]:
    """Calculate rolling drawdown from peak"""
    peak = np.maximum.accumulate(equity_series)
    dd = (equity_series - peak) / np.where(peak > 0, peak, 1.0)
    result = []
    for i in range(len(dd)):
        date = dates[i] if dates and i < len(dates) else str(i)
        result.append({'date': str(date).split('T')[0], 'value': round(float(dd[i]), 6)})
    return result


def _empty_metrics() -> Dict[str, Any]:
    """Return empty metrics structure"""
    return {
        'var95': 0, 'var99': 0, 'cvar95': 0, 'cvar99': 0,
        'ulcerIndex': 0, 'omegaRatio': 0, 'tailRatio': 0,
        'kurtosis': 0, 'skewness': 0,
        'avgDailyReturn': 0, 'dailyReturnStd': 0,
        'dailyReturnP5': 0, 'dailyReturnP25': 0,
        'dailyReturnP75': 0, 'dailyReturnP95': 0,
        'returnsHistogram': [],
        'monthlyReturns': [],
        'rollingSharpe': [], 'rollingVolatility': [], 'rollingDrawdown': [],
        'benchmarkAlpha': 0, 'benchmarkBeta': 0,
        'informationRatio': 0, 'trackingError': 0,
        'rSquared': 0, 'benchmarkCorrelation': 0,
    }
