"""
Momentum Strategy Module

Renaissance Technologies' momentum algorithms.
Identifies persistent trends and acceleration patterns.

Key concepts:
- Price momentum (rate of change)
- Earnings momentum
- Factor momentum
- Cross-sectional momentum rankings
"""

import math
from typing import List, Dict, Any, Optional
from dataclasses import dataclass


@dataclass
class MomentumResult:
    """Result of momentum analysis"""
    signal_strength: float  # -1 to 1
    trend_strength: float  # 0 to 1
    acceleration: float
    optimal_lookback: int
    details: List[str]


def calculate_returns(prices: List[float], period: int = 1) -> List[float]:
    """Calculate period returns from price series"""
    if len(prices) <= period:
        return []
    return [(prices[i] / prices[i + period]) - 1 for i in range(len(prices) - period)]


def calculate_momentum_score(prices: List[float], lookback: int) -> float:
    """
    Calculate momentum score for a given lookback period.
    Returns annualized return as percentage.
    """
    if len(prices) <= lookback:
        return 0.0

    return_pct = (prices[0] / prices[lookback]) - 1

    # Annualize (assuming daily data, 252 trading days)
    annualized = return_pct * (252 / lookback)
    return annualized


def calculate_trend_strength(prices: List[float], lookback: int = 20) -> float:
    """
    Calculate trend strength using linear regression R-squared.
    Higher R-squared indicates stronger, more consistent trend.
    """
    if len(prices) < lookback:
        return 0.0

    subset = prices[:lookback]
    n = len(subset)

    # X is time (0, 1, 2, ...)
    x = list(range(n))
    y = list(reversed(subset))  # Oldest to newest

    mean_x = sum(x) / n
    mean_y = sum(y) / n

    # Calculate regression components
    ss_xy = sum((x[i] - mean_x) * (y[i] - mean_y) for i in range(n))
    ss_xx = sum((x[i] - mean_x) ** 2 for i in range(n))
    ss_yy = sum((y[i] - mean_y) ** 2 for i in range(n))

    if ss_xx == 0 or ss_yy == 0:
        return 0.0

    # R-squared
    r_squared = (ss_xy ** 2) / (ss_xx * ss_yy)
    return min(1.0, max(0.0, r_squared))


def calculate_acceleration(prices: List[float], short_period: int = 5, long_period: int = 20) -> float:
    """
    Calculate momentum acceleration.
    Positive acceleration = momentum increasing
    Negative acceleration = momentum decreasing
    """
    if len(prices) <= long_period:
        return 0.0

    short_momentum = calculate_momentum_score(prices, short_period)
    long_momentum = calculate_momentum_score(prices, long_period)

    # Acceleration is the difference between short and long momentum
    acceleration = short_momentum - long_momentum
    return acceleration


def find_optimal_lookback(
    prices: List[float],
    lookbacks: List[int] = [5, 10, 20, 40, 60]
) -> int:
    """
    Find optimal lookback period that maximizes risk-adjusted momentum.
    Uses a simple Sharpe-like metric.
    """
    if len(prices) < max(lookbacks):
        return lookbacks[0]

    best_lookback = lookbacks[0]
    best_score = float('-inf')

    for lookback in lookbacks:
        if len(prices) <= lookback * 2:
            continue

        # Calculate rolling momentum scores
        momentums = []
        for i in range(min(lookback, len(prices) - lookback)):
            mom = calculate_momentum_score(prices[i:], lookback)
            momentums.append(mom)

        if not momentums:
            continue

        mean_mom = sum(momentums) / len(momentums)
        std_mom = math.sqrt(sum((m - mean_mom) ** 2 for m in momentums) / len(momentums))

        if std_mom > 0:
            sharpe_like = mean_mom / std_mom
            if sharpe_like > best_score:
                best_score = sharpe_like
                best_lookback = lookback

    return best_lookback


