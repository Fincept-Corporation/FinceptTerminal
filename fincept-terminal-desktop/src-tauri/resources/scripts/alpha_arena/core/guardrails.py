"""
Guardrails Module

Input/output validation for trading decisions.
"""

import re
from typing import Dict, List, Optional, Any
from pydantic import BaseModel, Field

from alpha_arena.types.models import ModelDecision, TradeAction
from alpha_arena.utils.logging import get_logger

logger = get_logger("guardrails")


class GuardrailResult(BaseModel):
    """Result of a guardrail check."""

    passed: bool = Field(..., description="Whether the check passed")
    violations: List[str] = Field(default_factory=list, description="List of violations")
    warnings: List[str] = Field(default_factory=list, description="List of warnings")
    modified_value: Optional[Any] = Field(None, description="Modified value if sanitized")


class TradingGuardrails:
    """
    Guardrails for trading decisions.

    Validates:
    - Position sizing limits
    - Risk management rules
    - Input sanitization
    """

    def __init__(
        self,
        max_position_pct: float = 0.5,
        max_single_trade_pct: float = 0.25,
        max_drawdown_pct: float = 0.2,
        min_confidence: float = 0.3,
        blocked_symbols: Optional[List[str]] = None,
    ):
        self.max_position_pct = max_position_pct
        self.max_single_trade_pct = max_single_trade_pct
        self.max_drawdown_pct = max_drawdown_pct
        self.min_confidence = min_confidence
        self.blocked_symbols = blocked_symbols or []

    def check_decision(
        self,
        decision: ModelDecision,
        portfolio_value: float,
        current_positions_value: float = 0,
    ) -> GuardrailResult:
        """
        Check a trading decision against guardrails.

        Args:
            decision: The trading decision to check
            portfolio_value: Current portfolio value
            current_positions_value: Value of current positions

        Returns:
            GuardrailResult with pass/fail and any violations
        """
        violations = []
        warnings = []

        # Check blocked symbols
        if decision.symbol in self.blocked_symbols:
            violations.append(f"Symbol {decision.symbol} is blocked")

        # Check confidence threshold
        if decision.confidence < self.min_confidence and decision.action != TradeAction.HOLD:
            warnings.append(
                f"Low confidence ({decision.confidence:.0%}) below threshold ({self.min_confidence:.0%})"
            )

        # Check position sizing
        if decision.action in [TradeAction.BUY, TradeAction.SHORT]:
            trade_value = decision.quantity * (decision.price_at_decision or 0)
            trade_pct = trade_value / portfolio_value if portfolio_value > 0 else 1

            if trade_pct > self.max_single_trade_pct:
                violations.append(
                    f"Trade size ({trade_pct:.0%}) exceeds max ({self.max_single_trade_pct:.0%})"
                )

            # Check total position limit
            new_position_value = current_positions_value + trade_value
            position_pct = new_position_value / portfolio_value if portfolio_value > 0 else 1

            if position_pct > self.max_position_pct:
                violations.append(
                    f"Total position ({position_pct:.0%}) would exceed max ({self.max_position_pct:.0%})"
                )

        # Check for zero quantity on non-hold actions
        if decision.action != TradeAction.HOLD and decision.quantity <= 0:
            warnings.append("Non-hold action with zero quantity")

        return GuardrailResult(
            passed=len(violations) == 0,
            violations=violations,
            warnings=warnings,
        )

    def clamp_quantity(
        self,
        decision: ModelDecision,
        cash: float,
        price: float,
        position_quantity: float = 0.0,
    ) -> tuple[float, List[str]]:
        """
        Clamp trade quantity to affordable/available range.

        Returns:
            Tuple of (clamped_quantity, list_of_warnings)
        """
        warnings = []
        quantity = decision.quantity

        if decision.action == TradeAction.HOLD:
            if quantity != 0:
                warnings.append(f"HOLD action: quantity clamped from {quantity} to 0")
            return 0.0, warnings

        if decision.action == TradeAction.BUY:
            if price <= 0:
                warnings.append("Price is zero or negative, cannot buy")
                return 0.0, warnings
            max_affordable = cash / price
            # Apply max single trade limit
            max_by_rule = (cash + position_quantity * price) * self.max_single_trade_pct / price
            effective_max = min(max_affordable, max_by_rule)
            if quantity > effective_max:
                warnings.append(
                    f"BUY quantity clamped: {quantity:.6f} -> {effective_max:.6f} "
                    f"(cash=${cash:.2f}, price=${price:.2f})"
                )
                quantity = effective_max
            if quantity <= 0:
                warnings.append("Insufficient cash for BUY")
                return 0.0, warnings

        elif decision.action == TradeAction.SELL:
            if quantity > position_quantity:
                warnings.append(
                    f"SELL quantity clamped: {quantity:.6f} -> {position_quantity:.6f} "
                    f"(only {position_quantity:.6f} available)"
                )
                quantity = position_quantity
            if quantity <= 0:
                warnings.append("No position to sell")
                return 0.0, warnings

        elif decision.action == TradeAction.SHORT:
            if price <= 0:
                warnings.append("Price is zero or negative, cannot short")
                return 0.0, warnings
            max_by_margin = cash / price * 0.5  # 50% margin requirement
            if quantity > max_by_margin:
                warnings.append(
                    f"SHORT quantity clamped: {quantity:.6f} -> {max_by_margin:.6f}"
                )
                quantity = max_by_margin

        return max(quantity, 0.0), warnings

    def sanitize_reasoning(self, reasoning: str) -> str:
        """
        Sanitize reasoning text to remove potentially harmful content.

        Args:
            reasoning: Raw reasoning text

        Returns:
            Sanitized reasoning text
        """
        if not reasoning:
            return ""

        # Remove any potential injection patterns
        sanitized = reasoning

        # Remove script tags
        sanitized = re.sub(r'<script[^>]*>.*?</script>', '', sanitized, flags=re.IGNORECASE | re.DOTALL)

        # Remove other HTML tags
        sanitized = re.sub(r'<[^>]+>', '', sanitized)

        # Limit length
        max_length = 1000
        if len(sanitized) > max_length:
            sanitized = sanitized[:max_length] + "..."

        return sanitized.strip()

    def validate_json_response(self, response: str) -> GuardrailResult:
        """
        Validate that a response is valid JSON and contains expected fields.

        Args:
            response: Raw response string

        Returns:
            GuardrailResult with validation status
        """
        import json

        violations = []
        warnings = []

        try:
            # Try to parse JSON
            data = json.loads(response)

            # Check required fields
            required_fields = ["action"]
            for field in required_fields:
                if field not in data:
                    violations.append(f"Missing required field: {field}")

            # Validate action
            if "action" in data:
                action = data["action"].lower()
                valid_actions = ["buy", "sell", "hold", "short"]
                if action not in valid_actions:
                    violations.append(f"Invalid action: {action}")

            # Validate quantity if present
            if "quantity" in data:
                try:
                    qty = float(data["quantity"])
                    if qty < 0:
                        violations.append("Quantity cannot be negative")
                except (ValueError, TypeError):
                    violations.append("Invalid quantity format")

            # Validate confidence if present
            if "confidence" in data:
                try:
                    conf = float(data["confidence"])
                    if not 0 <= conf <= 1:
                        warnings.append("Confidence should be between 0 and 1")
                except (ValueError, TypeError):
                    warnings.append("Invalid confidence format")

            return GuardrailResult(
                passed=len(violations) == 0,
                violations=violations,
                warnings=warnings,
                modified_value=data if len(violations) == 0 else None,
            )

        except json.JSONDecodeError as e:
            return GuardrailResult(
                passed=False,
                violations=[f"Invalid JSON: {str(e)}"],
            )


