"""
Post-Trade Analysis Workflow

Workflow for analyzing executed trades and extracting learnings.

Steps:
1. Execution Quality Analysis (Execution Trader)
2. Alpha Attribution (Quant Researcher)
3. Model Update Recommendations (Signal Scientist)
4. Learning Summary (Research Lead)
"""

from typing import Optional, Dict, Any, List
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
    get_execution_trader,
    get_quant_researcher,
    get_signal_scientist,
    get_research_lead,
)
from ..config import get_config
from .base import (
    WorkflowResult,
    StepResult,
    StepStatus,
    WorkflowStatus,
    SignalData,
    ExecutionResult,
)


class PostTradeWorkflow(Workflow):
    """
    Post-trade analysis workflow.

    Analyzes completed trades to extract learnings and
    improve future performance.
    """

    name: str = "post_trade_analysis"
    description: str = "Analyze executed trades and extract learnings"

    def run(
        self,
        signal: SignalData,
        execution: ExecutionResult,
        actual_pnl_bps: Optional[float] = None,
        holding_period_days: Optional[int] = None,
        **kwargs
    ) -> RunResponse:
        """
        Run post-trade analysis.

        Args:
            signal: The original signal
            execution: The execution results
            actual_pnl_bps: Actual P&L in basis points (if position closed)
            holding_period_days: How long position was held

        Returns:
            RunResponse with analysis and learnings
        """
        workflow_id = f"post_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{uuid.uuid4().hex[:8]}"
        steps_results = []
        start_time = datetime.utcnow()

        try:
            # =================================================================
            # STEP 1: Execution Quality Analysis
            # =================================================================
            step_start = datetime.utcnow()
            execution_trader = get_execution_trader()

            exec_analysis_prompt = f"""
Analyze execution quality for completed trade:

Signal: {signal.signal_id}
Ticker: {signal.ticker}
Execution ID: {execution.execution_id}

Execution Results:
- Fill Rate: {execution.fill_rate:.1%}
- vs Arrival: {execution.vs_arrival_bps:+.1f} bps
- vs VWAP: {execution.vs_vwap_bps:+.1f} bps
- Total Cost: {execution.actual_cost_bps:.1f} bps
- Alpha Preserved: {execution.alpha_preserved_pct:.0%}

Analyze:

1. **Execution Quality Assessment**
   - Overall grade (A/B/C/D/F)
   - Comparison to historical average
   - Comparison to pre-trade estimate

2. **What Worked Well**
   - List 2-3 things that worked
   - Venue performance
   - Algorithm choice

3. **What Could Improve**
   - List 2-3 improvement areas
   - Timing issues
   - Routing issues

4. **Model Updates**
   - Impact model accuracy
   - Any parameter adjustments needed
   - Data to add to training set

5. **Execution Score Update**
   - Update historical execution score
   - Trend analysis (improving/declining)

Provide detailed execution analysis.
"""
            exec_result = execution_trader.run(exec_analysis_prompt)

            steps_results.append(StepResult(
                step_name="execution_quality_analysis",
                status=StepStatus.COMPLETED,
                agent="execution_trader",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=exec_result.content if hasattr(exec_result, 'content') else str(exec_result),
            ))

            # =================================================================
            # STEP 2: Alpha Attribution
            # =================================================================
            step_start = datetime.utcnow()
            quant_researcher = get_quant_researcher()

            alpha_prompt = f"""
Perform alpha attribution analysis:

Signal: {signal.signal_id}
Signal Type: {signal.signal_type}
Expected Return: {signal.expected_return_bps} bps
{f'Actual P&L: {actual_pnl_bps} bps' if actual_pnl_bps else 'Position still open'}
{f'Holding Period: {holding_period_days} days' if holding_period_days else ''}

Analyze:

1. **Return Attribution**
   - Gross return
   - Transaction costs
   - Net return
   - vs Expected

2. **Alpha Decomposition**
   - Market contribution
   - Sector contribution
   - Factor contributions
   - Residual alpha (true alpha)

3. **Signal Quality Assessment**
   - Signal accuracy (direction correct?)
   - Magnitude accuracy
   - Timing accuracy
   - Information coefficient realized

4. **Regime Analysis**
   - What regime during trade?
   - Performance vs regime expectation

5. **Statistical Update**
   - Update signal statistics
   - Rolling IC
   - Rolling Sharpe

Provide attribution analysis with statistical updates.
"""
            alpha_result = quant_researcher.run(alpha_prompt)

            steps_results.append(StepResult(
                step_name="alpha_attribution",
                status=StepStatus.COMPLETED,
                agent="quant_researcher",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=alpha_result.content if hasattr(alpha_result, 'content') else str(alpha_result),
            ))

            # =================================================================
            # STEP 3: Model Update Recommendations
            # =================================================================
            step_start = datetime.utcnow()
            signal_scientist = get_signal_scientist()

            model_prompt = f"""
Provide model update recommendations based on trade results.

Signal: {signal.signal_id}
Type: {signal.signal_type}
Ticker: {signal.ticker}

Based on this trade's outcome:

1. **Signal Model Updates**
   - Should we adjust signal parameters?
   - Feature importance changes?
   - Threshold adjustments?

2. **Data Insights**
   - Any new patterns observed?
   - Data quality issues?
   - New features to explore?

3. **Capacity Updates**
   - Capacity estimate still valid?
   - Execution costs as expected?
   - Scale considerations?

4. **Decay Assessment**
   - Any evidence of signal decay?
   - Crowding indicators?
   - Alpha erosion?

5. **Research Priorities**
   - Follow-up research needed?
   - Related signals to explore?
   - Enhancement ideas?

Provide actionable model recommendations.
"""
            model_result = signal_scientist.run(model_prompt)

            steps_results.append(StepResult(
                step_name="model_update_recommendations",
                status=StepStatus.COMPLETED,
                agent="signal_scientist",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=model_result.content if hasattr(model_result, 'content') else str(model_result),
            ))

            # =================================================================
            # STEP 4: Learning Summary
            # =================================================================
            step_start = datetime.utcnow()
            research_lead = get_research_lead()

            summary_prompt = f"""
Synthesize post-trade learnings for {signal.ticker} ({signal.signal_type}).

Summarize:
- Execution quality analysis completed
- Alpha attribution completed
- Model recommendations provided

As Research Lead, provide learning summary:

1. **Trade Summary**
   - Overall outcome assessment
   - Key metrics recap
   - Grade (1-10)

2. **Key Learnings**
   - Top 3 learnings from this trade
   - What to repeat
   - What to avoid

3. **Action Items**
   - Immediate actions needed
   - Research follow-ups
   - Model updates to implement

4. **Memory Update**
   - What should we remember about this trade?
   - Pattern to store for future reference
   - Lessons for similar situations

5. **Portfolio Implications**
   - Impact on similar positions
   - Strategy weight implications
   - Risk budget implications

Provide concise learning summary for organizational memory.
"""
            summary_result = research_lead.run(summary_prompt)

            steps_results.append(StepResult(
                step_name="learning_summary",
                status=StepStatus.COMPLETED,
                agent="research_lead",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=summary_result.content if hasattr(summary_result, 'content') else str(summary_result),
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
                    "execution_id": execution.execution_id,
                    "analysis_complete": True,
                    "learnings_captured": True,
                },
            )

            return RunResponse(
                content=self._format_output(workflow_result, signal, execution, actual_pnl_bps),
                extra_data=workflow_result.to_dict(),
            )

        except Exception as e:
            return RunResponse(
                content=f"Post-trade analysis failed: {str(e)}",
                extra_data={"status": "failed", "error": str(e)},
            )

    def _format_output(
        self,
        result: WorkflowResult,
        signal: SignalData,
        execution: ExecutionResult,
        actual_pnl_bps: Optional[float]
    ) -> str:
        """Format the post-trade analysis output"""
        steps_summary = "\n".join([
            f"  {i+1}. {s.step_name}: {s.status.value} ({s.duration_seconds:.1f}s)"
            for i, s in enumerate(result.steps)
        ])

        return f"""
════════════════════════════════════════════════════════════════
POST-TRADE ANALYSIS COMPLETE
════════════════════════════════════════════════════════════════

Workflow ID: {result.workflow_id}
Signal: {signal.signal_id}
Ticker: {signal.ticker}
Execution: {execution.execution_id}
Duration: {result.duration_seconds:.1f} seconds

ANALYSIS STEPS:
{steps_summary}

TRADE SUMMARY:
  Signal Type: {signal.signal_type}
  Direction: {signal.direction}
  Expected Return: {signal.expected_return_bps} bps
  {f'Actual P&L: {actual_pnl_bps} bps' if actual_pnl_bps else 'Position: Still Open'}

EXECUTION RECAP:
  Fill Rate: {execution.fill_rate:.1%}
  vs Arrival: {execution.vs_arrival_bps:+.1f} bps
  Cost: {execution.actual_cost_bps:.1f} bps
  Quality Score: {execution.execution_quality_score:.0%}

STATUS: Analysis Complete - Learnings Captured ✓

════════════════════════════════════════════════════════════════
"""


def run_post_trade_analysis(
    signal: SignalData,
    execution: ExecutionResult,
    actual_pnl_bps: Optional[float] = None,
    holding_period_days: Optional[int] = None,
) -> Dict[str, Any]:
    """Run post-trade analysis workflow"""
    workflow = PostTradeWorkflow()
    result = workflow.run(
        signal=signal,
        execution=execution,
        actual_pnl_bps=actual_pnl_bps,
        holding_period_days=holding_period_days,
    )
    return {
        "content": result.content,
        "data": result.extra_data,
    }
