"""
Renaissance Technologies Analysis Functions

Pure Python quantitative analysis functions extracted from legacy agents.
These functions run without LLM - pure mathematical/statistical analysis.

Used by main.py for the RenaissanceTechnologiesSystem.analyze() method.
"""

from typing import Dict, Any, Optional, List
from datetime import datetime

from .mean_reversion import analyze_mean_reversion
from .momentum import analyze_momentum
from .statistical_arbitrage import (
    analyze_statistical_arbitrage,
    analyze_pairs_trading,
    detect_market_regime,
    estimate_edge,
)
from .risk_management import (
    analyze_risk,
    calculate_kelly_criterion,
    calculate_position_size,
)
from .execution import analyze_execution, ExecutionUrgency

# Import schemas
from ..models.schemas import MedallionDecision, PositionSizing, SignalType


# =============================================================================
# SIGNAL ANALYSIS
# =============================================================================

def run_signal_analysis(
    ticker: str,
    prices: List[float],
    revenues: Optional[List[float]] = None,
    earnings: Optional[List[float]] = None,
    metrics: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """
    Run comprehensive signal analysis without LLM (pure quant).

    Args:
        ticker: Stock ticker
        prices: Historical prices (most recent first)
        revenues: Historical revenues (optional)
        earnings: Historical earnings (optional)
        metrics: Additional financial metrics

    Returns:
        Dictionary with signal analysis results
    """
    # Mean reversion analysis
    mean_reversion = analyze_mean_reversion(prices)

    # Momentum analysis
    momentum = analyze_momentum(prices, revenues, earnings)

    # Pattern recognition (simplified autocorrelation)
    returns = [(prices[i] / prices[i + 1]) - 1 for i in range(len(prices) - 1)]
    autocorr = _calculate_autocorrelation(returns, lag=1) if len(returns) > 10 else 0

    # Determine dominant regime
    if abs(mean_reversion.z_score) > 2 and mean_reversion.statistical_significance > 0.7:
        regime = "mean_reverting"
    elif momentum.trend_strength > 0.6 and abs(momentum.signal_strength) > 0.5:
        regime = "trending"
    else:
        regime = "random_walk"

    # Aggregate signal
    aggregate_signal = (
        mean_reversion.signal_strength * 0.3 +
        momentum.signal_strength * 0.4 +
        (autocorr * 0.3 if regime == "mean_reverting" else -autocorr * 0.3)
    )

    # Statistical significance (combined)
    stat_sig = (
        mean_reversion.statistical_significance * 0.4 +
        momentum.trend_strength * 0.3 +
        (0.3 if abs(autocorr) > 0.1 else 0.15)
    )

    # Factor exposures placeholder
    factor_exposures = {
        "value": 0.0,
        "momentum": momentum.signal_strength,
        "quality": 0.0,
        "size": 0.0,
        "volatility": 0.0
    }

    if metrics:
        pe = metrics.get("price_to_earnings")
        roe = metrics.get("return_on_equity")

        if pe and pe > 0:
            factor_exposures["value"] = max(-1, min(1, (20 - pe) / 20))
        if roe:
            factor_exposures["quality"] = max(-1, min(1, (roe - 0.1) / 0.2))

    return {
        "ticker": ticker,
        "mean_reversion": {
            "signal_strength": mean_reversion.signal_strength,
            "z_score": mean_reversion.z_score,
            "half_life": mean_reversion.half_life,
            "statistical_significance": mean_reversion.statistical_significance,
            "details": mean_reversion.details
        },
        "momentum": {
            "signal_strength": momentum.signal_strength,
            "trend_strength": momentum.trend_strength,
            "acceleration": momentum.acceleration,
            "optimal_lookback": momentum.optimal_lookback,
            "details": momentum.details
        },
        "pattern_recognition": {
            "pattern_type": regime,
            "pattern_confidence": stat_sig,
            "autocorrelation": autocorr,
            "regime": regime
        },
        "cross_sectional": {
            "percentile_rank": 50.0,
            "factor_exposures": factor_exposures,
            "composite_score": 5.0
        },
        "aggregate_signal": max(-1, min(1, aggregate_signal)),
        "statistical_significance": max(0, min(1, stat_sig)),
        "reasoning": _generate_signal_reasoning(
            mean_reversion, momentum, regime, aggregate_signal, stat_sig
        )
    }


# =============================================================================
# RISK ANALYSIS
# =============================================================================

def run_risk_analysis(
    ticker: str,
    prices: List[float],
    market_prices: Optional[List[float]] = None,
    market_cap: float = 10_000_000_000,
    signal_strength: float = 0.0,
    signal_confidence: float = 50.0
) -> Dict[str, Any]:
    """
    Run comprehensive risk analysis without LLM (pure quant).
    """
    risk_metrics = analyze_risk(prices, market_prices, market_cap)

    win_probability = 0.5 + (signal_strength * 0.01)
    win_loss_ratio = 1.0 + abs(signal_strength) * 0.5

    kelly = calculate_kelly_criterion(win_probability, win_loss_ratio)
    position_size = calculate_position_size(
        portfolio_value=100_000_000,
        risk_score=risk_metrics.risk_score,
        signal_confidence=signal_confidence
    )

    risk_adjustment = 1 - (risk_metrics.risk_score / 10) * 0.5
    risk_adjusted_signal = signal_strength * risk_adjustment

    return {
        "ticker": ticker,
        "volatility": {
            "realized_volatility": risk_metrics.volatility_annualized,
            "implied_volatility": None,
            "volatility_regime": risk_metrics.volatility_regime.value,
            "vol_of_vol": risk_metrics.volatility_annualized * 0.3
        },
        "correlation": {
            "market_correlation": risk_metrics.correlation_to_market,
            "sector_correlation": risk_metrics.correlation_to_market * 0.8,
            "idiosyncratic_ratio": risk_metrics.idiosyncratic_ratio
        },
        "factor_decomposition": {
            "market_beta": risk_metrics.beta,
            "size_factor": 0.0,
            "value_factor": 0.0,
            "momentum_factor": signal_strength,
            "quality_factor": 0.0,
            "total_factor_risk": 1 - risk_metrics.idiosyncratic_ratio,
            "specific_risk": risk_metrics.idiosyncratic_ratio
        },
        "drawdown": {
            "max_drawdown": risk_metrics.max_drawdown,
            "current_drawdown": risk_metrics.current_drawdown,
            "recovery_time_estimate": _estimate_recovery_time(
                risk_metrics.current_drawdown,
                risk_metrics.volatility_annualized
            ),
            "tail_risk_var_95": risk_metrics.var_95
        },
        "var": {
            "var_95": risk_metrics.var_95,
            "var_99": risk_metrics.var_99
        },
        "ratios": {
            "sharpe_ratio": risk_metrics.sharpe_ratio,
            "sortino_ratio": risk_metrics.sortino_ratio
        },
        "position_sizing": {
            "kelly_fraction": kelly,
            "recommended_position_pct": position_size,
            "max_position_pct": min(5.0, position_size * 2)
        },
        "liquidity": {
            "liquidity_score": risk_metrics.liquidity_score
        },
        "risk_score": risk_metrics.risk_score,
        "risk_adjusted_signal": risk_adjusted_signal,
        "reasoning": _generate_risk_reasoning(risk_metrics, kelly, position_size),
        "details": risk_metrics.details
    }


# =============================================================================
# EXECUTION ANALYSIS
# =============================================================================

def run_execution_analysis(
    ticker: str,
    trade_value: float,
    market_cap: float,
    avg_daily_volume: Optional[float] = None,
    volatility: float = 0.02,
    expected_edge_bps: float = 20.0,
    signal_decay_hours: float = 24.0
) -> Dict[str, Any]:
    """
    Run comprehensive execution analysis without LLM (pure quant).
    """
    execution = analyze_execution(
        trade_value=trade_value,
        market_cap=market_cap,
        avg_daily_volume=avg_daily_volume,
        volatility=volatility,
        expected_edge_bps=expected_edge_bps,
        signal_decay_hours=signal_decay_hours
    )

    net_edge_bps = expected_edge_bps - execution.total_cost_bps

    if execution.urgency == ExecutionUrgency.AVOID:
        feasibility = "not_recommended"
    elif net_edge_bps > expected_edge_bps * 0.5:
        feasibility = "highly_feasible"
    elif net_edge_bps > 0:
        feasibility = "feasible"
    else:
        feasibility = "marginal"

    return {
        "ticker": ticker,
        "trade_value": trade_value,
        "market_impact": {
            "estimated_impact_bps": execution.market_impact_bps,
            "optimal_trade_size": execution.optimal_trade_size_pct,
            "time_to_execute": execution.execution_time_minutes
        },
        "transaction_costs": {
            "spread_cost_bps": execution.spread_cost_bps,
            "commission_bps": execution.commission_bps,
            "slippage_estimate_bps": execution.slippage_estimate_bps,
            "total_cost_bps": execution.total_cost_bps
        },
        "liquidity": {
            "average_daily_volume": avg_daily_volume or (market_cap * 0.007),
            "liquidity_score": _calculate_liquidity_score(market_cap, avg_daily_volume),
            "bid_ask_spread_bps": execution.spread_cost_bps,
            "depth_ratio": _estimate_depth_ratio(market_cap)
        },
        "execution_score": execution.execution_score,
        "recommended_urgency": execution.urgency.value,
        "net_edge_bps": net_edge_bps,
        "feasibility": feasibility,
        "reasoning": _generate_execution_reasoning(execution, net_edge_bps, feasibility),
        "details": execution.details
    }


# =============================================================================
# STATISTICAL ARBITRAGE ANALYSIS
# =============================================================================

def run_stat_arb_analysis(
    ticker: str,
    prices: List[float],
    intrinsic_estimates: Optional[List[float]] = None,
    peer_prices: Optional[List[float]] = None,
    metrics: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """
    Run comprehensive statistical arbitrage analysis without LLM (pure quant).
    """
    stat_arb = analyze_statistical_arbitrage(
        prices=prices,
        intrinsic_estimates=intrinsic_estimates,
        peer_prices=peer_prices,
        metrics=metrics
    )

    pairs_result = None
    if peer_prices and len(peer_prices) >= 30:
        pairs_result = analyze_pairs_trading(prices, peer_prices)

    regime, regime_confidence = detect_market_regime(prices)
    regime_duration = _estimate_regime_duration(prices, regime.value)
    mispricing_data = _analyze_mispricing(prices, intrinsic_estimates, metrics)

    arbitrage_score = stat_arb.arbitrage_score
    edge_bps = estimate_edge(
        signal_strength=(arbitrage_score - 5) / 5,
        statistical_significance=regime_confidence,
        transaction_costs_bps=10
    )

    return {
        "ticker": ticker,
        "pairs_trading": {
            "signal": pairs_result.signal if pairs_result else "neutral",
            "spread_z_score": pairs_result.spread_z_score if pairs_result else 0.0,
            "cointegration_score": pairs_result.cointegration_score if pairs_result else 0.0,
            "half_life": pairs_result.half_life if pairs_result else float('inf'),
            "details": pairs_result.details if pairs_result else []
        } if pairs_result else {
            "signal": "neutral",
            "spread_z_score": 0.0,
            "cointegration_score": 0.0,
            "half_life": float('inf'),
            "details": ["No peer data for pairs analysis"]
        },
        "mispricing": {
            "fair_value_estimate": mispricing_data["fair_value"],
            "current_price": prices[0],
            "mispricing_percentage": mispricing_data["mispricing_pct"],
            "confidence": mispricing_data["confidence"]
        },
        "regime": {
            "current_regime": regime.value,
            "regime_probability": regime_confidence,
            "regime_duration": regime_duration,
            "transition_probability": 1 - regime_confidence
        },
        "arbitrage_score": arbitrage_score,
        "edge_estimate_bps": edge_bps,
        "reasoning": _generate_stat_arb_reasoning(
            stat_arb, pairs_result, regime, regime_confidence, edge_bps
        ),
        "details": stat_arb.details
    }


# =============================================================================
# MEDALLION SYNTHESIZER
# =============================================================================

def synthesize_decision(
    ticker: str,
    signal_analysis: Dict[str, Any],
    risk_analysis: Dict[str, Any],
    execution_analysis: Dict[str, Any],
    stat_arb_analysis: Dict[str, Any]
) -> MedallionDecision:
    """
    Synthesize all team analyses into a final Medallion decision.
    Pure quantitative - no LLM needed.
    """
    aggregate_signal = signal_analysis.get("aggregate_signal", 0)
    stat_sig = signal_analysis.get("statistical_significance", 0.5)
    risk_score = risk_analysis.get("risk_score", 5)
    execution_score = execution_analysis.get("execution_score", 5)
    arbitrage_score = stat_arb_analysis.get("arbitrage_score", 5)

    composite_score = (
        (aggregate_signal + 1) * 5 * 0.35 +
        arbitrage_score * 0.30 +
        (10 - risk_score) * 0.20 +
        execution_score * 0.15
    )

    if composite_score >= 7.0 and stat_sig > 0.85:
        signal = SignalType.BULLISH
    elif composite_score <= 3.0 or stat_sig < 0.50:
        signal = SignalType.BEARISH
    else:
        signal = SignalType.NEUTRAL

    base_confidence = min(100, composite_score * 10)
    confidence = base_confidence * stat_sig

    edge_from_signal = abs(aggregate_signal) * 0.002
    edge_from_arb = (stat_arb_analysis.get("edge_estimate_bps", 0) or 0) / 10000
    execution_cost = (execution_analysis.get("transaction_costs", {}).get("total_cost_bps", 10) or 10) / 10000

    net_edge = (edge_from_signal + edge_from_arb) - execution_cost
    statistical_edge = max(0, min(1, net_edge * 100))

    kelly = risk_analysis.get("position_sizing", {}).get("kelly_fraction", 0.1)
    risk_adjusted_size = risk_analysis.get("position_sizing", {}).get("recommended_position_pct", 2.0)

    if execution_analysis.get("recommended_urgency") == "avoid":
        risk_adjusted_size *= 0.25
    elif execution_score < 5:
        risk_adjusted_size *= 0.75

    position_sizing = PositionSizing(
        recommended_size_pct=min(5.0, risk_adjusted_size),
        kelly_fraction=kelly,
        max_position_size_pct=min(5.0, kelly * 100),
        leverage_recommendation=_calculate_leverage(risk_score, stat_sig)
    )

    signal_strength = {
        "mean_reversion": signal_analysis.get("mean_reversion", {}).get("signal_strength", 0),
        "momentum": signal_analysis.get("momentum", {}).get("signal_strength", 0),
        "statistical_arbitrage": (arbitrage_score - 5) / 5,
        "risk_adjusted_return": aggregate_signal * (1 - risk_score / 10)
    }

    reasoning = _generate_medallion_reasoning(
        signal, composite_score, stat_sig,
        signal_analysis, risk_analysis, execution_analysis, stat_arb_analysis, net_edge
    )

    key_factors = _identify_key_factors(
        signal_analysis, risk_analysis, execution_analysis, stat_arb_analysis
    )

    risks = _identify_risks(risk_analysis, execution_analysis, stat_arb_analysis)

    return MedallionDecision(
        ticker=ticker,
        signal=signal,
        confidence=round(confidence, 2),
        statistical_edge=round(statistical_edge, 4),
        signal_score=round((aggregate_signal + 1) * 5, 2),
        risk_score=round(risk_score, 2),
        execution_score=round(execution_score, 2),
        arbitrage_score=round(arbitrage_score, 2),
        position_sizing=position_sizing,
        signal_strength=signal_strength,
        reasoning=reasoning,
        key_factors=key_factors,
        risks=risks,
        timestamp=datetime.utcnow().isoformat()
    )


# =============================================================================
# HELPER FUNCTIONS
# =============================================================================

def _calculate_autocorrelation(data: List[float], lag: int) -> float:
    if len(data) <= lag:
        return 0
    mean_val = sum(data) / len(data)
    numerator = sum(
        (data[i] - mean_val) * (data[i + lag] - mean_val)
        for i in range(len(data) - lag)
    )
    denominator = sum((x - mean_val) ** 2 for x in data)
    return numerator / denominator if denominator != 0 else 0


def _generate_signal_reasoning(mean_reversion, momentum, regime, aggregate_signal, stat_sig) -> str:
    parts = [f"Market regime detected: {regime}"]
    if abs(mean_reversion.z_score) > 1.5:
        direction = "below" if mean_reversion.z_score < 0 else "above"
        parts.append(f"Mean reversion: Price {abs(mean_reversion.z_score):.1f}s {direction} mean")
    if abs(momentum.signal_strength) > 0.3:
        direction = "positive" if momentum.signal_strength > 0 else "negative"
        parts.append(f"Momentum: {direction} ({momentum.trend_strength:.0%} trend)")
    if aggregate_signal > 0.3:
        parts.append(f"Aggregate: BULLISH ({aggregate_signal:+.2f})")
    elif aggregate_signal < -0.3:
        parts.append(f"Aggregate: BEARISH ({aggregate_signal:+.2f})")
    else:
        parts.append(f"Aggregate: NEUTRAL ({aggregate_signal:+.2f})")
    parts.append(f"Statistical significance: {stat_sig:.0%}")
    return ". ".join(parts) + "."


def _estimate_recovery_time(current_drawdown: float, volatility: float) -> Optional[int]:
    if current_drawdown <= 0:
        return None
    expected_daily_return = 0.10 / 252
    if expected_daily_return <= 0:
        return None
    recovery_days = int(current_drawdown / expected_daily_return)
    return min(recovery_days, 252 * 2)


def _generate_risk_reasoning(risk_metrics, kelly, position_size) -> str:
    parts = [
        f"Volatility regime: {risk_metrics.volatility_regime.value.upper()}",
        f"Annualized volatility: {risk_metrics.volatility_annualized:.1%}",
    ]
    if risk_metrics.risk_score < 3:
        parts.append("Risk profile: LOW")
    elif risk_metrics.risk_score < 6:
        parts.append("Risk profile: MODERATE")
    else:
        parts.append("Risk profile: HIGH")
    parts.append(f"Recommended position: {position_size:.1%}")
    if risk_metrics.sharpe_ratio:
        parts.append(f"Sharpe: {risk_metrics.sharpe_ratio:.2f}")
    return ". ".join(parts) + "."


def _calculate_liquidity_score(market_cap: float, avg_daily_volume: Optional[float]) -> float:
    if market_cap >= 200_000_000_000:
        cap_score = 10
    elif market_cap >= 50_000_000_000:
        cap_score = 9
    elif market_cap >= 10_000_000_000:
        cap_score = 8
    elif market_cap >= 2_000_000_000:
        cap_score = 6
    elif market_cap >= 500_000_000:
        cap_score = 4
    else:
        cap_score = 2
    if avg_daily_volume:
        if avg_daily_volume >= 50_000_000:
            return min(10, cap_score + 1)
        elif avg_daily_volume < 1_000_000:
            return max(1, cap_score - 2)
    return cap_score


def _estimate_depth_ratio(market_cap: float) -> float:
    if market_cap >= 100_000_000_000:
        return 0.9
    elif market_cap >= 10_000_000_000:
        return 0.7
    elif market_cap >= 1_000_000_000:
        return 0.5
    return 0.3


def _generate_execution_reasoning(execution, net_edge_bps, feasibility) -> str:
    parts = [
        f"Total cost: {execution.total_cost_bps:.1f} bps",
        f"Net edge: {net_edge_bps:.1f} bps",
        f"Urgency: {execution.urgency.value.upper()}",
        f"Score: {execution.execution_score:.1f}/10",
    ]
    return ". ".join(parts) + "."


def _analyze_mispricing(prices, intrinsic_estimates, metrics) -> Dict[str, Any]:
    current_price = prices[0]
    if intrinsic_estimates and len(intrinsic_estimates) > 0:
        fair_value = sum(intrinsic_estimates) / len(intrinsic_estimates)
        mispricing_pct = (current_price - fair_value) / fair_value
        confidence = 0.5
        return {"fair_value": fair_value, "mispricing_pct": mispricing_pct, "confidence": confidence}
    return {"fair_value": current_price, "mispricing_pct": 0.0, "confidence": 0.0}


def _estimate_regime_duration(prices: List[float], current_regime: str) -> int:
    if len(prices) < 40:
        return 20
    window = 20
    consistent_periods = 0
    for i in range(0, min(len(prices) - window, 200), 10):
        subset = prices[i:i + window]
        regime, _ = detect_market_regime(subset)
        if regime.value == current_regime:
            consistent_periods += 10
        else:
            break
    return max(window, consistent_periods)


def _generate_stat_arb_reasoning(stat_arb, pairs_result, regime, regime_confidence, edge_bps) -> str:
    parts = [f"Regime: {regime.value.upper()} ({regime_confidence:.0%})"]
    if stat_arb.mispricing_detected:
        direction = "undervalued" if stat_arb.mispricing_pct < 0 else "overvalued"
        parts.append(f"Mispricing: {abs(stat_arb.mispricing_pct) * 100:.1f}% {direction}")
    if edge_bps > 10:
        parts.append(f"Edge: {edge_bps:.1f} bps - ATTRACTIVE")
    elif edge_bps > 0:
        parts.append(f"Edge: {edge_bps:.1f} bps - MARGINAL")
    parts.append(f"Score: {stat_arb.arbitrage_score:.1f}/10")
    return ". ".join(parts) + "."


def _calculate_leverage(risk_score: float, stat_sig: float) -> float:
    base_leverage = 2.0
    risk_multiplier = 1 + (5 - risk_score) / 5
    confidence_multiplier = 0.5 + stat_sig * 0.5
    leverage = base_leverage * risk_multiplier * confidence_multiplier
    return min(12.5, max(1.0, leverage))


def _generate_medallion_reasoning(signal, composite_score, stat_sig, signal_analysis, risk_analysis, execution_analysis, stat_arb_analysis, net_edge) -> str:
    parts = [
        f"DECISION: {signal.value.upper()}",
        f"Composite: {composite_score:.2f}/10",
        f"Significance: {stat_sig:.0%}",
        f"Net edge: {net_edge * 10000:.1f} bps",
    ]
    return ". ".join(parts) + "."


def _identify_key_factors(signal_analysis, risk_analysis, execution_analysis, stat_arb_analysis) -> List[str]:
    factors = []
    mean_rev = signal_analysis.get("mean_reversion", {})
    if abs(mean_rev.get("z_score", 0)) > 2:
        direction = "below" if mean_rev.get("z_score", 0) < 0 else "above"
        factors.append(f"Price {abs(mean_rev.get('z_score', 0)):.1f}s {direction} mean")
    momentum = signal_analysis.get("momentum", {})
    if momentum.get("trend_strength", 0) > 0.6:
        factors.append(f"Strong trend ({momentum.get('trend_strength', 0):.0%})")
    if risk_analysis.get("risk_score", 5) < 3:
        factors.append("Low risk profile")
    elif risk_analysis.get("risk_score", 5) > 7:
        factors.append("High risk - reduced exposure")
    if execution_analysis.get("execution_score", 5) >= 8:
        factors.append("Excellent execution")
    return factors[:5]


def _identify_risks(risk_analysis, execution_analysis, stat_arb_analysis) -> List[str]:
    risks = []
    vol_regime = risk_analysis.get("volatility", {}).get("volatility_regime", "normal")
    if vol_regime in ["high", "extreme"]:
        risks.append(f"Elevated volatility ({vol_regime})")
    current_dd = risk_analysis.get("drawdown", {}).get("current_drawdown", 0)
    if current_dd > 0.1:
        risks.append(f"In {current_dd:.0%} drawdown")
    if execution_analysis.get("recommended_urgency") == "avoid":
        risks.append("Execution costs exceed edge")
    return risks[:5]


# Export all analysis functions
__all__ = [
    "run_signal_analysis",
    "run_risk_analysis",
    "run_execution_analysis",
    "run_stat_arb_analysis",
    "synthesize_decision",
]
