"""
Decision Memory

Memory system for tracking investment decisions:
- IC decisions and rationale
- Signal approvals/rejections
- Risk override decisions
- Position sizing decisions
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
from dataclasses import dataclass
from enum import Enum
import uuid

from .base import (
    RenTechMemory,
    MemoryType,
    MemoryEntry,
    MemoryPriority,
    get_memory_system,
)


class DecisionType(str, Enum):
    """Types of investment decisions"""
    IC_APPROVAL = "ic_approval"
    IC_REJECTION = "ic_rejection"
    SIGNAL_GENERATION = "signal_generation"
    RISK_OVERRIDE = "risk_override"
    POSITION_SIZING = "position_sizing"
    EXECUTION_CHOICE = "execution_choice"
    STOP_LOSS = "stop_loss"
    TAKE_PROFIT = "take_profit"
    REBALANCE = "rebalance"


class DecisionOutcome(str, Enum):
    """Outcomes of decisions"""
    CORRECT = "correct"
    INCORRECT = "incorrect"
    PARTIAL = "partial"
    PENDING = "pending"
    UNKNOWN = "unknown"


@dataclass
class Decision:
    """Structured decision record"""
    decision_id: str
    decision_type: DecisionType
    decision_maker: str  # Agent ID
    timestamp: str

    # What was decided
    subject: str  # What was the decision about (e.g., "AAPL long")
    action: str  # What action was taken
    rationale: List[str]  # Why this decision was made

    # Context
    signal_strength: Optional[float] = None
    confidence: Optional[float] = None
    risk_score: Optional[float] = None
    market_conditions: Optional[Dict[str, Any]] = None

    # Outcome (updated later)
    outcome: DecisionOutcome = DecisionOutcome.PENDING
    outcome_notes: Optional[str] = None
    outcome_pnl_bps: Optional[float] = None


class DecisionMemory:
    """
    Decision-specific memory manager.

    Tracks and learns from all investment decisions made by
    the fund's agents and teams. Used for:

    1. Understanding why decisions were made
    2. Learning from correct/incorrect decisions
    3. Improving decision-making over time
    4. Accountability and audit trails
    """

    def __init__(self, memory_system: Optional[RenTechMemory] = None):
        self.memory = memory_system or get_memory_system()

    def remember_decision(
        self,
        decision: Decision,
        supporting_data: Optional[Dict[str, Any]] = None,
    ) -> str:
        """
        Remember an investment decision.

        Args:
            decision: The decision to remember
            supporting_data: Additional data that supported the decision

        Returns:
            Memory ID
        """
        # Determine priority based on decision type and impact
        priority = self._determine_priority(decision)

        # Build content
        content = f"""
Decision: {decision.decision_type.value.upper()}
Subject: {decision.subject}
Maker: {decision.decision_maker}
Time: {decision.timestamp}

Action: {decision.action}

Rationale:
{chr(10).join([f"- {r}" for r in decision.rationale])}

Context:
- Signal Strength: {decision.signal_strength or 'N/A'}
- Confidence: {decision.confidence or 'N/A'}
- Risk Score: {decision.risk_score or 'N/A'}
- Market Conditions: {decision.market_conditions or 'N/A'}

