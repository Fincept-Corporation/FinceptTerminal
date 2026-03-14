"""
Agent Configuration Schema

Comprehensive configuration for Agno Agents with ALL available parameters.
Designed for frontend integration - every parameter is exposed and customizable.

Usage:
    # From CLI (JSON input)
    config = AgentConfig.from_json(json_string)
    agent = create_agent_from_config(config)

    # From Python (uses dynamic defaults from frontend config)
    config = AgentConfig(
        name="My Agent",
        instructions=["Be helpful"],
    )
"""

from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Any, Union, Literal
from enum import Enum
import json


class ModelProvider(str, Enum):
    """Supported LLM providers"""
    OPENAI = "openai"
    ANTHROPIC = "anthropic"
    OLLAMA = "ollama"
    GROQ = "groq"
    ZHIPUAI = "zhipuai"
    OPENAI_LIKE = "openai_like"


class ReferencesFormat(str, Enum):
    """Format for knowledge references"""
    JSON = "json"
    YAML = "yaml"


class DebugLevel(int, Enum):
    """Debug verbosity levels"""
    MINIMAL = 1
    VERBOSE = 2


def _get_default_provider() -> str:
    """Get default provider from frontend config"""
    try:
        # Import here to avoid circular dependency
        import sys
        import os
        parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if parent_dir not in sys.path:
            sys.path.insert(0, parent_dir)
        from utils.model_factory import get_default_model_settings
        return get_default_model_settings()["provider"]
    except Exception:
        return "openai"  # Final fallback


def _get_default_model_id() -> str:
    """Get default model ID from frontend config"""
    try:
        import sys
        import os
        parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if parent_dir not in sys.path:
            sys.path.insert(0, parent_dir)
        from utils.model_factory import get_default_model_settings
        return get_default_model_settings()["model_id"]
    except Exception:
        return "gpt-4o"  # Final fallback


def _get_default_temperature() -> float:
    """Get default temperature from frontend config"""
    try:
        import sys
        import os
        parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if parent_dir not in sys.path:
            sys.path.insert(0, parent_dir)
        from utils.model_factory import get_default_model_settings
        return get_default_model_settings()["temperature"]
    except Exception:
        return 0.7


def _get_default_max_tokens() -> int:
    """Get default max_tokens from frontend config"""
    try:
        import sys
        import os
        parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if parent_dir not in sys.path:
            sys.path.insert(0, parent_dir)
        from utils.model_factory import get_default_model_settings
        return get_default_model_settings()["max_tokens"]
    except Exception:
        return 4096


@dataclass
class ModelSettings:
    """
    Model-specific settings.
    Configure the LLM provider and model parameters.

    Defaults are loaded dynamically from frontend configuration.
    If no frontend config exists, falls back to OpenAI GPT-4o.
    """
    # Provider selection (loaded from frontend config)
    provider: str = field(default_factory=_get_default_provider)
    model_id: str = field(default_factory=_get_default_model_id)

    # API configuration
    api_key: Optional[str] = None
    base_url: Optional[str] = None

    # Generation parameters (loaded from frontend config)
    temperature: float = field(default_factory=_get_default_temperature)
    max_tokens: int = field(default_factory=_get_default_max_tokens)
    top_p: Optional[float] = None
    frequency_penalty: Optional[float] = None
    presence_penalty: Optional[float] = None
    stop_sequences: Optional[List[str]] = None

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class MemorySettings:
    """
    Memory and session management settings.
    Controls how agents remember and learn.
    """
    # Session management
    user_id: Optional[str] = None
    session_id: Optional[str] = None
    session_state: Optional[Dict[str, Any]] = None
    add_session_state_to_context: bool = False
    overwrite_db_session_state: bool = False
    enable_agentic_state: bool = False
    cache_session: bool = False

    # Memory features
    enable_agentic_memory: bool = False
    enable_user_memories: bool = False
    add_memories_to_context: Optional[bool] = None

    # Session summaries
    enable_session_summaries: bool = False
    add_session_summary_to_context: Optional[bool] = None

    # History search
    search_session_history: bool = False
    num_history_sessions: Optional[int] = None

    # Context history
    add_history_to_context: bool = False
    num_history_runs: Optional[int] = None
    num_history_messages: Optional[int] = None
    max_tool_calls_from_history: Optional[int] = None

    # Compression
    compress_tool_results: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class KnowledgeSettings:
    """
    Knowledge base settings.
    Controls how agents access and use knowledge.
    """
    # Knowledge base (set at runtime if using external KB)
    knowledge_filters: Optional[Dict[str, Any]] = None
    add_knowledge_to_context: bool = False
    enable_agentic_knowledge_filters: Optional[bool] = None
    update_knowledge: bool = False
    search_knowledge: bool = True
    references_format: ReferencesFormat = ReferencesFormat.JSON

    def to_dict(self) -> Dict[str, Any]:
        result = {}
        for k, v in asdict(self).items():
            if v is not None:
                if isinstance(v, Enum):
                    result[k] = v.value
                else:
                    result[k] = v
        return result


