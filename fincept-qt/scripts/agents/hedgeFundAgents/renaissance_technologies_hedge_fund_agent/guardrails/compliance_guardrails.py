"""
Compliance Guardrails

Regulatory and compliance guardrails:
- Restricted securities lists
- Insider trading prevention
- Trading time windows
- Approval requirements
"""

from typing import Dict, Any, List, Optional
from datetime import datetime, time
import pytz

from .base import (
    Guardrail,
    GuardrailResult,
    GuardrailSeverity,
    GuardrailAction,
)
from ..config import get_config


class RestrictedListGuardrail(Guardrail):
    """
    Checks against restricted securities list.

    Prevents trading in securities that are restricted due to:
    - Material non-public information
    - Client relationships
    - Regulatory restrictions
    - Corporate actions
    """

    def __init__(self):
        super().__init__(
            name="restricted_list",
            description="Checks if security is on restricted list",
            severity=GuardrailSeverity.CRITICAL,
        )

        # Simulated restricted list
        # In production, this would be fetched from compliance database
        self._restricted_securities: Dict[str, str] = {
            # "TICKER": "reason"
        }

        # Temporarily restricted (e.g., during earnings)
        self._temporarily_restricted: Dict[str, dict] = {
            # "TICKER": {"reason": "...", "until": datetime}
        }

    def add_to_restricted_list(self, ticker: str, reason: str):
        """Add a security to the restricted list"""
        self._restricted_securities[ticker.upper()] = reason

    def add_temporary_restriction(
        self,
        ticker: str,
        reason: str,
        until: datetime,
    ):
        """Add a temporary restriction"""
        self._temporarily_restricted[ticker.upper()] = {
            "reason": reason,
            "until": until,
        }

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check if security is restricted"""
        ticker = context.get("ticker", "").upper()

        # Check permanent restricted list
        if ticker in self._restricted_securities:
            reason = self._restricted_securities[ticker]
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.CRITICAL,
                action=GuardrailAction.BLOCK,
                message=f"{ticker} is on restricted list: {reason}",
                details={
                    "ticker": ticker,
                    "reason": reason,
                    "restriction_type": "permanent",
                },
            )

        # Check temporary restrictions
        if ticker in self._temporarily_restricted:
            restriction = self._temporarily_restricted[ticker]
            until = restriction["until"]

            if datetime.utcnow() < until:
                return GuardrailResult(
                    guardrail_name=self.name,
                    passed=False,
                    severity=GuardrailSeverity.CRITICAL,
                    action=GuardrailAction.BLOCK,
                    message=f"{ticker} temporarily restricted: {restriction['reason']}",
                    details={
                        "ticker": ticker,
                        "reason": restriction["reason"],
                        "restriction_type": "temporary",
                        "restricted_until": until.isoformat(),
                    },
                )
            else:
                # Restriction expired, remove it
                del self._temporarily_restricted[ticker]

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"{ticker} not on restricted list",
        )


class InsiderTradingGuardrail(Guardrail):
    """
    Prevents potential insider trading.

    Checks for:
    - Recent corporate communications
    - Upcoming earnings
    - Pending M&A
    - Material events
    """

    def __init__(self):
        super().__init__(
            name="insider_trading_prevention",
            description="Prevents trading on potential MNPI",
            severity=GuardrailSeverity.CRITICAL,
        )

        # Blackout periods
        self._blackout_periods: Dict[str, dict] = {}

    def add_blackout_period(
        self,
        ticker: str,
        reason: str,
        start: datetime,
        end: datetime,
    ):
        """Add a blackout period for a security"""
        self._blackout_periods[ticker.upper()] = {
            "reason": reason,
            "start": start,
            "end": end,
        }

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check for insider trading concerns"""
        ticker = context.get("ticker", "").upper()
        has_corporate_contact = context.get("has_corporate_contact", False)
        days_to_earnings = context.get("days_to_earnings")
        pending_ma = context.get("pending_ma", False)

        # Check blackout periods
        if ticker in self._blackout_periods:
            period = self._blackout_periods[ticker]
            now = datetime.utcnow()

            if period["start"] <= now <= period["end"]:
                return GuardrailResult(
                    guardrail_name=self.name,
                    passed=False,
                    severity=GuardrailSeverity.CRITICAL,
                    action=GuardrailAction.BLOCK,
                    message=f"{ticker} in blackout period: {period['reason']}",
                    details={
                        "ticker": ticker,
                        "reason": period["reason"],
                        "blackout_end": period["end"].isoformat(),
                    },
                )

        # Check for recent corporate contact (potential MNPI)
        if has_corporate_contact:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.CRITICAL,
                action=GuardrailAction.ESCALATE,
                message=f"Recent corporate contact for {ticker}. Compliance review required.",
                details={
                    "ticker": ticker,
                    "concern": "potential_mnpi",
                    "action": "compliance_review",
                },
            )

        # Check earnings proximity
        if days_to_earnings is not None and 0 <= days_to_earnings <= 3:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"{ticker} earnings in {days_to_earnings} days. Extra caution required.",
                details={
                    "ticker": ticker,
                    "days_to_earnings": days_to_earnings,
                },
            )

        # Check pending M&A
        if pending_ma:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.CRITICAL,
                action=GuardrailAction.BLOCK,
                message=f"{ticker} has pending M&A. Trading blocked.",
                details={
                    "ticker": ticker,
                    "concern": "pending_ma",
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"No insider trading concerns for {ticker}",
        )


