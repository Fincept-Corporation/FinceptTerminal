"""
Core Agent - Single configurable Agno agent for entire terminal

One agent to rule them all - highly configurable via parameters.
Integrates all modules: Memory, Teams, Workflows, Knowledge, Reasoning,
Guardrails, Evaluation, Tracing, Compression, Hooks, Structured Outputs.
"""

from typing import Dict, Any, Optional, List
import logging
import json

logger = logging.getLogger(__name__)


class CoreAgent:
    """
    Single Agno agent that adapts based on configuration.
    Used across all terminal tabs with different parameters.

    Supports:
    - Dynamic model selection (40+ providers)
    - 100+ tools
    - Memory management (including agentic memory)
    - Knowledge bases (RAG)
    - Reasoning strategies
    - Team collaboration
    - Workflow execution
    - Guardrails (PII, injection, compliance)
    - Evaluation (accuracy, performance, reliability)
    - Tracing (debugging, audit)
    - Compression (token optimization)
    - Hooks (pre/post processing)
    - Structured outputs (Pydantic models)
    """

    def __init__(
        self,
        api_keys: Optional[Dict[str, str]] = None,
        user_id: Optional[str] = None
    ):
        self.api_keys = api_keys or {}
        self.user_id = user_id
        self._agent = None
        self._current_config = None
        self._modules = {}

        # Advanced modules
        self._guardrails = None
        self._evaluation = None
        self._tracing = None
        self._compression = None
        self._hooks = None
        self._agentic_memory = None

    # ============================================================
    # Core Execution Methods
    # ============================================================

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
            query: User query/prompt
            config: Agent configuration dict containing:
                - model: {provider, model_id, temperature, max_tokens}
                - instructions: System prompt
                - tools: List of tool names
                - memory: Memory configuration (bool or dict)
                - knowledge: Knowledge base configuration
                - reasoning: Reasoning configuration (bool or dict)
                - markdown: Output format
            session_id: Session ID for memory persistence
            stream: Enable streaming response

        Returns:
            Agent response
        """
        # Inject session_id into config for storage setup
        if session_id:
            config = {**config, "session_id": session_id}

        # Recreate agent if config changed
        if self._should_recreate_agent(config):
            self._agent = self._create_agent(config)
            self._current_config = config

        # Build run kwargs
        run_kwargs = {"message": query}  # Agno uses 'message' not 'input'

        if session_id:
            run_kwargs["session_id"] = session_id

        if stream:
            run_kwargs["stream"] = True
            return self._agent.run(**run_kwargs)

        # Non-streaming run
        response = self._agent.run(**run_kwargs)
        return response

    def run_team(
        self,
        query: str,
        team_config: Dict[str, Any],
        session_id: Optional[str] = None
    ) -> Any:
        """Execute a multi-agent team."""
        from finagent_core.modules import TeamModule

        agents = []
        for agent_config in team_config.get("agents", team_config.get("members", [])):
            agent = self._create_agent(agent_config)
            agents.append(agent)

        # Create coordinator model from team_config if specified
        coordinator_model = None
        if team_config.get("model"):
            coordinator_model = self._create_model(team_config["model"])

        team_module = TeamModule.from_config(team_config, agents, model=coordinator_model)
        return team_module.run(query, session_id=session_id)

    def run_workflow(
        self,
        workflow_config: Dict[str, Any],
        input_data: Optional[Dict[str, Any]] = None
    ) -> Any:
        """Execute a workflow."""
        from finagent_core.modules import WorkflowModule

        for step in workflow_config.get("steps", []):
            if step.get("agent_config"):
                step["agent"] = self._create_agent(step["agent_config"])

        workflow_module = WorkflowModule.from_config(workflow_config)
        return workflow_module.run(input_data)

    def run_with_output_model(
        self,
        query: str,
        config: Dict[str, Any],
        output_model: str,
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """Run agent with structured output model."""
        from finagent_core.modules import OutputModelRegistry, OutputValidator

        model_class = OutputModelRegistry.get_model(output_model)
        if not model_class:
            return {"success": False, "error": f"Unknown output model: {output_model}"}

        config = config.copy()
        config["output_model"] = model_class
        config["structured_outputs"] = True

        response = self.run(query, config, session_id)
        content = self.get_response_content(response)

        try:
            data = json.loads(content) if isinstance(content, str) else content
            validation = OutputValidator.validate(data, output_model)
            return {
                "success": True,
                "model": output_model,
                "data": validation.get("data", data),
                "valid": validation.get("valid", True)
            }
        except Exception as e:
            return {
                "success": False,
                "model": output_model,
                "raw_response": content,
                "error": str(e)
            }

    # ============================================================
    # Financial Workflows
    # ============================================================

    def run_stock_analysis(self, symbol: str, config: Dict[str, Any] = None) -> Dict[str, Any]:
        """Run stock analysis workflow."""
        from finagent_core.modules import FinancialWorkflowTemplates

        config = config or {
            "model": {"provider": "openai", "model_id": "gpt-4-turbo"},
            "instructions": "You are a financial analyst"
        }
        agent = self._create_agent(config)
        workflow = FinancialWorkflowTemplates.stock_analysis_pipeline(
            fetch_agent=agent, analysis_agent=agent, report_agent=agent
        )
        return workflow.run({"symbol": symbol})

    def run_portfolio_rebalancing(self, portfolio_data: Dict[str, Any], config: Dict[str, Any] = None) -> Dict[str, Any]:
        """Run portfolio rebalancing workflow."""
        from finagent_core.modules import FinancialWorkflowTemplates

        config = config or {
            "model": {"provider": "openai", "model_id": "gpt-4-turbo"},
            "instructions": "You are a portfolio manager"
        }
        agent = self._create_agent(config)
        workflow = FinancialWorkflowTemplates.portfolio_rebalancing(
            analysis_agent=agent, optimization_agent=agent, execution_agent=agent
        )
        return workflow.run(portfolio_data)

    def run_risk_assessment(self, portfolio_data: Dict[str, Any], config: Dict[str, Any] = None) -> Dict[str, Any]:
        """Run risk assessment workflow."""
        from finagent_core.modules import FinancialWorkflowTemplates

        config = config or {
            "model": {"provider": "openai", "model_id": "gpt-4-turbo"},
            "instructions": "You are a risk analyst"
        }
        agent = self._create_agent(config)
        workflow = FinancialWorkflowTemplates.risk_assessment(
            risk_agent=agent, market_agent=agent
        )
        return workflow.run(portfolio_data)

    # ============================================================
    # Knowledge & Memory
    # ============================================================

    def search_knowledge(self, query: str, limit: int = 5) -> List[Dict[str, Any]]:
        """Search the knowledge base."""
        knowledge_module = self._modules.get("knowledge")
        if knowledge_module:
            return knowledge_module.search(query, limit=limit)
        return []

    def store_memory(self, content: str, memory_type: str = "fact", metadata: Dict[str, Any] = None) -> int:
        """Store a memory."""
        if not self._agentic_memory:
            self.setup_agentic_memory()
        return self._agentic_memory.store(content, memory_type, metadata)

    def recall_memories(self, query: str = None, memory_type: str = None, limit: int = 5) -> List[Dict[str, Any]]:
        """Recall memories."""
        if not self._agentic_memory:
            return []
        return self._agentic_memory.recall(query, memory_type, limit)

    # ============================================================
    # Guardrails
    # ============================================================

    def setup_guardrails(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """Setup guardrails for input/output validation."""
        from finagent_core.modules import GuardrailsModule
        self._guardrails = GuardrailsModule.from_config(config) if config else GuardrailsModule.default_financial()
        self._modules["guardrails"] = self._guardrails
        return self

    def check_input(self, text: str, context: Dict[str, Any] = None) -> Dict[str, Any]:
        """Check input against guardrails."""
        if not self._guardrails:
            return {"passed": True, "text": text, "violations": []}
        return self._guardrails.check_input(text, context)

    def check_output(self, output: Dict[str, Any]) -> Dict[str, Any]:
        """Check output against guardrails."""
        if not self._guardrails:
            return {"passed": True, "violations": [], "warnings": []}
        return self._guardrails.check_output(output)

    # ============================================================
    # Evaluation
    # ============================================================

    def setup_evaluation(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """Setup evaluation module."""
        from finagent_core.modules import EvaluationModule
        self._evaluation = EvaluationModule.from_config(config) if config else EvaluationModule()
        self._modules["evaluation"] = self._evaluation
        return self

    def evaluate_prediction(self, prediction: Dict[str, Any], actual: Dict[str, Any]) -> Dict[str, Any]:
        """Evaluate prediction accuracy."""
        if not self._evaluation:
            self.setup_evaluation()
        result = self._evaluation.evaluate_prediction(prediction, actual)
        return {"passed": result.passed, "score": result.score, "details": result.details}

    def evaluate_response(self, query: str, response: str, context: Dict[str, Any] = None) -> Dict[str, Any]:
        """Evaluate response quality with agent-as-judge."""
        if not self._evaluation:
            self.setup_evaluation()
        result = self._evaluation.evaluate_with_judge(query, response, context)
        return {"passed": result.passed, "score": result.score, "details": result.details}

    # ============================================================
    # Tracing
    # ============================================================

    def setup_tracing(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """Setup tracing for debugging and monitoring."""
        from finagent_core.modules import TracingModule
        self._tracing = TracingModule.from_config(config) if config else TracingModule()
        self._modules["tracing"] = self._tracing
        return self

    def start_trace(self, name: str, metadata: Dict[str, Any] = None):
        """Start a trace."""
        if not self._tracing:
            self.setup_tracing()
        return self._tracing.start_trace(name, metadata)

    def end_trace(self):
        """End current trace."""
        if self._tracing:
            return self._tracing.end_trace()
        return None

    # ============================================================
    # Compression & Hooks
    # ============================================================

    def setup_compression(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """Setup compression for token optimization."""
        from finagent_core.modules import CompressionModule
        self._compression = CompressionModule.from_config(config) if config else CompressionModule.for_financial_data()
        self._modules["compression"] = self._compression
        return self

    def compress_data(self, data: Any) -> str:
        """Compress data to reduce tokens."""
        if not self._compression:
            self.setup_compression()
        return self._compression.compress(data)

    def setup_hooks(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """Setup hooks for pre/post processing."""
        from finagent_core.modules import HooksModule
        self._hooks = HooksModule.from_config(config) if config else HooksModule.default_financial()
        self._modules["hooks"] = self._hooks
        return self

    def setup_agentic_memory(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """Setup agentic memory."""
        from finagent_core.modules import AgenticMemoryModule
        user_id = config.get("user_id") if config else self.user_id or "default"
        self._agentic_memory = AgenticMemoryModule.from_config({"user_id": user_id, **(config or {})})
        self._modules["agentic_memory"] = self._agentic_memory
        return self

    # ============================================================
    # Listings
    # ============================================================

    def list_available_tools(self) -> Dict[str, List[str]]:
        """List all available tools by category."""
        from finagent_core.registries import ToolsRegistry
        return ToolsRegistry.list_tools()

    def list_available_models(self) -> List[str]:
        """List all available model providers."""
        from finagent_core.registries import ModelsRegistry
        return ModelsRegistry.list_providers()

    def list_output_models(self) -> List[str]:
        """List available output models."""
        from finagent_core.modules import OutputModelRegistry
        return OutputModelRegistry.list_models()

    def get_system_info(self) -> Dict[str, Any]:
        """Get comprehensive system information."""
        from finagent_core.registries import ModelsRegistry, ToolsRegistry, VectorDBRegistry, EmbedderRegistry
        from finagent_core.modules import OutputModelRegistry

        tools = self.list_available_tools()
        return {
            "success": True,
            "version": "2.0.0",
            "framework": "agno",
            "capabilities": {
                "model_providers": len(ModelsRegistry.list_providers()),
                "tools": sum(len(v) for v in tools.values()),
                "tool_categories": list(tools.keys()),
                "vectordbs": VectorDBRegistry.list_vectordbs(),
                "embedders": EmbedderRegistry.list_providers(),
                "output_models": OutputModelRegistry.list_models()
            },
            "features": [
                "multi_agent_teams", "workflow_orchestration", "parallel_execution",
                "conditional_branching", "loop_execution", "dynamic_routing",
                "knowledge_base_rag", "memory_management", "agentic_memory",
                "reasoning_strategies", "session_management", "guardrails",
                "evaluation", "tracing", "compression", "hooks", "structured_outputs"
            ]
        }

    # ============================================================
    # Helper Methods
    # ============================================================

    def get_response_content(self, response) -> str:
        """Extract text content from response."""
        if hasattr(response, 'content'):
            return response.content
        return str(response)

    def get_module(self, name: str) -> Optional[Any]:
        """Get a specific module instance."""
        return self._modules.get(name)

    def _should_recreate_agent(self, config: Dict[str, Any]) -> bool:
        """Check if agent needs to be recreated."""
        if self._agent is None or self._current_config is None:
            return True
        for key in ["model", "instructions", "tools", "knowledge", "reasoning"]:
            if self._current_config.get(key) != config.get(key):
                return True
        return False

    def _create_model(self, model_config: Dict[str, Any]) -> Any:
        """Create a model instance from config (used for team coordinator)."""
        from finagent_core.registries import ModelsRegistry
        return ModelsRegistry.create_model(
            provider=model_config.get("provider", "openai"),
            model_id=model_config.get("model_id"),
            api_keys=self.api_keys,
            temperature=model_config.get("temperature"),
            max_tokens=model_config.get("max_tokens"),
        )

    def _create_agent(self, config: Dict[str, Any]) -> Any:
        """
        Create Agno agent from config with full feature integration.

        Supports all Agno Agent parameters:
        - model: LLM model instance
        - instructions: System prompt
        - tools: List of Toolkit instances
        - memory: Memory backend for persistence
        - knowledge: Knowledge base for RAG
        - reasoning: Extended thinking capabilities
        - structured_outputs: Pydantic model outputs
        """
        from agno.agent import Agent
        from finagent_core.registries import ModelsRegistry, ToolsRegistry

        # =================================================================
        # 1. Create Model
        # =================================================================
        model_config = config.get("model", {})
        model = ModelsRegistry.create_model(
            provider=model_config.get("provider", "openai"),
            model_id=model_config.get("model_id"),
            api_keys=self.api_keys,
            temperature=model_config.get("temperature"),
            max_tokens=model_config.get("max_tokens"),
        )

        agent_kwargs = {"model": model}

        # =================================================================
        # 2. Basic Identity
        # =================================================================
        if config.get("name"):
            agent_kwargs["name"] = config["name"]

        if config.get("role"):
            agent_kwargs["role"] = config["role"]

        if config.get("description"):
            agent_kwargs["description"] = config["description"]

        # =================================================================
        # 3. Instructions
        # =================================================================
        instructions = config.get("instructions", "You are a helpful AI assistant.")
        agent_kwargs["instructions"] = instructions

        # =================================================================
        # 4. Tools - Load and validate
        # =================================================================
        tool_names = config.get("tools", [])
        if tool_names:
            tools = ToolsRegistry.get_tools(tool_names, api_keys=self.api_keys)
            if tools:
                agent_kwargs["tools"] = tools
            else:
                logger.warning(f"No tools loaded from: {tool_names}")

        # =================================================================
        # 5. Memory Integration
        # =================================================================
        memory_config = config.get("memory", {})
        enable_memory = memory_config if isinstance(memory_config, bool) else memory_config.get("enabled", False)

        if enable_memory:
            agent_kwargs["enable_agentic_memory"] = True
            agent_kwargs["add_memories_to_context"] = True

            # Create memory backend if configured
            memory_backend = self._create_memory_backend(memory_config)
            if memory_backend:
                agent_kwargs["memory"] = memory_backend

        # =================================================================
        # 6. Knowledge Base (RAG)
        # =================================================================
        knowledge_config = config.get("knowledge", {})
        enable_knowledge = knowledge_config if isinstance(knowledge_config, bool) else bool(knowledge_config)

        if enable_knowledge and isinstance(knowledge_config, dict):
            knowledge_setup = self._setup_knowledge(knowledge_config)
            agent_kwargs.update(knowledge_setup)
            agent_kwargs["search_knowledge"] = True
            agent_kwargs["add_knowledge_to_context"] = True

        # =================================================================
        # 7. Reasoning (Extended Thinking)
        # =================================================================
        reasoning_config = config.get("reasoning", {})
        enable_reasoning = reasoning_config if isinstance(reasoning_config, bool) else reasoning_config.get("enabled", False)

        if enable_reasoning:
            agent_kwargs["reasoning"] = True

            # Optional: separate reasoning model
            if isinstance(reasoning_config, dict):
                if reasoning_config.get("min_steps"):
                    agent_kwargs["reasoning_min_steps"] = reasoning_config["min_steps"]
                if reasoning_config.get("max_steps"):
                    agent_kwargs["reasoning_max_steps"] = reasoning_config["max_steps"]

                # Create reasoning model if specified
                if reasoning_config.get("model"):
                    reasoning_model = ModelsRegistry.create_model(
                        provider=reasoning_config["model"].get("provider", model_config.get("provider", "openai")),
                        model_id=reasoning_config["model"].get("model_id"),
                        api_keys=self.api_keys,
                        temperature=0.3,  # Lower temperature for reasoning
                    )
                    agent_kwargs["reasoning_model"] = reasoning_model

        # =================================================================
        # 8. Structured Outputs
        # =================================================================
        if config.get("response_model"):
            agent_kwargs["response_model"] = config["response_model"]
        elif config.get("structured_outputs"):
            agent_kwargs["structured_outputs"] = config["structured_outputs"]

        # =================================================================
        # 9. Output Settings
        # =================================================================
        if config.get("markdown", True):
            agent_kwargs["markdown"] = True

        if config.get("debug"):
            agent_kwargs["debug_mode"] = True

        if config.get("show_tool_calls"):
            agent_kwargs["show_tool_calls"] = True

        # =================================================================
        # 10. Session/Storage for persistence
        # =================================================================
        if config.get("session_id") or config.get("storage"):
            storage = self._create_storage(config)
            if storage:
                agent_kwargs["storage"] = storage

        logger.debug(f"Creating agent with params: {list(agent_kwargs.keys())}")
        return Agent(**agent_kwargs)

    def _create_memory_backend(self, memory_config: Any) -> Optional[Any]:
        """Create Agno memory backend."""
        if isinstance(memory_config, bool):
            return None

        try:
            from agno.memory import AgentMemory
            from agno.memory.db.sqlite import SqliteMemoryDb

            db_path = memory_config.get("db_path", "agent_memory.db")
            table_name = memory_config.get("table_name", "agent_memory")

            db = SqliteMemoryDb(
                table_name=table_name,
                db_file=db_path,
            )

            return AgentMemory(
                db=db,
                create_user_memories=memory_config.get("create_user_memories", True),
                create_session_summary=memory_config.get("create_session_summary", True),
            )
        except ImportError as e:
            logger.warning(f"Memory backend not available: {e}")
            return None
        except Exception as e:
            logger.error(f"Failed to create memory backend: {e}")
            return None

    def _create_storage(self, config: Dict[str, Any]) -> Optional[Any]:
        """Create storage backend for session persistence."""
        try:
            storage_config = config.get("storage", {})
            storage_type = storage_config.get("type", "sqlite")

            if storage_type == "sqlite":
                from agno.storage.sqlite import SqliteStorage
                return SqliteStorage(
                    table_name=storage_config.get("table_name", "agent_sessions"),
                    db_file=storage_config.get("db_path", "agent_storage.db"),
                )
            elif storage_type == "postgres":
                from agno.storage.postgres import PostgresStorage
                return PostgresStorage(
                    table_name=storage_config.get("table_name", "agent_sessions"),
                    db_url=storage_config.get("db_url"),
                )
        except ImportError as e:
            logger.warning(f"Storage backend not available: {e}")
        except Exception as e:
            logger.error(f"Failed to create storage: {e}")
        return None

    def _setup_knowledge(self, knowledge_config: Dict[str, Any]) -> Dict[str, Any]:
        """Setup knowledge module."""
        from finagent_core.modules import KnowledgeModule
        module = KnowledgeModule.from_config({**knowledge_config, "api_keys": self.api_keys})
        self._modules["knowledge"] = module
        return module.to_agent_config()

    def _enhance_with_reasoning(self, instructions: str, reasoning_config: Dict[str, Any]) -> str:
        """Enhance instructions with reasoning prompt."""
        from finagent_core.modules import ReasoningModule
        if isinstance(reasoning_config, bool):
            reasoning_config = {"strategy": "chain_of_thought"}
        module = ReasoningModule.from_config(reasoning_config)
        return module.enhance_instructions(instructions)


# =============================================================================
# Builder Pattern
# =============================================================================

class CoreAgentBuilder:
    """
    Builder for creating agent configuration dictionaries.

    Usage:
        config = (CoreAgentBuilder()
            .with_model("openai", "gpt-4o")
            .with_tools(["yfinance", "duckduckgo"])
            .with_memory()
            .with_reasoning("chain_of_thought")
            .build())
        agent = CoreAgent(api_keys={...})
        response = agent.run("query", config)
    """

    def __init__(self):
        self._config: Dict[str, Any] = {}

    def with_model(self, provider: str, model_id: Optional[str] = None, **kwargs) -> "CoreAgentBuilder":
        """Set the model provider and ID."""
        self._config["model"] = {"provider": provider}
        if model_id:
            self._config["model"]["model_id"] = model_id
        if kwargs:
            self._config["model"].update(kwargs)
        return self

    def with_instructions(self, instructions: str) -> "CoreAgentBuilder":
        """Set agent instructions."""
        self._config["instructions"] = instructions
        return self

    def with_tools(self, tools: List[str]) -> "CoreAgentBuilder":
        """Set tools to use."""
        self._config["tools"] = tools
        return self

    def with_memory(self, enabled: bool = True) -> "CoreAgentBuilder":
        """Enable memory."""
        self._config["memory"] = enabled
        return self

    def with_reasoning(self, strategy: str = "chain_of_thought") -> "CoreAgentBuilder":
        """Enable reasoning with a strategy."""
        self._config["reasoning"] = {"strategy": strategy}
        return self

    def with_knowledge(self, knowledge_config: Dict[str, Any]) -> "CoreAgentBuilder":
        """Set knowledge base configuration."""
        self._config["knowledge"] = knowledge_config
        return self

    def with_output_format(self, fmt: str = "markdown") -> "CoreAgentBuilder":
        """Set output format."""
        self._config["output_format"] = fmt
        return self

    def with_guardrails(self, guardrails_config: Optional[Dict[str, Any]] = None) -> "CoreAgentBuilder":
        """Enable guardrails."""
        self._config["guardrails"] = guardrails_config or True
        return self

    def with_name(self, name: str) -> "CoreAgentBuilder":
        """Set agent name."""
        self._config["name"] = name
        return self

    def with_debug(self, enabled: bool = True) -> "CoreAgentBuilder":
        """Enable debug mode."""
        self._config["debug"] = enabled
        return self

    def build(self) -> Dict[str, Any]:
        """Build and return the configuration dictionary."""
        return self._config.copy()



# NOTE: All dispatch logic lives in finagent_core/main.py which is the
# single entry point called by Rust/Tauri. Do not add a separate main()
# here — it creates confusion and duplicated logic.
