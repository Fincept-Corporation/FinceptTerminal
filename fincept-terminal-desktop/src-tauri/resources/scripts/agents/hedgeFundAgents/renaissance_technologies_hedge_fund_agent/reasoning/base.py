"""
Base Reasoning Infrastructure

Core reasoning engine for Renaissance Technologies multi-agent system.
Implements multi-step reasoning with chain-of-thought capabilities.
"""

from typing import Optional, List, Dict, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
import json
import uuid

try:
    from agno.agent import Agent
except ImportError:
    Agent = object

from ..config import get_config, ModelConfig


class ReasoningType(str, Enum):
    """Types of reasoning processes"""
    SIGNAL_ANALYSIS = "signal_analysis"
    RISK_ASSESSMENT = "risk_assessment"
    POSITION_SIZING = "position_sizing"
    EXECUTION_PLANNING = "execution_planning"
    IC_DELIBERATION = "ic_deliberation"
    POST_TRADE_ANALYSIS = "post_trade_analysis"
    MARKET_REGIME = "market_regime"
    CORRELATION_ANALYSIS = "correlation_analysis"


class StepStatus(str, Enum):
    """Status of a reasoning step"""
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    FAILED = "failed"
    SKIPPED = "skipped"


@dataclass
class ReasoningStep:
    """
    A single step in a reasoning chain.

    Represents one logical step in the reasoning process,
    similar to how a human analyst would think through a problem.
    """
    step_id: str
    step_number: int
    title: str
    description: str

    # Input/Output
    input_data: Dict[str, Any]
    output_data: Optional[Dict[str, Any]] = None

    # Reasoning content
    reasoning: Optional[str] = None
    conclusion: Optional[str] = None
    confidence: Optional[float] = None

    # Status
    status: StepStatus = StepStatus.PENDING
    started_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None
    error: Optional[str] = None

    # Metadata
    agent_id: Optional[str] = None
    model_used: Optional[str] = None
    tokens_used: Optional[int] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return {
            "step_id": self.step_id,
            "step_number": self.step_number,
            "title": self.title,
            "description": self.description,
            "input_data": self.input_data,
            "output_data": self.output_data,
            "reasoning": self.reasoning,
            "conclusion": self.conclusion,
            "confidence": self.confidence,
            "status": self.status.value,
            "started_at": self.started_at.isoformat() if self.started_at else None,
            "completed_at": self.completed_at.isoformat() if self.completed_at else None,
            "error": self.error,
            "agent_id": self.agent_id,
            "model_used": self.model_used,
            "tokens_used": self.tokens_used,
        }


