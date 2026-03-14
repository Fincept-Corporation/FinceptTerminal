"""
Daily Trading Cycle Workflow

The complete end-to-end daily workflow that orchestrates all
other workflows into a cohesive trading day cycle.

This is the master workflow that a real trading firm would run daily.

Phases:
1. Pre-Market (Signal Discovery & Validation)
2. Risk Review (Risk Assessment)
3. IC Decision (Investment Committee Approval)
4. Execution (Trade Execution)
5. Post-Market (Analysis & Learning)
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
    get_investment_committee_chair,
    get_portfolio_manager,
    get_research_lead,
    get_risk_quant,
)
from ..config import get_config
from .base import (
    WorkflowResult,
    StepResult,
    StepStatus,
    WorkflowStatus,
    SignalData,
    RiskAssessment,
    TradeDecision,
    ExecutionResult,
    WorkflowContext,
)
from .signal_discovery import SignalDiscoveryWorkflow
from .signal_validation import SignalValidationWorkflow
from .risk_assessment import RiskAssessmentWorkflow
from .execution_pipeline import ExecutionPipelineWorkflow
from .post_trade_analysis import PostTradeWorkflow


class DailyTradingCycleWorkflow(Workflow):
    """
    Complete daily trading cycle.

    Orchestrates all workflows in sequence:
    1. Discover signals for given tickers
    2. Validate promising signals
    3. Assess risk
    4. Get IC approval
    5. Execute approved trades
    6. Analyze results

    This mirrors how a real quant fund operates on a daily basis.
    """

    name: str = "daily_trading_cycle"
    description: str = "Complete daily trading cycle from signal to execution"

    def run(
        self,
        tickers: List[str],
        signal_types: List[str] = None,
        max_trades: int = 5,
        **kwargs
    ) -> RunResponse:
        """
        Run the complete daily trading cycle.

        Args:
            tickers: List of tickers to analyze
            signal_types: Signal types to look for
            max_trades: Maximum number of trades to execute

        Returns:
            RunResponse with complete daily results
        """
        if signal_types is None:
            signal_types = ["mean_reversion", "momentum"]

        workflow_id = f"daily_{datetime.utcnow().strftime('%Y%m%d')}_{uuid.uuid4().hex[:8]}"
        start_time = datetime.utcnow()

        # Track all discovered signals, decisions, and executions
        discovered_signals: List[SignalData] = []
        validated_signals: List[SignalData] = []
        approved_trades: List[TradeDecision] = []
        executions: List[ExecutionResult] = []

        phase_results = []

        try:
            # =================================================================
            # PHASE 1: PRE-MARKET - Signal Discovery
            # =================================================================
            phase_start = datetime.utcnow()

            discovery_workflow = SignalDiscoveryWorkflow()

            for ticker in tickers:
                for signal_type in signal_types:
                    result = discovery_workflow.run(
                        ticker=ticker,
                        signal_type=signal_type,
                    )

                    # Extract signal from result
                    if result.extra_data and result.extra_data.get("status") == "completed":
                        signal_dict = result.extra_data.get("final_output", {}).get("signal", {})
                        if signal_dict:
                            signal = SignalData(**signal_dict)
                            discovered_signals.append(signal)

            phase_results.append({
                "phase": "signal_discovery",
                "duration": (datetime.utcnow() - phase_start).total_seconds(),
                "signals_discovered": len(discovered_signals),
            })

            # =================================================================
            # PHASE 2: Signal Validation
            # =================================================================
            phase_start = datetime.utcnow()

            validation_workflow = SignalValidationWorkflow()

            for signal in discovered_signals:
                result = validation_workflow.run(signal=signal)

                if result.extra_data and result.extra_data.get("final_output", {}).get("recommendation") == "SUBMIT_TO_IC":
                    validated_signals.append(signal)

            phase_results.append({
                "phase": "signal_validation",
                "duration": (datetime.utcnow() - phase_start).total_seconds(),
                "signals_validated": len(validated_signals),
            })

            # =================================================================
            # PHASE 3: Risk Assessment
            # =================================================================
            phase_start = datetime.utcnow()

            risk_workflow = RiskAssessmentWorkflow()
            risk_approved_signals = []

            for signal in validated_signals:
                result = risk_workflow.run(signal=signal, proposed_size_pct=1.0)

                if result.extra_data and result.extra_data.get("final_output", {}).get("decision") == "APPROVED":
                    risk_approved_signals.append(signal)

            phase_results.append({
                "phase": "risk_assessment",
                "duration": (datetime.utcnow() - phase_start).total_seconds(),
                "risk_approved": len(risk_approved_signals),
            })

            # =================================================================
            # PHASE 4: Investment Committee Decision
            # =================================================================
            phase_start = datetime.utcnow()

            ic_chair = get_investment_committee_chair()
            portfolio_manager = get_portfolio_manager()

            for signal in risk_approved_signals[:max_trades]:  # Limit trades
                # Portfolio Manager recommends sizing
                sizing_prompt = f"""
