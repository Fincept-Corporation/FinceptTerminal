"""
Core Agent - Single configurable Agno agent for entire terminal

One agent to rule them all - highly configurable via parameters.
Integrates all modules: Memory, Teams, Workflows, Knowledge, Reasoning,
Guardrails, Evaluation, Tracing, Compression, Hooks, Structured Outputs.
"""

from typing import Dict, Any, Optional, List
import logging
import json
import os

logger = logging.getLogger(__name__)


# Agent Studio's CREATE tab spells a few options differently from the embedder
# registry; saved agent configs keep that spelling, so both are accepted.
_UI_EMBEDDER_ALIASES = {
    "sentence-transformers": "sentence_transformer",
    "sentence_transformers": "sentence_transformer",
}


def _ui_str(value: Any) -> str:
    return value.strip() if isinstance(value, str) else ""


def _parse_json_answer(text: str) -> Any:
    """JSON from a model answer: as is, else without ```json fences, else the
    outermost {...} span. Raises ValueError when none of them parses."""
    s = text.strip()
    try:
        return json.loads(s)
    except ValueError:
        pass
    if s.startswith("```"):
        body = s[3:]
        if body[:4].lower() == "json":
            body = body[4:]
        if body.rstrip().endswith("```"):
            body = body.rstrip()[:-3]
        try:
            return json.loads(body.strip())
        except ValueError:
            pass
    start, end = s.find("{"), s.rfind("}")
    if start != -1 and end > start:
        return json.loads(s[start:end + 1])
    raise ValueError("The model did not return JSON")


