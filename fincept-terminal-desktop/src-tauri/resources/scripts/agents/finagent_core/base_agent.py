"""
Base Agent - Core configurable agent class for all Fincept Terminal agents

This module provides the foundational ConfigurableAgent class that replaces
all static agent implementations. All agents (geopolitics, investors, economic, etc.)
will extend or use this class.
"""

import json
from typing import Dict, Any, List, Optional, Union
from pathlib import Path
from pydantic import BaseModel, Field
from agno.agent import Agent
from agno.run import RunResponse

from finagent_core.config.llm_providers import LLMProviderManager
from finagent_core.tools.tool_registry import ToolRegistry, get_tool_registry
from finagent_core.database.db_manager import DatabaseManager
from finagent_core.utils.logger import get_logger
from finagent_core.utils.path_resolver import PathResolver

logger = get_logger(__name__)


class AgentConfig(BaseModel):
    """
    Configuration for an Agno-based agent

    This model defines all configurable parameters for agents,
    making them fully customizable via JSON configuration files.
    """

    id: str = Field(..., description="Unique agent identifier")
    name: str = Field(..., description="Human-readable agent name")
    role: str = Field(..., description="Agent's role/specialty")
    goal: str = Field(..., description="Agent's primary goal")
    description: str = Field(..., description="Detailed agent description")

    # LLM Configuration
    llm_config: Dict[str, Any] = Field(
        ...,
        description="LLM configuration (provider, model_id, temperature, max_tokens)"
    )

    # Tools and Capabilities
    tools: List[str] = Field(
        default_factory=list,
        description="List of tool names this agent can use"
    )

    knowledge_base: Optional[str] = Field(
        None,
        description="Knowledge base identifier for RAG"
    )

    # Memory Configuration
    enable_memory: bool = Field(
        True,
        description="Enable persistent memory across sessions"
    )

    enable_agentic_memory: bool = Field(
        False,
        description="Enable agent-controlled memory decisions"
    )

    # Agent Behavior
    instructions: str = Field(
        ...,
        description="Detailed instructions for agent behavior"
    )

    output_schema: Optional[Dict[str, Any]] = Field(
        None,
        description="Expected output schema (Pydantic model definition)"
    )

    # UI Configuration
    ui_parameters: Optional[Dict[str, Any]] = Field(
        None,
        description="UI-specific configuration for frontend display"
    )

    # Additional Settings
    show_tool_calls: bool = Field(True, description="Display tool calls")
    markdown: bool = Field(True, description="Enable markdown formatting")
    debug_mode: bool = Field(False, description="Enable debug logging")

    class Config:
        json_schema_extra = {
            "example": {
                "id": "warren_buffett_agent",
                "name": "Warren Buffett Investment Agent",
                "role": "Value investor following Warren Buffett's philosophy",
                "goal": "Identify wonderful companies at fair prices",
                "description": "Analyzes stocks through Buffett's framework",
                "llm_config": {
                    "provider": "openai",
                    "model_id": "gpt-4-turbo",
                    "temperature": 0.6,
                    "max_tokens": 3000
                },
                "tools": ["financial_metrics_tool", "stock_price_tool"],
                "enable_memory": True,
                "instructions": "Analyze stocks using Buffett's criteria..."
            }
        }