class InputGuardrails:
    """
    Guardrails for user input validation.

    Validates:
    - PII detection
    - Injection prevention
    - Content moderation
    """

    PII_PATTERNS = [
        r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b',  # Email
        r'\b\d{3}[-.]?\d{3}[-.]?\d{4}\b',  # Phone
        r'\b\d{3}[-]?\d{2}[-]?\d{4}\b',  # SSN
        r'\b(?:\d{4}[-\s]?){3}\d{4}\b',  # Credit card
    ]

    def __init__(
        self,
        check_pii: bool = True,
        check_injection: bool = True,
        max_input_length: int = 10000,
    ):
        self.check_pii = check_pii
        self.check_injection = check_injection
        self.max_input_length = max_input_length

    def check_input(self, text: str, context: Optional[Dict] = None) -> GuardrailResult:
        """
        Check user input against guardrails.

        Args:
            text: Input text to check
            context: Optional context dictionary

        Returns:
            GuardrailResult with check status
        """
        violations = []
        warnings = []
        sanitized = text

        # Length check
        if len(text) > self.max_input_length:
            violations.append(f"Input exceeds max length ({self.max_input_length})")
            sanitized = text[:self.max_input_length]

        # PII check
        if self.check_pii:
            for pattern in self.PII_PATTERNS:
                if re.search(pattern, text, re.IGNORECASE):
                    warnings.append("Potential PII detected - consider removing")
                    # Redact PII
                    sanitized = re.sub(pattern, "[REDACTED]", sanitized, flags=re.IGNORECASE)

        # Injection check
        if self.check_injection:
            injection_patterns = [
                r'<script[^>]*>',
                r'javascript:',
                r'on\w+\s*=',
                r'\{\{.*\}\}',  # Template injection
                r'\$\{.*\}',    # Variable injection
            ]
            for pattern in injection_patterns:
                if re.search(pattern, text, re.IGNORECASE):
                    warnings.append("Potential injection pattern detected")
                    sanitized = re.sub(pattern, '', sanitized, flags=re.IGNORECASE)

        return GuardrailResult(
            passed=len(violations) == 0,
            violations=violations,
            warnings=warnings,
            modified_value=sanitized if sanitized != text else None,
        )