def _ui_knowledge_config(ui_cfg: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """CREATE tab KNOWLEDGE block {type, urls, vector_db, embedder} ->
    KnowledgeModule.from_config shape {embedder, vectordb, documents}.
    None when no source is listed (nothing to index)."""
    sources = [_ui_str(u) for u in (ui_cfg.get("urls") or []) if _ui_str(u)]
    if not sources:
        return None
    doc_type = _ui_str(ui_cfg.get("type")).lower() or "url"
    documents = []
    for src in sources:
        t = doc_type
        if t == "combined":
            low = src.lower()
            if low.startswith(("http://", "https://")):
                t = "url"
            elif low.endswith(".pdf"):
                t = "pdf"
            else:
                t = "text"
        documents.append({"source": src, "type": t})
    out: Dict[str, Any] = {"documents": documents}
    embedder = ui_cfg.get("embedder")
    if isinstance(embedder, dict):
        out["embedder"] = embedder
    elif _ui_str(embedder):
        provider = _ui_str(embedder).lower()
        out["embedder"] = {"provider": _UI_EMBEDDER_ALIASES.get(provider, provider)}
    vector_db = _ui_str(ui_cfg.get("vector_db")).lower()
    if vector_db:
        out["vectordb"] = {"type": vector_db}
    return out


def normalize_ui_config(config: Any) -> Any:
    """Translate the CREATE tab's saved agent shape into the keys the agent
    builders read. Idempotent; returns a new dict (the input is not changed).

    The editor saves on/off flags (`knowledge`, `memory`, `storage`,
    `agentic_memory`: bool) plus the options in separate blocks
    (`knowledge_config`, `memory_config`, `storage_config`,
    `agentic_memory_user_id`) that nothing read — so a ticked KNOWLEDGE gave no
    RAG, STORAGE's options were ignored and memory sub-options changed nothing.
    Existing saved configs keep that shape, so it is translated here rather than
    changed in the editor.
    """
    if not isinstance(config, dict):
        return config
    out = dict(config)

    kc = out.get("knowledge_config")
    if out.get("knowledge") is True and isinstance(kc, dict):
        knowledge = _ui_knowledge_config(kc)
        if knowledge:
            out["knowledge"] = knowledge

    mc = out.get("memory_config")
    if out.get("memory") is True and isinstance(mc, dict):
        memory: Dict[str, Any] = {"enabled": True}
        for key in ("db_path", "table_name"):
            if _ui_str(mc.get(key)):
                memory[key] = _ui_str(mc.get(key))
        for key in ("create_user_memories", "create_session_summary"):
            if key in mc:
                memory[key] = bool(mc[key])
        out["memory"] = memory

    sc = out.get("storage_config")
    if out.get("storage") is True and isinstance(sc, dict):
        storage: Dict[str, Any] = {"type": _ui_str(sc.get("type")).lower() or "sqlite"}
        for key in ("db_path", "table_name", "db_url"):
            if _ui_str(sc.get(key)):
                storage[key] = _ui_str(sc.get(key))
        out["storage"] = storage

    user_id = _ui_str(out.get("agentic_memory_user_id"))
    if out.get("agentic_memory") is True and user_id:
        out["agentic_memory"] = {"user_id": user_id}

    return out


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
        from finagent_core.persona_registry import PersonaRegistry

        self.api_keys = api_keys or {}
        self.user_id = user_id
        self._modules = {}
        # Notes from the last _create_agent() (tools that could not be loaded,
        # ...) and from the persona used by the last run(). main.py returns
        # them as the result's "warnings".
        self.build_warnings: List[str] = []
        self.last_warnings: List[str] = []

        # Advanced modules — kept as attrs for backward-compat with setup_* calls;
        # actual per-persona state lives inside PersonaRuntime instances.
        self._guardrails = None
        self._evaluation = None
        self._tracing = None
        self._compression = None
        self._hooks = None
        self._agentic_memory = None

        # Per-persona runtime cache.
        self._registry = PersonaRegistry()

    # ============================================================
    # Core Execution Methods
    # ============================================================

    def run(
        self,
        query: str,
        config: Dict[str, Any],
        session_id: Optional[str] = None,
        stream: bool = False,
        agent_id: Optional[str] = None,
        user_id: Optional[str] = None,
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
            agent_id: Persona identifier (falls back to config["agent_id"] or config["id"]).
            user_id: User identifier (falls back to self.user_id or config["user_id"] or "default").

        Returns:
            Agent response
        """
        config = normalize_ui_config(config)
        if session_id:
            config = {**config, "session_id": session_id}

        resolved_user_id = (
            user_id
            or self.user_id
            or config.get("user_id")
            or "default"
        )
        resolved_agent_id = (
            agent_id
            or config.get("agent_id")
            or config.get("id")
            or "unnamed"
        )

        runtime = self._registry.get_or_create(
            user_id=resolved_user_id,
            agent_id=resolved_agent_id,
            config=config,
            api_keys=self.api_keys,
        )
        runtime_warnings = getattr(runtime, "warnings", None)
        self.last_warnings = list(runtime_warnings) if isinstance(runtime_warnings, list) else []
        return runtime.run(query, session_id=session_id, stream=stream)

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
            agent = self._agno_for(agent_config)
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
                step["agent"] = self._agno_for(step["agent_config"])

        workflow_module = WorkflowModule.from_config(workflow_config)
        return workflow_module.run(input_data)

    def _agno_for(self, member_config: Dict[str, Any]) -> Any:
        """Resolve an Agno Agent for a team member or workflow step via the registry.

        Keying on (resolved_user_id, resolved_agent_id) — identical to solo run() —
        so a team-member Buffett reuses the same PersonaRuntime as a solo Buffett
        run, preserving memory/storage consistency across team/solo contexts.
        """
        member_config = normalize_ui_config(member_config)
        resolved_user_id = self.user_id or member_config.get("user_id") or "default"
        resolved_agent_id = (
            member_config.get("agent_id")
            or member_config.get("id")
            or "unnamed"
        )
        runtime = self._registry.get_or_create(
            user_id=resolved_user_id,
            agent_id=resolved_agent_id,
            config=member_config,
            api_keys=self.api_keys,
        )
        return runtime.agno_agent

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

        # The model class itself is not handed to the agent builder, so the model
        # answers in JSON only when asked to: spell out the expected schema.
        schema = OutputModelRegistry.get_schema(output_model)
        if schema:
            query = (
                f"{query}\n\nRespond ONLY with a single JSON object (no prose, no code fences) "
                f"that matches this JSON schema:\n{json.dumps(schema, default=str)}"
            )

        response = self.run(query, config, session_id)
        # A failed run (no model configured, bad key, ...) used to surface as
        # "The model did not return JSON" with the real cause in raw_response.
        from finagent_core.run_errors import run_error_text
        run_err = run_error_text(response)
        if run_err:
            return {"success": False, "model": output_model, "error": run_err}
        content = self.get_response_content(response)

        try:
            data = _parse_json_answer(content) if isinstance(content, str) else content
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

    def _resolve_model_config(self) -> Dict[str, Any]:
        """Derive provider from api_keys passed at runtime instead of hardcoding openai."""
        keys = self.api_keys or {}
        preferred = ["fincept", "ollama", "anthropic", "google", "groq",
                     "deepseek", "openai", "openrouter"]
        for provider in preferred:
            if keys.get(provider) or keys.get(f"{provider.upper()}_API_KEY"):
                return {"provider": provider}
        for k, v in keys.items():
            if k.endswith("_API_KEY") and v:
                return {"provider": k[:-8].lower()}
        return {"provider": "openai"}

    def run_stock_analysis(self, symbol: str, config: Dict[str, Any] = None) -> Dict[str, Any]:
        """Run stock analysis workflow."""
        from finagent_core.modules import FinancialWorkflowTemplates
        cfg = config or {}
        model_config = cfg.get("model") or self._resolve_model_config()
        workflow = FinancialWorkflowTemplates.stock_analysis_pipeline(
            api_keys=self.api_keys,
            model_config=model_config,
            terminal_endpoint=cfg.get("terminal_mcp_endpoint"),
            terminal_tool_defs=cfg.get("terminal_tools", []),
            terminal_token=cfg.get("terminal_mcp_token"),
            terminal_destructive_token=cfg.get("terminal_mcp_destructive_token"),
            terminal_dry_run=bool(cfg.get("tools_dry_run", False)),
        )
        return workflow.run(symbol)

    def run_portfolio_rebalancing(self, portfolio_data: Dict[str, Any], config: Dict[str, Any] = None) -> Dict[str, Any]:
        """Run portfolio rebalancing workflow."""
        from finagent_core.modules import FinancialWorkflowTemplates
        cfg = config or {}
        model_config = cfg.get("model") or self._resolve_model_config()
        workflow = FinancialWorkflowTemplates.portfolio_rebalancing(
            api_keys=self.api_keys,
            model_config=model_config,
            terminal_endpoint=cfg.get("terminal_mcp_endpoint"),
            terminal_tool_defs=cfg.get("terminal_tools", []),
            terminal_token=cfg.get("terminal_mcp_token"),
            terminal_destructive_token=cfg.get("terminal_mcp_destructive_token"),
            terminal_dry_run=bool(cfg.get("tools_dry_run", False)),
        )
        return workflow.run(portfolio_data)

    def run_risk_assessment(self, portfolio_data: Dict[str, Any], config: Dict[str, Any] = None) -> Dict[str, Any]:
        """Run risk assessment workflow."""
        from finagent_core.modules import FinancialWorkflowTemplates
        cfg = config or {}
        model_config = cfg.get("model") or self._resolve_model_config()
        workflow = FinancialWorkflowTemplates.risk_assessment(
            api_keys=self.api_keys,
            model_config=model_config,
            terminal_endpoint=cfg.get("terminal_mcp_endpoint"),
            terminal_tool_defs=cfg.get("terminal_tools", []),
            terminal_token=cfg.get("terminal_mcp_token"),
            terminal_destructive_token=cfg.get("terminal_mcp_destructive_token"),
            terminal_dry_run=bool(cfg.get("tools_dry_run", False)),
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
        """Deprecated: per-persona agentic memory now lives inside PersonaRuntime.

        Shim kept for backward-compat with direct callers of store_memory /
        recall_memories. When agent_id is missing from the config we default
        to "default" and log a warning; prefer CoreAgent.run(..., agent_id=...)
        which routes through PersonaRegistry for proper isolation.
        """
        import logging
        from finagent_core.modules import AgenticMemoryModule
        cfg = dict(config or {})
        cfg.setdefault("user_id", self.user_id or "default")
        if "agent_id" not in cfg:
            logging.getLogger(__name__).warning(
                "CoreAgent.setup_agentic_memory called without agent_id; "
                "defaulting to 'default'. Prefer CoreAgent.run(..., agent_id=...)."
            )
            cfg["agent_id"] = "default"
        self._agentic_memory = AgenticMemoryModule.from_config(cfg)
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
        unavailable = {
            "tools": ToolsRegistry.unavailable_tools(),
            "model_providers": ModelsRegistry.unavailable_providers(),
            "embedders": EmbedderRegistry.unavailable_providers(),
            "vectordbs": VectorDBRegistry.unavailable_vectordbs(),
        }
        n_tools = sum(len(v) for v in tools.values())
        n_models = len(ModelsRegistry.list_providers())
        vectordbs = VectorDBRegistry.list_vectordbs()
        embedders = EmbedderRegistry.list_providers()
        return {
            "success": True,
            "version": "2.0.0",
            "framework": "agno",
            "capabilities": {
                # Catalogue sizes (unchanged meaning) ...
                "model_providers": n_models,
                "tools": n_tools,
                "tool_categories": list(tools.keys()),
                "vectordbs": vectordbs,
                "embedders": embedders,
                "output_models": OutputModelRegistry.list_models(),
                # ... and what can actually load on this install. `unavailable`
                # maps each entry that cannot load to the reason (usually the
                # package to install), so the UI can grey it out.
                "available_counts": {
                    "tools": n_tools - len(unavailable["tools"]),
                    "model_providers": n_models - len(unavailable["model_providers"]),
                    "embedders": len(embedders) - len(unavailable["embedders"]),
                    "vectordbs": len(vectordbs) - len(unavailable["vectordbs"]),
                },
                "unavailable": unavailable,
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
        """Extract text content from response.

        For TeamRunOutput, combines member responses with the coordinator summary
        so the user sees the full team output, not just the brief delegation message.
        """
        # TeamRunOutput: prefer member_responses + coordinator summary
        if hasattr(response, 'member_responses') and response.member_responses:
            parts = []
            for i, member_resp in enumerate(response.member_responses):
                member_content = None
                # RunOutput has .content
                if hasattr(member_resp, 'content') and member_resp.content:
                    member_content = str(member_resp.content)
                # Nested TeamRunOutput — recurse
                elif hasattr(member_resp, 'member_responses'):
                    member_content = self.get_response_content(member_resp)
                if member_content and member_content.strip():
                    # Use agent name if available
                    name = None
                    if hasattr(member_resp, 'agent_name') and member_resp.agent_name:
                        name = member_resp.agent_name
                    elif hasattr(member_resp, 'team_name') and member_resp.team_name:
                        name = member_resp.team_name
                    header = f"### {name}\n" if name else f"### Agent {i + 1}\n"
                    parts.append(header + member_content.strip())
            if parts:
                # Append coordinator summary if it adds substance beyond delegation
                coordinator_content = getattr(response, 'content', None)
                if coordinator_content and str(coordinator_content).strip():
                    coord_str = str(coordinator_content).strip()
                    # Only append if it's not just a delegation notice
                    delegation_phrases = ['delegated', 'delegate', 'assigned', 'routed']
                    is_delegation = any(p in coord_str.lower() for p in delegation_phrases) and len(coord_str) < 300
                    if not is_delegation:
                        parts.append(f"### Summary\n{coord_str}")
                return "\n\n---\n\n".join(parts)

        # Standard RunResponse / single-agent: .content attribute. A run that
        # produced no text has content None; that is an empty answer, not the
        # object's repr (the str() fallback below used to return
        # "RunOutput(run_id=..., content=None, ...)" as the answer).
        if hasattr(response, 'content'):
            return "" if response.content is None else str(response.content)

        # Dict response (from custom models)
        if isinstance(response, dict):
            for key in ['response', 'content', 'text', 'answer', 'message']:
                if key in response:
                    return str(response[key])

        # Fallback to string conversion
        return str(response)

    def get_module(self, name: str) -> Optional[Any]:
        """Get a specific module instance."""
        return self._modules.get(name)

    def _create_model(self, model_config: Dict[str, Any]) -> Any:
        """Create a model instance from config (used for team coordinator)."""
        from finagent_core.registries import ModelsRegistry
        provider = model_config.get("provider") or self._resolve_model_config()["provider"]
        return ModelsRegistry.create_model(
            provider=provider,
            model_id=model_config.get("model_id"),
            api_key=model_config.get("api_key") or None,
            api_keys=self.api_keys,
            temperature=model_config.get("temperature"),
            max_tokens=model_config.get("max_tokens"),
            base_url=model_config.get("base_url") or None,
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

        # CREATE-tab shape (bool flags + *_config blocks) -> builder keys.
        config = normalize_ui_config(config)
        self.build_warnings = []

        # =================================================================
        # 1. Create Model
        # =================================================================
        model_config = config.get("model", {})
        provider = model_config.get("provider", "")
        if not provider:
            fallback = self._resolve_model_config()
            provider = fallback.get("provider", "")
        if not provider:
            raise ValueError(
                "No LLM provider resolved from config or api_keys. "
                "Configure an LLM provider in Settings > LLM Config."
            )

        # Pass any extra model-level kwargs (e.g. max_tool_rounds for FinceptChat).
        # Explicitly exclude all known non-constructor fields so metadata never
        # leaks into provider constructors as unexpected kwargs.
        _MODEL_RESERVED_KEYS = {
            "provider", "model_id", "temperature", "max_tokens",
            "api_key", "base_url", "system_prompt",
            "profile_id", "profile_name",  # internal metadata from C++ LlmService
        }
        extra_model_kwargs = {
            k: v for k, v in model_config.items()
            if k not in _MODEL_RESERVED_KEYS and v is not None
        }

        model = ModelsRegistry.create_model(
            provider=provider,
            model_id=model_config.get("model_id"),
            api_key=model_config.get("api_key") or None,   # direct key from LlmService takes priority
            api_keys=self.api_keys,
            temperature=model_config.get("temperature"),
            max_tokens=model_config.get("max_tokens"),
            base_url=model_config.get("base_url") or None,
            **extra_model_kwargs,
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
        all_tools = []

        if tool_names:
            # Names served by the terminal bridge (e.g. polymarket_get_markets)
            # are loaded below by TerminalToolkit, not by the registry.
            terminal_names = {d.get("name") for d in (config.get("terminal_tools") or [])
                              if isinstance(d, dict)}
            registry_names = [n for n in tool_names if n not in terminal_names]
            # Substitutions (e.g. DuckDuckGo for an unconfigured Tavily) come back
            # as notes and are reported with the run like a missing tool.
            substitution_notes: List[str] = []
            tools, unavailable = ToolsRegistry.resolve_tools(registry_names, api_keys=self.api_keys,
                                                             notes=substitution_notes)
            all_tools.extend(tools)
            self.build_warnings.extend(substitution_notes)
            # A requested tool that cannot load used to be dropped at DEBUG level,
            # so the agent answered without it and the user never knew.
            for name, reason in unavailable.items():
                self.build_warnings.append(f"Tool '{name}' is unavailable: {reason}")
            if unavailable:
                logger.warning(f"Tools unavailable for this agent: {unavailable}")

        # MCP servers — user-configured servers from the MCP tab
        # Format: [{"id", "name", "command", "args", "env", "transport"}]
        mcp_servers = config.get("mcp_servers", [])
        if mcp_servers:
            mcp_tools = self._connect_mcp_servers(mcp_servers)
            all_tools.extend(mcp_tools)

        # Terminal MCP bridge — internal tools (navigation, market data, portfolio, etc.)
        # endpoint + token are injected by AgentService when the bridge is running.
        # destructive_token is only present when the agent config opts in via
        # `allow_destructive_tools=true`.
        terminal_endpoint = config.get("terminal_mcp_endpoint")
        terminal_token = config.get("terminal_mcp_token")
        terminal_destructive_token = config.get("terminal_mcp_destructive_token")
        terminal_tool_defs = config.get("terminal_tools", [])
        terminal_dry_run = bool(config.get("tools_dry_run", False))
        if terminal_endpoint and terminal_tool_defs:
            try:
                from finagent_core.tools.terminal_toolkit import TerminalToolkit
                terminal_toolkit = TerminalToolkit(
                    endpoint=terminal_endpoint,
                    tool_definitions=terminal_tool_defs,
                    token=terminal_token,
                    destructive_token=terminal_destructive_token,
                    dry_run=terminal_dry_run,
                )
                all_tools.extend(terminal_toolkit.get_tools())
                logger.info(f"TerminalToolkit: {len(terminal_toolkit.functions)} tools from MCP")
            except Exception as e:
                logger.warning(f"TerminalToolkit setup failed: {e}")

        if all_tools:
            agent_kwargs["tools"] = all_tools

        # =================================================================
        # 5. Memory Integration
        # =================================================================
        memory_config = config.get("memory", {})
        enable_memory = memory_config if isinstance(memory_config, bool) else memory_config.get("enabled", False)

        if enable_memory:
            # agno 2.x: the model manages memories through a tool (agentic
            # memory) and stored memories are added to its context. The
            # CREATE tab's "User Memories" / "Session Summary" boxes add an
            # extra model call after each run, so they stay opt-in.
            agent_kwargs["enable_agentic_memory"] = True
            agent_kwargs["add_memories_to_context"] = True
            mc = memory_config if isinstance(memory_config, dict) else {}
            if mc.get("create_user_memories"):
                agent_kwargs["update_memory_on_run"] = True
            if mc.get("create_session_summary"):
                agent_kwargs["enable_session_summaries"] = True

            # agno 2.x memory backend: a MemoryManager with its own db
            memory_backend = self._create_memory_backend(memory_config)
            if memory_backend is not None:
                agent_kwargs["memory_manager"] = memory_backend

        # =================================================================
        # 6. Knowledge Base (RAG)
        # =================================================================
        knowledge_config = config.get("knowledge", {})
        enable_knowledge = knowledge_config if isinstance(knowledge_config, bool) else bool(knowledge_config)

        if enable_knowledge and isinstance(knowledge_config, dict):
            knowledge_setup = self._setup_knowledge(knowledge_config)
            if knowledge_setup:
                agent_kwargs.update(knowledge_setup)
                agent_kwargs["search_knowledge"] = True
                agent_kwargs["add_knowledge_to_context"] = True
        elif knowledge_config is True:
            self.build_warnings.append(
                "Knowledge is enabled but no sources (URLs or files) are configured; "
                "the agent runs without a knowledge base.")

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
        # 8. Structured Outputs (agno 2.x renamed response_model -> output_schema)
        # =================================================================
        if config.get("response_model"):
            agent_kwargs["output_schema"] = config["response_model"]
        elif config.get("structured_outputs"):
            agent_kwargs["structured_outputs"] = config["structured_outputs"]

        # =================================================================
        # 9. Agentic Loop — CRITICAL for true autonomous operation
        #    tool_call_limit caps how many tool iterations the agent
        #    can make per run, preventing infinite loops while still
        #    allowing multi-step autonomous tool use.
        # =================================================================
        if all_tools:
            # Cap tool call iterations — default 10, max 20
            max_iter = min(int(config.get("max_iterations", 10)), 20)
            agent_kwargs["tool_call_limit"] = max_iter

        # =================================================================
        # 10. Output Settings
        # =================================================================
        if config.get("markdown", True):
            agent_kwargs["markdown"] = True

        if config.get("debug"):
            agent_kwargs["debug_mode"] = True

        # =================================================================
        # 11. Session/Storage for persistence
        # =================================================================
        # `storage: {}` (what build_agno_agent injects for a pre-built store) is
        # "on with defaults"; the old truthiness test treated it as off, so a
        # session store built for the persona was never attached.
        storage_flag = config.get("storage")
        storage_on = storage_flag is not None and storage_flag is not False
        if config.get("session_id") or storage_on:
            storage = self._create_storage(config)
            if storage is not None:
                # agno 2.x: sessions live in the agent's `db`; history of the
                # session is replayed into the context on the next turn.
                agent_kwargs["db"] = storage
                agent_kwargs["add_history_to_context"] = True

        logger.debug(f"Creating agent with params: {list(agent_kwargs.keys())}")
        return Agent(**agent_kwargs)

    def _create_memory_backend(self, memory_config: Any) -> Optional[Any]:
        """agno 2.x memory backend for agents built outside a PersonaRuntime
        (the streaming path). Raises AgentFeatureUnavailable when it cannot be
        built — memory used to be dropped silently."""
        from finagent_core.paths import agent_data_dir
        from finagent_core.persona_runtime import _build_memory_backend
        mc = memory_config if isinstance(memory_config, dict) else {}
        # A bare relative name used to resolve against the process cwd —
        # the scripts directory, which dev builds prune and installs keep
        # read-only. Default into the per-profile agent data dir instead.
        db_path = mc.get("db_path") or ""
        if not db_path or not os.path.isabs(db_path):
            db_path = str(agent_data_dir() / (db_path or "agent_memory.db"))
        return _build_memory_backend(db_path=db_path, table_name=mc.get("table_name") or "agent_memory")

    def _connect_mcp_servers(self, mcp_servers: List[Dict[str, Any]]) -> List[Any]:
        """
        Connect to user-configured MCP servers from the MCP tab and return
        initialized MCPTools instances ready for use by the agent.

        Each entry in mcp_servers should have:
          - command (str): executable + args as a single string, or
          - cmd (str) + args (list): separate command and arguments
          - env (dict, optional): environment variables
          - transport ("stdio" | "sse" | "streamable-http", default "stdio")
          - url (str): required for sse/streamable-http transport
          - name (str, optional): server name for logging
          - id (str, optional): server id for tool_name_prefix

        Returns list of connected MCPTools instances (failures are skipped).
        """
        import asyncio
        connected = []

        for srv in mcp_servers:
            srv_name = srv.get("name") or srv.get("id") or "mcp-server"
            transport = srv.get("transport", "stdio")

            try:
                from agno.tools.mcp import MCPTools

                env = srv.get("env") or {}
                tool_prefix = (srv.get("id") or "").replace("-", "_") or None

                if transport in ("sse", "streamable-http"):
                    url = srv.get("url")
                    if not url:
                        logger.warning(f"MCP server '{srv_name}' missing url for {transport} transport — skipping")
                        continue
                    mcp_tool = MCPTools(
                        url=url,
                        transport=transport,
                        env=env or None,
                        tool_name_prefix=tool_prefix,
                        timeout_seconds=30,
                    )
                else:
                    # stdio — build full command string from command + args
                    cmd_parts = []
                    if srv.get("command"):
                        cmd_parts.append(srv["command"])
                    if srv.get("args"):
                        args = srv["args"]
                        if isinstance(args, list):
                            cmd_parts.extend(args)
                        elif isinstance(args, str):
                            cmd_parts.extend(args.split())
                    if not cmd_parts:
                        logger.warning(f"MCP server '{srv_name}' has no command — skipping")
                        continue
                    full_command = " ".join(cmd_parts)
                    mcp_tool = MCPTools(
                        command=full_command,
                        env=env or None,
                        transport="stdio",
                        tool_name_prefix=tool_prefix,
                        timeout_seconds=30,
                    )

                # Connect synchronously using asyncio
                async def _connect(tool):
                    await tool.connect()

                try:
                    loop = asyncio.get_event_loop()
                    if loop.is_running():
                        # We're inside an event loop — use run_until_complete via thread
                        import concurrent.futures
                        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as ex:
                            future = ex.submit(asyncio.run, _connect(mcp_tool))
                            future.result(timeout=35)
                    else:
                        loop.run_until_complete(_connect(mcp_tool))
                except Exception as connect_err:
                    logger.error(f"MCP server '{srv_name}' connect failed: {connect_err}")
                    continue

                if mcp_tool.initialized:
                    fn_count = len(mcp_tool.functions) if mcp_tool.functions else 0
                    logger.info(f"MCP server '{srv_name}' connected — {fn_count} tools available")
                    connected.append(mcp_tool)
                else:
                    logger.warning(f"MCP server '{srv_name}' did not initialize properly")

            except ImportError:
                logger.warning("agno.tools.mcp not available — MCP servers cannot be used")
                break
            except Exception as e:
                logger.error(f"MCP server '{srv_name}' setup error: {e}")
                continue

        return connected

    def _create_storage(self, config: Dict[str, Any]) -> Optional[Any]:
        """agno 2.x session store (Agent `db`) for agents built outside a
        PersonaRuntime. Raises AgentFeatureUnavailable naming what is missing
        (it used to log and return None, so storage was silently off)."""
        from finagent_core.paths import agent_data_dir
        from finagent_core.persona_runtime import _build_storage_backend
        storage_config = config.get("storage", {})
        # The CREATE tab saves `storage: true` (a bool) — that means "on, defaults".
        if not isinstance(storage_config, dict):
            storage_config = {}
        db_path = storage_config.get("db_path") or ""
        if not db_path or not os.path.isabs(db_path):
            # Per-profile agent data dir, not the cwd (= scripts dir).
            db_path = str(agent_data_dir() / (db_path or "agent_storage.db"))
        return _build_storage_backend(
            db_path=db_path,
            table_name=storage_config.get("table_name") or "agent_sessions",
            storage_type=storage_config.get("type") or "sqlite",
            db_url=storage_config.get("db_url"),
        )

    def _setup_knowledge(self, knowledge_config: Dict[str, Any]) -> Dict[str, Any]:
        """Setup knowledge module. A knowledge base that cannot be built (no
        embedder key, vector DB library missing, unreadable sources) raises
        AgentFeatureUnavailable with the reason — it used to be logged and
        skipped, so the agent answered without it and the user never knew."""
        from finagent_core.modules import KnowledgeModule
        from finagent_core.paths import agent_data_dir
        cfg = {**knowledge_config, "api_keys": self.api_keys}
        # Vector store under the per-profile agent data dir, not the cwd / /tmp.
        cfg.setdefault("path", str(agent_data_dir() / "knowledge"))
        module = KnowledgeModule.from_config(cfg)
        agent_cfg = module.to_agent_config()
        self.build_warnings.extend(module.warnings)
        self._modules["knowledge"] = module
        return agent_cfg

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
# single entry point called by C++. Do not add a separate main()
# here — it creates confusion and duplicated logic.
