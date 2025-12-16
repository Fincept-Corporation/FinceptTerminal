"""
Agno Core - Base Infrastructure for Fincept Terminal Agents

This module provides the foundational infrastructure for all agents in the
Fincept Terminal, replacing the old LangGraph-based system with the production-ready
Agno framework.

Key Features:
- Multi-LLM provider support (OpenAI, Anthropic, Google, Ollama, etc.)
- Dynamic JSON-based agent configuration
- Persistent memory and session management
- Team collaboration and delegation
- RAG knowledge integration
- Production-ready AgentOS deployment
"""

__version__ = "1.0.0"
__author__ = "Fincept Terminal"

# Export main classes for easy imports
from finagent_core.config.llm_providers import LLMProviderManager
from finagent_core.base_agent import AgentConfig, ConfigurableAgent, AgentRegistry
from finagent_core.tools.tool_registry import ToolRegistry
from finagent_core.database.db_manager import DatabaseManager
from finagent_core.utils.path_resolver import PathResolver

__all__ = [
    "LLMProviderManager",
    "AgentConfig",
    "ConfigurableAgent",
    "AgentRegistry",
    "ToolRegistry",
    "DatabaseManager",
    "PathResolver",
]
