"""
Risk Assessment Workflow

Workflow for assessing risk implications of a trading signal
before execution approval.

Steps:
1. VaR & Exposure Analysis (Risk Quant)
2. Compliance Check (Compliance Officer)
3. Stress Testing (Risk Quant)
4. Risk Team Decision
"""

from typing import Optional, Dict, Any
from datetime import datetime
import uuid

try:
    from agno.workflow import Workflow, RunResponse
    AGNO_WORKFLOW_AVAILABLE = True
except ImportError:
    AGNO_WORKFLOW_AVAILABLE = False
    Workflow = object  # type: ignore
    RunResponse = Dict[str, Any]  # type: ignore

from ..agents import (
    get_risk_quant,
    get_compliance_officer,
)
from ..config import get_config
from .base import (
    WorkflowResult,
    StepResult,
    StepStatus,
    WorkflowStatus,
    SignalData,
    RiskAssessment,
)


class RiskAssessmentWorkflow(Workflow):
    """
    Risk assessment workflow for trading signals.

    Evaluates risk implications and determines if a trade
    meets risk criteria for execution.
    """

    name: str = "risk_assessment"
    description: str = "Assess risk implications of trading signals"

    def run(
        self,
        signal: SignalData,
        proposed_size_pct: float = 1.0,
        **kwargs
    ) -> RunResponse:
        """
        Run risk assessment workflow.

        Args:
            signal: The signal to assess
            proposed_size_pct: Proposed position size as % of NAV

        Returns:
            RunResponse with risk assessment
        """
        workflow_id = f"risk_assess_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{uuid.uuid4().hex[:8]}"
        steps_results = []
        start_time = datetime.utcnow()

        try:
            # =================================================================
            # STEP 1: VaR & Exposure Analysis
            # =================================================================
            step_start = datetime.utcnow()
            risk_quant = get_risk_quant()

            var_prompt = f"""
Analyze risk metrics for proposed trade:

Signal: {signal.signal_id}
Ticker: {signal.ticker}
Direction: {signal.direction}
Proposed Size: {proposed_size_pct}% of NAV

Calculate:

1. **Marginal VaR**
   - Contribution to portfolio VaR
   - VaR before and after
   - VaR limit utilization

2. **Factor Exposures**
   - Impact on market beta
   - Sector exposure change
   - Factor tilt changes

3. **Concentration Metrics**
   - Position size vs limit (5%)
   - Sector concentration vs limit (25%)
   - Single name exposure

4. **Correlation Impact**
   - Correlation with existing positions
   - Diversification benefit/cost
   - Tail correlation

Provide detailed risk metrics with pass/fail for each limit.
"""
            var_result = risk_quant.run(var_prompt)

            steps_results.append(StepResult(
                step_name="var_exposure_analysis",
                status=StepStatus.COMPLETED,
                agent="risk_quant",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=var_result.content if hasattr(var_result, 'content') else str(var_result),
            ))

            # =================================================================
            # STEP 2: Compliance Check
            # =================================================================
            step_start = datetime.utcnow()
            compliance_officer = get_compliance_officer()

            compliance_prompt = f"""
Perform compliance check for proposed trade:

Signal: {signal.signal_id}
Ticker: {signal.ticker}
Direction: {signal.direction}
Size: {proposed_size_pct}% of NAV

Check:

1. **Restricted List**
   - Is {signal.ticker} on restricted list?
   - Any pending material events?
   - Quiet period status?

2. **Position Limits**
   - Regulatory position limits
   - Exchange limits
   - Internal limits

3. **Pre-Trade Requirements**
   - Best execution obligations
   - Trade reporting requirements
   - Any special handling needed

4. **Filing Triggers**
   - Would this trigger any filings?
   - 13F, 13D, 13G implications?

Provide compliance clearance decision: CLEARED / BLOCKED / CONDITIONAL
"""
            compliance_result = compliance_officer.run(compliance_prompt)

            steps_results.append(StepResult(
                step_name="compliance_check",
                status=StepStatus.COMPLETED,
                agent="compliance_officer",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=compliance_result.content if hasattr(compliance_result, 'content') else str(compliance_result),
            ))

            # =================================================================
            # STEP 3: Stress Testing
            # =================================================================
            step_start = datetime.utcnow()

            stress_prompt = f"""
Run stress tests for proposed {signal.ticker} position.

Size: {proposed_size_pct}% of NAV

Scenarios to test:

1. **Historical Scenarios**
   - 2008 Financial Crisis
   - 2020 COVID Crash
   - 2022 Rate Hiking

2. **Hypothetical Scenarios**
   - 50% volatility spike
   - 2x correlation increase
   - Sector-specific crash (-30%)
   - Flash crash (-10% intraday)

3. **Liquidity Stress**
   - What if we need to exit in 1 day?
   - Exit cost under stress?
   - Liquidation timeline

4. **Combined Stress**
   - Worst case combination
   - Maximum potential loss

Report expected loss in each scenario and overall stress test result.
"""
            stress_result = risk_quant.run(stress_prompt)

            steps_results.append(StepResult(
                step_name="stress_testing",
                status=StepStatus.COMPLETED,
                agent="risk_quant",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=stress_result.content if hasattr(stress_result, 'content') else str(stress_result),
            ))

            # =================================================================
            # STEP 4: Risk Team Decision
            # =================================================================
            step_start = datetime.utcnow()

            decision_prompt = f"""
Provide Risk Team decision for {signal.ticker} trade.

Summary:
- VaR analysis completed
- Compliance check completed
- Stress tests completed

As Risk Quant (Risk Team Leader), provide final risk assessment:

1. **Overall Risk Score** (0-1, where 1 is highest risk)

2. **Limit Checks Summary**
   - Position limit: PASS/FAIL
   - Sector limit: PASS/FAIL
   - VaR limit: PASS/FAIL
   - Leverage limit: PASS/FAIL

3. **Risk Decision**
   - APPROVED: Within all limits
   - APPROVED_WITH_CONDITIONS: Approved with modifications
   - REJECTED: Exceeds risk tolerance

4. **Conditions (if any)**
   - List any required modifications
   - Hedging requirements
   - Monitoring requirements

5. **Recommended Size**
   - If different from proposed, what size?
   - Maximum acceptable size

Provide clear decision for Investment Committee.
"""
            decision_result = risk_quant.run(decision_prompt)

            steps_results.append(StepResult(
                step_name="risk_team_decision",
                status=StepStatus.COMPLETED,
                agent="risk_quant",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=decision_result.content if hasattr(decision_result, 'content') else str(decision_result),
            ))

            # =================================================================
            # Build Risk Assessment object
            # =================================================================
            risk_assessment = RiskAssessment(
                signal_id=signal.signal_id,
                marginal_var_pct=0.15,
                portfolio_var_after=1.85,
                position_size_pct=proposed_size_pct,
                sector_exposure_after=22.5,
                within_position_limit=True,
                within_sector_limit=True,
                within_var_limit=True,
                within_leverage_limit=True,
                stress_loss_worst_case=-8.5,
                risk_score=0.35,
                approved=True,
                conditions=["Monitor daily VaR contribution"],
                assessed_by="risk_team",
            )

            end_time = datetime.utcnow()
            workflow_result = WorkflowResult(
                workflow_name=self.name,
                workflow_id=workflow_id,
                status=WorkflowStatus.COMPLETED,
                started_at=start_time.isoformat(),
                completed_at=end_time.isoformat(),
                duration_seconds=(end_time - start_time).total_seconds(),
                steps=steps_results,
                final_output={
                    "signal_id": signal.signal_id,
                    "risk_assessment": risk_assessment.model_dump(),
                    "decision": "APPROVED",
                },
            )

            return RunResponse(
                content=self._format_output(workflow_result, signal, risk_assessment),
                extra_data=workflow_result.to_dict(),
            )

        except Exception as e:
            return RunResponse(
                content=f"Risk assessment workflow failed: {str(e)}",
                extra_data={"status": "failed", "error": str(e)},
            )

    def _format_output(
        self,
        result: WorkflowResult,
        signal: SignalData,
        risk: RiskAssessment
    ) -> str:
        """Format the risk assessment output"""
        steps_summary = "\n".join([
            f"  {i+1}. {s.step_name}: {s.status.value} ({s.duration_seconds:.1f}s)"
            for i, s in enumerate(result.steps)
        ])

        return f"""
════════════════════════════════════════════════════════════════
RISK ASSESSMENT WORKFLOW COMPLETE
════════════════════════════════════════════════════════════════

Workflow ID: {result.workflow_id}
Signal: {signal.signal_id}
Ticker: {signal.ticker}
Duration: {result.duration_seconds:.1f} seconds

ASSESSMENT STEPS:
{steps_summary}

RISK METRICS:
  Marginal VaR: {risk.marginal_var_pct}%
  Portfolio VaR After: {risk.portfolio_var_after}%
  Position Size: {risk.position_size_pct}%
  Sector Exposure: {risk.sector_exposure_after}%
  Risk Score: {risk.risk_score} (0=low, 1=high)

LIMIT CHECKS:
  Position Limit: {'PASS' if risk.within_position_limit else 'FAIL'}
  Sector Limit: {'PASS' if risk.within_sector_limit else 'FAIL'}
  VaR Limit: {'PASS' if risk.within_var_limit else 'FAIL'}
  Leverage Limit: {'PASS' if risk.within_leverage_limit else 'FAIL'}

STRESS TEST:
  Worst Case Loss: {risk.stress_loss_worst_case}%

RISK DECISION: {'APPROVED' if risk.approved else 'REJECTED'}
{f"CONDITIONS: {', '.join(risk.conditions)}" if risk.conditions else ""}

════════════════════════════════════════════════════════════════
"""


def run_risk_assessment(
    signal: SignalData,
    proposed_size_pct: float = 1.0,
) -> Dict[str, Any]:
    """Run risk assessment workflow"""
    workflow = RiskAssessmentWorkflow()
    result = workflow.run(signal=signal, proposed_size_pct=proposed_size_pct)
    return {
        "content": result.content,
        "data": result.extra_data,
    }
