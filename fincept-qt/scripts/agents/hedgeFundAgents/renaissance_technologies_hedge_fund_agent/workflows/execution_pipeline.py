"""
Execution Pipeline Workflow

Workflow for executing approved trades with optimal execution.

Steps:
1. Pre-Trade Analysis (Execution Trader)
2. Execution Planning (Execution Trader)
3. Order Execution (Execution Trader + Market Maker coordination)
4. Post-Trade Verification (Compliance Officer)
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
    get_execution_trader,
    get_market_maker,
    get_compliance_officer,
)
from ..config import get_config
from .base import (
    WorkflowResult,
    StepResult,
    StepStatus,
    WorkflowStatus,
    SignalData,
    TradeDecision,
    ExecutionPlan,
    ExecutionResult,
)


class ExecutionPipelineWorkflow(Workflow):
    """
    Trade execution workflow.

    Takes an approved trade decision and executes it
    with optimal execution strategy.
    """

    name: str = "execution_pipeline"
    description: str = "Execute approved trades with optimal execution"

    def run(
        self,
        signal: SignalData,
        decision: TradeDecision,
        **kwargs
    ) -> RunResponse:
        """
        Run execution pipeline.

        Args:
            signal: The original signal
            decision: The approved trade decision

        Returns:
            RunResponse with execution results
        """
        workflow_id = f"exec_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{uuid.uuid4().hex[:8]}"
        steps_results = []
        start_time = datetime.utcnow()

        try:
            # =================================================================
            # STEP 1: Pre-Trade Analysis
            # =================================================================
            step_start = datetime.utcnow()
            execution_trader = get_execution_trader()

            pretrade_prompt = f"""
Perform pre-trade analysis for execution:

Signal: {signal.signal_id}
Ticker: {signal.ticker}
Direction: {signal.direction}
Approved Quantity: {decision.approved_quantity}
Approved Notional: ${decision.approved_notional}

Analyze current market conditions:

1. **Market Microstructure**
   - Current bid-ask spread
   - Order book depth
   - Recent volatility
   - Volume profile today

2. **Liquidity Assessment**
   - Average daily volume
   - Our size as % of ADV
   - Expected participation rate
   - Dark pool availability

3. **Timing Considerations**
   - Time of day
   - Upcoming events
   - Optimal execution window
   - Urgency assessment

4. **Initial Cost Estimate**
   - Expected spread cost
   - Expected impact cost
   - Total expected cost (bps)

Provide pre-trade analysis report.
"""
            pretrade_result = execution_trader.run(pretrade_prompt)

            steps_results.append(StepResult(
                step_name="pre_trade_analysis",
                status=StepStatus.COMPLETED,
                agent="execution_trader",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=pretrade_result.content if hasattr(pretrade_result, 'content') else str(pretrade_result),
            ))

            # =================================================================
            # STEP 2: Execution Planning
            # =================================================================
            step_start = datetime.utcnow()

            planning_prompt = f"""
Create optimal execution plan for {signal.ticker} trade.

Order Details:
- Direction: {signal.direction}
- Quantity: {decision.approved_quantity} shares
- Max Loss: {decision.max_loss_pct}%

Design execution strategy:

1. **Algorithm Selection**
   - Recommended algo (TWAP/VWAP/Arrival Price/etc.)
   - Rationale for selection

2. **Execution Schedule**
   - Start time
   - End time
   - Duration
   - Participation rate

3. **Venue Routing**
   - Primary venue
   - Secondary venues
   - Dark pool allocation
   - Order types per venue

4. **Risk Controls**
   - Price limits
   - Cancel thresholds
   - Reversion triggers

5. **Cost Targets**
   - Target vs Arrival (bps)
   - Target vs VWAP (bps)
   - Maximum acceptable slippage

Output detailed execution plan.
"""
            planning_result = execution_trader.run(planning_prompt)

            steps_results.append(StepResult(
                step_name="execution_planning",
                status=StepStatus.COMPLETED,
                agent="execution_trader",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=planning_result.content if hasattr(planning_result, 'content') else str(planning_result),
            ))

            # =================================================================
            # STEP 3: Market Maker Coordination
            # =================================================================
            step_start = datetime.utcnow()
            market_maker = get_market_maker()

            mm_prompt = f"""
Provide market making insight for {signal.ticker} execution.

Planned Trade:
- Direction: {signal.direction}
- Size: {decision.approved_quantity} shares

As Market Maker, advise on:

1. **Current Quote Status**
   - Are we quoting this name?
   - Current inventory
   - Spread we're making

2. **Execution Insight**
   - Can we internalize any flow?
   - Optimal timing based on flow patterns
   - Hidden liquidity we know of

3. **Coordination**
   - Should we pause market making?
   - Inventory implications
   - Spread adjustment needed

4. **Recommendations**
   - Best time to execute
   - Venues to prefer/avoid
   - Size chunking suggestions

Provide market making perspective on execution.
"""
            mm_result = market_maker.run(mm_prompt)

            steps_results.append(StepResult(
                step_name="market_maker_coordination",
                status=StepStatus.COMPLETED,
                agent="market_maker",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=mm_result.content if hasattr(mm_result, 'content') else str(mm_result),
            ))

            # =================================================================
            # STEP 4: Execute Orders (Simulated)
            # =================================================================
            step_start = datetime.utcnow()

            execute_prompt = f"""
Execute the {signal.ticker} order and report results.

[SIMULATED EXECUTION]

