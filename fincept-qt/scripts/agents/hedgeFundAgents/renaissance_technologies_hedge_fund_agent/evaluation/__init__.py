"""
Renaissance Technologies Evaluation System

Evaluation framework for agent performance:
- Signal quality evaluation
- Trade outcome evaluation
- Agent performance metrics
- Team effectiveness analysis
- System health monitoring
"""

from .base import (
    Evaluator,
    EvaluationResult,
    EvaluationMetric,
    EvaluationRegistry,
    get_evaluation_registry,
)
from .signal_evaluation import SignalEvaluator, get_signal_evaluator
from .trade_evaluation import TradeEvaluator, get_trade_evaluator
from .agent_evaluation import AgentEvaluator, get_agent_evaluator
from .system_evaluation import SystemEvaluator, get_system_evaluator

__all__ = [
    # Base
    "Evaluator",
    "EvaluationResult",
    "EvaluationMetric",
    "EvaluationRegistry",
    "get_evaluation_registry",
    # Specialized evaluators
    "SignalEvaluator",
    "get_signal_evaluator",
    "TradeEvaluator",
    "get_trade_evaluator",
    "AgentEvaluator",
    "get_agent_evaluator",
    "SystemEvaluator",
    "get_system_evaluator",
]