class TradeTimeGuardrail(Guardrail):
    """
    Enforces trading time windows.

    Controls:
    - Market hours trading
    - Pre/post market restrictions
    - Holiday schedules
    """

    def __init__(self):
        super().__init__(
            name="trading_time_window",
            description="Ensures trading during appropriate times",
            severity=GuardrailSeverity.ERROR,
        )

        self.market_open = time(9, 30)  # 9:30 AM ET
        self.market_close = time(16, 0)  # 4:00 PM ET
        self.timezone = pytz.timezone("America/New_York")

        # Allow extended hours trading?
        self.allow_premarket = True
        self.allow_afterhours = True
        self.premarket_start = time(4, 0)
        self.afterhours_end = time(20, 0)

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check if current time is within trading window"""
        # Get current time in ET
        now_utc = datetime.utcnow()
        try:
            now_et = pytz.utc.localize(now_utc).astimezone(self.timezone)
        except:
            now_et = now_utc  # Fallback if pytz not available
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message="Time check skipped (timezone unavailable)",
            )

        current_time = now_et.time()
        is_weekend = now_et.weekday() >= 5

        # Check weekend
        if is_weekend:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.BLOCK,
                message="Markets closed (weekend)",
                details={
                    "current_day": now_et.strftime("%A"),
                    "reason": "weekend",
                },
            )

        # Check regular market hours
        if self.market_open <= current_time <= self.market_close:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message="Within regular market hours",
                details={
                    "current_time": current_time.strftime("%H:%M"),
                    "session": "regular",
                },
            )

        # Check premarket
        if self.allow_premarket and self.premarket_start <= current_time < self.market_open:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message="Pre-market session (limited liquidity)",
                details={
                    "current_time": current_time.strftime("%H:%M"),
                    "session": "premarket",
                    "warning": "reduced_liquidity",
                },
            )

        # Check after-hours
        if self.allow_afterhours and self.market_close < current_time <= self.afterhours_end:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message="After-hours session (limited liquidity)",
                details={
                    "current_time": current_time.strftime("%H:%M"),
                    "session": "afterhours",
                    "warning": "reduced_liquidity",
                },
            )

        # Outside all trading hours
        return GuardrailResult(
            guardrail_name=self.name,
            passed=False,
            severity=GuardrailSeverity.ERROR,
            action=GuardrailAction.BLOCK,
            message="Outside trading hours",
            details={
                "current_time": current_time.strftime("%H:%M"),
                "market_open": self.market_open.strftime("%H:%M"),
                "market_close": self.market_close.strftime("%H:%M"),
            },
        )


class ComplianceApprovalGuardrail(Guardrail):
    """
    Checks for required compliance approvals.

    Certain trades require pre-approval:
    - Large block trades
    - Concentrated positions
    - Related-party transactions
    """

    def __init__(self):
        super().__init__(
            name="compliance_approval",
            description="Checks for required compliance approvals",
            severity=GuardrailSeverity.ERROR,
        )

        # Thresholds requiring approval
        self.approval_threshold_pct = 3.0  # Positions > 3% need approval
        self.large_trade_threshold = 10000000  # $10M+ trades need approval

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check if compliance approval is required and obtained"""
        position_size_pct = context.get("position_size_pct", 0)
        trade_notional = context.get("trade_notional", 0)
        has_approval = context.get("compliance_approval", False)
        ticker = context.get("ticker", "UNKNOWN")

        needs_approval = False
        reasons = []

        # Check position size threshold
        if position_size_pct > self.approval_threshold_pct:
            needs_approval = True
            reasons.append(f"Position size {position_size_pct:.1f}% > {self.approval_threshold_pct}%")

        # Check trade size threshold
        if trade_notional > self.large_trade_threshold:
            needs_approval = True
            reasons.append(f"Trade notional ${trade_notional:,.0f} > ${self.large_trade_threshold:,.0f}")

        if needs_approval and not has_approval:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.ESCALATE,
                message=f"Compliance approval required for {ticker}: {'; '.join(reasons)}",
                details={
                    "ticker": ticker,
                    "reasons": reasons,
                    "position_size_pct": position_size_pct,
                    "trade_notional": trade_notional,
                    "approval_required": True,
                },
            )

        if needs_approval and has_approval:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message=f"Compliance approval obtained for {ticker}",
                details={
                    "ticker": ticker,
                    "approval_required": True,
                    "approval_obtained": True,
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"No additional approval required for {ticker}",
        )