def calculate_relative_strength(
    target_prices: List[float],
    benchmark_prices: List[float],
    lookback: int = 20
) -> float:
    """
    Calculate relative strength vs benchmark.
    Positive = outperforming, Negative = underperforming
    """
    if len(target_prices) <= lookback or len(benchmark_prices) <= lookback:
        return 0.0

    target_return = (target_prices[0] / target_prices[lookback]) - 1
    benchmark_return = (benchmark_prices[0] / benchmark_prices[lookback]) - 1

    return target_return - benchmark_return


def analyze_momentum(
    prices: List[float],
    revenues: Optional[List[float]] = None,
    earnings: Optional[List[float]] = None
) -> MomentumResult:
    """
    Comprehensive momentum analysis combining price and fundamental momentum.

    Args:
        prices: Historical price series (most recent first)
        revenues: Historical revenue series (optional)
        earnings: Historical earnings series (optional)
    """
    details = []

    if len(prices) < 20:
        return MomentumResult(
            signal_strength=0.0,
            trend_strength=0.0,
            acceleration=0.0,
            optimal_lookback=20,
            details=["Insufficient price data for momentum analysis"]
        )

    # Find optimal lookback
    optimal_lookback = find_optimal_lookback(prices)
    details.append(f"Optimal lookback period: {optimal_lookback} days")

    # Price momentum
    price_momentum = calculate_momentum_score(prices, optimal_lookback)
    details.append(f"Price momentum ({optimal_lookback}d): {price_momentum * 100:.1f}%")

    # Trend strength
    trend_strength = calculate_trend_strength(prices, optimal_lookback)
    details.append(f"Trend strength (R²): {trend_strength:.3f}")

    # Acceleration
    acceleration = calculate_acceleration(prices)
    if acceleration > 0:
        details.append(f"Momentum accelerating: {acceleration * 100:.1f}%")
    else:
        details.append(f"Momentum decelerating: {acceleration * 100:.1f}%")

    # Fundamental momentum (if available)
    fundamental_momentum = 0.0

    if revenues and len(revenues) >= 4:
        revenue_growth = (revenues[0] / revenues[-1]) ** (1 / len(revenues)) - 1
        fundamental_momentum += revenue_growth
        details.append(f"Revenue momentum: {revenue_growth * 100:.1f}%")

    if earnings and len(earnings) >= 4:
        # Handle negative earnings
        positive_earnings = [e for e in earnings if e > 0]
        if len(positive_earnings) >= 2:
            earnings_growth = (positive_earnings[0] / positive_earnings[-1]) ** (1 / len(positive_earnings)) - 1
            fundamental_momentum += earnings_growth
            details.append(f"Earnings momentum: {earnings_growth * 100:.1f}%")

    # Composite signal strength
    signal_strength = 0.0

    # Price momentum contribution (60% weight)
    price_signal = min(1, max(-1, price_momentum * 5))  # Scale to -1 to 1
    signal_strength += price_signal * 0.6

    # Trend strength contribution (20% weight)
    if price_momentum > 0:
        signal_strength += trend_strength * 0.2
    else:
        signal_strength -= trend_strength * 0.2

    # Acceleration contribution (10% weight)
    accel_signal = min(1, max(-1, acceleration * 10))
    signal_strength += accel_signal * 0.1

    # Fundamental momentum contribution (10% weight)
    if fundamental_momentum != 0:
        fund_signal = min(1, max(-1, fundamental_momentum * 5))
        signal_strength += fund_signal * 0.1

    return MomentumResult(
        signal_strength=max(-1, min(1, signal_strength)),
        trend_strength=trend_strength,
        acceleration=acceleration,
        optimal_lookback=optimal_lookback,
        details=details
    )


def calculate_cross_sectional_momentum_rank(
    target_momentum: float,
    peer_momentums: List[float]
) -> float:
    """
    Calculate percentile rank of target momentum vs peers.
    Returns percentile (0-100).
    """
    if not peer_momentums:
        return 50.0

    all_momentums = peer_momentums + [target_momentum]
    all_momentums.sort()

    rank = all_momentums.index(target_momentum) + 1
    percentile = (rank / len(all_momentums)) * 100

    return percentile