Outcome: {decision.outcome.value.upper()}
{f"Notes: {decision.outcome_notes}" if decision.outcome_notes else ""}
{f"P&L: {decision.outcome_pnl_bps:+.1f} bps" if decision.outcome_pnl_bps is not None else ""}
""".strip()

        # Context for structured retrieval
        context = {
            "decision": {
                "id": decision.decision_id,
                "type": decision.decision_type.value,
                "maker": decision.decision_maker,
                "subject": decision.subject,
                "action": decision.action,
                "rationale": decision.rationale,
                "signal_strength": decision.signal_strength,
                "confidence": decision.confidence,
                "risk_score": decision.risk_score,
                "outcome": decision.outcome.value,
            },
            "supporting_data": supporting_data or {},
        }

        # Tags
        tags = [
            decision.decision_type.value,
            decision.decision_maker,
            decision.outcome.value,
        ]

        # Extract ticker if present in subject
        subject_parts = decision.subject.split()
        if subject_parts:
            tags.append(subject_parts[0])  # First word often ticker

        # Emotional valence based on outcome
        emotional_valence = 0.0
        if decision.outcome == DecisionOutcome.CORRECT:
            emotional_valence = 0.5
        elif decision.outcome == DecisionOutcome.INCORRECT:
            emotional_valence = -0.5

        # IC decisions are critical memories
        if decision.decision_type in [DecisionType.IC_APPROVAL, DecisionType.IC_REJECTION]:
            memory_type = MemoryType.LONG_TERM
        else:
            memory_type = MemoryType.SHORT_TERM

        return self.memory.add_memory(
            content=content,
            memory_type=memory_type,
            agent_id=decision.decision_maker,
            context=context,
            priority=priority,
            tags=tags,
            emotional_valence=emotional_valence,
        )

    def _determine_priority(self, decision: Decision) -> MemoryPriority:
        """Determine memory priority for a decision"""
        # IC decisions are always high priority
        if decision.decision_type in [DecisionType.IC_APPROVAL, DecisionType.IC_REJECTION]:
            return MemoryPriority.HIGH

        # Risk overrides are critical
        if decision.decision_type == DecisionType.RISK_OVERRIDE:
            return MemoryPriority.CRITICAL

        # Incorrect decisions are important to remember
        if decision.outcome == DecisionOutcome.INCORRECT:
            return MemoryPriority.HIGH

        return MemoryPriority.MEDIUM

    def update_decision_outcome(
        self,
        decision_id: str,
        outcome: DecisionOutcome,
        outcome_notes: Optional[str] = None,
        pnl_bps: Optional[float] = None,
    ):
        """
        Update the outcome of a previously recorded decision.

        Called when we know how a decision played out.
        """
        # Search for the decision
        memories = self.memory.recall(
            query=decision_id,
            limit=5,
        )

        for m in memories:
            ctx = m.context.get("decision", {})
            if ctx.get("id") == decision_id:
                # Update context
                ctx["outcome"] = outcome.value
                ctx["outcome_notes"] = outcome_notes
                ctx["outcome_pnl_bps"] = pnl_bps
                m.context["decision"] = ctx

                # Adjust emotional valence
                if outcome == DecisionOutcome.CORRECT:
                    m.emotional_valence = 0.5
                    m.reinforce(0.3)  # Reinforce good decisions
                elif outcome == DecisionOutcome.INCORRECT:
                    m.emotional_valence = -0.5
                    m.reinforce(0.5)  # Strongly reinforce failures (to remember)

                # Promote to long-term if significant
                if outcome == DecisionOutcome.INCORRECT or abs(pnl_bps or 0) > 100:
                    if m.type == MemoryType.SHORT_TERM:
                        m.type = MemoryType.LONG_TERM
                        m.expires_at = None

                break

    def recall_similar_decisions(
        self,
        decision_type: DecisionType,
        subject: str,
        limit: int = 5,
    ) -> List[MemoryEntry]:
        """
        Recall similar past decisions.

        Used to inform current decision-making based on
        historical precedent.
        """
        query = f"{decision_type.value} {subject}"

        memories = self.memory.recall(
            query=query,
            memory_types=[MemoryType.SHORT_TERM, MemoryType.LONG_TERM],
            limit=limit * 2,
        )

        # Filter to matching decision type
        relevant = []
        for m in memories:
            ctx = m.context.get("decision", {})
            if ctx.get("type") == decision_type.value:
                relevant.append(m)

        return relevant[:limit]

    def recall_incorrect_decisions(
        self,
        decision_type: Optional[DecisionType] = None,
        limit: int = 10,
    ) -> List[MemoryEntry]:
        """
        Recall incorrect decisions for learning.

        Critical for improving decision quality over time.
        """
        query = "incorrect"
        if decision_type:
            query += f" {decision_type.value}"

        memories = self.memory.recall(
            query=query,
            memory_types=[MemoryType.SHORT_TERM, MemoryType.LONG_TERM],
            limit=limit * 2,
        )

        # Filter to actual incorrect outcomes
        incorrect = []
        for m in memories:
            ctx = m.context.get("decision", {})
            if ctx.get("outcome") == DecisionOutcome.INCORRECT.value:
                incorrect.append(m)

        return incorrect[:limit]

    def get_agent_decision_accuracy(
        self,
        agent_id: str,
        decision_type: Optional[DecisionType] = None,
    ) -> Dict[str, Any]:
        """
        Get decision accuracy statistics for an agent.

        Used for agent evaluation and improvement.
        """
        memories = self.memory.get_agent_memories(
            agent_id=agent_id,
            limit=100,
        )

        total = 0
        correct = 0
        incorrect = 0
        total_pnl = 0.0

        for m in memories:
            ctx = m.context.get("decision", {})
            if not ctx:
                continue

            if decision_type and ctx.get("type") != decision_type.value:
                continue

            total += 1
            outcome = ctx.get("outcome")

            if outcome == DecisionOutcome.CORRECT.value:
                correct += 1
            elif outcome == DecisionOutcome.INCORRECT.value:
                incorrect += 1

            pnl = ctx.get("outcome_pnl_bps")
            if pnl is not None:
                total_pnl += pnl

        return {
            "agent_id": agent_id,
            "decision_type": decision_type.value if decision_type else "all",
            "total_decisions": total,
            "correct": correct,
            "incorrect": incorrect,
            "accuracy": correct / total if total > 0 else 0,
            "total_pnl_bps": total_pnl,
        }

    def get_ic_approval_history(
        self,
        ticker: Optional[str] = None,
        limit: int = 20,
    ) -> List[MemoryEntry]:
        """Get history of IC approvals/rejections"""
        query = "ic_approval ic_rejection"
        if ticker:
            query += f" {ticker}"

        memories = self.memory.recall(
            query=query,
            memory_types=[MemoryType.LONG_TERM, MemoryType.SHORT_TERM],
            limit=limit,
        )

        # Filter to IC decisions
        ic_decisions = []
        for m in memories:
            ctx = m.context.get("decision", {})
            if ctx.get("type") in [DecisionType.IC_APPROVAL.value, DecisionType.IC_REJECTION.value]:
                ic_decisions.append(m)

        return ic_decisions[:limit]

    def get_risk_override_history(
        self,
        limit: int = 20,
    ) -> List[MemoryEntry]:
        """Get history of risk overrides"""
        return self.memory.get_memories_by_tags(
            tags=[DecisionType.RISK_OVERRIDE.value],
            memory_types=[MemoryType.LONG_TERM, MemoryType.SHORT_TERM],
        )[:limit]


# Global decision memory instance
_decision_memory: Optional[DecisionMemory] = None


def get_decision_memory() -> DecisionMemory:
    """Get the global decision memory instance"""
    global _decision_memory
    if _decision_memory is None:
        _decision_memory = DecisionMemory()
    return _decision_memory
