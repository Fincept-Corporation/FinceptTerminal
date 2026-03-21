"""
Execution Guardrails

Trade execution guardrails:
- Liquidity checks
- Market impact limits
- Execution timing
- Price deviation limits
"""

from typing import Dict, Any, Optional
from datetime import datetime

from .base import (
    Guardrail,
    GuardrailResult,
    GuardrailSeverity,
    GuardrailAction,
)
from ..config import get_config


class LiquidityGuardrail(Guardrail):
    """
    Enforces liquidity requirements for trades.

    Ensures order sizes don't exceed a percentage of
    average daily volume to minimize market impact.
    """

    def __init__(self):
        super().__init__(
            name="liquidity_check",
            description="Ensures trade size respects liquidity constraints",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check trade size against liquidity"""
        limits = self.config.risk_limits
        max_adv_pct = limits.max_order_size_adv_pct

        ticker = context.get("ticker", "UNKNOWN")
        order_shares = context.get("order_shares", 0)
        adv = context.get("average_daily_volume", 1)  # Avoid division by zero

        if adv == 0:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.BLOCK,
                message=f"No ADV data for {ticker}",
                details={"ticker": ticker, "adv": 0},
            )

        adv_pct = (order_shares / adv) * 100

        if adv_pct > max_adv_pct * 100:  # Convert to percentage
            # Calculate maximum allowed shares
            max_shares = int(adv * max_adv_pct)

            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.MODIFY,
                message=f"{ticker} order ({order_shares:,} shares) is {adv_pct:.1f}% of ADV, exceeds {max_adv_pct*100}% limit",
                details={
                    "ticker": ticker,
                    "order_shares": order_shares,
                    "adv": adv,
                    "adv_pct": adv_pct,
                    "limit_pct": max_adv_pct * 100,
                    "max_allowed_shares": max_shares,
                },
                modified_value=max_shares,
                original_value=order_shares,
            )

        # Warning if approaching limit
        if adv_pct > max_adv_pct * 50:  # 50% of limit
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"{ticker} order is {adv_pct:.1f}% of ADV, approaching liquidity limit",
                details={
                    "ticker": ticker,
                    "order_shares": order_shares,
                    "adv_pct": adv_pct,
                    "warning": "high_adv_utilization",
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"{ticker} order size ({adv_pct:.2f}% of ADV) within liquidity limits",
            details={
                "ticker": ticker,
                "order_shares": order_shares,
                "adv_pct": adv_pct,
            },
        )


class MarketImpactGuardrail(Guardrail):
    """
    Enforces market impact limits.

    Ensures estimated market impact doesn't exceed thresholds.
    """

    def __init__(self):
        super().__init__(
            name="market_impact_limit",
            description="Ensures estimated market impact is acceptable",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check estimated market impact"""
        limits = self.config.execution
        max_impact = limits.max_market_impact_bps

        ticker = context.get("ticker", "UNKNOWN")
        estimated_impact_bps = context.get("estimated_impact_bps", 0)

        if estimated_impact_bps > max_impact:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.MODIFY,
                message=f"{ticker} estimated impact {estimated_impact_bps:.1f} bps exceeds limit {max_impact} bps",
                details={
                    "ticker": ticker,
                    "estimated_impact_bps": estimated_impact_bps,
                    "limit_bps": max_impact,
                    "recommendation": "reduce_size_or_extend_duration",
                },
            )

        # Warning if high impact
        if estimated_impact_bps > max_impact * 0.7:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"{ticker} estimated impact {estimated_impact_bps:.1f} bps approaching limit",
                details={
                    "ticker": ticker,
                    "estimated_impact_bps": estimated_impact_bps,
                    "limit_bps": max_impact,
                    "recommendation": "consider_passive_execution",
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"{ticker} estimated impact {estimated_impact_bps:.1f} bps within limits",
        )


