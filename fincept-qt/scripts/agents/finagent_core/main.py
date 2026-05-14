"""
FinAgent Core Main Entry Point

Unified entry point for all agent operations via C++.
Handles JSON payload dispatch to appropriate modules.

Performance notes:
- Discovery actions have internal timeout protection
- Uses optimized agent loader to avoid slow file scanning
"""

import sys
import json
import logging
import signal

# Force UTF-8 on stdout/stderr so emoji and non-ASCII in LLM responses
# don't crash on Windows where the default encoding is cp1252.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
import time
from pathlib import Path
from typing import Dict, Any
from concurrent.futures import ThreadPoolExecutor, TimeoutError as FuturesTimeoutError

# Force ALL logging to stderr so stdout stays clean for JSON output
logging.basicConfig(
    level=logging.WARNING,
    format='%(levelname)-8s %(name)s: %(message)s',
    stream=sys.stderr,
    force=True
)

# Add parent directory to path
parent_dir = str(Path(__file__).parent.parent)
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)

# Timeout for discovery operations (seconds)
DISCOVERY_TIMEOUT = 30.0


import re as _re
_EMOJI_RE = _re.compile(
    "["
    "\U0001F600-\U0001F64F\U0001F300-\U0001F5FF\U0001F680-\U0001F6FF"
    "\U0001F700-\U0001F77F\U0001F780-\U0001F7FF\U0001F800-\U0001F8FF"
    "\U0001F900-\U0001F9FF\U0001FA00-\U0001FA6F\U0001FA70-\U0001FAFF"
    "\U00002702-\U000027B0\U000024C2-\U0001F251"
    "]+", flags=_re.UNICODE
)

def _strip_emoji(text: str) -> str:
    return _EMOJI_RE.sub("", text)

def stream_print(chunk_type: str, content: str):
    """Print a streaming chunk with immediate flush for real-time output"""
    print(f"{chunk_type.upper()}: {_strip_emoji(content)}", flush=True)


def _extract_workflow_response(result: Any) -> str:
    """Extract a human-readable string from a workflow result.

    Workflow.run() can return:
    - An Agno RunResponse object (has .content attribute)
    - A dict (from SimpleWorkflowExecutor)
    - A plain string
    """
    import json as _json
    if result is None:
        return "Workflow completed with no output."
    # Agno RunResponse object
    if hasattr(result, "content"):
        content = result.content
        if content:
            return str(content)
    # SimpleWorkflowExecutor dict: {"success": True, "results": [...], "context": {...}}
    if isinstance(result, dict):
        # Collect all step results into a readable string
        parts = []
        for step_result in result.get("results", []):
            if step_result is not None:
                parts.append(str(step_result))
        if parts:
            return "\n\n".join(parts)
        # Fallback: render the whole dict
        return _json.dumps(result, indent=2, default=str)
    return str(result)


def _setup_agent_modules(agent, config: Dict[str, Any], params: Dict[str, Any]):
    """
    Setup ALL optional CoreAgent modules from config/params.
    This is the single place where every feature gets wired.
    Called before any agent execution.
    """
    # Guardrails
    guardrails_cfg = config.get("guardrails") or params.get("guardrails")
    if guardrails_cfg:
        cfg = guardrails_cfg if isinstance(guardrails_cfg, dict) else None
        agent.setup_guardrails(cfg)

    # Tracing
    tracing_cfg = config.get("tracing") or params.get("tracing")
    if tracing_cfg:
        cfg = tracing_cfg if isinstance(tracing_cfg, dict) else None
        agent.setup_tracing(cfg)

    # Agentic Memory
    agentic_memory_cfg = config.get("agentic_memory") or params.get("agentic_memory")
    if agentic_memory_cfg:
        cfg = agentic_memory_cfg if isinstance(agentic_memory_cfg, dict) else {"user_id": params.get("user_id", "default")}
        cfg.setdefault("agent_id", config.get("id") or config.get("agent_id") or "unnamed")
        agent.setup_agentic_memory(cfg)

    # Compression
    compression_cfg = config.get("compression") or params.get("compression")
    if compression_cfg:
        cfg = compression_cfg if isinstance(compression_cfg, dict) else None
        agent.setup_compression(cfg)

    # Hooks
    hooks_cfg = config.get("hooks") or params.get("hooks")
    if hooks_cfg:
        cfg = hooks_cfg if isinstance(hooks_cfg, dict) else None
        agent.setup_hooks(cfg)

    # Evaluation
    evaluation_cfg = config.get("evaluation") or params.get("evaluation")
    if evaluation_cfg:
        cfg = evaluation_cfg if isinstance(evaluation_cfg, dict) else None
        agent.setup_evaluation(cfg)


