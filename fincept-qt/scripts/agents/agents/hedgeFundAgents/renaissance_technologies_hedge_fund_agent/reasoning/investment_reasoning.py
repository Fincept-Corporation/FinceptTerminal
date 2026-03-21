"""
Investment Reasoning

Specialized reasoning for investment analysis decisions:
- Signal evaluation
- Position sizing
- Entry/exit timing
- Expected return calculation
"""

from typing import Optional, List, Dict, Any
from dataclasses import dataclass
from datetime import datetime
import math

from .base import (
    ReasoningEngine,
    ReasoningChain,
    ReasoningStep,
    ReasoningType,
    StepStatus,
    get_reasoning_engine,
)
from ..config import get_config


@dataclass
class SignalEvaluation:
    """Result of signal reasoning"""
    signal_id: str
    ticker: str
    direction: str
    signal_type: str

    # Statistical quality
    statistical_quality: str  # excellent, good, marginal, poor
    p_value_assessment: str
    sample_size_assessment: str

    # Market fit
    regime_fit: str  # strong, moderate, weak, contrary
    sector_outlook: str

    # Risk/Reward
    expected_return_bps: float
    expected_risk_bps: float
    risk_reward_ratio: float

    # Overall
    overall_score: float  # 0-100
    recommendation: str  # trade, skip, investigate
    confidence: float
    reasoning_summary: str


@dataclass
class SizingDecision:
    """Result of position sizing reasoning"""
    ticker: str
    direction: str

    # Calculated sizes
    kelly_size_pct: float
    liquidity_adjusted_pct: float
    correlation_adjusted_pct: float
    risk_limit_adjusted_pct: float

    # Final recommendation
    final_size_pct: float
    final_shares: int
    final_notional: float

    # Confidence
    confidence: float
    reasoning_summary: str


