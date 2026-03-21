"""
Modules - High-level Agno feature modules

Provides unified access to:
- Memory (conversation history, user preferences, agentic memory)
- Teams (multi-agent collaboration)
- Workflows (complex task orchestration with Parallel, Condition, Loop, Router)
- Knowledge (RAG knowledge bases)
- Reasoning (step-by-step reasoning)
- Session (conversation state persistence, AI summaries, history search)
- Guardrails (PII protection, prompt injection, compliance)
- Evaluation (accuracy, performance, reliability)
- Tracing (debugging, monitoring, audit)
- Compression (token optimization for large datasets)
- Hooks (pre/post processing, rate limiting, cost tracking)
- Output Models (structured Pydantic outputs for financial data)
"""

from finagent_core.modules.memory_module import MemoryModule, AgenticMemoryModule
from finagent_core.modules.team_module import TeamModule, TeamBuilder
from finagent_core.modules.workflow_module import (
    WorkflowModule,
    WorkflowBuilder,
    FinancialWorkflowTemplates
)
from finagent_core.modules.knowledge_module import KnowledgeModule, KnowledgeBuilder
from finagent_core.modules.reasoning_module import ReasoningModule, ReasoningBuilder
from finagent_core.modules.session_module import SessionModule, SessionManager
from finagent_core.modules.guardrails_module import (
    GuardrailsModule,
    FinancialPIIGuardrail,
    PromptInjectionGuardrail,
    TradingComplianceGuardrail,
    OutputValidationGuardrail
)
from finagent_core.modules.evaluation_module import (
    EvaluationModule,
    AccuracyEvaluator,
    PerformanceEvaluator,
    ReliabilityEvaluator,
    AgentJudgeEvaluator,
    EvaluationResult
)
from finagent_core.modules.tracing_module import (
    TracingModule,
    Trace,
    Span,
    AuditLogger
)
from finagent_core.modules.compression_module import (
    CompressionModule,
    FinancialDataCompressor,
    SummarizeStrategy
)
from finagent_core.modules.hooks_module import (
    HooksModule,
    Hook,
    InputValidationHook,
    OutputFormattingHook,
    AuditLoggingHook,
    RateLimitHook,
    CostTrackingHook
)
from finagent_core.modules.output_models import (
    OutputModelRegistry,
    OutputValidator,
    TradeSignal,
    StockAnalysis,
    PortfolioAnalysis,
    RiskAssessment,
    MarketAnalysis,
    ResearchReport,
    TradeAction,
    RiskLevel,
    TimeHorizon,
    Sentiment
)

__all__ = [
    # Memory
    "MemoryModule",
    "AgenticMemoryModule",
    # Teams
    "TeamModule",
    "TeamBuilder",
    # Workflows
    "WorkflowModule",
    "WorkflowBuilder",
    "FinancialWorkflowTemplates",
    # Knowledge
    "KnowledgeModule",
    "KnowledgeBuilder",
    # Reasoning
    "ReasoningModule",
    "ReasoningBuilder",
    # Session
    "SessionModule",
    "SessionManager",
    # Guardrails
    "GuardrailsModule",
    "FinancialPIIGuardrail",
    "PromptInjectionGuardrail",
    "TradingComplianceGuardrail",
    "OutputValidationGuardrail",
    # Evaluation
    "EvaluationModule",
    "AccuracyEvaluator",
    "PerformanceEvaluator",
    "ReliabilityEvaluator",
    "AgentJudgeEvaluator",
    "EvaluationResult",
    # Tracing
    "TracingModule",
    "Trace",
    "Span",
    "AuditLogger",
    # Compression
    "CompressionModule",
    "FinancialDataCompressor",
    "SummarizeStrategy",
    # Hooks
    "HooksModule",
    "Hook",
    "InputValidationHook",
    "OutputFormattingHook",
    "AuditLoggingHook",
    "RateLimitHook",
    "CostTrackingHook",
    # Output Models
    "OutputModelRegistry",
    "OutputValidator",
    "TradeSignal",
    "StockAnalysis",
    "PortfolioAnalysis",
    "RiskAssessment",
    "MarketAnalysis",
    "ResearchReport",
    "TradeAction",
    "RiskLevel",
    "TimeHorizon",
    "Sentiment",
]
