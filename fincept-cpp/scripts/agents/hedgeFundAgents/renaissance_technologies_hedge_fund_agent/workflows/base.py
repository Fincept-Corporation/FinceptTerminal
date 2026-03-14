"""
Base Workflow Infrastructure

Provides base classes and utilities for all RenTech workflows.
Uses Agno's Workflow system for orchestration.
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Callable
from enum import Enum
from datetime import datetime
from pydantic import BaseModel, Field


class WorkflowStatus(str, Enum):
    """Status of a workflow execution"""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


class StepStatus(str, Enum):
    """Status of a workflow step"""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    SKIPPED = "skipped"


# =============================================================================
# WORKFLOW DATA MODELS
# =============================================================================

class SignalData(BaseModel):
    """Data model for trading signals"""
    signal_id: str = Field(..., description="Unique signal identifier")
    ticker: str = Field(..., description="Stock ticker symbol")
    signal_type: str = Field(..., description="Type of signal (momentum, mean_reversion, etc.)")
    direction: str = Field(..., description="Trade direction (long/short)")
    strength: float = Field(..., ge=-1.0, le=1.0, description="Signal strength (-1 to 1)")
    confidence: float = Field(..., ge=0.0, le=1.0, description="Confidence level (0 to 1)")

    # Statistical metrics
    p_value: Optional[float] = Field(None, description="Statistical significance")
    t_statistic: Optional[float] = Field(None, description="T-statistic")
    information_coefficient: Optional[float] = Field(None, description="IC score")

    # Expected outcomes
    expected_return_bps: Optional[float] = Field(None, description="Expected return in bps")
    expected_holding_period: Optional[int] = Field(None, description="Holding period in days")

    # Metadata
    discovered_at: str = Field(default_factory=lambda: datetime.utcnow().isoformat())
    discovered_by: Optional[str] = Field(None, description="Agent that discovered the signal")

    # Status
    status: str = Field(default="discovered", description="Signal status")

    class Config:
        extra = "allow"


class RiskAssessment(BaseModel):
    """Data model for risk assessment results"""
    signal_id: str

    # VaR metrics
    marginal_var_pct: float = Field(..., description="Marginal VaR contribution")
    portfolio_var_after: float = Field(..., description="Portfolio VaR after trade")

    # Exposure metrics
    position_size_pct: float = Field(..., description="Position size as % of NAV")
    sector_exposure_after: float = Field(..., description="Sector exposure after trade")

    # Risk limits
    within_position_limit: bool = True
    within_sector_limit: bool = True
    within_var_limit: bool = True
    within_leverage_limit: bool = True

    # Stress test results
    stress_loss_worst_case: float = Field(..., description="Worst case stress loss")

    # Overall assessment
    risk_score: float = Field(..., ge=0.0, le=1.0, description="Overall risk score (0=low, 1=high)")
    approved: bool = Field(..., description="Whether risk team approves")
    conditions: List[str] = Field(default_factory=list, description="Conditions for approval")

    assessed_at: str = Field(default_factory=lambda: datetime.utcnow().isoformat())
    assessed_by: str = Field(default="risk_team")


class ExecutionPlan(BaseModel):
    """Data model for execution planning"""
    signal_id: str
    ticker: str
    direction: str

    # Sizing
    target_quantity: int = Field(..., description="Target number of shares")
    target_notional: float = Field(..., description="Target notional value")

    # Execution strategy
    algorithm: str = Field(..., description="Execution algorithm")
    urgency: str = Field(default="normal", description="Execution urgency")
    participation_rate: float = Field(..., description="Target participation rate")

    # Timing
    start_time: Optional[str] = None
    end_time: Optional[str] = None
    duration_minutes: int = Field(..., description="Expected duration")

    # Cost estimates
    expected_cost_bps: float = Field(..., description="Expected execution cost")
    spread_cost_bps: float
    impact_cost_bps: float

    # Venue routing
    primary_venue: str
    venue_allocation: Dict[str, float] = Field(default_factory=dict)

    planned_at: str = Field(default_factory=lambda: datetime.utcnow().isoformat())
    planned_by: str = Field(default="execution_trader")


class ExecutionResult(BaseModel):
    """Data model for execution results"""
    signal_id: str
    execution_id: str

    # Fill details
    filled_quantity: int
    average_price: float
    fill_rate: float

    # Benchmarks
    arrival_price: float
    vwap: float
    twap: float

    # Performance
    vs_arrival_bps: float
    vs_vwap_bps: float
    implementation_shortfall_bps: float

    # Actual costs
    actual_cost_bps: float
    spread_paid_bps: float
    market_impact_bps: float

    # Quality metrics
    alpha_preserved_pct: float
    execution_quality_score: float

    executed_at: str = Field(default_factory=lambda: datetime.utcnow().isoformat())
    executed_by: str = Field(default="execution_trader")


class TradeDecision(BaseModel):
    """Final trade decision from Investment Committee"""
    signal_id: str

    decision: str = Field(..., description="APPROVED, REJECTED, or PENDING")
    rationale: List[str] = Field(default_factory=list)
    conditions: List[str] = Field(default_factory=list)

    # Approved sizing
    approved_quantity: Optional[int] = None
    approved_notional: Optional[float] = None

    # Risk limits for this trade
    max_loss_pct: Optional[float] = None
    stop_loss_price: Optional[float] = None

    decided_at: str = Field(default_factory=lambda: datetime.utcnow().isoformat())
    decided_by: str = Field(default="investment_committee_chair")


# =============================================================================
# WORKFLOW STEP RESULTS
# =============================================================================

@dataclass
class StepResult:
    """Result from a workflow step"""
    step_name: str
    status: StepStatus
    agent: str
    started_at: str
    completed_at: str
    duration_seconds: float
    output: Any
    error: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "step_name": self.step_name,
            "status": self.status.value,
            "agent": self.agent,
            "started_at": self.started_at,
            "completed_at": self.completed_at,
            "duration_seconds": self.duration_seconds,
            "output": self.output,
            "error": self.error,
        }


@dataclass
class WorkflowResult:
    """Result from a complete workflow"""
    workflow_name: str
    workflow_id: str
    status: WorkflowStatus
    started_at: str
    completed_at: str
    duration_seconds: float
    steps: List[StepResult]
    final_output: Any
    error: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "workflow_name": self.workflow_name,
            "workflow_id": self.workflow_id,
            "status": self.status.value,
            "started_at": self.started_at,
            "completed_at": self.completed_at,
            "duration_seconds": self.duration_seconds,
            "steps": [s.to_dict() for s in self.steps],
            "final_output": self.final_output,
            "error": self.error,
        }


# =============================================================================
# WORKFLOW CONTEXT
# =============================================================================

@dataclass
class WorkflowContext:
    """
    Context passed through workflow steps.
    Accumulates data as the workflow progresses.
    """
    workflow_id: str
    workflow_name: str
    started_at: str = field(default_factory=lambda: datetime.utcnow().isoformat())

    # Input data
    ticker: str = ""
    signal_type: str = ""

    # Accumulated results
    signal_data: Optional[SignalData] = None
    risk_assessment: Optional[RiskAssessment] = None
    execution_plan: Optional[ExecutionPlan] = None
    execution_result: Optional[ExecutionResult] = None
    trade_decision: Optional[TradeDecision] = None

    # Step history
    step_results: List[StepResult] = field(default_factory=list)

    # Metadata
    metadata: Dict[str, Any] = field(default_factory=dict)

    def add_step_result(self, result: StepResult):
        """Add a step result to history"""
        self.step_results.append(result)

    def get_summary(self) -> str:
        """Get a summary of the workflow context"""
        steps_summary = "\n".join([
            f"  - {s.step_name}: {s.status.value}"
            for s in self.step_results
        ])
        return f"""
Workflow: {self.workflow_name} ({self.workflow_id})
Started: {self.started_at}
Ticker: {self.ticker}
Signal Type: {self.signal_type}

Steps:
{steps_summary}

Current State:
- Signal: {'Discovered' if self.signal_data else 'None'}
- Risk: {'Assessed' if self.risk_assessment else 'None'}
- Decision: {self.trade_decision.decision if self.trade_decision else 'Pending'}
- Execution: {'Complete' if self.execution_result else 'Pending'}
"""