def main(args=None):
    """
    Main entry point - accepts single JSON payload from C++.

    Payload format:
    {
        "action": "<action_name>",
        "api_keys": {...},
        "params": {...}
    }

    Actions:
    - Core agent: run, run_team, run_workflow, run_structured
    - Dynamic loading: discover_agents, list_agents, create_agent
    - SuperAgent: route_query, execute_query
    - Execution planner: create_plan, execute_plan, get_plan_status
    - Paper trading: execute_trade, get_portfolio_value, get_positions_summary
    - System: system_info, list_tools, list_models

    Streaming mode:
    - Pass --stream flag to enable streaming output
    - Output format: "CHUNK_TYPE: content" with immediate flush
    - Chunk types: THINKING, TOKEN, TOOL, TOOL_RESULT, ERROR, DONE
    """
    if args is None:
        args = sys.argv[1:]

    # Check for streaming mode
    streaming = "--stream" in args
    use_stdin = "--stdin" in args
    args = [a for a in args if a not in ("--stream", "--stdin")]

    try:
        if use_stdin or len(args) == 0:
            # Read payload from stdin (used when payload is too large for argv)
            payload_str = sys.stdin.read().strip()
            if not payload_str:
                return json.dumps({"success": False, "error": "No payload provided"})
        else:
            payload_str = args[0]

        payload = json.loads(payload_str)
        action = payload.get("action")
        api_keys = payload.get("api_keys", {})
        params = payload.get("params", {})
        config = payload.get("config", {})
        active_llm = payload.get("active_llm", {})

        # Inject active_llm into config["model"] so every code path picks it up.
        # This carries the full resolved config (provider, model_id, api_key,
        # base_url) from LlmService — handles custom endpoints like
        # minimax-over-anthropic-compatible URLs correctly.
        if active_llm:
            config.setdefault("model", {})

            # Normalise base_url: strip provider-specific path suffixes that are
            # not part of the API root. E.g. "https://api.minimax.io/anthropic"
            # should be "https://api.minimax.io/v1" for OpenAI-compat endpoints.
            raw_base_url = active_llm.get("base_url", "")
            if raw_base_url:
                # Strip trailing /anthropic, /openai etc. — these are SDK routing
                # prefixes, not valid REST base paths for OpenAI-compatible APIs.
                import re as _re
                raw_base_url = _re.sub(r'/(anthropic|openai|google|gemini|mistral|cohere)/?$', '', raw_base_url)
                # Ensure /v1 suffix for known hosts that require it
                _V1_HOSTS = ["api.minimax.io", "api.deepseek.com", "api.groq.com",
                             "api.together.xyz", "api.fireworks.ai", "api.openrouter.ai"]
                from urllib.parse import urlparse as _up
                _host = _up(raw_base_url).netloc
                if any(_host == h for h in _V1_HOSTS) and not raw_base_url.rstrip("/").endswith("/v1"):
                    raw_base_url = raw_base_url.rstrip("/") + "/v1"

            config["model"]["provider"]    = active_llm.get("provider",    config["model"].get("provider", "openai"))
            config["model"]["model_id"]    = active_llm.get("model_id",    config["model"].get("model_id", ""))
            config["model"]["api_key"]     = active_llm.get("api_key",     "")
            config["model"]["base_url"]    = raw_base_url
            config["model"]["temperature"] = active_llm.get("temperature", config["model"].get("temperature", 0.7))
            config["model"]["max_tokens"]  = active_llm.get("max_tokens",  config["model"].get("max_tokens", 4096))

        # Enable streaming in params if flag is set
        if streaming:
            params["stream"] = True

        if not action:
            return json.dumps({"success": False, "error": "Missing 'action' in payload"})

        if streaming:
            # In streaming mode, dispatch_action_streaming handles output
            result = dispatch_action_streaming(action, api_keys, params, config)
            # Final JSON result
            print(json.dumps(result), flush=True)
            return ""
        else:
            result = dispatch_action(action, api_keys, params, config)
            return json.dumps(result)

    except json.JSONDecodeError as e:
        if streaming:
            stream_print("error", f"Invalid JSON: {str(e)}")
        return json.dumps({"success": False, "error": f"Invalid JSON: {str(e)}"})
    except Exception as e:
        if streaming:
            stream_print("error", str(e))
        return json.dumps({"success": False, "error": str(e)})


def _discover_agents_internal():
    """Internal helper for agent discovery with isolation"""
    from finagent_core.agent_loader import discover_agents
    return discover_agents()


