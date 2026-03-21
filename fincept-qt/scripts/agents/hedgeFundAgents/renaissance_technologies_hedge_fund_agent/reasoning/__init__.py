"""
Renaissance Technologies Reasoning System

Advanced reasoning capabilities for complex decision making:
- Multi-step reasoning for investment decisions
- Chain-of-thought for signal analysis
- Risk reasoning for position sizing
- IC deliberation reasoning
"""

from .base import (
    ReasoningEngine,
    ReasoningStep,
    ReasoningChain,
    ReasoningType,
    get_reasoning_engine,
)
from .investment_reasoning import InvestmentReasoner, get_investment_reasoner
from .risk_reasoning import RiskReasoner, get_risk_reasoner
from .ic_deliberation import ICDeliberator, get_ic_deliberator

__all__ = [
    # Base reasoning
    "ReasoningEngine",
    "ReasoningStep",
    "ReasoningChain",
    "ReasoningType",
    "get_reasoning_engine",
    # Specialized reasoners
    "InvestmentReasoner",
    "get_investment_reasoner",
    "RiskReasoner",
    "get_risk_reasoner",
    "ICDeliberator",
    "get_ic_deliberator",
]