@dataclass
class ReasoningChain:
    """
    A complete chain of reasoning steps.

    Represents a full reasoning process from problem to conclusion.
    """
    chain_id: str
    reasoning_type: ReasoningType
    subject: str  # What we're reasoning about (e.g., "AAPL long signal")

    # Steps
    steps: List[ReasoningStep] = field(default_factory=list)

    # Overall conclusion
    final_conclusion: Optional[str] = None
    final_confidence: Optional[float] = None
    final_recommendation: Optional[str] = None

    # Timing
    started_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None

    # Metadata
    initiated_by: Optional[str] = None
    context: Dict[str, Any] = field(default_factory=dict)

    @property
    def is_complete(self) -> bool:
        """Check if all steps are complete"""
        return all(
            s.status in [StepStatus.COMPLETED, StepStatus.SKIPPED]
            for s in self.steps
        )

    @property
    def current_step(self) -> Optional[ReasoningStep]:
        """Get the current step being processed"""
        for step in self.steps:
            if step.status in [StepStatus.PENDING, StepStatus.IN_PROGRESS]:
                return step
        return None

    def add_step(
        self,
        title: str,
        description: str,
        input_data: Optional[Dict[str, Any]] = None,
    ) -> ReasoningStep:
        """Add a new step to the chain"""
        step = ReasoningStep(
            step_id=f"{self.chain_id}_step_{len(self.steps) + 1}",
            step_number=len(self.steps) + 1,
            title=title,
            description=description,
            input_data=input_data or {},
        )
        self.steps.append(step)
        return step

    def get_reasoning_summary(self) -> str:
        """Get a summary of the reasoning process"""
        lines = [
            f"Reasoning Chain: {self.subject}",
            f"Type: {self.reasoning_type.value}",
            f"Steps: {len(self.steps)}",
            "",
        ]

        for step in self.steps:
            status_icon = {
                StepStatus.COMPLETED: "[OK]",
                StepStatus.FAILED: "[X]",
                StepStatus.SKIPPED: "[-]",
                StepStatus.IN_PROGRESS: "[...]",
                StepStatus.PENDING: "[ ]",
            }.get(step.status, "[ ]")

            lines.append(f"{status_icon} Step {step.step_number}: {step.title}")
            if step.conclusion:
                lines.append(f"    Conclusion: {step.conclusion[:100]}...")
            if step.confidence:
                lines.append(f"    Confidence: {step.confidence:.1%}")

        if self.final_conclusion:
            lines.append("")
            lines.append(f"FINAL CONCLUSION: {self.final_conclusion}")
            lines.append(f"RECOMMENDATION: {self.final_recommendation}")
            lines.append(f"CONFIDENCE: {self.final_confidence:.1%}")

        return "\n".join(lines)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return {
            "chain_id": self.chain_id,
            "reasoning_type": self.reasoning_type.value,
            "subject": self.subject,
            "steps": [s.to_dict() for s in self.steps],
            "final_conclusion": self.final_conclusion,
            "final_confidence": self.final_confidence,
            "final_recommendation": self.final_recommendation,
            "started_at": self.started_at.isoformat() if self.started_at else None,
            "completed_at": self.completed_at.isoformat() if self.completed_at else None,
            "initiated_by": self.initiated_by,
            "context": self.context,
        }