@dataclass
class ReasoningSettings:
    """
    Reasoning and chain-of-thought settings.
    Controls advanced reasoning capabilities.
    """
    reasoning: bool = False
    reasoning_min_steps: int = 1
    reasoning_max_steps: int = 10
    # reasoning_model and reasoning_agent set at runtime

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class ToolSettings:
    """
    Tool usage settings.
    Controls how agents use tools and functions.
    """
    # Tool limits
    tool_call_limit: Optional[int] = None
    tool_choice: Optional[Union[str, Dict[str, Any]]] = None

    # Tool behavior
    read_tool_call_history: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class OutputSettings:
    """
    Output and parsing settings.
    Controls response format and parsing.
    """
    # Response format
    markdown: bool = False
    use_json_mode: bool = False
    parse_response: bool = True

    # Schema enforcement (set input_schema/output_schema at runtime)
    structured_outputs: Optional[bool] = None

    # File output
    save_response_to_file: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class ContextSettings:
    """
    Context building settings.
    Controls what information is added to agent context.
    """
    build_context: bool = True
    build_user_context: bool = True
    resolve_in_context: bool = True

    # Context additions
    add_name_to_context: bool = False
    add_datetime_to_context: bool = False
    add_location_to_context: bool = False
    timezone_identifier: Optional[str] = None
    add_dependencies_to_context: bool = False

    # Additional context (free-form)
    additional_context: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class StreamingSettings:
    """
    Streaming and event settings.
    Controls real-time response streaming.
    """
    stream: Optional[bool] = None
    stream_events: Optional[bool] = None
    stream_intermediate_steps: Optional[bool] = None
    store_events: bool = False
    events_to_skip: Optional[List[str]] = None

    def to_dict(self) -> Dict[str, Any]:
        return {k: v for k, v in asdict(self).items() if v is not None}


@dataclass
class MediaSettings:
    """
    Media handling settings.
    Controls how agents handle files and media.
    """
    store_media: bool = True
    store_tool_messages: bool = True
    store_history_messages: bool = True
    send_media_to_model: bool = True

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class RetrySettings:
    """
    Retry and error handling settings.
    Controls retry behavior on failures.
    """
    retries: int = 0
    delay_between_retries: int = 1
    exponential_backoff: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class DebugSettings:
    """
    Debug and telemetry settings.
    Controls logging and observability.
    """
    debug_mode: bool = False
    debug_level: DebugLevel = DebugLevel.MINIMAL
    telemetry: bool = True

    def to_dict(self) -> Dict[str, Any]:
        return {
            "debug_mode": self.debug_mode,
            "debug_level": self.debug_level.value,
            "telemetry": self.telemetry,
        }


