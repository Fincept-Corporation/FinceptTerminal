"""
Fincept Terminal — Deep Agents Integration
Built on deepagents 0.4.12 (LangGraph-based hierarchical multi-agent framework)

Entry point: cli.py
"""

from models import create_model, extract_text, supports_tool_calling, TOOL_CALLING_PROVIDERS
from backends import get_backend
from subagents import get_subagents_for_type, list_agent_types, list_subagent_names
from agent import create_agent, build_fincept_context, FinceptContext
from orchestrator import FinceptOrchestrator

__all__ = [
    "create_model",
    "extract_text",
    "supports_tool_calling",
    "TOOL_CALLING_PROVIDERS",
    "get_backend",
    "get_subagents_for_type",
    "list_agent_types",
    "list_subagent_names",
    "create_agent",
    "build_fincept_context",
    "FinceptContext",
    "FinceptOrchestrator",
]
