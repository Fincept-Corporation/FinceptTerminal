"""
Signal Guardrails

Quality guardrails for trading signals:
- Statistical significance
- Sample size requirements
- Confidence thresholds
- Data quality checks
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


class PValueGuardrail(Guardrail):
    """
    Enforces statistical significance requirements.

    RenTech famously requires p-values below 0.01 for signals.
    "We only trade when we're right 50.75% of the time, but
    we know we're right to a high degree of certainty."
    """

    def __init__(self):
        super().__init__(
            name="p_value_threshold",
            description="Ensures signals meet statistical significance requirements",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check p-value against threshold"""
        limits = self.config.risk_limits
        required_p_value = limits.min_p_value

        signal_p_value = context.get("p_value", 1.0)
        signal_id = context.get("signal_id", "unknown")
        ticker = context.get("ticker", "UNKNOWN")

        if signal_p_value is None:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.BLOCK,
                message=f"Signal {signal_id} missing p-value",
                details={
                    "signal_id": signal_id,
                    "ticker": ticker,
                    "p_value": None,
                },
            )

        if signal_p_value > required_p_value:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.BLOCK,
                message=f"Signal {signal_id} p-value {signal_p_value:.4f} > threshold {required_p_value}",
                details={
                    "signal_id": signal_id,
                    "ticker": ticker,
                    "p_value": signal_p_value,
                    "threshold": required_p_value,
                    "excess": signal_p_value - required_p_value,
                },
            )

        # Excellent p-value
        if signal_p_value < 0.005:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message=f"Signal {signal_id} has excellent statistical significance (p={signal_p_value:.4f})",
                details={
                    "signal_id": signal_id,
                    "ticker": ticker,
                    "p_value": signal_p_value,
                    "quality": "excellent",
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"Signal {signal_id} meets p-value threshold (p={signal_p_value:.4f})",
            details={
                "signal_id": signal_id,
                "ticker": ticker,
                "p_value": signal_p_value,
            },
        )


class SampleSizeGuardrail(Guardrail):
    """
    Enforces minimum sample size requirements.

    Ensures signals have sufficient historical data
    to be statistically meaningful.
    """

    def __init__(self, min_sample_size: int = 100):
        super().__init__(
            name="sample_size_minimum",
            description="Ensures signals have sufficient sample size",
            severity=GuardrailSeverity.ERROR,
        )
        self.min_sample_size = min_sample_size

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check sample size"""
        sample_size = context.get("sample_size", 0)
        signal_id = context.get("signal_id", "unknown")
        ticker = context.get("ticker", "UNKNOWN")

        if sample_size < self.min_sample_size:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.BLOCK,
                message=f"Signal {signal_id} sample size {sample_size} < minimum {self.min_sample_size}",
                details={
                    "signal_id": signal_id,
                    "ticker": ticker,
                    "sample_size": sample_size,
                    "minimum": self.min_sample_size,
                    "shortfall": self.min_sample_size - sample_size,
                },
            )

        # Large sample size is better
        if sample_size >= self.min_sample_size * 5:
            quality = "excellent"
        elif sample_size >= self.min_sample_size * 2:
            quality = "good"
        else:
            quality = "adequate"

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"Signal {signal_id} has {quality} sample size ({sample_size})",
            details={
                "signal_id": signal_id,
                "ticker": ticker,
                "sample_size": sample_size,
                "quality": quality,
            },
        )


class SignalConfidenceGuardrail(Guardrail):
    """
    Enforces minimum signal confidence.

    The famous RenTech 50.75% threshold.
    """

    def __init__(self):
        super().__init__(
            name="signal_confidence",
            description="Ensures signals meet minimum confidence threshold",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check signal confidence"""
        limits = self.config.risk_limits
        min_confidence = limits.min_signal_confidence

        confidence = context.get("confidence", 0)
        signal_id = context.get("signal_id", "unknown")
        ticker = context.get("ticker", "UNKNOWN")

        if confidence < min_confidence:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.ERROR,
                action=GuardrailAction.BLOCK,
                message=f"Signal {signal_id} confidence {confidence:.2%} < threshold {min_confidence:.2%}",
                details={
                    "signal_id": signal_id,
                    "ticker": ticker,
                    "confidence": confidence,
                    "threshold": min_confidence,
                    "shortfall": min_confidence - confidence,
                },
            )

        # High confidence signals
        if confidence >= 0.55:
            quality = "high"
            severity = GuardrailSeverity.INFO
        elif confidence >= 0.52:
            quality = "moderate"
            severity = GuardrailSeverity.INFO
        else:
            quality = "marginal"
            severity = GuardrailSeverity.WARNING

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=severity,
            action=GuardrailAction.ALLOW if quality != "marginal" else GuardrailAction.WARN,
            message=f"Signal {signal_id} has {quality} confidence ({confidence:.2%})",
            details={
                "signal_id": signal_id,
                "ticker": ticker,
                "confidence": confidence,
                "quality": quality,
            },
        )