class ConfigurableAgent:
    """
    Base configurable agent class for all Fincept Terminal agents

    This class wraps Agno's Agent with Fincept-specific functionality,
    making it easy to create and configure agents dynamically.

    Usage:
        config = AgentConfig(...)
        agent = ConfigurableAgent(config)
        response = agent.run("Analyze AAPL stock")
    """

    def __init__(
        self,
        config: Union[AgentConfig, Dict[str, Any]],
        llm_provider_manager: Optional[LLMProviderManager] = None,
        tool_registry: Optional[ToolRegistry] = None,
        db_manager: Optional[DatabaseManager] = None,
        path_resolver: Optional[PathResolver] = None,
        api_keys: Optional[Dict[str, str]] = None
    ):
        """
        Initialize configurable agent

        Args:
            config: Agent configuration (AgentConfig or dict)
            llm_provider_manager: LLM provider manager (uses default if None)
            tool_registry: Tool registry (uses global if None)
            db_manager: Database manager (creates default if None)
            path_resolver: Path resolver (creates default if None)
            api_keys: API keys for LLM providers
        """
        # Convert dict to AgentConfig if needed
        if isinstance(config, dict):
            config = AgentConfig(**config)

        self.config = config
        self.api_keys = api_keys or {}

        # Initialize managers
        self.llm_provider_manager = llm_provider_manager or LLMProviderManager()
        self.tool_registry = tool_registry or get_tool_registry()
        self.db_manager = db_manager or DatabaseManager()
        self.path_resolver = path_resolver or PathResolver()

        # Set up logger
        self.logger = get_logger(f"Agent.{config.id}")
        self.logger.info(f"Initializing agent: {config.name}")

        # Create Agno agent
        self.agent = self._create_agent()

        self.logger.info(f"Agent initialized: {config.id}")

    def _create_agent(self) -> Agent:
        """Create Agno Agent instance from configuration"""

        # Get LLM model
        llm_cfg = self.config.llm_config
        api_key = self.api_keys.get(f"{llm_cfg['provider'].upper()}_API_KEY")

        model = self.llm_provider_manager.get_model(
            provider=llm_cfg["provider"],
            model_id=llm_cfg["model_id"],
            api_key=api_key,
            temperature=llm_cfg.get("temperature", 0.7),
            max_tokens=llm_cfg.get("max_tokens", 4096)
        )

        # Get tools
        tools = self.tool_registry.get_tools(self.config.tools)

        # Get database
        db = self.db_manager.get_database()

        # TODO: Get knowledge base (implement when knowledge system is added)
        knowledge = None

        # Create Agent
        agent = Agent(
            id=self.config.id,
            name=self.config.name,
            role=self.config.role,
            model=model,
            tools=tools,
            knowledge=knowledge,
            instructions=self.config.instructions,
            enable_user_memories=self.config.enable_memory,
            enable_agentic_memory=self.config.enable_agentic_memory,
            db=db,
            markdown=self.config.markdown,
            show_tool_calls=self.config.show_tool_calls,
            debug_mode=self.config.debug_mode
        )

        return agent

    def run(
        self,
        query: str,
        user_id: Optional[str] = None,
        session_id: Optional[str] = None,
        stream: bool = False,
        **kwargs
    ) -> RunResponse:
        """
        Execute agent with query

        Args:
            query: User query/message
            user_id: Optional user identifier
            session_id: Optional session identifier
            stream: Enable streaming responses
            **kwargs: Additional parameters

        Returns:
            RunResponse object with agent output
        """
        self.logger.info(f"Running agent {self.config.id} with query: {query[:100]}...")

        try:
            response = self.agent.run(
                message=query,
                user_id=user_id,
                session_id=session_id,
                stream=stream,
                **kwargs
            )

            self.logger.info(f"Agent {self.config.id} completed successfully")
            return response

        except Exception as e:
            self.logger.error(f"Error running agent {self.config.id}: {e}", exc_info=True)
            raise

    async def arun(
        self,
        query: str,
        user_id: Optional[str] = None,
        session_id: Optional[str] = None,
        stream: bool = False,
        **kwargs
    ) -> RunResponse:
        """
        Execute agent asynchronously

        Args:
            query: User query/message
            user_id: Optional user identifier
            session_id: Optional session identifier
            stream: Enable streaming responses
            **kwargs: Additional parameters

        Returns:
            RunResponse object with agent output
        """
        self.logger.info(f"Running async agent {self.config.id} with query: {query[:100]}...")

        try:
            response = await self.agent.arun(
                message=query,
                user_id=user_id,
                session_id=session_id,
                stream=stream,
                **kwargs
            )

            self.logger.info(f"Async agent {self.config.id} completed successfully")
            return response

        except Exception as e:
            self.logger.error(f"Error running async agent {self.config.id}: {e}", exc_info=True)
            raise

    def get_config(self) -> Dict[str, Any]:
        """
        Get agent configuration as dictionary (for UI)

        Returns:
            Configuration dictionary
        """
        return self.config.model_dump()

    def update_config(self, updates: Dict[str, Any]):
        """
        Update agent configuration dynamically

        Args:
            updates: Dictionary of configuration updates
        """
        for key, value in updates.items():
            if hasattr(self.config, key):
                setattr(self.config, key, value)
                self.logger.info(f"Updated config {key} = {value}")

        # Recreate agent with new configuration
        self.agent = self._create_agent()
        self.logger.info(f"Agent {self.config.id} reconfigured")

    def get_session_history(self, session_id: str) -> List[Dict[str, Any]]:
        """
        Get conversation history for a session

        Args:
            session_id: Session identifier

        Returns:
            List of messages in session
        """
        # TODO: Implement session history retrieval
        self.logger.warning("Session history retrieval not yet implemented")
        return []

    def get_memory(self, user_id: str) -> List[Dict[str, Any]]:
        """
        Get memories for a user

        Args:
            user_id: User identifier

        Returns:
            List of user memories
        """
        # TODO: Implement memory retrieval
        self.logger.warning("Memory retrieval not yet implemented")
        return []