class InvestmentReasoner:
    """
    Investment-focused reasoning system.

    Provides structured reasoning for:
    1. Signal quality evaluation
    2. Position sizing decisions
    3. Entry/exit timing
    4. Risk/reward assessment
    """

    def __init__(self, reasoning_engine: Optional[ReasoningEngine] = None):
        self.engine = reasoning_engine or get_reasoning_engine()
        self.config = get_config()

    async def evaluate_signal(
        self,
        signal: Dict[str, Any],
        market_context: Dict[str, Any],
        historical_signals: Optional[List[Dict[str, Any]]] = None,
    ) -> SignalEvaluation:
        """
        Perform structured reasoning to evaluate a trading signal.

        This mimics how a Renaissance quant would think through
        whether a signal is worth trading.
        """
        ticker = signal.get("ticker", "UNKNOWN")
        direction = signal.get("direction", "long")
        signal_type = signal.get("signal_type", "unknown")

        # Create reasoning chain
        chain = self.engine.create_chain(
            reasoning_type=ReasoningType.SIGNAL_ANALYSIS,
            subject=f"{ticker} {direction} {signal_type}",
            context={
                "signal": signal,
                "market": market_context,
                "history": historical_signals or [],
            },
        )

        # Populate step inputs
        chain.steps[0].input_data = {
            "ticker": ticker,
            "data_source": signal.get("data_source", "unknown"),
            "lookback_period": signal.get("lookback_days", 20),
        }

        chain.steps[1].input_data = {
            "p_value": signal.get("p_value", 1.0),
            "t_statistic": signal.get("t_statistic", 0),
            "sample_size": signal.get("sample_size", 0),
            "threshold": self.config.risk_limits.min_p_value,
        }

        chain.steps[2].input_data = {
            "signal_type": signal_type,
            "historical_signals": historical_signals or [],
        }

        chain.steps[3].input_data = {
            "current_regime": market_context.get("regime", "unknown"),
            "vix_level": market_context.get("vix", 20),
            "signal_regime_history": signal.get("regime_performance", {}),
        }

        chain.steps[4].input_data = {
            "expected_return_bps": signal.get("expected_return_bps", 0),
            "historical_volatility": signal.get("volatility", 0),
            "max_drawdown_history": signal.get("max_drawdown", 0),
        }

        # Execute reasoning chain
        await self.engine.execute_chain(chain)

        # Synthesize evaluation from chain results
        return self._synthesize_signal_evaluation(chain, signal)

    def _synthesize_signal_evaluation(
        self,
        chain: ReasoningChain,
        signal: Dict[str, Any],
    ) -> SignalEvaluation:
        """Synthesize evaluation from reasoning chain"""
        ticker = signal.get("ticker", "UNKNOWN")
        direction = signal.get("direction", "long")
        signal_type = signal.get("signal_type", "unknown")

        # Extract insights from each step
        step_conclusions = {
            step.title: step.conclusion
            for step in chain.steps
            if step.status == StepStatus.COMPLETED
        }

        # Determine statistical quality
        p_value = signal.get("p_value", 1.0)
        if p_value < 0.005:
            statistical_quality = "excellent"
        elif p_value < 0.01:
            statistical_quality = "good"
        elif p_value < 0.02:
            statistical_quality = "marginal"
        else:
            statistical_quality = "poor"

        # Determine regime fit
        vix = signal.get("vix_level", 20)
        if signal_type == "mean_reversion" and vix < 18:
            regime_fit = "strong"
        elif signal_type == "momentum" and vix > 15:
            regime_fit = "moderate"
        else:
            regime_fit = "weak"

        # Calculate risk/reward
        expected_return = signal.get("expected_return_bps", 50)
        expected_risk = signal.get("volatility", 100) * 2  # 2-sigma risk
        risk_reward = expected_return / expected_risk if expected_risk > 0 else 0

        # Calculate overall score (0-100)
        score = 0
        score += 30 if statistical_quality in ["excellent", "good"] else 10
        score += 25 if regime_fit in ["strong", "moderate"] else 5
        score += 25 if risk_reward > 0.5 else (15 if risk_reward > 0.3 else 5)
        score += 20 * (chain.final_confidence or 0.5)

        # Determine recommendation
        if score >= 70:
            recommendation = "trade"
        elif score >= 50:
            recommendation = "investigate"
        else:
            recommendation = "skip"

        # Build reasoning summary
        reasoning_summary = f"""
Signal Evaluation for {ticker} {direction}:
- Statistical Quality: {statistical_quality} (p={p_value:.4f})
- Regime Fit: {regime_fit}
- Risk/Reward: {risk_reward:.2f}
- Overall Score: {score:.0f}/100
- Recommendation: {recommendation.upper()}

Chain Reasoning:
{chain.get_reasoning_summary()}
""".strip()

        return SignalEvaluation(
            signal_id=signal.get("signal_id", "unknown"),
            ticker=ticker,
            direction=direction,
            signal_type=signal_type,
            statistical_quality=statistical_quality,
            p_value_assessment=f"p={p_value:.4f}, {'PASS' if p_value < 0.01 else 'FAIL'}",
            sample_size_assessment=f"n={signal.get('sample_size', 0)}",
            regime_fit=regime_fit,
            sector_outlook="neutral",  # Would be populated from market context
            expected_return_bps=expected_return,
            expected_risk_bps=expected_risk,
            risk_reward_ratio=risk_reward,
            overall_score=score,
            recommendation=recommendation,
            confidence=chain.final_confidence or 0.5,
            reasoning_summary=reasoning_summary,
        )

    async def determine_position_size(
        self,
        signal: Dict[str, Any],
        portfolio: Dict[str, Any],
        market_context: Dict[str, Any],
    ) -> SizingDecision:
        """
        Perform structured reasoning for position sizing.

        Uses Kelly criterion with adjustments for:
        - Liquidity constraints
        - Portfolio correlation
        - Risk limits
        """
        ticker = signal.get("ticker", "UNKNOWN")
        direction = signal.get("direction", "long")

        # Create reasoning chain
        chain = self.engine.create_chain(
            reasoning_type=ReasoningType.POSITION_SIZING,
            subject=f"{ticker} {direction} position size",
            context={
                "signal": signal,
                "portfolio": portfolio,
                "market": market_context,
            },
        )

        # Execute chain
        await self.engine.execute_chain(chain)

        # Calculate sizing components
        kelly_size = self._calculate_kelly_size(signal)
        liquidity_adjusted = self._adjust_for_liquidity(
            kelly_size, signal, market_context
        )
        correlation_adjusted = self._adjust_for_correlation(
            liquidity_adjusted, signal, portfolio
        )
        risk_limit_adjusted = self._apply_risk_limits(
            correlation_adjusted, signal, portfolio
        )

        # Calculate final shares
        price = signal.get("current_price", 100)
        portfolio_value = portfolio.get("nav", 1000000)
        notional = portfolio_value * (risk_limit_adjusted / 100)
        shares = int(notional / price)

        reasoning_summary = f"""
Position Sizing for {ticker} {direction}:
1. Kelly Size: {kelly_size:.2f}% (edge={signal.get('expected_return_bps', 0)/100:.2f}%, odds based)
2. Liquidity Adjusted: {liquidity_adjusted:.2f}% (ADV constraint)
3. Correlation Adjusted: {correlation_adjusted:.2f}% (portfolio correlation)
4. Risk Limit Adjusted: {risk_limit_adjusted:.2f}% (hard limits)

Final: {shares:,} shares (${notional:,.0f})
""".strip()

        return SizingDecision(
            ticker=ticker,
            direction=direction,
            kelly_size_pct=kelly_size,
            liquidity_adjusted_pct=liquidity_adjusted,
            correlation_adjusted_pct=correlation_adjusted,
            risk_limit_adjusted_pct=risk_limit_adjusted,
            final_size_pct=risk_limit_adjusted,
            final_shares=shares,
            final_notional=notional,
            confidence=chain.final_confidence or 0.7,
            reasoning_summary=reasoning_summary,
        )

    def _calculate_kelly_size(self, signal: Dict[str, Any]) -> float:
        """Calculate Kelly criterion optimal size"""
        # Kelly = (bp - q) / b
        # where b = win/loss ratio, p = win probability, q = 1-p

        win_rate = signal.get("confidence", 0.5)
        avg_win = signal.get("avg_win_bps", 50)
        avg_loss = abs(signal.get("avg_loss_bps", 50))

        if avg_loss == 0:
            return 0

        b = avg_win / avg_loss  # Win/loss ratio
        p = win_rate
        q = 1 - p

        kelly = (b * p - q) / b

        # Apply Kelly fraction (use 25% of full Kelly for safety)
        kelly_fraction = self.config.risk_limits.min_signal_confidence - 0.25
        fractional_kelly = kelly * 0.25

        # Cap at max position size
        max_size = self.config.risk_limits.max_position_size_pct
        return min(max(fractional_kelly * 100, 0), max_size)

    def _adjust_for_liquidity(
        self,
        size_pct: float,
        signal: Dict[str, Any],
        market_context: Dict[str, Any],
    ) -> float:
        """Adjust size for liquidity constraints"""
        adv = signal.get("average_daily_volume", 1000000)
        price = signal.get("current_price", 100)
        portfolio_nav = 1000000  # Placeholder

        # Max position = 1% of ADV
        max_adv_pct = self.config.risk_limits.max_order_size_adv_pct
        max_notional_from_adv = adv * price * max_adv_pct

        current_notional = portfolio_nav * (size_pct / 100)

        if current_notional > max_notional_from_adv:
            return (max_notional_from_adv / portfolio_nav) * 100

        return size_pct

    def _adjust_for_correlation(
        self,
        size_pct: float,
        signal: Dict[str, Any],
        portfolio: Dict[str, Any],
    ) -> float:
        """Adjust size for correlation with existing positions"""
        # Get correlation with existing book
        existing_positions = portfolio.get("positions", [])
        ticker = signal.get("ticker")

        max_correlation = 0
        for pos in existing_positions:
            # Would calculate actual correlation
            if pos.get("sector") == signal.get("sector"):
                max_correlation = max(max_correlation, 0.6)  # Same sector

        # Correlation penalty
        if max_correlation > 0.7:
            penalty = 1 - (max_correlation ** 2)
            return size_pct * penalty

        return size_pct

    def _apply_risk_limits(
        self,
        size_pct: float,
        signal: Dict[str, Any],
        portfolio: Dict[str, Any],
    ) -> float:
        """Apply hard risk limits"""
        limits = self.config.risk_limits

        # Single name limit
        size_pct = min(size_pct, limits.max_single_name_pct)

        # Position size limit
        size_pct = min(size_pct, limits.max_position_size_pct)

        # Sector exposure check (would reduce if sector maxed)
        # ...

        return size_pct


# Global investment reasoner instance
_investment_reasoner: Optional[InvestmentReasoner] = None


def get_investment_reasoner() -> InvestmentReasoner:
    """Get the global investment reasoner instance"""
    global _investment_reasoner
    if _investment_reasoner is None:
        _investment_reasoner = InvestmentReasoner()
    return _investment_reasoner
