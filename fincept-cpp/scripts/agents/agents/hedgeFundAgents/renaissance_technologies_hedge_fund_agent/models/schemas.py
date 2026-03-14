"""
Pydantic schemas for Renaissance Technologies agent outputs.
Structured outputs following RenTech's quantitative philosophy.
"""

from pydantic import BaseModel, Field
from typing import Dict, List, Optional, Literal
from enum import Enum


class SignalType(str, Enum):
    BULLISH = "bullish"
    BEARISH = "bearish"
    NEUTRAL = "neutral"


class ConfidenceLevel(str, Enum):
    HIGH = "high"      # >80% statistical significance
    MEDIUM = "medium"  # 60-80% significance
    LOW = "low"        # <60% significance


# ============================================================
# Signal Generation Models
# ============================================================

class MeanReversionSignal(BaseModel):
    """Mean reversion signal output"""
    signal_strength: float = Field(..., ge=-1.0, le=1.0, description="Signal strength (-1 to 1)")
    z_score: float = Field(..., description="Current z-score from historical mean")
    half_life: Optional[float] = Field(None, description="Mean reversion half-life in periods")
    statistical_significance: float = Field(..., ge=0, le=1, description="P-value significance")


class MomentumSignal(BaseModel):
    """Momentum signal output"""
    signal_strength: float = Field(..., ge=-1.0, le=1.0)
    trend_strength: float = Field(..., ge=0, le=1, description="Trend persistence measure")
    acceleration: float = Field(..., description="Rate of momentum change")
    lookback_period: int = Field(..., description="Optimal lookback period identified")


class PatternRecognitionSignal(BaseModel):
    """Statistical pattern recognition output"""
    pattern_type: str = Field(..., description="Identified pattern type")
    pattern_confidence: float = Field(..., ge=0, le=1)
    autocorrelation: float = Field(..., ge=-1, le=1)
    regime: Literal["trending", "mean_reverting", "random_walk"] = Field(...)


class CrossSectionalSignal(BaseModel):
    """Cross-sectional ranking signal"""
    percentile_rank: float = Field(..., ge=0, le=100, description="Percentile vs peers")
    factor_exposures: Dict[str, float] = Field(..., description="Factor exposure coefficients")
    composite_score: float = Field(..., ge=0, le=10)


class SignalGenerationOutput(BaseModel):
    """Complete signal generation team output"""
    mean_reversion: MeanReversionSignal
    momentum: MomentumSignal
    pattern_recognition: PatternRecognitionSignal
    cross_sectional: CrossSectionalSignal
    aggregate_signal: float = Field(..., ge=-1, le=1, description="Combined signal")
    statistical_significance: float = Field(..., ge=0, le=1)
    reasoning: str = Field(..., description="Signal generation reasoning")


# ============================================================
# Risk Modeling Models
# ============================================================

class VolatilityMetrics(BaseModel):
    """Volatility analysis output"""
    realized_volatility: float = Field(..., ge=0)
    implied_volatility: Optional[float] = Field(None, ge=0)
    volatility_regime: Literal["low", "normal", "high", "extreme"] = Field(...)
    vol_of_vol: float = Field(..., ge=0, description="Volatility of volatility")


class CorrelationRisk(BaseModel):
    """Correlation risk analysis"""
    market_correlation: float = Field(..., ge=-1, le=1)
    sector_correlation: float = Field(..., ge=-1, le=1)
    idiosyncratic_ratio: float = Field(..., ge=0, le=1, description="Portion of risk that is idiosyncratic")


class FactorRiskDecomposition(BaseModel):
    """Factor risk decomposition"""
    market_beta: float = Field(...)
    size_factor: float = Field(...)
    value_factor: float = Field(...)
    momentum_factor: float = Field(...)
    quality_factor: float = Field(...)
    total_factor_risk: float = Field(..., ge=0)
    specific_risk: float = Field(..., ge=0)


class DrawdownAnalysis(BaseModel):
    """Maximum drawdown risk analysis"""
    max_drawdown: float = Field(..., ge=0, le=1)
    current_drawdown: float = Field(..., ge=0, le=1)
    recovery_time_estimate: Optional[int] = Field(None, description="Estimated recovery periods")
    tail_risk_var_95: float = Field(..., description="95% Value at Risk")


class RiskModelingOutput(BaseModel):
    """Complete risk modeling team output"""
    volatility: VolatilityMetrics
    correlation: CorrelationRisk
    factor_decomposition: FactorRiskDecomposition
    drawdown: DrawdownAnalysis
    risk_score: float = Field(..., ge=0, le=10, description="Overall risk score (lower is better)")
    risk_adjusted_signal: float = Field(..., ge=-1, le=1)
    reasoning: str


# ============================================================
# Execution Optimization Models
# ============================================================

