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
        """Execute agent with given configuration."""
        if self._should_recreate_agent(config):
            self._agent = self._create_agent(config)
            self._current_config = config

        run_kwargs = {"input": query}
        if session_id:
            run_kwargs["session_id"] = session_id
        if stream:
            run_kwargs["stream"] = stream

        return self._agent.run(**run_kwargs)

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

        team_module = TeamModule.from_config(team_config, agents)
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

    def _create_agent(self, config: Dict[str, Any]) -> Any:
        """Create Agno agent from config."""
        from agno.agent import Agent
        from finagent_core.registries import ModelsRegistry, ToolsRegistry

        model_config = config.get("model", {})
        model = ModelsRegistry.create_model(
            provider=model_config.get("provider", "openai"),
            model_id=model_config.get("model_id"),
            api_keys=self.api_keys,
            temperature=model_config.get("temperature"),
            max_tokens=model_config.get("max_tokens"),
        )

        agent_kwargs = {"model": model}

        if config.get("name"):
            agent_kwargs["name"] = config["name"]

        instructions = config.get("instructions", "You are a helpful AI assistant.")
        if config.get("reasoning"):
            instructions = self._enhance_with_reasoning(instructions, config["reasoning"])
        agent_kwargs["instructions"] = instructions

        tool_names = config.get("tools", [])
        if tool_names:
            tools = ToolsRegistry.get_tools(tool_names)
            if tools:
                agent_kwargs["tools"] = tools

        if config.get("knowledge"):
            agent_kwargs.update(self._setup_knowledge(config["knowledge"]))

        if config.get("reasoning"):
            agent_kwargs["reasoning"] = True

        if config.get("output_format") == "markdown":
            agent_kwargs["markdown"] = True

        if config.get("debug"):
            agent_kwargs["debug_mode"] = True

        return Agent(**agent_kwargs)

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


# =============================================================================
# Entry Point - Single JSON Payload
# =============================================================================

def main(args=None):
    """
    Entry point - accepts single JSON payload from Rust/Tauri.

    Payload format:
    {
        "action": "run|run_team|run_workflow|...",
        "api_keys": {...},
        "user_id": "optional",
        "config": {...},
        "params": {...}  // action-specific parameters
    }
    """
    import sys
    from pathlib import Path

    # Add parent directory to path for imports
    parent_dir = str(Path(__file__).parent.parent)
    if parent_dir not in sys.path:
        sys.path.insert(0, parent_dir)

    if args is None:
        args = sys.argv[1:]

    try:
        if len(args) == 0:
            return json.dumps({"success": False, "error": "No payload provided"})

        # Parse single JSON payload
        payload = json.loads(args[0])
        action = payload.get("action")
        api_keys = payload.get("api_keys", {})
        user_id = payload.get("user_id")
        config = payload.get("config", {})
        params = payload.get("params", {})

        if not action:
            return json.dumps({"success": False, "error": "Missing 'action' in payload"})

        # Create agent
        agent = CoreAgent(api_keys=api_keys, user_id=user_id)

        # Setup optional modules from config
        if config.get("guardrails"):
            agent.setup_guardrails(config["guardrails"])
        if config.get("tracing"):
            agent.setup_tracing(config["tracing"])
        if config.get("agentic_memory"):
            agent.setup_agentic_memory(config["agentic_memory"])

        # Dispatch action
        result = _dispatch_action(agent, action, config, params)
        return json.dumps(result)

    except json.JSONDecodeError as e:
        return json.dumps({"success": False, "error": f"Invalid JSON: {str(e)}"})
    except Exception as e:
        return json.dumps({"success": False, "error": str(e)})


