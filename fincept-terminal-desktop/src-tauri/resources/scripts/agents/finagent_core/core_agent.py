"""
Core Agent - Single configurable Agno agent for entire terminal

One agent to rule them all - highly configurable via parameters.
Integrates all modules: Memory, Teams, Workflows, Knowledge, Reasoning.
"""

from typing import Dict, Any, Optional, List
import logging

logger = logging.getLogger(__name__)


class CoreAgent:
    """
    Single Agno agent that adapts based on configuration.
    Used across all terminal tabs with different parameters.

    Supports:
    - Dynamic model selection (40+ providers)
    - 100+ tools
    - Memory management
    - Knowledge bases (RAG)
    - Reasoning strategies
    - Team collaboration
    - Workflow execution
    """

    def __init__(self, api_keys: Optional[Dict[str, str]] = None):
        """
        Initialize core agent.

        Args:
            api_keys: API keys for LLM providers and services
        """
        self.api_keys = api_keys or {}
        self._agent = None
        self._current_config = None
        self._modules = {}

    def run(
        self,
        query: str,
        config: Dict[str, Any],
        session_id: Optional[str] = None,
        stream: bool = False
    ) -> Any:
        """
        Execute agent with given configuration.

        Args:
            query: User query (passed as 'input' to Agno Agent)
            config: Agent configuration (model, tools, instructions, etc.)
            session_id: Session ID for memory
            stream: Stream response

        Returns:
            Agent response

        Example:
            >>> agent = CoreAgent(api_keys={"OPENAI_API_KEY": "..."})
            >>> config = {
            ...     "model": {"provider": "openai", "model_id": "gpt-4-turbo"},
            ...     "instructions": "You are a helpful assistant",
            ...     "tools": ["yfinance", "calculator"],
            ...     "memory": {"enabled": True},
            ...     "reasoning": {"strategy": "chain_of_thought"}
            ... }
            >>> response = agent.run("Analyze AAPL", config)
        """
        # Create or update agent if config changed
        if self._should_recreate_agent(config):
            self._agent = self._create_agent(config)
            self._current_config = config

        # Execute - Agno uses 'input' not 'query'
        run_kwargs = {"input": query}
        if session_id:
            run_kwargs["session_id"] = session_id
        if stream:
            run_kwargs["stream"] = stream

        response = self._agent.run(**run_kwargs)

        return response

    def _should_recreate_agent(self, config: Dict[str, Any]) -> bool:
        """Check if agent needs to be recreated"""
        if self._agent is None:
            return True

        if self._current_config is None:
            return True

        # Compare critical config elements
        critical_keys = ["model", "instructions", "tools", "knowledge", "reasoning"]
        for key in critical_keys:
            if self._current_config.get(key) != config.get(key):
                return True

        return False

    def _create_agent(self, config: Dict[str, Any]) -> Any:
        """Create Agno agent from config with all modules"""
        from agno.agent import Agent
        from finagent_core.registries import ModelsRegistry, ToolsRegistry

        # Create model
        model_config = config.get("model", {})
        model = ModelsRegistry.create_model(
            provider=model_config.get("provider", "openai"),
            model_id=model_config.get("model_id"),
            api_keys=self.api_keys,
            temperature=model_config.get("temperature"),
            max_tokens=model_config.get("max_tokens"),
        )

        # Build agent kwargs
        agent_kwargs = {
            "model": model,
        }

        # Optional name
        if config.get("name"):
            agent_kwargs["name"] = config["name"]

        # Add instructions (with optional reasoning enhancement)
        instructions = config.get("instructions", "You are a helpful AI assistant.")
        if config.get("reasoning"):
            instructions = self._enhance_with_reasoning(instructions, config["reasoning"])
        agent_kwargs["instructions"] = instructions

        # Add tools
        tool_names = config.get("tools", [])
        if tool_names:
            tools = ToolsRegistry.get_tools(tool_names)
            if tools:
                agent_kwargs["tools"] = tools

        # Add knowledge base
        if config.get("knowledge"):
            knowledge_config = self._setup_knowledge(config["knowledge"])
            agent_kwargs.update(knowledge_config)

        # Add reasoning (Agno has built-in reasoning support)
        if config.get("reasoning"):
            reasoning_config = config.get("reasoning", {})
            if isinstance(reasoning_config, bool):
                agent_kwargs["reasoning"] = True
            else:
                agent_kwargs["reasoning"] = True
                if reasoning_config.get("max_steps"):
                    agent_kwargs["reasoning_max_steps"] = reasoning_config["max_steps"]
                if reasoning_config.get("min_steps"):
                    agent_kwargs["reasoning_min_steps"] = reasoning_config["min_steps"]

        # Optional: markdown output
        if config.get("output_format") == "markdown":
            agent_kwargs["markdown"] = True

        # Optional: debug mode
        if config.get("debug"):
            agent_kwargs["debug_mode"] = True

        # Session-related settings
        if config.get("enable_session_summaries"):
            agent_kwargs["enable_session_summaries"] = True
        if config.get("add_history_to_context"):
            agent_kwargs["add_history_to_context"] = True

        return Agent(**agent_kwargs)

    def _setup_memory(self, memory_config: Dict[str, Any]) -> Dict[str, Any]:
        """Setup memory module"""
        from finagent_core.modules import MemoryModule

        if isinstance(memory_config, bool):
            memory_config = {"enabled": memory_config}

        if not memory_config.get("enabled", True):
            return {}

        module = MemoryModule.from_config(memory_config)
        self._modules["memory"] = module

        return module.to_agent_config()

    def _setup_knowledge(self, knowledge_config: Dict[str, Any]) -> Dict[str, Any]:
        """Setup knowledge module"""
        from finagent_core.modules import KnowledgeModule

        module = KnowledgeModule.from_config({
            **knowledge_config,
            "api_keys": self.api_keys
        })
        self._modules["knowledge"] = module

        return module.to_agent_config()

    def _setup_reasoning(self, reasoning_config: Dict[str, Any]) -> Dict[str, Any]:
        """Setup reasoning module"""
        from finagent_core.modules import ReasoningModule

        if isinstance(reasoning_config, bool):
            reasoning_config = {"strategy": "chain_of_thought"}

        module = ReasoningModule.from_config(reasoning_config)
        self._modules["reasoning"] = module

        return module.to_agent_config()

    def _enhance_with_reasoning(
        self,
        instructions: str,
        reasoning_config: Dict[str, Any]
    ) -> str:
        """Enhance instructions with reasoning prompt"""
        from finagent_core.modules import ReasoningModule

        if isinstance(reasoning_config, bool):
            reasoning_config = {"strategy": "chain_of_thought"}

        module = ReasoningModule.from_config(reasoning_config)
        return module.enhance_instructions(instructions)

    def get_response_content(self, response) -> str:
        """Extract text content from response"""
        if hasattr(response, 'content'):
            return response.content
        return str(response)

    def run_team(
        self,
        query: str,
        team_config: Dict[str, Any],
        session_id: Optional[str] = None
    ) -> Any:
        """
        Execute a multi-agent team.

        Args:
            query: User query
            team_config: Team configuration with agent configs
            session_id: Session ID

        Returns:
            Team response
        """
        from finagent_core.modules import TeamModule

        # Create agents for the team
        agents = []
        for agent_config in team_config.get("agents", []):
            agent = self._create_agent(agent_config)
            agents.append(agent)

        # Create and run team
        team_module = TeamModule.from_config(team_config, agents)
        return team_module.run(query, session_id=session_id)

    def run_workflow(
        self,
        workflow_config: Dict[str, Any],
        input_data: Optional[Dict[str, Any]] = None
    ) -> Any:
        """
        Execute a workflow.

        Args:
            workflow_config: Workflow configuration
            input_data: Initial input data

        Returns:
            Workflow result
        """
        from finagent_core.modules import WorkflowModule

        # Create agents for workflow steps if needed
        for step in workflow_config.get("steps", []):
            if step.get("agent_config"):
                step["agent"] = self._create_agent(step["agent_config"])

        workflow_module = WorkflowModule.from_config(workflow_config)
        return workflow_module.run(input_data)

    def search_knowledge(
        self,
        query: str,
        limit: int = 5
    ) -> List[Dict[str, Any]]:
        """
        Search the knowledge base.

        Args:
            query: Search query
            limit: Max results

        Returns:
            Search results
        """
        knowledge_module = self._modules.get("knowledge")
        if knowledge_module:
            return knowledge_module.search(query, limit=limit)
        return []

    def get_module(self, name: str) -> Optional[Any]:
        """Get a specific module instance"""
        return self._modules.get(name)

    def list_available_tools(self) -> Dict[str, List[str]]:
        """List all available tools by category"""
        from finagent_core.registries import ToolsRegistry
        return ToolsRegistry.list_tools()

    def list_available_models(self) -> List[str]:
        """List all available model providers"""
        from finagent_core.registries import ModelsRegistry
        return ModelsRegistry.list_providers()


