"""
Core Agent - Single configurable Agno agent for entire terminal

One agent to rule them all - highly configurable via parameters.
Integrates all modules: Memory, Teams, Workflows, Knowledge, Reasoning,
Guardrails, Evaluation, Tracing, Compression, Hooks, Structured Outputs.
"""

from typing import Dict, Any, Optional, List, Union
import logging
import time

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
        """
        Initialize core agent.

        Args:
            api_keys: API keys for LLM providers and services
            user_id: User ID for user-specific features
        """
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

    # ============================================================
    # Guardrails
    # ============================================================

    def setup_guardrails(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """
        Setup guardrails for input/output validation.

        Args:
            config: Guardrails configuration

        Returns:
            self for chaining
        """
        from finagent_core.modules import GuardrailsModule

        if config:
            self._guardrails = GuardrailsModule.from_config(config)
        else:
            self._guardrails = GuardrailsModule.default_financial()

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
        """
        Setup evaluation module for quality assessment.

        Args:
            config: Evaluation configuration

        Returns:
            self for chaining
        """
        from finagent_core.modules import EvaluationModule

        if config:
            self._evaluation = EvaluationModule.from_config(config)
        else:
            self._evaluation = EvaluationModule()

        self._modules["evaluation"] = self._evaluation
        return self

    def evaluate_prediction(
        self,
        prediction: Dict[str, Any],
        actual: Dict[str, Any]
    ) -> Dict[str, Any]:
        """Evaluate prediction accuracy."""
        if not self._evaluation:
            self.setup_evaluation()
        result = self._evaluation.evaluate_prediction(prediction, actual)
        return {"passed": result.passed, "score": result.score, "details": result.details}

    def evaluate_response(
        self,
        query: str,
        response: str,
        context: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """Evaluate response quality with agent-as-judge."""
        if not self._evaluation:
            self.setup_evaluation()
        result = self._evaluation.evaluate_with_judge(query, response, context)
        return {"passed": result.passed, "score": result.score, "details": result.details}

    def get_evaluation_summary(self) -> Dict[str, Any]:
        """Get evaluation summary."""
        if not self._evaluation:
            return {"message": "Evaluation not setup"}
        return self._evaluation.get_summary()

    # ============================================================
    # Tracing
    # ============================================================

    def setup_tracing(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """
        Setup tracing for debugging and monitoring.

        Args:
            config: Tracing configuration

        Returns:
            self for chaining
        """
        from finagent_core.modules import TracingModule

        if config:
            self._tracing = TracingModule.from_config(config)
        else:
            self._tracing = TracingModule()

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

    def get_traces(self, limit: int = 100) -> List[Dict[str, Any]]:
        """Get recent traces."""
        if not self._tracing:
            return []
        return self._tracing.get_traces(limit)

    def get_performance_summary(self) -> Dict[str, Any]:
        """Get performance summary from traces."""
        if not self._tracing:
            return {"message": "Tracing not setup"}
        return self._tracing.get_performance_summary()

    # ============================================================
    # Compression
    # ============================================================

    def setup_compression(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """
        Setup compression for token optimization.

        Args:
            config: Compression configuration

        Returns:
            self for chaining
        """
        from finagent_core.modules import CompressionModule

        if config:
            self._compression = CompressionModule.from_config(config)
        else:
            self._compression = CompressionModule.for_financial_data()

        self._modules["compression"] = self._compression
        return self

    def compress_data(self, data: Any) -> str:
        """Compress data to reduce tokens."""
        if not self._compression:
            self.setup_compression()
        return self._compression.compress(data)

    # ============================================================
    # Hooks
    # ============================================================

    def setup_hooks(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """
        Setup hooks for pre/post processing.

        Args:
            config: Hooks configuration

        Returns:
            self for chaining
        """
        from finagent_core.modules import HooksModule

        if config:
            self._hooks = HooksModule.from_config(config)
        else:
            self._hooks = HooksModule.default_financial()

        self._modules["hooks"] = self._hooks
        return self

    def run_pre_hooks(self, data: Any, context: Dict[str, Any] = None) -> Dict[str, Any]:
        """Run pre-execution hooks."""
        if not self._hooks:
            return {"success": True, "data": data}
        result = self._hooks.run_pre_hooks(data, context)
        return {"success": result.success, "data": result.data, "error": result.error}

    def run_post_hooks(self, data: Any, context: Dict[str, Any] = None) -> Dict[str, Any]:
        """Run post-execution hooks."""
        if not self._hooks:
            return {"success": True, "data": data}
        result = self._hooks.run_post_hooks(data, context)
        return {"success": result.success, "data": result.data, "error": result.error}

    # ============================================================
    # Agentic Memory
    # ============================================================

    def setup_agentic_memory(self, config: Dict[str, Any] = None) -> "CoreAgent":
        """
        Setup agentic memory for autonomous memory management.

        Args:
            config: Memory configuration

        Returns:
            self for chaining
        """
        from finagent_core.modules import AgenticMemoryModule

        user_id = config.get("user_id") if config else self.user_id
        if not user_id:
            user_id = "default"

        if config:
            self._agentic_memory = AgenticMemoryModule.from_config({
                "user_id": user_id,
                **config
            })
        else:
            self._agentic_memory = AgenticMemoryModule(user_id=user_id)

        self._modules["agentic_memory"] = self._agentic_memory
        return self

    def store_memory(
        self,
        content: str,
        memory_type: str = "fact",
        metadata: Dict[str, Any] = None
    ) -> int:
        """Store a memory."""
        if not self._agentic_memory:
            self.setup_agentic_memory()
        return self._agentic_memory.store(content, memory_type, metadata)

    def recall_memories(
        self,
        query: str = None,
        memory_type: str = None,
        limit: int = 5
    ) -> List[Dict[str, Any]]:
        """Recall memories."""
        if not self._agentic_memory:
            return []
        return self._agentic_memory.recall(query, memory_type, limit)

    def get_memory_context(self, query: str, limit: int = 3) -> str:
        """Get relevant memory context for a query."""
        if not self._agentic_memory:
            return ""
        return self._agentic_memory.get_context_for_query(query, limit)

    # ============================================================
    # Structured Outputs
    # ============================================================

    def run_with_output_model(
        self,
        query: str,
        config: Dict[str, Any],
        output_model: str,
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Run agent with structured output model.

        Args:
            query: User query
            config: Agent configuration
            output_model: Name of output model (e.g., "trade_signal", "stock_analysis")
            session_id: Session ID

        Returns:
            Structured response matching the model
        """
        from finagent_core.modules import OutputModelRegistry

        model_class = OutputModelRegistry.get_model(output_model)
        if not model_class:
            return {"error": f"Unknown output model: {output_model}"}

        # Add output model to config
        config = config.copy()
        config["output_model"] = model_class
        config["structured_outputs"] = True

        # Run agent
        response = self.run(query, config, session_id)
        content = self.get_response_content(response)

        # Validate output
        try:
            import json
            data = json.loads(content) if isinstance(content, str) else content

            from finagent_core.modules import OutputValidator
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

    def list_output_models(self) -> List[str]:
        """List available output models."""
        from finagent_core.modules import OutputModelRegistry
        return OutputModelRegistry.list_models()

    # ============================================================
    # Financial Workflows
    # ============================================================

    def run_stock_analysis(
        self,
        symbol: str,
        config: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """
        Run stock analysis workflow.

        Args:
            symbol: Stock symbol
            config: Optional configuration

        Returns:
            Analysis results
        """
        from finagent_core.modules import FinancialWorkflowTemplates

        config = config or {
            "model": {"provider": "openai", "model_id": "gpt-4-turbo"},
            "instructions": "You are a financial analyst"
        }

        agent = self._create_agent(config)
        workflow = FinancialWorkflowTemplates.stock_analysis_pipeline(
            fetch_agent=agent,
            analysis_agent=agent,
            report_agent=agent
        )

        return workflow.run({"symbol": symbol})

    def run_portfolio_rebalancing(
        self,
        portfolio_data: Dict[str, Any],
        config: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """
        Run portfolio rebalancing workflow.

        Args:
            portfolio_data: Current portfolio
            config: Optional configuration

        Returns:
            Rebalancing recommendations
        """
        from finagent_core.modules import FinancialWorkflowTemplates

        config = config or {
            "model": {"provider": "openai", "model_id": "gpt-4-turbo"},
            "instructions": "You are a portfolio manager"
        }

        agent = self._create_agent(config)
        workflow = FinancialWorkflowTemplates.portfolio_rebalancing(
            analysis_agent=agent,
            optimization_agent=agent,
            execution_agent=agent
        )

        return workflow.run(portfolio_data)

    def run_risk_assessment(
        self,
        portfolio_data: Dict[str, Any],
        config: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """
        Run risk assessment workflow.

        Args:
            portfolio_data: Portfolio to assess
            config: Optional configuration

        Returns:
            Risk assessment results
        """
        from finagent_core.modules import FinancialWorkflowTemplates

        config = config or {
            "model": {"provider": "openai", "model_id": "gpt-4-turbo"},
            "instructions": "You are a risk analyst"
        }

        agent = self._create_agent(config)
        workflow = FinancialWorkflowTemplates.risk_assessment(
            risk_agent=agent,
            market_agent=agent
        )

        return workflow.run(portfolio_data)

    # ============================================================
    # System Info
    # ============================================================

    def get_system_info(self) -> Dict[str, Any]:
        """Get comprehensive system information."""
        from finagent_core.modules import OutputModelRegistry

        return {
            "version": "2.0.0",
            "framework": "agno",
            "api_keys_configured": list(self.api_keys.keys()),
            "user_id": self.user_id,
            "modules_initialized": list(self._modules.keys()),
            "available_tools": len(self.list_available_tools()),
            "available_models": self.list_available_models(),
            "output_models": self.list_output_models(),
            "features": {
                "guardrails": self._guardrails is not None,
                "evaluation": self._evaluation is not None,
                "tracing": self._tracing is not None,
                "compression": self._compression is not None,
                "hooks": self._hooks is not None,
                "agentic_memory": self._agentic_memory is not None
            }
        }


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

    def with_guardrails(self, config: Dict[str, Any] = None) -> "CoreAgentBuilder":
        """Enable guardrails"""
        self._config["guardrails"] = config or {"enabled": True}
        return self

    def with_evaluation(self, config: Dict[str, Any] = None) -> "CoreAgentBuilder":
        """Enable evaluation"""
        self._config["evaluation"] = config or {"enabled": True}
        return self

    def with_tracing(self, config: Dict[str, Any] = None) -> "CoreAgentBuilder":
        """Enable tracing"""
        self._config["tracing"] = config or {"enabled": True}
        return self

    def with_compression(self, max_tokens: int = 2000) -> "CoreAgentBuilder":
        """Enable compression"""
        self._config["compression"] = {"max_tokens": max_tokens}
        return self

    def with_hooks(self, config: Dict[str, Any] = None) -> "CoreAgentBuilder":
        """Enable hooks"""
        self._config["hooks"] = config or {"enabled": True}
        return self

    def with_agentic_memory(self, user_id: str = None) -> "CoreAgentBuilder":
        """Enable agentic memory"""
        self._config["agentic_memory"] = {"user_id": user_id}
        return self

    def with_output_model(self, model_name: str) -> "CoreAgentBuilder":
        """Set structured output model"""
        self._config["output_model"] = model_name
        return self

    def build(self) -> Dict[str, Any]:
        """Build and return the configuration"""
        return self._config.copy()


# =============================================================================
# CLI Interface - Direct script execution (no separate CLI file needed)
# =============================================================================

def main(args=None):
    """
    CLI entry point - called directly by Rust/Tauri.

    Commands:
        run <query_json> <config_json> <api_keys_json>
        run_structured <query_json> <config_json> <api_keys_json>
        run_team <query_json> <team_config_json> <api_keys_json>
        run_workflow <workflow_config_json> <input_data_json> <api_keys_json>
        stock_analysis <query_json> <config_json> <api_keys_json>
        portfolio_rebal <portfolio_json> <config_json> <api_keys_json>
        risk_assessment <portfolio_json> <config_json> <api_keys_json>
        search <query_json> <config_json> <api_keys_json>
        store_memory <memory_json> <config_json> <api_keys_json>
        recall_memories <query_json> <config_json> <api_keys_json>
        check_guardrails <input_json> <config_json> <api_keys_json>
        evaluate <eval_json> <config_json> <api_keys_json>
        list_tools
        list_models
        list_outputs
        system_info
    """
    import sys
    import json
    from pathlib import Path

    # Add parent directory to path for imports (when called as script)
    parent_dir = str(Path(__file__).parent.parent)
    if parent_dir not in sys.path:
        sys.path.insert(0, parent_dir)

    if args is None:
        args = sys.argv[1:]

    try:
        if len(args) == 0:
            return json.dumps({
                "success": False,
                "error": "Usage: core_agent.py <command> [args...]"
            })

        command = args[0]

        # Commands without JSON args
        if command == "list_tools":
            agent = CoreAgent()
            tools = agent.list_available_tools()
            return json.dumps({
                "success": True,
                "tools": tools,
                "categories": list(tools.keys()),
                "total_count": sum(len(v) for v in tools.values())
            })

        if command == "list_models":
            agent = CoreAgent()
            providers = agent.list_available_models()
            return json.dumps({
                "success": True,
                "providers": providers,
                "count": len(providers)
            })

        if command == "list_outputs":
            agent = CoreAgent()
            models = agent.list_output_models()
            return json.dumps({
                "success": True,
                "models": models,
                "count": len(models)
            })

        if command == "system_info":
            return _get_system_info()

        # Legacy support - if first arg is JSON, assume "run" command
        if command.startswith("{"):
            command = "run"
        else:
            args = args[1:]

        # Commands with JSON args (require 3 args)
        if len(args) < 3:
            return json.dumps({
                "success": False,
                "error": f"Command '{command}' requires 3 JSON arguments"
            })

        arg1 = json.loads(args[0])
        arg2 = json.loads(args[1])
        api_keys = json.loads(args[2])

        if command == "run":
            return _cmd_run(arg1, arg2, api_keys)
        elif command == "run_structured":
            return _cmd_run_structured(arg1, arg2, api_keys)
        elif command == "run_team":
            return _cmd_run_team(arg1, arg2, api_keys)
        elif command == "run_workflow":
            return _cmd_run_workflow(arg1, arg2, api_keys)
        elif command == "stock_analysis":
            return _cmd_stock_analysis(arg1, arg2, api_keys)
        elif command == "portfolio_rebal":
            return _cmd_portfolio_rebal(arg1, arg2, api_keys)
        elif command == "risk_assessment":
            return _cmd_risk_assessment(arg1, arg2, api_keys)
        elif command == "search":
            return _cmd_search(arg1, arg2, api_keys)
        elif command == "store_memory":
            return _cmd_store_memory(arg1, arg2, api_keys)
        elif command == "recall_memories":
            return _cmd_recall_memories(arg1, arg2, api_keys)
        elif command == "check_guardrails":
            return _cmd_check_guardrails(arg1, arg2, api_keys)
        elif command == "evaluate":
            return _cmd_evaluate(arg1, arg2, api_keys)
        else:
            return json.dumps({"success": False, "error": f"Unknown command: {command}"})

    except json.JSONDecodeError as e:
        return json.dumps({"success": False, "error": f"Invalid JSON: {str(e)}"})
    except Exception as e:
        return json.dumps({"success": False, "error": str(e)})


def _cmd_run(query_json: Dict, config: Dict, api_keys: Dict) -> str:
    """Execute single agent query."""
    import json

    query = query_json.get("query")
    if not query:
        return json.dumps({"success": False, "error": "Missing 'query'"})

    agent = CoreAgent(api_keys=api_keys, user_id=query_json.get("user_id"))

    # Setup guardrails if configured
    if config.get("guardrails"):
        agent.setup_guardrails(config["guardrails"])
        guard_result = agent.check_input(query)
        if not guard_result["passed"]:
            return json.dumps({
                "success": False,
                "error": "Input rejected by guardrails",
                "violations": guard_result["violations"]
            })
        query = guard_result["text"]

    # Setup tracing if configured
    if config.get("tracing"):
        agent.setup_tracing(config["tracing"])
        agent.start_trace("agent_run", {"query": query[:100]})

    try:
        response = agent.run(
            query=query,
            config=config,
            session_id=query_json.get("session_id"),
            stream=query_json.get("stream", False)
        )
        content = agent.get_response_content(response)

        return json.dumps({
            "success": True,
            "response": content,
            "session_id": query_json.get("session_id")
        })
    finally:
        if config.get("tracing"):
            agent.end_trace()


def _cmd_run_structured(query_json: Dict, config: Dict, api_keys: Dict) -> str:
    """Execute with structured output model."""
    import json

    query = query_json.get("query")
    output_model = query_json.get("output_model")

    if not query:
        return json.dumps({"success": False, "error": "Missing 'query'"})
    if not output_model:
        return json.dumps({"success": False, "error": "Missing 'output_model'"})

    agent = CoreAgent(api_keys=api_keys, user_id=query_json.get("user_id"))
    result = agent.run_with_output_model(
        query=query,
        config=config,
        output_model=output_model,
        session_id=query_json.get("session_id")
    )
    return json.dumps(result)


def _cmd_run_team(query_json: Dict, team_config: Dict, api_keys: Dict) -> str:
    """Execute multi-agent team."""
    import json

    query = query_json.get("query")
    if not query:
        return json.dumps({"success": False, "error": "Missing 'query'"})

    agent = CoreAgent(api_keys=api_keys)
    response = agent.run_team(
        query=query,
        team_config=team_config,
        session_id=query_json.get("session_id")
    )
    content = agent.get_response_content(response)

    return json.dumps({
        "success": True,
        "response": content,
        "session_id": query_json.get("session_id")
    })


def _cmd_run_workflow(workflow_config: Dict, input_data: Dict, api_keys: Dict) -> str:
    """Execute workflow."""
    import json

    agent = CoreAgent(api_keys=api_keys)
    result = agent.run_workflow(workflow_config=workflow_config, input_data=input_data)

    return json.dumps({
        "success": True,
        "result": result if isinstance(result, dict) else str(result),
        "workflow": workflow_config.get("name", "unnamed")
    })


def _cmd_stock_analysis(query_json: Dict, config: Dict, api_keys: Dict) -> str:
    """Run stock analysis workflow."""
    import json

    symbol = query_json.get("symbol")
    if not symbol:
        return json.dumps({"success": False, "error": "Missing 'symbol'"})

    agent = CoreAgent(api_keys=api_keys)
    result = agent.run_stock_analysis(symbol=symbol, config=config)

    return json.dumps({
        "success": True,
        "symbol": symbol,
        "result": result if isinstance(result, dict) else str(result)
    })


def _cmd_portfolio_rebal(portfolio_data: Dict, config: Dict, api_keys: Dict) -> str:
    """Run portfolio rebalancing workflow."""
    import json

    agent = CoreAgent(api_keys=api_keys)
    result = agent.run_portfolio_rebalancing(portfolio_data=portfolio_data, config=config)

    return json.dumps({
        "success": True,
        "result": result if isinstance(result, dict) else str(result)
    })


def _cmd_risk_assessment(portfolio_data: Dict, config: Dict, api_keys: Dict) -> str:
    """Run risk assessment workflow."""
    import json

    agent = CoreAgent(api_keys=api_keys)
    result = agent.run_risk_assessment(portfolio_data=portfolio_data, config=config)

    return json.dumps({
        "success": True,
        "result": result if isinstance(result, dict) else str(result)
    })


def _cmd_search(query_json: Dict, config: Dict, api_keys: Dict) -> str:
    """Search knowledge base."""
    import json

    query = query_json.get("query")
    if not query:
        return json.dumps({"success": False, "error": "Missing 'query'"})

    agent = CoreAgent(api_keys=api_keys)
    if config.get("knowledge"):
        agent._create_agent(config)

    results = agent.search_knowledge(query=query, limit=query_json.get("limit", 5))

    return json.dumps({
        "success": True,
        "query": query,
        "results": results,
        "count": len(results)
    })


def _cmd_store_memory(memory_json: Dict, config: Dict, api_keys: Dict) -> str:
    """Store a memory."""
    import json

    content = memory_json.get("content")
    if not content:
        return json.dumps({"success": False, "error": "Missing 'content'"})

    user_id = memory_json.get("user_id", config.get("user_id", "default"))
    agent = CoreAgent(api_keys=api_keys, user_id=user_id)
    agent.setup_agentic_memory({"user_id": user_id})

    memory_id = agent.store_memory(
        content=content,
        memory_type=memory_json.get("type", "fact"),
        metadata=memory_json.get("metadata", {})
    )

    return json.dumps({
        "success": True,
        "memory_id": memory_id,
        "type": memory_json.get("type", "fact")
    })


def _cmd_recall_memories(query_json: Dict, config: Dict, api_keys: Dict) -> str:
    """Recall memories."""
    import json

    user_id = query_json.get("user_id", config.get("user_id", "default"))
    agent = CoreAgent(api_keys=api_keys, user_id=user_id)
    agent.setup_agentic_memory({"user_id": user_id})

    memories = agent.recall_memories(
        query=query_json.get("query"),
        memory_type=query_json.get("type"),
        limit=query_json.get("limit", 5)
    )

    return json.dumps({
        "success": True,
        "memories": memories,
        "count": len(memories)
    })


def _cmd_check_guardrails(input_json: Dict, config: Dict, api_keys: Dict) -> str:
    """Check input/output against guardrails."""
    import json

    check_type = input_json.get("check_type", "input")
    agent = CoreAgent(api_keys=api_keys)
    agent.setup_guardrails(config.get("guardrails"))

    if check_type == "input":
        text = input_json.get("text")
        if not text:
            return json.dumps({"success": False, "error": "Missing 'text'"})
        result = agent.check_input(text, input_json.get("context"))
        return json.dumps({
            "success": True,
            "check_type": "input",
            "passed": result["passed"],
            "text": result["text"],
            "violations": result["violations"]
        })
    else:
        result = agent.check_output(input_json.get("output", {}))
        return json.dumps({
            "success": True,
            "check_type": "output",
            "passed": result["passed"],
            "violations": result["violations"],
            "warnings": result["warnings"]
        })


def _cmd_evaluate(eval_json: Dict, config: Dict, api_keys: Dict) -> str:
    """Evaluate response quality."""
    import json

    eval_type = eval_json.get("type", "judge")
    agent = CoreAgent(api_keys=api_keys)
    agent.setup_evaluation(config.get("evaluation"))

    if eval_type == "judge":
        query = eval_json.get("query")
        response = eval_json.get("response")
        if not query or not response:
            return json.dumps({"success": False, "error": "Missing 'query' or 'response'"})
        result = agent.evaluate_response(query=query, response=response, context=eval_json.get("context"))
        return json.dumps({"success": True, "eval_type": "judge", **result})

    elif eval_type == "prediction":
        prediction = eval_json.get("prediction")
        actual = eval_json.get("actual")
        if not prediction or not actual:
            return json.dumps({"success": False, "error": "Missing 'prediction' or 'actual'"})
        result = agent.evaluate_prediction(prediction, actual)
        return json.dumps({"success": True, "eval_type": "prediction", **result})

    return json.dumps({"success": False, "error": f"Unknown eval_type: {eval_type}"})


def _get_system_info() -> str:
    """Get comprehensive system information."""
    import json

    try:
        from finagent_core.registries import (
            ModelsRegistry, ToolsRegistry, VectorDBRegistry, EmbedderRegistry
        )
        from finagent_core.modules import OutputModelRegistry

        providers = ModelsRegistry.list_providers()
        tools = ToolsRegistry.list_tools()
        vectordbs = VectorDBRegistry.list_vectordbs()
        embedders = EmbedderRegistry.list_providers()
        output_models = OutputModelRegistry.list_models()

        return json.dumps({
            "success": True,
            "version": "2.0.0",
            "framework": "agno",
            "capabilities": {
                "model_providers": len(providers),
                "tools": sum(len(v) for v in tools.values()),
                "tool_categories": list(tools.keys()),
                "vectordbs": vectordbs,
                "embedders": embedders,
                "output_models": output_models
            },
            "features": [
                "multi_agent_teams", "workflow_orchestration", "parallel_execution",
                "conditional_branching", "loop_execution", "dynamic_routing",
                "knowledge_base_rag", "memory_management", "agentic_memory",
                "reasoning_strategies", "session_management", "guardrails",
                "evaluation", "tracing", "compression", "hooks", "structured_outputs"
            ]
        })
    except ImportError as e:
        return json.dumps({"success": False, "error": f"Import error: {str(e)}"})


if __name__ == "__main__":
    result = main()
    print(result)