def _dispatch_action(agent: CoreAgent, action: str, config: Dict, params: Dict) -> Dict[str, Any]:
    """Dispatch action to appropriate agent method."""

    # Run single agent
    if action == "run":
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}

        # Check guardrails if enabled
        if agent._guardrails:
            guard_result = agent.check_input(query)
            if not guard_result["passed"]:
                return {"success": False, "error": "Input rejected by guardrails", "violations": guard_result["violations"]}
            query = guard_result["text"]

        response = agent.run(query, config, params.get("session_id"))
        return {"success": True, "response": agent.get_response_content(response)}

    # Run with structured output
    if action == "run_structured":
        query = params.get("query")
        output_model = params.get("output_model")
        if not query or not output_model:
            return {"success": False, "error": "Missing 'query' or 'output_model' in params"}
        return agent.run_with_output_model(query, config, output_model, params.get("session_id"))

    # Run team
    if action == "run_team":
        query = params.get("query")
        team_config = params.get("team_config", config)
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}
        response = agent.run_team(query, team_config, params.get("session_id"))
        return {"success": True, "response": agent.get_response_content(response)}

    # Run workflow
    if action == "run_workflow":
        workflow_config = params.get("workflow_config", config)
        input_data = params.get("input_data", {})
        result = agent.run_workflow(workflow_config, input_data)
        return {"success": True, "result": result if isinstance(result, dict) else str(result)}

    # Stock analysis
    if action == "stock_analysis":
        symbol = params.get("symbol")
        if not symbol:
            return {"success": False, "error": "Missing 'symbol' in params"}
        result = agent.run_stock_analysis(symbol, config)
        return {"success": True, "symbol": symbol, "result": result if isinstance(result, dict) else str(result)}

    # Portfolio rebalancing
    if action == "portfolio_rebal":
        portfolio_data = params.get("portfolio_data", params)
        result = agent.run_portfolio_rebalancing(portfolio_data, config)
        return {"success": True, "result": result if isinstance(result, dict) else str(result)}

    # Risk assessment
    if action == "risk_assessment":
        portfolio_data = params.get("portfolio_data", params)
        result = agent.run_risk_assessment(portfolio_data, config)
        return {"success": True, "result": result if isinstance(result, dict) else str(result)}

    # Search knowledge
    if action == "search":
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}
        results = agent.search_knowledge(query, params.get("limit", 5))
        return {"success": True, "results": results, "count": len(results)}

    # Store memory
    if action == "store_memory":
        content = params.get("content")
        if not content:
            return {"success": False, "error": "Missing 'content' in params"}
        memory_id = agent.store_memory(content, params.get("type", "fact"), params.get("metadata"))
        return {"success": True, "memory_id": memory_id}

    # Recall memories
    if action == "recall_memories":
        memories = agent.recall_memories(params.get("query"), params.get("type"), params.get("limit", 5))
        return {"success": True, "memories": memories, "count": len(memories)}

    # Check guardrails
    if action == "check_guardrails":
        agent.setup_guardrails(config.get("guardrails"))
        if params.get("check_type") == "output":
            result = agent.check_output(params.get("output", {}))
            return {"success": True, "check_type": "output", **result}
        text = params.get("text")
        if not text:
            return {"success": False, "error": "Missing 'text' in params"}
        result = agent.check_input(text, params.get("context"))
        return {"success": True, "check_type": "input", **result}

    # Evaluate
    if action == "evaluate":
        agent.setup_evaluation(config.get("evaluation"))
        if params.get("type") == "prediction":
            result = agent.evaluate_prediction(params.get("prediction", {}), params.get("actual", {}))
        else:
            result = agent.evaluate_response(params.get("query", ""), params.get("response", ""), params.get("context"))
        return {"success": True, **result}

    # List tools
    if action == "list_tools":
        tools = agent.list_available_tools()
        return {"success": True, "tools": tools, "categories": list(tools.keys()), "total_count": sum(len(v) for v in tools.values())}

    # List models
    if action == "list_models":
        providers = agent.list_available_models()
        return {"success": True, "providers": providers, "count": len(providers)}

    # List output models
    if action == "list_outputs":
        models = agent.list_output_models()
        return {"success": True, "models": models, "count": len(models)}

    # System info
    if action == "system_info":
        return agent.get_system_info()

    return {"success": False, "error": f"Unknown action: {action}"}


if __name__ == "__main__":
    result = main()
    print(result)
