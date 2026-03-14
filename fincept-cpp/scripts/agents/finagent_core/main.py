"""
FinAgent Core Main Entry Point

Unified entry point for all agent operations via Tauri.
Handles JSON payload dispatch to appropriate modules.

Performance notes:
- Discovery actions have internal timeout protection
- Uses optimized agent loader to avoid slow file scanning
"""

import sys
import json
import logging
import signal
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


def stream_print(chunk_type: str, content: str):
    """Print a streaming chunk with immediate flush for real-time output"""
    print(f"{chunk_type.upper()}: {content}", flush=True)


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
    Main entry point - accepts single JSON payload from Rust/Tauri.

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
        # Config can contain: model, instructions, tools, memory, knowledge, reasoning,
        # guardrails, tracing, agentic_memory, compression, hooks, evaluation, storage
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
        return route_query(query, api_keys)

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
        agent = SuperAgent(api_keys=api_keys)
        return agent.execute_multi(query, params.get("session_id"), params.get("aggregate", True))

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
        return generate_dynamic_plan(query, api_keys)

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

    # =========================================================================
    # Agent Memory (via CoreAgent module)
    # =========================================================================

    if action == "store_memory":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        agent.setup_agentic_memory(params.get("agentic_memory") or config.get("agentic_memory") or {"user_id": params.get("user_id", "default")})
        content = params.get("content")
        if not content:
            return {"success": False, "error": "Missing 'content' in params"}
        memory_id = agent.store_memory(content, params.get("type", "fact"), params.get("metadata"))
        return {"success": True, "memory_id": memory_id}

    if action == "recall_memories":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        agent.setup_agentic_memory(params.get("agentic_memory") or config.get("agentic_memory") or {"user_id": params.get("user_id", "default")})
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
    Prints chunks in real-time for Rust to capture and emit via Tauri events.
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
                response = streaming_agent.run_streaming(query, full_config, session_id)
            except Exception:
                # Fallback to regular run with simulated streaming
                response = core_agent.run(query, full_config, session_id, stream=False)
                content = core_agent.get_response_content(response)
                if content:
                    words = content.split()
                    chunk_size = 5
                    for i in range(0, len(words), chunk_size):
                        chunk = ' '.join(words[i:i+chunk_size])
                        stream_print("token", chunk)

            if core_agent._tracing:
                core_agent.end_trace()

            stream_print("done", "completed")
            # response may be a generator (streaming) or a RunResponse/str (fallback)
            resp_content = ""
            if response is not None:
                if hasattr(response, 'content'):
                    resp_content = core_agent.get_response_content(response)
                elif not hasattr(response, '__iter__') or isinstance(response, str):
                    resp_content = str(response)
                # generators are already consumed above — leave resp_content as ""
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

    # For non-streaming actions, fall back to regular dispatch
    stream_print("thinking", f"Executing action: {action}")
    result = dispatch_action(action, api_keys, params, config)
    stream_print("done", "completed")
    return result


if __name__ == "__main__":
    result = main()
    print(result)