class ExecutionTimeGuardrail(Guardrail):
    """
    Enforces execution timing constraints.

    Controls when orders can be executed based on:
    - Time of day
    - Volatility
    - News events
    """

    def __init__(self):
        super().__init__(
            name="execution_timing",
            description="Validates execution timing is appropriate",
            severity=GuardrailSeverity.WARNING,
        )

        # Times to avoid (in minutes from market open)
        self.avoid_first_minutes = 15
        self.avoid_last_minutes = 15

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check execution timing"""
        ticker = context.get("ticker", "UNKNOWN")
        minutes_from_open = context.get("minutes_from_open", 60)
        minutes_to_close = context.get("minutes_to_close", 60)
        has_news = context.get("has_recent_news", False)
        volatility_spike = context.get("volatility_spike", False)

        warnings = []

        # Check market open period
        if minutes_from_open < self.avoid_first_minutes:
            warnings.append(f"First {self.avoid_first_minutes} minutes - higher spreads and volatility")

        # Check market close period
        if minutes_to_close < self.avoid_last_minutes:
            warnings.append(f"Last {self.avoid_last_minutes} minutes - higher volatility")

        # Check for news
        if has_news:
            warnings.append("Recent news detected - elevated volatility risk")

        # Check for volatility spike
        if volatility_spike:
            warnings.append("Volatility spike detected - consider delaying")

        if warnings:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,  # Warning only, not blocking
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"{ticker} execution timing warnings: {'; '.join(warnings)}",
                details={
                    "ticker": ticker,
                    "warnings": warnings,
                    "minutes_from_open": minutes_from_open,
                    "minutes_to_close": minutes_to_close,
                    "has_news": has_news,
                    "volatility_spike": volatility_spike,
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"{ticker} execution timing is optimal",
        )


class PriceDeviationGuardrail(Guardrail):
    """
    Monitors price deviation during execution.

    Alerts or blocks if price moves significantly during
    order execution.
    """

    def __init__(self, max_deviation_bps: float = 50):
        super().__init__(
            name="price_deviation",
            description="Monitors price deviation during execution",
            severity=GuardrailSeverity.ERROR,
        )
        self.max_deviation_bps = max_deviation_bps

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check price deviation"""
        ticker = context.get("ticker", "UNKNOWN")
        reference_price = context.get("reference_price", 0)
        current_price = context.get("current_price", 0)
        direction = context.get("direction", "long")

        if reference_price == 0:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message="No reference price, skipping deviation check",
            )

        deviation_bps = ((current_price - reference_price) / reference_price) * 10000

        # For long orders, price increase is adverse
        # For short orders, price decrease is adverse
        if direction == "long":
            adverse_deviation = deviation_bps  # Higher price is bad
        else:
            adverse_deviation = -deviation_bps  # Lower price is bad

        if adverse_deviation > self.max_deviation_bps:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.BLOCK,
                message=f"{ticker} price moved adversely by {adverse_deviation:.0f} bps, exceeds {self.max_deviation_bps} bps limit",
                details={
                    "ticker": ticker,
                    "reference_price": reference_price,
                    "current_price": current_price,
                    "deviation_bps": deviation_bps,
                    "adverse_deviation_bps": adverse_deviation,
                    "limit_bps": self.max_deviation_bps,
                    "direction": direction,
                    "recommendation": "reconsider_or_reprice",
                },
            )

        # Warning if significant deviation
        if abs(deviation_bps) > self.max_deviation_bps * 0.5:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"{ticker} price moved {deviation_bps:+.0f} bps from reference",
                details={
                    "ticker": ticker,
                    "deviation_bps": deviation_bps,
                    "warning": "significant_price_movement",
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"{ticker} price deviation {deviation_bps:+.0f} bps within acceptable range",
        )


class SlippageGuardrail(Guardrail):
    """
    Monitors execution slippage.

    Alerts if slippage exceeds expected levels.
    """

    def __init__(self):
        super().__init__(
            name="slippage_monitor",
            description="Monitors and limits execution slippage",
            severity=GuardrailSeverity.WARNING,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check slippage"""
        limits = self.config.execution
        max_slippage = limits.max_slippage_bps

        ticker = context.get("ticker", "UNKNOWN")
        expected_price = context.get("expected_price", 0)
        fill_price = context.get("fill_price", 0)
        direction = context.get("direction", "long")

        if expected_price == 0 or fill_price == 0:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message="Price data incomplete, skipping slippage check",
            )

        # Calculate slippage (negative = good, positive = bad for longs)
        if direction == "long":
            slippage_bps = ((fill_price - expected_price) / expected_price) * 10000
        else:
            slippage_bps = ((expected_price - fill_price) / expected_price) * 10000

        if slippage_bps > max_slippage:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"{ticker} slippage {slippage_bps:.1f} bps exceeds target {max_slippage} bps",
                details={
                    "ticker": ticker,
                    "expected_price": expected_price,
                    "fill_price": fill_price,
                    "slippage_bps": slippage_bps,
                    "limit_bps": max_slippage,
                    "direction": direction,
                },
            )

        # Good execution
        if slippage_bps < 0:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message=f"{ticker} achieved price improvement of {-slippage_bps:.1f} bps",
                details={
                    "ticker": ticker,
                    "slippage_bps": slippage_bps,
                    "quality": "excellent",
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"{ticker} slippage {slippage_bps:.1f} bps within acceptable range",
        )