def dispatch_action(
    action: str,
    api_keys: Dict[str, str],
    params: Dict[str, Any],
    config: Dict[str, Any]
) -> Dict[str, Any]:
    """Dispatch action to appropriate handler"""

    # =========================================================================
    # Core Agent Actions
    # =========================================================================

    if action == "run":
        from finagent_core.core_agent import CoreAgent

        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}

        # Load selected agent's card config (instructions, tools, persona)
        agent_id = config.get("agent_id", "")
        if agent_id:
            try:
                from finagent_core.agent_loader import get_loader
                card = get_loader().registry.get(agent_id)
                if card and card.config.get("instructions"):
                    agent_card_cfg = {**card.config}
                    if config.get("model"):
                        agent_card_cfg["model"] = config["model"]
                    config = agent_card_cfg
            except Exception as _e:
                logger.warning(f"Could not load agent card '{agent_id}': {_e}")

        # Create agent with API keys and user context
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))

        # Setup ALL optional modules from config
        _setup_agent_modules(agent, config, params)

        # Check guardrails on input if enabled
        if agent._guardrails:
            guard_result = agent.check_input(query)
            if not guard_result["passed"]:
                return {"success": False, "error": "Input rejected by guardrails", "violations": guard_result["violations"]}
            query = guard_result["text"]

        # Merge params into config for full agent configuration
        full_config = {**config}

        # Allow params to override config
        for key in ["model", "instructions", "tools", "knowledge", "storage"]:
            if params.get(key):
                full_config[key] = params[key]
        for key in ["memory", "reasoning"]:
            if params.get(key) is not None:
                full_config[key] = params[key]

        # Execute with session support
        session_id = params.get("session_id")
        stream = params.get("stream", False)

        try:
            # Start trace if tracing enabled
            if agent._tracing:
                agent.start_trace("agent_run", {"query": query[:100], "session_id": session_id})

            response = agent.run(query, full_config, session_id, stream)
            result = {"success": True, "response": agent.get_response_content(response)}

            # Check guardrails on output if enabled
            if agent._guardrails:
                output_check = agent.check_output(result)
                if not output_check.get("passed", True):
                    result["guardrail_warnings"] = output_check.get("warnings", [])

            # End trace
            if agent._tracing:
                agent.end_trace()

            return result
        except Exception as e:
            if agent._tracing:
                agent.end_trace()
            return {"success": False, "error": str(e)}

    if action == "run_team":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        query = params.get("query")
        team_config = params.get("team_config", config)
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}
        if agent._guardrails:
            guard_result = agent.check_input(query)
            if not guard_result["passed"]:
                return {"success": False, "error": "Input rejected by guardrails", "violations": guard_result["violations"]}
            query = guard_result["text"]
        response = agent.run_team(query, team_config, params.get("session_id"))
        return {"success": True, "response": agent.get_response_content(response)}

    if action == "run_workflow":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        workflow_config = params.get("workflow_config", config)
        input_data = params.get("input_data", {})
        result = agent.run_workflow(workflow_config, input_data)
        return {"success": True, "result": result if isinstance(result, dict) else str(result)}

    if action == "run_structured":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        query = params.get("query")
        output_model = params.get("output_model")
        if not query or not output_model:
            return {"success": False, "error": "Missing 'query' or 'output_model'"}
        return agent.run_with_output_model(query, config, output_model, params.get("session_id"))

    # =========================================================================
    # Dynamic Agent Loading
    # =========================================================================

    if action == "discover_agents":
        # Use timeout protection for discovery to prevent hanging
        try:
            with ThreadPoolExecutor(max_workers=1) as executor:
                future = executor.submit(_discover_agents_internal)
                agents = future.result(timeout=DISCOVERY_TIMEOUT)
                return {"success": True, "agents": agents, "count": len(agents)}
        except FuturesTimeoutError:
            return {"success": True, "agents": [], "count": 0, "warning": "Discovery timed out"}
        except Exception as e:
            return {"success": False, "error": f"Discovery failed: {str(e)}", "agents": []}

    if action == "list_agents":
        from finagent_core.agent_loader import list_agents, list_categories
        category = params.get("category")
        agents = list_agents(category)
        return {
            "success": True,
            "agents": agents,
            "count": len(agents),
            "categories": list_categories()
        }

    if action == "create_agent":
        from finagent_core.agent_loader import create_agent
        agent_id = params.get("agent_id")
        if not agent_id:
            return {"success": False, "error": "Missing 'agent_id'"}
        try:
            agent = create_agent(agent_id, api_keys, config)
            return {"success": True, "agent_id": agent_id, "created": True}
        except Exception as e:
            return {"success": False, "error": str(e)}

    # =========================================================================
    # SuperAgent Routing
    # =========================================================================

    if action == "route_query":
        from finagent_core.super_agent import route_query
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query'"}
        return route_query(query, api_keys, config)

    if action == "execute_query":
        from finagent_core.super_agent import execute_query
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query'"}
        return execute_query(query, api_keys, params.get("session_id"), config)

    if action == "execute_multi_query":
        from finagent_core.super_agent import SuperAgent
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query'"}
        # Forward user's model config so execute_multi's per-agent execute()
        # calls don't fall through to the "No LLM model configured" path.
        model_config = (config or {}).get("model")
        agent = SuperAgent(api_keys=api_keys, model_config=model_config)
        return agent.execute_multi(
            query,
            params.get("session_id"),
            params.get("aggregate", True),
            user_config=config,
        )

    # =========================================================================
    # Execution Planner
    # =========================================================================

    if action == "create_stock_plan":
        from finagent_core.execution_planner import create_stock_analysis_plan
        symbol = params.get("symbol")
        if not symbol:
            return {"success": False, "error": "Missing 'symbol'"}
        plan = create_stock_analysis_plan(symbol)
        return {"success": True, "plan": plan}

    if action == "create_portfolio_plan":
        from finagent_core.execution_planner import ExecutionPlanner
        portfolio_id = params.get("portfolio_id")
        if not portfolio_id:
            return {"success": False, "error": "Missing 'portfolio_id'"}
        plan = ExecutionPlanner.portfolio_rebalance_plan(portfolio_id)
        return {"success": True, "plan": plan.to_dict()}

    if action == "execute_plan":
        from finagent_core.execution_planner import execute_plan
        plan_dict = params.get("plan")
        if not plan_dict:
            return {"success": False, "error": "Missing 'plan'"}
        return execute_plan(plan_dict, api_keys)

    if action == "generate_dynamic_plan":
        from finagent_core.execution_planner import generate_dynamic_plan
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query'"}
        return generate_dynamic_plan(query, api_keys, config)

    # =========================================================================
    # Paper Trading Bridge
    # =========================================================================

    if action == "paper_execute_trade":
        from finagent_core.paper_trading_bridge import execute_trade
        portfolio_id = params.get("portfolio_id")
        symbol = params.get("symbol")
        trade_action = params.get("action")
        quantity = params.get("quantity", 1)
        price = params.get("price")
        if not all([portfolio_id, symbol, trade_action, price]):
            return {"success": False, "error": "Missing required params: portfolio_id, symbol, action, price"}
        return execute_trade(portfolio_id, symbol, trade_action, quantity, price)

    if action == "paper_get_portfolio":
        from finagent_core.paper_trading_bridge import get_portfolio_value
        portfolio_id = params.get("portfolio_id")
        if not portfolio_id:
            return {"success": False, "error": "Missing 'portfolio_id'"}
        return get_portfolio_value(portfolio_id)

    if action == "paper_get_positions":
        from finagent_core.paper_trading_bridge import get_positions_summary
        portfolio_id = params.get("portfolio_id")
        if not portfolio_id:
            return {"success": False, "error": "Missing 'portfolio_id'"}
        return get_positions_summary(portfolio_id)

    # =========================================================================
    # Repository Operations
    # =========================================================================

    if action == "save_session":
        from finagent_core.repositories import RepositoryFactory, AgentSession
        repo = RepositoryFactory.get_session_repository()
        session = AgentSession(**params)
        session_id = repo.create(session)
        return {"success": True, "session_id": session_id}

    if action == "get_session":
        from finagent_core.repositories import RepositoryFactory
        repo = RepositoryFactory.get_session_repository()
        session = repo.get(params.get("session_id"))
        if session:
            return {"success": True, "session": {
                "id": session.id,
                "agent_id": session.agent_id,
                "messages": session.messages,
                "status": session.status
            }}
        return {"success": False, "error": "Session not found"}

    if action == "add_message":
        from finagent_core.repositories import RepositoryFactory
        repo = RepositoryFactory.get_session_repository()
        success = repo.add_message(
            params.get("session_id"),
            params.get("role"),
            params.get("content")
        )
        return {"success": success}

    if action == "save_memory":
        from finagent_core.repositories import RepositoryFactory, AgentMemory
        import uuid
        repo = RepositoryFactory.get_memory_repository()
        memory = AgentMemory(
            id=str(uuid.uuid4()),
            agent_id=params.get("agent_id", "default"),
            user_id=params.get("user_id"),
            memory_type=params.get("type", "fact"),
            content=params.get("content"),
            metadata=params.get("metadata", {}),
            importance=params.get("importance", 0.5)
        )
        memory_id = repo.create(memory)
        return {"success": True, "memory_id": memory_id}

    if action == "search_memories":
        from finagent_core.repositories import RepositoryFactory
        repo = RepositoryFactory.get_memory_repository()
        memories = repo.search(
            params.get("query", ""),
            params.get("agent_id"),
            params.get("limit", 10)
        )
        return {
            "success": True,
            "memories": [
                {"id": m.id, "content": m.content, "type": m.memory_type, "importance": m.importance}
                for m in memories
            ]
        }

    if action == "save_trade_decision":
        from finagent_core.repositories import RepositoryFactory, TradeDecision
        import uuid
        from datetime import datetime
        repo = RepositoryFactory.get_trade_decision_repository()
        decision = TradeDecision(
            id=str(uuid.uuid4()),
            competition_id=params.get("competition_id"),
            model_name=params.get("model_name"),
            cycle_number=params.get("cycle_number", 0),
            symbol=params.get("symbol"),
            action=params.get("action"),
            quantity=params.get("quantity", 0),
            price=params.get("price"),
            reasoning=params.get("reasoning", ""),
            confidence=params.get("confidence", 0.5),
            timestamp=datetime.utcnow().isoformat()
        )
        decision_id = repo.create(decision)
        return {"success": True, "decision_id": decision_id}

    if action == "get_decisions":
        from finagent_core.repositories import RepositoryFactory
        repo = RepositoryFactory.get_trade_decision_repository()
        decisions = repo.list(
            competition_id=params.get("competition_id"),
            model_name=params.get("model_name"),
            cycle_number=params.get("cycle_number"),
            limit=params.get("limit", 100)
        )
        return {
            "success": True,
            "decisions": [
                {
                    "id": d.id,
                    "model_name": d.model_name,
                    "symbol": d.symbol,
                    "action": d.action,
                    "quantity": d.quantity,
                    "price": d.price,
                    "confidence": d.confidence,
                    "reasoning": d.reasoning
                }
                for d in decisions
            ]
        }

    # =========================================================================
    # Financial Workflows (via CoreAgent)
    # =========================================================================

    if action == "stock_analysis":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        symbol = params.get("symbol")
        if not symbol:
            return {"success": False, "error": "Missing 'symbol' in params"}
        result = agent.run_stock_analysis(symbol, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "symbol": symbol, "response": response_text, "result": result if isinstance(result, dict) else None}

    if action == "portfolio_rebal":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        portfolio_data = params.get("portfolio_data", params)
        result = agent.run_portfolio_rebalancing(portfolio_data, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "response": response_text, "result": result if isinstance(result, dict) else None}

    if action == "risk_assessment":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        portfolio_data = params.get("portfolio_data", params)
        result = agent.run_risk_assessment(portfolio_data, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "response": response_text, "result": result if isinstance(result, dict) else None}

    if action == "macro_scan":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        query = ("Provide a comprehensive macro market brief covering: current CPI and inflation trends, "
                 "GDP growth trajectory, central bank policy stance and yield curve shape, "
                 "key upcoming economic releases, and the overall macro risk environment for equities.")
        result = agent.run(query, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "response": response_text}

    if action == "earnings_brief":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        symbol = params.get("symbol")
        if not symbol:
            return {"success": False, "error": "Missing 'symbol' in params"}
        query = (f"Generate a detailed earnings analyst brief for {symbol}. Include: "
                 f"latest EPS vs consensus estimate, revenue beat/miss, key guidance changes, "
                 f"margin trends, management commentary highlights, and post-earnings price reaction.")
        result = agent.run(query, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "symbol": symbol, "response": response_text}

    if action == "sector_rotation":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        query = ("Analyse current sector rotation dynamics across the S&P 500. Identify: "
                 "which sectors are showing relative strength vs weakness over 1-month and 3-month periods, "
                 "key macro drivers behind the rotation, and specific sector ETF recommendations "
                 "to overweight and underweight in the current regime.")
        result = agent.run(query, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "response": response_text}

    if action == "options_scan":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        symbol = params.get("symbol", "")
        ticker_ctx = f" for {symbol}" if symbol else " across the market"
        query = (f"Scan and analyse unusual options activity{ticker_ctx}. Identify: "
                 f"significant call/put volume spikes vs open interest, large block trades, "
                 f"implied volatility skew changes, and what the options flow signals "
                 f"directionally for the underlying.")
        result = agent.run(query, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "symbol": symbol, "response": response_text}

    if action == "sentiment_scan":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        symbol = params.get("symbol")
        if not symbol:
            return {"success": False, "error": "Missing 'symbol' in params"}
        query = (f"Perform a comprehensive sentiment analysis for {symbol} over the past 48 hours. "
                 f"Cover: news headline sentiment (positive/negative/neutral breakdown), "
                 f"social media buzz and trending topics, analyst rating changes, "
                 f"insider activity, and an overall sentiment score with directional bias.")
        result = agent.run(query, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "symbol": symbol, "response": response_text}

    if action == "custom_query":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}
        result = agent.run(query, config)
        response_text = _extract_workflow_response(result)
        return {"success": True, "response": response_text}

    # =========================================================================
    # Agent Memory (via CoreAgent module)
    # =========================================================================

    if action == "store_memory":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        am_cfg = dict(params.get("agentic_memory") or config.get("agentic_memory") or {"user_id": params.get("user_id", "default")})
        am_cfg.setdefault("agent_id", params.get("agent_id") or config.get("id") or config.get("agent_id") or "unnamed")
        agent.setup_agentic_memory(am_cfg)
        content = params.get("content")
        if not content:
            return {"success": False, "error": "Missing 'content' in params"}
        memory_id = agent.store_memory(content, params.get("type", "fact"), params.get("metadata"))
        return {"success": True, "memory_id": memory_id}

    if action == "recall_memories":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        am_cfg = dict(params.get("agentic_memory") or config.get("agentic_memory") or {"user_id": params.get("user_id", "default")})
        am_cfg.setdefault("agent_id", params.get("agent_id") or config.get("id") or config.get("agent_id") or "unnamed")
        agent.setup_agentic_memory(am_cfg)
        memories = agent.recall_memories(params.get("query"), params.get("type"), params.get("limit", 5))
        return {"success": True, "memories": memories, "count": len(memories)}

    # =========================================================================
    # Guardrails Check
    # =========================================================================

    if action == "check_guardrails":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        agent.setup_guardrails(config.get("guardrails"))
        if params.get("check_type") == "output":
            result = agent.check_output(params.get("output", {}))
            return {"success": True, "check_type": "output", **result}
        text = params.get("text")
        if not text:
            return {"success": False, "error": "Missing 'text' in params"}
        result = agent.check_input(text, params.get("context"))
        return {"success": True, "check_type": "input", **result}

    # =========================================================================
    # Evaluation
    # =========================================================================

    if action == "evaluate":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        agent.setup_evaluation(config.get("evaluation"))
        if params.get("type") == "prediction":
            result = agent.evaluate_prediction(params.get("prediction", {}), params.get("actual", {}))
        else:
            result = agent.evaluate_response(params.get("query", ""), params.get("response", ""), params.get("context"))
        return {"success": True, **result}

    # =========================================================================
    # Knowledge Search
    # =========================================================================

    if action == "search_knowledge":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}
        results = agent.search_knowledge(query, params.get("limit", 5))
        return {"success": True, "results": results, "count": len(results)}

    # =========================================================================
    # System Information
    # =========================================================================

    if action == "system_info":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        return agent.get_system_info()

    if action == "list_tools":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        tools = agent.list_available_tools()
        return {
            "success": True,
            "tools": tools,
            "categories": list(tools.keys()),
            "total": sum(len(v) for v in tools.values())
        }

    if action == "list_models":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        providers = agent.list_available_models()
        return {"success": True, "providers": providers, "count": len(providers)}

    if action == "list_output_models":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        models = agent.list_output_models()
        return {"success": True, "models": models}

    # =========================================================================
    # Resumable Multi-Step Agentic Tasks
    # =========================================================================

    if action == "start_task":
        """
        Start a multi-step autonomous task with full checkpoint persistence.
        The agent plans its own steps, executes each one with tools,
        and saves state after every step so it can be resumed if interrupted.

        Params: { query, session_id? }
        Config: standard agent config (model, tools, instructions, etc.)
        Returns: { success, task_id, response, steps_completed, steps }
        """
        from finagent_core.task_state import ResumableTaskRunner
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}
        runner = ResumableTaskRunner(api_keys=api_keys)
        return runner.start_task(query, config)

    if action == "resume_task":
        """
        Resume a failed or interrupted multi-step task from its last checkpoint.
        Skips steps that already completed; continues from where it left off.

        Params: { task_id }
        Returns: { success, task_id, response, resumed_from_step, steps_completed }
        """
        from finagent_core.task_state import ResumableTaskRunner
        task_id = params.get("task_id")
        if not task_id:
            return {"success": False, "error": "Missing 'task_id' in params"}
        runner = ResumableTaskRunner(api_keys=api_keys)
        return runner.resume_task(task_id)

    if action == "get_task":
        """Get the current state of a task."""
        from finagent_core.task_state import get_state_manager
        task_id = params.get("task_id")
        if not task_id:
            return {"success": False, "error": "Missing 'task_id' in params"}
        task = get_state_manager().get_task(task_id)
        if not task:
            return {"success": False, "error": f"Task {task_id} not found"}
        return {"success": True, "task": task}

    if action == "list_tasks":
        """List recent tasks, optionally filtered by status."""
        from finagent_core.task_state import get_state_manager
        tasks = get_state_manager().list_tasks(
            status=params.get("status"),
            limit=params.get("limit", 20)
        )
        return {"success": True, "tasks": tasks, "count": len(tasks)}

    if action == "delete_task":
        """Delete a task by ID."""
        from finagent_core.task_state import get_state_manager
        task_id = params.get("task_id")
        if not task_id:
            return {"success": False, "error": "Missing 'task_id' in params"}
        get_state_manager().delete_task(task_id)
        return {"success": True, "deleted": task_id}

    # =========================================================================
    # Agentic Mode — cooperative pause/cancel signals.
    # The running task subprocess polls agent_tasks.status between steps and
    # exits cleanly when it sees pause_requested / cancel_requested.
    # =========================================================================

    if action == "agentic_pause_task":
        from finagent_core.task_state import get_state_manager
        task_id = params.get("task_id")
        if not task_id:
            return {"success": False, "error": "Missing 'task_id' in params"}
        get_state_manager().update_status(task_id, "pause_requested")
        return {"success": True, "task_id": task_id, "signal": "pause_requested"}

    if action == "agentic_cancel_task":
        from finagent_core.task_state import get_state_manager
        task_id = params.get("task_id")
        if not task_id:
            return {"success": False, "error": "Missing 'task_id' in params"}
        get_state_manager().update_status(task_id, "cancel_requested")
        return {"success": True, "task_id": task_id, "signal": "cancel_requested"}

    # ── Agentic Mode — Scheduled recurring tasks ────────────────────────────

    if action == "agentic_schedule_create":
        from finagent_core.agentic.scheduler import ScheduledTaskStore
        name = params.get("name") or "Untitled schedule"
        query = params.get("query")
        schedule_expr = params.get("schedule")
        sched_config = params.get("schedule_config") or {}
        if not query or not schedule_expr:
            return {"success": False, "error": "Missing 'query' or 'schedule' in params"}
        try:
            sid = ScheduledTaskStore().create(
                name=name, query=query, schedule_expr=schedule_expr,
                config=sched_config, start_now=bool(params.get("start_now")),
            )
        except ValueError as e:
            return {"success": False, "error": f"Invalid schedule: {e}"}
        return {"success": True, "schedule_id": sid}

    if action == "agentic_schedule_list":
        from finagent_core.agentic.scheduler import ScheduledTaskStore
        return {"success": True,
                "schedules": ScheduledTaskStore().list(
                    enabled_only=bool(params.get("enabled_only")),
                    limit=int(params.get("limit", 200)))}

    if action == "agentic_schedule_delete":
        from finagent_core.agentic.scheduler import ScheduledTaskStore
        sid = params.get("schedule_id")
        if not sid:
            return {"success": False, "error": "Missing 'schedule_id'"}
        ScheduledTaskStore().delete(sid)
        return {"success": True, "deleted": sid}

    if action == "agentic_schedule_set_enabled":
        from finagent_core.agentic.scheduler import ScheduledTaskStore
        sid = params.get("schedule_id")
        if not sid:
            return {"success": False, "error": "Missing 'schedule_id'"}
        ScheduledTaskStore().set_enabled(sid, bool(params.get("enabled", True)))
        return {"success": True, "schedule_id": sid}

    if action == "agentic_schedule_tick":
        # Lightweight poll: returns due schedules and advances their cursors.
        # The C++ caller then fires each as an agentic streaming task. Keeping
        # the firing on the C++ side means the streaming subprocess is owned
        # by AgentService and emits events on task:event:* directly.
        from finagent_core.agentic.scheduler import ScheduledTaskStore
        due = ScheduledTaskStore().take_due()
        return {"success": True, "due": due}

    # ── Agentic Mode — Library inspection (read-only UI surface) ────────────

    if action == "agentic_skills_list":
        from finagent_core.agentic.skill_library import SkillLibrary
        return {"success": True,
                "skills": SkillLibrary().list(limit=int(params.get("limit", 200)))}

    if action == "agentic_skill_delete":
        from finagent_core.agentic.skill_library import SkillLibrary
        sid = params.get("skill_id")
        if not sid:
            return {"success": False, "error": "Missing 'skill_id'"}
        SkillLibrary().delete(sid)
        return {"success": True, "deleted": sid}

    if action == "agentic_memory_list":
        from finagent_core.agentic.archival_memory import ArchivalMemoryStore
        return {"success": True,
                "memories": ArchivalMemoryStore().list(
                    user_id=params.get("user_id"),
                    memory_type=params.get("type"),
                    limit=int(params.get("limit", 200)))}

    if action == "agentic_memory_delete":
        from finagent_core.agentic.archival_memory import ArchivalMemoryStore
        mid = params.get("memory_id")
        if not mid:
            return {"success": False, "error": "Missing 'memory_id'"}
        ArchivalMemoryStore().delete(mid)
        return {"success": True, "deleted": mid}

    if action == "agentic_reflexion_list":
        # Returns recent reflections regardless of task. Mainly diagnostic.
        from finagent_core.agentic.reflexion_store import ReflexionStore
        import sqlite3 as _sql
        rs = ReflexionStore()
        with _sql.connect(rs.db_path) as c:
            c.row_factory = _sql.Row
            rows = c.execute(
                "SELECT id, task_id, query, step_idx, decision, reason, "
                "replan_hint, question, created_at FROM agent_reflections "
                "ORDER BY created_at DESC LIMIT ?",
                (int(params.get("limit", 200)),)
            ).fetchall()
        return {"success": True, "reflections": [dict(r) for r in rows]}

    # =========================================================================
    # Unknown Action
    # =========================================================================

    return {"success": False, "error": f"Unknown action: {action}"}


