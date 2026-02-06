"""
Renaissance Technologies Configuration Schemas

Comprehensive configuration schemas for frontend integration.
All Agno parameters are exposed for full customization.

Usage from CLI:
    python rentech_cli.py create_agent '{"name": "Research", "model": {"temperature": 0.3}}'
    python rentech_cli.py run_team '{"name": "IC", "task": "Analyze AAPL signal"}'

Usage from Python:
    from schemas import AgentConfig, TeamConfig

    config = AgentConfig.from_json(json_string)
    agent = create_agent_from_config(config)
"""

from .agent_config import (
    # Core config
    AgentConfig,
    # Settings groups
    ModelSettings,
    MemorySettings,
    KnowledgeSettings,
    ReasoningSettings,
    ToolSettings,
    OutputSettings,
    ContextSettings,
    StreamingSettings,
    MediaSettings,
    RetrySettings,
    DebugSettings,
    # Enums
    ModelProvider,
    ReferencesFormat,
    DebugLevel,
    # Presets
    get_research_agent_config,
    get_trading_agent_config,
    get_risk_agent_config,
    # Schema
    get_agent_config_schema,
)

from .team_config import (
    # Core config
    TeamConfig,
    # Settings groups
    TeamDelegationSettings,
    TeamHistorySettings,
    TeamMemberSettings,
    # Presets
    get_research_team_config,
    get_trading_team_config,
    get_risk_team_config,
    get_investment_committee_config,
    # Schema
    get_team_config_schema,
)

__all__ = [
    # Agent config
    "AgentConfig",
    "ModelSettings",
    "MemorySettings",
    "KnowledgeSettings",
    "ReasoningSettings",
    "ToolSettings",
    "OutputSettings",
    "ContextSettings",
    "StreamingSettings",
    "MediaSettings",
    "RetrySettings",
    "DebugSettings",
    "ModelProvider",
    "ReferencesFormat",
    "DebugLevel",
    "get_research_agent_config",
    "get_trading_agent_config",
    "get_risk_agent_config",
    "get_agent_config_schema",
    # Team config
    "TeamConfig",
    "TeamDelegationSettings",
    "TeamHistorySettings",
    "TeamMemberSettings",
    "get_research_team_config",
    "get_trading_team_config",
    "get_risk_team_config",
    "get_investment_committee_config",
    "get_team_config_schema",
]
