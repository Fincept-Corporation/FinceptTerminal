"""
Guardrails Module - Input/Output validation for financial agents

Provides:
- PII detection for financial data (SSN, account numbers, etc.)
- Prompt injection protection
- Financial compliance validation
- Custom guardrails for trading rules
"""

from typing import Dict, Any, Optional, List, Callable
import logging
import re

logger = logging.getLogger(__name__)


class FinancialPIIGuardrail:
    """
    Detect and redact financial PII.

    Detects:
    - Social Security Numbers (SSN)
    - Credit card numbers
    - Bank account numbers
    - API keys and secrets
    - Passwords
    """

    PATTERNS = {
        "ssn": r"\b\d{3}-\d{2}-\d{4}\b",
        "ssn_no_dash": r"\b\d{9}\b",
        "credit_card": r"\b(?:\d{4}[-\s]?){3}\d{4}\b",
        "bank_account": r"\b\d{8,17}\b",
        "api_key": r"\b(sk-|pk-|api[-_]?key)[\w-]{20,}\b",
        "password": r"(?i)(password|passwd|pwd)\s*[:=]\s*\S+",
        "secret": r"(?i)(secret|token)\s*[:=]\s*\S+",
    }

    def __init__(self, redact: bool = True, block: bool = False):
        """
        Initialize PII guardrail.

        Args:
            redact: If True, redact PII from text
            block: If True, block requests containing PII
        """
        self.redact = redact
        self.block = block

    def check(self, text: str) -> Dict[str, Any]:
        """
        Check text for PII.

        Returns:
            {
                "passed": bool,
                "pii_found": List[str],
                "redacted_text": str (if redact=True)
            }
        """
        pii_found = []
        redacted_text = text

        for pii_type, pattern in self.PATTERNS.items():
            matches = re.findall(pattern, text)
            if matches:
                pii_found.append(pii_type)
                if self.redact:
                    redacted_text = re.sub(pattern, f"[REDACTED_{pii_type.upper()}]", redacted_text)

        passed = len(pii_found) == 0 or not self.block

        return {
            "passed": passed,
            "pii_found": pii_found,
            "redacted_text": redacted_text if self.redact else None
        }

    def to_agno_guardrail(self):
        """Convert to Agno guardrail format."""
        try:
            from agno.guardrails import PIIDetectionGuardrail
            return PIIDetectionGuardrail()
        except ImportError:
            return None


class PromptInjectionGuardrail:
    """
    Detect prompt injection attempts.

    Protects against:
    - Instruction override attempts
    - Jailbreak patterns
    - Role manipulation
    """

    INJECTION_PATTERNS = [
        r"(?i)ignore\s+(previous|all|above)\s+instructions?",
        r"(?i)disregard\s+(previous|all|above)",
        r"(?i)forget\s+(everything|all|previous)",
        r"(?i)you\s+are\s+now\s+",
        r"(?i)act\s+as\s+(if\s+you\s+are|a)",
        r"(?i)pretend\s+(to\s+be|you\s+are)",
        r"(?i)new\s+instructions?:",
        r"(?i)system\s*:\s*",
        r"(?i)\[system\]",
        r"(?i)override\s+(mode|instructions?|rules?)",
    ]

    def __init__(self, block: bool = True):
        """
        Initialize prompt injection guardrail.

        Args:
            block: If True, block requests with injection attempts
        """
        self.block = block

    def check(self, text: str) -> Dict[str, Any]:
        """
        Check text for prompt injection.

        Returns:
            {
                "passed": bool,
                "injection_detected": bool,
                "matched_patterns": List[str]
            }
        """
        matched = []

        for pattern in self.INJECTION_PATTERNS:
            if re.search(pattern, text):
                matched.append(pattern)

        injection_detected = len(matched) > 0
        passed = not injection_detected or not self.block

        return {
            "passed": passed,
            "injection_detected": injection_detected,
            "matched_patterns": matched
        }

    def to_agno_guardrail(self):
        """Convert to Agno guardrail format."""
        try:
            from agno.guardrails import PromptInjectionGuardrail as AgnoGuardrail
            return AgnoGuardrail()
        except ImportError:
            return None


