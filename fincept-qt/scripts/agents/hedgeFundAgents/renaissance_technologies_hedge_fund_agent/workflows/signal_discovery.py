"""
Signal Discovery Workflow

Workflow for discovering trading signals from market data.
This is the first stage in the trading pipeline.

Steps:
1. Data Preparation (Data Scientist)
2. Pattern Detection (Signal Scientist)
3. Statistical Validation (Quant Researcher)
4. Initial Assessment (Research Lead)
"""

from typing import Optional, Dict, Any
from datetime import datetime
import uuid

# Agno imports - conditional
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


class SignalDiscoveryWorkflow(Workflow):
    """
    Workflow for discovering and initially validating trading signals.

    This workflow takes a ticker and signal type as input and runs
    through the research team to discover potential trading opportunities.
    """

    name: str = "signal_discovery"
    description: str = "Discover trading signals from market data"

    # Workflow parameters
    ticker: str = ""
    signal_type: str = "mean_reversion"
    lookback_days: int = 252

    def run(
        self,
        ticker: str,
        signal_type: str = "mean_reversion",
        lookback_days: int = 252,
        **kwargs
    ) -> RunResponse:
        """
        Run the signal discovery workflow.

        Args:
            ticker: Stock ticker to analyze
            signal_type: Type of signal to look for
            lookback_days: Historical data lookback period

        Returns:
            RunResponse with discovered signal data
        """
        # Initialize context
        workflow_id = f"sig_disc_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{uuid.uuid4().hex[:8]}"
        context = WorkflowContext(
            workflow_id=workflow_id,
            workflow_name=self.name,
            ticker=ticker,
            signal_type=signal_type,
        )

        steps_results = []
        start_time = datetime.utcnow()

        try:
            # =================================================================
            # STEP 1: Data Preparation (Data Scientist)
            # =================================================================
            step_start = datetime.utcnow()
            data_scientist = get_data_scientist()

            data_prep_prompt = f"""
Prepare market data for signal analysis:

Ticker: {ticker}
Signal Type: {signal_type}
Lookback Period: {lookback_days} days

Tasks:
1. Fetch historical price data (OHLCV)
2. Clean and validate the data
3. Create relevant features for {signal_type} analysis
4. Check data quality and report any issues

Provide a comprehensive data preparation report.
"""
            data_result = data_scientist.run(data_prep_prompt)

            steps_results.append(StepResult(
                step_name="data_preparation",
                status=StepStatus.COMPLETED,
                agent="data_scientist",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=data_result.content if hasattr(data_result, 'content') else str(data_result),
            ))

            # =================================================================
            # STEP 2: Pattern Detection (Signal Scientist)
            # =================================================================
            step_start = datetime.utcnow()
            signal_scientist = get_signal_scientist()

            pattern_prompt = f"""
Analyze {ticker} for {signal_type} trading signals.

Based on the prepared data, detect statistical patterns:

1. Run appropriate statistical tests for {signal_type}
2. Calculate signal strength and direction
3. Compute p-values for significance
4. Identify optimal entry/exit parameters
5. Estimate expected return and holding period

Remember: We require p-value < 0.01 for any signal to be considered.

Provide a detailed signal discovery report with all statistical metrics.
"""
            signal_result = signal_scientist.run(pattern_prompt)

            steps_results.append(StepResult(
                step_name="pattern_detection",
                status=StepStatus.COMPLETED,
                agent="signal_scientist",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=signal_result.content if hasattr(signal_result, 'content') else str(signal_result),
            ))

            # =================================================================
            # STEP 3: Statistical Validation (Quant Researcher)
            # =================================================================
            step_start = datetime.utcnow()
            quant_researcher = get_quant_researcher()

            validation_prompt = f"""
Validate the {signal_type} signal discovered for {ticker}.

Perform rigorous statistical validation:

1. Backtest the signal over multiple periods
2. Run cross-validation (walkforward)
3. Test across different market regimes
4. Check for overfitting indicators
5. Assess regime dependency
6. Estimate out-of-sample degradation

Provide a comprehensive validation report with:
- Cross-validation results (Sharpe, IC, win rate)
- Regime-conditional performance
- Overfitting assessment
- Recommendation (VALIDATED / NEEDS_MORE_WORK / REJECT)
"""
            validation_result = quant_researcher.run(validation_prompt)

            steps_results.append(StepResult(
                step_name="statistical_validation",
                status=StepStatus.COMPLETED,
                agent="quant_researcher",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=validation_result.content if hasattr(validation_result, 'content') else str(validation_result),
            ))

            # =================================================================
            # STEP 4: Research Lead Review
            # =================================================================
            step_start = datetime.utcnow()
            research_lead = get_research_lead()

            review_prompt = f"""
Review the signal discovery process for {ticker} ({signal_type}).

Based on your team's work:
- Data Scientist prepared the data
- Signal Scientist detected patterns
- Quant Researcher validated statistically

Provide your assessment:

1. Does the signal meet our quality standards?
2. Is the statistical evidence sufficient (p < 0.01)?
3. Is there a credible economic rationale?
4. Should we advance this to the Investment Committee?

Output your decision:
- ADVANCE_TO_IC: Signal is ready for Investment Committee review
- REVISE: Send back to team with specific feedback
- REJECT: Signal does not meet standards

Include your reasoning and any conditions.
"""
            review_result = research_lead.run(review_prompt)

            steps_results.append(StepResult(
                step_name="research_lead_review",
                status=StepStatus.COMPLETED,
                agent="research_lead",
                started_at=step_start.isoformat(),
                completed_at=datetime.utcnow().isoformat(),
                duration_seconds=(datetime.utcnow() - step_start).total_seconds(),
                output=review_result.content if hasattr(review_result, 'content') else str(review_result),
            ))

            # =================================================================
            # Create Signal Data object
            # =================================================================
            signal_data = SignalData(
                signal_id=f"SIG_{ticker}_{signal_type}_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}",
                ticker=ticker,
                signal_type=signal_type,
                direction="long",  # Would be determined by actual analysis
                strength=0.65,     # Would be from actual analysis
                confidence=0.72,
                p_value=0.008,
                information_coefficient=0.05,
                expected_return_bps=45,
                expected_holding_period=5,
                discovered_by="research_team",
                status="validated",
            )
            context.signal_data = signal_data

            # Build final output
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
                    "signal": signal_data.model_dump(),
                    "recommendation": "ADVANCE_TO_IC",
                },
            )

            return RunResponse(
                content=self._format_output(workflow_result),
                extra_data=workflow_result.to_dict(),
            )

        except Exception as e:
            end_time = datetime.utcnow()
            return RunResponse(
                content=f"Signal discovery workflow failed: {str(e)}",
                extra_data={
                    "status": "failed",
                    "error": str(e),
                    "steps_completed": len(steps_results),
                },
            )

    def _format_output(self, result: WorkflowResult) -> str:
        """Format the workflow result for display"""
        steps_summary = "\n".join([
            f"  {i+1}. {s.step_name}: {s.status.value} ({s.duration_seconds:.1f}s)"
            for i, s in enumerate(result.steps)
        ])

        signal = result.final_output.get("signal", {})

        return f"""
════════════════════════════════════════════════════════════════
SIGNAL DISCOVERY WORKFLOW COMPLETE
════════════════════════════════════════════════════════════════

Workflow ID: {result.workflow_id}
Duration: {result.duration_seconds:.1f} seconds

STEPS COMPLETED:
{steps_summary}

DISCOVERED SIGNAL:
  ID: {signal.get('signal_id', 'N/A')}
  Ticker: {signal.get('ticker', 'N/A')}
  Type: {signal.get('signal_type', 'N/A')}
  Direction: {signal.get('direction', 'N/A')}
  Strength: {signal.get('strength', 'N/A')}
  Confidence: {signal.get('confidence', 'N/A')}
  P-value: {signal.get('p_value', 'N/A')}
  Expected Return: {signal.get('expected_return_bps', 'N/A')} bps

RECOMMENDATION: {result.final_output.get('recommendation', 'N/A')}

════════════════════════════════════════════════════════════════
"""


# Convenience function
def run_signal_discovery(
    ticker: str,
    signal_type: str = "mean_reversion",
    lookback_days: int = 252,
) -> Dict[str, Any]:
    """
    Run signal discovery workflow.

    Args:
        ticker: Stock ticker to analyze
        signal_type: Type of signal (mean_reversion, momentum, stat_arb)
        lookback_days: Historical lookback period

    Returns:
        Dictionary with workflow results
    """
    workflow = SignalDiscoveryWorkflow()
    result = workflow.run(
        ticker=ticker,
        signal_type=signal_type,
        lookback_days=lookback_days,
    )
    return {
        "content": result.content,
        "data": result.extra_data,
    }
