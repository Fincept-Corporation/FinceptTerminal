"""
Risk Guardrails

Risk management guardrails for position and portfolio limits:
- Position size limits
- Sector exposure limits
- VaR limits
- Drawdown limits
- Leverage limits
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


class PositionSizeGuardrail(Guardrail):
    """
    Enforces maximum position size limits.

    RenTech typically limits individual positions to prevent
    concentration risk and ensure diversification.
    """

    def __init__(self):
        super().__init__(
            name="position_size_limit",
            description="Ensures position sizes don't exceed maximum limits",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check position size against limits"""
        limits = self.config.risk_limits

        position_size_pct = context.get("position_size_pct", 0)
        ticker = context.get("ticker", "UNKNOWN")
        max_single_name = limits.max_single_name_pct
        max_position = limits.max_position_size_pct

        # Check single name limit
        if position_size_pct > max_single_name:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.MODIFY,
                message=f"{ticker} position {position_size_pct:.2f}% exceeds single name limit {max_single_name}%",
                details={
                    "ticker": ticker,
                    "requested_size": position_size_pct,
                    "limit": max_single_name,
                    "limit_type": "single_name",
                },
                modified_value=max_single_name,
                original_value=position_size_pct,
            )

        # Check overall position limit
        if position_size_pct > max_position:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.MODIFY,
                message=f"{ticker} position {position_size_pct:.2f}% exceeds max position limit {max_position}%",
                details={
                    "ticker": ticker,
                    "requested_size": position_size_pct,
                    "limit": max_position,
                    "limit_type": "max_position",
                },
                modified_value=max_position,
                original_value=position_size_pct,
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"{ticker} position {position_size_pct:.2f}% within limits",
            details={
                "ticker": ticker,
                "size": position_size_pct,
                "max_single_name": max_single_name,
                "max_position": max_position,
            },
        )


class SectorExposureGuardrail(Guardrail):
    """
    Enforces sector concentration limits.

    Prevents excessive exposure to any single sector.
    """

    def __init__(self):
        super().__init__(
            name="sector_exposure_limit",
            description="Ensures sector exposure doesn't exceed limits",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check sector exposure against limits"""
        limits = self.config.risk_limits
        max_sector = limits.max_sector_exposure_pct

        sector = context.get("sector", "Unknown")
        current_sector_exposure = context.get("current_sector_exposure_pct", 0)
        proposed_addition = context.get("proposed_addition_pct", 0)
        new_exposure = current_sector_exposure + proposed_addition

        if new_exposure > max_sector:
            # Calculate how much can be added
            allowable_addition = max(0, max_sector - current_sector_exposure)

            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.MODIFY,
                message=f"Sector {sector} exposure would be {new_exposure:.1f}%, exceeds limit {max_sector}%",
                details={
                    "sector": sector,
                    "current_exposure": current_sector_exposure,
                    "proposed_addition": proposed_addition,
                    "new_exposure": new_exposure,
                    "limit": max_sector,
                    "allowable_addition": allowable_addition,
                },
                modified_value=allowable_addition,
                original_value=proposed_addition,
            )

        # Warning if approaching limit
        if new_exposure > max_sector * 0.8:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"Sector {sector} exposure at {new_exposure:.1f}%, approaching limit {max_sector}%",
                details={
                    "sector": sector,
                    "new_exposure": new_exposure,
                    "limit": max_sector,
                    "utilization": new_exposure / max_sector * 100,
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"Sector {sector} exposure {new_exposure:.1f}% within limits",
        )


class VaRLimitGuardrail(Guardrail):
    """
    Enforces Value at Risk limits.

    Ensures portfolio VaR stays within acceptable bounds.
    """

    def __init__(self):
        super().__init__(
            name="var_limit",
            description="Ensures VaR doesn't exceed daily limit",
            severity=GuardrailSeverity.CRITICAL,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check VaR against limits"""
        limits = self.config.risk_limits
        max_var = limits.max_daily_var_pct

        current_var = context.get("current_var_pct", 0)
        incremental_var = context.get("incremental_var_pct", 0)
        post_trade_var = context.get("post_trade_var_pct", current_var + incremental_var)

        if post_trade_var > max_var:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.CRITICAL,
                action=GuardrailAction.BLOCK,
                message=f"Post-trade VaR {post_trade_var:.2f}% would exceed limit {max_var}%",
                details={
                    "current_var": current_var,
                    "incremental_var": incremental_var,
                    "post_trade_var": post_trade_var,
                    "limit": max_var,
                    "excess": post_trade_var - max_var,
                },
            )

        # Warning if high utilization
        utilization = post_trade_var / max_var * 100
        if utilization > 80:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"VaR utilization at {utilization:.0f}%, approaching limit",
                details={
                    "post_trade_var": post_trade_var,
                    "limit": max_var,
                    "utilization": utilization,
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"VaR {post_trade_var:.2f}% within limit {max_var}%",
        )


