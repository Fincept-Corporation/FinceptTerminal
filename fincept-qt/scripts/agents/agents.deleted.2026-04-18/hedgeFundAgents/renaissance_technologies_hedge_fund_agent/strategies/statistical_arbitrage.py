"""
Statistical Arbitrage Strategy Module

Renaissance Technologies' stat arb algorithms.
Identifies mispricing, regime changes, and arbitrage opportunities.

Key concepts:
- Pairs trading (cointegration-based)
- Statistical mispricing detection
- Regime detection (Hidden Markov Models concept)
- Market microstructure analysis
"""

import math
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum


class MarketRegime(str, Enum):
    BULL = "bull"
    BEAR = "bear"
    SIDEWAYS = "sideways"
    TRANSITION = "transition"


@dataclass
class StatArbResult:
    """Result of statistical arbitrage analysis"""
    arbitrage_score: float  # 0 to 10
    edge_estimate_bps: float
    mispricing_detected: bool
    mispricing_pct: float
    regime: MarketRegime
    regime_confidence: float
    details: List[str]


@dataclass
class PairsTradingResult:
    """Result of pairs trading analysis"""
    signal: str  # "long_spread", "short_spread", "neutral"
    spread_z_score: float
    cointegration_score: float
    half_life: float
    hedge_ratio: float
    details: List[str]


def detect_market_regime(
    prices: List[float],
    lookback: int = 60
) -> Tuple[MarketRegime, float]:
    """
    Detect current market regime using simple state classification.
    Inspired by Hidden Markov Models but simplified.

    Returns:
        Tuple of (regime, confidence)
    """
    if len(prices) < lookback:
        return (MarketRegime.SIDEWAYS, 0.5)

    subset = prices[:lookback]

    # Calculate trend
    returns = [(subset[i] / subset[i + 1]) - 1 for i in range(len(subset) - 1)]
    mean_return = sum(returns) / len(returns)
    volatility = math.sqrt(sum((r - mean_return) ** 2 for r in returns) / len(returns))

    # Annualized metrics
    annualized_return = mean_return * 252
    annualized_vol = volatility * math.sqrt(252)

    # Regime classification
    if annualized_return > 0.15 and annualized_vol < 0.25:
        regime = MarketRegime.BULL
        confidence = min(1, annualized_return / 0.3)
    elif annualized_return < -0.15 and annualized_vol < 0.35:
        regime = MarketRegime.BEAR
        confidence = min(1, abs(annualized_return) / 0.3)
    elif annualized_vol > 0.35:
        regime = MarketRegime.TRANSITION
        confidence = min(1, (annualized_vol - 0.25) / 0.2)
    else:
        regime = MarketRegime.SIDEWAYS
        confidence = 1 - abs(annualized_return) / 0.15

    return (regime, max(0.3, min(0.95, confidence)))