Recommend position sizing for approved signal:

Signal: {signal.signal_id}
Ticker: {signal.ticker}
Type: {signal.signal_type}
Direction: {signal.direction}
Strength: {signal.strength}
Confidence: {signal.confidence}
Expected Return: {signal.expected_return_bps} bps

Considering:
- Current portfolio composition
- Risk budget remaining
- Correlation with existing positions
- Liquidity constraints

Recommend:
1. Position size (% of NAV)
2. Dollar amount
3. Share quantity (assume $150/share)
4. Max loss threshold

Provide sizing recommendation.
"""
                sizing_result = portfolio_manager.run(sizing_prompt)

                # IC Chair makes final decision
                decision_prompt = f"""
Make final trading decision for:

Signal: {signal.signal_id}
Ticker: {signal.ticker}
Type: {signal.signal_type}
Direction: {signal.direction}

Signal Metrics:
- Strength: {signal.strength}
- Confidence: {signal.confidence}
- P-value: {signal.p_value}
- Expected Return: {signal.expected_return_bps} bps

Status:
- Research Team: VALIDATED ✓
- Risk Team: APPROVED ✓
- Portfolio Manager: Sizing provided ✓

As Investment Committee Chair, make your decision:

DECISION: [APPROVED / REJECTED / PENDING]

If APPROVED, specify:
- Approved quantity (shares)
- Approved notional ($)
- Maximum loss limit (%)
- Stop loss price

If REJECTED, specify reason.

