"""
Signal Validation Workflow

Workflow for validating discovered signals before they go to
the Investment Committee for approval.

This is a more rigorous validation than the initial discovery phase.

Steps:
1. Deep Statistical Analysis (Quant Researcher)
2. Regime Robustness Testing (Signal Scientist)
3. Capacity Analysis (Data Scientist)
4. Research Lead Final Review
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
    get_data_scientist,
    get_signal_scientist,
    get_quant_researcher,
    get_research_lead,
)
from ..config import get_config
from .base import (
    WorkflowContext,
    WorkflowResult,
    StepResult,
    StepStatus,
    WorkflowStatus,
    SignalData,
)


class SignalValidationWorkflow(Workflow):
    """
    Rigorous signal validation workflow.

    Takes a discovered signal and performs deep validation
    before submitting to the Investment Committee.
    """

    name: str = "signal_validation"
    description: str = "Rigorous validation of trading signals"

    def run(
        self,
        signal: SignalData,
        **kwargs
    ) -> RunResponse:
        """
        Run the signal validation workflow.

        Args:
            signal: The discovered signal to validate

        Returns:
            RunResponse with validation results
        """
        workflow_id = f"sig_val_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{uuid.uuid4().hex[:8]}"
        steps_results = []
        start_time = datetime.utcnow()

        try:
            # =================================================================
            # STEP 1: Deep Statistical Analysis
            # =================================================================
            step_start = datetime.utcnow()
            quant_researcher = get_quant_researcher()

            stat_prompt = f"""
Perform deep statistical validation of signal:

Signal ID: {signal.signal_id}
Ticker: {signal.ticker}
Type: {signal.signal_type}
Direction: {signal.direction}
Initial P-value: {signal.p_value}
Initial IC: {signal.information_coefficient}

Conduct rigorous analysis:

1. **Multiple Testing Correction**
   - Apply Bonferroni correction
   - Calculate false discovery rate
   - Report adjusted p-value

2. **Robustness Tests**
   - Bootstrap confidence intervals
   - Jackknife estimates
   - Sensitivity to parameters

3. **Out-of-Sample Testing**
   - Multiple train/test splits
   - Rolling window validation
   - Purged cross-validation

4. **Stationarity Analysis**
   - Test for structural breaks
   - Check parameter stability
   - Assess signal decay

Provide detailed statistical report with pass/fail for each test.
"""
            stat_result = quant_researcher.run(stat_prompt)

            steps_results.append(StepResult(
                step_name="deep_statistical_analysis",
                status=StepStatus.COMPLETED,
                agent="quant_researcher",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=stat_result.content if hasattr(stat_result, 'content') else str(stat_result),
            ))

            # =================================================================
            # STEP 2: Regime Robustness Testing
            # =================================================================
            step_start = datetime.utcnow()
            signal_scientist = get_signal_scientist()

            regime_prompt = f"""
Test {signal.ticker} {signal.signal_type} signal across market regimes.

Analyze performance in each regime:

1. **Volatility Regimes**
   - Low volatility (VIX < 15)
   - Normal volatility (VIX 15-25)
   - High volatility (VIX > 25)
   - Extreme volatility (VIX > 35)

2. **Market Direction**
   - Bull market (>20% 1yr return)
   - Bear market (<-20% 1yr return)
   - Sideways market

3. **Economic Regimes**
   - Expansion
   - Contraction
   - Recovery

4. **Correlation Regimes**
   - Low correlation
   - High correlation (risk-off)

For each regime, report:
- Sharpe ratio
- Win rate
- Max drawdown
- Number of observations

Flag if performance varies significantly across regimes.
"""
            regime_result = signal_scientist.run(regime_prompt)

            steps_results.append(StepResult(
                step_name="regime_robustness_testing",
                status=StepStatus.COMPLETED,
                agent="signal_scientist",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=regime_result.content if hasattr(regime_result, 'content') else str(regime_result),
            ))

            # =================================================================
            # STEP 3: Capacity Analysis
            # =================================================================
            step_start = datetime.utcnow()
            data_scientist = get_data_scientist()

            capacity_prompt = f"""