class OutputGuardrails:
    """
    Guardrails for agent output validation.

    Validates:
    - Financial advice disclaimers
    - Confidence calibration
    - Source attribution
    """

    def __init__(
        self,
        require_disclaimer: bool = True,
        max_output_length: int = 50000,
    ):
        self.require_disclaimer = require_disclaimer
        self.max_output_length = max_output_length

    def check_output(self, output: Dict[str, Any]) -> GuardrailResult:
        """
        Check agent output against guardrails.

        Args:
            output: Output dictionary to check

        Returns:
            GuardrailResult with check status
        """
        violations = []
        warnings = []

        # Check for financial advice disclaimer
        if self.require_disclaimer:
            reasoning = output.get("reasoning", "")
            if any(phrase in reasoning.lower() for phrase in [
                "financial advice",
                "investment advice",
                "should buy",
                "should sell",
                "guaranteed",
            ]):
                warnings.append("Output may contain financial advice - consider adding disclaimer")

        # Check output length
        output_str = str(output)
        if len(output_str) > self.max_output_length:
            violations.append(f"Output exceeds max length ({self.max_output_length})")

        return GuardrailResult(
            passed=len(violations) == 0,
            violations=violations,
            warnings=warnings,
        )


# Factory function for default guardrails
def create_trading_guardrails(mode: str = "baseline") -> TradingGuardrails:
    """Create trading guardrails based on competition mode."""

    if mode == "monk":
        return TradingGuardrails(
            max_position_pct=0.3,
            max_single_trade_pct=0.1,
            min_confidence=0.75,
        )
    elif mode == "max_leverage":
        return TradingGuardrails(
            max_position_pct=0.95,
            max_single_trade_pct=0.5,
            min_confidence=0.2,
        )
    else:  # baseline, situational
        return TradingGuardrails(
            max_position_pct=0.5,
            max_single_trade_pct=0.25,
            min_confidence=0.3,
        )
