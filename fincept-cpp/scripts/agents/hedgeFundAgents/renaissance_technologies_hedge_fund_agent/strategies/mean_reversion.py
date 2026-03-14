"""
Mean Reversion Strategy Module

Renaissance Technologies' mean reversion algorithms.
Identifies when prices deviate from historical means and predicts convergence.

Key concepts:
- Z-score calculation from historical distribution
- Half-life estimation (Ornstein-Uhlenbeck process)
- Cointegration testing for pairs
"""

import math
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass


@dataclass
class MeanReversionResult:
    """Result of mean reversion analysis"""
    signal_strength: float  # -1 to 1
    z_score: float
    half_life: Optional[float]
    statistical_significance: float
    details: List[str]


def calculate_z_score(current: float, mean: float, std: float) -> float:
    """Calculate z-score from mean"""
    if std == 0:
        return 0.0
    return (current - mean) / std


def estimate_half_life(series: List[float]) -> Optional[float]:
    """
    Estimate mean reversion half-life using Ornstein-Uhlenbeck process.
    Half-life = -ln(2) / lambda, where lambda is the mean reversion speed.
    """
    if len(series) < 10:
        return None

    # Calculate lag-1 differences
    y = [series[i] - series[i - 1] for i in range(1, len(series))]
    x = series[:-1]

    # Simple linear regression: y = alpha + beta * x
    n = len(y)
    mean_x = sum(x) / n
    mean_y = sum(y) / n

    numerator = sum((x[i] - mean_x) * (y[i] - mean_y) for i in range(n))
    denominator = sum((x[i] - mean_x) ** 2 for i in range(n))

    if denominator == 0:
        return None

    beta = numerator / denominator

    # Mean reversion requires negative beta
    if beta >= 0:
        return None

    # Half-life = -ln(2) / beta
    half_life = -math.log(2) / beta

    # Sanity check
    if half_life < 0 or half_life > len(series):
        return None

    return half_life