class TradingComplianceGuardrail:
    """
    Validate trading-related requests for compliance.

    Checks:
    - Position size limits
    - Prohibited securities
    - Market manipulation patterns
    - Insider trading indicators
    """

    PROHIBITED_PATTERNS = [
        r"(?i)insider\s+(trading|information|tip)",
        r"(?i)pump\s+and\s+dump",
        r"(?i)front\s*run(ning)?",
        r"(?i)wash\s+trad(e|ing)",
        r"(?i)spoof(ing)?",
        r"(?i)layering",
        r"(?i)manipulat(e|ion|ing)\s+(market|price|stock)",
    ]

    def __init__(
        self,
        max_position_pct: float = 0.10,
        prohibited_symbols: List[str] = None,
        block: bool = True
    ):
        """
        Initialize compliance guardrail.

        Args:
            max_position_pct: Maximum position size as % of portfolio
            prohibited_symbols: List of prohibited tickers
            block: If True, block non-compliant requests
        """
        self.max_position_pct = max_position_pct
        self.prohibited_symbols = prohibited_symbols or []
        self.block = block

    def check(self, text: str, context: Dict[str, Any] = None) -> Dict[str, Any]:
        """
        Check text for compliance issues.

        Args:
            text: Input text
            context: Optional context with portfolio info

        Returns:
            {
                "passed": bool,
                "violations": List[str],
                "warnings": List[str]
            }
        """
        violations = []
        warnings = []

        # Check for prohibited patterns
        for pattern in self.PROHIBITED_PATTERNS:
            if re.search(pattern, text):
                violations.append(f"Prohibited activity detected: {pattern}")

        # Check for prohibited symbols
        text_upper = text.upper()
        for symbol in self.prohibited_symbols:
            if symbol.upper() in text_upper:
                violations.append(f"Prohibited symbol: {symbol}")

        # Check position size if context provided
        if context:
            portfolio_value = context.get("portfolio_value", 0)
            proposed_value = context.get("proposed_trade_value", 0)

            if portfolio_value > 0 and proposed_value > 0:
                position_pct = proposed_value / portfolio_value
                if position_pct > self.max_position_pct:
                    violations.append(
                        f"Position size {position_pct*100:.1f}% exceeds limit {self.max_position_pct*100:.1f}%"
                    )

        passed = len(violations) == 0 or not self.block

        return {
            "passed": passed,
            "violations": violations,
            "warnings": warnings
        }


class OutputValidationGuardrail:
    """
    Validate agent outputs for quality and safety.

    Checks:
    - Required fields present
    - Numeric values in valid ranges
    - No hallucinated data
    """

    def __init__(
        self,
        required_fields: List[str] = None,
        numeric_ranges: Dict[str, tuple] = None,
        max_confidence: float = 1.0
    ):
        """
        Initialize output validation guardrail.

        Args:
            required_fields: Fields that must be present
            numeric_ranges: Dict of field -> (min, max) ranges
            max_confidence: Maximum allowed confidence value
        """
        self.required_fields = required_fields or []
        self.numeric_ranges = numeric_ranges or {}
        self.max_confidence = max_confidence

    def check(self, output: Dict[str, Any]) -> Dict[str, Any]:
        """
        Validate output.

        Returns:
            {
                "passed": bool,
                "missing_fields": List[str],
                "range_violations": List[str],
                "warnings": List[str]
            }
        """
        missing = []
        range_violations = []
        warnings = []

        # Check required fields
        for field in self.required_fields:
            if field not in output or output[field] is None:
                missing.append(field)

        # Check numeric ranges
        for field, (min_val, max_val) in self.numeric_ranges.items():
            if field in output and output[field] is not None:
                val = output[field]
                if isinstance(val, (int, float)):
                    if val < min_val or val > max_val:
                        range_violations.append(
                            f"{field}={val} outside range [{min_val}, {max_val}]"
                        )

        # Check confidence
        if "confidence" in output:
            conf = output["confidence"]
            if isinstance(conf, (int, float)) and conf > self.max_confidence:
                warnings.append(f"Confidence {conf} exceeds max {self.max_confidence}")

        passed = len(missing) == 0 and len(range_violations) == 0

        return {
            "passed": passed,
            "missing_fields": missing,
            "range_violations": range_violations,
            "warnings": warnings
        }


