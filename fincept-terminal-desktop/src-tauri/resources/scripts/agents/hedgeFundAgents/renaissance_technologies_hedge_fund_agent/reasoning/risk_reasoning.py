"""
Risk Reasoning

Specialized reasoning for risk assessment decisions:
- VaR analysis
- Stress testing
- Correlation risk
- Tail risk assessment
"""

from typing import Optional, List, Dict, Any
from dataclasses import dataclass
from datetime import datetime
import math

from .base import (
    ReasoningEngine,
    ReasoningChain,
    ReasoningType,
    StepStatus,
    get_reasoning_engine,
)
from ..config import get_config


@dataclass
class RiskAssessmentResult:
    """Result of risk reasoning"""
    assessment_id: str
    subject: str  # What was assessed (trade, portfolio, scenario)

    # VaR Metrics
    current_var_pct: float
    incremental_var_pct: float
    post_trade_var_pct: float
    var_limit_pct: float
    var_utilization_pct: float

    # Exposure Analysis
    gross_exposure_pct: float
    net_exposure_pct: float
    beta_exposure: float
    sector_concentrations: Dict[str, float]

    # Stress Test Results
    stress_scenarios: Dict[str, float]  # Scenario name -> P&L impact %
    worst_case_scenario: str
    worst_case_impact_pct: float

    # Risk Flags
    risk_flags: List[str]
    risk_level: str  # low, medium, high, critical

    # Recommendation
    proceed: bool
    conditions: List[str]
    confidence: float
    reasoning_summary: str


@dataclass
class CorrelationRisk:
    """Correlation risk analysis result"""
    position_ticker: str
    correlated_positions: List[Dict[str, Any]]
    max_correlation: float
    effective_diversification: float
    concentration_risk: str  # low, medium, high


