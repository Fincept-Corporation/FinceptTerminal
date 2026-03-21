"""
Renaissance Technologies Workflow Pipelines

Agno Workflows that define the end-to-end processes from
signal discovery through order execution.

Workflows:
1. Signal Discovery - Data → Raw Signal
2. Signal Validation - Raw Signal → Validated Signal
3. Risk Assessment - Signal → Risk-Adjusted Signal
4. Execution Pipeline - Approved Signal → Executed Trade
5. Post-Trade Analysis - Execution → Learnings
6. Daily Trading Cycle - Full daily workflow
"""

from .signal_discovery import (
    SignalDiscoveryWorkflow,
    run_signal_discovery,
)
from .signal_validation import (
    SignalValidationWorkflow,
    run_signal_validation,
)
from .risk_assessment import (
    RiskAssessmentWorkflow,
    run_risk_assessment,
)
from .execution_pipeline import (
    ExecutionPipelineWorkflow,
    run_execution_pipeline,
)
from .post_trade_analysis import (
    PostTradeWorkflow,
    run_post_trade_analysis,
)
from .daily_cycle import (
    DailyTradingCycleWorkflow,
    run_daily_cycle,
)

__all__ = [
    # Signal Discovery
    "SignalDiscoveryWorkflow",
    "run_signal_discovery",
    # Signal Validation
    "SignalValidationWorkflow",
    "run_signal_validation",
    # Risk Assessment
    "RiskAssessmentWorkflow",
    "run_risk_assessment",
    # Execution Pipeline
    "ExecutionPipelineWorkflow",
    "run_execution_pipeline",
    # Post-Trade Analysis
    "PostTradeWorkflow",
    "run_post_trade_analysis",
    # Daily Cycle
    "DailyTradingCycleWorkflow",
    "run_daily_cycle",
]
