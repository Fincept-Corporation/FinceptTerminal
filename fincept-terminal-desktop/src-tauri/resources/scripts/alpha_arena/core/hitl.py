"""
Human-in-the-Loop (HITL) Approval System for Alpha Arena

Provides approval mechanisms for:
- Live trading decisions
- High-risk actions
- Large position sizes
- Strategy changes

Integrates with frontend via Tauri events.
"""

import asyncio
from typing import Dict, List, Optional, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum
import json

from alpha_arena.types.models import ModelDecision, TradeAction
from alpha_arena.utils.logging import get_logger

logger = get_logger("hitl")


class ApprovalStatus(Enum):
    PENDING = "pending"
    APPROVED = "approved"
    REJECTED = "rejected"
    TIMEOUT = "timeout"
    AUTO_APPROVED = "auto_approved"


class RiskLevel(Enum):
    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"
    CRITICAL = "critical"


@dataclass
class ApprovalRequest:
    """A request for human approval"""
    id: str
    decision: ModelDecision
    risk_level: RiskLevel
    reason: str
    created_at: datetime = field(default_factory=datetime.now)
    expires_at: Optional[datetime] = None
    status: ApprovalStatus = ApprovalStatus.PENDING
    approved_by: Optional[str] = None
    approval_time: Optional[datetime] = None
    notes: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "decision": {
                "model_name": self.decision.model_name,
                "action": self.decision.action.value if hasattr(self.decision.action, 'value') else str(self.decision.action),
                "symbol": self.decision.symbol,
                "quantity": self.decision.quantity,
                "confidence": self.decision.confidence,
                "reasoning": self.decision.reasoning,
                "price_at_decision": self.decision.price_at_decision,
            },
            "risk_level": self.risk_level.value,
            "reason": self.reason,
            "created_at": self.created_at.isoformat(),
            "expires_at": self.expires_at.isoformat() if self.expires_at else None,
            "status": self.status.value,
            "approved_by": self.approved_by,
            "approval_time": self.approval_time.isoformat() if self.approval_time else None,
            "notes": self.notes,
        }

    def is_expired(self) -> bool:
        if self.expires_at is None:
            return False
        return datetime.now() > self.expires_at


@dataclass
class ApprovalRule:
    """A rule that determines when approval is required"""
    name: str
    description: str
    condition: Callable[[ModelDecision, Dict[str, Any]], bool]
    risk_level: RiskLevel
    auto_approve_timeout: Optional[int] = None  # Seconds after which to auto-approve
    enabled: bool = True