class RiskReasoner:
    """
    Risk-focused reasoning system.

    Provides structured reasoning for:
    1. Portfolio risk assessment
    2. Trade risk evaluation
    3. Stress testing
    4. Correlation analysis
    5. Limit monitoring
    """

    def __init__(self, reasoning_engine: Optional[ReasoningEngine] = None):
        self.engine = reasoning_engine or get_reasoning_engine()
        self.config = get_config()

    async def assess_trade_risk(
        self,
        proposed_trade: Dict[str, Any],
        current_portfolio: Dict[str, Any],
        market_context: Dict[str, Any],
    ) -> RiskAssessmentResult:
        """
        Perform structured risk reasoning for a proposed trade.

        Evaluates:
        1. Incremental VaR impact
        2. Exposure changes
        3. Stress test outcomes
        4. Limit compliance
        """
        ticker = proposed_trade.get("ticker", "UNKNOWN")
        direction = proposed_trade.get("direction", "long")
        size_pct = proposed_trade.get("size_pct", 1.0)

        # Create reasoning chain
        chain = self.engine.create_chain(
            reasoning_type=ReasoningType.RISK_ASSESSMENT,
            subject=f"Trade risk: {ticker} {direction} {size_pct:.1f}%",
            context={
                "trade": proposed_trade,
                "portfolio": current_portfolio,
                "market": market_context,
            },
        )

        # Execute chain
        await self.engine.execute_chain(chain)

        # Perform calculations
        current_var = self._calculate_current_var(current_portfolio)
        incremental_var = self._calculate_incremental_var(
            proposed_trade, current_portfolio
        )
        post_trade_var = current_var + incremental_var

        # Exposure analysis
        exposures = self._calculate_exposures(proposed_trade, current_portfolio)

        # Stress tests
        stress_results = self._run_stress_tests(
            proposed_trade, current_portfolio, market_context
        )

        # Check limits and generate flags
        risk_flags, risk_level = self._check_limits(
            post_trade_var, exposures, stress_results
        )

        # Determine if we should proceed
        proceed, conditions = self._determine_proceed(
            risk_flags, risk_level, chain.final_confidence or 0.5
        )

        reasoning_summary = f"""
Risk Assessment for {ticker} {direction} ({size_pct:.1f}%):

VaR Analysis:
- Current VaR: {current_var:.2f}%
- Incremental VaR: {incremental_var:.2f}%
- Post-Trade VaR: {post_trade_var:.2f}%
- Limit: {self.config.risk_limits.max_daily_var_pct:.2f}%
- Utilization: {(post_trade_var / self.config.risk_limits.max_daily_var_pct * 100):.1f}%

Exposures:
- Gross: {exposures['gross']:.1f}%
- Net: {exposures['net']:.1f}%
- Beta: {exposures['beta']:.2f}

Stress Tests:
{chr(10).join([f"- {k}: {v:+.1f}%" for k, v in stress_results.items()])}

Risk Flags: {', '.join(risk_flags) if risk_flags else 'None'}
Risk Level: {risk_level.upper()}

Recommendation: {'PROCEED' if proceed else 'HALT'}
{('Conditions: ' + ', '.join(conditions)) if conditions else ''}
""".strip()

        return RiskAssessmentResult(
            assessment_id=f"risk_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}",
            subject=f"{ticker} {direction} trade",
            current_var_pct=current_var,
            incremental_var_pct=incremental_var,
            post_trade_var_pct=post_trade_var,
            var_limit_pct=self.config.risk_limits.max_daily_var_pct,
            var_utilization_pct=post_trade_var / self.config.risk_limits.max_daily_var_pct * 100,
            gross_exposure_pct=exposures["gross"],
            net_exposure_pct=exposures["net"],
            beta_exposure=exposures["beta"],
            sector_concentrations=exposures.get("sectors", {}),
            stress_scenarios=stress_results,
            worst_case_scenario=min(stress_results, key=stress_results.get),
            worst_case_impact_pct=min(stress_results.values()),
            risk_flags=risk_flags,
            risk_level=risk_level,
            proceed=proceed,
            conditions=conditions,
            confidence=chain.final_confidence or 0.7,
            reasoning_summary=reasoning_summary,
        )

    def _calculate_current_var(self, portfolio: Dict[str, Any]) -> float:
        """Calculate current portfolio VaR"""
        # Simplified VaR calculation
        # In production, would use historical simulation or Monte Carlo
        positions = portfolio.get("positions", [])

        if not positions:
            return 0.0

        total_var = 0
        for pos in positions:
            # Individual position VaR (assuming normal distribution)
            weight = pos.get("weight_pct", 0) / 100
            volatility = pos.get("volatility", 20) / 100
            # 95% VaR = 1.645 * sigma
            individual_var = 1.645 * volatility * weight
            total_var += individual_var ** 2

        # Portfolio VaR (assuming some correlation)
        # This is simplified - real calc would use correlation matrix
        return math.sqrt(total_var) * 100 * 0.8  # Diversification benefit

    def _calculate_incremental_var(
        self,
        trade: Dict[str, Any],
        portfolio: Dict[str, Any],
    ) -> float:
        """Calculate incremental VaR from proposed trade"""
        size_pct = trade.get("size_pct", 1.0) / 100
        volatility = trade.get("volatility", 25) / 100

        # Marginal VaR
        marginal_var = 1.645 * volatility * size_pct

        # Adjust for correlation with existing portfolio
        portfolio_positions = portfolio.get("positions", [])
        if portfolio_positions:
            # Check if correlated positions exist
            same_sector = [
                p for p in portfolio_positions
                if p.get("sector") == trade.get("sector")
            ]
            if same_sector:
                # Higher correlation means more incremental risk
                marginal_var *= 1.2

        return marginal_var * 100

    def _calculate_exposures(
        self,
        trade: Dict[str, Any],
        portfolio: Dict[str, Any],
    ) -> Dict[str, Any]:
        """Calculate portfolio exposures"""
        positions = portfolio.get("positions", [])

        # Current exposures
        long_exposure = sum(
            p.get("weight_pct", 0) for p in positions
            if p.get("direction") == "long"
        )
        short_exposure = abs(sum(
            p.get("weight_pct", 0) for p in positions
            if p.get("direction") == "short"
        ))

        # Add proposed trade
        trade_size = trade.get("size_pct", 1.0)
        if trade.get("direction") == "long":
            long_exposure += trade_size
        else:
            short_exposure += trade_size

        gross = long_exposure + short_exposure
        net = long_exposure - short_exposure

        # Calculate beta exposure
        beta_sum = sum(
            p.get("weight_pct", 0) * p.get("beta", 1.0) / 100
            for p in positions
        )
        trade_beta = trade.get("beta", 1.0)
        if trade.get("direction") == "long":
            beta_sum += trade_size / 100 * trade_beta
        else:
            beta_sum -= trade_size / 100 * trade_beta

        # Sector concentrations
        sectors = {}
        for p in positions:
            sector = p.get("sector", "Unknown")
            sectors[sector] = sectors.get(sector, 0) + p.get("weight_pct", 0)

        trade_sector = trade.get("sector", "Unknown")
        if trade.get("direction") == "long":
            sectors[trade_sector] = sectors.get(trade_sector, 0) + trade_size
        else:
            sectors[trade_sector] = sectors.get(trade_sector, 0) - trade_size

        return {
            "gross": gross,
            "net": net,
            "beta": beta_sum,
            "long": long_exposure,
            "short": short_exposure,
            "sectors": sectors,
        }

    def _run_stress_tests(
        self,
        trade: Dict[str, Any],
        portfolio: Dict[str, Any],
        market_context: Dict[str, Any],
    ) -> Dict[str, float]:
        """Run stress test scenarios"""
        scenarios = {
            "Market -5%": -5.0,
            "Market -10%": -10.0,
            "Market +5%": 5.0,
            "VIX Spike +50%": -3.0,
            "Sector -15%": -15.0,
            "Flash Crash -20%": -20.0,
        }

        results = {}
        beta = trade.get("beta", 1.0)
        size_pct = trade.get("size_pct", 1.0) / 100
        direction_mult = 1 if trade.get("direction") == "long" else -1

        for scenario, market_move in scenarios.items():
            if "Market" in scenario:
                # Market scenarios
                trade_impact = market_move * beta * size_pct * direction_mult
            elif "VIX" in scenario:
                # VIX spike hurts most positions
                trade_impact = -2 * size_pct
            elif "Sector" in scenario:
                # Sector-specific
                trade_impact = market_move * size_pct * direction_mult
            else:
                # Tail events
                trade_impact = market_move * 0.5 * size_pct * direction_mult

            results[scenario] = trade_impact

        return results

    def _check_limits(
        self,
        var_pct: float,
        exposures: Dict[str, Any],
        stress_results: Dict[str, float],
    ) -> tuple:
        """Check risk limits and generate flags"""
        flags = []
        limits = self.config.risk_limits

        # VaR limit
        if var_pct > limits.max_daily_var_pct:
            flags.append(f"VaR BREACH: {var_pct:.2f}% > {limits.max_daily_var_pct}%")

        # Gross exposure (leverage)
        max_gross = limits.max_leverage * 100
        if exposures["gross"] > max_gross:
            flags.append(f"LEVERAGE BREACH: {exposures['gross']:.1f}% > {max_gross}%")

        # Sector concentration
        for sector, weight in exposures.get("sectors", {}).items():
            if weight > limits.max_sector_exposure_pct:
                flags.append(f"SECTOR BREACH ({sector}): {weight:.1f}% > {limits.max_sector_exposure_pct}%")

        # Stress test
        worst_stress = min(stress_results.values())
        max_drawdown = -limits.max_drawdown_pct
        if worst_stress < max_drawdown:
            flags.append(f"STRESS TEST FAIL: {worst_stress:.1f}% < {max_drawdown}%")

        # Determine risk level
        if any("BREACH" in f for f in flags):
            risk_level = "critical"
        elif len(flags) >= 2:
            risk_level = "high"
        elif len(flags) == 1:
            risk_level = "medium"
        else:
            risk_level = "low"

        return flags, risk_level

    def _determine_proceed(
        self,
        flags: List[str],
        risk_level: str,
        confidence: float,
    ) -> tuple:
        """Determine if we should proceed with the trade"""
        if risk_level == "critical":
            return False, ["BLOCKED: Risk limit breach"]

        if risk_level == "high":
            if confidence > 0.8:
                return True, ["Reduce size by 50%", "Implement tight stop-loss"]
            else:
                return False, ["Risk too high for confidence level"]

        if risk_level == "medium":
            return True, ["Monitor closely", "Review at EOD"]

        return True, []

    async def analyze_correlation_risk(
        self,
        position: Dict[str, Any],
        portfolio: Dict[str, Any],
    ) -> CorrelationRisk:
        """Analyze correlation risk of a position with existing portfolio"""
        ticker = position.get("ticker", "UNKNOWN")
        positions = portfolio.get("positions", [])

        correlated = []
        max_correlation = 0

        for pos in positions:
            if pos.get("ticker") == ticker:
                continue

            # Estimate correlation (simplified)
            correlation = 0
            if pos.get("sector") == position.get("sector"):
                correlation = 0.6
            if abs(pos.get("beta", 1) - position.get("beta", 1)) < 0.2:
                correlation = max(correlation, 0.4)

            if correlation > 0.3:
                correlated.append({
                    "ticker": pos.get("ticker"),
                    "correlation": correlation,
                    "weight_pct": pos.get("weight_pct", 0),
                })
                max_correlation = max(max_correlation, correlation)

        # Calculate effective diversification
        if max_correlation > 0.7:
            effective_div = 0.3
            concentration = "high"
        elif max_correlation > 0.5:
            effective_div = 0.6
            concentration = "medium"
        else:
            effective_div = 0.9
            concentration = "low"

        return CorrelationRisk(
            position_ticker=ticker,
            correlated_positions=correlated,
            max_correlation=max_correlation,
            effective_diversification=effective_div,
            concentration_risk=concentration,
        )


# Global risk reasoner instance
_risk_reasoner: Optional[RiskReasoner] = None


def get_risk_reasoner() -> RiskReasoner:
    """Get the global risk reasoner instance"""
    global _risk_reasoner
    if _risk_reasoner is None:
        _risk_reasoner = RiskReasoner()
    return _risk_reasoner