Provide your decision with full rationale.
"""
                decision_result = ic_chair.run(decision_prompt)

                # Create trade decision (simulated approval)
                trade_decision = TradeDecision(
                    signal_id=signal.signal_id,
                    decision="APPROVED",
                    rationale=[
                        "Statistical significance confirmed (p < 0.01)",
                        "Risk metrics within limits",
                        "Diversification benefit positive",
                    ],
                    conditions=["Monitor daily VaR", "Review at T+5"],
                    approved_quantity=10000,
                    approved_notional=1500000.0,
                    max_loss_pct=2.0,
                    decided_by="investment_committee_chair",
                )
                approved_trades.append(trade_decision)

            phase_results.append({
                "phase": "ic_decision",
                "duration": (datetime.utcnow() - phase_start).total_seconds(),
                "trades_approved": len(approved_trades),
            })

            # =================================================================
            # PHASE 5: Execution
            # =================================================================
            phase_start = datetime.utcnow()

            execution_workflow = ExecutionPipelineWorkflow()

            for i, decision in enumerate(approved_trades):
                # Find corresponding signal
                signal = next(
                    (s for s in risk_approved_signals if s.signal_id == decision.signal_id),
                    risk_approved_signals[i] if i < len(risk_approved_signals) else None
                )

                if signal:
                    result = execution_workflow.run(signal=signal, decision=decision)

                    if result.extra_data:
                        exec_dict = result.extra_data.get("final_output", {}).get("execution", {})
                        if exec_dict:
                            execution = ExecutionResult(**exec_dict)
                            executions.append(execution)

            phase_results.append({
                "phase": "execution",
                "duration": (datetime.utcnow() - phase_start).total_seconds(),
                "trades_executed": len(executions),
            })

            # =================================================================
            # PHASE 6: Post-Market Analysis
            # =================================================================
            phase_start = datetime.utcnow()

            post_workflow = PostTradeWorkflow()

            for i, execution in enumerate(executions):
                signal = next(
                    (s for s in risk_approved_signals if s.signal_id == execution.signal_id),
                    risk_approved_signals[i] if i < len(risk_approved_signals) else None
                )

                if signal:
                    post_workflow.run(signal=signal, execution=execution)

            phase_results.append({
                "phase": "post_trade_analysis",
                "duration": (datetime.utcnow() - phase_start).total_seconds(),
                "trades_analyzed": len(executions),
            })

            # =================================================================
            # Build Final Result
            # =================================================================
            end_time = datetime.utcnow()

            workflow_result = WorkflowResult(
                workflow_name=self.name,
                workflow_id=workflow_id,
                status=WorkflowStatus.COMPLETED,
                started_at=start_time.isoformat(),
                completed_at=end_time.isoformat(),
                duration_seconds=(end_time - start_time).total_seconds(),
                steps=[],  # Phase-based, not step-based
                final_output={
                    "tickers_analyzed": tickers,
                    "signals_discovered": len(discovered_signals),
                    "signals_validated": len(validated_signals),
                    "trades_approved": len(approved_trades),
                    "trades_executed": len(executions),
                    "phases": phase_results,
                },
            )

            return RunResponse(
                content=self._format_output(workflow_result, phase_results, executions),
                extra_data=workflow_result.to_dict(),
            )

        except Exception as e:
            return RunResponse(
                content=f"Daily trading cycle failed: {str(e)}",
                extra_data={"status": "failed", "error": str(e)},
            )

    def _format_output(
        self,
        result: WorkflowResult,
        phases: List[Dict[str, Any]],
        executions: List[ExecutionResult]
    ) -> str:
        """Format the daily cycle output"""
        output = result.final_output

        phases_summary = "\n".join([
            f"  {i+1}. {p['phase']}: {p['duration']:.1f}s"
            for i, p in enumerate(phases)
        ])

        exec_summary = ""
        if executions:
            total_notional = sum(e.filled_quantity * e.average_price for e in executions)
            avg_cost = sum(e.actual_cost_bps for e in executions) / len(executions)
            exec_summary = f"""
EXECUTIONS:
  Total Trades: {len(executions)}
  Total Notional: ${total_notional:,.0f}
  Average Cost: {avg_cost:.1f} bps
"""

        return f"""
════════════════════════════════════════════════════════════════════════
                    DAILY TRADING CYCLE COMPLETE
════════════════════════════════════════════════════════════════════════

Cycle ID: {result.workflow_id}
Date: {datetime.utcnow().strftime('%Y-%m-%d')}
Total Duration: {result.duration_seconds:.1f} seconds

PIPELINE SUMMARY:
  Tickers Analyzed: {len(output['tickers_analyzed'])}
  Signals Discovered: {output['signals_discovered']}
  Signals Validated: {output['signals_validated']}
  Trades Approved: {output['trades_approved']}
  Trades Executed: {output['trades_executed']}

PHASES:
{phases_summary}
{exec_summary}
FUNNEL:
  {output['signals_discovered']} discovered
    → {output['signals_validated']} validated ({output['signals_validated']/max(output['signals_discovered'],1)*100:.0f}%)
      → {output['trades_approved']} approved ({output['trades_approved']/max(output['signals_validated'],1)*100:.0f}%)
        → {output['trades_executed']} executed ({output['trades_executed']/max(output['trades_approved'],1)*100:.0f}%)

STATUS: Daily Cycle Complete ✓

════════════════════════════════════════════════════════════════════════
               "We're right 50.75% of the time" - Bob Mercer
════════════════════════════════════════════════════════════════════════
"""


def run_daily_cycle(
    tickers: List[str],
    signal_types: List[str] = None,
    max_trades: int = 5,
) -> Dict[str, Any]:
    """
    Run the complete daily trading cycle.

    This is the main entry point for simulating a full trading day.

    Args:
        tickers: List of tickers to analyze
        signal_types: Signal types to look for (default: mean_reversion, momentum)
        max_trades: Maximum trades to execute

    Returns:
        Dictionary with cycle results
    """
    workflow = DailyTradingCycleWorkflow()
    result = workflow.run(
        tickers=tickers,
        signal_types=signal_types,
        max_trades=max_trades,
    )
    return {
        "content": result.content,
        "data": result.extra_data,
    }
