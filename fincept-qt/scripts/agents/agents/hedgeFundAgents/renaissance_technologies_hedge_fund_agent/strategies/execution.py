"""
Execution Optimization Module

Renaissance Technologies' execution algorithms.
Minimize market impact and transaction costs.

Key concepts:
- Market impact modeling
- Optimal execution timing (VWAP, TWAP concepts)
- Transaction cost analysis (TCA)
- Slippage estimation
- Order book analysis
"""

import math
from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from enum import Enum


class ExecutionUrgency(str, Enum):
    IMMEDIATE = "immediate"  # Execute now, accept higher costs
    PATIENT = "patient"      # Spread execution over time
    AVOID = "avoid"          # Execution costs too high


@dataclass
class ExecutionAnalysis:
    """Execution optimization analysis output"""
    market_impact_bps: float
    spread_cost_bps: float
    commission_bps: float
    slippage_estimate_bps: float
    total_cost_bps: float
    optimal_trade_size_pct: float  # As % of ADV
    execution_time_minutes: int
    urgency: ExecutionUrgency
    execution_score: float  # 0-10, higher is better
    details: List[str]


def estimate_market_impact(
    trade_value: float,
    market_cap: float,
    avg_daily_volume: float,
    volatility: float = 0.02
) -> float:
    """
    Estimate market impact using square-root model.
    Impact = sigma * sqrt(Q/V) * k

    where:
    - sigma = daily volatility
    - Q = trade size
    - V = average daily volume
    - k = market impact coefficient (~0.1 for liquid stocks)
    """
    if avg_daily_volume <= 0:
        return 100.0  # Max impact if no volume data

    # Participation rate
    participation = trade_value / avg_daily_volume

    # Square-root impact model
    k = 0.1  # Impact coefficient
    impact = volatility * math.sqrt(participation) * k

    # Convert to basis points
    return impact * 10000


def estimate_spread_cost(
    market_cap: float,
    avg_daily_volume: Optional[float] = None
) -> float:
    """
    Estimate bid-ask spread cost in basis points.
    Based on market cap and liquidity.
    """
    # Market cap based spread estimate
    if market_cap >= 100_000_000_000:  # $100B+
        base_spread = 1.0
    elif market_cap >= 20_000_000_000:  # $20B+
        base_spread = 2.0
    elif market_cap >= 5_000_000_000:  # $5B+
        base_spread = 5.0
    elif market_cap >= 1_000_000_000:  # $1B+
        base_spread = 10.0
    elif market_cap >= 200_000_000:  # $200M+
        base_spread = 20.0
    else:
        base_spread = 50.0

    # Volume adjustment
    if avg_daily_volume:
        if avg_daily_volume >= 50_000_000:  # $50M+ daily
            base_spread *= 0.7
        elif avg_daily_volume >= 10_000_000:  # $10M+ daily
            base_spread *= 0.85
        elif avg_daily_volume < 1_000_000:  # <$1M daily
            base_spread *= 1.5

    return base_spread


def estimate_commission(
    trade_value: float,
    is_institutional: bool = True
) -> float:
    """
    Estimate commission cost in basis points.
    """
    if is_institutional:
        # Institutional rates (per share, converted to bps)
        if trade_value >= 1_000_000:
            return 0.5  # 0.5 bps for large trades
        elif trade_value >= 100_000:
            return 1.0
        else:
            return 2.0
    else:
        # Retail rates
        return 5.0


def estimate_slippage(
    participation_rate: float,
    volatility: float,
    urgency: ExecutionUrgency
) -> float:
    """
    Estimate execution slippage in basis points.
    """
    # Base slippage from volatility
    base_slippage = volatility * 100  # Convert to bps

    # Participation rate adjustment
    if participation_rate > 0.1:  # >10% of volume
        base_slippage *= 2.0
    elif participation_rate > 0.05:  # >5% of volume
        base_slippage *= 1.5
    elif participation_rate > 0.01:  # >1% of volume
        base_slippage *= 1.2

    # Urgency adjustment
    if urgency == ExecutionUrgency.IMMEDIATE:
        base_slippage *= 1.5
    elif urgency == ExecutionUrgency.PATIENT:
        base_slippage *= 0.7

    return base_slippage


def calculate_optimal_trade_size(
    market_cap: float,
    avg_daily_volume: float,
    volatility: float,
    max_impact_bps: float = 10.0
) -> float:
    """
    Calculate optimal trade size as percentage of ADV to limit impact.
    """
    if avg_daily_volume <= 0:
        return 0.01  # 1% minimum

    # Target impact
    target_participation = (max_impact_bps / 10000 / (volatility * 0.1)) ** 2

    # Cap at reasonable levels
    optimal_pct = min(0.25, max(0.01, target_participation))  # 1-25% of ADV

    return optimal_pct


def estimate_execution_time(
    trade_value: float,
    avg_daily_volume: float,
    target_participation: float = 0.05
) -> int:
    """
    Estimate execution time in minutes to achieve target participation rate.
    Assumes 6.5 hour trading day (390 minutes).
    """
    if avg_daily_volume <= 0:
        return 390  # Full day

    # Daily participation
    daily_participation = trade_value / avg_daily_volume

    # If can complete in less than target participation
    if daily_participation <= target_participation:
        # Execute in proportion of day
        minutes = int(daily_participation / target_participation * 390)
        return max(1, minutes)

    # Need multiple days
    days = daily_participation / target_participation
    return int(days * 390)