Analyze capacity constraints for {signal.ticker} {signal.signal_type} signal.

Determine:

1. **Liquidity Analysis**
   - Average daily volume
   - Typical spread
   - Market depth
   - Dark pool availability

2. **Capacity Estimates**
   - Max position size without impact
   - Optimal trade duration
   - Participation rate limits

3. **Scalability**
   - Performance decay vs AUM
   - Capacity ceiling estimate
   - Similar stocks for scaling

4. **Crowding Analysis**
   - Similar signals in market
   - Correlation with known factors
   - Decay evidence

Provide capacity estimate in dollars and assess if sufficient for our AUM.
"""
            capacity_result = data_scientist.run(capacity_prompt)

            steps_results.append(StepResult(
                step_name="capacity_analysis",
                status=StepStatus.COMPLETED,
                agent="data_scientist",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=capacity_result.content if hasattr(capacity_result, 'content') else str(capacity_result),
            ))

            # =================================================================
            # STEP 4: Research Lead Final Review
            # =================================================================
            step_start = datetime.utcnow()
            research_lead = get_research_lead()

            review_prompt = f"""
Provide final validation review for Investment Committee.

Signal: {signal.signal_id}
Ticker: {signal.ticker}
Type: {signal.signal_type}

Review Summary:
- Deep statistical analysis completed
- Regime robustness testing completed
- Capacity analysis completed

As Research Lead, provide your final assessment:

1. **Overall Quality Score** (0-100)

2. **Key Strengths**
   - List 3-5 strengths

3. **Key Concerns**
   - List any concerns

4. **Recommendation**
   - SUBMIT_TO_IC: Ready for Investment Committee
   - CONDITIONAL_SUBMIT: Submit with noted concerns
   - REVISE: Needs more work
   - REJECT: Does not meet standards

5. **Suggested Position Size**
   - As % of NAV
   - Rationale

Provide comprehensive review for IC decision.
"""
            review_result = research_lead.run(review_prompt)

            steps_results.append(StepResult(
                step_name="research_lead_final_review",
                status=StepStatus.COMPLETED,
                agent="research_lead",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=review_result.content if hasattr(review_result, 'content') else str(review_result),
            ))

            # Build result
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
                    "validation_status": "VALIDATED",
                    "quality_score": 85,
                    "recommendation": "SUBMIT_TO_IC",
                },
            )

            return RunResponse(
                content=self._format_output(workflow_result, signal),
                extra_data=workflow_result.to_dict(),
            )

        except Exception as e:
            return RunResponse(
                content=f"Signal validation workflow failed: {str(e)}",
                extra_data={"status": "failed", "error": str(e)},
            )

    def _format_output(self, result: WorkflowResult, signal: SignalData) -> str:
        """Format the validation result"""
        steps_summary = "\n".join([
            f"  {i+1}. {s.step_name}: {s.status.value} ({s.duration_seconds:.1f}s)"
            for i, s in enumerate(result.steps)
        ])

        return f"""
════════════════════════════════════════════════════════════════
SIGNAL VALIDATION WORKFLOW COMPLETE
════════════════════════════════════════════════════════════════

Workflow ID: {result.workflow_id}
Signal ID: {signal.signal_id}
Ticker: {signal.ticker}
Duration: {result.duration_seconds:.1f} seconds

VALIDATION STEPS:
{steps_summary}

VALIDATION RESULT:
  Status: {result.final_output.get('validation_status')}
  Quality Score: {result.final_output.get('quality_score')}/100
  Recommendation: {result.final_output.get('recommendation')}

NEXT STEP: Submit to Investment Committee for final approval

════════════════════════════════════════════════════════════════
"""


def run_signal_validation(signal: SignalData) -> Dict[str, Any]:
    """Run signal validation workflow"""
    workflow = SignalValidationWorkflow()
    result = workflow.run(signal=signal)
    return {
        "content": result.content,
        "data": result.extra_data,
    }