@dataclass
class AgentConfig:
    """
    Complete Agent Configuration.

    Contains ALL Agno Agent parameters organized into logical groups.
    Use this for full customization when creating agents from frontend.

    Example JSON input:
    {
        "name": "Research Analyst",
        "role": "signal_validation",
        "description": "Validates trading signals",
        "instructions": ["Analyze signals", "Check p-values"],
        "model": {
            "temperature": 0.7
        },
        "memory": {
            "enable_agentic_memory": true
        },
        "reasoning": {
            "reasoning": true,
            "reasoning_max_steps": 5
        }
    }

    Note: provider and model_id are loaded from frontend config if not specified.
    """
    # Core identity
    name: Optional[str] = None
    id: Optional[str] = None
    role: Optional[str] = None
    description: Optional[str] = None

    # Instructions (can be string or list)
    instructions: Optional[Union[str, List[str]]] = None
    expected_output: Optional[str] = None
    introduction: Optional[str] = None
    system_message: Optional[str] = None
    system_message_role: str = "system"
    user_message_role: str = "user"

    # Grouped settings
    model: ModelSettings = field(default_factory=ModelSettings)
    memory: MemorySettings = field(default_factory=MemorySettings)
    knowledge: KnowledgeSettings = field(default_factory=KnowledgeSettings)
    reasoning: ReasoningSettings = field(default_factory=ReasoningSettings)
    tools: ToolSettings = field(default_factory=ToolSettings)
    output: OutputSettings = field(default_factory=OutputSettings)
    context: ContextSettings = field(default_factory=ContextSettings)
    streaming: StreamingSettings = field(default_factory=StreamingSettings)
    media: MediaSettings = field(default_factory=MediaSettings)
    retry: RetrySettings = field(default_factory=RetrySettings)
    debug: DebugSettings = field(default_factory=DebugSettings)

    # Metadata
    metadata: Optional[Dict[str, Any]] = None
    dependencies: Optional[Dict[str, Any]] = None

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "AgentConfig":
        """Create AgentConfig from dictionary (e.g., parsed JSON)"""
        # Extract nested settings
        model_data = data.pop("model", {})
        memory_data = data.pop("memory", {})
        knowledge_data = data.pop("knowledge", {})
        reasoning_data = data.pop("reasoning", {})
        tools_data = data.pop("tools", {})
        output_data = data.pop("output", {})
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
            model=ModelSettings(**model_data) if model_data else ModelSettings(),
            memory=MemorySettings(**memory_data) if memory_data else MemorySettings(),
            knowledge=KnowledgeSettings(**knowledge_data) if knowledge_data else KnowledgeSettings(),
            reasoning=ReasoningSettings(**reasoning_data) if reasoning_data else ReasoningSettings(),
            tools=ToolSettings(**tools_data) if tools_data else ToolSettings(),
            output=OutputSettings(**output_data) if output_data else OutputSettings(),
            context=ContextSettings(**context_data) if context_data else ContextSettings(),
            streaming=StreamingSettings(**streaming_data) if streaming_data else StreamingSettings(),
            media=MediaSettings(**media_data) if media_data else MediaSettings(),
            retry=RetrySettings(**retry_data) if retry_data else RetrySettings(),
            debug=DebugSettings(**debug_data) if debug_data else DebugSettings(),
        )

    @classmethod
    def from_json(cls, json_str: str) -> "AgentConfig":
        """Create AgentConfig from JSON string"""
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
            "user_message_role": self.user_message_role,
            "model": self.model.to_dict(),
            "memory": self.memory.to_dict(),
            "knowledge": self.knowledge.to_dict(),
            "reasoning": self.reasoning.to_dict(),
            "tools": self.tools.to_dict(),
            "output": self.output.to_dict(),
            "context": self.context.to_dict(),
            "streaming": self.streaming.to_dict(),
            "media": self.media.to_dict(),
            "retry": self.retry.to_dict(),
            "debug": self.debug.to_dict(),
            "metadata": self.metadata,
            "dependencies": self.dependencies,
        }
        # Remove None values at top level
        return {k: v for k, v in result.items() if v is not None}

    def to_json(self, indent: int = 2) -> str:
        """Convert to JSON string"""
        return json.dumps(self.to_dict(), indent=indent, default=str)

    def to_agno_kwargs(self) -> Dict[str, Any]:
        """
        Convert to kwargs dict for Agno Agent constructor.
        Flattens the nested structure into Agno's flat parameter list.
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
        kwargs["user_message_role"] = self.user_message_role

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

        # Tool settings
        if self.tools.tool_call_limit:
            kwargs["tool_call_limit"] = self.tools.tool_call_limit
        if self.tools.tool_choice:
            kwargs["tool_choice"] = self.tools.tool_choice
        kwargs["read_tool_call_history"] = self.tools.read_tool_call_history

        # Output settings
        kwargs["markdown"] = self.output.markdown
        kwargs["use_json_mode"] = self.output.use_json_mode
        kwargs["parse_response"] = self.output.parse_response
        if self.output.structured_outputs is not None:
            kwargs["structured_outputs"] = self.output.structured_outputs
        if self.output.save_response_to_file:
            kwargs["save_response_to_file"] = self.output.save_response_to_file

        # Context settings
        kwargs["build_context"] = self.context.build_context
        kwargs["build_user_context"] = self.context.build_user_context
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

        # Metadata
        if self.metadata:
            kwargs["metadata"] = self.metadata
        if self.dependencies:
            kwargs["dependencies"] = self.dependencies

        return kwargs


# =============================================================================
# PRESET CONFIGURATIONS
# =============================================================================

def get_research_agent_config() -> AgentConfig:
    """
    Preset config for research/analysis agents.

    Uses dynamic model settings from frontend config.
    Only overrides temperature for analytical tasks.
    """
    return AgentConfig(
        name="Research Agent",
        role="research",
        description="Quantitative research and signal analysis",
        instructions=[
            "Analyze data with statistical rigor",
            "Focus on p-values and significance",
            "Provide quantitative assessments",
        ],
        model=ModelSettings(
            temperature=0.3,  # Lower for analytical tasks
        ),
        reasoning=ReasoningSettings(
            reasoning=True,
            reasoning_max_steps=5,
        ),
        output=OutputSettings(
            markdown=True,
        ),
    )


def get_trading_agent_config() -> AgentConfig:
    """
    Preset config for trading/execution agents.

    Uses dynamic model settings from frontend config.
    """
    return AgentConfig(
        name="Trading Agent",
        role="execution",
        description="Trade execution and market analysis",
        instructions=[
            "Optimize execution with minimal market impact",
            "Consider liquidity and timing",
            "Provide clear trade recommendations",
        ],
        model=ModelSettings(
            temperature=0.5,
        ),
        output=OutputSettings(
            markdown=True,
        ),
    )


def get_risk_agent_config() -> AgentConfig:
    """
    Preset config for risk management agents.

    Uses dynamic model settings from frontend config.
    Overrides temperature to be very deterministic for risk assessment.
    """
    return AgentConfig(
        name="Risk Agent",
        role="risk_management",
        description="Portfolio risk assessment and monitoring",
        instructions=[
            "Assess VaR, drawdown, and concentration risk",
            "Flag any limit breaches immediately",
            "Provide risk-adjusted recommendations",
        ],
        model=ModelSettings(
            temperature=0.2,  # Very deterministic for risk
        ),
        reasoning=ReasoningSettings(
            reasoning=True,
            reasoning_max_steps=3,
        ),
        output=OutputSettings(
            markdown=True,
        ),
    )


# =============================================================================
# SCHEMA EXPORT (for frontend)
# =============================================================================

def get_agent_config_schema() -> Dict[str, Any]:
    """
    Get JSON schema for AgentConfig.
    This can be sent to frontend for form generation.
    """
    return {
        "type": "object",
        "title": "Agent Configuration",
        "description": "Complete configuration for creating an Agno Agent",
        "properties": {
            "name": {"type": "string", "description": "Agent name"},
            "id": {"type": "string", "description": "Unique agent ID"},
            "role": {"type": "string", "description": "Agent role (e.g., research, trading, risk)"},
            "description": {"type": "string", "description": "Agent description"},
            "instructions": {
                "oneOf": [
                    {"type": "string"},
                    {"type": "array", "items": {"type": "string"}}
                ],
                "description": "Agent instructions"
            },
            "model": {
                "type": "object",
                "description": "Model settings. Defaults loaded from frontend config.",
                "properties": {
                    "provider": {
                        "type": "string",
                        "description": "LLM provider. Defaults to active frontend config.",
                        "enum": ["openai", "anthropic", "google", "groq", "ollama", "mistral", "deepseek", "cohere", "x-ai", "perplexity", "openrouter", "openai_like"]
                    },
                    "model_id": {
                        "type": "string",
                        "description": "Model ID. Defaults to active frontend config."
                    },
                    "api_key": {"type": "string", "description": "Optional API key override"},
                    "base_url": {"type": "string", "description": "Optional base URL override"},
                    "temperature": {
                        "type": "number",
                        "minimum": 0,
                        "maximum": 2,
                        "description": "Sampling temperature. Defaults to frontend config."
                    },
                    "max_tokens": {
                        "type": "integer",
                        "description": "Max output tokens. Defaults to frontend config."
                    },
                }
            },
            "memory": {
                "type": "object",
                "properties": {
                    "enable_agentic_memory": {"type": "boolean", "default": False},
                    "enable_user_memories": {"type": "boolean", "default": False},
                    "enable_session_summaries": {"type": "boolean", "default": False},
                }
            },
            "reasoning": {
                "type": "object",
                "properties": {
                    "reasoning": {"type": "boolean", "default": False},
                    "reasoning_min_steps": {"type": "integer", "default": 1},
                    "reasoning_max_steps": {"type": "integer", "default": 10},
                }
            },
            "output": {
                "type": "object",
                "properties": {
                    "markdown": {"type": "boolean", "default": False},
                    "use_json_mode": {"type": "boolean", "default": False},
                }
            },
            "debug": {
                "type": "object",
                "properties": {
                    "debug_mode": {"type": "boolean", "default": False},
                    "debug_level": {"type": "integer", "enum": [1, 2], "default": 1},
                }
            }
        }
    }
