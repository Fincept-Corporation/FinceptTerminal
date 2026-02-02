"""
Fincept Terminal - DeepAgents Integration
Complete wrapper for hierarchical agentic automation with LangGraph
"""

from .deep_agent_wrapper import DeepAgentWrapper
from .fincept_llm_adapter import FinceptLLMAdapter
from .agent_factory import create_fincept_deep_agent
from .subagent_templates import (
    create_research_subagent,
    create_data_analyst_subagent,
    create_trading_subagent,
    create_risk_analyzer_subagent,
    create_reporter_subagent
)

__all__ = [
    'DeepAgentWrapper',
    'FinceptLLMAdapter',
    'create_fincept_deep_agent',
    'create_research_subagent',
    'create_data_analyst_subagent',
    'create_trading_subagent',
    'create_risk_analyzer_subagent',
    'create_reporter_subagent'
]