class DrawdownGuardrail(Guardrail):
    """
    Enforces maximum drawdown limits.

    Triggers risk reduction when drawdown exceeds thresholds.
    """

    def __init__(self):
        super().__init__(
            name="drawdown_limit",
            description="Monitors drawdown and triggers risk reduction",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check current drawdown against limits"""
        limits = self.config.risk_limits
        max_drawdown = limits.max_drawdown_pct

        current_drawdown = context.get("current_drawdown_pct", 0)
        peak_nav = context.get("peak_nav", 0)
        current_nav = context.get("current_nav", 0)

        # Hard limit breach
        if current_drawdown > max_drawdown:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.CRITICAL,
                action=GuardrailAction.BLOCK,
                message=f"Drawdown {current_drawdown:.1f}% exceeds limit {max_drawdown}%. Risk reduction required.",
                details={
                    "current_drawdown": current_drawdown,
                    "limit": max_drawdown,
                    "peak_nav": peak_nav,
                    "current_nav": current_nav,
                    "action_required": "reduce_risk",
                },
            )

        # Warning level (50% of limit)
        if current_drawdown > max_drawdown * 0.5:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"Drawdown {current_drawdown:.1f}% at {current_drawdown/max_drawdown*100:.0f}% of limit. Increased caution.",
                details={
                    "current_drawdown": current_drawdown,
                    "limit": max_drawdown,
                    "recommendation": "reduce_position_sizes",
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"Drawdown {current_drawdown:.1f}% within acceptable range",
        )


class LeverageGuardrail(Guardrail):
    """
    Enforces leverage limits.

    RenTech uses leverage but within controlled bounds.
    """

    def __init__(self):
        super().__init__(
            name="leverage_limit",
            description="Ensures leverage stays within limits",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check leverage against limits"""
        limits = self.config.risk_limits
        max_leverage = limits.max_leverage

        gross_exposure = context.get("gross_exposure_pct", 100)
        current_leverage = gross_exposure / 100  # Convert to multiplier

        if current_leverage > max_leverage:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.BLOCK,
                message=f"Leverage {current_leverage:.1f}x exceeds limit {max_leverage}x",
                details={
                    "current_leverage": current_leverage,
                    "gross_exposure_pct": gross_exposure,
                    "limit": max_leverage,
                    "excess": current_leverage - max_leverage,
                },
            )

        # Warning if high utilization
        utilization = current_leverage / max_leverage * 100
        if utilization > 80:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"Leverage {current_leverage:.1f}x at {utilization:.0f}% of limit",
                details={
                    "current_leverage": current_leverage,
                    "limit": max_leverage,
                    "utilization": utilization,
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"Leverage {current_leverage:.1f}x within limit {max_leverage}x",
        )


class DailyTradeCountGuardrail(Guardrail):
    """
    Monitors daily trade count.

    RenTech does high-frequency trading, but monitors for anomalies.
    """

    def __init__(self):
        super().__init__(
            name="daily_trade_count",
            description="Monitors daily trade volume",
            severity=GuardrailSeverity.WARNING,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check daily trade count"""
        limits = self.config.risk_limits
        max_trades = limits.max_daily_trades

        current_trades = context.get("daily_trade_count", 0)

        if current_trades > max_trades:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"Daily trades {current_trades:,} exceeds typical volume {max_trades:,}",
                details={
                    "current_trades": current_trades,
                    "typical_max": max_trades,
                    "excess_pct": (current_trades - max_trades) / max_trades * 100,
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"Daily trade count {current_trades:,} within normal range",
        )