class HITLManager:
    """
    Human-in-the-Loop Manager

    Manages approval requests and rules for trading decisions.
    """

    def __init__(
        self,
        default_timeout: int = 60,
        auto_approve_low_risk: bool = True,
    ):
        self.default_timeout = default_timeout
        self.auto_approve_low_risk = auto_approve_low_risk
        self._rules: List[ApprovalRule] = []
        self._pending_requests: Dict[str, ApprovalRequest] = {}
        self._history: List[ApprovalRequest] = []
        self._callbacks: Dict[str, Callable] = {}
        self._request_counter = 0

        # Setup default rules
        self._setup_default_rules()

    def _setup_default_rules(self):
        """Setup default approval rules"""

        # Rule 1: Large position size (>25% of portfolio)
        self.add_rule(ApprovalRule(
            name="large_position",
            description="Position size exceeds 25% of portfolio",
            condition=lambda d, ctx: self._check_large_position(d, ctx),
            risk_level=RiskLevel.HIGH,
            auto_approve_timeout=30,
        ))

        # Rule 2: Live trading mode
        self.add_rule(ApprovalRule(
            name="live_trading",
            description="Live trading enabled (real money)",
            condition=lambda d, ctx: ctx.get("is_live_trading", False),
            risk_level=RiskLevel.CRITICAL,
            auto_approve_timeout=None,  # Never auto-approve
        ))

        # Rule 3: High volatility market
        self.add_rule(ApprovalRule(
            name="high_volatility",
            description="Market volatility is extremely high",
            condition=lambda d, ctx: ctx.get("volatility_level") == "extreme",
            risk_level=RiskLevel.MEDIUM,
            auto_approve_timeout=15,
        ))

        # Rule 4: Low confidence decision
        self.add_rule(ApprovalRule(
            name="low_confidence",
            description="Agent confidence is below 50%",
            condition=lambda d, ctx: d.confidence < 0.5 and d.action != TradeAction.HOLD,
            risk_level=RiskLevel.MEDIUM,
            auto_approve_timeout=20,
        ))

        # Rule 5: Consecutive losses
        self.add_rule(ApprovalRule(
            name="consecutive_losses",
            description="3+ consecutive losing trades",
            condition=lambda d, ctx: ctx.get("consecutive_losses", 0) >= 3,
            risk_level=RiskLevel.HIGH,
            auto_approve_timeout=45,
        ))

        # Rule 6: Max drawdown exceeded
        self.add_rule(ApprovalRule(
            name="max_drawdown",
            description="Portfolio drawdown exceeds 20%",
            condition=lambda d, ctx: ctx.get("current_drawdown_pct", 0) > 20,
            risk_level=RiskLevel.HIGH,
            auto_approve_timeout=None,
        ))

    def _check_large_position(self, decision: ModelDecision, context: Dict[str, Any]) -> bool:
        """Check if position size is too large"""
        portfolio_value = context.get("portfolio_value", 10000)
        price = decision.price_at_decision or context.get("current_price", 1)
        position_value = decision.quantity * price
        position_pct = (position_value / portfolio_value) * 100
        return position_pct > 25

    def add_rule(self, rule: ApprovalRule):
        """Add an approval rule"""
        self._rules.append(rule)
        logger.info(f"Added HITL rule: {rule.name}")

    def remove_rule(self, name: str):
        """Remove an approval rule by name"""
        self._rules = [r for r in self._rules if r.name != name]

    def set_rule_enabled(self, name: str, enabled: bool):
        """Enable or disable a rule"""
        for rule in self._rules:
            if rule.name == name:
                rule.enabled = enabled
                break

    def check_approval_required(
        self,
        decision: ModelDecision,
        context: Dict[str, Any],
    ) -> tuple[bool, List[ApprovalRule]]:
        """
        Check if approval is required for a decision.

        Returns:
            Tuple of (requires_approval, triggered_rules)
        """
        triggered = []

        for rule in self._rules:
            if not rule.enabled:
                continue

            try:
                if rule.condition(decision, context):
                    triggered.append(rule)
            except Exception as e:
                logger.warning(f"Error checking rule {rule.name}: {e}")

        return len(triggered) > 0, triggered

    def create_approval_request(
        self,
        decision: ModelDecision,
        triggered_rules: List[ApprovalRule],
        context: Dict[str, Any],
    ) -> ApprovalRequest:
        """Create an approval request"""
        self._request_counter += 1

        # Determine highest risk level
        risk_levels = [r.risk_level for r in triggered_rules]
        highest_risk = max(risk_levels, key=lambda r: list(RiskLevel).index(r))

        # Determine timeout
        timeouts = [r.auto_approve_timeout for r in triggered_rules if r.auto_approve_timeout]
        if timeouts:
            timeout = max(timeouts)  # Use longest timeout
        elif highest_risk == RiskLevel.CRITICAL:
            timeout = None  # No auto-approve for critical
        else:
            timeout = self.default_timeout

        # Build reason
        reasons = [f"- {r.name}: {r.description}" for r in triggered_rules]
        reason = "Approval required:\n" + "\n".join(reasons)

        # Create request
        request = ApprovalRequest(
            id=f"hitl_{self._request_counter}_{datetime.now().strftime('%Y%m%d%H%M%S')}",
            decision=decision,
            risk_level=highest_risk,
            reason=reason,
            expires_at=datetime.now() + timedelta(seconds=timeout) if timeout else None,
        )

        self._pending_requests[request.id] = request
        logger.info(f"Created approval request {request.id} (risk: {highest_risk.value})")

        return request

    async def wait_for_approval(
        self,
        request: ApprovalRequest,
        poll_interval: float = 0.5,
    ) -> ApprovalStatus:
        """
        Wait for approval of a request.

        Returns the final status (approved, rejected, or timeout).
        """
        while request.status == ApprovalStatus.PENDING:
            # Check expiration
            if request.is_expired():
                # Auto-approve low/medium risk, reject high/critical
                if self.auto_approve_low_risk and request.risk_level in [RiskLevel.LOW, RiskLevel.MEDIUM]:
                    request.status = ApprovalStatus.AUTO_APPROVED
                    logger.info(f"Request {request.id} auto-approved (timeout)")
                else:
                    request.status = ApprovalStatus.TIMEOUT
                    logger.info(f"Request {request.id} timed out")
                break

            await asyncio.sleep(poll_interval)

        # Move to history
        if request.id in self._pending_requests:
            del self._pending_requests[request.id]
        self._history.append(request)

        return request.status

    def approve(self, request_id: str, approved_by: str = "user", notes: str = ""):
        """Approve a pending request"""
        if request_id in self._pending_requests:
            request = self._pending_requests[request_id]
            request.status = ApprovalStatus.APPROVED
            request.approved_by = approved_by
            request.approval_time = datetime.now()
            request.notes = notes
            logger.info(f"Request {request_id} approved by {approved_by}")
            return True
        return False

    def reject(self, request_id: str, rejected_by: str = "user", notes: str = ""):
        """Reject a pending request"""
        if request_id in self._pending_requests:
            request = self._pending_requests[request_id]
            request.status = ApprovalStatus.REJECTED
            request.approved_by = rejected_by
            request.approval_time = datetime.now()
            request.notes = notes
            logger.info(f"Request {request_id} rejected by {rejected_by}")
            return True
        return False

    def get_pending_requests(self) -> List[ApprovalRequest]:
        """Get all pending approval requests"""
        return list(self._pending_requests.values())

    def get_request(self, request_id: str) -> Optional[ApprovalRequest]:
        """Get a specific request"""
        return self._pending_requests.get(request_id)

    def get_history(self, limit: int = 50) -> List[ApprovalRequest]:
        """Get approval history"""
        return self._history[-limit:]

    def clear_pending(self):
        """Clear all pending requests"""
        for request in self._pending_requests.values():
            request.status = ApprovalStatus.TIMEOUT
            self._history.append(request)
        self._pending_requests.clear()

    def to_dict(self) -> Dict[str, Any]:
        """Get manager state as dict"""
        return {
            "rules": [
                {
                    "name": r.name,
                    "description": r.description,
                    "risk_level": r.risk_level.value,
                    "auto_approve_timeout": r.auto_approve_timeout,
                    "enabled": r.enabled,
                }
                for r in self._rules
            ],
            "pending_count": len(self._pending_requests),
            "pending": [r.to_dict() for r in self._pending_requests.values()],
            "history_count": len(self._history),
        }