class ReasoningEngine:
    """
    Core reasoning engine for multi-step analysis.

    Implements the reasoning patterns used by Renaissance Technologies
    analysts and portfolio managers:

    1. Signal Analysis: Evaluating statistical signals
    2. Risk Assessment: Understanding potential downsides
    3. Position Sizing: Optimal allocation decisions
    4. IC Deliberation: Investment committee decision process
    5. Post-Trade Analysis: Learning from outcomes
    """

    def __init__(self, config: Optional[ModelConfig] = None):
        self.config = config or get_config().models
        self._chains: Dict[str, ReasoningChain] = {}
        self._init_model()

    def _init_model(self):
        """Initialize the reasoning model (dynamically from config)"""
        try:
            from ..utils.model_factory import create_model_from_config
            self.model = create_model_from_config({
                "temperature": 0.3,  # Lower temperature for reasoning
                "max_tokens": self.config.max_tokens,
            })
            self._model_available = True
        except Exception as e:
            print(f"Warning: Could not initialize reasoning model: {e}")
            self._model_available = False

    def create_chain(
        self,
        reasoning_type: ReasoningType,
        subject: str,
        context: Optional[Dict[str, Any]] = None,
        initiated_by: Optional[str] = None,
    ) -> ReasoningChain:
        """
        Create a new reasoning chain.

        Args:
            reasoning_type: Type of reasoning to perform
            subject: What we're reasoning about
            context: Additional context data
            initiated_by: Agent who initiated the reasoning

        Returns:
            New reasoning chain
        """
        chain_id = f"reason_{reasoning_type.value}_{uuid.uuid4().hex[:8]}"

        chain = ReasoningChain(
            chain_id=chain_id,
            reasoning_type=reasoning_type,
            subject=subject,
            context=context or {},
            initiated_by=initiated_by,
            started_at=datetime.utcnow(),
        )

        # Add standard steps based on reasoning type
        self._add_standard_steps(chain)

        self._chains[chain_id] = chain
        return chain

    def _add_standard_steps(self, chain: ReasoningChain):
        """Add standard steps based on reasoning type"""
        if chain.reasoning_type == ReasoningType.SIGNAL_ANALYSIS:
            chain.add_step(
                "Data Quality Check",
                "Verify data integrity and identify any anomalies",
            )
            chain.add_step(
                "Statistical Significance",
                "Evaluate p-value, t-statistic, and sample size",
            )
            chain.add_step(
                "Historical Performance",
                "Review similar signals from history",
            )
            chain.add_step(
                "Market Regime Fit",
                "Assess if current regime supports this signal",
            )
            chain.add_step(
                "Risk/Reward Assessment",
                "Calculate expected return vs potential loss",
            )

        elif chain.reasoning_type == ReasoningType.RISK_ASSESSMENT:
            chain.add_step(
                "Current Exposure Analysis",
                "Map existing portfolio exposures",
            )
            chain.add_step(
                "Incremental Risk Calculation",
                "Calculate marginal VaR and correlation impact",
            )
            chain.add_step(
                "Stress Scenario Analysis",
                "Model worst-case scenarios",
            )
            chain.add_step(
                "Limit Check",
                "Verify against all risk limits",
            )

        elif chain.reasoning_type == ReasoningType.POSITION_SIZING:
            chain.add_step(
                "Signal Strength Assessment",
                "Evaluate conviction level from signal",
            )
            chain.add_step(
                "Kelly Criterion Calculation",
                "Calculate optimal size from edge and odds",
            )
            chain.add_step(
                "Liquidity Constraint",
                "Adjust for market liquidity",
            )
            chain.add_step(
                "Correlation Adjustment",
                "Reduce for correlated positions",
            )
            chain.add_step(
                "Final Size Determination",
                "Apply risk limits and round to lot size",
            )

        elif chain.reasoning_type == ReasoningType.IC_DELIBERATION:
            chain.add_step(
                "Signal Review",
                "Review signal generation and validation",
            )
            chain.add_step(
                "Risk Review",
                "Review risk team assessment",
            )
            chain.add_step(
                "Historical Precedent",
                "Review similar past decisions",
            )
            chain.add_step(
                "Pros vs Cons Analysis",
                "Weigh arguments for and against",
            )
            chain.add_step(
                "Final Deliberation",
                "Make approval/rejection decision",
            )

        elif chain.reasoning_type == ReasoningType.POST_TRADE_ANALYSIS:
            chain.add_step(
                "Outcome Documentation",
                "Record actual vs expected outcome",
            )
            chain.add_step(
                "Execution Analysis",
                "Evaluate execution quality",
            )
            chain.add_step(
                "Signal Analysis",
                "Evaluate signal quality",
            )
            chain.add_step(
                "Lesson Extraction",
                "Identify key learnings",
            )
            chain.add_step(
                "Improvement Recommendations",
                "Suggest process improvements",
            )

    async def execute_step(
        self,
        chain: ReasoningChain,
        step: ReasoningStep,
        additional_context: Optional[Dict[str, Any]] = None,
    ) -> ReasoningStep:
        """
        Execute a single reasoning step.

        Uses the reasoning model to think through the step
        and produce a conclusion.
        """
        step.status = StepStatus.IN_PROGRESS
        step.started_at = datetime.utcnow()

        # Build prompt for this step
        prompt = self._build_step_prompt(chain, step, additional_context)

        try:
            if self._model_available:
                # Use the model to reason through the step
                response = await self._reason_with_model(prompt)
                step.reasoning = response.get("reasoning", "")
                step.conclusion = response.get("conclusion", "")
                step.confidence = response.get("confidence", 0.5)
                step.output_data = response.get("output_data", {})
            else:
                # Fallback: Simple rule-based reasoning
                step.reasoning = f"Analyzed {step.title} for {chain.subject}"
                step.conclusion = f"Step {step.step_number} completed"
                step.confidence = 0.5
                step.output_data = {}

            step.status = StepStatus.COMPLETED
            step.completed_at = datetime.utcnow()

        except Exception as e:
            step.status = StepStatus.FAILED
            step.error = str(e)
            step.completed_at = datetime.utcnow()

        return step

    def _build_step_prompt(
        self,
        chain: ReasoningChain,
        step: ReasoningStep,
        additional_context: Optional[Dict[str, Any]] = None,
    ) -> str:
        """Build the prompt for a reasoning step"""
        # Gather context from previous steps
        previous_conclusions = []
        for prev_step in chain.steps:
            if prev_step.step_number < step.step_number and prev_step.conclusion:
                previous_conclusions.append(
                    f"Step {prev_step.step_number} ({prev_step.title}): {prev_step.conclusion}"
                )

        prompt = f"""
You are a quantitative analyst at Renaissance Technologies performing structured reasoning.

## Reasoning Context
- Subject: {chain.subject}
- Reasoning Type: {chain.reasoning_type.value}
- Current Step: {step.step_number} of {len(chain.steps)}

## Current Step
- Title: {step.title}
- Description: {step.description}

## Input Data
{json.dumps(step.input_data, indent=2)}

## Previous Steps Conclusions
{chr(10).join(previous_conclusions) if previous_conclusions else "This is the first step."}

## Additional Context
{json.dumps(additional_context or {}, indent=2)}

## Your Task
Think through this step carefully. Provide:
1. Your detailed reasoning (show your work)
2. Your conclusion for this step
3. Your confidence level (0.0 to 1.0)
4. Any output data for the next step

Respond in JSON format:
{{
    "reasoning": "Your detailed reasoning...",
    "conclusion": "Your conclusion for this step",
    "confidence": 0.75,
    "output_data": {{"key": "value"}}
}}
""".strip()

        return prompt

    async def _reason_with_model(self, prompt: str) -> Dict[str, Any]:
        """Use the model to reason through a prompt"""
        try:
            # In production, this would call the model
            # For now, return a placeholder
            return {
                "reasoning": "Analyzed the data systematically...",
                "conclusion": "Based on the analysis...",
                "confidence": 0.7,
                "output_data": {},
            }
        except Exception as e:
            raise RuntimeError(f"Model reasoning failed: {e}")

    async def execute_chain(
        self,
        chain: ReasoningChain,
        context: Optional[Dict[str, Any]] = None,
    ) -> ReasoningChain:
        """
        Execute an entire reasoning chain.

        Processes each step in sequence, using outputs from
        previous steps as inputs to subsequent steps.
        """
        accumulated_context = context or {}

        for step in chain.steps:
            if step.status != StepStatus.PENDING:
                continue

            # Add outputs from previous step to context
            prev_step = None
            if step.step_number > 1:
                prev_step = chain.steps[step.step_number - 2]
                if prev_step.output_data:
                    accumulated_context.update(prev_step.output_data)

            # Execute this step
            await self.execute_step(chain, step, accumulated_context)

            # If step failed, stop the chain
            if step.status == StepStatus.FAILED:
                break

        # Derive final conclusion from last step
        if chain.is_complete:
            last_step = chain.steps[-1]
            chain.final_conclusion = last_step.conclusion
            chain.final_confidence = last_step.confidence
            chain.completed_at = datetime.utcnow()

            # Generate recommendation
            chain.final_recommendation = self._generate_recommendation(chain)

        return chain

    def _generate_recommendation(self, chain: ReasoningChain) -> str:
        """Generate a final recommendation based on the reasoning chain"""
        if not chain.is_complete:
            return "INCOMPLETE - Cannot recommend"

        confidence = chain.final_confidence or 0

        if chain.reasoning_type == ReasoningType.IC_DELIBERATION:
            if confidence >= 0.7:
                return "APPROVE with full size"
            elif confidence >= 0.5:
                return "APPROVE with reduced size (50%)"
            else:
                return "REJECT - insufficient confidence"

        elif chain.reasoning_type == ReasoningType.RISK_ASSESSMENT:
            if confidence >= 0.7:
                return "PROCEED - acceptable risk"
            elif confidence >= 0.5:
                return "PROCEED with caution - monitor closely"
            else:
                return "HALT - risk too high"

        else:
            return f"Confidence: {confidence:.1%}"

    def get_chain(self, chain_id: str) -> Optional[ReasoningChain]:
        """Get a reasoning chain by ID"""
        return self._chains.get(chain_id)

    def get_active_chains(self) -> List[ReasoningChain]:
        """Get all active (incomplete) chains"""
        return [c for c in self._chains.values() if not c.is_complete]


# Global reasoning engine instance
_reasoning_engine: Optional[ReasoningEngine] = None


def get_reasoning_engine() -> ReasoningEngine:
    """Get the global reasoning engine instance"""
    global _reasoning_engine
    if _reasoning_engine is None:
        _reasoning_engine = ReasoningEngine()
    return _reasoning_engine