def calculate_hurst_exponent(series: List[float], max_lag: int = 20) -> float:
    """
    Calculate Hurst exponent to determine mean reversion tendency.
    H < 0.5: Mean reverting
    H = 0.5: Random walk
    H > 0.5: Trending
    """
    if len(series) < max_lag * 2:
        return 0.5  # Default to random walk

    lags = range(2, min(max_lag, len(series) // 2))
    tau = []
    rs = []

    for lag in lags:
        # Calculate returns
        returns = [series[i] - series[i - lag] for i in range(lag, len(series))]

        if not returns:
            continue

        # R/S statistic
        mean_r = sum(returns) / len(returns)
        cumdev = []
        cumsum = 0
        for r in returns:
            cumsum += r - mean_r
            cumdev.append(cumsum)

        R = max(cumdev) - min(cumdev)
        S = math.sqrt(sum((r - mean_r) ** 2 for r in returns) / len(returns))

        if S > 0:
            rs.append(R / S)
            tau.append(lag)

    if len(tau) < 3:
        return 0.5

    # Linear regression of log(R/S) vs log(tau)
    log_tau = [math.log(t) for t in tau]
    log_rs = [math.log(r) for r in rs]

    mean_log_tau = sum(log_tau) / len(log_tau)
    mean_log_rs = sum(log_rs) / len(log_rs)

    numerator = sum((log_tau[i] - mean_log_tau) * (log_rs[i] - mean_log_rs) for i in range(len(tau)))
    denominator = sum((log_tau[i] - mean_log_tau) ** 2 for i in range(len(tau)))

    if denominator == 0:
        return 0.5

    hurst = numerator / denominator
    return max(0, min(1, hurst))


def analyze_mean_reversion(
    prices: List[float],
    lookback_short: int = 20,
    lookback_long: int = 60
) -> MeanReversionResult:
    """
    Comprehensive mean reversion analysis.

    Args:
        prices: Historical price series (most recent first)
        lookback_short: Short-term lookback for recent mean
        lookback_long: Long-term lookback for historical mean
    """
    details = []

    if len(prices) < lookback_long:
        return MeanReversionResult(
            signal_strength=0.0,
            z_score=0.0,
            half_life=None,
            statistical_significance=0.0,
            details=["Insufficient data for mean reversion analysis"]
        )

    current_price = prices[0]
    short_term_prices = prices[:lookback_short]
    long_term_prices = prices[:lookback_long]

    # Calculate means
    short_mean = sum(short_term_prices) / len(short_term_prices)
    long_mean = sum(long_term_prices) / len(long_term_prices)

    # Calculate standard deviation
    long_std = math.sqrt(sum((p - long_mean) ** 2 for p in long_term_prices) / len(long_term_prices))

    # Z-score
    z_score = calculate_z_score(current_price, long_mean, long_std)
    details.append(f"Z-score from long-term mean: {z_score:.2f}")

    # Half-life estimation
    half_life = estimate_half_life(list(reversed(prices[:lookback_long])))
    if half_life:
        details.append(f"Estimated half-life: {half_life:.1f} periods")

    # Hurst exponent
    hurst = calculate_hurst_exponent(list(reversed(prices)))
    details.append(f"Hurst exponent: {hurst:.3f}")

    # Signal strength calculation
    # Strong mean reversion signal when:
    # 1. Large z-score (price far from mean)
    # 2. Low Hurst exponent (mean reverting behavior)
    # 3. Reasonable half-life

    signal_strength = 0.0

    # Z-score contribution (capped at 3 sigma)
    z_contribution = min(abs(z_score), 3) / 3
    if z_score < 0:  # Below mean = bullish for mean reversion
        signal_strength += z_contribution * 0.4
    else:  # Above mean = bearish for mean reversion
        signal_strength -= z_contribution * 0.4

    # Hurst contribution
    if hurst < 0.5:
        signal_strength += (0.5 - hurst) * 0.6  # Mean reverting
        details.append("Hurst indicates mean-reverting behavior")
    else:
        signal_strength *= 0.5  # Reduce signal if trending
        details.append("Hurst indicates trending behavior - reduced signal")

    # Statistical significance based on data quality
    stat_sig = min(len(prices) / 100, 1.0) * (1 - abs(hurst - 0.3))
    stat_sig = max(0, min(1, stat_sig))

    return MeanReversionResult(
        signal_strength=max(-1, min(1, signal_strength)),
        z_score=z_score,
        half_life=half_life,
        statistical_significance=stat_sig,
        details=details
    )


def analyze_pairs_cointegration(
    series_a: List[float],
    series_b: List[float]
) -> Tuple[float, float, float]:
    """
    Analyze cointegration between two price series for pairs trading.

    Returns:
        Tuple of (cointegration_score, spread_z_score, hedge_ratio)
    """
    if len(series_a) != len(series_b) or len(series_a) < 30:
        return (0.0, 0.0, 1.0)

    # Calculate hedge ratio using linear regression
    mean_a = sum(series_a) / len(series_a)
    mean_b = sum(series_b) / len(series_b)

    numerator = sum((series_a[i] - mean_a) * (series_b[i] - mean_b) for i in range(len(series_a)))
    denominator = sum((series_b[i] - mean_b) ** 2 for i in range(len(series_b)))

    if denominator == 0:
        return (0.0, 0.0, 1.0)

    hedge_ratio = numerator / denominator

    # Calculate spread
    spread = [series_a[i] - hedge_ratio * series_b[i] for i in range(len(series_a))]

    # Test spread for stationarity using Hurst exponent
    hurst = calculate_hurst_exponent(spread)

    # Cointegration score (lower Hurst = more cointegrated)
    cointegration_score = max(0, 1 - 2 * abs(hurst - 0.3))

    # Spread z-score
    spread_mean = sum(spread) / len(spread)
    spread_std = math.sqrt(sum((s - spread_mean) ** 2 for s in spread) / len(spread))
    spread_z_score = calculate_z_score(spread[0], spread_mean, spread_std) if spread_std > 0 else 0

    return (cointegration_score, spread_z_score, hedge_ratio)