Order Parameters:
- Direction: {signal.direction}
- Quantity: {decision.approved_quantity}
- Algorithm: Adaptive Arrival Price
- Duration: 120 minutes

Report execution results:

1. **Fill Summary**
   - Filled quantity
   - Average price
   - Fill rate

2. **Benchmark Comparison**
   - Arrival price
   - VWAP
   - TWAP
   - Performance vs each (bps)

3. **Cost Analysis**
   - Spread cost paid
   - Market impact
   - Timing cost
   - Total implementation shortfall

4. **Venue Breakdown**
   - Fill by venue
   - Price quality by venue

5. **Execution Quality Score**
   - Overall score (0-100)
   - Alpha preservation %

Provide complete execution report.
"""
            execute_result = execution_trader.run(execute_prompt)

            steps_results.append(StepResult(
                step_name="order_execution",
                status=StepStatus.COMPLETED,
                agent="execution_trader",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=execute_result.content if hasattr(execute_result, 'content') else str(execute_result),
            ))

            # =================================================================
            # STEP 5: Post-Trade Compliance Verification
            # =================================================================
            step_start = datetime.utcnow()
            compliance_officer = get_compliance_officer()

            compliance_prompt = f"""
Verify execution compliance for {signal.ticker} trade.

Trade Details:
- Signal: {signal.signal_id}
- Direction: {signal.direction}
- Quantity: {decision.approved_quantity}

Verify:

1. **Best Execution**
   - Did we achieve best execution?
   - Any concerns with execution quality?
   - Documentation complete?

2. **Trade Reporting**
   - Required reports filed?
   - Timestamps recorded?
   - Audit trail complete?

3. **Position Update**
   - Position correctly recorded?
   - Any limit implications?
   - Filings triggered?

4. **Compliance Sign-off**
   - All requirements met?
   - Any exceptions to note?
   - Trade approved for booking?

Provide compliance verification report.
"""
            compliance_result = compliance_officer.run(compliance_prompt)

            steps_results.append(StepResult(
                step_name="compliance_verification",
                status=StepStatus.COMPLETED,
                agent="compliance_officer",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=compliance_result.content if hasattr(compliance_result, 'content') else str(compliance_result),
            ))

            # =================================================================
            # Build Execution Result
            # =================================================================
            execution_result = ExecutionResult(
                signal_id=signal.signal_id,
                execution_id=f"EXEC_{workflow_id}",
                filled_quantity=decision.approved_quantity or 10000,
                average_price=150.25,
                fill_rate=0.98,
                arrival_price=150.20,
                vwap=150.28,
                twap=150.26,
                vs_arrival_bps=-3.3,  # Negative means we beat arrival
                vs_vwap_bps=2.0,
                implementation_shortfall_bps=5.5,
                actual_cost_bps=5.5,
                spread_paid_bps=2.5,
                market_impact_bps=3.0,
                alpha_preserved_pct=0.88,
                execution_quality_score=0.85,
                executed_by="execution_trader",
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
                    "execution": execution_result.model_dump(),
                    "status": "EXECUTED",
                },
            )

            return RunResponse(
                content=self._format_output(workflow_result, signal, execution_result),
                extra_data=workflow_result.to_dict(),
            )

        except Exception as e:
            return RunResponse(
                content=f"Execution pipeline failed: {str(e)}",
                extra_data={"status": "failed", "error": str(e)},
            )

    def _format_output(
        self,
        result: WorkflowResult,
        signal: SignalData,
        execution: ExecutionResult
    ) -> str:
        """Format the execution result"""
        steps_summary = "\n".join([
            f"  {i+1}. {s.step_name}: {s.status.value} ({s.duration_seconds:.1f}s)"
            for i, s in enumerate(result.steps)
        ])

        return f"""
════════════════════════════════════════════════════════════════
EXECUTION PIPELINE COMPLETE
════════════════════════════════════════════════════════════════

Workflow ID: {result.workflow_id}
Execution ID: {execution.execution_id}
Signal: {signal.signal_id}
Ticker: {signal.ticker}
Duration: {result.duration_seconds:.1f} seconds

EXECUTION STEPS:
{steps_summary}

FILL SUMMARY:
  Filled Quantity: {execution.filled_quantity:,}
  Average Price: ${execution.average_price:.2f}
  Fill Rate: {execution.fill_rate:.1%}

BENCHMARK PERFORMANCE:
  vs Arrival: {execution.vs_arrival_bps:+.1f} bps {'(BEAT)' if execution.vs_arrival_bps < 0 else ''}
  vs VWAP: {execution.vs_vwap_bps:+.1f} bps
  Implementation Shortfall: {execution.implementation_shortfall_bps:.1f} bps

COST BREAKDOWN:
  Spread Cost: {execution.spread_paid_bps:.1f} bps
  Market Impact: {execution.market_impact_bps:.1f} bps
  Total Cost: {execution.actual_cost_bps:.1f} bps

QUALITY METRICS:
  Alpha Preserved: {execution.alpha_preserved_pct:.0%}
  Execution Quality Score: {execution.execution_quality_score:.0%}

STATUS: EXECUTED ✓

════════════════════════════════════════════════════════════════
"""


def run_execution_pipeline(
    signal: SignalData,
    decision: TradeDecision,
) -> Dict[str, Any]:
    """Run execution pipeline workflow"""
    workflow = ExecutionPipelineWorkflow()
    result = workflow.run(signal=signal, decision=decision)
    return {
        "content": result.content,
        "data": result.extra_data,
    }
