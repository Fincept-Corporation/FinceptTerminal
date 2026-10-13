"""
FinAgent Core - Agno-based agent runtime for Fincept Terminal.

Entry point for the terminal is main.py (JSON payload on stdin); this package
only re-exports the public building blocks for scripts and tests:

    from finagent_core import CoreAgent, CoreAgentBuilder

    agent = CoreAgent(api_keys={"OPENAI_API_KEY": "..."})
    config = {
        "model": {"provider": "openai", "model_id": "gpt-4o"},
        "instructions": "You are a helpful assistant",
    }
    response = agent.run("Hello!", config)

Exports are resolved lazily (PEP 562). Importing any finagent_core submodule —
e.g. finagent_core.paths for a schedule tick or a task listing — no longer drags
in Agno, every registry, every module and the planner first; only the module a
name actually lives in is imported, on first use.
"""

import importlib
from typing import Any, Dict

__version__ = "2.1.0"

# Public name -> defining module.
_EXPORTS: Dict[str, str] = {
    # Core
    "CoreAgent": "finagent_core.core_agent",
    "CoreAgentBuilder": "finagent_core.core_agent",
    # Registries
    "ToolsRegistry": "finagent_core.registries",
    "ModelsRegistry": "finagent_core.registries",
    "VectorDBRegistry": "finagent_core.registries",
    "EmbedderRegistry": "finagent_core.registries",
    # Modules
    "MemoryModule": "finagent_core.modules",
    "TeamModule": "finagent_core.modules",
    "WorkflowModule": "finagent_core.modules",
    "KnowledgeModule": "finagent_core.modules",
    "ReasoningModule": "finagent_core.modules",
    # Builders
    "TeamBuilder": "finagent_core.modules.team_module",
    "WorkflowBuilder": "finagent_core.modules.workflow_module",
    "KnowledgeBuilder": "finagent_core.modules.knowledge_module",
    "ReasoningBuilder": "finagent_core.modules.reasoning_module",
    # Dynamic agent loading
    "AgentLoader": "finagent_core.agent_loader",
    "AgentCard": "finagent_core.agent_loader",
    "AgentRegistry": "finagent_core.agent_loader",
    "get_loader": "finagent_core.agent_loader",
    "discover_agents": "finagent_core.agent_loader",
    "create_agent": "finagent_core.agent_loader",
    "list_agents": "finagent_core.agent_loader",
    "list_categories": "finagent_core.agent_loader",
    # SuperAgent routing
    "SuperAgent": "finagent_core.super_agent",
    "QueryIntent": "finagent_core.super_agent",
    "RouteConfig": "finagent_core.super_agent",
    "RoutingResult": "finagent_core.super_agent",
    "IntentClassifier": "finagent_core.super_agent",
    "route_query": "finagent_core.super_agent",
    "execute_query": "finagent_core.super_agent",
    # Execution planner
    "ExecutionPlanner": "finagent_core.execution_planner",
    "ExecutionPlan": "finagent_core.execution_planner",
    "PlanStep": "finagent_core.execution_planner",
    "PlanBuilder": "finagent_core.execution_planner",
    "StepStatus": "finagent_core.execution_planner",
    "StepType": "finagent_core.execution_planner",
    "create_stock_analysis_plan": "finagent_core.execution_planner",
    "execute_plan": "finagent_core.execution_planner",
    # Repository pattern
    "Repository": "finagent_core.repositories",
    "AgentSession": "finagent_core.repositories",
    "AgentMemory": "finagent_core.repositories",
    "TradeDecision": "finagent_core.repositories",
    "PerformanceSnapshot": "finagent_core.repositories",
    "AgentSessionRepository": "finagent_core.repositories",
    "AgentMemoryRepository": "finagent_core.repositories",
    "TradeDecisionRepository": "finagent_core.repositories",
    "PerformanceSnapshotRepository": "finagent_core.repositories",
    "RepositoryFactory": "finagent_core.repositories",
}

__all__ = sorted(_EXPORTS)


def __getattr__(name: str) -> Any:
    module_name = _EXPORTS.get(name)
    if module_name is None:
        raise AttributeError(f"module 'finagent_core' has no attribute {name!r}")
    value = getattr(importlib.import_module(module_name), name)
    globals()[name] = value  # cache: later lookups skip __getattr__
    return value


def __dir__():
    return sorted(set(globals()) | set(_EXPORTS))