def determine_urgency(
    signal_decay_hours: float,
    execution_time_minutes: int,
    total_cost_bps: float,
    expected_edge_bps: float
) -> ExecutionUrgency:
    """
    Determine execution urgency based on signal decay and costs.
    """
    execution_hours = execution_time_minutes / 60

    # If signal decays before we can execute patiently
    if signal_decay_hours < execution_hours:
        if total_cost_bps < expected_edge_bps * 0.5:
            return ExecutionUrgency.IMMEDIATE
        else:
            return ExecutionUrgency.AVOID

    # If costs exceed expected edge
    if total_cost_bps >= expected_edge_bps:
        return ExecutionUrgency.AVOID

    # If costs are low enough to be patient
    if total_cost_bps < expected_edge_bps * 0.3:
        return ExecutionUrgency.PATIENT

    return ExecutionUrgency.PATIENT


def analyze_execution(
    trade_value: float,
    market_cap: float,
    avg_daily_volume: Optional[float] = None,
    volatility: float = 0.02,
    expected_edge_bps: float = 20.0,
    signal_decay_hours: float = 24.0
) -> ExecutionAnalysis:
    """
    Comprehensive execution optimization analysis.

    Args:
        trade_value: Intended trade value in dollars
        market_cap: Security market capitalization
        avg_daily_volume: Average daily trading volume in dollars
        volatility: Daily volatility (decimal)
        expected_edge_bps: Expected signal edge in basis points
        signal_decay_hours: Hours until signal loses value
    """
    details = []

    # Default ADV if not provided (estimate from market cap)
    if avg_daily_volume is None or avg_daily_volume <= 0:
        # Typical turnover: 0.5-1% of market cap daily
        avg_daily_volume = market_cap * 0.007
        details.append(f"Estimated ADV: ${avg_daily_volume / 1e6:.1f}M")

    # Participation rate
    participation_rate = trade_value / avg_daily_volume
    details.append(f"Participation rate: {participation_rate * 100:.2f}% of ADV")

    # Market impact
    market_impact_bps = estimate_market_impact(
        trade_value, market_cap, avg_daily_volume, volatility
    )
    details.append(f"Estimated market impact: {market_impact_bps:.1f} bps")

    # Spread cost
    spread_cost_bps = estimate_spread_cost(market_cap, avg_daily_volume)
    details.append(f"Spread cost: {spread_cost_bps:.1f} bps")

    # Commission
    commission_bps = estimate_commission(trade_value)

    # Optimal trade size
    optimal_trade_size_pct = calculate_optimal_trade_size(
        market_cap, avg_daily_volume, volatility
    )
    details.append(f"Optimal trade size: {optimal_trade_size_pct * 100:.1f}% of ADV")

    # Execution time
    execution_time = estimate_execution_time(
        trade_value, avg_daily_volume, optimal_trade_size_pct
    )
    details.append(f"Estimated execution time: {execution_time} minutes")

    # Preliminary urgency for slippage estimate
    preliminary_urgency = ExecutionUrgency.PATIENT

    # Slippage
    slippage_bps = estimate_slippage(participation_rate, volatility, preliminary_urgency)

    # Total cost
    total_cost_bps = market_impact_bps + spread_cost_bps + commission_bps + slippage_bps
    details.append(f"Total execution cost: {total_cost_bps:.1f} bps")

    # Final urgency determination
    urgency = determine_urgency(
        signal_decay_hours, execution_time, total_cost_bps, expected_edge_bps
    )
    details.append(f"Recommended urgency: {urgency.value}")

    # Recalculate slippage with final urgency
    slippage_bps = estimate_slippage(participation_rate, volatility, urgency)
    total_cost_bps = market_impact_bps + spread_cost_bps + commission_bps + slippage_bps

    # Execution score (0-10, higher is better)
    if urgency == ExecutionUrgency.AVOID:
        execution_score = 2.0
        details.append("Execution not recommended - costs exceed edge")
    else:
        # Score based on cost vs edge ratio
        cost_ratio = total_cost_bps / expected_edge_bps if expected_edge_bps > 0 else 1
        execution_score = max(0, 10 * (1 - cost_ratio))

        if execution_score >= 8:
            details.append("Excellent execution profile")
        elif execution_score >= 6:
            details.append("Good execution profile")
        elif execution_score >= 4:
            details.append("Acceptable execution profile")
        else:
            details.append("Challenging execution profile")

    return ExecutionAnalysis(
        market_impact_bps=market_impact_bps,
        spread_cost_bps=spread_cost_bps,
        commission_bps=commission_bps,
        slippage_estimate_bps=slippage_bps,
        total_cost_bps=total_cost_bps,
        optimal_trade_size_pct=optimal_trade_size_pct,
        execution_time_minutes=execution_time,
        urgency=urgency,
        execution_score=min(10, max(0, execution_score)),
        details=details
    )


def calculate_implementation_shortfall(
    decision_price: float,
    execution_price: float,
    benchmark_price: float
) -> Dict[str, float]:
    """
    Calculate implementation shortfall components.
    """
    # Total shortfall
    total_shortfall = (execution_price - decision_price) / decision_price

    # Delay cost (decision to benchmark)
    delay_cost = (benchmark_price - decision_price) / decision_price

    # Trading cost (benchmark to execution)
    trading_cost = (execution_price - benchmark_price) / benchmark_price

    return {
        "total_shortfall_bps": total_shortfall * 10000,
        "delay_cost_bps": delay_cost * 10000,
        "trading_cost_bps": trading_cost * 10000
    }