class MarketImpactEstimate(BaseModel):
    """Market impact estimation"""
    estimated_impact_bps: float = Field(..., ge=0, description="Estimated impact in basis points")
    optimal_trade_size: float = Field(..., ge=0, description="Optimal position size")
    time_to_execute: int = Field(..., ge=1, description="Recommended execution time in minutes")


class TransactionCostAnalysis(BaseModel):
    """Transaction cost analysis"""
    spread_cost_bps: float = Field(..., ge=0)
    commission_bps: float = Field(..., ge=0)
    slippage_estimate_bps: float = Field(..., ge=0)
    total_cost_bps: float = Field(..., ge=0)


class LiquidityAnalysis(BaseModel):
    """Liquidity risk analysis"""
    average_daily_volume: float = Field(..., ge=0)
    liquidity_score: float = Field(..., ge=0, le=10, description="10 = highly liquid")
    bid_ask_spread_bps: float = Field(..., ge=0)
    depth_ratio: float = Field(..., ge=0, description="Order book depth ratio")


class ExecutionOptimizationOutput(BaseModel):
    """Complete execution optimization output"""
    market_impact: MarketImpactEstimate
    transaction_costs: TransactionCostAnalysis
    liquidity: LiquidityAnalysis
    execution_score: float = Field(..., ge=0, le=10, description="Execution feasibility score")
    recommended_urgency: Literal["immediate", "patient", "avoid"] = Field(...)
    reasoning: str


# ============================================================
# Statistical Arbitrage Models
# ============================================================

class PairsTradingSignal(BaseModel):
    """Pairs trading signal"""
    spread_z_score: float = Field(...)
    cointegration_score: float = Field(..., ge=0, le=1)
    half_life: float = Field(..., ge=0, description="Spread mean reversion half-life")
    signal: Literal["long_spread", "short_spread", "neutral"] = Field(...)


class MispricingDetection(BaseModel):
    """Statistical mispricing detection"""
    fair_value_estimate: float = Field(..., ge=0)
    current_price: float = Field(..., ge=0)
    mispricing_percentage: float = Field(..., description="Percentage mispriced")
    confidence: float = Field(..., ge=0, le=1)


class RegimeDetection(BaseModel):
    """Market regime detection"""
    current_regime: Literal["bull", "bear", "sideways", "transition"] = Field(...)
    regime_probability: float = Field(..., ge=0, le=1)
    regime_duration: int = Field(..., description="Periods in current regime")
    transition_probability: float = Field(..., ge=0, le=1)


class StatisticalArbitrageOutput(BaseModel):
    """Complete statistical arbitrage output"""
    pairs_trading: PairsTradingSignal
    mispricing: MispricingDetection
    regime: RegimeDetection
    arbitrage_score: float = Field(..., ge=0, le=10)
    edge_estimate_bps: float = Field(..., description="Estimated edge in basis points")
    reasoning: str


# ============================================================
# Final Medallion Decision Model
# ============================================================

class PositionSizing(BaseModel):
    """Position sizing recommendation"""
    recommended_size_pct: float = Field(..., ge=0, le=100, description="Portfolio allocation %")
    kelly_fraction: float = Field(..., ge=0, le=1, description="Kelly criterion fraction")
    max_position_size_pct: float = Field(..., ge=0, le=100)
    leverage_recommendation: float = Field(..., ge=0, description="Recommended leverage")


class MedallionDecision(BaseModel):
    """
    Final Medallion Fund decision output.
    Jim Simons style systematic synthesis.
    """
    ticker: str = Field(...)
    signal: SignalType = Field(...)
    confidence: float = Field(..., ge=0, le=100, description="Confidence percentage")
    statistical_edge: float = Field(..., ge=0, le=1, description="Estimated edge")

    # Component scores
    signal_score: float = Field(..., ge=0, le=10)
    risk_score: float = Field(..., ge=0, le=10)
    execution_score: float = Field(..., ge=0, le=10)
    arbitrage_score: float = Field(..., ge=0, le=10)

    # Position recommendation
    position_sizing: PositionSizing

    # Detailed breakdown
    signal_strength: Dict[str, float] = Field(..., description="Component signal strengths")

    # Reasoning
    reasoning: str = Field(..., description="Systematic decision reasoning")
    key_factors: List[str] = Field(..., description="Key decision factors")
    risks: List[str] = Field(..., description="Key risks identified")

    # Metadata
    model_version: str = Field(default="2.0.0")
    timestamp: Optional[str] = Field(None)


class RenaissancePortfolioAnalysis(BaseModel):
    """Multi-ticker portfolio analysis output"""
    decisions: Dict[str, MedallionDecision] = Field(..., description="Per-ticker decisions")
    portfolio_signal: SignalType = Field(..., description="Overall portfolio signal")
    portfolio_confidence: float = Field(..., ge=0, le=100)
    correlation_matrix: Optional[Dict[str, Dict[str, float]]] = Field(None)
    portfolio_var_95: Optional[float] = Field(None, description="Portfolio 95% VaR")
    summary: str = Field(...)