class DataQualityGuardrail(Guardrail):
    """
    Checks for data quality issues.

    Validates that the data underlying signals is clean:
    - No missing values
    - No extreme outliers
    - Corporate action adjustments
    - Data freshness
    """

    def __init__(self):
        super().__init__(
            name="data_quality",
            description="Validates data quality for signals",
            severity=GuardrailSeverity.ERROR,
        )

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check data quality"""
        signal_id = context.get("signal_id", "unknown")
        ticker = context.get("ticker", "UNKNOWN")

        issues = []

        # Check for missing data
        missing_pct = context.get("missing_data_pct", 0)
        if missing_pct > 5:
            issues.append(f"High missing data: {missing_pct:.1f}%")

        # Check for outliers
        outlier_count = context.get("outlier_count", 0)
        if outlier_count > 10:
            issues.append(f"High outlier count: {outlier_count}")

        # Check for corporate actions
        has_corp_actions = context.get("has_corporate_actions", False)
        corp_actions_adjusted = context.get("corporate_actions_adjusted", True)
        if has_corp_actions and not corp_actions_adjusted:
            issues.append("Corporate actions not adjusted")

        # Check data freshness
        data_age_hours = context.get("data_age_hours", 0)
        if data_age_hours > 24:
            issues.append(f"Stale data: {data_age_hours:.0f} hours old")

        # Check for data gaps
        has_gaps = context.get("has_data_gaps", False)
        if has_gaps:
            issues.append("Data has gaps")

        if issues:
            severity = GuardrailSeverity.CRITICAL if "Corporate actions" in str(issues) else GuardrailSeverity.ERROR

            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=severity,
                action=GuardrailAction.BLOCK,
                message=f"Data quality issues for {ticker}: {'; '.join(issues)}",
                details={
                    "signal_id": signal_id,
                    "ticker": ticker,
                    "issues": issues,
                    "missing_pct": missing_pct,
                    "outlier_count": outlier_count,
                    "data_age_hours": data_age_hours,
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"Data quality check passed for {ticker}",
            details={
                "signal_id": signal_id,
                "ticker": ticker,
                "quality": "good",
            },
        )


class SignalStalenessGuardrail(Guardrail):
    """
    Checks if signal is still fresh.

    Alpha decays quickly - signals must be acted on promptly.
    """

    def __init__(self, max_age_minutes: int = 60):
        super().__init__(
            name="signal_staleness",
            description="Ensures signals are acted on before alpha decays",
            severity=GuardrailSeverity.WARNING,
        )
        self.max_age_minutes = max_age_minutes

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check signal age"""
        signal_id = context.get("signal_id", "unknown")
        signal_time = context.get("signal_timestamp")

        if signal_time is None:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message="Signal timestamp not provided, skipping staleness check",
            )

        # Calculate age
        if isinstance(signal_time, str):
            signal_time = datetime.fromisoformat(signal_time.replace("Z", "+00:00"))

        now = datetime.utcnow()
        if signal_time.tzinfo:
            now = now.replace(tzinfo=signal_time.tzinfo)

        age_minutes = (now - signal_time).total_seconds() / 60

        if age_minutes > self.max_age_minutes:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.WARNING,
                action=GuardrailAction.WARN,
                message=f"Signal {signal_id} is {age_minutes:.0f} minutes old (max {self.max_age_minutes})",
                details={
                    "signal_id": signal_id,
                    "age_minutes": age_minutes,
                    "max_age_minutes": self.max_age_minutes,
                    "alpha_decay_warning": True,
                },
            )

        return GuardrailResult(
            guardrail_name=self.name,
            passed=True,
            severity=GuardrailSeverity.INFO,
            action=GuardrailAction.ALLOW,
            message=f"Signal {signal_id} is fresh ({age_minutes:.0f} minutes old)",
            details={
                "signal_id": signal_id,
                "age_minutes": age_minutes,
            },
        )