def dispatch_action_streaming(
    action: str,
    api_keys: Dict[str, str],
    params: Dict[str, Any],
    config: Dict[str, Any]
) -> Dict[str, Any]:
    """
    Dispatch action with streaming output.
    Prints chunks in real-time for the host to capture and emit via C++ events.
    """

    # =========================================================================
    # Streaming Agent Run
    # =========================================================================

    if action == "run":
        from finagent_core.core_agent import CoreAgent
        from finagent_core.core_agent_stream import StreamingCoreAgent

        query = params.get("query")
        if not query:
            stream_print("error", "Missing 'query' in params")
            return {"success": False, "error": "Missing 'query' in params"}

        stream_print("thinking", "Initializing agent...")

        # Create core agent for module setup
        core_agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))

        # Setup ALL optional modules
        _setup_agent_modules(core_agent, config, params)

        # Check guardrails on input
        if core_agent._guardrails:
            guard_result = core_agent.check_input(query)
            if not guard_result["passed"]:
                stream_print("error", "Input rejected by guardrails")
                return {"success": False, "error": "Input rejected by guardrails", "violations": guard_result["violations"]}
            query = guard_result["text"]

        # If a specific agent_id is selected, load that agent's card config
        # (instructions, tools, persona, etc.) and merge it as the base,
        # letting active_llm/model still override the model provider.
        agent_id = config.get("agent_id", "")
        if agent_id:
            try:
                from finagent_core.agent_loader import get_loader
                loader = get_loader()
                card = loader.registry.get(agent_id)
                if card and card.config.get("instructions"):
                    # Agent card config is the base; active_llm model takes priority
                    agent_card_cfg = {**card.config}
                    # Preserve the resolved model from active_llm — don't let the
                    # card's provider override what the user configured in Settings
                    if config.get("model"):
                        agent_card_cfg["model"] = config["model"]
                    config = agent_card_cfg
                    stream_print("thinking", f"Using agent: {card.name}")
            except Exception as _e:
                logger.warning(f"Could not load agent card '{agent_id}': {_e}")

        # Build config
        full_config = {**config}
        for key in ["model", "instructions", "tools", "knowledge", "storage"]:
            if params.get(key):
                full_config[key] = params[key]
        for key in ["memory", "reasoning"]:
            if params.get(key) is not None:
                full_config[key] = params[key]

        stream_print("thinking", f"Processing query: {query[:50]}...")

        try:
            # Start trace if enabled
            if core_agent._tracing:
                core_agent.start_trace("agent_run_streaming", {"query": query[:100]})

            session_id = params.get("session_id")

            # Use StreamingCoreAgent for real streaming
            try:
                streaming_agent = StreamingCoreAgent(
                    api_keys=api_keys,
                    user_id=params.get("user_id"),
                    stream_callback=lambda ct, c, m=None: stream_print(ct, c),
                )
                # run_streaming returns a generator — must be iterated to drive
                # the stream_callback and emit TOKEN lines to stdout.
                resp_content = ""
                for chunk in streaming_agent.run_streaming(query, full_config, session_id):
                    if isinstance(chunk, dict) and chunk.get("type") == "token":
                        resp_content += chunk.get("content", "")
            except Exception:
                # Fallback to regular run with simulated streaming
                resp_content = ""
                try:
                    response = core_agent.run(query, full_config, session_id, stream=False)
                    resp_content = core_agent.get_response_content(response) or ""
                    if resp_content:
                        words = resp_content.split()
                        chunk_size = 5
                        for i in range(0, len(words), chunk_size):
                            chunk = ' '.join(words[i:i+chunk_size])
                            stream_print("token", chunk)
                except Exception as fallback_err:
                    stream_print("error", str(fallback_err))

            if core_agent._tracing:
                core_agent.end_trace()

            stream_print("done", "completed")
            return {"success": True, "response": resp_content}

        except Exception as e:
            if core_agent._tracing:
                core_agent.end_trace()
            stream_print("error", str(e))
            return {"success": False, "error": str(e)}

    if action == "run_team":
        from finagent_core.core_agent import CoreAgent

        query = params.get("query")
        team_config = params.get("team_config", config)
        if not query:
            stream_print("error", "Missing 'query' in params")
            return {"success": False, "error": "Missing 'query' in params"}

        stream_print("thinking", "Initializing team...")

        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        _setup_agent_modules(agent, config, params)

        if agent._guardrails:
            guard_result = agent.check_input(query)
            if not guard_result["passed"]:
                stream_print("error", "Input rejected by guardrails")
                return {"success": False, "error": "Input rejected by guardrails", "violations": guard_result["violations"]}
            query = guard_result["text"]

        try:
            members = team_config.get("members", [])
            stream_print("thinking", f"Team has {len(members)} members")

            for i, member in enumerate(members):
                name = member.get("name", f"Agent {i+1}")
                stream_print("thinking", f"Agent '{name}' processing...")

            response = agent.run_team(query, team_config, params.get("session_id"))
            content = agent.get_response_content(response)

            if content:
                words = content.split()
                chunk_size = 5
                for i in range(0, len(words), chunk_size):
                    chunk = ' '.join(words[i:i+chunk_size])
                    stream_print("token", chunk)

            stream_print("done", "completed")
            return {"success": True, "response": content}

        except Exception as e:
            stream_print("error", str(e))
            return {"success": False, "error": str(e)}

    # =========================================================================
    # Agentic Mode — durable, event-emitting task runners.
    # Events arrive on stdout as AGENTIC_EVENT: <json> lines, parsed by the
    # C++ host (AgentService_Execution.cpp) and republished on the DataHub
    # topic task:event:<task_id>.
    # =========================================================================

    if action == "agentic_start_task":
        from finagent_core.agentic.events import make_emitter
        from finagent_core.agentic.runner import AgenticRunner

        query = params.get("query")
        if not query:
            stream_print("error", "Missing 'query' in params")
            return {"success": False, "error": "Missing 'query' in params"}

        runner = AgenticRunner(api_keys=api_keys, emit=make_emitter())
        result = runner.start_task(query, config)
        stream_print("done", "completed")
        return result

    if action == "agentic_resume_task":
        from finagent_core.agentic.events import make_emitter
        from finagent_core.agentic.runner import AgenticRunner

        task_id = params.get("task_id")
        if not task_id:
            stream_print("error", "Missing 'task_id' in params")
            return {"success": False, "error": "Missing 'task_id' in params"}

        runner = AgenticRunner(api_keys=api_keys, emit=make_emitter())
        result = runner.resume_task(task_id)
        stream_print("done", "completed")
        return result

    if action == "agentic_reply_question":
        # HITL: user replied to a clarifying question. Persist the answer
        # atomically, then resume the task — same code path as agentic_resume_task
        # (which calls TaskStateManager.consume_answer and threads the reply into
        # the next step's prompt).
        from finagent_core.agentic.events import make_emitter
        from finagent_core.agentic.runner import AgenticRunner
        from finagent_core.task_state import get_state_manager

        task_id = params.get("task_id")
        answer = params.get("answer", "")
        if not task_id:
            stream_print("error", "Missing 'task_id' in params")
            return {"success": False, "error": "Missing 'task_id' in params"}

        get_state_manager().save_answer(task_id, answer)
        runner = AgenticRunner(api_keys=api_keys, emit=make_emitter())
        result = runner.resume_task(task_id)
        stream_print("done", "completed")
        return result

    # For non-streaming actions, fall back to regular dispatch
    stream_print("thinking", f"Executing action: {action}")
    result = dispatch_action(action, api_keys, params, config)
    stream_print("done", "completed")
    return result


if __name__ == "__main__":
    result = main()
    print(result)
