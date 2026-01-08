"""
FinAgent Core - Full-Featured Agno Framework Implementation

Complete agent system with all Agno capabilities:
- 40+ Model Providers
- 100+ Tools
- 20+ Vector Databases
- 18+ Embedders
- Memory Management
- Teams (Multi-Agent)
- Workflows
- Knowledge Bases (RAG)
- Reasoning Strategies

Usage:
    from finagent_core import CoreAgent, CoreAgentBuilder

    # Simple usage
    agent = CoreAgent(api_keys={"OPENAI_API_KEY": "..."})
    config = {
        "model": {"provider": "openai", "model_id": "gpt-4o"},
        "instructions": "You are a helpful assistant"
    }
    response = agent.run("Hello!", config)

    # With builder
    config = (CoreAgentBuilder()
        .with_model("anthropic", "claude-sonnet-4-5")
        .with_tools(["yfinance", "duckduckgo"])
        .with_memory()
        .with_reasoning("chain_of_thought")
        .build())
    response = agent.run("Analyze AAPL stock", config)
"""

# Core Agent
from finagent_core.core_agent import CoreAgent, CoreAgentBuilder

# Registries
from finagent_core.registries import (
    ToolsRegistry,
    ModelsRegistry,
    VectorDBRegistry,
    EmbedderRegistry,
)

# Modules
from finagent_core.modules import (
    MemoryModule,
    TeamModule,
    WorkflowModule,
    KnowledgeModule,
    ReasoningModule,
    SessionModule,
    SessionManager,
)

# Builders from modules
from finagent_core.modules.team_module import TeamBuilder
from finagent_core.modules.workflow_module import WorkflowBuilder
from finagent_core.modules.knowledge_module import KnowledgeBuilder
from finagent_core.modules.reasoning_module import ReasoningBuilder

# Legacy compatibility
from finagent_core.agent_factory import AgentFactory
from finagent_core.config_loader import ConfigLoader

__version__ = "2.0.0"

__all__ = [
    # Core
    "CoreAgent",
    "CoreAgentBuilder",

    # Registries
    "ToolsRegistry",
    "ModelsRegistry",
    "VectorDBRegistry",
    "EmbedderRegistry",

    # Modules
    "MemoryModule",
    "TeamModule",
    "WorkflowModule",
    "KnowledgeModule",
    "ReasoningModule",
    "SessionModule",
    "SessionManager",

    # Builders
    "TeamBuilder",
    "WorkflowBuilder",
    "KnowledgeBuilder",
    "ReasoningBuilder",

    # Legacy
    "AgentFactory",
    "ConfigLoader",
]


def get_system_info() -> dict:
    """Get information about the finagent_core system"""
    return {
        "version": __version__,
        "model_providers": ModelsRegistry.list_providers(),
        "tool_categories": ToolsRegistry.list_categories(),
        "vectordbs": VectorDBRegistry.list_vectordbs(),
        "embedders": EmbedderRegistry.list_providers(),
        "reasoning_strategies": ReasoningModule.list_strategies(),
    }


def list_all_tools() -> dict:
    """List all available tools by category"""
    return ToolsRegistry.list_tools()


def list_all_models() -> dict:
    """List all model providers with their available models"""
    providers = {}
    for provider in ModelsRegistry.list_providers():
        info = ModelsRegistry.get_provider_info(provider)
        providers[provider] = info
    return providers
