"""
Investment Committee Deliberation

Structured reasoning for IC decisions:
- Multi-stakeholder deliberation
- Pros/cons analysis
- Historical precedent review
- Final approval/rejection decision
"""

from typing import Optional, List, Dict, Any
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum

from .base import (
    ReasoningEngine,
    ReasoningChain,
    ReasoningType,
    StepStatus,
    get_reasoning_engine,
)
from ..config import get_config


class ICVote(str, Enum):
    """IC member votes"""
    APPROVE = "approve"
    REJECT = "reject"
    ABSTAIN = "abstain"
    CONDITIONAL = "conditional"


@dataclass
class ICMemberOpinion:
    """Opinion from an IC member"""
    member_role: str
    vote: ICVote
    rationale: str
    conditions: List[str] = field(default_factory=list)
    confidence: float = 0.5


@dataclass
class ICDeliberationResult:
    """Result of IC deliberation"""
    deliberation_id: str
    subject: str
    timestamp: str

    # Signal summary
    signal_summary: str
    statistical_strength: str
    expected_return_bps: float
    risk_assessment: str

    # Deliberation
    pros: List[str]
    cons: List[str]
    historical_precedents: List[Dict[str, Any]]

    # Votes
    member_opinions: List[ICMemberOpinion]
    vote_summary: Dict[str, int]  # approve/reject/abstain counts

    # Decision
    decision: str  # APPROVE, REJECT, CONDITIONAL
    decision_rationale: str
    conditions: List[str]  # Conditions if approved conditionally

    # Sizing
    approved_size_pct: Optional[float]
    size_rationale: str

    # Confidence
    confidence: float
    reasoning_chain_summary: str