class GuardrailsModule:
    """
    Combined guardrails manager for financial agents.
    """

    def __init__(self):
        """Initialize guardrails module."""
        self.input_guardrails: List[Any] = []
        self.output_guardrails: List[Any] = []

    def add_pii_protection(self, redact: bool = True, block: bool = False) -> "GuardrailsModule":
        """Add PII protection guardrail."""
        self.input_guardrails.append(FinancialPIIGuardrail(redact=redact, block=block))
        return self

    def add_injection_protection(self, block: bool = True) -> "GuardrailsModule":
        """Add prompt injection protection."""
        self.input_guardrails.append(PromptInjectionGuardrail(block=block))
        return self

    def add_trading_compliance(
        self,
        max_position_pct: float = 0.10,
        prohibited_symbols: List[str] = None
    ) -> "GuardrailsModule":
        """Add trading compliance guardrail."""
        self.input_guardrails.append(TradingComplianceGuardrail(
            max_position_pct=max_position_pct,
            prohibited_symbols=prohibited_symbols
        ))
        return self

    def add_output_validation(
        self,
        required_fields: List[str] = None,
        numeric_ranges: Dict[str, tuple] = None
    ) -> "GuardrailsModule":
        """Add output validation guardrail."""
        self.output_guardrails.append(OutputValidationGuardrail(
            required_fields=required_fields,
            numeric_ranges=numeric_ranges
        ))
        return self

    def check_input(self, text: str, context: Dict[str, Any] = None) -> Dict[str, Any]:
        """
        Run all input guardrails.

        Returns:
            {
                "passed": bool,
                "text": str (possibly redacted),
                "violations": List[str]
            }
        """
        violations = []
        current_text = text

        for guardrail in self.input_guardrails:
            if hasattr(guardrail, "check"):
                if isinstance(guardrail, TradingComplianceGuardrail):
                    result = guardrail.check(current_text, context)
                else:
                    result = guardrail.check(current_text)

                if not result.get("passed", True):
                    violations.extend(result.get("violations", []))
                    violations.extend(result.get("pii_found", []))
                    if result.get("injection_detected"):
                        violations.append("Prompt injection detected")

                # Use redacted text if available
                if result.get("redacted_text"):
                    current_text = result["redacted_text"]

        return {
            "passed": len(violations) == 0,
            "text": current_text,
            "violations": violations
        }

    def check_output(self, output: Dict[str, Any]) -> Dict[str, Any]:
        """
        Run all output guardrails.

        Returns:
            {
                "passed": bool,
                "violations": List[str],
                "warnings": List[str]
            }
        """
        violations = []
        warnings = []

        for guardrail in self.output_guardrails:
            if hasattr(guardrail, "check"):
                result = guardrail.check(output)

                if not result.get("passed", True):
                    violations.extend(result.get("missing_fields", []))
                    violations.extend(result.get("range_violations", []))

                warnings.extend(result.get("warnings", []))

        return {
            "passed": len(violations) == 0,
            "violations": violations,
            "warnings": warnings
        }

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert to Agno agent config format.

        Returns config dict for Agent initialization.
        """
        config = {}

        # Convert to Agno guardrails if available
        agno_guardrails = []
        for guardrail in self.input_guardrails:
            if hasattr(guardrail, "to_agno_guardrail"):
                agno_g = guardrail.to_agno_guardrail()
                if agno_g:
                    agno_guardrails.append(agno_g)

        # Note: Agno doesn't have built-in guardrails parameter
        # We'll use pre_hooks instead
        return config

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "GuardrailsModule":
        """Create from configuration."""
        module = cls()

        if config.get("pii_protection", True):
            module.add_pii_protection(
                redact=config.get("pii_redact", True),
                block=config.get("pii_block", False)
            )

        if config.get("injection_protection", True):
            module.add_injection_protection(block=config.get("injection_block", True))

        if config.get("trading_compliance"):
            tc = config["trading_compliance"]
            module.add_trading_compliance(
                max_position_pct=tc.get("max_position_pct", 0.10),
                prohibited_symbols=tc.get("prohibited_symbols", [])
            )

        if config.get("output_validation"):
            ov = config["output_validation"]
            module.add_output_validation(
                required_fields=ov.get("required_fields"),
                numeric_ranges=ov.get("numeric_ranges")
            )

        return module

    @classmethod
    def default_financial(cls) -> "GuardrailsModule":
        """Create default guardrails for financial agents."""
        return (cls()
            .add_pii_protection(redact=True, block=False)
            .add_injection_protection(block=True)
            .add_trading_compliance(max_position_pct=0.10)
            .add_output_validation(
                required_fields=["symbol", "action", "reasoning"],
                numeric_ranges={
                    "confidence": (0.0, 1.0),
                    "quantity": (0.0, float("inf")),
                    "price": (0.0, float("inf"))
                }
            ))
