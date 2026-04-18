"""
Base Guardrail Infrastructure

Core guardrail system for Renaissance Technologies multi-agent architecture.
Implements validation, enforcement, and monitoring of trading constraints.
"""

from typing import Optional, List, Dict, Any, Callable, Type
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from abc import ABC, abstractmethod
import uuid

from ..config import get_config


class GuardrailSeverity(str, Enum):
    """Severity levels for guardrail violations"""
    INFO = "info"           # Informational only
    WARNING = "warning"     # Caution, but allowed to proceed
    ERROR = "error"         # Must be addressed before proceeding
    CRITICAL = "critical"   # Hard block, cannot proceed


class GuardrailAction(str, Enum):
    """Actions to take on guardrail trigger"""
    ALLOW = "allow"         # Allow to proceed
    WARN = "warn"           # Warn but allow
    MODIFY = "modify"       # Modify the request to comply
    BLOCK = "block"         # Block the action
    ESCALATE = "escalate"   # Escalate to human


@dataclass
class GuardrailResult:
    """Result of a guardrail check"""
    guardrail_name: str
    passed: bool
    severity: GuardrailSeverity
    action: GuardrailAction

    # Details
    message: str
    details: Dict[str, Any] = field(default_factory=dict)

    # Modification (if action is MODIFY)
    modified_value: Optional[Any] = None
    original_value: Optional[Any] = None

    # Metadata
    timestamp: str = field(default_factory=lambda: datetime.utcnow().isoformat())
    guardrail_id: str = field(default_factory=lambda: uuid.uuid4().hex[:8])

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return {
            "guardrail_name": self.guardrail_name,
            "passed": self.passed,
            "severity": self.severity.value,
            "action": self.action.value,
            "message": self.message,
            "details": self.details,
            "modified_value": self.modified_value,
            "original_value": self.original_value,
            "timestamp": self.timestamp,
            "guardrail_id": self.guardrail_id,
        }


class Guardrail(ABC):
    """
    Base class for all guardrails.

    Guardrails are safety checks that run before, during, or after
    agent actions to ensure compliance with rules and limits.
    """

    def __init__(
        self,
        name: str,
        description: str,
        severity: GuardrailSeverity = GuardrailSeverity.ERROR,
        enabled: bool = True,
    ):
        self.name = name
        self.description = description
        self.severity = severity
        self.enabled = enabled
        self.config = get_config()

        # Statistics
        self.total_checks = 0
        self.violations = 0

    @abstractmethod
    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """
        Check if the guardrail condition is met.

        Args:
            context: Dictionary containing all relevant data for the check

        Returns:
            GuardrailResult indicating pass/fail and any actions
        """
        pass

    def execute(self, context: Dict[str, Any]) -> GuardrailResult:
        """
        Execute the guardrail check with tracking.
        """
        if not self.enabled:
            return GuardrailResult(
                guardrail_name=self.name,
                passed=True,
                severity=GuardrailSeverity.INFO,
                action=GuardrailAction.ALLOW,
                message="Guardrail disabled",
            )

        self.total_checks += 1

        try:
            result = self.check(context)

            if not result.passed:
                self.violations += 1

            return result

        except Exception as e:
            self.violations += 1
            return GuardrailResult(
                guardrail_name=self.name,
                passed=False,
                severity=GuardrailSeverity.CRITICAL,
                action=GuardrailAction.BLOCK,
                message=f"Guardrail error: {str(e)}",
                details={"error": str(e)},
            )

    def get_stats(self) -> Dict[str, Any]:
        """Get guardrail statistics"""
        return {
            "name": self.name,
            "enabled": self.enabled,
            "total_checks": self.total_checks,
            "violations": self.violations,
            "violation_rate": self.violations / self.total_checks if self.total_checks > 0 else 0,
        }


class CompositeGuardrail(Guardrail):
    """
    Composite guardrail that combines multiple guardrails.
    """

    def __init__(
        self,
        name: str,
        description: str,
        guardrails: List[Guardrail],
        require_all: bool = True,  # If True, all must pass. If False, any can pass
    ):
        super().__init__(name, description)
        self.guardrails = guardrails
        self.require_all = require_all

    def check(self, context: Dict[str, Any]) -> GuardrailResult:
        """Check all sub-guardrails"""
        results = []
        all_passed = True
        any_passed = False
        highest_severity = GuardrailSeverity.INFO
        messages = []

        for guardrail in self.guardrails:
            result = guardrail.execute(context)
            results.append(result)

            if result.passed:
                any_passed = True
            else:
                all_passed = False
                messages.append(f"{guardrail.name}: {result.message}")

            # Track highest severity
            severity_order = [
                GuardrailSeverity.INFO,
                GuardrailSeverity.WARNING,
                GuardrailSeverity.ERROR,
                GuardrailSeverity.CRITICAL,
            ]
            if severity_order.index(result.severity) > severity_order.index(highest_severity):
                highest_severity = result.severity

        # Determine overall pass/fail
        passed = all_passed if self.require_all else any_passed

        # Determine action
        if passed:
            action = GuardrailAction.ALLOW
        elif highest_severity == GuardrailSeverity.CRITICAL:
            action = GuardrailAction.BLOCK
        elif highest_severity == GuardrailSeverity.ERROR:
            action = GuardrailAction.BLOCK
        else:
            action = GuardrailAction.WARN

        return GuardrailResult(
            guardrail_name=self.name,
            passed=passed,
            severity=highest_severity,
            action=action,
            message="; ".join(messages) if messages else "All checks passed",
            details={"sub_results": [r.to_dict() for r in results]},
        )