class ICDeliberator:
    """
    Investment Committee deliberation reasoning system.

    Simulates the IC decision-making process:
    1. Signal review (Research Lead presents)
    2. Risk review (Risk team presents)
    3. Historical precedent analysis
    4. Pros vs cons deliberation
    5. Member opinions and votes
    6. Final decision and conditions
    """

    def __init__(self, reasoning_engine: Optional[ReasoningEngine] = None):
        self.engine = reasoning_engine or get_reasoning_engine()
        self.config = get_config()

        # IC members and their roles/perspectives
        self.ic_members = [
            {
                "role": "Chief Investment Officer",
                "perspective": "overall portfolio fit and strategy alignment",
                "weight": 2.0,
            },
            {
                "role": "Research Lead",
                "perspective": "signal quality and statistical validity",
                "weight": 1.5,
            },
            {
                "role": "Chief Risk Officer",
                "perspective": "risk management and limit compliance",
                "weight": 1.5,
            },
            {
                "role": "Head of Trading",
                "perspective": "execution feasibility and market impact",
                "weight": 1.0,
            },
            {
                "role": "Portfolio Manager",
                "perspective": "portfolio construction and correlation",
                "weight": 1.0,
            },
        ]

    async def deliberate(
        self,
        signal: Dict[str, Any],
        signal_evaluation: Dict[str, Any],
        risk_assessment: Dict[str, Any],
        sizing_recommendation: Dict[str, Any],
        market_context: Dict[str, Any],
        historical_decisions: Optional[List[Dict[str, Any]]] = None,
    ) -> ICDeliberationResult:
        """
        Perform full IC deliberation on a proposed trade.
        """
        import logging
        logger = logging.getLogger(__name__)

        # Log LLM configuration being used
        logger.info(f"=== IC DELIBERATION STARTING ===")
        logger.info(f"Model Provider: {self.config.models.provider.value}")
        logger.info(f"Model ID: {self.config.models.model_id}")
        logger.info(f"Temperature: {self.config.models.temperature}")

        ticker = signal.get("ticker", "UNKNOWN")
        direction = signal.get("direction", "long")
        signal_type = signal.get("signal_type", "unknown")

        # Create reasoning chain
        chain = self.engine.create_chain(
            reasoning_type=ReasoningType.IC_DELIBERATION,
            subject=f"IC Decision: {ticker} {direction} {signal_type}",
            context={
                "signal": signal,
                "evaluation": signal_evaluation,
                "risk": risk_assessment,
                "sizing": sizing_recommendation,
                "market": market_context,
                "history": historical_decisions or [],
            },
        )

        # Execute reasoning chain
        await self.engine.execute_chain(chain)

        # Generate signal summary
        signal_summary = self._generate_signal_summary(signal, signal_evaluation)

        # Generate pros and cons
        pros, cons = self._generate_pros_cons(
            signal, signal_evaluation, risk_assessment, market_context
        )

        # Find historical precedents
        precedents = self._find_precedents(
            signal, historical_decisions or []
        )

        # Gather member opinions
        member_opinions = await self._gather_member_opinions(
            signal, signal_evaluation, risk_assessment, sizing_recommendation,
            pros, cons, precedents
        )

        # Tally votes
        vote_summary = self._tally_votes(member_opinions)

        # Make final decision
        decision, rationale, conditions = self._make_decision(
            vote_summary, member_opinions, risk_assessment
        )

        # Determine approved size
        approved_size, size_rationale = self._determine_approved_size(
            decision, conditions, sizing_recommendation, risk_assessment
        )

        # Build reasoning summary
        reasoning_summary = f"""
IC DELIBERATION: {ticker} {direction.upper()}

SIGNAL SUMMARY:
{signal_summary}

PROS:
{chr(10).join([f"+ {p}" for p in pros])}

CONS:
{chr(10).join([f"- {c}" for c in cons])}

HISTORICAL PRECEDENTS:
{chr(10).join([f"- {p.get('subject', 'N/A')}: {p.get('outcome', 'N/A')}" for p in precedents[:3]])}

MEMBER OPINIONS:
{chr(10).join([f"- {m.member_role}: {m.vote.value.upper()} ({m.confidence:.0%})" for m in member_opinions])}

VOTE TALLY: {vote_summary}

DECISION: {decision}
RATIONALE: {rationale}
CONDITIONS: {', '.join(conditions) if conditions else 'None'}
APPROVED SIZE: {approved_size or 0:.2f}%

CONFIDENCE: {chain.final_confidence or 0.5:.1%}
""".strip()

        return ICDeliberationResult(
            deliberation_id=f"ic_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}",
            subject=f"{ticker} {direction} {signal_type}",
            timestamp=datetime.utcnow().isoformat(),
            signal_summary=signal_summary,
            statistical_strength=signal_evaluation.get("statistical_quality", "unknown"),
            expected_return_bps=signal.get("expected_return_bps", 0),
            risk_assessment=risk_assessment.get("risk_level", "unknown"),
            pros=pros,
            cons=cons,
            historical_precedents=precedents,
            member_opinions=member_opinions,
            vote_summary=vote_summary,
            decision=decision,
            decision_rationale=rationale,
            conditions=conditions,
            approved_size_pct=approved_size,
            size_rationale=size_rationale,
            confidence=chain.final_confidence or 0.5,
            reasoning_chain_summary=reasoning_summary,
        )

    def _generate_signal_summary(
        self,
        signal: Dict[str, Any],
        evaluation: Dict[str, Any],
    ) -> str:
        """Generate a summary of the signal for IC review"""
        return f"""
Ticker: {signal.get('ticker', 'N/A')}
Direction: {signal.get('direction', 'N/A').upper()}
Signal Type: {signal.get('signal_type', 'N/A')}
P-value: {signal.get('p_value', 1.0):.4f}
Confidence: {signal.get('confidence', 0):.1%}
Expected Return: {signal.get('expected_return_bps', 0):.0f} bps
Statistical Quality: {evaluation.get('statistical_quality', 'unknown')}
Overall Score: {evaluation.get('overall_score', 0):.0f}/100
""".strip()

    def _generate_pros_cons(
        self,
        signal: Dict[str, Any],
        evaluation: Dict[str, Any],
        risk: Dict[str, Any],
        market: Dict[str, Any],
    ) -> tuple:
        """Generate pros and cons for the trade"""
        pros = []
        cons = []

        # Statistical quality
        p_value = signal.get("p_value", 1.0)
        if p_value < 0.01:
            pros.append(f"Strong statistical significance (p={p_value:.4f})")
        elif p_value < 0.02:
            cons.append(f"Borderline statistical significance (p={p_value:.4f})")
        else:
            cons.append(f"Weak statistical significance (p={p_value:.4f})")

        # Confidence
        confidence = signal.get("confidence", 0.5)
        if confidence > 0.55:
            pros.append(f"High signal confidence ({confidence:.1%})")
        elif confidence < 0.52:
            cons.append(f"Low signal confidence ({confidence:.1%})")

        # Expected return
        expected_bps = signal.get("expected_return_bps", 0)
        if expected_bps > 100:
            pros.append(f"Attractive expected return ({expected_bps:.0f} bps)")
        elif expected_bps < 30:
            cons.append(f"Low expected return ({expected_bps:.0f} bps)")

        # Risk assessment
        risk_level = risk.get("risk_level", "medium")
        if risk_level == "low":
            pros.append("Low risk profile")
        elif risk_level in ["high", "critical"]:
            cons.append(f"Elevated risk level: {risk_level}")

        # Market regime
        regime = market.get("regime", "unknown")
        signal_type = signal.get("signal_type", "")
        if signal_type == "mean_reversion" and market.get("vix", 20) < 18:
            pros.append("Favorable low-vol regime for mean reversion")
        elif signal_type == "mean_reversion" and market.get("vix", 20) > 25:
            cons.append("High-vol regime unfavorable for mean reversion")

        # Liquidity
        if signal.get("average_daily_volume", 0) > 5000000:
            pros.append("High liquidity, easy to execute")
        elif signal.get("average_daily_volume", 0) < 1000000:
            cons.append("Low liquidity, execution risk")

        # VaR impact
        var_util = risk.get("var_utilization_pct", 50)
        if var_util < 50:
            pros.append(f"Moderate VaR utilization ({var_util:.0f}%)")
        elif var_util > 80:
            cons.append(f"High VaR utilization ({var_util:.0f}%)")

        return pros, cons

    def _find_precedents(
        self,
        signal: Dict[str, Any],
        historical_decisions: List[Dict[str, Any]],
    ) -> List[Dict[str, Any]]:
        """Find historical precedents for similar decisions"""
        ticker = signal.get("ticker")
        signal_type = signal.get("signal_type")

        precedents = []
        for decision in historical_decisions:
            # Match on ticker or signal type
            if (decision.get("ticker") == ticker or
                decision.get("signal_type") == signal_type):
                precedents.append({
                    "subject": decision.get("subject", "Unknown"),
                    "decision": decision.get("decision", "Unknown"),
                    "outcome": decision.get("outcome", "Unknown"),
                    "pnl_bps": decision.get("pnl_bps", None),
                    "date": decision.get("date", "Unknown"),
                })

        return precedents[:5]  # Top 5 precedents

    async def _gather_member_opinions(
        self,
        signal: Dict[str, Any],
        evaluation: Dict[str, Any],
        risk: Dict[str, Any],
        sizing: Dict[str, Any],
        pros: List[str],
        cons: List[str],
        precedents: List[Dict[str, Any]],
    ) -> List[ICMemberOpinion]:
        """Gather opinions from each IC member using REAL Agno agents"""
        import logging
        from ..agents import (
            create_investment_committee_chair,
            create_research_lead,
            create_risk_quant,
            create_execution_trader,
            create_portfolio_manager,
        )

        logger = logging.getLogger(__name__)
        opinions = []

        # Create actual agents for each IC member role
        agent_creators = {
            "Chief Investment Officer": create_investment_committee_chair,
            "Research Lead": create_research_lead,
            "Chief Risk Officer": create_risk_quant,
            "Head of Trading": create_execution_trader,
            "Portfolio Manager": create_portfolio_manager,
        }

        for member in self.ic_members:
            role = member["role"]
            perspective = member["perspective"]

            # Get the agent creator
            creator = agent_creators.get(role)
            if not creator:
                continue

            # Create agent
            logger.info(f"Creating agent for {role} using {self.config.models.provider.value}/{self.config.models.model_id}")
            agent = creator(self.config)

            # Build deliberation prompt
            prompt = self._build_deliberation_prompt(
                role, perspective, signal, evaluation, risk, sizing, pros, cons, precedents
            )

            # Get agent's opinion via LLM
            opinion = await self._get_agent_opinion(agent, role, prompt, member["weight"])
            opinions.append(opinion)

        return opinions

    def _build_deliberation_prompt(
        self,
        role: str,
        perspective: str,
        signal: Dict[str, Any],
        evaluation: Dict[str, Any],
        risk: Dict[str, Any],
        sizing: Dict[str, Any],
        pros: List[str],
        cons: List[str],
        precedents: List[Dict[str, Any]],
    ) -> str:
        """Build deliberation prompt for agent"""
        ticker = signal.get("ticker", "N/A")
        direction = signal.get("direction", "N/A")
        signal_type = signal.get("signal_type", "N/A")

        precedent_text = "\n".join([
            f"  - {p.get('subject', 'N/A')}: {p.get('decision', 'N/A')} → {p.get('outcome', 'N/A')}"
            for p in precedents[:3]
        ]) if precedents else "  - No similar precedents found"

        return f"""# Investment Committee Deliberation

You are participating in an IC meeting to decide on a proposed trade.

**Your Role**: {role}
**Your Focus**: {perspective}

## Proposed Trade

- **Ticker**: {ticker}
- **Direction**: {direction.upper()}
- **Signal Type**: {signal_type}
- **P-value**: {signal.get('p_value', 1.0):.4f}
- **Confidence**: {signal.get('confidence', 0):.1%}
- **Expected Return**: {signal.get('expected_return_bps', 0):.0f} bps

## Analysis Summary

**Overall Score**: {evaluation.get('overall_score', 0)}/100
**Statistical Quality**: {evaluation.get('statistical_quality', 'unknown')}

**Risk Assessment**:
- Risk Level: {risk.get('risk_level', 'unknown')}
- VaR Utilization: {risk.get('var_utilization_pct', 0):.0f}%
- Risk Flags: {', '.join(risk.get('risk_flags', [])) or 'None'}
- Sector Concentration: {risk.get('sector_concentrations', {})}

**Proposed Size**: {sizing.get('final_size_pct', 0):.2f}% of portfolio

## Pros
{chr(10).join([f'+ {p}' for p in pros]) or '+ None identified'}

## Cons
{chr(10).join([f'- {c}' for c in cons]) or '- None identified'}

## Historical Precedents
{precedent_text}

## Your Task

As the **{role}**, provide your vote and detailed rationale.

**Respond EXACTLY in this format**:

VOTE: [APPROVE/REJECT/CONDITIONAL/ABSTAIN]
CONFIDENCE: [0-100]%
RATIONALE:
[Your detailed reasoning from your perspective as {role}]

CONDITIONS (if CONDITIONAL):
- [List specific conditions, or write "None" if APPROVE/REJECT]
"""

    async def _get_agent_opinion(
        self,
        agent,
        role: str,
        prompt: str,
        weight: float,
    ) -> ICMemberOpinion:
        """Get opinion from agent via LLM"""
        import re

        try:
            # Run agent with deliberation prompt
            response = agent.run(prompt)
            response_text = response.content if hasattr(response, 'content') else str(response)

            # Parse response
            vote_match = re.search(r'VOTE:\s*(APPROVE|REJECT|CONDITIONAL|ABSTAIN)', response_text, re.IGNORECASE)
            conf_match = re.search(r'CONFIDENCE:\s*(\d+)%?', response_text)
            rationale_match = re.search(r'RATIONALE:\s*(.+?)(?=CONDITIONS:|$)', response_text, re.DOTALL | re.IGNORECASE)
            conditions_match = re.search(r'CONDITIONS.*?:\s*(.+?)$', response_text, re.DOTALL | re.IGNORECASE)

            # Extract values
            vote_str = vote_match.group(1).upper() if vote_match else "ABSTAIN"
            confidence = float(conf_match.group(1)) / 100 if conf_match else 0.5
            rationale = rationale_match.group(1).strip() if rationale_match else "No rationale provided"
            conditions_text = conditions_match.group(1).strip() if conditions_match else ""

            # Parse vote
            vote = ICVote.APPROVE if vote_str == "APPROVE" else \
                   ICVote.REJECT if vote_str == "REJECT" else \
                   ICVote.CONDITIONAL if vote_str == "CONDITIONAL" else \
                   ICVote.ABSTAIN

            # Parse conditions
            conditions = []
            if vote == ICVote.CONDITIONAL and conditions_text and "none" not in conditions_text.lower():
                conditions = [c.strip().lstrip('-•').strip() for c in conditions_text.split('\n') if c.strip()]

            return ICMemberOpinion(
                member_role=role,
                vote=vote,
                rationale=rationale[:500],  # Truncate if too long
                conditions=conditions,
                confidence=confidence,
            )

        except Exception as e:
            # Fallback if agent fails
            return ICMemberOpinion(
                member_role=role,
                vote=ICVote.ABSTAIN,
                rationale=f"Error getting opinion: {str(e)}",
                conditions=[],
                confidence=0.5,
            )

    def _tally_votes(
        self,
        opinions: List[ICMemberOpinion],
    ) -> Dict[str, int]:
        """Tally the votes from IC members"""
        tally = {
            "approve": 0,
            "reject": 0,
            "conditional": 0,
            "abstain": 0,
        }

        for opinion in opinions:
            tally[opinion.vote.value] += 1

        return tally

    def _make_decision(
        self,
        vote_summary: Dict[str, int],
        opinions: List[ICMemberOpinion],
        risk: Dict[str, Any],
    ) -> tuple:
        """Make final IC decision based on votes and risk"""
        approve = vote_summary["approve"]
        reject = vote_summary["reject"]
        conditional = vote_summary["conditional"]
        total = approve + reject + conditional

        # Risk-based override
        if risk.get("risk_level") == "critical":
            return "REJECT", "Risk limit breach - automatic rejection", []

        # Unanimous or strong approval
        if approve >= 4:
            return "APPROVE", "Strong consensus to approve", []

        # Majority approval + conditionals
        if approve + conditional >= 4 and reject <= 1:
            all_conditions = []
            for op in opinions:
                if op.vote == ICVote.CONDITIONAL:
                    all_conditions.extend(op.conditions)
            return "CONDITIONAL", "Approved with conditions", list(set(all_conditions))

        # Strong rejection
        if reject >= 3:
            return "REJECT", "Majority voted to reject", []

        # Mixed - lean towards caution
        if conditional >= 2:
            all_conditions = []
            for op in opinions:
                if op.vote == ICVote.CONDITIONAL:
                    all_conditions.extend(op.conditions)
            return "CONDITIONAL", "Mixed votes, proceeding with caution", list(set(all_conditions))

        return "REJECT", "Insufficient support for approval", []

    def _determine_approved_size(
        self,
        decision: str,
        conditions: List[str],
        sizing: Dict[str, Any],
        risk: Dict[str, Any],
    ) -> tuple:
        """Determine the approved position size"""
        if decision == "REJECT":
            return None, "Trade rejected, no size approved"

        base_size = sizing.get("final_size_pct", 1.0)

        if decision == "CONDITIONAL":
            # Apply size reduction based on conditions
            reduction = 1.0
            if any("reduce" in c.lower() for c in conditions):
                reduction = 0.5
            elif any("caution" in c.lower() for c in conditions):
                reduction = 0.7

            approved_size = base_size * reduction
            rationale = f"Size reduced from {base_size:.2f}% to {approved_size:.2f}% due to conditions"
        else:
            approved_size = base_size
            rationale = f"Full recommended size approved: {approved_size:.2f}%"

        return approved_size, rationale


# Global IC deliberator instance
_ic_deliberator: Optional[ICDeliberator] = None


def get_ic_deliberator() -> ICDeliberator:
    """Get the global IC deliberator instance"""
    global _ic_deliberator
    if _ic_deliberator is None:
        _ic_deliberator = ICDeliberator()
    return _ic_deliberator