class CoreAgentBuilder:
    """
    Fluent builder for CoreAgent configuration.

    Example:
        config = (CoreAgentBuilder()
            .with_model("anthropic", "claude-sonnet-4-5")
            .with_instructions("You are a financial analyst")
            .with_tools(["yfinance", "calculator"])
            .with_memory()
            .with_reasoning("chain_of_thought")
            .build())

        agent = CoreAgent(api_keys=keys)
        response = agent.run("Analyze AAPL", config)
    """

    def __init__(self):
        self._config = {}

    def with_model(
        self,
        provider: str,
        model_id: Optional[str] = None,
        temperature: float = 0.7,
        max_tokens: Optional[int] = None
    ) -> "CoreAgentBuilder":
        """Set model configuration"""
        self._config["model"] = {
            "provider": provider,
            "model_id": model_id,
            "temperature": temperature,
        }
        if max_tokens:
            self._config["model"]["max_tokens"] = max_tokens
        return self

    def with_name(self, name: str) -> "CoreAgentBuilder":
        """Set agent name"""
        self._config["name"] = name
        return self

    def with_instructions(self, instructions: str) -> "CoreAgentBuilder":
        """Set agent instructions"""
        self._config["instructions"] = instructions
        return self

    def with_tools(self, tools: List[str]) -> "CoreAgentBuilder":
        """Set tools to use"""
        self._config["tools"] = tools
        return self

    def with_memory(
        self,
        enabled: bool = True,
        optimization_strategy: Optional[str] = None
    ) -> "CoreAgentBuilder":
        """Enable memory"""
        self._config["memory"] = {
            "enabled": enabled,
            "optimization_strategy": optimization_strategy
        }
        return self

    def with_knowledge(
        self,
        embedder_provider: str = "openai",
        vectordb_type: str = "lancedb",
        **kwargs
    ) -> "CoreAgentBuilder":
        """Configure knowledge base"""
        self._config["knowledge"] = {
            "embedder": {"provider": embedder_provider},
            "vectordb": {"type": vectordb_type},
            **kwargs
        }
        return self

    def with_reasoning(
        self,
        strategy: str = "chain_of_thought",
        max_steps: int = 10
    ) -> "CoreAgentBuilder":
        """Configure reasoning"""
        self._config["reasoning"] = {
            "strategy": strategy,
            "max_steps": max_steps
        }
        return self

    def with_markdown_output(self) -> "CoreAgentBuilder":
        """Enable markdown output"""
        self._config["output_format"] = "markdown"
        return self

    def with_debug(self) -> "CoreAgentBuilder":
        """Enable debug mode"""
        self._config["debug"] = True
        return self

    def build(self) -> Dict[str, Any]:
        """Build and return the configuration"""
        return self._config.copy()