class GuardrailRegistry:
    """
    Registry for all guardrails in the system.

    Manages guardrail registration, lookup, and batch execution.
    """

    def __init__(self):
        self._guardrails: Dict[str, Guardrail] = {}
        self._categories: Dict[str, List[str]] = {}

    def register(
        self,
        guardrail: Guardrail,
        category: str = "default",
    ):
        """Register a guardrail"""
        self._guardrails[guardrail.name] = guardrail

        if category not in self._categories:
            self._categories[category] = []
        self._categories[category].append(guardrail.name)

    def get(self, name: str) -> Optional[Guardrail]:
        """Get a guardrail by name"""
        return self._guardrails.get(name)

    def get_by_category(self, category: str) -> List[Guardrail]:
        """Get all guardrails in a category"""
        names = self._categories.get(category, [])
        return [self._guardrails[name] for name in names if name in self._guardrails]

    def check_all(
        self,
        context: Dict[str, Any],
        categories: Optional[List[str]] = None,
    ) -> List[GuardrailResult]:
        """
        Run all guardrails (or specific categories) against context.
        """
        results = []

        if categories:
            guardrails = []
            for cat in categories:
                guardrails.extend(self.get_by_category(cat))
        else:
            guardrails = list(self._guardrails.values())

        for guardrail in guardrails:
            result = guardrail.execute(context)
            results.append(result)

        return results

    def check_and_block(
        self,
        context: Dict[str, Any],
        categories: Optional[List[str]] = None,
    ) -> tuple:
        """
        Run all guardrails and return (can_proceed, results).
        """
        results = self.check_all(context, categories)

        # Check for blocking violations
        for result in results:
            if result.action == GuardrailAction.BLOCK:
                return False, results

        return True, results

    def get_all_stats(self) -> Dict[str, Any]:
        """Get statistics for all guardrails"""
        return {
            name: guardrail.get_stats()
            for name, guardrail in self._guardrails.items()
        }

    def list_guardrails(self) -> List[Dict[str, Any]]:
        """List all registered guardrails"""
        return [
            {
                "name": g.name,
                "description": g.description,
                "severity": g.severity.value,
                "enabled": g.enabled,
            }
            for g in self._guardrails.values()
        ]


# Global registry instance
_guardrail_registry: Optional[GuardrailRegistry] = None


def get_guardrail_registry() -> GuardrailRegistry:
    """Get the global guardrail registry"""
    global _guardrail_registry
    if _guardrail_registry is None:
        _guardrail_registry = GuardrailRegistry()
        _register_default_guardrails(_guardrail_registry)
    return _guardrail_registry


def _register_default_guardrails(registry: GuardrailRegistry):
    """Register default guardrails"""
    # Import here to avoid circular imports
    from .risk_guardrails import (
        PositionSizeGuardrail,
        SectorExposureGuardrail,
        VaRLimitGuardrail,
        DrawdownGuardrail,
        LeverageGuardrail,
    )
    from .compliance_guardrails import (
        RestrictedListGuardrail,
        InsiderTradingGuardrail,
        TradeTimeGuardrail,
    )
    from .signal_guardrails import (
        PValueGuardrail,
        SampleSizeGuardrail,
        SignalConfidenceGuardrail,
        DataQualityGuardrail,
    )
    from .execution_guardrails import (
        LiquidityGuardrail,
        MarketImpactGuardrail,
    )

    # Risk guardrails
    registry.register(PositionSizeGuardrail(), "risk")
    registry.register(SectorExposureGuardrail(), "risk")
    registry.register(VaRLimitGuardrail(), "risk")
    registry.register(DrawdownGuardrail(), "risk")
    registry.register(LeverageGuardrail(), "risk")

    # Compliance guardrails
    registry.register(RestrictedListGuardrail(), "compliance")
    registry.register(InsiderTradingGuardrail(), "compliance")
    registry.register(TradeTimeGuardrail(), "compliance")

    # Signal guardrails
    registry.register(PValueGuardrail(), "signal")
    registry.register(SampleSizeGuardrail(), "signal")
    registry.register(SignalConfidenceGuardrail(), "signal")
    registry.register(DataQualityGuardrail(), "signal")

    # Execution guardrails
    registry.register(LiquidityGuardrail(), "execution")
    registry.register(MarketImpactGuardrail(), "execution")