class AgentRegistry:
    """
    Registry for managing multiple agents

    This class loads agent configurations from JSON files and
    creates agent instances on demand.
    """

    def __init__(
        self,
        base_path: Optional[Path] = None,
        llm_provider_manager: Optional[LLMProviderManager] = None,
        tool_registry: Optional[ToolRegistry] = None,
        db_manager: Optional[DatabaseManager] = None
    ):
        """
        Initialize agent registry

        Args:
            base_path: Base path for agent configurations
            llm_provider_manager: LLM provider manager
            tool_registry: Tool registry
            db_manager: Database manager
        """
        self.path_resolver = PathResolver(base_path=base_path)
        self.llm_provider_manager = llm_provider_manager or LLMProviderManager()
        self.tool_registry = tool_registry or get_tool_registry()
        self.db_manager = db_manager or DatabaseManager()

        self._agents: Dict[str, ConfigurableAgent] = {}
        self._configs: Dict[str, AgentConfig] = {}

        self.logger = get_logger("AgentRegistry")

    def load_configs_from_json(self, json_path: Path) -> List[AgentConfig]:
        """
        Load agent configurations from JSON file

        Args:
            json_path: Path to JSON configuration file

        Returns:
            List of AgentConfig objects
        """
        self.logger.info(f"Loading agent configs from: {json_path}")

        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        configs = []
        for agent_data in data.get("agents", []):
            try:
                config = AgentConfig(**agent_data)
                configs.append(config)
                self._configs[config.id] = config
                self.logger.info(f"Loaded config for agent: {config.id}")
            except Exception as e:
                self.logger.error(f"Error loading agent config: {e}")

        return configs

    def get_agent(
        self,
        agent_id: str,
        api_keys: Optional[Dict[str, str]] = None,
        force_reload: bool = False
    ) -> ConfigurableAgent:
        """
        Get agent instance by ID

        Args:
            agent_id: Agent identifier
            api_keys: API keys for LLM providers
            force_reload: Force recreation of agent

        Returns:
            ConfigurableAgent instance
        """
        # Return cached agent if exists and not forcing reload
        if agent_id in self._agents and not force_reload:
            return self._agents[agent_id]

        # Get configuration
        if agent_id not in self._configs:
            raise ValueError(f"Agent configuration not found: {agent_id}")

        config = self._configs[agent_id]

        # Create agent
        agent = ConfigurableAgent(
            config=config,
            llm_provider_manager=self.llm_provider_manager,
            tool_registry=self.tool_registry,
            db_manager=self.db_manager,
            path_resolver=self.path_resolver,
            api_keys=api_keys
        )

        # Cache agent
        self._agents[agent_id] = agent

        return agent

    def list_agents(self) -> List[Dict[str, Any]]:
        """
        List all available agents

        Returns:
            List of agent info dictionaries
        """
        return [
            {
                "id": config.id,
                "name": config.name,
                "role": config.role,
                "description": config.description,
                "tools": config.tools,
                "provider": config.llm_config["provider"],
                "model": config.llm_config["model_id"]
            }
            for config in self._configs.values()
        ]

    def get_config(self, agent_id: str) -> Dict[str, Any]:
        """
        Get agent configuration

        Args:
            agent_id: Agent identifier

        Returns:
            Configuration dictionary
        """
        if agent_id not in self._configs:
            raise ValueError(f"Agent configuration not found: {agent_id}")

        return self._configs[agent_id].model_dump()

    def update_config(self, agent_id: str, updates: Dict[str, Any]):
        """
        Update agent configuration

        Args:
            agent_id: Agent identifier
            updates: Configuration updates
        """
        if agent_id not in self._configs:
            raise ValueError(f"Agent configuration not found: {agent_id}")

        config = self._configs[agent_id]
        for key, value in updates.items():
            if hasattr(config, key):
                setattr(config, key, value)

        # Update cached agent if exists
        if agent_id in self._agents:
            self._agents[agent_id].update_config(updates)

        self.logger.info(f"Updated configuration for agent: {agent_id}")