# Singleton instance
_hitl_manager: Optional[HITLManager] = None


def get_hitl_manager() -> HITLManager:
    """Get the HITL manager singleton"""
    global _hitl_manager
    if _hitl_manager is None:
        _hitl_manager = HITLManager()
    return _hitl_manager


def reset_hitl_manager():
    """Reset the HITL manager"""
    global _hitl_manager
    _hitl_manager = None


# Convenience functions for Tauri integration
def check_decision_approval(
    decision_dict: Dict[str, Any],
    context: Dict[str, Any],
) -> Dict[str, Any]:
    """
    Check if a decision requires approval.

    Used by Tauri commands.
    """
    manager = get_hitl_manager()

    # Reconstruct decision object
    decision = ModelDecision(
        competition_id=decision_dict.get("competition_id", ""),
        model_name=decision_dict.get("model_name", ""),
        cycle_number=decision_dict.get("cycle_number", 0),
        action=TradeAction(decision_dict.get("action", "hold")),
        symbol=decision_dict.get("symbol", ""),
        quantity=decision_dict.get("quantity", 0),
        confidence=decision_dict.get("confidence", 0),
        reasoning=decision_dict.get("reasoning", ""),
        price_at_decision=decision_dict.get("price_at_decision", 0),
    )

    requires, rules = manager.check_approval_required(decision, context)

    if requires:
        request = manager.create_approval_request(decision, rules, context)
        return {
            "requires_approval": True,
            "request": request.to_dict(),
        }

    return {
        "requires_approval": False,
        "request": None,
    }


def approve_decision(request_id: str, approved_by: str = "user", notes: str = "") -> Dict[str, Any]:
    """Approve a decision request"""
    manager = get_hitl_manager()
    success = manager.approve(request_id, approved_by, notes)
    return {"success": success, "request_id": request_id, "action": "approved"}


def reject_decision(request_id: str, rejected_by: str = "user", notes: str = "") -> Dict[str, Any]:
    """Reject a decision request"""
    manager = get_hitl_manager()
    success = manager.reject(request_id, rejected_by, notes)
    return {"success": success, "request_id": request_id, "action": "rejected"}


def get_pending_approvals() -> List[Dict[str, Any]]:
    """Get all pending approval requests"""
    manager = get_hitl_manager()
    return [r.to_dict() for r in manager.get_pending_requests()]
