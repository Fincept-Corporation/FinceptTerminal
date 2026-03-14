"""
Renaissance Technologies Hedge Fund Agent

A multi-agent system inspired by Jim Simons' quantitative approach.
Implements statistical arbitrage, mean reversion, momentum, and market-neutral strategies.

Architecture:
- Signal Generation Agent: Pattern recognition & statistical signals
- Risk Modeling Agent: Volatility, correlation, factor risk analysis
- Execution Agent: Market impact & transaction cost optimization
- Statistical Arbitrage Agent: Pairs trading & mispricing detection
- Medallion Synthesizer: Final decision synthesis (Jim Simons style)

Built on Agno framework with ALL key features:
- Teams: Nested organizational hierarchy (Firm -> Teams -> Agents)
- Workflows: Signal discovery -> IC approval -> Order execution pipelines
- Knowledge: LanceDB-backed market research, strategies, risk models
- Memory: Multi-layer trade outcomes, decisions, lessons learned
- Reasoning: Chain-of-thought for IC deliberation and risk assessment
- Guardrails: Risk limits, compliance checks, signal validation
- Evaluation: Agent performance, trade quality, system health metrics
- Tracing: Distributed observability with spans and metrics collection
"""

from .main import RenaissanceTechnologiesSystem
from .config import get_config, set_config, RenTechConfig

# Teams
from .teams import (
    create_research_team,
    get_research_team,
    create_trading_team,
    get_trading_team,
    create_risk_team,
    get_risk_team,
    create_medallion_fund,
    get_medallion_fund,
    run_medallion_fund,
)

# Workflows
from .workflows import (
    SignalDiscoveryWorkflow,
    SignalValidationWorkflow,
    RiskAssessmentWorkflow,
    ExecutionPipelineWorkflow,
    PostTradeWorkflow,
    DailyTradingCycleWorkflow,
    run_signal_discovery,
    run_signal_validation,
    run_risk_assessment,
    run_execution_pipeline,
    run_post_trade_analysis,
    run_daily_cycle,
)

# Knowledge
from .knowledge import (
    RenTechKnowledge,
    get_knowledge_base,
    reset_knowledge_base,
    KnowledgeCategory,
    MarketDataKnowledge,
    get_market_data_knowledge,
    ResearchKnowledge,
    get_research_knowledge,
    StrategyKnowledge,
    get_strategy_knowledge,
    TradeHistoryKnowledge,
    get_trade_history_knowledge,
)

# Memory
from .memory import (
    RenTechMemory,
    MemoryType,
    MemoryEntry,
    get_memory_system,
    TradeMemory,
    get_trade_memory,
    DecisionMemory,
    get_decision_memory,
    LearningMemory,
    get_learning_memory,
    AgentMemoryManager,
    get_agent_memory_manager,
)

# Reasoning
from .reasoning import (
    get_reasoning_engine,
    get_investment_reasoner,
    get_risk_reasoner,
    get_ic_deliberator,
)

# Guardrails
from .guardrails import (
    Guardrail,
    GuardrailResult,
    GuardrailSeverity,
    GuardrailRegistry,
    get_guardrail_registry,
    PositionSizeGuardrail,
    SectorExposureGuardrail,
    VaRLimitGuardrail,
    DrawdownGuardrail,
    LeverageGuardrail,
)

# Evaluation
from .evaluation import (
    get_signal_evaluator,
    get_trade_evaluator,
    get_agent_evaluator,
    get_system_evaluator,
)

# Tracing
from .tracing import (
    get_tracer,
    get_agent_tracer,
    get_workflow_tracer,
    get_metrics_collector,
)

__all__ = [
    # Main system
    "RenaissanceTechnologiesSystem",
    # Configuration
    "get_config",
    "set_config",
    "RenTechConfig",
    # Teams
    "create_research_team",
    "get_research_team",
    "create_trading_team",
    "get_trading_team",
    "create_risk_team",
    "get_risk_team",
    "create_medallion_fund",
    "get_medallion_fund",
    "run_medallion_fund",
    # Workflows
    "SignalDiscoveryWorkflow",
    "SignalValidationWorkflow",
    "RiskAssessmentWorkflow",
    "ExecutionPipelineWorkflow",
    "PostTradeWorkflow",
    "DailyTradingCycleWorkflow",
    "run_signal_discovery",
    "run_signal_validation",
    "run_risk_assessment",
    "run_execution_pipeline",
    "run_post_trade_analysis",
    "run_daily_cycle",
    # Knowledge
    "RenTechKnowledge",
    "get_knowledge_base",
    "reset_knowledge_base",
    "KnowledgeCategory",
    "MarketDataKnowledge",
    "get_market_data_knowledge",
    "ResearchKnowledge",
    "get_research_knowledge",
    "StrategyKnowledge",
    "get_strategy_knowledge",
    "TradeHistoryKnowledge",
    "get_trade_history_knowledge",
    # Memory
    "RenTechMemory",
    "MemoryType",
    "MemoryEntry",
    "get_memory_system",
    "TradeMemory",
    "get_trade_memory",
    "DecisionMemory",
    "get_decision_memory",
    "LearningMemory",
    "get_learning_memory",
    "AgentMemoryManager",
    "get_agent_memory_manager",
    # Reasoning
    "get_reasoning_engine",
    "get_investment_reasoner",
    "get_risk_reasoner",
    "get_ic_deliberator",
    # Guardrails
    "Guardrail",
    "GuardrailResult",
    "GuardrailSeverity",
    "GuardrailRegistry",
    "get_guardrail_registry",
    "PositionSizeGuardrail",
    "SectorExposureGuardrail",
    "VaRLimitGuardrail",
    "DrawdownGuardrail",
    "LeverageGuardrail",
    # Evaluation
    "get_signal_evaluator",
    "get_trade_evaluator",
    "get_agent_evaluator",
    "get_system_evaluator",
    # Tracing
    "get_tracer",
    "get_agent_tracer",
    "get_workflow_tracer",
    "get_metrics_collector",
]
__version__ = "3.0.0"