def calculate_regime_duration(
    prices: List[float],
    current_regime: MarketRegime,
    window: int = 20
) -> int:
    """Calculate how long the current regime has persisted."""
    if len(prices) < window * 2:
        return window

    duration = 0
    for i in range(0, len(prices) - window, window // 2):
        regime, _ = detect_market_regime(prices[i:i + window * 2], window)
        if regime == current_regime:
            duration += window // 2
        else:
            break

    return max(duration, window // 2)


def detect_statistical_mispricing(
    current_price: float,
    intrinsic_estimates: List[float],
    peer_multiples: Optional[Dict[str, float]] = None
) -> Tuple[bool, float, float]:
    """
    Detect if security is statistically mispriced.

    Args:
        current_price: Current market price
        intrinsic_estimates: List of fair value estimates
        peer_multiples: Optional dict of peer valuation multiples

    Returns:
        Tuple of (is_mispriced, mispricing_pct, confidence)
    """
    if not intrinsic_estimates:
        return (False, 0.0, 0.0)

    # Calculate consensus fair value
    mean_estimate = sum(intrinsic_estimates) / len(intrinsic_estimates)
    std_estimate = math.sqrt(
        sum((e - mean_estimate) ** 2 for e in intrinsic_estimates) / len(intrinsic_estimates)
    ) if len(intrinsic_estimates) > 1 else mean_estimate * 0.1

    # Mispricing percentage
    mispricing_pct = (current_price - mean_estimate) / mean_estimate

    # Statistical significance
    z_score = abs(current_price - mean_estimate) / std_estimate if std_estimate > 0 else 0

    # Consider mispriced if >1.5 standard deviations from mean estimate
    is_mispriced = abs(z_score) > 1.5

    # Confidence based on estimate consistency and deviation size
    estimate_consistency = 1 - (std_estimate / mean_estimate) if mean_estimate > 0 else 0
    confidence = min(1, z_score / 3) * estimate_consistency

    return (is_mispriced, mispricing_pct, confidence)


def analyze_pairs_trading(
    series_a: List[float],
    series_b: List[float],
    entry_threshold: float = 2.0,
    exit_threshold: float = 0.5
) -> PairsTradingResult:
    """
    Analyze pairs trading opportunity between two securities.

    Args:
        series_a: Price series of security A (target)
        series_b: Price series of security B (hedge)
        entry_threshold: Z-score threshold for entry
        exit_threshold: Z-score threshold for exit
    """
    details = []

    if len(series_a) != len(series_b):
        min_len = min(len(series_a), len(series_b))
        series_a = series_a[:min_len]
        series_b = series_b[:min_len]

    if len(series_a) < 30:
        return PairsTradingResult(
            signal="neutral",
            spread_z_score=0.0,
            cointegration_score=0.0,
            half_life=float('inf'),
            hedge_ratio=1.0,
            details=["Insufficient data for pairs analysis"]
        )

    # Calculate hedge ratio using OLS
    mean_a = sum(series_a) / len(series_a)
    mean_b = sum(series_b) / len(series_b)

    numerator = sum((series_a[i] - mean_a) * (series_b[i] - mean_b) for i in range(len(series_a)))
    denominator = sum((series_b[i] - mean_b) ** 2 for i in range(len(series_b)))

    hedge_ratio = numerator / denominator if denominator != 0 else 1.0
    details.append(f"Hedge ratio: {hedge_ratio:.3f}")

    # Calculate spread
    spread = [series_a[i] - hedge_ratio * series_b[i] for i in range(len(series_a))]

    # Spread statistics
    spread_mean = sum(spread) / len(spread)
    spread_std = math.sqrt(sum((s - spread_mean) ** 2 for s in spread) / len(spread))

    # Current z-score
    spread_z_score = (spread[0] - spread_mean) / spread_std if spread_std > 0 else 0
    details.append(f"Spread z-score: {spread_z_score:.2f}")

    # Cointegration test (simplified Engle-Granger using Hurst exponent)
    from .mean_reversion import calculate_hurst_exponent
    hurst = calculate_hurst_exponent(list(reversed(spread)))

    # Cointegration score (lower Hurst = more cointegrated)
    cointegration_score = max(0, 1 - 2 * hurst) if hurst < 0.5 else 0
    details.append(f"Cointegration score: {cointegration_score:.3f}")

    # Half-life estimation
    from .mean_reversion import estimate_half_life
    half_life = estimate_half_life(list(reversed(spread)))
    if half_life:
        details.append(f"Spread half-life: {half_life:.1f} periods")
    else:
        half_life = float('inf')

    # Trading signal
    if cointegration_score < 0.3:
        signal = "neutral"
        details.append("Weak cointegration - no trade")
    elif spread_z_score > entry_threshold:
        signal = "short_spread"  # Short A, Long B
        details.append(f"Spread above {entry_threshold}σ - short spread")
    elif spread_z_score < -entry_threshold:
        signal = "long_spread"  # Long A, Short B
        details.append(f"Spread below -{entry_threshold}σ - long spread")
    elif abs(spread_z_score) < exit_threshold:
        signal = "neutral"
        details.append("Spread near mean - exit/neutral")
    else:
        signal = "neutral"
        details.append("No clear signal")

    return PairsTradingResult(
        signal=signal,
        spread_z_score=spread_z_score,
        cointegration_score=cointegration_score,
        half_life=half_life if half_life else float('inf'),
        hedge_ratio=hedge_ratio,
        details=details
    )


def estimate_edge(
    signal_strength: float,
    statistical_significance: float,
    transaction_costs_bps: float = 10
) -> float:
    """
    Estimate expected edge in basis points.

    Renaissance's philosophy: "We're right 50.75% of the time,
    but we're 100% right 50.75% of the time."
    """
    # Base edge from signal
    base_edge_bps = signal_strength * 50  # Max 50 bps raw edge

    # Adjust for statistical significance
    adjusted_edge = base_edge_bps * statistical_significance

    # Subtract transaction costs
    net_edge = adjusted_edge - transaction_costs_bps

    return max(0, net_edge)


def analyze_statistical_arbitrage(
    prices: List[float],
    intrinsic_estimates: Optional[List[float]] = None,
    peer_prices: Optional[List[float]] = None,
    metrics: Optional[Dict[str, Any]] = None
) -> StatArbResult:
    """
    Comprehensive statistical arbitrage analysis.

    Args:
        prices: Target security price series
        intrinsic_estimates: Fair value estimates
        peer_prices: Comparable security prices for pairs analysis
        metrics: Additional fundamental metrics
    """
    details = []
    scores = []

    if len(prices) < 30:
        return StatArbResult(
            arbitrage_score=0.0,
            edge_estimate_bps=0.0,
            mispricing_detected=False,
            mispricing_pct=0.0,
            regime=MarketRegime.SIDEWAYS,
            regime_confidence=0.5,
            details=["Insufficient data for stat arb analysis"]
        )

    # Regime detection
    regime, regime_confidence = detect_market_regime(prices)
    details.append(f"Market regime: {regime.value} (confidence: {regime_confidence:.2f})")

    # Mispricing detection
    mispricing_detected = False
    mispricing_pct = 0.0

    if intrinsic_estimates:
        mispricing_detected, mispricing_pct, mispricing_conf = detect_statistical_mispricing(
            prices[0], intrinsic_estimates
        )
        if mispricing_detected:
            details.append(f"Mispricing detected: {mispricing_pct * 100:.1f}%")
            scores.append(mispricing_conf * 10)
        else:
            details.append("No significant mispricing")
            scores.append(3)

    # Pairs trading analysis
    if peer_prices and len(peer_prices) >= 30:
        pairs_result = analyze_pairs_trading(prices, peer_prices)
        if pairs_result.signal != "neutral":
            details.append(f"Pairs signal: {pairs_result.signal}")
            scores.append(pairs_result.cointegration_score * 10)
        else:
            scores.append(2)
        details.extend(pairs_result.details[:2])  # Add top 2 details

    # Regime transition opportunity
    if regime == MarketRegime.TRANSITION:
        details.append("Regime transition - elevated opportunity")
        scores.append(7)
    elif regime_confidence > 0.8:
        details.append("Stable regime - reduced arbitrage opportunity")
        scores.append(4)
    else:
        scores.append(5)

    # Calculate composite score
    arbitrage_score = sum(scores) / len(scores) if scores else 5.0

    # Estimate edge
    edge_estimate = estimate_edge(
        signal_strength=(arbitrage_score - 5) / 5,  # Normalize to -1 to 1
        statistical_significance=regime_confidence
    )

    return StatArbResult(
        arbitrage_score=min(10, max(0, arbitrage_score)),
        edge_estimate_bps=edge_estimate,
        mispricing_detected=mispricing_detected,
        mispricing_pct=mispricing_pct,
        regime=regime,
        regime_confidence=regime_confidence,
        details=details
    )
