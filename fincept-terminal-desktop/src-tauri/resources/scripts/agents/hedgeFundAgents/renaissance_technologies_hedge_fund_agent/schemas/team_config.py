"""
Team Configuration Schema

Comprehensive configuration for Agno Teams with ALL available parameters.
Designed for frontend integration - every parameter is exposed and customizable.

Usage:
    # From CLI (JSON input)
    config = TeamConfig.from_json(json_string)
    team = create_team_from_config(config)

    # From Python
    config = TeamConfig(
        name="Research Team",
        member_configs=[agent1_config, agent2_config],
        share_member_interactions=True,
    )
"""

from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Any, Union
from enum import Enum
import json

from .agent_config import (
    AgentConfig,
    ModelSettings,
    MemorySettings,
    KnowledgeSettings,
    ReasoningSettings,
    ContextSettings,
    StreamingSettings,
    MediaSettings,
    RetrySettings,
    DebugSettings,
    ModelProvider,
    ReferencesFormat,
    DebugLevel,
)


@dataclass
class TeamDelegationSettings:
    """
    Team task delegation settings.
    Controls how tasks are routed to team members.
    """
    respond_directly: bool = False
    determine_input_for_members: bool = True
    delegate_task_to_all_members: bool = False
    delegate_to_all_members: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class TeamHistorySettings:
    """
    Team history and context sharing settings.
    Controls how team members share information.
    """
    add_team_history_to_members: bool = False
    num_team_history_runs: int = 3
    share_member_interactions: bool = False
    get_member_information_tool: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class TeamMemberSettings:
    """
    Team member display settings.
    Controls how member responses are handled.
    """
    store_member_responses: bool = False
    stream_member_events: bool = True
    show_members_responses: bool = False
    add_member_tools_to_context: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class TeamConfig:
    """
    Complete Team Configuration.

    Contains ALL Agno Team parameters organized into logical groups.
    Use this for full customization when creating teams from frontend.

    Example JSON input:
    {
        "name": "Investment Committee",
        "description": "Makes trading decisions",
        "member_configs": [
            {"name": "Research Lead", "role": "research", ...},
            {"name": "Risk Officer", "role": "risk", ...}
        ],
        "delegation": {
            "determine_input_for_members": true
        },
        "history": {
            "share_member_interactions": true,
            "add_team_history_to_members": true
        }
    }
    """
    # Core identity
    name: Optional[str] = None
    id: Optional[str] = None
    role: Optional[str] = None
    description: Optional[str] = None

    # Instructions
    instructions: Optional[Union[str, List[str]]] = None
    expected_output: Optional[str] = None
    introduction: Optional[str] = None
    system_message: Optional[str] = None
    system_message_role: str = "system"

    # Member configurations (list of AgentConfig)
    member_configs: List[AgentConfig] = field(default_factory=list)

    # Grouped settings
    model: ModelSettings = field(default_factory=ModelSettings)
    delegation: TeamDelegationSettings = field(default_factory=TeamDelegationSettings)
    history: TeamHistorySettings = field(default_factory=TeamHistorySettings)
    member_display: TeamMemberSettings = field(default_factory=TeamMemberSettings)
    memory: MemorySettings = field(default_factory=MemorySettings)
    knowledge: KnowledgeSettings = field(default_factory=KnowledgeSettings)
    reasoning: ReasoningSettings = field(default_factory=ReasoningSettings)
    context: ContextSettings = field(default_factory=ContextSettings)
    streaming: StreamingSettings = field(default_factory=StreamingSettings)
    media: MediaSettings = field(default_factory=MediaSettings)
    retry: RetrySettings = field(default_factory=RetrySettings)
    debug: DebugSettings = field(default_factory=DebugSettings)

    # Metadata
    metadata: Optional[Dict[str, Any]] = None
    dependencies: Optional[Dict[str, Any]] = None

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "TeamConfig":
        """Create TeamConfig from dictionary (e.g., parsed JSON)"""
        # Extract member configs
        member_configs_data = data.pop("member_configs", [])
        member_configs = [
            AgentConfig.from_dict(m) if isinstance(m, dict) else m
            for m in member_configs_data
        ]

        # Extract nested settings
        model_data = data.pop("model", {})
        delegation_data = data.pop("delegation", {})
        history_data = data.pop("history", {})
        member_display_data = data.pop("member_display", {})
        memory_data = data.pop("memory", {})
        knowledge_data = data.pop("knowledge", {})
        reasoning_data = data.pop("reasoning", {})
        context_data = data.pop("context", {})
        streaming_data = data.pop("streaming", {})
        media_data = data.pop("media", {})
        retry_data = data.pop("retry", {})
        debug_data = data.pop("debug", {})

        # Handle enum conversions
        if "provider" in model_data and isinstance(model_data["provider"], str):
            model_data["provider"] = ModelProvider(model_data["provider"])
        if "references_format" in knowledge_data and isinstance(knowledge_data["references_format"], str):
            knowledge_data["references_format"] = ReferencesFormat(knowledge_data["references_format"])
        if "debug_level" in debug_data and isinstance(debug_data["debug_level"], int):
            debug_data["debug_level"] = DebugLevel(debug_data["debug_level"])

        return cls(
            **data,
            member_configs=member_configs,
            model=ModelSettings(**model_data) if model_data else ModelSettings(),
            delegation=TeamDelegationSettings(**delegation_data) if delegation_data else TeamDelegationSettings(),
            history=TeamHistorySettings(**history_data) if history_data else TeamHistorySettings(),
            member_display=TeamMemberSettings(**member_display_data) if member_display_data else TeamMemberSettings(),
            memory=MemorySettings(**memory_data) if memory_data else MemorySettings(),
            knowledge=KnowledgeSettings(**knowledge_data) if knowledge_data else KnowledgeSettings(),
            reasoning=ReasoningSettings(**reasoning_data) if reasoning_data else ReasoningSettings(),
            context=ContextSettings(**context_data) if context_data else ContextSettings(),
            streaming=StreamingSettings(**streaming_data) if streaming_data else StreamingSettings(),
            media=MediaSettings(**media_data) if media_data else MediaSettings(),
            retry=RetrySettings(**retry_data) if retry_data else RetrySettings(),
            debug=DebugSettings(**debug_data) if debug_data else DebugSettings(),
        )

    @classmethod
    def from_json(cls, json_str: str) -> "TeamConfig":
        """Create TeamConfig from JSON string"""
        return cls.from_dict(json.loads(json_str))

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization"""
        result = {
            "name": self.name,
            "id": self.id,
            "role": self.role,
            "description": self.description,
            "instructions": self.instructions,
            "expected_output": self.expected_output,
            "introduction": self.introduction,
            "system_message": self.system_message,
            "system_message_role": self.system_message_role,
            "member_configs": [m.to_dict() for m in self.member_configs],
            "model": self.model.to_dict(),
            "delegation": self.delegation.to_dict(),
            "history": self.history.to_dict(),
            "member_display": self.member_display.to_dict(),
            "memory": self.memory.to_dict(),
            "knowledge": self.knowledge.to_dict(),
            "reasoning": self.reasoning.to_dict(),
            "context": self.context.to_dict(),
            "streaming": self.streaming.to_dict(),
            "media": self.media.to_dict(),
            "retry": self.retry.to_dict(),
            "debug": self.debug.to_dict(),
            "metadata": self.metadata,
            "dependencies": self.dependencies,
        }
        return {k: v for k, v in result.items() if v is not None}

    def to_json(self, indent: int = 2) -> str:
        """Convert to JSON string"""
        return json.dumps(self.to_dict(), indent=indent, default=str)

    def to_agno_kwargs(self) -> Dict[str, Any]:
        """
        Convert to kwargs dict for Agno Team constructor.
        Flattens the nested structure into Agno's flat parameter list.

        Note: 'members' must be created separately from member_configs
        """
        kwargs = {}

        # Core identity
        if self.name:
            kwargs["name"] = self.name
        if self.id:
            kwargs["id"] = self.id
        if self.role:
            kwargs["role"] = self.role
        if self.description:
            kwargs["description"] = self.description
        if self.instructions:
            kwargs["instructions"] = self.instructions
        if self.expected_output:
            kwargs["expected_output"] = self.expected_output
        if self.introduction:
            kwargs["introduction"] = self.introduction
        if self.system_message:
            kwargs["system_message"] = self.system_message
        kwargs["system_message_role"] = self.system_message_role

        # Delegation settings
        kwargs["respond_directly"] = self.delegation.respond_directly
        kwargs["determine_input_for_members"] = self.delegation.determine_input_for_members
        kwargs["delegate_task_to_all_members"] = self.delegation.delegate_task_to_all_members
        kwargs["delegate_to_all_members"] = self.delegation.delegate_to_all_members

        # History settings
        kwargs["add_team_history_to_members"] = self.history.add_team_history_to_members
        kwargs["num_team_history_runs"] = self.history.num_team_history_runs
        kwargs["share_member_interactions"] = self.history.share_member_interactions
        kwargs["get_member_information_tool"] = self.history.get_member_information_tool

        # Member display settings
        kwargs["store_member_responses"] = self.member_display.store_member_responses
        kwargs["stream_member_events"] = self.member_display.stream_member_events
        kwargs["show_members_responses"] = self.member_display.show_members_responses
        kwargs["add_member_tools_to_context"] = self.member_display.add_member_tools_to_context

        # Memory settings
        if self.memory.user_id:
            kwargs["user_id"] = self.memory.user_id
        if self.memory.session_id:
            kwargs["session_id"] = self.memory.session_id
        if self.memory.session_state:
            kwargs["session_state"] = self.memory.session_state
        kwargs["add_session_state_to_context"] = self.memory.add_session_state_to_context
        kwargs["overwrite_db_session_state"] = self.memory.overwrite_db_session_state
        kwargs["enable_agentic_state"] = self.memory.enable_agentic_state
        kwargs["cache_session"] = self.memory.cache_session
        kwargs["enable_agentic_memory"] = self.memory.enable_agentic_memory
        kwargs["enable_user_memories"] = self.memory.enable_user_memories
        if self.memory.add_memories_to_context is not None:
            kwargs["add_memories_to_context"] = self.memory.add_memories_to_context
        kwargs["enable_session_summaries"] = self.memory.enable_session_summaries
        if self.memory.add_session_summary_to_context is not None:
            kwargs["add_session_summary_to_context"] = self.memory.add_session_summary_to_context
        kwargs["search_session_history"] = self.memory.search_session_history
        if self.memory.num_history_sessions:
            kwargs["num_history_sessions"] = self.memory.num_history_sessions
        kwargs["add_history_to_context"] = self.memory.add_history_to_context
        if self.memory.num_history_runs:
            kwargs["num_history_runs"] = self.memory.num_history_runs
        if self.memory.num_history_messages:
            kwargs["num_history_messages"] = self.memory.num_history_messages
        if self.memory.max_tool_calls_from_history:
            kwargs["max_tool_calls_from_history"] = self.memory.max_tool_calls_from_history
        kwargs["compress_tool_results"] = self.memory.compress_tool_results

        # Knowledge settings
        if self.knowledge.knowledge_filters:
            kwargs["knowledge_filters"] = self.knowledge.knowledge_filters
        kwargs["add_knowledge_to_context"] = self.knowledge.add_knowledge_to_context
        if self.knowledge.enable_agentic_knowledge_filters is not None:
            kwargs["enable_agentic_knowledge_filters"] = self.knowledge.enable_agentic_knowledge_filters
        kwargs["update_knowledge"] = self.knowledge.update_knowledge
        kwargs["search_knowledge"] = self.knowledge.search_knowledge
        kwargs["references_format"] = self.knowledge.references_format.value

        # Reasoning settings
        kwargs["reasoning"] = self.reasoning.reasoning
        kwargs["reasoning_min_steps"] = self.reasoning.reasoning_min_steps
        kwargs["reasoning_max_steps"] = self.reasoning.reasoning_max_steps

        # Context settings
        kwargs["resolve_in_context"] = self.context.resolve_in_context
        kwargs["add_name_to_context"] = self.context.add_name_to_context
        kwargs["add_datetime_to_context"] = self.context.add_datetime_to_context
        kwargs["add_location_to_context"] = self.context.add_location_to_context
        if self.context.timezone_identifier:
            kwargs["timezone_identifier"] = self.context.timezone_identifier
        kwargs["add_dependencies_to_context"] = self.context.add_dependencies_to_context
        if self.context.additional_context:
            kwargs["additional_context"] = self.context.additional_context

        # Streaming settings
        if self.streaming.stream is not None:
            kwargs["stream"] = self.streaming.stream
        if self.streaming.stream_events is not None:
            kwargs["stream_events"] = self.streaming.stream_events
        if self.streaming.stream_intermediate_steps is not None:
            kwargs["stream_intermediate_steps"] = self.streaming.stream_intermediate_steps
        kwargs["store_events"] = self.streaming.store_events
        if self.streaming.events_to_skip:
            kwargs["events_to_skip"] = self.streaming.events_to_skip

        # Media settings
        kwargs["store_media"] = self.media.store_media
        kwargs["store_tool_messages"] = self.media.store_tool_messages
        kwargs["store_history_messages"] = self.media.store_history_messages
        kwargs["send_media_to_model"] = self.media.send_media_to_model

        # Retry settings
        kwargs["retries"] = self.retry.retries
        kwargs["delay_between_retries"] = self.retry.delay_between_retries
        kwargs["exponential_backoff"] = self.retry.exponential_backoff

        # Debug settings
        kwargs["debug_mode"] = self.debug.debug_mode
        kwargs["debug_level"] = self.debug.debug_level.value
        kwargs["telemetry"] = self.debug.telemetry

        # Output
        kwargs["markdown"] = True  # Default for teams

        # Metadata
        if self.metadata:
            kwargs["metadata"] = self.metadata
        if self.dependencies:
            kwargs["dependencies"] = self.dependencies

        return kwargs


# =============================================================================
# PRESET TEAM CONFIGURATIONS
# =============================================================================

def get_research_team_config() -> TeamConfig:
    """Preset config for research team"""
    from .agent_config import get_research_agent_config

    return TeamConfig(
        name="Quantitative Research Team",
        role="research",
        description="Alpha generation team for signal discovery and validation",
        instructions=[
            "Collaborate on signal research",
            "Validate statistical significance",
            "Share findings with precision",
        ],
        member_configs=[
            get_research_agent_config(),
        ],
        history=TeamHistorySettings(
            share_member_interactions=True,
            add_team_history_to_members=True,
        ),
        member_display=TeamMemberSettings(
            show_members_responses=True,
        ),
    )


def get_trading_team_config() -> TeamConfig:
    """Preset config for trading team"""
    from .agent_config import get_trading_agent_config

    return TeamConfig(
        name="Trading Execution Team",
        role="execution",
        description="Trade execution and market making team",
        instructions=[
            "Optimize trade execution",
            "Minimize market impact",
            "Coordinate order routing",
        ],
        member_configs=[
            get_trading_agent_config(),
        ],
        history=TeamHistorySettings(
            share_member_interactions=True,
        ),
    )


def get_risk_team_config() -> TeamConfig:
    """Preset config for risk team"""
    from .agent_config import get_risk_agent_config

    return TeamConfig(
        name="Risk Management Team",
        role="risk",
        description="Portfolio risk monitoring and compliance",
        instructions=[
            "Monitor all risk limits",
            "Flag breaches immediately",
            "Provide risk assessments",
        ],
        member_configs=[
            get_risk_agent_config(),
        ],
        reasoning=ReasoningSettings(
            reasoning=True,
            reasoning_max_steps=3,
        ),
    )


def get_investment_committee_config() -> TeamConfig:
    """Preset config for full Investment Committee"""
    from .agent_config import (
        get_research_agent_config,
        get_trading_agent_config,
        get_risk_agent_config,
    )

    pm_config = AgentConfig(
        name="Portfolio Manager",
        role="decision_maker",
        description="Makes final trading decisions",
        instructions=[
            "Synthesize team inputs",
            "Make APPROVE/REJECT decisions",
            "Set position sizes",
        ],
        model=ModelSettings(temperature=0.3),
        output=OutputSettings(markdown=True),
    )

    return TeamConfig(
        name="Investment Committee",
        role="decision",
        description="Top-level decision-making body for trading decisions",
        instructions=[
            "Research validates signal",
            "Risk assesses impact",
            "Trading plans execution",
            "PM makes final decision",
        ],
        member_configs=[
            pm_config,
            get_research_agent_config(),
            get_risk_agent_config(),
            get_trading_agent_config(),
        ],
        history=TeamHistorySettings(
            share_member_interactions=True,
            add_team_history_to_members=True,
        ),
        member_display=TeamMemberSettings(
            show_members_responses=True,
        ),
        reasoning=ReasoningSettings(
            reasoning=True,
            reasoning_max_steps=5,
        ),
    )


# =============================================================================
# SCHEMA EXPORT (for frontend)
# =============================================================================

def get_team_config_schema() -> Dict[str, Any]:
    """
    Get JSON schema for TeamConfig.
    This can be sent to frontend for form generation.
    """
    return {
        "type": "object",
        "title": "Team Configuration",
        "description": "Complete configuration for creating an Agno Team",
        "properties": {
            "name": {"type": "string", "description": "Team name"},
            "id": {"type": "string", "description": "Unique team ID"},
            "role": {"type": "string", "description": "Team role"},
            "description": {"type": "string", "description": "Team description"},
            "instructions": {
                "oneOf": [
                    {"type": "string"},
                    {"type": "array", "items": {"type": "string"}}
                ],
                "description": "Team instructions"
            },
            "member_configs": {
                "type": "array",
                "items": {"$ref": "#/definitions/AgentConfig"},
                "description": "Configuration for each team member"
            },
            "delegation": {
                "type": "object",
                "properties": {
                    "respond_directly": {"type": "boolean", "default": False},
                    "determine_input_for_members": {"type": "boolean", "default": True},
                    "delegate_task_to_all_members": {"type": "boolean", "default": False},
                }
            },
            "history": {
                "type": "object",
                "properties": {
                    "share_member_interactions": {"type": "boolean", "default": False},
                    "add_team_history_to_members": {"type": "boolean", "default": False},
                    "num_team_history_runs": {"type": "integer", "default": 3},
                }
            },
            "member_display": {
                "type": "object",
                "properties": {
                    "show_members_responses": {"type": "boolean", "default": False},
                    "store_member_responses": {"type": "boolean", "default": False},
                }
            }
        }
    }
