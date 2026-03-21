"""
Renaissance Technologies Guardrails System

Safety and compliance guardrails for the multi-agent system:
- Risk limit enforcement
- Compliance checks
- Position limit validation
- Trade validation
- Input/output validation
"""

from .base import (
    Guardrail,
    GuardrailResult,
    GuardrailSeverity,
    GuardrailRegistry,
    get_guardrail_registry,
)
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
    ComplianceApprovalGuardrail,
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
    ExecutionTimeGuardrail,
    PriceDeviationGuardrail,
)

__all__ = [
    # Base
    "Guardrail",
    "GuardrailResult",
    "GuardrailSeverity",
    "GuardrailRegistry",
    "get_guardrail_registry",
    # Risk guardrails
    "PositionSizeGuardrail",
    "SectorExposureGuardrail",
    "VaRLimitGuardrail",
    "DrawdownGuardrail",
    "LeverageGuardrail",
    # Compliance guardrails
    "RestrictedListGuardrail",
    "InsiderTradingGuardrail",
    "TradeTimeGuardrail",
    "ComplianceApprovalGuardrail",
    # Signal guardrails
    "PValueGuardrail",
    "SampleSizeGuardrail",
    "SignalConfidenceGuardrail",
    "DataQualityGuardrail",
    # Execution guardrails
    "LiquidityGuardrail",
    "MarketImpactGuardrail",
    "ExecutionTimeGuardrail",
    "PriceDeviationGuardrail",
]
